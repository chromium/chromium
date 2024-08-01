// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/hrd_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {
constexpr int kCommonFps = 30;
constexpr base::TimeDelta kCommonBufferDelay = base::Milliseconds(1000);
constexpr uint32_t kCommonAvgBitrate = 1000000;   // bits per second
constexpr uint32_t kCommonPeakBitrate = 2000000;  // bits per second

// Test HRDBufferTest runs the test frame size sequence in various
// scenarios and check whether the component behaves as expected.
class HRDBufferTest : public testing::Test {
 public:
  HRDBufferTest() = default;

  void SetUp() override {
    hrd_buffer_ = std::make_unique<HRDBuffer>(0, 0);
    EXPECT_EQ(0u, hrd_buffer_->buffer_size());
    EXPECT_EQ(0u, hrd_buffer_->average_bitrate());
  }

 protected:
  // Runs a loop of 60 frames with two intra encoded frames in the sequence.
  // Returns the index of the last frame.
  int RunTestSequence(uint32_t avg_bitrate, int fps, int start_frame_index) {
    // Generate steady encoded frame sizes aligned with the requested bitrate.
    constexpr int kFrameCount = 60;
    constexpr int kFirstIntraFrameIndex = 0;
    constexpr int kSecondIntraFrameIndex = 30;
    size_t frame_size = avg_bitrate / 8 / fps;
    std::vector<size_t> frames;
    for (int i = 0; i < kFrameCount; ++i) {
      frames.push_back(frame_size);
    }

    frames[kFirstIntraFrameIndex] = frames[kFirstIntraFrameIndex] * 3;
    frames[kSecondIntraFrameIndex] = frames[kSecondIntraFrameIndex] * 3;

    base::TimeDelta timestamp = base::Microseconds(
        start_frame_index * base::Time::kMicrosecondsPerSecond / fps);
    for (size_t encoded_size : frames) {
      hrd_buffer_->Shrink(timestamp);

      hrd_buffer_->AddFrameBytes(encoded_size, timestamp);

      timestamp += base::Microseconds(base::Time::kMicrosecondsPerSecond / fps);
    }

    return start_frame_index + kFrameCount;
  }

  // Size of HRD buffer calculated from the buffer delay is in milliseconds.
  int GetBufferSizeFromDelay(uint32_t avg_bitrate,
                             base::TimeDelta buffer_delay) const {
    return static_cast<int>(avg_bitrate * buffer_delay.InSecondsF() / 8);
  }

  int GetBufferFullness(base::TimeDelta timestamp) const {
    return 100 * hrd_buffer_->GetBytesAtTime(timestamp) /
           hrd_buffer_->buffer_size();
  }

  std::unique_ptr<HRDBuffer> hrd_buffer_;
};

// Test Cases

// Running a simple sequence of frames and taking a snapshot of the parameters
// after the last frame is added. The parameters must strictly satisfy the
// predefined state.
TEST_F(HRDBufferTest, RunBasicBufferTest) {
  constexpr int kExpectedBufferFullness = 13;
  constexpr int kExpectedBufferBytes = 16601;
  constexpr int kExpectedBufferBytesRemaining = 108399;
  constexpr int kExpectedLastFrameBufferBytes = 20771;
  constexpr int kExpectedFrameOvershooting = false;
  constexpr int kExpectedBufferFullnessBadTimestamp = 16;

  size_t buffer_size =
      GetBufferSizeFromDelay(kCommonAvgBitrate, kCommonBufferDelay);

  hrd_buffer_->SetParameters(buffer_size, kCommonAvgBitrate, kCommonPeakBitrate,
                             false);
  EXPECT_EQ(buffer_size, hrd_buffer_->buffer_size());
  EXPECT_EQ(kCommonAvgBitrate, hrd_buffer_->average_bitrate());

  int start_frame_index = 0;
  int last_frame_index =
      RunTestSequence(kCommonAvgBitrate, kCommonFps, start_frame_index);

  base::TimeDelta timestamp = base::Microseconds(
      last_frame_index * base::Time::kMicrosecondsPerSecond / kCommonFps);
  EXPECT_EQ(kExpectedBufferFullness, GetBufferFullness(timestamp));
  EXPECT_EQ(kExpectedBufferBytes, hrd_buffer_->GetBytesAtTime(timestamp));
  EXPECT_EQ(kExpectedBufferBytesRemaining,
            hrd_buffer_->GetBytesRemainingAtTime(timestamp));
  EXPECT_EQ(kExpectedLastFrameBufferBytes,
            hrd_buffer_->last_frame_buffer_bytes());
  EXPECT_EQ(kExpectedFrameOvershooting, hrd_buffer_->frame_overshooting());

  // Check behavior when invalid timestamp is provided.
  timestamp =
      base::Microseconds(last_frame_index * base::Time::kMicrosecondsPerSecond /
                         kCommonFps) -
      base::Microseconds(60000);
  EXPECT_EQ(kExpectedBufferFullnessBadTimestamp, GetBufferFullness(timestamp));
}

