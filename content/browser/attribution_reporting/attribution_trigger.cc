// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_trigger.h"

#include <utility>

#include "base/check.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
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

AttributionTrigger::AttributionTrigger(
    uint64_t trigger_data,
    url::Origin destination_origin,
    url::Origin reporting_origin,
    uint64_t event_source_trigger_data,
    int64_t priority,
    absl::optional<uint64_t> dedup_key,
    absl::optional<uint64_t> debug_key,
    AttributionAggregatableTrigger aggregatable_trigger)
    : AttributionTrigger(
          std::move(destination_origin),
          std::move(reporting_origin),
          /*filters=*/AttributionFilterData(),
          debug_key,
          std::vector<EventTriggerData>(
              {EventTriggerData(trigger_data,
                                priority,
                                dedup_key,
                                /*filters=*/
                                AttributionFilterData::ForSourceType(
                                    AttributionSourceType::kNavigation),
                                /*not_filters=*/AttributionFilterData()),
               EventTriggerData(event_source_trigger_data,
                                priority,
                                dedup_key,
                                /*filters=*/
                                AttributionFilterData::ForSourceType(
                                    AttributionSourceType::kEvent),
                                /*not_filters=*/AttributionFilterData())}),
          std::move(aggregatable_trigger)) {}

AttributionTrigger::AttributionTrigger(const AttributionTrigger& other) =
    default;

AttributionTrigger& AttributionTrigger::operator=(
    const AttributionTrigger& other) = default;

AttributionTrigger::AttributionTrigger(AttributionTrigger&& other) = default;

AttributionTrigger& AttributionTrigger::operator=(AttributionTrigger&& other) =
    default;

AttributionTrigger::~AttributionTrigger() = default;

}  // namespace content
