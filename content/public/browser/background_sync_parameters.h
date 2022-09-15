// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BACKGROUND_SYNC_PARAMETERS_H_
#define CONTENT_PUBLIC_BROWSER_BACKGROUND_SYNC_PARAMETERS_H_

#include <stdint.h>

#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/content_export.h"

namespace content {

struct CONTENT_EXPORT BackgroundSyncParameters {
  BackgroundSyncParameters();
  BackgroundSyncParameters(const BackgroundSyncParameters& other);
  BackgroundSyncParameters& operator=(const BackgroundSyncParameters& other);
  bool operator==(const BackgroundSyncParameters& other) const;

  // True if the manager should be disabled and registration attempts should
  // fail.
  bool disable;

#if BUILDFLAG(IS_ANDROID)
  // True if we should rely on Android's network detection where possible.
  bool rely_on_android_network_detection;
#endif

  // If true, we keep the browser awake till all (periodic)sync events fired
  // have completed. If false, we only keep the browser awake till all ready
  // (periodic)sync events have been fired.
  bool keep_browser_awake_till_events_complete;

  // True if the manager should skip checking for permissions.
  bool skip_permissions_check_for_testing;

  // The number of attempts the BackgroundSyncManager will make to fire an
  // event before giving up.
  int max_sync_attempts;

  // The number of attempts the BackgroundSyncManager will make to fire an
  // event before giving up, assuming the origin has notification permission.
  // This value will override |max_sync_attempts| assuming the Sync
  // Registration's origin has notification permissions.
  int max_sync_attempts_with_notification_permission;

  // The first time that a registration retries, it will wait at least this much
  // time before doing so.
  base::TimeDelta initial_retry_delay;

  // The factor by which retry delay increases. The retry time is determined by:
  // initial_retry_delay * pow(retry_delay_factor, |attempts|-1).
  int retry_delay_factor;

  // The minimum time to wait before waking the browser in case the browser
  // closes mid-sync.
  base::TimeDelta min_sync_recovery_time;

  // The maximum amount of time that a sync event can run for.
  base::TimeDelta max_sync_event_duration;

  // The minimum time interval between first attempts of Periodic Background
  // Sync events for a given registration.
  base::TimeDelta min_periodic_sync_events_interval;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BACKGROUND_SYNC_PARAMETERS_H_
