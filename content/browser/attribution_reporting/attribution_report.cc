// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_report.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/functional/overloaded.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_canon.h"

namespace content {

namespace {

base::Value SerializeDestinations(
    const base::flat_set<attribution_reporting::SuitableOrigin>& destinations) {
  DCHECK(!destinations.empty());

  base::flat_set<net::SchemefulSite> sites;
  for (const auto& destination : destinations) {
    sites.insert(net::SchemefulSite(destination));
  }

  if (sites.size() == 1)
    return base::Value(sites.begin()->Serialize());

  base::Value::List list;
  list.reserve(sites.size());

  for (const auto& site : sites) {
    list.Append(site.Serialize());
  }

  return base::Value(std::move(list));
}

}  // namespace

AttributionReport::EventLevelData::EventLevelData(
    uint64_t trigger_data,
    int64_t priority,
    double randomized_trigger_rate,
    Id id)
    : trigger_data(trigger_data),
      priority(priority),
      randomized_trigger_rate(randomized_trigger_rate),
      id(id) {
  DCHECK_GE(randomized_trigger_rate, 0);
  DCHECK_LE(randomized_trigger_rate, 1);
}

AttributionReport::EventLevelData::EventLevelData(const EventLevelData&) =
    default;

AttributionReport::EventLevelData& AttributionReport::EventLevelData::operator=(
    const EventLevelData&) = default;

AttributionReport::EventLevelData::EventLevelData(EventLevelData&&) = default;

AttributionReport::EventLevelData& AttributionReport::EventLevelData::operator=(
    EventLevelData&&) = default;

AttributionReport::EventLevelData::~EventLevelData() = default;

AttributionReport::AggregatableAttributionData::AggregatableAttributionData(
    std::vector<AggregatableHistogramContribution> contributions,
    Id id,
    base::Time initial_report_time,
    ::aggregation_service::mojom::AggregationCoordinator
        aggregation_coordinator)
    : contributions(std::move(contributions)),
      id(id),
      initial_report_time(initial_report_time),
      aggregation_coordinator(aggregation_coordinator) {}

AttributionReport::AggregatableAttributionData::AggregatableAttributionData(
    const AggregatableAttributionData&) = default;

AttributionReport::AggregatableAttributionData&
AttributionReport::AggregatableAttributionData::operator=(
    const AggregatableAttributionData&) = default;

AttributionReport::AggregatableAttributionData::AggregatableAttributionData(
    AggregatableAttributionData&&) = default;

AttributionReport::AggregatableAttributionData&
AttributionReport::AggregatableAttributionData::operator=(
    AggregatableAttributionData&&) = default;

AttributionReport::AggregatableAttributionData::~AggregatableAttributionData() =
    default;

base::CheckedNumeric<int64_t>
AttributionReport::AggregatableAttributionData::BudgetRequired() const {
  base::CheckedNumeric<int64_t> budget_required = 0;
  for (const AggregatableHistogramContribution& contribution : contributions) {
    budget_required += contribution.value();
  }
  return budget_required;
}

AttributionReport::AttributionReport(
    AttributionInfo attribution_info,
    base::Time report_time,
    base::GUID external_report_id,
    int failed_send_attempts,
    absl::variant<EventLevelData, AggregatableAttributionData> data)
    : attribution_info_(std::move(attribution_info)),
      report_time_(report_time),
      external_report_id_(std::move(external_report_id)),
      failed_send_attempts_(failed_send_attempts),
      data_(std::move(data)) {
  DCHECK(external_report_id_.is_valid());
  DCHECK_GE(failed_send_attempts_, 0);
}

AttributionReport::AttributionReport(const AttributionReport&) = default;

AttributionReport& AttributionReport::operator=(const AttributionReport&) =
    default;

AttributionReport::AttributionReport(AttributionReport&&) = default;

AttributionReport& AttributionReport::operator=(AttributionReport&&) = default;

AttributionReport::~AttributionReport() = default;

GURL AttributionReport::ReportURL(bool debug) const {
  static constexpr char kBasePath[] = "/.well-known/attribution-reporting/";
  static constexpr char kDebugPath[] = "debug/";

  const char* endpoint_path;
  switch (GetReportType()) {
    case Type::kEventLevel:
      endpoint_path = "report-event-attribution";
      break;
    case Type::kAggregatableAttribution:
      endpoint_path = "report-aggregate-attribution";
      break;
  }

  std::string path =
      base::StrCat({kBasePath, debug ? kDebugPath : "", endpoint_path});

  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  return attribution_info_.source.common_info()
      .reporting_origin()
      ->GetURL()
      .ReplaceComponents(replacements);
}

base::Value::Dict AttributionReport::ReportBody() const {
  return absl::visit(
      base::Overloaded{
          [this](const EventLevelData& data) {
            base::Value::Dict dict;

            const CommonSourceInfo& common_source_info =
                this->attribution_info().source.common_info();

            dict.Set("attribution_destination",
                     SerializeDestinations(
                         common_source_info.destination_origins()));

            // The API denotes these values as strings; a `uint64_t` cannot be
            // put in a dict as an integer in order to be opaque to various API
            // configurations.
            dict.Set(
                "source_event_id",
                base::NumberToString(common_source_info.source_event_id()));

            dict.Set("trigger_data", base::NumberToString(data.trigger_data));

            dict.Set("source_type", AttributionSourceTypeToString(
                                        common_source_info.source_type()));

            dict.Set("report_id",
                     this->external_report_id().AsLowercaseString());

            dict.Set("randomized_trigger_rate", data.randomized_trigger_rate);

            if (absl::optional<uint64_t> debug_key =
                    common_source_info.debug_key())
              dict.Set("source_debug_key", base::NumberToString(*debug_key));

            if (absl::optional<uint64_t> debug_key =
                    this->attribution_info().debug_key) {
              dict.Set("trigger_debug_key", base::NumberToString(*debug_key));
            }

            return dict;
          },

          [this](const AggregatableAttributionData& data) {
            base::Value::Dict dict;

            if (data.assembled_report.has_value()) {
              dict = data.assembled_report->GetAsJson();
            } else {
              // This generally should only be called when displaying the report
              // for debugging/internals.
              dict.Set("shared_info", "not generated prior to send");
              dict.Set("aggregation_service_payloads",
                       "not generated prior to send");
            }

            const CommonSourceInfo& common_info =
                this->attribution_info().source.common_info();

            if (absl::optional<uint64_t> debug_key = common_info.debug_key())
              dict.Set("source_debug_key", base::NumberToString(*debug_key));

            if (absl::optional<uint64_t> debug_key =
                    this->attribution_info().debug_key) {
              dict.Set("trigger_debug_key", base::NumberToString(*debug_key));
            }

            return dict;
          },
      },
      data_);
}

AttributionReport::Id AttributionReport::ReportId() const {
  return absl::visit([](const auto& v) { return Id(v.id); }, data_);
}

void AttributionReport::set_report_time(base::Time report_time) {
  report_time_ = report_time;
}

void AttributionReport::SetExternalReportIdForTesting(
    base::GUID external_report_id) {
  DCHECK(external_report_id.is_valid());
  external_report_id_ = std::move(external_report_id);
}

// static
absl::optional<base::Time> AttributionReport::MinReportTime(
    absl::optional<base::Time> a,
    absl::optional<base::Time> b) {
  if (!a.has_value())
    return b;

  if (!b.has_value())
    return a;

  return std::min(*a, *b);
}

}  // namespace content
