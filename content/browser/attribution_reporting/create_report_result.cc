// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/create_report_result.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/overloaded.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/types/optional_util.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {

namespace {

using EventLevelResult = ::content::AttributionTrigger::EventLevelResult;
using AggregatableResult = ::content::AttributionTrigger::AggregatableResult;

using EventLevelSuccess = ::content::CreateReportResult::EventLevelSuccess;

}  // namespace

EventLevelSuccess::EventLevelSuccess(
    AttributionReport new_report,
    std::optional<AttributionReport> replaced_report)
    : new_report(std::move(new_report)),
      replaced_report(std::move(replaced_report)) {}

EventLevelSuccess::~EventLevelSuccess() = default;

EventLevelSuccess::EventLevelSuccess(const EventLevelSuccess&) = default;

EventLevelSuccess& EventLevelSuccess::operator=(const EventLevelSuccess&) =
    default;

EventLevelSuccess::EventLevelSuccess(EventLevelSuccess&&) = default;

EventLevelSuccess& EventLevelSuccess::operator=(EventLevelSuccess&&) = default;

namespace {

CreateReportResult::EventLevel CreateEventLevelResult(
    EventLevelResult event_level_status,
    const CreateReportResult::Limits& limits,
    std::optional<AttributionReport> replaced_event_level_report,
    std::optional<AttributionReport> new_event_level_report,
    std::optional<AttributionReport> dropped_event_level_report) {
  switch (event_level_status) {
    case EventLevelResult::kSuccess:
      DCHECK(new_event_level_report.has_value());
      DCHECK_EQ(new_event_level_report->GetReportType(),
                AttributionReport::Type::kEventLevel);
      return CreateReportResult::EventLevelSuccess(
          *std::move(new_event_level_report),
          /*replaced_report=*/std::nullopt);
    case EventLevelResult::kSuccessDroppedLowerPriority:
      DCHECK(new_event_level_report.has_value());
      DCHECK_EQ(new_event_level_report->GetReportType(),
                AttributionReport::Type::kEventLevel);
      DCHECK(replaced_event_level_report.has_value());
      DCHECK_EQ(replaced_event_level_report->GetReportType(),
                AttributionReport::Type::kEventLevel);
      return CreateReportResult::EventLevelSuccess(
          *std::move(new_event_level_report),
          std::move(replaced_event_level_report));
    case EventLevelResult::kInternalError:
      return CreateReportResult::InternalError();
    case EventLevelResult::kNoCapacityForConversionDestination:
      DCHECK(limits.max_event_level_reports_per_destination.has_value());
      return CreateReportResult::NoCapacityForConversionDestination(
          *limits.max_event_level_reports_per_destination);
    case EventLevelResult::kNoMatchingImpressions:
      return CreateReportResult::NoMatchingImpressions();
    case EventLevelResult::kDeduplicated:
      return CreateReportResult::Deduplicated();
    case EventLevelResult::kExcessiveAttributions:
      DCHECK(limits.rate_limits_max_attributions.has_value());
      return CreateReportResult::ExcessiveAttributions(
          *limits.rate_limits_max_attributions);
    case EventLevelResult::kPriorityTooLow:
      DCHECK(dropped_event_level_report.has_value());
      DCHECK_EQ(dropped_event_level_report->GetReportType(),
                AttributionReport::Type::kEventLevel);
      return CreateReportResult::PriorityTooLow(
          *std::move(dropped_event_level_report));
    case EventLevelResult::kNeverAttributedSource:
      return CreateReportResult::NeverAttributedSource();
    case EventLevelResult::kExcessiveReportingOrigins:
      DCHECK(limits.rate_limits_max_attribution_reporting_origins.has_value());
      return CreateReportResult::ExcessiveReportingOrigins(
          *limits.rate_limits_max_attribution_reporting_origins);
    case EventLevelResult::kNoMatchingSourceFilterData:
      return CreateReportResult::NoMatchingSourceFilterData();
    case EventLevelResult::kProhibitedByBrowserPolicy:
      return CreateReportResult::ProhibitedByBrowserPolicy();
    case EventLevelResult::kNoMatchingConfigurations:
      return CreateReportResult::NoMatchingConfigurations();
    case EventLevelResult::kExcessiveReports:
      DCHECK(dropped_event_level_report.has_value());
      DCHECK_EQ(dropped_event_level_report->GetReportType(),
                AttributionReport::Type::kEventLevel);
      return CreateReportResult::ExcessiveEventLevelReports(
          *std::move(dropped_event_level_report));
    case EventLevelResult::kFalselyAttributedSource:
      return CreateReportResult::FalselyAttributedSource();
    case EventLevelResult::kReportWindowPassed:
      return CreateReportResult::ReportWindowPassed();
    case EventLevelResult::kNotRegistered:
      return CreateReportResult::NotRegistered();
    case EventLevelResult::kReportWindowNotStarted:
      return CreateReportResult::ReportWindowNotStarted();
    case EventLevelResult::kNoMatchingTriggerData:
      return CreateReportResult::NoMatchingTriggerData();
  }
  NOTREACHED();
}

CreateReportResult::Aggregatable CreateAggregatableResult(
    AggregatableResult aggregatable_status,
    const CreateReportResult::Limits& limits,
    std::optional<AttributionReport> new_aggregatable_report) {
  switch (aggregatable_status) {
    case AggregatableResult::kSuccess:
      DCHECK(new_aggregatable_report.has_value());
      DCHECK_EQ(new_aggregatable_report->GetReportType(),
                AttributionReport::Type::kAggregatableAttribution);
      return CreateReportResult::AggregatableSuccess(
          *std::move(new_aggregatable_report));
    case AggregatableResult::kInternalError:
      return CreateReportResult::InternalError();
    case AggregatableResult::kNoCapacityForConversionDestination:
      DCHECK(limits.max_aggregatable_reports_per_destination.has_value());
      return CreateReportResult::NoCapacityForConversionDestination(
          *limits.max_aggregatable_reports_per_destination);
    case AggregatableResult::kNoMatchingImpressions:
      return CreateReportResult::NoMatchingImpressions();
    case AggregatableResult::kExcessiveAttributions:
      DCHECK(limits.rate_limits_max_attributions.has_value());
      return CreateReportResult::ExcessiveAttributions(
          *limits.rate_limits_max_attributions);
    case AggregatableResult::kExcessiveReportingOrigins:
      DCHECK(limits.rate_limits_max_attribution_reporting_origins.has_value());
      return CreateReportResult::ExcessiveReportingOrigins(
          *limits.rate_limits_max_attribution_reporting_origins);
    case AggregatableResult::kNoHistograms:
      return CreateReportResult::NoHistograms();
    case AggregatableResult::kInsufficientBudget:
      return CreateReportResult::InsufficientBudget();
    case AggregatableResult::kNoMatchingSourceFilterData:
      return CreateReportResult::NoMatchingSourceFilterData();
    case AggregatableResult::kNotRegistered:
      return CreateReportResult::NotRegistered();
    case AggregatableResult::kProhibitedByBrowserPolicy:
      return CreateReportResult::ProhibitedByBrowserPolicy();
    case AggregatableResult::kDeduplicated:
      return CreateReportResult::Deduplicated();
    case AggregatableResult::kReportWindowPassed:
      return CreateReportResult::ReportWindowPassed();
    case AggregatableResult::kExcessiveReports:
      DCHECK(limits.max_aggregatable_reports_per_source.has_value());
      return CreateReportResult::ExcessiveAggregatableReports(
          *limits.max_aggregatable_reports_per_source);
  }
  NOTREACHED();
}

}  // namespace

