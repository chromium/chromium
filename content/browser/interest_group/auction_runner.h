// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_AUCTION_RUNNER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_AUCTION_RUNNER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"
#include "content/browser/interest_group/auction_worklet_manager.h"
#include "content/browser/interest_group/interest_group_storage.h"
#include "content/common/content_export.h"
#include "content/public/browser/content_browser_client.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {
struct AuctionConfig;
}

namespace content {

class InterestGroupManagerImpl;

// An AuctionRunner loads and runs the bidder and seller worklets, along with
// their reporting phases and produces the result via a callback.
//
// All auctions must be created on the same thread. This is just needed because
// the code to assign unique tracing IDs is not threadsafe.
class CONTENT_EXPORT AuctionRunner {
 public:
  // Post auction signals (signals only available after auction completes such
  // as winning bid) for debug loss/win reporting.
  struct PostAuctionSignals {
    PostAuctionSignals() = default;

    // For now, top level post auction signals do not have
    // `highest_scoring_other_bid` or `made_highest_scoring_other_bid`.
    PostAuctionSignals(double winning_bid, bool made_winning_bid)
        : winning_bid(winning_bid), made_winning_bid(made_winning_bid) {}

    PostAuctionSignals(double winning_bid,
                       bool made_winning_bid,
                       double highest_scoring_other_bid,
                       bool made_highest_scoring_other_bid)
        : winning_bid(winning_bid),
          made_winning_bid(made_winning_bid),
          highest_scoring_other_bid(highest_scoring_other_bid),
          made_highest_scoring_other_bid(made_highest_scoring_other_bid) {}

    ~PostAuctionSignals() = default;

    double winning_bid = 0.0;
    bool made_winning_bid = false;
    double highest_scoring_other_bid = 0.0;
    bool made_highest_scoring_other_bid = false;
  };

  // Invoked when a FLEDGE auction is complete.
  //
  // `winning_group_id` owner and name of the winning interest group (if any).
  //
  // `render_url` URL of auction winning ad to render. Null if there is no
  //  winner.
  //
  // `ad_component_urls` is the list of ad component URLs returned by the
  //  winning bidder. Null if there is no winner or no list was returned.
  //
  // `report_urls` Reporting URLs returned by seller worklet reportResult()
  //  methods and the winning bidder's reportWin() methods, if any.
  //
  // `debug_loss_report_urls` URLs to use for reporting loss result to bidders
  // and the seller. Empty if no report should be sent.
  //
  // `debug_win_report_urls` URLs to use for reporting win result to bidders and
  // the seller. Empty if no report should be sent.
  //
  // `errors` are various error messages to be used for debugging. These are too
  //  sensitive for the renderers to see.
  using RunAuctionCallback = base::OnceCallback<void(
      AuctionRunner* auction_runner,
      absl::optional<blink::InterestGroupKey> winning_group_id,
      absl::optional<GURL> render_url,
      std::vector<GURL> ad_component_urls,
      std::vector<GURL> report_urls,
      std::vector<GURL> debug_loss_report_urls,
      std::vector<GURL> debug_win_report_urls,
      ReportingMetadata ad_beacon_map,
      std::vector<std::string> errors)>;

  // Returns true if `origin` is allowed to use the interest group API. Will be
  // called on worklet / interest group origins before using them in any
  // interest group API.
  using IsInterestGroupApiAllowedCallback = base::RepeatingCallback<bool(
      ContentBrowserClient::InterestGroupApiOperation
          interest_group_api_operation,
      const url::Origin& origin)>;

  // Result of an auction or a component auction. Used for histograms. Only
  // recorded for valid auctions. These are used in histograms, so values of
  // existing entries must not change when adding/removing values, and obsolete
  // values must not be reused.
  enum class AuctionResult {
    // The auction succeeded, with a winning bidder.
    kSuccess = 0,

    // The auction was aborted, due to either navigating away from the frame
    // that started the auction or browser shutdown.
    kAborted = 1,

    // Bad message received over Mojo. This is potentially a security error.
    kBadMojoMessage = 2,

    // The user was in no interest groups that could participate in the auction.
    kNoInterestGroups = 3,

    // The seller worklet failed to load.
    kSellerWorkletLoadFailed = 4,

    // The seller worklet crashed.
    kSellerWorkletCrashed = 5,

    // All bidders failed to bid. This happens when all bidders choose not to
    // bid, fail to load, or crash before making a bid.
    kNoBids = 6,

    // The seller worklet rejected all bids (of which there was at least one).
    kAllBidsRejected = 7,

