// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/default_status/model/default_status_helper.h"

#import <UIKit/UIKit.h>

#import <optional>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/strings/stringprintf.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "base/time/time_override.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/default_browser/default_status/model/default_status_helper_types.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_browser/model/utils_test_support.h"
#import "ios/chrome/browser/shared/model/utils/first_run_test_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace default_status {

namespace {

// Verifies that no success-only histograms were recorded in `histogram_tester`.
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
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort4", 0);
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

// Returns a system status callback that fails the test if invoked.
base::OnceCallback<UIApplicationCategoryDefaultStatus(NSError**)>
UnexpectedSystemCall() API_AVAILABLE(ios(18.4)) {
  return base::BindOnce([](NSError** error) {
    ADD_FAILURE() << "Unexpected system status query.";
    return UIApplicationCategoryDefaultStatusUnavailable;
  });
}

// Returns a system status callback that succeeds with `status`.
base::OnceCallback<UIApplicationCategoryDefaultStatus(NSError**)>
SucceedingSystemCall(UIApplicationCategoryDefaultStatus status)
    API_AVAILABLE(ios(18.4)) {
  return base::BindOnce([](UIApplicationCategoryDefaultStatus result,
                           NSError** error) { return result; },
                        status);
}

}  // namespace

// Test fixture for testing the default status helper through its public seam.
class DefaultStatusHelperTest : public PlatformTest {
 public:
  DefaultStatusHelperTest() {
    now_override_ = base::Time();
    ClearDefaultBrowserPromoData();
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{kRunDefaultStatusCheck},
        /*disabled_features=*/{});
    RegisterDefaultStatusPrefs(pref_service_.registry());
    ForceFirstRunRecency(100);
  }

  ~DefaultStatusHelperTest() override {
    ClearDefaultBrowserPromoData();
    now_override_ = base::Time();
  }

  static base::Time NowOverride() { return now_override_; }

  static void SetNowOverride(base::Time now_override) {
    now_override_ = now_override;
  }

  static void SetNowFromUTCString(const char* utc_string) {
    base::Time parsed_time;
    ASSERT_TRUE(base::Time::FromUTCString(utc_string, &parsed_time));
    now_override_ = parsed_time;
  }

 protected:
  TestingPrefServiceSimple* prefs() { return &pref_service_; }

  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple pref_service_;
  static base::Time now_override_;
};

base::Time DefaultStatusHelperTest::now_override_;

// Tests that `TriggerDefaultStatusCheckForTesting` does not query the system
// API or record metrics when `kRunDefaultStatusCheck` is disabled.
TEST_F(DefaultStatusHelperTest, DisabledFeatureDoesNotQuerySystem)
API_AVAILABLE(ios(18.4)) {
  if (!@available(iOS 18.4, *)) {
    return;
  }

  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(kRunDefaultStatusCheck);

  base::subtle::ScopedTimeClockOverrides time_override(
      &DefaultStatusHelperTest::NowOverride, nullptr, nullptr);
  SetNowFromUTCString("2025-01-15");

  base::HistogramTester histogram_tester;
  TriggerDefaultStatusCheckForTesting(prefs(), UnexpectedSystemCall(),
                                      /*cohort_override=*/1);

  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.OutcomeType", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.CooldownError.DaysLeft", 0);
  ExpectSuccessHistogramsToHaveNoSamples(histogram_tester);
}