CreateReportResult::CreateReportResult(
    base::Time trigger_time,
    AttributionTrigger trigger,
    EventLevelResult event_level_status,
    AggregatableResult aggregatable_status,
    std::optional<AttributionReport> replaced_event_level_report,
    std::optional<AttributionReport> new_event_level_report,
    std::optional<AttributionReport> new_aggregatable_report,
    std::optional<StoredSource> source,
    const Limits limits,
    std::optional<AttributionReport> dropped_event_level_report,
    std::optional<base::Time> min_null_aggregatable_report_time)
    : CreateReportResult(
          trigger_time,
          std::move(trigger),
          CreateEventLevelResult(event_level_status,
                                 limits,
                                 std::move(replaced_event_level_report),
                                 std::move(new_event_level_report),
                                 std::move(dropped_event_level_report)),
          CreateAggregatableResult(aggregatable_status,
                                   limits,
                                   std::move(new_aggregatable_report)),
          std::move(source),
          min_null_aggregatable_report_time) {}

CreateReportResult::CreateReportResult(
    base::Time trigger_time,
    AttributionTrigger trigger,
    EventLevel event_level_result,
    Aggregatable aggregatable_result,
    std::optional<StoredSource> source,
    std::optional<base::Time> min_null_aggregatable_report_time)
    : trigger_time_(trigger_time),
      source_(std::move(source)),
      min_null_aggregatable_report_time_(min_null_aggregatable_report_time),
      event_level_result_(std::move(event_level_result)),
      aggregatable_result_(std::move(aggregatable_result)),
      trigger_(std::move(trigger)) {
  if (EventLevelResult event_level_status = this->event_level_status();
      event_level_status != EventLevelResult::kInternalError &&
      event_level_status != EventLevelResult::kNotRegistered) {
    DCHECK_EQ(
        source_.has_value(),
        event_level_status != EventLevelResult::kNoMatchingImpressions &&
            event_level_status != EventLevelResult::kProhibitedByBrowserPolicy);
  }

  if (AggregatableResult aggregatable_status = this->aggregatable_status();
      aggregatable_status != AggregatableResult::kInternalError &&
      aggregatable_status != AggregatableResult::kNotRegistered) {
    DCHECK_EQ(
        source_.has_value(),
        aggregatable_status != AggregatableResult::kNoMatchingImpressions &&
            aggregatable_status !=
                AggregatableResult::kProhibitedByBrowserPolicy);
  }
}

