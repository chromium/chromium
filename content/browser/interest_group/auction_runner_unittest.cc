// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_runner.h"

#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/browser/interest_group/auction_process_manager.h"
#include "content/browser/interest_group/debuggable_auction_worklet.h"
#include "content/browser/interest_group/debuggable_auction_worklet_tracker.h"
#include "content/browser/interest_group/interest_group_manager.h"
#include "content/browser/interest_group/interest_group_storage.h"
#include "content/public/test/test_renderer_host.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/auction_worklet_service_impl.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "content/services/auction_worklet/worklet_devtools_debug_test_util.h"
#include "content/services/auction_worklet/worklet_test_util.h"
#include "mojo/public/cpp/system/functions.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"

using auction_worklet::TestDevToolsAgentClient;

namespace content {
namespace {

// 0 `num_component_urls` means no component URLs, as opposed to an empty list
// (which isn't tested at this layer).
std::string MakeBidScript(const std::string& bid,
                          const std::string& render_url,
                          int num_ad_components,
                          const url::Origin& interest_group_owner,
                          const std::string& interest_group_name,
                          bool has_signals,
                          const std::string& signal_key,
                          const std::string& signal_val) {
  // TODO(morlovich): Use JsReplace.
  constexpr char kBidScript[] = R"(
    const bid = %s;
    const renderUrl = "%s";
    const numAdComponents = %i;
    const interestGroupOwner = "%s";
    const interestGroupName = "%s";
    const hasSignals = %s;

    function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                         trustedBiddingSignals, browserSignals) {
      var result = {ad: {"bidKey": "data for " + bid,
                         "groupName": interestGroupName,
                         "renderUrl": "data for " + renderUrl},
                    bid: bid, render: renderUrl};
      if (interestGroup.adComponents) {
        result.adComponents = [interestGroup.adComponents[0].renderUrl];
        result.ad.adComponentsUrl = interestGroup.adComponents[0].renderUrl;
      }

      if (interestGroup.name !== interestGroupName)
        throw new Error("wrong interestGroupName");
      if (interestGroup.owner !== interestGroupOwner)
        throw new Error("wrong interestGroupOwner");
      if (interestGroup.ads.length != 1)
        throw new Error("wrong interestGroup.ads length");
      if (interestGroup.ads[0].renderUrl != renderUrl)
        throw new Error("wrong interestGroup.ads URL");
      if (numAdComponents == 0) {
        if (interestGroup.adComponents !== undefined)
          throw new Error("Non-empty adComponents");
      } else {
        if (interestGroup.adComponents.length !== numAdComponents)
          throw new Error("Wrong adComponents length");
        for (let i = 0; i < numAdComponents; ++i) {
          if (interestGroup.adComponents[i].renderUrl !=
              renderUrl.slice(0, -1) + "-component" + (i+1) + ".com/") {
            throw new Error("Wrong adComponents renderUrl");
          }
        }
      }
      if (perBuyerSignals.signalsFor !== interestGroupName)
        throw new Error("wrong perBuyerSignals");
      if (!auctionSignals.isAuctionSignals)
        throw new Error("wrong auctionSignals");
      if (hasSignals) {
        if ('extra' in trustedBiddingSignals)
          throw new Error("why extra?");
        if (trustedBiddingSignals["%s"] !== "%s")
          throw new Error("wrong signals");
      } else if (trustedBiddingSignals !== null) {
        throw new Error("Expected null trustedBiddingSignals");
      }
      if (browserSignals.topWindowHostname !== 'publisher1.com')
        throw new Error("wrong topWindowHostname");
      if (browserSignals.seller != 'https://adstuff.publisher1.com')
         throw new Error("wrong seller");
      if (browserSignals.joinCount !== 3)
        throw new Error("joinCount")
      if (browserSignals.bidCount !== 5)
        throw new Error("bidCount");
      if (browserSignals.prevWins.length !== 3)
        throw new Error("prevWins");
      for (let i = 0; i < browserSignals.prevWins.length; ++i) {
        if (!(browserSignals.prevWins[i] instanceof Array))
          throw new Error("prevWins entry not an array");
        if (typeof browserSignals.prevWins[i][0] != "number")
          throw new Error("Not a Number in prevWin?");
        if (browserSignals.prevWins[i][1].winner !== -i)
          throw new Error("prevWin MD not what passed in");
      }
      return result;
    }

    function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
                       browserSignals) {
      if (!auctionSignals.isAuctionSignals)
        throw new Error("wrong auctionSignals");
      if (perBuyerSignals.signalsFor !== interestGroupName)
        throw new Error("wrong perBuyerSignals");

      // sellerSignals in these tests is just sellers' browserSignals, since
      // that's what reportResult passes through.
      if (sellerSignals.topWindowHostname !== 'publisher1.com')
        throw new Error("wrong topWindowHostname");
      if (sellerSignals.interestGroupOwner !== interestGroupOwner)
        throw new Error("wrong interestGroupOwner");
      if (sellerSignals.renderUrl !== renderUrl)
        throw new Error("wrong renderUrl");
      if (sellerSignals.bid !== bid)
        throw new Error("wrong bid");
      if (sellerSignals.desirability !== (bid * 2))
        throw new Error("wrong desirability");

      if (browserSignals.topWindowHostname !== 'publisher1.com')
        throw new Error("wrong browserSignals.topWindowHostname");
      if ("desirability" in browserSignals)
        throw new Error("why is desirability here?");
      if (browserSignals.interestGroupName !== interestGroupName)
        throw new Error("wrong browserSignals.interestGroupName");
      if (browserSignals.interestGroupOwner !== interestGroupOwner)
        throw new Error("wrong browserSignals.interestGroupOwner");

      if (browserSignals.renderUrl !== renderUrl)
        throw new Error("wrong browserSignals.renderUrl");
      if (browserSignals.bid !== bid)
        throw new Error("wrong browserSignals.bid");

      sendReportTo("https://buyer-reporting.example.com");
    }
  )";
  return base::StringPrintf(
      kBidScript, bid.c_str(), render_url.c_str(), num_ad_components,
      interest_group_owner.Serialize().c_str(), interest_group_name.c_str(),
      has_signals ? "true" : "false", signal_key.c_str(), signal_val.c_str());
}

// This can be appended to the standard script to override the function.
constexpr char kReportWinNoUrl[] = R"(
  function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
                     browserSignals) {
  }
)";

// This can be appended to the standard script to override the function.
constexpr char kReportWinExpectNullAuctionSignals[] = R"(
  function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
                     browserSignals) {
    if (sellerSignals === null)
      sendReportTo("https://seller.signals.were.null.test");
  }
)";

constexpr char kCheckingAuctionScript[] = R"(
  function scoreAd(adMetadata, bid, auctionConfig, trustedScoringSignals,
                   browserSignals) {
    if (adMetadata.bidKey !== ("data for " + bid)) {
      throw new Error("wrong data for bid:" +
                      JSON.stringify(adMetadata) + "/" + bid);
    }
    if (adMetadata.renderUrl !== ("data for " + browserSignals.renderUrl)) {
      throw new Error("wrong data for renderUrl:" +
                      JSON.stringify(adMetadata) + "/" +
                      browserSignals.renderUrl);
    }
    let components = browserSignals.adComponents;
    if (adMetadata.adComponentsUrl) {
      if (components.length !== 1 ||
          components[0] !== adMetadata.adComponentsUrl) {
        throw new Error("wrong data for adComponents:" +
                        JSON.stringify(adMetadata) + "/" +
                        browserSignals.adComponents);
      }
    } else if (components !== undefined) {
      throw new Error("wrong data for adComponents:" +
                      JSON.stringify(adMetadata) + "/" +
                      browserSignals.adComponents);
    }
    if (auctionConfig.decisionLogicUrl
        !== "https://adstuff.publisher1.com/auction.js") {
      throw new Error("wrong auctionConfig");
    }
    if (auctionConfig.perBuyerSignals['https://adplatform.com'].signalsFor
        !== 'Ad Platform') {
      throw new Error("Wrong perBuyerSignals in auctionConfig");
    }
    if (!auctionConfig.sellerSignals.isSellerSignals)
      throw new Error("Wrong sellerSignals");
    if (browserSignals.topWindowHostname !== 'publisher1.com')
      throw new Error("wrong topWindowHostname");
    if ("joinCount" in browserSignals)
      throw new Error("wrong kind of browser signals");
    if (typeof browserSignals.biddingDurationMsec !== "number")
      throw new Error("biddingDurationMsec is not a number. huh");
    if (browserSignals.biddingDurationMsec < 0)
      throw new Error("biddingDurationMsec should be non-negative.");

    return bid * 2;
  }
)";

constexpr char kReportResultScript[] = R"(
  function reportResult(auctionConfig, browserSignals) {
    if (auctionConfig.decisionLogicUrl
        !== "https://adstuff.publisher1.com/auction.js") {
      throw new Error("wrong auctionConfig");
    }
    if (browserSignals.topWindowHostname !== 'publisher1.com')
      throw new Error("wrong topWindowHostname");
    sendReportTo("https://reporting.example.com");
    return browserSignals;
  }
)";

constexpr char kReportResultScriptNoUrl[] = R"(
  function reportResult(auctionConfig, browserSignals) {
    return browserSignals;
  }
)";

std::string MakeAuctionScript() {
  return std::string(kCheckingAuctionScript) + kReportResultScript;
}

std::string MakeAuctionScriptNoReportUrl() {
  return std::string(kCheckingAuctionScript) + kReportResultScriptNoUrl;
}

const char kAuctionScriptRejects2[] = R"(
  function scoreAd(adMetadata, bid, auctionConfig, browserSignals) {
    if (bid === 2)
      return -1;
    return bid + 1;
  }
)";

std::string MakeAuctionScriptReject2() {
  return std::string(kAuctionScriptRejects2) + kReportResultScript;
}

// Sorts a vector of PreviousWinPtr so that the most recent wins are last.
void SortPrevWins(
    std::vector<auction_worklet::mojom::PreviousWinPtr>& prev_wins) {
  std::sort(prev_wins.begin(), prev_wins.end(),
            [](const auction_worklet::mojom::PreviousWinPtr& prev_win1,
               const auction_worklet::mojom::PreviousWinPtr& prev_win2) {
              return prev_win1->time < prev_win2->time;
            });
}

// BidderWorklet that holds onto passed in callbacks, to let the test fixture
// invoke them.
class MockBidderWorklet : public auction_worklet::mojom::BidderWorklet {
 public:
  explicit MockBidderWorklet(
      mojo::PendingReceiver<auction_worklet::mojom::BidderWorklet>
          pending_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_url_loader_factory)
      : url_loader_factory_(std::move(pending_url_loader_factory)),
        receiver_(this, std::move(pending_receiver)) {
    receiver_.set_disconnect_handler(base::BindOnce(
        &MockBidderWorklet::OnPipeClosed, base::Unretained(this)));
  }

  MockBidderWorklet(const MockBidderWorklet&) = delete;
  const MockBidderWorklet& operator=(const MockBidderWorklet&) = delete;

  ~MockBidderWorklet() override = default;

  // auction_worklet::mojom::BidderWorklet implementation:

  void GenerateBid(const absl::optional<std::string>& auction_signals_json,
                   const absl::optional<std::string>& per_buyer_signals_json,
                   const url::Origin& top_window_origin,
                   const url::Origin& seller_origin,
                   base::Time auction_start_time,
                   GenerateBidCallback generate_bid_callback) override {
    // While the real BidderWorklet implementation supports multiple pending
    // callbacks, this class does not.
    DCHECK(!generate_bid_callback_);
    generate_bid_callback_ = std::move(generate_bid_callback);
    if (generate_bid_run_loop_)
      generate_bid_run_loop_->Quit();
  }

  void ReportWin(const absl::optional<std::string>& auction_signals_json,
                 const absl::optional<std::string>& per_buyer_signals_json,
                 const url::Origin& top_window_origin,
                 const std::string& seller_signals_json,
                 const GURL& browser_signal_render_url,
                 double browser_signal_bid,
                 ReportWinCallback report_win_callback) override {
    // While the real BidderWorklet implementation supports multiple pending
    // callbacks, this class does not.
    DCHECK(!report_win_callback_);
    report_win_callback_ = std::move(report_win_callback);
    if (report_win_run_loop_)
      report_win_run_loop_->Quit();
  }

  void ConnectDevToolsAgent(
      mojo::PendingReceiver<blink::mojom::DevToolsAgent> agent) override {
    ADD_FAILURE()
        << "ConnectDevToolsAgent should not be called on MockBidderWorklet";
  }

  void WaitForGenerateBid() {
    if (!generate_bid_callback_) {
      generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
      generate_bid_run_loop_->Run();
      generate_bid_run_loop_.reset();
      DCHECK(generate_bid_callback_);
    }
  }

  // Invokes the GenerateBid callback. A bid of base::nullopt means no bid
  // should be offered. Waits for the GenerateBid() call first, if needed.
  void InvokeGenerateBidCallback(
      absl::optional<double> bid,
      const GURL& render_url = GURL(),
      absl::optional<std::vector<GURL>> ad_component_urls = absl::nullopt,
      base::TimeDelta duration = base::TimeDelta()) {
    WaitForGenerateBid();

    if (!bid.has_value()) {
      std::move(generate_bid_callback_)
          .Run(/*bid=*/nullptr, /*errors=*/std::vector<std::string>());
      return;
    }

    std::move(generate_bid_callback_)
        .Run(auction_worklet::mojom::BidderWorkletBid::New(
                 "ad", *bid, render_url, ad_component_urls, duration),
             /*errors=*/std::vector<std::string>());
  }

  // Returns the GenerateBidCallback for a worklet. Needed for cases when the
  // BidderWorklet is destroyed (to close its pipe) before the
  // AuctionWorkletService is destroyed, since Mojo DCHECKs if a callback is
  // destroyed when the pipe its over is still live.
  //
  // TODO(mmenke): To better simulate real crashes, give worklets their own
  // AuctionWorkletService pipes, and remove this method.
  GenerateBidCallback TakeLoadCallback() {
    WaitForGenerateBid();
    return std::move(generate_bid_callback_);
  }

  void WaitForReportWin() {
    DCHECK(!generate_bid_callback_);
    DCHECK(!report_win_run_loop_);
    if (!report_win_callback_) {
      report_win_run_loop_ = std::make_unique<base::RunLoop>();
      report_win_run_loop_->Run();
      report_win_run_loop_.reset();
      DCHECK(report_win_callback_);
    }
  }

  void InvokeReportWinCallback(
      absl::optional<GURL> report_url = absl::nullopt) {
    DCHECK(report_win_callback_);
    std::move(report_win_callback_)
        .Run(report_url, std::vector<std::string>() /* errors */);
  }

  mojo::Remote<network::mojom::URLLoaderFactory>& url_loader_factory() {
    return url_loader_factory_;
  }

  // Flush the receiver pipe and return whether or not its closed.
  bool PipeIsClosed() {
    receiver_.FlushForTesting();
    return pipe_closed_;
  }

 private:
  void OnPipeClosed() { pipe_closed_ = true; }

  BidderWorklet::GenerateBidCallback generate_bid_callback_;

  bool pipe_closed_ = false;

  std::unique_ptr<base::RunLoop> generate_bid_run_loop_;
  std::unique_ptr<base::RunLoop> report_win_run_loop_;
  ReportWinCallback report_win_callback_;

  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;

  // Receiver is last so that destroying `this` while there's a pending callback
  // over the pipe will not DCHECK.
  mojo::Receiver<auction_worklet::mojom::BidderWorklet> receiver_;
};

// SellerWorklet that holds onto passed in callbacks, to let the test fixture
// invoke them.
class MockSellerWorklet : public auction_worklet::mojom::SellerWorklet {
 public:
  // Subset of parameters passed to SellerWorklet's ScoreAd method.
  struct ScoreAdParams {
    double bid;
    url::Origin interest_group_owner;
  };

