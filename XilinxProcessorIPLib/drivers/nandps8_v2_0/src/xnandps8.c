/******************************************************************************
*
* Copyright (C) 2014 Xilinx, Inc. All rights reserved.
*
* This file contains confidential and proprietary information  of Xilinx, Inc.
* and is protected under U.S. and  international copyright and other
* intellectual property  laws.
*
* DISCLAIMER
* This disclaimer is not a license and does not grant any  rights to the
* materials distributed herewith. Except as  otherwise provided in a valid
* license issued to you by  Xilinx, and to the maximum extent permitted by
* applicable law: (1) THESE MATERIALS ARE MADE AVAILABLE "AS IS" AND  WITH ALL
* FAULTS, AND XILINX HEREBY DISCLAIMS ALL WARRANTIES  AND CONDITIONS, EXPRESS,
* IMPLIED, OR STATUTORY, INCLUDING  BUT NOT LIMITED TO WARRANTIES OF
* MERCHANTABILITY, NON-  INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE
* and
* (2) Xilinx shall not be liable (whether in contract or tort,  including
* negligence, or under any other theory of liability) for any loss or damage
* of any kind or nature  related to, arising under or in connection with these
* materials, including for any direct, or any indirect,  special, incidental,
* or consequential loss or damage  (including loss of data, profits,
* goodwill, or any type of  loss or damage suffered as a result of any
* action brought  by a third party) even if such damage or loss was
* reasonably foreseeable or Xilinx had been advised of the  possibility
* of the same.
*
* CRITICAL APPLICATIONS
* Xilinx products are not designed or intended to be fail-  safe, or for use
* in any application requiring fail-safe  performance, such as life-support
* or safety devices or  systems, Class III medical devices, nuclear
* facilities,  applications related to the deployment of airbags, or any
* other applications that could lead to death, personal  injury, or severe
* property or environmental damage  (individually and collectively,
* "Critical  Applications"). Customer assumes the sole risk and  liability
* of any use of Xilinx products in Critical  Applications, subject only to
* applicable laws and  regulations governing limitations on product liability.
*
* THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS  PART
* OF THIS FILE AT ALL TIMES.
*
******************************************************************************/

/*****************************************************************************/
/**
*
* @file xnandps8.c
*
* This file contains the implementation of the interface functions for
* XNandPs8 driver. Refer to the header file xnandps8.h for more detailed
* information.
*
* This module supports for NAND flash memory devices that conform to the
* "Open NAND Flash Interface" (ONFI) 3.0 Specification. This modules
* implements basic flash operations like read, write and erase.
*
* @note		None
*
* <pre>
* MODIFICATION HISTORY:
*
* Ver   Who    Date	   Changes
* ----- ----   ----------  -----------------------------------------------
* 1.0   nm     05/06/2014  First release
* 2.0   sb     12/19/2014  Removed Null checks for Buffer passed
*			   as parameter to Read API's
*			   - XNandPs8_Read()
*			   - XNandPs8_ReadPage
*			   Modified
*			   - XNandPs8_SetFeature()
*			   - XNandPs8_GetFeature()
*			   and made them public.
*			   Removed Failure Return for BCF Error check in
*			   XNandPs8_ReadPage() and added BCH_Error counter
*			   in the instance pointer structure.
* 			   Added XNandPs8_Prepare_Cmd API
*			   Replaced
*			   - XNandPs8_IntrStsEnable
*			   - XNandPs8_IntrStsClear
*			   - XNandPs8_IntrClear
*			   - XNandPs8_SetProgramReg
*			   with XNandPs8_WriteReg call
*			   Modified xnandps8.c file API's with above changes.
*  			   Corrected the program command for Set Feature API.
*			   Modified
*			   - XNandPs8_OnfiReadStatus
*			   - XNandPs8_GetFeature
*			   - XNandPs8_SetFeature
*			   to add support for DDR mode.
*			   Changed Convention for SLC/MLC
*			   SLC --> HAMMING
*			   MLC --> BCH
*			   SlcMlc --> IsBCH
*			   Removed extra DMA mode initialization from
*			   the XNandPs8_CfgInitialize API.
*			   Modified
*			   - XNandPs8_SetEccAddrSize
*			   ECC address now is calculated based upon the
*			   size of spare area
*			   Modified Block Erase API, removed clearing of
*			   packet register before erase.
* </pre>
*
******************************************************************************/

/***************************** Include Files *********************************/
#include "xnandps8.h"
#include "xnandps8_bbm.h"
/************************** Constant Definitions *****************************/

const static XNandPs8_TimingModeDesc TimingDesc[] = {
		/*
		 * SDR to SDR
		 */
		{SDR, SDR, SDR0, 0U, 0x00000000U},
		{SDR, SDR, SDR1, 0U, 0x00000001U},
		{SDR, SDR, SDR2, 0U, 0x00000002U},
		{SDR, SDR, SDR3, 0U, 0x00000003U},
		{SDR, SDR, SDR4, 0U, 0x00000004U},
		{SDR, SDR, SDR5, 0U, 0x00000005U},
		/*
		 * NVDDR to NVDDR
		 */
		{NVDDR, NVDDR, NVDDR0, NVDDR_CLK_0, 0x00001010U},
		{NVDDR, NVDDR, NVDDR1, NVDDR_CLK_1, 0x00001111U},
		{NVDDR, NVDDR, NVDDR2, NVDDR_CLK_2, 0x00001212U},
		{NVDDR, NVDDR, NVDDR3, NVDDR_CLK_3, 0x00001313U},
		{NVDDR, NVDDR, NVDDR4, NVDDR_CLK_4, 0x00001414U},
		{NVDDR, NVDDR, NVDDR5, NVDDR_CLK_5, 0x00001515U},
		/*
		 * SDR to NVDDR
		 */
		{SDR, NVDDR, NVDDR0, NVDDR_CLK_0, 0x00000010U},
		{SDR, NVDDR, NVDDR1, NVDDR_CLK_1, 0x00000011U},
		{SDR, NVDDR, NVDDR2, NVDDR_CLK_2, 0x00000012U},
		{SDR, NVDDR, NVDDR3, NVDDR_CLK_3, 0x00000013U},
		{SDR, NVDDR, NVDDR4, NVDDR_CLK_4, 0x00000014U},
		{SDR, NVDDR, NVDDR5, NVDDR_CLK_5, 0x00000015U},
		/*
		 * NVDDR to SDR
		 */
		{NVDDR, SDR, SDR0, SDR_CLK, 0U},
};

const XNandPs8_EccMatrix EccMatrix[] = {
	/*
	 * 512 byte page
	 */
	{XNANDPS8_PAGE_SIZE_512, 9U, 1U, XNANDPS8_HAMMING, 0x20DU, 0x3U},
	{XNANDPS8_PAGE_SIZE_512, 9U, 4U, XNANDPS8_BCH, 0x209U, 0x7U},
	{XNANDPS8_PAGE_SIZE_512, 9U, 8U, XNANDPS8_BCH, 0x203U, 0xDU},
	/*
	 * 2K byte page
	 */
	{XNANDPS8_PAGE_SIZE_2K, 9U, 1U, XNANDPS8_HAMMING, 0x834U, 0xCU},
	{XNANDPS8_PAGE_SIZE_2K, 9U, 4U, XNANDPS8_BCH, 0x826U, 0x1AU},
	{XNANDPS8_PAGE_SIZE_2K, 9U, 8U, XNANDPS8_BCH, 0x80cU, 0x34U},
	{XNANDPS8_PAGE_SIZE_2K, 9U, 12U, XNANDPS8_BCH, 0x822U, 0x4EU},
	{XNANDPS8_PAGE_SIZE_2K, 9U, 16U, XNANDPS8_BCH, 0x808U, 0x68U},
	{XNANDPS8_PAGE_SIZE_2K, 10U, 24U, XNANDPS8_BCH, 0x81cU, 0x54U},
	/*
	 * 4K byte page
	 */
	{XNANDPS8_PAGE_SIZE_4K, 9U, 1U, XNANDPS8_HAMMING, 0x1068U, 0x18U},
	{XNANDPS8_PAGE_SIZE_4K, 9U, 4U, XNANDPS8_BCH, 0x104cU, 0x34U},
	{XNANDPS8_PAGE_SIZE_4K, 9U, 8U, XNANDPS8_BCH, 0x1018U, 0x68U},
	{XNANDPS8_PAGE_SIZE_4K, 9U, 12U, XNANDPS8_BCH, 0x1044U, 0x9CU},
	{XNANDPS8_PAGE_SIZE_4K, 9U, 16U, XNANDPS8_BCH, 0x1010U, 0xD0U},
	{XNANDPS8_PAGE_SIZE_4K, 10U, 24U, XNANDPS8_BCH, 0x1038U, 0xA8U},
	/*
	 * 8K byte page
	 */
	{XNANDPS8_PAGE_SIZE_8K, 9U, 1U, XNANDPS8_HAMMING, 0x20d0U, 0x30U},
	{XNANDPS8_PAGE_SIZE_8K, 9U, 4U, XNANDPS8_BCH, 0x2098U, 0x68U},
	{XNANDPS8_PAGE_SIZE_8K, 9U, 8U, XNANDPS8_BCH, 0x2030U, 0xD0U},
	{XNANDPS8_PAGE_SIZE_8K, 9U, 12U, XNANDPS8_BCH, 0x2088U, 0x138U},
	{XNANDPS8_PAGE_SIZE_8K, 9U, 16U, XNANDPS8_BCH, 0x2020U, 0x1A0U},
	{XNANDPS8_PAGE_SIZE_8K, 10U, 24U, XNANDPS8_BCH, 0x2070U, 0x150U},
	/*
	 * 16K byte page
	 */
	{XNANDPS8_PAGE_SIZE_16K, 9U, 1U, XNANDPS8_HAMMING, 0x4460U, 0x60U},
	{XNANDPS8_PAGE_SIZE_16K, 9U, 4U, XNANDPS8_BCH, 0x43f0U, 0xD0U},
	{XNANDPS8_PAGE_SIZE_16K, 9U, 8U, XNANDPS8_BCH, 0x4320U, 0x1A0U},
	{XNANDPS8_PAGE_SIZE_16K, 9U, 12U, XNANDPS8_BCH, 0x4250U, 0x270U},
	{XNANDPS8_PAGE_SIZE_16K, 9U, 16U, XNANDPS8_BCH, 0x4180U, 0x340U},
	{XNANDPS8_PAGE_SIZE_16K, 10U, 24U, XNANDPS8_BCH, 0x4220U, 0x2A0U}
};

/**************************** Type Definitions *******************************/
static u8 isQemuPlatform = 0U;
/***************** Macros (Inline Functions) Definitions *********************/

/************************** Function Prototypes ******************************/

static s32 XNandPs8_FlashInit(XNandPs8 *InstancePtr);

static void XNandPs8_InitGeometry(XNandPs8 *InstancePtr, OnfiParamPage *Param);

static void XNandPs8_InitFeatures(XNandPs8 *InstancePtr, OnfiParamPage *Param);

static s32 XNandPs8_PollRegTimeout(XNandPs8 *InstancePtr, u32 RegOffset,
					u32 Mask, u32 Timeout);

static void XNandPs8_SetPktSzCnt(XNandPs8 *InstancePtr, u32 PktSize,
						u32 PktCount);

static void XNandPs8_SetPageColAddr(XNandPs8 *InstancePtr, u32 Page, u16 Col);

static void XNandPs8_SetPageSize(XNandPs8 *InstancePtr);

static void XNandPs8_SetBusWidth(XNandPs8 *InstancePtr);

static void XNandPs8_SelectChip(XNandPs8 *InstancePtr, u32 Target);

static s32 XNandPs8_OnfiReset(XNandPs8 *InstancePtr, u32 Target);

static s32 XNandPs8_OnfiReadStatus(XNandPs8 *InstancePtr, u32 Target,
							u16 *OnfiStatus);

static s32 XNandPs8_OnfiReadId(XNandPs8 *InstancePtr, u32 Target, u8 IdAddr,
							u32 IdLen, u8 *Buf);

static s32 XNandPs8_OnfiReadParamPage(XNandPs8 *InstancePtr, u32 Target,
						u8 *Buf);

static s32 XNandPs8_ProgramPage(XNandPs8 *InstancePtr, u32 Target, u32 Page,
							u32 Col, u8 *Buf);

static s32 XNandPs8_ReadPage(XNandPs8 *InstancePtr, u32 Target, u32 Page,
							u32 Col, u8 *Buf);

static s32 XNandPs8_CheckOnDie(XNandPs8 *InstancePtr, OnfiParamPage *Param);

static void XNandPs8_SetEccAddrSize(XNandPs8 *InstancePtr);

static s32 XNandPs8_ChangeReadColumn(XNandPs8 *InstancePtr, u32 Target,
					u32 Col, u32 PktSize, u32 PktCount,
					u8 *Buf);

static s32 XNandPs8_ChangeWriteColumn(XNandPs8 *InstancePtr, u32 Target,
					u32 Col, u32 PktSize, u32 PktCount,
					u8 *Buf);

static s32 XNandPs8_InitExtEcc(XNandPs8 *InstancePtr, OnfiExtPrmPage *ExtPrm);

/*****************************************************************************/
/**
*
* This function initializes a specific XNandPs8 instance. This function must
* be called prior to using the NAND flash device to read or write any data.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
* @param	ConfigPtr points to XNandPs8 device configuration structure.
* @param	EffectiveAddr is the base address of NAND flash controller.
*
* @return
*		- XST_SUCCESS if successful.
*		- XST_FAILURE if fail.
*
* @note		The user needs to first call the XNandPs8_LookupConfig() API
*		which returns the Configuration structure pointer which is
*		passed as a parameter to the XNandPs8_CfgInitialize() API.
*
******************************************************************************/
s32 XNandPs8_CfgInitialize(XNandPs8 *InstancePtr, XNandPs8_Config *ConfigPtr,
				u32 EffectiveAddr)
{
	s32 Status = XST_FAILURE;

	/*
	 * Assert the input arguments.
	 */
	Xil_AssertNonvoid(InstancePtr != NULL);
	Xil_AssertNonvoid(ConfigPtr != NULL);

	/*
	 * Initialize InstancePtr Config structure
	 */
	InstancePtr->Config.DeviceId = ConfigPtr->DeviceId;
	InstancePtr->Config.BaseAddress = EffectiveAddr;
	/*
	 * Operate in Polling Mode
	 */
	InstancePtr->Mode = POLLING;
	/*
	 * Enable MDMA mode by default
	 */
	InstancePtr->DmaMode = MDMA;
	InstancePtr->IsReady = XIL_COMPONENT_IS_READY;

	/*
	 * Temporary hack for disabling the ecc on qemu as currently there
	 * is no support in the utility for writing images with ecc enabled.
	 */
	#define CSU_VER_REG 0xFFCA0044U
	#define CSU_VER_PLATFORM_MASK 0xF000U
	#define CSU_VER_PLATFORM_QEMU_VAL 0x3000U
	if ((*(u32 *)CSU_VER_REG & CSU_VER_PLATFORM_MASK) ==
					CSU_VER_PLATFORM_QEMU_VAL) {
		isQemuPlatform = 1U;
	}
	/*
	 * Initialize the NAND flash targets
	 */
	Status = XNandPs8_FlashInit(InstancePtr);
	if (Status != XST_SUCCESS) {
#ifdef XNANDPS8_DEBUG
		xil_printf("%s: Flash init failed\r\n",__func__);
#endif
		goto Out;
	}
	/*
	 * Set ECC mode
	 */
	if (InstancePtr->Features.EzNand != 0U) {
		InstancePtr->EccMode = EZNAND;
	} else if (InstancePtr->Features.OnDie != 0U) {
		InstancePtr->EccMode = ONDIE;
	} else {
		InstancePtr->EccMode = HWECC;
	}

	if (isQemuPlatform != 0U) {
		InstancePtr->EccMode = NONE;
		goto Out;
	}

	/*
	 * Initialize BCH Error counter
	 */
	 InstancePtr->BCH_Error_Status = 0U;

	/*
	 * Scan for the bad block table(bbt) stored in the flash & load it in
	 * memory(RAM).  If bbt is not found, create bbt by scanning factory
	 * marked bad blocks and store it in last good blocks of flash.
	 */
	XNandPs8_InitBbtDesc(InstancePtr);
	Status = XNandPs8_ScanBbt(InstancePtr);
	if (Status != XST_SUCCESS) {
#ifdef XNANDPS8_DEBUG
		xil_printf("%s: BBT scan failed\r\n",__func__);
#endif
		goto Out;
	}

Out:
	return Status;
}