    // The winning bidder worklet crashed. The bidder must have successfully
    // bid, and the seller must have accepted the bid for this to be logged.
    kWinningBidderWorkletCrashed = 8,

    // The seller is not allowed to use the interest group API.
    kSellerRejected = 9,

    // The component auction completed with a winner, but that winner lost the
    // top-level auction.
    kComponentLostAuction = 10,

    // The component seller worklet with the winning bidder crashed during the
    // reporting phase.
    kWinningComponentSellerWorkletCrashed = 11,

    kMaxValue = kWinningComponentSellerWorkletCrashed
  };

  explicit AuctionRunner(const AuctionRunner&) = delete;
  AuctionRunner& operator=(const AuctionRunner&) = delete;

  // Runs an entire FLEDGE auction.
  //
  // Arguments:
  // `auction_worklet_manager` and `interest_group_manager` must remain valid
  //  until the  AuctionRunner is destroyed.
  //
  // `auction_config` is the configuration provided by client JavaScript in
  //  the renderer in order to initiate the auction.
  //
  //  `client_security_state` is the client security state of the frame that
  //  issued the auction request -- this is used for post-auction interest group
  //  updates.
  //
  // `is_interest_group_api_allowed_callback` will be called on all buyer and
  //  seller origins, and those for which it returns false will not be allowed
  //  to participate in the auction.
  //
  // `browser_signals` signals from the browser about the auction that are the
  //  same for all worklets.
  //
  //  `callback` is invoked on auction completion. It should synchronously
  //  destroy this AuctionRunner object. `callback` won't be invoked until after
  //  CreateAndStart() returns.
  static std::unique_ptr<AuctionRunner> CreateAndStart(
      AuctionWorkletManager* auction_worklet_manager,
      InterestGroupManagerImpl* interest_group_manager,
      const blink::AuctionConfig& auction_config,
      network::mojom::ClientSecurityStatePtr client_security_state,
      IsInterestGroupApiAllowedCallback is_interest_group_api_allowed_callback,
      RunAuctionCallback callback);

  ~AuctionRunner();

  // Fails the auction, invoking `callback_` and prevents any future calls into
  // `this` by closing mojo pipes and disposing of weak pointers. The owner must
  // be able to safely delete `this` when the callback is invoked. May only be
  // invoked if the auction has not yet completed.
  void FailAuction();

 private:
  class Auction;

  // TODO(mmenke): Move BidState, Bid, and ScoredBid into Auction.
  struct BidState {
    BidState();
    BidState(BidState&&);
    ~BidState();

    // Disable copy and assign, since this struct owns a
    // auction_worklet::mojom::BiddingInterestGroupPtr, and mojo classes are not
    // copiable.
    BidState(BidState&) = delete;
    BidState& operator=(BidState&) = delete;

    // Populates `trace_id` with a new trace ID and logs the first trace event
    // for it.
    void BeginTracing();

    // Logs the final event for `trace_id` and clears it. Automatically called
    // on destruction so trace events are all closed if an auction is cancelled.
    void EndTracing();

    StorageInterestGroup bidder;

    // Holds a reference to the BidderWorklet, once created.
    std::unique_ptr<AuctionWorkletManager::WorkletHandle> worklet_handle;

    // Tracing ID associated with the BidState. A nestable async "Bid" trace
    // event is started for a bid state during the generate and score bid phase
    // when the worklet is requested, and ended once the bid is score, or the
    // bidder worklet fails to bid.
    //
    // Additionally, if the BidState is a winner of a component auction, another
    // "Bid" trace event is created when the top-level auction scores the bid,
    // and ends when scoring is complete.
    //
    // Nested events are logged using this ID both by the Auction and by Mojo
    // bidder and seller worklets, potentially in another process.
    //
    // absl::nullopt means no ID is currently assigned, and there's no pending
    // event.
    absl::optional<uint64_t> trace_id;

    // True if the worklet successfully made a bid.
    bool made_bid = false;

    // URLs of forDebuggingOnly.reportAdAuctionLoss(url) and
    // forDebuggingOnly.reportAdAuctionWin(url) called in generateBid().
    // They support post auction signal placeholders in their URL string,
    // for example, "https://example.com/${highestScoringOtherBid}".
    // Placeholders will be replaced by corresponding values. For a component
    // auction, post auction signals are only from the component auction, but
    // not the top-level auction.
    absl::optional<GURL> bidder_debug_loss_report_url;
    absl::optional<GURL> bidder_debug_win_report_url;

