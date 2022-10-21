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
  EXPECT_TRUE(config.gain_controller1.enabled);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  EXPECT_TRUE(config.gain_controller2.enabled);
#else
  EXPECT_FALSE(config.gain_controller2.enabled);
#endif
  EXPECT_TRUE(config.noise_suppression.enabled);
  EXPECT_EQ(config.noise_suppression.level,
            webrtc::AudioProcessing::Config::NoiseSuppression::kHigh);

#if BUILDFLAG(IS_ANDROID)
  // Android uses echo cancellation optimized for mobiles, and does not
  // support keytap suppression.
  EXPECT_TRUE(config.echo_canceller.mobile_mode);
  EXPECT_FALSE(config.transient_suppression.enabled);
#else
  EXPECT_FALSE(config.echo_canceller.mobile_mode);
  EXPECT_TRUE(config.transient_suppression.enabled);
#endif
}

TEST(CreateWebRtcAudioProcessingModuleTest, CheckDefaultAgcConfig) {
  auto config = CreateApmGetConfig(/*settings=*/{});
  EXPECT_TRUE(config.gain_controller1.enabled);
  using Mode = webrtc::AudioProcessing::Config::GainController1::Mode;
  // TODO(bugs.webrtc.org/7909): Add OS_IOS once bug fixed.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(config.gain_controller1.mode, Mode::kFixedDigital);
#else
  EXPECT_EQ(config.gain_controller1.mode, Mode::kAdaptiveAnalog);
#endif

  const auto& agc1_analog_config =
      config.gain_controller1.analog_gain_controller;
  // TODO(bugs.webrtc.org/7909): Uncomment below once fixed.
  // #if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  //   // No analog controller available on mobile.
  //   EXPECT_FALSE(agc1_analog_config.enabled);
  // #else
  EXPECT_TRUE(agc1_analog_config.enabled);
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // Leaving `agc_startup_min_volume` unspecified on mobile does not override
  // `startup_min_volume`.
  EXPECT_EQ(agc1_analog_config.startup_min_volume,
            kDefaultApmConfig.gain_controller1.analog_gain_controller
                .startup_min_volume);
#else
  // TODO(bugs.webrtc.org/7494): Check if the following is unwanted, fix if so.
  // Leaving `agc_startup_min_volume` overrides the default WebRTC value with
  // zero.
  EXPECT_EQ(agc1_analog_config.startup_min_volume, 0);
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(agc1_analog_config.clipping_predictor.enabled);
#else
  EXPECT_FALSE(agc1_analog_config.clipping_predictor.enabled);
#endif
  // TODO(bugs.webrtc.org/7909): Uncomment below once fixed.
  // #endif

  // Check that either AGC1 digital or AGC2 digital is used based on the
  // platforms where the Hybrid AGC is enabled by default.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  EXPECT_FALSE(agc1_analog_config.enable_digital_adaptive);
  EXPECT_TRUE(config.gain_controller2.enabled);
  EXPECT_TRUE(config.gain_controller2.adaptive_digital.enabled);
#else
  // AGC1 Digital.
  EXPECT_TRUE(agc1_analog_config.enable_digital_adaptive);
  EXPECT_FALSE(config.gain_controller2.enabled);
#endif
}

// When `automatic_gain_control` and `experimental_automatic_gain_control` are
// false, the default AGC1 configuration is used, but on Chromecast AGC1 Analog
// is explicitly disabled.
TEST(CreateWebRtcAudioProcessingModuleTest,
     Agc1ConfigUnchangedIfAgcSettingsDisabled) {
  auto config = CreateApmGetConfig(
      /*settings=*/{.automatic_gain_control = false,
                    .experimental_automatic_gain_control = false});

// TODO(crbug.com/1336055): Make this check non-conditional following the launch
// of AGC2.
#if BUILDFLAG(IS_CASTOS) || BUILDFLAG(IS_CAST_ANDROID)
  // Override the default config since on Chromecast AGC1 is explicitly
  // disabled.
  auto expected_config = kDefaultApmConfig.gain_controller1;
  expected_config.analog_gain_controller.enabled = false;
  EXPECT_EQ(config.gain_controller1, expected_config);
#else
  EXPECT_EQ(config.gain_controller1, kDefaultApmConfig.gain_controller1);
#endif  // BUILDFLAG(IS_CASTOS) || BUILDFLAG(IS_CAST_ANDROID)
}

TEST(CreateWebRtcAudioProcessingModuleTest,
     Agc2ConfigUnchangedIfAgcSettingsDisabled) {
  auto config = CreateApmGetConfig(
      /*settings=*/{.automatic_gain_control = false,
                    .experimental_automatic_gain_control = false});
  EXPECT_EQ(config.gain_controller2, kDefaultApmConfig.gain_controller2);
}