  explicit MockSellerWorklet(
      mojo::PendingReceiver<auction_worklet::mojom::SellerWorklet>
          pending_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_url_loader_factory,
      auction_worklet::mojom::AuctionWorkletService::LoadSellerWorkletCallback
          load_worklet_callback)
      : load_worklet_callback_(std::move(load_worklet_callback)),
        url_loader_factory_(std::move(pending_url_loader_factory)),
        receiver_(this, std::move(pending_receiver)) {}

  MockSellerWorklet(const MockSellerWorklet&) = delete;
  const MockSellerWorklet& operator=(const MockSellerWorklet&) = delete;

  ~MockSellerWorklet() override = default;

  // auction_worklet::mojom::SellerWorklet implementation:

  void ScoreAd(const std::string& ad_metadata_json,
               double bid,
               blink::mojom::AuctionAdConfigPtr auction_config,
               const url::Origin& browser_signal_top_window_origin,
               const url::Origin& browser_signal_interest_group_owner,
               const GURL& browser_signal_render_url,
               const std::vector<GURL>& browser_signal_ad_components,
               uint32_t browser_signal_bidding_duration_msecs,
               ScoreAdCallback score_ad_callback) override {
    score_ad_callback_ = std::move(score_ad_callback);
    score_ad_params_ = std::make_unique<ScoreAdParams>();
    score_ad_params_->bid = bid;
    score_ad_params_->interest_group_owner =
        browser_signal_interest_group_owner;
    if (score_ad_run_loop_)
      score_ad_run_loop_->Quit();
  }

  void ReportResult(blink::mojom::AuctionAdConfigPtr auction_config,
                    const url::Origin& browser_signal_top_window_origin,
                    const url::Origin& browser_signal_interest_group_owner,
                    const GURL& browser_signal_render_url,
                    double browser_signal_bid,
                    double browser_signal_desirability,
                    ReportResultCallback report_result_callback) override {
    report_result_callback_ = std::move(report_result_callback);
    if (report_result_run_loop_)
      report_result_run_loop_->Quit();
  }

  void ConnectDevToolsAgent(
      mojo::PendingReceiver<blink::mojom::DevToolsAgent> agent) override {
    ADD_FAILURE()
        << "ConnectDevToolsAgent should not be called on MockSellerWorklet";
  }

  // Informs the consumer that the seller worklet has successfully loaded.
  void CompleteLoading() {
    DCHECK(load_worklet_callback_);
    std::move(load_worklet_callback_)
        .Run(true /* success */, std::vector<std::string>() /* errors */);
  }

  // Waits until ScoreAd() has been invoked, if it hasn't been already.
  std::unique_ptr<ScoreAdParams> WaitForScoreAd() {
    DCHECK(!score_ad_run_loop_);
    DCHECK(!load_worklet_callback_);
    if (!score_ad_params_) {
      score_ad_run_loop_ = std::make_unique<base::RunLoop>();
      score_ad_run_loop_->Run();
      score_ad_run_loop_.reset();
      DCHECK(score_ad_params_);
    }
    return std::move(score_ad_params_);
  }

  // Invokes the ScoreAdCallback for the most recent ScoreAd() call with the
  // provided score. WaitForScoreAd() must have been invoked first.
  void InvokeScoreAdCallback(double score) {
    DCHECK(score_ad_callback_);
    DCHECK(!score_ad_params_);
    std::move(score_ad_callback_)
        .Run(score, std::vector<std::string>() /* errors */);
  }

  void WaitForReportResult() {
    DCHECK(!report_result_run_loop_);
    DCHECK(!load_worklet_callback_);
    if (!report_result_callback_) {
      report_result_run_loop_ = std::make_unique<base::RunLoop>();
      report_result_run_loop_->Run();
      report_result_run_loop_.reset();
      DCHECK(report_result_callback_);
    }
  }

  // Invokes the ReportResultCallback for the most recent ScoreAd() call with
  // the provided score. WaitForReportResult() must have been invoked first.
  void InvokeReportResultCallback(
      absl::optional<GURL> report_url = absl::nullopt) {
    DCHECK(report_result_callback_);
    std::move(report_result_callback_)
        .Run(absl::nullopt /* signals_for_winner */, std::move(report_url),
             std::vector<std::string>() /* errors */);
  }

  mojo::Remote<network::mojom::URLLoaderFactory>& url_loader_factory() {
    return url_loader_factory_;
  }

  void Flush() { receiver_.FlushForTesting(); }

 private:
  auction_worklet::mojom::AuctionWorkletService::LoadSellerWorkletCallback
      load_worklet_callback_;

  std::unique_ptr<base::RunLoop> score_ad_run_loop_;
  std::unique_ptr<ScoreAdParams> score_ad_params_;
  ScoreAdCallback score_ad_callback_;

  std::unique_ptr<base::RunLoop> report_result_run_loop_;
  ReportResultCallback report_result_callback_;

  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;

  // Receiver is last so that destroying `this` while there's a pending callback
  // over the pipe will not DCHECK.
  mojo::Receiver<auction_worklet::mojom::SellerWorklet> receiver_;
};

// AuctionWorkletService that creates MockBidderWorklets and MockSellerWorklets
// to hold onto passed in PendingReceivers and Callbacks.

// AuctionProcessManager and AuctionWorkletService - combining the two with a
// mojo::ReceiverSet makes it easier to track which call came over which
// receiver than using separate classes.
class MockAuctionProcessManager
    : public AuctionProcessManager,
      public auction_worklet::mojom::AuctionWorkletService {
 public:
  MockAuctionProcessManager() = default;
  ~MockAuctionProcessManager() override = default;

  // AuctionProcessManager implementation:
  void LaunchProcess(
      mojo::PendingReceiver<auction_worklet::mojom::AuctionWorkletService>
          auction_worklet_service_receiver,
      const std::string& display_name) override {
    mojo::ReceiverId receiver_id =
        receiver_set_.Add(this, std::move(auction_worklet_service_receiver));

    // Have to flush the receiver set, so that any closed receivers are removed,
    // before searching for duplicate process names.
    receiver_set_.FlushForTesting();

    // Each receiver should get a unique display name. This check serves to help
    // ensure that processes are correctly reused.
    EXPECT_EQ(0u, receiver_display_name_map_.count(receiver_id));
    for (auto receiver : receiver_display_name_map_) {
      // Ignore closed receivers. ReportWin() will result in re-loading a
      // worklet, after closing the original worklet, which may require
      // re-creating the AuctionWorkletService.
      if (receiver_set_.HasReceiver(receiver.first))
        EXPECT_NE(receiver.second, display_name);
    }

    receiver_display_name_map_[receiver_id] = display_name;
  }

  // auction_worklet::mojom::AuctionWorkletService implementation:
  void LoadBidderWorklet(
      mojo::PendingReceiver<auction_worklet::mojom::BidderWorklet>
          bidder_worklet_receiver,
      bool should_pause_on_start,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_url_loader_factory,
      auction_worklet::mojom::BiddingInterestGroupPtr bidding_interest_group)
      override {
    // Make sure this request came over the right pipe.
    EXPECT_EQ(receiver_display_name_map_[receiver_set_.current_receiver()],
              ComputeDisplayName(AuctionProcessManager::WorkletType::kBidder,
                                 bidding_interest_group->group.owner));

    InterestGroupId interest_group_id(bidding_interest_group->group.owner,
                                      bidding_interest_group->group.name);
    EXPECT_EQ(0u, bidder_worklets_.count(interest_group_id));
    bidder_worklets_.emplace(std::make_pair(
        interest_group_id, std::make_unique<MockBidderWorklet>(
                               std::move(bidder_worklet_receiver),
                               std::move(pending_url_loader_factory))));
    // Whenever a worklet is created, one of the RunLoops should be waiting for
    // worklet creation.
    if (wait_for_bidder_reload_run_loop_) {
      wait_for_bidder_reload_run_loop_->Quit();
    } else {
      ASSERT_GT(waiting_for_num_bidders_, 0);
      --waiting_for_num_bidders_;
      MaybeQuitWaitForWorkletsRunLoop();
    }
  }

  void LoadSellerWorklet(
      mojo::PendingReceiver<auction_worklet::mojom::SellerWorklet>
          seller_worklet_receiver,
      bool should_pause_on_start,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_url_loader_factory,
      const GURL& script_source_url,
      LoadSellerWorkletCallback load_seller_worklet_callback) override {
    DCHECK(!seller_worklet_);

    // Make sure this request came over the right pipe.
    EXPECT_EQ(receiver_display_name_map_[receiver_set_.current_receiver()],
              ComputeDisplayName(AuctionProcessManager::WorkletType::kSeller,
                                 url::Origin::Create(script_source_url)));

    seller_worklet_ = std::make_unique<MockSellerWorklet>(
        std::move(seller_worklet_receiver),
        std::move(pending_url_loader_factory),
        std::move(load_seller_worklet_callback));

    ASSERT_TRUE(waiting_on_seller_);
    waiting_on_seller_ = false;
    MaybeQuitWaitForWorkletsRunLoop();
  }

  // Waits for a SellerWorklet and `num_bidders` bidder worklets to be created.
  void WaitForWorklets(int num_bidders) {
    waiting_on_seller_ = true;
    waiting_for_num_bidders_ = num_bidders;
    wait_for_worklets_run_loop_ = std::make_unique<base::RunLoop>();
    wait_for_worklets_run_loop_->Run();
    wait_for_worklets_run_loop_.reset();
  }

  // Waits for a single bidder script to be loaded. Intended to be used to wait
  // for the winning bidder script to be reloaded. WaitForWorklets() should be
  // used when waiting for worklets to be loaded at the start of an auction.
  void WaitForWinningBidderReload() {
    EXPECT_TRUE(bidder_worklets_.empty());
    wait_for_bidder_reload_run_loop_ = std::make_unique<base::RunLoop>();
    wait_for_bidder_reload_run_loop_->Run();
    wait_for_bidder_reload_run_loop_.reset();
    EXPECT_EQ(1u, bidder_worklets_.size());
  }

  // Returns the MockBidderWorklet created for the specified interest group
  // origin and name, if there is one.
  std::unique_ptr<MockBidderWorklet> TakeBidderWorklet(
      const url::Origin& interest_group_owner_origin,
      const std::string& interest_group_name) {
    InterestGroupId interest_group_id(interest_group_owner_origin,
                                      interest_group_name);
    auto it = bidder_worklets_.find(interest_group_id);
    if (it == bidder_worklets_.end())
      return nullptr;
    std::unique_ptr<MockBidderWorklet> out = std::move(it->second);
    bidder_worklets_.erase(it);
    return out;
  }

  // Returns the MockSellerWorklet, if one has been created.
  std::unique_ptr<MockSellerWorklet> TakeSellerWorklet() {
    return std::move(seller_worklet_);
  }

  void Flush() { receiver_set_.FlushForTesting(); }

  // Close all the AuctionWorkletService pipes. Needs to be called before
  // uninvoked worklet callbacks can be destroyed, which is useful after
  // simulating a worklet crash.
  void ClosePipes() { receiver_set_.Clear(); }

 private:
  void MaybeQuitWaitForWorkletsRunLoop() {
    DCHECK(wait_for_worklets_run_loop_);
    if (!waiting_on_seller_ && waiting_for_num_bidders_ == 0)
      wait_for_worklets_run_loop_->Quit();
  }

  // An interest group is uniquely identified by its owner's origin and name.
  using InterestGroupId = std::pair<url::Origin, std::string>;

  std::map<InterestGroupId, std::unique_ptr<MockBidderWorklet>>
      bidder_worklets_;

  std::unique_ptr<MockSellerWorklet> seller_worklet_;

  // Used to wait for the worklets to be loaded at the start of the auction.
  std::unique_ptr<base::RunLoop> wait_for_worklets_run_loop_;
  bool waiting_on_seller_ = false;
  int waiting_for_num_bidders_ = 0;

  // Used to wait for a bidder worklet to be reloaded at the end of an auction.
  std::unique_ptr<base::RunLoop> wait_for_bidder_reload_run_loop_;

  // Map from ReceiverSet IDs to display name when the process was launched.
  // Used to verify that worklets are created in the right process.
  std::map<mojo::ReceiverId, std::string> receiver_display_name_map_;

  // ReceiverSet is last so that destroying `this` while there's a pending
  // callback over the pipe will not DCHECK.
  mojo::ReceiverSet<auction_worklet::mojom::AuctionWorkletService>
      receiver_set_;
};

class SameThreadAuctionProcessManager : public AuctionProcessManager {
 public:
  SameThreadAuctionProcessManager() = default;
  SameThreadAuctionProcessManager(const SameThreadAuctionProcessManager&) =
      delete;
  SameThreadAuctionProcessManager& operator=(
      const SameThreadAuctionProcessManager&) = delete;
  ~SameThreadAuctionProcessManager() override = default;

  // Resume all worklets paused waiting for debugger on startup.
  void ResumeAllPaused() {
    for (const auto& svc : auction_worklet_services_) {
      svc->AuctionV8HelperForTesting()->v8_runner()->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](scoped_refptr<auction_worklet::AuctionV8Helper> v8_helper) {
                v8_helper->ResumeAllForTesting();
              },
              svc->AuctionV8HelperForTesting()));
    }
  }

 private:
  void LaunchProcess(
      mojo::PendingReceiver<auction_worklet::mojom::AuctionWorkletService>
          auction_worklet_service_receiver,
      const std::string& display_name) override {
    // Create one AuctionWorkletServiceImpl per Mojo pipe, just like in
    // production code. Don't bother to delete the service on pipe close,
    // though; just keep it in a vector instead.
    auction_worklet_services_.push_back(
        std::make_unique<auction_worklet::AuctionWorkletServiceImpl>(
            std::move(auction_worklet_service_receiver)));
  }

  std::vector<std::unique_ptr<auction_worklet::AuctionWorkletServiceImpl>>
      auction_worklet_services_;
};

