// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/model/default_status/default_status_helper_metrics.h"

#import "base/test/metrics/histogram_tester.h"
#import "ios/chrome/browser/default_browser/model/default_status/default_status_helper_types.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "testing/platform_test.h"

namespace default_status {

// Storage key for recording the last time an http link was opened via Chrome,
// which indicates that it's set as default browser.
NSString* const kLastHTTPURLOpenTime = @"lastHTTPURLOpenTime";

// Test fixture for testing the default status metrics.
class DefaultStatusHelperMetricsTest : public PlatformTest {};

// Tests that the heuristic assessment returns the correct value.
TEST_F(DefaultStatusHelperMetricsTest, HeuristicAssessment) {
  EXPECT_EQ(internal::AssessHeuristic(/*system_is_default=*/false,
                                      /*heuristic_is_default=*/false),
            DefaultStatusHeuristicAssessment::kTrueNegative);
  EXPECT_EQ(internal::AssessHeuristic(/*system_is_default=*/false,
                                      /*heuristic_is_default=*/true),
            DefaultStatusHeuristicAssessment::kFalsePositive);
  EXPECT_EQ(internal::AssessHeuristic(/*system_is_default=*/true,
                                      /*heuristic_is_default=*/false),
            DefaultStatusHeuristicAssessment::kFalseNegative);
  EXPECT_EQ(internal::AssessHeuristic(/*system_is_default=*/true,
                                      /*heuristic_is_default=*/true),
            DefaultStatusHeuristicAssessment::kTruePositive);
}

// Tests that the RecordCooldownErrorDaysLeft function records the correct
// values.
TEST_F(DefaultStatusHelperMetricsTest, RecordCooldownErrorDaysLeft) {
  base::HistogramTester histogram_tester;

  RecordCooldownErrorDaysLeft(15);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.CooldownError.DaysLeft", 15, 1);

  RecordCooldownErrorDaysLeft(1005);
  RecordCooldownErrorDaysLeft(1105);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.CooldownError.DaysLeft", 1000, 2);

  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.CooldownError.DaysLeft", 3);
}

// Tests that the RecordDaysSinceLastSuccessfulCall function records the
// correct values.
TEST_F(DefaultStatusHelperMetricsTest, RecordDaysSinceLastSuccessfulCall) {
  base::HistogramTester histogram_tester;

  RecordDaysSinceLastSuccessfulCall(15);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.DaysSinceLastSuccessfulCall", 15, 1);

  RecordDaysSinceLastSuccessfulCall(1005);
  RecordDaysSinceLastSuccessfulCall(1105);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.DaysSinceLastSuccessfulCall", 1000, 2);

  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.DaysSinceLastSuccessfulCall", 3);
}

// Tests that the RecordDefaultStatusAPIOutcomeType function records the
// correct values.
TEST_F(DefaultStatusHelperMetricsTest, RecordDefaultStatusAPIOutcomeType) {
  base::HistogramTester histogram_tester;

  RecordDefaultStatusAPIOutcomeType(DefaultStatusAPIOutcomeType::kSuccess);
  histogram_tester.ExpectBucketCount("IOS.DefaultStatusAPI.OutcomeType",
                                     DefaultStatusAPIOutcomeType::kSuccess, 1);

  RecordDefaultStatusAPIOutcomeType(
      DefaultStatusAPIOutcomeType::kCooldownError);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.OutcomeType",
      DefaultStatusAPIOutcomeType::kCooldownError, 1);

  RecordDefaultStatusAPIOutcomeType(DefaultStatusAPIOutcomeType::kOtherError);
  histogram_tester.ExpectBucketCount("IOS.DefaultStatusAPI.OutcomeType",
                                     DefaultStatusAPIOutcomeType::kOtherError,
                                     1);

  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.OutcomeType", 3);
}

