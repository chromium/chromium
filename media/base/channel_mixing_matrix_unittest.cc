// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/channel_mixing_matrix.h"

#include <stddef.h>

#include <array>

#include "base/strings/stringprintf.h"
#include "media/base/channel_mixer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// Test all possible layout conversions can be constructed and mixed.
TEST(ChannelMixingMatrixTest, ConstructAllPossibleLayouts) {
  for (ChannelLayout input_layout = CHANNEL_LAYOUT_MONO;
       input_layout <= CHANNEL_LAYOUT_MAX;
       input_layout = static_cast<ChannelLayout>(input_layout + 1)) {
    for (ChannelLayout output_layout = CHANNEL_LAYOUT_MONO;
         output_layout <= CHANNEL_LAYOUT_MAX;
         output_layout = static_cast<ChannelLayout>(output_layout + 1)) {
      // BITSTREAM can't be tested here based on the current approach.
      // CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC is deprecated.
      // Stereo down mix should never be the output layout.
      if (input_layout == CHANNEL_LAYOUT_BITSTREAM ||
          input_layout == CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC ||
          output_layout == CHANNEL_LAYOUT_BITSTREAM ||
          output_layout == CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC ||
          output_layout == CHANNEL_LAYOUT_STEREO_DOWNMIX) {
        continue;
      }

      SCOPED_TRACE(base::StringPrintf("Input Layout: %d, Output Layout: %d",
                                      input_layout, output_layout));
      std::vector<std::vector<float>> matrix;
      // `FromLayout()` cannot take DISCRETE, provide a default channel count
      // and ensure we can still create the transformation matrix.
      ChannelLayoutConfig input_config =
          input_layout == CHANNEL_LAYOUT_DISCRETE
              ? ChannelLayoutConfig(CHANNEL_LAYOUT_DISCRETE, 2)
              : ChannelLayoutConfig::FromLayout(input_layout);
      ChannelLayoutConfig output_config =
          output_layout == CHANNEL_LAYOUT_DISCRETE
              ? ChannelLayoutConfig(CHANNEL_LAYOUT_DISCRETE, 6)
              : ChannelLayoutConfig::FromLayout(output_layout);
      ChannelMixingMatrix matrix_builder(input_config, output_config);
      matrix_builder.CreateTransformationMatrix(&matrix);
    }
  }
}

// Verify channels are mixed and scaled correctly.
TEST(ChannelMixingMatrixTest, StereoToMono) {
  std::vector<std::vector<float>> matrix;
  ChannelMixingMatrix matrix_builder(ChannelLayoutConfig::Stereo(),
                                     ChannelLayoutConfig::Mono());
  bool remapping = matrix_builder.CreateTransformationMatrix(&matrix);

  //                      Input: stereo
  //                      LEFT  RIGHT
  // Output: mono CENTER  0.5   0.5
  //
  EXPECT_FALSE(remapping);
  EXPECT_EQ(1u, matrix.size());
  EXPECT_EQ(2u, matrix[0].size());
  EXPECT_EQ(0.5f, matrix[0][0]);
  EXPECT_EQ(0.5f, matrix[0][1]);
}

TEST(ChannelMixingMatrixTest, StereoTo1Point1) {
  std::vector<std::vector<float>> matrix;
  ChannelMixingMatrix matrix_builder(
      ChannelLayoutConfig::Stereo(),
      ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_1_1>());
  bool remapping = matrix_builder.CreateTransformationMatrix(&matrix);

  //                     Input: stereo
  //                     LEFT   RIGHT
  // Output: 1.1 CENTER  0.5    0.5
  //             LFE     0      0
  //
  EXPECT_FALSE(remapping);
  EXPECT_EQ(2u, matrix.size());
  EXPECT_EQ(2u, matrix[0].size());
  EXPECT_EQ(0.5f, matrix[0][0]);
  EXPECT_EQ(0.5f, matrix[0][1]);
  EXPECT_EQ(2u, matrix[1].size());
  EXPECT_EQ(0.0f, matrix[1][0]);
  EXPECT_EQ(0.0f, matrix[1][1]);
}

TEST(ChannelMixingMatrixTest, MonoToStereo) {
  std::vector<std::vector<float>> matrix;
  ChannelMixingMatrix matrix_builder(ChannelLayoutConfig::Mono(),
                                     ChannelLayoutConfig::Stereo());
  bool remapping = matrix_builder.CreateTransformationMatrix(&matrix);

  //                       Input: mono
  //                       CENTER
  // Output: stereo LEFT   1
  //                RIGHT  1
  //
  EXPECT_TRUE(remapping);
  EXPECT_EQ(2u, matrix.size());
  EXPECT_EQ(1u, matrix[0].size());
  EXPECT_EQ(1.0f, matrix[0][0]);
  EXPECT_EQ(1u, matrix[1].size());
  EXPECT_EQ(1.0f, matrix[1][0]);
}

