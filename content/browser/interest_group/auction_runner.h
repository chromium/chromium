// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_AUCTION_RUNNER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_AUCTION_RUNNER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/browser/interest_group/auction_metrics_recorder.h"
#include "content/browser/interest_group/auction_worklet_manager.h"
#include "content/browser/interest_group/interest_group_auction.h"
#include "content/browser/interest_group/interest_group_auction_reporter.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/ad_auction_service.mojom.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/origin.h"

namespace blink {
struct AuctionConfig;
}

namespace content {

class InterestGroupAuctionReporter;
class BrowserContext;
class InterestGroupManagerImpl;
class PrivateAggregationManager;

// An AuctionRunner loads and runs the bidder and seller worklets, along with
// their reporting phases and produces the result via a callback. Most of the
// logic is handled by InterestGroupAuction, with the AuctionRunner handling
// state transitions and assembling the final results of the auction.
//
// All auctions must be created on the same thread. This is just needed because
// the code to assign unique tracing IDs is not threadsafe.
class CONTENT_EXPORT AuctionRunner : public blink::mojom::AbortableAdAuction {
 public:
  using PrivateAggregationRequests =
      std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>;

  // Invoked when a FLEDGE auction is complete.
  //
  // `manually_aborted` is true only if the auction was successfully interrupted
  //  by the call to Abort().
  //
  // `winning_group_id` owner and name of the winning interest group (if any).
  //
  // `requested_ad_size` requested size for the ad auction (if any). Stored into
  // the fenced frame config container size.
  //
  // `ad_descriptor` URL of auction winning ad to render with optional
  // size. Null if there is no winner.
  //
  // `ad_component_descriptors` is the list of ad component URLs with
  // optional size returned by the winning bidder. Null if there is no winner or
  // no list was returned.
  //
  // `report_urls` Reporting URLs returned by seller worklet reportResult()
  //  methods and the winning bidder's reportWin() methods, if any.
  //
  // `errors` are various error messages to be used for debugging. These are too
  //  sensitive for the renderers to see.
  using RunAuctionCallback = base::OnceCallback<void(
      AuctionRunner* auction_runner,
      bool manually_aborted,
      absl::optional<blink::InterestGroupKey> winning_group_id,
      absl::optional<blink::AdSize> requested_ad_size,
      absl::optional<blink::AdDescriptor> ad_descriptor,
      std::vector<blink::AdDescriptor> ad_component_descriptors,
      std::vector<std::string> errors,
      std::unique_ptr<InterestGroupAuctionReporter>
          interest_group_auction_reporter)>;

  // Returns true if `origin` is allowed to use the interest group API. Will be
  // called on worklet / interest group origins before using them in any
  // interest group API.
  using IsInterestGroupApiAllowedCallback =
      InterestGroupAuction::IsInterestGroupApiAllowedCallback;

  // Creates an entire FLEDGE auction. Single-use object.
  //
  // Arguments: `auction_worklet_manager`, `interest_group_manager`,
  //  `browser_context`, and `private_aggregation_manager` must remain valid,
  //  and `log_private_aggregation_requests_callback` must be safe to call until
  //  the AuctionRunner and any InterestGroupAuctionReporter it returns are
  //  destroyed.
  //
  //  `auction_config` is the configuration provided by client JavaScript in the
  //   renderer in order to initiate the auction.
  //
  //  `main_frame_origin` is the origin of the main frame where the auction is
  //   running. Used for issuing reports.
  //
  //  `frame_origin` is the origin of the frame running the auction. Used for
  //   issuing reports.
  //
  //  `client_security_state` is the client security state of the frame that
  //   issued the auction request -- this is used for post-auction interest
  //   group updates, and sending reports.
  //
  //  `url_loader_factory` will be used to issue reporting requests. It should
  //  be backed by a trusted URLLoaderFactory.
  //
  //  `is_interest_group_api_allowed_callback` will be called on all buyer and
  //   seller origins, and those for which it returns false will not be allowed
  //   to participate in the auction.
  //
  //  `callback` is invoked on auction completion. It should synchronously
  //   destroy this AuctionRunner object. `callback` won't be invoked until
  //   after CreateAndStart() returns.
  static std::unique_ptr<AuctionRunner> CreateAndStart(
      AuctionWorkletManager* auction_worklet_manager,
      InterestGroupManagerImpl* interest_group_manager,
      BrowserContext* browser_context,
      PrivateAggregationManager* private_aggregation_manager,
      InterestGroupAuctionReporter::LogPrivateAggregationRequestsCallback
          log_private_aggregation_requests_callback,
      const blink::AuctionConfig& auction_config,
      const url::Origin& main_frame_origin,
      const url::Origin& frame_origin,
      ukm::SourceId ukm_source_id,
      network::mojom::ClientSecurityStatePtr client_security_state,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      IsInterestGroupApiAllowedCallback is_interest_group_api_allowed_callback,
      mojo::PendingReceiver<AbortableAdAuction> abort_receiver,
      RunAuctionCallback callback);

  explicit AuctionRunner(const AuctionRunner&) = delete;
  AuctionRunner& operator=(const AuctionRunner&) = delete;

  ~AuctionRunner() override;

