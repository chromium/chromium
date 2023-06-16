// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_test_utils.h"

#include <memory>
#include <tuple>

#include "base/functional/callback_helpers.h"
#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/browser/storage_partition_impl.h"

namespace content {

MockPrivateAggregationBudgeter::MockPrivateAggregationBudgeter() = default;
MockPrivateAggregationBudgeter::~MockPrivateAggregationBudgeter() = default;

MockPrivateAggregationHost::MockPrivateAggregationHost()
    : PrivateAggregationHost(
          /*on_report_request_received=*/base::DoNothing(),
          &test_browser_context_) {}

MockPrivateAggregationHost::~MockPrivateAggregationHost() = default;

MockPrivateAggregationManagerImpl::MockPrivateAggregationManagerImpl(
    StoragePartitionImpl* partition)
    : PrivateAggregationManagerImpl(
          std::make_unique<MockPrivateAggregationBudgeter>(),
          std::make_unique<MockPrivateAggregationHost>(),
          partition) {}

MockPrivateAggregationManagerImpl::~MockPrivateAggregationManagerImpl() =
    default;

bool operator==(const PrivateAggregationBudgetKey::TimeWindow& a,
                const PrivateAggregationBudgetKey::TimeWindow& b) {
  return a.start_time() == b.start_time();
}

bool operator==(const PrivateAggregationBudgetKey& a,
                const PrivateAggregationBudgetKey& b) {
  const auto tie = [](const PrivateAggregationBudgetKey& budget_key) {
    return std::make_tuple(budget_key.origin(), budget_key.time_window(),
                           budget_key.api());
  };
  return tie(a) == tie(b);
}

}  // namespace content
