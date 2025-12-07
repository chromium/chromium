// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/model/default_status/default_status_helper.h"

#import <UIKit/UIKit.h>

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time_override.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/default_browser/model/default_status/default_status_helper_constants.h"
#import "ios/chrome/browser/default_browser/model/default_status/default_status_helper_prefs.h"
#import "ios/chrome/browser/default_browser/model/default_status/default_status_helper_types.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/shared/model/utils/first_run_test_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "testing/platform_test.h"

namespace default_status {

namespace {

void ExpectSuccessHistogramsToHaveNoSamples(
    const base::HistogramTester& histogram_tester) {
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.DaysSinceLastSuccessfulCall", 0);
  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.IsDefaultBrowser", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort1", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort2", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort3", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.StrictCohorts", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.DefaultStatusRetention", 0);
  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.HeuristicAssessment1",
                                    0);
  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.HeuristicAssessment3",
                                    0);
  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.HeuristicAssessment7",
                                    0);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment14", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment21", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment28", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment35", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment42", 0);
}

}  // namespace

// Test fixture for testing the default status helper.
class DefaultStatusHelperTest : public PlatformTest {
 public:
  DefaultStatusHelperTest() {
    scoped_feature_list_.InitWithFeatures(
        /* enabled_features */ {kRunDefaultStatusCheck},
        /* disabled_features */ {});
    RegisterPrefs();
  }

  static base::Time NowOverride() { return now_override_; }

  static void SetNowOverride(base::Time now_override) {
    now_override_ = now_override;
  }

 protected:
  void RegisterPrefs() { RegisterDefaultStatusPrefs(prefs()->registry()); }

  TestingPrefServiceSimple* prefs() { return &pref_service_; }

  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple pref_service_;
  static base::Time now_override_;
};

base::Time DefaultStatusHelperTest::now_override_;

// Tests IsNewClient.
TEST_F(DefaultStatusHelperTest, IsNewClient) {
  ResetFirstRunSentinel();
  EXPECT_TRUE(internal::IsNewClient());
  ForceFirstRunRecency(27);
  EXPECT_TRUE(internal::IsNewClient());
  ForceFirstRunRecency(29);
  EXPECT_FALSE(internal::IsNewClient());
}

// Tests HasCohortAssigned.
TEST_F(DefaultStatusHelperTest, HasCohortAssigned) {
  prefs()->ClearPref(kDefaultStatusAPICohort);
  EXPECT_FALSE(internal::HasCohortAssigned(prefs()));
  prefs()->SetInteger(kDefaultStatusAPICohort, 0);
  EXPECT_FALSE(internal::HasCohortAssigned(prefs()));
  prefs()->SetInteger(kDefaultStatusAPICohort, 1);
  EXPECT_TRUE(internal::HasCohortAssigned(prefs()));
}

// Tests GenerateRandomCohort.
TEST_F(DefaultStatusHelperTest, GenerateRandomCohort) {
  for (int i = 0; i < 1000; ++i) {
    int cohort = internal::GenerateRandomCohort();
    EXPECT_TRUE(cohort > 0 && cohort <= kCohortCount);
  }
}

