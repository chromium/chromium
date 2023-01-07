// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/report_scheduler_timer.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

bool IsOffline() {
  return GetNetworkConnectionTracker()->IsOffline();
}

}  // namespace

ReportSchedulerTimer::ReportSchedulerTimer(std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)) {
  DCHECK(delegate_);
  GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);
  OnConnectionChanged(network::mojom::ConnectionType::CONNECTION_UNKNOWN);
}

ReportSchedulerTimer::~ReportSchedulerTimer() {
  GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);
}

void ReportSchedulerTimer::MaybeSet(absl::optional<base::Time> reporting_time) {
  if (!reporting_time.has_value() || IsOffline()) {
    return;
  }
  if (!reporting_time_reached_timer_.IsRunning() ||
      reporting_time_reached_timer_.desired_run_time() > reporting_time) {
    reporting_time_reached_timer_.Start(
        FROM_HERE, reporting_time.value(),
        base::BindOnce(&ReportSchedulerTimer::OnTimerFired,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void ReportSchedulerTimer::Refresh() {
  RefreshImpl(base::Time::Now());
}

void ReportSchedulerTimer::RefreshImpl(base::Time now) {
  if (IsOffline()) {
    return;
  }

  delegate_->GetNextReportTime(base::BindOnce(&ReportSchedulerTimer::MaybeSet,
                                              weak_ptr_factory_.GetWeakPtr()),
                               now);
}

void ReportSchedulerTimer::OnTimerFired() {
  base::Time now = base::Time::Now();
  delegate_->OnReportingTimeReached(now);
  RefreshImpl(now);
}

void ReportSchedulerTimer::OnConnectionChanged(
    network::mojom::ConnectionType connection_type) {
  if (IsOffline()) {
    reporting_time_reached_timer_.Stop();
  } else if (!reporting_time_reached_timer_.IsRunning()) {
    // Add delay to all reports that should have been sent while the browser was
    // offline so they are not temporally joinable. We only need to do this if
    // the connection changes from offline to online, not if an online
    // connection changes between, e.g., 3G and 4G. Rather than track the
    // previous connection state, we use the timer's running state: The timer is
    // running if and only if at least one report has been stored and the
    // browser is not offline. This results in an extra call to
    // `AdjustOfflineReportTimes()` when no reports have been stored and the
    // browser changes online connection types, but storage will have no reports
    // to adjust in that case, so we don't bother preventing it.
    delegate_->AdjustOfflineReportTimes(base::BindOnce(
        &ReportSchedulerTimer::MaybeSet, weak_ptr_factory_.GetWeakPtr()));
  }
}

}  // namespace content
