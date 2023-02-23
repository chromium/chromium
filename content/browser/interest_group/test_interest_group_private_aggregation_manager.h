// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_TEST_INTEREST_GROUP_PRIVATE_AGGREGATION_MANAGER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_TEST_INTEREST_GROUP_PRIVATE_AGGREGATION_MANAGER_H_

#include <map>
#include <vector>

#include "base/functional/callback_forward.h"
#include "content/browser/interest_group/interest_group_auction_reporter.h"
#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/browser/private_aggregation/private_aggregation_manager.h"
#include "content/common/aggregatable_report.mojom-forward.h"
#include "content/common/private_aggregation_host.mojom.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "url/origin.h"

namespace content {

// An implementation of PrivateAggregationManager used for interest group tests.
// It tracks PrivateAggregationBudgetKey::Api::kFledge reports, and compares
// them against calls to a
// InterestGroupAuctionReporter::LogPrivateAggregationRequestsCallback it
// provides.
class TestInterestGroupPrivateAggregationManager
    : public PrivateAggregationManager,
      public mojom::PrivateAggregationHost {
 public:
  // `expected_top_frame_origin` is the expected top-frame origin passed to all
  // calls.
  explicit TestInterestGroupPrivateAggregationManager(
      const url::Origin& expected_top_frame_origin);
  ~TestInterestGroupPrivateAggregationManager() override;

  // PrivateAggregationManager implementation:
  bool BindNewReceiver(url::Origin worklet_origin,
                       url::Origin top_frame_origin,
                       PrivateAggregationBudgetKey::Api api_for_budgeting,
                       mojo::PendingReceiver<mojom::PrivateAggregationHost>
                           pending_receiver) override;
  void ClearBudgetData(base::Time delete_begin,
                       base::Time delete_end,
                       StoragePartition::StorageKeyMatcherFunction filter,
                       base::OnceClosure done) override;

  // mojom::PrivateAggregationHost implementation:
  void SendHistogramReport(
      std::vector<mojom::AggregatableReportHistogramContributionPtr>
          contributions,
      mojom::AggregationServiceMode aggregation_mode,
      mojom::DebugModeDetailsPtr debug_mode_details) override;

  // Returns a logging callback for use with an InterestGroupAuctionReporter.
  // Each observed private aggregation request it sees is added to an internal
  // vector, and removed once the corresponding SendHistogramReport() is
  // observed. If SendHistogramReport() is invoked for a report that doesn't
  // match something passed to this callback, causes an EXPECT failure.
  InterestGroupAuctionReporter::LogPrivateAggregationRequestsCallback
  GetLogPrivateAggregationRequestsCallback();

  // Returns a per-origin map of reconstructed PrivateAggregationRequests make
  // from SendHistogramReport() calls. Also checks that every report observed
  // through the callback returned by GetLogPrivateAggregationRequestsCallback()
  // matches the report passed to one and only one call to
  // SendHistogramReport().
  //
  // Clears everything it returns from internal state, so future calls will only
  // return new reports. Calls RunLoop::RunUntilIdle(), since
  // SendHistogramReport() receives asynchronously calls over the Mojo pipe
  // returned by BindNewReceiver().
  std::map<url::Origin,
           InterestGroupAuctionReporter::PrivateAggregationRequests>
  TakePrivateAggregationRequests();

  // Non-reserved requests are not saved to
  // `logged_private_aggregation_requests_`, so should set
  // `should_match_logged_requests_` to false to avoid checking whether
  // `private_aggregation_requests_` and `logged_private_aggregation_requests_`
  // match. `logged_private_aggregation_requests_` should be empty when
  // `should_match_logged_requests_` is false.
  void SetShouldMatchLoggedRequests(bool should_match_logged_requests);

 private:
  void LogPrivateAggregationRequests(
      const std::map<
          url::Origin,
          std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>>&
          private_aggregation_requests);

  const url::Origin expected_top_frame_origin_;

  // Per-origin map of aggregation requests passed to the callback returned by
  // GetLogPrivateAggregationRequestsCallback(). Requests are removed from this
  // map once an identical request has been received through
  // SendHistogramReport().
  std::map<url::Origin,
           InterestGroupAuctionReporter::PrivateAggregationRequests>
      logged_private_aggregation_requests_;

  // Reports received through SendHistogramReport().
  std::map<url::Origin,
           InterestGroupAuctionReporter::PrivateAggregationRequests>
      private_aggregation_requests_;

  // Bound receivers received by BindNewReceiver. Each one is associated with
  // the worklet origin passed in to BindNewReceiver().
  mojo::ReceiverSet<mojom::PrivateAggregationHost, url::Origin> receiver_set_;

  // `private_aggregation_requests_` and `logged_private_aggregation_requests_`
  // are required to match if `should_match_logged_requests_` is true.
  bool should_match_logged_requests_ = true;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_TEST_INTEREST_GROUP_PRIVATE_AGGREGATION_MANAGER_H_