class AuctionRunnerTest : public testing::Test,
                          public AuctionRunner::Delegate,
                          public DebuggableAuctionWorkletTracker::Observer {
 protected:
  // Output of the RunAuctionCallback passed to AuctionRunner::CreateAndStart().
  struct Result {
    Result() = default;
    // Can't use default copy logic, since it contains Mojo types.
    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;

    absl::optional<GURL> ad_url;
    absl::optional<std::vector<GURL>> ad_component_urls;
    absl::optional<GURL> bidder_report_url;
    absl::optional<GURL> seller_report_url;
    std::vector<std::string> errors;

    // Metadata about `bidder1` and `bidder2`, pulled from the
    // InterestGroupManager on auction complete. Used to make sure number of
    // bids and win list are updated on auction complete. Previous wins arrays
    // are guaranteed to be sorted in chronological order.
    int bidder1_bid_count;
    std::vector<auction_worklet::mojom::PreviousWinPtr> bidder1_prev_wins;
    int bidder2_bid_count;
    std::vector<auction_worklet::mojom::PreviousWinPtr> bidder2_prev_wins;
  };

  AuctionRunnerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    mojo::SetDefaultProcessErrorHandler(base::BindRepeating(
        &AuctionRunnerTest::OnBadMessage, base::Unretained(this)));
    DebuggableAuctionWorkletTracker::GetInstance()->AddObserver(this);
  }

  ~AuctionRunnerTest() override {
    DebuggableAuctionWorkletTracker::GetInstance()->RemoveObserver(this);

    // Any bad message should have been inspected and cleared before the end of
    // the test.
    EXPECT_EQ(std::string(), bad_message_);
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());

    // Give off-thread things a chance to delete.
    task_environment_.RunUntilIdle();
  }

  void OnBadMessage(const std::string& reason) {
    // No test expects multiple bad messages at a time
    EXPECT_EQ(std::string(), bad_message_);
    // Empty bad messages aren't expected. This check allows an empty
    // `bad_message_` field to mean no bad message, avoiding using an optional,
    // which has less helpful output on EXPECT failures.
    EXPECT_FALSE(reason.empty());

    bad_message_ = reason;
  }

  // Gets and clear most recent bad Mojo message.
  std::string TakeBadMessage() { return std::move(bad_message_); }

  // Starts an auction without waiting for it to complete. Useful when using
  // MockAuctionProcessManager.
  //
  // `bidders` are added to a new InterestGroupManager before running the
  // auction. The times of their previous wins are ignored, as the
  // InterestGroupManager automatically attaches the current time, though their
  // wins will be added in order, with chronologically increasing times within
  // each InterestGroup.
  void StartAuction(const GURL& seller_decision_logic_url,
                    std::vector<StorageInterestGroup> bidders,
                    const std::string& auction_signals_json,
                    auction_worklet::mojom::BrowserSignalsPtr browser_signals) {
    auction_complete_ = false;

    blink::mojom::AuctionAdConfigPtr auction_config =
        blink::mojom::AuctionAdConfig::New();
    auction_config->seller = url::Origin::Create(seller_decision_logic_url);
    auction_config->decision_logic_url = seller_decision_logic_url;
    auction_config->trusted_scoring_signals_url = trusted_scoring_signals_url_;
    // This is ignored by AuctionRunner, in favor of its `filtered_buyers`
    // parameter.
    auction_config->interest_group_buyers =
        blink::mojom::InterestGroupBuyers::NewAllBuyers(
            blink::mojom::AllBuyers::New());
    auction_config->auction_signals = auction_signals_json;
    auction_config->seller_signals = R"({"isSellerSignals": true})";

    base::flat_map<url::Origin, std::string> per_buyer_signals;
    per_buyer_signals[kBidder1] = R"({"signalsFor": ")" + kBidder1Name + "\"}";
    per_buyer_signals[kBidder2] = R"({"signalsFor": ")" + kBidder2Name + "\"}";
    auction_config->per_buyer_signals = std::move(per_buyer_signals);

    interest_group_manager_ = std::make_unique<InterestGroupManager>(
        base::FilePath(), true /* in_memory */,
        /* url_loader_factory */ nullptr);
    if (auction_process_manager_) {
      interest_group_manager_->set_auction_process_manager_for_testing(
          std::move(auction_process_manager_));
    } else {
      interest_group_manager_->set_auction_process_manager_for_testing(
          std::make_unique<SameThreadAuctionProcessManager>());
    }

    histogram_tester_ = std::make_unique<base::HistogramTester>();

    // Add previous wins and bids to the interest group manager.
    for (auto& bidder : bidders) {
      for (int i = 0; i < bidder.bidding_group->signals->join_count; i++) {
        interest_group_manager_->JoinInterestGroup(
            bidder.bidding_group->group,
            bidder.bidding_group->group.owner.GetURL());
      }
      for (int i = 0; i < bidder.bidding_group->signals->bid_count; i++) {
        interest_group_manager_->RecordInterestGroupBid(
            bidder.bidding_group->group.owner,
            bidder.bidding_group->group.name);
      }
      for (const auto& prev_win : bidder.bidding_group->signals->prev_wins) {
        interest_group_manager_->RecordInterestGroupWin(
            bidder.bidding_group->group.owner, bidder.bidding_group->group.name,
            prev_win->ad_json);
        // Add some time between interest group wins, so that they'll be added
        // to the database in the order they appear. Their times will *not*
        // match those in `prev_wins`.
        task_environment_.FastForwardBy(base::Seconds(1));
      }
    }

    auction_run_loop_ = std::make_unique<base::RunLoop>();
    auction_runner_ = AuctionRunner::CreateAndStart(
        this, interest_group_manager_.get(), std::move(auction_config),
        std::vector<url::Origin>{kBidder1, kBidder2},
        std::move(browser_signals), frame_origin_,
        base::BindOnce(&AuctionRunnerTest::OnAuctionComplete,
                       base::Unretained(this)));
  }

  const Result& RunAuctionAndWait(
      const GURL& seller_decision_logic_url,
      std::vector<StorageInterestGroup> bidders,
      const std::string& auction_signals_json,
      auction_worklet::mojom::BrowserSignalsPtr browser_signals) {
    StartAuction(seller_decision_logic_url, std::move(bidders),
                 auction_signals_json, std::move(browser_signals));
    auction_run_loop_->Run();
    return result_;
  }

  void OnAuctionComplete(AuctionRunner* auction_runner,
                         absl::optional<GURL> ad_url,
                         absl::optional<std::vector<GURL>> ad_component_urls,
                         absl::optional<GURL> bidder_report_url,
                         absl::optional<GURL> seller_report_url,
                         std::vector<std::string> errors) {
    DCHECK(auction_run_loop_);
    DCHECK(!auction_complete_);

    auction_complete_ = true;
    result_.ad_url = std::move(ad_url);
    result_.ad_component_urls = std::move(ad_component_urls);
    result_.bidder_report_url = std::move(bidder_report_url);
    result_.seller_report_url = std::move(seller_report_url);
    result_.errors = std::move(errors);
    result_.bidder1_bid_count = -1;
    result_.bidder1_prev_wins.clear();
    result_.bidder2_bid_count = -1;
    result_.bidder2_prev_wins.clear();

    // Retrieve bid count and previous wins for kBidder1 (and subsequently
    // kBidder2).
    interest_group_manager_->GetInterestGroupsForOwner(
        kBidder1, base::BindOnce(&AuctionRunnerTest::OnBidder1GroupsRetrieved,
                                 base::Unretained(this)));
  }

  void OnBidder1GroupsRetrieved(
      std::vector<StorageInterestGroup> storage_interest_groups) {
    for (auto& bidder : storage_interest_groups) {
      if (bidder.bidding_group->group.owner == kBidder1 &&
          bidder.bidding_group->group.name == kBidder1Name) {
        result_.bidder1_bid_count = bidder.bidding_group->signals->bid_count;
        result_.bidder1_prev_wins =
            std::move(bidder.bidding_group->signals->prev_wins);
        SortPrevWins(result_.bidder1_prev_wins);
        break;
      }
    }
    interest_group_manager_->GetInterestGroupsForOwner(
        kBidder2, base::BindOnce(&AuctionRunnerTest::OnBidder2GroupsRetrieved,
                                 base::Unretained(this)));
  }

  void OnBidder2GroupsRetrieved(
      std::vector<StorageInterestGroup> storage_interest_groups) {
    for (auto& bidder : storage_interest_groups) {
      if (bidder.bidding_group->group.owner == kBidder2 &&
          bidder.bidding_group->group.name == kBidder2Name) {
        result_.bidder2_bid_count = bidder.bidding_group->signals->bid_count;
        result_.bidder2_prev_wins =
            std::move(bidder.bidding_group->signals->prev_wins);
        SortPrevWins(result_.bidder2_prev_wins);
        break;
      }
    }

    auction_run_loop_->Quit();
  }

  auction_worklet::mojom::BiddingInterestGroupPtr MakeInterestGroup(
      url::Origin owner,
      std::string name,
      absl::optional<GURL> bidding_url,
      absl::optional<GURL> trusted_bidding_signals_url,
      std::vector<std::string> trusted_bidding_signals_keys,
      absl::optional<GURL> ad_url,
      absl::optional<std::vector<GURL>> ad_component_urls = absl::nullopt) {
    std::vector<blink::InterestGroup::Ad> ads;
    // Give only kBidder1 an InterestGroupAd ad with non-empty metadata, to
    // better test the `ad_metadata` output.
    if (ad_url) {
      if (owner == kBidder1) {
        ads.emplace_back(blink::InterestGroup::Ad(*ad_url, R"({"ads": true})"));
      } else {
        ads.emplace_back(blink::InterestGroup::Ad(*ad_url, absl::nullopt));
      }
    }

    absl::optional<std::vector<blink::InterestGroup::Ad>> ad_components;
    if (ad_component_urls) {
      ad_components.emplace();
      for (const GURL& ad_component_url : *ad_component_urls)
        ad_components->emplace_back(
            blink::InterestGroup::Ad(ad_component_url, absl::nullopt));
    }

    // Create fake previous wins. The time of these wins is ignored, since the
    // InterestGroupManager attaches the current time when logging a win.
    std::vector<auction_worklet::mojom::PreviousWinPtr> previous_wins;
    previous_wins.push_back(auction_worklet::mojom::PreviousWin::New(
        base::Time::Now(), R"({"winner": 0})"));
    previous_wins.push_back(auction_worklet::mojom::PreviousWin::New(
        base::Time::Now(), R"({"winner": -1})"));
    previous_wins.push_back(auction_worklet::mojom::PreviousWin::New(
        base::Time::Now(), R"({"winner": -2})"));

    return auction_worklet::mojom::BiddingInterestGroup::New(
        blink::InterestGroup(
            base::Time::Max(), std::move(owner), std::move(name),
            std::move(bidding_url),
            /* update_url = */ GURL(), std::move(trusted_bidding_signals_url),
            std::move(trusted_bidding_signals_keys), absl::nullopt,
            std::move(ads), std::move(ad_components)),
        auction_worklet::mojom::BiddingBrowserSignals::New(
            3, 5, std::move(previous_wins)));
  }

  void StartStandardAuction() {
    std::vector<StorageInterestGroup> bidders;
    bidders.emplace_back(MakeInterestGroup(
        kBidder1, kBidder1Name, kBidder1Url, kBidder1TrustedSignalsUrl,
        {"k1", "k2"}, GURL("https://ad1.com"),
        std::vector<GURL>{GURL("https://ad1.com-component1.com"),
                          GURL("https://ad1.com-component2.com")}));
    bidders.emplace_back(MakeInterestGroup(
        kBidder2, kBidder2Name, kBidder2Url, kBidder2TrustedSignalsUrl,
        {"l1", "l2"}, GURL("https://ad2.com"),
        std::vector<GURL>{GURL("https://ad2.com-component1.com"),
                          GURL("https://ad2.com-component2.com")}));

    StartAuction(kSellerUrl, std::move(bidders),
                 R"({"isAuctionSignals": true})", /* auction_signals_json */
                 auction_worklet::mojom::BrowserSignals::New(
                     url::Origin::Create(GURL("https://publisher1.com")),
                     url::Origin::Create(kSellerUrl)));
  }

  const Result& RunStandardAuction() {
    StartStandardAuction();
    auction_run_loop_->Run();
    return result_;
  }

  // Starts the standard auction with the mock worklet service, and waits for
  // the service to receive the worklet construction calls.
  void StartStandardAuctionWithMockService() {
    UseMockWorkletService();
    StartStandardAuction();
    mock_auction_process_manager_->WaitForWorklets(2 /* num_bidders */);
  }

  // AuctionRunner::Delegate implementation:
  network::mojom::URLLoaderFactory* GetFrameURLLoaderFactory() override {
    return &url_loader_factory_;
  }
  network::mojom::URLLoaderFactory* GetTrustedURLLoaderFactory() override {
    return &url_loader_factory_;
  }
  RenderFrameHostImpl* GetFrame() override { return nullptr; }
  network::mojom::ClientSecurityStatePtr GetClientSecurityState() override {
    return network::mojom::ClientSecurityState::New();
  }

  // DebuggableAuctionWorkletTracker::Observer implementation
  void AuctionWorkletCreated(DebuggableAuctionWorklet* worklet,
                             bool& should_pause_on_start) override {
    should_pause_on_start = (worklet->url() == pause_worklet_url_);
    observer_log_.push_back(base::StrCat({"Create ", worklet->url().spec()}));
    title_log_.push_back(worklet->Title());
  }

  void AuctionWorkletDestroyed(DebuggableAuctionWorklet* worklet) override {
    observer_log_.push_back(base::StrCat({"Destroy ", worklet->url().spec()}));
  }

  // Gets script URLs of currently live DebuggableAuctionWorklet.
  std::vector<std::string> LiveDebuggables() {
    std::vector<std::string> result;
    for (DebuggableAuctionWorklet* debuggable :
         DebuggableAuctionWorkletTracker::GetInstance()->GetAll()) {
      result.push_back(debuggable->url().spec());
    }
    return result;
  }

  // Enables use of a mock AuctionProcessManager when the next auction is run.
  void UseMockWorkletService() {
    auto mock_auction_process_manager =
        std::make_unique<MockAuctionProcessManager>();
    mock_auction_process_manager_ = mock_auction_process_manager.get();
    auction_process_manager_ = std::move(mock_auction_process_manager);
  }

  void CheckHistograms(AuctionRunner::AuctionResult expected_result,
                       int expected_interest_groups,
                       int expected_owners) {
    histogram_tester_->ExpectUniqueSample("Ads.InterestGroup.Auction.Result",
                                          expected_result, 1);
    histogram_tester_->ExpectUniqueSample(
        "Ads.InterestGroup.Auction.NumInterestGroups", expected_interest_groups,
        1);
    histogram_tester_->ExpectUniqueSample(
        "Ads.InterestGroup.Auction.NumOwnersWithInterestGroups",
        expected_owners, 1);
    histogram_tester_->ExpectTotalCount(
        "Ads.InterestGroup.Auction.AbortTime",
        expected_result == AuctionRunner::AuctionResult::kAborted);
    histogram_tester_->ExpectTotalCount(
        "Ads.InterestGroup.Auction.CompletedWithoutWinnerTime",
        expected_result == AuctionRunner::AuctionResult::kNoBids ||
            expected_result == AuctionRunner::AuctionResult::kAllBidsRejected);
    histogram_tester_->ExpectTotalCount(
        "Ads.InterestGroup.Auction.AuctionWithWinnerTime",
        expected_result == AuctionRunner::AuctionResult::kSuccess);
  }

  const url::Origin frame_origin_ =
      url::Origin::Create(GURL("https://frame.origin.test"));
  const GURL kSellerUrl{"https://adstuff.publisher1.com/auction.js"};
  absl::optional<GURL> trusted_scoring_signals_url_;

  const GURL kBidder1Url{"https://adplatform.com/offers.js"};
  const url::Origin kBidder1 = url::Origin::Create(kBidder1Url);
  const std::string kBidder1Name{"Ad Platform"};
  const GURL kBidder1TrustedSignalsUrl{"https://adplatform.com/signals1"};

  const GURL kBidder2Url{"https://anotheradthing.com/bids.js"};
  const url::Origin kBidder2 = url::Origin::Create(kBidder2Url);
  const std::string kBidder2Name{"Another Ad Thing"};
  const GURL kBidder2TrustedSignalsUrl{"https://anotheradthing.com/signals2"};

  base::test::TaskEnvironment task_environment_;

  // RunLoop that's quit on auction completion.
  std::unique_ptr<base::RunLoop> auction_run_loop_;
  // True if the most recently started auction has completed.
  bool auction_complete_ = false;
  // Result of the most recent auction.
  Result result_;

  network::TestURLLoaderFactory url_loader_factory_;

  // This is used (and consumed) when starting an auction, if non-null. Allows
  // either using a MockAuctionProcessManager instead of a
  // SameThreadAuctionProcessManager, or using a SameThreadAuctionProcessManager
  // that has already vended processes. If nullptr, a new
  // SameThreadAuctionProcessManager() is created when an auction is started.
  std::unique_ptr<AuctionProcessManager> auction_process_manager_;

  // Set by UseMockWorkletService(). Non-owning reference to the
  // AuctionProcessManager that will be / has been passed to the
  // InterestGroupManager.
  raw_ptr<MockAuctionProcessManager> mock_auction_process_manager_ = nullptr;

  // The InterestGroupManager is recreated and repopulated for each auction.
  std::unique_ptr<InterestGroupManager> interest_group_manager_;

  std::unique_ptr<AuctionRunner> auction_runner_;
  // This should be inspected using TakeBadMessage(), which also clears it.
  std::string bad_message_;

  std::unique_ptr<base::HistogramTester> histogram_tester_;

  std::vector<std::string> observer_log_;
  std::vector<std::string> title_log_;

  // Which worklet to pause, if any.
  GURL pause_worklet_url_;
};

