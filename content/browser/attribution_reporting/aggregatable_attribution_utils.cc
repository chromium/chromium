// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/aggregatable_attribution_utils.h"

#include <iterator>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/aggregation_service/aggregation_service.mojom.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/filters.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/attribution_reporting/aggregatable_histogram_contribution.h"
#include "content/browser/attribution_reporting/attribution_info.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/attribution_utils.h"
#include "content/common/aggregatable_report.mojom.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {

namespace {

// Note: use the same time serialization as in aggregatable_report.cc.
// Consider sharing logic if more call-sites need this.
std::string SerializeTimeRoundedDownToWholeDayInSeconds(base::Time time) {
  // TODO(csharrison, linnan): Validate that `time` is valid (e.g. not null /
  // inf).
  base::Time rounded =
      base::Time::UnixEpoch() +
      (time - base::Time::UnixEpoch()).FloorToMultiple(base::Days(1));
  return base::NumberToString(rounded.ToJavaTime() /
                              base::Time::kMillisecondsPerSecond);
}

}  // namespace

std::vector<AggregatableHistogramContribution> CreateAggregatableHistogram(
    const attribution_reporting::FilterData& source_filter_data,
    AttributionSourceType source_type,
    const attribution_reporting::AggregationKeys& keys,
    const attribution_reporting::AggregatableTriggerDataList&
        aggregatable_trigger_data,
    const attribution_reporting::AggregatableValues& aggregatable_values) {
  int num_trigger_data_filtered = 0;

  attribution_reporting::AggregationKeys::Keys buckets = keys.keys();

  // For each piece of trigger data specified, check if its filters/not_filters
  // match for the given source, and if applicable modify the bucket based on
  // the given key piece.
  for (const auto& data : aggregatable_trigger_data.vec()) {
    if (!AttributionFiltersMatch(source_filter_data, source_type,
                                 data.filters(), data.not_filters())) {
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

  if (!aggregatable_trigger_data.vec().empty()) {
    base::UmaHistogramPercentage(
        "Conversions.AggregatableReport.FilteredTriggerDataPercentage",
        100 * num_trigger_data_filtered /
            aggregatable_trigger_data.vec().size());
  }

  if (!buckets.empty()) {
    base::UmaHistogramPercentage(
        "Conversions.AggregatableReport.DroppedKeysPercentage",
        100 * (buckets.size() - contributions.size()) / buckets.size());
  }

  const int kExclusiveMaxHistogramValue = 101;

  static_assert(attribution_reporting::kMaxAggregationKeysPerSourceOrTrigger <
                    kExclusiveMaxHistogramValue,
                "Bump the version for histogram "
                "Conversions.AggregatableReport.NumContributionsPerReport");

  base::UmaHistogramCounts100(
      "Conversions.AggregatableReport.NumContributionsPerReport",
      contributions.size());

  return contributions;
}

absl::optional<AggregatableReportRequest> CreateAggregatableReportRequest(
    const AttributionReport& report) {
  const auto* data =
      absl::get_if<AttributionReport::AggregatableAttributionData>(
          &report.data());
  DCHECK(data);

  const AttributionInfo& attribution_info = report.attribution_info();

  AggregatableReportSharedInfo::DebugMode debug_mode =
      attribution_info.source.common_info().debug_key().has_value() &&
              attribution_info.debug_key.has_value()
          ? AggregatableReportSharedInfo::DebugMode::kEnabled
          : AggregatableReportSharedInfo::DebugMode::kDisabled;

  std::vector<mojom::AggregatableReportHistogramContribution> contributions;
  base::ranges::transform(
      data->contributions, std::back_inserter(contributions),
      [](const auto& contribution) {
        return mojom::AggregatableReportHistogramContribution(
            /*bucket=*/contribution.key(),
            /*value=*/static_cast<int>(contribution.value()));
      });

  base::Value::Dict additional_fields;
  additional_fields.Set(
      "source_registration_time",
      SerializeTimeRoundedDownToWholeDayInSeconds(
          attribution_info.source.common_info().source_time()));
  additional_fields.Set(
      "attribution_destination",
      attribution_info.source.common_info().DestinationSite().Serialize());
  return AggregatableReportRequest::Create(
      AggregationServicePayloadContents(
          AggregationServicePayloadContents::Operation::kHistogram,
          std::move(contributions), mojom::AggregationServiceMode::kDefault,
          data->aggregation_coordinator),
      AggregatableReportSharedInfo(
          data->initial_report_time, report.external_report_id(),
          attribution_info.source.common_info().reporting_origin(), debug_mode,
          std::move(additional_fields),
          AttributionReport::AggregatableAttributionData::kVersion,
          AttributionReport::AggregatableAttributionData::kApiIdentifier));
}

}  // namespace content