// Tests that the RecordDefaultStatusAPIResult function records the correct
// values.
TEST_F(DefaultStatusHelperMetricsTest, RecordDefaultStatusAPIResult) {
  base::HistogramTester histogram_tester;

  // Cohort 1.
  RecordDefaultStatusAPIResult(/*is_default=*/true, /*cohort_number=*/1,
                               /*is_within_cohort_window=*/true);
  histogram_tester.ExpectBucketCount("IOS.DefaultStatusAPI.IsDefaultBrowser",
                                     true, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.StrictCohorts", true, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort1", true, 1);

  RecordDefaultStatusAPIResult(/*is_default=*/false, /*cohort_number=*/1,
                               /*is_within_cohort_window=*/true);
  histogram_tester.ExpectBucketCount("IOS.DefaultStatusAPI.IsDefaultBrowser",
                                     false, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.StrictCohorts", false, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort1", false, 1);

  RecordDefaultStatusAPIResult(/*is_default=*/true, /*cohort_number=*/1,
                               /*is_within_cohort_window=*/false);
  histogram_tester.ExpectBucketCount("IOS.DefaultStatusAPI.IsDefaultBrowser",
                                     true, 2);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort1", true, 2);

  RecordDefaultStatusAPIResult(/*is_default=*/false, /*cohort_number=*/1,
                               /*is_within_cohort_window=*/false);
  histogram_tester.ExpectBucketCount("IOS.DefaultStatusAPI.IsDefaultBrowser",
                                     false, 2);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort1", false, 2);

  // Cohort 2.
  RecordDefaultStatusAPIResult(/*is_default=*/true, /*cohort_number=*/2,
                               /*is_within_cohort_window=*/true);
  histogram_tester.ExpectBucketCount("IOS.DefaultStatusAPI.IsDefaultBrowser",
                                     true, 3);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.StrictCohorts", true, 2);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort2", true, 1);

  RecordDefaultStatusAPIResult(/*is_default=*/false, /*cohort_number=*/2,
                               /*is_within_cohort_window=*/true);
  histogram_tester.ExpectBucketCount("IOS.DefaultStatusAPI.IsDefaultBrowser",
                                     false, 3);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.StrictCohorts", false, 2);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort2", false, 1);

  RecordDefaultStatusAPIResult(/*is_default=*/true, /*cohort_number=*/2,
                               /*is_within_cohort_window=*/false);
  histogram_tester.ExpectBucketCount("IOS.DefaultStatusAPI.IsDefaultBrowser",
                                     true, 4);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort2", true, 2);

  RecordDefaultStatusAPIResult(/*is_default=*/false, /*cohort_number=*/2,
                               /*is_within_cohort_window=*/false);
  histogram_tester.ExpectBucketCount("IOS.DefaultStatusAPI.IsDefaultBrowser",
                                     false, 4);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort2", false, 2);

  // Cohort 3.
  RecordDefaultStatusAPIResult(/*is_default=*/true, /*cohort_number=*/3,
                               /*is_within_cohort_window=*/true);
  histogram_tester.ExpectBucketCount("IOS.DefaultStatusAPI.IsDefaultBrowser",
                                     true, 5);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.StrictCohorts", true, 3);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort3", true, 1);

  RecordDefaultStatusAPIResult(/*is_default=*/false, /*cohort_number=*/3,
                               /*is_within_cohort_window=*/true);
  histogram_tester.ExpectBucketCount("IOS.DefaultStatusAPI.IsDefaultBrowser",
                                     false, 5);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.StrictCohorts", false, 3);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort3", false, 1);

  RecordDefaultStatusAPIResult(/*is_default=*/true, /*cohort_number=*/3,
                               /*is_within_cohort_window=*/false);
  histogram_tester.ExpectBucketCount("IOS.DefaultStatusAPI.IsDefaultBrowser",
                                     true, 6);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort3", true, 2);

  RecordDefaultStatusAPIResult(/*is_default=*/false, /*cohort_number=*/3,
                               /*is_within_cohort_window=*/false);
  histogram_tester.ExpectBucketCount("IOS.DefaultStatusAPI.IsDefaultBrowser",
                                     false, 6);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort3", false, 2);

  // Cohort 4.
  RecordDefaultStatusAPIResult(/*is_default=*/true, /*cohort_number=*/4,
                               /*is_within_cohort_window=*/true);
  histogram_tester.ExpectBucketCount("IOS.DefaultStatusAPI.IsDefaultBrowser",
                                     true, 7);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.StrictCohorts", true, 4);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort4", true, 1);

  RecordDefaultStatusAPIResult(/*is_default=*/false, /*cohort_number=*/4,
                               /*is_within_cohort_window=*/true);
  histogram_tester.ExpectBucketCount("IOS.DefaultStatusAPI.IsDefaultBrowser",
                                     false, 7);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.StrictCohorts", false, 4);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort4", false, 1);

  RecordDefaultStatusAPIResult(/*is_default=*/true, /*cohort_number=*/4,
                               /*is_within_cohort_window=*/false);
  histogram_tester.ExpectBucketCount("IOS.DefaultStatusAPI.IsDefaultBrowser",
                                     true, 8);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort4", true, 2);

  RecordDefaultStatusAPIResult(/*is_default=*/false, /*cohort_number=*/4,
                               /*is_within_cohort_window=*/false);
  histogram_tester.ExpectBucketCount("IOS.DefaultStatusAPI.IsDefaultBrowser",
                                     false, 8);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort4", false, 2);

  // Summary.
  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.IsDefaultBrowser",
                                    16);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.StrictCohorts", 8);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort1", 4);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort2", 4);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort3", 4);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort4", 4);
}

