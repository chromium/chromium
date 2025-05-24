// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_AGGREGATABLE_NAMED_BUDGET_PAIR_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_AGGREGATABLE_NAMED_BUDGET_PAIR_H_

#include <optional>

#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT AggregatableNamedBudgetPair {
 public:
  static std::optional<AggregatableNamedBudgetPair> Create(
      int original_budget,
      int remaining_budget);

  // Returns false if `budget_consumed` is an invalid value.
  [[nodiscard]] bool SubtractRemainingBudget(int budget_consumed);

  int original_budget() const { return original_budget_; }

  int remaining_budget() const { return remaining_budget_; }

  friend bool operator==(const AggregatableNamedBudgetPair&,
                         const AggregatableNamedBudgetPair&) = default;

 private:
  AggregatableNamedBudgetPair(int original_budget, int remaining_budget);

  int original_budget_;
  int remaining_budget_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_AGGREGATABLE_NAMED_BUDGET_PAIR_H_