// Runs the standard auction, but without adding any interest groups to the
// manager.
TEST_F(AuctionRunnerTest, NoInterestGroups) {
  RunAuctionAndWait(kSellerUrl, std::vector<StorageInterestGroup>(),
                    R"({"isAuctionSignals": true})" /* auction_signals_json */,
                    auction_worklet::mojom::BrowserSignals::New(
                        url::Origin::Create(GURL("https://publisher1.com")),
                        url::Origin::Create(kSellerUrl)));

  EXPECT_FALSE(result_.ad_url);
  EXPECT_FALSE(result_.ad_component_urls);
  EXPECT_FALSE(result_.seller_report_url);
  EXPECT_FALSE(result_.bidder_report_url);
  EXPECT_EQ(-1, result_.bidder1_bid_count);
  EXPECT_EQ(0u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(-1, result_.bidder2_bid_count);
  EXPECT_EQ(0u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kNoInterestGroups,
                  0 /* expected_interest_groups */, 0 /* expected_owners */);
}

// Runs an standard auction, but with an interest group that does not list any
// ads.
TEST_F(AuctionRunnerTest, OneInterestGroupNoAds) {
  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url, kBidder1TrustedSignalsUrl,
      {"k1", "k2"}, /* ad_url= */ absl::nullopt));

  RunAuctionAndWait(kSellerUrl, std::move(bidders),
                    R"({"isAuctionSignals": true})" /* auction_signals_json */,
                    auction_worklet::mojom::BrowserSignals::New(
                        url::Origin::Create(GURL("https://publisher1.com")),
                        url::Origin::Create(kSellerUrl)));

  EXPECT_FALSE(result_.ad_url);
  EXPECT_FALSE(result_.ad_component_urls);
  EXPECT_FALSE(result_.seller_report_url);
  EXPECT_FALSE(result_.bidder_report_url);
  EXPECT_EQ(5, result_.bidder1_bid_count);
  EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(-1, result_.bidder2_bid_count);
  EXPECT_EQ(0u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kNoInterestGroups,
                  0 /* expected_interest_groups */, 1 /* expected_owners */);
}

// Runs an standard auction, but with an interest group that does not list a
// bidding script.
TEST_F(AuctionRunnerTest, OneInterestGroupNoBidScript) {
  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, /* bidding_url= */ absl::nullopt,
      kBidder1TrustedSignalsUrl, {"k1", "k2"}, GURL("https://ad1.com")));

  RunAuctionAndWait(kSellerUrl, std::move(bidders),
                    R"({"isAuctionSignals": true})" /* auction_signals_json */,
                    auction_worklet::mojom::BrowserSignals::New(
                        url::Origin::Create(GURL("https://publisher1.com")),
                        url::Origin::Create(kSellerUrl)));

  EXPECT_FALSE(result_.ad_url);
  EXPECT_FALSE(result_.ad_component_urls);
  EXPECT_FALSE(result_.seller_report_url);
  EXPECT_FALSE(result_.bidder_report_url);
  EXPECT_EQ(5, result_.bidder1_bid_count);
  EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(-1, result_.bidder2_bid_count);
  EXPECT_EQ(0u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kNoInterestGroups,
                  0 /* expected_interest_groups */, 1 /* expected_owners */);
}

// Runs the standard auction, but with only adding one of the two standard
// interest groups to the manager.
TEST_F(AuctionRunnerTest, OneInterestGroup) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", /*num_ad_components=*/0, kBidder1,
                    kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2": "b", "extra": "c"})");

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url, kBidder1TrustedSignalsUrl,
      {"k1", "k2"}, GURL("https://ad1.com")));

  RunAuctionAndWait(kSellerUrl, std::move(bidders),
                    R"({"isAuctionSignals": true})" /* auction_signals_json */,
                    auction_worklet::mojom::BrowserSignals::New(
                        url::Origin::Create(GURL("https://publisher1.com")),
                        url::Origin::Create(kSellerUrl)));

  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
  EXPECT_FALSE(result_.ad_component_urls);
  EXPECT_EQ(GURL("https://reporting.example.com/"), result_.seller_report_url);
  EXPECT_EQ(GURL("https://buyer-reporting.example.com/"),
            result_.bidder_report_url);
  EXPECT_EQ(6, result_.bidder1_bid_count);
  ASSERT_EQ(4u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
            result_.bidder1_prev_wins[3]->ad_json);
  EXPECT_EQ(-1, result_.bidder2_bid_count);
  EXPECT_EQ(0u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  1 /* expected_interest_groups */, 1 /* expected_owners */);
  EXPECT_THAT(observer_log_,
              testing::UnorderedElementsAre(
                  "Create https://adstuff.publisher1.com/auction.js",
                  "Create https://adplatform.com/offers.js",
                  "Destroy https://adplatform.com/offers.js",
                  "Destroy https://adstuff.publisher1.com/auction.js",
                  "Create https://adplatform.com/offers.js",
                  "Destroy https://adplatform.com/offers.js"));
}

// An auction with two successful bids.
TEST_F(AuctionRunnerTest, Basic) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", /*num_ad_components=*/2, kBidder1,
                    kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", /*num_ad_components=*/2, kBidder2,
                    kBidder2Name,
                    /*has_signals=*/true, "l2", "b"));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2": "b", "extra": "c"})");
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra": "c"})");

  const Result& res = RunStandardAuction();
  EXPECT_EQ(GURL("https://ad2.com/"), res.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
            res.ad_component_urls);
  EXPECT_EQ(GURL("https://reporting.example.com/"), res.seller_report_url);
  EXPECT_EQ(GURL("https://buyer-reporting.example.com/"),
            res.bidder_report_url);
  EXPECT_EQ(6, res.bidder1_bid_count);
  EXPECT_EQ(3u, res.bidder1_prev_wins.size());
  EXPECT_EQ(6, res.bidder2_bid_count);
  ASSERT_EQ(4u, res.bidder2_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            res.bidder2_prev_wins[3]->ad_json);
  EXPECT_THAT(res.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
  EXPECT_THAT(observer_log_,
              testing::UnorderedElementsAre(
                  "Create https://adstuff.publisher1.com/auction.js",
                  "Create https://adplatform.com/offers.js",
                  "Create https://anotheradthing.com/bids.js",
                  "Destroy https://adplatform.com/offers.js",
                  "Destroy https://anotheradthing.com/bids.js",
                  "Destroy https://adstuff.publisher1.com/auction.js",
                  "Create https://anotheradthing.com/bids.js",
                  "Destroy https://anotheradthing.com/bids.js"));
  EXPECT_THAT(
      title_log_,
      testing::UnorderedElementsAre(
          "FLEDGE seller worklet for https://adstuff.publisher1.com/auction.js",
          "FLEDGE bidder worklet for https://adplatform.com/offers.js",
          "FLEDGE bidder worklet for https://anotheradthing.com/bids.js",
          "FLEDGE bidder worklet for https://anotheradthing.com/bids.js"));
}

TEST_F(AuctionRunnerTest, BasicDebug) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", /*num_ad_components=*/2, kBidder1,
                    kBidder1Name, true /* has_signals */, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", /*num_ad_components=*/2, kBidder2,
                    kBidder2Name, true /* has_signals */, "l2", "b"));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2": "b", "extra": "c"})");
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra": "c"})");

  for (const GURL& debug_url : {kBidder1Url, kBidder2Url, kSellerUrl}) {
    SCOPED_TRACE(debug_url);
    pause_worklet_url_ = debug_url;

    // Seller breakpoint is expected to hit twice.
    int expected_hits = (debug_url == kSellerUrl ? 2 : 1);

    StartStandardAuction();
    task_environment_.RunUntilIdle();

    bool found = false;
    mojo::Remote<blink::mojom::DevToolsAgent> agent;

    for (DebuggableAuctionWorklet* debuggable :
         DebuggableAuctionWorkletTracker::GetInstance()->GetAll()) {
      if (debuggable->url() == debug_url) {
        found = true;
        debuggable->ConnectDevToolsAgent(agent.BindNewPipeAndPassReceiver());
      }
    }
    ASSERT_TRUE(found);

    TestDevToolsAgentClient debug(std::move(agent), "S1",
                                  true /* use_binary_protocol */);
    debug.RunCommandAndWaitForResult(
        TestDevToolsAgentClient::Channel::kMain, 1, "Runtime.enable",
        R"({"id":1,"method":"Runtime.enable","params":{}})");
    debug.RunCommandAndWaitForResult(
        TestDevToolsAgentClient::Channel::kMain, 2, "Debugger.enable",
        R"({"id":2,"method":"Debugger.enable","params":{}})");

    // Set a breakpoint, and let the worklet run.
    const char kBreakpointTemplate[] = R"({
        "id":3,
        "method":"Debugger.setBreakpointByUrl",
        "params": {
          "lineNumber": 7,
          "url": "%s",
          "columnNumber": 0,
          "condition": ""
        }})";

    debug.RunCommandAndWaitForResult(
        TestDevToolsAgentClient::Channel::kMain, 3,
        "Debugger.setBreakpointByUrl",
        base::StringPrintf(kBreakpointTemplate, debug_url.spec().c_str()));
    debug.RunCommandAndWaitForResult(
        TestDevToolsAgentClient::Channel::kMain, 4,
        "Runtime.runIfWaitingForDebugger",
        R"({"id":4,"method":"Runtime.runIfWaitingForDebugger","params":{}})");

    // Should get breakpoint hit eventually.
    for (int hit = 0; hit < expected_hits; ++hit) {
      TestDevToolsAgentClient::Event breakpoint_hit =
          debug.WaitForMethodNotification("Debugger.paused");

      base::Value* hit_breakpoints =
          breakpoint_hit.value.FindListPath("params.hitBreakpoints");
      ASSERT_TRUE(hit_breakpoints);
      base::Value::ConstListView hit_breakpoints_list =
          hit_breakpoints->GetList();
      ASSERT_EQ(1u, hit_breakpoints_list.size());
      ASSERT_TRUE(hit_breakpoints_list[0].is_string());
      EXPECT_EQ(base::StringPrintf("1:7:0:%s", debug_url.spec().c_str()),
                hit_breakpoints_list[0].GetString());

      // Just resume execution.
      debug.RunCommandAndWaitForResult(
          TestDevToolsAgentClient::Channel::kIO, 6, "Debugger.resume",
          R"({"id":6,"method":"Debugger.resume","params":{}})");
    }

    // In the case bidder 2 wins the auction, the script will be reloaded, and
    // the second time it's loaded the worklet will also start in the paused
    // state. Resume it, so the test doesn't hang.
    if (debug_url == kBidder2Url) {
      task_environment_.RunUntilIdle();
      found = false;
      mojo::Remote<blink::mojom::DevToolsAgent> agent;
      for (DebuggableAuctionWorklet* debuggable :
           DebuggableAuctionWorkletTracker::GetInstance()->GetAll()) {
        if (debuggable->url() == debug_url) {
          found = true;
          debuggable->ConnectDevToolsAgent(agent.BindNewPipeAndPassReceiver());
        }
      }
      ASSERT_TRUE(found);

      TestDevToolsAgentClient debug(std::move(agent), "S1",
                                    true /* use_binary_protocol */);

      debug.RunCommandAndWaitForResult(
          TestDevToolsAgentClient::Channel::kMain, 1,
          "Runtime.runIfWaitingForDebugger",
          R"({"id":1,"method":"Runtime.runIfWaitingForDebugger","params":{}})");
    }

    // Let it finish --- result should as in Basic test since this didn't
    // actually change anything.
    auction_run_loop_->Run();
    EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
    EXPECT_EQ(GURL("https://reporting.example.com/"),
              result_.seller_report_url);
    EXPECT_EQ(GURL("https://buyer-reporting.example.com/"),
              result_.bidder_report_url);
  }
}

TEST_F(AuctionRunnerTest, PauseBidder) {
  pause_worklet_url_ = kBidder2Url;

  // Save a pointer to SameThreadAuctionProcessManager since we'll need its help
  // to resume things.
  auto process_manager = std::make_unique<SameThreadAuctionProcessManager>();
  SameThreadAuctionProcessManager* process_manager_impl = process_manager.get();
  auction_process_manager_ = std::move(process_manager);

  // Have a 404 for script 2 until ready to resume.
  url_loader_factory_.AddResponse(kBidder2Url.spec(), "", net::HTTP_NOT_FOUND);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", /*num_ad_components=*/2, kBidder1,
                    kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2": "b", "extra": "c"})");
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra": "c"})");

  StartStandardAuction();
  // Run all threads as far as they can get.
  task_environment_.RunUntilIdle();
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", /*num_ad_components=*/2, kBidder2,
                    kBidder2Name,
                    /*has_signals=*/true, "l2", "b"));

  process_manager_impl->ResumeAllPaused();

  // Need to resume a second time, when the script is re-loaded to run
  // ReportWin().
  task_environment_.RunUntilIdle();
  process_manager_impl->ResumeAllPaused();

  auction_run_loop_->Run();
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
            result_.ad_component_urls);
  EXPECT_EQ(GURL("https://reporting.example.com/"), result_.seller_report_url);
  EXPECT_EQ(GURL("https://buyer-reporting.example.com/"),
            result_.bidder_report_url);
  EXPECT_THAT(result_.errors, testing::ElementsAre());
}

TEST_F(AuctionRunnerTest, PauseSeller) {
  pause_worklet_url_ = kSellerUrl;

  // Save a pointer to SameThreadAuctionProcessManager since we'll need its help
  // to resume things.
  auto process_manager = std::make_unique<SameThreadAuctionProcessManager>();
  SameThreadAuctionProcessManager* process_manager_impl = process_manager.get();
  auction_process_manager_ = std::move(process_manager);

  // Have a 404 for seller until ready to resume.
  url_loader_factory_.AddResponse(kSellerUrl.spec(), "", net::HTTP_NOT_FOUND);

  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", /*num_ad_components=*/2, kBidder1,
                    kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", /*num_ad_components=*/2, kBidder2,
                    kBidder2Name,
                    /*has_signals=*/true, "l2", "b"));
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2": "b", "extra": "c"})");
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra": "c"})");

  StartStandardAuction();
  // Run all threads as far as they can get.
  task_environment_.RunUntilIdle();
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());

  process_manager_impl->ResumeAllPaused();

  auction_run_loop_->Run();
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
            result_.ad_component_urls);
  EXPECT_EQ(GURL("https://reporting.example.com/"), result_.seller_report_url);
  EXPECT_EQ(GURL("https://buyer-reporting.example.com/"),
            result_.bidder_report_url);
  EXPECT_THAT(result_.errors, testing::ElementsAre());
}

// An auction where one bid is successful, another's script 404s.
TEST_F(AuctionRunnerTest, OneBidOne404) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", /*num_ad_components=*/2, kBidder1,
                    kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));
  url_loader_factory_.AddResponse(kBidder2Url.spec(), "", net::HTTP_NOT_FOUND);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2": "b", "extra": "c"})");
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra": "c"})");

  const Result& res = RunStandardAuction();
  EXPECT_EQ(GURL("https://ad1.com/"), res.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad1.com-component1.com")},
            res.ad_component_urls);
  EXPECT_EQ(GURL("https://reporting.example.com/"), res.seller_report_url);
  EXPECT_EQ(GURL("https://buyer-reporting.example.com/"),
            res.bidder_report_url);
  EXPECT_EQ(6, res.bidder1_bid_count);
  ASSERT_EQ(4u, res.bidder1_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
            res.bidder1_prev_wins[3]->ad_json);
  EXPECT_EQ(5, res.bidder2_bid_count);
  EXPECT_EQ(3u, res.bidder2_prev_wins.size());
  EXPECT_THAT(
      res.errors,
      testing::ElementsAre("Failed to load https://anotheradthing.com/bids.js "
                           "HTTP status = 404 Not Found."));
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  2 /* expected_interest_groups */, 2 /* expected_owners */);

  // 404 is detected after the worklet is created, so there are still events
  // for it.
  EXPECT_THAT(observer_log_,
              testing::UnorderedElementsAre(
                  "Create https://adstuff.publisher1.com/auction.js",
                  "Create https://adplatform.com/offers.js",
                  "Create https://anotheradthing.com/bids.js",
                  "Destroy https://adplatform.com/offers.js",
                  "Destroy https://anotheradthing.com/bids.js",
                  "Destroy https://adstuff.publisher1.com/auction.js",
                  "Create https://adplatform.com/offers.js",
                  "Destroy https://adplatform.com/offers.js"));
}

