// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_latency.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/limits.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// Tuple of <sample rate, hardware buffer size, min buffer size, max buffer
// size>.
using AudioLatencyTestData = std::tuple<int, int, int, int>;

class AudioLatencyTest : public testing::TestWithParam<AudioLatencyTestData> {
 public:
  AudioLatencyTest() = default;
  ~AudioLatencyTest() override = default;

  void TestExactBufferSizes() {
    const int hardware_sample_rate = std::get<0>(GetParam());
    const int hardware_buffer_size = std::get<1>(GetParam());
    const int min_buffer_size = std::get<2>(GetParam());
    const int max_buffer_size = std::get<3>(GetParam());

    const int platform_min_buffer_size =
        min_buffer_size ? min_buffer_size : hardware_buffer_size;

// Windows 10 may allow exactly the minimum buffer size using the IAudioClient3
// API but any other buffer size must be a multiple of the hardware_buffer_size
// and not the min_buffer_size.
#if defined(OS_WIN)
    const int multiplier = hardware_buffer_size;
#else
    const int multiplier = platform_min_buffer_size;
#endif

    const int platform_max_buffer_size =
        max_buffer_size
            ? (limits::kMaxWebAudioBufferSize / max_buffer_size) *
                  max_buffer_size
            : (limits::kMaxWebAudioBufferSize / multiplier) * multiplier;

    EXPECT_EQ(platform_min_buffer_size,
              media::AudioLatency::GetExactBufferSize(
                  base::TimeDelta::FromSecondsD(0.0), hardware_sample_rate,
                  hardware_buffer_size, min_buffer_size, max_buffer_size,
                  limits::kMaxWebAudioBufferSize));
    EXPECT_EQ(
        platform_min_buffer_size,
        media::AudioLatency::GetExactBufferSize(
            base::TimeDelta::FromSecondsD(
                min_buffer_size / static_cast<double>(hardware_sample_rate)),
            hardware_sample_rate, hardware_buffer_size, min_buffer_size,
            max_buffer_size, limits::kMaxWebAudioBufferSize));
    EXPECT_EQ(
        multiplier * 2,
        media::AudioLatency::GetExactBufferSize(
            base::TimeDelta::FromSecondsD(
                (multiplier * 2) / static_cast<double>(hardware_sample_rate)),
            hardware_sample_rate, hardware_buffer_size, min_buffer_size,
            max_buffer_size, limits::kMaxWebAudioBufferSize));
    EXPECT_EQ(
        multiplier * 2,
        media::AudioLatency::GetExactBufferSize(
            base::TimeDelta::FromSecondsD(
                (multiplier * 1.1) / static_cast<double>(hardware_sample_rate)),
            hardware_sample_rate, hardware_buffer_size, min_buffer_size,
            max_buffer_size, limits::kMaxWebAudioBufferSize));
    EXPECT_EQ(platform_max_buffer_size,
              media::AudioLatency::GetExactBufferSize(
                  base::TimeDelta::FromSecondsD(10.0), hardware_sample_rate,
                  hardware_buffer_size, min_buffer_size, max_buffer_size,
                  limits::kMaxWebAudioBufferSize));
    if (max_buffer_size) {
      EXPECT_EQ(
          max_buffer_size,
          media::AudioLatency::GetExactBufferSize(
              base::TimeDelta::FromSecondsD(
                  max_buffer_size / static_cast<double>(hardware_sample_rate)),
              hardware_sample_rate, hardware_buffer_size, min_buffer_size,
              max_buffer_size, limits::kMaxWebAudioBufferSize));
    }

#if defined(OS_WIN)
    if (min_buffer_size && min_buffer_size < hardware_buffer_size) {
      EXPECT_EQ(hardware_buffer_size,
                media::AudioLatency::GetExactBufferSize(
                    base::TimeDelta::FromSecondsD(
                        (min_buffer_size * 1.1) /
                        static_cast<double>(hardware_sample_rate)),
                    hardware_sample_rate, hardware_buffer_size, min_buffer_size,
                    max_buffer_size, limits::kMaxWebAudioBufferSize));
    }
#elif defined(OS_MACOSX)
    EXPECT_EQ(limits::kMaxWebAudioBufferSize,
              media::AudioLatency::GetExactBufferSize(
                  base::TimeDelta::FromSecondsD(
                      (limits::kMaxAudioBufferSize * 1.1) /
                      static_cast<double>(hardware_sample_rate)),
                  hardware_sample_rate, hardware_buffer_size, min_buffer_size,
                  max_buffer_size, limits::kMaxWebAudioBufferSize));
#endif

    int previous_buffer_size = 0;
    for (int i = 0; i < 1000; i++) {
      int buffer_size = media::AudioLatency::GetExactBufferSize(
          base::TimeDelta::FromSecondsD(i / 1000.0), hardware_sample_rate,
          hardware_buffer_size, min_buffer_size, max_buffer_size,
          limits::kMaxWebAudioBufferSize);
      EXPECT_GE(buffer_size, previous_buffer_size);
#if defined(OS_WIN)
      EXPECT_TRUE(buffer_size == min_buffer_size ||
                  buffer_size % multiplier == 0 ||
                  buffer_size % max_buffer_size == 0);
#else
      EXPECT_EQ(buffer_size, buffer_size / multiplier * multiplier);
#endif
      previous_buffer_size = buffer_size;
    }
  }
};

