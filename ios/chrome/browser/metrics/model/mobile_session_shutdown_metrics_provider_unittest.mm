// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/mobile_session_shutdown_metrics_provider.h"

#import <Foundation/Foundation.h>

#import <memory>
#import <string>
#import <tuple>

#import "base/containers/fixed_flat_map.h"
#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/test_simple_task_runner.h"
#import "components/metrics/metrics_pref_names.h"
#import "components/metrics/metrics_service.h"
#import "components/metrics/metrics_state_manager.h"
#import "components/metrics/test/test_enabled_state_provider.h"
#import "components/metrics/test/test_metrics_service_client.h"
#import "components/prefs/testing_pref_service.h"
#import "components/previous_session_info/previous_session_info.h"
#import "components/previous_session_info/previous_session_info_private.h"
#import "testing/gtest/include/gtest/gtest-param-test.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// A tuple of 5 bools.
using FiveBooleans = std::tuple<bool, bool, bool, bool, bool>;

// Expected bucket for each possible value of FiveBooleans.
static constexpr auto kExpectedShutdownTypes =
    base::MakeFixedFlatMap<FiveBooleans, MobileSessionShutdownType>({
        {
            {false, false, false, false, false},
            SHUTDOWN_IN_FOREGROUND_NO_CRASH_LOG_NO_MEMORY_WARNING,
        },
        {
            {true, false, false, false, false},
            SHUTDOWN_IN_FOREGROUND_NO_CRASH_LOG_WITH_MEMORY_WARNING,
        },
        {
            {false, true, false, false, false},
            SHUTDOWN_IN_FOREGROUND_WITH_CRASH_LOG_NO_MEMORY_WARNING,
        },
        {
            {true, true, false, false, false},
            SHUTDOWN_IN_FOREGROUND_WITH_CRASH_LOG_WITH_MEMORY_WARNING,
        },
        {
            {false, false, true, false, false},
            SHUTDOWN_IN_FOREGROUND_WITH_MAIN_THREAD_FROZEN,
        },
        {
            {true, false, true, false, false},
            SHUTDOWN_IN_FOREGROUND_WITH_MAIN_THREAD_FROZEN,
        },
        {
            {false, true, true, false, false},
            SHUTDOWN_IN_FOREGROUND_WITH_CRASH_LOG_NO_MEMORY_WARNING,
        },
        {
            {true, true, true, false, false},
            SHUTDOWN_IN_FOREGROUND_WITH_CRASH_LOG_WITH_MEMORY_WARNING,
        },

        // If was_last_shutdown_clean is true, the memory warning and crash
        // log don't matter.
        {
            {false, false, false, true, false},
            SHUTDOWN_IN_BACKGROUND,
        },
        {
            {true, false, false, true, false},
            SHUTDOWN_IN_BACKGROUND,
        },
        {
            {false, true, false, true, false},
            SHUTDOWN_IN_BACKGROUND,
        },
        {
            {true, true, false, true, false},
            SHUTDOWN_IN_BACKGROUND,
        },
        {
            {false, false, true, true, false},
            SHUTDOWN_IN_BACKGROUND,
        },
        {
            {true, false, true, true, false},
            SHUTDOWN_IN_BACKGROUND,
        },
        {
            {false, true, true, true, false},
            SHUTDOWN_IN_BACKGROUND,
        },
        {
            {true, true, true, true, false},
            SHUTDOWN_IN_BACKGROUND,
        },

        // If is_first_launch_after_upgrade is true, the other flags don't
        // matter.
        {
            {false, false, false, false, true},
            FIRST_LAUNCH_AFTER_UPGRADE,
        },
        {
            {true, false, false, false, true},
            FIRST_LAUNCH_AFTER_UPGRADE,
        },
        {
            {false, true, false, false, true},
            FIRST_LAUNCH_AFTER_UPGRADE,
        },
        {
            {true, true, false, false, true},
            FIRST_LAUNCH_AFTER_UPGRADE,
        },
        {
            {false, false, true, false, true},
            FIRST_LAUNCH_AFTER_UPGRADE,
        },
        {
            {true, false, true, false, true},
            FIRST_LAUNCH_AFTER_UPGRADE,
        },
        {
            {false, true, true, false, true},
            FIRST_LAUNCH_AFTER_UPGRADE,
        },
        {
            {true, true, true, false, true},
            FIRST_LAUNCH_AFTER_UPGRADE,
        },
        {
            {false, false, false, true, true},
            FIRST_LAUNCH_AFTER_UPGRADE,
        },
        {
            {true, false, false, true, true},
            FIRST_LAUNCH_AFTER_UPGRADE,
        },
        {
            {false, true, false, true, true},
            FIRST_LAUNCH_AFTER_UPGRADE,
        },
        {
            {true, true, false, true, true},
            FIRST_LAUNCH_AFTER_UPGRADE,
        },
        {
            {false, false, true, true, true},
            FIRST_LAUNCH_AFTER_UPGRADE,
        },
        {
            {true, false, true, true, true},
            FIRST_LAUNCH_AFTER_UPGRADE,
        },
        {
            {false, true, true, true, true},
            FIRST_LAUNCH_AFTER_UPGRADE,
        },
        {
            {true, true, true, true, true},
            FIRST_LAUNCH_AFTER_UPGRADE,
        },
    });

}  // namespace

