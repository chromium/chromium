// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_TEST_INTEREST_GROUP_PRIVATE_AGGREGATION_MANAGER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_TEST_INTEREST_GROUP_PRIVATE_AGGREGATION_MANAGER_H_

#include <stddef.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "content/browser/interest_group/interest_group_auction_reporter.h"
#include "content/browser/private_aggregation/private_aggregation_caller_api.h"
#include "content/browser/private_aggregation/private_aggregation_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom-forward.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom.h"
#include "url/origin.h"

namespace content {

// An implementation of PrivateAggregationManager used for interest group tests
// that tracks PrivateAggregationCallerApi::kProtectedAudience reports.
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
      PrivateAggregationCallerApi api_for_budgeting,
      std::optional<std::string> context_id,
      std::optional<base::TimeDelta> timeout,
      std::optional<url::Origin> aggregation_coordinator_origin,
      size_t filtering_id_max_bytes,
      mojo::PendingReceiver<blink::mojom::PrivateAggregationHost>
          pending_receiver) override;
  void ClearBudgetData(base::Time delete_begin,
                       base::Time delete_end,
                       StoragePartition::StorageKeyMatcherFunction filter,
                       base::OnceClosure done) override;
  bool IsDebugModeAllowed(const url::Origin& top_frame_origin,
                          const url::Origin& reporting_origin) override;

  // blink::mojom::PrivateAggregationHost implementation:
  void ContributeToHistogram(
      std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
          contribution_ptrs) override;
  void EnableDebugMode(blink::mojom::DebugKeyPtr debug_key) override;

  // Returns a logging callback and saves all requests passed to it. These can
  // then be retrieved by `TakeLoggedPrivateAggregationRequests()`
  InterestGroupAuctionReporter::LogPrivateAggregationRequestsCallback
  GetLogPrivateAggregationRequestsCallback();

  // Returns a per-origin map of reconstructed PrivateAggregationRequests make
  // from SendHistogramReport() calls. Note that the requests will have been
  // 'unbatched' -- i.e. each contribution will be in a separate report. This is
  // done for easier equality checks in tests.
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

  // Resets all internal state to the state just after construction.
  void Reset();

 private:
  void LogPrivateAggregationRequests(
      const std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>&
          private_aggregation_requests);

  const url::Origin expected_top_frame_origin_;

  // Contributions received through `ContributeToHistogram()`.
  std::map<
      mojo::ReceiverId,
      std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>>
      private_aggregation_contributions_;

  // Debug details set through `EnableDebugMode()`.
  std::map<mojo::ReceiverId, blink::mojom::DebugModeDetailsPtr>
      private_aggregation_debug_details_;

  // Worklet origins set through `BindNewReceiver()`.
  std::map<mojo::ReceiverId, url::Origin> private_aggregation_worklet_origins_;

  // Reports received through running
  // `GetLogPrivateAggregationRequestsCallback()`.
  std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>
      logged_private_aggregation_requests_;

  // Bound receivers received by `BindNewReceiver()`.
  mojo::ReceiverSet<blink::mojom::PrivateAggregationHost> receiver_set_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_TEST_INTEREST_GROUP_PRIVATE_AGGREGATION_MANAGER_H_
