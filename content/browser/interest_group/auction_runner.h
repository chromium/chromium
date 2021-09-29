// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_AUCTION_RUNNER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_AUCTION_RUNNER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/interest_group/auction_process_manager.h"
#include "content/browser/interest_group/interest_group_storage.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom-forward.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"

namespace content {

class AuctionURLLoaderFactoryProxy;
class DebuggableAuctionWorklet;
class InterestGroupManager;
class RenderFrameHostImpl;

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

  // Delegate class to allow dependency injection in tests. Note that all
  // objects this returns can crash and be restarted, so passing in raw pointers
  // would be problematic.
  class Delegate {
   public:
    // Returns the URLLoaderFactory of the frame running the auction. Used to
    // load the seller worklet in the context of the parent frame, since unlike
    // other worklet types, it has no first party opt-in, and it's not a
    // cross-origin leak if the parent from knows its URL, since the parent
    // frame provided the URL in the first place.
    virtual network::mojom::URLLoaderFactory* GetFrameURLLoaderFactory() = 0;

    // Trusted URLLoaderFactory used to load bidder worklets.
    virtual network::mojom::URLLoaderFactory* GetTrustedURLLoaderFactory() = 0;

    // Get containing frame. (Passed to debugging hooks).
    virtual RenderFrameHostImpl* GetFrame() = 0;
  };

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

  // Fails the auction, invoking `callback_` and preventis any future calls into
  // `this` by closing mojo pipes and disposing of weak pointers. The owner must
  // be able to safely delete `this` when the callback is invoked. May only be
  // invoked if the auction has not yet completed.
  //
  // `result` is used for logging purposes only.
  //
  // If `error` is non-null, it will be appended to `errors_`.
  //
  // Public so that the owner can fail the auction on teardown, to invoke any
  // pending Mojo callbacks.
  void FailAuction(AuctionResult result,
                   absl::optional<std::string> error = absl::nullopt);

