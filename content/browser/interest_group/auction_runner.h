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
#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom-forward.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"

namespace content {

class AuctionURLLoaderFactoryProxy;
class InterestGroupManager;

// An AuctionRunner loads and runs the bidder and seller worklets, along with
// their reporting phases and produces the result via a callback.
//
// At present it initiates all fetches in parallel, running all bidder scripts
// once they and any trusted signals they need are ready, then when all bids are
// in runs all the scoring, and finally the reporting worklets.
//
// TODO(morlovich): There is no need to wait for all bidders to finish to start
// scoring.
//
// TODO(mmenke): Merge this with `ad_auction`, and provide worklet-specific
// URLLoaderFactories.
//
// TODO(mmenke): Add checking of values returned by auctions (e.g., for bids <=
// 0).
class CONTENT_EXPORT AuctionRunner {
 public:
  // Invoked when a FLEDGE auction is complete.
  //
  // `render_url` URL of auction winning ad to render. Null if there is no
  // winner.
  //
  // `bidder_report_url` URL to use for reporting result to the bidder. Null if
  //  no report should be sent.
  //
  // `seller_report`  URL to use for reporting result to the seller. Null if no
  //  report should be sent.
  //
  // `errors` are various error messages to be used for debugging. These are too
  //  sensitive for the renderers to see.
  using RunAuctionCallback =
      base::OnceCallback<void(AuctionRunner* auction_runner,
                              const absl::optional<GURL> render_url,
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

    // Returns the AuctionWorkletService.
    virtual auction_worklet::mojom::AuctionWorkletService*
    GetWorkletService() = 0;
  };

  explicit AuctionRunner(const AuctionRunner&) = delete;
  AuctionRunner& operator=(const AuctionRunner&) = delete;

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
    BidState();
    BidState(BidState&&);
    ~BidState();

    // Disable copy and assign, since this struct owns a
    // auction_worklet::mojom::BiddingInterestGroupPtr, and mojo classes are not
    // copiable.
    BidState(BidState&) = delete;
    BidState& operator=(BidState&) = delete;

    auction_worklet::mojom::BiddingInterestGroupPtr bidder;

    // URLLoaderFactory proxy class configured only to load the URLs the bidder
    // needs.
    std::unique_ptr<AuctionURLLoaderFactoryProxy> url_loader_factory_;

    mojo::Remote<auction_worklet::mojom::BidderWorklet> bidder_worklet;
    auction_worklet::mojom::BidderWorkletBidPtr bid_result;
    // Points to the InterestGroupAd within `bidder` that won the auction. Only
    // nullptr when `bid_result` is also nullptr.
    blink::mojom::InterestGroupAd* bid_ad = nullptr;

    double seller_score = 0;
  };

  AuctionRunner(Delegate* delegate,
                InterestGroupManager* interest_group_manager,
                blink::mojom::AuctionAdConfigPtr auction_config,
                std::vector<url::Origin> filtered_buyers,
                auction_worklet::mojom::BrowserSignalsPtr browser_signals,
                const url::Origin& frame_origin,
                RunAuctionCallback callback);

  // Retrieves the next interest group in `pending_buyers_` from storage.
  // OnInterestGroupRead() will be invoked with the lookup results.
  void ReadNextInterestGroup();

  // Adds `interest_groups` to `bid_states_`. Continues retrieving bidders from
  // `pending_buyers_` if any have not been retrieved yet. Otherwise, invokes
  // StartBidding().
  void OnInterestGroupRead(
      std::vector<auction_worklet::mojom::BiddingInterestGroupPtr>
          interest_groups);

  // Starts loading worklets and generating bids.
  void StartBidding();

  void OnGenerateBidCrashed(BidState* state);
  void OnGenerateBidComplete(BidState* state,
                             auction_worklet::mojom::BidderWorkletBidPtr bid,
                             const std::vector<std::string>& errors);

  // True if all bid results and the seller script load are complete.
  bool ReadyToScore() const { return outstanding_bids_ == 0 && seller_loaded_; }
  void OnSellerWorkletLoaded(bool load_result,
                             const std::vector<std::string>& errors);

  // Calls into the seller asynchronously to score each outstanding bid, in
  // series. Once there are no outstanding bids, proceeds to selecting the
  // winner and running the Worklets reporting methods.
  void ScoreOne();
  void ScoreBid(const BidState* state);
  // Callback from ScoreBid().
  void OnBidScored(double score, const std::vector<std::string>& errors);

  std::string AdRenderFingerprint(const BidState* state);
  absl::optional<std::string> PerBuyerSignals(const BidState* state);

  // Completes the auction, invoking `callback_`. Consumer must be able to
  // safely delete `this` when the callback is invoked.
  void CompleteAuction();

  // Sequence of asynchronous methods to call into the bidder/seller results to
  // report a a win, Will ultimately invoke ReportSuccess(), which will delete
  // the auction.
  void ReportSellerResult(BidState* state);
  void OnReportSellerResultComplete(
      BidState* best_bid,
      const absl::optional<std::string>& signals_for_winner,
      const absl::optional<GURL>& seller_report_url,
      const std::vector<std::string>& error_msgs);
  void ReportBidWin(BidState* state);
  void OnReportBidWinComplete(const BidState* best_bid,
                              const absl::optional<GURL>& bidder_report_url,
                              const std::vector<std::string>& error_msgs);

  // These complete the auction, invoking `callback_` and preventing any future
  // calls into `this` by closing mojo pipes and disposing of weak pointers. The
  // owner must be able to safely delete `this` when the callback is invoked.
  void FailAuction();
  // Appends `error` to `errors_` before calling FailAuciton().
  void FailAuctionWithError(std::string error);
  void ReportSuccess(const BidState* state);

  // Closes all open pipes, to avoid receiving any Mojo callbacks after
  // completion.
  void ClosePipes();

  Delegate* const delegate_;
  InterestGroupManager* const interest_group_manager_;

  // Configuration.
  blink::mojom::AuctionAdConfigPtr auction_config_;
  // Buyers whose interest groups need to be looked up to be added to
  // `bid_states_`.
  std::vector<url::Origin> pending_buyers_;
  // Next entry in `pending_buyers_` to fetch the interest group for.
  size_t next_pending_buyer_ = 0;
  auction_worklet::mojom::BrowserSignalsPtr browser_signals_;
  const url::Origin frame_origin_;
  RunAuctionCallback callback_;

  // State for the bidding phase.
  int outstanding_bids_;  // number of bids for which we're waiting on a fetch.
  std::vector<BidState> bid_states_;  // State of all loaded bidders.
  // The time the auction started. Use a single base time for all Worklets, to
  // present a more consistent view of the universe.
  const base::Time auction_start_time_ = base::Time::Now();

  // URLLoaderFactory proxy class configured only to load the URL the seller
  // needs.
  std::unique_ptr<AuctionURLLoaderFactoryProxy> seller_url_loader_factory_;

  // State for the scoring phase.
  mojo::Remote<auction_worklet::mojom::SellerWorklet> seller_worklet_;

  // This is true if the seller script has been loaded successfully --- if the
  // load failed, the entire process is aborted since there is nothing useful
  // that can be done.
  bool seller_loaded_ = false;
  size_t seller_considering_ = 0;

  // Seller script reportResult() results.
  absl::optional<std::string> signals_for_winner_;
  absl::optional<GURL> seller_report_url_;

  // Bidder script reportWin() results.
  absl::optional<GURL> bidder_report_url_;

  // All errors reported by worklets thus far.
  std::vector<std::string> errors_;

  base::WeakPtrFactory<AuctionRunner> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_AUCTION_RUNNER_H_
