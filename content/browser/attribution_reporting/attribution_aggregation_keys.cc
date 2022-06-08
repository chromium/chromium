// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_aggregation_keys.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/ranges/algorithm.h"
#include "content/browser/attribution_reporting/attribution_reporting.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/attribution_reporting/constants.h"

namespace content {

// static
absl::optional<AttributionAggregationKeys> AttributionAggregationKeys::FromKeys(
    Keys keys) {
  bool is_valid =
      keys.size() <= blink::kMaxAttributionAggregationKeysPerSourceOrTrigger &&
      base::ranges::all_of(keys, [](const auto& key) {
        return key.first.size() <=
               blink::kMaxBytesPerAttributionAggregationKeyId;
      });
  return is_valid
             ? absl::make_optional(AttributionAggregationKeys(std::move(keys)))
             : absl::nullopt;
}

// static
absl::optional<AttributionAggregationKeys>
AttributionAggregationKeys::Deserialize(const std::string& str) {
  proto::AttributionAggregatableSource msg;
  if (!msg.ParseFromString(str))
    return absl::nullopt;

  Keys::container_type keys;
  keys.reserve(msg.keys().size());

  for (const auto& [id, key] : msg.keys()) {
    if (!key.has_high_bits() || !key.has_low_bits())
      return absl::nullopt;

    keys.emplace_back(id, absl::MakeUint128(key.high_bits(), key.low_bits()));
  }

  return FromKeys(std::move(keys));
}

AttributionAggregationKeys::AttributionAggregationKeys(Keys keys)
    : keys_(std::move(keys)) {}

AttributionAggregationKeys::AttributionAggregationKeys() = default;

AttributionAggregationKeys::~AttributionAggregationKeys() = default;

AttributionAggregationKeys::AttributionAggregationKeys(
    const AttributionAggregationKeys&) = default;

AttributionAggregationKeys::AttributionAggregationKeys(
    AttributionAggregationKeys&&) = default;

AttributionAggregationKeys& AttributionAggregationKeys::operator=(
    const AttributionAggregationKeys&) = default;

AttributionAggregationKeys& AttributionAggregationKeys::operator=(
    AttributionAggregationKeys&&) = default;

std::string AttributionAggregationKeys::Serialize() const {
  proto::AttributionAggregatableSource msg;

  for (const auto& [id, key] : keys_) {
    proto::AttributionAggregationKey key_msg;
    key_msg.set_high_bits(absl::Uint128High64(key));
    key_msg.set_low_bits(absl::Uint128Low64(key));
    (*msg.mutable_keys())[id] = std::move(key_msg);
  }

  std::string str;
  bool success = msg.SerializeToString(&str);
  DCHECK(success);
  return str;
}

}  // namespace content
