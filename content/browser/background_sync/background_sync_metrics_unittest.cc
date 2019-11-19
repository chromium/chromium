// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "content/browser/background_sync/background_sync_metrics.h"
#include "third_party/blink/public/mojom/background_sync/background_sync.mojom.h"

namespace content {

using blink::mojom::BackgroundSyncType;

class BackgroundSyncMetricsTest : public ::testing::Test {
 public:
  BackgroundSyncMetricsTest() = default;
  ~BackgroundSyncMetricsTest() override = default;

 protected:
  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncMetricsTest);
};

TEST_F(BackgroundSyncMetricsTest, RecordEventStarted) {
  BackgroundSyncMetrics::RecordEventStarted(BackgroundSyncType::ONE_SHOT,
                                            /* started_in_foreground= */ false);
  histogram_tester_.ExpectBucketCount(
      "BackgroundSync.Event.OneShotStartedInForeground", false, 1);
  BackgroundSyncMetrics::RecordEventStarted(BackgroundSyncType::PERIODIC,
                                            /* started_in_foreground= */ true);
  histogram_tester_.ExpectBucketCount(
      "BackgroundSync.Event.PeriodicStartedInForeground", true, 1);
}

TEST_F(BackgroundSyncMetricsTest, RecordRegistrationComplete) {
  BackgroundSyncMetrics::RecordRegistrationComplete(
      /* event_succeeded= */ true, /* num_attempts_required= */ 3);
  histogram_tester_.ExpectBucketCount(
      "BackgroundSync.Registration.OneShot.EventSucceededAtCompletion", true,
      1);
  histogram_tester_.ExpectBucketCount(
      "BackgroundSync.Registration.OneShot.NumAttemptsForSuccessfulEvent", 3,
      1);
}

TEST_F(BackgroundSyncMetricsTest, RecordEventResult) {
  BackgroundSyncMetrics::RecordEventResult(BackgroundSyncType::ONE_SHOT,
                                           /* event_succeeded= */ true,
                                           /* finished_in_foreground= */ true);
  histogram_tester_.ExpectBucketCount(
      "BackgroundSync.Event.OneShotResultPattern",
      BackgroundSyncMetrics::ResultPattern::RESULT_PATTERN_SUCCESS_FOREGROUND,
      1);

  BackgroundSyncMetrics::RecordEventResult(BackgroundSyncType::PERIODIC,
                                           /* event_succeeded= */ false,
                                           /* finished_in_foreground= */ false);
  histogram_tester_.ExpectBucketCount(
      "BackgroundSync.Event.PeriodicResultPattern",
      BackgroundSyncMetrics::ResultPattern::RESULT_PATTERN_FAILED_BACKGROUND,
      1);
}

TEST_F(BackgroundSyncMetricsTest, RecordBatchSyncEventComplete) {
  BackgroundSyncMetrics::RecordBatchSyncEventComplete(
      BackgroundSyncType::ONE_SHOT, base::TimeDelta::FromSeconds(1),
      /* from_wakeup_task= */ false,
      /* number_of_batched_sync_events= */ 1);
  histogram_tester_.ExpectUniqueSample(
      "BackgroundSync.Event.Time",
      base::TimeDelta::FromSeconds(1).InMilliseconds(), 1);

  BackgroundSyncMetrics::RecordBatchSyncEventComplete(
      BackgroundSyncType::PERIODIC, base::TimeDelta::FromMinutes(1),
      /* from_wakeup_task= */ false,
      /* number_of_batched_sync_events= */ 10);
  histogram_tester_.ExpectUniqueSample(
      "PeriodicBackgroundSync.Event.Time",
      base::TimeDelta::FromMinutes(1).InMilliseconds(), 1);
}

TEST_F(BackgroundSyncMetricsTest, CountRegisterSuccess) {
  BackgroundSyncMetrics::CountRegisterSuccess(
      BackgroundSyncType::ONE_SHOT,
      /* min_interval_ms= */ -1, BackgroundSyncMetrics::REGISTRATION_COULD_FIRE,
      /* registration_is_duplicate= */
      BackgroundSyncMetrics::REGISTRATION_IS_NOT_DUPLICATE);
  histogram_tester_.ExpectUniqueSample(
      "BackgroundSync.Registration.OneShot.CouldFire", 1, 1);
  histogram_tester_.ExpectUniqueSample("BackgroundSync.Registration.OneShot",
                                       BACKGROUND_SYNC_STATUS_OK, 1);
  histogram_tester_.ExpectUniqueSample(
      "BackgroundSync.Registration.OneShot.IsDuplicate", 0, 1);

  BackgroundSyncMetrics::CountRegisterSuccess(
      BackgroundSyncType::PERIODIC,
      /* min_interval_ms= */ 1000,
      BackgroundSyncMetrics::REGISTRATION_COULD_FIRE,
      /* registration_is_duplicate= */
      BackgroundSyncMetrics::REGISTRATION_IS_DUPLICATE);
  histogram_tester_.ExpectUniqueSample(
      "BackgroundSync.Registration.Periodic.MinInterval", 1, 1);
  histogram_tester_.ExpectUniqueSample("BackgroundSync.Registration.Periodic",
                                       BACKGROUND_SYNC_STATUS_OK, 1);
  histogram_tester_.ExpectUniqueSample(
      "BackgroundSync.Registration.Periodic.IsDuplicate", 1, 1);
}

TEST_F(BackgroundSyncMetricsTest, CountUnregisterPeriodicSync) {
  BackgroundSyncMetrics::CountUnregisterPeriodicSync(BACKGROUND_SYNC_STATUS_OK);
  histogram_tester_.ExpectUniqueSample("BackgroundSync.Unregistration.Periodic",
                                       BACKGROUND_SYNC_STATUS_OK, 1);
}

TEST_F(BackgroundSyncMetricsTest, EventsFiredFromWakeupTask) {
  BackgroundSyncMetrics::RecordEventsFiredFromWakeupTask(
      BackgroundSyncType::ONE_SHOT,
      /* events_fired= */ false);
  histogram_tester_.ExpectBucketCount(
      "BackgroundSync.WakeupTaskFiredEvents.OneShot", false, 1);
  BackgroundSyncMetrics::RecordEventsFiredFromWakeupTask(
      BackgroundSyncType::PERIODIC,
      /* events_fired= */ true);
  histogram_tester_.ExpectBucketCount(
      "BackgroundSync.WakeupTaskFiredEvents.Periodic", true, 1);
}

}  // namespace content
