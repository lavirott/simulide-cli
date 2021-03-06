/***************************************************************************
 *   Copyright (C) 2012 by santiago González                               *
 *   santigoro@gmail.com                                                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.  *
 *                                                                         *
 ***************************************************************************/

#include <QApplication>
#include <QTranslator>
#include <string>
#include <iostream>
#include "avrcomponentpin.h"
#include "arduino.h"

#include <csignal>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <chrono>
#include <thread>

#include "mainwindow.h"

using namespace std;

#define SIM 0
#define PLAY 1
#define COMPARE 2

int mode = SIM;
MainWindow* win = NULL;


void open_file(string logsPath, QJsonArray &json_array)
{
    QFile file_obj(QString::fromStdString(logsPath));

    if (!file_obj.open(QIODevice::ReadOnly))
    {
        qDebug() << "Failed to open " << QString::fromStdString(logsPath);
        exit(1);
    }
    QTextStream file_text(&file_obj);
    QString json_string;
    json_string = file_text.readAll();
    file_obj.close();
    QByteArray json_bytes = json_string.toLocal8Bit();
    auto json_doc = QJsonDocument::fromJson(json_bytes);

    if (json_doc.isNull())
    {
        qDebug() << "Failed to create JSON doc.";
        exit(2);
    }
    if (!json_doc.isArray())
    {
        qDebug() << "JSON doc is not an array.";
        exit(1);
    }

    json_array = json_doc.array();

    if (json_array.isEmpty())
    {
        qDebug() << "The array is empty";
        exit(1);
    }
}

void save_logs(int s)
{
    if (mode == SIM) {
        extern QJsonArray outputLogs;
        QFile save_file("output.json");
        if (!save_file.open(QIODevice::WriteOnly))
        {
            qDebug() << "failed to open save file";
            exit(1);
        }
        QJsonDocument json_doc(AVRComponentPin::outputLogs);
        QString json_string = json_doc.toJson();
        save_file.write(json_string.toLocal8Bit());
        save_file.close();

        extern QJsonArray inputLogs;
        QFile save_file2("input.json");
        if (!save_file2.open(QIODevice::WriteOnly))
        {
            qDebug() << "failed to open save file";
            exit(1);
        }
        QJsonDocument json_doc2(AVRComponentPin::inputLogs);
        QString json_string2 = json_doc2.toJson();
        save_file2.write(json_string2.toLocal8Bit());
        save_file2.close();
        printf("Logs saved !\n");
        win->close();
    }
        win->close();

}

void init(QJsonArray json_array)
{
    QList<AVRComponentPin *> pinList = Arduino::myPinLIst;
    bool isActive = false;
    for (int p = 0; p < json_array.count(); ++p)
    {
        for (int q = 0; q < pinList.size(); ++q)
        {
            AVRComponentPin *avrpin1 = pinList[q];
            QString port = json_array.at(p).toObject().value(QString::fromStdString("port")).toString();

            if (avrpin1->getId().compare(port) == 0)
            {
                avrpin1->set_pinImpedance(1);
            }
        }
    }
    for (int i = 0; i < json_array.count()-1; ++i)
    {
        QTimer *timer = new QTimer();
        int time;
        while (isActive != false)
        {
            QApplication::processEvents();
        }
        QString port = json_array.at(i).toObject().value(QString::fromStdString("port")).toString();
        int value = json_array.at(i).toObject().value(QString::fromStdString("value")).toInt();
        if (i != 0)
        {
            time = json_array.at(i).toObject().value(QString::fromStdString("time")).toInt() - json_array.at(i - 1).toObject().value(QString::fromStdString("time")).toInt();
        }
        else
        {
            time = json_array.at(i).toObject().value(QString::fromStdString("time")).toInt();
        }
        for (int k = 0; k < pinList.size(); ++k)
        {
            AVRComponentPin *avrpin = pinList[k];
            if (avrpin->getId().compare(port) == 0)
            {
                timer->setSingleShot(true);
                isActive = true;
                QObject::connect(timer, &QTimer::timeout, [avrpin, value, &isActive]() { avrpin->set_pinVoltage(value); isActive = false; });
                timer->start((long)time);
            }
        }
    }
    while (isActive != false)
        {
            QApplication::processEvents();
        }
    qDebug() << "Simulation terminée !";
}
bool inRange(double low, double high, double x)
{
    return (low <= x && x <= high);
}

