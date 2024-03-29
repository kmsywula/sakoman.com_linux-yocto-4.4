/*
 * Intel Skylake I2S Machine Driver for NAU88L25+SSM4567
 *
 * Copyright (C) 2015, Intel Corporation. All rights reserved.
 *
 * Modified from:
 *   Intel Skylake I2S Machine Driver for NAU88L25 and SSM4567
 *
 *   Copyright (C) 2015, Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/pcm_params.h>
#include "../../codecs/nau8825.h"
#include "../../codecs/hdac_hdmi.h"

#define SKL_NUVOTON_CODEC_DAI	"nau8825-hifi"
#define SKL_SSM_CODEC_DAI	"ssm4567-hifi"

static struct snd_soc_jack skylake_headset;
static struct snd_soc_card skylake_audio_card;

enum {
	SKL_DPCM_AUDIO_PB = 0,
	SKL_DPCM_AUDIO_CP,
	SKL_DPCM_AUDIO_REF_CP,
	SKL_DPCM_AUDIO_DMIC_CP,
	SKL_DPCM_AUDIO_HDMI1_PB,
	SKL_DPCM_AUDIO_HDMI2_PB,
	SKL_DPCM_AUDIO_HDMI3_PB,
};

static inline struct snd_soc_dai *skl_get_codec_dai(struct snd_soc_card *card)
{
	struct snd_soc_pcm_runtime *rtd;

	list_for_each_entry(rtd, &card->rtd_list, list) {

		if (!strncmp(rtd->codec_dai->name, SKL_NUVOTON_CODEC_DAI,
			     strlen(SKL_NUVOTON_CODEC_DAI)))
			return rtd->codec_dai;
	}

	return NULL;
}

static const struct snd_kcontrol_new skylake_controls[] = {
	SOC_DAPM_PIN_SWITCH("Headphone Jack"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
	SOC_DAPM_PIN_SWITCH("Left Speaker"),
	SOC_DAPM_PIN_SWITCH("Right Speaker"),
};

static int platform_clock_control(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int  event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;
	struct snd_soc_dai *codec_dai;
	int ret;

	codec_dai = skl_get_codec_dai(card);
	if (!codec_dai) {
		dev_err(card->dev, "Codec dai not found\n");
		return -EIO;
	}

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		ret = snd_soc_dai_set_sysclk(codec_dai,
				NAU8825_CLK_MCLK, 24000000, SND_SOC_CLOCK_IN);
		if (ret < 0) {
			dev_err(card->dev, "set sysclk err = %d\n", ret);
			return -EIO;
		}
	} else {
		ret = snd_soc_dai_set_sysclk(codec_dai,
				NAU8825_CLK_INTERNAL, 0, SND_SOC_CLOCK_IN);
		if (ret < 0) {
			dev_err(card->dev, "set sysclk err = %d\n", ret);
			return -EIO;
		}
	}
	return ret;
}

static const struct snd_soc_dapm_widget skylake_widgets[] = {
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_SPK("Left Speaker", NULL),
	SND_SOC_DAPM_SPK("Right Speaker", NULL),
	SND_SOC_DAPM_MIC("SoC DMIC", NULL),
	SND_SOC_DAPM_SPK("DP", NULL),
	SND_SOC_DAPM_SPK("HDMI", NULL),
	SND_SOC_DAPM_SUPPLY("Platform Clock", SND_SOC_NOPM, 0, 0,
			platform_clock_control, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMD),
};

static const struct snd_soc_dapm_route skylake_map[] = {
	/* HP jack connectors - unknown if we have jack detection */
	{"Headphone Jack", NULL, "HPOL"},
	{"Headphone Jack", NULL, "HPOR"},

	/* speaker */
	{"Left Speaker", NULL, "Left OUT"},
	{"Right Speaker", NULL, "Right OUT"},

	/* other jacks */
	{"MIC", NULL, "Headset Mic"},
	{"DMic", NULL, "SoC DMIC"},

	{"HDMI", NULL, "hif5 Output"},
	{"DP", NULL, "hif6 Output"},
	/* CODEC BE connections */
	{ "Left Playback", NULL, "ssp0 Tx"},
	{ "Right Playback", NULL, "ssp0 Tx"},
	{ "ssp0 Tx", NULL, "codec0_out"},

	/* IV feedback path */
	{ "codec0_lp_in", NULL, "ssp0 Rx"},
	{ "ssp0 Rx", NULL, "Left Capture Sense" },
	{ "ssp0 Rx", NULL, "Right Capture Sense" },

	{ "Playback", NULL, "ssp1 Tx"},
	{ "ssp1 Tx", NULL, "codec1_out"},

	{ "codec0_in", NULL, "ssp1 Rx" },
	{ "ssp1 Rx", NULL, "Capture" },

	/* DMIC */
	{ "dmic01_hifi", NULL, "DMIC01 Rx" },
	{ "DMIC01 Rx", NULL, "DMIC AIF" },

	{ "hifi3", NULL, "iDisp3 Tx"},
	{ "iDisp3 Tx", NULL, "iDisp3_out"},
	{ "hifi2", NULL, "iDisp2 Tx"},
	{ "iDisp2 Tx", NULL, "iDisp2_out"},
	{ "hifi1", NULL, "iDisp1 Tx"},
	{ "iDisp1 Tx", NULL, "iDisp1_out"},

	{ "Headphone Jack", NULL, "Platform Clock" },
	{ "Headset Mic", NULL, "Platform Clock" },
};

