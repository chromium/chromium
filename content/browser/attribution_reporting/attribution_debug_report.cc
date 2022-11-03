// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_debug_report.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content {

namespace {

absl::optional<AttributionDebugReport::DataType> GetReportDataType(
    StorableSource::Result result,
    bool is_debug_cookie_set) {
  switch (result) {
    case StorableSource::Result::kSuccess:
    case StorableSource::Result::kInternalError:
    case StorableSource::Result::kInsufficientSourceCapacity:
    case StorableSource::Result::kExcessiveReportingOrigins:
    case StorableSource::Result::kProhibitedByBrowserPolicy:
      return absl::nullopt;
    case StorableSource::Result::kInsufficientUniqueDestinationCapacity:
      return AttributionDebugReport::DataType::kSourceDestinationLimit;
    case StorableSource::Result::kSuccessNoised:
      return is_debug_cookie_set
                 ? absl::make_optional(
                       AttributionDebugReport::DataType::kSourceNoised)
                 : absl::nullopt;
  }
}

std::string SerializeReportDataType(
    AttributionDebugReport::DataType data_type) {
  switch (data_type) {
    case AttributionDebugReport::DataType::kSourceDestinationLimit:
      return "source-destination-limit";
    case AttributionDebugReport::DataType::kSourceNoised:
      return "source-noised";
  }
}

base::Value::Dict GetReportDataBody(
    AttributionDebugReport::DataType data_type,
    const StorableSource& source,
    absl::optional<int> max_destinations_per_source_site_reporting_origin) {
  DCHECK(!source.is_within_fenced_frame());

  const CommonSourceInfo& common_info = source.common_info();
  base::Value::Dict data_body;
  data_body.Set("attribution_destination",
                common_info.DestinationSite().Serialize());
  data_body.Set("source_event_id",
                base::NumberToString(common_info.source_event_id()));
  data_body.Set("source_site", common_info.SourceSite().Serialize());

  switch (data_type) {
    case AttributionDebugReport::DataType::kSourceDestinationLimit:
      DCHECK(max_destinations_per_source_site_reporting_origin);
      data_body.Set("limit",
                    *max_destinations_per_source_site_reporting_origin);
      break;
    case AttributionDebugReport::DataType::kSourceNoised:
      break;
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
  report_data.emplace_back(
      *data_type,
      GetReportDataBody(
          *data_type, source,
          result.max_destinations_per_source_site_reporting_origin));
  return AttributionDebugReport(std::move(report_data),
                                source.common_info().reporting_origin());
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
