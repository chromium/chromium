// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/test_interest_group_private_aggregation_manager.h"

#include <map>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "content/browser/interest_group/interest_group_auction_reporter.h"
#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/browser/private_aggregation/private_aggregation_manager.h"
#include "content/common/private_aggregation_host.mojom.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gtest/include/gtest/gtest.h"
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
    mojo::PendingReceiver<mojom::PrivateAggregationHost> pending_receiver) {
  EXPECT_EQ(expected_top_frame_origin_, top_frame_origin);
  EXPECT_EQ(PrivateAggregationBudgetKey::Api::kFledge, api_for_budgeting);

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
    std::vector<mojom::AggregatableReportHistogramContributionPtr>
        contributions,
    mojom::AggregationServiceMode aggregation_mode,
    mojom::DebugModeDetailsPtr debug_mode_details) {
  EXPECT_EQ(1u, contributions.size());
  const url::Origin& worklet_origin = receiver_set_.current_context();
  auction_worklet::mojom::PrivateAggregationRequestPtr request =
      auction_worklet::mojom::PrivateAggregationRequest::New(
          auction_worklet::mojom::AggregatableReportContribution::
              NewHistogramContribution(std::move(contributions[0])),
          aggregation_mode, std::move(debug_mode_details));

  if (!should_match_logged_requests_) {
    // Currently non-reserved private aggregation requests are not passed to
    // LogPrivateAggregationRequestsCallback(), and not stored in
    // `logged_private_aggregation_requests_`.
    DCHECK(logged_private_aggregation_requests_.empty());
    private_aggregation_requests_[worklet_origin].push_back(std::move(request));
    return;
  }

  bool found_matching_logged_request = false;
  InterestGroupAuctionReporter::PrivateAggregationRequests&
      logged_requests_for_origin =
          logged_private_aggregation_requests_[worklet_origin];
  for (auto it = logged_requests_for_origin.begin();
       it != logged_requests_for_origin.end(); ++it) {
    if (**it == *request) {
      found_matching_logged_request = true;
      logged_requests_for_origin.erase(it);
      if (logged_requests_for_origin.empty()) {
        logged_private_aggregation_requests_.erase(worklet_origin);
      }
      break;
    }
  }
  EXPECT_TRUE(found_matching_logged_request)
      << "Request unexpectedly not logged for origin: "
      << worklet_origin.Serialize();
  private_aggregation_requests_[worklet_origin].push_back(std::move(request));
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
  // All logged events should have had a matching SendHistogramReport() call.
  EXPECT_TRUE(logged_private_aggregation_requests_.empty());
  return std::exchange(private_aggregation_requests_, {});
}

void TestInterestGroupPrivateAggregationManager::SetShouldMatchLoggedRequests(
    bool should_match_logged_requests) {
  should_match_logged_requests_ = should_match_logged_requests;
}

void TestInterestGroupPrivateAggregationManager::LogPrivateAggregationRequests(
    const std::map<
        url::Origin,
        std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>>&
        private_aggregation_requests) {
  DCHECK(should_match_logged_requests_ || private_aggregation_requests.empty());
  for (auto& pair : private_aggregation_requests) {
    auto& requests_for_origin =
        logged_private_aggregation_requests_[pair.first];
    for (auto& request : pair.second) {
      requests_for_origin.push_back(request.Clone());
    }
  }
}

}  // namespace content
