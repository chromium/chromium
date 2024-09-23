// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/h264_ratectrl_rtc.h"

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

// Test H264RateCtrlRTCTest runs test cases for the class H264RateCtrlRTC.
class H264RateCtrlRTCTest : public testing::Test {
 public:
  H264RateCtrlRTCTest() = default;

  void SetUp() override {
    constexpr int kExpectedLoopFilterLevel = -1;

    rate_control_config_rtc_.content_type =
        VideoEncodeAccelerator::Config::ContentType::kCamera;
    rate_control_config_rtc_.frame_size.SetSize(kCommonFrameWidth,
                                                kCommonFrameHeight);
    rate_control_config_rtc_.fixed_delta_qp = true;
    rate_control_config_rtc_.ease_hrd_reduction = true;
    rate_control_config_rtc_.num_temporal_layers = 2;
    rate_control_config_rtc_.gop_max_duration = kCommonGopMaxDuration;
    rate_control_config_rtc_.frame_rate_max = kCommonFpsMax;
    rate_control_config_rtc_.layer_settings.emplace_back();
    rate_control_config_rtc_.layer_settings[0].avg_bitrate =
        kCommonAvgBitrate * 2 / 3;
    rate_control_config_rtc_.layer_settings[0].peak_bitrate =
        kCommonPeakBitrate * 2 / 3;
    rate_control_config_rtc_.layer_settings[0].hrd_buffer_size =
        kCommonHRDBufferSize * 2 / 3;
    rate_control_config_rtc_.layer_settings[0].min_qp = kCommonQpMin;
    rate_control_config_rtc_.layer_settings[0].max_qp = kCommonQpMax;
    rate_control_config_rtc_.layer_settings[0].frame_rate = kCommonFps / 2;
    rate_control_config_rtc_.layer_settings.emplace_back();
    rate_control_config_rtc_.layer_settings[1].avg_bitrate = kCommonAvgBitrate;
    rate_control_config_rtc_.layer_settings[1].peak_bitrate =
        kCommonPeakBitrate;
    rate_control_config_rtc_.layer_settings[1].hrd_buffer_size =
        kCommonHRDBufferSize;
    rate_control_config_rtc_.layer_settings[1].min_qp = kCommonQpMin;
    rate_control_config_rtc_.layer_settings[1].max_qp = kCommonQpMax;
    rate_control_config_rtc_.layer_settings[1].frame_rate = kCommonFps;

    rate_ctrl_rtc_ = H264RateCtrlRTC::Create(rate_control_config_rtc_);

    auto rate_control_config_rtc = rate_control_config_rtc_;
    rate_control_config_rtc.layer_settings[1].max_qp = kCommonQpMax + 1;

    EXPECT_FALSE((rate_control_config_rtc == rate_control_config_rtc_));
    EXPECT_TRUE((rate_control_config_rtc <=> rate_control_config_rtc_ ==
                 std::partial_ordering::greater));

    EXPECT_EQ(kExpectedLoopFilterLevel, rate_ctrl_rtc_->GetLoopfilterLevel());
  }

 protected:
  int RunTestSequence(int fps,
                      int start_frame_index,
                      int& last_intra_frame_qp,
                      int& last_inter_frame_qp,
                      int& drop_frames) {
    constexpr size_t kFirstIntraFrameIndex = 0;
    std::vector<size_t> frames{12500, 3000, 4000, 3000, 4000,  3000,
                               8000,  6000, 8000, 6000, 10000, 8000,
                               10000, 8000, 8000, 0};

    base::TimeDelta timestamp = base::Microseconds(
        start_frame_index * base::Time::kMicrosecondsPerSecond / fps);
    size_t layer_index = 0;
    size_t frame_index = 0;
    for (size_t encoded_size : frames) {
      bool keyframe = false;
      if (frame_index == kFirstIntraFrameIndex) {
        keyframe = true;
      }
      if (keyframe || layer_index == 1) {
        layer_index = 0;
      } else {
        layer_index = 1;
      }

      H264FrameParamsRTC frame_params;
      frame_params.keyframe = keyframe;
      frame_params.temporal_layer_id = layer_index;
      frame_params.timestamp = timestamp;

      H264RateCtrlRTC::FrameDropDecision frame_drop_decision =
          rate_ctrl_rtc_->ComputeQP(frame_params);
      if (keyframe) {
        EXPECT_EQ(H264RateCtrlRTC::FrameDropDecision::kOk, frame_drop_decision);
        last_intra_frame_qp = rate_ctrl_rtc_->GetQP();
      } else {
        last_inter_frame_qp = rate_ctrl_rtc_->GetQP();
        if (frame_drop_decision == H264RateCtrlRTC::FrameDropDecision::kDrop) {
          drop_frames++;
        }
      }
      rate_ctrl_rtc_->PostEncodeUpdate(encoded_size, frame_params);

      ++frame_index;
      timestamp += base::Microseconds(base::Time::kMicrosecondsPerSecond / fps);
    }

    return start_frame_index + frames.size();
  }

