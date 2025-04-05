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
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"

namespace content {

namespace {

bool InStandby(base::TimeDelta difference, base::TimeDelta cutoff) {
  return difference > cutoff;
}

}  // namespace

ReportSchedulerTimer::ReportSchedulerTimer(std::unique_ptr<Delegate> delegate,
                                           base::TimeDelta navigation_window)
    : delegate_(std::move(delegate)), navigation_window_(navigation_window) {
  CHECK(delegate_);
}

ReportSchedulerTimer::ReportSchedulerTimer(std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)) {
  CHECK(delegate_);

  network::NetworkConnectionTracker* tracker = GetNetworkConnectionTracker();
  obs_.Observe(tracker);

  network::mojom::ConnectionType connection_type;
  bool synchronous_return = tracker->GetConnectionType(
      &connection_type,
      base::BindOnce(&ReportSchedulerTimer::OnConnectionChanged,
                     weak_ptr_factory_.GetWeakPtr()));
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

  if (!reporting_time.has_value() ||
      (!IsNavigationFeatureEnabled() && IsOffline()) || standby_mode_) {
    return;
  }
  if (!reporting_time_reached_timer_.IsRunning() ||
      reporting_time_reached_timer_.desired_run_time() > reporting_time) {
    reporting_time_reached_timer_.Start(FROM_HERE, reporting_time.value(), this,
                                        &ReportSchedulerTimer::OnTimerFired);
  }
}

void ReportSchedulerTimer::Refresh(base::Time now) {
  CHECK(!standby_mode_);
  if (!IsNavigationFeatureEnabled() && IsOffline()) {
    return;
  }

  delegate_->GetNextReportTime(base::BindOnce(&ReportSchedulerTimer::MaybeSet,
                                              weak_ptr_factory_.GetWeakPtr()),
                               now);
}

void ReportSchedulerTimer::OnTimerFired() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::Time now = base::Time::Now();
  if (IsNavigationFeatureEnabled()) {
    standby_mode_ =
        !last_navigation_time_.has_value() ||
        InStandby(now - *last_navigation_time_, *navigation_window_);
    if (standby_mode_) {
      // Nothing needs to be queued until a new navigation is received.
      return;
    }
  }
  delegate_->OnReportingTimeReached(
      now, reporting_time_reached_timer_.desired_run_time());
  Refresh(now);
}

void ReportSchedulerTimer::OnConnectionChanged(
    network::mojom::ConnectionType connection_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool was_offline = IsOffline();
  connection_type_ = connection_type;

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

void ReportSchedulerTimer::OnNewNavigation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsNavigationFeatureEnabled()) {
    return;
  }
  last_navigation_time_ = base::Time::Now();

  // Standby mode only starts when the timer fires while past the standby
  // cutoff, i.e. when a report is scheduled to be sent but is prevented due to
  // not having a recent enough navigation. Therefore if we're in standby mode
  // there must be a report ready to be sent.
  if (standby_mode_) {
    standby_mode_ = false;

    // Add delay to all reports that should have been sent while the report
    // timer was in standby so they are not temporally joinable.
    delegate_->AdjustOfflineReportTimes(base::BindOnce(
        &ReportSchedulerTimer::MaybeSet, weak_ptr_factory_.GetWeakPtr()));
  }
}

bool ReportSchedulerTimer::IsOffline() const {
  return connection_type_ == network::mojom::ConnectionType::CONNECTION_NONE;
}

}  // namespace content