// Tests that `TriggerDefaultStatusCheckForTesting` does not assign a cohort or
// query the system API when the client completed First Run less than 28 days
// ago, and proceeds once the client is at least 28 days old.
TEST_F(DefaultStatusHelperTest, NewClientDoesNotAssignCohortOrQuerySystem)
API_AVAILABLE(ios(18.4)) {
  if (!@available(iOS 18.4, *)) {
    return;
  }

  base::subtle::ScopedTimeClockOverrides time_override(
      &DefaultStatusHelperTest::NowOverride, nullptr, nullptr);
  SetNowFromUTCString("2025-01-15");

  base::HistogramTester histogram_tester;

  // Missing First Run sentinel is treated as a new client.
  ResetFirstRunSentinel();
  TriggerDefaultStatusCheckForTesting(prefs(), UnexpectedSystemCall(),
                                      /*cohort_override=*/1);
  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.OutcomeType", 0);

  // 27 days since First Run is still below the 28-day threshold.
  ForceFirstRunRecency(27);
  TriggerDefaultStatusCheckForTesting(prefs(), UnexpectedSystemCall(),
                                      /*cohort_override=*/1);
  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.OutcomeType", 0);

  // Advance to February and mark First Run as 29 days old. Because no cohort
  // was persisted during the earlier January calls, `cohort_override = 2` takes
  // effect and queries immediately in February.
  SetNowFromUTCString("2025-02-15");
  ForceFirstRunRecency(29);
  TriggerDefaultStatusCheckForTesting(
      prefs(),
      SucceedingSystemCall(UIApplicationCategoryDefaultStatusIsDefault),
      /*cohort_override=*/2);

  histogram_tester.ExpectBucketCount("IOS.DefaultStatusAPI.OutcomeType",
                                     DefaultStatusAPIOutcomeType::kSuccess, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort2", true, 1);
}

// Tests that once a cohort is assigned, subsequent calls to
// `TriggerDefaultStatusCheckForTesting` keep the existing cohort assignment.
TEST_F(DefaultStatusHelperTest, CohortAssignmentPersistsAcrossCalls)
API_AVAILABLE(ios(18.4)) {
  if (!@available(iOS 18.4, *)) {
    return;
  }

  base::subtle::ScopedTimeClockOverrides time_override(
      &DefaultStatusHelperTest::NowOverride, nullptr, nullptr);
  base::HistogramTester histogram_tester;

  // Assign Cohort 2 in January (before Cohort 2's February window).
  SetNowFromUTCString("2025-01-15");
  TriggerDefaultStatusCheckForTesting(prefs(), UnexpectedSystemCall(),
                                      /*cohort_override=*/2);

  // Calling again in January with `cohort_override = 1` must not overwrite the
  // existing Cohort 2 assignment or trigger a January query.
  TriggerDefaultStatusCheckForTesting(prefs(), UnexpectedSystemCall(),
                                      /*cohort_override=*/1);
  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.OutcomeType", 0);

  // In February, the persisted Cohort 2 assignment triggers as expected.
  SetNowFromUTCString("2025-02-15");
  TriggerDefaultStatusCheckForTesting(
      prefs(),
      SucceedingSystemCall(UIApplicationCategoryDefaultStatusIsDefault),
      /*cohort_override=*/1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort2", true, 1);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort1", 0);
}

// Tests that omitting `cohort_override` randomly assigns a valid cohort in
// `1..4` that queries the system API exactly once during a 4-month cycle.
TEST_F(DefaultStatusHelperTest, RandomCohortAssignmentQueriesWithinCycle)
API_AVAILABLE(ios(18.4)) {
  if (!@available(iOS 18.4, *)) {
    return;
  }

  base::subtle::ScopedTimeClockOverrides time_override(
      &DefaultStatusHelperTest::NowOverride, nullptr, nullptr);

  static constexpr const char* kCycleDates[] = {"2025-01-15", "2025-02-15",
                                                "2025-03-15", "2025-04-15"};

  for (int i = 0; i < 50; ++i) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    TestingPrefServiceSimple iteration_prefs;
    RegisterDefaultStatusPrefs(iteration_prefs.registry());
    base::HistogramTester histogram_tester;

    for (const char* date_str : kCycleDates) {
      SetNowFromUTCString(date_str);
      TriggerDefaultStatusCheckForTesting(
          &iteration_prefs,
          SucceedingSystemCall(UIApplicationCategoryDefaultStatusIsDefault),
          /*cohort_override=*/std::nullopt);
    }

    // Across the 4 months of a cycle, the assigned cohort must query exactly
    // once and log to `StrictCohorts`.
    histogram_tester.ExpectUniqueSample("IOS.DefaultStatusAPI.OutcomeType",
                                        DefaultStatusAPIOutcomeType::kSuccess,
                                        1);
    histogram_tester.ExpectUniqueSample(
        "IOS.DefaultStatusAPI.IsDefaultBrowser.StrictCohorts", true, 1);
  }
}

