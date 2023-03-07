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
#include "content/public/browser/storage_partition.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/private_aggregation/aggregatable_report.mojom-forward.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom.h"
#include "url/origin.h"

namespace content {

// An implementation of PrivateAggregationManager used for interest group tests
// that tracks PrivateAggregationBudgetKey::Api::kFledge reports.
class TestInterestGroupPrivateAggregationManager
    : public PrivateAggregationManager,
      public blink::mojom::PrivateAggregationHost {
 public:
  // `expected_top_frame_origin` is the expected top-frame origin passed to all
  // calls.
  explicit TestInterestGroupPrivateAggregationManager(
      const url::Origin& expected_top_frame_origin);
  ~TestInterestGroupPrivateAggregationManager() override;

  // PrivateAggregationManager implementation:
  bool BindNewReceiver(
      url::Origin worklet_origin,
      url::Origin top_frame_origin,
      PrivateAggregationBudgetKey::Api api_for_budgeting,
      mojo::PendingReceiver<blink::mojom::PrivateAggregationHost>
          pending_receiver) override;
  void ClearBudgetData(base::Time delete_begin,
                       base::Time delete_end,
                       StoragePartition::StorageKeyMatcherFunction filter,
                       base::OnceClosure done) override;

  // blink::mojom::PrivateAggregationHost implementation:
  void SendHistogramReport(
      std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
          contributions,
      blink::mojom::AggregationServiceMode aggregation_mode,
      blink::mojom::DebugModeDetailsPtr debug_mode_details) override;

  // Returns a logging callback and saves all requests passed to it. These can
  // then be retrieved by `TakeLoggedPrivateAggregationRequests()`
  InterestGroupAuctionReporter::LogPrivateAggregationRequestsCallback
  GetLogPrivateAggregationRequestsCallback();

  // Returns a per-origin map of reconstructed PrivateAggregationRequests make
  // from SendHistogramReport() calls.
  //
  // Clears everything it returns from internal state, so future calls will only
  // return new reports. Calls RunLoop::RunUntilIdle(), since
  // SendHistogramReport() receives asynchronously calls over the Mojo pipe
  // returned by BindNewReceiver().
  std::map<url::Origin,
           InterestGroupAuctionReporter::PrivateAggregationRequests>
  TakePrivateAggregationRequests();

  std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>
  TakeLoggedPrivateAggregationRequests();

 private:
  void LogPrivateAggregationRequests(
      const std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>&
          private_aggregation_requests);

  const url::Origin expected_top_frame_origin_;

  // Reports received through `SendHistogramReport()`.
  std::map<url::Origin,
           InterestGroupAuctionReporter::PrivateAggregationRequests>
      private_aggregation_requests_;

  // Reports received through running
  // `GetLogPrivateAggregationRequestsCallback()`.
  std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>
      logged_private_aggregation_requests_;

  // Bound receivers received by BindNewReceiver. Each one is associated with
  // the worklet origin passed in to BindNewReceiver().
  mojo::ReceiverSet<blink::mojom::PrivateAggregationHost, url::Origin>
      receiver_set_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_TEST_INTEREST_GROUP_PRIVATE_AGGREGATION_MANAGER_H_
