// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_report_scheduler.h"

#include <utility>

#include "base/bind.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_storage.h"
#include "content/public/browser/network_service_instance.h"

namespace content {

namespace {

bool IsOffline() {
  return GetNetworkConnectionTracker()->IsOffline();
}

}  // namespace

AttributionReportScheduler::AttributionReportScheduler(
    base::RepeatingClosure send_reports,
    base::SequenceBound<AttributionStorage>& attribution_storage)
    : send_reports_(std::move(send_reports)),
      attribution_storage_(attribution_storage) {
  content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);
  OnConnectionChanged(network::mojom::ConnectionType::CONNECTION_UNKNOWN);
}

AttributionReportScheduler::~AttributionReportScheduler() {
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);
}

void AttributionReportScheduler::Refresh() {
  if (IsOffline())
    return;

  attribution_storage_.AsyncCall(&AttributionStorage::GetNextReportTime)
      .WithArgs(base::Time::Now())
      .Then(base::BindOnce(&AttributionReportScheduler::ScheduleSend,
                           weak_factory_.GetWeakPtr()));
}

void AttributionReportScheduler::ScheduleSend(absl::optional<base::Time> time) {
  if (!time.has_value() || IsOffline())
    return;

  if (!get_reports_to_send_timer_.IsRunning() ||
      *time < get_reports_to_send_timer_.desired_run_time()) {
    get_reports_to_send_timer_.Start(
        FROM_HERE, *time, this, &AttributionReportScheduler::InvokeCallback);
  }
}

void AttributionReportScheduler::InvokeCallback() {
  send_reports_.Run();
}

void AttributionReportScheduler::OnConnectionChanged(
    network::mojom::ConnectionType connection_type) {
  if (IsOffline()) {
    get_reports_to_send_timer_.Stop();
  } else if (!get_reports_to_send_timer_.IsRunning()) {
    // Add delay to all reports that should have been sent while the browser was
    // offline so they are not temporally joinable. We do this in storage to
    // avoid pulling an unbounded number of reports into memory, only to
    // immediately issue async storage calls to modify their report times.
    //
    // We only need to do this if the connection changes from offline to online,
    // not if an online connection changes between, e.g., 3G and 4G. Rather than
    // track the previous connection state, we use the timer's running state:
    // The timer is running if and only if at least one report has been stored
    // and the browser is not offline. This results in an extra call to
    // `AttributionStorage::AdjustOfflineReportTimes()` when no reports have
    // been stored and the browser changes online connection types, but storage
    // will have no reports to adjust in that case, so we don't bother
    // preventing it.
    attribution_storage_
        .AsyncCall(&AttributionStorage::AdjustOfflineReportTimes)
        .Then(base::BindOnce(&AttributionReportScheduler::ScheduleSend,
                             weak_factory_.GetWeakPtr()));
  }
}

}  // namespace content
