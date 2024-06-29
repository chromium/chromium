// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/h264_rate_controller.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {
constexpr uint32_t kCommonAvgBitrate = 1000000;   // bits per second
constexpr uint32_t kCommonPeakBitrate = 2000000;  // bits per second
constexpr int kCommonFps = 30;
constexpr int kCommonFpsMax = 30;
constexpr uint32_t kCommonFrameHeight = 600;
constexpr uint32_t kCommonFrameWidth = 800;
constexpr size_t kCommonHRDBufferSize = 40000;  // bytes
constexpr base::TimeDelta kCommonGopMaxDuration = base::Seconds(4);
constexpr uint32_t kCommonQpMax = 51u;
constexpr uint32_t kCommonQpMin = 1u;
constexpr size_t kLayer0Index = 0;
constexpr size_t kLayer1Index = 1;

// Test H264RateControllerTest executes various operations on the H264 Rate
// Controller component. A test sequence of predefined frames is filled into the
// rate controller and the controller state is compared with the expected
// values.
class H264RateControllerTest : public testing::Test {
 public:
  struct RateControllerTestValues {
    bool is_buffer_full;
    size_t buffer_size;
    size_t buffer_bytes;
    size_t buffer_bytes_remaining;
    size_t last_frame_buffer_bytes;
    float frame_rate_mean_min;
    float frame_rate_mean_max;
    size_t last_frame_size_target;
    int buffer_fullness;
  };

  H264RateControllerTest() = default;

  void SetUp() override {
    rate_controller_settings_.content_type =
        VideoEncodeAccelerator::Config::ContentType::kCamera;
    rate_controller_settings_.frame_size.SetSize(kCommonFrameWidth,
                                                 kCommonFrameHeight);
    rate_controller_settings_.fixed_delta_qp = false;
    rate_controller_settings_.num_temporal_layers = 1;
    rate_controller_settings_.gop_max_duration = kCommonGopMaxDuration;
    rate_controller_settings_.frame_rate_max = kCommonFpsMax;
    rate_controller_settings_.layer_settings.emplace_back();
    rate_controller_settings_.layer_settings[0].avg_bitrate = kCommonAvgBitrate;
    rate_controller_settings_.layer_settings[0].hrd_buffer_size =
        kCommonHRDBufferSize;
    rate_controller_settings_.layer_settings[0].min_qp = kCommonQpMin;
    rate_controller_settings_.layer_settings[0].max_qp = kCommonQpMax;
    rate_controller_settings_.layer_settings[0].frame_rate = kCommonFps;

    // Copy operation test
    H264RateControllerSettings rate_controller_settings_copy =
        rate_controller_settings_;
    rate_controller_settings_ = rate_controller_settings_copy;
  }

 protected:
  // Runs a loop of predefined encoded frames. The default frame sequence
  // contains two intra frames at the beginning and in the middle of the
  // sequence. There are custom test sequences provided (identified by
  // `test_sequence_number`) which are used to set the Rate Controller into
  // specific state during QP estimation. In each cycle the following methods
  // are executed on the rate controller:
  // 1. ShrinkHRDBuffers()
  // 2. EstimateIntraFrameQP() or EstimateInterFrameQP()
  // 3. FinishIntraFrame() or FinishInterFrame()
  int RunTestSequence(uint32_t avg_bitrate,
                      int fps,
                      int frame_count,
                      size_t num_temporal_layers,
                      int test_sequence_number,
                      int start_frame_index,
                      int& last_intra_frame_qp,
                      int& last_inter_frame_qp) {
    constexpr size_t kFirstIntraFrameIndex = 0;
    const size_t kSecondIntraFrameIndex = frame_count / 2;
    size_t frame_size = avg_bitrate / 8 / fps;
    std::vector<size_t> frames;
    // Two temporal layers test.
    std::vector<size_t> custom_frames_1{12500, 3000,  4000, 3000, 4000,
                                        15000, 13000, 2500, 3700, 3200,
                                        3500,  3000,  3500, 3000, 2000};
    // Two temporal layers test with Fixed Delta QP mode enabled.
    std::vector<size_t> custom_frames_2{12500, 3000, 4000, 3000, 4000, 15000,
                                        13000, 2500, 3700, 3200, 0,    0};
    for (int i = 0; i < frame_count; ++i) {
      if (test_sequence_number == 1) {
        if (i < 14) {
          frames.push_back(custom_frames_1[i]);
        } else {
          frames.push_back(custom_frames_1[14]);
        }
      } else if (test_sequence_number == 2) {
        if (i < 10) {
          frames.push_back(custom_frames_2[i]);
        } else {
          frames.push_back(custom_frames_2[10]);
        }
      } else if (test_sequence_number == 3) {
        frames.push_back(0);
      } else {
        frames.push_back(frame_size);
      }
    }

    if (test_sequence_number <= 0) {
      frames[kFirstIntraFrameIndex] = frames[kFirstIntraFrameIndex] * 3;
      frames[kSecondIntraFrameIndex] = frames[kSecondIntraFrameIndex] * 3;
    }

    base::TimeDelta timestamp = base::Microseconds(
        start_frame_index * base::Time::kMicrosecondsPerSecond / fps);
    size_t layer_index = 0;
    size_t frame_index = 0;
    for (size_t encoded_size : frames) {
      if (num_temporal_layers > 1) {
        if (frame_index == kFirstIntraFrameIndex ||
            (test_sequence_number <= 0 &&
             frame_index == kSecondIntraFrameIndex) ||
            layer_index == 1) {
          layer_index = 0;
        } else {
          layer_index = 1;
        }
      }

      for (size_t i = 0; i < num_temporal_layers; ++i) {
        rate_controller_->temporal_layers(i).ShrinkHRDBuffer(timestamp);
      }

      if (frame_index == kFirstIntraFrameIndex ||
          (test_sequence_number <= 0 &&
           frame_index == kSecondIntraFrameIndex)) {
        rate_controller_->EstimateIntraFrameQP(timestamp);
        last_intra_frame_qp =
            rate_controller_->temporal_layers(layer_index).curr_frame_qp();
        rate_controller_->FinishIntraFrame(encoded_size, timestamp);
      } else {
        rate_controller_->EstimateInterFrameQP(layer_index, timestamp);
        last_inter_frame_qp =
            rate_controller_->temporal_layers(layer_index).curr_frame_qp();
        rate_controller_->FinishInterFrame(layer_index, encoded_size,
                                           timestamp);
      }

      ++frame_index;
      timestamp += base::Microseconds(base::Time::kMicrosecondsPerSecond / fps);
    }

    return start_frame_index + frame_count;
  }

