/*
 *  参考drivers\mtd\nand\S3c2410.c
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/partitions.h>

#include <asm/io.h>

#include <plat/regs-nand.h>
#include <plat/nand.h>

struct s3c_nand_regs {
	unsigned int nfconf      ;
	unsigned int nfcont     ;
	unsigned int nfcmd      ;
	unsigned int nfaddr     ;
	unsigned int nfdata     ;
	unsigned int nfeccd0    ;
	unsigned int nfeccd1    ;
	unsigned int nfeccd     ;
	unsigned int nfstat     ;
	unsigned int nfestat0   ;
	unsigned int nfestat1   ;
	unsigned int nfmecc0    ;
	unsigned int nfmecc1    ;
	unsigned int nfsecc     ;
	unsigned int nfsblk     ;
	unsigned int nfeblk     ;

};

static struct mtd_partition s3c_nand_parts[] = {
	[0] = {
		.name	= "supervivi",
		.size	= 0x00040000,
		.offset	= 0,
	},
	[1] = {
		.name	= "param",
		.offset = 0x00040000,
		.size	= 0x00020000,
	},
	[2] = {
		.name	= "Kernel",
		.offset = 0x00060000,
		.size	= 0x00500000,
	},
	[3] = {
		.name	= "root",
		.offset = 0x00560000,
		.size	= 1024 * 1024 * 1024, //
	},
	[4] = {
		.name	= "nand",
		.offset = 0x00000000,
		.size	= 1024 * 1024 * 1024, //
	}
};

static struct nand_chip *s3c_nand_chip;
static struct mtd_info  *s3c_mtd_info;
static struct s3c_nand_regs *s3c_nand_regs;


static void s3c2440_select_nand_chip(struct mtd_info *mtd, int chip)
{
	if(chip == -1)
	/**	NFCOND[0] = 1 Disable chip select**/
		s3c_nand_regs->nfcont |= (1<<1); 
	else
	/***NFCOND[0] = 0  Enable chip select**/
		s3c_nand_regs->nfcont &= ~(1<<1); 
}

static void s3c2440_cmd_ctrl(struct mtd_info *mtd, int dat, unsigned int ctrl)
{
	if (ctrl == NAND_CMD_NONE)
		return;

	if (ctrl & NAND_CLE)
		/*写命令NFCMD[7:0]*/
		s3c_nand_regs->nfcmd = dat & 0xFF;
	else
		/*写地址NFADDR[7:0]*/
		s3c_nand_regs->nfaddr = dat & 0xFF;
}

static int s3c2440_device_ready(struct mtd_info *mtd)
{
	return (s3c_nand_regs->nfstat & 0x01);
}

static int s3c_nand_init(void)
{
	struct clk *nand_clk;
	/*1: 分配一个nand_chip结构体 */
	s3c_nand_chip = kzalloc(sizeof(struct nand_chip), GFP_KERNEL);
	s3c_nand_regs  = ioremap(0x4E000000, sizeof(struct s3c_nand_regs));
	/*2 设置 nand_chip
	  * 设置nand_chip是给nand_scan，参考nand_scan函数
	  * 它应该提供选中、发命令、发地址 、 发数据、读状态等
	  */
	s3c_nand_chip->select_chip = s3c2440_select_nand_chip;
	s3c_nand_chip->cmd_ctrl     = s3c2440_cmd_ctrl;
	s3c_nand_chip->IO_ADDR_R = &s3c_nand_regs->nfdata;
	s3c_nand_chip->IO_ADDR_W = &s3c_nand_regs->nfdata;
	s3c_nand_chip->dev_ready   = s3c2440_device_ready;
	s3c_nand_chip->ecc.mode     = NAND_ECC_SOFT;
	/* 3     硬件相关的设置  */
	/*3.1 使能nand flash 控制器时钟*/
	nand_clk = clk_get(NULL, "nand");
	clk_enable(nand_clk); //enable CLKCON[4] = 1
	
	/* 3.2 设置nand flash时间参数，参考nand flash和arm手册*/
#define TACLS	0
#define TWRPH0	1
#define TWRPH1	0

	s3c_nand_regs->nfconf =(TACLS<<12) | (TWRPH0<<8) | (TWRPH1<<4); 

	/* 设置NFCONT
	 * bit[1] = 1 Disable chip select
	 * bit[0] = 1 NAND flash controller enable
	 */
	s3c_nand_regs->nfcont = (1<<1) | (1<<0);

	
	/*4 使用nand_scan*/
	s3c_mtd_info = kzalloc(sizeof(struct mtd_info), GFP_KERNEL);
	s3c_mtd_info->owner = THIS_MODULE;
	s3c_mtd_info->priv    = s3c_nand_chip;

	nand_scan(s3c_mtd_info, 1); // one chip


	/*add_mtd_patition*/
	add_mtd_partitions(s3c_mtd_info, s3c_nand_parts, 4);
	return 0;
}

static void s3c_nand_exit(void)
{
	del_mtd_partitions(s3c_mtd_info);
	kfree(s3c_mtd_info);
	iounmap(s3c_nand_regs);
	kfree(s3c_nand_chip);
}

module_init(s3c_nand_init);
module_exit(s3c_nand_exit);
MODULE_LICENSE("GPL");


//mount -o nolock,rw -t nfs 192.168.1.102:/home/share /mnt