    // URLs of forDebuggingOnly.reportAdAuctionLoss(url) and
    // forDebuggingOnly.reportAdAuctionWin(url) called in scoreAd(). In the case
    // of a component auction, these are the values from component seller that
    // the scored ad was created in, and post auction signals are from the
    // component auction.
    absl::optional<GURL> seller_debug_loss_report_url;
    absl::optional<GURL> seller_debug_win_report_url;

    // URLs of forDebuggingOnly.reportAdAuctionLoss(url) and
    // forDebuggingOnly.reportAdAuctionWin(url) called in scoreAd() from the
    // top-level seller, in the case this bidder was made in a component
    // auction, won it, and was then scored by the top-level seller.
    absl::optional<GURL> top_level_seller_debug_win_report_url;
    absl::optional<GURL> top_level_seller_debug_loss_report_url;
  };

  // Result of generated a bid. Contains information that needs to score a bid
  // and is persisted to the end of the auction if the bidder wins. Largely
  // duplicates auction_worklet::mojom::BidderWorkletBid, with additional
  // information about the bidder.
  struct Bid {
    Bid(std::string ad_metadata,
        double bid,
        GURL render_url,
        std::vector<GURL> ad_components,
        base::TimeDelta bid_duration,
        absl::optional<uint32_t> bidding_signals_data_version,
        const blink::InterestGroup::Ad* bid_ad,
        BidState* bid_state,
        Auction* auction);

    Bid(Bid&);

    ~Bid();

    // These are taken directly from the
    // auction_worklet::mojom::BidderWorkletBid.
    const std::string ad_metadata;
    const double bid;
    const GURL render_url;
    const std::vector<GURL> ad_components;
    const base::TimeDelta bid_duration;
    const absl::optional<uint32_t> bidding_signals_data_version;

    // InterestGroup that made the bid. Owned by the BidState of that
    // InterestGroup.
    const raw_ptr<const blink::InterestGroup, DanglingUntriaged> interest_group;

    // Points to the InterestGroupAd within `interest_group`.
    const raw_ptr<const blink::InterestGroup::Ad, DanglingUntriaged> bid_ad;

    // `bid_state` of the InterestGroup that made the bid. This should not be
    // written to, except for adding seller debug reporting URLs.
    const raw_ptr<BidState, DanglingUntriaged> bid_state;

    // The Auction with the interest group that made this bid. Important in the
    // case of component auctions.
    const raw_ptr<Auction, DanglingUntriaged> auction;
  };

  // Combines a Bid with seller score and seller state needed to invoke its
  // ReportResult() method.
  struct ScoredBid {
    ScoredBid(double score,
              absl::optional<uint32_t> scoring_signals_data_version,
              std::unique_ptr<Bid> bid,
              auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr
                  component_auction_modified_bid_params);
    ~ScoredBid();

    // The seller's desirability score for the bid.
    const double score;

    // The seller's scoring signals version.
    const absl::optional<uint32_t> scoring_signals_data_version;

    // The bid that came from the bidder or component Auction.
    const std::unique_ptr<Bid> bid;

    // Modifications that should be applied to `bid` before the parent
    // auction uses it. Only present for bids in component Auctions. When
    // the top-level auction creates a ScoredBid represending the result from
    // a component auction, the params have already been applied to the
    // underlying Bid, so the params are no longer needed.
    const auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr
        component_auction_modified_bid_params;
  };

  // Handles running an auction rooted at a given AuctionConfig. Separate from
  // AuctionRunner so that component auctions can use the same logic as the
  // top-level auction.
  //
  // When complete, Auctions will have three phases, with phase transitions
  // handled by the parent class. All phases complete asynchronously:
  //
  // * Loading interest groups phase: This loads interest groups that can
  // participate in an auction. Waiting for all component auctions to complete
  // this phase before advance to the next ensures that if any auctions share
  // bidder worklets, they'll all be loaded together, and only send out a single
  // trusted bidding signals request.
  //
  // * Bidding/scoring phase: This phase loads bidder and seller worklets,
  // generates bids, scores bids, and the highest scoring bid for each component
  // auction is passed to its parent auction, which also scores it. When this
  // phase completes, the winner will have been decided.
  //
  // * ReportResult / ReportWin phase: This phase invokes ReportResult() on
  // winning seller worklets and ReportWin() in the winning bidder worklet.
  class Auction {
   public:
    // Callback that's called when a phase of the Auction completes. Always
    // invoked asynchronously.
    using AuctionPhaseCompletionCallback =
        base::OnceCallback<void(bool success)>;

    // All passed in raw pointers must remain valid until the Auction is
    // destroyed. `config` is typically owned by the AuctionRunner's
    // `owned_auction_config_` field. `parent` should be the parent Auction if
    // this is a component auction, and null, otherwise.
    Auction(const blink::AuctionConfig* config,
            const Auction* parent,
            AuctionWorkletManager* auction_worklet_manager,
            InterestGroupManagerImpl* interest_group_manager,
            base::Time auction_start_time);

