// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/h264_rate_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {
constexpr uint32_t kCommonAvgBitrate = 1000000;   // bits per second
constexpr uint32_t kCommonPeakBitrate = 2000000;  // bits per second
constexpr int kCommonFps = 30;
constexpr uint32_t kCommonFrameHeight = 600;
constexpr uint32_t kCommonFrameWidth = 800;
constexpr size_t kCommonHRDBufferSize = 40000;  // bytes
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
  };

  H264RateControllerTest() = default;

  void SetUp() override {
    settings_.content_type =
        VideoEncodeAccelerator::Config::ContentType::kCamera;
    settings_.frame_size.SetSize(kCommonFrameWidth, kCommonFrameHeight);
    settings_.fixed_delta_qp = false;
    settings_.num_temporal_layers = 1;
    settings_.layers.emplace_back();
    settings_.layers[0].avg_bitrate = kCommonAvgBitrate;
    settings_.layers[0].peak_bitrate = kCommonPeakBitrate;
    settings_.layers[0].hrd_buffer_size = kCommonHRDBufferSize;
    settings_.layers[0].min_qp = kCommonQpMin;
    settings_.layers[0].max_qp = kCommonQpMax;
    settings_.layers[0].frame_rate = kCommonFps;

    // Copy operation test
    H264RateController::ControllerSettings settings_copy = settings_;
    settings_ = settings_copy;
  }

 protected:
  // Runs a loop of predefined encoded frames. The frame sequence contains two
  // intra frames at the beginning and in the middle of the sequence. In each
  // cycle the following methods are executed on the rate controller:
  // 1. ShrinkHRDBuffers()
  // 2. AddFrameTimestampToLayer()
  // 3. AddFrameBytesToLayer()
  int RunTestSequence(uint32_t avg_bitrate,
                      int fps,
                      int frame_count,
                      size_t num_temporal_layers,
                      int start_frame_index) {
    constexpr size_t kFirstIntraFrameIndex = 0;
    const size_t kSecondIntraFrameIndex = frame_count / 2;
    size_t frame_size = avg_bitrate / 8 / fps;
    std::vector<size_t> frames;
    for (int i = 0; i < frame_count; ++i) {
      frames.push_back(frame_size);
    }

    frames[kFirstIntraFrameIndex] = frames[kFirstIntraFrameIndex] * 3;
    frames[kSecondIntraFrameIndex] = frames[kSecondIntraFrameIndex] * 3;

    base::TimeDelta timestamp = base::Microseconds(
        start_frame_index * base::Time::kMicrosecondsPerSecond / fps);
    size_t layer_index = 0;
    size_t frame_index = 0;
    for (size_t encoded_size : frames) {
      if (num_temporal_layers > 1) {
        if (frame_index == kFirstIntraFrameIndex ||
            frame_index == kSecondIntraFrameIndex || layer_index == 1) {
          layer_index = 0;
        } else {
          layer_index = 1;
        }
      }

      for (size_t i = 0; i < num_temporal_layers; ++i) {
        rate_controller_->temporal_layers(i).ShrinkHRDBuffer(timestamp);
      }

      rate_controller_->temporal_layers(layer_index)
          .AddFrameTimestamp(timestamp);

      rate_controller_->temporal_layers(layer_index)
          .AddFrameBytes(encoded_size, timestamp);
      if (layer_index == 0 && num_temporal_layers > 1) {
        rate_controller_->temporal_layers(kLayer1Index)
            .AddFrameBytes(encoded_size, timestamp);
      }

      ++frame_index;
      timestamp += base::Microseconds(base::Time::kMicrosecondsPerSecond / fps);
    }

    return start_frame_index + frame_count;
  }

  std::unique_ptr<H264RateController> rate_controller_;
  H264RateController::ControllerSettings settings_;
};

// Test Cases

// The test runs a predefined sequence of frame sizes two times and checks
// the stats after running each sequence. Different window size is used in
// each run.
TEST_F(H264RateControllerTest, RunH264RateController1TemporalLayerTest) {
  constexpr size_t kTestSequenceFrameCount = 30;
  constexpr RateControllerTestValues kExpectedValues1 = {
      false, 40000, 0, 40000, 0, 29.9f, 30.1f};
  constexpr RateControllerTestValues kExpectedValues2 = {
      false, 40000, 16633, 23367, 20801, 29.9f, 30.1f};
  constexpr uint32_t kExpectedIntraFrameQP2 = 34;

  rate_controller_ = std::make_unique<H264RateController>(settings_);

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

  int start_frame_index = 0;
  int last_frame_index =
      RunTestSequence(kCommonAvgBitrate, kCommonFps, kTestSequenceFrameCount,
                      settings_.num_temporal_layers, start_frame_index);
  base::TimeDelta timestamp = base::Microseconds(
      last_frame_index * base::Time::kMicrosecondsPerSecond / kCommonFps);

  EXPECT_EQ(kExpectedIntraFrameQP2,
            rate_controller_->EstimateIntraFrameQP(timestamp));

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
}

