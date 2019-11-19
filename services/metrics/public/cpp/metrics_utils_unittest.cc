// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/metrics/public/cpp/metrics_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(MetricsUtilsTest, GetLinearBucketMin) {
  struct {
    int64_t expected_result;
    int64_t sample;
    int32_t bucket_size;
  } int_test_cases[] = {
      // Typical positive cases.
      {35, 38, 5},
      {50, 51, 50},
      {50, 99, 50},
      {20, 25, 10},
      // Negative samples.
      {-50, -45, 10},
      {-50, -48, 10},
      {-50, -41, 10},
      {-42, -41, 2},
      // Zero samples.
      {0, 0, 1},
      {0, 0, 10},
  };

  struct {
    int64_t expected_result;
    double sample;
    int32_t bucket_size;
  } double_test_cases[] = {
      // Typical positive cases.
      {35, 38.0, 5},
      {50, 50.5, 50},
      {50, 99.5, 50},
      {20, 25.0, 10},
      // Negative samples.
      {-50, -45.0, 10},
      {-42, -41.2, 2},
      {-42, -40.8, 2},
      // Test that a double close to the next bucker never rounds up.
      {5, 9.95, 5},
  };

  // Test int64_t sample cases.
  for (const auto& test : int_test_cases) {
    EXPECT_EQ(test.expected_result,
              ukm::GetLinearBucketMin(test.sample, test.bucket_size))
        << "For sample: " << test.sample
        << " with bucket_size: " << test.bucket_size;
  }

  // Test double sample cases.
  for (const auto& test : double_test_cases) {
    EXPECT_EQ(test.expected_result,
              ukm::GetLinearBucketMin(test.sample, test.bucket_size))
        << "For sample: " << test.sample
        << " with bucket_size: " << test.bucket_size;
  }
}

TEST(MetricsUtilsTest, GetExponentialBucketMinForUserTiming) {
  struct {
    int64_t expected_result;
    int64_t sample;
  } int_test_cases[] = {
      // Typical positive cases.
      {1, 1},
      {32, 38},
      {32, 51},
      {64, 99},
      {16, 25},
      {512, 1023},
      {1024, 1024},
      {1024, 1025},
      // Negative samples.
      {0, -45},
      // Zero samples.
      {0, 0},
  };

  // Test int64_t sample cases.
  for (const auto& test : int_test_cases) {
    EXPECT_EQ(test.expected_result,
              ukm::GetExponentialBucketMinForUserTiming(test.sample))
        << "For sample: " << test.sample;
  }
}