  // Runs an entire FLEDGE auction.
  //
  // Arguments:
  // `delegate` and `interest_group_manager` must remain valid until the
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
      Delegate* delegate,
      InterestGroupManager* interest_group_manager,
      blink::mojom::AuctionAdConfigPtr auction_config,
      std::vector<url::Origin> filtered_buyers,
      auction_worklet::mojom::BrowserSignalsPtr browser_signals,
      const url::Origin& frame_origin,
      RunAuctionCallback callback);

  ~AuctionRunner();

 private:
  struct BidState {
    enum class State {
      // Waiting for all the interest groups to load, and then for the seller
      // worklet to get a process.
      kLoadingWorkletsAndOnSellerProcess,

      // Waiting for the AuctionProcessManager to provide a process usable for
      // this particular bidder worklet.
      kWaitingForProcess,

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

    // Convenient function to destroy `bidder_worklet`, `bidder_worklet_debug`,
    // and `process_handle`.
    // Safe to call if they're already null.
    void ClosePipes();

    State state = State::kLoadingWorkletsAndOnSellerProcess;

    BiddingInterestGroup bidder;

    // URLLoaderFactory proxy class configured only to load the URLs the bidder
    // needs.
    std::unique_ptr<AuctionURLLoaderFactoryProxy> url_loader_factory;

    std::unique_ptr<AuctionProcessManager::ProcessHandle> process_handle;

    mojo::Remote<auction_worklet::mojom::BidderWorklet> bidder_worklet;
    std::unique_ptr<DebuggableAuctionWorklet> bidder_worklet_debug;
    auction_worklet::mojom::BidderWorkletBidPtr bid_result;
    // Points to the InterestGroupAd within `bidder` that won the auction. Only
    // nullptr when `bid_result` is also nullptr.
    const blink::InterestGroup::Ad* bid_ad = nullptr;

    double seller_score = 0;
  };

  AuctionRunner(Delegate* delegate,
                InterestGroupManager* interest_group_manager,
                blink::mojom::AuctionAdConfigPtr auction_config,
                auction_worklet::mojom::BrowserSignalsPtr browser_signals,
                const url::Origin& frame_origin,
                RunAuctionCallback callback);

  // Starts retrieving all interest groups owned by `filtered_buyers` from
  // storage. OnInterestGroupRead() will be invoked with the lookup results for
  // each buyer.
  void ReadInterestGroups(std::vector<url::Origin> filtered_buyers);

  // Adds `interest_groups` to `bid_states_`. Continues retrieving bidders from
  // `pending_buyers_` if any have not been retrieved yet. Otherwise, invokes
  // StartBidding().
  void OnInterestGroupRead(std::vector<BiddingInterestGroup> interest_groups);

  // Request seller worklet process. No bidder processes are requested until a
  // seller worklet process has been received.
  void RequestSellerWorkletProcess();

  // Invoked once the AuctionProcessManager has provided a process for the
  // seller worklet. Starts loading the seller worklet, and requests processes
  // for all bidders.
  void OnSellerWorkletProcessReceived();

  // Invoked whenever the AuctionProcessManager has provided a process for a
  // bidder worklet. Starts loading the corresponding worklet and generating a
  // bid.
  void OnBidderWorkletProcessReceived(BidState* bid_state);

  void OnGenerateBidCrashed(BidState* state);
  void OnGenerateBidComplete(BidState* state,
                             auction_worklet::mojom::BidderWorkletBidPtr bid,
                             const std::vector<std::string>& errors);

  // True if all bid results and the seller script load are complete.
  bool AllBidsScored() const { return outstanding_bids_ == 0; }
  void OnSellerWorkletLoaded(bool load_result,
                             const std::vector<std::string>& errors);

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
  // report a a win, Will ultimately invoke ReportSuccess(), which will delete
  // the auction.
  void ReportSellerResult();
  void OnReportSellerResultComplete(
      const absl::optional<std::string>& signals_for_winner,
      const absl::optional<GURL>& seller_report_url,
      const std::vector<std::string>& error_msgs);
  void ReportBidWin(const absl::optional<std::string>& signals_for_winner);
  void OnReportBidWinComplete(const absl::optional<GURL>& bidder_report_url,
                              const std::vector<std::string>& error_msgs);

  // Completes the auction, invoking `callback_` and preventing any future
  // calls into `this` by closing mojo pipes and disposing of weak pointers. The
  // owner must be able to safely delete `this` when the callback is invoked.
  void ReportSuccess();

  // Closes all open pipes, to avoid receiving any Mojo callbacks after
  // completion.
  void ClosePipes();

  // Logs the result of the auction to UMA.
  void RecordResult(AuctionResult result) const;

  Delegate* const delegate_;
  InterestGroupManager* const interest_group_manager_;

  // Configuration.
  blink::mojom::AuctionAdConfigPtr auction_config_;
  // The number of buyers with pending interest group loads from storage.
  // Decremented each time OnInterestGroupRead() is invoked. The auction is
  // started once this hits 0.
  size_t num_pending_buyers_ = 0;
  auction_worklet::mojom::BrowserSignalsPtr browser_signals_;
  const url::Origin frame_origin_;
  RunAuctionCallback callback_;

  // Number of bids which the seller has not yet scored. These bids may be
  // fetching URLs, generating bids, waiting for the seller worklet to load, or
  // the seller worklet may be scoring their bids.
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
  BidState* top_bidder_ = nullptr;
  // Number of bidders with the same score as `top_bidder`.
  size_t num_top_bidders_ = 0;

  // URLLoaderFactory proxy class configured only to load the URL the seller
  // needs.
  std::unique_ptr<AuctionURLLoaderFactoryProxy> seller_url_loader_factory_;

  // State for the scoring phase.
  std::unique_ptr<AuctionProcessManager::ProcessHandle>
      seller_worklet_process_handle_;
  mojo::Remote<auction_worklet::mojom::SellerWorklet> seller_worklet_;
  std::unique_ptr<DebuggableAuctionWorklet> seller_worklet_debug_;

  // This is true if the seller script has been loaded successfully --- if the
  // load failed, the entire process is aborted since there is nothing useful
  // that can be done.
  bool seller_loaded_ = false;

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
