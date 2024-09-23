// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/helpers.h"

#include "base/logging.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "media/webrtc/webrtc_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

constexpr webrtc::AudioProcessing::Config kDefaultApmConfig{};

webrtc::AudioProcessing::Config CreateApmGetConfig(
    const AudioProcessingSettings& settings) {
  rtc::scoped_refptr<webrtc::AudioProcessing> apm =
      CreateWebRtcAudioProcessingModule(settings);
  DCHECK(!!apm);
  return apm->GetConfig();
}

// TODO(b/260315490): Add missing tests with different `AudioProcessingSettings`
// values.

// Verify that the default settings in AudioProcessingSettings are applied
// correctly by `CreateWebRtcAudioProcessingModule()`.
TEST(CreateWebRtcAudioProcessingModuleTest, CheckDefaultAudioProcessingConfig) {
  auto config = CreateApmGetConfig(/*settings=*/{});

  EXPECT_TRUE(config.pipeline.multi_channel_render);
  EXPECT_TRUE(config.pipeline.multi_channel_capture);
  EXPECT_EQ(config.pipeline.maximum_internal_processing_rate, 48000);
  EXPECT_TRUE(config.high_pass_filter.enabled);
  EXPECT_FALSE(config.pre_amplifier.enabled);
  EXPECT_TRUE(config.echo_canceller.enabled);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
  EXPECT_FALSE(config.gain_controller1.enabled);
  EXPECT_TRUE(config.gain_controller2.enabled);
#elif BUILDFLAG(IS_CASTOS) || BUILDFLAG(IS_CAST_ANDROID)
  EXPECT_TRUE(config.gain_controller1.enabled);
  EXPECT_FALSE(config.gain_controller2.enabled);
#elif BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_FALSE(config.gain_controller1.enabled);
  EXPECT_TRUE(config.gain_controller2.enabled);
#else
  GTEST_FAIL() << "Undefined expectation.";
#endif

  EXPECT_TRUE(config.noise_suppression.enabled);
  EXPECT_EQ(config.noise_suppression.level,
            webrtc::AudioProcessing::Config::NoiseSuppression::kHigh);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // Android and iOS use echo cancellation optimized for mobiles.
  EXPECT_TRUE(config.echo_canceller.mobile_mode);
#else
  EXPECT_FALSE(config.echo_canceller.mobile_mode);
#endif
}

TEST(CreateWebRtcAudioProcessingModuleTest,
     Agc2ConfigUnchangedIfAgcSettingsDisabled) {
  auto config = CreateApmGetConfig(
      /*settings=*/{.automatic_gain_control = false});
  EXPECT_EQ(config.gain_controller2, kDefaultApmConfig.gain_controller2);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
TEST(CreateWebRtcAudioProcessingModuleTest,
     InputVolumeAdjustmentEnabledWithAgc2) {
  ::base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kWebRtcAllowInputVolumeAdjustment);
  auto config = CreateApmGetConfig(
      /*settings=*/{.automatic_gain_control = true});
  EXPECT_FALSE(config.gain_controller1.enabled);
  EXPECT_FALSE(config.gain_controller1.analog_gain_controller.enabled);
  EXPECT_TRUE(config.gain_controller2.enabled);
  EXPECT_TRUE(config.gain_controller2.input_volume_controller.enabled);
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
TEST(CreateWebRtcAudioProcessingModuleTest,
     CanDisableInputVolumeAdjustmentWithAgc2) {
  ::base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kWebRtcAllowInputVolumeAdjustment);
  auto config = CreateApmGetConfig(
      /*settings=*/{.automatic_gain_control = true});
  // Check that AGC1 is entirely disabled since, in the Hybrid AGC setup, AGC1
  // is only used for input volume adaptations.
  EXPECT_FALSE(config.gain_controller1.enabled);
  // Check that AGC2 input volume controller is disabled.
  EXPECT_FALSE(config.gain_controller2.input_volume_controller.enabled);
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
TEST(CreateWebRtcAudioProcessingModuleTest,
     OnlyOneInputVolumeControllerEnabledOnDesktopPlatforms) {
  auto config = CreateApmGetConfig(
      /*settings=*/{.automatic_gain_control = true});
  if (!config.gain_controller1.enabled && !config.gain_controller2.enabled) {
    GTEST_SUCCEED() << "AGC is altogether disabled.";
    return;
  }
  // Enabled state for the input volume controller in AGC1 and AGC2
  // respectively.
  bool agc1_enabled = config.gain_controller1.enabled &&
                      config.gain_controller1.analog_gain_controller.enabled;
  bool agc2_enabled = config.gain_controller2.enabled &&
                      config.gain_controller2.input_volume_controller.enabled;
  if (!agc1_enabled && !agc2_enabled) {
    GTEST_SUCCEED() << "No input volume controller is enabled.";
  }
  EXPECT_NE(agc1_enabled, agc2_enabled);
}
#else
TEST(CreateWebRtcAudioProcessingModuleTest,
     InputVolumeControllerDisabledOnNonDesktopPlatforms) {
  auto config = CreateApmGetConfig(
      /*settings=*/{.automatic_gain_control = true});
  if (!config.gain_controller1.enabled && !config.gain_controller2.enabled) {
    GTEST_SUCCEED() << "AGC is altogether disabled.";
  }
  if (config.gain_controller1.enabled) {
    EXPECT_NE(config.gain_controller1.mode,
              webrtc::AudioProcessing::Config::GainController1::Mode::
                  kAdaptiveAnalog);
    EXPECT_FALSE(config.gain_controller1.analog_gain_controller.enabled);
  }
  if (config.gain_controller2.enabled) {
    EXPECT_FALSE(config.gain_controller2.input_volume_controller.enabled);
  }
}
#endif

TEST(CreateWebRtcAudioProcessingModuleTest, VerifyNoiseSuppressionSettings) {
  for (bool noise_suppressor_enabled : {true, false}) {
    SCOPED_TRACE(noise_suppressor_enabled);
    auto config = CreateApmGetConfig(
        /*settings=*/{.noise_suppression = noise_suppressor_enabled});

    EXPECT_EQ(config.noise_suppression.enabled, noise_suppressor_enabled);
    EXPECT_EQ(config.noise_suppression.level,
              webrtc::AudioProcessing::Config::NoiseSuppression::kHigh);
  }
}

TEST(CreateWebRtcAudioProcessingModuleTest, VerifyEchoCancellerSettings) {
  for (bool echo_canceller_enabled : {true, false}) {
    SCOPED_TRACE(echo_canceller_enabled);
    auto config = CreateApmGetConfig(
        /*settings=*/{.echo_cancellation = echo_canceller_enabled});

    EXPECT_EQ(config.echo_canceller.enabled, echo_canceller_enabled);
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    EXPECT_TRUE(config.echo_canceller.mobile_mode);
#else
    EXPECT_FALSE(config.echo_canceller.mobile_mode);
#endif
  }
}

TEST(CreateWebRtcAudioProcessingModuleTest, ToggleHighPassFilter) {
  for (bool high_pass_filter_enabled : {true, false}) {
    SCOPED_TRACE(high_pass_filter_enabled);
    auto config = CreateApmGetConfig(
        /*settings=*/{.high_pass_filter = high_pass_filter_enabled});

    EXPECT_EQ(config.high_pass_filter.enabled, high_pass_filter_enabled);
  }
}

}  // namespace
}  // namespace media
