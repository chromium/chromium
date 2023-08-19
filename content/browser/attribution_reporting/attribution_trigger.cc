// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_trigger.h"

#include <utility>
#include <vector>

#include "components/attribution_reporting/suitable_origin.h"
#include "services/network/public/cpp/trigger_verification.h"

namespace content {

AttributionTrigger::AttributionTrigger(
    attribution_reporting::SuitableOrigin reporting_origin,
    attribution_reporting::TriggerRegistration registration,
    attribution_reporting::SuitableOrigin destination_origin,
    std::vector<network::TriggerVerification> verifications,
    bool is_within_fenced_frame)
    : reporting_origin_(std::move(reporting_origin)),
      registration_(std::move(registration)),
      destination_origin_(std::move(destination_origin)),
      verifications_(std::move(verifications)),
      is_within_fenced_frame_(is_within_fenced_frame) {}

AttributionTrigger::AttributionTrigger(const AttributionTrigger&) = default;

AttributionTrigger& AttributionTrigger::operator=(const AttributionTrigger&) =
    default;

AttributionTrigger::AttributionTrigger(AttributionTrigger&&) = default;

AttributionTrigger& AttributionTrigger::operator=(AttributionTrigger&&) =
    default;

AttributionTrigger::~AttributionTrigger() = default;

}  // namespace content