    Auction(const Auction&) = delete;
    Auction& operator=(const Auction&) = delete;

    ~Auction();

    // Starts loading the interest groups that can participate in an auction.
    //
    // Both seller and buyer origins are filtered by
    // `is_interest_group_api_allowed`, and any any not allowed to use the API
    // are excluded from participating in the auction.
    //
    // Invokes `load_interest_groups_phase_callback` asynchronously on
    // completion. Passes it false if there are no interest groups that may
    // participate in the auction (possibly because sellers aren't allowed to
    // participate in the auction)
    void StartLoadInterestGroupsPhase(
        IsInterestGroupApiAllowedCallback
            is_interest_group_api_allowed_callback,
        AuctionPhaseCompletionCallback load_interest_groups_phase_callback);

    // Starts bidding and scoring phase of the auction.
    //
    // `on_seller_receiver_callback`, if non-null, is invoked once the seller
    // worklet has been received, or if the seller worklet is no longer needed
    // (e.g., if all bidders fail to bid before the seller worklet has
    // been received). This is needed so that in the case of component auctions,
    // the top-level seller worklet will only be requested once all component
    // seller worklets have been received, to prevent deadlock (the top-level
    // auction could be waiting on a bid from a seller, while the top-level
    // seller worklet being is blocking a component seller worklet from being
    // created, due to the process limit). Unlike other callbacks,
    // `on_seller_receiver_callback` may be called synchronously.
    //
    // `bidding_and_scoring_phase_callback` is invoked asynchronously when
    // either the auction has failed to produce a winner, or the auction has a
    // winner. `success` is true only when there is a winner.
    void StartBiddingAndScoringPhase(
        base::OnceClosure on_seller_receiver_callback,
        AuctionPhaseCompletionCallback bidding_and_scoring_phase_callback);

    // Starts the reporting phase of the auction. Callback is invoked
    // asynchronously when either the auction has encountered a fatal error, or
    // when all reporting URLs (if any) have been retrieved from the applicable
    // worklets. `success` is true if the final status of the auction is
    // `kSuccess`.
    //
    // If this is a component auction, `top_seller_signals` must populated and
    // be the output from the top-level seller's reportResult() method.
    void StartReportingPhase(
        absl::optional<std::string> top_seller_signals,
        AuctionPhaseCompletionCallback reporting_phase_callback);

    // Close all Mojo pipes and release all weak pointers. Called when an
    // auction fails and on auction complete.
    void ClosePipes();

    // Returns all interest groups that bid in an auction. Expected to be called
    // after the bidding and scoring phase completes, but before the reporting
    // phase. Returns an empty set if the auction failed for any reason other
    // than the seller rejecting all bids.
    //
    // TODO(mmenke): Consider calling this after the reporting phase.
    void GetInterestGroupsThatBid(
        blink::InterestGroupSet& interest_groups) const;

    // Retrieves any debug reporting URLs. May only be called once, since it
    // takes ownership of stored reporting URLs.
    void TakeDebugReportUrls(std::vector<GURL>& debug_win_report_urls,
                             std::vector<GURL>& debug_loss_report_urls);

    // Retrieves the ad beacon map. May only be called once, since it takes
    // ownership of the stored ad beacon map.
    ReportingMetadata TakeAdBeaconMap() { return std::move(ad_beacon_map_); }

    // Retrieves any reporting URLs returned by ReportWin() and ReportResult()
    // methods. May only be called after an auction has completed successfully.
    // May only be called once, since it takes ownership of stored reporting
    // URLs.
    std::vector<GURL> TakeReportUrls();

    // Retrieves any errors from the auction. May only be called once, since it
    // takes ownership of stored errors.
    std::vector<std::string> TakeErrors();

    // Retrieves (by appending) all owners of interest groups that participated
    // in this auction (or any of its child auctions) that successfully loaded
    // at least one interest group. May only be called after the auction has
    // completed, for either success or failure. Duplication is possible,
    // particularly if an owner is listed in multiple auction components. May
    // only be called once, since it moves the stored origins.
    void TakePostAuctionUpdateOwners(std::vector<url::Origin>& owners);

    // Returns the top bid of the auction. May only be invoked after the
    // bidding and scoring phase has completed successfully.
    ScoredBid* top_bid();

   private:
    using AuctionList = std::list<std::unique_ptr<Auction>>;