  std::unique_ptr<H264RateController> rate_controller_;
  H264RateControllerSettings rate_controller_settings_;
};

// Test Cases

// The test runs a predefined sequence of frame sizes and checks the Rate
// Controller status after running the sequence.
TEST_F(H264RateControllerTest, RunH264RateController1TemporalLayerTest) {
  constexpr size_t kTestSequenceFrameCount = 30;
  constexpr RateControllerTestValues kExpectedValues1 = {
      false, 40000, 0, 40000, 0, 29.9f, 30.1f, 0, 0};
  constexpr RateControllerTestValues kExpectedValues2 = {
      false, 40000, 16633, 23367, 20801, 29.9f, 30.1f, 4104, 41};
  constexpr int kExpectedIntraFrameQP2 = 34;
  constexpr int kExpectedInterFrameQP2 = 36;

  rate_controller_ =
      std::make_unique<H264RateController>(rate_controller_settings_);

  rate_controller_->reset_frame_number();

  std::array<int, 1> buffer_fullness_array = {0};
  base::span<int> buffer_fullness_values(buffer_fullness_array);

  EXPECT_EQ(kExpectedValues1.buffer_bytes,
            rate_controller_->temporal_layers(kLayer0Index)
                .GetBufferBytesAtTime(base::TimeDelta()));
  EXPECT_EQ(kExpectedValues1.buffer_bytes_remaining,
            rate_controller_->temporal_layers(kLayer0Index)
                .GetBufferBytesRemainingAtTime(base::TimeDelta()));
  EXPECT_LT(kExpectedValues1.frame_rate_mean_min,
            rate_controller_->temporal_layers(kLayer0Index).GetFrameRateMean());
  EXPECT_GT(kExpectedValues1.frame_rate_mean_max,
            rate_controller_->temporal_layers(kLayer0Index).GetFrameRateMean());
  EXPECT_EQ(kExpectedValues1.is_buffer_full,
            rate_controller_->temporal_layers(kLayer0Index).is_buffer_full());
  EXPECT_EQ(kExpectedValues1.buffer_size,
            rate_controller_->temporal_layers(kLayer0Index).buffer_size());
  EXPECT_EQ(kExpectedValues1.last_frame_buffer_bytes,
            rate_controller_->temporal_layers(kLayer0Index)
                .last_frame_buffer_bytes());
  EXPECT_EQ(kExpectedValues1.last_frame_size_target,
            rate_controller_->temporal_layers(kLayer0Index)
                .last_frame_size_target_for_testing());
  rate_controller_->GetHRDBufferFullness(buffer_fullness_values,
                                         base::TimeDelta());
  EXPECT_EQ(kExpectedValues1.buffer_fullness,
            buffer_fullness_values[kLayer0Index]);

  int start_frame_index = 0;
  int last_intra_frame_qp;
  int last_inter_frame_qp;
  int last_frame_index = RunTestSequence(
      kCommonAvgBitrate, kCommonFps, kTestSequenceFrameCount,
      rate_controller_settings_.num_temporal_layers, 0, start_frame_index,
      last_intra_frame_qp, last_inter_frame_qp);
  base::TimeDelta timestamp = base::Microseconds(
      last_frame_index * base::Time::kMicrosecondsPerSecond / kCommonFps);

  EXPECT_EQ(kExpectedIntraFrameQP2, last_intra_frame_qp);
  EXPECT_EQ(kExpectedInterFrameQP2, last_inter_frame_qp);

  EXPECT_EQ(kExpectedValues2.buffer_bytes,
            rate_controller_->temporal_layers(kLayer0Index)
                .GetBufferBytesAtTime(timestamp));
  EXPECT_EQ(kExpectedValues2.buffer_bytes_remaining,
            rate_controller_->temporal_layers(kLayer0Index)
                .GetBufferBytesRemainingAtTime(timestamp));
  EXPECT_LT(kExpectedValues2.frame_rate_mean_min,
            rate_controller_->temporal_layers(kLayer0Index).GetFrameRateMean());
  EXPECT_GT(kExpectedValues2.frame_rate_mean_max,
            rate_controller_->temporal_layers(kLayer0Index).GetFrameRateMean());
  EXPECT_EQ(kExpectedValues2.is_buffer_full,
            rate_controller_->temporal_layers(kLayer0Index).is_buffer_full());
  EXPECT_EQ(kExpectedValues2.buffer_size,
            rate_controller_->temporal_layers(kLayer0Index).buffer_size());
  EXPECT_EQ(kExpectedValues2.last_frame_buffer_bytes,
            rate_controller_->temporal_layers(kLayer0Index)
                .last_frame_buffer_bytes());
  EXPECT_EQ(kExpectedValues2.last_frame_size_target,
            rate_controller_->temporal_layers(kLayer0Index)
                .last_frame_size_target_for_testing());
  rate_controller_->GetHRDBufferFullness(buffer_fullness_values, timestamp);
  EXPECT_EQ(kExpectedValues2.buffer_fullness,
            buffer_fullness_values[kLayer0Index]);
}

