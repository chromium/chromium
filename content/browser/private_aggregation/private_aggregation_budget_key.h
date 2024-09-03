// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_BUDGET_KEY_H_
#define CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_BUDGET_KEY_H_

#include <optional>

#include "base/time/time.h"
#include "content/browser/private_aggregation/private_aggregation_caller_api.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

// Represents all information needed to record the budget usage against the
// right counter. Note that the budget limits are not enforced per-key, but
// instead per-site per-API per-10 min and per-site per-API per-day. That is,
// they are enforced against two sets of budget keys with contiguous time
// windows  -- one spanning a 10 min period and one spanning a 24 hour period
// (both with identical `site` and `api` fields). See
// `PrivateAggregationBudgeter::kSmallerScopeValues.budget_scope_duration` and
// `PrivateAggregationBudgeter::kLargerScopeValues.budget_scope_duration`.
class CONTENT_EXPORT PrivateAggregationBudgetKey {
 public:
  // Represents the smallest period of time for which budget usage is recorded.
  // This interval includes the `start_time()` instant, but excludes the end
  // time (`start_time() + kDuration`) instant. (But note the
  // `base::Time::Min()` start time caveat below.) No instant is included in
  // multiple time windows.
  class CONTENT_EXPORT TimeWindow {
   public:
    static constexpr base::TimeDelta kDuration = base::Minutes(1);

    // Constructs the window that the `api_invocation_time` lies within.
    // `base::Time::Max()` is disallowed.
    explicit TimeWindow(base::Time api_invocation_time);

    TimeWindow(const TimeWindow& other) = default;
    TimeWindow& operator=(const TimeWindow& other) = default;

    base::Time start_time() const { return start_time_; }

   private:
    // Must be 'on the minute' in UTC, or `base::Time::Min()` for the window
    // that includes `base::Time::Min()` (as its start time cannot be
    // represented).
    base::Time start_time_;

    // When adding new members, the corresponding `operator==()` definition in
    // `private_aggregation_test_utils.h` should also be updated.
  };

  // Copyable and movable.
  PrivateAggregationBudgetKey(const PrivateAggregationBudgetKey&) = default;
  PrivateAggregationBudgetKey& operator=(const PrivateAggregationBudgetKey&) =
      default;
  PrivateAggregationBudgetKey(PrivateAggregationBudgetKey&& other) = default;
  PrivateAggregationBudgetKey& operator=(PrivateAggregationBudgetKey&& other) =
      default;

  // Returns `std::nullopt` if `origin` is not potentially trustworthy.
  static std::optional<PrivateAggregationBudgetKey> Create(
      url::Origin origin,
      base::Time api_invocation_time,
      PrivateAggregationCallerApi api);

  // Skips validity checks
  static PrivateAggregationBudgetKey CreateForTesting(
      url::Origin origin,
      base::Time api_invocation_time,
      PrivateAggregationCallerApi api);

  const url::Origin& origin() const { return origin_; }
  TimeWindow time_window() const { return time_window_; }
  PrivateAggregationCallerApi api() const { return api_; }

 private:
  PrivateAggregationBudgetKey(url::Origin origin,
                              base::Time api_invocation_time,
                              PrivateAggregationCallerApi api);

  // `origin_` must be potentially trustworthy. Even though the budget is scoped
  // per-site, we store the origin to support deleting the data by origin later.
  url::Origin origin_;
  TimeWindow time_window_;
  PrivateAggregationCallerApi api_;

  // When adding new members, the corresponding `operator==()` definition in
  // `private_aggregation_test_utils.h` should also be updated.
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_BUDGET_KEY_H_
