#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h>
#include <signal.h>
#include <error.h>

#include <QDebug>
#include "HW_interface.h"
#include "GPIO.h"
#include "ds18b20.h"
#include "PWM.h"
#include "pid.h"
#include "sht21.h"
#include "tlc1543.h"

/* 定义GPIO结构体以进行初始化,高电平打开，低电平关闭 */
GPIO_Init_Struct gpio47_Beep;//蜂鸣器，用于声音提示
GPIO_Init_Struct gpio46_Heat_S;//加热电路开关，控制加热电路通断
GPIO_Init_Struct gpio27_Pump_S;//气泵开关
GPIO_Init_Struct gpio67_M1;//电磁阀1
GPIO_Init_Struct gpio69_M2;//电磁阀2
GPIO_Init_Struct gpio65_M3;//电磁阀3
GPIO_Init_Struct gpio68_M4;//电磁阀4

/* 定义PWM结构体以进行初始化 */
PWM_Init_Struct pwm_8_13_airpump;//该PWM输出用于控制气泵转速
PWM_Init_Struct pwm_9_22_fanyingshi;//该PWM输出用于控制反应室加热带
PWM_Init_Struct pwm_9_42_zhenfashi;//该PWM输出用于控制蒸发室加热带

/* 上电后系统初始化各个硬件电路，配置操作系统状态，等待用户操作 */
void init_hardware(void)
{
    /* 所有gpio口均设置为输出，并且输出低电平*/
    gpio47_Beep.pin = 47;
    gpio47_Beep.dir = OUTPUT_PIN;
    GPIO_Init(&gpio47_Beep);
    gpio_set_value(&gpio47_Beep,LOW);

    gpio46_Heat_S.pin = 46;
    gpio46_Heat_S.dir = OUTPUT_PIN;
    GPIO_Init(&gpio46_Heat_S);
    gpio_set_value(&gpio46_Heat_S,LOW);

    gpio27_Pump_S.pin = 27;
    gpio27_Pump_S.dir = OUTPUT_PIN;
    GPIO_Init(&gpio27_Pump_S);
    gpio_set_value(&gpio27_Pump_S,LOW);

    gpio67_M1.pin = 67;
    gpio67_M1.dir = OUTPUT_PIN;
    GPIO_Init(&gpio67_M1);
    gpio_set_value(&gpio67_M1,LOW);

    gpio69_M2.pin = 69;
    gpio69_M2.dir = OUTPUT_PIN;
    GPIO_Init(&gpio69_M2);
    gpio_set_value(&gpio69_M2, LOW);

    gpio65_M3.pin = 65;
    gpio65_M3.dir = OUTPUT_PIN;
    GPIO_Init(&gpio65_M3);
    gpio_set_value(&gpio65_M3,LOW);

    gpio68_M4.pin = 68;
    gpio68_M4.dir = OUTPUT_PIN;
    GPIO_Init(&gpio68_M4);
    gpio_set_value(&gpio68_M4,LOW);

    /* 控制气泵的PWM波相关配置 */
    strcpy(pwm_8_13_airpump.name,"pwm_test_P8_13.11");
    pwm_8_13_airpump.duty = PWM_period_airpump;//默认duty等于周期，表示气泵全速运转;duty越大转速越快，duty取值范围0-PWM_period_airpump.
    pwm_8_13_airpump.period = PWM_period_airpump;//PWM波周期
    pwm_8_13_airpump.polarity = 1;//控制气泵的PWM波采用负逻辑
    PWM_Init(&pwm_8_13_airpump);

    /* 控制反应室加热带加热功率的相关配置 */
    strcpy(pwm_9_22_fanyingshi.name,"pwm_test_P9_22.12");
    pwm_9_22_fanyingshi.duty = 0;//duty越大一个周期加热时间越长，温升越快，duty取值范围0-PWM_period_heater
    pwm_9_22_fanyingshi.period = PWM_period_heater;//PWM波周期
    pwm_9_22_fanyingshi.polarity = 0;//采用正逻辑
    PWM_Init(&pwm_9_22_fanyingshi);

    /* 控制蒸发室加热带加热功率的相关配置 */
    strcpy(pwm_9_42_zhenfashi.name,"pwm_test_P9_42.13");
    pwm_9_42_zhenfashi.duty = 0;
    pwm_9_42_zhenfashi.period = PWM_period_heater;
    pwm_9_42_zhenfashi.polarity = 0;
    PWM_Init(&pwm_9_42_zhenfashi);

    /* 载入所有相关的恒温控制参数 */
    pid_Init();

    /* 片选线用gpio31, 修改采样程序后，不在需要SPI通信 */
//    tlc1543_Init(31);

}

//Switch = HIGH,表示打开;Switch = LOW,表示关闭
void Beep_Switch(int Switch)
{
    gpio_set_value(&gpio47_Beep,Switch);
}
void Heat_S_Switch(int Switch)
{
    gpio_set_value(&gpio46_Heat_S,Switch);
}
void Pump_S_Switch(int Switch)
{
    gpio_set_value(&gpio27_Pump_S,Switch);
}
void M1_Switch(int Switch)
{
    gpio_set_value(&gpio67_M1,Switch);
}
void M2_Switch(int Switch)
{
    gpio_set_value(&gpio69_M2, Switch);
}
void M3_Switch(int Switch)
{
    gpio_set_value(&gpio65_M3,Switch);
}
void M4_Switch(int Switch)
{
    gpio_set_value(&gpio68_M4,Switch);
}

void Application_quit(unsigned int seconds)
{
    pid_t new_pid;
    char *const arg_shutdown[] = {"shutdown","-h","now",NULL};

    new_pid = fork();

    if(new_pid < 0)
        printf("fork() failed!\n");
    else if(new_pid == 0)//子进程运行内容
    {
        printf("System poweroff in %d seconds!\n",seconds);
        sleep(seconds);//seconds秒钟后关机
        if(execve("/sbin/shutdown", arg_shutdown, NULL) == -1)
        {
            printf("execve() failed!\n");
        }
    }
    else//主进程运行内容
    {
        printf("Application quit!\n");
    }
}

/* 操作结束前，
 * 关闭各个功能硬件电路，恢复系统配置；
 */
void close_hardware(void)
{
    /* 关闭tlc1543, , 修改采样程序后，不在需要SPI通信 */
//    tlc1543_Close();

    /* 关闭各个PWM输出 */
    pwm_close(&pwm_8_13_airpump);
    pwm_close(&pwm_9_22_fanyingshi);
    pwm_close(&pwm_9_42_zhenfashi);

    /* 断开加热电路，安保措施 */
    Heat_S_Switch(LOW);

    /* 安保措施 */
    Beep_Switch(LOW);

    /* 断开气泵电源 */
    Pump_S_Switch(LOW);

    /* 控制相应的电磁阀 */
    M1_Switch(LOW);
    M2_Switch(LOW);
    M3_Switch(LOW);
    M4_Switch(LOW);

    /* 关闭各个IO口 */
//    GPIO_Close(&gpio47_Beep);
//    GPIO_Close(&gpio46_Heat_S);
//    GPIO_Close(&gpio27_Pump_S);
//    GPIO_Close(&gpio67_M1);
//    GPIO_Close(&gpio69_M2);
//    GPIO_Close(&gpio65_M3);
//    GPIO_Close(&gpio68_M4);
}