TEST_F(H264RateControllerTest, RunH264RateController2TemporalLayersTest) {
  constexpr size_t kTestSequenceFrameCount = 30;
  constexpr RateControllerTestValues kExpectedValuesLayer01 = {
      false, 26666, 0, 26666, 0, 29.9f, 30.1f, 0, 0};
  constexpr RateControllerTestValues kExpectedValuesLayer11 = {
      false, 40000, 0, 40000, 0, 29.9f, 30.1f, 0, 0};
  constexpr RateControllerTestValues kExpectedValuesLayer02 = {
      false, 26666, 1387, 25279, 4166, 15.0f, 15.1f, 6344, 5};
  constexpr RateControllerTestValues kExpectedValuesLayer12 = {
      false, 40000, 16633, 23367, 20801, 15.0f, 15.1f, 21928, 41};
  constexpr int kExpectedIntraFrameQP2 = 34;
  constexpr int kExpectedInterFrameQP2 = 29;
  constexpr RateControllerTestValues kExpectedValuesLayer03 = {
      false, 26666, 0, 26666, 2000, 15.0f, 15.1f, 7569, 0};
  constexpr RateControllerTestValues kExpectedValuesLayer13 = {
      false, 40000, 522, 39478, 4690, 15.0f, 15.1f, 34775, 1};
  constexpr int kExpectedIntraFrameQP3 = 34;
  constexpr int kExpectedInterFrameQP3 = 43;

  rate_controller_settings_.num_temporal_layers = 2;
  rate_controller_settings_.layer_settings[0].avg_bitrate =
      kCommonAvgBitrate * 2 / 3;
  rate_controller_settings_.layer_settings[0].hrd_buffer_size =
      kCommonHRDBufferSize * 2 / 3;
  rate_controller_settings_.layer_settings[0].frame_rate = kCommonFps / 2;
  rate_controller_settings_.layer_settings.emplace_back();
  rate_controller_settings_.layer_settings[1].avg_bitrate = kCommonAvgBitrate;
  rate_controller_settings_.layer_settings[1].hrd_buffer_size =
      kCommonHRDBufferSize;
  rate_controller_settings_.layer_settings[1].min_qp = kCommonQpMin;
  rate_controller_settings_.layer_settings[1].max_qp = kCommonQpMax;
  rate_controller_settings_.layer_settings[1].frame_rate = kCommonFps;

  rate_controller_ =
      std::make_unique<H264RateController>(rate_controller_settings_);

  std::array<int, 2> buffer_fullness_array = {0, 0};
  base::span<int> buffer_fullness_values(buffer_fullness_array);

  EXPECT_EQ(kExpectedValuesLayer01.buffer_bytes,
            rate_controller_->temporal_layers(kLayer0Index)
                .GetBufferBytesAtTime(base::TimeDelta()));
  EXPECT_EQ(kExpectedValuesLayer01.buffer_bytes_remaining,
            rate_controller_->temporal_layers(kLayer0Index)
                .GetBufferBytesRemainingAtTime(base::TimeDelta()));
  EXPECT_LT(kExpectedValuesLayer01.frame_rate_mean_min,
            rate_controller_->temporal_layers(kLayer0Index).GetFrameRateMean());
  EXPECT_GT(kExpectedValuesLayer01.frame_rate_mean_max,
            rate_controller_->temporal_layers(kLayer0Index).GetFrameRateMean());
  EXPECT_EQ(kExpectedValuesLayer01.is_buffer_full,
            rate_controller_->temporal_layers(kLayer0Index).is_buffer_full());
  EXPECT_EQ(kExpectedValuesLayer01.buffer_size,
            rate_controller_->temporal_layers(kLayer0Index).buffer_size());
  EXPECT_EQ(kExpectedValuesLayer01.last_frame_buffer_bytes,
            rate_controller_->temporal_layers(kLayer0Index)
                .last_frame_buffer_bytes());
  EXPECT_EQ(kExpectedValuesLayer01.last_frame_size_target,
            rate_controller_->temporal_layers(kLayer0Index)
                .last_frame_size_target_for_testing());
  rate_controller_->GetHRDBufferFullness(buffer_fullness_values,
                                         base::TimeDelta());
  EXPECT_EQ(kExpectedValuesLayer01.buffer_fullness,
            buffer_fullness_values[kLayer0Index]);
  EXPECT_EQ(kExpectedValuesLayer11.buffer_bytes,
            rate_controller_->temporal_layers(kLayer1Index)
                .GetBufferBytesAtTime(base::TimeDelta()));
  EXPECT_EQ(kExpectedValuesLayer11.buffer_bytes_remaining,
            rate_controller_->temporal_layers(kLayer1Index)
                .GetBufferBytesRemainingAtTime(base::TimeDelta()));
  EXPECT_LT(kExpectedValuesLayer11.frame_rate_mean_min,
            rate_controller_->temporal_layers(kLayer1Index).GetFrameRateMean());
  EXPECT_GT(kExpectedValuesLayer11.frame_rate_mean_max,
            rate_controller_->temporal_layers(kLayer1Index).GetFrameRateMean());
  EXPECT_EQ(kExpectedValuesLayer11.is_buffer_full,
            rate_controller_->temporal_layers(kLayer1Index).is_buffer_full());
  EXPECT_EQ(kExpectedValuesLayer11.buffer_size,
            rate_controller_->temporal_layers(kLayer1Index).buffer_size());
  EXPECT_EQ(kExpectedValuesLayer11.last_frame_buffer_bytes,
            rate_controller_->temporal_layers(kLayer1Index)
                .last_frame_buffer_bytes());
  EXPECT_EQ(kExpectedValuesLayer11.last_frame_size_target,
            rate_controller_->temporal_layers(kLayer1Index)
                .last_frame_size_target_for_testing());
  rate_controller_->GetHRDBufferFullness(buffer_fullness_values,
                                         base::TimeDelta());
  EXPECT_EQ(kExpectedValuesLayer11.buffer_fullness,
            buffer_fullness_values[kLayer1Index]);

  int start_frame_index = 0;
  int last_intra_frame_qp;
  int last_inter_frame_qp;
  int last_frame_index = RunTestSequence(
      kCommonAvgBitrate, kCommonFps, kTestSequenceFrameCount,
      rate_controller_settings_.num_temporal_layers, 0, start_frame_index,
      last_intra_frame_qp, last_inter_frame_qp);
  base::TimeDelta timestamp = base::Microseconds(
      last_frame_index * base::Time::kMicrosecondsPerSecond / kCommonFps);

  EXPECT_EQ(kExpectedIntraFrameQP2, last_intra_frame_qp);
  EXPECT_EQ(kExpectedInterFrameQP2, last_inter_frame_qp);

  EXPECT_EQ(kExpectedValuesLayer02.buffer_bytes,
            rate_controller_->temporal_layers(kLayer0Index)
                .GetBufferBytesAtTime(timestamp));
  EXPECT_EQ(kExpectedValuesLayer02.buffer_bytes_remaining,
            rate_controller_->temporal_layers(kLayer0Index)
                .GetBufferBytesRemainingAtTime(timestamp));
  EXPECT_LT(kExpectedValuesLayer02.frame_rate_mean_min,
            rate_controller_->temporal_layers(kLayer0Index).GetFrameRateMean());
  EXPECT_GT(kExpectedValuesLayer02.frame_rate_mean_max,
            rate_controller_->temporal_layers(kLayer0Index).GetFrameRateMean());
  EXPECT_EQ(kExpectedValuesLayer02.is_buffer_full,
            rate_controller_->temporal_layers(kLayer0Index).is_buffer_full());
  EXPECT_EQ(kExpectedValuesLayer02.buffer_size,
            rate_controller_->temporal_layers(kLayer0Index).buffer_size());
  EXPECT_EQ(kExpectedValuesLayer02.last_frame_buffer_bytes,
            rate_controller_->temporal_layers(kLayer0Index)
                .last_frame_buffer_bytes());
  EXPECT_EQ(kExpectedValuesLayer02.last_frame_size_target,
            rate_controller_->temporal_layers(kLayer0Index)
                .last_frame_size_target_for_testing());
  rate_controller_->GetHRDBufferFullness(buffer_fullness_values, timestamp);
  EXPECT_EQ(kExpectedValuesLayer02.buffer_fullness,
            buffer_fullness_values[kLayer0Index]);
  EXPECT_EQ(kExpectedValuesLayer12.buffer_bytes,
            rate_controller_->temporal_layers(kLayer1Index)
                .GetBufferBytesAtTime(timestamp));
  EXPECT_EQ(kExpectedValuesLayer12.buffer_bytes_remaining,
            rate_controller_->temporal_layers(kLayer1Index)
                .GetBufferBytesRemainingAtTime(timestamp));
  EXPECT_LT(kExpectedValuesLayer12.frame_rate_mean_min,
            rate_controller_->temporal_layers(kLayer1Index).GetFrameRateMean());
  EXPECT_GT(kExpectedValuesLayer12.frame_rate_mean_max,
            rate_controller_->temporal_layers(kLayer1Index).GetFrameRateMean());
  EXPECT_EQ(kExpectedValuesLayer12.is_buffer_full,
            rate_controller_->temporal_layers(kLayer1Index).is_buffer_full());
  EXPECT_EQ(kExpectedValuesLayer12.buffer_size,
            rate_controller_->temporal_layers(kLayer1Index).buffer_size());
  EXPECT_EQ(kExpectedValuesLayer12.last_frame_buffer_bytes,
            rate_controller_->temporal_layers(kLayer1Index)
                .last_frame_buffer_bytes());
  EXPECT_EQ(kExpectedValuesLayer12.last_frame_size_target,
            rate_controller_->temporal_layers(kLayer1Index)
                .last_frame_size_target_for_testing());
  rate_controller_->GetHRDBufferFullness(buffer_fullness_values, timestamp);
  EXPECT_EQ(kExpectedValuesLayer12.buffer_fullness,
            buffer_fullness_values[kLayer1Index]);

  // Run test sequence 1
  // The following use cases are tested:
  //   1. Start Limit Base QP procedure in ClipInterFrameQP(). The buffer size
  //      gradually decreases and the Limit Base QP is turned off when the
  //      buffer fullness of the enhanched layer reaches 35%.
  //   2. Adjusting the min_qp value in ClipInterFrameQP() method when the
  //      buffer reaches full capacity.
  //   3. Setting the overshooting timestamp and increasing min_qp and max_qp in
  //      ClipInterFrameQP().
  //   4. The QP is set to maximum value when the buffer is full in
  //      EstimateInterFrameQP() method.
  start_frame_index = last_frame_index;
  last_frame_index = RunTestSequence(
      kCommonAvgBitrate, kCommonFps, kTestSequenceFrameCount,
      rate_controller_settings_.num_temporal_layers, 1, start_frame_index,
      last_intra_frame_qp, last_inter_frame_qp);
  timestamp = base::Microseconds(
      last_frame_index * base::Time::kMicrosecondsPerSecond / kCommonFps);

  EXPECT_EQ(kExpectedIntraFrameQP3, last_intra_frame_qp);
  EXPECT_EQ(kExpectedInterFrameQP3, last_inter_frame_qp);

  EXPECT_EQ(kExpectedValuesLayer03.buffer_bytes,
            rate_controller_->temporal_layers(kLayer0Index)
                .GetBufferBytesAtTime(timestamp));
  EXPECT_EQ(kExpectedValuesLayer03.buffer_bytes_remaining,
            rate_controller_->temporal_layers(kLayer0Index)
                .GetBufferBytesRemainingAtTime(timestamp));
  EXPECT_LT(kExpectedValuesLayer03.frame_rate_mean_min,
            rate_controller_->temporal_layers(kLayer0Index).GetFrameRateMean());
  EXPECT_GT(kExpectedValuesLayer03.frame_rate_mean_max,
            rate_controller_->temporal_layers(kLayer0Index).GetFrameRateMean());
  EXPECT_EQ(kExpectedValuesLayer03.is_buffer_full,
            rate_controller_->temporal_layers(kLayer0Index).is_buffer_full());
  EXPECT_EQ(kExpectedValuesLayer03.buffer_size,
            rate_controller_->temporal_layers(kLayer0Index).buffer_size());
  EXPECT_EQ(kExpectedValuesLayer03.last_frame_buffer_bytes,
            rate_controller_->temporal_layers(kLayer0Index)
                .last_frame_buffer_bytes());
  EXPECT_EQ(kExpectedValuesLayer03.last_frame_size_target,
            rate_controller_->temporal_layers(kLayer0Index)
                .last_frame_size_target_for_testing());
  rate_controller_->GetHRDBufferFullness(buffer_fullness_values, timestamp);
  EXPECT_EQ(kExpectedValuesLayer03.buffer_fullness,
            buffer_fullness_values[kLayer0Index]);
  EXPECT_EQ(kExpectedValuesLayer13.buffer_bytes,
            rate_controller_->temporal_layers(kLayer1Index)
                .GetBufferBytesAtTime(timestamp));
  EXPECT_EQ(kExpectedValuesLayer13.buffer_bytes_remaining,
            rate_controller_->temporal_layers(kLayer1Index)
                .GetBufferBytesRemainingAtTime(timestamp));
  EXPECT_LT(kExpectedValuesLayer13.frame_rate_mean_min,
            rate_controller_->temporal_layers(kLayer1Index).GetFrameRateMean());
  EXPECT_GT(kExpectedValuesLayer13.frame_rate_mean_max,
            rate_controller_->temporal_layers(kLayer1Index).GetFrameRateMean());
  EXPECT_EQ(kExpectedValuesLayer13.is_buffer_full,
            rate_controller_->temporal_layers(kLayer1Index).is_buffer_full());
  EXPECT_EQ(kExpectedValuesLayer13.buffer_size,
            rate_controller_->temporal_layers(kLayer1Index).buffer_size());
  EXPECT_EQ(kExpectedValuesLayer13.last_frame_buffer_bytes,
            rate_controller_->temporal_layers(kLayer1Index)
                .last_frame_buffer_bytes());
  EXPECT_EQ(kExpectedValuesLayer13.last_frame_size_target,
            rate_controller_->temporal_layers(kLayer1Index)
                .last_frame_size_target_for_testing());
  rate_controller_->GetHRDBufferFullness(buffer_fullness_values, timestamp);
  EXPECT_EQ(kExpectedValuesLayer13.buffer_fullness,
            buffer_fullness_values[kLayer1Index]);
}

