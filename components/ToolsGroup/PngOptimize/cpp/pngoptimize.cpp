﻿/*
    This file is part of JQTools

    Project introduce: https://github.com/188080501/JQTools

    Copyright: Jason

    Contact email: Jason@JasonServer.com

    GitHub: https://github.com/188080501/
*/

#include "pngoptimize.h"

// C++ lib import
#include <functional>

// Qt lib import
#include <QFileDialog>
#include <QStandardPaths>
#include <QtConcurrent>

// JQLibrary import
#include "JQZopfli.h"
#include "JQFile.h"

using namespace PngOptimize;

QString Manage::optimizePngByFilePaths(const bool &coverOldFile, const QJsonArray &filePaths_)
{
    QStringList filePaths;

    for ( const auto &filePath: filePaths_ )
    {
        filePaths.push_back( filePath.toString() );
    }

    return this->optimizePng( coverOldFile, filePaths );
}

QString Manage::optimizePngByOpenFiles(const bool &coverOldFile)
{
    QStringList filePaths;

    filePaths = QFileDialog::getOpenFileNames(
                        nullptr,
                        QStringLiteral( "\u8BF7\u9009\u62E9PNG\u56FE\u7247\uFF08\u53EF\u591A\u9009\uFF09" ),
                        QStandardPaths::writableLocation( QStandardPaths::DesktopLocation ),
                        "*.png"
                    );

    if ( filePaths.isEmpty() ) { return "cancel"; }

    return this->optimizePng( coverOldFile, filePaths );
}

QString Manage::optimizePngByOpenDirectory(const bool &coverOldFile)
{
    QStringList filePaths;

    const auto &&directoryPath = QFileDialog::getExistingDirectory(
                nullptr,
                QStringLiteral( "\u8BF7\u9009\u62E9\u5305\u542BPNG\u56FE\u7247\u7684\u6587\u4EF6\u5939" ),
                QStandardPaths::writableLocation( QStandardPaths::DesktopLocation )
            );

    if ( directoryPath.isEmpty() ) { return "cancel"; }

    JQFile::foreachFileFromDirectory(
                directoryPath,
                [ &filePaths ]
                (const QFileInfo &fileInfo)
                {
                    if ( fileInfo.suffix().toLower() != "png" ) { return; }

                    filePaths.push_back( fileInfo.filePath() );
                },
                true
            );

    if ( directoryPath.isEmpty() ) { return "empty"; }

    return this->optimizePng( coverOldFile, filePaths );
}

void Manage::startOptimize(const QString &currentFilePath)
{
    if ( !waitOptimizeQueue_.contains( currentFilePath ) ) { return; }

    QtConcurrent::run( waitOptimizeQueue_[ currentFilePath ] );

    waitOptimizeQueue_.remove( currentFilePath );
}

QString Manage::urlToLocalPngFilePath(const QVariant &url)
{
    QFileInfo fileInfo( url.toUrl().toLocalFile() );
    if ( !fileInfo.isFile() ) { return { }; }
    if ( !fileInfo.filePath().toLower().endsWith( ".png" ) ) { return { }; }
    return fileInfo.filePath();
}

QString Manage::optimizePng(const bool &coverOldFile, const QStringList &filePaths)
{
    QString targetDir;

    if ( !coverOldFile )
    {
        targetDir = QStandardPaths::writableLocation( QStandardPaths::DesktopLocation ) + "/JQTools_OptimizePngResult/";

        if ( !QDir( targetDir ).exists() && !QDir().mkdir( targetDir ) )
        {
            return "mkdir error";
        }
    }

    QJsonArray fileList;

    auto makeSizeString = [](const int &size)
    {
        if ( size < 1024 )
        {
            return QString( "%1 byte" ).arg( size );
        }
        else if ( size < ( 1024 * 1024 ) )
        {
            return QString( "%1 kb" ).arg( size / 1024 );
        }
        else
        {
            return QString( "%1.%2 mb" ).arg( size / 1024 / 1024 ).arg( size / 1024 % 1024 );
        }
    };

    static auto packageCount = 0;
    static QMutex mutex;

    for ( const auto &filePath: filePaths )
    {
        QFileInfo fileInfo( filePath );

        fileList.push_back( QJsonObject( { {
                                               { "fileName", fileInfo.fileName() },
                                               { "filePath", filePath },
                                               { "originalSize", makeSizeString( fileInfo.size() ) }
                                           } } ) );

        ++packageCount;

        waitOptimizeQueue_[ filePath ] = [
                this,
                filePath,
                makeSizeString,
                fileName = fileInfo.fileName(),
                originalFilePath = filePath,
                resultFilePath = ( targetDir.isEmpty() ) ? ( filePath ) : ( targetDir + "/" + fileInfo.fileName() )
                ]()
        {
            emit this->optimizePngStart( filePath );

            auto optimizeResult = JQZopfli::optimize( originalFilePath, resultFilePath );

            emit this->optimizePngFinish(
                        filePath,
                        { {
                              { "optimizeSucceed", optimizeResult.optimizeSucceed },
                              { "resultSize", makeSizeString( optimizeResult.resultSize ) },
                              { "compressionRatio", QString( "%1%2%" ).
                                arg( ( optimizeResult.compressionRatio < 1 ) ? ( "-" ) : ( "" )  ).
                                arg( 100 - (int)(optimizeResult.compressionRatio * 100) ) },
                              { "compressionRatioColor", QString( "%1" ).
                                arg( ( optimizeResult.compressionRatio < 1 ) ? ( "#64dd17" ) : ( "#f44336" )  ) },
                              { "timeConsuming", QString( "%1ms" ).arg( optimizeResult.timeConsuming ) }
                          } }
                    );

            mutex.lock();

            --packageCount;

            if ( packageCount <= 0 )
            {
                emit this->optimizeEnd();
            }

            mutex.unlock();
        };
    }

    emit this->optimizeStart( fileList );

    return "OK";
}
