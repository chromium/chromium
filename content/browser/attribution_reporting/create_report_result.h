// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_CREATE_REPORT_RESULT_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_CREATE_REPORT_RESULT_H_

#include <stdint.h>

#include <optional>

#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT CreateReportResult {
 public:
  struct Limits {
    // `std::nullopt` unless `event_level_status_` or `aggregatable_status_` is
    // `kExcessiveAttributions`.
    std::optional<int64_t> rate_limits_max_attributions;

    // `std::nullopt` unless `event_level_status_` or `aggregatable_status_` is
    // `kExcessiveReportingOrigins`.
    std::optional<int64_t> rate_limits_max_attribution_reporting_origins;

    // `std::nullopt` unless `event_level_status_` is
    // `kNoCapacityForConversionDestination`.
    std::optional<int> max_event_level_reports_per_destination;

    // `std::nullopt` unless `aggregatable_status_` is
    // `kNoCapacityForConversionDestination`.
    std::optional<int> max_aggregatable_reports_per_destination;

    // `std::nullopt` unless `aggregatable_status_` is
    // `kExcessiveReports`..
    std::optional<int> max_aggregatable_reports_per_source;
  };

  CreateReportResult(
      base::Time trigger_time,
      AttributionTrigger,
      AttributionTrigger::EventLevelResult event_level_status,
      AttributionTrigger::AggregatableResult aggregatable_status,
      std::optional<AttributionReport> replaced_event_level_report =
          std::nullopt,
      std::optional<AttributionReport> new_event_level_report = std::nullopt,
      std::optional<AttributionReport> new_aggregatable_report = std::nullopt,
      std::optional<StoredSource> source = std::nullopt,
      Limits limits = Limits(),
      std::optional<AttributionReport> dropped_event_level_report =
          std::nullopt,
      std::optional<base::Time> min_null_aggregatble_report_time =
          std::nullopt);
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

  const std::optional<AttributionReport>& replaced_event_level_report() const {
    return replaced_event_level_report_;
  }

  const std::optional<AttributionReport>& new_event_level_report() const {
    return new_event_level_report_;
  }

  std::optional<AttributionReport>& new_event_level_report() {
    return new_event_level_report_;
  }

  const std::optional<AttributionReport>& new_aggregatable_report() const {
    return new_aggregatable_report_;
  }

  std::optional<AttributionReport>& new_aggregatable_report() {
    return new_aggregatable_report_;
  }

  const std::optional<StoredSource>& source() const { return source_; }

  const Limits& limits() const { return limits_; }

  const std::optional<AttributionReport>& dropped_event_level_report() const {
    return dropped_event_level_report_;
  }

  std::optional<base::Time> min_null_aggregatable_report_time() const {
    return min_null_aggregatable_report_time_;
  }

  const AttributionTrigger& trigger() const { return trigger_; }

 private:
  base::Time trigger_time_;

  AttributionTrigger::EventLevelResult event_level_status_;

  AttributionTrigger::AggregatableResult aggregatable_status_;

  // `std::nullopt` unless `event_level_status_` is
  // `kSuccessDroppedLowerPriority`.
  std::optional<AttributionReport> replaced_event_level_report_;

  // `std::nullopt` unless `event_level_status_` is `kSuccess` or
  // `kSuccessDroppedLowerPriority`.
  std::optional<AttributionReport> new_event_level_report_;

  // `std::nullopt` unless `aggregatable_status_` is `kSuccess`.
  std::optional<AttributionReport> new_aggregatable_report_;

  // `std::nullopt` if there's no matching source.
  std::optional<StoredSource> source_;

  Limits limits_;

  // `std::nullopt` unless `event_level_status_` is `kPriorityTooLow` or
  // `kExcessiveReports`.
  std::optional<AttributionReport> dropped_event_level_report_;

  std::optional<base::Time> min_null_aggregatable_report_time_;

  AttributionTrigger trigger_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_CREATE_REPORT_RESULT_H_