// The test runs the predefined test sequence three times using different buffer
// parameters. A snapshot of the buffer state is taken after each sequence
// run. The snapshot must strictly satisfy the predefined state.
TEST_F(HRDBufferTest, CheckBufferParameterChange) {
  constexpr int kExpectedBufferFullness1 = 13;
  constexpr int kExpectedBufferBytes1 = 16601;
  constexpr int kExpectedBufferBytesRemaining1 = 108399;
  constexpr int kExpectedLastFrameBufferBytes1 = 20771;
  constexpr int kExpectedBufferFullness2 = 0;
  constexpr int kExpectedBufferBytes2 = 0;
  constexpr int kExpectedBufferBytesRemaining2 = 125000;
  constexpr int kExpectedLastFrameBufferBytes2 = 4166;
  constexpr int kExpectedBufferFullness3 = 81;
  constexpr int kExpectedBufferBytes3 = 101328;
  constexpr int kExpectedBufferBytesRemaining3 = 23672;
  constexpr int kExpectedLastFrameBufferBytes3 = 104108;

  size_t buffer_size =
      GetBufferSizeFromDelay(kCommonAvgBitrate, kCommonBufferDelay);

  hrd_buffer_->SetParameters(buffer_size, kCommonAvgBitrate, kCommonPeakBitrate,
                             false);
  EXPECT_EQ(buffer_size, hrd_buffer_->buffer_size());
  EXPECT_EQ(kCommonAvgBitrate, hrd_buffer_->average_bitrate());

  int start_frame_index = 0;
  int last_frame_index =
      RunTestSequence(kCommonAvgBitrate, kCommonFps, start_frame_index);

  base::TimeDelta timestamp = base::Microseconds(
      last_frame_index * base::Time::kMicrosecondsPerSecond / kCommonFps);
  EXPECT_EQ(kExpectedBufferFullness1, GetBufferFullness(timestamp));
  EXPECT_EQ(kExpectedBufferBytes1, hrd_buffer_->GetBytesAtTime(timestamp));
  EXPECT_EQ(kExpectedBufferBytesRemaining1,
            hrd_buffer_->GetBytesRemainingAtTime(timestamp));
  EXPECT_EQ(kExpectedLastFrameBufferBytes1,
            hrd_buffer_->last_frame_buffer_bytes());

  // Increase average bitrate 50%.
  hrd_buffer_->SetParameters(buffer_size, kCommonAvgBitrate * 3 / 2,
                             kCommonPeakBitrate * 3 / 2, false);
  EXPECT_EQ(buffer_size, hrd_buffer_->buffer_size());
  EXPECT_EQ(kCommonAvgBitrate * 3 / 2, hrd_buffer_->average_bitrate());

  start_frame_index = last_frame_index;
  last_frame_index =
      RunTestSequence(kCommonAvgBitrate, kCommonFps, start_frame_index);

  timestamp = base::Microseconds(
      last_frame_index * base::Time::kMicrosecondsPerSecond / kCommonFps);
  EXPECT_EQ(kExpectedBufferFullness2, GetBufferFullness(timestamp));
  EXPECT_EQ(kExpectedBufferBytes2, hrd_buffer_->GetBytesAtTime(timestamp));
  EXPECT_EQ(kExpectedBufferBytesRemaining2,
            hrd_buffer_->GetBytesRemainingAtTime(timestamp));
  EXPECT_EQ(kExpectedLastFrameBufferBytes2,
            hrd_buffer_->last_frame_buffer_bytes());

  // Decrease average bitrate 33%.
  hrd_buffer_->SetParameters(buffer_size, kCommonAvgBitrate * 2 / 3,
                             kCommonPeakBitrate * 2 / 3, false);
  EXPECT_EQ(buffer_size, hrd_buffer_->buffer_size());
  EXPECT_EQ(kCommonAvgBitrate * 2 / 3, hrd_buffer_->average_bitrate());

  start_frame_index = last_frame_index;
  last_frame_index =
      RunTestSequence(kCommonAvgBitrate, kCommonFps, start_frame_index);

  timestamp = base::Microseconds(
      last_frame_index * base::Time::kMicrosecondsPerSecond / kCommonFps);
  EXPECT_EQ(kExpectedBufferFullness3, GetBufferFullness(timestamp));
  EXPECT_EQ(kExpectedBufferBytes3, hrd_buffer_->GetBytesAtTime(timestamp));
  EXPECT_EQ(kExpectedBufferBytesRemaining3,
            hrd_buffer_->GetBytesRemainingAtTime(timestamp));
  EXPECT_EQ(kExpectedLastFrameBufferBytes3,
            hrd_buffer_->last_frame_buffer_bytes());
}

