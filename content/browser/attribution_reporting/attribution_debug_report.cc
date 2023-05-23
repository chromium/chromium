// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_debug_report.h"

#include <stdint.h>

#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/attribution_reporting/destination_set.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/create_report_result.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/store_source_result.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content {

namespace {

using EventLevelResult = ::content::AttributionTrigger::EventLevelResult;
using AggregatableResult = ::content::AttributionTrigger::AggregatableResult;

constexpr char kAttributionDestination[] = "attribution_destination";

enum class DebugDataType {
  kSourceDestinationLimit,
  kSourceNoised,
  kSourceStorageLimit,
  kSourceSuccess,
  kSourceUnknownError,
  // TODO(tquintanilla): Add interop test for `kSourceDestinationRateLimit`
  // case.
  kSourceDestinationRateLimit,
  kTriggerNoMatchingSource,
  kTriggerAttributionsPerSourceDestinationLimit,
  kTriggerNoMatchingFilterData,
  kTriggerReportingOriginLimit,
  kTriggerEventDeduplicated,
  kTriggerEventNoMatchingConfigurations,
  kTriggerEventNoise,
  kTriggerEventLowPriority,
  kTriggerEventExcessiveReports,
  kTriggerEventStorageLimit,
  kTriggerEventReportWindowPassed,
  kTriggerAggregateDeduplicated,
  kTriggerAggregateNoContributions,
  kTriggerAggregateInsufficientBudget,
  kTriggerAggregateStorageLimit,
  kTriggerAggregateReportWindowPassed,
  kTriggerAggregateExcessiveReports,
  kTriggerUnknownError,
};

absl::optional<DebugDataType> DataTypeIfCookieSet(DebugDataType data_type,
                                                  bool is_debug_cookie_set) {
  return is_debug_cookie_set ? absl::make_optional(data_type) : absl::nullopt;
}

absl::optional<DebugDataType> GetReportDataType(StorableSource::Result result,
                                                bool is_debug_cookie_set) {
  switch (result) {
    case StorableSource::Result::kProhibitedByBrowserPolicy:
      return absl::nullopt;
    case StorableSource::Result::kSuccess:
    // `kSourceSuccess` is sent for unattributed reporting origin limit and max
    // unique destinations per source site to mitigate the security concerns on
    // reporting these errors. Because `kDestinationGlobalLimitReached` and
    // `kExcessiveReportingOrigins` are thrown based on information across
    // reporting origins, reporting on them would violate the same-origin
    // policy.
    case StorableSource::Result::kExcessiveReportingOrigins:
    case StorableSource::Result::kDestinationGlobalLimitReached:
      return DataTypeIfCookieSet(DebugDataType::kSourceSuccess,
                                 is_debug_cookie_set);
    case StorableSource::Result::kInsufficientUniqueDestinationCapacity:
      return DebugDataType::kSourceDestinationLimit;
    case StorableSource::Result::kDestinationReportingLimitReached:
    case StorableSource::Result::kDestinationBothLimitsReached:
      return DebugDataType::kSourceDestinationRateLimit;
    case StorableSource::Result::kSuccessNoised:
      return DataTypeIfCookieSet(DebugDataType::kSourceNoised,
                                 is_debug_cookie_set);
    case StorableSource::Result::kInsufficientSourceCapacity:
      return DataTypeIfCookieSet(DebugDataType::kSourceStorageLimit,
                                 is_debug_cookie_set);
    case StorableSource::Result::kInternalError:
      return DataTypeIfCookieSet(DebugDataType::kSourceUnknownError,
                                 is_debug_cookie_set);
  }
}

absl::optional<DebugDataType> GetReportDataType(EventLevelResult result,
                                                bool is_debug_cookie_set) {
  switch (result) {
    case EventLevelResult::kSuccess:
    case EventLevelResult::kProhibitedByBrowserPolicy:
    case EventLevelResult::kSuccessDroppedLowerPriority:
    case EventLevelResult::kNotRegistered:
      return absl::nullopt;
    case EventLevelResult::kInternalError:
      return DataTypeIfCookieSet(DebugDataType::kTriggerUnknownError,
                                 is_debug_cookie_set);
    case EventLevelResult::kNoCapacityForConversionDestination:
      return DataTypeIfCookieSet(DebugDataType::kTriggerEventStorageLimit,
                                 is_debug_cookie_set);
    case EventLevelResult::kExcessiveReportingOrigins:
      return DataTypeIfCookieSet(DebugDataType::kTriggerReportingOriginLimit,
                                 is_debug_cookie_set);
    case EventLevelResult::kNoMatchingImpressions:
      return DataTypeIfCookieSet(DebugDataType::kTriggerNoMatchingSource,
                                 is_debug_cookie_set);
    case EventLevelResult::kExcessiveAttributions:
      return DataTypeIfCookieSet(
          DebugDataType::kTriggerAttributionsPerSourceDestinationLimit,
          is_debug_cookie_set);
    case EventLevelResult::kNoMatchingSourceFilterData:
      return DataTypeIfCookieSet(DebugDataType::kTriggerNoMatchingFilterData,
                                 is_debug_cookie_set);
    case EventLevelResult::kDeduplicated:
      return DataTypeIfCookieSet(DebugDataType::kTriggerEventDeduplicated,
                                 is_debug_cookie_set);
    case EventLevelResult::kNoMatchingConfigurations:
      return DataTypeIfCookieSet(
          DebugDataType::kTriggerEventNoMatchingConfigurations,
          is_debug_cookie_set);
    case EventLevelResult::kDroppedForNoise:
    case EventLevelResult::kFalselyAttributedSource:
      return DataTypeIfCookieSet(DebugDataType::kTriggerEventNoise,
                                 is_debug_cookie_set);
    case EventLevelResult::kPriorityTooLow:
      return DataTypeIfCookieSet(DebugDataType::kTriggerEventLowPriority,
                                 is_debug_cookie_set);
    case EventLevelResult::kExcessiveReports:
      return DataTypeIfCookieSet(DebugDataType::kTriggerEventExcessiveReports,
                                 is_debug_cookie_set);
    case EventLevelResult::kReportWindowPassed:
      return DataTypeIfCookieSet(DebugDataType::kTriggerEventReportWindowPassed,
                                 is_debug_cookie_set);
  }
}

absl::optional<DebugDataType> GetReportDataType(AggregatableResult result,
                                                bool is_debug_cookie_set) {
  switch (result) {
    case AggregatableResult::kSuccess:
    case AggregatableResult::kNotRegistered:
    case AggregatableResult::kProhibitedByBrowserPolicy:
      return absl::nullopt;
    case AggregatableResult::kInternalError:
      return DataTypeIfCookieSet(DebugDataType::kTriggerUnknownError,
                                 is_debug_cookie_set);
    case AggregatableResult::kNoCapacityForConversionDestination:
      return DataTypeIfCookieSet(DebugDataType::kTriggerAggregateStorageLimit,
                                 is_debug_cookie_set);
    case AggregatableResult::kExcessiveReportingOrigins:
      return DataTypeIfCookieSet(DebugDataType::kTriggerReportingOriginLimit,
                                 is_debug_cookie_set);
    case AggregatableResult::kNoMatchingImpressions:
      return DataTypeIfCookieSet(DebugDataType::kTriggerNoMatchingSource,
                                 is_debug_cookie_set);
    case AggregatableResult::kExcessiveAttributions:
      return DataTypeIfCookieSet(
          DebugDataType::kTriggerAttributionsPerSourceDestinationLimit,
          is_debug_cookie_set);
    case AggregatableResult::kNoMatchingSourceFilterData:
      return DataTypeIfCookieSet(DebugDataType::kTriggerNoMatchingFilterData,
                                 is_debug_cookie_set);
    case AggregatableResult::kDeduplicated:
      return DataTypeIfCookieSet(DebugDataType::kTriggerAggregateDeduplicated,
                                 is_debug_cookie_set);
    case AggregatableResult::kNoHistograms:
      return DataTypeIfCookieSet(
          DebugDataType::kTriggerAggregateNoContributions, is_debug_cookie_set);
    case AggregatableResult::kInsufficientBudget:
      return DataTypeIfCookieSet(
          DebugDataType::kTriggerAggregateInsufficientBudget,
          is_debug_cookie_set);
    case AggregatableResult::kReportWindowPassed:
      return DataTypeIfCookieSet(
          DebugDataType::kTriggerAggregateReportWindowPassed,
          is_debug_cookie_set);
    case AggregatableResult::kExcessiveReports:
      return DataTypeIfCookieSet(
          DebugDataType::kTriggerAggregateExcessiveReports,
          is_debug_cookie_set);
  }
}

std::string SerializeReportDataType(DebugDataType data_type) {
  switch (data_type) {
    case DebugDataType::kSourceDestinationLimit:
      return "source-destination-limit";
    case DebugDataType::kSourceNoised:
      return "source-noised";
    case DebugDataType::kSourceStorageLimit:
      return "source-storage-limit";
    case DebugDataType::kSourceSuccess:
      return "source-success";
    case DebugDataType::kSourceDestinationRateLimit:
      return "source-destination-rate-limit";
    case DebugDataType::kSourceUnknownError:
      return "source-unknown-error";
    case DebugDataType::kTriggerNoMatchingSource:
      return "trigger-no-matching-source";
    case DebugDataType::kTriggerAttributionsPerSourceDestinationLimit:
      return "trigger-attributions-per-source-destination-limit";
    case DebugDataType::kTriggerNoMatchingFilterData:
      return "trigger-no-matching-filter-data";
    case DebugDataType::kTriggerReportingOriginLimit:
      return "trigger-reporting-origin-limit";
    case DebugDataType::kTriggerEventDeduplicated:
      return "trigger-event-deduplicated";
    case DebugDataType::kTriggerEventNoMatchingConfigurations:
      return "trigger-event-no-matching-configurations";
    case DebugDataType::kTriggerEventNoise:
      return "trigger-event-noise";
    case DebugDataType::kTriggerEventLowPriority:
      return "trigger-event-low-priority";
    case DebugDataType::kTriggerEventExcessiveReports:
      return "trigger-event-excessive-reports";
    case DebugDataType::kTriggerEventStorageLimit:
      return "trigger-event-storage-limit";
    case DebugDataType::kTriggerEventReportWindowPassed:
      return "trigger-event-report-window-passed";
    case DebugDataType::kTriggerAggregateDeduplicated:
      return "trigger-aggregate-deduplicated";
    case DebugDataType::kTriggerAggregateNoContributions:
      return "trigger-aggregate-no-contributions";
    case DebugDataType::kTriggerAggregateInsufficientBudget:
      return "trigger-aggregate-insufficient-budget";
    case DebugDataType::kTriggerAggregateStorageLimit:
      return "trigger-aggregate-storage-limit";
    case DebugDataType::kTriggerAggregateReportWindowPassed:
      return "trigger-aggregate-report-window-passed";
    case DebugDataType::kTriggerAggregateExcessiveReports:
      return "trigger-aggregate-excessive-reports";
    case DebugDataType::kTriggerUnknownError:
      return "trigger-unknown-error";
  }
}

void SetSourceData(base::Value::Dict& data_body,
                   uint64_t source_event_id,
                   const net::SchemefulSite& source_site,
                   absl::optional<uint64_t> source_debug_key) {
  data_body.Set("source_event_id", base::NumberToString(source_event_id));
  data_body.Set("source_site", source_site.Serialize());
  if (source_debug_key) {
    data_body.Set("source_debug_key", base::NumberToString(*source_debug_key));
  }
}

template <typename T>
void SetLimit(base::Value::Dict& data_body, absl::optional<T> limit) {
  DCHECK(limit.has_value());
  data_body.Set("limit", base::NumberToString(*limit));
}

base::Value::Dict GetReportDataBody(DebugDataType data_type,
                                    const StorableSource& source,
                                    const StoreSourceResult& result) {
  DCHECK(!source.is_within_fenced_frame());

  const attribution_reporting::SourceRegistration& registration =
      source.registration();

  base::Value::Dict data_body;
  data_body.Set(kAttributionDestination, registration.destination_set.ToJson());
  SetSourceData(data_body, registration.source_event_id,
                source.common_info().source_site(), registration.debug_key);

  switch (data_type) {
    case DebugDataType::kSourceDestinationLimit:
      SetLimit(data_body,
               result.max_destinations_per_source_site_reporting_site);
      break;
    case DebugDataType::kSourceStorageLimit:
      SetLimit(data_body, result.max_sources_per_origin);
      break;
    case DebugDataType::kSourceDestinationRateLimit:
      SetLimit(data_body,
               result.max_destinations_per_rate_limit_window_reporting_origin);
      break;
    case DebugDataType::kSourceNoised:
    case DebugDataType::kSourceSuccess:
    case DebugDataType::kSourceUnknownError:
      break;
    case DebugDataType::kTriggerNoMatchingSource:
    case DebugDataType::kTriggerAttributionsPerSourceDestinationLimit:
    case DebugDataType::kTriggerNoMatchingFilterData:
    case DebugDataType::kTriggerReportingOriginLimit:
    case DebugDataType::kTriggerEventDeduplicated:
    case DebugDataType::kTriggerEventNoMatchingConfigurations:
    case DebugDataType::kTriggerEventNoise:
    case DebugDataType::kTriggerEventLowPriority:
    case DebugDataType::kTriggerEventExcessiveReports:
    case DebugDataType::kTriggerEventStorageLimit:
    case DebugDataType::kTriggerEventReportWindowPassed:
    case DebugDataType::kTriggerAggregateDeduplicated:
    case DebugDataType::kTriggerAggregateNoContributions:
    case DebugDataType::kTriggerAggregateInsufficientBudget:
    case DebugDataType::kTriggerAggregateStorageLimit:
    case DebugDataType::kTriggerAggregateReportWindowPassed:
    case DebugDataType::kTriggerAggregateExcessiveReports:
    case DebugDataType::kTriggerUnknownError:
      NOTREACHED();
      return base::Value::Dict();
  }

  return data_body;
}

// `original_report_time` must be non-null when `data_type`'s body will contain
// a `scheduled_report_time` field, which is only true for certain event-level
// failures that use the entire body of the report that would have been stored
// if attribution had succeeded.
base::Value::Dict GetReportDataBody(DebugDataType data_type,
                                    const AttributionTrigger& trigger,
                                    const CreateReportResult& result,
                                    base::Time* original_report_time) {
  base::Value::Dict data_body;
  data_body.Set(kAttributionDestination,
                net::SchemefulSite(trigger.destination_origin()).Serialize());
  if (absl::optional<uint64_t> debug_key = trigger.registration().debug_key) {
    data_body.Set("trigger_debug_key", base::NumberToString(*debug_key));
  }

  if (const absl::optional<StoredSource>& source = result.source()) {
    SetSourceData(data_body, source->source_event_id(),
                  source->common_info().source_site(), source->debug_key());
  }

  switch (data_type) {
    case DebugDataType::kTriggerNoMatchingSource:
    case DebugDataType::kTriggerNoMatchingFilterData:
    case DebugDataType::kTriggerEventDeduplicated:
    case DebugDataType::kTriggerEventNoMatchingConfigurations:
    case DebugDataType::kTriggerEventNoise:
    case DebugDataType::kTriggerEventReportWindowPassed:
    case DebugDataType::kTriggerAggregateDeduplicated:
    case DebugDataType::kTriggerAggregateNoContributions:
    case DebugDataType::kTriggerAggregateReportWindowPassed:
    case DebugDataType::kTriggerUnknownError:
      break;
    case DebugDataType::kTriggerAttributionsPerSourceDestinationLimit:
      SetLimit(data_body, result.limits().rate_limits_max_attributions);
      break;
    case DebugDataType::kTriggerAggregateInsufficientBudget:
      SetLimit(data_body, result.limits().aggregatable_budget_per_source);
      break;
    case DebugDataType::kTriggerAggregateExcessiveReports:
      SetLimit(data_body, result.limits().max_aggregatable_reports_per_source);
      break;
    case DebugDataType::kTriggerReportingOriginLimit:
      SetLimit(data_body,
               result.limits().rate_limits_max_attribution_reporting_origins);
      break;
    case DebugDataType::kTriggerEventStorageLimit:
      SetLimit(data_body,
               result.limits().max_event_level_reports_per_destination);
      break;
    case DebugDataType::kTriggerAggregateStorageLimit:
      SetLimit(data_body,
               result.limits().max_aggregatable_reports_per_destination);
      break;
    case DebugDataType::kTriggerEventLowPriority:
    case DebugDataType::kTriggerEventExcessiveReports:
      DCHECK(result.dropped_event_level_report());
      DCHECK(original_report_time);
      *original_report_time =
          result.dropped_event_level_report()->initial_report_time();
      return result.dropped_event_level_report()->ReportBody();
    case DebugDataType::kSourceDestinationLimit:
    case DebugDataType::kSourceNoised:
    case DebugDataType::kSourceStorageLimit:
    case DebugDataType::kSourceSuccess:
    case DebugDataType::kSourceUnknownError:
    case DebugDataType::kSourceDestinationRateLimit:
      NOTREACHED();
      return base::Value::Dict();
  }

  return data_body;
}

base::Value::Dict GetReportData(DebugDataType type, base::Value::Dict body) {
  base::Value::Dict dict;
  dict.Set("type", SerializeReportDataType(type));
  dict.Set("body", std::move(body));
  return dict;
}

GURL ReportURL(const attribution_reporting::SuitableOrigin& reporting_origin) {
  static constexpr char kPath[] =
      "/.well-known/attribution-reporting/debug/verbose";

  GURL::Replacements replacements;
  replacements.SetPathStr(kPath);
  return reporting_origin->GetURL().ReplaceComponents(replacements);
}

}  // namespace