// TODO(olka): extend unit tests, use real-world sample rates.

TEST(AudioLatency, HighLatencyBufferSizes) {
#if defined(OS_WIN)
  for (int i = 6400; i <= 204800; i *= 2) {
    EXPECT_EQ(2 * (i / 100),
              AudioLatency::GetHighLatencyBufferSize(i, i / 100));
  }
#else
  for (int i = 6400; i <= 204800; i *= 2)
    EXPECT_EQ(2 * (i / 100), AudioLatency::GetHighLatencyBufferSize(i, 32));
#endif  // defined(OS_WIN)
}

TEST(AudioLatency, InteractiveBufferSizes) {
  for (int i = 6400; i <= 204800; i *= 2)
    EXPECT_EQ(i / 100, AudioLatency::GetInteractiveBufferSize(i / 100));
}

TEST(AudioLatency, RtcBufferSizes) {
  for (int i = 6400; i < 204800; i *= 2) {
    EXPECT_EQ(i / 100, AudioLatency::GetRtcBufferSize(i, 0));
#if defined(OS_WIN)
    EXPECT_EQ(500, AudioLatency::GetRtcBufferSize(i, 500));
#elif defined(OS_ANDROID)
    EXPECT_EQ(i / 50, AudioLatency::GetRtcBufferSize(i, i / 50 - 1));
    EXPECT_EQ(i / 50 + 1, AudioLatency::GetRtcBufferSize(i, i / 50 + 1));
#else
    EXPECT_EQ(i / 100, AudioLatency::GetRtcBufferSize(i, 500));
#endif  // defined(OS_WIN)
  }
}

TEST_P(AudioLatencyTest, ExactBufferSizes) {
  TestExactBufferSizes();
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    AudioLatencyTest,
#if defined(OS_WIN)
    // Windows 10 with supported driver will have valid min and max buffer sizes
    // whereas older Windows will have zeros. The specific min, max and hardware
    // are device-dependent.
    testing::Values(std::make_tuple(44100, 440, 128, 440),
                    std::make_tuple(44100, 440, 440, 440),
                    std::make_tuple(44100, 440, 440, 880),
                    std::make_tuple(44100, 440, 440, 4400),
                    std::make_tuple(44100, 440, 128, 4196),
                    std::make_tuple(44100, 440, 440, 4196),
                    std::make_tuple(44100, 440, 0, 0),
                    std::make_tuple(44100, 256, 128, 512),
                    std::make_tuple(44100, 256, 0, 0))
#elif defined(OS_MACOSX) || defined(USE_CRAS)
    // These values are constant on Mac and ChromeOS, regardless of device.
    testing::Values(std::make_tuple(44100,
                                    256,
                                    limits::kMinAudioBufferSize,
                                    limits::kMaxAudioBufferSize))
#else
    testing::Values(std::make_tuple(44100, 256, 0, 0),
                    std::make_tuple(44100, 440, 0, 0))
#endif
);
}  // namespace media
