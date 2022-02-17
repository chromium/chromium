// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_report.h"

#include <utility>

#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "content/browser/attribution_reporting/attribution_utils.h"
#include "content/browser/attribution_reporting/common_source_info.h"
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

AttributionReport::AggregatableContributionData::AggregatableContributionData(
    HistogramContribution contribution,
    absl::optional<Id> id)
    : contribution(std::move(contribution)), id(id) {}

AttributionReport::AggregatableContributionData::AggregatableContributionData(
    const AggregatableContributionData& other) = default;

AttributionReport::AggregatableContributionData&
AttributionReport::AggregatableContributionData::operator=(
    const AggregatableContributionData& other) = default;

AttributionReport::AggregatableContributionData::AggregatableContributionData(
    AggregatableContributionData&& other) = default;

AttributionReport::AggregatableContributionData&
AttributionReport::AggregatableContributionData::operator=(
    AggregatableContributionData&& other) = default;

AttributionReport::AggregatableContributionData::
    ~AggregatableContributionData() = default;

AttributionReport::AttributionReport(
    AttributionInfo attribution_info,
    base::Time report_time,
    base::GUID external_report_id,
    absl::variant<EventLevelData, AggregatableContributionData> data)
    : attribution_info_(std::move(attribution_info)),
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

    const char* operator()(const AggregatableContributionData&) {
      static constexpr char kAggregateEndpointPath[] =
          "/.well-known/attribution-reporting/report-aggregate-attribution";
      return kAggregateEndpointPath;
    }
  };

  const char* path = absl::visit(Visitor{}, data_);
  url::Replacements<char> replacements;
  replacements.SetPath(path, url::Component(0, strlen(path)));
  return attribution_info_.source.common_info()
      .reporting_origin()
      .GetURL()
      .ReplaceComponents(replacements);
}

base::Value AttributionReport::ReportBody() const {
  const auto* event_data = absl::get_if<EventLevelData>(&data_);
  DCHECK(event_data);

  base::Value dict(base::Value::Type::DICTIONARY);

  const CommonSourceInfo& common_source_info =
      attribution_info_.source.common_info();

  dict.SetStringKey("attribution_destination",
                    common_source_info.ConversionDestination().Serialize());

  // The API denotes these values as strings; a `uint64_t` cannot be put in
  // a dict as an integer in order to be opaque to various API configurations.
  dict.SetStringKey("source_event_id",
                    base::NumberToString(common_source_info.source_event_id()));

  dict.SetStringKey("trigger_data",
                    base::NumberToString(event_data->trigger_data));

  const char* source_type = nullptr;
  switch (common_source_info.source_type()) {
    case CommonSourceInfo::SourceType::kNavigation:
      source_type = "navigation";
      break;
    case CommonSourceInfo::SourceType::kEvent:
      source_type = "event";
      break;
  }
  dict.SetStringKey("source_type", source_type);

  dict.SetStringKey("report_id", external_report_id_.AsLowercaseString());

  // TODO(apaseltiner): When the values returned by
  // `RandomizedTriggerRate()` are changed for the first time, we must
  // remove the call to that function here and instead associate each newly
  // stored source and report with the current configuration. One way to do that
  // is to permanently store the configuration history in the binary with each
  // version having a unique ID, and storing that ID in a new column in the
  // impressions and conversions DB tables. This code would then look up the
  // values for the particular IDs. Because such an approach would entail
  // complicating the DB schema, we hardcode the values for now and will wait
  // for the first time the values are changed before complicating the codebase.
  dict.SetDoubleKey("randomized_trigger_rate",
                    RandomizedTriggerRate(common_source_info.source_type()));

  if (absl::optional<uint64_t> debug_key = common_source_info.debug_key())
    dict.SetStringKey("source_debug_key", base::NumberToString(*debug_key));

  if (absl::optional<uint64_t> debug_key = attribution_info_.debug_key)
    dict.SetStringKey("trigger_debug_key", base::NumberToString(*debug_key));

  return dict;
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
