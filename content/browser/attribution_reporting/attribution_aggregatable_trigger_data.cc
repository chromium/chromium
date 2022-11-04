// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_aggregatable_trigger_data.h"

#include <utility>

#include "base/ranges/algorithm.h"
#include "components/attribution_reporting/constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

// static
absl::optional<AttributionAggregatableTriggerData>
AttributionAggregatableTriggerData::Create(
    absl::uint128 key_piece,
    base::flat_set<std::string> source_keys,
    AttributionFilters filters,
    AttributionFilters not_filters) {
  if (source_keys.size() >
      attribution_reporting::kMaxAggregationKeysPerSourceOrTrigger) {
    return absl::nullopt;
  }

  bool is_valid = base::ranges::all_of(source_keys, [](const auto& key) {
    return key.size() <= attribution_reporting::kMaxBytesPerAggregationKeyId;
  });
  if (!is_valid)
    return absl::nullopt;

  return AttributionAggregatableTriggerData(key_piece, std::move(source_keys),
                                            std::move(filters),
                                            std::move(not_filters));
}

// static
AttributionAggregatableTriggerData
AttributionAggregatableTriggerData::CreateForTesting(
    absl::uint128 key_piece,
    base::flat_set<std::string> source_keys,
    AttributionFilters filters_values,
    AttributionFilters not_filters_values) {
  return AttributionAggregatableTriggerData(key_piece, std::move(source_keys),
                                            std::move(filters_values),
                                            std::move(not_filters_values));
}

AttributionAggregatableTriggerData::AttributionAggregatableTriggerData(
    absl::uint128 key_piece,
    base::flat_set<std::string> source_keys,
    AttributionFilters filters,
    AttributionFilters not_filters)
    : key_piece_(key_piece),
      source_keys_(std::move(source_keys)),
      filters_(std::move(filters)),
      not_filters_(std::move(not_filters)) {}

AttributionAggregatableTriggerData::~AttributionAggregatableTriggerData() =
    default;

AttributionAggregatableTriggerData::AttributionAggregatableTriggerData(
    const AttributionAggregatableTriggerData&) = default;

AttributionAggregatableTriggerData::AttributionAggregatableTriggerData(
    AttributionAggregatableTriggerData&&) = default;

AttributionAggregatableTriggerData&
AttributionAggregatableTriggerData::operator=(
    const AttributionAggregatableTriggerData&) = default;

AttributionAggregatableTriggerData&
AttributionAggregatableTriggerData::operator=(
    AttributionAggregatableTriggerData&&) = default;

}  // namespace content
