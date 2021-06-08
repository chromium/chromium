// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_processor_options.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace blink {
namespace {
using Agc2Config = webrtc::AudioProcessing::Config::GainController2;
using ClippingPredictor = webrtc::AudioProcessing::Config::GainController1::
    AnalogGainController::ClippingPredictor;

constexpr WebRtcHybridAgcParams kHybridAgcParams{
    .dry_run = false,
    .vad_reset_period_ms = 1500,
    .adjacent_speech_frames_threshold = 12,
    .max_gain_change_db_per_second = 3.0f,
    .max_output_noise_level_dbfs = -50.0f,
    .sse2_allowed = true,
    .avx2_allowed = true,
    .neon_allowed = true};

constexpr AudioProcessingProperties kAudioProcessingNoAgc{
    .goog_auto_gain_control = false,
    .goog_experimental_auto_gain_control = false};

constexpr AudioProcessingProperties kAudioProcessingNoExperimentalAgc{
    .goog_auto_gain_control = true,
    .goog_experimental_auto_gain_control = false};

constexpr AudioProcessingProperties kAudioProcessingExperimentalAgc{
    .goog_auto_gain_control = true,
    .goog_experimental_auto_gain_control = true};

constexpr double kCompressionGainDb = 10.0;

constexpr WebRtcAnalogAgcClippingControlParams kClippingControlParams{
    .mode = 2,  // ClippingPredictor::Mode::kFixedStepClippingPeakPrediction
    .window_length = 111,
    .reference_window_length = 222,
    .reference_window_delay = 333,
    .clipping_threshold = 4.44f,
    .crest_factor_margin = 5.55f,
    .clipped_level_step = 666,
    .clipped_ratio_threshold = 0.777f,
    .clipped_wait_frames = 300};
}  // namespace

TEST(ConfigAutomaticGainControlTest, DoNotChangeApmConfig) {
  constexpr webrtc::AudioProcessing::Config kDefaultConfig;
  webrtc::AudioProcessing::Config apm_config;

  ConfigAutomaticGainControl(kAudioProcessingNoAgc, kHybridAgcParams,
                             kClippingControlParams,
                             /*compression_gain_db=*/7, apm_config);
  EXPECT_EQ(apm_config.gain_controller1, kDefaultConfig.gain_controller1);
  EXPECT_EQ(apm_config.gain_controller2, kDefaultConfig.gain_controller2);

  ConfigAutomaticGainControl(kAudioProcessingNoAgc,
                             /*hybrid_agc_params=*/absl::nullopt,
                             /*clipping_control_params=*/absl::nullopt,
                             /*compression_gain_db=*/absl::nullopt, apm_config);
  EXPECT_EQ(apm_config.gain_controller1, kDefaultConfig.gain_controller1);
  EXPECT_EQ(apm_config.gain_controller2, kDefaultConfig.gain_controller2);
}

TEST(ConfigAutomaticGainControlTest, SystemAgcDeactivatesBrowserAgcs) {
  webrtc::AudioProcessing::Config apm_config;
  constexpr AudioProcessingProperties kProperties{
      .system_gain_control_activated = true,
      .goog_auto_gain_control = true,
      .goog_experimental_auto_gain_control = true};

  ConfigAutomaticGainControl(kProperties, kHybridAgcParams,
                             kClippingControlParams,
                             /*compression_gain_db=*/10.0, apm_config);
  EXPECT_FALSE(apm_config.gain_controller1.enabled);
  EXPECT_FALSE(apm_config.gain_controller2.enabled);
}

TEST(ConfigAutomaticGainControlTest, EnableDefaultAGC1) {
  webrtc::AudioProcessing::Config apm_config;

  ConfigAutomaticGainControl(kAudioProcessingNoExperimentalAgc,
                             /*hybrid_agc_params=*/absl::nullopt,
                             /*clipping_control_params=*/absl::nullopt,
                             /*compression_gain_db=*/absl::nullopt, apm_config);

  EXPECT_TRUE(apm_config.gain_controller1.enabled);
  EXPECT_EQ(
      apm_config.gain_controller1.mode,
#if defined(OS_ANDROID)
      webrtc::AudioProcessing::Config::GainController1::Mode::kFixedDigital);
#else
      webrtc::AudioProcessing::Config::GainController1::Mode::kAdaptiveAnalog);
#endif  // defined(OS_ANDROID)

  EXPECT_FALSE(apm_config.gain_controller1.analog_gain_controller
                   .clipping_predictor.enabled);
}

TEST(ConfigAutomaticGainControlTest, EnableFixedDigitalAGC2) {
  webrtc::AudioProcessing::Config apm_config;

  ConfigAutomaticGainControl(kAudioProcessingNoExperimentalAgc,
                             /*hybrid_agc_params=*/absl::nullopt,
                             /*clipping_control_params=*/absl::nullopt,
                             kCompressionGainDb, apm_config);
  EXPECT_FALSE(apm_config.gain_controller1.enabled);
  EXPECT_TRUE(apm_config.gain_controller2.enabled);
  EXPECT_FALSE(apm_config.gain_controller2.adaptive_digital.enabled);
  EXPECT_FLOAT_EQ(apm_config.gain_controller2.fixed_digital.gain_db,
                  kCompressionGainDb);
}

