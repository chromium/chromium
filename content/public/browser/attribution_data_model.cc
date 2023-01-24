// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/attribution_data_model.h"

#include <tuple>
#include <utility>

#include "base/check.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace content {

AttributionDataModel::DataKey::DataKey(
    url::Origin reporting_origin,
    absl::optional<url::Origin> source_origin,
    absl::optional<url::Origin> destination_origin)
    : reporting_origin_(std::move(reporting_origin)),
      source_origin_(std::move(source_origin)),
      destination_origin_(std::move(destination_origin)) {
  DCHECK(!reporting_origin_.opaque());

  DCHECK(!source_origin_.has_value() || !source_origin_->opaque());

  DCHECK(!destination_origin_.has_value() || !destination_origin_->opaque());
}

AttributionDataModel::DataKey::DataKey(const DataKey&) = default;

AttributionDataModel::DataKey::DataKey(DataKey&&) = default;

AttributionDataModel::DataKey& AttributionDataModel::DataKey::operator=(
    const DataKey&) = default;

AttributionDataModel::DataKey& AttributionDataModel::DataKey::operator=(
    DataKey&&) = default;

AttributionDataModel::DataKey::~DataKey() = default;

bool AttributionDataModel::DataKey::operator<(
    const AttributionDataModel::DataKey& other) const {
  return std::tie(reporting_origin_, source_origin_, destination_origin_) <
         std::tie(other.reporting_origin_, other.source_origin_,
                  other.destination_origin_);
}

}  // namespace content