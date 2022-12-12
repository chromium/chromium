// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_debug_report.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "content/browser/attribution_reporting/attribution_observer_types.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content {

namespace {

using DebugDataType = ::content::AttributionDebugReport::DataType;
using EventLevelResult = ::content::AttributionTrigger::EventLevelResult;
using AggregatableResult = ::content::AttributionTrigger::AggregatableResult;

absl::optional<DebugDataType> DataTypeIfCookieSet(DebugDataType data_type,
                                                  bool is_debug_cookie_set) {
  return is_debug_cookie_set ? absl::make_optional(data_type) : absl::nullopt;
}

absl::optional<DebugDataType> GetReportDataType(StorableSource::Result result,
                                                bool is_debug_cookie_set) {
  switch (result) {
    case StorableSource::Result::kSuccess:
    case StorableSource::Result::kExcessiveReportingOrigins:
    case StorableSource::Result::kProhibitedByBrowserPolicy:
      return absl::nullopt;
    case StorableSource::Result::kInsufficientUniqueDestinationCapacity:
      return DebugDataType::kSourceDestinationLimit;
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
    case DebugDataType::kTriggerUnknownError:
      return "trigger-unknown-error";
  }
}

void SetSourceData(base::Value::Dict& data_body,
                   const CommonSourceInfo& common_info) {
  data_body.Set("source_event_id",
                base::NumberToString(common_info.source_event_id()));
  data_body.Set("source_site", common_info.SourceSite().Serialize());
  if (common_info.debug_key()) {
    data_body.Set("source_debug_key",
                  base::NumberToString(*common_info.debug_key()));
  }
}

void SetAttributionDestination(base::Value::Dict& data_body,
                               const net::SchemefulSite& destination) {
  data_body.Set("attribution_destination", destination.Serialize());
}

template <typename T>
void SetLimit(base::Value::Dict& data_body, absl::optional<T> limit) {
  DCHECK(limit.has_value());
  data_body.Set("limit", base::NumberToString(*limit));
}

base::Value::Dict GetReportDataBody(
    DebugDataType data_type,
    const StorableSource& source,
    const AttributionStorage::StoreSourceResult& result) {
  DCHECK(!source.is_within_fenced_frame());

  const CommonSourceInfo& common_info = source.common_info();
  base::Value::Dict data_body;
  SetAttributionDestination(data_body, common_info.DestinationSite());
  SetSourceData(data_body, common_info);

  switch (data_type) {
    case DebugDataType::kSourceDestinationLimit:
      SetLimit(data_body,
               result.max_destinations_per_source_site_reporting_origin);
      break;
    case DebugDataType::kSourceStorageLimit:
      SetLimit(data_body, result.max_sources_per_origin);
      break;
    case DebugDataType::kSourceNoised:
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
    case DebugDataType::kTriggerUnknownError:
      NOTREACHED();
      return base::Value::Dict();
  }

  return data_body;
}

base::Value::Dict GetReportDataBody(DebugDataType data_type,
                                    const AttributionTrigger& trigger,
                                    const CreateReportResult& result) {
  base::Value::Dict data_body;
  SetAttributionDestination(data_body,
                            net::SchemefulSite(trigger.destination_origin()));
  if (absl::optional<uint64_t> debug_key = trigger.registration().debug_key)
    data_body.Set("trigger_debug_key", base::NumberToString(*debug_key));

  if (result.source())
    SetSourceData(data_body, result.source()->common_info());

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
      return result.dropped_event_level_report()->ReportBody();
    case DebugDataType::kSourceDestinationLimit:
    case DebugDataType::kSourceNoised:
    case DebugDataType::kSourceStorageLimit:
    case DebugDataType::kSourceUnknownError:
      NOTREACHED();
      return base::Value::Dict();
  }

  return data_body;
}

}  // namespace

class AttributionDebugReport::ReportData {
 public:
  ReportData(DataType type, base::Value::Dict body);
  ~ReportData();

  ReportData(const ReportData&) = delete;
  ReportData& operator=(const ReportData&) = delete;

  ReportData(ReportData&&);
  ReportData& operator=(ReportData&&);

  base::Value::Dict SerializeAsJson() const;

 private:
  DataType type_;
  base::Value::Dict body_;
};

AttributionDebugReport::ReportData::ReportData(DataType type,
                                               base::Value::Dict body)
    : type_(type), body_(std::move(body)) {}

AttributionDebugReport::ReportData::~ReportData() = default;

AttributionDebugReport::ReportData::ReportData(ReportData&&) = default;

AttributionDebugReport::ReportData&
AttributionDebugReport::ReportData::operator=(ReportData&&) = default;

base::Value::Dict AttributionDebugReport::ReportData::SerializeAsJson() const {
  base::Value::Dict dict;
  dict.Set("type", SerializeReportDataType(type_));
  dict.Set("body", body_.Clone());
  return dict;
}

// static
absl::optional<AttributionDebugReport> AttributionDebugReport::Create(
    const StorableSource& source,
    bool is_debug_cookie_set,
    const AttributionStorage::StoreSourceResult& result) {
  if (!source.debug_reporting() || source.is_within_fenced_frame())
    return absl::nullopt;

  absl::optional<DataType> data_type =
      GetReportDataType(result.status, is_debug_cookie_set);
  if (!data_type)
    return absl::nullopt;

  std::vector<ReportData> report_data;
  report_data.emplace_back(*data_type,
                           GetReportDataBody(*data_type, source, result));
  return AttributionDebugReport(std::move(report_data),
                                source.common_info().reporting_origin());
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

  std::vector<ReportData> report_data;

  absl::optional<DataType> event_level_data_type =
      GetReportDataType(result.event_level_status(), is_debug_cookie_set);
  if (event_level_data_type) {
    report_data.emplace_back(
        *event_level_data_type,
        GetReportDataBody(*event_level_data_type, trigger, result));
  }

  if (absl::optional<DataType> aggregatable_data_type =
          GetReportDataType(result.aggregatable_status(), is_debug_cookie_set);
      aggregatable_data_type &&
      aggregatable_data_type != event_level_data_type) {
    report_data.emplace_back(
        *aggregatable_data_type,
        GetReportDataBody(*aggregatable_data_type, trigger, result));
  }

  if (report_data.empty())
    return absl::nullopt;

  return AttributionDebugReport(std::move(report_data),
                                trigger.reporting_origin());
}

AttributionDebugReport::AttributionDebugReport(
    std::vector<ReportData> report_data,
    attribution_reporting::SuitableOrigin reporting_origin)
    : report_data_(std::move(report_data)),
      reporting_origin_(std::move(reporting_origin)) {
  DCHECK(!report_data_.empty());
}

AttributionDebugReport::~AttributionDebugReport() = default;

AttributionDebugReport::AttributionDebugReport(AttributionDebugReport&&) =
    default;

AttributionDebugReport& AttributionDebugReport::operator=(
    AttributionDebugReport&&) = default;

base::Value::List AttributionDebugReport::ReportBody() const {
  base::Value::List report_body;
  for (const ReportData& data : report_data_) {
    report_body.Append(data.SerializeAsJson());
  }
  return report_body;
}

GURL AttributionDebugReport::ReportURL() const {
  static constexpr char kPath[] =
      "/.well-known/attribution-reporting/debug/verbose";

  GURL::Replacements replacements;
  replacements.SetPathStr(kPath);
  return reporting_origin_->GetURL().ReplaceComponents(replacements);
}

}  // namespace content