TEST(ConfigAutomaticGainControlTest, EnableHybridAGC) {
  webrtc::AudioProcessing::Config apm_config;

  ConfigAutomaticGainControl(kAudioProcessingExperimentalAgc, kHybridAgcParams,
                             /*clipping_control_params=*/absl::nullopt,
                             kCompressionGainDb, apm_config);
  EXPECT_TRUE(apm_config.gain_controller1.enabled);
  EXPECT_EQ(
      apm_config.gain_controller1.mode,
#if defined(OS_ANDROID)
      webrtc::AudioProcessing::Config::GainController1::Mode::kFixedDigital);
#else
      webrtc::AudioProcessing::Config::GainController1::Mode::kAdaptiveAnalog);
#endif  // defined(OS_ANDROID)
  EXPECT_TRUE(apm_config.gain_controller2.enabled);
  // `compression_gain_db` has no effect when hybrid AGC is active.
  EXPECT_FLOAT_EQ(apm_config.gain_controller2.fixed_digital.gain_db, 0.0f);
  EXPECT_TRUE(apm_config.gain_controller2.adaptive_digital.enabled);
  EXPECT_EQ(apm_config.gain_controller2.adaptive_digital.dry_run,
            kHybridAgcParams.dry_run);
  EXPECT_EQ(apm_config.gain_controller2.adaptive_digital.vad_reset_period_ms,
            kHybridAgcParams.vad_reset_period_ms);
  EXPECT_EQ(apm_config.gain_controller2.adaptive_digital
                .adjacent_speech_frames_threshold,
            kHybridAgcParams.adjacent_speech_frames_threshold);
  EXPECT_FLOAT_EQ(apm_config.gain_controller2.adaptive_digital
                      .max_gain_change_db_per_second,
                  kHybridAgcParams.max_gain_change_db_per_second);
  EXPECT_FLOAT_EQ(
      apm_config.gain_controller2.adaptive_digital.max_output_noise_level_dbfs,
      kHybridAgcParams.max_output_noise_level_dbfs);
  EXPECT_EQ(apm_config.gain_controller2.adaptive_digital.sse2_allowed,
            kHybridAgcParams.sse2_allowed);
  EXPECT_EQ(apm_config.gain_controller2.adaptive_digital.avx2_allowed,
            kHybridAgcParams.avx2_allowed);
  EXPECT_EQ(apm_config.gain_controller2.adaptive_digital.neon_allowed,
            kHybridAgcParams.neon_allowed);
}

TEST(ConfigAutomaticGainControlTest, EnableClippingControl) {
  webrtc::AudioProcessing::Config apm_config;
  ConfigAutomaticGainControl(
      kAudioProcessingExperimentalAgc, /*hybrid_agc_params=*/absl::nullopt,
      kClippingControlParams, kCompressionGainDb, apm_config);
  EXPECT_TRUE(apm_config.gain_controller1.enabled);
  EXPECT_TRUE(apm_config.gain_controller1.analog_gain_controller.enabled);

  const auto& analog_gain_controller =
      apm_config.gain_controller1.analog_gain_controller;
  EXPECT_EQ(analog_gain_controller.clipped_level_step,
            kClippingControlParams.clipped_level_step);
  EXPECT_FLOAT_EQ(analog_gain_controller.clipped_ratio_threshold,
                  kClippingControlParams.clipped_ratio_threshold);
  EXPECT_EQ(analog_gain_controller.clipped_wait_frames,
            kClippingControlParams.clipped_wait_frames);

  const auto& clipping_predictor = analog_gain_controller.clipping_predictor;
  EXPECT_TRUE(clipping_predictor.enabled);
  EXPECT_EQ(clipping_predictor.mode,
            ClippingPredictor::Mode::kFixedStepClippingPeakPrediction);
  EXPECT_EQ(clipping_predictor.window_length,
            kClippingControlParams.window_length);
  EXPECT_EQ(clipping_predictor.reference_window_length,
            kClippingControlParams.reference_window_length);
  EXPECT_EQ(clipping_predictor.reference_window_delay,
            kClippingControlParams.reference_window_delay);
  EXPECT_FLOAT_EQ(clipping_predictor.clipping_threshold,
                  kClippingControlParams.clipping_threshold);
  EXPECT_FLOAT_EQ(clipping_predictor.crest_factor_margin,
                  kClippingControlParams.crest_factor_margin);
}