// Tests that for all 12 calendar months, a client assigned to the current
// month's cohort immediately queries the system API and schedules its next
// retry for the 1st of the cohort's month in the next cycle (including year
// rollover).
TEST_F(DefaultStatusHelperTest, CohortSchedulingWhenAssignedInOwnMonth)
API_AVAILABLE(ios(18.4)) {
  if (!@available(iOS 18.4, *)) {
    return;
  }

  base::subtle::ScopedTimeClockOverrides time_override(
      &DefaultStatusHelperTest::NowOverride, nullptr, nullptr);

  struct ScheduleCase {
    const char* assign_date;
    int cohort;
    const char* next_window_start;
  };

  static constexpr ScheduleCase kCases[] = {
      {"2025-01-15", 1, "2025-05-01"}, {"2025-02-15", 2, "2025-06-01"},
      {"2025-03-15", 3, "2025-07-01"}, {"2025-04-15", 4, "2025-08-01"},
      {"2025-05-15", 1, "2025-09-01"}, {"2025-06-15", 2, "2025-10-01"},
      {"2025-07-15", 3, "2025-11-01"}, {"2025-08-15", 4, "2025-12-01"},
      {"2025-09-15", 1, "2026-01-01"}, {"2025-10-15", 2, "2026-02-01"},
      {"2025-11-15", 3, "2026-03-01"}, {"2025-12-15", 4, "2026-04-01"},
  };

  for (const ScheduleCase& test_case : kCases) {
    SCOPED_TRACE(base::StringPrintf("Assign %s cohort %d -> next %s",
                                    test_case.assign_date, test_case.cohort,
                                    test_case.next_window_start));
    TestingPrefServiceSimple case_prefs;
    RegisterDefaultStatusPrefs(case_prefs.registry());
    base::HistogramTester histogram_tester;

    // Initial check in the cohort's own month executes immediately.
    SetNowFromUTCString(test_case.assign_date);
    TriggerDefaultStatusCheckForTesting(
        &case_prefs,
        SucceedingSystemCall(UIApplicationCategoryDefaultStatusIsDefault),
        test_case.cohort);
    histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.OutcomeType", 1);

    base::Time next_window;
    ASSERT_TRUE(
        base::Time::FromUTCString(test_case.next_window_start, &next_window));

    // One day before the next cycle's window, the client is still on cooldown.
    SetNowOverride(next_window - base::Days(1));
    TriggerDefaultStatusCheckForTesting(&case_prefs, UnexpectedSystemCall(),
                                        test_case.cohort);
    histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.OutcomeType", 1);

    // On the exact first day of the next cycle's window, the query executes.
    SetNowOverride(next_window);
    TriggerDefaultStatusCheckForTesting(
        &case_prefs,
        SucceedingSystemCall(UIApplicationCategoryDefaultStatusIsDefault),
        test_case.cohort);
    histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.OutcomeType", 2);
  }
}

