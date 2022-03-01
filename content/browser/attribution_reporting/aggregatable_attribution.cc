// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/aggregatable_attribution.h"

#include <stdint.h>

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/numerics/checked_math.h"
#include "base/ranges/algorithm.h"

namespace content {

AggregatableHistogramContribution::AggregatableHistogramContribution(
    absl::uint128 key,
    uint32_t value)
    : key_(key), value_(value) {
  DCHECK_GT(value, 0u);
}

// static
AggregatableAttribution AggregatableAttribution::CreateForTesting(
    AttributionInfo attribution_info,
    base::Time report_time,
    std::vector<AggregatableHistogramContribution> contributions,
    std::vector<base::GUID> external_report_ids) {
  DCHECK_EQ(contributions.size(), external_report_ids.size());

  std::vector<ContributionAndExternalId> contributions_and_ids;
  for (size_t i = 0u; i < contributions.size(); ++i) {
    contributions_and_ids.push_back(ContributionAndExternalId{
        .contribution = std::move(contributions[i]),
        .external_report_id = std::move(external_report_ids[i])});
  }

  return AggregatableAttribution(std::move(attribution_info), report_time,
                                 std::move(contributions_and_ids));
}

AggregatableAttribution::AggregatableAttribution(
    AttributionInfo attribution_info,
    base::Time report_time,
    std::vector<ContributionAndExternalId> contributions_and_ids)
    : attribution_info_(std::move(attribution_info)),
      report_time_(report_time),
      contributions_and_ids_(std::move(contributions_and_ids)) {
  DCHECK(base::ranges::all_of(
      contributions_and_ids_, [](const auto& contribution_and_id) {
        return contribution_and_id.external_report_id.is_valid();
      }));
}

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
  for (const ContributionAndExternalId& contribution_and_id :
       contributions_and_ids_) {
    budget_required += contribution_and_id.contribution.value();
  }
  return budget_required;
}

}  // namespace content
