// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace {

content::BackgroundSyncMetrics::ResultPattern EventResultToResultPattern(
    bool success,
    bool finished_in_foreground) {
  if (success) {
    return finished_in_foreground ? content::BackgroundSyncMetrics::
                                        RESULT_PATTERN_SUCCESS_FOREGROUND
                                  : content::BackgroundSyncMetrics::
                                        RESULT_PATTERN_SUCCESS_BACKGROUND;
  }
  return finished_in_foreground
             ? content::BackgroundSyncMetrics::RESULT_PATTERN_FAILED_FOREGROUND
             : content::BackgroundSyncMetrics::RESULT_PATTERN_FAILED_BACKGROUND;
}

const std::string GetBackgroundSyncSuffix(
    blink::mojom::BackgroundSyncType sync_type) {
  if (sync_type == blink::mojom::BackgroundSyncType::ONE_SHOT)
    return "OneShot";
  else
    return "Periodic";
}

const std::string GetBackgroundSyncPrefix(
    blink::mojom::BackgroundSyncType sync_type) {
  if (sync_type == blink::mojom::BackgroundSyncType::ONE_SHOT)
    return "";
  else
    return "Periodic";
}

}  // namespace

namespace content {

// static
void BackgroundSyncMetrics::RecordEventStarted(
    blink::mojom::BackgroundSyncType sync_type,
    bool started_in_foreground) {
  base::UmaHistogramBoolean("BackgroundSync.Event." +
                                GetBackgroundSyncSuffix(sync_type) +
                                "StartedInForeground",
                            started_in_foreground);
}

// static
void BackgroundSyncMetrics::RecordRegistrationComplete(
    bool event_succeeded,
    int num_attempts_required) {
  base::UmaHistogramBoolean(
      "BackgroundSync.Registration.OneShot.EventSucceededAtCompletion",
      event_succeeded);

  if (!event_succeeded)
    return;

  base::UmaHistogramExactLinear(
      "BackgroundSync.Registration.OneShot.NumAttemptsForSuccessfulEvent",
      num_attempts_required, 50);
}

// static
void BackgroundSyncMetrics::RecordEventResult(
    blink::mojom::BackgroundSyncType sync_type,
    bool success,
    bool finished_in_foreground) {
  base::UmaHistogramEnumeration(
      "BackgroundSync.Event." + GetBackgroundSyncSuffix(sync_type) +
          "ResultPattern",
      EventResultToResultPattern(success, finished_in_foreground),
      static_cast<ResultPattern>(RESULT_PATTERN_MAX + 1));
}

// static
void BackgroundSyncMetrics::RecordBatchSyncEventComplete(
    blink::mojom::BackgroundSyncType sync_type,
    const base::TimeDelta& time,
    bool from_wakeup_task,
    int number_of_batched_sync_events) {
  // The total batch handling time should be under 5 minutes; we'll record up to
  // 6 minutes, to be safe.
  base::UmaHistogramCustomTimes(
      GetBackgroundSyncPrefix(sync_type) + "BackgroundSync.Event.Time", time,
      /* min= */ base::Milliseconds(10),
      /* max= */ base::Minutes(6),
      /* buckets= */ 50);
  base::UmaHistogramCounts100(
      GetBackgroundSyncPrefix(sync_type) + "BackgroundSync.Event.BatchSize",
      number_of_batched_sync_events);

  base::UmaHistogramBoolean(GetBackgroundSyncPrefix(sync_type) +
                                "BackgroundSync.Event.FromWakeupTask",
                            from_wakeup_task);
}

// static
void BackgroundSyncMetrics::CountRegisterSuccess(
    blink::mojom::BackgroundSyncType sync_type,
    int64_t min_interval_ms,
    RegistrationCouldFire registration_could_fire,
    RegistrationIsDuplicate registration_is_duplicate) {
  base::UmaHistogramEnumeration(
      "BackgroundSync.Registration." + GetBackgroundSyncSuffix(sync_type),
      BACKGROUND_SYNC_STATUS_OK,
      static_cast<BackgroundSyncStatus>(BACKGROUND_SYNC_STATUS_MAX + 1));

  if (sync_type == blink::mojom::BackgroundSyncType::ONE_SHOT) {
    base::UmaHistogramBoolean(
        "BackgroundSync.Registration.OneShot.CouldFire",
        registration_could_fire == REGISTRATION_COULD_FIRE);
  } else {
    DCHECK_GE(min_interval_ms, 0);
    base::UmaHistogramCounts10M(
        "BackgroundSync.Registration.Periodic.MinInterval",
        min_interval_ms / 1000);
  }

  base::UmaHistogramBoolean(
      "BackgroundSync.Registration." + GetBackgroundSyncSuffix(sync_type) +
          ".IsDuplicate",
      registration_is_duplicate == REGISTRATION_IS_DUPLICATE);
}

// static
void BackgroundSyncMetrics::CountRegisterFailure(
    blink::mojom::BackgroundSyncType sync_type,
    BackgroundSyncStatus result) {
  base::UmaHistogramEnumeration(
      std::string("BackgroundSync.Registration.") +
          GetBackgroundSyncSuffix(sync_type),
      result,
      static_cast<BackgroundSyncStatus>(BACKGROUND_SYNC_STATUS_MAX + 1));
}

// static
void BackgroundSyncMetrics::CountUnregisterPeriodicSync(
    BackgroundSyncStatus status) {
  base::UmaHistogramEnumeration(
      "BackgroundSync.Unregistration.Periodic", status,
      static_cast<BackgroundSyncStatus>(BACKGROUND_SYNC_STATUS_MAX + 1));
}

// static
void BackgroundSyncMetrics::RecordEventsFiredFromWakeupTask(
    blink::mojom::BackgroundSyncType sync_type,
    bool fired_events) {
  base::UmaHistogramBoolean("BackgroundSync.WakeupTaskFiredEvents." +
                                GetBackgroundSyncSuffix(sync_type),
                            fired_events);
}

}  // namespace content
