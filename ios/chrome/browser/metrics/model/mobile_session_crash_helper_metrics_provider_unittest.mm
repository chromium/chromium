// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/mobile_session_crash_helper_metrics_provider.h"

#import <Foundation/Foundation.h>

#import <tuple>

#import "base/containers/fixed_flat_map.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/previous_session_info/previous_session_info.h"
#import "components/previous_session_info/previous_session_info_private.h"
#import "ios/chrome/test/testing_application_context.h"
#import "testing/platform_test.h"

namespace {

// A tuple of 4 bools
// - received memory warning
// - crash log present
// - last shutdown was clean
// - first launch after upgrade
using FourBooleans = std::tuple<bool, bool, bool, bool>;

// Expected bucket for each possible value of FourBooleans.
static constexpr auto kExpectedShutdownTypes =
    base::MakeFixedFlatMap<FourBooleans, MobileSessionShutdownType>({
        {
            {false, false, false, false},
            SHUTDOWN_IN_FOREGROUND_NO_CRASH_LOG_NO_MEMORY_WARNING,
        },
        {
            {true, false, false, false},
            SHUTDOWN_IN_FOREGROUND_NO_CRASH_LOG_WITH_MEMORY_WARNING,
        },
        {
            {false, true, false, false},
            SHUTDOWN_IN_FOREGROUND_WITH_CRASH_LOG_NO_MEMORY_WARNING,
        },
        {
            {true, true, false, false},
            SHUTDOWN_IN_FOREGROUND_WITH_CRASH_LOG_WITH_MEMORY_WARNING,
        },

        // If was_last_shutdown_clean is true, the memory warning and crash
        // log don't matter.
        {
            {false, false, true, false},
            SHUTDOWN_IN_BACKGROUND,
        },
        {
            {true, false, true, false},
            SHUTDOWN_IN_BACKGROUND,
        },
        {
            {false, true, true, false},
            SHUTDOWN_IN_BACKGROUND,
        },
        {
            {true, true, true, false},
            SHUTDOWN_IN_BACKGROUND,
        },

        // If is_first_launch_after_upgrade is true, the other flags don't
        // matter except was_last_shutdown_clean.
        {
            {false, false, false, true},
            FIRST_LAUNCH_AFTER_UPGRADE,
        },
        {
            {true, false, false, true},
            FIRST_LAUNCH_AFTER_UPGRADE,
        },
        {
            {false, true, false, true},
            FIRST_LAUNCH_AFTER_UPGRADE,
        },
        {
            {true, true, false, true},
            FIRST_LAUNCH_AFTER_UPGRADE,
        },
        {
            {false, false, true, true},
            SHUTDOWN_IN_BACKGROUND,
        },
        {
            {true, false, true, true},
            SHUTDOWN_IN_BACKGROUND,
        },
        {
            {false, true, true, true},
            SHUTDOWN_IN_BACKGROUND,
        },
        {
            {true, true, true, true},
            SHUTDOWN_IN_BACKGROUND,
        },
    });

}  // namespace

class MobileSessionCrashHelperMetricsProviderTest
    : public PlatformTest,
      public testing::WithParamInterface<FourBooleans> {
 public:
  MobileSessionCrashHelperMetricsProviderTest() {
    TestingApplicationContext::GetGlobal()->SetLastShutdownClean(false);
    [PreviousSessionInfo resetSharedInstanceForTesting];
    [[PreviousSessionInfo sharedInstance] setIsFirstSessionAfterUpgrade:NO];
  }

  void TearDown() override {
    [PreviousSessionInfo resetSharedInstanceForTesting];
    TestingApplicationContext::GetGlobal()->SetLastShutdownClean(false);
    PlatformTest::TearDown();
  }

 protected:
  base::HistogramTester histogram_tester_;
};

TEST_P(MobileSessionCrashHelperMetricsProviderTest, RecordsHistogramOnSignal) {
  const bool received_memory_warning = std::get<0>(GetParam());
  const bool has_crash_logs = std::get<1>(GetParam());
  const bool was_last_shutdown_clean = std::get<2>(GetParam());
  const bool is_first_launch_after_upgrade = std::get<3>(GetParam());

  const auto expected_bucket_iter = kExpectedShutdownTypes.find(GetParam());
  ASSERT_TRUE(expected_bucket_iter != kExpectedShutdownTypes.end());

  TestingApplicationContext::GetGlobal()->SetLastShutdownClean(
      was_last_shutdown_clean);
  [[PreviousSessionInfo sharedInstance]
      setIsFirstSessionAfterUpgrade:is_first_launch_after_upgrade];
  [[PreviousSessionInfo sharedInstance]
      setDidSeeMemoryWarningShortlyBeforeTerminating:received_memory_warning];

  mobile_session_metrics::OnProcessIntermediateDumpsFinished(has_crash_logs);

  histogram_tester_.ExpectUniqueSample("Stability.MobileSessionShutdownType",
                                       expected_bucket_iter->second, 1);
}