/*****************************************************************************/
/**
*
* This function initializes the NAND flash and gets the geometry information.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
*
* @return
*		- XST_SUCCESS if successful.
*		- XST_FAILURE if failed.
*
* @note		None
*
******************************************************************************/
static s32 XNandPs8_FlashInit(XNandPs8 *InstancePtr)
{
	u32 Target;
	u8 Id[ONFI_SIG_LEN] = {0U};
	OnfiParamPage Param = {0U};
	s32 Status = XST_FAILURE;
	u32 Index;
	u32 Crc;
	u32 PrmPgOff;
	u32 PrmPgLen;
	OnfiExtPrmPage ExtParam __attribute__ ((aligned(64)));

	/*
	 * Assert the input arguments.
	 */
	Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

	for (Target = 0U; Target < XNANDPS8_MAX_TARGETS; Target++) {
		/*
		 * Reset the Target
		 */
		Status = XNandPs8_OnfiReset(InstancePtr, Target);
		if (Status != XST_SUCCESS) {
			goto Out;
		}
		/*
		 * Read ONFI ID
		 */
		Status = XNandPs8_OnfiReadId(InstancePtr, Target,
						ONFI_READ_ID_ADDR,
						ONFI_SIG_LEN,
						(u8 *)&Id[0]);
		if (Status != XST_SUCCESS) {
			goto Out;
		}

		if (!IS_ONFI(Id)) {
			if (Target == 0U) {
#ifdef XNANDPS8_DEBUG
				xil_printf("%s: ONFI ID doesn't match\r\n",
								__func__);
#endif
				Status = XST_FAILURE;
				goto Out;
			}
		}

		/* Read Parameter Page */
		for(Index = 0U; Index < ONFI_MND_PRM_PGS; Index++) {
			if (Index == 0U) {
				Status = XNandPs8_OnfiReadParamPage(InstancePtr,
							Target, (u8 *)&Param);
			} else {
				PrmPgOff = Index * ONFI_PRM_PG_LEN;
				PrmPgLen = ONFI_PRM_PG_LEN;
				Status = XNandPs8_ChangeReadColumn(InstancePtr,
							Target,PrmPgOff,
							ONFI_PRM_PG_LEN, 1U,
							(u8 *) &Param);
			}
			if (Status != XST_SUCCESS) {
				goto Out;
			}
			/* Check CRC */
			Crc = XNandPs8_OnfiParamPageCrc((u8*)&Param, 0U,
								ONFI_CRC_LEN);
			if (Crc != Param.Crc) {
#ifdef XNANDPS8_DEBUG
				xil_printf("%s: ONFI parameter page (%d) crc check failed\r\n",
							__func__, Index);
#endif
				continue;
			} else {
				break;
			}
		}
		if (Index >= ONFI_MND_PRM_PGS) {
			Status = XST_FAILURE;
			goto Out;
		}
		/* Fill Geometry for the first target */
		if (Target == 0U) {
			XNandPs8_InitGeometry(InstancePtr, &Param);
			XNandPs8_InitFeatures(InstancePtr, &Param);
			if ((!InstancePtr->Features.EzNand) != 0U) {
				Status =XNandPs8_CheckOnDie(InstancePtr,&Param);
				if (Status != XST_SUCCESS) {
					InstancePtr->Features.OnDie = 0U;
				}
			}
			if (isQemuPlatform != 0U) {
				InstancePtr->Geometry.NumTargets++;
				break;
			}
			if ((InstancePtr->Geometry.NumBitsECC == 0xFFU) &&
				(InstancePtr->Features.ExtPrmPage != 0U)) {
				/* ONFI 3.1 section 5.7.1.6 & 5.7.1.7 */
				PrmPgLen = (u32)Param.ExtParamPageLen * 16U;
					PrmPgOff = (u32)((u32)Param.NumOfParamPages *
							ONFI_PRM_PG_LEN) +
							(Index * (u32)PrmPgLen);
					Status = XNandPs8_ChangeReadColumn(
							InstancePtr,
							Target,
							PrmPgOff,
							PrmPgLen, 1U,
							(u8 *)(void *)&ExtParam);
					if (Status != XST_SUCCESS) {
						goto Out;
					}
					/*
					 * Check CRC
					 */
					Crc = XNandPs8_OnfiParamPageCrc(
							(u8 *)&ExtParam,
							2U,
							PrmPgLen);
					if (Crc != ExtParam.Crc) {
#ifdef XNANDPS8_DEBUG
	xil_printf("%s: ONFI extended parameter page (%d) crc check failed\r\n",
							__func__, Index);
#endif
						Status = XST_FAILURE;
						goto Out;
					}
					/*
					 * Initialize Extended ECC info
					 */
					Status = XNandPs8_InitExtEcc(
							InstancePtr,
							&ExtParam);
					if (Status != XST_SUCCESS) {
#ifdef XNANDPS8_DEBUG
	xil_printf("%s: Init extended ecc failed\r\n",__func__);
#endif
						goto Out;
				}
			}
			/* Configure ECC settings */
			XNandPs8_SetEccAddrSize(InstancePtr);
		}
		InstancePtr->Geometry.NumTargets++;
	}
	/*
	 * Calculate total number of blocks and total size of flash
	 */
	InstancePtr->Geometry.NumPages = InstancePtr->Geometry.NumTargets *
					InstancePtr->Geometry.NumTargetPages;
	InstancePtr->Geometry.NumBlocks = InstancePtr->Geometry.NumTargets *
					InstancePtr->Geometry.NumTargetBlocks;
	InstancePtr->Geometry.DeviceSize =
					(u64)InstancePtr->Geometry.NumTargets *
					InstancePtr->Geometry.TargetSize;

	Status = XST_SUCCESS;
Out:
	return Status;
}

/*****************************************************************************/
/**
*
* This function initializes the geometry information from ONFI parameter page.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
* @param	Param is pointer to the ONFI parameter page.
*
* @return
*		None
*
* @note		None
*
******************************************************************************/
static void XNandPs8_InitGeometry(XNandPs8 *InstancePtr, OnfiParamPage *Param)
{
	/*
	 * Assert the input arguments.
	 */
	Xil_AssertVoid(Param != NULL);

	InstancePtr->Geometry.BytesPerPage = Param->BytesPerPage;
	InstancePtr->Geometry.SpareBytesPerPage = Param->SpareBytesPerPage;
	InstancePtr->Geometry.PagesPerBlock = Param->PagesPerBlock;
	InstancePtr->Geometry.BlocksPerLun = Param->BlocksPerLun;
	InstancePtr->Geometry.NumLuns = Param->NumLuns;
	InstancePtr->Geometry.RowAddrCycles = Param->AddrCycles & 0xFU;
	InstancePtr->Geometry.ColAddrCycles = (Param->AddrCycles >> 4U) & 0xFU;
	InstancePtr->Geometry.NumBitsPerCell = Param->BitsPerCell;
	InstancePtr->Geometry.NumBitsECC = Param->EccBits;
	InstancePtr->Geometry.BlockSize = (Param->PagesPerBlock *
						Param->BytesPerPage);
	InstancePtr->Geometry.NumTargetBlocks = (Param->BlocksPerLun *
						(u32)Param->NumLuns);
	InstancePtr->Geometry.NumTargetPages = (Param->BlocksPerLun *
						(u32)Param->NumLuns *
						Param->PagesPerBlock);
	InstancePtr->Geometry.TargetSize = ((u64)Param->BlocksPerLun *
						(u64)Param->NumLuns *
						(u64)Param->PagesPerBlock *
						(u64)Param->BytesPerPage);
	InstancePtr->Geometry.EccCodeWordSize = 9U; /* 2 power of 9 = 512 */

#ifdef XNANDPS8_DEBUG
	xil_printf("Manufacturer: %s\r\n", Param->DeviceManufacturer);
	xil_printf("Device Model: %s\r\n", Param->DeviceModel);
	xil_printf("Jedec ID: 0x%x\r\n", Param->JedecManufacturerId);
	xil_printf("Bytes Per Page: 0x%x\r\n", Param->BytesPerPage);
	xil_printf("Spare Bytes Per Page: 0x%x\r\n", Param->SpareBytesPerPage);
	xil_printf("Pages Per Block: 0x%x\r\n", Param->PagesPerBlock);
	xil_printf("Blocks Per LUN: 0x%x\r\n", Param->BlocksPerLun);
	xil_printf("Number of LUNs: 0x%x\r\n", Param->NumLuns);
	xil_printf("Number of bits per cell: 0x%x\r\n", Param->BitsPerCell);
	xil_printf("Number of ECC bits: 0x%x\r\n", Param->EccBits);
	xil_printf("Block Size: 0x%x\r\n", InstancePtr->Geometry.BlockSize);

	xil_printf("Number of Target Blocks: 0x%x\r\n",
					InstancePtr->Geometry.NumTargetBlocks);
	xil_printf("Number of Target Pages: 0x%x\r\n",
					InstancePtr->Geometry.NumTargetPages);

#endif
}

/*****************************************************************************/
/**
*
* This function initializes the feature list from ONFI parameter page.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
* @param	Param is pointer to ONFI parameter page buffer.
*
* @return
*		None
*
* @note		None
*
******************************************************************************/
static void XNandPs8_InitFeatures(XNandPs8 *InstancePtr, OnfiParamPage *Param)
{
	/*
	 * Assert the input arguments.
	 */
	Xil_AssertVoid(Param != NULL);

	InstancePtr->Features.BusWidth = ((Param->Features & (1U << 0U)) != 0U) ?
						XNANDPS8_BUS_WIDTH_16 :
						XNANDPS8_BUS_WIDTH_8;
	InstancePtr->Features.NvDdr = ((Param->Features & (1U << 5)) != 0U) ?
								1U : 0U;
	InstancePtr->Features.EzNand = ((Param->Features & (1U << 9)) != 0U) ?
								1U : 0U;
	InstancePtr->Features.ExtPrmPage = ((Param->Features & (1U << 7)) != 0U) ?
								1U : 0U;
}

/*****************************************************************************/
/**
*
* This function checks if the flash supports on-die ECC.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
* @param	Param is pointer to ONFI parameter page.
*
* @return
*		None
*
* @note		None
*
******************************************************************************/
static s32 XNandPs8_CheckOnDie(XNandPs8 *InstancePtr, OnfiParamPage *Param)
{
	s32 Status = XST_FAILURE;
	u8 JedecId[2] = {0U};
	u8 EccSetFeature[4] = {0x08U, 0x00U, 0x00U, 0x00U};
	u8 EccGetFeature[4] ={0U};

	/*
	 * Assert the input arguments.
	 */
	Xil_AssertNonvoid(Param != NULL);

	/*
	 * Check if this flash supports On-Die ECC.
	 * For more information, refer to Micron TN2945.
	 * Micron Flash: MT29F1G08ABADA, MT29F1G08ABBDA
	 *		 MT29F1G16ABBDA,
	 *		 MT29F2G08ABBEA, MT29F2G16ABBEA,
	 *		 MT29F2G08ABAEA, MT29F2G16ABAEA,
	 *		 MT29F4G08ABBDA, MT29F4G16ABBDA,
	 *		 MT29F4G08ABADA, MT29F4G16ABADA,
	 *		 MT29F8G08ADBDA, MT29F8G16ADBDA,
	 *		 MT29F8G08ADADA, MT29F8G16ADADA
	 */

	/*
	 * Read JEDEC ID
	 */
	Status = XNandPs8_OnfiReadId(InstancePtr, 0U, 0x00U, 2U, &JedecId[0]);
	if (Status != XST_SUCCESS) {
		goto Out;
	}

	if ((JedecId[0] == 0x2CU) &&
	/*
	 * 1 Gb flash devices
	 */
	((JedecId[1] == 0xF1U) ||
	(JedecId[1] == 0xA1U) ||
	(JedecId[1] == 0xB1U) ||
	/*
	 * 2 Gb flash devices
	 */
	(JedecId[1] == 0xAAU) ||
	(JedecId[1] == 0xBAU) ||
	(JedecId[1] == 0xDAU) ||
	(JedecId[1] == 0xCAU) ||
	/*
	 * 4 Gb flash devices
	 */
	(JedecId[1] == 0xACU) ||
	(JedecId[1] == 0xBCU) ||
	(JedecId[1] == 0xDCU) ||
	(JedecId[1] == 0xCCU) ||
	/*
	 * 8 Gb flash devices
	 */
	(JedecId[1] == 0xA3U) ||
	(JedecId[1] == 0xB3U) ||
	(JedecId[1] == 0xD3U) ||
	(JedecId[1] == 0xC3U))) {
#ifdef XNANDPS8_DEBUG
		xil_printf("%s: Ondie flash detected, jedec id 0x%x 0x%x\r\n",
					__func__, JedecId[0], JedecId[1]);
#endif
		/*
		 * On-Die Set Feature
		 */
		Status = XNandPs8_SetFeature(InstancePtr, 0U, 0x90U,
						&EccSetFeature[0]);
		if (Status != XST_SUCCESS) {
#ifdef XNANDPS8_DEBUG
			xil_printf("%s: Ondie set_feature failed\r\n",
								__func__);
#endif
			goto Out;
		}
		/*
		 * Check to see if ECC feature is set
		 */
		Status = XNandPs8_GetFeature(InstancePtr, 0U, 0x90U,
						&EccGetFeature[0]);
		if (Status != XST_SUCCESS) {
#ifdef XNANDPS8_DEBUG
			xil_printf("%s: Ondie get_feature failed\r\n",
								__func__);
#endif
			goto Out;
		}
		if ((EccGetFeature[0] & 0x08U) != 0U) {
			InstancePtr->Features.OnDie = 1U;
			Status = XST_SUCCESS;
		}
	} else {
		/*
		 * On-Die flash not found
		 */
		Status = XST_FAILURE;
	}
Out:
	return Status;
}

/*****************************************************************************/
/**
*
* This function enables DMA mode of controller operation.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
*
* @return
*		None
*
* @note		None
*
******************************************************************************/
void XNandPs8_EnableDmaMode(XNandPs8 *InstancePtr)
{
	/*
	 * Assert the input arguments.
	 */
	Xil_AssertVoid(InstancePtr != NULL);
	Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

	InstancePtr->DmaMode = MDMA;
}

/*****************************************************************************/
/**
*
* This function disables DMA mode of driver/controller operation.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
*
* @return
*		None
*
* @note		None
*
******************************************************************************/
void XNandPs8_DisableDmaMode(XNandPs8 *InstancePtr)
{
	/*
	 * Assert the input arguments.
	 */
	Xil_AssertVoid(InstancePtr != NULL);
	Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

	InstancePtr->DmaMode = PIO;
}

/*****************************************************************************/
/**
*
* This function enables ECC mode of driver/controller operation.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
*
* @return
*		None
*
* @note		None
*
******************************************************************************/
void XNandPs8_EnableEccMode(XNandPs8 *InstancePtr)
{
	/*
	 * Assert the input arguments.
	 */
	Xil_AssertVoid(InstancePtr != NULL);
	Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

	InstancePtr->EccMode = HWECC;
}

/*****************************************************************************/
/**
*
* This function disables ECC mode of driver/controller operation.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
*
* @return
*		None
*
* @note		None
*
******************************************************************************/
void XNandPs8_DisableEccMode(XNandPs8 *InstancePtr)
{
	/*
	 * Assert the input arguments.
	 */
	Xil_AssertVoid(InstancePtr != NULL);
	Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

	InstancePtr->EccMode = NONE;
}

/*****************************************************************************/
/**
*
* This function polls for a register bit set status till the timeout.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
* @param	RegOffset is the offset of register.
* @param	Mask is the bitmask.
* @param	Timeout is the timeout value.
*
* @return
*		- XST_SUCCESS if successful.
*		- XST_FAILURE if failed.
*
* @note		None
*
******************************************************************************/
static s32 XNandPs8_PollRegTimeout(XNandPs8 *InstancePtr, u32 RegOffset,
					u32 Mask, u32 Timeout)
{
	s32 Status = XST_FAILURE;
	volatile u32 RegVal;
	u32 TimeoutVar = Timeout;

	while (TimeoutVar > 0U) {
		RegVal = XNandPs8_ReadReg(InstancePtr->Config.BaseAddress,
						RegOffset);
		if ((RegVal & Mask) != 0U) {
			break;
		}
		TimeoutVar--;
	}

	if (TimeoutVar <= 0U) {
		Status = XST_FAILURE;
	} else {
		Status = XST_SUCCESS;
	}

	return Status;
}

/*****************************************************************************/
/**
*
* This function sets packet size and packet count values in packet register.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
* @param	PktSize is the packet size.
* @param	PktCount is the packet count.
*
* @return
*		None
*
* @note		None
*
******************************************************************************/
static void XNandPs8_SetPktSzCnt(XNandPs8 *InstancePtr, u32 PktSize,
						u32 PktCount)
{
	/*
	 * Assert the input arguments.
	 */
	Xil_AssertVoid(PktSize <= XNANDPS8_MAX_PKT_SIZE);
	Xil_AssertVoid(PktCount <= XNANDPS8_MAX_PKT_COUNT);

	/*
	 * Update Packet Register with pkt size and count
	 */
	XNandPs8_ReadModifyWrite(InstancePtr, XNANDPS8_PKT_OFFSET,
				((u32)XNANDPS8_PKT_PKT_SIZE_MASK |
				(u32)XNANDPS8_PKT_PKT_CNT_MASK),
				((PktSize & XNANDPS8_PKT_PKT_SIZE_MASK) |
				((PktCount << XNANDPS8_PKT_PKT_CNT_SHIFT) &
				XNANDPS8_PKT_PKT_CNT_MASK)));
}

