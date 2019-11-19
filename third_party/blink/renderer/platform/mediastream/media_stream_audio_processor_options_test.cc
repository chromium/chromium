// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_processor_options.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(ConfigAutomaticGainControlTest, EnableDefaultAGC1) {
  webrtc::AudioProcessing::Config apm_config;
  ConfigAutomaticGainControl(&apm_config,
                             true,   // |agc_enabled|.
                             false,  // |experimental_agc_enabled|.
                             false,  // |use_hybrid_agc|.
                             base::nullopt, base::nullopt, base::nullopt);
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
  ConfigAutomaticGainControl(&apm_config,
                             true,   // |agc_enabled|.
                             false,  // |experimental_agc_enabled|.
                             false,  // |use_hybrid_agc|.
                             base::nullopt, base::nullopt, compression_gain_db);
  EXPECT_FALSE(apm_config.gain_controller1.enabled);
  EXPECT_TRUE(apm_config.gain_controller2.enabled);
  EXPECT_FALSE(apm_config.gain_controller2.adaptive_digital.enabled);
  EXPECT_FLOAT_EQ(apm_config.gain_controller2.fixed_digital.gain_db,
                  compression_gain_db);
}

TEST(ConfigAutomaticGainControlTest, EnableHybridAGC) {
  webrtc::AudioProcessing::Config apm_config;
  const bool use_peaks_not_rms = true;
  const int saturation_margin = 10;
  const double compression_gain_db = 10.0;  // Will test that it has no effect.
  ConfigAutomaticGainControl(&apm_config,
                             true,  // |agc_enabled|.
                             true,  // |experimental_agc_enabled|.
                             true,  // |use_hybrid_agc|.
                             use_peaks_not_rms, saturation_margin,
                             compression_gain_db);
  EXPECT_TRUE(apm_config.gain_controller1.enabled);
  EXPECT_EQ(
      apm_config.gain_controller1.mode,
#if defined(OS_ANDROID)
      webrtc::AudioProcessing::Config::GainController1::Mode::kFixedDigital);
#else
      webrtc::AudioProcessing::Config::GainController1::Mode::kAdaptiveAnalog);
#endif  // defined(OS_ANDROID)
  EXPECT_TRUE(apm_config.gain_controller2.enabled);
  EXPECT_EQ(apm_config.gain_controller2.fixed_digital.gain_db, 0);
  EXPECT_TRUE(apm_config.gain_controller2.adaptive_digital.enabled);
  EXPECT_EQ(
      apm_config.gain_controller2.adaptive_digital.level_estimator,
      webrtc::AudioProcessing::Config::GainController2::LevelEstimator::kPeak);
  EXPECT_EQ(
      apm_config.gain_controller2.adaptive_digital.extra_saturation_margin_db,
      saturation_margin);
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
