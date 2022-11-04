// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_filter_data.h"

#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"

namespace content {

namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;

bool IsValidForSourceOrTrigger(const AttributionFilterValues& filter_values) {
  if (filter_values.size() > attribution_reporting::kMaxFiltersPerSource)
    return false;

  for (const auto& [filter, values] : filter_values) {
    if (filter.size() > attribution_reporting::kMaxBytesPerFilterString)
      return false;

    if (values.size() > attribution_reporting::kMaxValuesPerFilter)
      return false;

    for (const auto& value : values) {
      if (value.size() > attribution_reporting::kMaxBytesPerFilterString)
        return false;
    }
  }

  return true;
}

bool IsValidForSource(const AttributionFilterValues& filter_values) {
  return !filter_values.contains(AttributionFilterData::kSourceTypeFilterKey) &&
         IsValidForSourceOrTrigger(filter_values);
}

}  // namespace

// static
absl::optional<AttributionFilterData> AttributionFilterData::Create(
    AttributionFilterValues filter_values) {
  if (!IsValidForSource(filter_values))
    return absl::nullopt;

  return AttributionFilterData(std::move(filter_values));
}

// static
base::expected<AttributionFilterData, SourceRegistrationError>
AttributionFilterData::FromJSON(base::Value* input_value) {
  // TODO(johnidel): Consider logging registration JSON metrics here.
  if (!input_value)
    return AttributionFilterData();

  base::Value::Dict* dict = input_value->GetIfDict();
  if (!dict)
    return base::unexpected(SourceRegistrationError::kFilterDataWrongType);

  const size_t num_filters = dict->size();
  if (num_filters > attribution_reporting::kMaxFiltersPerSource)
    return base::unexpected(SourceRegistrationError::kFilterDataTooManyKeys);

  if (dict->contains(kSourceTypeFilterKey)) {
    return base::unexpected(
        SourceRegistrationError::kFilterDataHasSourceTypeKey);
  }

  AttributionFilterValues::container_type filter_values;
  filter_values.reserve(dict->size());

  for (auto [filter, value] : *dict) {
    if (filter.size() > attribution_reporting::kMaxBytesPerFilterString)
      return base::unexpected(SourceRegistrationError::kFilterDataKeyTooLong);

    base::Value::List* list = value.GetIfList();
    if (!list) {
      return base::unexpected(
          SourceRegistrationError::kFilterDataListWrongType);
    }

    const size_t num_values = list->size();
    if (num_values > attribution_reporting::kMaxValuesPerFilter)
      return base::unexpected(SourceRegistrationError::kFilterDataListTooLong);

    std::vector<std::string> values;
    values.reserve(num_values);

    for (base::Value& item : *list) {
      std::string* string = item.GetIfString();
      if (!string) {
        return base::unexpected(
            SourceRegistrationError::kFilterDataValueWrongType);
      }

      if (string->size() > attribution_reporting::kMaxBytesPerFilterString) {
        return base::unexpected(
            SourceRegistrationError::kFilterDataValueTooLong);
      }

      values.push_back(std::move(*string));
    }

    filter_values.emplace_back(filter, std::move(values));
  }

  return AttributionFilterData(
      AttributionFilterValues(base::sorted_unique, std::move(filter_values)));
}

AttributionFilterData::AttributionFilterData() = default;

AttributionFilterData::AttributionFilterData(
    AttributionFilterValues filter_values)
    : filter_values_(std::move(filter_values)) {
  DCHECK(IsValidForSource(filter_values_));
}

AttributionFilterData::~AttributionFilterData() = default;

AttributionFilterData::AttributionFilterData(const AttributionFilterData&) =
    default;

AttributionFilterData::AttributionFilterData(AttributionFilterData&&) = default;

AttributionFilterData& AttributionFilterData::operator=(
    const AttributionFilterData&) = default;

AttributionFilterData& AttributionFilterData::operator=(
    AttributionFilterData&&) = default;

// static
absl::optional<AttributionFilters> AttributionFilters::Create(
    AttributionFilterValues filter_values) {
  if (!IsValidForSourceOrTrigger(filter_values))
    return absl::nullopt;

  return AttributionFilters(std::move(filter_values));
}

AttributionFilters::AttributionFilters() = default;

AttributionFilters::AttributionFilters(AttributionFilterValues filter_values)
    : filter_values_(std::move(filter_values)) {
  DCHECK(IsValidForSourceOrTrigger(filter_values_));
}

AttributionFilters::~AttributionFilters() = default;

AttributionFilters::AttributionFilters(const AttributionFilters&) = default;

AttributionFilters::AttributionFilters(AttributionFilters&&) = default;

AttributionFilters& AttributionFilters::operator=(const AttributionFilters&) =
    default;

AttributionFilters& AttributionFilters::operator=(AttributionFilters&&) =
    default;

}  // namespace content
