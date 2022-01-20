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
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"

namespace content {

class InterestGroupManager;

// An AuctionRunner loads and runs the bidder and seller worklets, along with
// their reporting phases and produces the result via a callback.
class CONTENT_EXPORT AuctionRunner {
 public:
  // Invoked when a FLEDGE auction is complete.
  //
  // `render_url` URL of auction winning ad to render. Null if there is no
  // winner.
  //
  // `ad_component_urls` is the list of ad component URLs returned by the
  // winning bidder. Null if there is no winner or no list was returned.
  //
  // `bidder_report_url` URL to use for reporting result to the bidder. Null if
  //  no report should be sent.
  //
  // `seller_report`  URL to use for reporting result to the seller. Null if no
  //  report should be sent.
  //
  // `errors` are various error messages to be used for debugging. These are too
  //  sensitive for the renderers to see.
  using RunAuctionCallback = base::OnceCallback<void(
      AuctionRunner* auction_runner,
      const absl::optional<GURL> render_url,
      const absl::optional<std::vector<GURL>> ad_component_urls,
      const absl::optional<GURL> bidder_report_url,
      const absl::optional<GURL> seller_report_url,
      std::vector<std::string> errors)>;

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

    kMaxValue = kWinningBidderWorkletCrashed
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
  // `errors` is appended to `errors_`.
  //
  // Public so that the owner can fail the auction on teardown, to invoke any
  // pending Mojo callbacks.
  void FailAuction(AuctionResult result,
                   const std::vector<std::string>& errors = {});

  // Runs an entire FLEDGE auction.
  //
  // Arguments:
  // `auction_worklet_manager`, `auction_worklet_manager_delegate`, and
  // `interest_group_manager` must remain valid until the
  //  AuctionRunner is destroyed.
  //
  // `auction_config` is the configuration provided by client JavaScript in
  //  the renderer in order to initiate the auction.
  //
  // `filtered_buyers` owners of bidders allowed to participate in this auction.
  //  These should be a subset of `auction_config`'s `interest_group_buyers`,
  //  filtered to account for browser configuration (like cookie blocking). Must
  //  not be empty.
  //
  // `browser_signals` signals from the browser about the auction that are the
  //  same for all worklets.
  //
  // `frame_origin` is the origin running the auction (not the top frame
  // origin), used as the initiator in network requests.
  static std::unique_ptr<AuctionRunner> CreateAndStart(
      AuctionWorkletManager* auction_worklet_manager,
      AuctionWorkletManager::Delegate* auction_worklet_manager_delegate,
      InterestGroupManager* interest_group_manager,
      blink::mojom::AuctionAdConfigPtr auction_config,
      std::vector<url::Origin> filtered_buyers,
      const url::Origin& frame_origin,
      RunAuctionCallback callback);

  ~AuctionRunner();

 private:
  struct BidState {
    enum class State {
      // Waiting for all the interest groups to load, and then for the seller
      // worklet to get a process.
      kLoadingWorkletsAndOnSellerProcess,

      // Waiting for the AuctionWorkletManager to provide a BidderWorklet.
      kWaitingForWorklet,

      // Loading the bidder worklet script / trusted data and generating the
      // bid.
      kGeneratingBid,

      // Waiting on the seller worklet to load.
      kWaitingOnSellerWorkletLoad,

      // Waiting on the seller worklet to score the bid.
      kSellerScoringBid,

      // Seller worklet has completed scoring the bid, or doesn't need to. If
      // this is not potentially the winning bidder, the worklet has been
      // unloaded. Otherwise, the worklet is still in memory, as it may still be
      // necessary to call reporting methods, if this is the winning bidder.
      kScoringComplete,
    };

    BidState();
    BidState(BidState&&);
    ~BidState();

    // Disable copy and assign, since this struct owns a
    // auction_worklet::mojom::BiddingInterestGroupPtr, and mojo classes are not
    // copiable.
    BidState(BidState&) = delete;
    BidState& operator=(BidState&) = delete;

    State state = State::kLoadingWorkletsAndOnSellerProcess;

    StorageInterestGroup bidder;

    // Holds a reference to the BidderWorklet, once created.
    std::unique_ptr<AuctionWorkletManager::WorkletHandle> worklet_handle;

