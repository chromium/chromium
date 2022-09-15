// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/background_sync_parameters.h"

#include "build/build_config.h"

namespace content {

namespace {
const int kMaxSyncAttempts = 3;
const int kRetryDelayFactor = 3;
constexpr base::TimeDelta kInitialRetryDelay = base::Minutes(5);
constexpr base::TimeDelta kMaxSyncEventDuration = base::Minutes(3);
constexpr base::TimeDelta kMinSyncRecoveryTime = base::Minutes(6);
constexpr base::TimeDelta kMinPeriodicSyncEventsInterval = base::Hours(12);
}

BackgroundSyncParameters::BackgroundSyncParameters()
    : disable(false),
#if BUILDFLAG(IS_ANDROID)
      rely_on_android_network_detection(false),
#endif
      keep_browser_awake_till_events_complete(false),
      skip_permissions_check_for_testing(false),
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

BackgroundSyncParameters& BackgroundSyncParameters::operator=(
    const BackgroundSyncParameters& other) = default;

bool BackgroundSyncParameters::operator==(
    const BackgroundSyncParameters& other) const {
  return disable == other.disable &&
#if BUILDFLAG(IS_ANDROID)
         rely_on_android_network_detection ==
             other.rely_on_android_network_detection &&
#endif
         keep_browser_awake_till_events_complete ==
             other.keep_browser_awake_till_events_complete &&
         skip_permissions_check_for_testing ==
             other.skip_permissions_check_for_testing &&
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
