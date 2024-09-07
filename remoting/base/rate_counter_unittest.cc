// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/base/rate_counter.h"

#include <stddef.h>
#include <stdint.h>

#include "base/test/simple_test_tick_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

static const int64_t kTestValues[] = {10, 20, 30, 10, 25, 16, 15};

// One second window and one sample per second, so rate equals each sample.
TEST(RateCounterTest, OneSecondWindow) {
  base::SimpleTestTickClock tick_clock;
  RateCounter rate_counter(base::Seconds(1), &tick_clock);
  EXPECT_EQ(0, rate_counter.Rate());

  for (size_t i = 0; i < std::size(kTestValues); ++i) {
    tick_clock.Advance(base::Seconds(1));
    rate_counter.Record(kTestValues[i]);
    EXPECT_EQ(static_cast<double>(kTestValues[i]), rate_counter.Rate());
  }
}

// Record all samples instantaneously, so the rate is the total of the samples.
TEST(RateCounterTest, OneSecondWindowAllSamples) {
  base::SimpleTestTickClock tick_clock;
  RateCounter rate_counter(base::Seconds(1), &tick_clock);
  EXPECT_EQ(0, rate_counter.Rate());

  double expected = 0.0;
  for (size_t i = 0; i < std::size(kTestValues); ++i) {
    rate_counter.Record(kTestValues[i]);
    expected += kTestValues[i];
  }

  EXPECT_EQ(expected, rate_counter.Rate());
}

// Two second window, one sample per second.  For all but the first sample, the
// rate should be the average of it and the preceding one.  For the first it
// will be the average of the sample with zero.
TEST(RateCounterTest, TwoSecondWindow) {
  base::SimpleTestTickClock tick_clock;
  RateCounter rate_counter(base::Seconds(2), &tick_clock);
  EXPECT_EQ(0, rate_counter.Rate());

  for (size_t i = 0; i < std::size(kTestValues); ++i) {
    tick_clock.Advance(base::Seconds(1));
    rate_counter.Record(kTestValues[i]);
    double expected = kTestValues[i];
    if (i > 0) {
      expected += kTestValues[i - 1];
    }
    expected /= 2;
    EXPECT_EQ(expected, rate_counter.Rate());
  }
}

// Sample over a window one second shorter than the number of samples.
// Rate should be the average of all but the first sample.
TEST(RateCounterTest, LongWindow) {
  const size_t kWindowSeconds = std::size(kTestValues) - 1;

  base::SimpleTestTickClock tick_clock;
  RateCounter rate_counter(base::Seconds(kWindowSeconds), &tick_clock);
  EXPECT_EQ(0, rate_counter.Rate());

  double expected = 0.0;
  for (size_t i = 0; i < std::size(kTestValues); ++i) {
    tick_clock.Advance(base::Seconds(1));
    rate_counter.Record(kTestValues[i]);
    if (i != 0) {
      expected += kTestValues[i];
    }
  }
  expected /= kWindowSeconds;

  EXPECT_EQ(expected, rate_counter.Rate());
}

}  // namespace remoting