static struct snd_soc_codec_conf ssm4567_codec_conf[] = {
	{
		.dev_name = "i2c-INT343B:00",
		.name_prefix = "Left",
	},
	{
		.dev_name = "i2c-INT343B:01",
		.name_prefix = "Right",
	},
};

static struct snd_soc_dai_link_component ssm4567_codec_components[] = {
	{ /* Left */
		.name = "i2c-INT343B:00",
		.dai_name = SKL_SSM_CODEC_DAI,
	},
	{ /* Right */
		.name = "i2c-INT343B:01",
		.dai_name = SKL_SSM_CODEC_DAI,
	},
};

static int skylake_ssm4567_codec_init(struct snd_soc_pcm_runtime *rtd)
{
	int ret;

	/* Slot 1 for left */
	ret = snd_soc_dai_set_tdm_slot(rtd->codec_dais[0], 0x01, 0x01, 2, 48);
	if (ret < 0)
		return ret;

	/* Slot 2 for right */
	ret = snd_soc_dai_set_tdm_slot(rtd->codec_dais[1], 0x02, 0x02, 2, 48);
	if (ret < 0)
		return ret;

	return ret;
}

static int skylake_nau8825_codec_init(struct snd_soc_pcm_runtime *rtd)
{
	int ret;
	struct snd_soc_codec *codec = rtd->codec;

	/*
	 * 4 buttons here map to the google Reference headset
	 * The use of these buttons can be decided by the user space.
	 */
	ret = snd_soc_card_jack_new(&skylake_audio_card, "Headset Jack",
		SND_JACK_HEADSET | SND_JACK_BTN_0 | SND_JACK_BTN_1 |
		SND_JACK_BTN_2 | SND_JACK_BTN_3, &skylake_headset,
		NULL, 0);
	if (ret) {
		dev_err(rtd->dev, "Headset Jack creation failed %d\n", ret);
		return ret;
	}

	nau8825_enable_jack_detect(codec, &skylake_headset);

	snd_soc_dapm_ignore_suspend(&rtd->card->dapm, "SoC DMIC");

	return ret;
}

static int skylake_hdmi1_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *dai = rtd->codec_dai;

	return hdac_hdmi_jack_init(dai, SKL_DPCM_AUDIO_HDMI1_PB);
}

static int skylake_hdmi2_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *dai = rtd->codec_dai;

	return hdac_hdmi_jack_init(dai, SKL_DPCM_AUDIO_HDMI2_PB);
}


static int skylake_hdmi3_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *dai = rtd->codec_dai;

	return hdac_hdmi_jack_init(dai, SKL_DPCM_AUDIO_HDMI3_PB);
}

static int skylake_nau8825_fe_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dapm_context *dapm;
	struct snd_soc_component *component = rtd->cpu_dai->component;

	dapm = snd_soc_component_get_dapm(component);
	snd_soc_dapm_ignore_suspend(dapm, "Reference Capture");

	return 0;
}

static unsigned int rates[] = {
	48000,
};

static struct snd_pcm_hw_constraint_list constraints_rates = {
	.count = ARRAY_SIZE(rates),
	.list  = rates,
	.mask = 0,
};

static unsigned int channels[] = {
	2,
};

static struct snd_pcm_hw_constraint_list constraints_channels = {
	.count = ARRAY_SIZE(channels),
	.list = channels,
	.mask = 0,
};

static int skl_fe_startup(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	/*
	 * on this platform for PCM device we support,
	 *	48Khz
	 *	stereo
	 *	16 bit audio
	 */

	runtime->hw.channels_max = 2;
	snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_CHANNELS,
					   &constraints_channels);

	runtime->hw.formats = SNDRV_PCM_FMTBIT_S16_LE;
	snd_pcm_hw_constraint_msbits(runtime, 0, 16, 16);

	snd_pcm_hw_constraint_list(runtime, 0,
				SNDRV_PCM_HW_PARAM_RATE, &constraints_rates);

	return 0;
}

static const struct snd_soc_ops skylake_nau8825_fe_ops = {
	.startup = skl_fe_startup,
};

