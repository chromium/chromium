// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_aggregatable_sources.h"

#include <utility>

#include "base/ranges/algorithm.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/attribution_reporting/constants.h"

namespace content {

// static
absl::optional<AttributionAggregatableSources>
AttributionAggregatableSources::Create(
    proto::AttributionAggregatableSources proto) {
  bool is_valid =
      proto.sources().size() <=
          blink::kMaxAttributionAggregatableKeysPerSourceOrTrigger &&
      base::ranges::all_of(proto.sources(), [](const auto& source) {
        return source.first.size() <=
                   blink::kMaxBytesPerAttributionAggregatableKeyId &&
               source.second.has_high_bits() && source.second.has_low_bits();
      });
  return is_valid ? absl::make_optional(
                        AttributionAggregatableSources(std::move(proto)))
                  : absl::nullopt;
}

AttributionAggregatableSources::AttributionAggregatableSources(
    proto::AttributionAggregatableSources proto)
    : proto_(std::move(proto)) {}

AttributionAggregatableSources::AttributionAggregatableSources() = default;

AttributionAggregatableSources::~AttributionAggregatableSources() = default;

AttributionAggregatableSources::AttributionAggregatableSources(
    const AttributionAggregatableSources&) = default;

AttributionAggregatableSources::AttributionAggregatableSources(
    AttributionAggregatableSources&&) = default;

AttributionAggregatableSources& AttributionAggregatableSources::operator=(
    const AttributionAggregatableSources&) = default;

AttributionAggregatableSources& AttributionAggregatableSources::operator=(
    AttributionAggregatableSources&&) = default;

}  // namespace content