  std::unique_ptr<H264RateCtrlRTC> rate_ctrl_rtc_;
  H264RateControlConfigRTC rate_control_config_rtc_;
};

// Test Cases

// The test case runs a test sequence and validates QP and buffer fullness
// values. During the test, an HRD buffer overflow and frame drop condition are
// triggered. The test verifies the controller configuration update procedure.
TEST_F(H264RateCtrlRTCTest, RunBasicRateCtrlRTCTest) {
  constexpr int kExpectedBufferFullness01 = 0;
  constexpr int kExpectedBufferFullness11 = 0;
  constexpr int kExpectedIntraFrameQP2 = 34;
  constexpr int kExpectedInterFrameQP2 = 49;
  constexpr int kExpectedBufferFullness02 = 75;
  constexpr int kExpectedBufferFullness12 = 87;
  constexpr int kExpectedIntraFrameQP3 = 39;
  constexpr int kExpectedInterFrameQP3 = -1;
  constexpr int kExpectedBufferFullness03 = 56;
  constexpr int kExpectedBufferFullness13 = 111;
  constexpr int kExpectedDropFrames = 4;

  std::array<int, 2> buffer_fullness_array = {0, 0};
  base::span<int> buffer_fullness_values(buffer_fullness_array);

  rate_ctrl_rtc_->GetBufferFullness(buffer_fullness_values, base::TimeDelta());
  EXPECT_EQ(kExpectedBufferFullness01, buffer_fullness_values[kLayer0Index]);
  EXPECT_EQ(kExpectedBufferFullness11, buffer_fullness_values[kLayer1Index]);

  int start_frame_index = 0;
  int last_intra_frame_qp;
  int last_inter_frame_qp;
  int drop_frames = 0;
  int last_frame_index =
      RunTestSequence(kCommonFps, start_frame_index, last_intra_frame_qp,
                      last_inter_frame_qp, drop_frames);
  base::TimeDelta timestamp = base::Microseconds(
      last_frame_index * base::Time::kMicrosecondsPerSecond / kCommonFps);

  EXPECT_EQ(kExpectedIntraFrameQP2, last_intra_frame_qp);
  EXPECT_EQ(kExpectedInterFrameQP2, last_inter_frame_qp);

  rate_ctrl_rtc_->GetBufferFullness(buffer_fullness_values, timestamp);
  EXPECT_EQ(kExpectedBufferFullness02, buffer_fullness_values[kLayer0Index]);
  EXPECT_EQ(kExpectedBufferFullness12, buffer_fullness_values[kLayer1Index]);
  EXPECT_EQ(0, drop_frames);

  rate_control_config_rtc_.layer_settings[0].avg_bitrate = kCommonAvgBitrate;
  rate_control_config_rtc_.layer_settings[0].peak_bitrate = kCommonPeakBitrate;
  rate_control_config_rtc_.layer_settings[1].avg_bitrate =
      kCommonAvgBitrate * 4 / 3;
  rate_control_config_rtc_.layer_settings[1].peak_bitrate =
      kCommonPeakBitrate * 4 / 3;

  rate_ctrl_rtc_->UpdateRateControl(rate_control_config_rtc_);

  start_frame_index = last_frame_index;
  last_frame_index =
      RunTestSequence(kCommonFps, start_frame_index, last_intra_frame_qp,
                      last_inter_frame_qp, drop_frames);
  timestamp = base::Microseconds(
      last_frame_index * base::Time::kMicrosecondsPerSecond / kCommonFps);

  EXPECT_EQ(kExpectedIntraFrameQP3, last_intra_frame_qp);
  EXPECT_EQ(kExpectedInterFrameQP3, last_inter_frame_qp);

  rate_ctrl_rtc_->GetBufferFullness(buffer_fullness_values, timestamp);
  EXPECT_EQ(kExpectedBufferFullness03, buffer_fullness_values[kLayer0Index]);
  EXPECT_EQ(kExpectedBufferFullness13, buffer_fullness_values[kLayer1Index]);
  EXPECT_EQ(kExpectedDropFrames, drop_frames);
}

}  // namespace

}  // namespace media
