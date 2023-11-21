// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_OBSERVER_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_OBSERVER_H_

#include <optional>

#include "base/observer_list_types.h"
#include "content/browser/aggregation_service/aggregation_service_storage.h"

namespace base {
class Time;
}  // namespace base

namespace content {

class AggregatableReport;
class AggregatableReportRequest;

// Observes events in the Aggregation Service. Observers are registered on
// `AggregationService`.
class AggregationServiceObserver : public base::CheckedObserver {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum ReportStatus {
    kSent = 0,
    kFailedToAssemble = 1,
    kFailedToSend = 2,
    kMaxValue = kFailedToSend,
  };

  ~AggregationServiceObserver() override = default;

  // Called when requests in storage change.
  virtual void OnRequestStorageModified() {}

  // Called when a report has been handled, i.e. attempted to be assembled and
  // sent, regardless of success. `report_handled_time` indicates when the
  // report has been handled. `id` should be `std::nullopt` iff the request was
  // not stored/scheduled.
  virtual void OnReportHandled(
      const AggregatableReportRequest& request,
      std::optional<AggregationServiceStorage::RequestId> id,
      const std::optional<AggregatableReport>& report,
      base::Time report_handled_time,
      ReportStatus status) {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_OBSERVER_H_
