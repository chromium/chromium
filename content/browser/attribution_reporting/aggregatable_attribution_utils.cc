// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/aggregatable_attribution_utils.h"

#include <stddef.h>
#include <stdint.h>

#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/overloaded.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/clamped_math.h"
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
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_registration_time_config.mojom.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service_features.h"
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

bool IsAggregatableFilteringIdsEnabled() {
  return base::FeatureList::IsEnabled(
             attribution_reporting::features::
                 kAttributionReportingAggregatableFilteringIds) &&
         base::FeatureList::IsEnabled(
             kPrivacySandboxAggregationServiceFilteringIds);
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
  size_t num_trigger_data_filtered = 0;

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
  const bool filtering_id_enabled = IsAggregatableFilteringIdsEnabled();
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

        std::optional<uint64_t> filtering_id;
        if (filtering_id_enabled) {
          filtering_id = value->second.filtering_id();
        }

        contributions.emplace_back(
            key, base::checked_cast<int32_t>(value->second.value()),
            filtering_id);
      }
      break;
    }
  }

  if (!aggregatable_trigger_data.empty()) {
    base::ClampedNumeric<size_t> percentage = num_trigger_data_filtered;
    percentage *= 100;
    percentage /= aggregatable_trigger_data.size();

    base::UmaHistogramPercentage(
        "Conversions.AggregatableReport.FilteredTriggerDataPercentage",
        percentage);
  }

  if (!buckets.empty()) {
    base::ClampedNumeric<size_t> percentage = buckets.size();
    percentage -= contributions.size();
    percentage *= 100;
    percentage /= buckets.size();

    base::UmaHistogramPercentage(
        "Conversions.AggregatableReport.DroppedKeysPercentage", percentage);
  }

  static_assert(attribution_reporting::kMaxAggregationKeysPerSource == 20,
                "Bump the version for histogram "
                "Conversions.AggregatableReport.NumContributionsPerReport2");

  base::UmaHistogramExactLinear(
      "Conversions.AggregatableReport.NumContributionsPerReport2",
      base::saturated_cast<int>(contributions.size()),
      attribution_reporting::kMaxAggregationKeysPerSource + 1);

  // If total values exceeds the max, log the metrics as 100,000 to measure
  // how often the max is exceeded.
  static_assert(attribution_reporting::kMaxAggregatableValue == 65536);
  const int64_t max_value = attribution_reporting::kMaxAggregatableValue + 1;
  int64_t adjusted_value = std::min(
      base::MakeStrictNum(max_value),
      GetTotalAggregatableValues(contributions).ValueOrDefault(max_value));
  base::UmaHistogramCounts100000(
      "Conversions.AggregatableReport.TotalBudgetPerReport",
      adjusted_value == max_value ? 100000
                                  : base::saturated_cast<int>(adjusted_value));

  return contributions;
}

std::optional<AggregatableReportRequest> CreateAggregatableReportRequest(
    const AttributionReport& report) {
  base::Time source_time;
  const AttributionReport::CommonAggregatableData* common_aggregatable_data =
      nullptr;
  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      contributions;

  absl::visit(
      base::Overloaded{
          [](const AttributionReport::EventLevelData&) {
            NOTREACHED_IN_MIGRATION();
          },
          [&](const AttributionReport::AggregatableAttributionData& data) {
            source_time = data.source_time;
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
      report.CanDebuggingBeEnabled()
          ? AggregatableReportSharedInfo::DebugMode::kEnabled
          : AggregatableReportSharedInfo::DebugMode::kDisabled;

  base::Value::Dict additional_fields;
  switch (common_aggregatable_data->aggregatable_trigger_config
              .source_registration_time_config()) {
    case attribution_reporting::mojom::SourceRegistrationTimeConfig::kInclude:
      additional_fields.Set(
          "source_registration_time",
          SerializeTimeRoundedDownToWholeDayInSeconds(source_time));
      break;
    case attribution_reporting::mojom::SourceRegistrationTimeConfig::kExclude:
      break;
  }

  SetAttributionDestination(
      additional_fields, net::SchemefulSite(attribution_info.context_origin));

  std::optional<size_t> filtering_id_max_bytes;
  if (IsAggregatableFilteringIdsEnabled()) {
    filtering_id_max_bytes =
        common_aggregatable_data->aggregatable_trigger_config
            .aggregatable_filtering_id_max_bytes()
            .value();
  } else {
    // We clear the filtering ids to avoid hitting `FilteringIdsFitInMaxBytes()`
    // invalidly in case that filtering ids were unexpectedly set in the db for
    // some reason like db corruption.
    for (auto& contribution : contributions) {
      contribution.filtering_id.reset();
    }
  }
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
          filtering_id_max_bytes),
      AggregatableReportSharedInfo(
          report.initial_report_time(), report.external_report_id(),
          report.reporting_origin(), debug_mode, std::move(additional_fields),
          filtering_id_max_bytes.has_value()
              ? AttributionReport::CommonAggregatableData::
                    kVersionWithFlexibleContributionFiltering
              : AttributionReport::CommonAggregatableData::kVersion,
          AttributionReport::CommonAggregatableData::kApiIdentifier),
      // The returned request cannot be serialized due to the null `delay_type`.
      /*delay_type=*/std::nullopt);
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

void SetAttributionDestination(base::Value::Dict& dict,
                               const net::SchemefulSite& destination) {
  dict.Set("attribution_destination", destination.Serialize());
}

}  // namespace content
