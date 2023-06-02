// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_budget_key.h"

#include "base/check.h"
#include "base/time/time.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace content {

namespace {
base::Time FloorToDuration(base::Time time) {
  // `FloorToMultiple` would no-op on `base::Time::Max()`.
  DCHECK(!time.is_max());

  return base::Time() + time.since_origin().FloorToMultiple(
                            PrivateAggregationBudgetKey::TimeWindow::kDuration);
}
}  // namespace

PrivateAggregationBudgetKey::TimeWindow::TimeWindow(base::Time start_time)
    : start_time_(FloorToDuration(start_time)) {}

PrivateAggregationBudgetKey::PrivateAggregationBudgetKey(
    url::Origin origin,
    base::Time api_invocation_time,
    Api api)
    : origin_(std::move(origin)), time_window_(api_invocation_time), api_(api) {
  DCHECK(network::IsOriginPotentiallyTrustworthy(origin_));
}

absl::optional<PrivateAggregationBudgetKey> PrivateAggregationBudgetKey::Create(
    url::Origin origin,
    base::Time api_invocation_time,
    Api api) {
  if (!network::IsOriginPotentiallyTrustworthy(origin)) {
    return absl::nullopt;
  }

  return PrivateAggregationBudgetKey(std::move(origin), api_invocation_time,
                                     api);
}

PrivateAggregationBudgetKey PrivateAggregationBudgetKey::CreateForTesting(
    url::Origin origin,
    base::Time api_invocation_time,
    Api api) {
  return PrivateAggregationBudgetKey(std::move(origin), api_invocation_time,
                                     api);
}

}  // namespace content
