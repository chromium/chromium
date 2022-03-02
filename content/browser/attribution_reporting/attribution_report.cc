// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_report.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"
#include "url/url_canon.h"

namespace content {

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

AttributionReport::AggregatableContributionData::AggregatableContributionData(
    AggregatableHistogramContribution contribution,
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
          "/.well-known/attribution-reporting/report-event-attribution";
      return kEventEndpointPath;
    }

    const char* operator()(const AggregatableContributionData&) {
      static constexpr char kAggregateEndpointPath[] =
          "/.well-known/attribution-reporting/report-aggregate-attribution";
      return kAggregateEndpointPath;
    }
  };

  const char* path = absl::visit(Visitor{}, data_);
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

    base::Value operator()(const AggregatableContributionData& data) {
      DCHECK(data.assembled_report.has_value());

      const CommonSourceInfo& common_info =
          report->attribution_info().source.common_info();

      base::Value::DictStorage dict = data.assembled_report->GetAsJson();
      dict.emplace("source_site", common_info.ImpressionSite().Serialize());
      dict.emplace("attribution_destination",
                   common_info.ConversionDestination().Serialize());
      dict.emplace(
          "source_registration_time",
          base::NumberToString(common_info.impression_time().ToJavaTime() /
                               base::Time::kMillisecondsPerSecond));

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
  DCHECK(absl::holds_alternative<AggregatableContributionData>(data_));

  std::unique_ptr<crypto::SecureHash> ctx =
      crypto::SecureHash::Create(crypto::SecureHash::Algorithm::SHA256);

  const CommonSourceInfo& common_source_info =
      attribution_info_.source.common_info();
  const std::string serialized_reporting_origin =
      common_source_info.reporting_origin().Serialize();
  const std::string serialized_source_site =
      common_source_info.ImpressionSite().Serialize();
  const std::string serialized_attribution_destination =
      common_source_info.ConversionDestination().Serialize();

  static constexpr char kDelimiter[] = ";";

  ctx->Update(serialized_reporting_origin.data(),
              serialized_reporting_origin.size());

  ctx->Update(kDelimiter, sizeof(kDelimiter));
  ctx->Update(serialized_source_site.data(), serialized_source_site.size());

  ctx->Update(kDelimiter, sizeof(kDelimiter));
  ctx->Update(serialized_attribution_destination.data(),
              serialized_attribution_destination.size());

  // TODO(linnan): Replace with a real version once a version string is decided.
  static constexpr char kVersion[] = "1.0";
  ctx->Update(kDelimiter, sizeof(kDelimiter));
  ctx->Update(kVersion, sizeof(kVersion));

  std::string output(crypto::kSHA256Length, 0);
  ctx->Finish(std::data(output), output.size());

  return output;
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