static int skylake_ssp_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);
	struct snd_mask *fmt = hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT);

	/* The ADSP will covert the FE rate to 48k, stereo */
	rate->min = rate->max = 48000;
	channels->min = channels->max = 2;

	/* set SSP0 to 24 bit */
	snd_mask_none(fmt);
	snd_mask_set(fmt, SNDRV_PCM_FORMAT_S24_LE);
	return 0;
}

static int skylake_dmic_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);
	if (params_channels(params) == 2)
		channels->min = channels->max = 2;
	else
		channels->min = channels->max = 4;

	return 0;
}

static int skylake_nau8825_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;

	ret = snd_soc_dai_set_sysclk(codec_dai,
			NAU8825_CLK_MCLK, 24000000, SND_SOC_CLOCK_IN);

	if (ret < 0)
		dev_err(rtd->dev, "snd_soc_dai_set_sysclk err = %d\n", ret);

	return ret;
}

static struct snd_soc_ops skylake_nau8825_ops = {
	.hw_params = skylake_nau8825_hw_params,
};

static unsigned int channels_dmic[] = {
	2, 4,
};

static struct snd_pcm_hw_constraint_list constraints_dmic_channels = {
	.count = ARRAY_SIZE(channels_dmic),
	.list = channels_dmic,
	.mask = 0,
};

static int skylake_dmic_startup(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	runtime->hw.channels_max = 4;
	snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_CHANNELS,
			&constraints_dmic_channels);

	return snd_pcm_hw_constraint_list(substream->runtime, 0,
			SNDRV_PCM_HW_PARAM_RATE, &constraints_rates);
}

static struct snd_soc_ops skylake_dmic_ops = {
	.startup = skylake_dmic_startup,
};

static unsigned int rates_16000[] = {
	16000,
};

static struct snd_pcm_hw_constraint_list constraints_16000 = {
	.count = ARRAY_SIZE(rates_16000),
	.list  = rates_16000,
};

static int skylake_refcap_startup(struct snd_pcm_substream *substream)
{
	return snd_pcm_hw_constraint_list(substream->runtime, 0,
			SNDRV_PCM_HW_PARAM_RATE,
			&constraints_16000);
}

static struct snd_soc_ops skylaye_refcap_ops = {
	.startup = skylake_refcap_startup,
};

