// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_aggregatable_trigger.h"

#include <iterator>
#include <utility>

#include "base/ranges/algorithm.h"
#include "third_party/blink/public/common/attribution_reporting/constants.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom.h"

namespace content {

// static
absl::optional<AttributionAggregatableTriggerData>
AttributionAggregatableTriggerData::FromMojo(
    blink::mojom::AttributionAggregatableTriggerDataPtr mojo) {
  if (mojo->source_keys.size() >
      blink::kMaxAttributionAggregatableKeysPerSourceOrTrigger) {
    return absl::nullopt;
  }

  bool is_valid = base::ranges::all_of(mojo->source_keys, [](const auto& key) {
    return key.size() <= blink::kMaxBytesPerAttributionAggregatableKeyId;
  });
  if (!is_valid)
    return absl::nullopt;

  absl::optional<AttributionFilterData> filters =
      AttributionFilterData::FromTriggerFilterValues(
          std::move(mojo->filters->filter_values));
  if (!filters.has_value())
    return absl::nullopt;

  absl::optional<AttributionFilterData> not_filters =
      AttributionFilterData::FromTriggerFilterValues(
          std::move(mojo->not_filters->filter_values));
  if (!not_filters.has_value())
    return absl::nullopt;

  return AttributionAggregatableTriggerData(
      mojo->key,
      base::flat_set<std::string>(
          std::make_move_iterator(mojo->source_keys.begin()),
          std::make_move_iterator(mojo->source_keys.end())),
      std::move(*filters), std::move(*not_filters));
}

AttributionAggregatableTriggerData::AttributionAggregatableTriggerData() =
    default;

AttributionAggregatableTriggerData::AttributionAggregatableTriggerData(
    absl::uint128 key,
    base::flat_set<std::string> source_keys,
    AttributionFilterData filters,
    AttributionFilterData not_filters)
    : key_(key),
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

// static
absl::optional<AttributionAggregatableTrigger>
AttributionAggregatableTrigger::FromMojo(
    blink::mojom::AttributionAggregatableTriggerPtr mojo) {
  if (mojo->trigger_data.size() >
      blink::kMaxAttributionAggregatableTriggerDataPerTrigger) {
    return absl::nullopt;
  }

  std::vector<AttributionAggregatableTriggerData> trigger_data;
  for (auto& data_mojo : mojo->trigger_data) {
    absl::optional<AttributionAggregatableTriggerData> data =
        AttributionAggregatableTriggerData::FromMojo(std::move(data_mojo));
    if (!data.has_value())
      return absl::nullopt;

    trigger_data.push_back(std::move(*data));
  }

  if (mojo->values.size() >
      blink::kMaxAttributionAggregatableKeysPerSourceOrTrigger) {
    return absl::nullopt;
  }

  bool is_valid = base::ranges::all_of(mojo->values, [](const auto& value) {
    return value.first.size() <=
               blink::kMaxBytesPerAttributionAggregatableKeyId &&
           value.second > 0 &&
           value.second <= blink::kMaxAttributionAggregatableValue;
  });
  if (!is_valid)
    return absl::nullopt;

  return AttributionAggregatableTrigger(std::move(trigger_data),
                                        std::move(mojo->values));
}

AttributionAggregatableTrigger::AttributionAggregatableTrigger() = default;

AttributionAggregatableTrigger::AttributionAggregatableTrigger(
    std::vector<AttributionAggregatableTriggerData> trigger_data,
    Values values)
    : trigger_data_(std::move(trigger_data)), values_(std::move(values)) {}

AttributionAggregatableTrigger::~AttributionAggregatableTrigger() = default;

AttributionAggregatableTrigger::AttributionAggregatableTrigger(
    const AttributionAggregatableTrigger&) = default;

AttributionAggregatableTrigger::AttributionAggregatableTrigger(
    AttributionAggregatableTrigger&&) = default;

AttributionAggregatableTrigger& AttributionAggregatableTrigger::operator=(
    const AttributionAggregatableTrigger&) = default;

AttributionAggregatableTrigger& AttributionAggregatableTrigger::operator=(
    AttributionAggregatableTrigger&&) = default;

}  // namespace content