/*****************************************************************************/
/**
*
* This function sets Page and Column values in the Memory address registers.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
* @param	Page is the page value.
* @param	Col is the column value.
*
* @return
*		None
*
* @note		None
*
******************************************************************************/
static void XNandPs8_SetPageColAddr(XNandPs8 *InstancePtr, u32 Page, u16 Col)
{
	/*
	 * Program Memory Address Register 1
	 */
	XNandPs8_WriteReg(InstancePtr->Config.BaseAddress,
				XNANDPS8_MEM_ADDR1_OFFSET,
				((Col & XNANDPS8_MEM_ADDR1_COL_ADDR_MASK) |
				((Page << (u32)XNANDPS8_MEM_ADDR1_PG_ADDR_SHIFT) &
				XNANDPS8_MEM_ADDR1_PG_ADDR_MASK)));
	/*
	 * Program Memory Address Register 2
	 */
	XNandPs8_ReadModifyWrite(InstancePtr, XNANDPS8_MEM_ADDR2_OFFSET,
				XNANDPS8_MEM_ADDR2_MEM_ADDR_MASK,
				((Page >> XNANDPS8_MEM_ADDR1_PG_ADDR_SHIFT) &
				XNANDPS8_MEM_ADDR2_MEM_ADDR_MASK));
}

/*****************************************************************************/
/**
*
* This function sets the size of page in Command Register.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
*
* @return
*		None
*
* @note		None
*
******************************************************************************/
static void XNandPs8_SetPageSize(XNandPs8 *InstancePtr)
{
	u32 PageSizeMask = 0;
	u32 PageSize = InstancePtr->Geometry.BytesPerPage;

	/*
	 * Calculate page size mask
	 */
	switch(PageSize) {
		case XNANDPS8_PAGE_SIZE_512:
			PageSizeMask = (0U << XNANDPS8_CMD_PG_SIZE_SHIFT);
			break;
		case XNANDPS8_PAGE_SIZE_2K:
			PageSizeMask = (1U << XNANDPS8_CMD_PG_SIZE_SHIFT);
			break;
		case XNANDPS8_PAGE_SIZE_4K:
			PageSizeMask = (2U << XNANDPS8_CMD_PG_SIZE_SHIFT);
			break;
		case XNANDPS8_PAGE_SIZE_8K:
			PageSizeMask = (3U << XNANDPS8_CMD_PG_SIZE_SHIFT);
			break;
		case XNANDPS8_PAGE_SIZE_16K:
			PageSizeMask = (4U << XNANDPS8_CMD_PG_SIZE_SHIFT);
			break;
		case XNANDPS8_PAGE_SIZE_1K_16BIT:
			PageSizeMask = (5U << XNANDPS8_CMD_PG_SIZE_SHIFT);
			break;
		default:
			/*
			 * Not supported
			 */
			break;
	}
	/*
	 * Update Command Register
	 */
	XNandPs8_ReadModifyWrite(InstancePtr, XNANDPS8_CMD_OFFSET,
				XNANDPS8_CMD_PG_SIZE_MASK, PageSizeMask);
}

/*****************************************************************************/
/**
*
* This function setup the Ecc Register.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
*
* @return
*		None
*
* @note		None
*
******************************************************************************/
static void XNandPs8_SetEccAddrSize(XNandPs8 *InstancePtr)
{
	u32 PageSize = InstancePtr->Geometry.BytesPerPage;
	u32 CodeWordSize = InstancePtr->Geometry.EccCodeWordSize;
	u32 NumEccBits = InstancePtr->Geometry.NumBitsECC;
	u32 Index;
	u32 Found = 0U;
	u8 BchModeVal = 0U;

	for (Index = 0U; Index < (sizeof(EccMatrix)/sizeof(XNandPs8_EccMatrix));
						Index++) {
		if ((EccMatrix[Index].PageSize == PageSize) &&
			(EccMatrix[Index].CodeWordSize >= CodeWordSize)) {
			if (EccMatrix[Index].NumEccBits >= NumEccBits) {
				Found = Index;
				break;
			}
			else {
				Found = Index;
			}
		}
	}

	if (Found != 0U) {
#ifdef XNANDPS8_DEBUG
		xil_printf("ECC: addr 0x%x size 0x%x numbits %d codesz %d\r\n",
			PageSize + (InstancePtr->Geometry.SpareBytesPerPage
						- EccMatrix[Found].EccSize),
						EccMatrix[Found].EccSize,
						EccMatrix[Found].NumEccBits,
						EccMatrix[Found].CodeWordSize);
#endif
		InstancePtr->EccCfg.EccAddr = PageSize +
				(InstancePtr->Geometry.SpareBytesPerPage
					- EccMatrix[Found].EccSize);
		InstancePtr->EccCfg.EccSize = EccMatrix[Found].EccSize;
		InstancePtr->EccCfg.NumEccBits = EccMatrix[Found].NumEccBits;
		InstancePtr->EccCfg.CodeWordSize =
						EccMatrix[Found].CodeWordSize;

		if (EccMatrix[Found].IsBCH == XNANDPS8_HAMMING) {
			InstancePtr->EccCfg.IsBCH = 0U;
		} else {
			InstancePtr->EccCfg.IsBCH = 1U;
		}
		/*
		 * Write ECC register
		 */
		XNandPs8_WriteReg(InstancePtr->Config.BaseAddress,
				(u32)XNANDPS8_ECC_OFFSET,
				((u32)InstancePtr->EccCfg.EccAddr |
				((u32)InstancePtr->EccCfg.EccSize << (u32)16) |
				((u32)InstancePtr->EccCfg.IsBCH << (u32)27)));

		if (EccMatrix[Found].IsBCH == XNANDPS8_BCH) {
			/*
			 * Write memory address register 2
			 */
			switch(InstancePtr->EccCfg.NumEccBits) {
				case 16U:
					BchModeVal = 0x0U;
					break;
				case 12U:
					BchModeVal = 0x1U;
					break;
				case 8U:
					BchModeVal = 0x2U;
					break;
				case 4U:
					BchModeVal = 0x3U;
					break;
				case 24U:
					BchModeVal = 0x4U;
					break;
				default:
					BchModeVal = 0x0U;
			}
			XNandPs8_ReadModifyWrite(InstancePtr,
				XNANDPS8_MEM_ADDR2_OFFSET,
				XNANDPS8_MEM_ADDR2_NFC_BCH_MODE_MASK,
				(BchModeVal <<
				(u32)XNANDPS8_MEM_ADDR2_NFC_BCH_MODE_SHIFT));
		}
	}
}

/*****************************************************************************/
/**
*
* This function setup the Ecc Spare Command Register.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
*
* @return
*		None
*
* @note		None
*
******************************************************************************/
static void XNandPs8_SetEccSpareCmd(XNandPs8 *InstancePtr, u16 SpareCmd,
								u8 AddrCycles)
{
	XNandPs8_WriteReg(InstancePtr->Config.BaseAddress,
				(u32)XNANDPS8_ECC_SPR_CMD_OFFSET,
				(u32)SpareCmd | ((u32)AddrCycles << 28U));
}

/*****************************************************************************/
/**
*
* This function sets the flash bus width in memory address2 register.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
*
* @return
*		None
*
* @note		None
*
******************************************************************************/
static void XNandPs8_SetBusWidth(XNandPs8 *InstancePtr)
{
	/*
	 * Update Memory Address2 register with bus width
	 */
	XNandPs8_ReadModifyWrite(InstancePtr, XNANDPS8_MEM_ADDR2_OFFSET,
				XNANDPS8_MEM_ADDR2_BUS_WIDTH_MASK,
				(InstancePtr->Features.BusWidth <<
				XNANDPS8_MEM_ADDR2_BUS_WIDTH_SHIFT));

}

/*****************************************************************************/
/**
*
* This function sets the chip select value in memory address2 register.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
* @param	Target is the chip select value.
*
* @return
*		None
*
* @note		None
*
******************************************************************************/
static void XNandPs8_SelectChip(XNandPs8 *InstancePtr, u32 Target)
{
	/*
	 * Update Memory Address2 register with chip select
	 */
	XNandPs8_ReadModifyWrite(InstancePtr, XNANDPS8_MEM_ADDR2_OFFSET,
			XNANDPS8_MEM_ADDR2_CHIP_SEL_MASK,
			((Target << XNANDPS8_MEM_ADDR2_CHIP_SEL_SHIFT) &
			XNANDPS8_MEM_ADDR2_CHIP_SEL_MASK));
}

/*****************************************************************************/
/**
*
* This function sends ONFI Reset command to the flash.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
* @param	Target is the chip select value.
*
* @return
*		- XST_SUCCESS if successful.
*		- XST_FAILURE if failed.
*
* @note		None
*
******************************************************************************/
static s32 XNandPs8_OnfiReset(XNandPs8 *InstancePtr, u32 Target)
{
	s32 Status = XST_FAILURE;

	/*
	 * Assert the input arguments.
	 */
	Xil_AssertNonvoid(Target < XNANDPS8_MAX_TARGETS);

	/*
	 * Enable Transfer Complete Interrupt in Interrupt Status Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
		XNANDPS8_INTR_STS_EN_OFFSET,
		XNANDPS8_INTR_STS_EN_TRANS_COMP_STS_EN_MASK);
	/*
	 * Program Command Register
	 */
	XNandPs8_Prepare_Cmd(InstancePtr, ONFI_CMD_RST, ONFI_CMD_INVALID, 0U,
			0U, 0U);
	/*
	 * Program Memory Address Register2 for chip select
	 */
	XNandPs8_SelectChip(InstancePtr, Target);
	/*
	 * Set Reset in Program Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
		XNANDPS8_PROG_OFFSET, XNANDPS8_PROG_RST_MASK);

	/*
	 * Poll for Transfer Complete event
	 */
	Status = XNandPs8_PollRegTimeout(
			InstancePtr,
			XNANDPS8_INTR_STS_OFFSET,
			XNANDPS8_INTR_STS_TRANS_COMP_STS_EN_MASK,
			XNANDPS8_INTR_POLL_TIMEOUT);
	if (Status != XST_SUCCESS) {
#ifdef XNANDPS8_DEBUG
		xil_printf("%s: Poll for xfer complete timeout\r\n",
							__func__);
#endif
		goto Out;
	}
	/*
	 * Clear Transfer Complete Interrupt in Interrupt Status Enable
	 * Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
		(XNANDPS8_INTR_STS_EN_OFFSET), 0U);
	/*
	 * Clear Transfer Complete Interrupt in Interrupt Status Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_INTR_STS_OFFSET,
			XNANDPS8_INTR_STS_TRANS_COMP_STS_EN_MASK);

Out:
	return Status;
}

/*****************************************************************************/
/**
*
* This function sends ONFI Read Status command to the flash.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
* @param	Target is the chip select value.
* @param	OnfiStatus is the ONFI status value to return.
*
* @return
*		- XST_SUCCESS if successful.
*		- XST_FAILURE if failed.
*
* @note		None
*
******************************************************************************/
static s32 XNandPs8_OnfiReadStatus(XNandPs8 *InstancePtr, u32 Target,
							u16 *OnfiStatus)
{
	s32 Status = XST_FAILURE;

	/*
	 * Assert the input arguments.
	 */
	Xil_AssertNonvoid(Target < XNANDPS8_MAX_TARGETS);
	Xil_AssertNonvoid(OnfiStatus != NULL);
	/*
	 * Enable Transfer Complete Interrupt in Interrupt Status Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
		XNANDPS8_INTR_STS_EN_OFFSET,
		XNANDPS8_INTR_STS_EN_TRANS_COMP_STS_EN_MASK);
	/*
	 * Program Command Register
	 */
	XNandPs8_Prepare_Cmd(InstancePtr, ONFI_CMD_RD_STS, ONFI_CMD_INVALID,
				0U, 0U, 0U);
	/*
	 * Program Memory Address Register2 for chip select
	 */
	XNandPs8_SelectChip(InstancePtr, Target);
	/*
	 * Program Packet Size and Packet Count
	 */
	if(InstancePtr->DataInterface == SDR){
		XNandPs8_SetPktSzCnt(InstancePtr, 1U, 1U);
	}
	else{
		XNandPs8_SetPktSzCnt(InstancePtr, 2U, 1U);
	}

	/*
	 * Set Read Status in Program Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_PROG_OFFSET,XNANDPS8_PROG_RD_STS_MASK);
	/*
	 * Poll for Transfer Complete event
	 */
	Status = XNandPs8_PollRegTimeout(
			InstancePtr,
			XNANDPS8_INTR_STS_OFFSET,
			XNANDPS8_INTR_STS_TRANS_COMP_STS_EN_MASK,
			XNANDPS8_INTR_POLL_TIMEOUT);
	if (Status != XST_SUCCESS) {
#ifdef XNANDPS8_DEBUG
		xil_printf("%s: Poll for xfer complete timeout\r\n",
							__func__);
#endif
		goto Out;
	}
	/*
	 * Clear Transfer Complete Interrupt in Interrupt Status Enable
	 * Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
		XNANDPS8_INTR_STS_EN_OFFSET, 0U);
	/*
	 * Clear Transfer Complete Interrupt in Interrupt Status Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_INTR_STS_OFFSET,
			XNANDPS8_INTR_STS_TRANS_COMP_STS_EN_MASK);
	/*
	 * Read Flash Status
	 */
	*OnfiStatus = (u8) XNandPs8_ReadReg(InstancePtr->Config.BaseAddress,
						XNANDPS8_FLASH_STS_OFFSET);

Out:

	return Status;
}

/*****************************************************************************/
/**
*
* This function sends ONFI Read ID command to the flash.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
* @param	Target is the chip select value.
* @param	Buf is the ONFI ID value to return.
*
* @return
*		- XST_SUCCESS if successful.
*		- XST_FAILURE if failed.
*
* @note		None
*
******************************************************************************/
static s32 XNandPs8_OnfiReadId(XNandPs8 *InstancePtr, u32 Target, u8 IdAddr,
							u32 IdLen, u8 *Buf)
{
	s32 Status = XST_FAILURE;
	u32 Index;
	u32 Rem;
	u32 *BufPtr = (u32 *)(void *)Buf;
	u32 RegVal;
	u32 RemIdx;

	/*
	 * Assert the input arguments.
	 */
	Xil_AssertNonvoid(Target < XNANDPS8_MAX_TARGETS);
	Xil_AssertNonvoid(Buf != NULL);

	/*
	 * Enable Buffer Read Ready Interrupt in Interrupt Status Enable
	 * Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
		XNANDPS8_INTR_STS_EN_OFFSET,
		XNANDPS8_INTR_STS_EN_BUFF_RD_RDY_STS_EN_MASK);
	/*
	 * Program Command
	 */
	XNandPs8_Prepare_Cmd(InstancePtr, ONFI_CMD_RD_ID, ONFI_CMD_INVALID, 0U,
					0U, ONFI_READ_ID_ADDR_CYCLES);

	/*
	 * Program Column, Page, Block address
	 */
	XNandPs8_SetPageColAddr(InstancePtr, 0U, IdAddr);
	/*
	 * Program Memory Address Register2 for chip select
	 */
	XNandPs8_SelectChip(InstancePtr, Target);
	/*
	 * Program Packet Size and Packet Count
	 */
	XNandPs8_SetPktSzCnt(InstancePtr, IdLen, 1U);
	/*
	 * Set Read ID in Program Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_PROG_OFFSET,XNANDPS8_PROG_RD_ID_MASK);

	/*
	 * Poll for Buffer Read Ready event
	 */
	Status = XNandPs8_PollRegTimeout(
		InstancePtr,
		XNANDPS8_INTR_STS_OFFSET,
		XNANDPS8_INTR_STS_BUFF_RD_RDY_STS_EN_MASK,
		XNANDPS8_INTR_POLL_TIMEOUT);
	if (Status != XST_SUCCESS) {
#ifdef XNANDPS8_DEBUG
		xil_printf("%s: Poll for buf read ready timeout\r\n",
							__func__);
#endif
		goto Out;
	}
	/*
	 * Enable Transfer Complete Interrupt in Interrupt
	 * Status Enable Register
	 */

		XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
		XNANDPS8_INTR_STS_EN_OFFSET,
		XNANDPS8_INTR_STS_EN_TRANS_COMP_STS_EN_MASK);

	/*
	 * Clear Buffer Read Ready Interrupt in Interrupt Status
	 * Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_INTR_STS_OFFSET,
			XNANDPS8_INTR_STS_BUFF_RD_RDY_STS_EN_MASK);
	/*
	 * Read Packet Data from Data Port Register
	 */
	for (Index = 0U; Index < (IdLen/4); Index++) {
		BufPtr[Index] = XNandPs8_ReadReg(
					InstancePtr->Config.BaseAddress,
					XNANDPS8_BUF_DATA_PORT_OFFSET);
	}
	Rem = IdLen % 4;
	if (Rem != 0U) {
		RegVal = XNandPs8_ReadReg(
					InstancePtr->Config.BaseAddress,
					XNANDPS8_BUF_DATA_PORT_OFFSET);
		for (RemIdx = 0U; RemIdx < Rem; RemIdx++) {
			Buf[(Index * 4U) + RemIdx] = (u8) (RegVal >>
						(RemIdx * 8U)) & 0xFFU;
		}
	}

	/*
	 * Poll for Transfer Complete event
	 */
	Status = XNandPs8_PollRegTimeout(
			InstancePtr,
			XNANDPS8_INTR_STS_OFFSET,
			XNANDPS8_INTR_STS_TRANS_COMP_STS_EN_MASK,
			XNANDPS8_INTR_POLL_TIMEOUT);
	if (Status != XST_SUCCESS) {
#ifdef XNANDPS8_DEBUG
		xil_printf("%s: Poll for xfer complete timeout\r\n",
							__func__);
#endif
		goto Out;
	}
	/*
	 * Clear Transfer Complete Interrupt in Interrupt Status Enable
	 * Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
		XNANDPS8_INTR_STS_EN_OFFSET,0U);
	/*
	 * Clear Transfer Complete Interrupt in Interrupt Status Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_INTR_STS_OFFSET,
			XNANDPS8_INTR_STS_TRANS_COMP_STS_EN_MASK);

Out:
	return Status;
}

/*****************************************************************************/
/**
*
* This function sends the ONFI Read Parameter Page command to flash.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
* @param	Target is the chip select value.
* @param	PrmIndex is the index of parameter page.
* @param	Buf is the parameter page information to return.
*
* @return
*		- XST_SUCCESS if successful.
*		- XST_FAILURE if failed.
*
* @note		None
*
******************************************************************************/
static s32 XNandPs8_OnfiReadParamPage(XNandPs8 *InstancePtr, u32 Target,
						u8 *Buf)
{
	s32 Status = XST_FAILURE;
	u32 *BufPtr = (u32 *)(void *)Buf;
	u32 Index;

	/*
	 * Assert the input arguments.
	 */
	Xil_AssertNonvoid(Target < XNANDPS8_MAX_TARGETS);
	Xil_AssertNonvoid(Buf != NULL);

	/*
	 * Enable Buffer Read Ready Interrupt in Interrupt Status Enable
	 * Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
		XNANDPS8_INTR_STS_EN_OFFSET,
		XNANDPS8_INTR_STS_EN_BUFF_RD_RDY_STS_EN_MASK);
	/*
	 * Program Command
	 */
	XNandPs8_Prepare_Cmd(InstancePtr, ONFI_CMD_RD_PRM_PG, ONFI_CMD_INVALID,
					0U, 0U, ONFI_PRM_PG_ADDR_CYCLES);
	/*
	 * Program Column, Page, Block address
	 */
	XNandPs8_SetPageColAddr(InstancePtr, 0U, 0U);
	/*
	 * Program Memory Address Register2 for chip select
	 */
	XNandPs8_SelectChip(InstancePtr, Target);
	/*
	 * Program Packet Size and Packet Count
	 */
	XNandPs8_SetPktSzCnt(InstancePtr, ONFI_PRM_PG_LEN, 1U);
	/*
	 * Set Read Parameter Page in Program Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_PROG_OFFSET,XNANDPS8_PROG_RD_PRM_PG_MASK);

	/*
	 * Poll for Buffer Read Ready event
	 */
	 Status = XNandPs8_PollRegTimeout(
			InstancePtr,
			XNANDPS8_INTR_STS_OFFSET,
			XNANDPS8_INTR_STS_BUFF_RD_RDY_STS_EN_MASK,
			XNANDPS8_INTR_POLL_TIMEOUT);
	 if (Status != XST_SUCCESS) {
#ifdef XNANDPS8_DEBUG
			xil_printf("%s: Poll for buf read ready timeout\r\n",
							__func__);
#endif
			goto Out;
		}


			/*
			 * Enable Transfer Complete Interrupt in Interrupt
			 * Status Enable Register
			 */
			XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
				(XNANDPS8_INTR_STS_EN_OFFSET),
				XNANDPS8_INTR_STS_EN_TRANS_COMP_STS_EN_MASK);
		/*
		 * Clear Buffer Read Ready Interrupt in Interrupt Status
		 * Register
		 */
		XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
				XNANDPS8_INTR_STS_OFFSET,
				XNANDPS8_INTR_STS_BUFF_RD_RDY_STS_EN_MASK);
		/*
		 * Read Packet Data from Data Port Register
		 */
		for (Index = 0U; Index < (ONFI_PRM_PG_LEN/4); Index++) {
			BufPtr[Index] = XNandPs8_ReadReg(
					InstancePtr->Config.BaseAddress,
						XNANDPS8_BUF_DATA_PORT_OFFSET);
		}

	/*
	 * Poll for Transfer Complete event
	 */
	Status = XNandPs8_PollRegTimeout(
			InstancePtr,
			XNANDPS8_INTR_STS_OFFSET,
			XNANDPS8_INTR_STS_TRANS_COMP_STS_EN_MASK,
			XNANDPS8_INTR_POLL_TIMEOUT);
	if (Status != XST_SUCCESS) {
#ifdef XNANDPS8_DEBUG
		xil_printf("%s: Poll for xfer complete timeout\r\n",
							__func__);
#endif
		goto Out;
	}
	/*
	 * Clear Transfer Complete Interrupt in Interrupt Status Enable
	 * Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
		XNANDPS8_INTR_STS_EN_OFFSET, 0U);
	/*
	 * Clear Transfer Complete Interrupt in Interrupt Status Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_INTR_STS_OFFSET,
			XNANDPS8_INTR_STS_TRANS_COMP_STS_EN_MASK);

Out:
	return Status;
}

/*****************************************************************************/
/**
*
* This function returns the length including bad blocks from a given offset and
* length.
*
* @param	InstancePtr is the pointer to the XNandPs8 instance.
* @param	Offset is the flash data address to read from.
* @param	Length is number of bytes to read.
*
* @return
*		- Return actual length including bad blocks.
*
* @note		None.
*
******************************************************************************/
static s32 XNandPs8_CalculateLength(XNandPs8 *InstancePtr, u64 Offset,
							u64 Length)
{
	s32 Status;
	u32 BlockSize;
	u32 BlockLen;
	u32 Block;
	u32 TempLen = 0;
	u64 OffsetVar = Offset;

	BlockSize = InstancePtr->Geometry.BlockSize;

	while (TempLen < Length) {
		Block = (u32) ((u32)OffsetVar/BlockSize);
		BlockLen = BlockSize - ((u32)OffsetVar % BlockSize);
		/*
		 * Check if the block is bad
		 */
		Status = XNandPs8_IsBlockBad(InstancePtr, Block);
		if (Status != XST_SUCCESS) {
			/*
			 * Good block
			 */
			TempLen += BlockLen;
		}
		if (OffsetVar >= InstancePtr->Geometry.DeviceSize) {
			Status = XST_FAILURE;
			goto Out;
		}
		OffsetVar += BlockLen;
	}

	Status = XST_SUCCESS;
Out:
	return Status;
}

