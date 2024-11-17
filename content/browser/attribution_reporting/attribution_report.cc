// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_report.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/overloaded.h"
#include "base/numerics/checked_math.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregatable_trigger_config.h"
#include "components/attribution_reporting/destination_set.h"
#include "components/attribution_reporting/source_type.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/attribution_reporting/aggregatable_attribution_utils.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_canon.h"

namespace content {

namespace {

using ::attribution_reporting::SuitableOrigin;

void PopulateReportBody(base::Value::Dict& dict,
                        const AttributionReport::AggregatableData& data) {
  if (const auto& assembled_report = data.assembled_report();
      assembled_report.has_value()) {
    dict = assembled_report->GetAsJson();
  } else {
    // This generally should only be called when displaying the report
    // for debugging/internals.
    dict.Set("shared_info", "not generated prior to send");
    dict.Set("aggregation_service_payloads", "not generated prior to send");
  }

  if (const auto& trigger_context_id =
          data.aggregatable_trigger_config().trigger_context_id();
      trigger_context_id.has_value()) {
    dict.Set("trigger_context_id", *trigger_context_id);
  }
}

}  // namespace

AttributionReport::EventLevelData::EventLevelData(uint32_t trigger_data,
                                                  int64_t priority,
                                                  const StoredSource& source)
    : trigger_data(trigger_data),
      priority(priority),
      source_origin(source.common_info().source_origin()),
      destinations(source.destination_sites()),
      source_event_id(source.source_event_id()),
      source_type(source.common_info().source_type()),
      randomized_response_rate(source.randomized_response_rate()),
      attributed_truthfully(source.attribution_logic() ==
                            StoredSource::AttributionLogic::kTruthfully) {}

AttributionReport::EventLevelData::EventLevelData(const EventLevelData&) =
    default;

AttributionReport::EventLevelData& AttributionReport::EventLevelData::operator=(
    const EventLevelData&) = default;

AttributionReport::EventLevelData::EventLevelData(EventLevelData&&) = default;

AttributionReport::EventLevelData& AttributionReport::EventLevelData::operator=(
    EventLevelData&&) = default;

AttributionReport::EventLevelData::~EventLevelData() = default;

AttributionReport::AggregatableData::AggregatableData(
    std::optional<SuitableOrigin> aggregation_coordinator_origin,
    attribution_reporting::AggregatableTriggerConfig
        aggregatable_trigger_config,
    base::Time source_time,
    std::vector<blink::mojom::AggregatableReportHistogramContribution>
        contributions,
    std::optional<attribution_reporting::SuitableOrigin> source_origin)
    : aggregation_coordinator_origin_(
          std::move(aggregation_coordinator_origin)),
      aggregatable_trigger_config_(std::move(aggregatable_trigger_config)),
      source_time_(source_time),
      contributions_(std::move(contributions)),
      source_origin_(std::move(source_origin)) {
  CHECK(source_origin_.has_value() || contributions_.empty());
}

AttributionReport::AggregatableData::AggregatableData(const AggregatableData&) =
    default;

AttributionReport::AggregatableData&
AttributionReport::AggregatableData::operator=(const AggregatableData&) =
    default;

AttributionReport::AggregatableData::AggregatableData(AggregatableData&&) =
    default;

AttributionReport::AggregatableData&
AttributionReport::AggregatableData::operator=(AggregatableData&&) = default;

void AttributionReport::AggregatableData::SetContributions(
    std::vector<blink::mojom::AggregatableReportHistogramContribution>
        contributions) {
  CHECK(source_origin_.has_value());
  CHECK(!contributions.empty());
  contributions_ = std::move(contributions);
}

void AttributionReport::AggregatableData::SetAssembledReport(
    std::optional<AggregatableReport> assembled_report) {
  DCHECK(!assembled_report_.has_value());
  assembled_report_ = std::move(assembled_report);
}

AttributionReport::AggregatableData::~AggregatableData() = default;

base::CheckedNumeric<int64_t>
AttributionReport::AggregatableData::BudgetRequired() const {
  return GetTotalAggregatableValues(contributions_);
}

AttributionReport::AttributionReport(
    AttributionInfo attribution_info,
    Id id,
    base::Time report_time,
    base::Time initial_report_time,
    base::Uuid external_report_id,
    int failed_send_attempts,
    Data data,
    attribution_reporting::SuitableOrigin reporting_origin,
    std::optional<uint64_t> source_debug_key)
    : attribution_info_(std::move(attribution_info)),
      id_(id),
      report_time_(report_time),
      initial_report_time_(initial_report_time),
      external_report_id_(std::move(external_report_id)),
      failed_send_attempts_(failed_send_attempts),
      data_(std::move(data)),
      reporting_origin_(std::move(reporting_origin)),
      source_debug_key_(source_debug_key) {
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
    case Type::kNullAggregatable:
      endpoint_path = "report-aggregate-attribution";
      break;
  }