    auction_worklet::mojom::BidderWorkletBidPtr bid_result;
    // Points to the InterestGroupAd within `bidder` that won the auction. Only
    // nullptr when `bid_result` is also nullptr.
    raw_ptr<const blink::InterestGroup::Ad> bid_ad = nullptr;

    double seller_score = 0;
  };

  AuctionRunner(
      AuctionWorkletManager* auction_worklet_manager,
      AuctionWorkletManager::Delegate* auction_worklet_manager_delegate,
      InterestGroupManager* interest_group_manager,
      blink::mojom::AuctionAdConfigPtr auction_config,
      const url::Origin& frame_origin,
      RunAuctionCallback callback);

  // Starts retrieving all interest groups owned by `filtered_buyers` from
  // storage. OnInterestGroupRead() will be invoked with the lookup results for
  // each buyer.
  void ReadInterestGroups(std::vector<url::Origin> filtered_buyers);

  // Adds `interest_groups` to `bid_states_`. Continues retrieving bidders from
  // `pending_buyers_` if any have not been retrieved yet. Otherwise, invokes
  // StartBidding().
  void OnInterestGroupRead(std::vector<StorageInterestGroup> interest_groups);

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
                             const std::vector<std::string>& errors);

  // True if all bid results and the seller script load are complete.
  bool AllBidsScored() const { return outstanding_bids_ == 0; }

  // Calls into the seller asynchronously to score the passed in bid.
  void ScoreBid(BidState* state);
  // Callback from ScoreBid().
  void OnBidScored(BidState* state,
                   double score,
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
  const raw_ptr<AuctionWorkletManager::Delegate>
      auction_worklet_manager_delegate_;
  const raw_ptr<InterestGroupManager> interest_group_manager_;

  // Configuration.
  blink::mojom::AuctionAdConfigPtr auction_config_;
  // The number of buyers with pending interest group loads from storage.
  // Decremented each time OnInterestGroupRead() is invoked. The auction is
  // started once this hits 0.
  size_t num_pending_buyers_ = 0;
  const url::Origin frame_origin_;
  RunAuctionCallback callback_;

  // True once a seller worklet has been received from the
  // AuctionWorkletManager.
  bool seller_worklet_received_ = false;

  // Number of bids that have yet to be sent to the SellerWorklet. This
  // includes BidderWorklets that have not yet been loaded, those whose
  // GenerateBid() method is currently being run, and those that are waiting on
  // the seller worklet to load. Decremented when GenerateBid() fails to
  // generate a bid, or just after invoking the SellerWorklet's ScoreAd()
  // method. When this reaches 0, the SellerWorklet's
  // SendPendingSignalsRequests() should be invoked, so it can send any pending
  // scoring signals requests.
  int num_bids_not_sent_to_seller_worklet_;
  // Number of bids which the seller has not yet finished scoring. These bids
  // may be fetching URLs, generating bids, waiting for the seller worklet to
  // load, or the seller worklet may be scoring their bids. When this reaches 0,
  // the bid with the highest score is the winner, and the auction is completed,
  // apart from reporting the result.
  int outstanding_bids_;

  // State of all loaded interest groups.
  std::vector<BidState> bid_states_;
  // The time the auction started. Use a single base time for all Worklets, to
  // present a more consistent view of the universe.
  const base::Time auction_start_time_ = base::Time::Now();

  // The number of owners with InterestGroups participating in an auction.
  int num_owners_with_interest_groups_ = 0;

  // The bidder with the highest scoring bid so far. No other scored bidder
  // worklet can win the auction, so the other worklets are all unloaded right
  // after scoring.
  raw_ptr<BidState> top_bidder_ = nullptr;
  // Number of bidders with the same score as `top_bidder`.
  size_t num_top_bidders_ = 0;

  // Holds a reference to the SellerWorklet used by the auction.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> seller_worklet_handle_;

  // Seller script reportResult() results.
  absl::optional<GURL> seller_report_url_;

  // Bidder script reportWin() results.
  absl::optional<GURL> bidder_report_url_;

  // All errors reported by worklets thus far.
  std::vector<std::string> errors_;

  base::WeakPtrFactory<AuctionRunner> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_AUCTION_RUNNER_H_
