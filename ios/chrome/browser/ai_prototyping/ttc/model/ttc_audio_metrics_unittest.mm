// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_metrics.h"

#import <cmath>
#import <vector>

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using TTCAudioMetricsTest = PlatformTest;

// Tests that CalculateRMS returns 0.0 for an empty buffer.
TEST_F(TTCAudioMetricsTest, TestCalculateRMSEmptyBuffer) {
  EXPECT_FLOAT_EQ(ttc::CalculateRMS({}), 0.0f);
}

// Tests that CalculateRMS returns 0.0 for digital silence.
TEST_F(TTCAudioMetricsTest, TestCalculateRMSSilence) {
  std::vector<float> silence(1024, 0.0f);
  EXPECT_FLOAT_EQ(ttc::CalculateRMS(silence), 0.0f);
}

// Tests that CalculateRMS returns 1.0 for a full scale DC signal.
TEST_F(TTCAudioMetricsTest, TestCalculateRMSFullScaleDC) {
  std::vector<float> dc(1024, 1.0f);
  EXPECT_FLOAT_EQ(ttc::CalculateRMS(dc), 1.0f);
}

// Tests that CalculateRMS calculates ~0.7071 for a unit amplitude sine wave.
TEST_F(TTCAudioMetricsTest, TestCalculateRMSSineWave) {
  constexpr size_t kSampleCount = 1000;
  std::vector<float> sine(kSampleCount);
  for (size_t i = 0; i < kSampleCount; ++i) {
    sine[i] = std::sin(2.0f * static_cast<float>(M_PI) * 5.0f * i /
                       static_cast<float>(kSampleCount));
  }
  EXPECT_NEAR(ttc::CalculateRMS(sine), 0.7071f, 0.005f);
}

// Tests that CalculateRMS safely clamps corrupted NaN samples to 0.0.
TEST_F(TTCAudioMetricsTest, TestCalculateRMSNaNValues) {
  std::vector<float> nan_samples(256, NAN);
  EXPECT_FLOAT_EQ(ttc::CalculateRMS(nan_samples), 0.0f);
}

// Tests that LinearRmsToPerceptualLevel returns 0.0 for silence or zero RMS.
TEST_F(TTCAudioMetricsTest, TestLinearRmsToPerceptualLevelZeroOrSilence) {
  EXPECT_FLOAT_EQ(ttc::LinearRmsToPerceptualLevel(0.0f), 0.0f);
}

// Tests that LinearRmsToPerceptualLevel safely handles negative or NaN values.
TEST_F(TTCAudioMetricsTest, TestLinearRmsToPerceptualLevelNegativeOrNaN) {
  EXPECT_FLOAT_EQ(ttc::LinearRmsToPerceptualLevel(-0.5f), 0.0f);
  EXPECT_FLOAT_EQ(ttc::LinearRmsToPerceptualLevel(NAN), 0.0f);
}

// Tests that LinearRmsToPerceptualLevel returns 0.0 for signals below -45 dBFS
// ambient noise floor.
TEST_F(TTCAudioMetricsTest, TestLinearRmsToPerceptualLevelBelowFloor) {
  // -50 dBFS corresponds to amplitude ~0.003162.
  EXPECT_FLOAT_EQ(ttc::LinearRmsToPerceptualLevel(0.003162f), 0.0f);
}

// Tests that LinearRmsToPerceptualLevel correctly maps speech range (-30 dBFS
// and -20 dBFS) to human perceptual scale [0.0, 1.0].
TEST_F(TTCAudioMetricsTest, TestLinearRmsToPerceptualLevelSpeechRange) {
  // -30 dBFS corresponds to amplitude 10^(-30/20) ~ 0.0316227.
  // Normalized: (-30 - (-45)) / (0 - (-45)) = 15 / 45 = 0.3333.
  EXPECT_NEAR(ttc::LinearRmsToPerceptualLevel(0.0316227f), 0.3333f, 0.005f);

  // -20 dBFS corresponds to amplitude 10^(-20/20) = 0.1.
  // Normalized: (-20 - (-45)) / 45 = 25 / 45 = 0.5555.
  EXPECT_NEAR(ttc::LinearRmsToPerceptualLevel(0.1f), 0.5555f, 0.005f);
}

// Tests that LinearRmsToPerceptualLevel returns 1.0 for full scale 0 dBFS and
// above.
TEST_F(TTCAudioMetricsTest, TestLinearRmsToPerceptualLevelFullScale) {
  EXPECT_FLOAT_EQ(ttc::LinearRmsToPerceptualLevel(1.0f), 1.0f);
  EXPECT_FLOAT_EQ(ttc::LinearRmsToPerceptualLevel(1.5f), 1.0f);
}

}  // namespace