// An MobileSessionShutdownMetricsProvider that returns fake values for the last
// session environment query methods.
class MobileSessionShutdownMetricsProviderForTesting
    : public MobileSessionShutdownMetricsProvider {
 public:
  explicit MobileSessionShutdownMetricsProviderForTesting(
      metrics::MetricsService* metrics_service)
      : MobileSessionShutdownMetricsProvider(metrics_service) {}

  MobileSessionShutdownMetricsProviderForTesting(
      const MobileSessionShutdownMetricsProviderForTesting&) = delete;
  MobileSessionShutdownMetricsProviderForTesting& operator=(
      const MobileSessionShutdownMetricsProviderForTesting&) = delete;

  void set_is_first_launch_after_upgrade(bool value) {
    is_first_launch_after_upgrade_ = value;
  }
  void set_has_crash_logs(bool value) { has_crash_logs_ = value; }
  void set_received_memory_warning_before_last_shutdown(bool value) {
    received_memory_warning_before_last_shutdown_ = value;
  }
  void set_was_last_shutdown_frozen(bool value) {
    was_last_shutdown_frozen_ = value;
  }

 protected:
  // MobileSessionShutdownMetricsProvider:
  bool IsFirstLaunchAfterUpgrade() override {
    return is_first_launch_after_upgrade_;
  }
  bool HasCrashLogs() override { return has_crash_logs_; }
  bool ReceivedMemoryWarningBeforeLastShutdown() override {
    return received_memory_warning_before_last_shutdown_;
  }
  bool LastSessionEndedFrozen() override { return was_last_shutdown_frozen_; }

 private:
  bool is_first_launch_after_upgrade_ = false;
  bool has_crash_logs_ = false;
  bool received_memory_warning_before_last_shutdown_ = false;
  bool was_last_shutdown_frozen_ = false;
};

class MobileSessionShutdownMetricsProviderTest
    : public PlatformTest,
      public testing::WithParamInterface<FiveBooleans> {
 public:
  MobileSessionShutdownMetricsProviderTest() {
    metrics::MetricsService::RegisterPrefs(local_state_.registry());
  }

  MobileSessionShutdownMetricsProviderTest(
      const MobileSessionShutdownMetricsProviderTest&) = delete;
  MobileSessionShutdownMetricsProviderTest& operator=(
      const MobileSessionShutdownMetricsProviderTest&) = delete;

  // Initializes the MetricsStateManager, CleanExitBeacon, and MetricsService.
  void InitializeMetrics() {
    enabled_state_provider_ =
        std::make_unique<metrics::TestEnabledStateProvider>(/*consent=*/false,
                                                            /*enabled=*/false);
    metrics_state_ = metrics::MetricsStateManager::Create(
        &local_state_, enabled_state_provider_.get(),
        /*backup_registry_key=*/{},
        /*user_data_dir=*/{});
    metrics_state_->InstantiateFieldTrialList();
    metrics_service_ = std::make_unique<metrics::MetricsService>(
        metrics_state_.get(), &metrics_client_, &local_state_);
  }

 protected:
  TestingPrefServiceSimple local_state_;
  metrics::TestMetricsServiceClient metrics_client_;
  std::unique_ptr<metrics::EnabledStateProvider> enabled_state_provider_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_;
  std::unique_ptr<metrics::MetricsService> metrics_service_;
};

