// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_trigger.h"

#include <utility>

#include "base/check.h"

namespace content {

AttributionTrigger::EventTriggerData::EventTriggerData(
    uint64_t data,
    int64_t priority,
    absl::optional<uint64_t> dedup_key,
    CommonSourceInfo::SourceType source_type)
    : data(data),
      priority(priority),
      dedup_key(dedup_key),
      source_type(source_type) {}

AttributionTrigger::AttributionTrigger(
    net::SchemefulSite conversion_destination,
    url::Origin reporting_origin,
    absl::optional<uint64_t> debug_key,
    std::vector<EventTriggerData> event_triggers)
    : conversion_destination_(std::move(conversion_destination)),
      reporting_origin_(std::move(reporting_origin)),
      debug_key_(debug_key),
      event_triggers_(std::move(event_triggers)) {
  DCHECK(!reporting_origin_.opaque());
  DCHECK(!conversion_destination_.opaque());
}

AttributionTrigger::AttributionTrigger(
    uint64_t trigger_data,
    net::SchemefulSite conversion_destination,
    url::Origin reporting_origin,
    uint64_t event_source_trigger_data,
    int64_t priority,
    absl::optional<uint64_t> dedup_key,
    absl::optional<uint64_t> debug_key)
    : AttributionTrigger(
          std::move(conversion_destination),
          std::move(reporting_origin),
          debug_key,
          std::vector<EventTriggerData>(
              {EventTriggerData(trigger_data,
                                priority,
                                dedup_key,
                                CommonSourceInfo::SourceType::kNavigation),
               EventTriggerData(event_source_trigger_data,
                                priority,
                                dedup_key,
                                CommonSourceInfo::SourceType::kEvent)})) {}

AttributionTrigger::AttributionTrigger(const AttributionTrigger& other) =
    default;

AttributionTrigger& AttributionTrigger::operator=(
    const AttributionTrigger& other) = default;

AttributionTrigger::AttributionTrigger(AttributionTrigger&& other) = default;

AttributionTrigger& AttributionTrigger::operator=(AttributionTrigger&& other) =
    default;

AttributionTrigger::~AttributionTrigger() = default;

}  // namespace content
