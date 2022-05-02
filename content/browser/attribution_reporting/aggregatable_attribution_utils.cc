// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/aggregatable_attribution_utils.h"

#include <iterator>
#include <sstream>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/ranges/algorithm.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/attribution_reporting/aggregatable_histogram_contribution.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_source.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_trigger.h"
#include "content/browser/attribution_reporting/attribution_filter_data.h"
#include "content/browser/attribution_reporting/attribution_info.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {

std::vector<AggregatableHistogramContribution> CreateAggregatableHistogram(
    const AttributionFilterData& source_filter_data,
    const AttributionAggregatableSource& source,
    const AttributionAggregatableTrigger& trigger) {
  // TODO(linnan): Log metrics for early returns.

  AttributionAggregatableSource::Keys buckets = source.keys();

  // For each piece of trigger data specified, check if its filters/not_filters
  // match for the given source, and if applicable modify the bucket based on
  // the given key piece.
  for (const auto& data : trigger.trigger_data()) {
    if (!AttributionFiltersMatch(source_filter_data, data.filters(),
                                 data.not_filters())) {
      continue;
    }

    for (const auto& source_key : data.source_keys()) {
      auto bucket = buckets.find(source_key);
      if (bucket == buckets.end())
        continue;

      bucket->second |= data.key();
    }
  }

  std::vector<AggregatableHistogramContribution> contributions;
  for (const auto& [key_id, key] : buckets) {
    auto value = trigger.values().find(key_id);
    if (value == trigger.values().end())
      continue;

    contributions.emplace_back(key, value->second);
  }

  return contributions;
}

std::string HexEncodeAggregatableKey(absl::uint128 value) {
  std::ostringstream out;
  out << "0x";
  out.setf(out.hex, out.basefield);
  out << value;
  return out.str();
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

  std::vector<AggregationServicePayloadContents::HistogramContribution>
      contributions;
  base::ranges::transform(
      data->contributions, std::back_inserter(contributions),
      [](const auto& contribution) {
        return AggregationServicePayloadContents::HistogramContribution{
            .bucket = contribution.key(),
            .value = static_cast<int>(contribution.value())};
      });

  return AggregatableReportRequest::Create(
      AggregationServicePayloadContents(
          AggregationServicePayloadContents::Operation::kHistogram,
          std::move(contributions),
          AggregationServicePayloadContents::AggregationMode::kDefault),
      AggregatableReportSharedInfo(
          data->initial_report_time, report.PrivacyBudgetKey(),
          report.external_report_id(),
          attribution_info.source.common_info().reporting_origin(),
          debug_mode));
}

}  // namespace content
