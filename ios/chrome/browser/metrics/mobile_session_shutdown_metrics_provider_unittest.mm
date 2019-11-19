// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/metrics/mobile_session_shutdown_metrics_provider.h"

#include <memory>

#import <Foundation/Foundation.h>

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_simple_task_runner.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test_enabled_state_provider.h"
#include "components/metrics/test_metrics_service_client.h"
#include "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/metrics/previous_session_info.h"
#import "ios/chrome/browser/metrics/previous_session_info_private.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// An MobileSessionShutdownMetricsProvider that returns fake values for the last
// session environment query methods.
class MobileSessionShutdownMetricsProviderForTesting
    : public MobileSessionShutdownMetricsProvider {
 public:
  explicit MobileSessionShutdownMetricsProviderForTesting(
      metrics::MetricsService* metrics_service)
      : MobileSessionShutdownMetricsProvider(metrics_service) {}

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

  DISALLOW_COPY_AND_ASSIGN(MobileSessionShutdownMetricsProviderForTesting);
};

class MobileSessionShutdownMetricsProviderTest
    : public PlatformTest,
      public testing::WithParamInterface<int> {
 public:
  MobileSessionShutdownMetricsProviderTest() {
    metrics::MetricsService::RegisterPrefs(local_state_.registry());
  }

 protected:
  TestingPrefServiceSimple local_state_;
  metrics::TestMetricsServiceClient metrics_client_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_;
  std::unique_ptr<metrics::MetricsService> metrics_service_;
  std::unique_ptr<MobileSessionShutdownMetricsProviderForTesting>
      metrics_provider_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MobileSessionShutdownMetricsProviderTest);
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
      SHUTDOWN_IN_FOREGROUND_WITH_MAIN_THREAD_FROZEN,
      SHUTDOWN_IN_FOREGROUND_WITH_MAIN_THREAD_FROZEN,
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

  // Setup the MetricsService.
  local_state_.SetBoolean(metrics::prefs::kStabilityExitedCleanly,
                          was_last_shutdown_clean);
  metrics_state_ = metrics::MetricsStateManager::Create(
      &local_state_, new metrics::TestEnabledStateProvider(false, false),
      base::string16(), metrics::MetricsStateManager::StoreClientInfoCallback(),
      metrics::MetricsStateManager::LoadClientInfoCallback());
  metrics_service_.reset(new metrics::MetricsService(
      metrics_state_.get(), &metrics_client_, &local_state_));

  // Create the metrics provider to test.
  metrics_provider_.reset(new MobileSessionShutdownMetricsProviderForTesting(
      metrics_service_.get()));

  // Setup the metrics provider for the current test.
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

// Tests logging the following metrics:
//   - Stability.iOS.UTE.HasPossibleExplanation
//   - Stability.iOS.UTE.OSRestartedAfterPreviousSession
TEST_F(MobileSessionShutdownMetricsProviderTest,
       ProvideHasPossibleExplanationMetric) {
  // Setup the MetricsService and HistogramTester.
  local_state_.SetBoolean(metrics::prefs::kStabilityExitedCleanly, false);
  metrics_state_ = metrics::MetricsStateManager::Create(
      &local_state_, new metrics::TestEnabledStateProvider(false, false),
      base::string16(), metrics::MetricsStateManager::StoreClientInfoCallback(),
      metrics::MetricsStateManager::LoadClientInfoCallback());
  metrics_service_.reset(new metrics::MetricsService(
      metrics_state_.get(), &metrics_client_, &local_state_));
  metrics_provider_.reset(new MobileSessionShutdownMetricsProviderForTesting(
      metrics_service_.get()));

  // Test UTE with no possible explanation.
  auto histogram_tester = std::make_unique<base::HistogramTester>();
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

  // Test UTE with low battery when OS restarted after previous session.
  [PreviousSessionInfo sharedInstance].OSRestartedAfterPreviousSession = YES;
  histogram_tester = std::make_unique<base::HistogramTester>();
  metrics_provider_->ProvidePreviousSessionData(nullptr);
  histogram_tester->ExpectUniqueSample(
      "Stability.iOS.UTE.OSRestartedAfterPreviousSession", true, 1);
  histogram_tester->ExpectUniqueSample(
      "Stability.iOS.UTE.HasPossibleExplanation", true, 1);

  // Test UTE with normal battery when OS restarted after previous session.
  [PreviousSessionInfo sharedInstance].deviceBatteryLevel = 50;
  histogram_tester = std::make_unique<base::HistogramTester>();
  metrics_provider_->ProvidePreviousSessionData(nullptr);
  histogram_tester->ExpectUniqueSample(
      "Stability.iOS.UTE.OSRestartedAfterPreviousSession", true, 1);
  histogram_tester->ExpectUniqueSample(
      "Stability.iOS.UTE.HasPossibleExplanation", false, 1);
}

INSTANTIATE_TEST_SUITE_P(/* No InstantiationName */,
                         MobileSessionShutdownMetricsProviderTest,
                         testing::Range(0, 32));
