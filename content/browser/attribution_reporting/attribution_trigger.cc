// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_trigger.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"

namespace content {

AttributionTrigger::EventTriggerData::EventTriggerData(
    uint64_t data,
    int64_t priority,
    absl::optional<uint64_t> dedup_key,
    AttributionFilterData filters,
    AttributionFilterData not_filters)
    : data(data),
      priority(priority),
      dedup_key(dedup_key),
      filters(std::move(filters)),
      not_filters(std::move(not_filters)) {}

AttributionTrigger::AttributionTrigger(
    url::Origin destination_origin,
    url::Origin reporting_origin,
    AttributionFilterData filters,
    absl::optional<uint64_t> debug_key,
    std::vector<EventTriggerData> event_triggers,
    AttributionAggregatableTrigger aggregatable_trigger)
    : destination_origin_(std::move(destination_origin)),
      reporting_origin_(std::move(reporting_origin)),
      filters_(std::move(filters)),
      debug_key_(debug_key),
      event_triggers_(std::move(event_triggers)),
      aggregatable_trigger_(std::move(aggregatable_trigger)) {
  DCHECK(network::IsOriginPotentiallyTrustworthy(reporting_origin_));
  DCHECK(network::IsOriginPotentiallyTrustworthy(destination_origin_));
}

AttributionTrigger::AttributionTrigger(const AttributionTrigger& other) =
    default;

AttributionTrigger& AttributionTrigger::operator=(
    const AttributionTrigger& other) = default;

AttributionTrigger::AttributionTrigger(AttributionTrigger&& other) = default;

AttributionTrigger& AttributionTrigger::operator=(AttributionTrigger&& other) =
    default;

AttributionTrigger::~AttributionTrigger() = default;

}  // namespace content
