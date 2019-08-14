/*
 * This file is part of the coreboot project.
 *
 * Copyright 2015 Google Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <device/pci_ids.h>
#include <device/pci_ops.h>
#include <fsp/api.h>
#include <soc/ramstage.h>
#include <soc/vr_config.h>
#include <console/console.h>

/* Default values for domain configuration. PSI3 and PSI4 are disabled. */
static const struct vr_config default_configs[NUM_VR_DOMAINS] = {
	[VR_SYSTEM_AGENT] = {
		.vr_config_enable = 1,
		.psi1threshold = VR_CFG_AMP(20),
		.psi2threshold = VR_CFG_AMP(4),
		.psi3threshold = VR_CFG_AMP(1),
		.psi3enable = 0,
		.psi4enable = 0,
		.imon_slope = 0x0,
		.imon_offset = 0x0,
		.icc_max = VR_CFG_AMP(7),
		.voltage_limit = 1520,
	},
	[VR_IA_CORE] = {
		.vr_config_enable = 1,
		.psi1threshold = VR_CFG_AMP(20),
		.psi2threshold = VR_CFG_AMP(5),
		.psi3threshold = VR_CFG_AMP(1),
		.psi3enable = 0,
		.psi4enable = 0,
		.imon_slope = 0x0,
		.imon_offset = 0x0,
		.icc_max = VR_CFG_AMP(34),
		.voltage_limit = 1520,
	},
#if CONFIG(PLATFORM_USES_FSP1_1)
	[VR_RING] = {
		.vr_config_enable = 1,
		.psi1threshold = VR_CFG_AMP(20),
		.psi2threshold = VR_CFG_AMP(5),
		.psi3threshold = VR_CFG_AMP(1),
		.psi3enable = 0,
		.psi4enable = 0,
		.imon_slope = 0x0,
		.imon_offset = 0x0,
		.icc_max = VR_CFG_AMP(34),
		.voltage_limit = 1520,
	},
#endif
	[VR_GT_UNSLICED] = {
		.vr_config_enable = 1,
		.psi1threshold = VR_CFG_AMP(20),
		.psi2threshold = VR_CFG_AMP(5),
		.psi3threshold = VR_CFG_AMP(1),
		.psi3enable = 0,
		.psi4enable = 0,
		.imon_slope = 0x0,
		.imon_offset = 0x0,
		.icc_max = VR_CFG_AMP(35),
		.voltage_limit = 1520,
	},
	[VR_GT_SLICED] = {
		.vr_config_enable = 1,
		.psi1threshold = VR_CFG_AMP(20),
		.psi2threshold = VR_CFG_AMP(5),
		.psi3threshold = VR_CFG_AMP(1),
		.psi3enable = 0,
		.psi4enable = 0,
		.imon_slope = 0x0,
		.imon_offset = 0x0,
		.icc_max = VR_CFG_AMP(35),
		.voltage_limit = 1520,
	},
};