TEST(CreateWebRtcAudioProcessingModuleTest, DisableAgcEnableExperimentalAgc) {
  auto config = CreateApmGetConfig(
      /*settings=*/{.automatic_gain_control = false,
                    .experimental_automatic_gain_control = true});
  EXPECT_FALSE(config.gain_controller1.enabled);
  EXPECT_TRUE(config.gain_controller1.analog_gain_controller.enabled);
}

// TODO(bugs.webrtc.org/7909): Remove #IF once fixed.
#if BUILDFLAG(IS_CASTOS) || BUILDFLAG(IS_CAST_ANDROID)
TEST(CreateWebRtcAudioProcessingModuleTest, DisableAnalogAgc) {
  auto config = CreateApmGetConfig(
      /*settings=*/{.automatic_gain_control = true,
                    .experimental_automatic_gain_control = false});
  EXPECT_TRUE(config.gain_controller1.enabled);
  EXPECT_FALSE(config.gain_controller1.analog_gain_controller.enabled);
}
#else
// Checks that setting `experimental_automatic_gain_control` to false does not
// disable the analog controller.
// TODO(bugs.webrtc.org/7909): Remove once fixed.
TEST(CreateWebRtcAudioProcessingModuleTest, CannotDisableAnalogAgc) {
  auto config = CreateApmGetConfig(
      /*settings=*/{.automatic_gain_control = true,
                    .experimental_automatic_gain_control = false});
  EXPECT_TRUE(config.gain_controller1.enabled);
  EXPECT_TRUE(config.gain_controller1.analog_gain_controller.enabled);
}
#endif  // BUILDFLAG(IS_CASTOS) || BUILDFLAG(IS_CAST_ANDROID)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
TEST(CreateWebRtcAudioProcessingModuleTest,
     InputVolumeAdjustmentEnabledWithHybridAgc) {
  ::base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kWebRtcAllowInputVolumeAdjustment);
  auto config = CreateApmGetConfig(
      /*settings=*/{.automatic_gain_control = true,
                    .experimental_automatic_gain_control = true});
  EXPECT_TRUE(config.gain_controller1.enabled);
  EXPECT_TRUE(config.gain_controller1.analog_gain_controller.enabled);
}
#else
TEST(CreateWebRtcAudioProcessingModuleTest,
     InputVolumeAdjustmentEnabledWithAgc1) {
  ::base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kWebRtcAllowInputVolumeAdjustment);
  auto config = CreateApmGetConfig(
      /*settings=*/{.automatic_gain_control = true,
                    .experimental_automatic_gain_control = true});
  EXPECT_TRUE(config.gain_controller1.enabled);
  EXPECT_TRUE(config.gain_controller1.analog_gain_controller.enabled);
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
TEST(CreateWebRtcAudioProcessingModuleTest,
     CanDisableInputVolumeAdjustmentWithHybridAgc) {
  ::base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kWebRtcAllowInputVolumeAdjustment);
  auto config = CreateApmGetConfig(
      /*settings=*/{.automatic_gain_control = true,
                    .experimental_automatic_gain_control = true});
  // Check that AGC1 is entirely disabled since, in the Hybrid AGC setup, AGC1
  // is only used for input volume adaptations.
  EXPECT_FALSE(config.gain_controller1.enabled);
}
#else
TEST(CreateWebRtcAudioProcessingModuleTest,
     CannotDisableInputVolumeAdjustmentWithAgc1) {
  ::base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kWebRtcAllowInputVolumeAdjustment);
  auto config = CreateApmGetConfig(
      /*settings=*/{.automatic_gain_control = true,
                    .experimental_automatic_gain_control = true});
  EXPECT_TRUE(config.gain_controller1.enabled);
  EXPECT_TRUE(config.gain_controller1.analog_gain_controller.enabled);
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

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
#if BUILDFLAG(IS_ANDROID)
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

TEST(CreateWebRtcAudioProcessingModuleTest, ToggleTransientSuppression) {
  for (bool transient_suppression_enabled : {true, false}) {
    SCOPED_TRACE(transient_suppression_enabled);
    auto config = CreateApmGetConfig(/*settings=*/{
        .transient_noise_suppression = transient_suppression_enabled});

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    // Transient suppression is not supported (nor useful) on mobile platforms.
    EXPECT_FALSE(config.transient_suppression.enabled);
#else
    EXPECT_EQ(config.transient_suppression.enabled,
              transient_suppression_enabled);
#endif
  }
}

}  // namespace
}  // namespace media
