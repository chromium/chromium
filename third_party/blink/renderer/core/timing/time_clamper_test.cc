// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/time_clamper.h"

#include "testing/gtest/include/gtest/gtest.h"

#include <cmath>

namespace blink {
namespace {
const double kInterval = TimeClamper::kResolutionSeconds;
}

TEST(TimeClamperTest, TimeStampsAreNonNegative) {
  TimeClamper clamper;
  EXPECT_GE(clamper.ClampTimeResolution(0), 0.f);
  EXPECT_GE(clamper.ClampTimeResolution(TimeClamper::kResolutionSeconds), 0.f);
}

TEST(TimeClamperTest, TimeStampsIncreaseByFixedAmount) {
  const double kEpsilon = 1e-10;
  TimeClamper clamper;
  double prev = clamper.ClampTimeResolution(0);
  for (double time_seconds = 0; time_seconds < kInterval * 100;
       time_seconds += kInterval * 0.1) {
    double clamped_time = clamper.ClampTimeResolution(time_seconds);
    double delta = clamped_time - prev;
    ASSERT_GE(delta, 0);
    if (delta > kEpsilon) {
      ASSERT_TRUE(std::fabs(delta - kInterval) < kEpsilon);
      prev = clamped_time;
    }
  }
}

TEST(TimeClamperTest, ClampingIsConsistent) {
  TimeClamper clamper;
  for (double time_seconds = 0; time_seconds < kInterval * 100;
       time_seconds += kInterval * 0.1) {
    double t1 = clamper.ClampTimeResolution(time_seconds);
    double t2 = clamper.ClampTimeResolution(time_seconds);
    EXPECT_EQ(t1, t2);
  }
}

TEST(TimeClamperTest, ClampingNegativeNumbersIsConsistent) {
  TimeClamper clamper;
  for (double time_seconds = -kInterval * 100; time_seconds <= 0;
       time_seconds += kInterval * 0.1) {
    double t1 = clamper.ClampTimeResolution(time_seconds);
    double t2 = clamper.ClampTimeResolution(time_seconds);
    EXPECT_EQ(t1, t2);
  }
}

TEST(TimeClamperTest, ClampingIsPerInstance) {
  const double kEpsilon = 1e-10;
  TimeClamper clamper1;
  TimeClamper clamper2;
  double time_seconds = 0;
  while (true) {
    if (std::fabs(clamper1.ClampTimeResolution(time_seconds) -
                  clamper2.ClampTimeResolution(time_seconds)) > kEpsilon) {
      break;
    }
    time_seconds += kInterval;
  }
}

TEST(TimeClamperTest, ClampingIsUniform) {
  const int kBuckets = 8;
  const int kSampleCount = 10000;
  const double kEpsilon = 1e-10;
  const double kTimeStep = kInterval / kBuckets;
  double time_seconds = 299792.458;
  int histogram[kBuckets] = {0};
  TimeClamper clamper;

  // This test ensures the jitter thresholds are approximately uniformly
  // distributed inside the clamping intervals. It samples individual intervals
  // to detect where the threshold is and counts the number of steps taken.
  for (int i = 0; i < kSampleCount; i++) {
    double start = clamper.ClampTimeResolution(time_seconds);
    for (int step = 0; step < kBuckets; step++) {
      time_seconds += kTimeStep;
      if (std::abs(clamper.ClampTimeResolution(time_seconds) - start) >
          kEpsilon) {
        histogram[step]++;
        // Skip to the next interval to make sure each measurement is
        // independent.
        time_seconds = floor(time_seconds / kInterval) * kInterval + kInterval;
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

}  // namespace blink