TEST(PopulateApmConfigTest, DefaultWithoutConfigJson) {
  webrtc::AudioProcessing::Config apm_config;
  const AudioProcessingProperties properties;
  absl::optional<double> gain_control_compression_gain_db;

  PopulateApmConfig(&apm_config, properties,
                    absl::nullopt,  // |audio_processing_platform_config_json|.
                    &gain_control_compression_gain_db);
  EXPECT_FALSE(gain_control_compression_gain_db.has_value());
  EXPECT_TRUE(apm_config.high_pass_filter.enabled);
  EXPECT_FALSE(apm_config.pre_amplifier.enabled);
  EXPECT_TRUE(apm_config.noise_suppression.enabled);
  EXPECT_EQ(apm_config.noise_suppression.level,
            webrtc::AudioProcessing::Config::NoiseSuppression::kHigh);
  EXPECT_TRUE(apm_config.echo_canceller.enabled);
#if defined(OS_ANDROID)
  EXPECT_TRUE(
#else
  EXPECT_FALSE(
#endif
      apm_config.echo_canceller.mobile_mode);
}

TEST(PopulateApmConfigTest, SetGainsInConfigJson) {
  webrtc::AudioProcessing::Config apm_config;
  const AudioProcessingProperties properties;
  absl::optional<std::string> audio_processing_platform_config_json =
      "{\"gain_control_compression_gain_db\": 10, "
      "\"pre_amplifier_fixed_gain_factor\": 2.0}";
  absl::optional<double> gain_control_compression_gain_db;

  PopulateApmConfig(&apm_config, properties,
                    audio_processing_platform_config_json,
                    &gain_control_compression_gain_db);
  EXPECT_TRUE(gain_control_compression_gain_db.has_value());
  EXPECT_EQ(gain_control_compression_gain_db.value(), 10);
  EXPECT_TRUE(apm_config.high_pass_filter.enabled);
  EXPECT_TRUE(apm_config.pre_amplifier.enabled);
  EXPECT_FLOAT_EQ(apm_config.pre_amplifier.fixed_gain_factor, 2.0);
  EXPECT_TRUE(apm_config.noise_suppression.enabled);
  EXPECT_EQ(apm_config.noise_suppression.level,
            webrtc::AudioProcessing::Config::NoiseSuppression::kHigh);
  EXPECT_TRUE(apm_config.echo_canceller.enabled);
#if defined(OS_ANDROID)
  EXPECT_TRUE(
#else
  EXPECT_FALSE(
#endif
      apm_config.echo_canceller.mobile_mode);
}

TEST(PopulateApmConfigTest, SetNoiseSuppressionLevelInConfigJson) {
  webrtc::AudioProcessing::Config apm_config;
  const AudioProcessingProperties properties;
  absl::optional<std::string> audio_processing_platform_config_json =
      "{\"noise_suppression_level\": 3}";
  absl::optional<double> gain_control_compression_gain_db;

  PopulateApmConfig(&apm_config, properties,
                    audio_processing_platform_config_json,
                    &gain_control_compression_gain_db);
  EXPECT_FALSE(gain_control_compression_gain_db.has_value());
  EXPECT_TRUE(apm_config.high_pass_filter.enabled);
  EXPECT_FALSE(apm_config.pre_amplifier.enabled);
  EXPECT_TRUE(apm_config.noise_suppression.enabled);
  EXPECT_EQ(apm_config.noise_suppression.level,
            webrtc::AudioProcessing::Config::NoiseSuppression::kVeryHigh);
  EXPECT_TRUE(apm_config.echo_canceller.enabled);
#if defined(OS_ANDROID)
  EXPECT_TRUE(
#else
  EXPECT_FALSE(
#endif
      apm_config.echo_canceller.mobile_mode);
}

TEST(PopulateApmConfigTest, SystemNsDeactivatesBrowserNs) {
  absl::optional<double> gain_control_compression_gain_db;

  // Verify that the default value of `noise_suppression.enabled`
  // is true, since otherwise this test does not work.
  AudioProcessingProperties properties_without_system_ns;
  properties_without_system_ns.system_noise_suppression_activated = false;
  webrtc::AudioProcessing::Config apm_config_without_system_ns;
  PopulateApmConfig(&apm_config_without_system_ns, properties_without_system_ns,
                    /*audio_processing_platform_config_json=*/absl::nullopt,
                    &gain_control_compression_gain_db);
  EXPECT_TRUE(apm_config_without_system_ns.noise_suppression.enabled);

  // Verify that the presence of a system noise suppressor deactivates the
  // browser counterpart.
  AudioProcessingProperties properties_with_system_ns;
  properties_with_system_ns.system_noise_suppression_activated = true;
  webrtc::AudioProcessing::Config apm_config_with_system_ns;
  PopulateApmConfig(&apm_config_with_system_ns, properties_with_system_ns,
                    /*audio_processing_platform_config_json=*/absl::nullopt,
                    &gain_control_compression_gain_db);
  EXPECT_FALSE(apm_config_with_system_ns.noise_suppression.enabled);
}

}  // namespace blink
