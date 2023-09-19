// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/moving_average.h"
#include "base/moving_window.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(MovingAverageTest, AverageAndDeviation) {
  const int kSamples = 5;
  MovingAverage moving_average(kSamples);
  moving_average.AddSample(base::Seconds(1));
  EXPECT_EQ(base::Seconds(1), moving_average.Average());
  EXPECT_EQ(base::TimeDelta(), moving_average.Deviation());

  for (int i = 0; i < kSamples - 1; ++i)
    moving_average.AddSample(base::Seconds(1));
  EXPECT_EQ(base::Seconds(1), moving_average.Average());
  EXPECT_EQ(base::TimeDelta(), moving_average.Deviation());

  base::TimeDelta expect_deviation[] = {
      base::Microseconds(200000), base::Microseconds(244948),
      base::Microseconds(244948), base::Microseconds(200000),
      base::Milliseconds(0),
  };
  for (int i = 0; i < kSamples; ++i) {
    moving_average.AddSample(base::Milliseconds(500));
    EXPECT_EQ(base::Milliseconds(1000 - (i + 1) * 100),
              moving_average.Average());
    EXPECT_EQ(expect_deviation[i], moving_average.Deviation());
  }
}

TEST(MovingAverageTest, Reset) {
  MovingAverage moving_average(2);
  moving_average.AddSample(base::Seconds(1));
  EXPECT_EQ(base::Seconds(1), moving_average.Average());
  moving_average.Reset();
  moving_average.AddSample(base::TimeDelta());
  EXPECT_EQ(base::TimeDelta(), moving_average.Average());
  EXPECT_EQ(base::TimeDelta(), moving_average.Deviation());
}

// TODO(crbug.com/1475160): Remove the MovingAverage altogether.
// This test confirms that the new base::MovingWindow implementation is
// functially equivalent.
TEST(MovingAverageTest, TheNewIsSameAsOld) {
  const int kSamples = 32;
  MovingAverage moving_average(kSamples);
  base::MovingMeanVariance<int64_t, int64_t, double> new_window(kSamples);

  int64_t cur_us = 1;
  for (int i = 0; i < kSamples * 1000; ++i) {
    cur_us *= 239017;
    cur_us %= 64001;
    moving_average.AddSample(base::Milliseconds(cur_us));
    new_window.AddSample(base::Milliseconds(cur_us).InMicroseconds());
    EXPECT_EQ(new_window.Mean(), moving_average.Average().InMicroseconds());

    EXPECT_EQ(static_cast<int64_t>(new_window.Deviation()),
              moving_average.Deviation().InMicroseconds());
  }
}

}  // namespace media
