// Copyright 2018 The Chromium Authors
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

TEST(MetricsUtilsTest, GetSemanticBucketMinForDurationTiming) {
  // Per-ms bucketing (until 10ms)
  EXPECT_EQ(3, ukm::GetSemanticBucketMinForDurationTiming(3));
  EXPECT_EQ(9, ukm::GetSemanticBucketMinForDurationTiming(9));

  // Per-10ms bucketing (until 100ms)
  EXPECT_EQ(10, ukm::GetSemanticBucketMinForDurationTiming(11));
  EXPECT_EQ(70, ukm::GetSemanticBucketMinForDurationTiming(73));
  EXPECT_EQ(90, ukm::GetSemanticBucketMinForDurationTiming(99));

  // Per-100ms bucketing (until 5s)
  EXPECT_EQ(100, ukm::GetSemanticBucketMinForDurationTiming(101));
  EXPECT_EQ(800, ukm::GetSemanticBucketMinForDurationTiming(899));
  EXPECT_EQ(4900, ukm::GetSemanticBucketMinForDurationTiming(4999));

  // Per-second bucketing (until 20s))
  EXPECT_EQ(1000, ukm::GetSemanticBucketMinForDurationTiming(1001));
  EXPECT_EQ(6000, ukm::GetSemanticBucketMinForDurationTiming(6973));
  EXPECT_EQ(19000, ukm::GetSemanticBucketMinForDurationTiming(19999));

  // Per-10s bucketing (until 1 minute)
  EXPECT_EQ(20000, ukm::GetSemanticBucketMinForDurationTiming(20001));
  EXPECT_EQ(40000, ukm::GetSemanticBucketMinForDurationTiming(48731));
  EXPECT_EQ(50000, ukm::GetSemanticBucketMinForDurationTiming(59999));

  // Per-minute up to 10 minutes bucketing
  EXPECT_EQ(60000, ukm::GetSemanticBucketMinForDurationTiming(60001));
  EXPECT_EQ(420000, ukm::GetSemanticBucketMinForDurationTiming(476532));
  EXPECT_EQ(540000, ukm::GetSemanticBucketMinForDurationTiming(599999));

  // Per ten-minute up to 1 hour bucketing
  EXPECT_EQ(600000, ukm::GetSemanticBucketMinForDurationTiming(600001));
  EXPECT_EQ(2400000, ukm::GetSemanticBucketMinForDurationTiming(2787923));
  EXPECT_EQ(3000000, ukm::GetSemanticBucketMinForDurationTiming(3599999));

  // Per hour up to 1 day bucketing
  EXPECT_EQ(3600000, ukm::GetSemanticBucketMinForDurationTiming(3600001));
  EXPECT_EQ(7200000, ukm::GetSemanticBucketMinForDurationTiming(9101234));
  EXPECT_EQ(82800000, ukm::GetSemanticBucketMinForDurationTiming(86399999));

  // Exponentional after 1 day bucketing.
  EXPECT_EQ(86400000, ukm::GetSemanticBucketMinForDurationTiming(86400001));
  // 22 days, should bucket to 16 days.
  EXPECT_EQ(1382400000, ukm::GetSemanticBucketMinForDurationTiming(1900856300));
  // 62 days, should bucket to 32 days.
  EXPECT_EQ(2764800000, ukm::GetSemanticBucketMinForDurationTiming(2764805612));
}