CreateReportResult::~CreateReportResult() = default;

CreateReportResult::CreateReportResult(const CreateReportResult&) = default;
CreateReportResult::CreateReportResult(CreateReportResult&&) = default;

CreateReportResult& CreateReportResult::operator=(const CreateReportResult&) =
    default;
CreateReportResult& CreateReportResult::operator=(CreateReportResult&&) =
    default;

EventLevelResult CreateReportResult::event_level_status() const {
  return absl::visit(
      base::Overloaded{
          [](const EventLevelSuccess& v) {
            return v.replaced_report.has_value()
                       ? EventLevelResult::kSuccessDroppedLowerPriority
                       : EventLevelResult::kSuccess;
          },
          [](const InternalError&) { return EventLevelResult::kInternalError; },
          [](const NoCapacityForConversionDestination&) {
            return EventLevelResult::kNoCapacityForConversionDestination;
          },
          [](const NoMatchingImpressions&) {
            return EventLevelResult::kNoMatchingImpressions;
          },
          [](const Deduplicated&) { return EventLevelResult::kDeduplicated; },
          [](const ExcessiveAttributions&) {
            return EventLevelResult::kExcessiveAttributions;
          },
          [](const PriorityTooLow&) {
            return EventLevelResult::kPriorityTooLow;
          },
          [](const NeverAttributedSource&) {
            return EventLevelResult::kNeverAttributedSource;
          },
          [](const ExcessiveReportingOrigins&) {
            return EventLevelResult::kExcessiveReportingOrigins;
          },
          [](const NoMatchingSourceFilterData&) {
            return EventLevelResult::kNoMatchingSourceFilterData;
          },
          [](const ProhibitedByBrowserPolicy&) {
            return EventLevelResult::kProhibitedByBrowserPolicy;
          },
          [](const NoMatchingConfigurations&) {
            return EventLevelResult::kNoMatchingConfigurations;
          },
          [](const ExcessiveEventLevelReports&) {
            return EventLevelResult::kExcessiveReports;
          },
          [](const FalselyAttributedSource&) {
            return EventLevelResult::kFalselyAttributedSource;
          },
          [](const ReportWindowPassed&) {
            return EventLevelResult::kReportWindowPassed;
          },
          [](const NotRegistered&) { return EventLevelResult::kNotRegistered; },
          [](const ReportWindowNotStarted&) {
            return EventLevelResult::kReportWindowNotStarted;
          },
          [](const NoMatchingTriggerData&) {
            return EventLevelResult::kNoMatchingTriggerData;
          },
      },
      event_level_result_);
}

