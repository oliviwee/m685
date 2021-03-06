/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2010-2016, FocalTech Systems, Ltd., all rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

 /************************************************************************
*
* File Name: focaltech_test.c
*
* Author:	  Software Department, FocalTech
*
* Created: 2016-03-24
*
* Modify:
*
* Abstract: create char device and proc node for  the comm between APK and TP
*
************************************************************************/

/*******************************************************************************
* Included header files
*******************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <asm/uaccess.h>

#include <linux/i2c.h>//iic
#include <linux/delay.h>//msleep

#include "focaltech_comm.h"
#include "focaltech_test_main.h"
#include "focaltech_test_ini.h"
#include "focaltech_core.h"

/*******************************************************************************
* Private constant and macro definitions using #define
*******************************************************************************/
#define FOCALTECH_TEST_INFO  "File Version of  focaltech_test.c:  V1.0.0 2016-03-24"
#define FTS_INI_FILE_PATH "/data/data/"
#define FTS_TEST_BUFFER_SIZE		80*1024
#define FTS_TEST_PRINT_SIZE		128

int fts_test_i2c_read(unsigned char *writebuf, int writelen, unsigned char *readbuf, int readlen)
{
	int iret = -1;
	#if 1
	iret = fts_i2c_read(fts_i2c_client, writebuf, writelen, readbuf, readlen);
	#else
	iret = fts_i2c_read(writebuf, writelen, readbuf, readlen);
	#endif

	return iret;

}

int fts_test_i2c_write(unsigned char *writebuf, int writelen)
{
	int iret = -1;
	#if 1
	//	old fts_i2c_write function.  need to set fts_i2c_client.
	//修改成此项目用到的i2c_write函数
	iret = fts_i2c_write(fts_i2c_client, writebuf, writelen);
	#else
	iret = fts_i2c_write(writebuf, writelen);
	#endif

	return iret;
}

