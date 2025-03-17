// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/time_clamper.h"

#include <array>
#include <cmath>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {
const int64_t kIntervalInMicroseconds =
    TimeClamper::kFineResolutionMicroseconds;
}

class TimeClamperTest : public testing::Test {
 protected:
  test::TaskEnvironment task_environment_;
};

TEST_F(TimeClamperTest, TimeStampsAreNonNegative) {
  TimeClamper clamper;
  EXPECT_GE(
      clamper.ClampTimeResolution(base::TimeDelta(), true).InMicroseconds(),
      0.f);
  EXPECT_GE(
      clamper
          .ClampTimeResolution(
              base::Microseconds(TimeClamper::kFineResolutionMicroseconds),
              true)
          .InMicroseconds(),
      0.f);
}

TEST_F(TimeClamperTest, TimeStampsIncreaseByFixedAmount) {
  TimeClamper clamper;
  int64_t prev =
      clamper.ClampTimeResolution(base::TimeDelta(), true).InMicroseconds();
  for (int64_t time_microseconds = 0;
       time_microseconds < kIntervalInMicroseconds * 100;
       time_microseconds += 1) {
    int64_t clamped_time =
        clamper.ClampTimeResolution(base::Microseconds(time_microseconds), true)
            .InMicroseconds();
    int64_t delta = clamped_time - prev;
    ASSERT_GE(delta, 0);
    if (delta >= 1) {
      ASSERT_EQ(delta, kIntervalInMicroseconds);
      prev = clamped_time;
    }
  }
}

TEST_F(TimeClamperTest, ClampingIsDeterministic) {
  TimeClamper clamper;
  for (int64_t time_microseconds = 0;
       time_microseconds < kIntervalInMicroseconds * 100;
       time_microseconds += 1) {
    int64_t t1 =
        clamper.ClampTimeResolution(base::Microseconds(time_microseconds), true)
            .InMicroseconds();
    int64_t t2 =
        clamper.ClampTimeResolution(base::Microseconds(time_microseconds), true)
            .InMicroseconds();
    EXPECT_EQ(t1, t2);
  }
}

TEST_F(TimeClamperTest, ClampingNegativeNumbersIsConsistent) {
  TimeClamper clamper;
  for (int64_t time_microseconds = -kIntervalInMicroseconds * 100;
       time_microseconds < kIntervalInMicroseconds * 100;
       time_microseconds += 1) {
    int64_t t1 =
        clamper.ClampTimeResolution(base::Microseconds(time_microseconds), true)
            .InMicroseconds();
    int64_t t2 =
        clamper.ClampTimeResolution(base::Microseconds(time_microseconds), true)
            .InMicroseconds();
    EXPECT_EQ(t1, t2);
  }
}

TEST_F(TimeClamperTest, ClampingIsPerInstance) {
  TimeClamper clamper1;
  TimeClamper clamper2;
  int64_t time_microseconds = kIntervalInMicroseconds / 2;
  while (true) {
    if (std::abs(clamper1
                     .ClampTimeResolution(base::Microseconds(time_microseconds),
                                          true)
                     .InMicroseconds() -
                 clamper2
                     .ClampTimeResolution(base::Microseconds(time_microseconds),
                                          true)
                     .InMicroseconds()) >= 1) {
      break;
    }
    time_microseconds += kIntervalInMicroseconds;
  }
}

TEST_F(TimeClamperTest, OverflowHandling) {
  // Tests that when starting with base::TimeDelta::Min(), the
  // smallest possible value, we won't overflow.
  for (bool cross_origin : {true, false}) {
    for (int ii = 0; ii < 10000; ++ii) {  // For testing TimeClamper::secret_.
      TimeClamper clamper;
      int64_t clamped =
          clamper.ClampTimeResolution(base::TimeDelta::Min(), cross_origin)
              .InMicroseconds();
      ASSERT_LT(clamped, 0);  // Should still be negative.
      // Can't be smaller than the smallest allowed time.
      ASSERT_GE(clamped, base::TimeDelta::Min().InMicroseconds());
    }
  }
}

void UniformityTest(int64_t time_microseconds,
                    int interval,
                    bool cross_origin_isolated_capability) {
  // Number of buckets should be a divisor of the tested intervals.
  const int kBuckets = 5;
  const int kSampleCount = 10000;
  const int kTimeStep = interval / kBuckets;
  std::array<int, kBuckets> histogram = {};
  TimeClamper clamper;

  // This test ensures the jitter thresholds are approximately uniformly
  // distributed inside the clamping intervals. It samples individual intervals
  // to detect where the threshold is and counts the number of steps taken.
  for (int i = 0; i < kSampleCount; i++) {
    int64_t start =
        clamper
            .ClampTimeResolution(base::Microseconds(time_microseconds),
                                 cross_origin_isolated_capability)
            .InMicroseconds();
    for (int step = 0; step < kBuckets; step++) {
      time_microseconds += kTimeStep;
      if (std::abs(
              clamper
                  .ClampTimeResolution(base::Microseconds(time_microseconds),
                                       cross_origin_isolated_capability)
                  .InMicroseconds() -
              start) >= 1) {
        histogram[step]++;
        // Skip to the next interval to make sure each measurement is
        // independent.
        time_microseconds =
            floor(time_microseconds / interval) * interval + interval;
        break;
      }
    }
  }

  double expected_count = kSampleCount / kBuckets;
  double chi_squared = 0;
  for (int i = 0; i < kBuckets; ++i) {
    double difference = histogram[i] - expected_count;
    chi_squared += difference * difference / expected_count;
  }
  // P-value for a 0.001 significance level with 7 degrees of freedom.
  EXPECT_LT(chi_squared, 24.322);
}

TEST_F(TimeClamperTest, ClampingIsUniform) {
  UniformityTest(299792458238, 5, true);
  UniformityTest(29979245823800, 5, true);
  UniformityTest(1616533323846260, 5, true);
  UniformityTest(299792458238, 100, false);
  UniformityTest(29979245823800, 100, false);
  UniformityTest(1616533323846260, 100, false);
}

}  // namespace blink
