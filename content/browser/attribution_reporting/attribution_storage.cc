// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage.h"

#include "content/browser/attribution_reporting/attribution_observer_types.h"

namespace content {

namespace {
using StoreSourceResult = ::content::AttributionStorage::StoreSourceResult;
}  // namespace

StoreSourceResult::StoreSourceResult(
    StorableSource::Result status,
    absl::optional<base::Time> min_fake_report_time)
    : status(status), min_fake_report_time(min_fake_report_time) {}

StoreSourceResult::~StoreSourceResult() = default;

StoreSourceResult::StoreSourceResult(const StoreSourceResult&) = default;

StoreSourceResult::StoreSourceResult(StoreSourceResult&&) = default;

StoreSourceResult& StoreSourceResult::operator=(const StoreSourceResult&) =
    default;

StoreSourceResult& StoreSourceResult::operator=(StoreSourceResult&&) = default;

}  // namespace content
