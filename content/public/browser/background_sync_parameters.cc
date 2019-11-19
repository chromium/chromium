// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/background_sync_parameters.h"

namespace content {

namespace {
const int kMaxSyncAttempts = 3;
const int kRetryDelayFactor = 3;
constexpr base::TimeDelta kInitialRetryDelay = base::TimeDelta::FromMinutes(5);
constexpr base::TimeDelta kMaxSyncEventDuration =
    base::TimeDelta::FromMinutes(3);
constexpr base::TimeDelta kMinSyncRecoveryTime =
    base::TimeDelta::FromMinutes(6);
constexpr base::TimeDelta kMinPeriodicSyncEventsInterval =
    base::TimeDelta::FromHours(12);
}

BackgroundSyncParameters::BackgroundSyncParameters()
    : disable(false),
#if defined(OS_ANDROID)
      rely_on_android_network_detection(false),
#endif
      keep_browser_awake_till_events_complete(false),
      max_sync_attempts(kMaxSyncAttempts),
      max_sync_attempts_with_notification_permission(kMaxSyncAttempts),
      initial_retry_delay(kInitialRetryDelay),
      retry_delay_factor(kRetryDelayFactor),
      min_sync_recovery_time(kMinSyncRecoveryTime),
      max_sync_event_duration(kMaxSyncEventDuration),
      min_periodic_sync_events_interval(kMinPeriodicSyncEventsInterval) {
}

BackgroundSyncParameters::BackgroundSyncParameters(
    const BackgroundSyncParameters& other) = default;

bool BackgroundSyncParameters::operator==(
    const BackgroundSyncParameters& other) const {
  return disable == other.disable &&
#if defined(OS_ANDROID)
         rely_on_android_network_detection ==
             other.rely_on_android_network_detection &&
#endif
         keep_browser_awake_till_events_complete ==
             other.keep_browser_awake_till_events_complete &&
         max_sync_attempts == other.max_sync_attempts &&
         max_sync_attempts_with_notification_permission ==
             other.max_sync_attempts_with_notification_permission &&
         initial_retry_delay == other.initial_retry_delay &&
         retry_delay_factor == other.retry_delay_factor &&
         min_sync_recovery_time == other.min_sync_recovery_time &&
         max_sync_event_duration == other.max_sync_event_duration &&
         min_periodic_sync_events_interval ==
             other.min_periodic_sync_events_interval;
}

}  // namespace content