/* skylake digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link skylake_dais[] = {
	/* Front End DAI links */
	[SKL_DPCM_AUDIO_PB] = {
		.name = "Skl Audio Port",
		.stream_name = "Audio",
		.cpu_dai_name = "System Pin",
		.platform_name = "0000:00:1f.3",
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.nonatomic = 1,
		.init = skylake_nau8825_fe_init,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_playback = 1,
		.ops = &skylake_nau8825_fe_ops,
	},
	[SKL_DPCM_AUDIO_CP] = {
		.name = "Skl Audio Capture Port",
		.stream_name = "Audio Record",
		.cpu_dai_name = "System Pin",
		.platform_name = "0000:00:1f.3",
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.nonatomic = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_capture = 1,
		.ops = &skylake_nau8825_fe_ops,
	},
	[SKL_DPCM_AUDIO_REF_CP] = {
		.name = "Skl Audio Reference cap",
		.stream_name = "Wake on Voice",
		.cpu_dai_name = "Reference Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:1f.3",
		.init = NULL,
		.dpcm_capture = 1,
		.nonatomic = 1,
		.dynamic = 1,
		.ops = &skylaye_refcap_ops,
	},
	[SKL_DPCM_AUDIO_DMIC_CP] = {
		.name = "Skl Audio DMIC cap",
		.stream_name = "dmiccap",
		.cpu_dai_name = "DMIC Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:1f.3",
		.init = NULL,
		.dpcm_capture = 1,
		.nonatomic = 1,
		.dynamic = 1,
		.ops = &skylake_dmic_ops,
	},
	[SKL_DPCM_AUDIO_HDMI1_PB] = {
		.name = "Skl HDMI Port1",
		.stream_name = "Hdmi1",
		.cpu_dai_name = "HDMI1 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:1f.3",
		.dpcm_playback = 1,
		.init = NULL,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.nonatomic = 1,
		.dynamic = 1,
	},
	[SKL_DPCM_AUDIO_HDMI2_PB] = {
		.name = "Skl HDMI Port2",
		.stream_name = "Hdmi2",
		.cpu_dai_name = "HDMI2 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:1f.3",
		.dpcm_playback = 1,
		.init = NULL,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.nonatomic = 1,
		.dynamic = 1,
	},
	[SKL_DPCM_AUDIO_HDMI3_PB] = {
		.name = "Skl HDMI Port3",
		.stream_name = "Hdmi3",
		.cpu_dai_name = "HDMI3 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:1f.3",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_playback = 1,
		.init = NULL,
		.nonatomic = 1,
		.dynamic = 1,
	},

	/* Back End DAI links */
	{
		/* SSP0 - Codec */
		.name = "SSP0-Codec",
		.be_id = 0,
		.cpu_dai_name = "SSP0 Pin",
		.platform_name = "0000:00:1f.3",
		.no_pcm = 1,
		.codecs = ssm4567_codec_components,
		.num_codecs = ARRAY_SIZE(ssm4567_codec_components),
		.dai_fmt = SND_SOC_DAIFMT_DSP_A |
			SND_SOC_DAIFMT_IB_NF |
			SND_SOC_DAIFMT_CBS_CFS,
		.init = skylake_ssm4567_codec_init,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = skylake_ssp_fixup,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		/* SSP1 - Codec */
		.name = "SSP1-Codec",
		.be_id = 1,
		.cpu_dai_name = "SSP1 Pin",
		.platform_name = "0000:00:1f.3",
		.no_pcm = 1,
		.codec_name = "i2c-10508825:00",
		.codec_dai_name = SKL_NUVOTON_CODEC_DAI,
		.init = skylake_nau8825_codec_init,
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBS_CFS,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = skylake_ssp_fixup,
		.ops = &skylake_nau8825_ops,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "dmic01",
		.be_id = 2,
		.cpu_dai_name = "DMIC01 Pin",
		.codec_name = "dmic-codec",
		.codec_dai_name = "dmic-hifi",
		.platform_name = "0000:00:1f.3",
		.ignore_suspend = 1,
		.be_hw_params_fixup = skylake_dmic_fixup,
		.dpcm_capture = 1,
		.no_pcm = 1,
	},
	{
		.name = "iDisp1",
		.be_id = 3,
		.cpu_dai_name = "iDisp1 Pin",
		.codec_name = "ehdaudio0D2",
		.codec_dai_name = "intel-hdmi-hifi1",
		.platform_name = "0000:00:1f.3",
		.dpcm_playback = 1,
		.init = skylake_hdmi1_init,
		.no_pcm = 1,
	},
	{
		.name = "iDisp2",
		.be_id = 4,
		.cpu_dai_name = "iDisp2 Pin",
		.codec_name = "ehdaudio0D2",
		.codec_dai_name = "intel-hdmi-hifi2",
		.platform_name = "0000:00:1f.3",
		.init = skylake_hdmi2_init,
		.dpcm_playback = 1,
		.no_pcm = 1,
	},
	{
		.name = "iDisp3",
		.be_id = 5,
		.cpu_dai_name = "iDisp3 Pin",
		.codec_name = "ehdaudio0D2",
		.codec_dai_name = "intel-hdmi-hifi3",
		.platform_name = "0000:00:1f.3",
		.init = skylake_hdmi3_init,
		.dpcm_playback = 1,
		.no_pcm = 1,
	},
};

/* skylake audio machine driver for SPT + NAU88L25 */
static struct snd_soc_card skylake_audio_card = {
	.name = "sklnau8825adi",
	.owner = THIS_MODULE,
	.dai_link = skylake_dais,
	.num_links = ARRAY_SIZE(skylake_dais),
	.controls = skylake_controls,
	.num_controls = ARRAY_SIZE(skylake_controls),
	.dapm_widgets = skylake_widgets,
	.num_dapm_widgets = ARRAY_SIZE(skylake_widgets),
	.dapm_routes = skylake_map,
	.num_dapm_routes = ARRAY_SIZE(skylake_map),
	.codec_conf = ssm4567_codec_conf,
	.num_configs = ARRAY_SIZE(ssm4567_codec_conf),
	.fully_routed = true,
};

static int skylake_audio_probe(struct platform_device *pdev)
{
	skylake_audio_card.dev = &pdev->dev;

	return devm_snd_soc_register_card(&pdev->dev, &skylake_audio_card);
}

static struct platform_driver skylake_audio = {
	.probe = skylake_audio_probe,
	.driver = {
		.name = "skl_nau88l25_ssm4567_i2s",
		.pm = &snd_soc_pm_ops,
	},
};

module_platform_driver(skylake_audio)

/* Module information */
MODULE_AUTHOR("Conrad Cooke  <conrad.cooke@intel.com>");
MODULE_AUTHOR("Harsha Priya <harshapriya.n@intel.com>");
MODULE_AUTHOR("Naveen M <naveen.m@intel.com>");
MODULE_AUTHOR("Sathya Prakash M R <sathya.prakash.m.r@intel.com>");
MODULE_AUTHOR("Yong Zhi <yong.zhi@intel.com>");
MODULE_DESCRIPTION("Intel Audio Machine driver for SKL with NAU88L25 and SSM4567 in I2S Mode");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:skl_nau88l25_ssm4567_i2s");