TEST(ChannelMixingMatrixTest, 1Point1ToStereo) {
  std::vector<std::vector<float>> matrix;
  ChannelMixingMatrix matrix_builder(
      ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_1_1>(),
      ChannelLayoutConfig::Stereo());
  bool remapping = matrix_builder.CreateTransformationMatrix(&matrix);

  //                       Input: 1.1
  //                       CENTER  LFE
  // Output: stereo LEFT   1       0.707107
  //                RIGHT  1       0.707107
  //
  EXPECT_FALSE(remapping);
  EXPECT_EQ(2u, matrix.size());
  EXPECT_EQ(2u, matrix[0].size());
  EXPECT_EQ(1.0f, matrix[0][0]);
  EXPECT_FLOAT_EQ(ChannelMixer::kHalfPower, matrix[0][1]);
  EXPECT_EQ(2u, matrix[1].size());
  EXPECT_EQ(1.0f, matrix[1][0]);
  EXPECT_FLOAT_EQ(ChannelMixer::kHalfPower, matrix[1][1]);
}

TEST(ChannelMixingMatrixTest, MonoTo5Point1) {
  std::vector<std::vector<float>> matrix;
  ChannelMixingMatrix matrix_builder(
      ChannelLayoutConfig::Mono(),
      ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_5_1>());
  bool remapping = matrix_builder.CreateTransformationMatrix(&matrix);

  //                       Input: mono
  //                       CENTER
  // Output: 5.1    LEFT   1
  //                RIGHT  1
  //                CENTER 0
  //                LFE    0
  //                SL     0
  //                SR     0
  //
  EXPECT_FALSE(remapping);
  EXPECT_EQ(6u, matrix.size());
  EXPECT_EQ(1u, matrix[0].size());
  EXPECT_EQ(1.0f, matrix[0][0]);
  EXPECT_EQ(1u, matrix[1].size());
  EXPECT_EQ(1.0f, matrix[1][0]);
  for (size_t i = 2; i < 6; i++) {
    EXPECT_EQ(1u, matrix[i].size());
    EXPECT_EQ(0.0f, matrix[i][0]);
  }
}

TEST(ChannelMixingMatrixTest, 1Point1To5Point1) {
  std::vector<std::vector<float>> matrix;
  ChannelMixingMatrix matrix_builder(
      ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_1_1>(),
      ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_5_1>());
  bool remapping = matrix_builder.CreateTransformationMatrix(&matrix);

  //                       Input: 1.1
  //                       CENTER  LFE
  // Output: 5.1    LEFT   1       0
  //                RIGHT  1       0
  //                CENTER 0       0
  //                LFE    0       1
  //                SL     0       0
  //                SR     0       0
  //
  EXPECT_FALSE(remapping);
  EXPECT_EQ(6u, matrix.size());
  EXPECT_EQ(2u, matrix[0].size());
  EXPECT_EQ(1.0f, matrix[0][0]);
  EXPECT_EQ(0.0f, matrix[0][1]);
  EXPECT_EQ(2u, matrix[1].size());
  EXPECT_EQ(1.0f, matrix[1][0]);
  EXPECT_EQ(0.0f, matrix[1][1]);
  EXPECT_EQ(2u, matrix[2].size());
  EXPECT_EQ(0.0f, matrix[2][0]);
  EXPECT_EQ(0.0f, matrix[2][1]);
  EXPECT_EQ(2u, matrix[2].size());
  EXPECT_EQ(0.0f, matrix[3][0]);
  EXPECT_EQ(1.0f, matrix[3][1]);
  for (size_t i = 4; i < 6; i++) {
    EXPECT_EQ(2u, matrix[i].size());
    EXPECT_EQ(0.0f, matrix[i][0]);
    EXPECT_EQ(0.0f, matrix[i][1]);
  }
}

