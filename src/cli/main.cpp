/****************************************************************************
**
** SVG Cleaner is batch, tunable, crossplatform SVG cleaning program.
** Copyright (C) 2012-2015 Evgeniy Reizner
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License along
** with this program; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
**
****************************************************************************/

#include <QDir>
#include <QtDebug>
#include <locale.h>

#ifdef USE_IPC
#include <QSharedMemory>
#include <QBuffer>
#include "../3rdparty/systemsemaphore/systemsemaphore.h"

#include <QtGlobal>
#if QT_VERSION >= 0x050000
    #include <QDataStream>
#endif

#endif

#include "remover.h"
#include "replacer.h"

void printLine(int keyId, const QString &desc = QString())
{
    if (desc.isEmpty())
        qDebug("  %s %s", qPrintable(Keys.keyName(keyId).leftJustified(35, QL1C(' '))),
                          qPrintable(Keys.description(keyId)));
    else
        qDebug("  %s %s", qPrintable(Keys.keyName(keyId).leftJustified(35, QL1C(' '))),
                          qPrintable(desc));
}

void printLine(const QString &key, const QString &desc)
{
    qDebug("  %s %s", qPrintable(key.leftJustified(35, QL1C(' '))), qPrintable(desc));
}

void showPresetInfo(const QString &presetName)
{
    QList<int> list;
    if (presetName.endsWith(Preset::Basic)) {
        Keys.setPreset(Preset::Basic);
        list = Keys.basicPresetKeys();
    } else if (presetName.endsWith(Preset::Complete)) {
        Keys.setPreset(Preset::Complete);
        list = Keys.completePresetKeys();
    } else if (presetName.endsWith(Preset::Extreme)) {
        Keys.setPreset(Preset::Extreme);
        list = Keys.extremePresetKeys();
    }
    foreach (const int &key, list) {
        if (   key == Key::TransformPrecision
            || key == Key::AttributesPrecision
            || key == Key::CoordsPrecision) {
            qDebug() << Keys.keyName(key) + QL1S("=") + fromDouble(Keys.intNumber(key));
        } else if (key == Key::RemoveTinyGaussianBlur) {
            qDebug() << Keys.keyName(key) + QL1S("=") + fromDouble(Keys.doubleNumber(key));
        } else {
            qDebug() << Keys.keyName(key);
        }
    }
}

void showHelp()
{
    Keys.prepareDescription();

    qDebug() << "SVG Cleaner could help you to clean up your SVG files from unnecessary data.";
    qDebug() << "";
    qDebug() << "Usage:";
    qDebug() << "  svgcleaner-cli <in-file> <out-file> [--preset=] [--options]";
    qDebug() << "Show options included in preset:";
    qDebug() << "  svgcleaner-cli --info --preset=<name>";
    qDebug() << "";
    qDebug() << "Presets:";
    printLine(QL1S("--preset=basic"),    QL1S("Basic cleaning"));
    printLine(QL1S("--preset=complete"), QL1S("Complete cleaning [default]"));
    printLine(QL1S("--preset=extreme"),  QL1S("Extreme cleaning"));
    qDebug() << "";
    qDebug() << "Options:";
    qDebug() << "";
    printLine(QL1S("-h --help"),    QL1S("Show this text"));
    printLine(QL1S("-v --version"), QL1S("Show version"));
    qDebug() << "";

    qDebug() << "Elements:";
    foreach (const int &key, Keys.elementsKeysId()) {
        if (key == Key::RemoveTinyGaussianBlur)
            printLine(Keys.keyName(key) + QL1S("=<0..1.0>"),
                      Keys.description(key) + QString(QL1S(" [default: %1]"))
                        .arg(Keys.doubleNumber(Key::RemoveTinyGaussianBlur)));
        else
            printLine(key);
    }
    qDebug() << "";
    qDebug() << "Attributes:";
    foreach (const int &key, Keys.attributesKeysId())
        printLine(key);
    qDebug() << "Additional:";
    foreach (const int &key, Keys.attributesUtilsKeysId())
        printLine(key);
    qDebug() << "";
    qDebug() << "Paths:";
    foreach (const int &key, Keys.pathsKeysId())
        printLine(key, Keys.description(key));
    qDebug() << "";
    qDebug() << "Optimizations:";
    foreach (const int &key, Keys.optimizationsKeysId()) {
        if (   key == Key::TransformPrecision
            || key == Key::AttributesPrecision
            || key == Key::CoordsPrecision) {
            printLine(Keys.keyName(key) + QL1S("=<1..8>"),
                      Keys.description(key) + QString(QL1S(" [default: %1]"))
                        .arg(Keys.doubleNumber(key)));
        } else
            printLine(key);
    }
    qDebug() << "Additional:";
    foreach (const int &key, Keys.optimizationsUtilsKeysId())
        printLine(key);
}