TEST_F(H264RateControllerTest,
       RunH264RateController2TemporalLayersFixedDeltaQPTest) {
  constexpr size_t kTestSequenceFrameCount = 30;
  constexpr uint32_t kQpMin = 25;
  constexpr RateControllerTestValues kExpectedValuesLayer01 = {
      false, 26666, 0, 26666, 0, 29.9f, 30.1f, 0, 0};
  constexpr RateControllerTestValues kExpectedValuesLayer11 = {
      false, 40000, 0, 40000, 0, 29.9f, 30.1f, 0, 0};
  constexpr int kExpectedIntraFrameQP2 = 28;
  constexpr int kExpectedInterFrameQP2 = 30;
  constexpr RateControllerTestValues kExpectedValuesLayer02 = {
      false, 26666, 1387, 25279, 4166, 15.0f, 15.1f, 3790, 5};
  constexpr RateControllerTestValues kExpectedValuesLayer12 = {
      false, 40000, 16633, 23367, 20801, 15.0f, 15.1f, 0, 41};
  constexpr int kExpectedIntraFrameQP3 = 39;
  constexpr int kExpectedInterFrameQP3 = 25;
  constexpr RateControllerTestValues kExpectedValuesLayer03 = {
      false, 26666, 0, 26666, 0, 15.0f, 15.1f, 8007, 0};
  constexpr RateControllerTestValues kExpectedValuesLayer13 = {
      false, 40000, 0, 40000, 0, 15.0f, 15.1f, 1333, 0};

  rate_controller_settings_.content_type =
      VideoEncodeAccelerator::Config::ContentType::kDisplay;
  rate_controller_settings_.fixed_delta_qp = true;
  rate_controller_settings_.num_temporal_layers = 2;
  rate_controller_settings_.layer_settings[0].avg_bitrate =
      kCommonAvgBitrate * 2 / 3;
  rate_controller_settings_.layer_settings[0].hrd_buffer_size =
      kCommonHRDBufferSize * 2 / 3;
  rate_controller_settings_.layer_settings[0].min_qp = kQpMin;
  rate_controller_settings_.layer_settings[0].frame_rate = kCommonFps / 2;
  rate_controller_settings_.layer_settings.emplace_back();
  rate_controller_settings_.layer_settings[1].avg_bitrate = kCommonAvgBitrate;
  rate_controller_settings_.layer_settings[1].hrd_buffer_size =
      kCommonHRDBufferSize;
  rate_controller_settings_.layer_settings[1].min_qp = kQpMin;
  rate_controller_settings_.layer_settings[1].max_qp = kCommonQpMax;
  rate_controller_settings_.layer_settings[1].frame_rate = kCommonFps;

  rate_controller_ =
      std::make_unique<H264RateController>(rate_controller_settings_);

  std::array<int, 2> buffer_fullness_array = {0, 0};
  base::span<int> buffer_fullness_values(buffer_fullness_array);

  EXPECT_EQ(kExpectedValuesLayer01.buffer_bytes,
            rate_controller_->temporal_layers(kLayer0Index)
                .GetBufferBytesAtTime(base::TimeDelta()));
  EXPECT_EQ(kExpectedValuesLayer01.buffer_bytes_remaining,
            rate_controller_->temporal_layers(kLayer0Index)
                .GetBufferBytesRemainingAtTime(base::TimeDelta()));
  EXPECT_LT(kExpectedValuesLayer01.frame_rate_mean_min,
            rate_controller_->temporal_layers(kLayer0Index).GetFrameRateMean());
  EXPECT_GT(kExpectedValuesLayer01.frame_rate_mean_max,
            rate_controller_->temporal_layers(kLayer0Index).GetFrameRateMean());
  EXPECT_EQ(kExpectedValuesLayer01.is_buffer_full,
            rate_controller_->temporal_layers(kLayer0Index).is_buffer_full());
  EXPECT_EQ(kExpectedValuesLayer01.buffer_size,
            rate_controller_->temporal_layers(kLayer0Index).buffer_size());
  EXPECT_EQ(kExpectedValuesLayer01.last_frame_buffer_bytes,
            rate_controller_->temporal_layers(kLayer0Index)
                .last_frame_buffer_bytes());
  EXPECT_EQ(kExpectedValuesLayer01.last_frame_size_target,
            rate_controller_->temporal_layers(kLayer0Index)
                .last_frame_size_target_for_testing());
  rate_controller_->GetHRDBufferFullness(buffer_fullness_values,
                                         base::TimeDelta());
  EXPECT_EQ(kExpectedValuesLayer01.buffer_fullness,
            buffer_fullness_values[kLayer0Index]);
  EXPECT_EQ(kExpectedValuesLayer11.buffer_bytes,
            rate_controller_->temporal_layers(kLayer1Index)
                .GetBufferBytesAtTime(base::TimeDelta()));
  EXPECT_EQ(kExpectedValuesLayer11.buffer_bytes_remaining,
            rate_controller_->temporal_layers(kLayer1Index)
                .GetBufferBytesRemainingAtTime(base::TimeDelta()));
  EXPECT_LT(kExpectedValuesLayer11.frame_rate_mean_min,
            rate_controller_->temporal_layers(kLayer1Index).GetFrameRateMean());
  EXPECT_GT(kExpectedValuesLayer11.frame_rate_mean_max,
            rate_controller_->temporal_layers(kLayer1Index).GetFrameRateMean());
  EXPECT_EQ(kExpectedValuesLayer11.is_buffer_full,
            rate_controller_->temporal_layers(kLayer1Index).is_buffer_full());
  EXPECT_EQ(kExpectedValuesLayer11.buffer_size,
            rate_controller_->temporal_layers(kLayer1Index).buffer_size());
  EXPECT_EQ(kExpectedValuesLayer11.last_frame_buffer_bytes,
            rate_controller_->temporal_layers(kLayer1Index)
                .last_frame_buffer_bytes());
  EXPECT_EQ(kExpectedValuesLayer11.last_frame_size_target,
            rate_controller_->temporal_layers(kLayer1Index)
                .last_frame_size_target_for_testing());
  rate_controller_->GetHRDBufferFullness(buffer_fullness_values,
                                         base::TimeDelta());
  EXPECT_EQ(kExpectedValuesLayer11.buffer_fullness,
            buffer_fullness_values[kLayer1Index]);

  int start_frame_index = 0;
  int last_intra_frame_qp;
  int last_inter_frame_qp;
  int last_frame_index = RunTestSequence(
      kCommonAvgBitrate, kCommonFps, kTestSequenceFrameCount,
      rate_controller_settings_.num_temporal_layers, 0, start_frame_index,
      last_intra_frame_qp, last_inter_frame_qp);
  base::TimeDelta timestamp = base::Microseconds(
      last_frame_index * base::Time::kMicrosecondsPerSecond / kCommonFps);

  EXPECT_EQ(kExpectedIntraFrameQP2, last_intra_frame_qp);
  EXPECT_EQ(kExpectedInterFrameQP2, last_inter_frame_qp);

  EXPECT_EQ(kExpectedValuesLayer02.buffer_bytes,
            rate_controller_->temporal_layers(kLayer0Index)
                .GetBufferBytesAtTime(timestamp));
  EXPECT_EQ(kExpectedValuesLayer02.buffer_bytes_remaining,
            rate_controller_->temporal_layers(kLayer0Index)
                .GetBufferBytesRemainingAtTime(timestamp));
  EXPECT_LT(kExpectedValuesLayer02.frame_rate_mean_min,
            rate_controller_->temporal_layers(kLayer0Index).GetFrameRateMean());
  EXPECT_GT(kExpectedValuesLayer02.frame_rate_mean_max,
            rate_controller_->temporal_layers(kLayer0Index).GetFrameRateMean());
  EXPECT_EQ(kExpectedValuesLayer02.is_buffer_full,
            rate_controller_->temporal_layers(kLayer0Index).is_buffer_full());
  EXPECT_EQ(kExpectedValuesLayer02.buffer_size,
            rate_controller_->temporal_layers(kLayer0Index).buffer_size());
  EXPECT_EQ(kExpectedValuesLayer02.last_frame_buffer_bytes,
            rate_controller_->temporal_layers(kLayer0Index)
                .last_frame_buffer_bytes());
  EXPECT_EQ(kExpectedValuesLayer02.last_frame_size_target,
            rate_controller_->temporal_layers(kLayer0Index)
                .last_frame_size_target_for_testing());
  rate_controller_->GetHRDBufferFullness(buffer_fullness_values, timestamp);
  EXPECT_EQ(kExpectedValuesLayer02.buffer_fullness,
            buffer_fullness_values[kLayer0Index]);
  EXPECT_EQ(kExpectedValuesLayer12.buffer_bytes,
            rate_controller_->temporal_layers(kLayer1Index)
                .GetBufferBytesAtTime(timestamp));
  EXPECT_EQ(kExpectedValuesLayer12.buffer_bytes_remaining,
            rate_controller_->temporal_layers(kLayer1Index)
                .GetBufferBytesRemainingAtTime(timestamp));
  EXPECT_LT(kExpectedValuesLayer12.frame_rate_mean_min,
            rate_controller_->temporal_layers(kLayer1Index).GetFrameRateMean());
  EXPECT_GT(kExpectedValuesLayer12.frame_rate_mean_max,
            rate_controller_->temporal_layers(kLayer1Index).GetFrameRateMean());
  EXPECT_EQ(kExpectedValuesLayer12.is_buffer_full,
            rate_controller_->temporal_layers(kLayer1Index).is_buffer_full());
  EXPECT_EQ(kExpectedValuesLayer12.buffer_size,
            rate_controller_->temporal_layers(kLayer1Index).buffer_size());
  EXPECT_EQ(kExpectedValuesLayer12.last_frame_buffer_bytes,
            rate_controller_->temporal_layers(kLayer1Index)
                .last_frame_buffer_bytes());
  EXPECT_EQ(kExpectedValuesLayer12.last_frame_size_target,
            rate_controller_->temporal_layers(kLayer1Index)
                .last_frame_size_target_for_testing());
  rate_controller_->GetHRDBufferFullness(buffer_fullness_values, timestamp);
  EXPECT_EQ(kExpectedValuesLayer12.buffer_fullness,
            buffer_fullness_values[kLayer1Index]);

  // Run test sequences 2 and 3
  // The following use cases are tested:
  //   1. The QP difference between enchanched and base layers is more than 4.
  //      This indicates the HRD overflow and the min_qp is increased. The
  //      ClipInterFrameQP() method handles this scenario.
  //   2. The test sequence triggers the setting of undershoot_delta_qp. Under
  //      this condition, the QP for the enhanced layer is reduced by the
  //      undershoot value within the EstimateInterFrameQP() method.
  start_frame_index = last_frame_index;
  last_frame_index = RunTestSequence(
      kCommonAvgBitrate, kCommonFps, kTestSequenceFrameCount,
      rate_controller_settings_.num_temporal_layers, 2, start_frame_index,
      last_intra_frame_qp, last_inter_frame_qp);
  timestamp = base::Microseconds(
      last_frame_index * base::Time::kMicrosecondsPerSecond / kCommonFps);

  start_frame_index = last_frame_index;
  last_frame_index = RunTestSequence(
      kCommonAvgBitrate, kCommonFps, kTestSequenceFrameCount,
      rate_controller_settings_.num_temporal_layers, 3, start_frame_index,
      last_intra_frame_qp, last_inter_frame_qp);
  timestamp = base::Microseconds(
      last_frame_index * base::Time::kMicrosecondsPerSecond / kCommonFps);

  EXPECT_EQ(kExpectedIntraFrameQP3, last_intra_frame_qp);
  EXPECT_EQ(kExpectedInterFrameQP3, last_inter_frame_qp);

  EXPECT_EQ(kExpectedValuesLayer03.buffer_bytes,
            rate_controller_->temporal_layers(kLayer0Index)
                .GetBufferBytesAtTime(timestamp));
  EXPECT_EQ(kExpectedValuesLayer03.buffer_bytes_remaining,
            rate_controller_->temporal_layers(kLayer0Index)
                .GetBufferBytesRemainingAtTime(timestamp));
  EXPECT_LT(kExpectedValuesLayer03.frame_rate_mean_min,
            rate_controller_->temporal_layers(kLayer0Index).GetFrameRateMean());
  EXPECT_GT(kExpectedValuesLayer03.frame_rate_mean_max,
            rate_controller_->temporal_layers(kLayer0Index).GetFrameRateMean());
  EXPECT_EQ(kExpectedValuesLayer03.is_buffer_full,
            rate_controller_->temporal_layers(kLayer0Index).is_buffer_full());
  EXPECT_EQ(kExpectedValuesLayer03.buffer_size,
            rate_controller_->temporal_layers(kLayer0Index).buffer_size());
  EXPECT_EQ(kExpectedValuesLayer03.last_frame_buffer_bytes,
            rate_controller_->temporal_layers(kLayer0Index)
                .last_frame_buffer_bytes());
  EXPECT_EQ(kExpectedValuesLayer03.last_frame_size_target,
            rate_controller_->temporal_layers(kLayer0Index)
                .last_frame_size_target_for_testing());
  rate_controller_->GetHRDBufferFullness(buffer_fullness_values, timestamp);
  EXPECT_EQ(kExpectedValuesLayer03.buffer_fullness,
            buffer_fullness_values[kLayer0Index]);
  EXPECT_EQ(kExpectedValuesLayer13.buffer_bytes,
            rate_controller_->temporal_layers(kLayer1Index)
                .GetBufferBytesAtTime(timestamp));
  EXPECT_EQ(kExpectedValuesLayer13.buffer_bytes_remaining,
            rate_controller_->temporal_layers(kLayer1Index)
                .GetBufferBytesRemainingAtTime(timestamp));
  EXPECT_LT(kExpectedValuesLayer13.frame_rate_mean_min,
            rate_controller_->temporal_layers(kLayer1Index).GetFrameRateMean());
  EXPECT_GT(kExpectedValuesLayer13.frame_rate_mean_max,
            rate_controller_->temporal_layers(kLayer1Index).GetFrameRateMean());
  EXPECT_EQ(kExpectedValuesLayer13.is_buffer_full,
            rate_controller_->temporal_layers(kLayer1Index).is_buffer_full());
  EXPECT_EQ(kExpectedValuesLayer13.buffer_size,
            rate_controller_->temporal_layers(kLayer1Index).buffer_size());
  EXPECT_EQ(kExpectedValuesLayer13.last_frame_buffer_bytes,
            rate_controller_->temporal_layers(kLayer1Index)
                .last_frame_buffer_bytes());
  EXPECT_EQ(kExpectedValuesLayer13.last_frame_size_target,
            rate_controller_->temporal_layers(kLayer1Index)
                .last_frame_size_target_for_testing());
  rate_controller_->GetHRDBufferFullness(buffer_fullness_values, timestamp);
  EXPECT_EQ(kExpectedValuesLayer13.buffer_fullness,
            buffer_fullness_values[kLayer1Index]);
}

