// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/attribution_data_model.h"

#include <tuple>
#include <utility>

#include "base/check.h"
#include "url/origin.h"

namespace content {

AttributionDataModel::DataKey::DataKey(url::Origin reporting_origin,
                                       url::Origin context_origin,
                                       Scope scope)
    : reporting_origin_(std::move(reporting_origin)),
      context_origin_(std::move(context_origin)),
      scope_(scope) {
  DCHECK(!reporting_origin_.opaque());
  DCHECK(!context_origin_.opaque());
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
  return std::tie(reporting_origin_, context_origin_, scope_) <
         std::tie(other.reporting_origin_, other.context_origin_, other.scope_);
}

bool AttributionDataModel::DataKey::operator==(
    const AttributionDataModel::DataKey& other) const {
  return std::tie(reporting_origin_, context_origin_, scope_) ==
         std::tie(other.reporting_origin_, other.context_origin_, other.scope_);
}

}  // namespace content