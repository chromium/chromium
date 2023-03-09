// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/store_source_result.h"

#include "base/check.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/store_source_result.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

StoreSourceResult::StoreSourceResult(
    attribution_reporting::mojom::StoreSourceResult status,
    absl::optional<base::Time> min_fake_report_time,
    absl::optional<int> max_destinations_per_source_site_reporting_origin,
    absl::optional<int> max_sources_per_origin)
    : status(status),
      min_fake_report_time(min_fake_report_time),
      max_destinations_per_source_site_reporting_origin(
          max_destinations_per_source_site_reporting_origin),
      max_sources_per_origin(max_sources_per_origin) {
  DCHECK(!max_destinations_per_source_site_reporting_origin.has_value() ||
         status == attribution_reporting::mojom::StoreSourceResult::
                       kInsufficientUniqueDestinationCapacity);
  DCHECK(!max_sources_per_origin.has_value() ||
         status == attribution_reporting::mojom::StoreSourceResult::
                       kInsufficientSourceCapacity);
}

StoreSourceResult::~StoreSourceResult() = default;

StoreSourceResult::StoreSourceResult(const StoreSourceResult&) = default;

StoreSourceResult::StoreSourceResult(StoreSourceResult&&) = default;

StoreSourceResult& StoreSourceResult::operator=(const StoreSourceResult&) =
    default;

StoreSourceResult& StoreSourceResult::operator=(StoreSourceResult&&) = default;

}  // namespace content