// static
absl::optional<AttributionDebugReport> AttributionDebugReport::Create(
    const StorableSource& source,
    bool is_debug_cookie_set,
    const StoreSourceResult& result) {
  if (!source.registration().debug_reporting ||
      source.is_within_fenced_frame()) {
    return absl::nullopt;
  }

  absl::optional<DebugDataType> data_type =
      GetReportDataType(result.status, is_debug_cookie_set);
  if (!data_type) {
    return absl::nullopt;
  }

  base::Value::List report_body;
  report_body.Append(
      GetReportData(*data_type, GetReportDataBody(*data_type, source, result)));
  return AttributionDebugReport(std::move(report_body),
                                source.common_info().reporting_origin(),
                                /*original_report_time=*/base::Time());
}

// static
absl::optional<AttributionDebugReport> AttributionDebugReport::Create(
    const AttributionTrigger& trigger,
    bool is_debug_cookie_set,
    const CreateReportResult& result) {
  if (!trigger.registration().debug_reporting ||
      trigger.is_within_fenced_frame()) {
    return absl::nullopt;
  }

  base::Value::List report_body;
  base::Time original_report_time;

  absl::optional<DebugDataType> event_level_data_type =
      GetReportDataType(result.event_level_status(), is_debug_cookie_set);
  if (event_level_data_type) {
    report_body.Append(
        GetReportData(*event_level_data_type,
                      GetReportDataBody(*event_level_data_type, trigger, result,
                                        &original_report_time)));
  }

  if (absl::optional<DebugDataType> aggregatable_data_type =
          GetReportDataType(result.aggregatable_status(), is_debug_cookie_set);
      aggregatable_data_type &&
      aggregatable_data_type != event_level_data_type) {
    report_body.Append(GetReportData(
        *aggregatable_data_type,
        GetReportDataBody(*aggregatable_data_type, trigger, result,
                          /*original_report_time=*/nullptr)));
  }

  if (report_body.empty()) {
    return absl::nullopt;
  }

  return AttributionDebugReport(
      std::move(report_body), trigger.reporting_origin(), original_report_time);
}

AttributionDebugReport::AttributionDebugReport(
    base::Value::List report_body,
    const attribution_reporting::SuitableOrigin& reporting_origin,
    base::Time original_report_time)
    : report_body_(std::move(report_body)),
      report_url_(ReportURL(reporting_origin)),
      original_report_time_(original_report_time) {
  DCHECK(!report_body_.empty());
}

AttributionDebugReport::~AttributionDebugReport() = default;

AttributionDebugReport::AttributionDebugReport(AttributionDebugReport&&) =
    default;

AttributionDebugReport& AttributionDebugReport::operator=(
    AttributionDebugReport&&) = default;

}  // namespace content
