// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_aggregatable_source.h"

#include <utility>

#include "base/ranges/algorithm.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/attribution_reporting/constants.h"

namespace content {

// static
absl::optional<AttributionAggregatableSource>
AttributionAggregatableSource::Create(
    proto::AttributionAggregatableSource proto) {
  bool is_valid =
      proto.keys().size() <=
          blink::kMaxAttributionAggregatableKeysPerSourceOrTrigger &&
      base::ranges::all_of(proto.keys(), [](const auto& key) {
        return key.first.size() <=
                   blink::kMaxBytesPerAttributionAggregatableKeyId &&
               key.second.has_high_bits() && key.second.has_low_bits();
      });
  return is_valid ? absl::make_optional(
                        AttributionAggregatableSource(std::move(proto)))
                  : absl::nullopt;
}

AttributionAggregatableSource::AttributionAggregatableSource(
    proto::AttributionAggregatableSource proto)
    : proto_(std::move(proto)) {}

AttributionAggregatableSource::AttributionAggregatableSource() = default;

AttributionAggregatableSource::~AttributionAggregatableSource() = default;

AttributionAggregatableSource::AttributionAggregatableSource(
    const AttributionAggregatableSource&) = default;

AttributionAggregatableSource::AttributionAggregatableSource(
    AttributionAggregatableSource&&) = default;

AttributionAggregatableSource& AttributionAggregatableSource::operator=(
    const AttributionAggregatableSource&) = default;

AttributionAggregatableSource& AttributionAggregatableSource::operator=(
    AttributionAggregatableSource&&) = default;

}  // namespace content