    // ---------------------------------
    // Load interest group phase methods
    // ---------------------------------

    // Invoked whenever the interest groups for a buyer have loaded. Adds
    // `interest_groups` to `bid_states_`.
    void OnInterestGroupRead(std::vector<StorageInterestGroup> interest_groups);

    // Invoked when the interest groups for an entire component auction have
    // loaded. If `success` is false, removes the component auction.
    void OnComponentInterestGroupsRead(AuctionList::iterator component_auction,
                                       bool success);

    // Invoked when the interest groups for a buyer or for an entire component
    // auction have loaded. Completes the loading phase if no pending loads
    // remain.
    void OnOneLoadCompleted();

    // Invoked once the interest group load phase has completed. Never called
    // synchronously from StartLoadInterestGroupsPhase(), to avoid reentrancy
    // (AuctionRunner::callback_ cannot be invoked until
    // AuctionRunner::CreateAndStart() completes). `auction_result` is the
    // result of trying to load the interest groups that can participate in the
    // auction. It's AuctionResult::kSuccess if there are interest groups that
    // can take part in the auction, and a failure value otherwise.
    void OnStartLoadInterestGroupsPhaseComplete(AuctionResult auction_result);

    // -------------------------------------
    // Generate and score bids phase methods
    // -------------------------------------

    // Called when a component auction has received a worklet. Calls
    // RequestSellerWorklet() if all component auctions have received worklets.
    // See StartBiddingAndScoringPhase() for discussion of this.
    void OnComponentSellerWorkletReceived();

    // Requests a seller worklet from the AuctionWorkletManager.
    void RequestSellerWorklet();

    // Called when RequestSellerWorklet() returns. Starts scoring bids, if there
    // are any.
    void OnSellerWorkletReceived();

    // Requests bidder worklets from the AuctionWorkletManager for all bidders.
    void RequestBidderWorklets();

    // Invoked by the AuctionWorkletManager on fatal errors, at any point after
    // a SellerWorklet has been provided. Results in auction immediately
    // failing. Unlike most other methods, may be invoked during either the
    // generate bid phase or the reporting phase, since the seller worklet is
    // not unloaded between the two phases.
    void OnSellerWorkletFatalError(
        AuctionWorkletManager::FatalErrorType fatal_error_type,
        const std::vector<std::string>& errors);

    // Invoked whenever the AuctionWorkletManager has provided a BidderWorket
    // for the bidder identified by `bid_state`. Starts generating a bid.
    void OnBidderWorkletReceived(BidState* bid_state);

    // Calls SendPendingSignalsRequests() for the BidderWorklet of `bid_state`,
    // if it hasn't been destroyed. This is done asynchronously, so that
    // BidStates that share a BidderWorklet all call GenerateBid() before this
    // is invoked for all of them.
    //
    // This does result in invoking SendPendingSignalsRequests() multiple times
    // for BidStates that share BidderWorklets, though that should be fairly low
    // overhead.
    void SendPendingSignalsRequestsForBidder(BidState* bid_state);

    // Called when the `bid_state` BidderWorklet crashes or fails to load.
    // Invokes OnGenerateBidComplete() for the worklet with a failure.
    void OnBidderWorkletGenerateBidFatalError(
        BidState* bid_state,
        AuctionWorkletManager::FatalErrorType fatal_error_type,
        const std::vector<std::string>& errors);

    // Called once a bid has been generated, or has failed to be generated.
    // Releases the BidderWorklet handle and instructs the SellerWorklet to
    // start scoring the bid, if there is one.
    void OnGenerateBidComplete(
        BidState* state,
        auction_worklet::mojom::BidderWorkletBidPtr bid,
        uint32_t bidding_signals_data_version,
        bool has_bidding_signals_data_version,
        const absl::optional<GURL>& debug_loss_report_url,
        const absl::optional<GURL>& debug_win_report_url,
        double set_priority,
        bool has_set_priority,
        const std::vector<std::string>& errors);

    // True if all bid results and the seller script load are complete.
    bool AllBidsScored() const { return outstanding_bids_ == 0; }

    // Invoked when a component auction completes. If `success` is true, gets
    // the Bid from `component_auction` and passes a copy of it to ScoreBid().
    void OnComponentAuctionComplete(Auction* component_auction, bool success);

    // Called when either a bidder worklet's GenerateBid() method or a component
    // seller worklet's bid and scoring phase completes without creating a bid
    // (this includes worklet crashes and load failures).
    void OnNoBid();