INSTANTIATE_TEST_SUITE_P(/* No InstantiationName */,
                         MobileSessionCrashHelperMetricsProviderTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

// Tests Stability.iOS.TabCountBefore* metrics recording after clean shutdown.
TEST_F(MobileSessionCrashHelperMetricsProviderTest,
       TabCountMetricCleanShutdown) {
  [PreviousSessionInfo sharedInstance].tabCount = 2;
  [PreviousSessionInfo sharedInstance].OTRTabCount = 3;
  TestingApplicationContext::GetGlobal()->SetLastShutdownClean(true);

  mobile_session_metrics::OnProcessIntermediateDumpsFinished(false);

  histogram_tester_.ExpectTotalCount(
      "Stability.iOS.TabCountBeforeCleanShutdown", 0);
  histogram_tester_.ExpectTotalCount("Stability.iOS.TabCountBeforeCrash", 0);
  histogram_tester_.ExpectTotalCount("Stability.iOS.TabCountBeforeUTE", 0);
  histogram_tester_.ExpectTotalCount("Stability.iOS.TabCountBeforeSignalCrash",
                                     0);
}

// Tests Stability.iOS.TabCountBefore* metrics recording after
// Unexplained Termination Event or Explained Termination Event.
TEST_F(MobileSessionCrashHelperMetricsProviderTest, TabCountMetricUte) {
  [PreviousSessionInfo sharedInstance].tabCount = 2;
  [PreviousSessionInfo sharedInstance].OTRTabCount = 3;
  TestingApplicationContext::GetGlobal()->SetLastShutdownClean(false);

  mobile_session_metrics::OnProcessIntermediateDumpsFinished(false);

  histogram_tester_.ExpectTotalCount(
      "Stability.iOS.TabCountBeforeCleanShutdown", 0);
  histogram_tester_.ExpectTotalCount("Stability.iOS.TabCountBeforeCrash", 0);
  histogram_tester_.ExpectUniqueSample("Stability.iOS.TabCountBeforeUTE", 5, 1);
  histogram_tester_.ExpectTotalCount("Stability.iOS.TabCountBeforeSignalCrash",
                                     0);
}

// Tests Stability.iOS.TabCountBefore* metrics recording after crash with log.
TEST_F(MobileSessionCrashHelperMetricsProviderTest,
       TabCountMetricCrashWithLog) {
  [PreviousSessionInfo sharedInstance].tabCount = 2;
  [PreviousSessionInfo sharedInstance].OTRTabCount = 3;
  TestingApplicationContext::GetGlobal()->SetLastShutdownClean(false);

  mobile_session_metrics::OnProcessIntermediateDumpsFinished(true);

  histogram_tester_.ExpectTotalCount(
      "Stability.iOS.TabCountBeforeCleanShutdown", 0);
  histogram_tester_.ExpectTotalCount("Stability.iOS.TabCountBeforeCrash", 0);
  histogram_tester_.ExpectTotalCount("Stability.iOS.TabCountBeforeUTE", 0);
  histogram_tester_.ExpectUniqueSample(
      "Stability.iOS.TabCountBeforeSignalCrash", 5, 1);
}

// Tests logging the following metrics:
//   - Stability.iOS.UTE.HasPossibleExplanation
//   - Stability.iOS.UTE.OSRestartedAfterPreviousSession
TEST_F(MobileSessionCrashHelperMetricsProviderTest,
       ProvideHasPossibleExplanationMetric) {
  TestingApplicationContext::GetGlobal()->SetLastShutdownClean(false);

  // Test UTE with no possible explanation.
  {
    base::HistogramTester histogram_tester;
    [PreviousSessionInfo resetSharedInstanceForTesting];
    [[PreviousSessionInfo sharedInstance] setIsFirstSessionAfterUpgrade:NO];
    [PreviousSessionInfo sharedInstance].OSRestartedAfterPreviousSession = NO;
    mobile_session_metrics::OnProcessIntermediateDumpsFinished(false);
    histogram_tester.ExpectUniqueSample(
        "Stability.iOS.UTE.OSRestartedAfterPreviousSession", false, 1);
    histogram_tester.ExpectUniqueSample(
        "Stability.iOS.UTE.HasPossibleExplanation", false, 1);
  }

  // Test UTE when OS restarted after previous session.
  {
    base::HistogramTester histogram_tester;
    [PreviousSessionInfo sharedInstance].OSRestartedAfterPreviousSession = YES;
    mobile_session_metrics::OnProcessIntermediateDumpsFinished(false);
    histogram_tester.ExpectUniqueSample(
        "Stability.iOS.UTE.OSRestartedAfterPreviousSession", true, 1);
    histogram_tester.ExpectUniqueSample(
        "Stability.iOS.UTE.HasPossibleExplanation", true, 1);
  }
}
