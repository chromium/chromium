// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_budget_key.h"

#include <optional>

#include "base/check.h"
#include "base/time/time.h"
#include "content/browser/private_aggregation/private_aggregation_caller_api.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "url/origin.h"

namespace content {

namespace {
base::Time FloorToDuration(base::Time time) {
  // `FloorToMultiple` would no-op on `base::Time::Max()`.
  CHECK(!time.is_max());

  return base::Time() + time.since_origin().FloorToMultiple(
                            PrivateAggregationBudgetKey::TimeWindow::kDuration);
}
}  // namespace

PrivateAggregationBudgetKey::TimeWindow::TimeWindow(base::Time start_time)
    : start_time_(FloorToDuration(start_time)) {}

PrivateAggregationBudgetKey::PrivateAggregationBudgetKey(
    url::Origin origin,
    base::Time api_invocation_time,
    PrivateAggregationCallerApi api)
    : origin_(std::move(origin)), time_window_(api_invocation_time), api_(api) {
  CHECK(network::IsOriginPotentiallyTrustworthy(origin_));
}

std::optional<PrivateAggregationBudgetKey> PrivateAggregationBudgetKey::Create(
    url::Origin origin,
    base::Time api_invocation_time,
    PrivateAggregationCallerApi api) {
  if (!network::IsOriginPotentiallyTrustworthy(origin)) {
    return std::nullopt;
  }

  return PrivateAggregationBudgetKey(std::move(origin), api_invocation_time,
                                     api);
}

PrivateAggregationBudgetKey PrivateAggregationBudgetKey::CreateForTesting(
    url::Origin origin,
    base::Time api_invocation_time,
    PrivateAggregationCallerApi api) {
  return PrivateAggregationBudgetKey(std::move(origin), api_invocation_time,
                                     api);
}

}  // namespace content