/*****************************************************************************/
/**
*
* This function writes to the flash.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
* @param	Offset is the starting offset of flash to write.
* @param	Length is the number of bytes to write.
* @param	SrcBuf is the source data buffer to write.
*
* @return
*		- XST_SUCCESS if successful.
*		- XST_FAILURE if failed.
*
* @note		None
*
******************************************************************************/
s32 XNandPs8_Write(XNandPs8 *InstancePtr, u64 Offset, u64 Length, u8 *SrcBuf)
{
	s32 Status = XST_FAILURE;
	u32 Page;
	u32 Col;
	u32 Target;
	u32 Block;
	u32 PartialBytes = 0;
	u32 NumBytes;
	u32 RemLen;
	u8 *BufPtr;
	u8 *Ptr = (u8 *)SrcBuf;
	u16 OnfiStatus;
	u64 OffsetVar = Offset;
	u64 LengthVar = Length;

	/*
	 * Assert the input arguments.
	 */
	Xil_AssertNonvoid(InstancePtr != NULL);
	Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);
	Xil_AssertNonvoid(SrcBuf != NULL);
	Xil_AssertNonvoid(LengthVar != 0U);
	Xil_AssertNonvoid((OffsetVar + LengthVar) <
				InstancePtr->Geometry.DeviceSize);

	/*
	 * Check if write operation exceeds flash size when including
	 * bad blocks.
	 */
	Status = XNandPs8_CalculateLength(InstancePtr, OffsetVar, LengthVar);
	if (Status != XST_SUCCESS) {
		goto Out;
	}

	while (LengthVar > 0U) {
		Block = (u32) (OffsetVar/InstancePtr->Geometry.BlockSize);
		/*
		 * Skip the bad blocks. Increment the offset by block size.
		 * For better results, always program the flash starting at
		 * a block boundary.
		 */
		if (XNandPs8_IsBlockBad(InstancePtr, Block) == XST_SUCCESS) {
			OffsetVar += (u64)InstancePtr->Geometry.BlockSize;
			continue;
		}
		/*
		 * Calculate Page and Column address values
		 */
		Page = (u32) (OffsetVar/InstancePtr->Geometry.BytesPerPage);
		Col = (u32) (OffsetVar &
				(InstancePtr->Geometry.BytesPerPage - 1U));
		PartialBytes = 0U;
		/*
		 * Check if partial write.
		 * If column address is > 0 or Length is < page size
		 */
		if ((Col > 0U) ||
			(LengthVar < InstancePtr->Geometry.BytesPerPage)) {
			RemLen = InstancePtr->Geometry.BytesPerPage - Col;
			PartialBytes = (RemLen < (u32)LengthVar) ?
					RemLen : (u32)LengthVar;
		}

		Target = (u32) (OffsetVar/InstancePtr->Geometry.TargetSize);
		if (Page > InstancePtr->Geometry.NumTargetPages) {
			Page %= InstancePtr->Geometry.NumTargetPages;
		}

		/*
		 * Check if partial write
		 */
		if (PartialBytes > 0U) {
			BufPtr = &InstancePtr->PartialDataBuf[0];
			memset(BufPtr, 0xFF,
					InstancePtr->Geometry.BytesPerPage);
			memcpy(BufPtr + Col, Ptr, PartialBytes);

			NumBytes = PartialBytes;
		} else {
			BufPtr = (u8 *)Ptr;
			NumBytes = (InstancePtr->Geometry.BytesPerPage <
					(u32)LengthVar) ?
					InstancePtr->Geometry.BytesPerPage :
					(u32)LengthVar;
		}
		/*
		 * Program page
		 */
		Status = XNandPs8_ProgramPage(InstancePtr, Target, Page, 0U,
								BufPtr);
		if (Status != XST_SUCCESS) {
			goto Out;
		}
		/*
		 * ONFI ReadStatus
		 */
		do {
			Status = XNandPs8_OnfiReadStatus(InstancePtr, Target,
								&OnfiStatus);
			if (Status != XST_SUCCESS) {
				goto Out;
			}
			if ((OnfiStatus & (1U << 6U)) != 0U) {
				if ((OnfiStatus & (1U << 0U)) != 0U) {
					Status = XST_FAILURE;
					goto Out;
				}
			}
		} while (((OnfiStatus >> 6U) & 0x1U) == 0U);

		Ptr += NumBytes;
		OffsetVar += NumBytes;
		LengthVar -= NumBytes;
	}

	Status = XST_SUCCESS;
Out:
	return Status;
}

/*****************************************************************************/
/**
*
* This function reads from the flash.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
* @param	Offset is the starting offset of flash to read.
* @param	Length is the number of bytes to read.
* @param	DestBuf is the destination data buffer to fill in.
*
* @return
*		- XST_SUCCESS if successful.
*		- XST_FAILURE if failed.
*
* @note		None
*
******************************************************************************/
s32 XNandPs8_Read(XNandPs8 *InstancePtr, u64 Offset, u64 Length, u8 *DestBuf)
{
	s32 Status = XST_FAILURE;
	u32 Page;
	u32 Col;
	u32 Target;
	u32 Block;
	u32 PartialBytes = 0U;
	u32 RemLen;
	u32 NumBytes;
	u8 *BufPtr;
	u8 *Ptr = (u8 *)DestBuf;
	u64 OffsetVar = Offset;
	u64 LengthVar = Length;

	/*
	 * Assert the input arguments.
	 */
	Xil_AssertNonvoid(InstancePtr != NULL);
	Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);
	Xil_AssertNonvoid(LengthVar != 0U);
	Xil_AssertNonvoid((OffsetVar + LengthVar) <
				InstancePtr->Geometry.DeviceSize);

	/*
	 * Check if read operation exceeds flash size when including
	 * bad blocks.
	 */
	Status = XNandPs8_CalculateLength(InstancePtr, OffsetVar, LengthVar);
	if (Status != XST_SUCCESS) {
		goto Out;
	}

	while (LengthVar > 0U) {
		Block = (u32) (OffsetVar/InstancePtr->Geometry.BlockSize);
		/*
		 * Skip the bad block. Increment the offset by block size.
		 * The flash programming utility must make sure to start
		 * writing always at a block boundary and skip blocks if any.
		 */
		if (XNandPs8_IsBlockBad(InstancePtr, Block) == XST_SUCCESS) {
			OffsetVar += (u64)InstancePtr->Geometry.BlockSize;
			continue;
		}
		/*
		 * Calculate Page and Column address values
		 */
		Page = (u32) (OffsetVar/InstancePtr->Geometry.BytesPerPage);
		Col = (u32) (OffsetVar &
				(InstancePtr->Geometry.BytesPerPage - 1U));
		PartialBytes = 0U;
		/*
		 * Check if partial write.
		 * If column address is > 0 or Length is < page size
		 */
		if ((Col > 0U) ||
			(LengthVar < InstancePtr->Geometry.BytesPerPage)) {
			RemLen = InstancePtr->Geometry.BytesPerPage - Col;
			PartialBytes = ((u32)RemLen < (u32)LengthVar) ?
						(u32)RemLen : (u32)LengthVar;
		}

		Target = (u32) (OffsetVar/InstancePtr->Geometry.TargetSize);
		if (Page > InstancePtr->Geometry.NumTargetPages) {
			Page %= InstancePtr->Geometry.NumTargetPages;
		}
		/*
		 * Check if partial read
		 */
		if (PartialBytes > 0U) {
			BufPtr = &InstancePtr->PartialDataBuf[0];
			NumBytes = PartialBytes;
		} else {
			BufPtr = Ptr;
			NumBytes = (InstancePtr->Geometry.BytesPerPage <
					(u32)LengthVar) ?
					InstancePtr->Geometry.BytesPerPage :
					(u32)LengthVar;
		}
		/*
		 * Read page
		 */
		Xil_AssertNonvoid(BufPtr != NULL);
		Status = XNandPs8_ReadPage(InstancePtr, Target, Page, 0U,
								BufPtr);
		if (Status != XST_SUCCESS) {
			goto Out;
		}
		if (PartialBytes > 0U) {
			memcpy(Ptr, BufPtr + Col, NumBytes);
		}
		Ptr += NumBytes;
		OffsetVar += NumBytes;
		LengthVar -= NumBytes;
	}

	Status = XST_SUCCESS;
Out:
	return Status;
}

/*****************************************************************************/
/**
*
* This function erases the flash.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
* @param	Offset is the starting offset of flash to erase.
* @param	Length is the number of bytes to erase.
*
* @return
*		- XST_SUCCESS if successful.
*		- XST_FAILURE if failed.
*
* @note
*		The Offset and Length should be aligned to block size boundary
*		to get better results.
*
******************************************************************************/
s32 XNandPs8_Erase(XNandPs8 *InstancePtr, u64 Offset, u64 Length)
{
	s32 Status = XST_FAILURE;
	u32 Target = 0;
	u32 StartBlock;
	u32 NumBlocks = 0;
	u32 Block;
	u32 AlignOff;
	u32 EraseLen;
	u32 BlockRemLen;
	u16 OnfiStatus;
	u64 OffsetVar = Offset;
	u64 LengthVar = Length;

	/*
	 * Assert the input arguments.
	 */
	Xil_AssertNonvoid(InstancePtr != NULL);
	Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);
	Xil_AssertNonvoid(LengthVar != 0U);
	Xil_AssertNonvoid((OffsetVar + LengthVar) <
			InstancePtr->Geometry.DeviceSize);

	/*
	 * Check if erase operation exceeds flash size when including
	 * bad blocks.
	 */
	Status = XNandPs8_CalculateLength(InstancePtr, OffsetVar, LengthVar);
	if (Status != XST_SUCCESS) {
		goto Out;
	}
	/*
	 * Calculate number of blocks to erase
	 */
	StartBlock = (u32) (OffsetVar/InstancePtr->Geometry.BlockSize);

	while (LengthVar > 0U) {
		Block = (u32) (OffsetVar/InstancePtr->Geometry.BlockSize);
		if (XNandPs8_IsBlockBad(InstancePtr, Block) ==
							XST_SUCCESS) {
			OffsetVar += (u64)InstancePtr->Geometry.BlockSize;
			NumBlocks++;
			continue;
		}

		AlignOff = (u32)OffsetVar &
				(InstancePtr->Geometry.BlockSize - (u32)1);
		if (AlignOff > 0U) {
			BlockRemLen = InstancePtr->Geometry.BlockSize -
								AlignOff;
			EraseLen = (BlockRemLen < (u32)LengthVar) ?
						BlockRemLen :(u32)LengthVar;
		} else {
			EraseLen = (InstancePtr->Geometry.BlockSize <
						(u32)LengthVar) ?
					InstancePtr->Geometry.BlockSize:
						(u32)LengthVar;
		}
		NumBlocks++;
		OffsetVar += EraseLen;
		LengthVar -= EraseLen;
	}

	for (Block = StartBlock; Block < (StartBlock + NumBlocks); Block++) {
		Target = Block/InstancePtr->Geometry.NumTargetBlocks;
		Block %= InstancePtr->Geometry.NumTargetBlocks;
		if (XNandPs8_IsBlockBad(InstancePtr, Block) ==
							XST_SUCCESS) {
			/*
			 * Don't erase bad block
			 */
			continue;
		}
		/*
		 * Block Erase
		 */
		Status = XNandPs8_EraseBlock(InstancePtr, Target, Block);
		if (Status != XST_SUCCESS) {
			goto Out;
		}
		/*
		 * ONFI ReadStatus
		 */
		do {
			Status = XNandPs8_OnfiReadStatus(InstancePtr, Target,
								&OnfiStatus);
			if (Status != XST_SUCCESS) {
				goto Out;
			}
			if ((OnfiStatus & (1U << 6U)) != 0U) {
				if ((OnfiStatus & (1U << 0U)) != 0U) {
					Status = XST_FAILURE;
					goto Out;
				}
			}
		} while (((OnfiStatus >> 6U) & 0x1U) == 0U);
	}

	Status = XST_SUCCESS;
