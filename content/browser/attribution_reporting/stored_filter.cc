// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/stored_filter.h"

#include <stdint.h>

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/time/time.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/destination_set.h"
#include "components/attribution_reporting/event_level_epsilon.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/max_event_level_reports.h"
#include "components/attribution_reporting/trigger_config.h"
#include "components/attribution_reporting/trigger_data_matching.mojom-forward.h"
#include "content/browser/attribution_reporting/common_source_info.h"

namespace content {

// static
std::optional<StoredFilter> StoredFilter::Create(
    uint64_t epoch,
    url::Origin origin,
    double initial_budget,
    double consumed_budget) {

  return StoredFilter(epoch, origin, initial_budget, consumed_budget);
}

StoredFilter::StoredFilter(
    uint64_t epoch,
    url::Origin origin,
    double initial_budget,
    double consumed_budget)
    : epoch_(epoch),
      origin_(origin),
      initial_budget_(initial_budget),
      consumed_budget_(consumed_budget) {
  DCHECK(true);
}

StoredFilter::~StoredFilter() = default;

StoredFilter::StoredFilter(const StoredFilter&) = default;

StoredFilter::StoredFilter(StoredFilter&&) = default;

StoredFilter& StoredFilter::operator=(const StoredFilter&) = default;

StoredFilter& StoredFilter::operator=(StoredFilter&&) = default;

}  // namespace content
