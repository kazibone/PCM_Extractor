#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "QFileDialog"
#include "QMessageBox"
#include "parasetting.h"


#define WARNING_WAVE_FILE       1
#define WARNING_DATA_FILE       2


unsigned int dataLen = 0;
char dataString[] = {"data"};
char riffString[] = {"RIFF"};
char waveString[] = {"WAVEfmt"};


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowTitle(QString("PCM Extractor(.wav file only)") + QString(SOFTWARE_VERSION));

//    // Set LogFile to Save message while running
//    QDir dir;
//    dir.cd(QDir::currentPath());
//    if (!dir.exists("LogFiles")) {
//        dir.mkdir("LogFiles");
//    }

    wavFileFullPath = QDir::currentPath();
    isWavFile = false;
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_enterButton_clicked()
{
    // Now try to get PCM data

    if (!isWavFile) {
        // No wav file, select one
        if (!SelectWavFile())
            return;
    }         

    if (ReadWavFileData()) {
        QMessageBox::warning(this, tr("Success!"),
                tr("Now PCM data already got!"),
                QMessageBox::Yes);
    }
    else {
        QMessageBox::warning(this, tr("Fail!"),
                tr("Please try again!"),
                QMessageBox::Yes);
    }

    // close file
    if (wavfile.is_open())
        wavfile.close();
    if (data_file.is_open())
        data_file.close();
    if (data_file.is_open())
        data_file.close();
}


bool MainWindow::SelectWavFile(void) {
    wavFileFullPath = QFileDialog::getOpenFileName(this,
                                                   tr("Select a .wav file"),
                                                   wavFileFullPath,
                                                   "WAV files (*.wav);;All files (*.*)");

    if (!wavFileFullPath.isEmpty()) {
        if (MainWindow::ui->directoryComboBox->findText(wavFileFullPath) == -1) {
            ui->directoryComboBox->addItem(wavFileFullPath);
        }
        ui->directoryComboBox->setCurrentIndex(ui->directoryComboBox->findText(wavFileFullPath));

        isWavFile = CheckWavFile();
    }
    else {
        isWavFile = false;
    }

    if (!isWavFile) {
        if (CallWarningBox(WARNING_WAVE_FILE)) {
            return SelectWavFile();
        }
        else
            return false;
    }

    return true;
}


bool MainWindow::CheckWavFile(void) {
    if (wavfile.is_open())
        wavfile.close();

    wavfile.open(wavFileFullPath.toStdString(), std::ios::in | std::ios::binary);
    if (!wavfile.is_open()) {
        return false;
    }

    // Check wav form data
    char temp[15] = {0};

    wavfile.read(temp, 4);      // Read "RIFF"

    wavfile.seekg(0x08);
    wavfile.read(temp+4, 7);    // Read "WAVEfmt"

    wavfile.seekg(0x24);
    wavfile.read(temp+11, 4);   // Read "data"

    int i=0;
    for (i=0; i<15; i++) {
        if (    (i<4 && riffString[i] != temp[i]) ||
                (i>=4 && i<11 && waveString[i-4] != temp[i]) ||
                (i>=11 && dataString[i-11] != temp[i]) ) {

            wavfile.close();
            return false;
        }
    }

    // Read data length
    unsigned char datalength[4] = {0};
    wavfile.seekg(0x28);
    wavfile.read((char*)datalength, 4);

    dataLen = 0;
    for (int i=0; i<4; i++) {
        dataLen += ((unsigned int)(datalength[i]) << i*8);
    }

    wavfile.seekg(0, std::ios::end);
    unsigned int endFile = wavfile.tellg();
    wavfile.seekg(0x2C);
    unsigned int startData = wavfile.tellg();
    unsigned int tmpLen = endFile - startData;

    if (dataLen > tmpLen) {
        wavfile.close();
        return false;
    }

    return true;
}

bool MainWindow::CallWarningBox(unsigned char textNum) {
    QMessageBox box;
    box.setWindowTitle(tr("WARNING"));
    box.setIcon(QMessageBox::Warning);

    switch (textNum) {
        case WARNING_WAVE_FILE:
            box.setText(tr("Invaild wave file, select other vaild file again?"));
            break;
        case WARNING_DATA_FILE:
            box.setText(tr("Invaild Data file, select other vaild file again?"));
            break;
        default:
            box.setText(tr("Error occur, still continue?"));;
    }

    QPushButton *yesBtn = box.addButton(tr("Yes(&Y)"),
                                        QMessageBox::YesRole);
    QPushButton *cancelBut = box.addButton(tr("No"),
                                           QMessageBox::RejectRole);
    box.exec();

    if (box.clickedButton() == yesBtn) {
        return true;
    }
    else if (box.clickedButton() == cancelBut) {
        return false;
    }

    return false;
}


bool MainWindow::ReadWavFileData(void) {
    int locate_tmp1, locate_tmp2;
    QString dataFileDefaultPath = wavFileFullPath;
    QString data_file_path;

    // Get data
    unsigned char pcmData[dataLen];
    wavfile.read((char*)pcmData, dataLen);


    locate_tmp1 = dataFileDefaultPath.lastIndexOf(".");
    dataFileDefaultPath = dataFileDefaultPath.left(locate_tmp1);


    // Open a file that saving PCM data
    while(1) {
        data_file_path = QFileDialog::getSaveFileName(this,
                                                            tr("Select a file to save data"),
                                                            dataFileDefaultPath,
                                                            "head files (*.h);;All files (*.*)");
        data_file.open(data_file_path.toStdString(), std::ios::out);

        if (!data_file.is_open()) {
            if (!CallWarningBox(WARNING_DATA_FILE))
                return false;
        }
        else {
            break;
        }
    }

    locate_tmp1 = data_file_path.length() - data_file_path.lastIndexOf("/") - 1;
    data_file_path = data_file_path.right(locate_tmp1);
    locate_tmp2 = data_file_path.lastIndexOf(".");
    data_file_path = data_file_path.left(locate_tmp2);


    // Output data to file
    if (!ui->checkBox->isChecked()) {
        data_file << "const unsigned char "<< data_file_path.toStdString() << "[" << std::dec << dataLen << "] = {\n    ";

        for (unsigned int i=0; i<dataLen; i++) {
            data_file << "0x" << std::hex << (unsigned int)(pcmData[i]);
            if (i != dataLen - 1) {
                data_file << ", ";

                if ((i+1)%16 == 0) {
                    data_file << "\n    ";
                }
            }
        }
        data_file << "\n};" << std::endl;
    }
    else {
        // convert data
        data_file << "const unsigned char "<< data_file_path.toStdString() << "[" << std::dec << dataLen << "] = {\n    ";
        for (unsigned int i=0; i<dataLen; i+=2) {
            unsigned int tmp_data = (unsigned int)(pcmData[i+1]) * 256 + (unsigned int)(pcmData[i]) + 0x8000;
            tmp_data = (tmp_data % 65536) / 16;
            pcmData[i] = tmp_data % 256;
            pcmData[i+1] = tmp_data / 256;
            data_file << "0x" << std::hex << (unsigned int)(pcmData[i]);
            data_file << ", 0x" << std::hex << (unsigned int)(pcmData[i+1]);
            if (i != dataLen - 2) {
                data_file << ", ";

                if ((i+2)%16 == 0) {
                    data_file << "\n    ";
                }
            }
        }
        data_file << "\n};" << std::endl;
    }

    return true;
}



void MainWindow::on_exitButton_clicked()
{

}

void MainWindow::on_toolButton_clicked()
{
    SelectWavFile();
}