Out:
	return Status;
}

/*****************************************************************************/
/**
*
* This function sends ONFI Program Page command to flash.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
* @param	Target is the chip select value.
* @param	Page is the page address value to program.
* @param	Col is the column address value to program.
* @param	Buf is the data buffer to program.
*
* @return
*		- XST_SUCCESS if successful.
*		- XST_FAILURE if failed.
*
* @note		None
*
******************************************************************************/
static s32 XNandPs8_ProgramPage(XNandPs8 *InstancePtr, u32 Target, u32 Page,
							u32 Col, u8 *Buf)
{
	u32 AddrCycles = InstancePtr->Geometry.RowAddrCycles +
				InstancePtr->Geometry.ColAddrCycles;
	u32 PktSize;
	u32 PktCount;
	u32 BufWrCnt = 0U;
	u32 *BufPtr = (u32 *)(void *)Buf;
	s32 Status = XST_FAILURE;
	u32 Index;

	/*
	 * Assert the input arguments.
	 */
	Xil_AssertNonvoid(Page < InstancePtr->Geometry.NumPages);
	Xil_AssertNonvoid(Buf != NULL);

	if (InstancePtr->EccCfg.CodeWordSize > 9U) {
		PktSize = 1024U;
	} else {
		PktSize = 512U;
	}
	PktCount = InstancePtr->Geometry.BytesPerPage/PktSize;

	XNandPs8_Prepare_Cmd(InstancePtr, ONFI_CMD_PG_PROG1, ONFI_CMD_PG_PROG2,
					1U, 1U, (u8)AddrCycles);

	if (InstancePtr->DmaMode == MDMA) {

		/*
		 * Enable DMA boundary Interrupt in Interrupt Status
		 * Enable Register
		 */
		XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_INTR_STS_EN_OFFSET,
			XNANDPS8_INTR_STS_EN_TRANS_COMP_STS_EN_MASK |
			XNANDPS8_INTR_STS_EN_DMA_INT_STS_EN_MASK);
	} else {
		/*
		 * Enable Buffer Write Ready Interrupt in Interrupt Status
		 * Enable Register
		 */
		XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_INTR_STS_EN_OFFSET,
			XNANDPS8_INTR_STS_EN_BUFF_WR_RDY_STS_EN_MASK);
	}
	/*
	 * Program Page Size
	 */
	XNandPs8_SetPageSize(InstancePtr);
	/*
	 * Program Packet Size and Packet Count
	 */
	XNandPs8_SetPktSzCnt(InstancePtr, PktSize, PktCount);
	/*
	 * Program DMA system address and DMA buffer boundary
	 */
	if (InstancePtr->DmaMode == MDMA) {
		/*
		 * Flush the Data Cache
		 */
		Xil_DCacheFlushRange(Buf, (PktSize * PktCount));

#ifdef __aarch64__
		XNandPs8_WriteReg(InstancePtr->Config.BaseAddress,
				XNANDPS8_DMA_SYS_ADDR1_OFFSET,
				(u32) (((INTPTR)Buf >> 32) & 0xFFFFFFFFU));
#endif
		XNandPs8_WriteReg(InstancePtr->Config.BaseAddress,
				XNANDPS8_DMA_SYS_ADDR0_OFFSET,
				(u32) ((INTPTR)(void *)Buf & 0xFFFFFFFFU));
	}
	/*
	 * Program Column, Page, Block address
	 */
	XNandPs8_SetPageColAddr(InstancePtr, Page, (u16)Col);
	/*
	 * Set Bus Width
	 */
	XNandPs8_SetBusWidth(InstancePtr);
	/*
	 * Program Memory Address Register2 for chip select
	 */
	XNandPs8_SelectChip(InstancePtr, Target);
	/*
	 * Set ECC
	 */
	if (InstancePtr->EccMode == HWECC) {
		XNandPs8_SetEccSpareCmd(InstancePtr, ONFI_CMD_CHNG_WR_COL,
					InstancePtr->Geometry.ColAddrCycles);
	}
	/*
	 * Set Page Program in Program Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_PROG_OFFSET,XNANDPS8_PROG_PG_PROG_MASK);

	if (InstancePtr->DmaMode == MDMA) {
		goto WriteDmaDone;
	}

	while (BufWrCnt < PktCount) {
		/*
		 * Poll for Buffer Write Ready event
		 */
		Status = XNandPs8_PollRegTimeout(
			InstancePtr,
			XNANDPS8_INTR_STS_OFFSET,
			XNANDPS8_INTR_STS_BUFF_WR_RDY_STS_EN_MASK,
			XNANDPS8_INTR_POLL_TIMEOUT);
		if (Status != XST_SUCCESS) {
#ifdef XNANDPS8_DEBUG
			xil_printf("%s: Poll for buf write ready timeout\r\n",
							__func__);
#endif
			goto Out;
		}
		/*
		 * Increment Buffer Write Interrupt Count
		 */
		BufWrCnt++;

		if (BufWrCnt == PktCount) {
			/*
			 * Enable Transfer Complete Interrupt in Interrupt
			 * Status Enable Register
			 */
			XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
				XNANDPS8_INTR_STS_EN_OFFSET,
				XNANDPS8_INTR_STS_EN_TRANS_COMP_STS_EN_MASK);
		} else {
			/*
			 * Clear Buffer Write Ready Interrupt in Interrupt
			 * Status Enable Register
			 */
			XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
				XNANDPS8_INTR_STS_EN_OFFSET, 0U);

		}
		/*
		 * Clear Buffer Write Ready Interrupt in Interrupt Status
		 * Register
		 */
		XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
				XNANDPS8_INTR_STS_OFFSET,
				XNANDPS8_INTR_STS_BUFF_WR_RDY_STS_EN_MASK);
		/*
		 * Write Packet Data to Data Port Register
		 */
		for (Index = 0U; Index < (PktSize/4U); Index++) {
			XNandPs8_WriteReg(InstancePtr->Config.BaseAddress,
						XNANDPS8_BUF_DATA_PORT_OFFSET,
						BufPtr[Index]);
		}
		BufPtr += (PktSize/4U);

		if (BufWrCnt < PktCount) {
			/*
			 * Enable Buffer Write Ready Interrupt in Interrupt
			 * Status Enable Register
			 */
			XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
				XNANDPS8_INTR_STS_EN_OFFSET,
				XNANDPS8_INTR_STS_EN_BUFF_WR_RDY_STS_EN_MASK);
		} else {
			break;
		}
	}
WriteDmaDone:
	/*
	 * Poll for Transfer Complete event
	 */
	Status = XNandPs8_PollRegTimeout(
			InstancePtr,
			XNANDPS8_INTR_STS_OFFSET,
			XNANDPS8_INTR_STS_TRANS_COMP_STS_EN_MASK,
			XNANDPS8_INTR_POLL_TIMEOUT);
	if (Status != XST_SUCCESS) {
#ifdef XNANDPS8_DEBUG
		xil_printf("%s: Poll for xfer complete timeout\r\n",
							__func__);
#endif
		goto Out;
	}
	/*
	 * Clear Transfer Complete Interrupt in Interrupt Status Enable
	 * Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
		XNANDPS8_INTR_STS_EN_OFFSET, 0U);

	/*
	 * Clear Transfer Complete Interrupt in Interrupt Status Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_INTR_STS_OFFSET,
			XNANDPS8_INTR_STS_TRANS_COMP_STS_EN_MASK);

Out:
	return Status;
}

/*****************************************************************************/
/**
*
* This function sends ONFI Program Page command to flash.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
* @param	Target is the chip select value.
* @param	Page is the page address value to program.
* @param	Col is the column address value to program.
* @param	Buf is the data buffer to program.
*
* @return
*		- XST_SUCCESS if successful.
*		- XST_FAILURE if failed.
*
* @note		None
*
******************************************************************************/
s32 XNandPs8_WriteSpareBytes(XNandPs8 *InstancePtr, u32 Page, u8 *Buf)
{
	u32 AddrCycles = InstancePtr->Geometry.RowAddrCycles +
				InstancePtr->Geometry.ColAddrCycles;
	u32 Col = InstancePtr->Geometry.BytesPerPage;
	u32 Target = Page/InstancePtr->Geometry.NumTargetPages;
	u32 PktSize = InstancePtr->Geometry.SpareBytesPerPage;
	u32 PktCount = 1U;
	u32 BufWrCnt = 0U;
	u32 *BufPtr = (u32 *)(void *)Buf;
	u16 PreEccSpareCol = 0U;
	u16 PreEccSpareWrCnt = 0U;
	u16 PostEccSpareCol = 0U;
	u16 PostEccSpareWrCnt = 0U;
	u32 PostWrite = 0U;
	OnfiCmdFormat Cmd;
	s32 Status = XST_FAILURE;
	u32 Index;
	u32 PageVar = Page;

	/*
	 * Assert the input arguments.
	 */
	Xil_AssertNonvoid(InstancePtr != NULL);
	Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);
	Xil_AssertNonvoid(PageVar < InstancePtr->Geometry.NumPages);
	Xil_AssertNonvoid(Buf != NULL);

	PageVar %= InstancePtr->Geometry.NumTargetPages;

	if (InstancePtr->EccMode == HWECC) {
		/*
		 * Calculate ECC free positions before and after ECC code
		 */
		PreEccSpareCol = 0x0U;
		PreEccSpareWrCnt = InstancePtr->EccCfg.EccAddr -
				(u16)InstancePtr->Geometry.BytesPerPage;

		PostEccSpareCol = PreEccSpareWrCnt +
					InstancePtr->EccCfg.EccSize;
		PostEccSpareWrCnt = InstancePtr->Geometry.SpareBytesPerPage -
					PostEccSpareCol;

		PreEccSpareWrCnt = (PreEccSpareWrCnt/4U) * 4U;
		PostEccSpareWrCnt = (PostEccSpareWrCnt/4U) * 4U;

		if (PreEccSpareWrCnt > 0U) {
			PktSize = PreEccSpareWrCnt;
			PktCount = 1U;
			Col = InstancePtr->Geometry.BytesPerPage +
							PreEccSpareCol;
			BufPtr = (u32 *)(void *)Buf;
			if (PostEccSpareWrCnt > 0U) {
				PostWrite = 1U;
			}
		} else if (PostEccSpareWrCnt > 0U) {
			PktSize = PostEccSpareWrCnt;
			PktCount = 1U;
			Col = InstancePtr->Geometry.BytesPerPage +
							PostEccSpareCol;
			BufPtr = (u32 *)(void *)&Buf[Col];
		} else {
			/*
			 * No free spare bytes available for writing
			 */
			Status = XST_FAILURE;
			goto Out;
		}
	}

	if (InstancePtr->DmaMode == MDMA) {
		/*
		 * Enable Transfer Complete Interrupt in Interrupt Status
		 * Enable Register
		 */
		XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_INTR_STS_EN_OFFSET,
			XNANDPS8_INTR_STS_EN_TRANS_COMP_STS_EN_MASK);

	} else {
		/*
		 * Enable Buffer Write Ready Interrupt in Interrupt Status
		 * Enable Register
		 */
		XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_INTR_STS_EN_OFFSET,
			XNANDPS8_INTR_STS_EN_BUFF_WR_RDY_STS_EN_MASK);

	}
	/*
	 * Program Command hack for change write column
	 */
	if (PostWrite > 0U) {
		Cmd.Command1 = 0x80U;
		Cmd.Command2 = 0x00U;
		XNandPs8_Prepare_Cmd(InstancePtr, Cmd.Command1, Cmd.Command2,
				0U , 1U, (u8)AddrCycles);

	} else {
		XNandPs8_Prepare_Cmd(InstancePtr, ONFI_CMD_PG_PROG1,
				ONFI_CMD_PG_PROG2, 0U , 1U, (u8)AddrCycles);
	}
	/*
	 * Program Page Size
	 */
	XNandPs8_SetPageSize(InstancePtr);
	/*
	 * Program Packet Size and Packet Count
	 */
	XNandPs8_SetPktSzCnt(InstancePtr, PktSize, PktCount);
	/*
	 * Program DMA system address and DMA buffer boundary
	 */
	if (InstancePtr->DmaMode == MDMA) {
		/*
		 * Flush the Data Cache
		 */
		Xil_DCacheFlushRange(BufPtr, (PktSize * PktCount));

#ifdef __aarch64__
		XNandPs8_WriteReg(InstancePtr->Config.BaseAddress,
				XNANDPS8_DMA_SYS_ADDR1_OFFSET,
				(u32) (((INTPTR)BufPtr >> 32) & 0xFFFFFFFFU));
#endif
		XNandPs8_WriteReg(InstancePtr->Config.BaseAddress,
				XNANDPS8_DMA_SYS_ADDR0_OFFSET,
				(u32) ((INTPTR)(void *)BufPtr & 0xFFFFFFFFU));

		XNandPs8_WriteReg(InstancePtr->Config.BaseAddress,
				XNANDPS8_DMA_BUF_BND_OFFSET,
				XNANDPS8_DMA_BUF_BND_512K);
	}
	/*
	 * Program Column, Page, Block address
	 */
	XNandPs8_SetPageColAddr(InstancePtr, PageVar, (u16)Col);
	/*
	 * Set Bus Width
	 */
	XNandPs8_SetBusWidth(InstancePtr);
	/*
	 * Program Memory Address Register2 for chip select
	 */
	XNandPs8_SelectChip(InstancePtr, Target);
	/*
	 * Set Page Program in Program Register
	 */
	if (PostWrite > 0U) {
		XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_PROG_OFFSET,((u32)XNANDPS8_PROG_PG_PROG_MASK |
				(u32)XNANDPS8_PROG_CHNG_ROW_ADDR_MASK));
	} else {
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_PROG_OFFSET,XNANDPS8_PROG_PG_PROG_MASK);
	}

	if (InstancePtr->DmaMode == MDMA) {
		goto WriteDmaDone;
	}

	while (BufWrCnt < PktCount) {
		/*
		 * Poll for Buffer Write Ready event
		 */
		Status = XNandPs8_PollRegTimeout(
			InstancePtr,
			XNANDPS8_INTR_STS_OFFSET,
			XNANDPS8_INTR_STS_BUFF_WR_RDY_STS_EN_MASK,
			XNANDPS8_INTR_POLL_TIMEOUT);
		if (Status != XST_SUCCESS) {
#ifdef XNANDPS8_DEBUG
			xil_printf("%s: Poll for buf write ready timeout\r\n",
							__func__);
#endif
			goto Out;
		}
		/*
		 * Increment Buffer Write Interrupt Count
		 */
		BufWrCnt++;

		if (BufWrCnt == PktCount) {
			/*
			 * Enable Transfer Complete Interrupt in Interrupt
			 * Status Enable Register
			 */
			XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
				XNANDPS8_INTR_STS_EN_OFFSET,
				XNANDPS8_INTR_STS_EN_TRANS_COMP_STS_EN_MASK);

		} else {
			/*
			 * Clear Buffer Write Ready Interrupt in Interrupt
			 * Status Enable Register
			 */
			XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
				XNANDPS8_INTR_STS_EN_OFFSET, 0U);
		}
		/*
		 * Clear Buffer Write Ready Interrupt in Interrupt Status
		 * Register
		 */
		XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
				XNANDPS8_INTR_STS_OFFSET,
				XNANDPS8_INTR_STS_BUFF_WR_RDY_STS_EN_MASK);
		/*
		 * Write Packet Data to Data Port Register
		 */
		for (Index = 0U; Index < (PktSize/4U); Index++) {
			XNandPs8_WriteReg(InstancePtr->Config.BaseAddress,
						XNANDPS8_BUF_DATA_PORT_OFFSET,
						BufPtr[Index]);
		}
		BufPtr += (PktSize/4U);

		if (BufWrCnt < PktCount) {
			/*
			 * Enable Buffer Write Ready Interrupt in Interrupt
			 * Status Enable Register
			 */
			XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
				XNANDPS8_INTR_STS_EN_OFFSET,
				XNANDPS8_INTR_STS_EN_BUFF_WR_RDY_STS_EN_MASK);
		} else {
			break;
		}
	}
