// Copyright 2021 The Chromium Authors. All rights reserved.
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
  EXPECT_FALSE(agc1_analog_config.clipping_predictor.enabled);
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
#if BUILDFLAG(IS_CHROMECAST)
  // Override the default config since on Chromecast AGC1 is explicitly
  // disabled.
  auto expected_config = kDefaultApmConfig.gain_controller1;
  expected_config.analog_gain_controller.enabled = false;
  EXPECT_EQ(config.gain_controller1, expected_config);
#else
  EXPECT_EQ(config.gain_controller1, kDefaultApmConfig.gain_controller1);
#endif
}

TEST(CreateWebRtcAudioProcessingModuleTest,
     Agc2ConfigUnchangedIfAgcSettingsDisabled) {
  auto config = CreateApmGetConfig(
      /*settings=*/{.automatic_gain_control = false,
                    .experimental_automatic_gain_control = false});
  EXPECT_EQ(config.gain_controller2, kDefaultApmConfig.gain_controller2);
}

TEST(CreateWebRtcAudioProcessingModuleTest,
     Agc2ConfigUnchangedIfAgcSettingsDisabledAndHybridAgcEnabled) {
  ::base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kWebRtcAnalogAgcClippingControl);
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
#if BUILDFLAG(IS_CHROMECAST)
TEST(CreateWebRtcAudioProcessingModuleTest, DisableAnalogAgc) {
  auto config = CreateApmGetConfig(
      /*settings=*/{.automatic_gain_control = true,
                    .experimental_automatic_gain_control = false});
  EXPECT_TRUE(config.gain_controller1.enabled);
  EXPECT_FALSE(config.gain_controller1.analog_gain_controller.enabled);
}
#else  // !BUILDFLAG(IS_CHROMECAST)
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
#endif  // !BUILDFLAG(IS_CHROMECAST)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
// Checks that on mobile the AGC1 Analog startup minimum volume cannot be
// overridden.
TEST(CreateWebRtcAudioProcessingModuleTest, CannotOverrideAgcStartupMinVolume) {
  ::base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kWebRtcAnalogAgcStartupMinVolume, {{"volume", "123"}});
  ASSERT_NE(kDefaultApmConfig.gain_controller1.analog_gain_controller
                .startup_min_volume,
            123);
  auto config = CreateApmGetConfig(/*settings=*/{});
  EXPECT_EQ(config.gain_controller1.analog_gain_controller.startup_min_volume,
            kDefaultApmConfig.gain_controller1.analog_gain_controller
                .startup_min_volume);
}
#else   // !(BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS))
// Checks that on all the platforms other than mobile the AGC1 Analog startup
// minimum volume can be overridden.
TEST(CreateWebRtcAudioProcessingModuleTest, OverrideAgcStartupMinVolume) {
  ::base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kWebRtcAnalogAgcStartupMinVolume, {{"volume", "123"}});
  ASSERT_NE(kDefaultApmConfig.gain_controller1.analog_gain_controller
                .startup_min_volume,
            123);
  auto config = CreateApmGetConfig(/*settings=*/{});
  EXPECT_EQ(config.gain_controller1.analog_gain_controller.startup_min_volume,
            123);
}
#endif  // !(BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS))

TEST(CreateWebRtcAudioProcessingModuleTest, EnableAgc1AnalogClippingControl) {
  ::base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kWebRtcAnalogAgcClippingControl,
      {{"mode", "2"},
       {"window_length", "111"},
       {"reference_window_length", "222"},
       {"reference_window_delay", "333"},
       {"clipping_threshold", "4.44"},
       {"crest_factor_margin", ".555"},
       {"clipped_level_step", "255"},
       {"clipped_ratio_threshold", "0.77"},
       {"clipped_wait_frames", "888"},
       {"use_predicted_step", "false"}});

  auto config = CreateApmGetConfig(
      /*settings=*/{.automatic_gain_control = true,
                    .experimental_automatic_gain_control = true});
  const auto& analog_agc = config.gain_controller1.analog_gain_controller;
  EXPECT_TRUE(analog_agc.clipping_predictor.enabled);

  using Mode = webrtc::AudioProcessing::Config::GainController1::
      AnalogGainController::ClippingPredictor::Mode;
  EXPECT_EQ(analog_agc.clipping_predictor.mode,
            Mode::kFixedStepClippingPeakPrediction);
  EXPECT_EQ(analog_agc.clipping_predictor.window_length, 111);
  EXPECT_EQ(analog_agc.clipping_predictor.reference_window_length, 222);
  EXPECT_EQ(analog_agc.clipping_predictor.reference_window_delay, 333);
  EXPECT_FLOAT_EQ(analog_agc.clipping_predictor.clipping_threshold, 4.44f);
  EXPECT_FLOAT_EQ(analog_agc.clipping_predictor.crest_factor_margin, 0.555f);
  EXPECT_FALSE(analog_agc.clipping_predictor.use_predicted_step);
  EXPECT_EQ(analog_agc.clipped_level_step, 255);
  EXPECT_FLOAT_EQ(analog_agc.clipped_ratio_threshold, 0.77f);
  EXPECT_EQ(analog_agc.clipped_wait_frames, 888);
}

