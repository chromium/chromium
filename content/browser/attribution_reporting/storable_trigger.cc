// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/storable_trigger.h"

#include <utility>

#include "base/check.h"

namespace content {

StorableTrigger::StorableTrigger(uint64_t trigger_data,
                                 net::SchemefulSite conversion_destination,
                                 url::Origin reporting_origin,
                                 uint64_t event_source_trigger_data,
                                 int64_t priority,
                                 absl::optional<int64_t> dedup_key)
    : trigger_data_(trigger_data),
      conversion_destination_(std::move(conversion_destination)),
      reporting_origin_(std::move(reporting_origin)),
      event_source_trigger_data_(event_source_trigger_data),
      priority_(priority),
      dedup_key_(dedup_key) {
  DCHECK(!reporting_origin_.opaque());
  DCHECK(!conversion_destination_.opaque());
}

StorableTrigger::StorableTrigger(const StorableTrigger& other) = default;

StorableTrigger& StorableTrigger::operator=(const StorableTrigger& other) =
    default;

StorableTrigger::StorableTrigger(StorableTrigger&& other) = default;

StorableTrigger& StorableTrigger::operator=(StorableTrigger&& other) = default;

StorableTrigger::~StorableTrigger() = default;

}  // namespace content