// Tests that for all 12 calendar months and all cohorts whose window is not the
// current month, `TriggerDefaultStatusCheckForTesting` defers querying until
// the 1st of the cohort's next reporting month (including year rollover).
TEST_F(DefaultStatusHelperTest, CohortSchedulingWhenAssignedOutsideOwnMonth)
API_AVAILABLE(ios(18.4)) {
  if (!@available(iOS 18.4, *)) {
    return;
  }

  base::subtle::ScopedTimeClockOverrides time_override(
      &DefaultStatusHelperTest::NowOverride, nullptr, nullptr);

  struct DeferredScheduleCase {
    const char* assign_date;
    int cohort;
    const char* first_window_start;
  };

  static constexpr DeferredScheduleCase kCases[] = {
      // January (Cohort 1 active; Cohorts 2, 3, 4 deferred).
      {"2025-01-15", 2, "2025-02-01"},
      {"2025-01-15", 3, "2025-03-01"},
      {"2025-01-15", 4, "2025-04-01"},
      // February (Cohort 2 active; Cohorts 1, 3, 4 deferred).
      {"2025-02-15", 1, "2025-05-01"},
      {"2025-02-15", 3, "2025-03-01"},
      {"2025-02-15", 4, "2025-04-01"},
      // March (Cohort 3 active; Cohorts 1, 2, 4 deferred).
      {"2025-03-15", 1, "2025-05-01"},
      {"2025-03-15", 2, "2025-06-01"},
      {"2025-03-15", 4, "2025-04-01"},
      // April (Cohort 4 active; Cohorts 1, 2, 3 deferred).
      {"2025-04-15", 1, "2025-05-01"},
      {"2025-04-15", 2, "2025-06-01"},
      {"2025-04-15", 3, "2025-07-01"},
      // May (Cohort 1 active; Cohorts 2, 3, 4 deferred).
      {"2025-05-15", 2, "2025-06-01"},
      {"2025-05-15", 3, "2025-07-01"},
      {"2025-05-15", 4, "2025-08-01"},
      // June (Cohort 2 active; Cohorts 1, 3, 4 deferred).
      {"2025-06-15", 1, "2025-09-01"},
      {"2025-06-15", 3, "2025-07-01"},
      {"2025-06-15", 4, "2025-08-01"},
      // July (Cohort 3 active; Cohorts 1, 2, 4 deferred).
      {"2025-07-15", 1, "2025-09-01"},
      {"2025-07-15", 2, "2025-10-01"},
      {"2025-07-15", 4, "2025-08-01"},
      // August (Cohort 4 active; Cohorts 1, 2, 3 deferred).
      {"2025-08-15", 1, "2025-09-01"},
      {"2025-08-15", 2, "2025-10-01"},
      {"2025-08-15", 3, "2025-11-01"},
      // September (Cohort 1 active; Cohorts 2, 3, 4 deferred).
      {"2025-09-15", 2, "2025-10-01"},
      {"2025-09-15", 3, "2025-11-01"},
      {"2025-09-15", 4, "2025-12-01"},
      // October (Cohort 2 active; Cohorts 1, 3, 4 deferred).
      {"2025-10-15", 1, "2026-01-01"},
      {"2025-10-15", 3, "2025-11-01"},
      {"2025-10-15", 4, "2025-12-01"},
      // November (Cohort 3 active; Cohorts 1, 2, 4 deferred).
      {"2025-11-15", 1, "2026-01-01"},
      {"2025-11-15", 2, "2026-02-01"},
      {"2025-11-15", 4, "2025-12-01"},
      // December (Cohort 4 active; Cohorts 1, 2, 3 deferred).
      {"2025-12-15", 1, "2026-01-01"},
      {"2025-12-15", 2, "2026-02-01"},
      {"2025-12-15", 3, "2026-03-01"},
  };

  for (const DeferredScheduleCase& test_case : kCases) {
    SCOPED_TRACE(base::StringPrintf("Assign %s cohort %d -> first %s",
                                    test_case.assign_date, test_case.cohort,
                                    test_case.first_window_start));
    TestingPrefServiceSimple case_prefs;
    RegisterDefaultStatusPrefs(case_prefs.registry());
    base::HistogramTester histogram_tester;

    // Initial check outside the cohort's month assigns the cohort without
    // querying the system API.
    SetNowFromUTCString(test_case.assign_date);
    TriggerDefaultStatusCheckForTesting(&case_prefs, UnexpectedSystemCall(),
                                        test_case.cohort);
    histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.OutcomeType", 0);

    base::Time first_window;
    ASSERT_TRUE(
        base::Time::FromUTCString(test_case.first_window_start, &first_window));

    // One day before the cohort's first window, the client is still waiting.
    SetNowOverride(first_window - base::Days(1));
    TriggerDefaultStatusCheckForTesting(&case_prefs, UnexpectedSystemCall(),
                                        test_case.cohort);
    histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.OutcomeType", 0);

    // On the exact first day of the cohort's window, the query executes.
    SetNowOverride(first_window);
    TriggerDefaultStatusCheckForTesting(
        &case_prefs,
        SucceedingSystemCall(UIApplicationCategoryDefaultStatusIsDefault),
        test_case.cohort);
    histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.OutcomeType", 1);
  }
}

