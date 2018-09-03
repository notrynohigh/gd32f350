/*************************************************************************************************
 *                                     Trochili RTOS Kernel                                      *
 *                                  Copyright(C) 2017 LIUXUMING                                  *
 *                                       www.trochili.com                                        *
 *************************************************************************************************/
#include "tcl.types.h"
#include "tcl.config.h"
#include "tcl.cpu.h"

/* SysTick Ctrl & Status Reg.          */
#define CM3_SYSTICK_CTRL     (0xE000E010)
#define CM3_SYSTICK_CLKSRC   (0x00000004)   /* Clock Source.                    */
#define CM3_SYSTICK_INTEN    (0x00000002)   /* Interrupt enable.                */
#define CM3_SYSTICK_ENABLE   (0x00000001)   /* Counter mode.                    */

/* SysTick Reload  Value Reg.          */
#define CM3_SYSTICK_RELOAD   (0xE000E014)

/* SysTick Current Value Reg.          */
#define CM3_SYSTICK_CURRENT  (0xE000E018)

/* SysTick Cal     Value Reg.          */
#define CM3_SYSTICK_TENMS    (0xE000E01C)

/* Interrupt control & state register. */
#define CM3_ICSR             (0xE000ED04)
#define CM3_ICSR_PENDSVSET   (0x1<<28)       /* Value to trigger PendSV exception.  */
#define CM3_ICSR_PENDSVCLR   (0x1<<27)       /* Value to clear PendSV exception.    */
#define CM3_ICSR_PENDSTSET   (0x1<<26)       /* Value to trigger PendST exception.  */
#define CM3_ICSR_PENDSTCLR   (0x1<<25)       /* Value to clear PendST exception.    */

/* PendSV priority register            */
#define CM3_PRIO_PENDSV      (0xE000ED22)
#define CM3_PENDSV_PRIORITY  (0xFF)


/*************************************************************************************************
 *  功能：启动内核节拍定时器                                                                     *
 *  参数：无                                                                                     *
 *  返回：无                                                                                     *
 *  说明：                                                                                       *
 *************************************************************************************************/
void OsCpuStartTickClock(void)
{
    /* 初始化systick定时器 */
    TBase32 value = TCLC_CPU_CLOCK_FREQ / TCLC_TIME_TICK_RATE;
    TCLM_SET_REG32(CM3_SYSTICK_RELOAD, value - 1U);
    TCLM_SET_REG32(CM3_SYSTICK_CTRL, CM3_SYSTICK_CLKSRC|CM3_SYSTICK_INTEN|CM3_SYSTICK_ENABLE);
}

/*************************************************************************************************
 *  功能：内核加载第一个线程                                                                     *
 *  参数：无                                                                                     *
 *  返回：无                                                                                     *
 *  说明：                                                                                       *
 *************************************************************************************************/
void OsCpuLoadRootThread()
{
    TCLM_SET_REG32(CM3_ICSR, CM3_ICSR_PENDSVSET);
}


/*************************************************************************************************
 *  功能：请求线程调度                                                                           *
 *  参数：无                                                                                     *
 *  返回：无                                                                                     *
 *  说明：                                                                                       *
 *************************************************************************************************/
void OsCpuConfirmThreadSwitch(void)
{
    TCLM_SET_REG32(CM3_ICSR, CM3_ICSR_PENDSVSET);
}


/*************************************************************************************************
 *  功能：取消线程调度                                                                           *
 *  参数：无                                                                                     *
 *  返回：无                                                                                     *
 *  说明：                                                                                       *
 *************************************************************************************************/
void OsCpuCancelThreadSwitch(void)
{
    TCLM_SET_REG32(CM3_ICSR, CM3_ICSR_PENDSVCLR);
}


/*************************************************************************************************
 *  功能：线程栈和栈帧初始化函数                                                                 *
 *  参数：(1) pTop      线程栈顶地址                                                             *
 *        (2) pStack    线程栈底地址                                                             *
 *        (3) bytes     线程栈大小，以节为单位                                                   *
 *        (4) pEntry    线程函数地址                                                             *
 *        (5) argument  线程函数参数                                                             *
 *  返回：无                                                                                     *
 *  说明：线程栈起始地址必须4字节对齐                                                            *
 *************************************************************************************************/
void OsCpuBuildThreadStack(TAddr32* pTop, void* pStack, TBase32 bytes,
                           void* pEntry, TArgument argument)
{
    TBase32* pTemp;
    pTemp = (TBase32*)((TBase32)pStack + bytes);

    /* 伪造处理器中断栈现场，在线程第一次被加载运行时使用。
       注意LR的值是个非法值，这决定了线程没法通过LR退出 */
    *(--pTemp) = (TBase32)0x01000000;    /* PSR                     */
    *(--pTemp) = (TBase32)pEntry;        /* 线程函数                */
    *(--pTemp) = (TBase32)0xFFFFFFFE;    /* R14 (LR)                */
    *(--pTemp) = (TBase32)0x12121212;    /* R12                     */
    *(--pTemp) = (TBase32)0x03030303;    /* R3                      */
    *(--pTemp) = (TBase32)0x02020202;    /* R2                      */
    *(--pTemp) = (TBase32)0x01010101;    /* R1                      */
    *(--pTemp) = (TBase32)argument;      /* R0, 线程参数            */

    /* 初始化在处理器硬件中断时不会自动保存的线程上下文，
       这几个寄存器数值没有什么意义,就算内核的指纹吧 */
    *(--pTemp) = (TBase32)0x00000054;    /* R11 ,T                  */
    *(--pTemp) = (TBase32)0x00000052;    /* R10 ,R                  */
    *(--pTemp) = (TBase32)0x0000004F;    /* R9  ,O                  */
    *(--pTemp) = (TBase32)0x00000043;    /* R8  ,C                  */
    *(--pTemp) = (TBase32)0x00000048;    /* R7  ,H                  */
    *(--pTemp) = (TBase32)0x00000049;    /* R6  ,I                  */
    *(--pTemp) = (TBase32)0x0000004C;    /* R5  ,L                  */
    *(--pTemp) = (TBase32)0x00000049;    /* R4  ,I                  */

    *pTop = (TBase32)pTemp;
}


/*************************************************************************************************
 *  功能：初始化处理器                                                                           *
 *  参数：无                                                                                     *
 *  返回：无                                                                                     *
 *  说明：                                                                                       *
 *************************************************************************************************/
void OsCpuSetupEntry(void)
{
    /* 配置PENDSV中断优先级 */
    TCLM_SET_REG32(CM3_PRIO_PENDSV, CM3_PENDSV_PRIORITY);
}


void OsCpuHardFaultEntry(unsigned int * hardfault_args)
{
    while(1)
    {
        ;;
    }
}
