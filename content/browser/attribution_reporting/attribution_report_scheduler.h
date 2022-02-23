// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORT_SCHEDULER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORT_SCHEDULER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "base/timer/wall_clock_timer.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class Time;
}  // namespace base

namespace content {

class AttributionStorage;

// This class consolidates logic regarding when to schedule the browser to send
// attribution reports. It talks directly to the `AttributionStorage` to help
// make these decisions.
//
// While the class does not make large changes to the underlying database, it
// is responsible for notifying the `AttributionStorage` when the browser comes
// back online, which mutates report times for some scheduled reports.
//
// TODO(apaseltiner): Consider making this class an observer to allow it to
// manage when to schedule things.
class AttributionReportScheduler
    : public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  AttributionReportScheduler(
      base::RepeatingClosure send_reports_,
      base::SequenceBound<AttributionStorage>& attribution_storage);
  ~AttributionReportScheduler() override;

  AttributionReportScheduler(const AttributionReportScheduler& other) = delete;
  AttributionReportScheduler& operator=(
      const AttributionReportScheduler& other) = delete;
  AttributionReportScheduler(AttributionReportScheduler&& other) = delete;
  AttributionReportScheduler& operator=(AttributionReportScheduler&& other) =
      delete;

  void Refresh();
  void ScheduleSend(absl::optional<base::Time> time);

 private:
  // Needed to avoid requiring `send_reports_` being safe to call after the
  // manager or `this` is destroyed.
  void InvokeCallback();

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(
      network::mojom::ConnectionType connection_type) override;

  base::RepeatingClosure send_reports_;
  base::WallClockTimer get_reports_to_send_timer_;
  base::SequenceBound<AttributionStorage>& attribution_storage_;
  base::WeakPtrFactory<AttributionReportScheduler> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORT_SCHEDULER_H_
