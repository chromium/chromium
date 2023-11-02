// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_BUDGET_KEY_H_
#define CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_BUDGET_KEY_H_

#include "base/time/time.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace content {

// Represents all information needed to record the budget usage against the
// right counter. Note that the budget limits are enforced against not per-key,
// but per-origin per-day per-API. That is, they are enforced against a set of
// budget keys with contiguous time windows spanning one 24-hour period (and
// identical `origin` and `api` fields). See
// `PrivateAggregationBudgeter::kBudgetScopeDuration`.
class CONTENT_EXPORT PrivateAggregationBudgetKey {
 public:
  enum class Api { kFledge, kSharedStorage };

  // Represents a period of time for which budget usage is recorded. This
  // interval includes the `start_time()` instant but excludes the end time
  // (`start_time() + kDuration`) instant. (But note the `base::Time::Min()`
  // `start_time()` caveat below.) No instant is included in multiple time
  // windows.
  class CONTENT_EXPORT TimeWindow {
   public:
    static constexpr base::TimeDelta kDuration = base::Hours(1);

    // Constructs the window that the `api_invocation_time` lies within.
    // `base::Time::Max()` is disallowed.
    explicit TimeWindow(base::Time api_invocation_time);

    TimeWindow(const TimeWindow& other) = default;
    TimeWindow& operator=(const TimeWindow& other) = default;

    base::Time start_time() const { return start_time_; }

   private:
    // Must be 'on the hour' in UTC, or `base::Time::Min()` for the window that
    // includes `base::Time::Min()` (as its start time cannot be represented.)
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

  // Returns `absl::nullopt` if `origin` is not potentially trustworthy.
  static absl::optional<PrivateAggregationBudgetKey>
  Create(url::Origin origin, base::Time api_invocation_time, Api api);

  // Skips validity checks
  static PrivateAggregationBudgetKey
  CreateForTesting(url::Origin origin, base::Time api_invocation_time, Api api);

  const url::Origin& origin() const { return origin_; }
  TimeWindow time_window() const { return time_window_; }
  Api api() const { return api_; }

 private:
  PrivateAggregationBudgetKey(url::Origin origin,
                              base::Time api_invocation_time,
                              Api api);

  // `origin_` must be potentially trustworthy.
  url::Origin origin_;
  TimeWindow time_window_;
  Api api_;

  // When adding new members, the corresponding `operator==()` definition in
  // `private_aggregation_test_utils.h` should also be updated.
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_BUDGET_KEY_H_