// Verifies that a sample is recorded in the correct bucket of the shutdown type
// histogram when ProvideStabilityMetrics is invoked.
//
// This parameterized test receives a tuple of 5 booleans, which corresponds to:
//  - received memory warning;
//  - crash log present;
//  - last shutdown was frozen;
//  - last shutdown was clean;
//  - first launch after upgrade.
TEST_P(MobileSessionShutdownMetricsProviderTest, ProvideStabilityMetrics) {
  const bool received_memory_warning = std::get<0>(GetParam());
  const bool has_crash_logs = std::get<1>(GetParam());
  const bool was_last_shutdown_frozen = std::get<2>(GetParam());
  const bool was_last_shutdown_clean = std::get<3>(GetParam());
  const bool is_first_launch_after_upgrade = std::get<4>(GetParam());

  const auto expected_bucket_iter = kExpectedShutdownTypes.find(GetParam());
  ASSERT_TRUE(expected_bucket_iter != kExpectedShutdownTypes.end());

  metrics::CleanExitBeacon::SetStabilityExitedCleanlyForTesting(
      &local_state_, was_last_shutdown_clean);

  // Set up the MetricsService and metrics provider.
  InitializeMetrics();

  // Setup the metrics provider for the current test.
  MobileSessionShutdownMetricsProviderForTesting metrics_provider(
      metrics_service_.get());
  metrics_provider.set_is_first_launch_after_upgrade(
      is_first_launch_after_upgrade);
  metrics_provider.set_received_memory_warning_before_last_shutdown(
      received_memory_warning);
  metrics_provider.set_has_crash_logs(has_crash_logs);
  metrics_provider.set_was_last_shutdown_frozen(was_last_shutdown_frozen);

  // Create a histogram tester for verifying samples written to the shutdown
  // type histogram.
  base::HistogramTester histogram_tester;

  // Now call the method under test and verify exactly one sample is written to
  // the expected bucket.
  metrics_provider.ProvidePreviousSessionData(nullptr);
  histogram_tester.ExpectUniqueSample("Stability.MobileSessionShutdownType",
                                      expected_bucket_iter->second, 1);
  [PreviousSessionInfo resetSharedInstanceForTesting];
}

// Tests Stability.iOS.TabCountBefore* metrics recording after clean shutdown.
TEST_F(MobileSessionShutdownMetricsProviderTest, TabCountMetricCleanShutdown) {
  [PreviousSessionInfo sharedInstance].tabCount = 2;
  [PreviousSessionInfo sharedInstance].OTRTabCount = 3;
  metrics::CleanExitBeacon::SetStabilityExitedCleanlyForTesting(&local_state_,
                                                                true);

  // Set up the MetricsService and metrics provider.
  InitializeMetrics();
  MobileSessionShutdownMetricsProviderForTesting metrics_provider(
      metrics_service_.get());

  // Create a histogram tester for verifying samples written to the shutdown
  // type histogram.
  base::HistogramTester histogram_tester;

  // Now call the method under test and verify exactly one sample is written to
  // the expected bucket.
  metrics_provider.ProvidePreviousSessionData(nullptr);
  histogram_tester.ExpectUniqueSample(
      "Stability.iOS.TabCountBeforeCleanShutdown", 5, 1);
  histogram_tester.ExpectTotalCount("Stability.iOS.TabCountBeforeCrash", 0);
  histogram_tester.ExpectTotalCount("Stability.iOS.TabCountBeforeUTE", 0);
  histogram_tester.ExpectTotalCount("Stability.iOS.TabCountBeforeSignalCrash",
                                    0);
  histogram_tester.ExpectTotalCount("Stability.iOS.TabCountBeforeFreeze", 0);

  [PreviousSessionInfo resetSharedInstanceForTesting];
}

// Tests Stability.iOS.TabCountBefore* metrics recording after
// Unexplained Termination Event or Explained Termination Event.
TEST_F(MobileSessionShutdownMetricsProviderTest, TabCountMetricUte) {
  [PreviousSessionInfo sharedInstance].tabCount = 2;
  [PreviousSessionInfo sharedInstance].OTRTabCount = 3;
  metrics::CleanExitBeacon::SetStabilityExitedCleanlyForTesting(&local_state_,
                                                                false);

  // Set up the MetricsService and metrics provider.
  InitializeMetrics();
  MobileSessionShutdownMetricsProviderForTesting metrics_provider(
      metrics_service_.get());

  // Create a histogram tester for verifying samples written to the shutdown
  // type histogram.
  base::HistogramTester histogram_tester;

  // Now call the method under test and verify exactly one sample is written to
  // the expected bucket.
  metrics_provider.ProvidePreviousSessionData(nullptr);
  histogram_tester.ExpectTotalCount("Stability.iOS.TabCountBeforeCleanShutdown",
                                    0);
  histogram_tester.ExpectUniqueSample("Stability.iOS.TabCountBeforeCrash", 5,
                                      1);
  histogram_tester.ExpectUniqueSample("Stability.iOS.TabCountBeforeUTE", 5, 1);
  histogram_tester.ExpectTotalCount("Stability.iOS.TabCountBeforeSignalCrash",
                                    0);
  histogram_tester.ExpectTotalCount("Stability.iOS.TabCountBeforeFreeze", 0);

  [PreviousSessionInfo resetSharedInstanceForTesting];
}

