// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_trigger.h"

#include <utility>

#include "base/check.h"
#include "components/attribution_reporting/suitable_origin.h"

namespace content {

AttributionTrigger::AttributionTrigger(
    attribution_reporting::TriggerRegistration registration,
    url::Origin destination_origin,
    bool is_within_fenced_frame)
    : registration_(std::move(registration)),
      destination_origin_(std::move(destination_origin)),
      is_within_fenced_frame_(is_within_fenced_frame) {
  DCHECK(
      attribution_reporting::SuitableOrigin::IsSuitable(destination_origin_));
}

AttributionTrigger::AttributionTrigger(const AttributionTrigger&) = default;

AttributionTrigger& AttributionTrigger::operator=(const AttributionTrigger&) =
    default;

AttributionTrigger::AttributionTrigger(AttributionTrigger&&) = default;

AttributionTrigger& AttributionTrigger::operator=(AttributionTrigger&&) =
    default;

AttributionTrigger::~AttributionTrigger() = default;

}  // namespace content