// Tests that `TriggerDefaultStatusCheckForTesting` records `kCooldownError` and
// `IOS.DefaultStatusAPI.CooldownError.DaysLeft` when the system API returns a
// rate-limited error, suppresses calls during the cooldown period, and retries
// once the retry date is reached.
TEST_F(DefaultStatusHelperTest,
       SystemCooldownErrorSchedulesRetryAndRecordsDaysLeft)
API_AVAILABLE(ios(18.4)) {
  if (!@available(iOS 18.4, *)) {
    return;
  }

  base::subtle::ScopedTimeClockOverrides time_override(
      &DefaultStatusHelperTest::NowOverride, nullptr, nullptr);
  SetNowFromUTCString("2025-01-15");

  base::HistogramTester histogram_tester;
  base::Time retry_date = base::Time::Now() + base::Days(100);

  TriggerDefaultStatusCheckForTesting(
      prefs(),
      base::BindOnce(
          [](base::Time target_retry, NSError** error) {
            NSDictionary* error_info = @{
              UIApplicationCategoryDefaultRetryAvailabilityDateErrorKey :
                  target_retry.ToNSDate()
            };
            *error = [NSError
                errorWithDomain:UIApplicationCategoryDefaultErrorDomain
                           code:UIApplicationCategoryDefaultErrorRateLimited
                       userInfo:error_info];
            return UIApplicationCategoryDefaultStatusUnavailable;
          },
          retry_date),
      /*cohort_override=*/1);

  histogram_tester.ExpectUniqueSample(
      "IOS.DefaultStatusAPI.OutcomeType",
      DefaultStatusAPIOutcomeType::kCooldownError, 1);
  histogram_tester.ExpectUniqueSample(
      "IOS.DefaultStatusAPI.CooldownError.DaysLeft", 100, 1);
  ExpectSuccessHistogramsToHaveNoSamples(histogram_tester);

  // Calling before `retry_date` does not invoke the system callback.
  SetNowOverride(retry_date - base::Days(1));
  TriggerDefaultStatusCheckForTesting(prefs(), UnexpectedSystemCall(),
                                      /*cohort_override=*/1);
  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.OutcomeType", 1);

  // Calling at `retry_date` invokes the system callback.
  SetNowOverride(retry_date);
  TriggerDefaultStatusCheckForTesting(
      prefs(),
      SucceedingSystemCall(UIApplicationCategoryDefaultStatusIsDefault),
      /*cohort_override=*/1);
  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.OutcomeType", 2);
  histogram_tester.ExpectBucketCount("IOS.DefaultStatusAPI.OutcomeType",
                                     DefaultStatusAPIOutcomeType::kSuccess, 1);
}

// Tests that `TriggerDefaultStatusCheckForTesting` records `kOtherError` when
// the system API returns an unknown error and allows retrying on the next check
// without advancing the cooldown timestamp.
TEST_F(DefaultStatusHelperTest, SystemUnknownErrorDoesNotAdvanceCooldown)
API_AVAILABLE(ios(18.4)) {
  if (!@available(iOS 18.4, *)) {
    return;
  }

  base::subtle::ScopedTimeClockOverrides time_override(
      &DefaultStatusHelperTest::NowOverride, nullptr, nullptr);
  SetNowFromUTCString("2025-01-15");

  base::HistogramTester histogram_tester;
  TriggerDefaultStatusCheckForTesting(
      prefs(), base::BindOnce([](NSError** error) {
        *error =
            [NSError errorWithDomain:UIApplicationCategoryDefaultErrorDomain
                                code:123456
                            userInfo:@{
                              @"Fake" : @42
                            }];
        return UIApplicationCategoryDefaultStatusUnavailable;
      }),
      /*cohort_override=*/1);

  histogram_tester.ExpectUniqueSample("IOS.DefaultStatusAPI.OutcomeType",
                                      DefaultStatusAPIOutcomeType::kOtherError,
                                      1);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.CooldownError.DaysLeft", 0);
  ExpectSuccessHistogramsToHaveNoSamples(histogram_tester);

  // Because the unknown error did not advance the retry timestamp, a subsequent
  // check at the same timestamp immediately retries the system call.
  TriggerDefaultStatusCheckForTesting(
      prefs(),
      SucceedingSystemCall(UIApplicationCategoryDefaultStatusIsDefault),
      /*cohort_override=*/1);
  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.OutcomeType", 2);
  histogram_tester.ExpectBucketCount("IOS.DefaultStatusAPI.OutcomeType",
                                     DefaultStatusAPIOutcomeType::kSuccess, 1);
}