TEST(ChannelMixingMatrixTest, 5Point1ToMono) {
  std::vector<std::vector<float>> matrix;
  ChannelMixingMatrix matrix_builder(
      ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_5_1>(),
      ChannelLayoutConfig::Mono());
  bool remapping = matrix_builder.CreateTransformationMatrix(&matrix);

  // Note: 1/sqrt(2) is shown as 0.707.
  //
  //                       Input: 5.1
  //                       LEFT   RIGHT  CENTER  LFE    SIDE_LEFT  SIDE_RIGHT
  // Output: mono  CENTER  0.707  0.707  1       0.707  0.707      0.707
  //
  EXPECT_FALSE(remapping);
  EXPECT_EQ(1u, matrix.size());
  EXPECT_EQ(6u, matrix[0].size());
  EXPECT_FLOAT_EQ(ChannelMixer::kHalfPower, matrix[0][0]);
  EXPECT_FLOAT_EQ(ChannelMixer::kHalfPower, matrix[0][1]);
  // The center channel will be mixed at scale 1.
  EXPECT_EQ(1.0f, matrix[0][2]);
  EXPECT_FLOAT_EQ(ChannelMixer::kHalfPower, matrix[0][3]);
  EXPECT_FLOAT_EQ(ChannelMixer::kHalfPower, matrix[0][4]);
  EXPECT_FLOAT_EQ(ChannelMixer::kHalfPower, matrix[0][5]);
}

TEST(ChannelMixingMatrixTest, 5Point1To1Point1) {
  std::vector<std::vector<float>> matrix;
  ChannelMixingMatrix matrix_builder(
      ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_5_1>(),
      ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_1_1>());
  bool remapping = matrix_builder.CreateTransformationMatrix(&matrix);

  // Note: 1/sqrt(2) is shown as 0.707.
  //
  //                      Input: 5.1
  //                      LEFT   RIGHT  CENTER  LFE    SIDE_LEFT  SIDE_RIGHT
  // Output: 1.1  CENTER  0.707  0.707  1       0      0.707      0.707
  //              LFE     0      0      0       1      0          0
  //
  EXPECT_FALSE(remapping);
  EXPECT_EQ(2u, matrix.size());
  EXPECT_EQ(6u, matrix[0].size());
  EXPECT_FLOAT_EQ(ChannelMixer::kHalfPower, matrix[0][0]);
  EXPECT_FLOAT_EQ(ChannelMixer::kHalfPower, matrix[0][1]);
  // The center channel will be mixed at scale 1.
  EXPECT_EQ(1.0f, matrix[0][2]);
  EXPECT_EQ(0.0f, matrix[0][3]);
  EXPECT_FLOAT_EQ(ChannelMixer::kHalfPower, matrix[0][4]);
  EXPECT_FLOAT_EQ(ChannelMixer::kHalfPower, matrix[0][5]);
  EXPECT_EQ(6u, matrix[1].size());
  EXPECT_EQ(0.0f, matrix[1][0]);
  EXPECT_EQ(0.0f, matrix[1][1]);
  EXPECT_EQ(0.0f, matrix[1][2]);
  EXPECT_EQ(1.0f, matrix[1][3]);
  EXPECT_EQ(0.0f, matrix[1][4]);
  EXPECT_EQ(0.0f, matrix[1][5]);
}

TEST(ChannelMixingMatrixTest, 5Point1Point4To5Point1) {

  std::vector<std::vector<float>> matrix;
  ChannelMixingMatrix matrix_builder(
      ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_5_1_4>(),
      ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_5_1>());
  bool remapping = matrix_builder.CreateTransformationMatrix(&matrix);

  // Note: 1/sqrt(2) is shown as 0.707.
  //
  // Input: 5.1.4
  // L  R  C  LFE  SL  SR  TFL  TFR  TBL  TBR
  //
  // Output: 5.1
  //       L  R  C  LFE  SL  SR  TFL    TFR    TBL    TBR
  // L     1  0  0   0   0   0   0      0      0      0
  // R     0  1  0   0   0   0   0      0      0      0
  // C     0  0  1   0   0   0   0      0      0      0
  // LFE   0  0  0   1   0   0   0      0      0      0
  // SL    0  0  0   0   1   0   0.707  0      0.707  0
  // SR    0  0  0   0   0   1   0      0.707  0      0.707
  //
  EXPECT_FALSE(remapping);
  EXPECT_EQ(6u, matrix.size());

  EXPECT_EQ(10u, matrix[0].size());
  EXPECT_EQ(1.0f, matrix[0][0]);
  EXPECT_EQ(0.0f, matrix[0][6]);

  EXPECT_EQ(10u, matrix[1].size());
  EXPECT_EQ(1.0f, matrix[1][1]);
  EXPECT_EQ(0.0f, matrix[1][7]);

  EXPECT_EQ(10u, matrix[2].size());
  EXPECT_EQ(1.0f, matrix[2][2]);
  EXPECT_EQ(0.0f, matrix[2][6]);

  EXPECT_EQ(10u, matrix[3].size());
  EXPECT_EQ(1.0f, matrix[3][3]);

  EXPECT_EQ(10u, matrix[4].size());
  EXPECT_EQ(1.0f, matrix[4][4]);
  EXPECT_FLOAT_EQ(ChannelMixer::kHalfPower, matrix[4][8]);
  EXPECT_FLOAT_EQ(ChannelMixer::kHalfPower, matrix[4][6]);

  EXPECT_EQ(10u, matrix[5].size());
  EXPECT_EQ(1.0f, matrix[5][5]);
  EXPECT_FLOAT_EQ(ChannelMixer::kHalfPower, matrix[5][9]);
  EXPECT_FLOAT_EQ(ChannelMixer::kHalfPower, matrix[5][7]);
}

