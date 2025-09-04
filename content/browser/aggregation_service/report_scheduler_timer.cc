// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/report_scheduler_timer.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/application_status_listener.h"
#endif

namespace content {

ReportSchedulerTimer::ReportSchedulerTimer(std::unique_ptr<Delegate> delegate
#if BUILDFLAG(IS_ANDROID)
                                           ,
                                           bool observe_app_state
#endif
                                           )
    : delegate_(std::move(delegate)) {
  CHECK(delegate_);

  network::NetworkConnectionTracker* tracker = GetNetworkConnectionTracker();
  obs_.Observe(tracker);

  network::mojom::ConnectionType connection_type;
  bool synchronous_return = tracker->GetConnectionType(
      &connection_type,
      base::BindOnce(&ReportSchedulerTimer::OnConnectionChanged,
                     weak_ptr_factory_.GetWeakPtr()));

#if BUILDFLAG(IS_ANDROID)
  if (observe_app_state) {
    application_status_listener_ =
        base::android::ApplicationStatusListener::New(base::BindRepeating(
            &ReportSchedulerTimer::OnApplicationStateChanged,
            weak_ptr_factory_.GetWeakPtr()));

    app_state_ = base::android::ApplicationStatusListener::GetState();
  }
#endif

  if (synchronous_return) {
    OnConnectionChanged(connection_type);
  }
}

ReportSchedulerTimer::~ReportSchedulerTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

network::mojom::ConnectionType ReportSchedulerTimer::connection_type() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return connection_type_;
}

void ReportSchedulerTimer::MaybeSet(std::optional<base::Time> reporting_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!reporting_time.has_value() || IsOffline()) {
    return;
  }
  if (!reporting_time_reached_timer_.IsRunning() ||
      reporting_time_reached_timer_.desired_run_time() > reporting_time) {
    reporting_time_reached_timer_.Start(FROM_HERE, reporting_time.value(), this,
                                        &ReportSchedulerTimer::OnTimerFired);
  }
}

void ReportSchedulerTimer::Refresh(base::Time now) {
  if (IsOffline()) {
    return;
  }

  delegate_->GetNextReportTime(base::BindOnce(&ReportSchedulerTimer::MaybeSet,
                                              weak_ptr_factory_.GetWeakPtr()),
                               now);
}

void ReportSchedulerTimer::OnTimerFired() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::Time now = base::Time::Now();
  delegate_->OnReportingTimeReached(
      now, reporting_time_reached_timer_.desired_run_time());
  Refresh(now);
}

void ReportSchedulerTimer::OnConnectionChanged(
    network::mojom::ConnectionType connection_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if BUILDFLAG(IS_ANDROID)
  UpdateState(connection_type, app_state_);
#else
  UpdateState(connection_type);
#endif
}

void ReportSchedulerTimer::UpdateState(
    network::mojom::ConnectionType connection_type
#if BUILDFLAG(IS_ANDROID)
    ,
    base::android::ApplicationState app_state
#endif
) {
  bool was_offline = IsOffline();
  connection_type_ = connection_type;
#if BUILDFLAG(IS_ANDROID)
  app_state_ = app_state;
#endif

  if (IsOffline()) {
    reporting_time_reached_timer_.Stop();
  } else if (was_offline) {
    // Add delay to all reports that should have been sent while the browser was
    // offline so they are not temporally joinable. We only need to do this if
    // the connection changes from offline to online, not if an online
    // connection changes between, e.g., 3G and 4G.
    delegate_->AdjustOfflineReportTimes(base::BindOnce(
        &ReportSchedulerTimer::MaybeSet, weak_ptr_factory_.GetWeakPtr()));
  }
}

bool ReportSchedulerTimer::IsOffline() const {
#if BUILDFLAG(IS_ANDROID)
  switch (app_state_) {
    case base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES:
    case base::android::APPLICATION_STATE_HAS_DESTROYED_ACTIVITIES:
      return true;
    case base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES:
    case base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES:
    case base::android::APPLICATION_STATE_UNKNOWN:
      break;
  }
#endif

  return connection_type_ == network::mojom::ConnectionType::CONNECTION_NONE;
}

#if BUILDFLAG(IS_ANDROID)
void ReportSchedulerTimer::OnApplicationStateChanged(
    base::android::ApplicationState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateState(connection_type_, state);
}
#endif

}  // namespace content
