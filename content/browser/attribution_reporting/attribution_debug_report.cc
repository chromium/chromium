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
#include "content/browser/attribution_reporting/attribution_observer_types.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content {

namespace {

using DebugDataType = ::content::AttributionDebugReport::DataType;
using EventLevelResult = ::content::AttributionTrigger::EventLevelResult;
using AggregatableResult = ::content::AttributionTrigger::AggregatableResult;

constexpr char kAttributionDestination[] = "attribution_destination";

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
    case EventLevelResult::kSuccessDroppedLowerPriority:
    case EventLevelResult::kInternalError:
    case EventLevelResult::kNoCapacityForConversionDestination:
    case EventLevelResult::kDeduplicated:
    case EventLevelResult::kExcessiveAttributions:
    case EventLevelResult::kPriorityTooLow:
    case EventLevelResult::kDroppedForNoise:
    case EventLevelResult::kExcessiveReportingOrigins:
    case EventLevelResult::kNoMatchingSourceFilterData:
    case EventLevelResult::kProhibitedByBrowserPolicy:
    case EventLevelResult::kNoMatchingConfigurations:
    case EventLevelResult::kExcessiveReports:
      return absl::nullopt;
    case EventLevelResult::kNoMatchingImpressions:
      return DataTypeIfCookieSet(DebugDataType::kTriggerNoMatchingSource,
                                 is_debug_cookie_set);
  }
}

absl::optional<DebugDataType> GetReportDataType(AggregatableResult result,
                                                bool is_debug_cookie_set) {
  switch (result) {
    case AggregatableResult::kSuccess:
    case AggregatableResult::kInternalError:
    case AggregatableResult::kNoCapacityForConversionDestination:
    case AggregatableResult::kExcessiveAttributions:
    case AggregatableResult::kExcessiveReportingOrigins:
    case AggregatableResult::kNoHistograms:
    case AggregatableResult::kInsufficientBudget:
    case AggregatableResult::kNoMatchingSourceFilterData:
    case AggregatableResult::kNotRegistered:
    case AggregatableResult::kProhibitedByBrowserPolicy:
    case AggregatableResult::kDeduplicated:
      return absl::nullopt;
    case AggregatableResult::kNoMatchingImpressions:
      return DataTypeIfCookieSet(DebugDataType::kTriggerNoMatchingSource,
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
  }
}

base::Value::Dict GetReportDataBody(
    DebugDataType data_type,
    const StorableSource& source,
    const AttributionStorage::StoreSourceResult& result) {
  DCHECK(!source.is_within_fenced_frame());

  const CommonSourceInfo& common_info = source.common_info();
  base::Value::Dict data_body;
  data_body.Set(kAttributionDestination,
                common_info.DestinationSite().Serialize());
  data_body.Set("source_event_id",
                base::NumberToString(common_info.source_event_id()));
  data_body.Set("source_site", common_info.SourceSite().Serialize());

  static constexpr char kLimit[] = "limit";

  switch (data_type) {
    case DebugDataType::kSourceDestinationLimit:
      DCHECK(result.max_destinations_per_source_site_reporting_origin);
      data_body.Set(kLimit,
                    *result.max_destinations_per_source_site_reporting_origin);
      break;
    case DebugDataType::kSourceStorageLimit:
      DCHECK(result.max_sources_per_origin);
      data_body.Set(kLimit, *result.max_sources_per_origin);
      break;
    case DebugDataType::kSourceNoised:
    case DebugDataType::kSourceUnknownError:
      break;
    case DebugDataType::kTriggerNoMatchingSource:
      NOTREACHED();
      return base::Value::Dict();
  }

  return data_body;
}

base::Value::Dict GetReportDataBody(DebugDataType data_type,
                                    const AttributionTrigger& trigger) {
  switch (data_type) {
    case DebugDataType::kTriggerNoMatchingSource: {
      base::Value::Dict data_body;
      data_body.Set(
          kAttributionDestination,
          net::SchemefulSite(trigger.destination_origin()).Serialize());
      return data_body;
    }
    case DebugDataType::kSourceDestinationLimit:
    case DebugDataType::kSourceNoised:
    case DebugDataType::kSourceStorageLimit:
    case DebugDataType::kSourceUnknownError:
      NOTREACHED();
      return base::Value::Dict();
  }
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
  if (!trigger.debug_reporting() || trigger.is_within_fenced_frame())
    return absl::nullopt;

  std::vector<ReportData> report_data;

  absl::optional<DataType> event_level_data_type =
      GetReportDataType(result.event_level_status(), is_debug_cookie_set);
  if (event_level_data_type) {
    report_data.emplace_back(
        *event_level_data_type,
        GetReportDataBody(*event_level_data_type, trigger));
  }

  if (absl::optional<DataType> aggregatable_data_type =
          GetReportDataType(result.aggregatable_status(), is_debug_cookie_set);
      aggregatable_data_type &&
      aggregatable_data_type != event_level_data_type) {
    report_data.emplace_back(
        *aggregatable_data_type,
        GetReportDataBody(*aggregatable_data_type, trigger));
  }

  if (report_data.empty())
    return absl::nullopt;

  return AttributionDebugReport(std::move(report_data),
                                trigger.reporting_origin());
}

AttributionDebugReport::AttributionDebugReport(
    std::vector<ReportData> report_data,
    url::Origin reporting_origin)
    : report_data_(std::move(report_data)),
      reporting_origin_(std::move(reporting_origin)) {
  DCHECK(!report_data_.empty());
  DCHECK(network::IsOriginPotentiallyTrustworthy(reporting_origin_));
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
  return reporting_origin_.GetURL().ReplaceComponents(replacements);
}

}  // namespace content
