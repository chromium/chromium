// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/aggregatable_attribution_utils.h"

#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "content/browser/attribution_reporting/aggregatable_attribution.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_sources.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_trigger.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_values.h"
#include "content/browser/attribution_reporting/attribution_filter_data.h"
#include "content/browser/attribution_reporting/attribution_utils.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom.h"

namespace content {

std::vector<AggregatableHistogramContribution> CreateAggregatableHistogram(
    const AttributionFilterData& source_filter_data,
    const AttributionAggregatableSources& sources,
    const AttributionAggregatableTrigger& trigger,
    const AttributionAggregatableValues& values) {
  // TODO(linnan): Log metrics for early returns.

  // Pairs of key id and bucket key.
  std::vector<std::pair<std::string, absl::uint128>> buckets;
  buckets.reserve(sources.proto().sources().size());
  for (const auto& [key_id, key] : sources.proto().sources()) {
    buckets.emplace_back(key_id,
                         absl::MakeUint128(key.high_bits(), key.low_bits()));
  }

  base::flat_map<std::string, absl::uint128> buckets_map(std::move(buckets));

  // For each piece of trigger data specified, check if its filters/not_filters
  // match for the given source, and if applicable modify the bucket based on
  // the given key piece.
  for (const auto& data : trigger.trigger_data()) {
    if (!data.filters().filter_values().empty() &&
        !AttributionFilterDataMatch(source_filter_data, data.filters())) {
      continue;
    }

    if (!data.not_filters().filter_values().empty() &&
        AttributionFilterDataMatch(source_filter_data, data.not_filters())) {
      continue;
    }

    for (const auto& source_key : data.source_keys()) {
      auto bucket = buckets_map.find(source_key);
      if (bucket == buckets_map.end())
        continue;

      bucket->second |=
          absl::MakeUint128(data.key().high_bits, data.key().low_bits);
    }
  }

  std::vector<AggregatableHistogramContribution> contributions;
  for (const auto& [key_id, key] : buckets_map) {
    auto value = values.values().find(key_id);
    if (value == values.values().end())
      continue;

    contributions.emplace_back(key, value->second);
  }

  return contributions;
}

}  // namespace content
