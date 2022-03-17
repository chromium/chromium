// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_report.h"

#include <string>
#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "crypto/sha2.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"
#include "url/url_canon.h"

namespace content {

namespace {

int64_t EncodeTimeRoundDownToWholeDayInSeconds(base::Time time) {
  return (time - base::Time::UnixEpoch())
      .FloorToMultiple(base::Days(1))
      .InSeconds();
}

}  // namespace

AttributionReport::EventLevelData::EventLevelData(
    uint64_t trigger_data,
    int64_t priority,
    double randomized_trigger_rate,
    absl::optional<Id> id)
    : trigger_data(trigger_data),
      priority(priority),
      randomized_trigger_rate(randomized_trigger_rate),
      id(id) {
  DCHECK_GE(randomized_trigger_rate, 0);
  DCHECK_LE(randomized_trigger_rate, 1);
}

AttributionReport::EventLevelData::EventLevelData(const EventLevelData& other) =
    default;

AttributionReport::EventLevelData& AttributionReport::EventLevelData::operator=(
    const EventLevelData& other) = default;

AttributionReport::EventLevelData::EventLevelData(EventLevelData&& other) =
    default;

AttributionReport::EventLevelData& AttributionReport::EventLevelData::operator=(
    EventLevelData&& other) = default;

AttributionReport::EventLevelData::~EventLevelData() = default;

AttributionReport::AggregatableAttributionData::AggregatableAttributionData(
    std::vector<AggregatableHistogramContribution> contributions,
    absl::optional<Id> id)
    : contributions(std::move(contributions)), id(id) {}

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
    absl::variant<EventLevelData, AggregatableAttributionData> data)
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

GURL AttributionReport::ReportURL(bool debug) const {
  static constexpr char kBasePath[] = "/.well-known/attribution-reporting/";
  static constexpr char kDebugPath[] = "debug/";

  struct Visitor {
    const char* operator()(const EventLevelData&) {
      static constexpr char kEventEndpointPath[] = "report-event-attribution";
      return kEventEndpointPath;
    }

    const char* operator()(const AggregatableAttributionData&) {
      static constexpr char kAggregateEndpointPath[] =
          "report-aggregate-attribution";
      return kAggregateEndpointPath;
    }
  };

  const char* endpoint_path = absl::visit(Visitor{}, data_);

  std::string path =
      base::StrCat({kBasePath, debug ? kDebugPath : "", endpoint_path});

  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  return attribution_info_.source.common_info()
      .reporting_origin()
      .GetURL()
      .ReplaceComponents(replacements);
}

base::Value AttributionReport::ReportBody() const {
  struct Visitor {
    raw_ptr<const AttributionReport> report;

    base::Value operator()(const EventLevelData& data) {
      base::Value dict(base::Value::Type::DICTIONARY);

      const CommonSourceInfo& common_source_info =
          report->attribution_info().source.common_info();

      dict.SetStringKey("attribution_destination",
                        common_source_info.ConversionDestination().Serialize());

      // The API denotes these values as strings; a `uint64_t` cannot be put in
      // a dict as an integer in order to be opaque to various API
      // configurations.
      dict.SetStringKey(
          "source_event_id",
          base::NumberToString(common_source_info.source_event_id()));

      dict.SetStringKey("trigger_data",
                        base::NumberToString(data.trigger_data));

      dict.SetStringKey("source_type", AttributionSourceTypeToString(
                                           common_source_info.source_type()));

      dict.SetStringKey("report_id",
                        report->external_report_id().AsLowercaseString());

      dict.SetDoubleKey("randomized_trigger_rate",
                        data.randomized_trigger_rate);

      if (absl::optional<uint64_t> debug_key = common_source_info.debug_key()) {
        dict.SetStringKey("source_debug_key", base::NumberToString(*debug_key));
      }

      if (absl::optional<uint64_t> debug_key =
              report->attribution_info().debug_key) {
        dict.SetStringKey("trigger_debug_key",
                          base::NumberToString(*debug_key));
      }

      return dict;
    }

    base::Value operator()(const AggregatableAttributionData& data) {
      base::Value::DictStorage dict;

      if (data.assembled_report.has_value()) {
        dict = data.assembled_report->GetAsJson();
      } else {
        // This generally should only be called when displaying the report for
        // debugging/internals.
        dict.emplace("shared_info", "not generated prior to send");
        dict.emplace("aggregation_service_payloads",
                     "not generated prior to send");
      }

      const CommonSourceInfo& common_info =
          report->attribution_info().source.common_info();

      dict.emplace("source_site", common_info.ImpressionSite().Serialize());
      dict.emplace("attribution_destination",
                   common_info.ConversionDestination().Serialize());

      // source_registration_time is rounded down to whole day and in seconds.
      dict.emplace("source_registration_time",
                   base::NumberToString(EncodeTimeRoundDownToWholeDayInSeconds(
                       common_info.impression_time())));

      if (absl::optional<uint64_t> debug_key = common_info.debug_key())
        dict.emplace("source_debug_key", base::NumberToString(*debug_key));

      if (absl::optional<uint64_t> debug_key =
              report->attribution_info().debug_key) {
        dict.emplace("trigger_debug_key", base::NumberToString(*debug_key));
      }

      return base::Value(std::move(dict));
    }
  };

  return absl::visit(Visitor{.report = this}, data_);
}

absl::optional<AttributionReport::Id> AttributionReport::ReportId() const {
  return absl::visit([](const auto& v) { return absl::optional<Id>(v.id); },
                     data_);
}

std::string AttributionReport::PrivacyBudgetKey() const {
  DCHECK(absl::holds_alternative<AggregatableAttributionData>(data_));

  const CommonSourceInfo& common_source_info =
      attribution_info_.source.common_info();

  // Use CBOR to be deterministic.
  cbor::Value::MapValue value;
  value.emplace("reporting_origin",
                common_source_info.reporting_origin().Serialize());
  value.emplace("source_site", common_source_info.ImpressionSite().Serialize());
  value.emplace("destination",
                common_source_info.ConversionDestination().Serialize());

  // TODO(linnan): Replace with a real version once a version string is decided.
  static constexpr char kVersion[] = "";
  value.emplace("version", kVersion);

  value.emplace("source_registration_time",
                EncodeTimeRoundDownToWholeDayInSeconds(
                    common_source_info.impression_time()));

  absl::optional<std::vector<uint8_t>> bytes =
      cbor::Writer::Write(cbor::Value(std::move(value)));
  DCHECK(bytes.has_value());

  return base::Base64Encode(crypto::SHA256Hash(*bytes));
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
