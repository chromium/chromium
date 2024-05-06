// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/aggregatable_attribution_utils.h"

#include <stdint.h>

#include <iterator>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/overloaded.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregatable_trigger_config.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_utils.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_registration_time_config.mojom.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/attribution_reporting/attribution_info.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"

namespace content {

namespace {

// Note: use the same time serialization as in aggregatable_report.cc.
// Consider sharing logic if more call-sites need this.
std::string SerializeTimeRoundedDownToWholeDayInSeconds(base::Time time) {
  // TODO(csharrison, linnan): Validate that `time` is valid (e.g. not null /
  // inf).
  base::Time rounded =
      attribution_reporting::RoundDownToWholeDaySinceUnixEpoch(time);
  return base::NumberToString(rounded.InMillisecondsSinceUnixEpoch() /
                              base::Time::kMillisecondsPerSecond);
}

}  // namespace

std::vector<blink::mojom::AggregatableReportHistogramContribution>
CreateAggregatableHistogram(
    const attribution_reporting::FilterData& source_filter_data,
    attribution_reporting::mojom::SourceType source_type,
    const base::Time& source_time,
    const base::Time& trigger_time,
    const attribution_reporting::AggregationKeys& keys,
    const std::vector<attribution_reporting::AggregatableTriggerData>&
        aggregatable_trigger_data,
    const std::vector<attribution_reporting::AggregatableValues>&
        aggregatable_values) {
  int num_trigger_data_filtered = 0;

  attribution_reporting::AggregationKeys::Keys buckets = keys.keys();

  // For each piece of trigger data specified, check if its filters/not_filters
  // match for the given source, and if applicable modify the bucket based on
  // the given key piece.
  for (const auto& data : aggregatable_trigger_data) {
    if (!source_filter_data.Matches(source_type, source_time, trigger_time,
                                    data.filters())) {
      ++num_trigger_data_filtered;
      continue;
    }

    for (const auto& source_key : data.source_keys()) {
      auto bucket = buckets.find(source_key);
      if (bucket == buckets.end()) {
        continue;
      }

      bucket->second |= data.key_piece();
    }
  }

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      contributions;
  for (const auto& aggregatable_value : aggregatable_values) {
    if (source_filter_data.Matches(source_type, source_time, trigger_time,
                                   aggregatable_value.filters())) {
      const attribution_reporting::AggregatableValues::Values& values =
          aggregatable_value.values();
      for (const auto& [key_id, key] : buckets) {
        auto value = values.find(key_id);
        if (value == values.end()) {
          continue;
        }

        contributions.emplace_back(key,
                                   base::checked_cast<int32_t>(value->second),
                                   /*filtering_id=*/std::nullopt);
      }
      break;
    }
  }

  if (!aggregatable_trigger_data.empty()) {
    base::UmaHistogramPercentage(
        "Conversions.AggregatableReport.FilteredTriggerDataPercentage",
        100 * num_trigger_data_filtered / aggregatable_trigger_data.size());
  }

  if (!buckets.empty()) {
    base::UmaHistogramPercentage(
        "Conversions.AggregatableReport.DroppedKeysPercentage",
        100 * (buckets.size() - contributions.size()) / buckets.size());
  }

  static_assert(attribution_reporting::kMaxAggregationKeysPerSource == 20,
                "Bump the version for histogram "
                "Conversions.AggregatableReport.NumContributionsPerReport2");

  base::UmaHistogramExactLinear(
      "Conversions.AggregatableReport.NumContributionsPerReport2",
      contributions.size(),
      attribution_reporting::kMaxAggregationKeysPerSource + 1);

  // If total values exceeds the max, log the metrics as 100,000 to measure
  // how often the max is exceeded.
  static_assert(attribution_reporting::kMaxAggregatableValue == 65536);
  const int64_t max_value = attribution_reporting::kMaxAggregatableValue + 1;
  int adjusted_value = std::min(
      max_value,
      static_cast<int64_t>(
          GetTotalAggregatableValues(contributions).ValueOrDefault(max_value)));
  base::UmaHistogramCounts100000(
      "Conversions.AggregatableReport.TotalBudgetPerReport",
      adjusted_value == max_value ? 100000 : adjusted_value);

  return contributions;
}

std::optional<AggregatableReportRequest> CreateAggregatableReportRequest(
    const AttributionReport& report) {
  base::Time source_time;
  std::optional<uint64_t> source_debug_key;
  const AttributionReport::CommonAggregatableData* common_aggregatable_data =
      nullptr;
  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      contributions;

  absl::visit(
      base::Overloaded{
          [](const AttributionReport::EventLevelData&) { NOTREACHED(); },
          [&](const AttributionReport::AggregatableAttributionData& data) {
            source_time = data.source.source_time();
            source_debug_key = data.source.debug_key();
            common_aggregatable_data = &data.common_data;
            contributions = data.contributions;
          },
          [&](const AttributionReport::NullAggregatableData& data) {
            source_time = data.fake_source_time;
            common_aggregatable_data = &data.common_data;
          },
      },
      report.data());
  DCHECK(common_aggregatable_data);

  const AttributionInfo& attribution_info = report.attribution_info();

  AggregatableReportSharedInfo::DebugMode debug_mode =
      source_debug_key.has_value() && attribution_info.debug_key.has_value()
          ? AggregatableReportSharedInfo::DebugMode::kEnabled
          : AggregatableReportSharedInfo::DebugMode::kDisabled;

  base::Value::Dict additional_fields;
  std::string serialized_source_time;
  switch (common_aggregatable_data->aggregatable_trigger_config
              .source_registration_time_config()) {
    case attribution_reporting::mojom::SourceRegistrationTimeConfig::kInclude:
      serialized_source_time =
          SerializeTimeRoundedDownToWholeDayInSeconds(source_time);
      break;
    case attribution_reporting::mojom::SourceRegistrationTimeConfig::kExclude:
      // Use a default valid but impossible value to indicate exclusion of
      // source registration time.
      serialized_source_time = "0";
      break;
  }
  additional_fields.Set("source_registration_time",
                        std::move(serialized_source_time));
  additional_fields.Set(
      "attribution_destination",
      net::SchemefulSite(attribution_info.context_origin).Serialize());
  return AggregatableReportRequest::Create(
      AggregationServicePayloadContents(
          AggregationServicePayloadContents::Operation::kHistogram,
          std::move(contributions),
          blink::mojom::AggregationServiceMode::kDefault,
          common_aggregatable_data->aggregation_coordinator_origin
              ? std::make_optional(
                    **common_aggregatable_data->aggregation_coordinator_origin)
              : std::nullopt,
          /*max_contributions_allowed=*/
          attribution_reporting::kMaxAggregationKeysPerSource,
          /*filtering_id_max_bytes=*/std::nullopt),
      AggregatableReportSharedInfo(
          report.initial_report_time(), report.external_report_id(),
          report.GetReportingOrigin(), debug_mode, std::move(additional_fields),
          AttributionReport::CommonAggregatableData::kVersion,
          AttributionReport::CommonAggregatableData::kApiIdentifier));
}

base::CheckedNumeric<int64_t> GetTotalAggregatableValues(
    const std::vector<blink::mojom::AggregatableReportHistogramContribution>&
        contributions) {
  base::CheckedNumeric<int64_t> total_value = 0;
  for (const blink::mojom::AggregatableReportHistogramContribution&
           contribution : contributions) {
    total_value += contribution.value;
  }
  return total_value;
}

}  // namespace content