TEST(CreateWebRtcAudioProcessingModuleTest, DisableAgc1AnalogClippingControl) {
  ::base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kWebRtcAnalogAgcClippingControl);
  auto config = CreateApmGetConfig(
      /*settings=*/{.automatic_gain_control = true,
                    .experimental_automatic_gain_control = true});
  const auto& analog_agc = config.gain_controller1.analog_gain_controller;
  EXPECT_FALSE(analog_agc.clipping_predictor.enabled);
}

TEST(CreateWebRtcAudioProcessingModuleTest,
     CannotEnableAgc1AnalogClippingControlWhenAgcIsDisabled) {
  ::base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kWebRtcAnalogAgcClippingControl);
  auto config =
      CreateApmGetConfig(/*settings=*/{.automatic_gain_control = false});
  EXPECT_FALSE(config.gain_controller1.analog_gain_controller.clipping_predictor
                   .enabled);
}

TEST(CreateWebRtcAudioProcessingModuleTest,
     CannotEnableAgc1AnalogClippingControlWhenExperimentalAgcIsDisabled) {
  ::base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kWebRtcAnalogAgcClippingControl);
  auto config = CreateApmGetConfig(
      /*settings=*/{.automatic_gain_control = true,
                    .experimental_automatic_gain_control = false});
  EXPECT_FALSE(config.gain_controller1.analog_gain_controller.clipping_predictor
                   .enabled);
}

TEST(CreateWebRtcAudioProcessingModuleTest, EnableHybridAgc) {
  ::base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kWebRtcHybridAgc, {{"dry_run", "false"},
                                   {"vad_reset_period_ms", "1230"},
                                   {"adjacent_speech_frames_threshold", "4"},
                                   {"max_gain_change_db_per_second", "5"},
                                   {"max_output_noise_level_dbfs", "-6"}});

  auto config = CreateApmGetConfig(
      /*settings=*/{.automatic_gain_control = true,
                    .experimental_automatic_gain_control = true});

  // Checks that the analog AGC is enabled and that its digital adaptive
  // controller is disabled.
  const auto& agc1_analog = config.gain_controller1.analog_gain_controller;
  EXPECT_TRUE(agc1_analog.enabled);
  EXPECT_FALSE(agc1_analog.enable_digital_adaptive);

  // Check that AGC2 is enabled and that the properties are correctly read from
  // the field trials.
  const auto& agc2 = config.gain_controller2;
  EXPECT_TRUE(agc2.enabled);
  EXPECT_EQ(config.gain_controller2.fixed_digital.gain_db, 0);
  EXPECT_TRUE(agc2.adaptive_digital.enabled);
  EXPECT_FALSE(agc2.adaptive_digital.dry_run);
  EXPECT_EQ(agc2.adaptive_digital.vad_reset_period_ms, 1230);
  EXPECT_EQ(agc2.adaptive_digital.adjacent_speech_frames_threshold, 4);
  EXPECT_FLOAT_EQ(agc2.adaptive_digital.max_gain_change_db_per_second, 5.0f);
  EXPECT_FLOAT_EQ(agc2.adaptive_digital.max_output_noise_level_dbfs, -6.0f);
}

TEST(CreateWebRtcAudioProcessingModuleTest, EnableHybridAgcDryRun) {
  ::base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(features::kWebRtcHybridAgc,
                                                  {{"dry_run", "true"}});
  auto config = CreateApmGetConfig(
      /*settings=*/{.automatic_gain_control = true,
                    .experimental_automatic_gain_control = true});
  // Checks that the analog AGC is enabled together with its digital adaptive
  // controller.
  const auto& agc1_analog = config.gain_controller1.analog_gain_controller;
  EXPECT_TRUE(agc1_analog.enabled);
  EXPECT_TRUE(agc1_analog.enable_digital_adaptive);

  // Check that AGC2 is enabled in dry run mode.
  const auto& agc2 = config.gain_controller2;
  EXPECT_TRUE(agc2.enabled);
  EXPECT_TRUE(agc2.adaptive_digital.enabled);
  EXPECT_TRUE(agc2.adaptive_digital.dry_run);
}

TEST(CreateWebRtcAudioProcessingModuleTest,
     HybridAgcDisabledWhenAgcIsDisabled) {
  ::base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kWebRtcHybridAgc);
  auto config =
      CreateApmGetConfig(/*settings=*/{.automatic_gain_control = false});
  EXPECT_FALSE(config.gain_controller2.enabled);
  EXPECT_FALSE(config.gain_controller2.adaptive_digital.enabled);
}

TEST(CreateWebRtcAudioProcessingModuleTest,
     HybridAgcDisabledWhenExperimentalAgcIsDisabled) {
  ::base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kWebRtcHybridAgc);
  auto config = CreateApmGetConfig(
      /*settings=*/{.automatic_gain_control = true,
                    .experimental_automatic_gain_control = false});
  EXPECT_FALSE(config.gain_controller2.enabled);
  EXPECT_FALSE(config.gain_controller2.adaptive_digital.enabled);
}

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