// An auction where one bid is successful, another's script does not provide a
// bidding function.
TEST_F(AuctionRunnerTest, OneBidOneNotMade) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", /*num_ad_components=*/2, kBidder1,
                    kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));

  // The auction script doesn't make any bids.
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder2Url,
                                         MakeAuctionScript());
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2": "b", "extra": "c"})");
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra": "c"})");

  const Result& res = RunStandardAuction();
  EXPECT_EQ(GURL("https://ad1.com/"), res.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad1.com-component1.com")},
            res.ad_component_urls);
  EXPECT_EQ(GURL("https://reporting.example.com/"), res.seller_report_url);
  EXPECT_EQ(GURL("https://buyer-reporting.example.com/"),
            res.bidder_report_url);
  EXPECT_EQ(6, res.bidder1_bid_count);
  ASSERT_EQ(4u, res.bidder1_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
            res.bidder1_prev_wins[3]->ad_json);
  EXPECT_EQ(5, res.bidder2_bid_count);
  EXPECT_EQ(3u, res.bidder2_prev_wins.size());
  EXPECT_THAT(res.errors,
              testing::ElementsAre("https://anotheradthing.com/bids.js "
                                   "`generateBid` is not a function."));
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

// An auction where no bidding scripts load successfully.
TEST_F(AuctionRunnerTest, NoBids) {
  url_loader_factory_.AddResponse(kBidder1Url.spec(), "", net::HTTP_NOT_FOUND);
  url_loader_factory_.AddResponse(kBidder2Url.spec(), "", net::HTTP_NOT_FOUND);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2":"b", "extra":"c"})");
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra":"c"})");

  const Result& res = RunStandardAuction();
  EXPECT_FALSE(res.ad_url);
  EXPECT_FALSE(res.ad_component_urls);
  EXPECT_FALSE(res.seller_report_url);
  EXPECT_FALSE(res.bidder_report_url);
  EXPECT_EQ(5, res.bidder1_bid_count);
  EXPECT_EQ(3u, res.bidder1_prev_wins.size());
  EXPECT_EQ(5, res.bidder2_bid_count);
  EXPECT_EQ(3u, res.bidder2_prev_wins.size());
  EXPECT_THAT(res.errors,
              testing::UnorderedElementsAre(
                  "Failed to load https://adplatform.com/offers.js "
                  "HTTP status = 404 Not Found.",
                  "Failed to load https://anotheradthing.com/bids.js "
                  "HTTP status = 404 Not Found."));
  CheckHistograms(AuctionRunner::AuctionResult::kNoBids,
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

// An auction where none of the bidding scripts has a valid bidding function.
TEST_F(AuctionRunnerTest, NoBidMadeByScript) {
  // MakeAuctionScript() is a valid script that doesn't have a bidding function.
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         MakeAuctionScript());
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder2Url,
                                         MakeAuctionScript());
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2":"b", "extra":"c"})");
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra":"c"})");

  const Result& res = RunStandardAuction();
  EXPECT_FALSE(res.ad_url);
  EXPECT_FALSE(res.ad_component_urls);
  EXPECT_FALSE(res.seller_report_url);
  EXPECT_FALSE(res.bidder_report_url);
  EXPECT_EQ(5, res.bidder1_bid_count);
  EXPECT_EQ(3u, res.bidder1_prev_wins.size());
  EXPECT_EQ(5, res.bidder2_bid_count);
  EXPECT_EQ(3u, res.bidder2_prev_wins.size());
  EXPECT_THAT(
      res.errors,
      testing::UnorderedElementsAre(
          "https://adplatform.com/offers.js `generateBid` is not a function.",
          "https://anotheradthing.com/bids.js `generateBid` is not a "
          "function."));
  CheckHistograms(AuctionRunner::AuctionResult::kNoBids,
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

// An auction where the seller script doesn't have a scoring function.
TEST_F(AuctionRunnerTest, SellerRejectsAll) {
  std::string bid_script1 = MakeBidScript(
      "1", "https://ad1.com/", /*num_ad_components=*/2, kBidder1, kBidder1Name,
      /*has_signals=*/true, "k1", "a");
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         bid_script1);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", /*num_ad_components=*/2, kBidder2,
                    kBidder2Name,
                    /*has_signals=*/true, "l2", "b"));

  // No seller scoring function in a bid script.
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         bid_script1);
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2":"b", "extra":"c"})");
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra":"c"})");

  const Result& res = RunStandardAuction();
  EXPECT_FALSE(res.ad_url);
  EXPECT_FALSE(res.ad_component_urls);
  EXPECT_FALSE(res.seller_report_url);
  EXPECT_FALSE(res.bidder_report_url);
  EXPECT_EQ(6, res.bidder1_bid_count);
  EXPECT_EQ(3u, res.bidder1_prev_wins.size());
  EXPECT_EQ(6, res.bidder2_bid_count);
  EXPECT_EQ(3u, res.bidder2_prev_wins.size());
  EXPECT_THAT(res.errors, testing::UnorderedElementsAre(
                              "https://adstuff.publisher1.com/auction.js "
                              "`scoreAd` is not a function.",
                              "https://adstuff.publisher1.com/auction.js "
                              "`scoreAd` is not a function."));
  CheckHistograms(AuctionRunner::AuctionResult::kAllBidsRejected,
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

// An auction where seller rejects one bid when scoring.
TEST_F(AuctionRunnerTest, SellerRejectsOne) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", /*num_ad_components=*/2, kBidder1,
                    kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", /*num_ad_components=*/2, kBidder2,
                    kBidder2Name,
                    /*has_signals=*/true, "l2", "b"));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScriptReject2());
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2": "b", "extra": "c"})");
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra": "c"})");

  const Result& res = RunStandardAuction();
  EXPECT_EQ(GURL("https://ad1.com/"), res.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad1.com-component1.com")},
            res.ad_component_urls);
  EXPECT_EQ(GURL("https://reporting.example.com/"), res.seller_report_url);
  EXPECT_EQ(GURL("https://buyer-reporting.example.com/"),
            res.bidder_report_url);
  EXPECT_EQ(6, res.bidder1_bid_count);
  ASSERT_EQ(4u, res.bidder1_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
            res.bidder1_prev_wins[3]->ad_json);
  EXPECT_EQ(6, res.bidder2_bid_count);
  EXPECT_EQ(3u, res.bidder2_prev_wins.size());
  EXPECT_THAT(res.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

// An auction where the seller script fails to load.
TEST_F(AuctionRunnerTest, NoSellerScript) {
  // Tests to make sure that if seller script fails the other fetches are
  // cancelled, too.
  url_loader_factory_.AddResponse(kSellerUrl.spec(), "", net::HTTP_NOT_FOUND);
  const Result& res = RunStandardAuction();
  EXPECT_FALSE(res.ad_url);
  EXPECT_FALSE(res.ad_component_urls);
  EXPECT_FALSE(res.seller_report_url);
  EXPECT_FALSE(res.bidder_report_url);

  EXPECT_EQ(0, url_loader_factory_.NumPending());
  EXPECT_EQ(5, res.bidder1_bid_count);
  EXPECT_EQ(3u, res.bidder1_prev_wins.size());
  EXPECT_EQ(5, res.bidder2_bid_count);
  EXPECT_EQ(3u, res.bidder2_prev_wins.size());
  EXPECT_THAT(res.errors,
              testing::ElementsAre(
                  "Failed to load https://adstuff.publisher1.com/auction.js "
                  "HTTP status = 404 Not Found."));
  CheckHistograms(AuctionRunner::AuctionResult::kSellerWorkletLoadFailed,
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

// An auction where bidders don't request trusted bidding signals.
TEST_F(AuctionRunnerTest, NoTrustedBiddingSignals) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", /*num_ad_components=*/0, kBidder1,
                    kBidder1Name,
                    /*has_signals=*/false, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", /*num_ad_components=*/0, kBidder2,
                    kBidder2Name,
                    /*has_signals=*/false, "l2", "b"));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(kBidder1, kBidder1Name, kBidder1Url,
                                         absl::nullopt, {"k1", "k2"},
                                         GURL("https://ad1.com")));
  bidders.emplace_back(MakeInterestGroup(kBidder2, kBidder2Name, kBidder2Url,
                                         absl::nullopt, {"l1", "l2"},
                                         GURL("https://ad2.com")));

  const Result& res = RunAuctionAndWait(
      kSellerUrl, std::move(bidders),
      R"({"isAuctionSignals": true})", /* auction_signals_json */
      auction_worklet::mojom::BrowserSignals::New(
          url::Origin::Create(GURL("https://publisher1.com")),
          url::Origin::Create(kSellerUrl)));

  EXPECT_EQ(GURL("https://ad2.com/"), res.ad_url);
  EXPECT_FALSE(result_.ad_component_urls);
  EXPECT_EQ(GURL("https://reporting.example.com/"), res.seller_report_url);
  EXPECT_EQ(GURL("https://buyer-reporting.example.com/"),
            res.bidder_report_url);
  EXPECT_EQ(6, res.bidder1_bid_count);
  EXPECT_EQ(3u, res.bidder1_prev_wins.size());
  EXPECT_EQ(6, res.bidder2_bid_count);
  ASSERT_EQ(4u, res.bidder2_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            res.bidder2_prev_wins[3]->ad_json);
  EXPECT_THAT(res.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

// An auction where trusted bidding signals are requested, but the fetch 404s.
TEST_F(AuctionRunnerTest, TrustedBiddingSignals404) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", /*num_ad_components=*/2, kBidder1,
                    kBidder1Name,
                    /*has_signals=*/false, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", /*num_ad_components=*/2, kBidder2,
                    kBidder2Name,
                    /*has_signals=*/false, "l2", "b"));
  url_loader_factory_.AddResponse(
      kBidder1TrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=k1,k2",
      "", net::HTTP_NOT_FOUND);
  url_loader_factory_.AddResponse(
      kBidder2TrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=l1,l2",
      "", net::HTTP_NOT_FOUND);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());

  const Result& res = RunStandardAuction();
  EXPECT_EQ(GURL("https://ad2.com/"), res.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
            res.ad_component_urls);
  EXPECT_EQ(GURL("https://reporting.example.com/"), res.seller_report_url);
  EXPECT_EQ(GURL("https://buyer-reporting.example.com/"),
            res.bidder_report_url);
  EXPECT_EQ(6, res.bidder1_bid_count);
  EXPECT_EQ(3u, res.bidder1_prev_wins.size());
  EXPECT_EQ(6, res.bidder2_bid_count);
  ASSERT_EQ(4u, res.bidder2_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            res.bidder2_prev_wins[3]->ad_json);
  EXPECT_THAT(res.errors, testing::UnorderedElementsAre(
                              "Failed to load "
                              "https://adplatform.com/"
                              "signals1?hostname=publisher1.com&keys=k1,k2 "
                              "HTTP status = 404 Not Found.",
                              "Failed to load "
                              "https://anotheradthing.com/"
                              "signals2?hostname=publisher1.com&keys=l1,l2 "
                              "HTTP status = 404 Not Found."));
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

// A successful auction where seller reporting worklet doesn't set a URL.
TEST_F(AuctionRunnerTest, NoReportResultUrl) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", /*num_ad_components=*/2, kBidder1,
                    kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", /*num_ad_components=*/2, kBidder2,
                    kBidder2Name,
                    /*has_signals=*/true, "l2", "b"));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScriptNoReportUrl());
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2": "b", "extra": "c"})");
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra": "c"})");

  const Result& res = RunStandardAuction();
  EXPECT_EQ(GURL("https://ad2.com/"), res.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
            res.ad_component_urls);
  EXPECT_FALSE(res.seller_report_url);
  EXPECT_EQ(GURL("https://buyer-reporting.example.com/"),
            res.bidder_report_url);
  EXPECT_EQ(6, res.bidder1_bid_count);
  EXPECT_EQ(3u, res.bidder1_prev_wins.size());
  EXPECT_EQ(6, res.bidder2_bid_count);
  ASSERT_EQ(4u, res.bidder2_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            res.bidder2_prev_wins[3]->ad_json);
  EXPECT_THAT(res.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

// A successful auction where bidder reporting worklet doesn't set a URL.
TEST_F(AuctionRunnerTest, NoReportWinUrl) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", /*num_ad_components=*/2, kBidder1,
                    kBidder1Name,
                    /*has_signals=*/true, "k1", "a") +
          kReportWinNoUrl);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", /*num_ad_components=*/2, kBidder2,
                    kBidder2Name,
                    /*has_signals=*/true, "l2", "b") +
          kReportWinNoUrl);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2": "b", "extra": "c"})");
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra": "c"})");

  const Result& res = RunStandardAuction();
  EXPECT_EQ(GURL("https://ad2.com/"), res.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
            res.ad_component_urls);
  EXPECT_EQ(GURL("https://reporting.example.com/"), res.seller_report_url);
  EXPECT_FALSE(res.bidder_report_url);
  EXPECT_EQ(6, res.bidder1_bid_count);
  EXPECT_EQ(3u, res.bidder1_prev_wins.size());
  EXPECT_EQ(6, res.bidder2_bid_count);
  ASSERT_EQ(4u, res.bidder2_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            res.bidder2_prev_wins[3]->ad_json);
  EXPECT_THAT(res.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

// A successful auction where neither reporting worklets sets a URL.
TEST_F(AuctionRunnerTest, NeitherReportUrl) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", /*num_ad_components=*/2, kBidder1,
                    kBidder1Name,
                    /*has_signals=*/true, "k1", "a") +
          kReportWinNoUrl);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", /*num_ad_components=*/2, kBidder2,
                    kBidder2Name,
                    /*has_signals=*/true, "l2", "b") +
          kReportWinNoUrl);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScriptNoReportUrl());
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2": "b", "extra": "c"})");
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra": "c"})");

  const Result& res = RunStandardAuction();
  EXPECT_EQ(GURL("https://ad2.com/"), res.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
            res.ad_component_urls);
  EXPECT_FALSE(res.seller_report_url);
  EXPECT_FALSE(res.bidder_report_url);
  EXPECT_EQ(6, res.bidder1_bid_count);
  EXPECT_EQ(3u, res.bidder1_prev_wins.size());
  EXPECT_EQ(6, res.bidder2_bid_count);
  ASSERT_EQ(4u, res.bidder2_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            res.bidder2_prev_wins[3]->ad_json);
  EXPECT_THAT(res.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

// Test the case where the seller worklet provides no signals for the winner,
// since it has no reportResult() method. The winning bidder's reportWin()
// function should be passed null as `sellerSignals`, and should still be able
// to send a report.
TEST_F(AuctionRunnerTest, NoReportResult) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", /*num_ad_components=*/2, kBidder1,
                    kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", /*num_ad_components=*/2, kBidder2,
                    kBidder2Name,
                    /*has_signals=*/true, "l2", "b") +
          kReportWinExpectNullAuctionSignals);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         kCheckingAuctionScript);
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2": "b", "extra": "c"})");
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra": "c"})");

  const Result& res = RunStandardAuction();
  EXPECT_EQ(GURL("https://ad2.com/"), res.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
            res.ad_component_urls);
  EXPECT_FALSE(res.seller_report_url);
  EXPECT_EQ(GURL("https://seller.signals.were.null.test/"),
            res.bidder_report_url);
  EXPECT_EQ(6, res.bidder1_bid_count);
  EXPECT_EQ(3u, res.bidder1_prev_wins.size());
  EXPECT_EQ(6, res.bidder2_bid_count);
  ASSERT_EQ(4u, res.bidder2_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            res.bidder2_prev_wins[3]->ad_json);
  EXPECT_THAT(res.errors, testing::ElementsAre(base::StringPrintf(
                              "%s `reportResult` is not a function.",
                              kSellerUrl.spec().c_str())));
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

