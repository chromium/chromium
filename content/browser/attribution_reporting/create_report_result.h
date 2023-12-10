// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_CREATE_REPORT_RESULT_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_CREATE_REPORT_RESULT_H_

#include <stdint.h>

#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

class CONTENT_EXPORT CreateReportResult {
 public:
  struct Limits {
    // `absl::nullopt` unless `event_level_status_` or `aggregatable_status_` is
    // `kExcessiveAttributions`.
    absl::optional<int64_t> rate_limits_max_attributions;

    // `absl::nullopt` unless `event_level_status_` or `aggregatable_status_` is
    // `kExcessiveReportingOrigins`.
    absl::optional<int64_t> rate_limits_max_attribution_reporting_origins;

    // `absl::nullopt` unless `event_level_status_` is
    // `kNoCapacityForConversionDestination`.
    absl::optional<int> max_event_level_reports_per_destination;

    // `absl::nullopt` unless `aggregatable_status_` is
    // `kNoCapacityForConversionDestination`.
    absl::optional<int> max_aggregatable_reports_per_destination;

    // `absl::nullopt` unless `aggregatable_status_` is
    // `kExcessiveReports`..
    absl::optional<int> max_aggregatable_reports_per_source;
  };

  CreateReportResult(
      base::Time trigger_time,
      AttributionTrigger::EventLevelResult event_level_status,
      AttributionTrigger::AggregatableResult aggregatable_status,
      absl::optional<AttributionReport> replaced_event_level_report =
          absl::nullopt,
      absl::optional<AttributionReport> new_event_level_report = absl::nullopt,
      absl::optional<AttributionReport> new_aggregatable_report = absl::nullopt,
      absl::optional<StoredSource> source = absl::nullopt,
      Limits limits = Limits(),
      absl::optional<AttributionReport> dropped_event_level_report =
          absl::nullopt,
      absl::optional<base::Time> min_null_aggregatble_report_time =
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

  const absl::optional<StoredSource>& source() const { return source_; }

  const Limits& limits() const { return limits_; }

  const absl::optional<AttributionReport>& dropped_event_level_report() const {
    return dropped_event_level_report_;
  }

  absl::optional<base::Time> min_null_aggregatable_report_time() const {
    return min_null_aggregatable_report_time_;
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

  // `absl::nullopt` if there's no matching source.
  absl::optional<StoredSource> source_;

  Limits limits_;

  // `absl::nullopt` unless `event_level_status_` is `kPriorityTooLow` or
  // `kExcessiveReports`.
  absl::optional<AttributionReport> dropped_event_level_report_;

  absl::optional<base::Time> min_null_aggregatable_report_time_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_CREATE_REPORT_RESULT_H_
