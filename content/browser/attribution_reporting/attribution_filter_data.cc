// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_filter_data.h"

#include <iterator>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "content/browser/attribution_reporting/attribution_reporting.pb.h"
#include "third_party/blink/public/common/attribution_reporting/constants.h"

namespace content {

// static
absl::optional<AttributionFilterData> AttributionFilterData::Deserialize(
    const std::string& string) {
  proto::AttributionFilterData msg;
  if (!msg.ParseFromString(string))
    return absl::nullopt;

  FilterValues::container_type filter_values;
  filter_values.reserve(msg.filter_values().size());

  for (google::protobuf::MapPair<std::string, proto::AttributionFilterValues>&
           entry : *msg.mutable_filter_values()) {
    google::protobuf::RepeatedPtrField<std::string>* values =
        entry.second.mutable_values();

    filter_values.emplace_back(
        entry.first,
        std::vector<std::string>(std::make_move_iterator(values->begin()),
                                 std::make_move_iterator(values->end())));
  }

  return AttributionFilterData::FromFilterValues(std::move(filter_values));
}

// static
absl::optional<AttributionFilterData> AttributionFilterData::FromFilterValues(
    FilterValues&& filter_values) {
  if (filter_values.size() > blink::kMaxAttributionFiltersPerSource)
    return absl::nullopt;

  for (const auto& [filter, values] : filter_values) {
    if (filter.size() > blink::kMaxBytesPerAttributionFilterString)
      return absl::nullopt;

    if (values.size() > blink::kMaxValuesPerAttributionFilter)
      return absl::nullopt;

    for (const auto& value : values) {
      if (value.size() > blink::kMaxBytesPerAttributionFilterString)
        return absl::nullopt;
    }
  }

  return AttributionFilterData(std::move(filter_values));
}

AttributionFilterData::AttributionFilterData() = default;

AttributionFilterData::AttributionFilterData(FilterValues filter_values)
    : filter_values_(std::move(filter_values)) {}

AttributionFilterData::~AttributionFilterData() = default;

AttributionFilterData::AttributionFilterData(const AttributionFilterData&) =
    default;

AttributionFilterData::AttributionFilterData(AttributionFilterData&&) = default;

AttributionFilterData& AttributionFilterData::operator=(
    const AttributionFilterData&) = default;

AttributionFilterData& AttributionFilterData::operator=(
    AttributionFilterData&&) = default;

std::string AttributionFilterData::Serialize() const {
  proto::AttributionFilterData msg;

  for (const auto& [filter, values] : filter_values_) {
    proto::AttributionFilterValues filter_values_msg;
    filter_values_msg.mutable_values()->Reserve(values.size());
    for (std::string value : values) {
      filter_values_msg.mutable_values()->Add(std::move(value));
    }
    (*msg.mutable_filter_values())[filter] = std::move(filter_values_msg);
  }

  std::string string;
  bool success = msg.SerializeToString(&string);
  DCHECK(success);
  return string;
}

}  // namespace content