static uint16_t get_sku_icc_max(int domain)
{
	static uint16_t mch_id = 0, igd_id = 0, lpc_id = 0;
	if (!mch_id) {
		struct device *dev = pcidev_path_on_root(SA_DEVFN_ROOT);
		mch_id = pci_read_config16(dev, PCI_DEVICE_ID);
	}
	if (!igd_id) {
		struct device *dev = pcidev_path_on_root(SA_DEVFN_IGD);
		igd_id = pci_read_config16(dev, PCI_DEVICE_ID);
	}
	if (!lpc_id) {
		struct device *dev = pcidev_path_on_root(PCH_DEVFN_LPC);
		lpc_id = pci_read_config16(dev, PCI_DEVICE_ID);
	}

	/*
	* Iccmax table from Doc #559100 Section 7.2 DC Specifications, the
	* Iccmax is the same among KBL-Y but KBL-U/R.
	* Addendum for AML-Y #594883, IccMax for IA core is 28A.
	* +----------------+-------------+---------------+------+-----+
	* | Domain/Setting |  SA         |  IA           | GTUS | GTS |
	* +----------------+-------------+---------------+------+-----+
	* | IccMax(KBL-U/R)| 6A(U42)     | 64A(U42)      | 31A  | 31A |
	* |                | 4.5A(Others)| 29A(P/C)      loadline|      |     |
	* |                |             | 32A(i3/i5)    |      |     |
	* +----------------+-------------+---------------+------+-----+
	* | IccMax(KBL-Y)  | 4.1A        | 24A           | 24A  | 24A |
	* +----------------+-------------+---------------+------+-----+
	* | IccMax(AML-Y)  | 4.1A        | 28A           | 24A  | 24A |
	* +----------------+-------------+---------------+------+-----+
	*/

	switch (mch_id) {
	case PCI_DEVICE_ID_INTEL_KBL_U_R: {
		static const uint16_t icc_max[NUM_VR_DOMAINS] = {
			VR_CFG_AMP(6),
			VR_CFG_AMP(64),
			VR_CFG_AMP(31),
			VR_CFG_AMP(31),
		};
		return icc_max[domain];
	}
	case PCI_DEVICE_ID_INTEL_KBL_ID_Y: {
		uint16_t icc_max[NUM_VR_DOMAINS] = {
			VR_CFG_AMP(4.1),
			VR_CFG_AMP(24),
			VR_CFG_AMP(24),
			VR_CFG_AMP(24),
		};

		if (igd_id == PCI_DEVICE_ID_INTEL_AML_GT2_ULX)
			icc_max[VR_IA_CORE] = VR_CFG_AMP(28);

		return icc_max[domain];
	}
	case PCI_DEVICE_ID_INTEL_KBL_ID_U: {
		uint16_t icc_max[NUM_VR_DOMAINS] = {
			VR_CFG_AMP(4.5),
			VR_CFG_AMP(32),
			VR_CFG_AMP(31),
			VR_CFG_AMP(31),
		};

		if (igd_id == PCI_DEVICE_ID_INTEL_SPT_LP_U_BASE_HDCP22)
			icc_max[VR_IA_CORE] = VR_CFG_AMP(29);

		return icc_max[domain];
	}
	default:
		printk(BIOS_ERR, "ERROR: Unknown MCH in VR-config\n");
	}
	return 0;
}

void fill_vr_domain_config(void *params,
		int domain, const struct vr_config *chip_cfg)
{
	FSP_SIL_UPD *vr_params = (FSP_SIL_UPD *)params;
	const struct vr_config *cfg;

	if (domain < 0 || domain >= NUM_VR_DOMAINS)
		return;

	/* Use device tree override if requested. */
	if (chip_cfg->vr_config_enable)
		cfg = chip_cfg;
	else
		cfg = &default_configs[domain];

	vr_params->VrConfigEnable[domain] = cfg->vr_config_enable;
	vr_params->Psi1Threshold[domain] = cfg->psi1threshold;
	vr_params->Psi2Threshold[domain] = cfg->psi2threshold;
	vr_params->Psi3Threshold[domain] = cfg->psi3threshold;
	vr_params->Psi3Enable[domain] = cfg->psi3enable;
	vr_params->Psi4Enable[domain] = cfg->psi4enable;
	vr_params->ImonSlope[domain] = cfg->imon_slope;
	vr_params->ImonOffset[domain] = cfg->imon_offset;
	/* If board provided non-zero value, use it. */
	if (cfg->icc_max)
		vr_params->IccMax[domain] = cfg->icc_max;
	else
		vr_params->IccMax[domain] = get_sku_icc_max(domain);
	vr_params->VrVoltageLimit[domain] = cfg->voltage_limit;

#if CONFIG(PLATFORM_USES_FSP2_0)
	vr_params->AcLoadline[domain] = cfg->ac_loadline;
	vr_params->DcLoadline[domain] = cfg->dc_loadline;
#endif
}
