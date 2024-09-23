// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_glitch_info.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class AudioGlitchInfoTester {
 public:
  static AudioGlitchInfo SingleBoundedSystemGlitch(
      const base::TimeDelta duration,
      const AudioGlitchInfo::Direction direction) {
    return AudioGlitchInfo::SingleBoundedSystemGlitch(duration, direction);
  }
};

TEST(AudioGlitchInfo, EqualityOperator) {
  AudioGlitchInfo info1{.duration = base::Seconds(1), .count = 123};
  AudioGlitchInfo info2{.duration = base::Seconds(3), .count = 369};
  AudioGlitchInfo info3 = info1;

  EXPECT_EQ(info1.duration, base::Seconds(1));
  EXPECT_EQ(info1.count, 123u);
  EXPECT_EQ(info2.duration, base::Seconds(3));
  EXPECT_EQ(info2.count, 369u);

  EXPECT_FALSE(info1 == info2);
  EXPECT_TRUE(info1 == info3);
}

TEST(AudioGlitchInfo, AudioGlitchInfoAccumulator) {
  AudioGlitchInfo::Accumulator accumulator;

  EXPECT_EQ(accumulator.GetAndReset(), AudioGlitchInfo());
  accumulator.Add(AudioGlitchInfo{.duration = base::Seconds(1), .count = 123});
  accumulator.Add(AudioGlitchInfo{.duration = base::Seconds(2), .count = 246});

  auto accumulated_glitches = accumulator.GetAndReset();
  EXPECT_EQ(accumulated_glitches.duration, base::Seconds(3));
  EXPECT_EQ(accumulated_glitches.count, 369u);

  EXPECT_EQ(accumulator.GetAndReset(), AudioGlitchInfo());
}

TEST(AudioGlitchInfo, ToString) {
  AudioGlitchInfo info{.duration = base::Milliseconds(123), .count = 456};

  EXPECT_EQ(info.ToString(), "duration (ms): 123, count: 456");
}

TEST(AudioGlitchInfo, SingleBoundedSystemGlitch) {
  base::HistogramTester histogram_tester_;
  {
    AudioGlitchInfo result{.duration = base::Seconds(1), .count = 1};
    EXPECT_EQ(AudioGlitchInfoTester::SingleBoundedSystemGlitch(
                  base::Seconds(1.5), AudioGlitchInfo::Direction::kRender),
              result);
    histogram_tester_.ExpectTimeBucketCount(
        "Media.Audio.Render.SystemGlitchDuration", base::Seconds(1.5), 1);
  }
  {
    AudioGlitchInfo result{.duration = base::Seconds(0.5), .count = 1};
    EXPECT_EQ(AudioGlitchInfoTester::SingleBoundedSystemGlitch(
                  base::Seconds(0.5), AudioGlitchInfo::Direction::kCapture),
              result);
    histogram_tester_.ExpectTimeBucketCount(
        "Media.Audio.Capture.SystemGlitchDuration", base::Seconds(0.5), 1);
  }
}

}  // namespace media
