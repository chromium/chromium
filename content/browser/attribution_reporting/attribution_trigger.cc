// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_trigger.h"

#include <utility>

#include "base/check.h"

namespace content {

AttributionTrigger::AttributionTrigger(
    uint64_t trigger_data,
    net::SchemefulSite conversion_destination,
    url::Origin reporting_origin,
    uint64_t event_source_trigger_data,
    int64_t priority,
    absl::optional<uint64_t> dedup_key,
    absl::optional<uint64_t> debug_key)
    : trigger_data_(trigger_data),
      conversion_destination_(std::move(conversion_destination)),
      reporting_origin_(std::move(reporting_origin)),
      event_source_trigger_data_(event_source_trigger_data),
      priority_(priority),
      dedup_key_(dedup_key),
      debug_key_(debug_key) {
  DCHECK(!reporting_origin_.opaque());
  DCHECK(!conversion_destination_.opaque());
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
