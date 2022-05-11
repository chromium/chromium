// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage.h"

#include <utility>

#include "content/browser/attribution_reporting/attribution_observer_types.h"

namespace content {

namespace {
using StoreSourceResult = ::content::AttributionStorage::StoreSourceResult;
}  // namespace

StoreSourceResult::StoreSourceResult(
    StorableSource::Result status,
    std::vector<StoredSource> deactivated_sources,
    absl::optional<base::Time> min_fake_report_time)
    : status(status),
      deactivated_sources(std::move(deactivated_sources)),
      min_fake_report_time(min_fake_report_time) {}

StoreSourceResult::~StoreSourceResult() = default;

StoreSourceResult::StoreSourceResult(const StoreSourceResult&) = default;

StoreSourceResult::StoreSourceResult(StoreSourceResult&&) = default;

StoreSourceResult& StoreSourceResult::operator=(const StoreSourceResult&) =
    default;

StoreSourceResult& StoreSourceResult::operator=(StoreSourceResult&&) = default;

}  // namespace content