TEST_F(AuctionRunnerTest, TrustedScoringSignals) {
  trusted_scoring_signals_url_ =
      GURL("https://adstuff.publisher1.com/seller_signals");

  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", /*num_ad_components=*/2, kBidder1,
                    kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", /*num_ad_components=*/2, kBidder2,
                    kBidder2Name,
                    /*has_signals=*/true, "l2", "b") +
          kReportWinExpectNullAuctionSignals);
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2": "b", "extra": "c"})");
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra": "c"})");

  // scoreAd() that only accepts bids where the scoring signals of the
  // `renderUrl` is "accept".
  auction_worklet::AddJavascriptResponse(&url_loader_factory_,
                                         kSellerUrl, std::string(R"(
function scoreAd(adMetadata, bid, auctionConfig, trustedScoringSignals,
                 browserSignals) {
  let signal = trustedScoringSignals.renderUrl[browserSignals.renderUrl];
  // 2 * bid is expected by the BidderWorklet ReportWin() script.
  if (signal == "accept")
    return 2 * bid;
  if (signal == "reject")
    return 0;
  throw "incorrect trustedScoringSignals";
}
                                         )") + kReportResultScript);

  // Only accept first bidder's bid.
  auction_worklet::AddJsonResponse(
      &url_loader_factory_,
      GURL(trusted_scoring_signals_url_->spec() +
           "?hostname=publisher1.com"
           "&renderUrls=https%3A%2F%2Fad1.com%2F"
           "&adComponentRenderUrls=https%3A%2F%2Fad1.com-component1.com%2F"),
      R"({"renderUrls":{"https://ad1.com/":"accept"}})");
  auction_worklet::AddJsonResponse(
      &url_loader_factory_,
      GURL(trusted_scoring_signals_url_->spec() +
           "?hostname=publisher1.com"
           "&renderUrls=https%3A%2F%2Fad2.com%2F"
           "&adComponentRenderUrls=https%3A%2F%2Fad2.com-component1.com%2F"),
      R"({"renderUrls":{"https://ad2.com/":"reject"}})");

  RunStandardAuction();
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad1.com-component1.com")},
            result_.ad_component_urls);
  EXPECT_EQ(GURL("https://reporting.example.com/"), result_.seller_report_url);
  EXPECT_EQ(GURL("https://buyer-reporting.example.com/"),
            result_.bidder_report_url);
  EXPECT_EQ(6, result_.bidder1_bid_count);
  EXPECT_EQ(4u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
            result_.bidder1_prev_wins[3]->ad_json);
  EXPECT_EQ(6, result_.bidder2_bid_count);
  ASSERT_EQ(3u, result_.bidder2_prev_wins.size());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2);
}

// Test the case where the ProcessManager delays the auction.
TEST_F(AuctionRunnerTest, ProcessManagerDelaysAuction) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", /*num_ad_components=*/2, kBidder1,
                    kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", /*num_ad_components=*/2, kBidder2,
                    kBidder2Name,
                    /*has_signals=*/true, "l2", "b"));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2": "b", "extra": "c"})");
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra": "c"})");

  // Create AuctionProcessManager in advance of starting the auction so can
  // create seller worklets before the auction starts.
  auction_process_manager_ =
      std::make_unique<SameThreadAuctionProcessManager>();

  AuctionProcessManager* auction_process_manager =
      auction_process_manager_.get();

  // Make kMaxActiveSellerWorklets seller worklet requests for kSellerUrl's
  // origin.
  std::list<std::unique_ptr<AuctionProcessManager::ProcessHandle>> sellers;
  for (size_t i = 0; i < AuctionProcessManager::kMaxActiveSellerWorklets; ++i) {
    sellers.push_back(std::make_unique<AuctionProcessManager::ProcessHandle>());
    EXPECT_TRUE(auction_process_manager->RequestWorkletService(
        AuctionProcessManager::WorkletType::kSeller,
        url::Origin::Create(kSellerUrl), &*sellers.back(), base::BindOnce([]() {
          ADD_FAILURE() << "This should not be called";
        })));
  }

  StartStandardAuction();

  // The auction should be waiting for a seller worklet slot.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1u, auction_process_manager->GetPendingSellerRequestsForTesting());
  EXPECT_EQ(0u, auction_process_manager->GetPendingBidderRequestsForTesting());
  EXPECT_FALSE(auction_complete_);

  // Make kMaxBidderProcesses bidder worklet requests different bidder origins.
  // Do this after starting the auction, so the auction will incorrectly
  // complete once a seller worklet slot is freed if the auction already
  // requested bidder worklet processes.
  std::list<std::unique_ptr<AuctionProcessManager::ProcessHandle>> bidders;
  for (size_t i = 0; i < AuctionProcessManager::kMaxBidderProcesses; ++i) {
    bidders.push_back(std::make_unique<AuctionProcessManager::ProcessHandle>());
    url::Origin origin = url::Origin::Create(
        GURL(base::StringPrintf("https://blocking.bidder.%zu.test", i)));
    EXPECT_TRUE(auction_process_manager->RequestWorkletService(
        AuctionProcessManager::WorkletType::kBidder, origin, &*bidders.back(),
        base::BindOnce(
            []() { ADD_FAILURE() << "This should not be called"; })));
  }

  // Free up a seller slot. Auction should now be blocked waiting for bidder
  // slots.
  sellers.pop_front();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0u, auction_process_manager->GetPendingSellerRequestsForTesting());
  EXPECT_EQ(2u, auction_process_manager->GetPendingBidderRequestsForTesting());
  EXPECT_FALSE(auction_complete_);

  // Free up a single bidder slot. The auction should now run to completion -
  // since each bidder is freed once it is run, only need one bidder slot free
  // to run the auction.
  bidders.pop_front();
  auction_run_loop_->Run();
  EXPECT_TRUE(auction_complete_);

  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
            result_.ad_component_urls);
  EXPECT_EQ(GURL("https://reporting.example.com/"), result_.seller_report_url);
  EXPECT_EQ(GURL("https://buyer-reporting.example.com/"),
            result_.bidder_report_url);
  EXPECT_EQ(6, result_.bidder1_bid_count);
  EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(6, result_.bidder2_bid_count);
  ASSERT_EQ(4u, result_.bidder2_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            result_.bidder2_prev_wins[3]->ad_json);
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

TEST_F(AuctionRunnerTest, AllBiddersCrashBeforeBidding) {
  for (bool seller_worklet_loads_first : {false, true}) {
    observer_log_.clear();
    SCOPED_TRACE(seller_worklet_loads_first);

    StartStandardAuctionWithMockService();
    auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
    ASSERT_TRUE(seller_worklet);
    auto bidder1_worklet = mock_auction_process_manager_->TakeBidderWorklet(
        kBidder1, kBidder1Name);
    ASSERT_TRUE(bidder1_worklet);
    auto bidder2_worklet = mock_auction_process_manager_->TakeBidderWorklet(
        kBidder2, kBidder2Name);
    ASSERT_TRUE(bidder2_worklet);

    if (seller_worklet_loads_first) {
      seller_worklet->CompleteLoading();
      mock_auction_process_manager_->Flush();
    }
    EXPECT_FALSE(auction_complete_);

    EXPECT_THAT(observer_log_,
                testing::UnorderedElementsAre(
                    "Create https://adstuff.publisher1.com/auction.js",
                    "Create https://adplatform.com/offers.js",
                    "Create https://anotheradthing.com/bids.js"));

    EXPECT_THAT(LiveDebuggables(),
                testing::UnorderedElementsAre(
                    "https://adplatform.com/offers.js",
                    "https://anotheradthing.com/bids.js",
                    "https://adstuff.publisher1.com/auction.js"));

    // Have to keep the callbacks around, since the AuctionWorkletService is
    // still live. Closing it here causes more issues than its worth (seller
    // worklet can't complete loading if it hasn't already). In production,
    // there would be multiple service processes in this case, with different
    // pipes.
    auto bidder1_callback = bidder1_worklet->TakeLoadCallback();
    auto bidder2_callback = bidder2_worklet->TakeLoadCallback();
    bidder1_worklet.reset();
    bidder2_worklet.reset();

    base::RunLoop().RunUntilIdle();

    EXPECT_THAT(observer_log_,
                testing::UnorderedElementsAre(
                    "Create https://adstuff.publisher1.com/auction.js",
                    "Create https://adplatform.com/offers.js",
                    "Create https://anotheradthing.com/bids.js",
                    "Destroy https://adplatform.com/offers.js",
                    "Destroy https://anotheradthing.com/bids.js",
                    "Destroy https://adstuff.publisher1.com/auction.js"));

    EXPECT_THAT(LiveDebuggables(), testing::ElementsAre());

    // The auction isn't failed until the seller worklet has completed loading.
    if (!seller_worklet_loads_first)
      seller_worklet->CompleteLoading();

    auction_run_loop_->Run();

    EXPECT_FALSE(result_.ad_url);
    EXPECT_FALSE(result_.ad_component_urls);
    EXPECT_FALSE(result_.seller_report_url);
    EXPECT_FALSE(result_.bidder_report_url);
    EXPECT_EQ(5, result_.bidder1_bid_count);
    EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
    EXPECT_EQ(5, result_.bidder2_bid_count);
    EXPECT_EQ(3u, result_.bidder2_prev_wins.size());
    EXPECT_THAT(
        result_.errors,
        testing::UnorderedElementsAre(
            base::StringPrintf("%s crashed while trying to run generateBid().",
                               kBidder1Url.spec().c_str()),
            base::StringPrintf("%s crashed while trying to run generateBid().",
                               kBidder2Url.spec().c_str())));

    // Need to close the AuctionWorkletService pipes so callbacks can be
    // destroyed without DCHECKing.
    mock_auction_process_manager_->ClosePipes();
    CheckHistograms(AuctionRunner::AuctionResult::kNoBids,
                    2 /* expected_interest_groups */, 2 /* expected_owners */);
  }
}

// Test the case a single bidder worklet crashes before bidding. The auction
// should continue, without that bidder's bid.
TEST_F(AuctionRunnerTest, BidderCrashBeforeBidding) {
  for (bool other_bidder_finishes_first : {false, true}) {
    SCOPED_TRACE(other_bidder_finishes_first);
    for (bool seller_worklet_loads_first : {false, true}) {
      SCOPED_TRACE(seller_worklet_loads_first);
      observer_log_.clear();
      StartStandardAuctionWithMockService();
      auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
      ASSERT_TRUE(seller_worklet);
      auto bidder1_worklet = mock_auction_process_manager_->TakeBidderWorklet(
          kBidder1, kBidder1Name);
      ASSERT_TRUE(bidder1_worklet);
      auto bidder2_worklet = mock_auction_process_manager_->TakeBidderWorklet(
          kBidder2, kBidder2Name);
      ASSERT_TRUE(bidder2_worklet);

      ASSERT_FALSE(auction_complete_);
      if (seller_worklet_loads_first)
        seller_worklet->CompleteLoading();
      if (other_bidder_finishes_first) {
        bidder2_worklet->InvokeGenerateBidCallback(7 /* bid */,
                                                   GURL("https://ad2.com/"));
        // The bidder pipe should be closed after it bids.
        EXPECT_TRUE(bidder2_worklet->PipeIsClosed());
        bidder2_worklet.reset();
      }
      mock_auction_process_manager_->Flush();

      ASSERT_FALSE(auction_complete_);

      // Close Bidder1's pipe, keeping its callback alive to avoid a DCHECK.
      auto bidder1_callback = bidder1_worklet->TakeLoadCallback();
      bidder1_worklet.reset();
      // Can't flush the closed pipe without reaching into AuctionRunner, so use
      // RunUntilIdle() instead.
      base::RunLoop().RunUntilIdle();

      if (!seller_worklet_loads_first)
        seller_worklet->CompleteLoading();
      if (!other_bidder_finishes_first) {
        bidder2_worklet->InvokeGenerateBidCallback(7 /* bid */,
                                                   GURL("https://ad2.com/"));
        // The bidder pipe should be closed after it bids.
        EXPECT_TRUE(bidder2_worklet->PipeIsClosed());
        bidder2_worklet.reset();
      }
      mock_auction_process_manager_->Flush();

      EXPECT_THAT(observer_log_,
                  testing::UnorderedElementsAre(
                      "Create https://adstuff.publisher1.com/auction.js",
                      "Create https://adplatform.com/offers.js",
                      "Create https://anotheradthing.com/bids.js",
                      "Destroy https://adplatform.com/offers.js",
                      "Destroy https://anotheradthing.com/bids.js"));

      EXPECT_THAT(
          LiveDebuggables(),
          testing::ElementsAre("https://adstuff.publisher1.com/auction.js"));

      // The auction should be scored without waiting on the crashed kBidder1.
      auto score_ad_params = seller_worklet->WaitForScoreAd();
      EXPECT_EQ(kBidder2, score_ad_params->interest_group_owner);
      EXPECT_EQ(7, score_ad_params->bid);
      seller_worklet->InvokeScoreAdCallback(11);

      // Finish the auction.
      seller_worklet->WaitForReportResult();
      seller_worklet->InvokeReportResultCallback();

      // Worklet 2 should be reloaded and ReportWin() invoked.
      mock_auction_process_manager_->WaitForWinningBidderReload();
      bidder2_worklet = mock_auction_process_manager_->TakeBidderWorklet(
          kBidder2, kBidder2Name);
      bidder2_worklet->WaitForReportWin();
      bidder2_worklet->InvokeReportWinCallback();

      // Bidder2 won, Bidder1 crashed.
      auction_run_loop_->Run();
      EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
      EXPECT_FALSE(result_.ad_component_urls);
      EXPECT_FALSE(result_.seller_report_url);
      EXPECT_FALSE(result_.bidder_report_url);
      EXPECT_EQ(5, result_.bidder1_bid_count);
      EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
      EXPECT_EQ(6, result_.bidder2_bid_count);
      ASSERT_EQ(4u, result_.bidder2_prev_wins.size());
      EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
                result_.bidder2_prev_wins[3]->ad_json);
      EXPECT_THAT(result_.errors,
                  testing::ElementsAre(base::StringPrintf(
                      "%s crashed while trying to run generateBid().",
                      kBidder1Url.spec().c_str())));
      CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                      2 /* expected_interest_groups */,
                      2 /* expected_owners */);

      // Need to close the AuctionWorkletService pipes so callbacks can be
      // destroyed without DCHECKing.
      mock_auction_process_manager_->ClosePipes();
    }
  }
}

