// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/test_interest_group_private_aggregation_manager.h"

#include <map>
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
#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/browser/private_aggregation/private_aggregation_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/private_aggregation/aggregatable_report.mojom.h"
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
    PrivateAggregationBudgetKey::Api api_for_budgeting,
    absl::optional<std::string> context_id,
    absl::optional<base::TimeDelta> timeout,
    absl::optional<url::Origin> aggregation_coordinator_origin,
    mojo::PendingReceiver<blink::mojom::PrivateAggregationHost>
        pending_receiver) {
  EXPECT_EQ(expected_top_frame_origin_, top_frame_origin);
  EXPECT_EQ(PrivateAggregationBudgetKey::Api::kProtectedAudience,
            api_for_budgeting);
  EXPECT_FALSE(context_id.has_value());
  EXPECT_FALSE(timeout.has_value());

  // TODO(alexmt): Change once selecting the origin is possible.
  EXPECT_FALSE(aggregation_coordinator_origin.has_value());

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

bool TestInterestGroupPrivateAggregationManager::IsDebugModeAllowed(
    const url::Origin& top_frame_origin,
    const url::Origin& reporting_origin) {
  return true;
}

void TestInterestGroupPrivateAggregationManager::ContributeToHistogram(
    std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
        contribution_ptrs) {
  const url::Origin& worklet_origin = receiver_set_.current_context();

  if (!allow_multiple_calls_per_origin_) {
    EXPECT_FALSE(
        base::Contains(private_aggregation_contributions_, worklet_origin));
  }

  // Here, we 'unbatch' the contributions into separate requests. This allows
  // for simpler equality checks in testing.
  for (blink::mojom::AggregatableReportHistogramContributionPtr& contribution :
       contribution_ptrs) {
    private_aggregation_contributions_[worklet_origin].push_back(
        std::move(contribution));
  }
}

void TestInterestGroupPrivateAggregationManager::EnableDebugMode(
    blink::mojom::DebugKeyPtr debug_key) {
  const url::Origin& worklet_origin = receiver_set_.current_context();

  // This does not support multiple debug mode calls per origin, but that is not
  // currently needed.
  EXPECT_FALSE(
      base::Contains(private_aggregation_debug_details_, worklet_origin));

  private_aggregation_debug_details_[worklet_origin] =
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

  for (auto& [origin, contributions] : private_aggregation_contributions_) {
    InterestGroupAuctionReporter::PrivateAggregationRequests& requests =
        private_aggregation_requests_map[origin];
    if (!base::Contains(private_aggregation_debug_details_, origin)) {
      private_aggregation_debug_details_[origin] =
          blink::mojom::DebugModeDetails::New();
    }

    for (auto& contribution : contributions) {
      requests.push_back(auction_worklet::mojom::PrivateAggregationRequest::New(
          auction_worklet::mojom::AggregatableReportContribution::
              NewHistogramContribution(std::move(contribution)),
          blink::mojom::AggregationServiceMode::kDefault,
          private_aggregation_debug_details_[origin]->Clone()));
    }
  }

  private_aggregation_contributions_.clear();
  private_aggregation_debug_details_.clear();

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
  allow_multiple_calls_per_origin_ = false;
}

}  // namespace content
