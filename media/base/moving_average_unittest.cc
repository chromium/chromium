// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "media/base/moving_average.h"
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

TEST(MovingAverageTest, MinAndMax) {
  MovingAverage moving_average(5);
  base::TimeDelta min = base::Seconds(1);
  base::TimeDelta med = base::Seconds(50);
  base::TimeDelta max = base::Seconds(100);
  moving_average.AddSample(min);
  moving_average.AddSample(med);
  moving_average.AddSample(med);
  moving_average.AddSample(med);
  moving_average.AddSample(max);
  auto extremes = moving_average.GetMinAndMax();
  EXPECT_EQ(extremes.first, min);
  EXPECT_EQ(extremes.second, max);
}

}  // namespace media
