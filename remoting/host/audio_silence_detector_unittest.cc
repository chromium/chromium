// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/audio_silence_detector.h"

#include <stdint.h>

#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

const int kSamplingRate = 1000;

void TestSilenceDetector(AudioSilenceDetector* target,
                         base::span<const int16_t> samples,
                         bool silence_expected) {
  target->Reset(kSamplingRate, 1);
  bool silence_started = false;
  int threshold_length = 0;
  base::span<const unsigned char> samples_bytes = base::as_bytes(samples);
  for (size_t i = 0; i < 3 * kSamplingRate / samples.size(); ++i) {
    bool result = target->IsSilence(samples_bytes);
    if (silence_started) {
      ASSERT_TRUE(result);
    } else if (result) {
      silence_started = true;
      threshold_length = i * samples.size();
    }
  }

  // Check that the silence was detected if it was expected.
  EXPECT_EQ(silence_expected, silence_started);

  if (silence_expected) {
    // Check that silence threshold is between 0.5 and 2 seconds.
    EXPECT_GE(threshold_length, kSamplingRate / 2);
    EXPECT_LE(threshold_length, kSamplingRate * 2);
  }
}

}  // namespace

TEST(AudioSilenceDetectorTest, Silence) {
  auto kSamples = std::to_array<int16_t>({0, 0, 0, 0, 0, 0, 0, 0, 0, 0});

  AudioSilenceDetector target(0);
  TestSilenceDetector(&target, kSamples, true);
}

TEST(AudioSilenceDetectorTest, Sound) {
  auto kSamples =
      std::to_array<int16_t>({65, 73, 83, 89, 92, -1, 5, 9, 123, 0});

  AudioSilenceDetector target(0);
  TestSilenceDetector(&target, kSamples, false);
}

TEST(AudioSilenceDetectorTest, Threshold) {
  auto kSamples = std::to_array<int16_t>({0, 0, 0, 0, 1, 0, 0, -1, 0, 0});

  AudioSilenceDetector target1(0);
  TestSilenceDetector(&target1, kSamples, false);

  AudioSilenceDetector target2(1);
  TestSilenceDetector(&target2, kSamples, true);
}

}  // namespace remoting
