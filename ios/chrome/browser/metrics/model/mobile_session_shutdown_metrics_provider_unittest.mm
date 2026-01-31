// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/mobile_session_shutdown_metrics_provider.h"

#import <Foundation/Foundation.h>

#import <memory>

#import "base/test/metrics/histogram_tester.h"
#import "components/metrics/metrics_pref_names.h"
#import "components/metrics/metrics_service.h"
#import "components/metrics/metrics_state_manager.h"
#import "components/metrics/test/test_enabled_state_provider.h"
#import "components/metrics/test/test_metrics_service_client.h"
#import "components/prefs/testing_pref_service.h"
#import "components/previous_session_info/previous_session_info.h"
#import "components/previous_session_info/previous_session_info_private.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// A MobileSessionShutdownMetricsProvider that returns fake values for the last
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

 protected:
  // MobileSessionShutdownMetricsProvider:
  bool IsFirstLaunchAfterUpgrade() override {
    return is_first_launch_after_upgrade_;
  }

 private:
  bool is_first_launch_after_upgrade_ = false;
};

class MobileSessionShutdownMetricsProviderTest : public PlatformTest {
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

}  // namespace

// Tests Stability.iOS.TabCountBeforeCleanShutdown metrics recording after clean
// shutdown.
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

  [PreviousSessionInfo resetSharedInstanceForTesting];
}

// Tests Stability.iOS.TabCountBeforeCrash metrics recording after
// unclean shutdown.
TEST_F(MobileSessionShutdownMetricsProviderTest,
       TabCountMetricUncleanShutdown) {
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

  [PreviousSessionInfo resetSharedInstanceForTesting];
}

// Tests no metrics are recorded if it is first launch after upgrade.
TEST_F(MobileSessionShutdownMetricsProviderTest, FirstLaunchAfterUpgrade) {
  [PreviousSessionInfo sharedInstance].tabCount = 2;
  [PreviousSessionInfo sharedInstance].OTRTabCount = 3;
  metrics::CleanExitBeacon::SetStabilityExitedCleanlyForTesting(&local_state_,
                                                                true);

  // Set up the MetricsService and metrics provider.
  InitializeMetrics();
  MobileSessionShutdownMetricsProviderForTesting metrics_provider(
      metrics_service_.get());
  metrics_provider.set_is_first_launch_after_upgrade(true);

  // Create a histogram tester for verifying samples written to the shutdown
  // type histogram.
  base::HistogramTester histogram_tester;

  // Now call the method under test and verify exactly one sample is written to
  // the expected bucket.
  metrics_provider.ProvidePreviousSessionData(nullptr);
  histogram_tester.ExpectTotalCount("Stability.iOS.TabCountBeforeCleanShutdown",
                                    0);
  histogram_tester.ExpectTotalCount("Stability.iOS.TabCountBeforeCrash", 0);

  [PreviousSessionInfo resetSharedInstanceForTesting];
}
