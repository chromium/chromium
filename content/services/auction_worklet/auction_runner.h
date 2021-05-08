// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_RUNNER_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_RUNNER_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/bidder_worklet.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/seller_worklet.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"

namespace auction_worklet {

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

    // true if the generateBid() callback passed to the BidderWorklet's
    // constructor has been invoked. This may indicated either successful
    // generation of a bid, or failure to load or run the script.
    bool bid_generate_complete = false;

    std::unique_ptr<BidderWorklet> bidder_worklet;
    base::Optional<BidderWorklet::Bid> bid_result;
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
  void OnGenerateBidComplete(BidState* state,
                             base::Optional<BidderWorklet::Bid> bid,
                             std::vector<std::string> errors_msgs);

  // True if all bid results and the seller script load are complete.
  bool ReadyToScore() const { return outstanding_bids_ == 0 && seller_loaded_; }
  void OnSellerWorkletLoaded(bool load_result,
                             base::Optional<std::string> error_msg);

  // Calls into the seller to asynchronously each of outstanding bids, in
  // series. Once there are no outstanding bids, proceeds to selecting the
  // winner and running the Worklets reporting methods.
  //
  // Destroys `this` (indirectly), upon wrapping up the auction if all bids have
  // been scored (including if there were none).
  void ScoreOne();
  void ScoreBid(const BidState* state);
  // Callback from ScoreBid().
  void OnBidScored(SellerWorklet::ScoreResult score_result);

  std::string AdRenderFingerprint(const BidState* state);
  base::Optional<std::string> PerBuyerSignals(const BidState* state);

  void CompleteAuction();  // Indirectly deletes `this`, if there's no winner.

  // Sequence of asynchronous methods to call into the bidder/seller results to
  // report a a win, Will ultimately invoke ReportSuccess(), which will delete
  // the auction.
  void ReportSellerResult(const BidState* state);
  void OnReportSellerResultComplete(const BidState* best_bid,
                                    SellerWorklet::Report seller_report);
  void ReportBidWin(const BidState* state, SellerWorklet::Report seller_report);
  void OnReportBidWinComplete(const BidState* best_bid,
                              SellerWorklet::Report seller_report,
                              const base::Optional<GURL>& bidder_report_url,
                              const std::vector<std::string>& error_msgs);

  // Destroys `this`.
  void FailAuction();

  // Destroys `this`.
  void ReportSuccess(const BidState* state,
                     const SellerWorklet::Report& seller_report,
                     const base::Optional<GURL>& bidder_report_url);

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

  // All errors reported by worklets thus far.
  std::vector<std::string> errors_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_RUNNER_H_