// If the winning bidder crashes while coming up with the reporting URL, the
// auction should fail.
TEST_F(AuctionRunnerTest, WinningBidderCrashWhileReporting) {
  StartStandardAuctionWithMockService();

  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1, kBidder1Name);
  ASSERT_TRUE(bidder1_worklet);
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2, kBidder2Name);
  ASSERT_TRUE(bidder2_worklet);

  seller_worklet->CompleteLoading();
  bidder1_worklet->InvokeGenerateBidCallback(7 /* bid */,
                                             GURL("https://ad1.com/"));
  // The bidder pipe should be closed after it bids.
  EXPECT_TRUE(bidder1_worklet->PipeIsClosed());
  bidder1_worklet.reset();
  bidder2_worklet->InvokeGenerateBidCallback(5 /* bid */,
                                             GURL("https://ad2.com/"));
  // The bidder pipe should be closed after it bids.
  EXPECT_TRUE(bidder2_worklet->PipeIsClosed());
  bidder2_worklet.reset();

  // Score Bidder1's bid.
  auto score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder1, score_ad_params->interest_group_owner);
  EXPECT_EQ(7, score_ad_params->bid);
  seller_worklet->InvokeScoreAdCallback(11 /* score */);

  // Score Bidder2's bid.
  score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder2, score_ad_params->interest_group_owner);
  EXPECT_EQ(5, score_ad_params->bid);
  seller_worklet->InvokeScoreAdCallback(10 /* score */);

  // Bidder1 crashes while running ReportWin.
  seller_worklet->WaitForReportResult();
  seller_worklet->InvokeReportResultCallback();
  mock_auction_process_manager_->WaitForWinningBidderReload();
  bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1, kBidder1Name);
  bidder1_worklet->WaitForReportWin();
  bidder1_worklet.reset();
  auction_run_loop_->Run();

  // No bidder won, Bidder1 crashed.
  EXPECT_FALSE(result_.ad_url);
  EXPECT_FALSE(result_.ad_component_urls);
  EXPECT_FALSE(result_.seller_report_url);
  EXPECT_FALSE(result_.bidder_report_url);
  EXPECT_EQ(6, result_.bidder1_bid_count);
  EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(6, result_.bidder2_bid_count);
  EXPECT_EQ(3u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors, testing::ElementsAre(base::StringPrintf(
                                  "%s crashed while trying to run reportWin().",
                                  kBidder1Url.spec().c_str())));
  CheckHistograms(AuctionRunner::AuctionResult::kWinningBidderWorkletCrashed,
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

// If the seller crashes at any point in the auction, the auction fails.
TEST_F(AuctionRunnerTest, SellerCrash) {
  enum class CrashPhase {
    kLoad,
    kLoadAfterBiddersLoaded,
    kScoreBid,
    kReportResult,
  };
  for (CrashPhase crash_phase :
       {CrashPhase::kLoad, CrashPhase::kLoadAfterBiddersLoaded,
        CrashPhase::kScoreBid, CrashPhase::kReportResult}) {
    SCOPED_TRACE(static_cast<int>(crash_phase));

    StartStandardAuctionWithMockService();

    auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
    ASSERT_TRUE(seller_worklet);
    auto bidder1_worklet = mock_auction_process_manager_->TakeBidderWorklet(
        kBidder1, kBidder1Name);
    ASSERT_TRUE(bidder1_worklet);
    auto bidder2_worklet = mock_auction_process_manager_->TakeBidderWorklet(
        kBidder2, kBidder2Name);
    ASSERT_TRUE(bidder2_worklet);

    // While loop to allow breaking when the crash stage is reached.
    while (true) {
      if (crash_phase == CrashPhase::kLoad) {
        // Need to close the AuctionWorkletService pipes so callbacks can be
        // destroyed without DCHECKing.
        mock_auction_process_manager_->ClosePipes();
        seller_worklet.reset();
        break;
      }

      bidder1_worklet->InvokeGenerateBidCallback(5 /* bid */,
                                                 GURL("https://ad1.com/"));
      bidder2_worklet->InvokeGenerateBidCallback(7 /* bid */,
                                                 GURL("https://ad2.com/"));
      // Wait for bids to be received.
      base::RunLoop().RunUntilIdle();

      if (crash_phase == CrashPhase::kLoadAfterBiddersLoaded) {
        // Need to close the AuctionWorkletService pipes so callbacks can be
        // destroyed without DCHECKing.
        mock_auction_process_manager_->ClosePipes();
        seller_worklet.reset();
        break;
      }

      seller_worklet->CompleteLoading();

      // Wait for Bidder1's bid.
      auto score_ad_params = seller_worklet->WaitForScoreAd();
      if (crash_phase == CrashPhase::kScoreBid) {
        seller_worklet.reset();
        break;
      }
      // Score Bidder1's bid.
      bidder1_worklet.reset();
      EXPECT_EQ(kBidder1, score_ad_params->interest_group_owner);
      EXPECT_EQ(5, score_ad_params->bid);
      seller_worklet->InvokeScoreAdCallback(10 /* score */);

      // Score Bidder2's bid.
      score_ad_params = seller_worklet->WaitForScoreAd();
      EXPECT_EQ(kBidder2, score_ad_params->interest_group_owner);
      EXPECT_EQ(7, score_ad_params->bid);
      seller_worklet->InvokeScoreAdCallback(11 /* score */);

      seller_worklet->WaitForReportResult();
      DCHECK_EQ(CrashPhase::kReportResult, crash_phase);
      seller_worklet.reset();
      break;
    }

    // Wait for auction to complete.
    auction_run_loop_->Run();

    // No bidder won, seller crashed.
    EXPECT_FALSE(result_.ad_url);
    EXPECT_FALSE(result_.ad_component_urls);
    EXPECT_FALSE(result_.seller_report_url);
    EXPECT_FALSE(result_.bidder_report_url);
    if (crash_phase != CrashPhase::kReportResult) {
      EXPECT_EQ(5, result_.bidder1_bid_count);
      EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
      EXPECT_EQ(5, result_.bidder2_bid_count);
      EXPECT_EQ(3u, result_.bidder2_prev_wins.size());
    } else {
      // If the seller worklet crashes while calculating the report URL, still
      // report bids.
      EXPECT_EQ(6, result_.bidder1_bid_count);
      EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
      EXPECT_EQ(6, result_.bidder2_bid_count);
      EXPECT_EQ(3u, result_.bidder2_prev_wins.size());
    }
    CheckHistograms(AuctionRunner::AuctionResult::kSellerWorkletCrashed,
                    2 /* expected_interest_groups */, 2 /* expected_owners */);
    EXPECT_THAT(result_.errors, testing::ElementsAre(base::StringPrintf(
                                    "%s crashed.", kSellerUrl.spec().c_str())));
  }
}

TEST_F(AuctionRunnerTest, NullAdComponents) {
  const GURL kRenderUrl = GURL("https://ad1.com");
  const struct {
    absl::optional<std::vector<GURL>> bid_ad_component_urls;
    bool expect_successful_bid;
  } kTestCases[] = {
      {absl::nullopt, true},
      {std::vector<GURL>{}, false},
      {std::vector<GURL>{GURL("https://ad1.com-component1.com")}, false},
  };

  for (const auto& test_case : kTestCases) {
    UseMockWorkletService();
    std::vector<StorageInterestGroup> bidders;
    bidders.emplace_back(
        MakeInterestGroup(kBidder1, kBidder1Name, kBidder1Url,
                          kBidder1TrustedSignalsUrl, {"k1", "k2"}, kRenderUrl,
                          /*ad_component_urls=*/absl::nullopt));

    StartAuction(kSellerUrl, std::move(bidders),
                 /*auction_signals_json=*/"{}",
                 auction_worklet::mojom::BrowserSignals::New(
                     url::Origin::Create(GURL("https://publisher1.com")),
                     url::Origin::Create(kSellerUrl)));

    mock_auction_process_manager_->WaitForWorklets(/*num_bidders=*/1);

    auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
    ASSERT_TRUE(seller_worklet);
    auto bidder_worklet = mock_auction_process_manager_->TakeBidderWorklet(
        kBidder1, kBidder1Name);
    ASSERT_TRUE(bidder_worklet);

    seller_worklet->CompleteLoading();
    bidder_worklet->InvokeGenerateBidCallback(
        /*bid=*/1, kRenderUrl, test_case.bid_ad_component_urls,
        base::TimeDelta());

    if (test_case.expect_successful_bid) {
      // Since the bid was valid, it should be scored.
      auto score_ad_params = seller_worklet->WaitForScoreAd();
      EXPECT_EQ(kBidder1, score_ad_params->interest_group_owner);
      EXPECT_EQ(1, score_ad_params->bid);
      seller_worklet->InvokeScoreAdCallback(/*score=*/11);

      // Finish the auction.
      seller_worklet->WaitForReportResult();
      seller_worklet->InvokeReportResultCallback();
      mock_auction_process_manager_->WaitForWinningBidderReload();
      bidder_worklet = mock_auction_process_manager_->TakeBidderWorklet(
          kBidder1, kBidder1Name);
      bidder_worklet->WaitForReportWin();
      bidder_worklet->InvokeReportWinCallback();
      auction_run_loop_->Run();

      // The bidder should win the auction.
      EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
      EXPECT_FALSE(result_.ad_component_urls);
      EXPECT_FALSE(result_.seller_report_url);
      EXPECT_FALSE(result_.bidder_report_url);
      EXPECT_EQ(6, result_.bidder1_bid_count);
      ASSERT_EQ(4u, result_.bidder1_prev_wins.size());
      EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
                result_.bidder1_prev_wins[3]->ad_json);
      EXPECT_THAT(result_.errors, testing::ElementsAre());
      CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                      1 /* expected_interest_groups */,
                      1 /* expected_owners */);
    } else {
      // Since there's no acceptable bid, the seller worklet is never asked to
      // score a bid.
      auction_run_loop_->Run();

      EXPECT_EQ("Unexpected non-null ad component list", TakeBadMessage());

      // No bidder won.
      EXPECT_FALSE(result_.ad_url);
      EXPECT_FALSE(result_.ad_component_urls);
      EXPECT_FALSE(result_.seller_report_url);
      EXPECT_FALSE(result_.bidder_report_url);
      EXPECT_EQ(5, result_.bidder1_bid_count);
      EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
      EXPECT_THAT(result_.errors, testing::ElementsAre());
      CheckHistograms(AuctionRunner::AuctionResult::kNoBids,
                      /*expected_interest_groups=*/1, /*expected_owners=*/1);
    }
  }
}

// Test that the limit of kMaxAdComponents ad components per bid is enforced.
TEST_F(AuctionRunnerTest, AdComponentsLimit) {
  const GURL kRenderUrl = GURL("https://ad1.com");

  for (size_t num_components = 1;
       num_components < blink::kMaxAdAuctionAdComponents + 2;
       num_components++) {
    std::vector<GURL> ad_component_urls;
    for (size_t i = 0; i < num_components; ++i) {
      ad_component_urls.emplace_back(
          GURL(base::StringPrintf("https://%zu.com", i)));
    }
    UseMockWorkletService();
    std::vector<StorageInterestGroup> bidders;
    bidders.emplace_back(MakeInterestGroup(
        kBidder1, kBidder1Name, kBidder1Url, kBidder1TrustedSignalsUrl,
        {"k1", "k2"}, kRenderUrl, ad_component_urls));

    StartAuction(kSellerUrl, std::move(bidders),
                 /*auction_signals_json=*/"{}",
                 auction_worklet::mojom::BrowserSignals::New(
                     url::Origin::Create(GURL("https://publisher1.com")),
                     url::Origin::Create(kSellerUrl)));

    mock_auction_process_manager_->WaitForWorklets(/*num_bidders=*/1);

    auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
    ASSERT_TRUE(seller_worklet);
    auto bidder_worklet = mock_auction_process_manager_->TakeBidderWorklet(
        kBidder1, kBidder1Name);
    ASSERT_TRUE(bidder_worklet);

    seller_worklet->CompleteLoading();
    bidder_worklet->InvokeGenerateBidCallback(
        /*bid=*/1, kRenderUrl, ad_component_urls, base::TimeDelta());

    if (num_components <= blink::kMaxAdAuctionAdComponents) {
      // Since the bid was valid, it should be scored.
      auto score_ad_params = seller_worklet->WaitForScoreAd();
      EXPECT_EQ(kBidder1, score_ad_params->interest_group_owner);
      EXPECT_EQ(1, score_ad_params->bid);
      seller_worklet->InvokeScoreAdCallback(/*score=*/11);

      // Finish the auction.
      seller_worklet->WaitForReportResult();
      seller_worklet->InvokeReportResultCallback();
      mock_auction_process_manager_->WaitForWinningBidderReload();
      bidder_worklet = mock_auction_process_manager_->TakeBidderWorklet(
          kBidder1, kBidder1Name);
      bidder_worklet->WaitForReportWin();
      bidder_worklet->InvokeReportWinCallback();
      auction_run_loop_->Run();

      // The bidder should win the auction.
      EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
      EXPECT_EQ(ad_component_urls, result_.ad_component_urls);
      EXPECT_FALSE(result_.seller_report_url);
      EXPECT_FALSE(result_.bidder_report_url);
      EXPECT_EQ(6, result_.bidder1_bid_count);
      ASSERT_EQ(4u, result_.bidder1_prev_wins.size());
      EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
                result_.bidder1_prev_wins[3]->ad_json);
      EXPECT_THAT(result_.errors, testing::ElementsAre());
      CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                      1 /* expected_interest_groups */,
                      1 /* expected_owners */);
    } else {
      // Since there's no acceptable bid, the seller worklet is never asked to
      // score a bid.
      auction_run_loop_->Run();

      EXPECT_EQ("Too many ad component URLs", TakeBadMessage());

      // No bidder won.
      EXPECT_FALSE(result_.ad_url);
      EXPECT_FALSE(result_.ad_component_urls);
      EXPECT_FALSE(result_.seller_report_url);
      EXPECT_FALSE(result_.bidder_report_url);
      EXPECT_EQ(5, result_.bidder1_bid_count);
      EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
      EXPECT_THAT(result_.errors, testing::ElementsAre());
      CheckHistograms(AuctionRunner::AuctionResult::kNoBids,
                      /*expected_interest_groups=*/1, /*expected_owners=*/1);
    }
  }
}

