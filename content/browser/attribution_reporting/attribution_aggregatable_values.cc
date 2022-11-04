// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_aggregatable_values.h"

#include <utility>

#include "base/ranges/algorithm.h"
#include "components/attribution_reporting/constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

// static
absl::optional<AttributionAggregatableValues>
AttributionAggregatableValues::FromValues(Values values) {
  if (values.size() >
      attribution_reporting::kMaxAggregationKeysPerSourceOrTrigger) {
    return absl::nullopt;
  }

  bool is_valid = base::ranges::all_of(values, [](const auto& value) {
    return value.first.size() <=
               attribution_reporting::kMaxBytesPerAggregationKeyId &&
           value.second > 0 &&
           value.second <= attribution_reporting::kMaxAggregatableValue;
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