// The test uses extended HRD buffer constructor which initiates the buffer
// internal state with the provided parameters. After running the first test
// sequence, a new buffer is created with predefined state and another sequence
// is run after that. A snapshot of the buffer state is checked stirictly
// against predefined values after each sequence run.
TEST_F(HRDBufferTest, CheckSettingBufferState) {
  constexpr int kExpectedBufferFullness1 = 0;
  constexpr int kExpectedBufferBytes1 = 0;
  constexpr int kExpectedBufferBytesRemaining1 = 125000;
  constexpr int kExpectedLastFrameBufferBytes1 = 0;
  constexpr base::TimeDelta kExpectedLastFrameTimestamp1 =
      base::Microseconds(-1);
  constexpr int kExpectedBufferFullness2 = 12;
  constexpr int kExpectedBufferBytes2 = 15875;
  constexpr int kExpectedBufferBytesRemaining2 = 109125;
  constexpr int kExpectedLastFrameBufferBytes2 = 20000;
  constexpr base::TimeDelta kExpectedLastFrameTimestamp2 =
      base::Microseconds(99000);

  size_t buffer_size =
      GetBufferSizeFromDelay(kCommonAvgBitrate, kCommonBufferDelay);

  hrd_buffer_->SetParameters(buffer_size, kCommonAvgBitrate, kCommonPeakBitrate,
                             false);
  EXPECT_EQ(buffer_size, hrd_buffer_->buffer_size());
  EXPECT_EQ(kCommonAvgBitrate, hrd_buffer_->average_bitrate());

  base::TimeDelta timestamp = base::Microseconds(0);

  EXPECT_EQ(kExpectedBufferFullness1, GetBufferFullness(timestamp));
  EXPECT_EQ(kExpectedBufferBytes1, hrd_buffer_->GetBytesAtTime(timestamp));
  EXPECT_EQ(kExpectedBufferBytesRemaining1,
            hrd_buffer_->GetBytesRemainingAtTime(timestamp));
  EXPECT_EQ(kExpectedLastFrameBufferBytes1,
            hrd_buffer_->last_frame_buffer_bytes());
  EXPECT_EQ(kExpectedLastFrameTimestamp1, hrd_buffer_->last_frame_timestamp());

  constexpr int kLastFrameBufferBytes2 = 20000;
  constexpr base::TimeDelta kCurrFrameTimestamp2 = base::Microseconds(132000);
  constexpr base::TimeDelta kLastFrameTimestamp2 = base::Microseconds(99000);

  hrd_buffer_ =
      std::make_unique<HRDBuffer>(buffer_size, kCommonAvgBitrate,
                                  kLastFrameBufferBytes2, kLastFrameTimestamp2);

  timestamp = kCurrFrameTimestamp2;

  EXPECT_EQ(kExpectedBufferFullness2, GetBufferFullness(timestamp));
  EXPECT_EQ(kExpectedBufferBytes2, hrd_buffer_->GetBytesAtTime(timestamp));
  EXPECT_EQ(kExpectedBufferBytesRemaining2,
            hrd_buffer_->GetBytesRemainingAtTime(timestamp));
  EXPECT_EQ(kExpectedLastFrameBufferBytes2,
            hrd_buffer_->last_frame_buffer_bytes());
  EXPECT_EQ(kExpectedLastFrameTimestamp2, hrd_buffer_->last_frame_timestamp());
}

// Checks the last frame timestamp parameter after a frame is added to the
// buffer.
TEST_F(HRDBufferTest, CheckBufferLastFrameTimestamp) {
  size_t buffer_size =
      GetBufferSizeFromDelay(kCommonAvgBitrate, kCommonBufferDelay);

  hrd_buffer_->SetParameters(buffer_size, kCommonAvgBitrate, kCommonPeakBitrate,
                             false);
  EXPECT_EQ(buffer_size, hrd_buffer_->buffer_size());
  EXPECT_EQ(kCommonAvgBitrate, hrd_buffer_->average_bitrate());

  base::TimeDelta timestamp = base::Microseconds(100000);

  size_t encoded_size(10000);
  hrd_buffer_->AddFrameBytes(encoded_size, timestamp);

  EXPECT_EQ(timestamp, hrd_buffer_->last_frame_timestamp());
}