// Test cases where a bad bid is received over Mojo. Bad bids should be rejected
// in the Mojo process, so these are treated as security errors.
TEST_F(AuctionRunnerTest, BadBid) {
  const struct TestCase {
    const char* expected_error_message;
    double bid;
    GURL render_url;
    absl::optional<std::vector<GURL>> ad_component_urls;
    base::TimeDelta duration;
  } kTestCases[] = {
      // Bids that aren't positive integers.
      {
          "Invalid bid value",
          -10,
          GURL("https://ad1.com"),
          absl::nullopt,
          base::TimeDelta(),
      },
      {
          "Invalid bid value",
          0,
          GURL("https://ad1.com"),
          absl::nullopt,
          base::TimeDelta(),
      },
      {
          "Invalid bid value",
          std::numeric_limits<double>::infinity(),
          GURL("https://ad1.com"),
          absl::nullopt,
          base::TimeDelta(),
      },
      {
          "Invalid bid value",
          std::numeric_limits<double>::quiet_NaN(),
          GURL("https://ad1.com"),
          absl::nullopt,
          base::TimeDelta(),
      },

      // Invalid render URL.
      {
          "Bid render URL must be a valid ad URL",
          1,
          GURL(":"),
          absl::nullopt,
          base::TimeDelta(),
      },

      // Non-HTTPS render URLs.
      {
          "Bid render URL must be a valid ad URL",
          1,
          GURL("data:,foo"),
          absl::nullopt,
          base::TimeDelta(),
      },
      {
          "Bid render URL must be a valid ad URL",
          1,
          GURL("http://ad1.com"),
          absl::nullopt,
          base::TimeDelta(),
      },

      // HTTPS render URL that's not in the list of allowed renderUrls.
      {
          "Bid render URL must be a valid ad URL",
          1,
          GURL("https://ad2.com"),
          absl::nullopt,
          base::TimeDelta(),
      },

      // Invalid component URL.
      {
          "Bid ad components URL must match a valid ad component URL",
          1,
          GURL("https://ad1.com"),
          std::vector<GURL>{GURL(":")},
          base::TimeDelta(),
      },

      // HTTPS component URL that's not in the list of allowed ad component
      // URLs.
      {
          "Bid ad components URL must match a valid ad component URL",
          1,
          GURL("https://ad1.com"),
          std::vector<GURL>{GURL("https://ad2.com-component1.com")},
          base::TimeDelta(),
      },
      {
          "Bid ad components URL must match a valid ad component URL",
          1,
          GURL("https://ad1.com"),
          std::vector<GURL>{GURL("https://ad1.com-component1.com"),
                            GURL("https://ad2.com-component1.com")},
          base::TimeDelta(),
      },

      // Negative time.
      {
          "Invalid bid duration",
          1,
          GURL("https://ad2.com"),
          absl::nullopt,
          base::Milliseconds(-1),
      },
  };

  for (const auto& test_case : kTestCases) {
    StartStandardAuctionWithMockService();

    auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
    ASSERT_TRUE(seller_worklet);
    auto bidder1_worklet = mock_auction_process_manager_->TakeBidderWorklet(
        kBidder1, kBidder1Name);
    ASSERT_TRUE(bidder1_worklet);
    auto bidder2_worklet = mock_auction_process_manager_->TakeBidderWorklet(
        kBidder2, kBidder2Name);
    ASSERT_TRUE(bidder2_worklet);

    seller_worklet->CompleteLoading();
    bidder1_worklet->InvokeGenerateBidCallback(
        test_case.bid, test_case.render_url, test_case.ad_component_urls,
        test_case.duration);
    // Bidder 2 doesn't bid.
    bidder2_worklet->InvokeGenerateBidCallback(/*bid=*/absl::nullopt);

    // Since there's no acceptable bid, the seller worklet is never asked to
    // score a bid.
    auction_run_loop_->Run();

    EXPECT_EQ(test_case.expected_error_message, TakeBadMessage());

    // No bidder won.
    EXPECT_FALSE(result_.ad_url);
    EXPECT_FALSE(result_.ad_component_urls);
    EXPECT_FALSE(result_.seller_report_url);
    EXPECT_FALSE(result_.bidder_report_url);
    EXPECT_EQ(5, result_.bidder1_bid_count);
    EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
    EXPECT_EQ(5, result_.bidder2_bid_count);
    EXPECT_EQ(3u, result_.bidder2_prev_wins.size());
    EXPECT_THAT(result_.errors, testing::ElementsAre());
    CheckHistograms(AuctionRunner::AuctionResult::kNoBids,
                    /*expected_interest_groups=*/2, /*expected_owners=*/2);
  }
}

// Test cases where bad a report URL is received over Mojo from the bidder
// worklet. Bad report URLs should be rejected in the Mojo process, so these are
// treated as security errors.
TEST_F(AuctionRunnerTest, BadSellerReportUrl) {
  StartStandardAuctionWithMockService();

  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1, kBidder1Name);
  ASSERT_TRUE(bidder1_worklet);
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2, kBidder2Name);
  ASSERT_TRUE(bidder2_worklet);

  seller_worklet->CompleteLoading();
  // Only Bidder1 bids, to keep things simple.
  bidder1_worklet->InvokeGenerateBidCallback(5 /* bid */,
                                             GURL("https://ad1.com/"));
  bidder2_worklet->InvokeGenerateBidCallback(/*bid=*/absl::nullopt);

  auto score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder1, score_ad_params->interest_group_owner);
  EXPECT_EQ(5, score_ad_params->bid);
  seller_worklet->InvokeScoreAdCallback(10 /* score */);

  // Bidder1 never gets to report anything, since the seller providing a bad
  // report URL aborts the auction.
  seller_worklet->WaitForReportResult();
  seller_worklet->InvokeReportResultCallback(GURL("http://not.https.test/"));
  auction_run_loop_->Run();

  EXPECT_EQ("Invalid seller report URL", TakeBadMessage());

  // No bidder won.
  EXPECT_FALSE(result_.ad_url);
  EXPECT_FALSE(result_.ad_component_urls);
  EXPECT_FALSE(result_.seller_report_url);
  EXPECT_FALSE(result_.bidder_report_url);
  EXPECT_EQ(6, result_.bidder1_bid_count);
  EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(5, result_.bidder2_bid_count);
  EXPECT_EQ(3u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kBadMojoMessage,
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

// Test cases where bad a report URL is received over Mojo from the seller
// worklet. Bad report URLs should be rejected in the Mojo process, so these are
// treated as security errors.
TEST_F(AuctionRunnerTest, BadBidderReportUrl) {
  StartStandardAuctionWithMockService();

  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1, kBidder1Name);
  ASSERT_TRUE(bidder1_worklet);
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2, kBidder2Name);
  ASSERT_TRUE(bidder2_worklet);

  seller_worklet->CompleteLoading();
  // Only Bidder1 bids, to keep things simple.
  bidder1_worklet->InvokeGenerateBidCallback(5 /* bid */,
                                             GURL("https://ad1.com/"));
  bidder2_worklet->InvokeGenerateBidCallback(/*bid=*/absl::nullopt);

  auto score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder1, score_ad_params->interest_group_owner);
  EXPECT_EQ(5, score_ad_params->bid);
  seller_worklet->InvokeScoreAdCallback(10 /* score */);

  seller_worklet->WaitForReportResult();
  seller_worklet->InvokeReportResultCallback(
      GURL("https://valid.url.that.is.thrown.out.test/"));
  mock_auction_process_manager_->WaitForWinningBidderReload();
  bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1, kBidder1Name);
  bidder1_worklet->WaitForReportWin();
  bidder1_worklet->InvokeReportWinCallback(GURL("http://not.https.test/"));
  auction_run_loop_->Run();

  EXPECT_EQ("Invalid bidder report URL", TakeBadMessage());

  // No bidder won.
  EXPECT_FALSE(result_.ad_url);
  EXPECT_FALSE(result_.ad_component_urls);
  EXPECT_FALSE(result_.seller_report_url);
  EXPECT_FALSE(result_.bidder_report_url);
  EXPECT_EQ(6, result_.bidder1_bid_count);
  EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(5, result_.bidder2_bid_count);
  EXPECT_EQ(3u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kBadMojoMessage,
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

// Make sure that requesting unexpected URLs is blocked.
TEST_F(AuctionRunnerTest, UrlRequestProtection) {
  StartStandardAuctionWithMockService();

  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1, kBidder1Name);
  ASSERT_TRUE(bidder1_worklet);
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2, kBidder2Name);
  ASSERT_TRUE(bidder2_worklet);

  // It should be possible to request the seller URL from the seller's
  // URLLoaderFactory.
  network::ResourceRequest request;
  request.url = kSellerUrl;
  request.headers.SetHeader(net::HttpRequestHeaders::kAccept,
                            "application/javascript");
  mojo::PendingRemote<network::mojom::URLLoader> receiver;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client;
  seller_worklet->url_loader_factory()->CreateLoaderAndStart(
      receiver.InitWithNewPipeAndPassReceiver(), 0 /* request_id_ */,
      0 /* options */, request, client.InitWithNewPipeAndPassRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  seller_worklet->url_loader_factory().FlushForTesting();
  EXPECT_TRUE(seller_worklet->url_loader_factory().is_connected());
  ASSERT_EQ(1u, url_loader_factory_.pending_requests()->size());
  EXPECT_EQ(kSellerUrl,
            (*url_loader_factory_.pending_requests())[0].request.url);
  receiver.reset();
  client.reset();

  // A bidder's URLLoaderFactory should reject the seller URL, closing the Mojo
  // pipe.
  bidder1_worklet->url_loader_factory()->CreateLoaderAndStart(
      receiver.InitWithNewPipeAndPassReceiver(), 0 /* request_id_ */,
      0 /* options */, request, client.InitWithNewPipeAndPassRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  bidder1_worklet->url_loader_factory().FlushForTesting();
  EXPECT_FALSE(bidder1_worklet->url_loader_factory().is_connected());
  EXPECT_EQ(1u, url_loader_factory_.pending_requests()->size());
  EXPECT_EQ("Unexpected request", TakeBadMessage());
  receiver.reset();
  client.reset();

  // A bidder's URLLoaderFactory should allow the bidder's URL to be requested.
  request.url = kBidder2Url;
  bidder2_worklet->url_loader_factory()->CreateLoaderAndStart(
      receiver.InitWithNewPipeAndPassReceiver(), 0 /* request_id_ */,
      0 /* options */, request, client.InitWithNewPipeAndPassRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  bidder2_worklet->url_loader_factory().FlushForTesting();
  EXPECT_TRUE(bidder2_worklet->url_loader_factory().is_connected());
  ASSERT_EQ(2u, url_loader_factory_.pending_requests()->size());
  EXPECT_EQ(kBidder2Url,
            (*url_loader_factory_.pending_requests())[1].request.url);
  receiver.reset();
  client.reset();

  // The seller's URLLoaderFactory should reject a bidder worklet's URL, closing
  // the Mojo pipe.
  seller_worklet->url_loader_factory()->CreateLoaderAndStart(
      receiver.InitWithNewPipeAndPassReceiver(), 0 /* request_id_ */,
      0 /* options */, request, client.InitWithNewPipeAndPassRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  seller_worklet->url_loader_factory().FlushForTesting();
  EXPECT_FALSE(seller_worklet->url_loader_factory().is_connected());
  EXPECT_EQ(2u, url_loader_factory_.pending_requests()->size());
  EXPECT_EQ("Unexpected request", TakeBadMessage());
  receiver.reset();
  client.reset();

  // A bidder's URLLoaderFactory should also reject the URL from another bidder.
  request.url = kBidder1Url;
  bidder2_worklet->url_loader_factory()->CreateLoaderAndStart(
      receiver.InitWithNewPipeAndPassReceiver(), 0 /* request_id_ */,
      0 /* options */, request, client.InitWithNewPipeAndPassRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  bidder2_worklet->url_loader_factory().FlushForTesting();
  EXPECT_FALSE(bidder2_worklet->url_loader_factory().is_connected());
  ASSERT_EQ(2u, url_loader_factory_.pending_requests()->size());
  EXPECT_EQ("Unexpected request", TakeBadMessage());

  // Need to close the AuctionWorkletService pipes so callbacks can be destroyed
  // without DCHECKing.
  mock_auction_process_manager_->ClosePipes();
}

// Check that BidderWorklets that don't make a bid are destroyed immediately.
TEST_F(AuctionRunnerTest, DestroyBidderWorkletWithoutBid) {
  StartStandardAuctionWithMockService();

  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1, kBidder1Name);
  ASSERT_TRUE(bidder1_worklet);
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2, kBidder2Name);
  ASSERT_TRUE(bidder2_worklet);

  seller_worklet->CompleteLoading();

  bidder1_worklet->InvokeGenerateBidCallback(/*bid=*/absl::nullopt);
  // Need to flush the service pipe to make sure the AuctionRunner has received
  // the bid.
  mock_auction_process_manager_->Flush();
  // The AuctionRunner should have closed the pipe.
  EXPECT_TRUE(bidder1_worklet->PipeIsClosed());

  // Bidder2 returns a bid, which is then scored.
  bidder2_worklet->InvokeGenerateBidCallback(7 /* bid */,
                                             GURL("https://ad2.com/"));
  auto score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder2, score_ad_params->interest_group_owner);
  EXPECT_EQ(7, score_ad_params->bid);
  seller_worklet->InvokeScoreAdCallback(11 /* score */);

  // Finish the auction.
  seller_worklet->WaitForReportResult();
  seller_worklet->InvokeReportResultCallback();
  mock_auction_process_manager_->WaitForWinningBidderReload();
  bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2, kBidder2Name);
  bidder2_worklet->WaitForReportWin();
  bidder2_worklet->InvokeReportWinCallback();
  auction_run_loop_->Run();

  // Bidder2 won.
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
  EXPECT_FALSE(result_.ad_component_urls);
  EXPECT_FALSE(result_.seller_report_url);
  EXPECT_FALSE(result_.bidder_report_url);
  EXPECT_EQ(5, result_.bidder1_bid_count);
  EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(6, result_.bidder2_bid_count);
  ASSERT_EQ(4u, result_.bidder2_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            result_.bidder2_prev_wins[3]->ad_json);
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

// Check that the winner of ties is randomized. Mock out bidders so can make
// sure that which bidder wins isn't changed just due to script execution order
// changing.
TEST_F(AuctionRunnerTest, Tie) {
  bool seen_bidder1_win = false;
  bool seen_bidder2_win = false;

  while (!seen_bidder1_win || !seen_bidder2_win) {
    StartStandardAuctionWithMockService();

    auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
    ASSERT_TRUE(seller_worklet);
    auto bidder1_worklet = mock_auction_process_manager_->TakeBidderWorklet(
        kBidder1, kBidder1Name);
    ASSERT_TRUE(bidder1_worklet);
    auto bidder2_worklet = mock_auction_process_manager_->TakeBidderWorklet(
        kBidder2, kBidder2Name);
    ASSERT_TRUE(bidder2_worklet);

    seller_worklet->CompleteLoading();

    // Bidder1 returns a bid, which is then scored.
    bidder1_worklet->InvokeGenerateBidCallback(5 /* bid */,
                                               GURL("https://ad1.com/"));
    auto score_ad_params = seller_worklet->WaitForScoreAd();
    EXPECT_EQ(kBidder1, score_ad_params->interest_group_owner);
    EXPECT_EQ(5, score_ad_params->bid);
    seller_worklet->InvokeScoreAdCallback(10 /* score */);

    // Bidder2 returns a bid, which is then scored.
    bidder2_worklet->InvokeGenerateBidCallback(5 /* bid */,
                                               GURL("https://ad2.com/"));
    score_ad_params = seller_worklet->WaitForScoreAd();
    EXPECT_EQ(kBidder2, score_ad_params->interest_group_owner);
    EXPECT_EQ(5, score_ad_params->bid);
    seller_worklet->InvokeScoreAdCallback(10 /* score */);
    // Need to flush the service pipe to make sure the AuctionRunner has
    // received the score.
    seller_worklet->Flush();

    seller_worklet->WaitForReportResult();
    seller_worklet->InvokeReportResultCallback();

    // Wait for a worklet to be reloaded, and try to get worklets for both
    // InterestGroups - only the InterestGroup that was picked as the winner
    // will be non-null.
    mock_auction_process_manager_->WaitForWinningBidderReload();
    bidder1_worklet = mock_auction_process_manager_->TakeBidderWorklet(
        kBidder1, kBidder1Name);
    bidder2_worklet = mock_auction_process_manager_->TakeBidderWorklet(
        kBidder2, kBidder2Name);

    if (bidder1_worklet) {
      seen_bidder1_win = true;
      bidder1_worklet->WaitForReportWin();
      bidder1_worklet->InvokeReportWinCallback();
      auction_run_loop_->Run();

      EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
      EXPECT_FALSE(result_.ad_component_urls);
      ASSERT_EQ(4u, result_.bidder1_prev_wins.size());
      EXPECT_EQ(3u, result_.bidder2_prev_wins.size());
      EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
                result_.bidder1_prev_wins[3]->ad_json);
    } else {
      seen_bidder2_win = true;

      ASSERT_TRUE(bidder2_worklet);
      bidder2_worklet->WaitForReportWin();
      bidder2_worklet->InvokeReportWinCallback();
      auction_run_loop_->Run();

      EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
      EXPECT_FALSE(result_.ad_component_urls);
      EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
      ASSERT_EQ(4u, result_.bidder2_prev_wins.size());
      EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
                result_.bidder2_prev_wins[3]->ad_json);
    }

    EXPECT_FALSE(result_.seller_report_url);
    EXPECT_FALSE(result_.bidder_report_url);
    EXPECT_EQ(6, result_.bidder1_bid_count);
    EXPECT_EQ(6, result_.bidder2_bid_count);
    EXPECT_THAT(result_.errors, testing::ElementsAre());
    CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                    2 /* expected_interest_groups */, 2 /* expected_owners */);
  }
}

}  // namespace
}  // namespace content