    // Validates that `mojo_bid` is valid and, if it is, creates a Bid
    // corresponding to it, consuming it. Returns nullptr and calls
    // ReportBadMessage() if it's not valid. Does not mutate `bid_state`, but
    // the returned Bid has a non-const pointer to it.
    std::unique_ptr<Bid> TryToCreateBid(
        auction_worklet::mojom::BidderWorkletBidPtr mojo_bid,
        BidState& bid_state,
        const absl::optional<uint32_t>& bidding_signals_data_version,
        const absl::optional<GURL>& debug_loss_report_url,
        const absl::optional<GURL>& debug_win_report_url);

    // Calls into the seller asynchronously to score the passed in bid.
    void ScoreBidIfReady(std::unique_ptr<Bid> bid);

    // Callback from ScoreBid().
    void OnBidScored(
        std::unique_ptr<Bid> bid,
        double score,
        auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr
            component_auction_modified_bid_params,
        uint32_t scoring_signals_data_version,
        bool has_scoring_signals_data_version,
        const absl::optional<GURL>& debug_loss_report_url,
        const absl::optional<GURL>& debug_win_report_url,
        const std::vector<std::string>& errors);

    // Invoked when the bid becomes the new highest scoring other bid, to handle
    // calculation of post auction signals. `owner` is nullptr in the event the
    // bid is tied with the top bid, and they have different origins.
    void OnNewHighestScoringOtherBid(double score,
                                     double bid_value,
                                     const url::Origin* owner);

    absl::optional<std::string> PerBuyerSignals(const BidState* state);
    absl::optional<base::TimeDelta> PerBuyerTimeout(const BidState* state);
    absl::optional<base::TimeDelta> SellerTimeout();

    // If there are no `outstanding_bids_`, completes the bidding and scoring
    // phase.
    void MaybeCompleteBiddingAndScoringPhase();

    // Invoked when the bidding and scoring phase of an auction completes.
    // `auction_result` is AuctionResult::kSuccess if the auction has a winner,
    // and some other value otherwise. Appends `errors` to `errors_`.
    void OnBiddingAndScoringComplete(
        AuctionResult auction_result,
        const std::vector<std::string>& errors = {});

    // -----------------------
    // Reporting phase methods
    // -----------------------

    // Sequence of asynchronous methods to call into the seller and then bidder
    // worklet to report a win. Will ultimately invoke
    // `reporting_phase_callback_`, which will delete the auction.
    void ReportSellerResult(absl::optional<std::string> top_seller_signals);
    void OnReportSellerResultComplete(
        const absl::optional<std::string>& signals_for_winner,
        const absl::optional<GURL>& seller_report_url,
        const base::flat_map<std::string, GURL>& seller_ad_beacon_map,
        const std::vector<std::string>& error_msgs);
    void LoadBidderWorkletToReportBidWin(const std::string& signals_for_winner);
    void ReportBidWin(const std::string& signals_for_winner);
    void OnReportBidWinComplete(
        const absl::optional<GURL>& bidder_report_url,
        const base::flat_map<std::string, GURL>& bidder_ad_beacon_map,
        const std::vector<std::string>& error_msgs);

    // Called when the component SellerWorklet with the bidder that won an
    // auction has an out-of-band fatal error during the ReportResult() call.
    void OnWinningComponentSellerWorkletFatalError(
        AuctionWorkletManager::FatalErrorType fatal_error_type,
        const std::vector<std::string>& errors);

    // Called when the BidderWorklet that won an auction has an out-of-band
    // fatal error during the ReportWin() call.
    void OnWinningBidderWorkletFatalError(
        AuctionWorkletManager::FatalErrorType fatal_error_type,
        const std::vector<std::string>& errors);

    // Invoked when the nested component auction with the winning bid's
    // reporting phase is complete. Completes the reporting phase for `this`.
    void OnComponentAuctionReportingPhaseComplete(bool success);

    // Called when the final phase of the auction completes. Unconditionally
    // sets `final_auction_result`, even if `auction_result` is
    // AuctionResult::kSuccess, unlike other phase completion methods. Appends
    // `errors` to `errors_`.
    void OnReportingPhaseComplete(AuctionResult auction_result,
                                  const std::vector<std::string>& errors = {});

    // -----------------------------------
    // Methods not associated with a phase
    // -----------------------------------

    // Creates a ComponentAuctionOtherSeller to pass to SellerWorklets when
    // dealing with `bid`. If `this` is a component auction, returns an object
    // with a `top_level_seller`. If this is a top-level auction and `bid` comes
    // from a component auction, returns an object with a `component_seller` to
    // `bid's` seller.
    auction_worklet::mojom::ComponentAuctionOtherSellerPtr GetOtherSellerParam(
        const Bid& bid) const;