// Tests GetCohortNextStartDate.
TEST_F(DefaultStatusHelperTest, GetCohortNextStartDate) {
  base::subtle::ScopedTimeClockOverrides time_override(
      &DefaultStatusHelperTest::NowOverride, nullptr, nullptr);
  base::Time now_override;
  base::Time expected;

  // January.
  EXPECT_TRUE(base::Time::FromUTCString("2025-01-15", &now_override));
  SetNowOverride(now_override);

  EXPECT_TRUE(base::Time::FromUTCString("2025-01-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/1), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-02-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/2), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-03-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/3), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-04-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/4), expected);

  EXPECT_TRUE(base::Time::FromUTCString("2025-05-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/1,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-02-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/2,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-03-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/3,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-04-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/4,
                                             /*after_current_month=*/true),
            expected);

  // February.
  EXPECT_TRUE(base::Time::FromUTCString("2025-02-15", &now_override));
  SetNowOverride(now_override);

  EXPECT_TRUE(base::Time::FromUTCString("2025-05-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/1), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-02-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/2), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-03-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/3), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-04-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/4), expected);

  EXPECT_TRUE(base::Time::FromUTCString("2025-05-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/1,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-06-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/2,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-03-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/3,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-04-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/4,
                                             /*after_current_month=*/true),
            expected);

  // March.
  EXPECT_TRUE(base::Time::FromUTCString("2025-03-15", &now_override));
  SetNowOverride(now_override);

  EXPECT_TRUE(base::Time::FromUTCString("2025-05-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/1), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-06-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/2), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-03-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/3), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-04-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/4), expected);

  EXPECT_TRUE(base::Time::FromUTCString("2025-05-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/1,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-06-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/2,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-07-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/3,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-04-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/4,
                                             /*after_current_month=*/true),
            expected);

  // April.
  EXPECT_TRUE(base::Time::FromUTCString("2025-04-15", &now_override));
  SetNowOverride(now_override);

  EXPECT_TRUE(base::Time::FromUTCString("2025-05-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/1), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-06-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/2), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-07-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/3), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-04-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/4), expected);

  EXPECT_TRUE(base::Time::FromUTCString("2025-05-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/1,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-06-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/2,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-07-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/3,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-08-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/4,
                                             /*after_current_month=*/true),
            expected);

  // May.
  EXPECT_TRUE(base::Time::FromUTCString("2025-05-15", &now_override));
  SetNowOverride(now_override);

  EXPECT_TRUE(base::Time::FromUTCString("2025-05-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/1), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-06-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/2), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-07-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/3), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-08-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/4), expected);

  EXPECT_TRUE(base::Time::FromUTCString("2025-09-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/1,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-06-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/2,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-07-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/3,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-08-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/4,
                                             /*after_current_month=*/true),
            expected);

  // June.
  EXPECT_TRUE(base::Time::FromUTCString("2025-06-15", &now_override));
  SetNowOverride(now_override);

  EXPECT_TRUE(base::Time::FromUTCString("2025-09-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/1), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-06-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/2), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-07-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/3), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-08-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/4), expected);

  EXPECT_TRUE(base::Time::FromUTCString("2025-09-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/1,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-10-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/2,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-07-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/3,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-08-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/4,
                                             /*after_current_month=*/true),
            expected);

  // July.
  EXPECT_TRUE(base::Time::FromUTCString("2025-07-15", &now_override));
  SetNowOverride(now_override);

  EXPECT_TRUE(base::Time::FromUTCString("2025-09-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/1), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-10-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/2), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-07-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/3), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-08-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/4), expected);

  EXPECT_TRUE(base::Time::FromUTCString("2025-09-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/1,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-10-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/2,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-11-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/3,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-08-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/4,
                                             /*after_current_month=*/true),
            expected);

  // August.
  EXPECT_TRUE(base::Time::FromUTCString("2025-08-15", &now_override));
  SetNowOverride(now_override);

  EXPECT_TRUE(base::Time::FromUTCString("2025-09-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/1), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-10-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/2), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-11-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/3), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-08-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/4), expected);

  EXPECT_TRUE(base::Time::FromUTCString("2025-09-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/1,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-10-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/2,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-11-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/3,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-12-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/4,
                                             /*after_current_month=*/true),
            expected);

  // September.
  EXPECT_TRUE(base::Time::FromUTCString("2025-09-15", &now_override));
  SetNowOverride(now_override);

  EXPECT_TRUE(base::Time::FromUTCString("2025-09-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/1), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-10-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/2), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-11-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/3), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-12-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/4), expected);

  EXPECT_TRUE(base::Time::FromUTCString("2026-01-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/1,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-10-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/2,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-11-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/3,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-12-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/4,
                                             /*after_current_month=*/true),
            expected);

  // October.
  EXPECT_TRUE(base::Time::FromUTCString("2025-10-15", &now_override));
  SetNowOverride(now_override);

  EXPECT_TRUE(base::Time::FromUTCString("2026-01-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/1), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-10-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/2), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-11-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/3), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-12-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/4), expected);

  EXPECT_TRUE(base::Time::FromUTCString("2026-01-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/1,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2026-02-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/2,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-11-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/3,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-12-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/4,
                                             /*after_current_month=*/true),
            expected);

  // November.
  EXPECT_TRUE(base::Time::FromUTCString("2025-11-15", &now_override));
  SetNowOverride(now_override);

  EXPECT_TRUE(base::Time::FromUTCString("2026-01-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/1), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2026-02-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/2), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-11-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/3), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-12-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/4), expected);

  EXPECT_TRUE(base::Time::FromUTCString("2026-01-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/1,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2026-02-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/2,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2026-03-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/3,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-12-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/4,
                                             /*after_current_month=*/true),
            expected);

  // December.
  EXPECT_TRUE(base::Time::FromUTCString("2025-12-15", &now_override));
  SetNowOverride(now_override);

  EXPECT_TRUE(base::Time::FromUTCString("2026-01-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/1), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2026-02-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/2), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2026-03-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/3), expected);
  EXPECT_TRUE(base::Time::FromUTCString("2025-12-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/4), expected);

  EXPECT_TRUE(base::Time::FromUTCString("2026-01-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/1,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2026-02-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/2,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2026-03-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/3,
                                             /*after_current_month=*/true),
            expected);
  EXPECT_TRUE(base::Time::FromUTCString("2026-04-01", &expected));
  EXPECT_EQ(internal::GetCohortNextStartDate(/*cohort_number=*/4,
                                             /*after_current_month=*/true),
            expected);
}

// Tests AssignNewCohortIfNeeded.
TEST_F(DefaultStatusHelperTest, AssignNewCohortIfNeeded) {
  if (!@available(iOS 18.4, *)) {
    // If the iOS version condition isn't met, no cohort should be assigned even
    // if the other conditions are met.
    prefs()->SetInteger(kDefaultStatusAPICohort, 0);
    prefs()->SetTime(kDefaultStatusAPINextRetry, base::Time());
    ForceFirstRunRecency(100);
    internal::AssignNewCohortIfNeeded(prefs());
    EXPECT_EQ(prefs()->GetInteger(kDefaultStatusAPICohort), 0);
    EXPECT_EQ(prefs()->GetTime(kDefaultStatusAPINextRetry), base::Time());
    return;
  }

  // If this is a new client, no cohort should be assigned even if the other
  // conditions are met.
  prefs()->SetInteger(kDefaultStatusAPICohort, 0);
  prefs()->SetTime(kDefaultStatusAPINextRetry, base::Time());
  ForceFirstRunRecency(27);
  internal::AssignNewCohortIfNeeded(prefs());
  EXPECT_EQ(prefs()->GetInteger(kDefaultStatusAPICohort), 0);
  EXPECT_EQ(prefs()->GetTime(kDefaultStatusAPINextRetry), base::Time());

  // If a cohort is already assigned, the cohort and next availability should
  // not be changed, even if the other conditions are met.
  prefs()->SetInteger(kDefaultStatusAPICohort, 1000);
  base::Time time;
  EXPECT_TRUE(base::Time::FromUTCString("2000-01-15", &time));
  prefs()->SetTime(kDefaultStatusAPINextRetry, time);
  ForceFirstRunRecency(100);
  internal::AssignNewCohortIfNeeded(prefs());
  EXPECT_EQ(prefs()->GetInteger(kDefaultStatusAPICohort), 1000);
  EXPECT_EQ(prefs()->GetTime(kDefaultStatusAPINextRetry), time);

  // If all conditions are met, a new cohort should be assigned and the next
  // availability should be set.
  prefs()->SetInteger(kDefaultStatusAPICohort, 0);
  prefs()->SetTime(kDefaultStatusAPINextRetry, base::Time());
  ForceFirstRunRecency(100);
  internal::AssignNewCohortIfNeeded(prefs());
  EXPECT_NE(prefs()->GetInteger(kDefaultStatusAPICohort), 0);
  EXPECT_NE(prefs()->GetTime(kDefaultStatusAPINextRetry), base::Time());
}

// Tests SystemToLocalEnum.

TEST_F(DefaultStatusHelperTest, SystemToLocalEnum) API_AVAILABLE(ios(18.4)) {
  DefaultStatusAPIResult result =
      internal::SystemToLocalEnum(UIApplicationCategoryDefaultStatusIsDefault);
  EXPECT_EQ(result, DefaultStatusAPIResult::kIsDefault);
  result =
      internal::SystemToLocalEnum(UIApplicationCategoryDefaultStatusNotDefault);
  EXPECT_EQ(result, DefaultStatusAPIResult::kIsNotDefault);
  result =
      internal::SystemToLocalEnum((UIApplicationCategoryDefaultStatus)1000);
  EXPECT_EQ(result, DefaultStatusAPIResult::kUnknown);
}

// Tests DetermineRetentionStatus.
TEST_F(DefaultStatusHelperTest, DetermineRetentionStatus) {
  DefaultStatusRetention result = internal::DetermineRetentionStatus(
      /*previous=*/DefaultStatusAPIResult::kIsNotDefault,
      /*current=*/DefaultStatusAPIResult::kIsDefault);
  EXPECT_EQ(result, DefaultStatusRetention::kBecameDefault);
  result = internal::DetermineRetentionStatus(
      /*previous=*/DefaultStatusAPIResult::kIsDefault,
      /*current=*/DefaultStatusAPIResult::kIsNotDefault);
  EXPECT_EQ(result, DefaultStatusRetention::kBecameNonDefault);
  result = internal::DetermineRetentionStatus(
      /*previous=*/DefaultStatusAPIResult::kIsDefault,
      /*current=*/DefaultStatusAPIResult::kIsDefault);
  EXPECT_EQ(result, DefaultStatusRetention::kRemainedDefault);
  result = internal::DetermineRetentionStatus(
      /*previous=*/DefaultStatusAPIResult::kIsNotDefault,
      /*current=*/DefaultStatusAPIResult::kIsNotDefault);
  EXPECT_EQ(result, DefaultStatusRetention::kRemainedNonDefault);
}

// Tests that QueryDefaultStatusIfReadyAndLogResults doesn't do anything if the
// system isn't running on the minimum version.

TEST_F(DefaultStatusHelperTest, QueryDefaultStatusAPINotAvailable)
API_AVAILABLE(ios(18.4)) {
  if (@available(iOS 18.4, *)) {
    return;
  }

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.OutcomeType", 0);
  prefs()->SetInteger(kDefaultStatusAPICohort, 1);
  prefs()->SetTime(kDefaultStatusAPINextRetry, base::Time());
  internal::OverrideSystemCallForTesting(base::BindOnce([](NSError** error) {
    // This should not be called.
    EXPECT_TRUE(false);
    return UIApplicationCategoryDefaultStatusUnavailable;
  }));

  internal::QueryDefaultStatusIfReadyAndLogResults(prefs());

  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.OutcomeType", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.CooldownError.DaysLeft", 0);
  ExpectSuccessHistogramsToHaveNoSamples(histogram_tester);
}

// Test that QueryDefaultStatusIfReadyAndLogResults does not do anything if
// the client is still on cooldown according to local prefs.

TEST_F(DefaultStatusHelperTest, QueryDefaultStatusAPIOnLocalCooldown)
API_AVAILABLE(ios(18.4)) {
  if (!@available(iOS 18.4, *)) {
    return;
  }

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.OutcomeType", 0);
  prefs()->SetInteger(kDefaultStatusAPICohort, 1);
  base::Time some_day_in_future =
      base::subtle::TimeNowIgnoringOverride() + base::Days(100);
  prefs()->SetTime(kDefaultStatusAPINextRetry, some_day_in_future);
  internal::OverrideSystemCallForTesting(base::BindOnce([](NSError** error) {
    // This should not be called.
    EXPECT_TRUE(false);
    return UIApplicationCategoryDefaultStatusUnavailable;
  }));

  internal::QueryDefaultStatusIfReadyAndLogResults(prefs());

  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.OutcomeType", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.CooldownError.DaysLeft", 0);
  ExpectSuccessHistogramsToHaveNoSamples(histogram_tester);
}

// Test that QueryDefaultStatusIfReadyAndLogResults correctly handles cooldown
// errors returned by the default status system API.

TEST_F(DefaultStatusHelperTest, QueryDefaultStatusAPISystemCooldownError)
API_AVAILABLE(ios(18.4)) {
  if (!@available(iOS 18.4, *)) {
    return;
  }

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.OutcomeType", 0);
  prefs()->SetInteger(kDefaultStatusAPICohort, 1);
  prefs()->SetTime(kDefaultStatusAPINextRetry, base::Time());
  base::Time next_retry =
      base::subtle::TimeNowIgnoringOverride() + base::Days(100);
  internal::OverrideSystemCallForTesting(base::BindOnce(
      [](base::Time retry_date, NSError** error) {
        NSDictionary* errorInfo = @{
          UIApplicationCategoryDefaultRetryAvailabilityDateErrorKey :
              retry_date.ToNSDate()
        };
        *error = [NSError
            errorWithDomain:UIApplicationCategoryDefaultErrorDomain
                       code:UIApplicationCategoryDefaultErrorRateLimited
                   userInfo:errorInfo];
        return UIApplicationCategoryDefaultStatusUnavailable;
      },
      next_retry));

  internal::QueryDefaultStatusIfReadyAndLogResults(prefs());

  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.OutcomeType", 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.OutcomeType",
      DefaultStatusAPIOutcomeType::kCooldownError, 1);
  EXPECT_EQ(prefs()->GetTime(kDefaultStatusAPINextRetry), next_retry);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.CooldownError.DaysLeft", 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.CooldownError.DaysLeft", 100, 1);
  ExpectSuccessHistogramsToHaveNoSamples(histogram_tester);
}

// Test that QueryDefaultStatusIfReadyAndLogResults can handle unknown error
// types returned by the default status system API.

TEST_F(DefaultStatusHelperTest, QueryDefaultStatusAPISystemUnknownError)
API_AVAILABLE(ios(18.4)) {
  if (!@available(iOS 18.4, *)) {
    return;
  }

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.OutcomeType", 0);
  prefs()->SetInteger(kDefaultStatusAPICohort, 1);
  base::Time some_days_ago =
      base::subtle::TimeNowIgnoringOverride() - base::Days(10);
  prefs()->SetTime(kDefaultStatusAPINextRetry, some_days_ago);
  internal::OverrideSystemCallForTesting(base::BindOnce([](NSError** error) {
    NSDictionary* errorInfo = @{@"Fake" : @42};
    *error = [NSError errorWithDomain:UIApplicationCategoryDefaultErrorDomain
                                 code:123456
                             userInfo:errorInfo];
    return UIApplicationCategoryDefaultStatusUnavailable;
  }));

  internal::QueryDefaultStatusIfReadyAndLogResults(prefs());

  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.OutcomeType", 1);
  histogram_tester.ExpectBucketCount("IOS.DefaultStatusAPI.OutcomeType",
                                     DefaultStatusAPIOutcomeType::kOtherError,
                                     1);
  EXPECT_EQ(prefs()->GetTime(kDefaultStatusAPINextRetry), some_days_ago);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.CooldownError.DaysLeft", 0);
  ExpectSuccessHistogramsToHaveNoSamples(histogram_tester);
}

// Tests the success case of QueryDefaultStatusIfReadyAndLogResults for the
// following scenario:
//    - Cohort 1
//    - Within cohort 1 reporting window
//    - Is default
//    - No previous default status API result

TEST_F(DefaultStatusHelperTest,
       QueryDefaultStatusAPIStrictCohort1IsDefaultFirstTime)
API_AVAILABLE(ios(18.4)) {
  if (!@available(iOS 18.4, *)) {
    return;
  }

  base::subtle::ScopedTimeClockOverrides time_override(
      &DefaultStatusHelperTest::NowOverride, nullptr, nullptr);
  base::Time now_override;
  EXPECT_TRUE(base::Time::FromUTCString("2025-01-15", &now_override));
  SetNowOverride(now_override);

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.OutcomeType", 0);
  prefs()->SetInteger(kDefaultStatusAPICohort, 1);
  prefs()->SetTime(kDefaultStatusAPILastSuccessfulCall, base::Time());
  prefs()->SetTime(kDefaultStatusAPINextRetry, base::Time());
  prefs()->SetInteger(kDefaultStatusAPIResult,
                      static_cast<int>(DefaultStatusAPIResult::kUnknown));
  internal::OverrideSystemCallForTesting(base::BindOnce([](NSError** error) {
    return UIApplicationCategoryDefaultStatusIsDefault;
  }));

  internal::QueryDefaultStatusIfReadyAndLogResults(prefs());

  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.OutcomeType", 1);
  histogram_tester.ExpectBucketCount("IOS.DefaultStatusAPI.OutcomeType",
                                     DefaultStatusAPIOutcomeType::kSuccess, 1);
  base::Time expected_next;
  EXPECT_TRUE(base::Time::FromUTCString("2025-05-01", &expected_next));
  EXPECT_EQ(prefs()->GetTime(kDefaultStatusAPINextRetry), expected_next);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.CooldownError.DaysLeft", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.DaysSinceLastSuccessfulCall", 0);
  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.IsDefaultBrowser", 1);
  histogram_tester.ExpectBucketCount("IOS.DefaultStatusAPI.IsDefaultBrowser",
                                     true, 1);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort1", 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort1", true, 1);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort2", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort3", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.StrictCohorts", 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.StrictCohorts", true, 1);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.DefaultStatusRetention", 0);
  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.HeuristicAssessment1",
                                    1);
  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.HeuristicAssessment3",
                                    1);
  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.HeuristicAssessment7",
                                    1);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment14", 1);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment21", 1);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment28", 1);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment35", 1);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment42", 1);
}

// Tests the success case of QueryDefaultStatusIfReadyAndLogResults for the
// following scenario:
//    - Cohort 2
//    - Outside cohort 2 reporting window
//    - Is not default
//    - Has a previous default status API result of "is default"

TEST_F(DefaultStatusHelperTest,
       QueryDefaultStatusAPICohort2IsNotDefaultRecurring)
API_AVAILABLE(ios(18.4)) {
  if (!@available(iOS 18.4, *)) {
    return;
  }

  base::subtle::ScopedTimeClockOverrides time_override(
      &DefaultStatusHelperTest::NowOverride, nullptr, nullptr);
  base::Time now_override;
  EXPECT_TRUE(base::Time::FromUTCString("2025-03-15", &now_override));
  SetNowOverride(now_override);

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.OutcomeType", 0);
  prefs()->SetInteger(kDefaultStatusAPICohort, 2);
  base::Time last_successful_call = base::Time::Now() - base::Days(100);
  prefs()->SetTime(kDefaultStatusAPILastSuccessfulCall, last_successful_call);
  prefs()->SetTime(kDefaultStatusAPINextRetry, base::Time());
  prefs()->SetInteger(kDefaultStatusAPIResult,
                      static_cast<int>(DefaultStatusAPIResult::kIsDefault));
  internal::OverrideSystemCallForTesting(base::BindOnce([](NSError** error) {
    return UIApplicationCategoryDefaultStatusNotDefault;
  }));

  internal::QueryDefaultStatusIfReadyAndLogResults(prefs());

  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.OutcomeType", 1);
  histogram_tester.ExpectBucketCount("IOS.DefaultStatusAPI.OutcomeType",
                                     DefaultStatusAPIOutcomeType::kSuccess, 1);
  base::Time expected_next;
  EXPECT_TRUE(base::Time::FromUTCString("2025-06-01", &expected_next));
  EXPECT_EQ(prefs()->GetTime(kDefaultStatusAPINextRetry), expected_next);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.CooldownError.DaysLeft", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.DaysSinceLastSuccessfulCall", 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.DaysSinceLastSuccessfulCall", 100, 1);
  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.IsDefaultBrowser", 1);
  histogram_tester.ExpectBucketCount("IOS.DefaultStatusAPI.IsDefaultBrowser",
                                     false, 1);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort1", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort2", 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort2", false, 1);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort3", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.StrictCohorts", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.DefaultStatusRetention", 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.DefaultStatusRetention",
      DefaultStatusRetention::kBecameNonDefault, 1);
  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.HeuristicAssessment1",
                                    1);
  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.HeuristicAssessment3",
                                    1);
  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.HeuristicAssessment7",
                                    1);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment14", 1);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment21", 1);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment28", 1);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment35", 1);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment42", 1);
}

// Tests the success case of QueryDefaultStatusIfReadyAndLogResults for the
// following scenario:
//    - Cohort 3
//    - Inside cohort 3 reporting window
//    - Is default
//    - Has a previous default status API result of "is default"

TEST_F(DefaultStatusHelperTest,
       QueryDefaultStatusAPIStrictCohort3IsDefaultRecurring)
API_AVAILABLE(ios(18.4)) {
  if (!@available(iOS 18.4, *)) {
    return;
  }

  base::subtle::ScopedTimeClockOverrides time_override(
      &DefaultStatusHelperTest::NowOverride, nullptr, nullptr);
  base::Time now_override;
  EXPECT_TRUE(base::Time::FromUTCString("2025-07-15", &now_override));
  SetNowOverride(now_override);

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.OutcomeType", 0);
  prefs()->SetInteger(kDefaultStatusAPICohort, 3);
  base::Time last_successful_call = base::Time::Now() - base::Days(150);
  prefs()->SetTime(kDefaultStatusAPILastSuccessfulCall, last_successful_call);
  prefs()->SetTime(kDefaultStatusAPINextRetry, base::Time());
  prefs()->SetInteger(kDefaultStatusAPIResult,
                      static_cast<int>(DefaultStatusAPIResult::kIsDefault));
  internal::OverrideSystemCallForTesting(base::BindOnce([](NSError** error) {
    return UIApplicationCategoryDefaultStatusIsDefault;
  }));

  internal::QueryDefaultStatusIfReadyAndLogResults(prefs());

  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.OutcomeType", 1);
  histogram_tester.ExpectBucketCount("IOS.DefaultStatusAPI.OutcomeType",
                                     DefaultStatusAPIOutcomeType::kSuccess, 1);
  base::Time expected_next;
  EXPECT_TRUE(base::Time::FromUTCString("2025-11-01", &expected_next));
  EXPECT_EQ(prefs()->GetTime(kDefaultStatusAPINextRetry), expected_next);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.CooldownError.DaysLeft", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.DaysSinceLastSuccessfulCall", 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.DaysSinceLastSuccessfulCall", 150, 1);
  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.IsDefaultBrowser", 1);
  histogram_tester.ExpectBucketCount("IOS.DefaultStatusAPI.IsDefaultBrowser",
                                     true, 1);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort1", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort2", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort3", 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort3", true, 1);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.StrictCohorts", 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.StrictCohorts", true, 1);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.DefaultStatusRetention", 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.DefaultStatusRetention",
      DefaultStatusRetention::kRemainedDefault, 1);
  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.HeuristicAssessment1",
                                    1);
  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.HeuristicAssessment3",
                                    1);
  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.HeuristicAssessment7",
                                    1);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment14", 1);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment21", 1);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment28", 1);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment35", 1);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.HeuristicAssessment42", 1);
}

}  // namespace default_status