WriteDmaDone:
	/*
	 * Poll for Transfer Complete event
	 */
	Status = XNandPs8_PollRegTimeout(
			InstancePtr,
			XNANDPS8_INTR_STS_OFFSET,
			XNANDPS8_INTR_STS_TRANS_COMP_STS_EN_MASK,
			XNANDPS8_INTR_POLL_TIMEOUT);
	if (Status != XST_SUCCESS) {
#ifdef XNANDPS8_DEBUG
		xil_printf("%s: Poll for xfer complete timeout\r\n",
							__func__);
#endif
		goto Out;
	}
	/*
	 * Clear Transfer Complete Interrupt in Interrupt Status Enable
	 * Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
		XNANDPS8_INTR_STS_EN_OFFSET, 0U);

	/*
	 * Clear Transfer Complete Interrupt in Interrupt Status Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
				XNANDPS8_INTR_STS_OFFSET,
				XNANDPS8_INTR_STS_TRANS_COMP_STS_EN_MASK);

	if (InstancePtr->EccMode == HWECC) {
		if (PostWrite > 0U) {
			BufPtr = (u32 *)(void *)&Buf[PostEccSpareCol];
			Status = XNandPs8_ChangeWriteColumn(InstancePtr,
					Target,
					PostEccSpareCol, PostEccSpareWrCnt, 1U,
					(u8 *)(void *)BufPtr);
			if (Status != XST_SUCCESS) {
				goto Out;
			}
		}
	}
Out:

	return Status;
}

/*****************************************************************************/
/**
*
* This function sends ONFI Read Page command to flash.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
* @param	Target is the chip select value.
* @param	Page is the page address value to read.
* @param	Col is the column address value to read.
* @param	Buf is the data buffer to fill in.
*
* @return
*		- XST_SUCCESS if successful.
*		- XST_FAILURE if failed.
*
* @note		None
*
******************************************************************************/
static s32 XNandPs8_ReadPage(XNandPs8 *InstancePtr, u32 Target, u32 Page,
							u32 Col, u8 *Buf)
{
	u32 AddrCycles = InstancePtr->Geometry.RowAddrCycles +
				InstancePtr->Geometry.ColAddrCycles;
	u32 PktSize;
	u32 PktCount;
	u32 BufRdCnt = 0U;
	u32 *BufPtr = (u32 *)(void *)Buf;
	s32 Status = XST_FAILURE;
	u32 Index;

	/*
	 * Assert the input arguments.
	 */
	Xil_AssertNonvoid(Page < InstancePtr->Geometry.NumPages);
	Xil_AssertNonvoid(Target < XNANDPS8_MAX_TARGETS);

	if (InstancePtr->EccCfg.CodeWordSize > 9U) {
		PktSize = 1024U;
	} else {
		PktSize = 512U;
	}
	PktCount = InstancePtr->Geometry.BytesPerPage/PktSize;

	XNandPs8_Prepare_Cmd(InstancePtr, ONFI_CMD_RD1, ONFI_CMD_RD2,
					1U, 1U, (u8)AddrCycles);

	if (InstancePtr->DmaMode == MDMA) {

		/*
		 * Enable DMA boundary Interrupt in Interrupt Status
		 * Enable Register
		 */
		XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_INTR_STS_EN_OFFSET,
			XNANDPS8_INTR_STS_EN_TRANS_COMP_STS_EN_MASK |
			XNANDPS8_INTR_STS_EN_DMA_INT_STS_EN_MASK);

	} else {
		/*
		 * Enable Buffer Read Ready Interrupt in Interrupt Status
		 * Enable Register
		 */
		XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_INTR_STS_EN_OFFSET,
			XNANDPS8_INTR_STS_EN_BUFF_RD_RDY_STS_EN_MASK);

	}
	/*
	 * Enable Single bit error and Multi bit error
	 */
	if (InstancePtr->EccMode == HWECC) {
		/*
		 * Interrupt Status Enable Register
		 */
		XNandPs8_IntrStsEnable(InstancePtr,
			(XNANDPS8_INTR_STS_EN_MUL_BIT_ERR_STS_EN_MASK |
			XNANDPS8_INTR_STS_EN_ERR_INTR_STS_EN_MASK));
	}
	/*
	 * Program Page Size
	 */
	XNandPs8_SetPageSize(InstancePtr);
	/*
	 * Program Column, Page, Block address
	 */
	XNandPs8_SetPageColAddr(InstancePtr, Page, (u16)Col);
	/*
	 * Program Packet Size and Packet Count
	 */
	XNandPs8_SetPktSzCnt(InstancePtr, PktSize, PktCount);
	/*
	 * Program DMA system address and DMA buffer boundary
	 */
	if (InstancePtr->DmaMode == MDMA) {
		/*
		 * Invalidate the Data Cache
		 */
		Xil_DCacheInvalidateRange(Buf, (PktSize * PktCount));

#ifdef __aarch64__
		XNandPs8_WriteReg(InstancePtr->Config.BaseAddress,
				XNANDPS8_DMA_SYS_ADDR1_OFFSET,
				(u32) (((INTPTR)(void *)Buf >> 32) &
				0xFFFFFFFFU));
#endif
		XNandPs8_WriteReg(InstancePtr->Config.BaseAddress,
				XNANDPS8_DMA_SYS_ADDR0_OFFSET,
				(u32) ((INTPTR)(void *)Buf & 0xFFFFFFFFU));
	}
	/*
	 * Set Bus Width
	 */
	XNandPs8_SetBusWidth(InstancePtr);
	/*
	 * Program Memory Address Register2 for chip select
	 */
	XNandPs8_SelectChip(InstancePtr, Target);
	/*
	 * Set ECC
	 */
	if (InstancePtr->EccMode == HWECC) {
		XNandPs8_SetEccSpareCmd(InstancePtr,
					(ONFI_CMD_CHNG_RD_COL1 |
					(ONFI_CMD_CHNG_RD_COL2 << (u8)8U)),
					InstancePtr->Geometry.ColAddrCycles);
	}

	/*
	 * Set Read command in Program Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
				XNANDPS8_PROG_OFFSET,XNANDPS8_PROG_RD_MASK);

	if (InstancePtr->DmaMode == MDMA) {
		goto ReadDmaDone;
	}

	while (BufRdCnt < PktCount) {
		/*
		 * Poll for Buffer Read Ready event
		 */
		Status = XNandPs8_PollRegTimeout(
			InstancePtr,
			XNANDPS8_INTR_STS_OFFSET,
			XNANDPS8_INTR_STS_BUFF_RD_RDY_STS_EN_MASK,
			XNANDPS8_INTR_POLL_TIMEOUT);
		if (Status != XST_SUCCESS) {
#ifdef XNANDPS8_DEBUG
			xil_printf("%s: Poll for buf read ready timeout\r\n",
							__func__);
#endif
			goto CheckEccError;
		}
		/*
		 * Increment Buffer Read Interrupt Count
		 */
		BufRdCnt++;

		if (BufRdCnt == PktCount) {
			/*
			 * Enable Transfer Complete Interrupt in Interrupt
			 * Status Enable Register
			 */
			XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
				XNANDPS8_INTR_STS_EN_OFFSET,
				XNANDPS8_INTR_STS_EN_TRANS_COMP_STS_EN_MASK);
		} else {
			/*
			 * Clear Buffer Read Ready Interrupt in Interrupt
			 * Status Enable Register
			 */
			XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
					XNANDPS8_INTR_STS_EN_OFFSET, 0U);
		}
		/*
		 * Clear Buffer Read Ready Interrupt in Interrupt Status
		 * Register
		 */
		XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
				XNANDPS8_INTR_STS_OFFSET,
				XNANDPS8_INTR_STS_BUFF_RD_RDY_STS_EN_MASK);
		/*
		 * Read Packet Data from Data Port Register
		 */
		for (Index = 0U; Index < (PktSize/4); Index++) {
			BufPtr[Index] = XNandPs8_ReadReg(
					InstancePtr->Config.BaseAddress,
					XNANDPS8_BUF_DATA_PORT_OFFSET);
		}
		BufPtr += (PktSize/4);

		if (BufRdCnt < PktCount) {
			/*
			 * Enable Buffer Read Ready Interrupt in Interrupt
			 * Status Enable Register
			 */
			XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
				XNANDPS8_INTR_STS_EN_OFFSET,
				XNANDPS8_INTR_STS_EN_BUFF_RD_RDY_STS_EN_MASK);

		} else {
			break;
		}
	}
ReadDmaDone:
	/*
	 * Poll for Transfer Complete event
	 */
	Status = XNandPs8_PollRegTimeout(
			InstancePtr,
			XNANDPS8_INTR_STS_OFFSET,
			XNANDPS8_INTR_STS_TRANS_COMP_STS_EN_MASK,
			XNANDPS8_INTR_POLL_TIMEOUT);
	if (Status != XST_SUCCESS) {
#ifdef XNANDPS8_DEBUG
		xil_printf("%s: Poll for xfer complete timeout\r\n",
							__func__);
#endif
		goto CheckEccError;
	}
	/*
	 * Clear Transfer Complete Interrupt in Interrupt Status Enable
	 * Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_INTR_STS_EN_OFFSET, 0U);

	/*
	 * Clear Transfer Complete Interrupt in Interrupt Status Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_INTR_STS_OFFSET,
			XNANDPS8_INTR_STS_TRANS_COMP_STS_EN_MASK);

CheckEccError:
	/*
	 * Check ECC Errors
	 */
	if (InstancePtr->EccMode == HWECC) {
		/*
		 * Hamming Multi Bit Errors
		 */
		if (((u32)XNandPs8_ReadReg(InstancePtr->Config.BaseAddress,
				XNANDPS8_INTR_STS_OFFSET) &
			(u32)XNANDPS8_INTR_STS_MUL_BIT_ERR_STS_EN_MASK) != 0U) {

			XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
				XNANDPS8_INTR_STS_OFFSET,
				XNANDPS8_INTR_STS_MUL_BIT_ERR_STS_EN_MASK);

#ifdef XNANDPS8_DEBUG
			xil_printf("%s: ECC Hamming multi bit error\r\n",
							__func__);
#endif
			Status = XST_FAILURE;
		}
		/*
		 * Hamming Single Bit or BCH Errors
		 */
		if (((u32)XNandPs8_ReadReg(InstancePtr->Config.BaseAddress,
				XNANDPS8_INTR_STS_OFFSET) &
			(u32)XNANDPS8_INTR_STS_ERR_INTR_STS_EN_MASK) != 0U) {

			XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
					XNANDPS8_INTR_STS_OFFSET,
					XNANDPS8_INTR_STS_ERR_INTR_STS_EN_MASK);

			if (InstancePtr->EccCfg.IsBCH == 1U) {
				InstancePtr->BCH_Error_Status++;

				Status = XST_SUCCESS;
			}
		}
	}
Out:
	return Status;
}

/*****************************************************************************/
/**
*
* This function reads spare bytes from flash.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
* @param	Target is the chip select value.
* @param	Page is the page address value to read.
* @param	Buf is the data buffer to fill in.
*
* @return
*		- XST_SUCCESS if successful.
*		- XST_FAILURE if failed.
*
* @note		None
*
******************************************************************************/
s32 XNandPs8_ReadSpareBytes(XNandPs8 *InstancePtr, u32 Page, u8 *Buf)
{
	u32 AddrCycles = InstancePtr->Geometry.RowAddrCycles +
				InstancePtr->Geometry.ColAddrCycles;
	u32 Col = InstancePtr->Geometry.BytesPerPage;
	u32 Target = Page/InstancePtr->Geometry.NumTargetPages;
	u32 PktSize = InstancePtr->Geometry.SpareBytesPerPage;
	u32 PktCount = 1U;
	u32 BufRdCnt = 0U;
	u32 *BufPtr = (u32 *)(void *)Buf;
	s32 Status = XST_FAILURE;
	u32 Index;
	u32 PageVar = Page;

	/*
	 * Assert the input arguments.
	 */
	Xil_AssertNonvoid(InstancePtr != NULL);
	Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);
	Xil_AssertNonvoid(PageVar < InstancePtr->Geometry.NumPages);
	Xil_AssertNonvoid(Buf != NULL);

	PageVar %= InstancePtr->Geometry.NumTargetPages;

	if (InstancePtr->DmaMode == MDMA) {
		/*
		 * Enable Transfer Complete Interrupt in Interrupt Status
		 * Enable Register
		 */
		XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
				XNANDPS8_INTR_STS_EN_OFFSET,
				XNANDPS8_INTR_STS_EN_TRANS_COMP_STS_EN_MASK);
	} else {
		/*
		 * Enable Buffer Read Ready Interrupt in Interrupt Status
		 * Enable Register
		 */
		XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
				XNANDPS8_INTR_STS_EN_OFFSET,
				XNANDPS8_INTR_STS_EN_BUFF_RD_RDY_STS_EN_MASK);
	}
	/*
	 * Program Command
	 */
	XNandPs8_Prepare_Cmd(InstancePtr, ONFI_CMD_RD1, ONFI_CMD_RD2, 0U,
						1U, (u8)AddrCycles);
	/*
	 * Program Page Size
	 */
	XNandPs8_SetPageSize(InstancePtr);
	/*
	 * Program Column, Page, Block address
	 */
	XNandPs8_SetPageColAddr(InstancePtr, PageVar, (u16)Col);
	/*
	 * Program Packet Size and Packet Count
	 */
	XNandPs8_SetPktSzCnt(InstancePtr, PktSize, PktCount);
	/*
	 * Program DMA system address and DMA buffer boundary
	 */
	if (InstancePtr->DmaMode == MDMA) {

		/*
		 * Invalidate the Data Cache
		 */
		Xil_DCacheInvalidateRange(Buf, (PktSize * PktCount));
#ifdef __aarch64__
		XNandPs8_WriteReg(InstancePtr->Config.BaseAddress,
				XNANDPS8_DMA_SYS_ADDR1_OFFSET,
				(u32) (((INTPTR)(void *)Buf >> 32) &
				0xFFFFFFFFU));
#endif
		XNandPs8_WriteReg(InstancePtr->Config.BaseAddress,
				XNANDPS8_DMA_SYS_ADDR0_OFFSET,
				(u32) ((INTPTR)(void *)Buf & 0xFFFFFFFFU));
	}
	/*
	 * Set Bus Width
	 */
	XNandPs8_SetBusWidth(InstancePtr);
	/*
	 * Program Memory Address Register2 for chip select
	 */
	XNandPs8_SelectChip(InstancePtr, Target);
	/*
	 * Set Read command in Program Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
				XNANDPS8_PROG_OFFSET,XNANDPS8_PROG_RD_MASK);

	if (InstancePtr->DmaMode == MDMA) {
		goto ReadDmaDone;
	}

	while (BufRdCnt < PktCount) {
		/*
		 * Poll for Buffer Read Ready event
		 */
		Status = XNandPs8_PollRegTimeout(
			InstancePtr,
			XNANDPS8_INTR_STS_OFFSET,
			XNANDPS8_INTR_STS_BUFF_RD_RDY_STS_EN_MASK,
			XNANDPS8_INTR_POLL_TIMEOUT);
		if (Status != XST_SUCCESS) {
#ifdef XNANDPS8_DEBUG
			xil_printf("%s: Poll for buf read ready timeout\r\n",
							__func__);
#endif
			goto Out;
		}
		/*
		 * Increment Buffer Read Interrupt Count
		 */
		BufRdCnt++;

		if (BufRdCnt == PktCount) {
			/*
			 * Enable Transfer Complete Interrupt in Interrupt
			 * Status Enable Register
			 */
			XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
				XNANDPS8_INTR_STS_EN_OFFSET,
				XNANDPS8_INTR_STS_EN_TRANS_COMP_STS_EN_MASK);

		} else {
			/*
			 * Clear Buffer Read Ready Interrupt in Interrupt
			 * Status Enable Register
			 */
			XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
					XNANDPS8_INTR_STS_EN_OFFSET, 0U);

		}
		/*
		 * Clear Buffer Read Ready Interrupt in Interrupt Status
		 * Register
		 */
		XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
				XNANDPS8_INTR_STS_OFFSET,
				XNANDPS8_INTR_STS_BUFF_RD_RDY_STS_EN_MASK);
		/*
		 * Read Packet Data from Data Port Register
		 */
		for (Index = 0U; Index < (PktSize/4); Index++) {
			BufPtr[Index] = XNandPs8_ReadReg(
					InstancePtr->Config.BaseAddress,
					XNANDPS8_BUF_DATA_PORT_OFFSET);
		}
		BufPtr += (PktSize/4);

		if (BufRdCnt < PktCount) {
			/*
			 * Enable Buffer Read Ready Interrupt in Interrupt
			 * Status Enable Register
			 */
			XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
				XNANDPS8_INTR_STS_EN_OFFSET,
				XNANDPS8_INTR_STS_EN_BUFF_RD_RDY_STS_EN_MASK);
		} else {
			break;
		}
	}