TEST_F(H264RateControllerTest, RunH264RateControllerFramerateMeanTest) {
  constexpr float kFrameRateExpectedValues[] = {29.9f, 30.1f};

  rate_controller_ =
      std::make_unique<H264RateController>(rate_controller_settings_);

  size_t frame_size = kCommonAvgBitrate / 8 / kCommonFps;
  base::TimeDelta timestamp;

  rate_controller_->temporal_layers(kLayer0Index).AddFrameTimestamp(timestamp);
  rate_controller_->temporal_layers(kLayer0Index)
      .AddFrameBytes(frame_size, timestamp);

  timestamp +=
      base::Microseconds(base::Time::kMicrosecondsPerSecond / kCommonFps);

  rate_controller_->temporal_layers(kLayer0Index).AddFrameTimestamp(timestamp);
  rate_controller_->temporal_layers(kLayer0Index)
      .AddFrameBytes(frame_size, timestamp);

  EXPECT_LT(kFrameRateExpectedValues[0],
            rate_controller_->temporal_layers(kLayer0Index).GetFrameRateMean());
  EXPECT_GT(kFrameRateExpectedValues[1],
            rate_controller_->temporal_layers(kLayer0Index).GetFrameRateMean());

  timestamp += base::Microseconds(100);

  rate_controller_->temporal_layers(kLayer0Index).AddFrameTimestamp(timestamp);
  rate_controller_->temporal_layers(kLayer0Index)
      .AddFrameBytes(frame_size, timestamp);

  timestamp += base::Microseconds(100);

  rate_controller_->temporal_layers(kLayer0Index).AddFrameTimestamp(timestamp);
  rate_controller_->temporal_layers(kLayer0Index)
      .AddFrameBytes(frame_size, timestamp);

  EXPECT_LT(kFrameRateExpectedValues[0],
            rate_controller_->temporal_layers(kLayer0Index).GetFrameRateMean());
  EXPECT_GT(kFrameRateExpectedValues[1],
            rate_controller_->temporal_layers(kLayer0Index).GetFrameRateMean());
}

