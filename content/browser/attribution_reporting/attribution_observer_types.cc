// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_observer_types.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"

namespace content {

namespace {

using EventLevelResult = ::content::AttributionTrigger::EventLevelResult;
using AggregatableResult = ::content::AttributionTrigger::AggregatableResult;

}  // namespace

CreateReportResult::CreateReportResult(
    base::Time trigger_time,
    EventLevelResult event_level_status,
    AggregatableResult aggregatable_status,
    absl::optional<AttributionReport> replaced_event_level_report,
    absl::optional<AttributionReport> new_event_level_report,
    absl::optional<AttributionReport> new_aggregatable_report,
    absl::optional<StoredSource> source,
    Limits limits,
    absl::optional<AttributionReport> dropped_event_level_report)
    : trigger_time_(trigger_time),
      event_level_status_(event_level_status),
      aggregatable_status_(aggregatable_status),
      replaced_event_level_report_(std::move(replaced_event_level_report)),
      new_event_level_report_(std::move(new_event_level_report)),
      new_aggregatable_report_(std::move(new_aggregatable_report)),
      source_(std::move(source)),
      limits_(limits),
      dropped_event_level_report_(std::move(dropped_event_level_report)) {
  DCHECK_EQ(
      event_level_status_ == EventLevelResult::kSuccess ||
          event_level_status_ == EventLevelResult::kSuccessDroppedLowerPriority,
      new_event_level_report_.has_value());

  DCHECK(!new_event_level_report_.has_value() ||
         new_event_level_report_->GetReportType() ==
             AttributionReport::Type::kEventLevel);

  DCHECK_EQ(aggregatable_status_ == AggregatableResult::kSuccess,
            new_aggregatable_report_.has_value());

  DCHECK(!new_aggregatable_report_.has_value() ||
         new_aggregatable_report_->GetReportType() ==
             AttributionReport::Type::kAggregatableAttribution);

  DCHECK_EQ(
      replaced_event_level_report_.has_value(),
      event_level_status_ == EventLevelResult::kSuccessDroppedLowerPriority);

  if (event_level_status_ != EventLevelResult::kInternalError &&
      event_level_status_ != EventLevelResult::kNotRegistered) {
    DCHECK_EQ(source_.has_value(),
              event_level_status_ != EventLevelResult::kNoMatchingImpressions &&
                  event_level_status_ !=
                      EventLevelResult::kProhibitedByBrowserPolicy);
  }

  if (aggregatable_status_ != AggregatableResult::kInternalError &&
      aggregatable_status_ != AggregatableResult::kNotRegistered) {
    DCHECK_EQ(
        source_.has_value(),
        aggregatable_status_ != AggregatableResult::kNoMatchingImpressions &&
            aggregatable_status_ !=
                AggregatableResult::kProhibitedByBrowserPolicy);
  }

  DCHECK_EQ(
      limits.rate_limits_max_attributions.has_value(),
      event_level_status_ == EventLevelResult::kExcessiveAttributions ||
          aggregatable_status_ == AggregatableResult::kExcessiveAttributions);

  DCHECK_EQ(limits.aggregatable_budget_per_source.has_value(),
            aggregatable_status_ == AggregatableResult::kInsufficientBudget);

  DCHECK_EQ(
      limits.rate_limits_max_attribution_reporting_origins.has_value(),
      event_level_status_ == EventLevelResult::kExcessiveReportingOrigins ||
          aggregatable_status_ ==
              AggregatableResult::kExcessiveReportingOrigins);

  DCHECK_EQ(limits.max_event_level_reports_per_destination.has_value(),
            event_level_status_ ==
                EventLevelResult::kNoCapacityForConversionDestination);

  DCHECK_EQ(limits.max_aggregatable_reports_per_destination.has_value(),
            aggregatable_status_ ==
                AggregatableResult::kNoCapacityForConversionDestination);

  DCHECK_EQ(dropped_event_level_report_.has_value(),
            event_level_status_ == EventLevelResult::kPriorityTooLow ||
                event_level_status_ == EventLevelResult::kExcessiveReports);
  DCHECK(!dropped_event_level_report_.has_value() ||
         dropped_event_level_report_->GetReportType() ==
             AttributionReport::Type::kEventLevel);
}

CreateReportResult::~CreateReportResult() = default;

CreateReportResult::CreateReportResult(const CreateReportResult&) = default;
CreateReportResult::CreateReportResult(CreateReportResult&&) = default;

CreateReportResult& CreateReportResult::operator=(const CreateReportResult&) =
    default;
CreateReportResult& CreateReportResult::operator=(CreateReportResult&&) =
    default;

}  // namespace content
