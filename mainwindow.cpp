#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <qDebug>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setupPlot(ui->customPlot);

    isMyComOpen = false;
    isStart = false;
    ui->button_close->setEnabled(false);

    read_saved_pid_parameter();
    select_serial_port();
}

//检测可用串口
void MainWindow::select_serial_port()
{
    QString portName;
    ui->comboBox->removeItem(0);
    ui->comboBox->removeItem(1);
    myCom = new Win_QextSerialPort("COM1",QextSerialBase::EventDriven);
    for(int i=0; i<12; i++){
        portName = QString("COM%0").arg(i+1);
         myCom->setPortName(portName);
         if(myCom->open(QIODevice::ReadWrite) == true){
             ui->comboBox->addItem(portName);
             myCom->close();
         }
    }
    delete myCom;
    if(ui->comboBox->count() > 0){
        ui->button_open->setEnabled(true);
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

//绘图初始化
void MainWindow::setupPlot(QCustomPlot *customPlot)
{
    t = new QVector<double>;
    x1 = new QVector<double>;
    x2 = new QVector<double>;
    pwm = new QVector<double>;

    customPlot->legend->setVisible(true);
    customPlot->legend->setFont(QFont("Helvetica",9));

    customPlot->addGraph();
    customPlot->graph(0)->setName("x1");
    customPlot->graph(0)->setData(*t, *x1);
    customPlot->graph(0)->setPen(QPen(Qt::blue));

    customPlot->addGraph();
    customPlot->graph(1)->setName("x2");
    customPlot->graph(1)->setData(*t, *x2);
    customPlot->graph(1)->setPen(QPen(Qt::red));

    customPlot->addGraph();
    customPlot->graph(2)->setName("pwm");
    customPlot->graph(2)->setData(*t, *pwm);
    customPlot->graph(2)->setPen(QPen(Qt::green));

    customPlot->xAxis->setLabel("time/s");
    customPlot->xAxis->setAutoTickCount(9);
    customPlot->yAxis->setLabel("angle/(m/s)");
    customPlot->yAxis->setAutoTickCount(9);
    customPlot->xAxis2->setVisible(true);
    customPlot->xAxis2->setTickLabels(false);
    customPlot->yAxis2->setVisible(true);
    customPlot->yAxis2->setTickLabels(false);
    connect(customPlot->xAxis, SIGNAL(rangeChanged(QCPRange)), customPlot->xAxis2, SLOT(setRange(QCPRange)));
    connect(customPlot->yAxis, SIGNAL(rangeChanged(QCPRange)), customPlot->yAxis2, SLOT(setRange(QCPRange)));
    customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
    customPlot->replot();
}

//绘图更新
void MainWindow::plotUpdate(QCustomPlot *customPlot, double _x1, double _x2, double _pwm)
{
    static int num = 0;
    static int counter = 0;
    double time = (counter++)*0.01;
    t->append(time);
    x1->append(_x1);
    x2->append(_x2);
    pwm->append(_pwm);
    if(counter >= 300){
        t->remove(0);
        x1->remove(0);
        x2->remove(0);
        pwm->remove(0);
    }
    //customPlot->graph(0)->rescaleAxes(false);
    customPlot->yAxis->setRange(-180, 180);
    customPlot->xAxis->setRange(time-3, time+1);
    customPlot->graph(0)->setData(*t, *x1);
    customPlot->graph(1)->setData(*t, *x2);
    customPlot->graph(2)->setData(*t, *pwm);
    num++;
    if(num == 10){
        num = 0;
        customPlot->replot();
    }
}

//打开串口
void MainWindow::on_button_open_clicked()
{
    QString portName = ui->comboBox->currentText();

    myCom = new Win_QextSerialPort(portName,QextSerialBase::EventDriven);
    if(myCom ->open(QIODevice::ReadWrite) == false){
        QMessageBox::information(NULL, "星海坊主", "打开串口失败！");
        return;
    }
    myCom->setBaudRate(BAUD115200);
    myCom->setDataBits(DATA_8);
    myCom->setParity(PAR_NONE);
    myCom->setStopBits(STOP_1);
    myCom->setFlowControl(FLOW_OFF);
    myCom->setTimeout(1000);
    connect(myCom,SIGNAL(readyRead()),this,SLOT(readMyCom()));
    isMyComOpen = true;

    ui->button_close->setEnabled(true);
    ui->button_open->setEnabled(false);
}

//检测按钮回调
void MainWindow::on_button_filter_clicked()
{
    select_serial_port();
}

#define BUFFER_HEAD1    0X55
#define BUFFER_HEAD2    0X66
#define BUFFER_SIZE     14
//读串口
void MainWindow::readMyCom()
{
    static unsigned char buffer[BUFFER_SIZE] = {0};
    static unsigned  char counter = 0;
    char ch;
    double _x1, _x2, _pwm;
    while(myCom->bytesAvailable()){
        myCom->getChar(&ch);
        buffer[counter] = ch;
        if(buffer[0] != BUFFER_HEAD1 && counter == 0){
            return;
        }
        counter++;
        if(counter == 2 && buffer[1] != BUFFER_HEAD2){
            counter = 0;
            return;
        }
        if(counter == BUFFER_SIZE){
            counter = 0;
            _x1 = (double)((buffer[2]<<24)|(buffer[3]<<16)|(buffer[4]<<8)|buffer[5])/1000.0f-180;
            _x2 = (double)((buffer[6]<<24)|(buffer[7]<<16)|(buffer[8]<<8)|buffer[9])/1000.0f-180;
            _pwm = (double)((buffer[10]<<24)|(buffer[11]<<16)|(buffer[12]<<8)|buffer[13])/1000.0f-100;
            plotUpdate(ui->customPlot, _x1, _x2, _pwm);
        }
    }
}

//关闭串口
void MainWindow::on_button_close_clicked()
{
    isMyComOpen = false;
    if(myCom->isOpen()){
        myCom->close();
    }
    delete myCom;
    ui->button_close->setEnabled(false);
    ui->button_open->setEnabled(true);
}

#define HEAD1   0XAA
#define HEAD2_A 0X11
#define HEAD2_B 0X22
#define HEAD2_C 0X33
#define HEAD2_SWITCH_START  0XBB
#define HEAD2_SWITCH_STOP   0XCC
//发送一个参数
void MainWindow::send_parameter(float val, unsigned char head2)
{
    char buffer[7] = {0};
    buffer[0] = HEAD1;
    buffer[1] = head2;
    unsigned int int_val = (unsigned int)(val*1000+0.5f);
    buffer[2] = int_val>>24;
    buffer[3] = int_val>>16;
    buffer[4] = int_val>>8;
    buffer[5] = int_val;
    buffer[6] = buffer[2]^buffer[3]^buffer[4]^buffer[5];
    if(isMyComOpen){
        myCom->write(buffer, 7);
    }
}

//滑动条PID->A回调
void MainWindow::on_hSlider_pid_a_valueChanged(int value)
{
    int min = ui->lineEdit_min_a->text().toInt();
    int max = ui->lineEdit_max_a->text().toInt();
    float a = value/100.0f*(max-min)+min;
    QString a_str = QString("%1").arg(a);
    ui->lineEdit_pid_a->setText(a_str);
    send_parameter(a, HEAD2_A);
}

//滑动条PID->B回调
void MainWindow::on_hSlider_pid_b_valueChanged(int value)
{
    int min = ui->lineEdit_min_b->text().toInt();
    int max = ui->lineEdit_max_b->text().toInt();
    float b = value/100.0f*(max-min)+min;
    QString b_str = QString("%1").arg(b);
    ui->lineEdit_pid_b->setText(b_str);
    send_parameter(b, HEAD2_B);
}

//滑动条PID->C回调
void MainWindow::on_hSlider_pid_c_valueChanged(int value)
{
    int min = ui->lineEdit_min_c->text().toInt();
    int max = ui->lineEdit_max_c->text().toInt();
    float c = value/100.0f*(max-min)+min;
    QString c_str = QString("%1").arg(c);
    ui->lineEdit_pid_c->setText(c_str);
    send_parameter(c, HEAD2_C);
}

#include <stdio.h>
//读取PID参数
void MainWindow::read_saved_pid_parameter()
{
    FILE *fp;
    fp = fopen("./pid.txt", "r");
    if(fp == NULL){
        QMessageBox::information(NULL, "星海坊主", "pid.txt打开失败！");
        return;
    }
    QString str;
    float a;
    int pos_a, max_a, min_a;
    float b;
    int pos_b, max_b, min_b;
    float c;
    int pos_c, max_c, min_c;

    fscanf(fp, "%f %d %d %d\n", &a, &pos_a, &max_a, &min_a);
    fscanf(fp, "%f %d %d %d\n", &b, &pos_b, &max_b, &min_b);
    fscanf(fp, "%f %d %d %d\n", &c, &pos_c, &max_c, &min_c);

    str = QString("%1").arg(max_a);
    ui->lineEdit_max_a->setText(str);
    str = QString("%1").arg(min_a);
    ui->lineEdit_min_a->setText(str);
    str = QString("%1").arg(a);
    ui->lineEdit_pid_a->setText(str);
    ui->hSlider_pid_a->setValue(pos_a);

    str = QString("%1").arg(max_b);
    ui->lineEdit_max_b->setText(str);
    str = QString("%1").arg(min_b);
    ui->lineEdit_min_b->setText(str);
    str = QString("%1").arg(b);
    ui->lineEdit_pid_b->setText(str);
    ui->hSlider_pid_b->setValue(pos_b);

    str = QString("%1").arg(max_c);
    ui->lineEdit_max_c->setText(str);
    str = QString("%1").arg(min_c);
    ui->lineEdit_min_c->setText(str);
    str = QString("%1").arg(c);
    ui->lineEdit_pid_c->setText(str);
    ui->hSlider_pid_c->setValue(pos_c);

    fclose(fp);
}

//保存PID参数
void MainWindow::save_pid_parameter()
{
    FILE *fp;
    fp = fopen("./pid.txt", "w");
    if(fp == NULL){
        QMessageBox::information(NULL, "星海坊主", "pid.txt打开失败！");
        return;
    }
    float a;
    int pos_a, max_a, min_a;
    float b;
    int pos_b, max_b, min_b;
    float c;
    int pos_c, max_c, min_c;

    a = ui->lineEdit_pid_a->text().toFloat();
    b = ui->lineEdit_pid_b->text().toFloat();
    c = ui->lineEdit_pid_c->text().toFloat();

    pos_a = ui->hSlider_pid_a->value();
    pos_b = ui->hSlider_pid_b->value();
    pos_c = ui->hSlider_pid_c->value();

    max_a = ui->lineEdit_max_a->text().toInt();
    max_b = ui->lineEdit_max_b->text().toInt();
    max_c = ui->lineEdit_max_c->text().toInt();

    min_a = ui->lineEdit_min_a->text().toInt();
    min_b = ui->lineEdit_min_b->text().toInt();
    min_c = ui->lineEdit_min_c->text().toInt();

    fprintf(fp, "%f %d %d %d\n", a, pos_a, max_a, min_a);
    fprintf(fp, "%f %d %d %d\n", b, pos_b, max_b, min_b);
    fprintf(fp, "%f %d %d %d\n", c, pos_c, max_c, min_c);
    fclose(fp);
}

//串口关闭事件，保存PID参数
void MainWindow::closeEvent(QCloseEvent *event)
{
    save_pid_parameter();
}

void MainWindow::on_button_switch_clicked()
{
    if(isStart){
        isStart = false;
        send_parameter(0, HEAD2_SWITCH_STOP);
    }
    else{
        isStart = true;
        send_parameter(0, HEAD2_SWITCH_START);
    }
}