TEST_F(H264RateControllerTest, RunH264RateControllerSetBufferParametersTest) {
  constexpr RateControllerTestValues kExpectedValues = {
      false, 80000, 4166, 75834, 4166, 0.0f, 0.0f, 0, 0};

  rate_controller_ =
      std::make_unique<H264RateController>(rate_controller_settings_);

  size_t frame_size = kCommonAvgBitrate / 8 / kCommonFps;
  base::TimeDelta timestamp;

  rate_controller_->temporal_layers(kLayer0Index).AddFrameTimestamp(timestamp);
  rate_controller_->temporal_layers(kLayer0Index)
      .AddFrameBytes(frame_size, timestamp);

  timestamp +=
      base::Microseconds(base::Time::kMicrosecondsPerSecond / kCommonFps);

  rate_controller_->temporal_layers(kLayer0Index)
      .SetBufferParameters(kCommonHRDBufferSize * 2, kCommonAvgBitrate * 2,
                           kCommonPeakBitrate * 2, false);

  rate_controller_->temporal_layers(0).AddFrameTimestamp(timestamp);
  rate_controller_->temporal_layers(0).AddFrameBytes(frame_size, timestamp);

  EXPECT_EQ(kExpectedValues.buffer_bytes,
            rate_controller_->temporal_layers(kLayer0Index)
                .GetBufferBytesAtTime(timestamp));
  EXPECT_EQ(kExpectedValues.buffer_bytes_remaining,
            rate_controller_->temporal_layers(kLayer0Index)
                .GetBufferBytesRemainingAtTime(timestamp));
  EXPECT_EQ(kExpectedValues.is_buffer_full,
            rate_controller_->temporal_layers(kLayer0Index).is_buffer_full());
  EXPECT_EQ(kExpectedValues.buffer_size,
            rate_controller_->temporal_layers(kLayer0Index).buffer_size());
  EXPECT_EQ(kExpectedValues.last_frame_buffer_bytes,
            rate_controller_->temporal_layers(kLayer0Index)
                .last_frame_buffer_bytes());
}

