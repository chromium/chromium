// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/aggregatable_named_budget_pair.h"

#include <optional>

#include "base/check.h"
#include "base/numerics/checked_math.h"
#include "components/attribution_reporting/aggregatable_utils.h"

namespace content {

namespace {

bool IsValid(int original_budget, int remaining_budget) {
  return attribution_reporting::IsAggregatableBudgetInRange(original_budget) &&
         attribution_reporting::IsAggregatableBudgetInRange(remaining_budget) &&
         remaining_budget <= original_budget;
}

}  // namespace

// static
std::optional<AggregatableNamedBudgetPair> AggregatableNamedBudgetPair::Create(
    int original_budget,
    int remaining_budget) {
  if (!IsValid(original_budget, remaining_budget)) {
    return std::nullopt;
  }
  return AggregatableNamedBudgetPair(original_budget, remaining_budget);
}

bool AggregatableNamedBudgetPair::SubtractRemainingBudget(int budget_consumed) {
  return budget_consumed >= 0 && budget_consumed <= remaining_budget_ &&
         base::CheckSub(remaining_budget_, budget_consumed)
             .AssignIfValid(&remaining_budget_);
}

AggregatableNamedBudgetPair::AggregatableNamedBudgetPair(int original_budget,
                                                         int remaining_budget)
    : original_budget_(original_budget), remaining_budget_(remaining_budget) {
  CHECK(IsValid(original_budget_, remaining_budget_));
}

}  // namespace content
