// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_AUCTION_RUNNER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_AUCTION_RUNNER_H_

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
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"

namespace content {

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
  explicit AuctionRunner(const AuctionRunner&) = delete;
  AuctionRunner& operator=(const AuctionRunner&) = delete;

  // Invoked when a FLEDGE auction is complete.
  //
  // `render_url` URL of auction winning ad to render.
  //  An empty URL is used if there is no winner.
  //
  // `winning_interest_group_owner` owner of the winning interest group.
  //  An opaque origin if there is no winner.
  //
  // `winning_interest_group_name` name of winning interest group. Empty if
  //  there is no winner.
  //
  // `bidder_report_url` URL to use for reporting result to the bidder. Empty if
  //  no report should be sent.
  //
  // `seller_report`  URL to use for reporting result to the seller. Empty if no
  //  report should be sent.
  //
  // `errors` are various error messages to be used for debugging. These are too
  //  sensitive for the renderers to see.
  using RunAuctionCallback =
      base::OnceCallback<void(const GURL& render_url,
                              const url::Origin& winning_interest_group_owner,
                              const std::string& winning_interest_group_name,
                              const GURL& bidder_report_url,
                              const GURL& seller_report_url,
                              const std::vector<std::string>& errors)>;

  using GetAuctionServiceCallback =
      base::RepeatingCallback<auction_worklet::mojom::AuctionWorkletService*()>;

  // Runs an entire FLEDGE auction.
  //
  // Arguments:
  // `get_auction_service` is a callback to provide access to the
  //  AuctionWorkletService. Must be safe to invoke at any time until the
  //  AuctionRunner is destroyed.
  //
  // `url_loader_factory` is used to load worklet scripts and trusted bidding
  //  signals. It's recommended that the implementation be restricted to exactly
  //  those URLs (keeping in mind query parameter usage for trusted bidding
  //  signals and the allowed coalescing).
  //
  // `auction_config` is the configuration provided by client JavaScript in
  //  the renderer in order to initiate the auction.
  //
  // `bidders` includes definitions of the interest groups that are selected to
  //  participate in this auction (initially added by client JS in the renderer,
  //  but managed by the browser's interest group store), as well as some
  //  bidding history collected by the interest group store. The bidding
  //  worklets of these groups will be fetched and executed. `bidders` must not
  //  be empty.
  //
  // `browser_signals` signals from the browser about the auction that are the
  //  same for all worklets.
  static std::unique_ptr<AuctionRunner> CreateAndStart(
      GetAuctionServiceCallback get_auction_service,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory,
      blink::mojom::AuctionAdConfigPtr auction_config,
      std::vector<auction_worklet::mojom::BiddingInterestGroupPtr> bidders,
      auction_worklet::mojom::BrowserSignalsPtr browser_signals,
      RunAuctionCallback callback);

  ~AuctionRunner();

 private:
  struct BidState {
    BidState();
    BidState(BidState&&);
    ~BidState();

    auction_worklet::mojom::BiddingInterestGroup* bidder = nullptr;

    mojo::Remote<auction_worklet::mojom::BidderWorklet> bidder_worklet;
    auction_worklet::mojom::BidderWorkletBidPtr bid_result;
    double seller_score = 0;
  };

  AuctionRunner(
      GetAuctionServiceCallback get_auction_service,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory,
      blink::mojom::AuctionAdConfigPtr auction_config,
      std::vector<auction_worklet::mojom::BiddingInterestGroupPtr> bidders,
      auction_worklet::mojom::BrowserSignalsPtr browser_signals,
      RunAuctionCallback callback);

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
  void ReportSuccess(const BidState* state);

  // Closes all open pipes, to avoid receiving any Mojo callbacks after
  // completion.
  void ClosePipes();

  const GetAuctionServiceCallback get_auction_service_;

  // Configuration.
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;
  blink::mojom::AuctionAdConfigPtr auction_config_;
  std::vector<auction_worklet::mojom::BiddingInterestGroupPtr> bidders_;
  auction_worklet::mojom::BrowserSignalsPtr browser_signals_;
  RunAuctionCallback callback_;

  // State for the bidding phase.
  int outstanding_bids_;  // number of bids for which we're waiting on a fetch.
  std::vector<BidState> bid_states_;  // parallel to `bidders_`.
  // The time the auction started. Use a single base time for all Worklets, to
  // present a more consistent view of the universe.
  const base::Time auction_start_time_ = base::Time::Now();

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