  std::string path =
      base::StrCat({kBasePath, debug ? kDebugPath : "", endpoint_path});

  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  return reporting_origin_->GetURL().ReplaceComponents(replacements);
}

base::Value::Dict AttributionReport::ReportBody() const {
  base::Value::Dict dict;

  absl::visit(
      base::Overloaded{
          [&](const EventLevelData& data) {
            dict.Set("attribution_destination", data.destinations.ToJson());

            // The API denotes these values as strings; a `uint64_t` cannot be
            // put in a dict as an integer in order to be opaque to various API
            // configurations.
            dict.Set("source_event_id",
                     base::NumberToString(data.source_event_id));

            dict.Set("trigger_data", base::NumberToString(data.trigger_data));

            dict.Set("source_type",
                     attribution_reporting::SourceTypeName(data.source_type));

            dict.Set("report_id", external_report_id_.AsLowercaseString());

            // Round to 7 digits of precision, which allows us to express binary
            // randomized response with epsilon = 14 without rounding to 0
            // (0.00000166305 -> 0.0000017).
            double rounded_rate =
                round(data.randomized_response_rate * 10000000) / 10000000.0;
            dict.Set("randomized_trigger_rate", rounded_rate);

            dict.Set("scheduled_report_time",
                     base::NumberToString(
                         (initial_report_time_ - base::Time::UnixEpoch())
                             .InSeconds()));
          },

          [&](const AggregatableData& data) { PopulateReportBody(dict, data); },
      },
      data_);

  if (CanDebuggingBeEnabled()) {
    CHECK(source_debug_key_.has_value());
    std::optional<uint64_t> trigger_debug_key = attribution_info_.debug_key;
    CHECK(trigger_debug_key.has_value());
    dict.Set("source_debug_key", base::NumberToString(*source_debug_key_));
    dict.Set("trigger_debug_key", base::NumberToString(*trigger_debug_key));
  }

  return dict;
}

AttributionReport::Type AttributionReport::GetReportType() const {
  return absl::visit(
      base::Overloaded{
          [](const EventLevelData&) {
            return AttributionReport::Type::kEventLevel;
          },

          [](const AggregatableData& data) {
            return data.is_null()
                       ? AttributionReport::Type::kNullAggregatable
                       : AttributionReport::Type::kAggregatableAttribution;
          },
      },
      data_);
}

void AttributionReport::set_report_time(base::Time report_time) {
  report_time_ = report_time;
}

// static
std::optional<base::Time> AttributionReport::MinReportTime(
    std::optional<base::Time> a,
    std::optional<base::Time> b) {
  if (!a.has_value()) {
    return b;
  }

  if (!b.has_value()) {
    return a;
  }

  return std::min(*a, *b);
}

const SuitableOrigin& AttributionReport::GetSourceOrigin() const {
  return absl::visit(
      base::Overloaded{
          [](const AttributionReport::EventLevelData& data)
              -> const SuitableOrigin& { return data.source_origin; },
          [&](const AttributionReport::AggregatableData& data)
              -> const SuitableOrigin& {
            if (data.source_origin().has_value()) {
              return *data.source_origin();
            }
            return attribution_info_.context_origin;
          },
      },
      data_);
}

bool AttributionReport::CanDebuggingBeEnabled() const {
  return attribution_info_.debug_key.has_value() &&
         source_debug_key_.has_value();
}

}  // namespace content