TEST(ChannelMixingMatrixTest, 7Point1Point4To7Point1) {

  std::vector<std::vector<float>> matrix;
  ChannelMixingMatrix matrix_builder(
      ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_7_1_4>(),
      ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_7_1>());
  bool remapping = matrix_builder.CreateTransformationMatrix(&matrix);

  // Note: 1/sqrt(2) is shown as 0.707.
  //
  // Input: 7.1.4
  // L  R  C  LFE  BL  BR  SL  SR  TFL  TFR  TBL  TBR
  //
  // Output: 7.1
  //       L  R  C  LFE  BL  BR  SL  SR  TFL    TFR    TBL    TBR
  // L     1  0  0   0   0   0   0   0   0      0      0      0
  // R     0  1  0   0   0   0   0   0   0      0      0      0
  // C     0  0  1   0   0   0   0   0   0      0      0      0
  // LFE   0  0  0   1   0   0   0   0   0      0      0      0
  // BL    0  0  0   0   1   0   0   0   0      0      0.707  0
  // BR    0  0  0   0   0   1   0   0   0      0      0      0.707
  // SL    0  0  0   0   0   0   1   0   0.707  0      0      0
  // SR    0  0  0   0   0   0   0   1   0      0.707  0      0
  //
  EXPECT_FALSE(remapping);
  EXPECT_EQ(8u, matrix.size());

  EXPECT_EQ(12u, matrix[0].size());
  EXPECT_EQ(1.0f, matrix[0][0]);
  EXPECT_EQ(0.0f, matrix[0][8]);

  EXPECT_EQ(12u, matrix[1].size());
  EXPECT_EQ(1.0f, matrix[1][1]);
  EXPECT_EQ(0.0f, matrix[1][9]);

  EXPECT_EQ(12u, matrix[2].size());
  EXPECT_EQ(1.0f, matrix[2][2]);
  EXPECT_EQ(0.0f, matrix[2][8]);

  EXPECT_EQ(12u, matrix[3].size());
  EXPECT_EQ(1.0f, matrix[3][3]);

  EXPECT_EQ(12u, matrix[4].size());
  EXPECT_EQ(1.0f, matrix[4][4]);
  EXPECT_FLOAT_EQ(ChannelMixer::kHalfPower, matrix[4][10]);
  EXPECT_EQ(0.0f, matrix[4][8]);

  EXPECT_EQ(12u, matrix[5].size());
  EXPECT_EQ(1.0f, matrix[5][5]);
  EXPECT_FLOAT_EQ(ChannelMixer::kHalfPower, matrix[5][11]);
  EXPECT_EQ(0.0f, matrix[5][9]);

  EXPECT_EQ(12u, matrix[6].size());
  EXPECT_EQ(1.0f, matrix[6][6]);
  EXPECT_FLOAT_EQ(ChannelMixer::kHalfPower, matrix[6][8]);
  EXPECT_EQ(0.0f, matrix[6][10]);

  EXPECT_EQ(12u, matrix[7].size());
  EXPECT_EQ(1.0f, matrix[7][7]);
  EXPECT_FLOAT_EQ(ChannelMixer::kHalfPower, matrix[7][9]);
  EXPECT_EQ(0.0f, matrix[7][11]);
}

void VerifyMatrix(const std::vector<std::vector<float>>& matrix,
                  const std::vector<std::vector<float>>& expected_matrix) {
  ASSERT_EQ(expected_matrix.size(), matrix.size());
  for (size_t i = 0; i < expected_matrix.size(); ++i) {
    ASSERT_EQ(expected_matrix[i].size(), matrix[i].size()) << "At row " << i;
    for (size_t j = 0; j < expected_matrix[i].size(); ++j) {
      EXPECT_FLOAT_EQ(expected_matrix[i][j], matrix[i][j])
          << "At row " << i << ", col " << j;
    }
  }
}

