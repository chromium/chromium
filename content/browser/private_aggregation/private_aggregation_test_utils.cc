// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_test_utils.h"

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/browser/private_aggregation/private_aggregation_host.h"
#include "content/browser/private_aggregation/private_aggregation_pending_contributions.h"
#include "content/browser/storage_partition_impl.h"
#include "third_party/blink/public/common/features_generated.h"

namespace content {

MockPrivateAggregationBudgeter::MockPrivateAggregationBudgeter() = default;
MockPrivateAggregationBudgeter::~MockPrivateAggregationBudgeter() = default;

MockPrivateAggregationHost::MockPrivateAggregationHost()
    : PrivateAggregationHost(
          /*on_report_request_details_received=*/base::DoNothing(),
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
                           budget_key.caller_api());
  };
  return tie(a) == tie(b);
}

AggregatableReportRequest GenerateReportRequest(
    PrivateAggregationHost::ReportRequestGenerator generator,
    PrivateAggregationPendingContributions::Wrapper contributions) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kPrivateAggregationApiErrorReporting)) {
    return std::move(generator).Run(
        std::move(contributions.GetContributionsVector()));
  }

  EXPECT_TRUE(
      contributions.GetPendingContributions().are_contributions_finalized());

  // This function should only be used for flows that don't call
  // `ContributeToHistogramOnEvent()`.
  EXPECT_TRUE(contributions.GetPendingContributions()
                  .GetConditionalContributionsForTesting()
                  .empty());

  std::vector<PrivateAggregationPendingContributions::BudgeterResult>
      all_approved(
          /*n=*/contributions.GetPendingContributions()
              .unconditional_contributions()
              .size(),
          PrivateAggregationPendingContributions::BudgeterResult::kApproved);

  const std::vector<blink::mojom::AggregatableReportHistogramContribution>&
      final_unmerged_contributions =
          contributions.GetPendingContributions()
              .CompileFinalUnmergedContributions(
                  /*test_budgeter_results=*/all_approved,
                  PrivateAggregationPendingContributions::
                      PendingReportLimitResult::kNotAtLimit,
                  PrivateAggregationPendingContributions::NullReportBehavior::
                      kSendNullReport);

  all_approved.resize(final_unmerged_contributions.size());

  return std::move(generator).Run(
      std::move(contributions.GetPendingContributions())
          .TakeFinalContributions(all_approved));
}

}  // namespace content
