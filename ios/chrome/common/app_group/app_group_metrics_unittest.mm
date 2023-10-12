// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/app_group/app_group_metrics_mainapp.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using app_group::KeyForOpenExtensionOutcomeType;
using app_group::kOpenExtensionOutcomeFailureInvalidURL;
using app_group::kOpenExtensionOutcomeFailureOpenInNotFound;
using app_group::kOpenExtensionOutcomeFailureUnsupportedScheme;
using app_group::kOpenExtensionOutcomeFailureURLNotFound;
using app_group::kOpenExtensionOutcomes;
using app_group::kOpenExtensionOutcomeSuccess;
using app_group::OpenExtensionOutcome;
using app_group::OutcomeTypeFromKey;
using app_group::main_app::LogOpenExtensionMetrics;

class AppGroupMetricsTest : public PlatformTest {
 protected:
  void TearDown() override { PlatformTest::TearDown(); }
};

// Verify that the input key is converted correctly to OpenExtensionOutcome.
TEST_F(AppGroupMetricsTest, TestKeyForOpenExtensionOutcomeType) {
  EXPECT_EQ(KeyForOpenExtensionOutcomeType(OpenExtensionOutcome::kSuccess),
            kOpenExtensionOutcomeSuccess);

  EXPECT_EQ(
      KeyForOpenExtensionOutcomeType(OpenExtensionOutcome::kFailureInvalidURL),
      kOpenExtensionOutcomeFailureInvalidURL);

  EXPECT_EQ(
      KeyForOpenExtensionOutcomeType(OpenExtensionOutcome::kFailureURLNotFound),
      kOpenExtensionOutcomeFailureURLNotFound);

  EXPECT_EQ(KeyForOpenExtensionOutcomeType(
                OpenExtensionOutcome::kFailureOpenInNotFound),
            kOpenExtensionOutcomeFailureOpenInNotFound);

  EXPECT_EQ(KeyForOpenExtensionOutcomeType(
                OpenExtensionOutcome::kFailureUnsupportedScheme),
            kOpenExtensionOutcomeFailureUnsupportedScheme);
}

// Verify that the OpenExtensionOutcome is converted correctly to
// OpenExtensionOutcome enum key.
TEST_F(AppGroupMetricsTest, TestOutcomeTypeFromKey) {
  EXPECT_EQ(OutcomeTypeFromKey(kOpenExtensionOutcomeSuccess),
            OpenExtensionOutcome::kSuccess);

  EXPECT_EQ(OutcomeTypeFromKey(kOpenExtensionOutcomeFailureInvalidURL),
            OpenExtensionOutcome::kFailureInvalidURL);

  EXPECT_EQ(OutcomeTypeFromKey(kOpenExtensionOutcomeFailureURLNotFound),
            OpenExtensionOutcome::kFailureURLNotFound);

  EXPECT_EQ(OutcomeTypeFromKey(kOpenExtensionOutcomeFailureOpenInNotFound),
            OpenExtensionOutcome::kFailureOpenInNotFound);

  EXPECT_EQ(OutcomeTypeFromKey(kOpenExtensionOutcomeFailureUnsupportedScheme),
            OpenExtensionOutcome::kFailureUnsupportedScheme);

  EXPECT_EQ(OutcomeTypeFromKey(@"OpenExtensionOutcomeURL"),
            OpenExtensionOutcome::kInvalid);
}

// Verify that the open extension metrics are logged correctly and
// cleaned from the UserDefaults.
TEST_F(AppGroupMetricsTest, TestLogOpenExtensionMetrics) {
  base::HistogramTester histogram_tester;

  NSDictionary<NSString*, NSNumber*>* open_extension_test_dictionnary = @{
    kOpenExtensionOutcomeSuccess : @1,
    kOpenExtensionOutcomeFailureInvalidURL : @2,
    kOpenExtensionOutcomeFailureURLNotFound : @3,
    kOpenExtensionOutcomeFailureOpenInNotFound : @4,
    kOpenExtensionOutcomeFailureUnsupportedScheme : @5,
    @"InvalidString" : @6
  };

  NSUserDefaults* shared_defaults = app_group::GetGroupUserDefaults();

  [shared_defaults setObject:open_extension_test_dictionnary
                      forKey:kOpenExtensionOutcomes];

  LogOpenExtensionMetrics();

  // Verify that the total of events is registered.
  histogram_tester.ExpectTotalCount("IOSOpenExtensionOutcome", 21);

  // Verify that the total of each bucket contains the right number of events.
  histogram_tester.ExpectBucketCount("IOSOpenExtensionOutcome",
                                     OpenExtensionOutcome::kSuccess, 1);
  histogram_tester.ExpectBucketCount(
      "IOSOpenExtensionOutcome", OpenExtensionOutcome::kFailureInvalidURL, 2);
  histogram_tester.ExpectBucketCount(
      "IOSOpenExtensionOutcome", OpenExtensionOutcome::kFailureURLNotFound, 3);
  histogram_tester.ExpectBucketCount(
      "IOSOpenExtensionOutcome", OpenExtensionOutcome::kFailureOpenInNotFound,
      4);
  histogram_tester.ExpectBucketCount(
      "IOSOpenExtensionOutcome",
      OpenExtensionOutcome::kFailureUnsupportedScheme, 5);
  histogram_tester.ExpectBucketCount("IOSOpenExtensionOutcome",
                                     OpenExtensionOutcome::kInvalid, 6);

  EXPECT_FALSE([shared_defaults objectForKey:kOpenExtensionOutcomes]);
}

