// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_METRICS_H_
#define CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_METRICS_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "content/browser/background_sync/background_sync.pb.h"
#include "content/browser/background_sync/background_sync_status.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/background_sync/background_sync.mojom.h"

namespace content {

// This class contains the logic for recording usage metrics for the Background
// Sync API. It is stateless, containing only static methods, so it can be used
// by any of the Background Sync code, without needing to be instantiated
// explicitly.
class CONTENT_EXPORT BackgroundSyncMetrics {
 public:
  // ResultPattern is used by Histograms, append new entries at the end.
  enum ResultPattern {
    RESULT_PATTERN_SUCCESS_FOREGROUND = 0,
    RESULT_PATTERN_SUCCESS_BACKGROUND = 1,
    RESULT_PATTERN_FAILED_FOREGROUND = 2,
    RESULT_PATTERN_FAILED_BACKGROUND = 3,
    RESULT_PATTERN_MAX = RESULT_PATTERN_FAILED_BACKGROUND,
  };

  enum RegistrationCouldFire {
    REGISTRATION_COULD_NOT_FIRE,
    REGISTRATION_COULD_FIRE
  };

  enum RegistrationIsDuplicate {
    REGISTRATION_IS_NOT_DUPLICATE,
    REGISTRATION_IS_DUPLICATE
  };

  // Records the start of a sync event.
  static void RecordEventStarted(blink::mojom::BackgroundSyncType sync_type,
                                 bool startedin_foreground);

  // Records the result of a single sync event firing.
  static void RecordEventResult(blink::mojom::BackgroundSyncType sync_type,
                                bool result,
                                bool finished_in_foreground);

  // Records, at the completion of a one-shot sync registration, whether the
  // sync event was successful, and how many attempts it took to get there.
  static void RecordRegistrationComplete(bool event_succeeded,
                                         int num_attempts_required);

  // Records the result of running a batch of sync events, including the total
  // time spent, the batch size, and whether the operation originated from a
  // wakeup task.
  static void RecordBatchSyncEventComplete(
      blink::mojom::BackgroundSyncType sync_type,
      const base::TimeDelta& time,
      bool from_wakeup_task,
      int number_of_batched_sync_events);

  // Records the result of successfully registering a sync. |could_fire|
  // indicates whether the conditions were sufficient for the sync to fire
  // immediately at the time it was registered. |could_fire| is only relevant to
  // and recorded for one-shot Background Sync registrations.
  // |min_interval_ms| is only recorded for Periodic Background Sync
  // registrations, and records the min_interval in ms requested for this
  // registration.
  static void CountRegisterSuccess(
      blink::mojom::BackgroundSyncType sync_type,
      int64_t min_interval_ms,
      RegistrationCouldFire could_fire,
      RegistrationIsDuplicate registration_is_duplicate);

  // Records the status of a failed sync registration.
  static void CountRegisterFailure(blink::mojom::BackgroundSyncType sync_type,
                                   BackgroundSyncStatus status);

  // Records the status of an attempt to remove a Periodic Background
  // Sync registration.
  static void CountUnregisterPeriodicSync(BackgroundSyncStatus status);

  // Records whether the Chrome wakeup task actually resulted in us firing
  // any sync events corresponding to |sync_type|.
  static void RecordEventsFiredFromWakeupTask(
      blink::mojom::BackgroundSyncType sync_type,
      bool fired_events);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(BackgroundSyncMetrics);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_METRICS_H_