AggregatableResult CreateReportResult::aggregatable_status() const {
  return absl::visit(
      base::Overloaded{
          [](const AggregatableSuccess&) {
            return AggregatableResult::kSuccess;
          },
          [](const InternalError&) {
            return AggregatableResult::kInternalError;
          },
          [](const NoCapacityForConversionDestination&) {
            return AggregatableResult::kNoCapacityForConversionDestination;
          },
          [](const NoMatchingImpressions&) {
            return AggregatableResult::kNoMatchingImpressions;
          },
          [](const ExcessiveAttributions&) {
            return AggregatableResult::kExcessiveAttributions;
          },
          [](const ExcessiveReportingOrigins&) {
            return AggregatableResult::kExcessiveReportingOrigins;
          },
          [](const NoHistograms&) { return AggregatableResult::kNoHistograms; },
          [](const InsufficientBudget&) {
            return AggregatableResult::kInsufficientBudget;
          },
          [](const NoMatchingSourceFilterData&) {
            return AggregatableResult::kNoMatchingSourceFilterData;
          },
          [](const NotRegistered&) {
            return AggregatableResult::kNotRegistered;
          },
          [](const ProhibitedByBrowserPolicy&) {
            return AggregatableResult::kProhibitedByBrowserPolicy;
          },
          [](const Deduplicated&) { return AggregatableResult::kDeduplicated; },
          [](const ReportWindowPassed&) {
            return AggregatableResult::kReportWindowPassed;
          },
          [](const ExcessiveAggregatableReports) {
            return AggregatableResult::kExcessiveReports;
          },
      },
      aggregatable_result_);
}

const AttributionReport* CreateReportResult::replaced_event_level_report()
    const {
  if (const auto* v = absl::get_if<EventLevelSuccess>(&event_level_result_)) {
    return base::OptionalToPtr(v->replaced_report);
  }
  return nullptr;
}

const AttributionReport* CreateReportResult::new_event_level_report() const {
  if (const auto* v = absl::get_if<EventLevelSuccess>(&event_level_result_)) {
    return &v->new_report;
  }
  return nullptr;
}

AttributionReport* CreateReportResult::new_event_level_report() {
  if (auto* v = absl::get_if<EventLevelSuccess>(&event_level_result_)) {
    return &v->new_report;
  }
  return nullptr;
}

const AttributionReport* CreateReportResult::new_aggregatable_report() const {
  if (const auto* v =
          absl::get_if<AggregatableSuccess>(&aggregatable_result_)) {
    return &v->new_report;
  }
  return nullptr;
}

AttributionReport* CreateReportResult::new_aggregatable_report() {
  if (auto* v = absl::get_if<AggregatableSuccess>(&aggregatable_result_)) {
    return &v->new_report;
  }
  return nullptr;
}

const AttributionReport* CreateReportResult::dropped_event_level_report()
    const {
  return absl::visit(
      base::Overloaded{
          [](const PriorityTooLow& v) { return &v.dropped_report; },
          [](const ExcessiveEventLevelReports& v) { return &v.dropped_report; },
          [](const auto&) -> const AttributionReport* { return nullptr; }},
      event_level_result_);
}

}  // namespace content
