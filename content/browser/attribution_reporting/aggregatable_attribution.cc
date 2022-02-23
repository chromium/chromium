// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/aggregatable_attribution.h"

#include <stdint.h>

#include <utility>

#include "base/check_op.h"
#include "base/numerics/checked_math.h"

namespace content {

AggregatableHistogramContribution::AggregatableHistogramContribution(
    absl::uint128 key,
    uint32_t value)
    : key_(key), value_(value) {
  DCHECK_GT(value, 0u);
}

AggregatableAttribution::AggregatableAttribution(
    StoredSource::Id source_id,
    base::Time trigger_time,
    base::Time report_time,
    std::vector<AggregatableHistogramContribution> contributions)
    : source_id(source_id),
      trigger_time(trigger_time),
      report_time(report_time),
      contributions(std::move(contributions)) {}

AggregatableAttribution::AggregatableAttribution(
    const AggregatableAttribution& other) = default;

AggregatableAttribution& AggregatableAttribution::operator=(
    const AggregatableAttribution& other) = default;

AggregatableAttribution::AggregatableAttribution(
    AggregatableAttribution&& other) = default;

AggregatableAttribution& AggregatableAttribution::operator=(
    AggregatableAttribution&& other) = default;

AggregatableAttribution::~AggregatableAttribution() = default;

base::CheckedNumeric<int64_t> AggregatableAttribution::BudgetRequired() const {
  base::CheckedNumeric<int64_t> budget_required = 0;
  for (const AggregatableHistogramContribution& contribution : contributions) {
    budget_required += contribution.value();
  }
  return budget_required;
}

}  // namespace content
