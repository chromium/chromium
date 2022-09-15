// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_aggregation_keys.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/ranges/algorithm.h"
#include "base/strings/abseil_string_number_conversions.h"
#include "base/values.h"
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
absl::optional<AttributionAggregationKeys> AttributionAggregationKeys::FromJSON(
    const base::Value* value) {
  // TODO(johnidel): Consider logging registration JSON metrics here.
  if (!value)
    return AttributionAggregationKeys();

  const base::Value::Dict* dict = value->GetIfDict();
  if (!dict)
    return absl::nullopt;

  const size_t num_keys = dict->size();

  if (num_keys > blink::kMaxAttributionAggregationKeysPerSourceOrTrigger)
    return absl::nullopt;

  Keys::container_type keys;
  keys.reserve(num_keys);

  for (auto [key_id, maybe_string_value] : *dict) {
    if (key_id.size() > blink::kMaxBytesPerAttributionAggregationKeyId)
      return absl::nullopt;

    const std::string* s = maybe_string_value.GetIfString();
    if (!s)
      return absl::nullopt;

    absl::uint128 key;
    if (!base::HexStringToUInt128(*s, &key))
      return absl::nullopt;

    keys.emplace_back(key_id, key);
  }

  return AttributionAggregationKeys(Keys(base::sorted_unique, std::move(keys)));
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
