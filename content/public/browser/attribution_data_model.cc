// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/attribution_data_model.h"

#include <utility>

#include "base/check.h"
#include "url/origin.h"

namespace content {

AttributionDataModel::DataKey::DataKey(url::Origin reporting_origin)
    : reporting_origin_(std::move(reporting_origin)) {
  DCHECK(!reporting_origin_.opaque());
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
  return reporting_origin_ < other.reporting_origin_;
}

bool AttributionDataModel::DataKey::operator==(
    const AttributionDataModel::DataKey& other) const {
  return reporting_origin_ == other.reporting_origin_;
}

}  // namespace content