void processFile(const QString &inPath, const QString &outPath)
{
    if (!Keys.flag(Key::ShortOutput))
        qDebug("The initial file size is: %u", (int)QFile(inPath).size());

    SvgDocument doc;
    bool flag = doc.loadFile(inPath);
    if (!flag)
        qFatal("%s", qPrintable(doc.lastError()));
    if (BaseCleaner::svgElement(doc).isNull())
        qFatal("invalid svg file");

    Replacer replacer(doc);
    Remover remover(doc);

    doc.calcElemAttrCount(QL1S("initial"));

    // TODO: fix double clean issues

    // mandatory fixes used to simplify subsequent functions
    // cannot be disabled
    replacer.convertEntityData();
    replacer.splitStyleAttributes();
    replacer.convertCDATAStyle();
    replacer.convertUnits();
    replacer.prepareDefs();
    replacer.fixWrongAttr();
    replacer.roundNumericAttributes();
    replacer.prepareLinkedStyles();
    replacer.convertColors();

    // cleaning methods
    remover.cleanSvgElementAttribute();
    if (Keys.flag(Key::CreateViewbox))
        replacer.convertSizeToViewbox();
    if (Keys.flag(Key::RemoveUnusedDefs))
        remover.removeUnusedDefs();
    if (Keys.flag(Key::ApplyTransformsToDefs))
        replacer.applyTransformToDefs();
    if (Keys.flag(Key::RemoveNotAppliedAttributes))
        remover.removeUnusedDefsAttributes();
    if (Keys.flag(Key::RemoveDuplicatedDefs))
        remover.removeDuplicatedDefs();
    if (Keys.flag(Key::MergeGradients)) {
        replacer.mergeGradients();
        replacer.mergeGradientsWithEqualStopElem();
    }
    remover.removeElements();
    remover.removeAttributes();
    remover.removeElementsFinal();
    if (Keys.flag(Key::RemoveUnreferencedIds))
        remover.removeUnreferencedIds();
    remover.cleanPresentationAttributes();
    if (Keys.flag(Key::ApplyTransformsToShapes))
        replacer.applyTransformToShapes();
    if (Keys.flag(Key::RemoveOutsideElements))
        replacer.calcElementsBoundingBox();
    if (Keys.flag(Key::ConvertBasicShapes))
        replacer.convertBasicShapes();
    if (Keys.flag(Key::UngroupContainers)) {
        remover.ungroupAElement();
        remover.ungroupSwitchElement();
        remover.removeGroups();
    }
    if (Keys.flag(Key::GroupRemoveFill)) { // MBCHANGE!!!
        remover.removeGroupFill();
    }
    if (Keys.flag(Key::PathRemoveFill)) { // MBCHANGE!!!
        remover.removePathFill();
    }
    replacer.processPaths();
    if (Keys.flag(Key::ReplaceEqualPathsByUse))
        replacer.replaceEqualPathsWithUse();
    if (Keys.flag(Key::RemoveOutsideElements))
        remover.removeElementsOutsideTheViewbox();
    if (Keys.flag(Key::ReplaceEqualEltsByUse))
        replacer.replaceEqualElementsByUse();
    if (Keys.flag(Key::RemoveNotAppliedAttributes))
        replacer.moveStyleFromUsedElemToUse();
    if (Keys.flag(Key::GroupTextStyles))
        replacer.groupTextElementsStyles();
    if (Keys.flag(Key::GroupElemByStyle))
        replacer.groupElementsByStyles();
    if (Keys.flag(Key::ApplyTransformsToDefs))
        replacer.applyTransformToDefs();
    if (Keys.flag(Key::TrimIds))
        replacer.trimIds();
    remover.checkXlinkDeclaration();
    if (Keys.flag(Key::SortDefs))
        replacer.sortDefs();
    replacer.finalFixes();
    if (Keys.flag(Key::JoinStyleAttributes))
        replacer.joinStyleAttr();


    // save file
    QFile outFile(outPath);
    if (!outFile.open(QIODevice::WriteOnly))
        qFatal("could not write output file");

    int indent = 1;
    if (Keys.flag(Key::CompactOutput))
        indent = -1;
    outFile.write(doc.toString(indent).toUtf8());
    outFile.close();

    if (!Keys.flag(Key::ShortOutput))
        qDebug("The final file size is: %u", (int)QFile(outPath).size());

    doc.calcElemAttrCount(QL1S("final"));
}