// Checks the buffer fullness parameter when the size of the buffer is being
// reduced. The size should follow strictly the predefined buffer size values.
TEST_F(HRDBufferTest, CheckBufferShrinking) {
  constexpr int kFrameSequenceValues[] = {10000, 10000, 10000, 10000, 10000,
                                          10000, 10000, 10000, 10000, 10000};
  constexpr size_t kBufferShrinkingValues[] = {122917, 120834, 118751, 116668,
                                               114585, 112502, 110419, 108336,
                                               106253, 104170};

  const size_t buffer_size =
      GetBufferSizeFromDelay(kCommonAvgBitrate, kCommonBufferDelay);

  hrd_buffer_->SetParameters(buffer_size, kCommonAvgBitrate, kCommonPeakBitrate,
                             false);
  EXPECT_EQ(buffer_size, hrd_buffer_->buffer_size());
  EXPECT_EQ(kCommonAvgBitrate, hrd_buffer_->average_bitrate());

  base::TimeDelta timestamp = base::Microseconds(0);
  for (size_t encoded_size : kFrameSequenceValues) {
    hrd_buffer_->AddFrameBytes(encoded_size, timestamp);

    timestamp +=
        base::Microseconds(base::Time::kMicrosecondsPerSecond / kCommonFps);
  }

  hrd_buffer_->SetParameters(buffer_size / 2, kCommonAvgBitrate,
                             kCommonPeakBitrate, true);
  // The size of the buffer remains the same, since it will be reduced
  // gradually.
  EXPECT_EQ(buffer_size, hrd_buffer_->buffer_size());
  EXPECT_EQ(kCommonAvgBitrate, hrd_buffer_->average_bitrate());

  int frame_index = 0;
  for (size_t encoded_size : kFrameSequenceValues) {
    hrd_buffer_->Shrink(timestamp);

    hrd_buffer_->AddFrameBytes(encoded_size, timestamp);

    EXPECT_EQ(kBufferShrinkingValues[frame_index], hrd_buffer_->buffer_size());

    ++frame_index;
    timestamp +=
        base::Microseconds(base::Time::kMicrosecondsPerSecond / kCommonFps);
  }

  hrd_buffer_->SetParameters(buffer_size / 3, kCommonAvgBitrate,
                             static_cast<uint32_t>(kCommonAvgBitrate * 1.2f),
                             true);
  // The size of the buffer changes immeditely since the peak bitrate is less
  // than 1.5 of the average bitrate.
  EXPECT_EQ(buffer_size / 3, hrd_buffer_->buffer_size());
  EXPECT_EQ(kCommonAvgBitrate, hrd_buffer_->average_bitrate());
}

// Checks the buffer overshoot condition. After running the test frame sequence
// the buffer should overshoot at the predefined frame index.
TEST_F(HRDBufferTest, CheckBufferOvershoot) {
  constexpr int kFrameSequenceValues[] = {30000, 10000, 10000, 10000, 10000,
                                          10000, 10000, 10000, 10000, 10000};
  constexpr int kExpectedFrameOvershootingIndex = 6;

  const size_t buffer_size =
      GetBufferSizeFromDelay(kCommonAvgBitrate, kCommonBufferDelay / 2);

  hrd_buffer_->SetParameters(buffer_size, kCommonAvgBitrate, kCommonPeakBitrate,
                             false);
  EXPECT_EQ(buffer_size, hrd_buffer_->buffer_size());
  EXPECT_EQ(kCommonAvgBitrate, hrd_buffer_->average_bitrate());

  base::TimeDelta timestamp = base::Microseconds(0);
  int frame_index = 0;
  for (size_t encoded_size : kFrameSequenceValues) {
    hrd_buffer_->AddFrameBytes(encoded_size, timestamp);

    if (!hrd_buffer_->frame_overshooting()) {
      EXPECT_GT(kExpectedFrameOvershootingIndex, frame_index);
    } else {
      EXPECT_LE(kExpectedFrameOvershootingIndex, frame_index);
    }

    ++frame_index;
    timestamp +=
        base::Microseconds(base::Time::kMicrosecondsPerSecond / kCommonFps);
  }
}

}  // namespace
}  // namespace media
