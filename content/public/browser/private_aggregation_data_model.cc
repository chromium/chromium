// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/private_aggregation_data_model.h"

#include <utility>

#include "base/check.h"
#include "url/origin.h"

namespace content {

PrivateAggregationDataModel::DataKey::DataKey(url::Origin reporting_origin)
    : reporting_origin_(std::move(reporting_origin)) {
  CHECK(!reporting_origin_.opaque());
}

PrivateAggregationDataModel::DataKey::DataKey(const DataKey&) = default;

PrivateAggregationDataModel::DataKey::DataKey(DataKey&&) = default;

PrivateAggregationDataModel::DataKey&
PrivateAggregationDataModel::DataKey::operator=(const DataKey&) = default;

PrivateAggregationDataModel::DataKey&
PrivateAggregationDataModel::DataKey::operator=(DataKey&&) = default;

PrivateAggregationDataModel::DataKey::~DataKey() = default;

bool PrivateAggregationDataModel::DataKey::operator<(
    const PrivateAggregationDataModel::DataKey& other) const {
  return reporting_origin_ < other.reporting_origin_;
}

bool PrivateAggregationDataModel::DataKey::operator==(
    const PrivateAggregationDataModel::DataKey& other) const {
  return reporting_origin_ == other.reporting_origin_;
}

}  // namespace content