// the code underneath is from QtCore module (qcorecmdlineargs_p.h) (LGPLv2 license)
#ifdef Q_OS_WIN
#include "windows.h"
template<typename Char>
static QVector<Char*> qWinCmdLine(Char *cmdParam, int length, int &argc)
{
    QVector<Char*> argv(8);
    Char *p = cmdParam;
    Char *p_end = p + length;

    argc = 0;

    while (*p && p < p_end) {
        while (QChar((short)(*p)).isSpace())
            p++;
        if (*p && p < p_end) {
            int quote;
            Char *start, *r;
            if (*p == Char('\"') || *p == Char('\'')) {
                quote = *p;
                start = ++p;
            } else {
                quote = 0;
                start = p;
            }
            r = start;
            while (*p && p < p_end) {
                if (quote) {
                    if (*p == quote) {
                        p++;
                        if (QChar((short)(*p)).isSpace())
                            break;
                        quote = 0;
                    }
                }
                if (*p == '\\') {
                    if (*(p+1) == quote)
                        p++;
                } else {
                    if (!quote && (*p == Char('\"') || *p == Char('\''))) {
                        quote = *p++;
                        continue;
                    } else if (QChar((short)(*p)).isSpace() && !quote)
                        break;
                }
                if (*p)
                    *r++ = *p++;
            }
            if (*p && p < p_end)
                p++;
            *r = Char('\0');

            if (argc >= (int)argv.size()-1)
                argv.resize(argv.size()*2);
            argv[argc++] = start;
        }
    }
    argv[argc] = 0;

    return argv;
}

QStringList arguments(int &argc, char **argv)
{
    Q_UNUSED(argv);
    QStringList list;
    argc = 0;
    QString cmdLine = QString::fromWCharArray(GetCommandLine());
    QVector<wchar_t*> argvVec = qWinCmdLine<wchar_t>((wchar_t *)cmdLine.utf16(),
                                                     cmdLine.length(), argc);
    for (int a = 0; a < argc; ++a)
        list << QString::fromWCharArray(argvVec[a]);
    return list;
}
#else
QStringList arguments(int &argc, char **argv)
{
    QStringList list;
    const int ac = argc;
    char ** const av = argv;
    for (int a = 0; a < ac; ++a)
        list << QString::fromLocal8Bit(av[a]);
    return list;
}
#endif

#ifdef USE_IPC
SystemSemaphore *semaphore2 = 0;
QString appLog;
#if QT_VERSION >= 0x050000
void slaveMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    appLog += msg + QL1S("\n");
#else
void slaveMessageOutput(QtMsgType type, const char *msg)
{
    appLog += QString(QL1S(msg)) + QL1S("\n");
#endif

    if (type == QtFatalMsg) {
        // emit to 'gui' that we crashed
        // crash will be detected by timeout anyway, but this way is much faster
        if (semaphore2)
            semaphore2->release();
        exit(1);
    }
}
#endif