// Tests that a first-time successful query for Cohort 1 inside its reporting
// window records `kSuccess`, `IsDefaultBrowser`, `IsDefaultBrowser.Cohort1`,
// `IsDefaultBrowser.StrictCohorts`, and heuristic assessments, without
// recording `DaysSinceLastSuccessfulCall` or `DefaultStatusRetention`.
TEST_F(DefaultStatusHelperTest, StrictCohort1IsDefaultFirstTime)
API_AVAILABLE(ios(18.4)) {
  if (!@available(iOS 18.4, *)) {
    return;
  }

  base::subtle::ScopedTimeClockOverrides time_override(
      &DefaultStatusHelperTest::NowOverride, nullptr, nullptr);
  SetNowFromUTCString("2025-01-15");

  base::HistogramTester histogram_tester;
  TriggerDefaultStatusCheckForTesting(
      prefs(),
      SucceedingSystemCall(UIApplicationCategoryDefaultStatusIsDefault),
      /*cohort_override=*/1);

  histogram_tester.ExpectUniqueSample("IOS.DefaultStatusAPI.OutcomeType",
                                      DefaultStatusAPIOutcomeType::kSuccess, 1);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.CooldownError.DaysLeft", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.DaysSinceLastSuccessfulCall", 0);
  histogram_tester.ExpectUniqueSample("IOS.DefaultStatusAPI.IsDefaultBrowser",
                                      true, 1);
  histogram_tester.ExpectUniqueSample(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort1", true, 1);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort2", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort3", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort4", 0);
  histogram_tester.ExpectUniqueSample(
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

// Tests that a client in Cohort 2 that queries outside its reporting window
// logs `IsDefaultBrowser` and `IsDefaultBrowser.Cohort2` without logging
// `IsDefaultBrowser.StrictCohorts`, and schedules its next retry for Cohort 2's
// next reporting window.
TEST_F(DefaultStatusHelperTest, NonStrictCohort2IsNotDefaultRecurring)
API_AVAILABLE(ios(18.4)) {
  if (!@available(iOS 18.4, *)) {
    return;
  }

  base::subtle::ScopedTimeClockOverrides time_override(
      &DefaultStatusHelperTest::NowOverride, nullptr, nullptr);

  // First successful query in February (Cohort 2's strict window) with
  // `IsDefault`. Next retry is scheduled for `2025-06-01`.
  SetNowFromUTCString("2025-02-03");
  TriggerDefaultStatusCheckForTesting(
      prefs(),
      SucceedingSystemCall(UIApplicationCategoryDefaultStatusIsDefault),
      /*cohort_override=*/2);

  // Client does not launch in June; next launch is 150 days later in July
  // (`2025-07-03`), which is outside Cohort 2's reporting window.
  SetNowFromUTCString("2025-07-03");
  base::HistogramTester histogram_tester;
  TriggerDefaultStatusCheckForTesting(
      prefs(),
      SucceedingSystemCall(UIApplicationCategoryDefaultStatusNotDefault),
      /*cohort_override=*/2);

  histogram_tester.ExpectUniqueSample("IOS.DefaultStatusAPI.OutcomeType",
                                      DefaultStatusAPIOutcomeType::kSuccess, 1);
  histogram_tester.ExpectUniqueSample(
      "IOS.DefaultStatusAPI.DaysSinceLastSuccessfulCall", 150, 1);
  histogram_tester.ExpectUniqueSample("IOS.DefaultStatusAPI.IsDefaultBrowser",
                                      false, 1);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort1", 0);
  histogram_tester.ExpectUniqueSample(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort2", false, 1);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort3", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort4", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.StrictCohorts", 0);
  histogram_tester.ExpectUniqueSample(
      "IOS.DefaultStatusAPI.DefaultStatusRetention",
      DefaultStatusRetention::kBecameNonDefault, 1);

  // Next retry is scheduled for Cohort 2's October window (`2025-10-01`).
  SetNowFromUTCString("2025-09-30");
  TriggerDefaultStatusCheckForTesting(prefs(), UnexpectedSystemCall(),
                                      /*cohort_override=*/2);
  SetNowFromUTCString("2025-10-01");
  TriggerDefaultStatusCheckForTesting(
      prefs(),
      SucceedingSystemCall(UIApplicationCategoryDefaultStatusNotDefault),
      /*cohort_override=*/2);
  histogram_tester.ExpectTotalCount("IOS.DefaultStatusAPI.OutcomeType", 2);
}

// Tests Cohort 3 and Cohort 4 reporting along with all four
// `DefaultStatusRetention` transitions (`kRemainedDefault`,
// `kBecameNonDefault`, `kRemainedNonDefault`, `kBecameDefault`).
TEST_F(DefaultStatusHelperTest, StrictCohorts3And4AndAllRetentionTransitions)
API_AVAILABLE(ios(18.4)) {
  if (!@available(iOS 18.4, *)) {
    return;
  }

  base::subtle::ScopedTimeClockOverrides time_override(
      &DefaultStatusHelperTest::NowOverride, nullptr, nullptr);
  base::HistogramTester histogram_tester;

  // 1. Cohort 3 initial call in March (`kIsDefault`).
  SetNowFromUTCString("2025-03-15");
  TriggerDefaultStatusCheckForTesting(
      prefs(),
      SucceedingSystemCall(UIApplicationCategoryDefaultStatusIsDefault),
      /*cohort_override=*/3);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort3", true, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.StrictCohorts", true, 1);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.DefaultStatusRetention", 0);

  // 2. Cohort 3 second call in July (`kIsDefault` -> `kIsDefault`:
  // `kRemainedDefault`).
  SetNowFromUTCString("2025-07-15");
  TriggerDefaultStatusCheckForTesting(
      prefs(),
      SucceedingSystemCall(UIApplicationCategoryDefaultStatusIsDefault),
      /*cohort_override=*/3);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.DefaultStatusRetention",
      DefaultStatusRetention::kRemainedDefault, 1);

  // 3. Cohort 3 third call in November (`kIsDefault` -> `kIsNotDefault`:
  // `kBecameNonDefault`).
  SetNowFromUTCString("2025-11-15");
  TriggerDefaultStatusCheckForTesting(
      prefs(),
      SucceedingSystemCall(UIApplicationCategoryDefaultStatusNotDefault),
      /*cohort_override=*/3);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.DefaultStatusRetention",
      DefaultStatusRetention::kBecameNonDefault, 1);

  // 4. Cohort 3 fourth call in March 2026 (`kIsNotDefault` -> `kIsNotDefault`:
  // `kRemainedNonDefault`).
  SetNowFromUTCString("2026-03-15");
  TriggerDefaultStatusCheckForTesting(
      prefs(),
      SucceedingSystemCall(UIApplicationCategoryDefaultStatusNotDefault),
      /*cohort_override=*/3);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.DefaultStatusRetention",
      DefaultStatusRetention::kRemainedNonDefault, 1);

  // 5. Cohort 3 fifth call in July 2026 (`kIsNotDefault` -> `kIsDefault`:
  // `kBecameDefault`).
  SetNowFromUTCString("2026-07-15");
  TriggerDefaultStatusCheckForTesting(
      prefs(),
      SucceedingSystemCall(UIApplicationCategoryDefaultStatusIsDefault),
      /*cohort_override=*/3);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultStatusAPI.DefaultStatusRetention",
      DefaultStatusRetention::kBecameDefault, 1);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultStatusAPI.DefaultStatusRetention", 4);

  // Verify Cohort 4 strict reporting on a separate preference store.
  TestingPrefServiceSimple cohort4_prefs;
  RegisterDefaultStatusPrefs(cohort4_prefs.registry());
  SetNowFromUTCString("2026-08-15");
  TriggerDefaultStatusCheckForTesting(
      &cohort4_prefs,
      SucceedingSystemCall(UIApplicationCategoryDefaultStatusNotDefault),
      /*cohort_override=*/4);
  histogram_tester.ExpectUniqueSample(
      "IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort4", false, 1);
}