    // Requests a WorkletHandle for the interest group identified by
    // `bid_state`, using the provided callbacks. Returns true if a worklet was
    // received synchronously.
    [[nodiscard]] bool RequestBidderWorklet(
        BidState& bid_state,
        base::OnceClosure worklet_available_callback,
        AuctionWorkletManager::FatalErrorCallback fatal_error_callback);

    // Replace `${}` placeholders in debug report URLs for post auction signals
    // if exist.
    GURL FillPostAuctionSignals(const GURL& url,
                                const PostAuctionSignals& signals,
                                const absl::optional<PostAuctionSignals>&
                                    top_level_signals = absl::nullopt);

    // Tracing ID associated with the Auction. A nestable async "Auction" trace
    // event lasts for the lifetime of `this`. Sequential events that apply to
    // the entire auction are logged using this ID, including potentially
    // out-of-process events by bidder and seller worklet reporting methods.
    const uint64_t trace_id_;

    const raw_ptr<AuctionWorkletManager> auction_worklet_manager_;
    const raw_ptr<InterestGroupManagerImpl> interest_group_manager_;

    // Configuration of this auction.
    raw_ptr<const blink::AuctionConfig> config_;
    // If this is a component auction, the parent Auction. Null, otherwise.
    const raw_ptr<const Auction> parent_;

    // Component auctions that are part of this auction. This auction manages
    // their state transition, and their bids may participate in this auction as
    // well. Component auctions that fail in the load phase are removed from
    // this list, to avoid trying to load their worklets during the scoring
    // phase.
    AuctionList component_auctions_;

    // Final result of the auction, once completed. Null before completion.
    absl::optional<AuctionResult> final_auction_result_;

    // Each phases uses its own callback, to make sure that the right callback
    // is invoked when the phase completes.
    AuctionPhaseCompletionCallback load_interest_groups_phase_callback_;
    AuctionPhaseCompletionCallback bidding_and_scoring_phase_callback_;
    AuctionPhaseCompletionCallback reporting_phase_callback_;

    // Invoked in the bidding and scoring phase, once the seller worklet has
    // loaded. May be null.
    base::OnceClosure on_seller_receiver_callback_;

    // The number of buyers and component auctions with pending interest group
    // loads from storage. Decremented each time either the interest groups for
    // a buyer or all buyers for a component are read.
    // `load_interest_groups_phase_callback` is invoked once this hits 0.
    size_t num_pending_loads_ = 0;

    // True once a seller worklet has been received from the
    // AuctionWorkletManager.
    bool seller_worklet_received_ = false;

    // Number of bids that have yet to be sent to the SellerWorklet. This
    // includes BidderWorklets that have not yet been loaded, those whose
    // GenerateBid() method is currently being run, and those that are waiting
    // on the seller worklet to load, as well as component auctions that are
    // still running. When this reaches 0, the SellerWorklet's
    // SendPendingSignalsRequests() should be invoked, so it can send any
    // pending scoring signals requests.
    int num_bids_not_sent_to_seller_worklet_;

    // Number of bids which the seller has not yet finished scoring. This
    // includes bids included in `num_bids_not_sent_to_seller_worklet_`, as well
    // as any bids waiting on the seller worklet to load, and bids the seller
    // worklet is currently scoring. When this reaches 0, the bid with the
    // highest score is the winner, and bidding and scoring phase is completed.
    int outstanding_bids_;

    // The number of `component_auctions_` that have yet to request seller
    // worklets. Once it hits 0, the seller worklet for `this` is loaded. See
    // StartBiddingAndScoringPhase() for more details.
    size_t pending_component_seller_worklet_requests_ = 0;

    // State of all loaded interest groups.
    std::vector<BidState> bid_states_;

    // Bids waiting on the seller worklet to load before scoring. Does not
    // include bids that are currently waiting on the worklet's ScoreAd() method
    // to complete.
    std::vector<std::unique_ptr<Bid>> unscored_bids_;

    // The time the auction started. Use a single base time for all Worklets, to
    // present a more consistent view of the universe.
    const base::Time auction_start_time_;

    // The number of buyers in the AuctionConfig that passed the
    // IsInterestGroupApiAllowedCallback filter and interest groups were found
    // for. Includes buyers from nested component auctions. Double-counts buyers
    // in multiple Auctions.
    int num_owners_loaded_ = 0;

    // The number of buyers with InterestGroups participating in an auction.
    // Includes buyers from nested component auctions. Double-counts buyers in
    // multiple Auctions.
    int num_owners_with_interest_groups_ = 0;

