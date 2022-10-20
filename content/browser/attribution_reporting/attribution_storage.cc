// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage.h"

#include "base/check.h"
#include "content/browser/attribution_reporting/attribution_observer_types.h"
#include "content/browser/attribution_reporting/storable_source.h"

namespace content {

namespace {
using StoreSourceResult = ::content::AttributionStorage::StoreSourceResult;
}  // namespace

StoreSourceResult::StoreSourceResult(
    StorableSource::Result status,
    absl::optional<base::Time> min_fake_report_time,
    absl::optional<int> max_destinations_per_source_site_reporting_origin)
    : status(status),
      min_fake_report_time(min_fake_report_time),
      max_destinations_per_source_site_reporting_origin(
          max_destinations_per_source_site_reporting_origin) {
  DCHECK(!max_destinations_per_source_site_reporting_origin.has_value() ||
         status ==
             StorableSource::Result::kInsufficientUniqueDestinationCapacity);
}

StoreSourceResult::~StoreSourceResult() = default;

StoreSourceResult::StoreSourceResult(const StoreSourceResult&) = default;

StoreSourceResult::StoreSourceResult(StoreSourceResult&&) = default;

StoreSourceResult& StoreSourceResult::operator=(const StoreSourceResult&) =
    default;

StoreSourceResult& StoreSourceResult::operator=(StoreSourceResult&&) = default;

}  // namespace content
