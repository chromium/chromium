// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/frame_size_estimator.h"

#include "media/gpu/h264_rate_control_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {
constexpr int kCommonFps = 30;
constexpr uint32_t kCommonAvgBitrate = 1000000;  // bits per second

// Test FrameSizeEstimatorTest updates the estimator running the test frame
// sequence and checks whether the expected output from the component is
// generated.
class FrameSizeEstimatorTest : public testing::Test {
 public:
  FrameSizeEstimatorTest() = default;

  void SetUp() override {
    float bytes_per_frame_avg = kCommonAvgBitrate / 8 / kCommonFps;
    float qp_size_init =
        h264_rate_control_util::QP2QStepSize(24) * bytes_per_frame_avg;
    float size_correction_init = 0.3f * bytes_per_frame_avg;

    frame_size_estimator_ = std::make_unique<FrameSizeEstimator>(
        base::Milliseconds(300), qp_size_init, size_correction_init);
    EXPECT_EQ(41660.0f, frame_size_estimator_->qp_size_mean());
    EXPECT_EQ(1249.800049f, frame_size_estimator_->size_correction_mean());
  }

 protected:
  // Runs a loop of P encoded frames in the sequence.
  // Returns the index of the last frame. QP is circularly taken from the
  // set { 22, 24, 26, 28, 26, 24 }.
  int RunTestSequence(uint32_t avg_bitrate,
                      int fps,
                      int frame_count,
                      int start_frame_index) {
    constexpr uint32_t kQpValues[] = {22, 24, 26, 28, 26, 24};
    size_t common_frame_size = avg_bitrate / 8 / fps;

    base::TimeDelta timestamp = base::Microseconds(
        start_frame_index * base::Time::kMicrosecondsPerSecond / fps);
    for (int i = 0; i < frame_count; ++i) {
      uint32_t qp = kQpValues[(i + 1) % (sizeof(kQpValues) / sizeof(uint32_t))];
      uint32_t qp_prev = kQpValues[i % (sizeof(kQpValues) / sizeof(uint32_t))];
      size_t encoded_size = 0.625 * 16 * common_frame_size /
                            h264_rate_control_util::QP2QStepSize(qp);

      frame_size_estimator_->Update(encoded_size, qp, qp_prev, timestamp);

      timestamp += base::Microseconds(base::Time::kMicrosecondsPerSecond / fps);
    }

    return start_frame_index + frame_count;
  }

  std::unique_ptr<FrameSizeEstimator> frame_size_estimator_;
};

// Test Cases

// The test runs a predefined sequence of frame sizes two times and checks
// the stats after running each sequence. Different window size is used in
// each run.
TEST_F(FrameSizeEstimatorTest, RunBasicFrameSizeEstimatorTest) {
  constexpr float kExpectedQpSizeMeanMin1 = 33060.32f;
  constexpr float kExpectedQpSizeMeanMax1 = 33060.34f;
  constexpr float kExpectedSizeCorrectionMeanMin1 = -0.01f;
  constexpr float kExpectedSizeCorrectionMeanMax1 = 0.01f;
  constexpr float kExpectedQpSizeMeanMin2 = 52165.11f;
  constexpr float kExpectedQpSizeMeanMax2 = 52165.13f;
  constexpr float kExpectedSizeCorrectionMeanMin2 = 38.68f;
  constexpr float kExpectedSizeCorrectionMeanMax2 = 38.70f;

  int start_frame_index = 0;
  int last_frame_index =
      RunTestSequence(kCommonAvgBitrate, kCommonFps, 30, start_frame_index);

  EXPECT_LT(kExpectedQpSizeMeanMin1, frame_size_estimator_->qp_size_mean());
  EXPECT_GT(kExpectedQpSizeMeanMax1, frame_size_estimator_->qp_size_mean());
  EXPECT_LT(kExpectedSizeCorrectionMeanMin1,
            frame_size_estimator_->size_correction_mean());
  EXPECT_GT(kExpectedSizeCorrectionMeanMax1,
            frame_size_estimator_->size_correction_mean());

  start_frame_index = last_frame_index;
  frame_size_estimator_->UpdateMaxWindowSize(base::Milliseconds(3000));
  RunTestSequence(kCommonAvgBitrate, kCommonFps, 50, start_frame_index);

  EXPECT_LT(kExpectedQpSizeMeanMin2, frame_size_estimator_->qp_size_mean());
  EXPECT_GT(kExpectedQpSizeMeanMax2, frame_size_estimator_->qp_size_mean());
  EXPECT_LT(kExpectedSizeCorrectionMeanMin2,
            frame_size_estimator_->size_correction_mean());
  EXPECT_GT(kExpectedSizeCorrectionMeanMax2,
            frame_size_estimator_->size_correction_mean());
}

// The test updates the buffer with predefined sample sequence and checks the
// estimated frame size values.
TEST_F(FrameSizeEstimatorTest, CheckEstimatorStates) {
  constexpr size_t kEstimatedFrameSizeValues[] = {4556, 3874, 2663, 5259, 4916,
                                                  5540, 2352, 3095, 2655, 5250};
  constexpr uint32_t kQpValues[] = {22, 24, 26, 28, 26, 24};

  size_t common_frame_size = kCommonAvgBitrate / 8 / kCommonFps;
  base::TimeDelta timestamp = base::Microseconds(0);
  for (int i = 0; i < 10; ++i) {
    uint32_t qp = kQpValues[(i + 1) % (sizeof(kQpValues) / sizeof(uint32_t))];
    uint32_t qp_prev = kQpValues[i % (sizeof(kQpValues) / sizeof(uint32_t))];
    size_t encoded_size = 0.625 * 16 * common_frame_size /
                          h264_rate_control_util::QP2QStepSize(qp);

    size_t estimated_size = frame_size_estimator_->Estimate(qp, qp_prev);

    frame_size_estimator_->Update(encoded_size, qp, qp_prev, timestamp);

    EXPECT_EQ(kEstimatedFrameSizeValues[i], estimated_size);

    timestamp +=
        base::Microseconds(base::Time::kMicrosecondsPerSecond / kCommonFps);
  }
}

}  // namespace

}  // namespace media