// Verify dictionary values cases for NSNumber and
// that the total of each bucket is less/equal 10.
TEST_F(AppGroupMetricsTest, TestValuesCasesForLogOpenExtensionMetrics) {
  base::HistogramTester histogram_tester;
  // NSNumber Test cases
  // 1 bucket with value 100
  // 1 bucket with negative value
  // 1 bucket with decimal value
  // 1 bucket with [NSDecimalNumber notANumber]
  // 1 bucket with something that is not a number (string)*/
  NSDictionary* open_extension_test_dictionnary = @{
    kOpenExtensionOutcomeSuccess : @100,
    kOpenExtensionOutcomeFailureInvalidURL : @-6,
    kOpenExtensionOutcomeFailureURLNotFound : @3.2,
    kOpenExtensionOutcomeFailureOpenInNotFound : NSDecimalNumber.notANumber,
    kOpenExtensionOutcomeFailureUnsupportedScheme : @"str",
    @"InvalidString" : @10
  };

  NSUserDefaults* shared_defaults = app_group::GetGroupUserDefaults();

  [shared_defaults setObject:open_extension_test_dictionnary
                      forKey:kOpenExtensionOutcomes];

  LogOpenExtensionMetrics();

  // Verify that the total of events is registered.
  histogram_tester.ExpectTotalCount("IOSOpenExtensionOutcome", 23);

  // Verify that the total of each bucket contains the right number of events.
  histogram_tester.ExpectBucketCount("IOSOpenExtensionOutcome",
                                     OpenExtensionOutcome::kSuccess, 10);
  histogram_tester.ExpectBucketCount(
      "IOSOpenExtensionOutcome", OpenExtensionOutcome::kFailureInvalidURL, 0);
  histogram_tester.ExpectBucketCount(
      "IOSOpenExtensionOutcome", OpenExtensionOutcome::kFailureURLNotFound, 3);
  histogram_tester.ExpectBucketCount(
      "IOSOpenExtensionOutcome", OpenExtensionOutcome::kFailureOpenInNotFound,
      0);
  histogram_tester.ExpectBucketCount(
      "IOSOpenExtensionOutcome",
      OpenExtensionOutcome::kFailureUnsupportedScheme, 0);
  histogram_tester.ExpectBucketCount("IOSOpenExtensionOutcome",
                                     OpenExtensionOutcome::kInvalid, 10);

  EXPECT_FALSE([shared_defaults objectForKey:kOpenExtensionOutcomes]);
}

// Verify the case of when there's no dictionary.
TEST_F(AppGroupMetricsTest, TestLogOpenExtensionMetricsMissingDictionary) {
  base::HistogramTester histogram_tester;

  LogOpenExtensionMetrics();

  histogram_tester.ExpectTotalCount("IOSOpenExtensionOutcome", 0);
}

// Verify that the open extension metrics are logged correctly with
// missing buckets and cleaned from the UserDefaults.
TEST_F(AppGroupMetricsTest, TestLogOpenExtensionMetricsWithMissingBuckets) {
  base::HistogramTester histogram_tester;

  NSDictionary<NSString*, NSNumber*>* open_extension_test_dictionnary = @{
    kOpenExtensionOutcomeSuccess : @5,
    kOpenExtensionOutcomeFailureInvalidURL : @10,
    kOpenExtensionOutcomeFailureURLNotFound : @3,
  };

  NSUserDefaults* shared_defaults = app_group::GetGroupUserDefaults();

  [shared_defaults setObject:open_extension_test_dictionnary
                      forKey:kOpenExtensionOutcomes];

  LogOpenExtensionMetrics();

  histogram_tester.ExpectTotalCount("IOSOpenExtensionOutcome", 18);

  EXPECT_FALSE([shared_defaults objectForKey:kOpenExtensionOutcomes]);
}