int main(int argc, char *argv[])
{

#ifdef _WIN32
    QStringList paths = QCoreApplication::libraryPaths();
    paths.append("plugins");
    paths.append("plugins/platforms");
    paths.append("plugins/imageformats");
    paths.append("plugins/sqldrivers");
    paths.append("plugins/bearer");
    paths.append("plugins/generic");
    paths.append("plugins/iconengines");
    paths.append("plugins/qmltooling");
    paths.append("plugins/printsupport");
    QCoreApplication::setLibraryPaths(paths);
    
    if (AttachConsole(ATTACH_PARENT_PROCESS)) 
    {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }
#endif

    string simuPath;
    string hexPath;
    string logsPath;
    string logsPath2;
    string hexplay;
    int margin = 0;
    QJsonArray json_array;
    QJsonArray json_array2;

    for (int i = 0; i < argc - 1; ++i)
    {

        if (strcmp(argv[i], "--simu") == 0)
        {
            simuPath = argv[++i];
        }
        if (strcmp(argv[i], "--hex") == 0)
        {
            mode = SIM;
            hexPath = argv[++i];
        }
        if (strcmp(argv[i], "--play") == 0)
        {
            mode = PLAY;
            logsPath = argv[++i];
            open_file(logsPath, json_array);
        }
        if (strcmp(argv[i], "--hexplay") == 0)
        {
            hexplay = argv[++i];
        }
        if (strcmp(argv[i], "--logs") == 0)
        {
            mode = COMPARE;
            logsPath = argv[++i];
            logsPath2 = argv[++i];
            if (argc >= i + 1)
            {
                margin = atoi(argv[++i]);
            }

            open_file(logsPath, json_array);
            open_file(logsPath2, json_array2);

            int count = 0;
            if (json_array.count() < json_array2.count())
            {
                count = json_array.count();
            }
            else
            {
                count = json_array2.count();
            }
            for (int i = 0; i < count; ++i)
            {
                double time1 = json_array.at(i).toObject().value(QString::fromStdString("time")).toDouble();
                double time2 = json_array2.at(i).toObject().value(QString::fromStdString("time")).toDouble();
                QString port1 = json_array.at(i).toObject().value(QString::fromStdString("port")).toString();
                QString port2 = json_array2.at(i).toObject().value(QString::fromStdString("port")).toString();
                int value1 = json_array.at(i).toObject().value(QString::fromStdString("value")).toInt();
                int value2 = json_array2.at(i).toObject().value(QString::fromStdString("value")).toInt();

                bool time = inRange(time1 - margin, time1 + margin, time2);
                bool port = port2.compare(port1);
                bool value = value2 == value1;

                qDebug() << "Same Time :" << time;
                qDebug() << "Same port :" << !port;
                qDebug() << "Same value :" << value;
            }
        }
    }
    QApplication app( argc, argv );
    MainWindow window;
    win = &window;

    QString locale   = QLocale::system().name().split("_").first();
    QString langFile = "../share/simulide/translations/simulide_"+locale+".qm";
    
    QFile file( langFile );
    if( !file.exists() ) langFile = "../share/simulide/translations/simulide_en.qm";
    
    QTranslator translator;
    translator.load( langFile );
    app.installTranslator( &translator );

    
    if (mode == SIM && !simuPath.empty() && !hexPath.empty())
    {
        window.autoStart(simuPath, hexPath);
    }
    else if (mode == PLAY)
    {
        if (hexplay.empty()){
            QString hex = QDir::currentPath() + "/play.hex";
            hexplay = hex.toStdString();
        }   
        window.autoStart(simuPath, hexplay);
    }

    window.scroll( 0, 50 );

    window.show();
    app.setApplicationVersion( APP_VERSION );
    if (mode == PLAY ){
        QTimer::singleShot(2000, [&json_array] {
            init(json_array);
        });
    }
    signal(SIGINT, save_logs);
    return app.exec();
}

