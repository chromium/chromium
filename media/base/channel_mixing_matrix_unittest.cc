// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/channel_mixing_matrix.h"

#include <stddef.h>

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
      // DISCRETE, BITSTREAM can't be tested here based on the current approach.
      // CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC is deprecated.
      // Stereo down mix should never be the output layout.
      if (input_layout == CHANNEL_LAYOUT_BITSTREAM ||
          input_layout == CHANNEL_LAYOUT_DISCRETE ||
          input_layout == CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC ||
          output_layout == CHANNEL_LAYOUT_BITSTREAM ||
          output_layout == CHANNEL_LAYOUT_DISCRETE ||
          output_layout == CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC ||
          output_layout == CHANNEL_LAYOUT_STEREO_DOWNMIX) {
        continue;
      }

      SCOPED_TRACE(base::StringPrintf("Input Layout: %d, Output Layout: %d",
                                      input_layout, output_layout));
      std::vector<std::vector<float>> matrix;
      ChannelMixingMatrix matrix_builder(
          input_layout, ChannelLayoutToChannelCount(input_layout),
          output_layout, ChannelLayoutToChannelCount(output_layout));
      matrix_builder.CreateTransformationMatrix(&matrix);
    }
  }
}

// Verify channels are mixed and scaled correctly.
TEST(ChannelMixingMatrixTest, StereoToMono) {
  ChannelLayout input_layout = CHANNEL_LAYOUT_STEREO;
  ChannelLayout output_layout = CHANNEL_LAYOUT_MONO;
  std::vector<std::vector<float>> matrix;
  ChannelMixingMatrix matrix_builder(
      input_layout, ChannelLayoutToChannelCount(input_layout), output_layout,
      ChannelLayoutToChannelCount(output_layout));
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
  ChannelLayout input_layout = CHANNEL_LAYOUT_STEREO;
  ChannelLayout output_layout = CHANNEL_LAYOUT_1_1;
  std::vector<std::vector<float>> matrix;
  ChannelMixingMatrix matrix_builder(
      input_layout, ChannelLayoutToChannelCount(input_layout), output_layout,
      ChannelLayoutToChannelCount(output_layout));
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
  ChannelLayout input_layout = CHANNEL_LAYOUT_MONO;
  ChannelLayout output_layout = CHANNEL_LAYOUT_STEREO;
  std::vector<std::vector<float>> matrix;
  ChannelMixingMatrix matrix_builder(
      input_layout, ChannelLayoutToChannelCount(input_layout), output_layout,
      ChannelLayoutToChannelCount(output_layout));
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
  ChannelLayout input_layout = CHANNEL_LAYOUT_1_1;
  ChannelLayout output_layout = CHANNEL_LAYOUT_STEREO;
  std::vector<std::vector<float>> matrix;
  ChannelMixingMatrix matrix_builder(
      input_layout, ChannelLayoutToChannelCount(input_layout), output_layout,
      ChannelLayoutToChannelCount(output_layout));
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
  ChannelLayout input_layout = CHANNEL_LAYOUT_MONO;
  ChannelLayout output_layout = CHANNEL_LAYOUT_5_1;
  std::vector<std::vector<float>> matrix;
  ChannelMixingMatrix matrix_builder(
      input_layout, ChannelLayoutToChannelCount(input_layout), output_layout,
      ChannelLayoutToChannelCount(output_layout));
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
  ChannelLayout input_layout = CHANNEL_LAYOUT_1_1;
  ChannelLayout output_layout = CHANNEL_LAYOUT_5_1;
  std::vector<std::vector<float>> matrix;
  ChannelMixingMatrix matrix_builder(
      input_layout, ChannelLayoutToChannelCount(input_layout), output_layout,
      ChannelLayoutToChannelCount(output_layout));
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
  ChannelLayout input_layout = CHANNEL_LAYOUT_5_1;
  ChannelLayout output_layout = CHANNEL_LAYOUT_MONO;
  std::vector<std::vector<float>> matrix;
  ChannelMixingMatrix matrix_builder(
      input_layout, ChannelLayoutToChannelCount(input_layout), output_layout,
      ChannelLayoutToChannelCount(output_layout));
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
  ChannelLayout input_layout = CHANNEL_LAYOUT_5_1;
  ChannelLayout output_layout = CHANNEL_LAYOUT_1_1;
  std::vector<std::vector<float>> matrix;
  ChannelMixingMatrix matrix_builder(
      input_layout, ChannelLayoutToChannelCount(input_layout), output_layout,
      ChannelLayoutToChannelCount(output_layout));
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

TEST(ChannelMixingMatrixTest, DiscreteToDiscrete) {
  const struct {
    int input_channels;
    int output_channels;
  } test_case[] = {
    {2, 2}, {2, 5}, {5, 2},
  };

  for (size_t n = 0; n < std::size(test_case); n++) {
    int input_channels = test_case[n].input_channels;
    int output_channels = test_case[n].output_channels;
    std::vector<std::vector<float>> matrix;
    ChannelMixingMatrix matrix_builder(CHANNEL_LAYOUT_DISCRETE, input_channels,
                                       CHANNEL_LAYOUT_DISCRETE,
                                       output_channels);
    bool remapping = matrix_builder.CreateTransformationMatrix(&matrix);
    EXPECT_TRUE(remapping);
    EXPECT_EQ(static_cast<size_t>(output_channels), matrix.size());
    for (int i = 0; i < output_channels; i++) {
      EXPECT_EQ(static_cast<size_t>(input_channels), matrix[i].size());
      for (int j = 0; j < input_channels; j++) {
        if (i == j) {
          EXPECT_EQ(1.0f, matrix[i][j]);
        } else {
          EXPECT_EQ(0.0f, matrix[i][j]);
        }
      }
    }
  }
}

}  // namespace media