// TODO: fix msgbox on Win
#if QT_VERSION >= 0x050000
void ownMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &strMsg)
{
    const char *msg = strMsg.toStdString().c_str();
#else
void ownMessageOutput(QtMsgType type, const char *msg)
{
#endif
    if (type == QtFatalMsg) {
        fprintf(stderr, "Error: %s\n", msg);
        exit(1);
    } else if (type == QtWarningMsg) {
        fprintf(stderr, "Warning: %s\n", msg);
    } else {
        fprintf(stderr, "%s\n", msg);
    }
}

int main(int argc, char *argv[])
{
#ifdef Q_OS_UNIX
    setlocale(LC_ALL, "");
#endif
    QLocale::setDefault(QLocale::English);

    QStringList argList = arguments(argc, argv);
    // remove executable path
    argList.removeFirst();

    if (argList.contains(QL1S("-v")) || argList.contains(QL1S("--version"))) {
        qDebug() << "0.7.0";
        return 0;
    }

    if (argList.contains(QL1S("-h")) || argList.contains(QL1S("--help")) || argList.size() < 2) {
        showHelp();
        return 0;
    }

    if (argList.contains(QL1S("--info")) && argList.size() == 2) {
        showPresetInfo(argList.at(1));
        return 0;
    }

#ifdef USE_IPC
    if (argList.first() == QL1S("--slave")) {
        argList.removeFirst();
        QString id = argList.takeFirst();
        QSharedMemory sharedMemory(QL1S("SvgCleanerMem_") + id);
        SystemSemaphore semaphore1(QL1S("SvgCleanerSemaphore1_") + id);
        semaphore2 = new SystemSemaphore(QL1S("SvgCleanerSemaphore2_") + id);

        if (!sharedMemory.attach())
            qFatal("unable to attach to shared memory segment.");

        #if QT_VERSION >= 0x050000
            qInstallMessageHandler(slaveMessageOutput);
        #else
            qInstallMsgHandler(slaveMessageOutput);
        #endif

        Keys.parseOptions(argList);

        // emit to 'gui' that 'cli' ready to clean files
        semaphore2->release();

        while (true) {
            // wait while 'gui' set paths to shared memory
            if (!semaphore1.acquire())
                break;

            // read shared memory
            QBuffer buffer;
            QDataStream in(&buffer);
            QString inFile;
            QString outFile;
            buffer.setData((char*)sharedMemory.constData(), sharedMemory.size());
            buffer.open(QBuffer::ReadWrite);
            in >> inFile;
            in >> outFile;

            // if both paths is empty - this is signal to stop 'cli'
            if (inFile.isEmpty() && outFile.isEmpty()) {
                semaphore2->release();
                break;
            }

            appLog.clear();

            // clean svg
            processFile(inFile, outFile);

            // write to shared memory
            buffer.seek(0);
            QDataStream out(&buffer);
            out << appLog;
            int size = buffer.size();
            char *to = (char*)sharedMemory.data();
            const char *from = buffer.data().data();
            memcpy(to, from, qMin(sharedMemory.size(), size));

            // emit to 'gui' that file was cleaned
            semaphore2->release();
        }
    }
    else
#endif
    {
        #if QT_VERSION >= 0x050000
                qInstallMessageHandler(ownMessageOutput);
        #else
                qInstallMsgHandler(ownMessageOutput);
        #endif

        QString inputFile  = argList.takeFirst();
        QString outputFile = argList.takeFirst();

        if (!QFile(inputFile).exists())
            qFatal("input file does not exist");
        if (!QFileInfo(outputFile).absoluteDir().exists())
            qFatal("output folder does not exist");

        Keys.parseOptions(argList);
        processFile(inputFile, outputFile);
    }

    return 0;
}
