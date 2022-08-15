// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_OBSERVER_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_OBSERVER_H_

#include "base/observer_list_types.h"
#include "content/browser/aggregation_service/aggregation_service_storage.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class Time;
}  // namespace base

namespace content {

class AggregatableReport;

// Observes events in the Aggregation Service. Observers are registered on
// `AggregationService`.
class AggregationServiceObserver : public base::CheckedObserver {
 public:
  enum ReportStatus {
    kPending,
    kSent,
    kFailedToAssemble,
    kFailedToSend,
  };

  ~AggregationServiceObserver() override = default;

  // Called when requests in storage change.
  virtual void OnRequestStorageModified() {}

  // Called when a report has been handled, i.e. attempted to be assembled and
  // sent, regardless of success. `report_handled_time` indicates when the
  // report has been handled.
  virtual void OnReportHandled(
      const AggregationServiceStorage::RequestAndId& request_and_id,
      const absl::optional<AggregatableReport>& report,
      base::Time report_handled_time,
      ReportStatus status) {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_OBSERVER_H_
