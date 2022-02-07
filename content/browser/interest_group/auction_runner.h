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
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/interest_group/auction_worklet_manager.h"
#include "content/browser/interest_group/interest_group_storage.h"
#include "content/common/content_export.h"
#include "content/public/browser/content_browser_client.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"

namespace content {

class InterestGroupManagerImpl;

// An AuctionRunner loads and runs the bidder and seller worklets, along with
// their reporting phases and produces the result via a callback.
class CONTENT_EXPORT AuctionRunner {
 public:
  // Invoked when a FLEDGE auction is complete.
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
      absl::optional<GURL> render_url,
      absl::optional<std::vector<GURL>> ad_component_urls,
      std::vector<GURL> report_urls,
      std::vector<GURL> debug_loss_report_urls,
      std::vector<GURL> debug_win_report_urls,
      std::vector<std::string> errors)>;

  // Returns true if `origin` is allowed to use the interest group API. Will be
  // called on worklet / interest group origins before using them in any
  // interest group API.
  using IsInterestGroupApiAllowedCallback = base::RepeatingCallback<bool(
      ContentBrowserClient::InterestGroupApiOperation
          interest_group_api_operation,
      const url::Origin& origin)>;

  // Result of an auction. Used for histograms. Only recorded for valid
  // auctions. These are used in histograms, so values of existing entries must
  // not change when adding/removing values, and obsolete values must not be
  // reused.
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

