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
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {

class CONTENT_EXPORT CreateReportResult {
 public:
  struct CONTENT_EXPORT EventLevelSuccess {
    AttributionReport new_report;
    std::optional<AttributionReport> replaced_report;

    EventLevelSuccess(AttributionReport new_report,
                      std::optional<AttributionReport> replaced_report);

    ~EventLevelSuccess();

    EventLevelSuccess(const EventLevelSuccess&);
    EventLevelSuccess& operator=(const EventLevelSuccess&);

    EventLevelSuccess(EventLevelSuccess&&);
    EventLevelSuccess& operator=(EventLevelSuccess&&);
  };

  struct InternalError {};

  struct NoCapacityForConversionDestination {
    int max;
    explicit NoCapacityForConversionDestination(int max) : max(max) {}
  };

  struct NoMatchingImpressions {};

  struct Deduplicated {};

  struct ExcessiveAttributions {
    int64_t max;
    explicit ExcessiveAttributions(int64_t max) : max(max) {}
  };

  struct PriorityTooLow {
    AttributionReport dropped_report;
  };

  struct NeverAttributedSource {};

  struct ExcessiveReportingOrigins {
    int64_t max;
    explicit ExcessiveReportingOrigins(int64_t max) : max(max) {}
  };

  struct NoMatchingSourceFilterData {};

  struct ProhibitedByBrowserPolicy {};

  struct NoMatchingConfigurations {};

  struct ExcessiveEventLevelReports {
    AttributionReport dropped_report;
  };

  struct FalselyAttributedSource {};

  struct ReportWindowPassed {};

  struct NotRegistered {};

  struct ReportWindowNotStarted {};

  struct NoMatchingTriggerData {};

  struct AggregatableSuccess {
    AttributionReport new_report;
  };

  struct ExcessiveAggregatableReports {
    int max;
    explicit ExcessiveAggregatableReports(int max) : max(max) {}
  };

  struct NoHistograms {};

  struct InsufficientBudget {};

  using EventLevel = absl::variant<EventLevelSuccess,
                                   InternalError,
                                   NoCapacityForConversionDestination,
                                   NoMatchingImpressions,
                                   Deduplicated,
                                   ExcessiveAttributions,
                                   PriorityTooLow,
                                   NeverAttributedSource,
                                   ExcessiveReportingOrigins,
                                   NoMatchingSourceFilterData,
                                   ProhibitedByBrowserPolicy,
                                   NoMatchingConfigurations,
                                   ExcessiveEventLevelReports,
                                   FalselyAttributedSource,
                                   ReportWindowPassed,
                                   NotRegistered,
                                   ReportWindowNotStarted,
                                   NoMatchingTriggerData>;

  using Aggregatable = absl::variant<AggregatableSuccess,
                                     InternalError,
                                     NoCapacityForConversionDestination,
                                     NoMatchingImpressions,
                                     ExcessiveAttributions,
                                     ExcessiveReportingOrigins,
                                     NoHistograms,
                                     InsufficientBudget,
                                     NoMatchingSourceFilterData,
                                     NotRegistered,
                                     ProhibitedByBrowserPolicy,
                                     Deduplicated,
                                     ReportWindowPassed,
                                     ExcessiveAggregatableReports>;

  // TODO(apaseltiner): Remove this struct in favor of moving the individual
  // fields into the variant structs.
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

  // TODO(apaseltiner): Remove this constructor.
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
      std::optional<base::Time> min_null_aggregatable_report_time =
          std::nullopt);

  CreateReportResult(
      base::Time trigger_time,
      AttributionTrigger,
      EventLevel,
      Aggregatable,
      std::optional<StoredSource> source,
      std::optional<base::Time> min_null_aggregatable_report_time);

  ~CreateReportResult();

  CreateReportResult(const CreateReportResult&);
  CreateReportResult(CreateReportResult&&);

  CreateReportResult& operator=(const CreateReportResult&);
  CreateReportResult& operator=(CreateReportResult&&);

  base::Time trigger_time() const { return trigger_time_; }

  AttributionTrigger::EventLevelResult event_level_status() const;

  const EventLevel& event_level_result() const { return event_level_result_; }

  AttributionTrigger::AggregatableResult aggregatable_status() const;

  const Aggregatable& aggregatable_result() const {
    return aggregatable_result_;
  }

  const AttributionReport* replaced_event_level_report() const;

  const AttributionReport* new_event_level_report() const;

  AttributionReport* new_event_level_report();

  const AttributionReport* new_aggregatable_report() const;

  AttributionReport* new_aggregatable_report();

  const std::optional<StoredSource>& source() const { return source_; }

  const AttributionReport* dropped_event_level_report() const;

  std::optional<base::Time> min_null_aggregatable_report_time() const {
    return min_null_aggregatable_report_time_;
  }

  const AttributionTrigger& trigger() const { return trigger_; }

 private:
  base::Time trigger_time_;

  // `std::nullopt` if there's no matching source.
  // TODO(apaseltiner): Combine this field with the result fields below.
  std::optional<StoredSource> source_;

  std::optional<base::Time> min_null_aggregatable_report_time_;

  EventLevel event_level_result_;
  Aggregatable aggregatable_result_;

  AttributionTrigger trigger_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_CREATE_REPORT_RESULT_H_