// The test verifies the following conditions:
// 1. The first intra encoded frame fills up the buffer more than 50%. This is
//    handled in FinishIntraFrame() method.
// 2. QP adjustment in EstimateIntraFrameQP() method when the previous encoded
//    frame is an intra frame.
// 3. Modifying QP value in EstimateIntraFrameQP() method when the buffer is
//    full.
TEST_F(H264RateControllerTest, RunH264RateControllerIntraEncodedFrameTest) {
  constexpr RateControllerTestValues kExpectedValues = {
      true, 40000, 53499, 0, 53499, 0.0f, 0.0f, 0, 0};
  constexpr float kExpectedTargetFpsMin = 4.9f;
  constexpr float kExpectedTargetFpsMax = 5.1f;

  rate_controller_ =
      std::make_unique<H264RateController>(rate_controller_settings_);

  size_t kFrameSize1 = 21000;
  size_t kFrameSize2 = 25000;
  size_t kFrameSize3 = 10000;
  size_t kFrameSize4 = 10000;
  base::TimeDelta timestamp;

  rate_controller_->UpdateFrameSize(gfx::Size(1920, 1080));

  rate_controller_->EstimateIntraFrameQP(timestamp);

  rate_controller_->FinishIntraFrame(kFrameSize1, timestamp);

  timestamp +=
      base::Microseconds(base::Time::kMicrosecondsPerSecond / kCommonFps);

  rate_controller_->UpdateFrameSize(gfx::Size(1280, 720));

  rate_controller_->EstimateIntraFrameQP(timestamp);

  rate_controller_->FinishIntraFrame(kFrameSize2, timestamp);

  timestamp +=
      base::Microseconds(base::Time::kMicrosecondsPerSecond / kCommonFps);

  rate_controller_->UpdateFrameSize(gfx::Size(1920, 1080));

  rate_controller_->EstimateIntraFrameQP(timestamp);

  rate_controller_->FinishIntraFrame(kFrameSize3, timestamp);

  timestamp +=
      base::Microseconds(base::Time::kMicrosecondsPerSecond / kCommonFps);

  rate_controller_->EstimateIntraFrameQP(timestamp);

  rate_controller_->FinishIntraFrame(kFrameSize4, timestamp);

  EXPECT_EQ(kExpectedValues.buffer_bytes,
            rate_controller_->temporal_layers(kLayer0Index)
                .GetBufferBytesAtTime(timestamp));
  EXPECT_EQ(kExpectedValues.buffer_bytes_remaining,
            rate_controller_->temporal_layers(kLayer0Index)
                .GetBufferBytesRemainingAtTime(timestamp));
  EXPECT_EQ(kExpectedValues.is_buffer_full,
            rate_controller_->temporal_layers(kLayer0Index).is_buffer_full());
  EXPECT_EQ(kExpectedValues.buffer_size,
            rate_controller_->temporal_layers(kLayer0Index).buffer_size());
  EXPECT_EQ(kExpectedValues.last_frame_buffer_bytes,
            rate_controller_->temporal_layers(kLayer0Index)
                .last_frame_buffer_bytes());
  EXPECT_LT(kExpectedTargetFpsMin, rate_controller_->target_fps_for_testing());
  EXPECT_GT(kExpectedTargetFpsMax, rate_controller_->target_fps_for_testing());
}
}  // namespace

}  // namespace media