    kMaxValue = kSellerRejected
  };

  explicit AuctionRunner(const AuctionRunner&) = delete;
  AuctionRunner& operator=(const AuctionRunner&) = delete;

  // Fails the auction, invoking `callback_` and prevents any future calls into
  // `this` by closing mojo pipes and disposing of weak pointers. The owner must
  // be able to safely delete `this` when the callback is invoked. May only be
  // invoked if the auction has not yet completed.
  //
  // `result` is used for logging purposes only.
  //
  //`errors` is appended to `errors_`.
  //
  // Public so that the owner can fail the auction on teardown, to invoke any
  // pending Mojo callbacks.
  void FailAuction(AuctionResult result,
                   const std::vector<std::string>& errors = {});

  // Runs an entire FLEDGE auction.
  //
  // Arguments:
  // `auction_worklet_manager` and `interest_group_manager` must remain valid
  //  until the  AuctionRunner is destroyed.
  //
  // `auction_config` is the configuration provided by client JavaScript in
  //  the renderer in order to initiate the auction.
  //
  // `is_interest_group_api_allowed_callback` will be called on all buyer and
  //  seller origins, and those for which it returns false will not be allowed
  //  to participate in the auction.
  //
  // `browser_signals` signals from the browser about the auction that are the
  //  same for all worklets.
  static std::unique_ptr<AuctionRunner> CreateAndStart(
      AuctionWorkletManager* auction_worklet_manager,
      InterestGroupManagerImpl* interest_group_manager,
      blink::mojom::AuctionAdConfigPtr auction_config,
      IsInterestGroupApiAllowedCallback is_interest_group_api_allowed_callback,
      RunAuctionCallback callback);

  ~AuctionRunner();

 private:
  struct BidState {
    BidState();
    BidState(BidState&&);
    ~BidState();

    // Disable copy and assign, since this struct owns a
    // auction_worklet::mojom::BiddingInterestGroupPtr, and mojo classes are not
    // copiable.
    BidState(BidState&) = delete;
    BidState& operator=(BidState&) = delete;

    StorageInterestGroup bidder;

    // Holds a reference to the BidderWorklet, once created.
    std::unique_ptr<AuctionWorkletManager::WorkletHandle> worklet_handle;

    // True if the worklet successfully made a bid.
    bool made_bid = false;

    // URLs of forDebuggingOnly.reportAdAuctionLoss(url) and
    // forDebuggingOnly.reportAdAuctionWin(url) called in generateBid().
    absl::optional<GURL> bidder_debug_loss_report_url;
    absl::optional<GURL> bidder_debug_win_report_url;

    // URLs of forDebuggingOnly.reportAdAuctionLoss(url) and
    // forDebuggingOnly.reportAdAuctionWin(url) called in scoreAd().
    absl::optional<GURL> seller_debug_loss_report_url;
    absl::optional<GURL> seller_debug_win_report_url;
  };

  // Result of generated a bid. Contains information that needs to score a bid
  // and is persisted to the end of the auction if the bidder wins. Largely
  // duplicates auction_worklet::mojom::BidderWorkletBid, with additional
  // information about the bidder.
  struct Bid {
    Bid(std::string ad_metadata,
        double bid,
        GURL render_url,
        absl::optional<std::vector<GURL>> ad_components,
        base::TimeDelta bid_duration,
        const blink::InterestGroup::Ad* bid_ad,
        BidState* bid_state);

    ~Bid();

    // These are taken directly from the
    // auction_worklet::mojom::BidderWorkletBid.
    const std::string ad_metadata;
    const double bid;
    const GURL render_url;
    const absl::optional<std::vector<GURL>> ad_components;
    const base::TimeDelta bid_duration;

    // InterestGroup that made the bid. Owned by the BidState of that
    // InterestGroup.
    const raw_ptr<const blink::InterestGroup> interest_group;

    // Points to the InterestGroupAd within `interest_group`.
    const raw_ptr<const blink::InterestGroup::Ad> bid_ad;

    // `bid_state` of the InterestGroup that made the bid. This should not be
    // writted to, except for adding seller debug reporting URLs.
    const raw_ptr<BidState> bid_state;
  };

  // Combines a Bid with seller score and seller state needed to invoke its
  // ReportResult() method.
  struct ScoredBid {
    ScoredBid(double score,
              absl::optional<uint32_t> data_version,
              std::unique_ptr<Bid> bid);
    ~ScoredBid();

    const double score;
    const absl::optional<uint32_t> data_version;

    const std::unique_ptr<Bid> bid;
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
    // Callback that's invoked once interest groups have been loaded. Always
    // invoked asynchronously.
    using LoadInterestGroupsCallback = base::OnceCallback<void(bool success)>;

    // All passed in raw pointers must remain valid until the Auction is
    // destroyed. `config` is typically owned by the AuctionRunner's
    // `owned_auction_config_` field.
    Auction(blink::mojom::AuctionAdConfig* config,
            AuctionWorkletManager* auction_worklet_manager,
            InterestGroupManagerImpl* interest_group_manager,
            base::Time auction_start_time);

    ~Auction();

    // Starts loading the interest groups that can participate in an auction.
    //
    // Both seller and buyer origins are filtered by
    // `is_interest_group_api_allowed`, and any any not allowed to use the API
    // are excluded from participating in the auction.
    //
    // Invokes `load_interest_groups_callback` asynchronously on completion.
    // Passes it false if there are no interest groups that may participate in
    // the auction (possibly because sellers aren't allowed to participate in
    // the auction)
    void LoadInterestGroups(
        IsInterestGroupApiAllowedCallback
            is_interest_group_api_allowed_callback,
        LoadInterestGroupsCallback load_interest_groups_callback);

   private:
    // TODO(mmenke): Remove this once Auction fully manages its own state.
    friend class AuctionRunner;

    // Adds `interest_groups` to `bid_states_`. If all bidders have been loaded,
    // calls OnLoadInterestGroupsComplete().
    void OnInterestGroupRead(std::vector<StorageInterestGroup> interest_groups);

    // Invoked once the interest group load phase has completed. Never called
    // synchronously from LoadInterestGroups(), to avoid reentrancy.
    // `auction_result` is the result of trying to load the interest groups that
    // can participate in the auction. It's AuctionResult::kSuccess if there are
    // interest groups that can take part in the auction, and a failure value
    // otherwise.
    void OnLoadInterestGroupsComplete(AuctionResult auction_result);

    const raw_ptr<AuctionWorkletManager> auction_worklet_manager_;
    const raw_ptr<InterestGroupManagerImpl> interest_group_manager_;

    // Configuration of this auction.
    raw_ptr<const blink::mojom::AuctionAdConfig> config_;

    // Final result of the auction, once completed. Null before completion.
    absl::optional<AuctionResult> final_auction_result_;

    // The number of buyers with pending interest group loads from storage.
    // Decremented each time OnInterestGroupRead() is invoked.
    // `load_interest_groups_callback` is invoked once this hits 0.
    size_t num_pending_buyers_ = 0;
    LoadInterestGroupsCallback load_interest_groups_callback_;

    // True once a seller worklet has been received from the
    // AuctionWorkletManager.
    bool seller_worklet_received_ = false;

    // Number of bids that have yet to be sent to the SellerWorklet. This
    // includes BidderWorklets that have not yet been loaded, those whose
    // GenerateBid() method is currently being run, and those that are waiting
    // on the seller worklet to load. Decremented when GenerateBid() fails to
    // generate a bid, or just after invoking the SellerWorklet's ScoreAd()
    // method. When this reaches 0, the SellerWorklet's
    // SendPendingSignalsRequests() should be invoked, so it can send any
    // pending scoring signals requests.
    int num_bids_not_sent_to_seller_worklet_;
    // Number of bids which the seller has not yet finished scoring. These bids
    // may be fetching URLs, generating bids, waiting for the seller worklet to
    // load, or the seller worklet may be scoring their bids. When this reaches
    // 0, the bid with the highest score is the winner, and the auction is
    // completed, apart from reporting the result.
    int outstanding_bids_;

    // State of all loaded interest groups.
    std::vector<BidState> bid_states_;

    // Bids waiting on the seller worklet to load before scoring. Does not
    // include bids that are currently waiting on the worklet's ScoreAd() method
    // to complete.
    std::vector<std::unique_ptr<Bid>> unscored_bids_;

    // The time the auction started. Use a single base time for all Worklets, to
    // present a more consistent view of the universe.
    const base::Time auction_start_time_;

    // The number of owners with InterestGroups participating in an auction.
    int num_owners_with_interest_groups_ = 0;

    // The highest scoring bid so far. Null if no bid has been accepted yet.
    std::unique_ptr<ScoredBid> top_bid_;
    // Number of bidders with the same score as `top_bidder`.
    size_t num_top_bids_ = 0;

    // Holds a reference to the SellerWorklet used by the auction.
    std::unique_ptr<AuctionWorkletManager::WorkletHandle>
        seller_worklet_handle_;

    // Report URLs from reportResult() and reportWin() methods. Returned to
    // caller for it to deal with, so the Auction itself can be deleted at the
    // end of the auction.
    std::vector<GURL> report_urls_;

    // All errors reported by worklets thus far.
    std::vector<std::string> errors_;

    base::WeakPtrFactory<Auction> weak_ptr_factory_{this};
  };

  AuctionRunner(AuctionWorkletManager* auction_worklet_manager,
                InterestGroupManagerImpl* interest_group_manager,
                blink::mojom::AuctionAdConfigPtr auction_config,
                RunAuctionCallback callback);

  // Validates that `mojo_bid` is valid and, if it is, creates a Bid
  // corresponding to it, consuming it. Returns nullptr and calls
  // ReportBadMessage() if it's not valid. Does not mutate `bid_state`, but the
  // returned Bid has a non-const pointer to it.
  static std::unique_ptr<Bid> TryToCreateBid(
      auction_worklet::mojom::BidderWorkletBidPtr mojo_bid,
      BidState& bid_state,
      const absl::optional<GURL>& debug_loss_report_url,
      const absl::optional<GURL>& debug_win_report_url);

  // Tells `auction_` to start loading interest groups.
  void StartAuction(
      IsInterestGroupApiAllowedCallback is_interest_group_api_allowed_callback);

  // Invoked asynchronously by `auction_` once all interest groups have loaded.
  // Fails the auction if `success` is false. Otherwise, starts loading
  // worklets.
  void OnInterestGroupsLoaded(bool success);

  // Requests a seller worklet from the AuctionWorkletManager.
  void RequestSellerWorklet();

  // Called when RequestSellerWorklet() returns. Starts scoring bids, if there
  // are any.
  void OnSellerWorkletReceived();

  // Requests bidder worklets from the AuctionWorkletManager for all bidders.
  void RequestBidderWorklets();

  // Invoked by the SellerWorkletManager on fatal errors, at any point after a
  // SellerWorklet has been provided. Results in auction immediately failing.
  void OnSellerWorkletFatalError(
      AuctionWorkletManager::FatalErrorType fatal_error_type,
      const std::vector<std::string>& errors);

  // Invoked whenever the AuctionWorkletManager has provided a BidderWorket for
  // the bidder identified by `bid_state`. Starts generating a bid.
  void OnBidderWorkletReceived(BidState* bid_state);

  // Calls SendPendingSignalsRequests() for the BidderWorklet of `bid_state`, if
  // it hasn't been destroyed. This is done asynchronously, so that BidStates
  // that share a BidderWorklet all call GenerateBid() before this is invoked
  // for all of them.
  //
  // This does result in invoking SendPendingSignalsRequests() multiple times
  // for BidStates that share BidderWorklets, though that should be fairly low
  // overhead.
  void SendPendingSignalsRequestsForBidder(BidState* bid_state);

  // Called when the `bid_state` BidderWorklet crashes or fails to load, and
  // `bid_state` is in state kGeneratingBid. Fails the GenerateBid() call and
  // releases the worklet handle, as the callback passed to the GenerateBid Mojo
  // call will not be invoked after this method is.
  void OnBidderWorkletGenerateBidFatalError(
      BidState* bid_state,
      AuctionWorkletManager::FatalErrorType fatal_error_type,
      const std::vector<std::string>& errors);

  // Called once a bid has been generated, or has failed to be generated.
  // Releases the BidderWorklet handle and instructs the SellerWorklet to start
  // scoring the bid, if there is one.
  void OnGenerateBidComplete(BidState* state,
                             auction_worklet::mojom::BidderWorkletBidPtr bid,
                             const absl::optional<GURL>& debug_loss_report_url,
                             const absl::optional<GURL>& debug_win_report_url,
                             const std::vector<std::string>& errors);

  // True if all bid results and the seller script load are complete.
  bool AllBidsScored() const { return auction_.outstanding_bids_ == 0; }

  // Calls into the seller asynchronously to score the passed in bid.
  void ScoreBid(std::unique_ptr<Bid> bid);

  // Callback from ScoreBid().
  void OnBidScored(std::unique_ptr<Bid> bid,
                   double score,
                   uint32_t data_version,
                   bool has_data_version,
                   const absl::optional<GURL>& debug_loss_report_url,
                   const absl::optional<GURL>& debug_win_report_url,
                   const std::vector<std::string>& errors);

  std::string AdRenderFingerprint(const BidState* state);
  absl::optional<std::string> PerBuyerSignals(const BidState* state);

  // If there are no `outstanding_bids_`, starts starts completing the auction,
  // either invoking `callback_` or calling reporting methods on worklets.
  // Consumer must be able to safely delete `this` when the callback is invoked.
  void MaybeCompleteAuction();

  // Sequence of asynchronous methods to call into the bidder/seller results to
  // report a a win. Will ultimately invoke ReportSuccess(), which will delete
  // the auction.
  void ReportSellerResult();
  void OnReportSellerResultComplete(
      const absl::optional<std::string>& signals_for_winner,
      const absl::optional<GURL>& seller_report_url,
      const std::vector<std::string>& error_msgs);
  void LoadBidderWorkletToReportBidWin(
      const absl::optional<std::string>& signals_for_winner);
  void ReportBidWin(const absl::optional<std::string>& signals_for_winner);
  void OnReportBidWinComplete(const absl::optional<GURL>& bidder_report_url,
                              const std::vector<std::string>& error_msgs);

  // Called when the BidderWorklet that won an auction has an out-of-band fatal
  // error during the ReportWin() call.
  void OnWinningBidderWorkletFatalError(
      AuctionWorkletManager::FatalErrorType fatal_error_type,
      const std::vector<std::string>& errors);

  // Completes the auction, invoking `callback_` and preventing any future
  // calls into `this` by closing mojo pipes and disposing of weak pointers. The
  // owner must be able to safely delete `this` when the callback is invoked.
  void ReportSuccess();

  // Closes all open pipes, to avoid receiving any Mojo callbacks after
  // completion.
  void ClosePipes();

  // Logs the result of the auction to UMA.
  void RecordResult(AuctionResult result) const;

  // Requests a WorkletHandle for the interest group identified by `bid_state`,
  // using the provided callbacks. Returns true if a worklet was received
  // synchronously.
  [[nodiscard]] bool RequestBidderWorklet(
      BidState& bid_state,
      base::OnceClosure worklet_available_callback,
      AuctionWorkletManager::FatalErrorCallback fatal_error_callback);

  const raw_ptr<AuctionWorkletManager> auction_worklet_manager_;
  const raw_ptr<InterestGroupManagerImpl> interest_group_manager_;

  // Configuration.
  blink::mojom::AuctionAdConfigPtr owned_auction_config_;
  RunAuctionCallback callback_;

  Auction auction_;

  // URLs of forDebuggingOnly.reportAdAuctionLoss(url) and
  // forDebuggingOnly.reportAdAuctionWin(url).
  std::vector<GURL> debug_loss_report_urls_;
  std::vector<GURL> debug_win_report_urls_;

  base::WeakPtrFactory<AuctionRunner> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_AUCTION_RUNNER_H_
