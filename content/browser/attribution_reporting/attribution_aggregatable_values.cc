// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_aggregatable_values.h"

#include <utility>

#include "base/ranges/algorithm.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/attribution_reporting/constants.h"

namespace content {

// static
absl::optional<AttributionAggregatableValues>
AttributionAggregatableValues::FromValues(Values values) {
  if (values.size() > blink::kMaxAttributionAggregationKeysPerSourceOrTrigger) {
    return absl::nullopt;
  }

  bool is_valid = base::ranges::all_of(values, [](const auto& value) {
    return value.first.size() <=
               blink::kMaxBytesPerAttributionAggregationKeyId &&
           value.second > 0 &&
           value.second <= blink::kMaxAttributionAggregatableValue;
  });
  if (!is_valid)
    return absl::nullopt;

  return AttributionAggregatableValues(std::move(values));
}

// static
AttributionAggregatableValues AttributionAggregatableValues::CreateForTesting(
    Values values) {
  return AttributionAggregatableValues(std::move(values));
}

AttributionAggregatableValues::AttributionAggregatableValues() = default;

AttributionAggregatableValues::AttributionAggregatableValues(Values values)
    : values_(std::move(values)) {}

AttributionAggregatableValues::~AttributionAggregatableValues() = default;

AttributionAggregatableValues::AttributionAggregatableValues(
    const AttributionAggregatableValues&) = default;
AttributionAggregatableValues::AttributionAggregatableValues(
    AttributionAggregatableValues&&) = default;

AttributionAggregatableValues& AttributionAggregatableValues::operator=(
    const AttributionAggregatableValues&) = default;
AttributionAggregatableValues& AttributionAggregatableValues::operator=(
    AttributionAggregatableValues&&) = default;

}  // namespace content
