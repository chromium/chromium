// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OBSERVER_TYPES_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OBSERVER_TYPES_H_

#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

class CONTENT_EXPORT CreateReportResult {
 public:
  CreateReportResult(
      base::Time trigger_time,
      AttributionTrigger::EventLevelResult event_level_status,
      AttributionTrigger::AggregatableResult aggregatable_status,
      absl::optional<AttributionReport> replaced_event_level_report =
          absl::nullopt,
      absl::optional<AttributionReport> new_event_level_report = absl::nullopt,
      absl::optional<AttributionReport> new_aggregatable_report =
          absl::nullopt);
  ~CreateReportResult();

  CreateReportResult(const CreateReportResult&);
  CreateReportResult(CreateReportResult&&);

  CreateReportResult& operator=(const CreateReportResult&);
  CreateReportResult& operator=(CreateReportResult&&);

  base::Time trigger_time() const { return trigger_time_; }

  AttributionTrigger::EventLevelResult event_level_status() const {
    return event_level_status_;
  }

  AttributionTrigger::AggregatableResult aggregatable_status() const {
    return aggregatable_status_;
  }

  const absl::optional<AttributionReport>& replaced_event_level_report() const {
    return replaced_event_level_report_;
  }

  const absl::optional<AttributionReport>& new_event_level_report() const {
    return new_event_level_report_;
  }

  absl::optional<AttributionReport>& new_event_level_report() {
    return new_event_level_report_;
  }

  const absl::optional<AttributionReport>& new_aggregatable_report() const {
    return new_aggregatable_report_;
  }

  absl::optional<AttributionReport>& new_aggregatable_report() {
    return new_aggregatable_report_;
  }

 private:
  base::Time trigger_time_;

  AttributionTrigger::EventLevelResult event_level_status_;

  AttributionTrigger::AggregatableResult aggregatable_status_;

  // `absl::nullopt` unless `event_level_status_` is
  // `kSuccessDroppedLowerPriority`.
  absl::optional<AttributionReport> replaced_event_level_report_;

  // `absl::nullopt` unless `event_level_status_` is `kSuccess` or
  // `kSuccessDroppedLowerPriority`.
  absl::optional<AttributionReport> new_event_level_report_;

  // `absl::nullopt` unless `aggregatable_status_` is `kSuccess`.
  absl::optional<AttributionReport> new_aggregatable_report_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OBSERVER_TYPES_H_
