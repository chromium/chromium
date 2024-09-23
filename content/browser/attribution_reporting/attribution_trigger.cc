// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_trigger.h"

#include <utility>

#include "base/containers/flat_map.h"
#include "base/ranges/algorithm.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/suitable_origin.h"

namespace content {

AttributionTrigger::AttributionTrigger(
    attribution_reporting::SuitableOrigin reporting_origin,
    attribution_reporting::TriggerRegistration registration,
    attribution_reporting::SuitableOrigin destination_origin,
    bool is_within_fenced_frame)
    : reporting_origin_(std::move(reporting_origin)),
      registration_(std::move(registration)),
      destination_origin_(std::move(destination_origin)),
      is_within_fenced_frame_(is_within_fenced_frame) {}

AttributionTrigger::AttributionTrigger(const AttributionTrigger&) = default;

AttributionTrigger& AttributionTrigger::operator=(const AttributionTrigger&) =
    default;

AttributionTrigger::AttributionTrigger(AttributionTrigger&&) = default;

AttributionTrigger& AttributionTrigger::operator=(AttributionTrigger&&) =
    default;

AttributionTrigger::~AttributionTrigger() = default;

bool AttributionTrigger::HasAggregatableData() const {
  return !registration_.aggregatable_trigger_data.empty() ||
         base::ranges::any_of(
             registration_.aggregatable_values,
             [](const attribution_reporting::AggregatableValues& values) {
               return !values.values().empty();
             });
}

}  // namespace content
