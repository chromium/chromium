// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/mobile_session_shutdown_metrics_provider.h"

#import <memory>
#import <string>

#import <Foundation/Foundation.h>

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
      public testing::WithParamInterface<int> {
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
    metrics_state_ = metrics::MetricsStateManager::Create(
        &local_state_, new metrics::TestEnabledStateProvider(false, false),
        std::wstring(), base::FilePath());
    metrics_state_->InstantiateFieldTrialList();
    metrics_service_.reset(new metrics::MetricsService(
        metrics_state_.get(), &metrics_client_, &local_state_));
  }

 protected:
  TestingPrefServiceSimple local_state_;
  metrics::TestMetricsServiceClient metrics_client_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_;
  std::unique_ptr<metrics::MetricsService> metrics_service_;
  std::unique_ptr<MobileSessionShutdownMetricsProviderForTesting>
      metrics_provider_;
};

// Verifies that a sample is recorded in the correct bucket of the shutdown type
// histogram when ProvideStabilityMetrics is invoked.
//
// This parameterized test receives a parameter in the range [0, 32), which is
// used to generate values for five booleans based on the binary representation
// of the parameter. The bits are assigned as follows (from least significant to
// most significant):
//  - received memory warning;
//  - crash log present;
//  - last shutdown was clean;
//  - first launch after upgrade.
TEST_P(MobileSessionShutdownMetricsProviderTest, ProvideStabilityMetrics) {
  const bool received_memory_warning = GetParam() % 2;
  const bool has_crash_logs = (GetParam() >> 1) % 2;
  const bool was_last_shutdown_frozen = (GetParam() >> 2) % 2;
  const bool was_last_shutdown_clean = (GetParam() >> 3) % 2;
  const bool is_first_launch_after_upgrade = (GetParam() >> 4) % 2;

  // Expected bucket for each possible value of GetParam().
  const MobileSessionShutdownType expected_buckets[] = {
      SHUTDOWN_IN_FOREGROUND_NO_CRASH_LOG_NO_MEMORY_WARNING,
      SHUTDOWN_IN_FOREGROUND_NO_CRASH_LOG_WITH_MEMORY_WARNING,
      SHUTDOWN_IN_FOREGROUND_WITH_CRASH_LOG_NO_MEMORY_WARNING,
      SHUTDOWN_IN_FOREGROUND_WITH_CRASH_LOG_WITH_MEMORY_WARNING,
      SHUTDOWN_IN_FOREGROUND_WITH_MAIN_THREAD_FROZEN,
      SHUTDOWN_IN_FOREGROUND_WITH_MAIN_THREAD_FROZEN,
      SHUTDOWN_IN_FOREGROUND_WITH_CRASH_LOG_NO_MEMORY_WARNING,
      SHUTDOWN_IN_FOREGROUND_WITH_CRASH_LOG_WITH_MEMORY_WARNING,
      // If wasLastShutdownClean is true, the memory warning and crash log don't
      // matter.
      SHUTDOWN_IN_BACKGROUND,
      SHUTDOWN_IN_BACKGROUND,
      SHUTDOWN_IN_BACKGROUND,
      SHUTDOWN_IN_BACKGROUND,
      SHUTDOWN_IN_BACKGROUND,
      SHUTDOWN_IN_BACKGROUND,
      SHUTDOWN_IN_BACKGROUND,
      SHUTDOWN_IN_BACKGROUND,
      // If firstLaunchAfterUpgrade is true, the other flags don't matter.
      FIRST_LAUNCH_AFTER_UPGRADE,
      FIRST_LAUNCH_AFTER_UPGRADE,
      FIRST_LAUNCH_AFTER_UPGRADE,
      FIRST_LAUNCH_AFTER_UPGRADE,
      FIRST_LAUNCH_AFTER_UPGRADE,
      FIRST_LAUNCH_AFTER_UPGRADE,
      FIRST_LAUNCH_AFTER_UPGRADE,
      FIRST_LAUNCH_AFTER_UPGRADE,
      FIRST_LAUNCH_AFTER_UPGRADE,
      FIRST_LAUNCH_AFTER_UPGRADE,
      FIRST_LAUNCH_AFTER_UPGRADE,
      FIRST_LAUNCH_AFTER_UPGRADE,
      FIRST_LAUNCH_AFTER_UPGRADE,
      FIRST_LAUNCH_AFTER_UPGRADE,
      FIRST_LAUNCH_AFTER_UPGRADE,
      FIRST_LAUNCH_AFTER_UPGRADE,
  };

  metrics::CleanExitBeacon::SetStabilityExitedCleanlyForTesting(
      &local_state_, was_last_shutdown_clean);

  // Set up the MetricsService and metrics provider.
  InitializeMetrics();

  // Setup the metrics provider for the current test.
  metrics_provider_.reset(new MobileSessionShutdownMetricsProviderForTesting(
      metrics_service_.get()));
  metrics_provider_->set_is_first_launch_after_upgrade(
      is_first_launch_after_upgrade);
  metrics_provider_->set_received_memory_warning_before_last_shutdown(
      received_memory_warning);
  metrics_provider_->set_has_crash_logs(has_crash_logs);
  metrics_provider_->set_was_last_shutdown_frozen(was_last_shutdown_frozen);

  // Create a histogram tester for verifying samples written to the shutdown
  // type histogram.
  base::HistogramTester histogram_tester;

  // Now call the method under test and verify exactly one sample is written to
  // the expected bucket.
  metrics_provider_->ProvidePreviousSessionData(nullptr);
  histogram_tester.ExpectUniqueSample("Stability.MobileSessionShutdownType",
                                      expected_buckets[GetParam()], 1);
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
  metrics_provider_.reset(new MobileSessionShutdownMetricsProviderForTesting(
      metrics_service_.get()));

  // Create a histogram tester for verifying samples written to the shutdown
  // type histogram.
  base::HistogramTester histogram_tester;

  // Now call the method under test and verify exactly one sample is written to
  // the expected bucket.
  metrics_provider_->ProvidePreviousSessionData(nullptr);
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
  metrics_provider_.reset(new MobileSessionShutdownMetricsProviderForTesting(
      metrics_service_.get()));

  // Create a histogram tester for verifying samples written to the shutdown
  // type histogram.
  base::HistogramTester histogram_tester;

  // Now call the method under test and verify exactly one sample is written to
  // the expected bucket.
  metrics_provider_->ProvidePreviousSessionData(nullptr);
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
  metrics_provider_.reset(new MobileSessionShutdownMetricsProviderForTesting(
      metrics_service_.get()));
  metrics_provider_->set_has_crash_logs(true);

  // Create a histogram tester for verifying samples written to the shutdown
  // type histogram.
  base::HistogramTester histogram_tester;

  // Now call the method under test and verify exactly one sample is written to
  // the expected bucket.
  metrics_provider_->ProvidePreviousSessionData(nullptr);
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
  metrics_provider_.reset(new MobileSessionShutdownMetricsProviderForTesting(
      metrics_service_.get()));
  metrics_provider_->set_was_last_shutdown_frozen(true);

  // Create a histogram tester for verifying samples written to the shutdown
  // type histogram.
  base::HistogramTester histogram_tester;

  // Now call the method under test and verify exactly one sample is written to
  // the expected bucket.
  metrics_provider_->ProvidePreviousSessionData(nullptr);
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
  metrics_provider_.reset(new MobileSessionShutdownMetricsProviderForTesting(
      metrics_service_.get()));

  // Test UTE with no possible explanation.
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  [PreviousSessionInfo resetSharedInstanceForTesting];
  [PreviousSessionInfo sharedInstance].OSRestartedAfterPreviousSession = NO;
  metrics_provider_->ProvidePreviousSessionData(nullptr);
  histogram_tester->ExpectUniqueSample(
      "Stability.iOS.UTE.OSRestartedAfterPreviousSession", false, 1);
  histogram_tester->ExpectUniqueSample(
      "Stability.iOS.UTE.HasPossibleExplanation", false, 1);

  // Test UTE with low battery when OS did not restart.
  [PreviousSessionInfo sharedInstance].deviceBatteryLevel =
      kCriticallyLowBatteryLevel;
  [PreviousSessionInfo sharedInstance].deviceBatteryState =
      previous_session_info_constants::DeviceBatteryState::kUnplugged;
  histogram_tester = std::make_unique<base::HistogramTester>();
  metrics_provider_->ProvidePreviousSessionData(nullptr);
  histogram_tester->ExpectUniqueSample(
      "Stability.iOS.UTE.OSRestartedAfterPreviousSession", false, 1);
  histogram_tester->ExpectUniqueSample(
      "Stability.iOS.UTE.HasPossibleExplanation", false, 1);

  // Test UTE when OS restarted after previous session.
  [PreviousSessionInfo sharedInstance].OSRestartedAfterPreviousSession = YES;
  histogram_tester = std::make_unique<base::HistogramTester>();
  metrics_provider_->ProvidePreviousSessionData(nullptr);
  histogram_tester->ExpectUniqueSample(
      "Stability.iOS.UTE.OSRestartedAfterPreviousSession", true, 1);
  histogram_tester->ExpectUniqueSample(
      "Stability.iOS.UTE.HasPossibleExplanation", true, 1);
}

INSTANTIATE_TEST_SUITE_P(/* No InstantiationName */,
                         MobileSessionShutdownMetricsProviderTest,
                         testing::Range(0, 32));
