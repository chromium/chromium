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
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {

namespace {

using EventLevelResult = ::content::AttributionTrigger::EventLevelResult;
using AggregatableResult = ::content::AttributionTrigger::AggregatableResult;

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
    Limits limits,
    std::optional<AttributionReport> dropped_event_level_report,
    std::optional<base::Time> min_null_aggregatable_report_time)
    : trigger_time_(trigger_time),
      source_(std::move(source)),
      min_null_aggregatable_report_time_(min_null_aggregatable_report_time),
      trigger_(std::move(trigger)) {
  switch (event_level_status) {
    case EventLevelResult::kSuccess:
      DCHECK(new_event_level_report.has_value());
      DCHECK_EQ(new_event_level_report->GetReportType(),
                AttributionReport::Type::kEventLevel);
      event_level_result_.emplace<Success>(*std::move(new_event_level_report));
      break;
    case EventLevelResult::kSuccessDroppedLowerPriority:
      DCHECK(new_event_level_report.has_value());
      DCHECK_EQ(new_event_level_report->GetReportType(),
                AttributionReport::Type::kEventLevel);
      DCHECK(replaced_event_level_report.has_value());
      DCHECK_EQ(replaced_event_level_report->GetReportType(),
                AttributionReport::Type::kEventLevel);
      event_level_result_.emplace<SuccessDroppedLowerPriority>(
          *std::move(new_event_level_report),
          *std::move(replaced_event_level_report));
      break;
    case EventLevelResult::kInternalError:
      event_level_result_.emplace<InternalError>();
      break;
    case EventLevelResult::kNoCapacityForConversionDestination:
      DCHECK(limits.max_event_level_reports_per_destination.has_value());
      event_level_result_.emplace<NoCapacityForConversionDestination>(
          *limits.max_event_level_reports_per_destination);
      break;
    case EventLevelResult::kNoMatchingImpressions:
      event_level_result_.emplace<NoMatchingImpressions>();
      break;
    case EventLevelResult::kDeduplicated:
      event_level_result_.emplace<Deduplicated>();
      break;
    case EventLevelResult::kExcessiveAttributions:
      DCHECK(limits.rate_limits_max_attributions.has_value());
      event_level_result_.emplace<ExcessiveAttributions>(
          *limits.rate_limits_max_attributions);
      break;
    case EventLevelResult::kPriorityTooLow:
      DCHECK(dropped_event_level_report.has_value());
      DCHECK_EQ(dropped_event_level_report->GetReportType(),
                AttributionReport::Type::kEventLevel);
      event_level_result_.emplace<PriorityTooLow>(
          *std::move(dropped_event_level_report));
      break;
    case EventLevelResult::kNeverAttributedSource:
      event_level_result_.emplace<NeverAttributedSource>();
      break;
    case EventLevelResult::kExcessiveReportingOrigins:
      DCHECK(limits.rate_limits_max_attribution_reporting_origins.has_value());
      event_level_result_.emplace<ExcessiveReportingOrigins>(
          *limits.rate_limits_max_attribution_reporting_origins);
      break;
    case EventLevelResult::kNoMatchingSourceFilterData:
      event_level_result_.emplace<NoMatchingSourceFilterData>();
      break;
    case EventLevelResult::kProhibitedByBrowserPolicy:
      event_level_result_.emplace<ProhibitedByBrowserPolicy>();
      break;
    case EventLevelResult::kNoMatchingConfigurations:
      event_level_result_.emplace<NoMatchingConfigurations>();
      break;
    case EventLevelResult::kExcessiveReports:
      DCHECK(dropped_event_level_report.has_value());
      DCHECK_EQ(dropped_event_level_report->GetReportType(),
                AttributionReport::Type::kEventLevel);
      event_level_result_.emplace<ExcessiveEventLevelReports>(
          *std::move(dropped_event_level_report));
      break;
    case EventLevelResult::kFalselyAttributedSource:
      event_level_result_.emplace<FalselyAttributedSource>();
      break;
    case EventLevelResult::kReportWindowPassed:
      event_level_result_.emplace<ReportWindowPassed>();
      break;
    case EventLevelResult::kNotRegistered:
      event_level_result_.emplace<NotRegistered>();
      break;
    case EventLevelResult::kReportWindowNotStarted:
      event_level_result_.emplace<ReportWindowNotStarted>();
      break;
    case EventLevelResult::kNoMatchingTriggerData:
      event_level_result_.emplace<NoMatchingTriggerData>();
      break;
    default:
      NOTREACHED_NORETURN();
  }

  switch (aggregatable_status) {
    case AggregatableResult::kSuccess:
      DCHECK(new_aggregatable_report.has_value());
      DCHECK_EQ(new_aggregatable_report->GetReportType(),
                AttributionReport::Type::kAggregatableAttribution);
      aggregatable_result_.emplace<Success>(
          *std::move(new_aggregatable_report));
      break;
    case AggregatableResult::kInternalError:
      aggregatable_result_.emplace<InternalError>();
      break;
    case AggregatableResult::kNoCapacityForConversionDestination:
      DCHECK(limits.max_aggregatable_reports_per_destination.has_value());
      aggregatable_result_.emplace<NoCapacityForConversionDestination>(
          *limits.max_aggregatable_reports_per_destination);
      break;
    case AggregatableResult::kNoMatchingImpressions:
      aggregatable_result_.emplace<NoMatchingImpressions>();
      break;
    case AggregatableResult::kExcessiveAttributions:
      DCHECK(limits.rate_limits_max_attributions.has_value());
      aggregatable_result_.emplace<ExcessiveAttributions>(
          *limits.rate_limits_max_attributions);
      break;
    case AggregatableResult::kExcessiveReportingOrigins:
      DCHECK(limits.rate_limits_max_attribution_reporting_origins.has_value());
      aggregatable_result_.emplace<ExcessiveReportingOrigins>(
          *limits.rate_limits_max_attribution_reporting_origins);
      break;
    case AggregatableResult::kNoHistograms:
      aggregatable_result_.emplace<NoHistograms>();
      break;
    case AggregatableResult::kInsufficientBudget:
      aggregatable_result_.emplace<InsufficientBudget>();
      break;
    case AggregatableResult::kNoMatchingSourceFilterData:
      aggregatable_result_.emplace<NoMatchingSourceFilterData>();
      break;
    case AggregatableResult::kNotRegistered:
      aggregatable_result_.emplace<NotRegistered>();
      break;
    case AggregatableResult::kProhibitedByBrowserPolicy:
      aggregatable_result_.emplace<ProhibitedByBrowserPolicy>();
      break;
    case AggregatableResult::kDeduplicated:
      aggregatable_result_.emplace<Deduplicated>();
      break;
    case AggregatableResult::kReportWindowPassed:
      aggregatable_result_.emplace<ReportWindowPassed>();
      break;
    case AggregatableResult::kExcessiveReports:
      DCHECK(limits.max_aggregatable_reports_per_source.has_value());
      aggregatable_result_.emplace<ExcessiveAggregatableReports>(
          *limits.max_aggregatable_reports_per_source);
      break;
    default:
      NOTREACHED_NORETURN();
  }

  if (event_level_status != EventLevelResult::kInternalError &&
      event_level_status != EventLevelResult::kNotRegistered) {
    DCHECK_EQ(
        source_.has_value(),
        event_level_status != EventLevelResult::kNoMatchingImpressions &&
            event_level_status != EventLevelResult::kProhibitedByBrowserPolicy);
  }

  if (aggregatable_status != AggregatableResult::kInternalError &&
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
          [](const Success&) { return EventLevelResult::kSuccess; },
          [](const SuccessDroppedLowerPriority&) {
            return EventLevelResult::kSuccessDroppedLowerPriority;
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
          [](const Success&) { return AggregatableResult::kSuccess; },
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
  if (const auto* v =
          absl::get_if<SuccessDroppedLowerPriority>(&event_level_result_)) {
    return &v->replaced_report;
  }
  return nullptr;
}

const AttributionReport* CreateReportResult::new_event_level_report() const {
  return absl::visit(
      base::Overloaded{
          [](const Success& v) { return &v.new_report; },
          [](const SuccessDroppedLowerPriority& v) { return &v.new_report; },
          [](const auto&) -> const AttributionReport* { return nullptr; }},
      event_level_result_);
}

AttributionReport* CreateReportResult::new_event_level_report() {
  return absl::visit(
      base::Overloaded{
          [](Success& v) { return &v.new_report; },
          [](SuccessDroppedLowerPriority& v) { return &v.new_report; },
          [](auto&) -> AttributionReport* { return nullptr; }},
      event_level_result_);
}

const AttributionReport* CreateReportResult::new_aggregatable_report() const {
  if (const auto* v = absl::get_if<Success>(&aggregatable_result_)) {
    return &v->new_report;
  }
  return nullptr;
}

AttributionReport* CreateReportResult::new_aggregatable_report() {
  if (auto* v = absl::get_if<Success>(&aggregatable_result_)) {
    return &v->new_report;
  }
  return nullptr;
}

CreateReportResult::Limits CreateReportResult::limits() const {
  Limits limits;

  absl::visit(base::Overloaded{
                  [&](const NoCapacityForConversionDestination& v) {
                    limits.max_event_level_reports_per_destination = v.max;
                  },
                  [&](const ExcessiveAttributions& v) {
                    limits.rate_limits_max_attributions = v.max;
                  },
                  [&](const ExcessiveReportingOrigins& v) {
                    limits.rate_limits_max_attribution_reporting_origins =
                        v.max;
                  },
                  [](const auto&) {},
              },
              event_level_result_);

  absl::visit(base::Overloaded{
                  [&](const NoCapacityForConversionDestination& v) {
                    limits.max_aggregatable_reports_per_destination = v.max;
                  },
                  [&](const ExcessiveAttributions& v) {
                    limits.rate_limits_max_attributions = v.max;
                  },
                  [&](const ExcessiveReportingOrigins& v) {
                    limits.rate_limits_max_attribution_reporting_origins =
                        v.max;
                  },
                  [&](const ExcessiveAggregatableReports& v) {
                    limits.max_aggregatable_reports_per_source = v.max;
                  },
                  [](const auto&) {},
              },
              aggregatable_result_);

  return limits;
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
