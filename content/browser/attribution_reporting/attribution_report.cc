// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_report.h"

#include <utility>

#include "base/check.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"
#include "url/url_canon.h"

namespace content {

AttributionReport::EventLevelData::EventLevelData(uint64_t trigger_data,
                                                  int64_t priority,
                                                  absl::optional<Id> id)
    : trigger_data(trigger_data), priority(priority), id(id) {}

AttributionReport::EventLevelData::EventLevelData(const EventLevelData& other) =
    default;

AttributionReport::EventLevelData& AttributionReport::EventLevelData::operator=(
    const EventLevelData& other) = default;

AttributionReport::EventLevelData::EventLevelData(EventLevelData&& other) =
    default;

AttributionReport::EventLevelData& AttributionReport::EventLevelData::operator=(
    EventLevelData&& other) = default;

AttributionReport::EventLevelData::~EventLevelData() = default;

AttributionReport::AggregateContributionData::AggregateContributionData(
    HistogramContribution contribution,
    absl::optional<Id> id)
    : contribution(std::move(contribution)), id(id) {}

AttributionReport::AggregateContributionData::AggregateContributionData(
    const AggregateContributionData& other) = default;

AttributionReport::AggregateContributionData&
AttributionReport::AggregateContributionData::operator=(
    const AggregateContributionData& other) = default;

AttributionReport::AggregateContributionData::AggregateContributionData(
    AggregateContributionData&& other) = default;

AttributionReport::AggregateContributionData&
AttributionReport::AggregateContributionData::operator=(
    AggregateContributionData&& other) = default;

AttributionReport::AggregateContributionData::~AggregateContributionData() =
    default;

AttributionReport::AttributionReport(
    StorableSource source,
    base::Time trigger_time,
    base::Time report_time,
    base::GUID external_report_id,
    absl::variant<EventLevelData, AggregateContributionData> data)
    : source_(std::move(source)),
      trigger_time_(trigger_time),
      report_time_(report_time),
      external_report_id_(std::move(external_report_id)),
      data_(std::move(data)) {
  DCHECK(external_report_id_.is_valid());
}

AttributionReport::AttributionReport(const AttributionReport& other) = default;

AttributionReport& AttributionReport::operator=(
    const AttributionReport& other) = default;

AttributionReport::AttributionReport(AttributionReport&& other) = default;

AttributionReport& AttributionReport::operator=(AttributionReport&& other) =
    default;

AttributionReport::~AttributionReport() = default;

GURL AttributionReport::ReportURL() const {
  struct Visitor {
    const char* operator()(const EventLevelData&) {
      static constexpr char kEventEndpointPath[] =
          "/.well-known/attribution-reporting/report-attribution";
      return kEventEndpointPath;
    }

    const char* operator()(const AggregateContributionData&) {
      static constexpr char kAggregateEndpointPath[] =
          "/.well-known/attribution-reporting/report-aggregate-attribution";
      return kAggregateEndpointPath;
    }
  };

  const char* path = absl::visit(Visitor{}, data_);
  url::Replacements<char> replacements;
  replacements.SetPath(path, url::Component(0, strlen(path)));
  return source_.reporting_origin().GetURL().ReplaceComponents(replacements);
}

std::string AttributionReport::ReportBody(bool pretty_print) const {
  const auto* event_data = absl::get_if<EventLevelData>(&data_);
  DCHECK(event_data);

  base::Value dict(base::Value::Type::DICTIONARY);

  dict.SetStringKey("attribution_destination",
                    source_.ConversionDestination().Serialize());

  // The API denotes these values as strings; a `uint64_t` cannot be put in
  // a dict as an integer in order to be opaque to various API configurations.
  dict.SetStringKey("source_event_id",
                    base::NumberToString(source_.source_event_id()));

  dict.SetStringKey("trigger_data",
                    base::NumberToString(event_data->trigger_data));

  const char* source_type = nullptr;
  switch (source_.source_type()) {
    case StorableSource::SourceType::kNavigation:
      source_type = "navigation";
      break;
    case StorableSource::SourceType::kEvent:
      source_type = "event";
      break;
  }
  dict.SetStringKey("source_type", source_type);

  dict.SetStringKey("report_id", external_report_id_.AsLowercaseString());

  // Write the dict to json;
  std::string output_json;
  bool success = base::JSONWriter::WriteWithOptions(
      dict, pretty_print ? base::JSONWriter::OPTIONS_PRETTY_PRINT : 0,
      &output_json);
  DCHECK(success);
  return output_json;
}

absl::optional<AttributionReport::Id> AttributionReport::ReportId() const {
  return absl::visit([](const auto& v) { return absl::optional<Id>(v.id); },
                     data_);
}

void AttributionReport::set_report_time(base::Time report_time) {
  report_time_ = report_time;
}

void AttributionReport::set_failed_send_attempts(int failed_send_attempts) {
  DCHECK_GE(failed_send_attempts, 0);
  failed_send_attempts_ = failed_send_attempts;
}

void AttributionReport::SetExternalReportIdForTesting(
    base::GUID external_report_id) {
  DCHECK(external_report_id.is_valid());
  external_report_id_ = std::move(external_report_id);
}

}  // namespace content
