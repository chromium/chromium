// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/test_interest_group_private_aggregation_manager.h"

#include <stddef.h>

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "content/browser/interest_group/interest_group_auction_reporter.h"
#include "content/browser/private_aggregation/private_aggregation_caller_api.h"
#include "content/browser/private_aggregation/private_aggregation_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom.h"
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
    PrivateAggregationCallerApi api_for_budgeting,
    std::optional<std::string> context_id,
    std::optional<base::TimeDelta> timeout,
    std::optional<url::Origin> aggregation_coordinator_origin,
    size_t filtering_id_max_bytes,
    mojo::PendingReceiver<blink::mojom::PrivateAggregationHost>
        pending_receiver) {
  EXPECT_EQ(expected_top_frame_origin_, top_frame_origin);
  EXPECT_EQ(PrivateAggregationCallerApi::kProtectedAudience, api_for_budgeting);
  EXPECT_FALSE(context_id.has_value());
  EXPECT_FALSE(timeout.has_value());
  EXPECT_EQ(filtering_id_max_bytes, 1u);

  // TODO(alexmt): Change once selecting the origin is possible.
  EXPECT_FALSE(aggregation_coordinator_origin.has_value());

  mojo::ReceiverId receiver_id =
      receiver_set_.Add(this, std::move(pending_receiver));
  private_aggregation_worklet_origins_[receiver_id] = std::move(worklet_origin);

  return true;
}

void TestInterestGroupPrivateAggregationManager::ClearBudgetData(
    base::Time delete_begin,
    base::Time delete_end,
    StoragePartition::StorageKeyMatcherFunction filter,
    base::OnceClosure done) {
  NOTREACHED_IN_MIGRATION();
}

bool TestInterestGroupPrivateAggregationManager::IsDebugModeAllowed(
    const url::Origin& top_frame_origin,
    const url::Origin& reporting_origin) {
  return true;
}

void TestInterestGroupPrivateAggregationManager::ContributeToHistogram(
    std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
        contribution_ptrs) {
  const mojo::ReceiverId receiver_id = receiver_set_.current_receiver();

  // Multiple calls to the same receiver are not supported.
  EXPECT_FALSE(base::Contains(private_aggregation_contributions_, receiver_id));

  // Here, we 'unbatch' the contributions into separate requests. This allows
  // for simpler equality checks in testing.
  for (blink::mojom::AggregatableReportHistogramContributionPtr& contribution :
       contribution_ptrs) {
    private_aggregation_contributions_[receiver_id].push_back(
        std::move(contribution));
  }
}

void TestInterestGroupPrivateAggregationManager::EnableDebugMode(
    blink::mojom::DebugKeyPtr debug_key) {
  const mojo::ReceiverId receiver_id = receiver_set_.current_receiver();

  EXPECT_FALSE(base::Contains(private_aggregation_debug_details_, receiver_id));

  private_aggregation_debug_details_[receiver_id] =
      blink::mojom::DebugModeDetails::New(/*is_enabled=*/true,
                                          std::move(debug_key));
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

  std::map<url::Origin,
           InterestGroupAuctionReporter::PrivateAggregationRequests>
      private_aggregation_requests_map;

  for (auto& [receiver_id, contributions] :
       private_aggregation_contributions_) {
    EXPECT_TRUE(
        base::Contains(private_aggregation_worklet_origins_, receiver_id));
    if (!base::Contains(private_aggregation_debug_details_, receiver_id)) {
      private_aggregation_debug_details_[receiver_id] =
          blink::mojom::DebugModeDetails::New();
    }

    InterestGroupAuctionReporter::PrivateAggregationRequests& requests =
        private_aggregation_requests_map
            [private_aggregation_worklet_origins_[receiver_id]];

    for (auto& contribution : contributions) {
      requests.push_back(auction_worklet::mojom::PrivateAggregationRequest::New(
          auction_worklet::mojom::AggregatableReportContribution::
              NewHistogramContribution(std::move(contribution)),
          blink::mojom::AggregationServiceMode::kDefault,
          private_aggregation_debug_details_[receiver_id]->Clone()));
    }
  }

  private_aggregation_contributions_.clear();
  private_aggregation_debug_details_.clear();
  private_aggregation_worklet_origins_.clear();

  return private_aggregation_requests_map;
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

void TestInterestGroupPrivateAggregationManager::Reset() {
  private_aggregation_contributions_.clear();
  private_aggregation_debug_details_.clear();
  logged_private_aggregation_requests_.clear();
  receiver_set_.Clear();
}

}  // namespace content
