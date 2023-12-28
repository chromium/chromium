// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/silence_detector.h"

#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_timestamp_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {
constexpr base::TimeDelta kSilenceThreshold = base::Milliseconds(500);
constexpr base::TimeDelta kTypicalBufferLength = base::Milliseconds(20);
constexpr int kSampleRate = 48000;
constexpr int kChannels = 1;
}  // namespace

class SilenceDetectorTest : public ::testing::Test {
 public:
  SilenceDetectorTest() : silence_detector_(kSampleRate, kSilenceThreshold) {}

  SilenceDetectorTest(const SilenceDetectorTest&) = delete;
  SilenceDetectorTest& operator=(const SilenceDetectorTest&) = delete;

  void FeedSilence(base::TimeDelta duration) {
    auto silent_buffer = AudioBus::Create(
        kChannels, AudioTimestampHelper::TimeToFrames(duration, kSampleRate));
    silent_buffer->Zero();
    silence_detector_.Scan(*silent_buffer);
  }

  void FeedData(base::TimeDelta duration) {
    auto audio_bus = AudioBus::Create(
        kChannels, AudioTimestampHelper::TimeToFrames(duration, kSampleRate));

    // A single non-zero value should be enough for the entire buffer not be
    // considered silence.
    audio_bus->channel(0)[0] = 1.0;
    audio_bus->ZeroFramesPartial(1, audio_bus->frames() - 1);
    ASSERT_FALSE(audio_bus->AreFramesZero());

    silence_detector_.Scan(*audio_bus);
  }

 protected:
  SilenceDetector silence_detector_;
};

// Makes sure the silence detector starts silent.
TEST_F(SilenceDetectorTest, StartsSilent) {
  EXPECT_TRUE(silence_detector_.IsSilent());
}

// Makes sure the silence detector stays silent when it has only ever scanned
// silence.
TEST_F(SilenceDetectorTest, ScanningSilence_IsSilent) {
  base::TimeDelta total_duration_scanned;
  while (total_duration_scanned < 2 * kSilenceThreshold) {
    FeedSilence(kTypicalBufferLength);
    total_duration_scanned += kTypicalBufferLength;
    EXPECT_TRUE(silence_detector_.IsSilent());
  }
}

// Makes sure the silence detector isn't silent after a single audible buffer.
TEST_F(SilenceDetectorTest, ScanningSingleAudibleBuffer_IsNotSilent) {
  FeedData(kTypicalBufferLength);
  EXPECT_FALSE(silence_detector_.IsSilent());
}

// Makes sure the silence detector isn't silent after scanning multiple audible
// buffer.
TEST_F(SilenceDetectorTest, ScanningMultipleAudibleBuffers_IsNotSilent) {
  base::TimeDelta total_duration_scanned;
  while (total_duration_scanned < 2 * kSilenceThreshold) {
    FeedData(kTypicalBufferLength);
    total_duration_scanned += kTypicalBufferLength;
    EXPECT_FALSE(silence_detector_.IsSilent());
  }
}

// Makes sure the silence detector can detect silence after scanning multiple
// audible buffer.
TEST_F(SilenceDetectorTest, ScanningAudibleBufferThenSilence_IsSilent) {
  FeedData(kTypicalBufferLength);
  EXPECT_FALSE(silence_detector_.IsSilent());

  constexpr base::TimeDelta kSilenceIncrement = kTypicalBufferLength;

  // Scan silence until the next buffer would push us across the silence
  // threshold
  base::TimeDelta total_silence_scanned;
  while (total_silence_scanned + kSilenceIncrement < kSilenceThreshold) {
    FeedSilence(kSilenceIncrement);
    total_silence_scanned += kSilenceIncrement;
    EXPECT_FALSE(silence_detector_.IsSilent());
  }

  // One more buffer of silence should push us across the silence threshold.
  FeedSilence(kSilenceIncrement);
  EXPECT_TRUE(silence_detector_.IsSilent());
}

// Makes sure that any audible data resets the silence threshold
TEST_F(SilenceDetectorTest, ScanningAnyAudibleDataResetsSilence) {
  // Start with audible data.
  FeedData(kTypicalBufferLength);
  EXPECT_FALSE(silence_detector_.IsSilent());

  constexpr base::TimeDelta kSmallDuration = base::Milliseconds(1);

  // Feed almost enough silence to trigger the detector.
  FeedSilence(kSilenceThreshold - kSmallDuration);
  EXPECT_FALSE(silence_detector_.IsSilent());

  // Inject a bit of audible data.
  FeedData(kSmallDuration);
  EXPECT_FALSE(silence_detector_.IsSilent());

  // A bit more silence shouldn't trigger the silence threshold.
  FeedSilence(kSmallDuration);
  EXPECT_FALSE(silence_detector_.IsSilent());

  // We should detect silence after enough data is added.
  FeedSilence(kSilenceThreshold - kSmallDuration);
  EXPECT_TRUE(silence_detector_.IsSilent());
}

// Makes sure the silence detector is silent after a reset.
TEST_F(SilenceDetectorTest, Reset_IsSilent) {
  // Force audibility.
  FeedData(kTypicalBufferLength);
  EXPECT_FALSE(silence_detector_.IsSilent());

  silence_detector_.ResetToSilence();
  EXPECT_TRUE(silence_detector_.IsSilent());
}

}  // namespace media