// Tests that the RecordDefaultStatusRetention function records the correct
// values.
TEST_F(DefaultStatusHelperMetricsTest, RecordDefaultStatusRetention) {
  base::HistogramTester histogram_tester;

  RecordDefaultStatusRetention(DefaultStatusRetention::kBecameDefault);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.DefaultStatusRetention",
      DefaultStatusRetention::kBecameDefault, 1);

  RecordDefaultStatusRetention(DefaultStatusRetention::kBecameNonDefault);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.DefaultStatusRetention",
      DefaultStatusRetention::kBecameNonDefault, 1);

  RecordDefaultStatusRetention(DefaultStatusRetention::kRemainedDefault);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.DefaultStatusRetention",
      DefaultStatusRetention::kRemainedDefault, 1);

  RecordDefaultStatusRetention(DefaultStatusRetention::kRemainedNonDefault);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.DefaultStatusRetention",
      DefaultStatusRetention::kRemainedNonDefault, 1);

  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.DefaultStatusRetention", 4);
}

// Tests that the RecordHeuristicAssessments function records the correct
// values.
TEST_F(DefaultStatusHelperMetricsTest, RecordHeuristicAssessments) {
  base::HistogramTester histogram_tester;

  NSDate* under_one_day_ago =
      (base::Time::Now() - base::Days(1) + base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, under_one_day_ago);
  RecordHeuristicAssessments(/*is_default_in_system_api=*/true);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment1",
      DefaultStatusHeuristicAssessment::kTruePositive, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment3",
      DefaultStatusHeuristicAssessment::kTruePositive, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment7",
      DefaultStatusHeuristicAssessment::kTruePositive, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment14",
      DefaultStatusHeuristicAssessment::kTruePositive, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment21",
      DefaultStatusHeuristicAssessment::kTruePositive, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment28",
      DefaultStatusHeuristicAssessment::kTruePositive, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment35",
      DefaultStatusHeuristicAssessment::kTruePositive, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment42",
      DefaultStatusHeuristicAssessment::kTruePositive, 1);
  RecordHeuristicAssessments(/*is_default_in_system_api=*/false);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment1",
      DefaultStatusHeuristicAssessment::kFalsePositive, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment3",
      DefaultStatusHeuristicAssessment::kFalsePositive, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment7",
      DefaultStatusHeuristicAssessment::kFalsePositive, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment14",
      DefaultStatusHeuristicAssessment::kFalsePositive, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment21",
      DefaultStatusHeuristicAssessment::kFalsePositive, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment28",
      DefaultStatusHeuristicAssessment::kFalsePositive, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment35",
      DefaultStatusHeuristicAssessment::kFalsePositive, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment42",
      DefaultStatusHeuristicAssessment::kFalsePositive, 1);

  NSDate* under_three_days_ago =
      (base::Time::Now() - base::Days(3) + base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, under_three_days_ago);
  RecordHeuristicAssessments(/*is_default_in_system_api=*/true);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment1",
      DefaultStatusHeuristicAssessment::kFalseNegative, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment3",
      DefaultStatusHeuristicAssessment::kTruePositive, 2);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment7",
      DefaultStatusHeuristicAssessment::kTruePositive, 2);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment14",
      DefaultStatusHeuristicAssessment::kTruePositive, 2);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment21",
      DefaultStatusHeuristicAssessment::kTruePositive, 2);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment28",
      DefaultStatusHeuristicAssessment::kTruePositive, 2);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment35",
      DefaultStatusHeuristicAssessment::kTruePositive, 2);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment42",
      DefaultStatusHeuristicAssessment::kTruePositive, 2);
  RecordHeuristicAssessments(/*is_default_in_system_api=*/false);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment1",
      DefaultStatusHeuristicAssessment::kTrueNegative, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment3",
      DefaultStatusHeuristicAssessment::kFalsePositive, 2);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment7",
      DefaultStatusHeuristicAssessment::kFalsePositive, 2);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment14",
      DefaultStatusHeuristicAssessment::kFalsePositive, 2);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment21",
      DefaultStatusHeuristicAssessment::kFalsePositive, 2);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment28",
      DefaultStatusHeuristicAssessment::kFalsePositive, 2);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment35",
      DefaultStatusHeuristicAssessment::kFalsePositive, 2);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment42",
      DefaultStatusHeuristicAssessment::kFalsePositive, 2);

  NSDate* under_seven_days_ago =
      (base::Time::Now() - base::Days(7) + base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, under_seven_days_ago);
  RecordHeuristicAssessments(/*is_default_in_system_api=*/true);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment1",
      DefaultStatusHeuristicAssessment::kFalseNegative, 2);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment3",
      DefaultStatusHeuristicAssessment::kFalseNegative, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment7",
      DefaultStatusHeuristicAssessment::kTruePositive, 3);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment14",
      DefaultStatusHeuristicAssessment::kTruePositive, 3);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment21",
      DefaultStatusHeuristicAssessment::kTruePositive, 3);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment28",
      DefaultStatusHeuristicAssessment::kTruePositive, 3);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment35",
      DefaultStatusHeuristicAssessment::kTruePositive, 3);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment42",
      DefaultStatusHeuristicAssessment::kTruePositive, 3);
  RecordHeuristicAssessments(/*is_default_in_system_api=*/false);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment1",
      DefaultStatusHeuristicAssessment::kTrueNegative, 2);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment3",
      DefaultStatusHeuristicAssessment::kTrueNegative, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment7",
      DefaultStatusHeuristicAssessment::kFalsePositive, 3);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment14",
      DefaultStatusHeuristicAssessment::kFalsePositive, 3);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment21",
      DefaultStatusHeuristicAssessment::kFalsePositive, 3);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment28",
      DefaultStatusHeuristicAssessment::kFalsePositive, 3);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment35",
      DefaultStatusHeuristicAssessment::kFalsePositive, 3);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment42",
      DefaultStatusHeuristicAssessment::kFalsePositive, 3);

  NSDate* under_fourteen_days_ago =
      (base::Time::Now() - base::Days(14) + base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, under_fourteen_days_ago);
  RecordHeuristicAssessments(/*is_default_in_system_api=*/true);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment1",
      DefaultStatusHeuristicAssessment::kFalseNegative, 3);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment3",
      DefaultStatusHeuristicAssessment::kFalseNegative, 2);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment7",
      DefaultStatusHeuristicAssessment::kFalseNegative, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment14",
      DefaultStatusHeuristicAssessment::kTruePositive, 4);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment21",
      DefaultStatusHeuristicAssessment::kTruePositive, 4);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment28",
      DefaultStatusHeuristicAssessment::kTruePositive, 4);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment35",
      DefaultStatusHeuristicAssessment::kTruePositive, 4);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment42",
      DefaultStatusHeuristicAssessment::kTruePositive, 4);
  RecordHeuristicAssessments(/*is_default_in_system_api=*/false);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment1",
      DefaultStatusHeuristicAssessment::kTrueNegative, 3);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment3",
      DefaultStatusHeuristicAssessment::kTrueNegative, 2);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment7",
      DefaultStatusHeuristicAssessment::kTrueNegative, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment14",
      DefaultStatusHeuristicAssessment::kFalsePositive, 4);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment21",
      DefaultStatusHeuristicAssessment::kFalsePositive, 4);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment28",
      DefaultStatusHeuristicAssessment::kFalsePositive, 4);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment35",
      DefaultStatusHeuristicAssessment::kFalsePositive, 4);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment42",
      DefaultStatusHeuristicAssessment::kFalsePositive, 4);

  NSDate* under_twenty_one_days_ago =
      (base::Time::Now() - base::Days(21) + base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, under_twenty_one_days_ago);
  RecordHeuristicAssessments(/*is_default_in_system_api=*/true);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment1",
      DefaultStatusHeuristicAssessment::kFalseNegative, 4);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment3",
      DefaultStatusHeuristicAssessment::kFalseNegative, 3);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment7",
      DefaultStatusHeuristicAssessment::kFalseNegative, 2);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment14",
      DefaultStatusHeuristicAssessment::kFalseNegative, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment21",
      DefaultStatusHeuristicAssessment::kTruePositive, 5);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment28",
      DefaultStatusHeuristicAssessment::kTruePositive, 5);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment35",
      DefaultStatusHeuristicAssessment::kTruePositive, 5);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment42",
      DefaultStatusHeuristicAssessment::kTruePositive, 5);
  RecordHeuristicAssessments(/*is_default_in_system_api=*/false);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment1",
      DefaultStatusHeuristicAssessment::kTrueNegative, 4);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment3",
      DefaultStatusHeuristicAssessment::kTrueNegative, 3);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment7",
      DefaultStatusHeuristicAssessment::kTrueNegative, 2);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment14",
      DefaultStatusHeuristicAssessment::kTrueNegative, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment21",
      DefaultStatusHeuristicAssessment::kFalsePositive, 5);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment28",
      DefaultStatusHeuristicAssessment::kFalsePositive, 5);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment35",
      DefaultStatusHeuristicAssessment::kFalsePositive, 5);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment42",
      DefaultStatusHeuristicAssessment::kFalsePositive, 5);

  NSDate* under_twenty_eight_days_ago =
      (base::Time::Now() - base::Days(28) + base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, under_twenty_eight_days_ago);
  RecordHeuristicAssessments(/*is_default_in_system_api=*/true);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment1",
      DefaultStatusHeuristicAssessment::kFalseNegative, 5);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment3",
      DefaultStatusHeuristicAssessment::kFalseNegative, 4);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment7",
      DefaultStatusHeuristicAssessment::kFalseNegative, 3);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment14",
      DefaultStatusHeuristicAssessment::kFalseNegative, 2);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment21",
      DefaultStatusHeuristicAssessment::kFalseNegative, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment28",
      DefaultStatusHeuristicAssessment::kTruePositive, 6);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment35",
      DefaultStatusHeuristicAssessment::kTruePositive, 6);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment42",
      DefaultStatusHeuristicAssessment::kTruePositive, 6);
  RecordHeuristicAssessments(/*is_default_in_system_api=*/false);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment1",
      DefaultStatusHeuristicAssessment::kTrueNegative, 5);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment3",
      DefaultStatusHeuristicAssessment::kTrueNegative, 4);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment7",
      DefaultStatusHeuristicAssessment::kTrueNegative, 3);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment14",
      DefaultStatusHeuristicAssessment::kTrueNegative, 2);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment21",
      DefaultStatusHeuristicAssessment::kTrueNegative, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment28",
      DefaultStatusHeuristicAssessment::kFalsePositive, 6);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment35",
      DefaultStatusHeuristicAssessment::kFalsePositive, 6);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment42",
      DefaultStatusHeuristicAssessment::kFalsePositive, 6);

  NSDate* under_thirty_five_days_ago =
      (base::Time::Now() - base::Days(35) + base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, under_thirty_five_days_ago);
  RecordHeuristicAssessments(/*is_default_in_system_api=*/true);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment1",
      DefaultStatusHeuristicAssessment::kFalseNegative, 6);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment3",
      DefaultStatusHeuristicAssessment::kFalseNegative, 5);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment7",
      DefaultStatusHeuristicAssessment::kFalseNegative, 4);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment14",
      DefaultStatusHeuristicAssessment::kFalseNegative, 3);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment21",
      DefaultStatusHeuristicAssessment::kFalseNegative, 2);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment28",
      DefaultStatusHeuristicAssessment::kFalseNegative, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment35",
      DefaultStatusHeuristicAssessment::kTruePositive, 7);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment42",
      DefaultStatusHeuristicAssessment::kTruePositive, 7);
  RecordHeuristicAssessments(/*is_default_in_system_api=*/false);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment1",
      DefaultStatusHeuristicAssessment::kTrueNegative, 6);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment3",
      DefaultStatusHeuristicAssessment::kTrueNegative, 5);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment7",
      DefaultStatusHeuristicAssessment::kTrueNegative, 4);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment14",
      DefaultStatusHeuristicAssessment::kTrueNegative, 3);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment21",
      DefaultStatusHeuristicAssessment::kTrueNegative, 2);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment28",
      DefaultStatusHeuristicAssessment::kTrueNegative, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment35",
      DefaultStatusHeuristicAssessment::kFalsePositive, 7);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment42",
      DefaultStatusHeuristicAssessment::kFalsePositive, 7);

  NSDate* under_forty_two_days_ago =
      (base::Time::Now() - base::Days(42) + base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, under_forty_two_days_ago);
  RecordHeuristicAssessments(/*is_default_in_system_api=*/true);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment1",
      DefaultStatusHeuristicAssessment::kFalseNegative, 7);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment3",
      DefaultStatusHeuristicAssessment::kFalseNegative, 6);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment7",
      DefaultStatusHeuristicAssessment::kFalseNegative, 5);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment14",
      DefaultStatusHeuristicAssessment::kFalseNegative, 4);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment21",
      DefaultStatusHeuristicAssessment::kFalseNegative, 3);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment28",
      DefaultStatusHeuristicAssessment::kFalseNegative, 2);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment35",
      DefaultStatusHeuristicAssessment::kFalseNegative, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment42",
      DefaultStatusHeuristicAssessment::kTruePositive, 8);
  RecordHeuristicAssessments(/*is_default_in_system_api=*/false);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment1",
      DefaultStatusHeuristicAssessment::kTrueNegative, 7);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment3",
      DefaultStatusHeuristicAssessment::kTrueNegative, 6);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment7",
      DefaultStatusHeuristicAssessment::kTrueNegative, 5);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment14",
      DefaultStatusHeuristicAssessment::kTrueNegative, 4);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment21",
      DefaultStatusHeuristicAssessment::kTrueNegative, 3);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment28",
      DefaultStatusHeuristicAssessment::kTrueNegative, 2);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment35",
      DefaultStatusHeuristicAssessment::kTrueNegative, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment42",
      DefaultStatusHeuristicAssessment::kFalsePositive, 8);

  NSDate* over_forty_two_days_ago =
      (base::Time::Now() - base::Days(45)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, over_forty_two_days_ago);
  RecordHeuristicAssessments(/*is_default_in_system_api=*/true);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment1",
      DefaultStatusHeuristicAssessment::kFalseNegative, 8);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment3",
      DefaultStatusHeuristicAssessment::kFalseNegative, 7);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment7",
      DefaultStatusHeuristicAssessment::kFalseNegative, 6);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment14",
      DefaultStatusHeuristicAssessment::kFalseNegative, 5);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment21",
      DefaultStatusHeuristicAssessment::kFalseNegative, 4);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment28",
      DefaultStatusHeuristicAssessment::kFalseNegative, 3);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment35",
      DefaultStatusHeuristicAssessment::kFalseNegative, 2);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment42",
      DefaultStatusHeuristicAssessment::kFalseNegative, 1);
  RecordHeuristicAssessments(/*is_default_in_system_api=*/false);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment1",
      DefaultStatusHeuristicAssessment::kTrueNegative, 8);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment3",
      DefaultStatusHeuristicAssessment::kTrueNegative, 7);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment7",
      DefaultStatusHeuristicAssessment::kTrueNegative, 6);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment14",
      DefaultStatusHeuristicAssessment::kTrueNegative, 5);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment21",
      DefaultStatusHeuristicAssessment::kTrueNegative, 4);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment28",
      DefaultStatusHeuristicAssessment::kTrueNegative, 3);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment35",
      DefaultStatusHeuristicAssessment::kTrueNegative, 2);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment42",
      DefaultStatusHeuristicAssessment::kTrueNegative, 1);

  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.HeuristicAssessment1",
                                    18);
  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.HeuristicAssessment3",
                                    18);
  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.HeuristicAssessment7",
                                    18);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment14", 18);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment21", 18);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment28", 18);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment35", 18);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment42", 18);
}

}  // namespace default_status