TEST(ChannelMixingMatrixTest, DiscreteToDiscrete) {
  struct TestCase {
    int input_channels;
    int output_channels;
  };
  const auto test_case = std::to_array<TestCase>({
      {1, 1},
      {2, 2},
      {2, 5},
      {5, 2},
  });

  for (auto n : test_case) {
    int input_channels = n.input_channels;
    int output_channels = n.output_channels;
    std::vector<std::vector<float>> matrix;
    ChannelMixingMatrix matrix_builder(
        ChannelLayoutConfig(CHANNEL_LAYOUT_DISCRETE, input_channels),
        ChannelLayoutConfig(CHANNEL_LAYOUT_DISCRETE, output_channels));
    bool remapping = matrix_builder.CreateTransformationMatrix(&matrix);
    EXPECT_TRUE(remapping);

    std::vector<std::vector<float>> expected(
        output_channels, std::vector<float>(input_channels, 0.0f));
    int passthrough = std::min(input_channels, output_channels);
    for (int i = 0; i < passthrough; ++i) {
      expected[i][i] = 1.0f;
    }
    VerifyMatrix(matrix, expected);
  }
}

TEST(ChannelMixingMatrixTest, DiscreteToOther) {
  {
    std::vector<std::vector<float>> matrix;
    ChannelMixingMatrix matrix_builder(
        ChannelLayoutConfig(CHANNEL_LAYOUT_DISCRETE, 4),
        ChannelLayoutConfig::Stereo());
    bool remapping = matrix_builder.CreateTransformationMatrix(&matrix);

    // For discrete input, channels are passed through by index up to output
    // count.
    //                       Input: discrete 4-channel
    //                       CH_0  CH_1  CH_2  CH_3
    // Output: stereo LEFT   1     0     0     0
    //                RIGHT  0     1     0     0
    //
    const std::vector<std::vector<float>> expected = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
    };

    EXPECT_TRUE(remapping);
    VerifyMatrix(matrix, expected);
  }

  {
    std::vector<std::vector<float>> matrix;
    ChannelMixingMatrix matrix_builder(
        ChannelLayoutConfig(CHANNEL_LAYOUT_DISCRETE, 2),
        ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_QUAD>());
    bool remapping = matrix_builder.CreateTransformationMatrix(&matrix);

    //                     Input: discrete 2-channel
    //                     CH_0  CH_1
    // Output: quad LEFT   1     0
    //              RIGHT  0     1
    //              BACK_L 0     0
    //              BACK_R 0     0
    //
    const std::vector<std::vector<float>> expected = {
        {1.0f, 0.0f},
        {0.0f, 1.0f},
        {0.0f, 0.0f},
        {0.0f, 0.0f},
    };

    EXPECT_TRUE(remapping);
    VerifyMatrix(matrix, expected);
  }
}

TEST(ChannelMixingMatrixTest, OtherToDiscrete) {
  {
    std::vector<std::vector<float>> matrix;
    ChannelMixingMatrix matrix_builder(
        ChannelLayoutConfig::Stereo(),
        ChannelLayoutConfig(CHANNEL_LAYOUT_DISCRETE, 4));
    bool remapping = matrix_builder.CreateTransformationMatrix(&matrix);

    // For discrete output, input channels are mapped 1:1 by index to output
    // channels.
    //                              Input: stereo
    //                              LEFT  RIGHT
    // Output: discrete 4-channel
    //         CH_0                 1     0
    //         CH_1                 0     1
    //         CH_2                 0     0
    //         CH_3                 0     0
    //
    const std::vector<std::vector<float>> expected = {
        {1.0f, 0.0f},
        {0.0f, 1.0f},
        {0.0f, 0.0f},
        {0.0f, 0.0f},
    };

    EXPECT_TRUE(remapping);
    VerifyMatrix(matrix, expected);
  }

  {
    std::vector<std::vector<float>> matrix;
    ChannelMixingMatrix matrix_builder(
        ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_QUAD>(),
        ChannelLayoutConfig(CHANNEL_LAYOUT_DISCRETE, 2));
    bool remapping = matrix_builder.CreateTransformationMatrix(&matrix);

    //                              Input: quad
    //                              FL    FR    BL    BR
    // Output: discrete 2-channel
    //         CH_0                 1     0     0     0
    //         CH_1                 0     1     0     0
    //
    const std::vector<std::vector<float>> expected = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
    };

    EXPECT_TRUE(remapping);
    VerifyMatrix(matrix, expected);
  }
}

}  // namespace media