TEST_F(H264RateControllerTest, RunH264RateController2TemporalLayerTest) {
  constexpr size_t kTestSequenceFrameCount = 30;
  constexpr RateControllerTestValues kExpectedValuesLayer01 = {
      false, 26666, 0, 26666, 0, 29.9f, 30.1f};
  constexpr RateControllerTestValues kExpectedValuesLayer11 = {
      false, 40000, 0, 40000, 0, 29.9f, 30.1f};
  constexpr RateControllerTestValues kExpectedValuesLayer02 = {
      false, 26666, 1387, 25279, 4166, 15.0f, 15.1f};
  constexpr uint32_t kExpectedIntraFrameQP2 = 31;
  constexpr RateControllerTestValues kExpectedValuesLayer12 = {
      false, 40000, 16633, 23367, 20801, 15.0f, 15.1f};

  settings_.num_temporal_layers = 2;
  settings_.layers[0].avg_bitrate = kCommonAvgBitrate * 2 / 3;
  settings_.layers[0].peak_bitrate = kCommonPeakBitrate * 2 / 3;
  settings_.layers[0].hrd_buffer_size = kCommonHRDBufferSize * 2 / 3;
  settings_.layers[0].frame_rate = kCommonFps / 2;
  settings_.layers.emplace_back();
  settings_.layers[1].avg_bitrate = kCommonAvgBitrate;
  settings_.layers[1].peak_bitrate = kCommonPeakBitrate;
  settings_.layers[1].hrd_buffer_size = kCommonHRDBufferSize;
  settings_.layers[1].min_qp = kCommonQpMin;
  settings_.layers[1].max_qp = kCommonQpMax;
  settings_.layers[1].frame_rate = kCommonFps;

  rate_controller_ = std::make_unique<H264RateController>(settings_);

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

  int start_frame_index = 0;
  int last_frame_index =
      RunTestSequence(kCommonAvgBitrate, kCommonFps, kTestSequenceFrameCount,
                      settings_.num_temporal_layers, start_frame_index);
  base::TimeDelta timestamp = base::Microseconds(
      last_frame_index * base::Time::kMicrosecondsPerSecond / kCommonFps);

  EXPECT_EQ(kExpectedIntraFrameQP2,
            rate_controller_->EstimateIntraFrameQP(timestamp));

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
}

TEST_F(H264RateControllerTest,
       RunH264RateController2TemporalLayerFixedLayerQPTest) {
  constexpr size_t kTestSequenceFrameCount = 30;
  constexpr RateControllerTestValues kExpectedValuesLayer01 = {
      false, 26666, 0, 26666, 0, 29.9f, 30.1f};
  constexpr RateControllerTestValues kExpectedValuesLayer11 = {
      false, 40000, 0, 40000, 0, 29.9f, 30.1f};
  constexpr uint32_t kExpectedIntraFrameQP2 = 28;
  constexpr RateControllerTestValues kExpectedValuesLayer02 = {
      false, 26666, 1387, 25279, 4166, 15.0f, 15.1f};
  constexpr RateControllerTestValues kExpectedValuesLayer12 = {
      false, 40000, 16633, 23367, 20801, 15.0f, 15.1f};

  settings_.fixed_delta_qp = true;
  settings_.num_temporal_layers = 2;
  settings_.layers[0].avg_bitrate = kCommonAvgBitrate * 2 / 3;
  settings_.layers[0].peak_bitrate = kCommonPeakBitrate * 2 / 3;
  settings_.layers[0].hrd_buffer_size = kCommonHRDBufferSize * 2 / 3;
  settings_.layers[0].frame_rate = kCommonFps / 2;
  settings_.layers.emplace_back();
  settings_.layers[1].avg_bitrate = kCommonAvgBitrate;
  settings_.layers[1].peak_bitrate = kCommonPeakBitrate;
  settings_.layers[1].hrd_buffer_size = kCommonHRDBufferSize;
  settings_.layers[1].min_qp = kCommonQpMin;
  settings_.layers[1].max_qp = kCommonQpMax;
  settings_.layers[1].frame_rate = kCommonFps;

  rate_controller_ = std::make_unique<H264RateController>(settings_);

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

  int start_frame_index = 0;
  int last_frame_index =
      RunTestSequence(kCommonAvgBitrate, kCommonFps, kTestSequenceFrameCount,
                      settings_.num_temporal_layers, start_frame_index);
  base::TimeDelta timestamp = base::Microseconds(
      last_frame_index * base::Time::kMicrosecondsPerSecond / kCommonFps);

  EXPECT_EQ(kExpectedIntraFrameQP2,
            rate_controller_->EstimateIntraFrameQP(timestamp));

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
}

TEST_F(H264RateControllerTest, RunH264RateControllerFramerateMeanTest) {
  constexpr float kFrameRateExpectedValues[] = {29.9f, 30.1f};

  rate_controller_ = std::make_unique<H264RateController>(settings_);

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
  constexpr RateControllerTestValues kExpectedValues = {false, 80000, 4166,
                                                        75834, 4166};

  rate_controller_ = std::make_unique<H264RateController>(settings_);

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
}  // namespace

}  // namespace media