  // AbortableAdAuction implementation.
  void ResolvedPromiseParam(
      blink::mojom::AuctionAdConfigAuctionIdPtr auction_id,
      blink::mojom::AuctionAdConfigField field,
      const absl::optional<std::string>& json_value) override;
  void ResolvedPerBuyerSignalsPromise(
      blink::mojom::AuctionAdConfigAuctionIdPtr auction_id,
      const absl::optional<base::flat_map<url::Origin, std::string>>&
          per_buyer_signals) override;
  void ResolvedBuyerTimeoutsPromise(
      blink::mojom::AuctionAdConfigAuctionIdPtr auction_id,
      blink::mojom::AuctionAdConfigBuyerTimeoutField field,
      const blink::AuctionConfig::BuyerTimeouts& buyer_timeouts) override;
  void ResolvedBuyerCurrenciesPromise(
      blink::mojom::AuctionAdConfigAuctionIdPtr auction_id,
      const blink::AuctionConfig::BuyerCurrencies& buyer_currencies) override;
  void ResolvedDirectFromSellerSignalsPromise(
      blink::mojom::AuctionAdConfigAuctionIdPtr auction_id,
      const absl::optional<blink::DirectFromSellerSignals>&
          direct_from_seller_signals) override;
  void Abort() override;

  // Fails the auction, invoking `callback_` and prevents any future calls into
  // `this` by closing mojo pipes and disposing of weak pointers. The owner must
  // be able to safely delete `this` when the callback is invoked. May only be
  // invoked if the auction has not yet completed.
  //
  // `interest_groups_that_bid` is a list of the interest groups that bid in the
  // auction.
  void FailAuction(bool manually_aborted,
                   blink::InterestGroupSet interest_groups_that_bid =
                       blink::InterestGroupSet());

 private:
  enum class State {
    kLoadingGroupsPhase,
    kBiddingAndScoringPhase,
    kSucceeded,
    kFailed,
  };

  AuctionRunner(
      AuctionWorkletManager* auction_worklet_manager,
      InterestGroupManagerImpl* interest_group_manager,
      BrowserContext* browser_context,
      PrivateAggregationManager* private_aggregation_manager,
      InterestGroupAuctionReporter::LogPrivateAggregationRequestsCallback
          log_private_aggregation_requests_callback,
      auction_worklet::mojom::KAnonymityBidMode kanon_mode,
      const blink::AuctionConfig& auction_config,
      const url::Origin& main_frame_origin,
      const url::Origin& frame_origin,
      ukm::SourceId ukm_source_id,
      network::mojom::ClientSecurityStatePtr client_security_state,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      IsInterestGroupApiAllowedCallback is_interest_group_api_allowed_callback,
      mojo::PendingReceiver<AbortableAdAuction> abort_receiver,
      RunAuctionCallback callback);

  // Tells `auction_` to start the loading interest groups phase.
  void StartAuction();

  // Invoked asynchronously by `auction_` once all interest groups have loaded.
  // Fails the auction if `success` is false. Otherwise, starts the bidding and
  // scoring phase.
  void OnLoadInterestGroupsComplete(bool success);

  // Invoked asynchronously by `auction_` once the bidding and scoring phase is
  // complete. Either fails the auction (in which case it records the interest
  // groups that bid) or starts the reporting phase, depending on the value of
  // `success`.
  void OnBidsGeneratedAndScored(bool success);

  // Invoked asynchronously by `auction_` once the reporting phase has
  // completed. Records `interest_groups_that_bid`. If `success` is false, fails
  // the auction. Otherwise, records which interest group won the auction and
  // collects parameters needed to invoke the auction callback.
  void OnReportingPhaseComplete(
      blink::InterestGroupSet interest_groups_that_bid,
      bool success);

  // After an auction completes (success or failure -- wherever `callback_` is
  // invoked), updates the set of interest groups that participated in the
  // auction.
  void UpdateInterestGroupsPostAuction();

  // Notify relevant InterestGroupAuctions of progress in resolving promises in
  // config, as appropriate. Manages `promise_fields_in_auction_config_`.
  void NotifyPromiseResolved(
      const blink::mojom::AuctionAdConfigAuctionId* auction_id,
      blink::AuctionConfig* config);

  const raw_ptr<InterestGroupManagerImpl> interest_group_manager_;

  // Needed to create `FencedFrameReporter`.
  const raw_ptr<BrowserContext> browser_context_;

  const raw_ptr<PrivateAggregationManager> private_aggregation_manager_;

  const url::Origin main_frame_origin_;
  const url::Origin frame_origin_;

  // ClientSecurityState built from the frame that issued the auction request;
  // will be used to update interest groups that participated in the auction
  // after the auction.
  const network::mojom::ClientSecurityStatePtr client_security_state_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // For checking if operations like running auctions, updating interest groups,
  // etc. are allowed or not.
  IsInterestGroupApiAllowedCallback is_interest_group_api_allowed_callback_;

  mojo::Receiver<blink::mojom::AbortableAdAuction> abort_receiver_;

  // Configuration.

  // Whether k-anonymity enforcement or simulation (or none) are performed.
  const auction_worklet::mojom::KAnonymityBidMode kanon_mode_;
  // Use a smart pointer so can pass ownership to InterestGroupAuctionReporter
  // without invalidating pointers.
  std::unique_ptr<blink::AuctionConfig> owned_auction_config_;

  RunAuctionCallback callback_;

  // Number of fields in `owned_auction_config_` that are promises; decremented
  // as they get resolved.
  int promise_fields_in_auction_config_;

  // Used to store data needed to record UKM.
  AuctionMetricsRecorder auction_metrics_recorder_;

  InterestGroupAuction auction_;
  State state_ = State::kLoadingGroupsPhase;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_AUCTION_RUNNER_H_