ReadDmaDone:
	/*
	 * Poll for Transfer Complete event
	 */
	Status = XNandPs8_PollRegTimeout(
			InstancePtr,
			XNANDPS8_INTR_STS_OFFSET,
			XNANDPS8_INTR_STS_TRANS_COMP_STS_EN_MASK,
			XNANDPS8_INTR_POLL_TIMEOUT);
	if (Status != XST_SUCCESS) {
#ifdef XNANDPS8_DEBUG
		xil_printf("%s: Poll for xfer complete timeout\r\n",
							__func__);
#endif
		goto Out;
	}
	/*
	 * Clear Transfer Complete Interrupt in Interrupt Status Enable
	 * Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_INTR_STS_EN_OFFSET, 0U);

	/*
	 * Clear Transfer Complete Interrupt in Interrupt Status Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_INTR_STS_OFFSET,
				XNANDPS8_INTR_STS_TRANS_COMP_STS_EN_MASK);
Out:

	return Status;
}

/*****************************************************************************/
/**
*
* This function sends ONFI block erase command to the flash.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
* @param	Target is the chip select value.
* @param	Block is the block to erase.
*
* @return
*		- XST_SUCCESS if successful.
*		- XST_FAILURE if failed.
*
* @note		None
*
******************************************************************************/
s32 XNandPs8_EraseBlock(XNandPs8 *InstancePtr, u32 Target, u32 Block)
{
	s32 Status = XST_FAILURE;
	u32 AddrCycles = InstancePtr->Geometry.RowAddrCycles;
	u32 Page;
	u32 ErasePage;
	u32 EraseCol;

	/*
	 * Assert the input arguments.
	 */
	Xil_AssertNonvoid(InstancePtr != NULL);
	Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);
	Xil_AssertNonvoid(Target < XNANDPS8_MAX_TARGETS);
	Xil_AssertNonvoid(Block < InstancePtr->Geometry.NumBlocks);

	Page = Block * InstancePtr->Geometry.PagesPerBlock;
	ErasePage = (Page >> 16U) & 0xFFFFU;
	EraseCol = Page & 0xFFFFU;

	/*
	 * Enable Transfer Complete Interrupt in Interrupt Status Enable
	 * Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_INTR_STS_EN_OFFSET,
			XNANDPS8_INTR_STS_EN_TRANS_COMP_STS_EN_MASK);

	/*
	 * Program Command
	 */
	XNandPs8_Prepare_Cmd(InstancePtr, ONFI_CMD_BLK_ERASE1,
			ONFI_CMD_BLK_ERASE2, 0U , 0U, (u8)AddrCycles);
	/*
	 * Program Column, Page, Block address
	 */
	XNandPs8_SetPageColAddr(InstancePtr, ErasePage, (u16)EraseCol);
	/*
	 * Program Memory Address Register2 for chip select
	 */
	XNandPs8_SelectChip(InstancePtr, Target);
	/*
	 * Set Block Erase in Program Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_PROG_OFFSET,XNANDPS8_PROG_BLK_ERASE_MASK);
	/*
	 * Poll for Transfer Complete event
	 */
	Status = XNandPs8_PollRegTimeout(
			InstancePtr,
			XNANDPS8_INTR_STS_OFFSET,
			XNANDPS8_INTR_STS_TRANS_COMP_STS_EN_MASK,
			XNANDPS8_INTR_POLL_TIMEOUT);
	if (Status != XST_SUCCESS) {
#ifdef XNANDPS8_DEBUG
		xil_printf("%s: Poll for xfer complete timeout\r\n",
							__func__);
#endif
		goto Out;
	}
	/*
	 * Clear Transfer Complete Interrupt in Interrupt Status Enable
	 * Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_INTR_STS_EN_OFFSET, 0U);

	/*
	 * Clear Transfer Complete Interrupt in Interrupt Status Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_INTR_STS_OFFSET,
				XNANDPS8_INTR_STS_TRANS_COMP_STS_EN_MASK);

Out:
	return Status;
}

/*****************************************************************************/
/**
*
* This function sends ONFI Get Feature command to flash.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
* @param	Target is the chip select value.
* @param	Feature is the feature selector.
* @param	Buf is the buffer to fill feature value.
*
* @return
*		- XST_SUCCESS if successful.
*		- XST_FAILURE if failed.
*
* @note		None
*
******************************************************************************/
s32 XNandPs8_GetFeature(XNandPs8 *InstancePtr, u32 Target, u8 Feature,
								u8 *Buf)
{
	s32 Status;
	u32 Index;
	u32 PktSize = 4;
	u32 PktCount = 1;
	u32 *BufPtr = (u32 *)(void *)Buf;

	/*
	 * Assert the input arguments.
	 */
	Xil_AssertNonvoid(Buf != NULL);

	if (InstancePtr->DataInterface == NVDDR) {
		PktSize = 8U;
	}

	/*
	 * Enable Buffer Read Ready Interrupt in Interrupt Status
	 * Enable Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_INTR_STS_EN_OFFSET,
			XNANDPS8_INTR_STS_EN_BUFF_RD_RDY_STS_EN_MASK);
	/*
	 * Program Command
	 */
	XNandPs8_Prepare_Cmd(InstancePtr, ONFI_CMD_GET_FEATURES,
				ONFI_CMD_INVALID, 0U, 0U, 1U);
	/*
	 * Program Column, Page, Block address
	 */
	XNandPs8_SetPageColAddr(InstancePtr, 0x0U, Feature);
	/*
	 * Program Memory Address Register2 for chip select
	 */
	XNandPs8_SelectChip(InstancePtr, Target);
	/*
	 * Program Packet Size and Packet Count
	 */
	XNandPs8_SetPktSzCnt(InstancePtr, PktSize, PktCount);
	/*
	 * Set Read Parameter Page in Program Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_PROG_OFFSET,XNANDPS8_PROG_GET_FEATURES_MASK);
	/*
	 * Poll for Buffer Read Ready event
	 */
	Status = XNandPs8_PollRegTimeout(
		InstancePtr,
		XNANDPS8_INTR_STS_OFFSET,
		XNANDPS8_INTR_STS_BUFF_RD_RDY_STS_EN_MASK,
		XNANDPS8_INTR_POLL_TIMEOUT);
	if (Status != XST_SUCCESS) {
#ifdef XNANDPS8_DEBUG
		xil_printf("%s: Poll for buf read ready timeout\r\n",
							__func__);
#endif
		goto Out;
	}
	/*
	 * Clear Buffer Read Ready Interrupt in Interrupt Status Enable
	 * Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_INTR_STS_EN_OFFSET, 0U);

	/*
	 * Clear Buffer Read Ready Interrupt in Interrupt Status Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_INTR_STS_OFFSET,
				XNANDPS8_INTR_STS_BUFF_RD_RDY_STS_EN_MASK);
	/*
	 * Enable Transfer Complete Interrupt in Interrupt Status Enable
	 * Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_INTR_STS_EN_OFFSET,
			XNANDPS8_INTR_STS_EN_TRANS_COMP_STS_EN_MASK);

	/*
	 * Read Data from Data Port Register
	 */
	for (Index = 0U; Index < (PktSize/4U); Index++) {
		BufPtr[Index] = XNandPs8_ReadReg(
					InstancePtr->Config.BaseAddress,
					XNANDPS8_BUF_DATA_PORT_OFFSET);
	}
	/*
	 * Poll for Transfer Complete event
	 */
	Status = XNandPs8_PollRegTimeout(
			InstancePtr,
			XNANDPS8_INTR_STS_OFFSET,
			XNANDPS8_INTR_STS_TRANS_COMP_STS_EN_MASK,
			XNANDPS8_INTR_POLL_TIMEOUT);
	if (Status != XST_SUCCESS) {
#ifdef XNANDPS8_DEBUG
		xil_printf("%s: Poll for xfer complete timeout\r\n",
							__func__);
#endif
		goto Out;
	}
	/*
	 * Clear Transfer Complete Interrupt in Interrupt Status Enable
	 * Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_INTR_STS_EN_OFFSET, 0U);

	/*
	 * Clear Transfer Complete Interrupt in Interrupt Status Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_INTR_STS_OFFSET,
				XNANDPS8_INTR_STS_TRANS_COMP_STS_EN_MASK);

Out:
	return Status;
}

/*****************************************************************************/
/**
*
* This function sends ONFI Set Feature command to flash.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
* @param	Target is the chip select value.
* @param	Feature is the feature selector.
* @param	Buf is the feature value to send.
*
* @return
*		- XST_SUCCESS if successful.
*		- XST_FAILURE if failed.
*
* @note		None
*
******************************************************************************/
s32 XNandPs8_SetFeature(XNandPs8 *InstancePtr, u32 Target, u8 Feature,
								u8 *Buf)
{
	s32 Status;
	u32 Index;
	u32 PktSize = 4U;
	u32 PktCount = 1U;
	u32 *BufPtr = (u32 *)(void *)Buf;

	/*
	 * Assert the input arguments.
	 */
	Xil_AssertNonvoid(Buf != NULL);
	if (InstancePtr->DataInterface == NVDDR) {
		PktSize = 8U;
	}

	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_INTR_STS_EN_OFFSET, 0U);

	/*
	 * Enable Buffer Write Ready Interrupt in Interrupt Status
	 * Enable Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_INTR_STS_EN_OFFSET,
			XNANDPS8_INTR_STS_EN_BUFF_WR_RDY_STS_EN_MASK);

	/*
	 * Program Command
	 */
	XNandPs8_Prepare_Cmd(InstancePtr, ONFI_CMD_SET_FEATURES,
				ONFI_CMD_INVALID, 0U , 0U, 1U);
	/*
	 * Program Column, Page, Block address
	 */
	XNandPs8_SetPageColAddr(InstancePtr, 0x0U, Feature);
	/*
	 * Program Memory Address Register2 for chip select
	 */
	XNandPs8_SelectChip(InstancePtr, Target);
	/*
	 * Program Packet Size and Packet Count
	 */
	XNandPs8_SetPktSzCnt(InstancePtr, PktSize, PktCount);
	/*
	 * Set Read Parameter Page in Program Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_PROG_OFFSET,XNANDPS8_PROG_SET_FEATURES_MASK);
	/*
	 * Poll for Buffer Write Ready event
	 */
	Status = XNandPs8_PollRegTimeout(
		InstancePtr,
		XNANDPS8_INTR_STS_OFFSET,
		XNANDPS8_INTR_STS_BUFF_WR_RDY_STS_EN_MASK,
		XNANDPS8_INTR_POLL_TIMEOUT);
	if (Status != XST_SUCCESS) {
#ifdef XNANDPS8_DEBUG
		xil_printf("%s: Poll for buf write ready timeout\r\n",
							__func__);
#endif
		goto Out;
	}
	/*
	 * Clear Buffer Write Ready Interrupt in Interrupt Status Enable
	 * Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_INTR_STS_EN_OFFSET, 0U);

	/*
	 * Clear Buffer Write Ready Interrupt in Interrupt Status Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_INTR_STS_OFFSET,
				XNANDPS8_INTR_STS_BUFF_WR_RDY_STS_EN_MASK);
	/*
	 * Enable Transfer Complete Interrupt in Interrupt Status Enable
	 * Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			(XNANDPS8_INTR_STS_EN_OFFSET),
			XNANDPS8_INTR_STS_EN_TRANS_COMP_STS_EN_MASK);
	/*
	 * Write Data to Data Port Register
	 */
	for (Index = 0U; Index < (PktSize/4U); Index++) {
		XNandPs8_WriteReg(InstancePtr->Config.BaseAddress,
					XNANDPS8_BUF_DATA_PORT_OFFSET,
					BufPtr[Index]);
	}
	/*
	 * Poll for Transfer Complete event
	 */
	Status = XNandPs8_PollRegTimeout(
			InstancePtr,
			XNANDPS8_INTR_STS_OFFSET,
			XNANDPS8_INTR_STS_TRANS_COMP_STS_EN_MASK,
			XNANDPS8_INTR_POLL_TIMEOUT);
	if (Status != XST_SUCCESS) {
#ifdef XNANDPS8_DEBUG
		xil_printf("%s: Poll for xfer complete timeout\r\n",
							__func__);
#endif
		goto Out;
	}
	/*
	 * Clear Transfer Complete Interrupt in Interrupt Status Enable
	 * Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_INTR_STS_EN_OFFSET, 0U);

	/*
	 * Clear Transfer Complete Interrupt in Interrupt Status Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_INTR_STS_OFFSET,
				XNANDPS8_INTR_STS_TRANS_COMP_STS_EN_MASK);

Out:
	return Status;
}

/*****************************************************************************/
/**
*
* This function changes clock frequency of flash controller.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
* @param	ClockFreq is the clock frequency to change.
*
* @return
*		None
*
* @note		None
*
******************************************************************************/
static void XNandPs8_ChangeClockFreq(XNandPs8 *InstancePtr, u32 ClockFreq)
{
	/*
	 * Not implemented
	 */
}
/*****************************************************************************/
/**
*
* This function changes the data interface and timing mode.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
* @param	NewIntf is the new data interface.
* @param	NewMode is the new timing mode.
*
* @return
*		- XST_SUCCESS if successful.
*		- XST_FAILURE if failed.
*
* @note		None
*
******************************************************************************/
s32 XNandPs8_ChangeTimingMode(XNandPs8 *InstancePtr,
				XNandPs8_DataInterface NewIntf,
				XNandPs8_TimingMode NewMode)
{
	s32 Status;
	u32 Target;
	u32 Index;
	u32 Found = 0U;
	u32 RegVal;
	u8 Buf[4] = {0U};
	u32 *Feature = (u32 *)(void *)&Buf[0];
	const XNandPs8_TimingModeDesc *Desc = NULL;

	/*
	 * Assert the input arguments.
	 */
	Xil_AssertNonvoid(InstancePtr != NULL);
	Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

	/*
	 * Get current data interface type and timing mode
	 */
	XNandPs8_DataInterface CurIntf = InstancePtr->DataInterface;
	XNandPs8_TimingMode CurMode = InstancePtr->TimingMode;
	/*
	 * Find the timing mode descriptor
	 */
	for (Index = 0U; Index <
		(sizeof(TimingDesc)/sizeof(XNandPs8_TimingModeDesc)); Index++) {
		Desc = &TimingDesc[Index];
		if ((Desc->CurDataIntf == CurIntf) &&
			(Desc->NewDataIntf == NewIntf) &&
			(Desc->NewTimingMode == NewMode)) {
			Found = 1U;
			break;
		}
	}
	if ((!Found) != 0U) {
#ifdef XNANDPS8_DEBUG
		xil_printf("%s: Timing mode desc not found\r\n",__func__);
#endif
		Status = XST_FAILURE;
		goto Out;
	}
	/*
	 * Check if the flash is in same mode
	 */
	if ((CurIntf == NewIntf) && (CurMode == NewMode)) {
		Status = XST_SUCCESS;
		goto Out;
	}

	if ((CurIntf == NVDDR) && (NewIntf == SDR)) {
		/*
		 * Change the clock frequency
		 */
		XNandPs8_ChangeClockFreq(InstancePtr, Desc->ClockFreq);
		/*
		 * Issue Reset command
		 */
		for (Target = 0U; Target < InstancePtr->Geometry.NumTargets;
							Target++) {
			Status = XNandPs8_OnfiReset(InstancePtr, Target);
			if (Status != XST_SUCCESS) {
				goto Out;
			}
			/*
			 * Get Feature
			 */
			Status = XNandPs8_GetFeature(InstancePtr, Target,
							0x01U, &Buf[0]);
			if (Status != XST_SUCCESS) {
				goto Out;
			}
			/*
			 * Check SDR mode and Timing Mode 0
			 */
			if (Feature != 0x0U) {
				Status = XST_FAILURE;
				goto Out;
			}
		}
		InstancePtr->DataInterface = SDR;
		InstancePtr->TimingMode = SDR0;
		Status = XNandPs8_ChangeTimingMode(InstancePtr, NewIntf,
								NewMode);
		goto Out;
	}
	/*
	 * Set Feature
	 */
	for (Target = 0U; Target < InstancePtr->Geometry.NumTargets;
							Target++) {
		Status = XNandPs8_SetFeature(InstancePtr, Target, 0x01U,
						(u8 *)&Desc->FeatureVal);
		if (Status != XST_SUCCESS) {
			goto Out;
		}
	}
	/*
	 * Change the clock frequency
	 */
	if (Desc->ClockFreq > 0U) {
		XNandPs8_ChangeClockFreq(InstancePtr, Desc->ClockFreq);
	}
	/*
	 * Get Feature
	 */
	for (Target = 0U; Target < InstancePtr->Geometry.NumTargets;
							Target++) {
		Status = XNandPs8_GetFeature(InstancePtr, Target, 0x01U,
							&Buf[0]);
		if (Status != XST_SUCCESS) {
			goto Out;
		}
		/*
		 * Check if set_feature was successful
		 */
		if (*Feature != Desc->FeatureVal) {
			Status = XST_FAILURE;
			goto Out;
		}
	}
	InstancePtr->DataInterface = NewIntf;
	InstancePtr->TimingMode = NewMode;
	/*
	 * Update Data Interface Register
	 */
	RegVal = ((NewMode % 6U) << ((NewIntf == NVDDR) ? 3U : 0U)) |
			((u32)NewIntf << XNANDPS8_DATA_INTF_DATA_INTF_SHIFT);
	XNandPs8_WriteReg(InstancePtr->Config.BaseAddress,
				XNANDPS8_DATA_INTF_OFFSET, RegVal);

	Status = XST_SUCCESS;
Out:
	return Status;
}

/*****************************************************************************/
/**
*
* This function issues change read column and reads the data into buffer
* specified by user.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
* @param	Target is the chip select value.
* @param	Col is the coulmn address.
* @param	PktSize is the number of bytes to read.
* @param	PktCount is the number of transactions to read.
* @param	Buf is the data buffer to fill in.
*
* @return
*		- XST_SUCCESS if successful.
*		- XST_FAILURE if failed.
*
* @note		None
*
******************************************************************************/
static s32 XNandPs8_ChangeReadColumn(XNandPs8 *InstancePtr, u32 Target,
					u32 Col, u32 PktSize, u32 PktCount,
					u8 *Buf)
{
	u32 AddrCycles = InstancePtr->Geometry.ColAddrCycles;
	u32 BufRdCnt = 0U;
	u32 *BufPtr = (u32 *)(void *)Buf;
	s32 Status = XST_FAILURE;
	u32 Index;

	/*
	 * Assert the input arguments.
	 */
	Xil_AssertNonvoid(Target < XNANDPS8_MAX_TARGETS);
	Xil_AssertNonvoid(Buf != NULL);

	if (InstancePtr->DmaMode == MDMA) {
		/*
		 * Enable DMA boundary Interrupt in Interrupt Status
		 * Enable Register
		 */
		XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
				XNANDPS8_INTR_STS_EN_OFFSET,
				XNANDPS8_INTR_STS_EN_TRANS_COMP_STS_EN_MASK |
				XNANDPS8_INTR_STS_EN_DMA_INT_STS_EN_MASK);
	} else {
		/*
		 * Enable Buffer Read Ready Interrupt in Interrupt Status
		 * Enable Register
		 */
		XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
				XNANDPS8_INTR_STS_EN_OFFSET,
				XNANDPS8_INTR_STS_EN_BUFF_RD_RDY_STS_EN_MASK);
	}
	/*
	 * Program Command
	 */
	XNandPs8_Prepare_Cmd(InstancePtr, ONFI_CMD_CHNG_RD_COL1,
			ONFI_CMD_CHNG_RD_COL2, 0U , 1U, (u8)AddrCycles);
	/*
	 * Program Page Size
	 */
	XNandPs8_SetPageSize(InstancePtr);
	/*
	 * Program Column, Page, Block address
	 */
	XNandPs8_SetPageColAddr(InstancePtr, 0U, (u16)Col);
	/*
	 * Program Packet Size and Packet Count
	 */
	XNandPs8_SetPktSzCnt(InstancePtr, PktSize, PktCount);
	/*
	 * Program DMA system address and DMA buffer boundary
	 */
	if (InstancePtr->DmaMode == MDMA) {
		/*
		 * Invalidate the Data Cache
		 */
		Xil_DCacheInvalidateRange(Buf, (PktSize * PktCount));
#ifdef __aarch64__
		XNandPs8_WriteReg(InstancePtr->Config.BaseAddress,
				XNANDPS8_DMA_SYS_ADDR1_OFFSET,
				(u32) (((INTPTR)Buf >> 32) & 0xFFFFFFFFU));
#endif
		XNandPs8_WriteReg(InstancePtr->Config.BaseAddress,
				XNANDPS8_DMA_SYS_ADDR0_OFFSET,
				(u32) ((INTPTR)(void *)Buf & 0xFFFFFFFFU));
	}
	/*
	 * Set Bus Width
	 */
	XNandPs8_SetBusWidth(InstancePtr);
	/*
	 * Program Memory Address Register2 for chip select
	 */
	XNandPs8_SelectChip(InstancePtr, Target);
	/*
	 * Set Read command in Program Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_PROG_OFFSET,XNANDPS8_PROG_RD_MASK);

	if (InstancePtr->DmaMode == MDMA) {
		goto ReadDmaDone;
	}

	while (BufRdCnt < PktCount) {
		/*
		 * Poll for Buffer Read Ready event
		 */
		Status = XNandPs8_PollRegTimeout(
			InstancePtr,
			XNANDPS8_INTR_STS_OFFSET,
			XNANDPS8_INTR_STS_BUFF_RD_RDY_STS_EN_MASK,
			XNANDPS8_INTR_POLL_TIMEOUT);
		if (Status != XST_SUCCESS) {
#ifdef XNANDPS8_DEBUG
			xil_printf("%s: Poll for buf read ready timeout\r\n",
							__func__);
#endif
			goto Out;
		}
		/*
		 * Increment Buffer Read Interrupt Count
		 */
		BufRdCnt++;

		if (BufRdCnt == PktCount) {
			/*
			 * Enable Transfer Complete Interrupt in Interrupt
			 * Status Enable Register
			 */
			XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
				XNANDPS8_INTR_STS_EN_OFFSET,
				XNANDPS8_INTR_STS_EN_TRANS_COMP_STS_EN_MASK);
		} else {
			/*
			 * Clear Buffer Read Ready Interrupt in Interrupt
			 * Status Enable Register
			 */
			XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
				XNANDPS8_INTR_STS_EN_OFFSET, 0U);

		}
		/*
		 * Clear Buffer Read Ready Interrupt in Interrupt Status
		 * Register
		 */
		XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
				XNANDPS8_INTR_STS_OFFSET,
				XNANDPS8_INTR_STS_BUFF_RD_RDY_STS_EN_MASK);
		/*
		 * Read Packet Data from Data Port Register
		 */
		for (Index = 0U; Index < (PktSize/4); Index++) {
			BufPtr[Index] = XNandPs8_ReadReg(
						InstancePtr->Config.BaseAddress,
						XNANDPS8_BUF_DATA_PORT_OFFSET);
		}
		BufPtr += (PktSize/4U);

		if (BufRdCnt < PktCount) {
			/*
			 * Enable Buffer Read Ready Interrupt in Interrupt
			 * Status Enable Register
			 */
			XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
				XNANDPS8_INTR_STS_EN_OFFSET,
				XNANDPS8_INTR_STS_EN_BUFF_RD_RDY_STS_EN_MASK);
		} else {
			break;
		}
	}
