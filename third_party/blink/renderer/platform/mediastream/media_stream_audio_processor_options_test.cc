// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_processor_options.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(ConfigAutomaticGainControlTest, EnableDefaultAGC1) {
  webrtc::AudioProcessing::Config apm_config;
  ConfigAutomaticGainControl(
      /*agc_enabled=*/true,
      /*experimental_agc_enabled=*/false,
      /*agc2_properties=*/base::nullopt,
      /*compression_gain_db=*/base::nullopt, apm_config);
  EXPECT_TRUE(apm_config.gain_controller1.enabled);
  EXPECT_EQ(
      apm_config.gain_controller1.mode,
#if defined(OS_ANDROID)
      webrtc::AudioProcessing::Config::GainController1::Mode::kFixedDigital);
#else
      webrtc::AudioProcessing::Config::GainController1::Mode::kAdaptiveAnalog);
#endif  // defined(OS_ANDROID)
}

TEST(ConfigAutomaticGainControlTest, EnableFixedDigitalAGC2) {
  webrtc::AudioProcessing::Config apm_config;
  const double compression_gain_db = 10.0;
  ConfigAutomaticGainControl(
      /*agc_enabled=*/true,
      /*experimental_agc_enabled=*/false,
      /*agc2_properties=*/base::nullopt, compression_gain_db, apm_config);
  EXPECT_FALSE(apm_config.gain_controller1.enabled);
  EXPECT_TRUE(apm_config.gain_controller2.enabled);
  EXPECT_FALSE(apm_config.gain_controller2.adaptive_digital.enabled);
  EXPECT_FLOAT_EQ(apm_config.gain_controller2.fixed_digital.gain_db,
                  compression_gain_db);
}

TEST(ConfigAutomaticGainControlTest, EnableHybridAGC) {
  webrtc::AudioProcessing::Config apm_config;
  blink::AdaptiveGainController2Properties agc2_properties;
  agc2_properties.vad_probability_attack = 0.2f;
  agc2_properties.use_peaks_not_rms = true;
  agc2_properties.level_estimator_speech_frames_threshold = 3;
  agc2_properties.initial_saturation_margin_db = 10;
  agc2_properties.extra_saturation_margin_db = 10;
  agc2_properties.gain_applier_speech_frames_threshold = 5;
  agc2_properties.max_gain_change_db_per_second = 4;
  agc2_properties.max_output_noise_level_dbfs = -22;
  const double compression_gain_db = 10.0;

  ConfigAutomaticGainControl(
      /*agc_enabled=*/true,
      /*experimental_agc_enabled=*/true, agc2_properties, compression_gain_db,
      apm_config);
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
  EXPECT_EQ(apm_config.gain_controller2.fixed_digital.gain_db, 0);

  const auto& adaptive_digital = apm_config.gain_controller2.adaptive_digital;
  EXPECT_TRUE(adaptive_digital.enabled);
  EXPECT_FLOAT_EQ(adaptive_digital.vad_probability_attack,
                  agc2_properties.vad_probability_attack);
  EXPECT_EQ(
      adaptive_digital.level_estimator,
      webrtc::AudioProcessing::Config::GainController2::LevelEstimator::kPeak);
  EXPECT_EQ(adaptive_digital.level_estimator_adjacent_speech_frames_threshold,
            agc2_properties.level_estimator_speech_frames_threshold);
  EXPECT_FLOAT_EQ(adaptive_digital.initial_saturation_margin_db,
                  agc2_properties.initial_saturation_margin_db);
  EXPECT_FLOAT_EQ(adaptive_digital.extra_saturation_margin_db,
                  agc2_properties.extra_saturation_margin_db);
  EXPECT_EQ(adaptive_digital.gain_applier_adjacent_speech_frames_threshold,
            agc2_properties.gain_applier_speech_frames_threshold);
  EXPECT_FLOAT_EQ(adaptive_digital.max_gain_change_db_per_second,
                  agc2_properties.max_gain_change_db_per_second);
  EXPECT_FLOAT_EQ(adaptive_digital.max_output_noise_level_dbfs,
                  agc2_properties.max_output_noise_level_dbfs);
}

TEST(PopulateApmConfigTest, DefaultWithoutConfigJson) {
  webrtc::AudioProcessing::Config apm_config;
  AudioProcessingProperties properties;
  base::Optional<double> gain_control_compression_gain_db;

  PopulateApmConfig(&apm_config, properties,
                    base::nullopt,  // |audio_processing_platform_config_json|.
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
  AudioProcessingProperties properties;
  base::Optional<std::string> audio_processing_platform_config_json =
      "{\"gain_control_compression_gain_db\": 10, "
      "\"pre_amplifier_fixed_gain_factor\": 2.0}";
  base::Optional<double> gain_control_compression_gain_db;

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
  AudioProcessingProperties properties;
  base::Optional<std::string> audio_processing_platform_config_json =
      "{\"noise_suppression_level\": 3}";
  base::Optional<double> gain_control_compression_gain_db;

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

}  // namespace blink