// Tests Stability.iOS.TabCountBefore* metrics recording after crash with log.
TEST_F(MobileSessionShutdownMetricsProviderTest, TabCountMetricCrashWithLog) {
  [PreviousSessionInfo sharedInstance].tabCount = 2;
  [PreviousSessionInfo sharedInstance].OTRTabCount = 3;
  metrics::CleanExitBeacon::SetStabilityExitedCleanlyForTesting(&local_state_,
                                                                false);

  // Set up the MetricsService and metrics provider.
  InitializeMetrics();
  MobileSessionShutdownMetricsProviderForTesting metrics_provider(
      metrics_service_.get());
  metrics_provider.set_has_crash_logs(true);

  // Create a histogram tester for verifying samples written to the shutdown
  // type histogram.
  base::HistogramTester histogram_tester;

  // Now call the method under test and verify exactly one sample is written to
  // the expected bucket.
  metrics_provider.ProvidePreviousSessionData(nullptr);
  histogram_tester.ExpectTotalCount("Stability.iOS.TabCountBeforeCleanShutdown",
                                    0);
  histogram_tester.ExpectUniqueSample("Stability.iOS.TabCountBeforeCrash", 5,
                                      1);
  histogram_tester.ExpectTotalCount("Stability.iOS.TabCountBeforeUTE", 0);
  histogram_tester.ExpectUniqueSample("Stability.iOS.TabCountBeforeSignalCrash",
                                      5, 1);
  histogram_tester.ExpectTotalCount("Stability.iOS.TabCountBeforeFreeze", 0);

  [PreviousSessionInfo resetSharedInstanceForTesting];
}

// Tests Stability.iOS.TabCountBefore* metrics recording after UI Freeze.
TEST_F(MobileSessionShutdownMetricsProviderTest, TabCountMetricFreeze) {
  [PreviousSessionInfo sharedInstance].tabCount = 2;
  [PreviousSessionInfo sharedInstance].OTRTabCount = 3;
  metrics::CleanExitBeacon::SetStabilityExitedCleanlyForTesting(&local_state_,
                                                                false);

  // Set up the MetricsService and metrics provider.
  InitializeMetrics();
  MobileSessionShutdownMetricsProviderForTesting metrics_provider(
      metrics_service_.get());
  metrics_provider.set_was_last_shutdown_frozen(true);

  // Create a histogram tester for verifying samples written to the shutdown
  // type histogram.
  base::HistogramTester histogram_tester;

  // Now call the method under test and verify exactly one sample is written to
  // the expected bucket.
  metrics_provider.ProvidePreviousSessionData(nullptr);
  histogram_tester.ExpectTotalCount("Stability.iOS.TabCountBeforeCleanShutdown",
                                    0);
  histogram_tester.ExpectUniqueSample("Stability.iOS.TabCountBeforeCrash", 5,
                                      1);
  histogram_tester.ExpectTotalCount("Stability.iOS.TabCountBeforeUTE", 0);
  histogram_tester.ExpectTotalCount("Stability.iOS.TabCountBeforeSignalCrash",
                                    0);
  histogram_tester.ExpectUniqueSample("Stability.iOS.TabCountBeforeFreeze", 5,
                                      1);

  [PreviousSessionInfo resetSharedInstanceForTesting];
}

// Tests logging the following metrics:
//   - Stability.iOS.UTE.HasPossibleExplanation
//   - Stability.iOS.UTE.OSRestartedAfterPreviousSession
TEST_F(MobileSessionShutdownMetricsProviderTest,
       ProvideHasPossibleExplanationMetric) {
  metrics::CleanExitBeacon::SetStabilityExitedCleanlyForTesting(&local_state_,
                                                                false);

  // Set up the MetricsService and metrics provider.
  InitializeMetrics();
  MobileSessionShutdownMetricsProviderForTesting metrics_provider(
      metrics_service_.get());

  // Test UTE with no possible explanation.
  {
    base::HistogramTester histogram_tester;
    [PreviousSessionInfo resetSharedInstanceForTesting];
    [PreviousSessionInfo sharedInstance].OSRestartedAfterPreviousSession = NO;
    metrics_provider.ProvidePreviousSessionData(nullptr);
    histogram_tester.ExpectUniqueSample(
        "Stability.iOS.UTE.OSRestartedAfterPreviousSession", false, 1);
    histogram_tester.ExpectUniqueSample(
        "Stability.iOS.UTE.HasPossibleExplanation", false, 1);
  }

  // Test UTE when OS restarted after previous session.
  {
    base::HistogramTester histogram_tester;
    [PreviousSessionInfo sharedInstance].OSRestartedAfterPreviousSession = YES;
    metrics_provider.ProvidePreviousSessionData(nullptr);
    histogram_tester.ExpectUniqueSample(
        "Stability.iOS.UTE.OSRestartedAfterPreviousSession", true, 1);
    histogram_tester.ExpectUniqueSample(
        "Stability.iOS.UTE.HasPossibleExplanation", true, 1);
  }
}

INSTANTIATE_TEST_SUITE_P(/* No InstantiationName */,
                         MobileSessionShutdownMetricsProviderTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));