ReadDmaDone:
	/*
	 * Poll for Transfer Complete event
	 */
	Status = XNandPs8_PollRegTimeout(
			InstancePtr,
			XNANDPS8_INTR_STS_OFFSET,
			XNANDPS8_INTR_STS_TRANS_COMP_STS_EN_MASK,
			XNANDPS8_INTR_POLL_TIMEOUT);
	if (Status != XST_SUCCESS) {
#ifdef XNANDPS8_DEBUG
		xil_printf("%s: Poll for xfer complete timeout\r\n",
							__func__);
#endif
		goto Out;
	}
	/*
	 * Clear Transfer Complete Interrupt in Interrupt Status Enable
	 * Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
		XNANDPS8_INTR_STS_EN_OFFSET, 0U);

	/*
	 * Clear Transfer Complete Interrupt in Interrupt Status Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
				XNANDPS8_INTR_STS_OFFSET,
				XNANDPS8_INTR_STS_TRANS_COMP_STS_EN_MASK);
Out:
	return Status;
}

/*****************************************************************************/
/**
*
* This function issues change read column and reads the data into buffer
* specified by user.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
* @param	Target is the chip select value.
* @param	Col is the coulmn address.
* @param	PktSize is the number of bytes to read.
* @param	PktCount is the number of transactions to read.
* @param	Buf is the data buffer to fill in.
*
* @return
*		- XST_SUCCESS if successful.
*		- XST_FAILURE if failed.
*
* @note		None
*
******************************************************************************/
static s32 XNandPs8_ChangeWriteColumn(XNandPs8 *InstancePtr, u32 Target,
					u32 Col, u32 PktSize, u32 PktCount,
					u8 *Buf)
{
	u32 AddrCycles = InstancePtr->Geometry.ColAddrCycles;
	u32 BufWrCnt = 0U;
	u32 *BufPtr = (u32 *)(void *)Buf;
	s32 Status = XST_FAILURE;
	OnfiCmdFormat OnfiCommand;
	u32 Index;

	/*
	 * Assert the input arguments.
	 */
	Xil_AssertNonvoid(Target < XNANDPS8_MAX_TARGETS);
	Xil_AssertNonvoid(Buf != NULL);

	if (PktCount == 0U) {
		return XST_SUCCESS;
	}

	if (InstancePtr->DmaMode == MDMA) {
		/*
		 * Enable DMA boundary Interrupt in Interrupt Status
		 * Enable Register
		 */
		XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
				XNANDPS8_INTR_STS_EN_OFFSET,
				XNANDPS8_INTR_STS_EN_TRANS_COMP_STS_EN_MASK |
				XNANDPS8_INTR_STS_EN_DMA_INT_STS_EN_MASK);
	} else {
		/*
		 * Enable Buffer Write Ready Interrupt in Interrupt Status
		 * Enable Register
		 */
		XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
				XNANDPS8_INTR_STS_EN_OFFSET,
				XNANDPS8_INTR_STS_EN_BUFF_WR_RDY_STS_EN_MASK);
	}
	/*
	 * Change write column hack
	 */
	OnfiCommand.Command1 = 0x85U;
	OnfiCommand.Command2 = 0x10U;
	XNandPs8_Prepare_Cmd(InstancePtr, OnfiCommand.Command1,
				OnfiCommand.Command2, 0U , 0U, (u8)AddrCycles);

	/*
	 * Program Page Size
	 */
	XNandPs8_SetPageSize(InstancePtr);
	/*
	 * Program Column, Page, Block address
	 */
	XNandPs8_SetPageColAddr(InstancePtr, 0U, (u16)Col);
	/*
	 * Program Packet Size and Packet Count
	 */
	XNandPs8_SetPktSzCnt(InstancePtr, PktSize, PktCount);
	/*
	 * Program DMA system address and DMA buffer boundary
	 */
	if (InstancePtr->DmaMode == MDMA) {
#ifdef __aarch64__
		XNandPs8_WriteReg(InstancePtr->Config.BaseAddress,
				XNANDPS8_DMA_SYS_ADDR1_OFFSET,
				(u32) (((INTPTR)Buf >> 32U) & 0xFFFFFFFFU));
#endif
		XNandPs8_WriteReg(InstancePtr->Config.BaseAddress,
				XNANDPS8_DMA_SYS_ADDR0_OFFSET,
				(u32) ((INTPTR)(void *)Buf & 0xFFFFFFFFU));
	}
	/*
	 * Set Bus Width
	 */
	XNandPs8_SetBusWidth(InstancePtr);
	/*
	 * Program Memory Address Register2 for chip select
	 */
	XNandPs8_SelectChip(InstancePtr, Target);
	/*
	 * Set Page Program in Program Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
		XNANDPS8_PROG_OFFSET,XNANDPS8_PROG_CHNG_ROW_ADDR_END_MASK);

	if (InstancePtr->DmaMode == MDMA) {
		goto WriteDmaDone;
	}

	while (BufWrCnt < PktCount) {
		/*
		 * Poll for Buffer Write Ready event
		 */
		Status = XNandPs8_PollRegTimeout(
			InstancePtr,
			XNANDPS8_INTR_STS_OFFSET,
			XNANDPS8_INTR_STS_BUFF_WR_RDY_STS_EN_MASK,
			XNANDPS8_INTR_POLL_TIMEOUT);
		if (Status != XST_SUCCESS) {
#ifdef XNANDPS8_DEBUG
			xil_printf("%s: Poll for buf write ready timeout\r\n",
							__func__);
#endif
			goto Out;
		}
		/*
		 * Increment Buffer Write Interrupt Count
		 */
		BufWrCnt++;

		if (BufWrCnt == PktCount) {
			/*
			 * Enable Transfer Complete Interrupt in Interrupt
			 * Status Enable Register
			 */
			XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
				XNANDPS8_INTR_STS_EN_OFFSET,
				XNANDPS8_INTR_STS_EN_TRANS_COMP_STS_EN_MASK);
		} else {
			/*
			 * Clear Buffer Write Ready Interrupt in Interrupt
			 * Status Enable Register
			 */
			XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
				XNANDPS8_INTR_STS_EN_OFFSET, 0U);
		}
		/*
		 * Clear Buffer Write Ready Interrupt in Interrupt Status
		 * Register
		 */
		XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
				XNANDPS8_INTR_STS_OFFSET,
				XNANDPS8_INTR_STS_BUFF_WR_RDY_STS_EN_MASK);
		/*
		 * Write Packet Data to Data Port Register
		 */
		for (Index = 0U; Index < (PktSize/4U); Index++) {
			XNandPs8_WriteReg(InstancePtr->Config.BaseAddress,
						XNANDPS8_BUF_DATA_PORT_OFFSET,
						BufPtr[Index]);
		}
		BufPtr += (PktSize/4U);

		if (BufWrCnt < PktCount) {
			/*
			 * Enable Buffer Write Ready Interrupt in Interrupt
			 * Status Enable Register
			 */
			XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
				XNANDPS8_INTR_STS_EN_OFFSET,
				XNANDPS8_INTR_STS_EN_BUFF_WR_RDY_STS_EN_MASK);
		} else {
			break;
		}
	}
WriteDmaDone:
	/*
	 * Poll for Transfer Complete event
	 */
	Status = XNandPs8_PollRegTimeout(
			InstancePtr,
			XNANDPS8_INTR_STS_OFFSET,
			XNANDPS8_INTR_STS_TRANS_COMP_STS_EN_MASK,
			XNANDPS8_INTR_POLL_TIMEOUT);
	if (Status != XST_SUCCESS) {
#ifdef XNANDPS8_DEBUG
		xil_printf("%s: Poll for xfer complete timeout\r\n",
							__func__);
#endif
		goto Out;
	}
	/*
	 * Clear Transfer Complete Interrupt in Interrupt Status Enable
	 * Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
		XNANDPS8_INTR_STS_EN_OFFSET, 0U);

	/*
	 * Clear Transfer Complete Interrupt in Interrupt Status Register
	 */
	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
		XNANDPS8_INTR_STS_OFFSET,
			XNANDPS8_INTR_STS_TRANS_COMP_STS_EN_MASK);

Out:
	return Status;
}

/*****************************************************************************/
/**
*
* This function initializes extended parameter page ECC information.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
* @param	ExtPrm is the Extended parameter page buffer.
*
* @return
*		- XST_SUCCESS if successful.
*		- XST_FAILURE if failed.
*
* @note		None
*
******************************************************************************/
static s32 XNandPs8_InitExtEcc(XNandPs8 *InstancePtr, OnfiExtPrmPage *ExtPrm)
{
	s32 Status = XST_FAILURE;
	u32 Index;
	u32 SectionType;
	u32 SectionLen;
	u32 Offset = 0U;
	u32 Found = 0U;
	OnfiExtEccBlock *EccBlock;

	if (ExtPrm->Section0Type != 0x2U) {
		Offset += (u32)ExtPrm->Section0Len;
		if (ExtPrm->Section1Type != 0x2U) {
#ifdef XNANDPS8_DEBUG
		xil_printf("%s: Extended ECC section not found\r\n",__func__);
#endif
			Status = XST_FAILURE;
		} else {
			Found = 1U;
		}
	} else {
		Found = 1U;
	}

	if (Found != 0U) {
		EccBlock = (OnfiExtEccBlock *)&ExtPrm->SectionData[Offset];
		Xil_AssertNonvoid(EccBlock != NULL);
		if (EccBlock->CodeWordSize == 0U) {
			Status = XST_FAILURE;
		} else {
			InstancePtr->Geometry.NumBitsECC =
						EccBlock->NumBitsEcc;
			InstancePtr->Geometry.EccCodeWordSize =
						(u32)EccBlock->CodeWordSize;
			Status = XST_SUCCESS;
		}
	}
	return Status;
}

/*****************************************************************************/
/**
*
* This function prepares command to be written into command register.
*
* @param	InstancePtr is a pointer to the XNandPs8 instance.
* @param	Cmd1 is the first Onfi Command.
* @param	Cmd1 is the second Onfi Command.
* @param	EccState is the flag to set Ecc State.
* @param	DmaMode is the flag to set DMA mode.
*
* @return
*		None
*
* @note		None
*
******************************************************************************/
void XNandPs8_Prepare_Cmd(XNandPs8 *InstancePtr, u8 Cmd1, u8 Cmd2, u8 EccState,
			u8 DmaMode, u8 AddrCycles)
{
	u32 RegValue = 0U;

	Xil_AssertVoid(InstancePtr != NULL);

	RegValue = (u32)Cmd1 | (((u32)Cmd2 << (u32)XNANDPS8_CMD_CMD2_SHIFT) &
			(u32)XNANDPS8_CMD_CMD2_MASK);

	if ((EccState != 0U) && (InstancePtr->EccMode == HWECC)) {
		RegValue |= 1U << XNANDPS8_CMD_ECC_ON_SHIFT;
	}

	if ((DmaMode != 0U) && (InstancePtr->DmaMode == MDMA)) {
		RegValue |= MDMA << XNANDPS8_CMD_DMA_EN_SHIFT;
	}

	if (AddrCycles != 0U) {
		RegValue |= (u32)AddrCycles <<
				(u32)XNANDPS8_CMD_ADDR_CYCLES_SHIFT;
	}

	XNandPs8_WriteReg((InstancePtr)->Config.BaseAddress,
			XNANDPS8_CMD_OFFSET, RegValue);
}