//获取配置文件大小, 用于分配内存读取配置
int fts_test_get_ini_size(char *config_name)
{
	struct file *pfile = NULL;
	struct inode *inode = NULL;
	//unsigned long magic;
	off_t fsize = 0;
	char filepath[128];
	memset(filepath, 0, sizeof(filepath));

	sprintf(filepath, "%s%s", FTS_INI_FILE_PATH, config_name);

	if (NULL == pfile)
		pfile = filp_open(filepath, O_RDONLY, 0);

	if (IS_ERR(pfile)) {
		pr_emerg("error occured while opening file %s.",  filepath);
		return -EIO;
	}

	inode = pfile->f_dentry->d_inode;
	//magic = inode->i_sb->s_magic;
	fsize = inode->i_size;
	filp_close(pfile, NULL);

	return fsize;
}
//读取配置到内存
int fts_test_read_ini_data(char *config_name, char *config_buf)
{
	struct file *pfile = NULL;
	struct inode *inode = NULL;
	//unsigned long magic;
	off_t fsize = 0;
	char filepath[128];
	loff_t pos = 0;
	mm_segment_t old_fs;

	memset(filepath, 0, sizeof(filepath));
	sprintf(filepath, "%s%s", FTS_INI_FILE_PATH, config_name);
	if (NULL == pfile)
		pfile = filp_open(filepath, O_RDONLY, 0);
	if (IS_ERR(pfile)) {
		pr_emerg("error occured while opening file %s.",  filepath);
		return -EIO;
	}

	inode = pfile->f_dentry->d_inode;
	//magic = inode->i_sb->s_magic;
	fsize = inode->i_size;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_read(pfile, config_buf, fsize, &pos);
	filp_close(pfile, NULL);
	set_fs(old_fs);

	return 0;
}
//保存测试数据到SD卡 etc.
int fts_test_save_test_data(char *file_name, char *data_buf, int iLen)
{
	struct file *pfile = NULL;

	char filepath[128];
	loff_t pos;
	mm_segment_t old_fs;

	memset(filepath, 0, sizeof(filepath));
	sprintf(filepath, "%s%s", FTS_INI_FILE_PATH, file_name);
	if (NULL == pfile)
		pfile = filp_open(filepath, O_CREAT|O_RDWR, 0);
	if (IS_ERR(pfile)) {
		pr_emerg("error occured while opening file %s.",  filepath);
		return -EIO;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_write(pfile, data_buf, iLen, &pos);
	filp_close(pfile, NULL);
	set_fs(old_fs);

	return 0;
}

//读取,解析配置文件,初始化测试变量
int fts_test_get_testparam_from_ini(char *config_name)
{
	char *filedata = NULL;
	int ret = 0;

	int inisize = fts_test_get_ini_size(config_name);

	pr_emerg("ini_size = %d ", inisize);
	if (inisize <= 0) {
		pr_emerg("%s ERROR:Get firmware size failed",  __func__);
		return -EIO;
	}

	filedata = kmalloc(inisize + 1, GFP_ATOMIC);

	if (fts_test_read_ini_data(config_name, filedata)) {
		pr_emerg("%s() - ERROR: request_firmware failed",  __func__);
		kfree(filedata);
		return -EIO;
	} else {
		pr_emerg("fts_test_read_ini_data successful");
	}

	ret = set_param_data(filedata);
	if(ret < 0)
		return ret;

	return 0;
}

/////////////////////////////////
//测试库调用总入口
///////////////////////////////////
int fts_test_entry(char *ini_file_name)
{
	/* place holder for future use */
    char cfgname[128];
	char *testdata = NULL;
	char *printdata = NULL;
	int iTestDataLen=0;//库中测试数据实际长度,用于保存到文件
	int ret = 0;
	int icycle = 0, i =0;
	int print_index = 0;


	pr_emerg("");
	/*用于获取存放在库中的测试数据,注意分配空间大小.*/
	pr_emerg("Allocate memory, size: %d", FTS_TEST_BUFFER_SIZE);
	testdata = kmalloc(FTS_TEST_BUFFER_SIZE, GFP_ATOMIC);
	if(NULL == testdata)
	{
		//printk("kmalloc failed in function:%s",  __func__);
		pr_emerg("kmalloc failed in function:%s",  __func__);
		return -1;
	}
	printdata = kmalloc(FTS_TEST_PRINT_SIZE, GFP_ATOMIC);
	if(NULL == printdata)
	{
		//printk("kmalloc failed in function:%s",  __func__);
		pr_emerg("kmalloc failed in function:%s",  __func__);
		return -1;
	}
	/*初始化平台相关的I2C读写函数*/

	#if 0
	init_i2c_write_func(fts_i2c_write);
	init_i2c_read_func(fts_i2c_read);
	#else
	init_i2c_write_func(fts_test_i2c_write);
	init_i2c_read_func(fts_test_i2c_read);
	#endif

	/*初始化指针内存*/
	ret = focaltech_test_main_init();
	if(ret < 0)
	{
		pr_emerg("focaltech_test_main_init() error.");
		goto TEST_ERR;
	}

	/*读取解析配置文件*/
	memset(cfgname, 0, sizeof(cfgname));
	sprintf(cfgname, "%s", ini_file_name);
	pr_emerg("ini_file_name = %s", cfgname);
	if(fts_test_get_testparam_from_ini(cfgname) <0)
	{
		pr_emerg("get testparam from ini failure");
		goto TEST_ERR;
	}

	/*根据测试配置开始测试*/
	if(true == start_test_tp())
		pr_emerg("tp test pass");
	else
		pr_emerg("tp test failure");

	/*获取测试库中的测试数，并保存*/
	iTestDataLen = get_test_data(testdata);
	//pr_emerg("\n%s", testdata);

	icycle = 0;
	/*打印触摸数据包 */
	pr_emerg("print test data: \n");
	for(i = 0; i < iTestDataLen; i++)
	{
		if(('\0' == testdata[i])//遇到结束符
			||(icycle == FTS_TEST_PRINT_SIZE -2)//满足打印字符串长度要求
			||(i == iTestDataLen-1)//已是最后一个字符
		)
		{
			if(icycle == 0)
			{
				print_index++;
			}
			else
			{
				memcpy(printdata, testdata + print_index, icycle);
				printdata[FTS_TEST_PRINT_SIZE-1] = '\0';
				printk("%s", printdata);
				print_index += icycle;
				icycle = 0;
			}
		}
		else
		{
			icycle++;
		}
	}
	printk("\n");
	fts_test_save_test_data("testdata.csv", testdata, iTestDataLen);

	/*释放内存等... */
	focaltech_test_main_exit();

	//mutex_unlock(&g_device_mutex);
	if(NULL != testdata) kfree(testdata);
	if(NULL != printdata) kfree(printdata);
	return 0;

TEST_ERR:
	if(NULL != testdata) kfree(testdata);
	if(NULL != printdata) kfree(printdata);
	return -1;
}
