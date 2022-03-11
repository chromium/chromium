// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_aggregatable_values.h"

#include <utility>

#include "base/ranges/algorithm.h"
#include "third_party/blink/public/common/attribution_reporting/constants.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom.h"

namespace content {

// static
absl::optional<AttributionAggregatableValues>
AttributionAggregatableValues::FromMojo(
    blink::mojom::AttributionAggregatableValuesPtr mojo) {
  if (mojo->values.size() >
      blink::kMaxAttributionAggregatableKeysPerSourceOrTrigger) {
    return absl::nullopt;
  }

  bool is_valid = base::ranges::all_of(mojo->values, [](const auto& value) {
    return value.first.size() <=
               blink::kMaxBytesPerAttributionAggregatableKeyId &&
           value.second > 0;
  });
  if (!is_valid)
    return absl::nullopt;

  return AttributionAggregatableValues(std::move(mojo->values));
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
