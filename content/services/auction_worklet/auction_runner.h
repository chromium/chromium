// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_RUNNER_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_RUNNER_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/bidder_worklet.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/seller_worklet.h"
#include "content/services/auction_worklet/trusted_bidding_signals.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"

namespace auction_worklet {

class TrustedBiddingSignals;

// An AuctionRunner loads and runs the bidder and seller worklets, along with
// their reporting phases and produces the result via a callback.
//
// This is a self-owned object. It destroys itself after invoking the callback
// passed to CreateAndStart.
//
// At present it initiates all fetches in parallel, running all bidder scripts
// once they and any trusted signals they need are ready, then when all bids are
// in runs all the scoring, and finally the reporting worklets.
//
// TODO(morlovich): There is no need to wait for all bidders to finish to start
// scoring.
class AuctionRunner {
 public:
  explicit AuctionRunner(const AuctionRunner&) = delete;
  AuctionRunner& operator=(const AuctionRunner&) = delete;

  static void CreateAndStart(
      mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory,
      blink::mojom::AuctionAdConfigPtr auction_config,
      std::vector<mojom::BiddingInterestGroupPtr> bidders,
      mojom::BrowserSignalsPtr browser_signals,
      mojom::AuctionWorkletService::RunAuctionCallback callback);

 private:
  struct BidState {
    BidState();
    BidState(BidState&&);
    ~BidState();

    mojom::BiddingInterestGroup* bidder = nullptr;

    // true if loading of the bidder script failed, meaning that no bidding
    // will actually be done.
    bool failed = false;

    // true if there is no outstanding load of trusted bidding signals pending
    // for this bidder (including if none were configured or it failed; in such
    // cases `trusted_bidding_signals` will be null).
    bool trusted_signals_loaded = false;

    // true if there is no outstanding load of the bidder script pending
    // (including if the load failed).
    bool bidder_script_loaded = false;

    std::unique_ptr<TrustedBiddingSignals> trusted_bidding_signals;
    std::unique_ptr<BidderWorklet> bidder_worklet;
    BidderWorklet::BidResult bid_result;
    base::TimeDelta bid_duration;
    SellerWorklet::ScoreResult score_result;
  };

  AuctionRunner(
      mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory,
      blink::mojom::AuctionAdConfigPtr auction_config,
      std::vector<mojom::BiddingInterestGroupPtr> bidders,
      mojom::BrowserSignalsPtr browser_signals,
      mojom::AuctionWorkletService::RunAuctionCallback callback);
  ~AuctionRunner();

  void StartBidding();
  void OnBidderScriptLoaded(BidState* state, bool load_result);
  void OnTrustedSignalsLoaded(BidState* state, bool load_result);
  void MaybeRunBid(BidState* state);
  void RunBid(BidState* state);

  // True if all bid results and the seller script load are complete.
  bool ReadyToScore() const { return outstanding_bids_ == 0 && seller_loaded_; }
  void OnSellerWorkletLoaded(bool load_result);

  // Lets the seller score a single outstanding bid, if any, and then either
  // re-queues itself on event loop if there is more to check, or proceeds to
  // selecting the winner and running reporting worklets.
  //
  // Destroys `this` (indirectly), upon wrapping up the auction if all bids have
  // been scored (including if there were none).
  void ScoreOne();
  SellerWorklet::ScoreResult ScoreBid(const BidState* state);
  std::string AdRenderFingerprint(const BidState* state);
  base::Optional<std::string> PerBuyerSignals(const BidState* state);

  void CompleteAuction();  // Indirectly deletes `this`.
  SellerWorklet::Report ReportSellerResult(const BidState* state);
  BidderWorklet::ReportWinResult ReportBidWin(
      const BidState* state,
      const SellerWorklet::Report& seller_report);

  // Destroys `this`.
  void FailAuction();

  // Destroys `this`.
  void ReportSuccess(const BidState* state,
                     const BidderWorklet::ReportWinResult& bidder_report,
                     const SellerWorklet::Report& seller_report);

  // `auction_v8_helper_` needs to be before the worklets, since they refer to
  // it.
  AuctionV8Helper auction_v8_helper_;

  // Configuration.
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;
  blink::mojom::AuctionAdConfigPtr auction_config_;
  std::vector<mojom::BiddingInterestGroupPtr> bidders_;
  mojom::BrowserSignalsPtr browser_signals_;
  mojom::AuctionWorkletService::RunAuctionCallback callback_;

  // State for the bidding phase.
  int outstanding_bids_;  // number of bids for which we're waiting on a fetch.
  std::vector<BidState> bid_states_;  // parallel to `bidders_`.
  // The time the auction started. Use a single base time for all Worklets, to
  // present a more consistent view of the universe.
  const base::Time auction_start_time_ = base::Time::Now();

  // State for the scoring phase.
  std::unique_ptr<SellerWorklet> seller_worklet_;

  // This is true if the seller script has been loaded successfully --- if the
  // load failed, the entire process is aborted since there is nothing useful
  // that can be done.
  bool seller_loaded_ = false;
  size_t seller_considering_ = 0;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_RUNNER_H_