    // A list of all buyer owners that participated in this auction and had at
    // least one interest group. These owners will have their interest groups
    // updated after a successful auction, barring rate-limiting.
    std::vector<url::Origin> post_auction_update_owners_;

    // A list of all interest groups that need to have their priority adjusted.
    // The new rates will be committed after a successful auction.
    std::vector<std::pair<blink::InterestGroupKey, double>>
        post_auction_priority_updates_;

    // The highest scoring bid so far. Null if no bid has been accepted yet.
    std::unique_ptr<ScoredBid> top_bid_;
    // Number of bidders with the same score as `top_bidder`.
    size_t num_top_bids_ = 0;
    // Number of bidders with the same score as `second_highest_score_`. If the
    // second highest score matches the highest score, this does not include the
    // top bid.
    size_t num_second_highest_bids_ = 0;

    // The numeric value of the bid that got the second highest score. When
    // there's a tie for the second highest score, one of the second highest
    // scoring bids is randomly chosen.
    double highest_scoring_other_bid_ = 0.0;
    double second_highest_score_ = 0.0;
    // Whether all bids of the highest score are from the same interest group
    // owner.
    bool at_most_one_top_bid_owner_ = true;
    // Will be null in the end if there are interest groups having the second
    // highest score with different owners. That includes the top bid itself, in
    // the case there's a tie for the top bid.
    absl::optional<url::Origin> highest_scoring_other_bid_owner_;

    // Holds a reference to the SellerWorklet used by the auction.
    std::unique_ptr<AuctionWorkletManager::WorkletHandle>
        seller_worklet_handle_;

    // Report URLs from reportResult() and reportWin() methods. An auction's
    // report URL from reportResult() comes before the URL from its reportWin()
    // method if there is one. Returned to `callback_` to deal with, so the
    // auction itself can be deleted at the end of the auction.
    std::vector<GURL> report_urls_;

    // All errors reported by worklets thus far.
    std::vector<std::string> errors_;

    // Ad Beacon URL mapping generated from reportResult() or reportWin() from
    // this auction and its components. Destination is relative to this auction.
    // Returned to `callback_` to deal with, so the Auction itself can be
    // deleted at the end of the auction.
    ReportingMetadata ad_beacon_map_;

    // This is set to true if the scoring phase ran and was able to score all
    // bids that were made (of which there may have been none). This is used to
    // gate accessors that should return nothing if the entire auction failed
    // (e.g., don't want to report bids as having "lost" an auction if the
    // seller failed to load, since neither the bids nor the bidders were the
    // problem).
    bool all_bids_scored_ = false;

    base::WeakPtrFactory<Auction> weak_ptr_factory_{this};
  };

  AuctionRunner(
      AuctionWorkletManager* auction_worklet_manager,
      InterestGroupManagerImpl* interest_group_manager,
      const blink::AuctionConfig& auction_config,
      network::mojom::ClientSecurityStatePtr client_security_state,
      IsInterestGroupApiAllowedCallback is_interest_group_api_allowed_callback,
      RunAuctionCallback callback);

  // Tells `auction_` to start the loading interest groups phase.
  void StartAuction();

  // Invoked asynchronously by `auction_` once all interest groups have loaded.
  // Fails the auction if `success` is false. Otherwise, starts the bidding and
  // scoring phase.
  void OnLoadInterestGroupsComplete(bool success);

  // Invoked asynchronously by `auction_` once the bidding and scoring phase is
  // complete. Records which bidders bid, if necessary, and either fails the
  // auction or starts the reporting phase, depending on the value of `success`.
  void OnBidsGeneratedAndScored(bool success);

  // Invoked asynchronously by `auction_` once the reporting phase has
  // completed. If `success` is false, fails the auction. Otherwise, records
  // which interest group won the auction and collects parameters needed to
  // invoke the auction callback.
  void OnReportingPhaseComplete(bool success);

  // After an auction completes (success or failure -- wherever `callback_` is
  // invoked), updates the set of interest groups that participated in the
  // auction.
  void UpdateInterestGroupsPostAuction();

  const raw_ptr<InterestGroupManagerImpl> interest_group_manager_;

  // ClientSecurityState built from the frame that issued the auction request;
  // will be used to update interest groups that participated in the auction
  // after the auction.
  network::mojom::ClientSecurityStatePtr client_security_state_;

  // For checking if operations like running auctions, updating interest groups,
  // etc. are allowed or not.
  IsInterestGroupApiAllowedCallback is_interest_group_api_allowed_callback_;

  // Configuration.
  blink::AuctionConfig owned_auction_config_;
  RunAuctionCallback callback_;

  Auction auction_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_AUCTION_RUNNER_H_
