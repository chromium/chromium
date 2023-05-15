// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/test_interest_group_private_aggregation_manager.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "content/browser/interest_group/interest_group_auction_reporter.h"
#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/browser/private_aggregation/private_aggregation_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/private_aggregation/aggregatable_report.mojom.h"
#include "url/origin.h"

namespace content {

TestInterestGroupPrivateAggregationManager::
    TestInterestGroupPrivateAggregationManager(
        const url::Origin& expected_top_frame_origin)
    : expected_top_frame_origin_(expected_top_frame_origin) {}

TestInterestGroupPrivateAggregationManager::
    ~TestInterestGroupPrivateAggregationManager() = default;

bool TestInterestGroupPrivateAggregationManager::BindNewReceiver(
    url::Origin worklet_origin,
    url::Origin top_frame_origin,
    PrivateAggregationBudgetKey::Api api_for_budgeting,
    absl::optional<std::string> context_id,
    mojo::PendingReceiver<blink::mojom::PrivateAggregationHost>
        pending_receiver) {
  EXPECT_EQ(expected_top_frame_origin_, top_frame_origin);
  EXPECT_EQ(PrivateAggregationBudgetKey::Api::kFledge, api_for_budgeting);
  EXPECT_FALSE(context_id.has_value());

  receiver_set_.Add(this, std::move(pending_receiver), worklet_origin);
  return true;
}

void TestInterestGroupPrivateAggregationManager::ClearBudgetData(
    base::Time delete_begin,
    base::Time delete_end,
    StoragePartition::StorageKeyMatcherFunction filter,
    base::OnceClosure done) {
  NOTREACHED();
}

void TestInterestGroupPrivateAggregationManager::SendHistogramReport(
    std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
        contributions,
    blink::mojom::AggregationServiceMode aggregation_mode,
    blink::mojom::DebugModeDetailsPtr debug_mode_details) {
  EXPECT_EQ(1u, contributions.size());
  const url::Origin& worklet_origin = receiver_set_.current_context();
  auction_worklet::mojom::PrivateAggregationRequestPtr request =
      auction_worklet::mojom::PrivateAggregationRequest::New(
          auction_worklet::mojom::AggregatableReportContribution::
              NewHistogramContribution(std::move(contributions[0])),
          aggregation_mode, std::move(debug_mode_details));

  private_aggregation_requests_[worklet_origin].push_back(std::move(request));
}
void TestInterestGroupPrivateAggregationManager::
    SetDebugModeDetailsOnNullReport(
        blink::mojom::DebugModeDetailsPtr debug_mode_details) {
  ADD_FAILURE() << "Null report details are not expected in FLEDGE.";
}

InterestGroupAuctionReporter::LogPrivateAggregationRequestsCallback
TestInterestGroupPrivateAggregationManager::
    GetLogPrivateAggregationRequestsCallback() {
  return base::BindRepeating(&TestInterestGroupPrivateAggregationManager::
                                 LogPrivateAggregationRequests,
                             base::Unretained(this));
}

std::map<url::Origin, InterestGroupAuctionReporter::PrivateAggregationRequests>
TestInterestGroupPrivateAggregationManager::TakePrivateAggregationRequests() {
  base::RunLoop().RunUntilIdle();
  return std::exchange(private_aggregation_requests_, {});
}

std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>
TestInterestGroupPrivateAggregationManager::
    TakeLoggedPrivateAggregationRequests() {
  return std::move(logged_private_aggregation_requests_);
}

void TestInterestGroupPrivateAggregationManager::LogPrivateAggregationRequests(
    const std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>&
        private_aggregation_requests) {
  logged_private_aggregation_requests_.reserve(
      logged_private_aggregation_requests_.size() +
      private_aggregation_requests.size());
  base::ranges::for_each(
      private_aggregation_requests,
      [this](
          const auction_worklet::mojom::PrivateAggregationRequestPtr& request) {
        logged_private_aggregation_requests_.push_back(request->Clone());
      });
}

}  // namespace content
