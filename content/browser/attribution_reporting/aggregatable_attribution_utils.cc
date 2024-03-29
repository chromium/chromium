// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/aggregatable_attribution_utils.h"

#include <set>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>
#include "base/logging.h"

#include "base/metrics/histogram_functions.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "base/check.h"
#include "base/functional/overloaded.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregatable_trigger_config.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_registration_time_config.mojom.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/attribution_reporting/aggregatable_histogram_contribution.h"
#include "content/browser/attribution_reporting/attribution_info.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/partition.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/mojom/private_aggregation/aggregatable_report.mojom.h"

namespace content {

namespace {

// using AggregatableResult = ::content::AttributionTrigger::AggregatableResult;


// Note: use the same time serialization as in aggregatable_report.cc.
// Consider sharing logic if more call-sites need this.
std::string SerializeTimeRoundedDownToWholeDayInSeconds(base::Time time) {
  // TODO(csharrison, linnan): Validate that `time` is valid (e.g. not null /
  // inf).
  base::Time rounded = RoundDownToWholeDaySinceUnixEpoch(time);
  return base::NumberToString(rounded.InMillisecondsSinceUnixEpoch() /
                              base::Time::kMillisecondsPerSecond);
}

}  // namespace

std::vector<AggregatableHistogramContribution> CreateAggregatableHistogram(
    const attribution_reporting::FilterData& source_filter_data,
    attribution_reporting::mojom::SourceType source_type,
    const base::Time& source_time,
    const base::Time& trigger_time,
    const attribution_reporting::AggregationKeys& keys,
    const std::vector<attribution_reporting::AggregatableTriggerData>&
        aggregatable_trigger_data,
    const attribution_reporting::AggregatableValues& aggregatable_values) {
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

  const attribution_reporting::AggregatableValues::Values& values =
      aggregatable_values.values();

  std::vector<AggregatableHistogramContribution> contributions;
  for (const auto& [key_id, key] : buckets) {
    auto value = values.find(key_id);
    if (value == values.end()) {
      continue;
    }

    contributions.emplace_back(key, value->second);
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

  return contributions;
}


// TODO(kelly): Organize in classes
void AttributionLogicLastTouch(Partition& partition, 
        base::flat_map<std::string, std::vector<absl::uint128>>& trigger_keypieces_per_source) {

  auto& attribution_window = partition.attribution_window;
  auto& sources_per_epoch = partition.sources_per_epoch;
  
  std::optional<StoredSource*> latest_source;

  for (uint64_t i=attribution_window.epoch_end(); 
          i>= attribution_window.epoch_start(); i--) {
    
    auto it = sources_per_epoch.find(i);
    if (it != sources_per_epoch.end()) {
      // Obtaining latest source (we fetched them in order from the database)
        if (!sources_per_epoch[i].empty()) {
          latest_source = sources_per_epoch[i].back();
          // Keep latest source to display in user logs 
          partition.logging_source = latest_source;
        }
      // Stop searching for more sources in other epochs
      break;
    }
  }

  // Populate partition.report_value_pairs[*].report for all source_keys
  if (latest_source.has_value()) {
      auto aggregation_keys = (*latest_source)->aggregation_keys().keys();
      for (auto& pair : aggregation_keys) {
        auto& source_key = pair.first;
        auto& source_keypiece = pair.second;
        auto& report_value_pair = partition.report_value_pairs[source_key];
        auto& trigger_keypieces = trigger_keypieces_per_source[source_key];

        // Extend the source key_pieces for source_key
        for (auto& trigger_keypiece : trigger_keypieces) {
          source_keypiece |= trigger_keypiece;
        }
        report_value_pair.report.emplace_back(source_keypiece, report_value_pair.value);      
    }
  }
}

void AttributionLogicUniform(Partition& partition,
        base::flat_map<std::string, std::vector<absl::uint128>>& trigger_keypieces_per_source) {

  auto& attribution_window = partition.attribution_window;
  auto& sources_per_epoch = partition.sources_per_epoch;
  
  double total_sources_count = 0;

  base::flat_map<std::string, base::flat_map<absl::uint128, double>>
          source_counts_per_sourcekey;


  for (uint64_t i=attribution_window.epoch_start(); 
          i <= attribution_window.epoch_end(); i++) {
    
    // Ignore empty epochs
    auto it = sources_per_epoch.find(i);
    if (it == sources_per_epoch.end()) {
      continue;
    }

    // Count occurrences per source keypiece across all epochs
    for (StoredSource* source : sources_per_epoch[i]) {
      // Keep latest source to display in user logs 
      partition.logging_source = source;
      auto aggregation_keys = source->aggregation_keys().keys();
      for (auto& pair : aggregation_keys) {
        auto& source_key = pair.first;
        auto& key_piece = pair.second;

        auto it1 = source_counts_per_sourcekey.find(source_key);
        if (it1 == source_counts_per_sourcekey.end()) {
          source_counts_per_sourcekey[source_key] = {};
        }

        auto& source_counts = source_counts_per_sourcekey[source_key];
        auto it2 = source_counts.find(key_piece);
        if (it2 == source_counts.end()) {
          source_counts[key_piece] = 0;
        }
        source_counts[key_piece] += 1;
      }
      total_sources_count++;
    }
  }

  // Populate partition.report_value_pairs[*].report for all source_keys
  for (auto& outer : source_counts_per_sourcekey) {
    auto source_key = outer.first;
    auto& report_value_pair = partition.report_value_pairs[source_key];
    auto& trigger_keypieces = trigger_keypieces_per_source[source_key];

    for (auto& inner : outer.second) {
      auto source_keypiece = inner.first;
      auto source_count = inner.second;

      // Extend the source key_pieces for source_key
      for (auto& trigger_keypiece : trigger_keypieces) {
        source_keypiece |= trigger_keypiece;
      }

      double contribution_value = (source_count / total_sources_count) * report_value_pair.value;
      report_value_pair.report.emplace_back(source_keypiece, contribution_value);      
    }
  }
 }

void CreateAggregatableHistogramM2M(
    Partition& partition,
    const std::vector<attribution_reporting::AggregatableTriggerData>& aggregatable_trigger_data) {
  
  // Collect trigger keypieces per source_key
  base::flat_map<std::string, std::vector<absl::uint128>> trigger_keypieces_per_source;  
  for (const auto& data : aggregatable_trigger_data) {
    for (const auto& source_key : data.source_keys()) {

        auto it = trigger_keypieces_per_source.find(source_key);
        if (it == trigger_keypieces_per_source.end()) {
          trigger_keypieces_per_source[source_key] = {};
        }
        trigger_keypieces_per_source[source_key].push_back(data.key_piece());
    }
  }
  // Apply "attribution_logic" on the union of all epochs
  if (partition.attribution_logic == "last_touch") {
    AttributionLogicLastTouch(partition, trigger_keypieces_per_source);
  } else if (partition.attribution_logic == "uniform") {
    AttributionLogicUniform(partition, trigger_keypieces_per_source);
  }
}

std::optional<AggregatableReportRequest> CreateAggregatableReportRequest(
    const AttributionReport& report) {
  base::Time source_time;
  std::optional<uint64_t> source_debug_key;
  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      contributions;
  const AttributionReport::CommonAggregatableData* common_aggregatable_data =
      nullptr;

  absl::visit(
      base::Overloaded{
          [](const AttributionReport::EventLevelData&) { NOTREACHED(); },
          [&](const AttributionReport::AggregatableAttributionData& data) {
            source_time = data.source.source_time();
            source_debug_key = data.source.debug_key();
            common_aggregatable_data = &data.common_data;
            base::ranges::transform(
                data.contributions, std::back_inserter(contributions),
                [](const auto& contribution) {
                  return blink::mojom::AggregatableReportHistogramContribution(
                      /*bucket=*/contribution.key(),
                      /*value=*/base::checked_cast<int32_t>(
                          contribution.value()));
                });
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
          attribution_reporting::kMaxAggregationKeysPerSource),
      AggregatableReportSharedInfo(
          report.initial_report_time(), report.external_report_id(),
          report.GetReportingOrigin(), debug_mode, std::move(additional_fields),
          AttributionReport::CommonAggregatableData::kVersion,
          AttributionReport::CommonAggregatableData::kApiIdentifier));
}

base::Time RoundDownToWholeDaySinceUnixEpoch(base::Time time) {
  return base::Time::UnixEpoch() +
         (time - base::Time::UnixEpoch()).FloorToMultiple(base::Days(1));
}

}  // namespace content
