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

constexpr media::AudioProcessingSettings kAudioProcessingNoAgc{
    .automatic_gain_control = false,
    .experimental_automatic_gain_control = false};

constexpr media::AudioProcessingSettings kAudioProcessingNoExperimentalAgc{
    .automatic_gain_control = true,
    .experimental_automatic_gain_control = false};

constexpr media::AudioProcessingSettings kAudioProcessingExperimentalAgc{
    .automatic_gain_control = true,
    .experimental_automatic_gain_control = true};

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
    .clipped_wait_frames = 300,
    .use_predicted_step = true};
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
  EXPECT_EQ(clipping_predictor.use_predicted_step,
            kClippingControlParams.use_predicted_step);
}

TEST(PopulateApmConfigTest, DefaultWithoutConfigJson) {
  webrtc::AudioProcessing::Config apm_config;
  const media::AudioProcessingSettings settings;
  absl::optional<double> gain_control_compression_gain_db;

  PopulateApmConfig(&apm_config, settings,
                    /*audio_processing_platform_config_json=*/absl::nullopt,
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
  const media::AudioProcessingSettings settings;
  absl::optional<std::string> audio_processing_platform_config_json =
      "{\"gain_control_compression_gain_db\": 10, "
      "\"pre_amplifier_fixed_gain_factor\": 2.0}";
  absl::optional<double> gain_control_compression_gain_db;

  PopulateApmConfig(&apm_config, settings,
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
  const media::AudioProcessingSettings settings;
  absl::optional<std::string> audio_processing_platform_config_json =
      "{\"noise_suppression_level\": 3}";
  absl::optional<double> gain_control_compression_gain_db;

  PopulateApmConfig(&apm_config, settings,
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

TEST(CreateWebRtcAudioProcessingModuleTest, SetGainsInConfigJson) {
  const media::AudioProcessingSettings settings{
      .automatic_gain_control = true,
      .experimental_automatic_gain_control = false};
  absl::optional<std::string> audio_processing_platform_config_json =
      "{\"gain_control_compression_gain_db\": 12.1212, "
      "\"pre_amplifier_fixed_gain_factor\": 2.345}";

  rtc::scoped_refptr<webrtc::AudioProcessing> apm =
      CreateWebRtcAudioProcessingModule(
          settings, audio_processing_platform_config_json,
          /*agc_startup_min_volume=*/absl::nullopt);
  ASSERT_TRUE(!!apm);
  webrtc::AudioProcessing::Config config = apm->GetConfig();

  // Pre-amplifier is enabled if a gain factor is specified.
  EXPECT_TRUE(config.pre_amplifier.enabled);
  EXPECT_FLOAT_EQ(config.pre_amplifier.fixed_gain_factor, 2.345);

  // Fixed digital AGC2 is enabled if AGC is on, analog AGC is off, and a
  // compression gain is specified.
  EXPECT_TRUE(config.gain_controller2.enabled);
  EXPECT_FALSE(config.gain_controller2.adaptive_digital.enabled);
  EXPECT_FLOAT_EQ(config.gain_controller2.fixed_digital.gain_db, 12.1212);
}

TEST(CreateWebRtcAudioProcessingModuleTest,
     SetNoiseSuppressionLevelInConfigJson) {
  const media::AudioProcessingSettings settings{.noise_suppression = true};
  absl::optional<std::string> audio_processing_platform_config_json =
      "{\"noise_suppression_level\": 3}";

  rtc::scoped_refptr<webrtc::AudioProcessing> apm =
      CreateWebRtcAudioProcessingModule(
          settings, audio_processing_platform_config_json,
          /*agc_startup_min_volume=*/absl::nullopt);
  ASSERT_TRUE(!!apm);

  webrtc::AudioProcessing::Config config = apm->GetConfig();

  EXPECT_TRUE(config.noise_suppression.enabled);
  EXPECT_EQ(config.noise_suppression.level,
            webrtc::AudioProcessing::Config::NoiseSuppression::kVeryHigh);
}

// Verify the default audio processing effects.
TEST(CreateWebRtcAudioProcessingModuleTest, VerifyDefaultProperties) {
  const AudioProcessingProperties properties;
  const media::AudioProcessingSettings settings =
      properties.ToAudioProcessingSettings(
          /*multi_channel_capture_processing=*/true);
  rtc::scoped_refptr<webrtc::AudioProcessing> apm =
      CreateWebRtcAudioProcessingModule(
          settings,
          /*audio_processing_platform_config_json=*/absl::nullopt,
          /*agc_startup_min_volume=*/absl::nullopt);
  ASSERT_TRUE(!!apm);

  webrtc::AudioProcessing::Config config = apm->GetConfig();

  EXPECT_TRUE(config.pipeline.multi_channel_render);
  EXPECT_TRUE(config.pipeline.multi_channel_capture);
  EXPECT_EQ(config.pipeline.maximum_internal_processing_rate, 48000);
  EXPECT_TRUE(config.high_pass_filter.enabled);
  EXPECT_TRUE(config.echo_canceller.enabled);
  EXPECT_TRUE(config.gain_controller1.enabled);
  EXPECT_TRUE(config.gain_controller1.analog_gain_controller.enabled);
  EXPECT_FALSE(config.gain_controller2.enabled);
  EXPECT_TRUE(config.noise_suppression.enabled);
  EXPECT_FALSE(config.voice_detection.enabled);
  EXPECT_FALSE(config.residual_echo_detector.enabled);

#if defined(OS_ANDROID)
  // Android uses echo cancellation optimized for mobiles, and does not
  // support keytap suppression.
  EXPECT_TRUE(config.echo_canceller.mobile_mode);
  EXPECT_FALSE(config.transient_suppression.enabled);
#else
  EXPECT_FALSE(config.echo_canceller.mobile_mode);
  EXPECT_TRUE(config.transient_suppression.enabled);
#endif
}

TEST(CreateWebRtcAudioProcessingModuleTest, VerifyNoiseSuppressionSettings) {
  for (bool ns_enabled : {true, false}) {
    const media::AudioProcessingSettings settings{.noise_suppression =
                                                      ns_enabled};
    rtc::scoped_refptr<webrtc::AudioProcessing> apm =
        CreateWebRtcAudioProcessingModule(
            settings,
            /*audio_processing_platform_config_json=*/absl::nullopt,
            /*agc_startup_min_volume=*/absl::nullopt);
    ASSERT_TRUE(!!apm);
    webrtc::AudioProcessing::Config config = apm->GetConfig();

    EXPECT_EQ(config.noise_suppression.enabled, ns_enabled);
    EXPECT_EQ(config.noise_suppression.level,
              webrtc::AudioProcessing::Config::NoiseSuppression::kHigh);
  }
}

TEST(CreateWebRtcAudioProcessingModuleTest, VerifyEchoCancellerSettings) {
  for (bool ec_enabled : {true, false}) {
    const media::AudioProcessingSettings settings{.echo_cancellation =
                                                      ec_enabled};
    rtc::scoped_refptr<webrtc::AudioProcessing> apm =
        CreateWebRtcAudioProcessingModule(
            settings,
            /*audio_processing_platform_config_json=*/absl::nullopt,
            /*agc_startup_min_volume=*/absl::nullopt);
    ASSERT_TRUE(!!apm);
    webrtc::AudioProcessing::Config config = apm->GetConfig();

    EXPECT_EQ(config.echo_canceller.enabled, ec_enabled);
#if defined(OS_ANDROID)
    EXPECT_TRUE(config.echo_canceller.mobile_mode);
#else
    EXPECT_FALSE(config.echo_canceller.mobile_mode);
#endif
  }
}

TEST(CreateWebRtcAudioProcessingModuleTest, ToggleHighPassFilter) {
  for (bool high_pass_filter_enabled : {true, false}) {
    const media::AudioProcessingSettings settings{.high_pass_filter =
                                                      high_pass_filter_enabled};
    rtc::scoped_refptr<webrtc::AudioProcessing> apm =
        CreateWebRtcAudioProcessingModule(
            settings,
            /*audio_processing_platform_config_json=*/absl::nullopt,
            /*agc_startup_min_volume=*/absl::nullopt);
    ASSERT_TRUE(!!apm);
    webrtc::AudioProcessing::Config config = apm->GetConfig();

    EXPECT_EQ(config.high_pass_filter.enabled, high_pass_filter_enabled);
  }
}

TEST(CreateWebRtcAudioProcessingModuleTest, ToggleTransientSuppression) {
  for (bool transient_suppression_enabled : {true, false}) {
    const media::AudioProcessingSettings settings{
        .transient_noise_suppression = transient_suppression_enabled};
    rtc::scoped_refptr<webrtc::AudioProcessing> apm =
        CreateWebRtcAudioProcessingModule(
            settings,
            /*audio_processing_platform_config_json=*/absl::nullopt,
            /*agc_startup_min_volume=*/absl::nullopt);
    ASSERT_TRUE(!!apm);
    webrtc::AudioProcessing::Config config = apm->GetConfig();

#if !defined(OS_IOS) && !defined(OS_ANDROID)
    EXPECT_EQ(config.transient_suppression.enabled,
              transient_suppression_enabled);
#else
    // Transient suppression is not supported (nor useful) on mobile platforms.
    EXPECT_FALSE(config.transient_suppression.enabled);
#endif
  }
}

// There is no way to test what echo cancellation configuration is applied, but
// this test at least exercises the code that handles echo cancellation
// configuration from JSON.
TEST(CreateWebRtcAudioProcessingModuleTest,
     ApplyEchoCancellationConfigFromJson) {
  // Arbitrary settings to have something to parse.
  absl::optional<std::string> audio_processing_platform_config_json =
      "{\"aec3\": {"
      "\"comfort_noise\": {\"noise_floor_dbfs\": -123.4567},"
      "\"echo_model\": {\"min_noise_floor_power\": 1234567.8},"
      "},}";
  const media::AudioProcessingSettings settings{.echo_cancellation = true};
  rtc::scoped_refptr<webrtc::AudioProcessing> apm =
      CreateWebRtcAudioProcessingModule(
          settings, audio_processing_platform_config_json,
          /*agc_startup_min_volume=*/absl::nullopt);
  ASSERT_TRUE(!!apm);
  webrtc::AudioProcessing::Config config = apm->GetConfig();
  EXPECT_TRUE(config.echo_canceller.enabled);
}

TEST(AudioProcessingPropertiesToAudioProcessingSettingsTest,
     DefaultPropertiesAndSettingsMatch) {
  const media::AudioProcessingSettings default_settings;
  AudioProcessingProperties properties;
  const media::AudioProcessingSettings generated_settings =
      properties.ToAudioProcessingSettings(
          default_settings.multi_channel_capture_processing);
  EXPECT_EQ(default_settings, generated_settings);
}

TEST(AudioProcessingPropertiesToAudioProcessingSettingsTest,
     DisableDefaultProperties) {
  AudioProcessingProperties properties;
  properties.DisableDefaultProperties();
  const media::AudioProcessingSettings settings =
      properties.ToAudioProcessingSettings(
          /*multi_channel_capture_processing=*/true);
  EXPECT_FALSE(settings.echo_cancellation);
  EXPECT_FALSE(settings.noise_suppression);
  EXPECT_FALSE(settings.transient_noise_suppression);
  EXPECT_FALSE(settings.automatic_gain_control);
  EXPECT_FALSE(settings.experimental_automatic_gain_control);
  EXPECT_FALSE(settings.high_pass_filter);
  EXPECT_FALSE(settings.stereo_mirroring);
  EXPECT_FALSE(settings.force_apm_creation);
}

TEST(AudioProcessingPropertiesToAudioProcessingSettingsTest,
     AllBrowserPropertiesEnabled) {
  const AudioProcessingProperties properties{
      .echo_cancellation_type = AudioProcessingProperties::
          EchoCancellationType::kEchoCancellationAec3,
      .goog_audio_mirroring = true,
      .goog_auto_gain_control = true,
      .goog_experimental_echo_cancellation = true,
      .goog_noise_suppression = true,
      .goog_experimental_noise_suppression = true,
      .goog_highpass_filter = true,
      .goog_experimental_auto_gain_control = true};
  const media::AudioProcessingSettings settings =
      properties.ToAudioProcessingSettings(
          /*multi_channel_capture_processing=*/true);
  EXPECT_TRUE(settings.echo_cancellation);
  EXPECT_TRUE(settings.noise_suppression);
  EXPECT_TRUE(settings.transient_noise_suppression);
  EXPECT_TRUE(settings.automatic_gain_control);
  EXPECT_TRUE(settings.experimental_automatic_gain_control);
  EXPECT_TRUE(settings.high_pass_filter);
  EXPECT_TRUE(settings.stereo_mirroring);
  EXPECT_TRUE(settings.force_apm_creation);
}

TEST(AudioProcessingPropertiesToAudioProcessingSettingsTest,
     SystemAecDisablesBrowserAec) {
  AudioProcessingProperties properties{
      .echo_cancellation_type = AudioProcessingProperties::
          EchoCancellationType::kEchoCancellationSystem};
  media::AudioProcessingSettings settings =
      properties.ToAudioProcessingSettings(
          /*multi_channel_capture_processing=*/true);
  EXPECT_FALSE(settings.echo_cancellation);
}

TEST(AudioProcessingPropertiesToAudioProcessingSettingsTest,
     SystemNsDeactivatesBrowserNs) {
  // Verify that noise suppression is by default enabled, since otherwise this
  // test does not work.
  constexpr AudioProcessingProperties kPropertiesWithoutSystemNs{
      .system_noise_suppression_activated = false};
  media::AudioProcessingSettings settings_without_system_ns =
      kPropertiesWithoutSystemNs.ToAudioProcessingSettings(
          /*multi_channel_capture_processing=*/true);
  EXPECT_TRUE(settings_without_system_ns.noise_suppression);

  constexpr AudioProcessingProperties kPropertiesWithSystemNs{
      .system_noise_suppression_activated = true};
  media::AudioProcessingSettings settings_with_system_ns =
      kPropertiesWithSystemNs.ToAudioProcessingSettings(
          /*multi_channel_capture_processing=*/true);
  EXPECT_FALSE(settings_with_system_ns.noise_suppression);
}

TEST(AudioProcessingPropertiesToAudioProcessingSettingsTest,
     SystemAgcDeactivatesBrowserAgc) {
  // Verify that gain control is by default enabled, since otherwise this test
  // does not work.
  constexpr AudioProcessingProperties kPropertiesWithoutSystemAgc{
      .system_gain_control_activated = false};
  media::AudioProcessingSettings settings_without_system_agc =
      kPropertiesWithoutSystemAgc.ToAudioProcessingSettings(
          /*multi_channel_capture_processing=*/true);
  EXPECT_TRUE(settings_without_system_agc.automatic_gain_control);

  constexpr AudioProcessingProperties kPropertiesWithSystemAgc{
      .system_gain_control_activated = true};
  media::AudioProcessingSettings settings_with_system_agc =
      kPropertiesWithSystemAgc.ToAudioProcessingSettings(
          /*multi_channel_capture_processing=*/true);
  EXPECT_FALSE(settings_with_system_agc.automatic_gain_control);
}

}  // namespace blink