// Tests that `TriggerDefaultStatusCheckForTesting` records the expected
// `DefaultStatusHeuristicAssessment` across all 8 lookback windows (`1, 3, 7,
// 14, 21, 28, 35, 42` days) as `lastHTTPURLOpenTime` ages.
TEST_F(DefaultStatusHelperTest, HeuristicAssessmentsAcrossLookbackWindows)
API_AVAILABLE(ios(18.4)) {
  if (!@available(iOS 18.4, *)) {
    return;
  }

  base::subtle::ScopedTimeClockOverrides time_override(
      &DefaultStatusHelperTest::NowOverride, nullptr, nullptr);
  SetNowFromUTCString("2025-05-15");

  static constexpr int kWindows[] = {1, 3, 7, 14, 21, 28, 35, 42};
  static constexpr base::TimeDelta kTimesSinceOpen[] = {
      base::Days(1) - base::Minutes(10),
      base::Days(3) - base::Minutes(10),
      base::Days(7) - base::Minutes(10),
      base::Days(14) - base::Minutes(10),
      base::Days(21) - base::Minutes(10),
      base::Days(28) - base::Minutes(10),
      base::Days(35) - base::Minutes(10),
      base::Days(42) - base::Minutes(10),
      base::Days(45),
  };

  for (base::TimeDelta time_since_open : kTimesSinceOpen) {
    SCOPED_TRACE(base::StringPrintf("minutes_since_open=%d",
                                    time_since_open.InMinutes()));
    SetObjectIntoStorageForKey(
        kLastHTTPURLOpenTime, (base::Time::Now() - time_since_open).ToNSDate());

    // System reports Chrome IS default: expired windows are `kFalseNegative`,
    // active windows are `kTruePositive`.
    {
      TestingPrefServiceSimple call_prefs;
      RegisterDefaultStatusPrefs(call_prefs.registry());
      base::HistogramTester histogram_tester;
      TriggerDefaultStatusCheckForTesting(
          &call_prefs,
          SucceedingSystemCall(UIApplicationCategoryDefaultStatusIsDefault),
          /*cohort_override=*/1);
      for (int window : kWindows) {
        const bool expired = base::Days(window) <= time_since_open;
        histogram_tester.ExpectUniqueSample(
            base::StringPrintf("IOS.DefaultStatusAPI.HeuristicAssessment%d",
                               window),
            expired ? DefaultStatusHeuristicAssessment::kFalseNegative
                    : DefaultStatusHeuristicAssessment::kTruePositive,
            1);
      }
    }

    // System reports Chrome is NOT default: expired windows are
    // `kTrueNegative`, active windows are `kFalsePositive`.
    {
      TestingPrefServiceSimple call_prefs;
      RegisterDefaultStatusPrefs(call_prefs.registry());
      base::HistogramTester histogram_tester;
      TriggerDefaultStatusCheckForTesting(
          &call_prefs,
          SucceedingSystemCall(UIApplicationCategoryDefaultStatusNotDefault),
          /*cohort_override=*/1);
      for (int window : kWindows) {
        const bool expired = base::Days(window) <= time_since_open;
        histogram_tester.ExpectUniqueSample(
            base::StringPrintf("IOS.DefaultStatusAPI.HeuristicAssessment%d",
                               window),
            expired ? DefaultStatusHeuristicAssessment::kTrueNegative
                    : DefaultStatusHeuristicAssessment::kFalsePositive,
            1);
      }
    }
  }
}

}  // namespace default_status
