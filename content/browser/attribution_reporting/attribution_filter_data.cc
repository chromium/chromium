// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_filter_data.h"

#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/values.h"
#include "content/browser/attribution_reporting/attribution_reporting.pb.h"
#include "third_party/blink/public/common/attribution_reporting/constants.h"

namespace content {

namespace {

constexpr char kFilterSourceType[] = "source_type";

}  // namespace

// static
absl::optional<AttributionFilterData>
AttributionFilterData::DeserializeSourceFilterData(
    const std::string& string,
    AttributionSourceType source_type) {
  proto::AttributionFilterData msg;
  if (!msg.ParseFromString(string))
    return absl::nullopt;

  FilterValues::container_type filter_values;
  filter_values.reserve(msg.filter_values().size() + 1);

  // Add the auto-generated filter first so that it is retained instead of any
  // existing filter of the same name in `msg`, which should only be possible
  // with database corruption (extremely unlikely) or deliberate modification of
  // the DB. This approach works because `base::flat_map` uses a stable
  // sort/unique when being constructed from an existing container.
  filter_values.emplace_back(
      kFilterSourceType,
      std::vector<std::string>{AttributionSourceTypeToString(source_type)});

  for (google::protobuf::MapPair<std::string, proto::AttributionFilterValues>&
           entry : *msg.mutable_filter_values()) {
    google::protobuf::RepeatedPtrField<std::string>* values =
        entry.second.mutable_values();

    filter_values.emplace_back(
        entry.first,
        std::vector<std::string>(std::make_move_iterator(values->begin()),
                                 std::make_move_iterator(values->end())));
  }

  return FromFilterValues(std::move(filter_values),
                          /*extra_filters_allowed=*/1);
}

// static
absl::optional<AttributionFilterData>
AttributionFilterData::FromSourceFilterValues(FilterValues&& filter_values) {
  absl::optional<AttributionFilterData> result =
      FromFilterValues(std::move(filter_values),
                       /*extra_filters_allowed=*/0);

  if (!result || result->filter_values_.contains(kFilterSourceType))
    return absl::nullopt;

  return result;
}

// static
absl::optional<AttributionFilterData>
AttributionFilterData::FromTriggerFilterValues(FilterValues&& filter_values) {
  return FromFilterValues(std::move(filter_values),
                          /*extra_filters_allowed=*/0);
}

// static
absl::optional<AttributionFilterData> AttributionFilterData::FromSourceJSON(
    base::Value* input_value) {
  // TODO(johnidel): Consider logging registration JSON metrics here.
  if (!input_value)
    return AttributionFilterData();

  base::Value::Dict* dict = input_value->GetIfDict();
  if (!dict)
    return absl::nullopt;

  const size_t num_filters = dict->size();
  if (num_filters > blink::kMaxAttributionFiltersPerSource)
    return absl::nullopt;

  if (dict->contains(kFilterSourceType))
    return absl::nullopt;

  FilterValues::container_type filter_values;
  filter_values.reserve(dict->size());

  for (auto [filter, value] : *dict) {
    if (filter.size() > blink::kMaxBytesPerAttributionFilterString)
      return absl::nullopt;

    base::Value::List* list = value.GetIfList();
    if (!list)
      return absl::nullopt;

    const size_t num_values = list->size();
    if (num_values > blink::kMaxValuesPerAttributionFilter)
      return absl::nullopt;

    std::vector<std::string> values;
    values.reserve(num_values);

    for (base::Value& item : *list) {
      std::string* string = item.GetIfString();
      if (!string)
        return absl::nullopt;

      if (string->size() > blink::kMaxBytesPerAttributionFilterString)
        return absl::nullopt;

      values.push_back(std::move(*string));
    }

    filter_values.emplace_back(filter, std::move(values));
  }

  return AttributionFilterData(
      FilterValues(base::sorted_unique, std::move(filter_values)));
}

// static
AttributionFilterData AttributionFilterData::ForSourceType(
    AttributionSourceType source_type) {
  std::vector<std::string> values;
  values.reserve(1);
  values.push_back(AttributionSourceTypeToString(source_type));

  AttributionFilterData::FilterValues filter_values;
  filter_values.reserve(1);
  filter_values.emplace(kFilterSourceType, std::move(values));

  return AttributionFilterData(std::move(filter_values));
}

// static
absl::optional<AttributionFilterData> AttributionFilterData::FromFilterValues(
    FilterValues&& filter_values,
    size_t extra_filters_allowed) {
  if (filter_values.size() >
      blink::kMaxAttributionFiltersPerSource + extra_filters_allowed) {
    return absl::nullopt;
  }

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

// static
AttributionFilterData AttributionFilterData::CreateForTesting(
    FilterValues filter_values) {
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
  DCHECK(!filter_values_.contains(kFilterSourceType));

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
