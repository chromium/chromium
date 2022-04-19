// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/aggregatable_attribution_utils.h"

#include <sstream>
#include <vector>

#include "content/browser/attribution_reporting/aggregatable_histogram_contribution.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_source.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_trigger.h"
#include "content/browser/attribution_reporting/attribution_filter_data.h"
#include "content/browser/attribution_reporting/attribution_utils.h"

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

}  // namespace content
