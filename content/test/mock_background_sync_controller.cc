// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_background_sync_controller.h"
#include "base/containers/contains.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"

namespace {

const char kFieldTrialName[] = "BackgroundSync";
const char kMaxAttemptsParameterName[] = "max_sync_attempts";
const char kMinPeriodicSyncEventsInterval[] =
    "min_periodic_sync_events_interval_sec";

}  // namespace

namespace content {

MockBackgroundSyncController::MockBackgroundSyncController() = default;

MockBackgroundSyncController::~MockBackgroundSyncController() = default;

void MockBackgroundSyncController::NotifyOneShotBackgroundSyncRegistered(
    const url::Origin& origin,
    bool can_fire,
    bool is_reregistered) {
  registration_count_ += 1;
  registration_origin_ = origin;
}

void MockBackgroundSyncController::ScheduleBrowserWakeUpWithDelay(
    blink::mojom::BackgroundSyncType sync_type,
    base::TimeDelta delay) {
  if (sync_type == blink::mojom::BackgroundSyncType::PERIODIC) {
    run_in_background_for_periodic_sync_count_ += 1;
    periodic_sync_browser_wakeup_delay_ = delay;
    return;
  }
  run_in_background_for_one_shot_sync_count_ += 1;
  one_shot_sync_browser_wakeup_delay_ = delay;
}

void MockBackgroundSyncController::CancelBrowserWakeup(
    blink::mojom::BackgroundSyncType sync_type) {
  if (sync_type == blink::mojom::BackgroundSyncType::PERIODIC) {
    periodic_sync_browser_wakeup_delay_ = base::TimeDelta::Max();
  } else {
    one_shot_sync_browser_wakeup_delay_ = base::TimeDelta::Max();
  }
}

void MockBackgroundSyncController::ApplyFieldTrialParamsOverrides() {
  std::map<std::string, std::string> field_params;
  if (!base::GetFieldTrialParams(kFieldTrialName, &field_params))
    return;

  if (base::Contains(field_params, kMaxAttemptsParameterName)) {
    int max_attempts;
    if (base::StringToInt(field_params[kMaxAttemptsParameterName],
                          &max_attempts)) {
      background_sync_parameters_.max_sync_attempts = max_attempts;
    }
  }

  if (base::Contains(field_params, kMinPeriodicSyncEventsInterval)) {
    int min_periodic_sync_events_interval_sec;
    if (base::StringToInt(field_params[kMinPeriodicSyncEventsInterval],
                          &min_periodic_sync_events_interval_sec)) {
      background_sync_parameters_.min_periodic_sync_events_interval =
          base::Seconds(min_periodic_sync_events_interval_sec);
    }
  }
}

void MockBackgroundSyncController::GetParameterOverrides(
    BackgroundSyncParameters* parameters) {
  ApplyFieldTrialParamsOverrides();
  *parameters = background_sync_parameters_;
}

base::TimeDelta MockBackgroundSyncController::GetNextEventDelay(
    const BackgroundSyncRegistration& registration,
    BackgroundSyncParameters* parameters,
    base::TimeDelta time_till_soonest_scheduled_event_for_origin) {
  DCHECK(parameters);

  if (suspended_periodic_sync_origins_.count(registration.origin()))
    return base::TimeDelta::Max();

  int num_attempts = registration.num_attempts();

  if (!num_attempts) {
    // First attempt.
    switch (registration.sync_type()) {
      case blink::mojom::BackgroundSyncType::ONE_SHOT:
        return base::TimeDelta();
      case blink::mojom::BackgroundSyncType::PERIODIC:
        int64_t effective_gap_ms =
            parameters->min_periodic_sync_events_interval.InMilliseconds();
        return base::Milliseconds(
            std::max(registration.options()->min_interval, effective_gap_ms));
    }
  }

  // After a sync event has been fired.
  DCHECK_LT(num_attempts, parameters->max_sync_attempts);
  return parameters->initial_retry_delay *
         pow(parameters->retry_delay_factor, num_attempts - 1);
}

std::unique_ptr<BackgroundSyncController::BackgroundSyncEventKeepAlive>
MockBackgroundSyncController::CreateBackgroundSyncEventKeepAlive() {
  return nullptr;
}

void MockBackgroundSyncController::NoteSuspendedPeriodicSyncOrigins(
    std::set<url::Origin> suspended_origins) {
  for (auto& origin : suspended_origins)
    suspended_periodic_sync_origins_.insert(std::move(origin));
}

void MockBackgroundSyncController::NoteRegisteredPeriodicSyncOrigins(
    std::set<url::Origin> registered_origins) {
  for (auto& origin : registered_origins)
    suspended_periodic_sync_origins_.insert(std::move(origin));
}

void MockBackgroundSyncController::AddToTrackedOrigins(
    const url::Origin& origin) {
  periodic_sync_origins_.insert(origin);
}

void MockBackgroundSyncController::RemoveFromTrackedOrigins(
    const url::Origin& origin) {
  periodic_sync_origins_.erase(origin);
}

}  // namespace content
