// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_runner.h"

#include <vector>

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/services/auction_worklet/auction_worklet_service_impl.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "content/services/auction_worklet/worklet_test_util.h"
#include "net/http/http_status_code.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

std::string MakeBidScript(const std::string& bid,
                          const std::string& render_url,
                          const url::Origin& interest_group_owner,
                          const std::string& interest_group_name,
                          bool has_signals,
                          const std::string& signal_key,
                          const std::string& signal_val) {
  // TODO(morlovich): Use JsReplace.
  constexpr char kBidScript[] = R"(
    const bid = %s;
    const renderUrl = "%s";
    const interestGroupOwner = "%s";
    const interestGroupName = "%s";
    const hasSignals = %s;

    function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                          trustedBiddingSignals, browserSignals) {
      var result = {ad: {bidKey: "data for " + bid,
                         groupName: interestGroupName},
                    bid: bid, render: renderUrl};
      if (interestGroup.name !== interestGroupName)
        throw new Error("wrong interestGroupName");
      if (interestGroup.owner !== interestGroupOwner)
        throw new Error("wrong interestGroupOwner");
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
      kBidScript, bid.c_str(), render_url.c_str(),
      interest_group_owner.Serialize().c_str(), interest_group_name.c_str(),
      has_signals ? "true" : "false", signal_key.c_str(), signal_val.c_str());
}

// This can be appended to the standard script to override the function.
constexpr char kReportWinNoUrl[] = R"(
  function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
                      browserSignals) {
  }
)";

constexpr char kCheckingAuctionScript[] = R"(
  function scoreAd(adMetadata, bid, auctionConfig, trustedScoringSignals,
      browserSignals) {
    if (adMetadata.bidKey !== ("data for " + bid)) {
      throw new Error("wrong data for bid:" +
                      JSON.stringify(adMetadata) + "/" + bid);
    }
    if (auctionConfig.decisionLogicUrl
        !== "https://adstuff.publisher1.com/auction.js") {
      throw new Error("wrong auctionConfig");
    }
    if (auctionConfig.perBuyerSignals['adplatform.com'].signalsFor
        !== 'Ad Platform') {
      throw new Error("Wrong perBuyerSignals in auctionConfig");
    }
    if (!auctionConfig.sellerSignals.isSellerSignals)
      throw new Error("Wrong sellerSignals");
    if (browserSignals.topWindowHostname !== 'publisher1.com')
      throw new Error("wrong topWindowHostname");
    if ("joinCount" in browserSignals)
      throw new Error("wrong kind of browser signals");
    if (browserSignals.adRenderFingerprint !== "#####")
      throw new Error("wrong adRenderFingerprint");
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

// BidderWorklet that holds onto passed in callbacks, to let the test fixture
// invoke them.
class MockBidderWorklet : public auction_worklet::mojom::BidderWorklet {
 public:
  explicit MockBidderWorklet(
      mojo::PendingReceiver<auction_worklet::mojom::BidderWorklet>
          pending_receiver,
      auction_worklet::mojom::AuctionWorkletService::
          LoadBidderWorkletAndGenerateBidCallback
              load_bidder_worklet_and_generate_bid_callback)
      : load_bidder_worklet_and_generate_bid_callback_(
            std::move(load_bidder_worklet_and_generate_bid_callback)),
        receiver_(this, std::move(pending_receiver)) {}

  MockBidderWorklet(const MockBidderWorklet&) = delete;
  const MockBidderWorklet& operator=(const MockBidderWorklet&) = delete;

  ~MockBidderWorklet() override = default;

  // auction_worklet::mojom::BidderWorklet implementation:
  void ReportWin(const std::string& seller_signals_json,
                 const GURL& browser_signal_render_url,
                 const std::string& browser_signal_ad_render_fingerprint,
                 double browser_signal_bid,
                 ReportWinCallback report_win_callback) override {
    report_win_callback_ = std::move(report_win_callback);
    if (report_win_run_loop_)
      report_win_run_loop_->Quit();
  }

  // Handles successful load case only.
  void CompleteLoadingAndBid(double bid, const GURL& render_url) {
    DCHECK(load_bidder_worklet_and_generate_bid_callback_);
    std::move(load_bidder_worklet_and_generate_bid_callback_)
        .Run(auction_worklet::mojom::BidderWorkletBid::New(
                 "ad", bid, render_url, base::TimeDelta()),
             std::vector<std::string>() /* errors */);
  }

  // Returns the LoadBidderWorkletAndGenerateBidCallback for a worklet. Needed
  // for cases when the BidderWorklet is destroyed (to close its pipe) before
  // the AuctionWorkletService is destroyed, since Mojo DCHECKs if a callback is
  // destroyed when the pipe its over is still live.
  //
  // TODO(mmenke): To better simulate real crashes, give worklets their own
  // AuctionWorkletService pipes, and remove this method.
  auction_worklet::mojom::AuctionWorkletService::
      LoadBidderWorkletAndGenerateBidCallback
      TakeLoadCallback() {
    return std::move(load_bidder_worklet_and_generate_bid_callback_);
  }

  void WaitForReportWin() {
    DCHECK(!load_bidder_worklet_and_generate_bid_callback_);
    DCHECK(!report_win_run_loop_);
    if (!report_win_callback_) {
      report_win_run_loop_ = std::make_unique<base::RunLoop>();
      report_win_run_loop_->Run();
      report_win_run_loop_.reset();
      DCHECK(report_win_callback_);
    }
  }

  void InvokeReportWinCallback() {
    DCHECK(report_win_callback_);
    std::move(report_win_callback_)
        .Run(absl::nullopt /* report_url */,
             std::vector<std::string>() /* errors */);
  }

 private:
  auction_worklet::mojom::AuctionWorkletService::
      LoadBidderWorkletAndGenerateBidCallback
          load_bidder_worklet_and_generate_bid_callback_;

  std::unique_ptr<base::RunLoop> report_win_run_loop_;
  ReportWinCallback report_win_callback_;

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
      auction_worklet::mojom::AuctionWorkletService::LoadSellerWorkletCallback
          load_worklet_callback)
      : load_worklet_callback_(std::move(load_worklet_callback)),
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
               const std::string& browser_signal_ad_render_fingerprint,
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
                    const std::string& browser_signal_ad_render_fingerprint,
                    double browser_signal_bid,
                    double browser_signal_desirability,
                    ReportResultCallback report_result_callback) override {
    report_result_callback_ = std::move(report_result_callback);
    if (report_result_run_loop_)
      report_result_run_loop_->Quit();
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
  void InvokeReportResultCallback() {
    DCHECK(report_result_callback_);
    std::move(report_result_callback_)
        .Run(absl::nullopt /* signals_for_winner */,
             absl::nullopt /* report_url */,
             std::vector<std::string>() /* errors */);
  }

 private:
  auction_worklet::mojom::AuctionWorkletService::LoadSellerWorkletCallback
      load_worklet_callback_;

  std::unique_ptr<base::RunLoop> score_ad_run_loop_;
  std::unique_ptr<ScoreAdParams> score_ad_params_;
  ScoreAdCallback score_ad_callback_;

  std::unique_ptr<base::RunLoop> report_result_run_loop_;
  ReportResultCallback report_result_callback_;

  // Receiver is last so that destroying `this` while there's a pending callback
  // over the pipe will not DCHECK.
  mojo::Receiver<auction_worklet::mojom::SellerWorklet> receiver_;
};

// AuctionWorkletService that creates MockBidderWorklets and MockSellerWorklets
// to hold onto passed in PendingReceivers and Callbacks.
class MockAuctionWorkletService
    : public auction_worklet::mojom::AuctionWorkletService {
 public:
  explicit MockAuctionWorkletService(
      mojo::PendingReceiver<auction_worklet::mojom::AuctionWorkletService>
          pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {}

  ~MockAuctionWorkletService() override = default;

  // auction_worklet::mojom::AuctionWorkletService implementation:

  void LoadBidderWorkletAndGenerateBid(
      mojo::PendingReceiver<auction_worklet::mojom::BidderWorklet>
          bidder_worklet_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_url_loader_factory,
      auction_worklet::mojom::BiddingInterestGroupPtr bidding_interest_group,
      const absl::optional<std::string>& auction_signals_json,
      const absl::optional<std::string>& per_buyer_signals_json,
      const url::Origin& top_window_origin,
      const url::Origin& seller_origin,
      base::Time auction_start_time,
      LoadBidderWorkletAndGenerateBidCallback
          load_bidder_worklet_and_generate_bid_callback) override {
    InterestGroupId interest_group_id(bidding_interest_group->group->owner,
                                      bidding_interest_group->group->name);
    EXPECT_EQ(0u, bidder_worklets_.count(interest_group_id));
    bidder_worklets_.emplace(std::make_pair(
        interest_group_id,
        std::make_unique<MockBidderWorklet>(
            std::move(bidder_worklet_receiver),
            std::move(load_bidder_worklet_and_generate_bid_callback))));

    ASSERT_GT(waiting_for_num_bidders_, 0);
    --waiting_for_num_bidders_;
    MaybeQuitRunLoop();
  }

  void LoadSellerWorklet(
      mojo::PendingReceiver<auction_worklet::mojom::SellerWorklet>
          seller_worklet_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_url_loader_factory,
      const GURL& script_source_url,
      LoadSellerWorkletCallback load_seller_worklet_callback) override {
    DCHECK(!seller_worklet_);

    seller_worklet_ = std::make_unique<MockSellerWorklet>(
        std::move(seller_worklet_receiver),
        std::move(load_seller_worklet_callback));

    ASSERT_TRUE(waiting_on_seller_);
    waiting_on_seller_ = false;
    MaybeQuitRunLoop();
  }

  // Waits for a SellerWorklet and `num_bidders` bidder worklets to be created.
  void WaitForWorklets(int num_bidders) {
    waiting_on_seller_ = true;
    waiting_for_num_bidders_ = num_bidders;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();
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

  void Flush() { receiver_.FlushForTesting(); }

 private:
  void MaybeQuitRunLoop() {
    if (!waiting_on_seller_ && waiting_for_num_bidders_ == 0)
      run_loop_->Quit();
  }

  // An interest group is uniquely identified by its owner's origin and name.
  using InterestGroupId = std::pair<url::Origin, std::string>;

  std::map<InterestGroupId, std::unique_ptr<MockBidderWorklet>>
      bidder_worklets_;

  std::unique_ptr<MockSellerWorklet> seller_worklet_;

  std::unique_ptr<base::RunLoop> run_loop_;
  bool waiting_on_seller_ = false;
  int waiting_for_num_bidders_ = 0;

  // Receiver is last so that destroying `this` while there's a pending callback
  // over the pipe will not DCHECK.
  mojo::Receiver<auction_worklet::mojom::AuctionWorkletService> receiver_;
};

class AuctionRunnerTest : public testing::Test {
 protected:
  struct Result {
    GURL ad_url;
    url::Origin interest_group_owner;
    std::string interest_group_name;
    GURL bidder_report_url;
    GURL seller_report_url;
    std::vector<std::string> errors;
  };

  AuctionRunnerTest()
      : auction_worklet_service_(
            auction_worklet_service_remote_.BindNewPipeAndPassReceiver()) {}

  // Starts an auction without waiting for it to complete. Useful when using
  // MockAuctionWorkletService.
  void StartAuction(
      const GURL& seller_decision_logic_url,
      std::vector<auction_worklet::mojom::BiddingInterestGroupPtr> bidders,
      const std::string& auction_signals_json,
      auction_worklet::mojom::BrowserSignalsPtr browser_signals) {
    auction_complete_ = false;
    mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
    url_loader_factory_.Clone(factory_remote.InitWithNewPipeAndPassReceiver());

    blink::mojom::AuctionAdConfigPtr auction_config =
        blink::mojom::AuctionAdConfig::New();
    auction_config->seller = url::Origin::Create(seller_decision_logic_url);
    auction_config->decision_logic_url = seller_decision_logic_url;
    auction_config->interest_group_buyers =
        blink::mojom::InterestGroupBuyers::NewAllBuyers(
            blink::mojom::AllBuyers::New());
    auction_config->auction_signals = auction_signals_json;
    auction_config->seller_signals = R"({"isSellerSignals": true})";

    base::flat_map<url::Origin, std::string> per_buyer_signals;
    per_buyer_signals[kBidder1] = R"({"signalsFor": ")" + kBidder1Name + "\"}";
    per_buyer_signals[kBidder2] = R"({"signalsFor": ")" + kBidder2Name + "\"}";
    auction_config->per_buyer_signals = std::move(per_buyer_signals);

    auction_run_loop_ = std::make_unique<base::RunLoop>();
    Result result;
    auction_runner_ = AuctionRunner::CreateAndStart(
        base::BindRepeating(&AuctionRunnerTest::GetWorkletService,
                            base::Unretained(this)),
        std::move(factory_remote), std::move(auction_config),
        std::move(bidders), std::move(browser_signals),
        base::BindOnce(&AuctionRunnerTest::OnAuctionComplete,
                       base::Unretained(this)));
  }

  Result RunAuctionAndWait(
      const GURL& seller_decision_logic_url,
      std::vector<auction_worklet::mojom::BiddingInterestGroupPtr> bidders,
      const std::string& auction_signals_json,
      auction_worklet::mojom::BrowserSignalsPtr browser_signals) {
    StartAuction(seller_decision_logic_url, std::move(bidders),
                 auction_signals_json, std::move(browser_signals));
    auction_run_loop_->Run();
    return result_;
  }

  void OnAuctionComplete(const GURL& ad_url,
                         const url::Origin& interest_group_owner,
                         const std::string& interest_group_name,
                         const GURL& bidder_report_url,
                         const GURL& seller_report_url,
                         const std::vector<std::string>& errors) {
    auction_complete_ = true;
    result_.ad_url = ad_url;
    result_.interest_group_owner = interest_group_owner;
    result_.interest_group_name = interest_group_name;
    result_.bidder_report_url = bidder_report_url;
    result_.seller_report_url = seller_report_url;
    result_.errors = errors;
    auction_run_loop_->Quit();
  }

  auction_worklet::mojom::BiddingInterestGroupPtr MakeInterestGroup(
      const url::Origin& owner,
      const std::string& name,
      const GURL& bidding_url,
      const absl::optional<GURL>& trusted_bidding_signals_url,
      const std::vector<std::string>& trusted_bidding_signals_keys,
      const GURL& ad_url) {
    std::vector<blink::mojom::InterestGroupAdPtr> ads;
    ads.push_back(
        blink::mojom::InterestGroupAd::New(ad_url, R"({"ads": true})"));

    std::vector<auction_worklet::mojom::PreviousWinPtr> previous_wins;
    previous_wins.push_back(auction_worklet::mojom::PreviousWin::New(
        base::Time::Now(), R"({"winner": 0})"));
    previous_wins.push_back(auction_worklet::mojom::PreviousWin::New(
        base::Time::Now(), R"({"winner": -1})"));
    previous_wins.push_back(auction_worklet::mojom::PreviousWin::New(
        base::Time::Now(), R"({"winner": -2})"));

    return auction_worklet::mojom::BiddingInterestGroup::New(
        blink::mojom::InterestGroup::New(
            base::Time::Max(), owner, name, bidding_url,
            GURL() /* update_url */, trusted_bidding_signals_url,
            trusted_bidding_signals_keys, absl::nullopt, std::move(ads)),
        auction_worklet::mojom::BiddingBrowserSignals::New(
            3, 5, std::move(previous_wins)));
  }

  void StartStandardAuction() {
    std::vector<auction_worklet::mojom::BiddingInterestGroupPtr> bidders;
    bidders.push_back(MakeInterestGroup(kBidder1, kBidder1Name, kBidder1Url,
                                        kTrustedSignalsUrl, {"k1", "k2"},
                                        GURL("https://ad1.com")));
    bidders.push_back(MakeInterestGroup(kBidder2, kBidder2Name, kBidder2Url,
                                        kTrustedSignalsUrl, {"l1", "l2"},
                                        GURL("https://ad2.com")));

    StartAuction(kSellerUrl, std::move(bidders),
                 R"({"isAuctionSignals": true})", /* auction_signals_json */
                 auction_worklet::mojom::BrowserSignals::New(
                     url::Origin::Create(GURL("https://publisher1.com")),
                     url::Origin::Create(kSellerUrl)));
  }

  Result RunStandardAuction() {
    StartStandardAuction();
    auction_run_loop_->Run();
    return result_;
  }

  // Starts the standard auction with the mock worklet service, and waits for
  // the service to receive the worklet construction calls.
  void StartStandardAuctionWithMockService() {
    use_mock_service_ = true;
    StartStandardAuction();
    mock_worklet_service_->WaitForWorklets(2 /* num_bidders */);
  }

  virtual auction_worklet::mojom::AuctionWorkletService* GetWorkletService() {
    if (use_mock_service_) {
      if (!mock_worklet_service_) {
        mock_worklet_service_remote_.reset();
        mock_worklet_service_ = std::make_unique<MockAuctionWorkletService>(
            mock_worklet_service_remote_.BindNewPipeAndPassReceiver());
      }
      return mock_worklet_service_remote_.get();
    }
    return auction_worklet_service_remote_.get();
  }

  const GURL kSellerUrl{"https://adstuff.publisher1.com/auction.js"};
  const GURL kBidder1Url{"https://adplatform.com/offers.js"};
  const url::Origin kBidder1 =
      url::Origin::Create(GURL("https://adplatform.com"));
  const std::string kBidder1Name{"Ad Platform"};
  const GURL kBidder2Url{"https://anotheradthing.com/bids.js"};
  const url::Origin kBidder2 =
      url::Origin::Create(GURL("https://anotheradthing.com"));
  const std::string kBidder2Name{"Another Ad Thing"};

  const GURL kTrustedSignalsUrl{"https://trustedsignaller.org/signals"};

  base::test::TaskEnvironment task_environment_;

  bool use_mock_service_ = false;

  // RunLoop that's quit on auction completion.
  std::unique_ptr<base::RunLoop> auction_run_loop_;
  // True if the most recently started auction has completed.
  bool auction_complete_ = false;
  // Result of the most recent auction.
  Result result_;

  network::TestURLLoaderFactory url_loader_factory_;
  mojo::Remote<auction_worklet::mojom::AuctionWorkletService>
      mock_worklet_service_remote_;
  std::unique_ptr<MockAuctionWorkletService> mock_worklet_service_;

  mojo::Remote<auction_worklet::mojom::AuctionWorkletService>
      auction_worklet_service_remote_;
  auction_worklet::AuctionWorkletServiceImpl auction_worklet_service_;

  std::unique_ptr<AuctionRunner> auction_runner_;
};

// An auction with two successful bids.
TEST_F(AuctionRunnerTest, Basic) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", kBidder1, kBidder1Name,
                    true /* has_signals */, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", kBidder2, kBidder2Name,
                    true /* has_signals */, "l2", "b"));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddJsonResponse(
      &url_loader_factory_,
      GURL(kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=k1,k2"),
      R"({"k1":"a", "k2": "b", "extra": "c"})");
  auction_worklet::AddJsonResponse(
      &url_loader_factory_,
      GURL(kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=l1,l2"),
      R"({"l1":"a", "l2": "b", "extra": "c"})");

  Result res = RunStandardAuction();
  EXPECT_EQ("https://ad2.com/", res.ad_url.spec());
  EXPECT_EQ("https://anotheradthing.com", res.interest_group_owner.Serialize());
  EXPECT_EQ("Another Ad Thing", res.interest_group_name);
  EXPECT_EQ("https://reporting.example.com/", res.seller_report_url.spec());
  EXPECT_EQ("https://buyer-reporting.example.com/",
            res.bidder_report_url.spec());
  EXPECT_THAT(res.errors, testing::ElementsAre());
}

// An auction where one bid is successful, another's script 404s.
TEST_F(AuctionRunnerTest, OneBidOne404) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", kBidder1, kBidder1Name,
                    true /* has_signals */, "k1", "a"));
  url_loader_factory_.AddResponse(kBidder2Url.spec(), "", net::HTTP_NOT_FOUND);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddJsonResponse(
      &url_loader_factory_,
      GURL(kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=k1,k2"),
      R"({"k1":"a", "k2": "b", "extra": "c"})");
  auction_worklet::AddJsonResponse(
      &url_loader_factory_,
      GURL(kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=l1,l2"),
      R"({"l1":"a", "l2": "b", "extra": "c"})");

  Result res = RunStandardAuction();
  EXPECT_EQ("https://ad1.com/", res.ad_url.spec());
  EXPECT_EQ("https://adplatform.com", res.interest_group_owner.Serialize());
  EXPECT_EQ("Ad Platform", res.interest_group_name);
  EXPECT_EQ("https://reporting.example.com/", res.seller_report_url.spec());
  EXPECT_EQ("https://buyer-reporting.example.com/",
            res.bidder_report_url.spec());
  EXPECT_THAT(
      res.errors,
      testing::ElementsAre("Failed to load https://anotheradthing.com/bids.js "
                           "HTTP status = 404 Not Found."));
}

// An auction where one bid is successful, another's script does not provide a
// bidding function.
TEST_F(AuctionRunnerTest, OneBidOneNotMade) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", kBidder1, kBidder1Name,
                    true /* has_signals */, "k1", "a"));

  // The auction script doesn't make any bids.
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder2Url,
                                         MakeAuctionScript());
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddJsonResponse(
      &url_loader_factory_,
      GURL(kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=k1,k2"),
      R"({"k1":"a", "k2": "b", "extra": "c"})");
  auction_worklet::AddJsonResponse(
      &url_loader_factory_,
      GURL(kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=l1,l2"),
      R"({"l1":"a", "l2": "b", "extra": "c"})");

  Result res = RunStandardAuction();
  EXPECT_EQ("https://ad1.com/", res.ad_url.spec());
  EXPECT_EQ("https://adplatform.com", res.interest_group_owner.Serialize());
  EXPECT_EQ("Ad Platform", res.interest_group_name);
  EXPECT_EQ("https://reporting.example.com/", res.seller_report_url.spec());
  EXPECT_EQ("https://buyer-reporting.example.com/",
            res.bidder_report_url.spec());
  EXPECT_THAT(res.errors,
              testing::ElementsAre("https://anotheradthing.com/bids.js "
                                   "`generateBid` is not a function."));
}

// An auction where no bidding scripts load successfully.
TEST_F(AuctionRunnerTest, NoBids) {
  url_loader_factory_.AddResponse(kBidder1Url.spec(), "", net::HTTP_NOT_FOUND);
  url_loader_factory_.AddResponse(kBidder2Url.spec(), "", net::HTTP_NOT_FOUND);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddJsonResponse(
      &url_loader_factory_,
      GURL(kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=k1,k2"),
      R"({"k1":"a", "k2":"b", "extra":"c"})");
  auction_worklet::AddJsonResponse(
      &url_loader_factory_,
      GURL(kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=l1,l2"),
      R"({"l1":"a", "l2": "b", "extra":"c"})");

  Result res = RunStandardAuction();
  EXPECT_TRUE(res.ad_url.is_empty());
  EXPECT_TRUE(res.interest_group_owner.opaque());
  EXPECT_EQ("", res.interest_group_name);
  EXPECT_TRUE(res.seller_report_url.is_empty());
  EXPECT_TRUE(res.bidder_report_url.is_empty());
  EXPECT_THAT(
      res.errors,
      testing::ElementsAre("Failed to load https://adplatform.com/offers.js "
                           "HTTP status = 404 Not Found.",
                           "Failed to load https://anotheradthing.com/bids.js "
                           "HTTP status = 404 Not Found."));
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
  auction_worklet::AddJsonResponse(
      &url_loader_factory_,
      GURL(kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=k1,k2"),
      R"({"k1":"a", "k2":"b", "extra":"c"})");
  auction_worklet::AddJsonResponse(
      &url_loader_factory_,
      GURL(kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=l1,l2"),
      R"({"l1":"a", "l2": "b", "extra":"c"})");

  Result res = RunStandardAuction();
  EXPECT_TRUE(res.ad_url.is_empty());
  EXPECT_TRUE(res.interest_group_owner.opaque());
  EXPECT_EQ("", res.interest_group_name);
  EXPECT_TRUE(res.seller_report_url.is_empty());
  EXPECT_TRUE(res.bidder_report_url.is_empty());
  EXPECT_THAT(
      res.errors,
      testing::ElementsAre(
          "https://adplatform.com/offers.js `generateBid` is not a function.",
          "https://anotheradthing.com/bids.js `generateBid` is not a "
          "function."));
}

// An auction where the seller script doesn't have a scoring function.
TEST_F(AuctionRunnerTest, SellerRejectsAll) {
  std::string bid_script1 =
      MakeBidScript("1", "https://ad1.com/", kBidder1, kBidder1Name,
                    true /* has_signals */, "k1", "a");
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         bid_script1);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", kBidder2, kBidder2Name,
                    true /* has_signals */, "l2", "b"));

  // No seller scoring function in a bid script.
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         bid_script1);
  auction_worklet::AddJsonResponse(
      &url_loader_factory_,
      GURL(kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=k1,k2"),
      R"({"k1":"a", "k2":"b", "extra":"c"})");
  auction_worklet::AddJsonResponse(
      &url_loader_factory_,
      GURL(kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=l1,l2"),
      R"({"l1":"a", "l2": "b", "extra":"c"})");

  Result res = RunStandardAuction();
  EXPECT_TRUE(res.ad_url.is_empty());
  EXPECT_TRUE(res.interest_group_owner.opaque());
  EXPECT_EQ("", res.interest_group_name);
  EXPECT_TRUE(res.seller_report_url.is_empty());
  EXPECT_TRUE(res.bidder_report_url.is_empty());
  EXPECT_THAT(res.errors,
              testing::ElementsAre("https://adstuff.publisher1.com/auction.js "
                                   "`scoreAd` is not a function.",
                                   "https://adstuff.publisher1.com/auction.js "
                                   "`scoreAd` is not a function."));
}

// An auction where seller rejects one bid when scoring.
TEST_F(AuctionRunnerTest, SellerRejectsOne) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", kBidder1, kBidder1Name,
                    true /* has_signals */, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", kBidder2, kBidder2Name,
                    true /* has_signals */, "l2", "b"));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScriptReject2());
  auction_worklet::AddJsonResponse(
      &url_loader_factory_,
      GURL(kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=k1,k2"),
      R"({"k1":"a", "k2": "b", "extra": "c"})");
  auction_worklet::AddJsonResponse(
      &url_loader_factory_,
      GURL(kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=l1,l2"),
      R"({"l1":"a", "l2": "b", "extra": "c"})");

  Result res = RunStandardAuction();
  EXPECT_EQ("https://ad1.com/", res.ad_url.spec());
  EXPECT_EQ("https://adplatform.com", res.interest_group_owner.Serialize());
  EXPECT_EQ("Ad Platform", res.interest_group_name);
  EXPECT_EQ("https://reporting.example.com/", res.seller_report_url.spec());
  EXPECT_EQ("https://buyer-reporting.example.com/",
            res.bidder_report_url.spec());
}

// An auction where the seller script fails to load.
TEST_F(AuctionRunnerTest, NoSellerScript) {
  // Tests to make sure that if seller script fails the other fetches are
  // cancelled, too.
  url_loader_factory_.AddResponse(kSellerUrl.spec(), "", net::HTTP_NOT_FOUND);
  Result res = RunStandardAuction();
  EXPECT_TRUE(res.ad_url.is_empty());
  EXPECT_TRUE(res.interest_group_owner.opaque());
  EXPECT_EQ("", res.interest_group_name);
  EXPECT_TRUE(res.seller_report_url.is_empty());
  EXPECT_TRUE(res.bidder_report_url.is_empty());

  EXPECT_EQ(0, url_loader_factory_.NumPending());
  EXPECT_THAT(res.errors,
              testing::ElementsAre(
                  "Failed to load https://adstuff.publisher1.com/auction.js "
                  "HTTP status = 404 Not Found."));
}

// An auction where bidders don't requested trusted bidding signals.
TEST_F(AuctionRunnerTest, NoTrustedBiddingSignals) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", kBidder1, kBidder1Name,
                    false /* has_signals */, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", kBidder2, kBidder2Name,
                    false /* has_signals */, "l2", "b"));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());

  std::vector<auction_worklet::mojom::BiddingInterestGroupPtr> bidders;
  bidders.push_back(MakeInterestGroup(kBidder1, kBidder1Name, kBidder1Url,
                                      absl::nullopt, {"k1", "k2"},
                                      GURL("https://ad1.com")));
  bidders.push_back(MakeInterestGroup(kBidder2, kBidder2Name, kBidder2Url,
                                      absl::nullopt, {"l1", "l2"},
                                      GURL("https://ad2.com")));

  Result res = RunAuctionAndWait(
      kSellerUrl, std::move(bidders),
      R"({"isAuctionSignals": true})", /* auction_signals_json */
      auction_worklet::mojom::BrowserSignals::New(
          url::Origin::Create(GURL("https://publisher1.com")),
          url::Origin::Create(kSellerUrl)));

  EXPECT_EQ("https://ad2.com/", res.ad_url.spec());
  EXPECT_EQ("https://anotheradthing.com", res.interest_group_owner.Serialize());
  EXPECT_EQ("Another Ad Thing", res.interest_group_name);
  EXPECT_EQ("https://reporting.example.com/", res.seller_report_url.spec());
  EXPECT_EQ("https://buyer-reporting.example.com/",
            res.bidder_report_url.spec());
  EXPECT_THAT(res.errors, testing::ElementsAre());
}

// An auction where trusted bidding signals are requested, but the fetch 404s.
TEST_F(AuctionRunnerTest, TrustedBiddingSignals404) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", kBidder1, kBidder1Name,
                    false /* has_signals */, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", kBidder2, kBidder2Name,
                    false /* has_signals */, "l2", "b"));
  url_loader_factory_.AddResponse(
      kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=k1,k2", "",
      net::HTTP_NOT_FOUND);
  url_loader_factory_.AddResponse(
      kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=l1,l2", "",
      net::HTTP_NOT_FOUND);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());

  Result res = RunStandardAuction();
  EXPECT_EQ("https://ad2.com/", res.ad_url.spec());
  EXPECT_EQ("https://anotheradthing.com", res.interest_group_owner.Serialize());
  EXPECT_EQ("Another Ad Thing", res.interest_group_name);
  EXPECT_EQ("https://reporting.example.com/", res.seller_report_url.spec());
  EXPECT_EQ("https://buyer-reporting.example.com/",
            res.bidder_report_url.spec());
  EXPECT_THAT(res.errors,
              testing::ElementsAre("Failed to load "
                                   "https://trustedsignaller.org/"
                                   "signals?hostname=publisher1.com&keys=k1,k2 "
                                   "HTTP status = 404 Not Found.",
                                   "Failed to load "
                                   "https://trustedsignaller.org/"
                                   "signals?hostname=publisher1.com&keys=l1,l2 "
                                   "HTTP status = 404 Not Found."));
}

// A successful auction where seller reporting worklet doesn't set a URL.
TEST_F(AuctionRunnerTest, NoReportResultUrl) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", kBidder1, kBidder1Name,
                    true /* has_signals */, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", kBidder2, kBidder2Name,
                    true /* has_signals */, "l2", "b"));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScriptNoReportUrl());
  auction_worklet::AddJsonResponse(
      &url_loader_factory_,
      GURL(kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=k1,k2"),
      R"({"k1":"a", "k2": "b", "extra": "c"})");
  auction_worklet::AddJsonResponse(
      &url_loader_factory_,
      GURL(kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=l1,l2"),
      R"({"l1":"a", "l2": "b", "extra": "c"})");

  Result res = RunStandardAuction();
  EXPECT_EQ("https://ad2.com/", res.ad_url.spec());
  EXPECT_EQ("https://anotheradthing.com", res.interest_group_owner.Serialize());
  EXPECT_EQ("Another Ad Thing", res.interest_group_name);
  EXPECT_TRUE(res.seller_report_url.is_empty());
  EXPECT_EQ("https://buyer-reporting.example.com/",
            res.bidder_report_url.spec());
  EXPECT_THAT(res.errors, testing::ElementsAre());
}

// A successful auction where bidder reporting worklet doesn't set a URL.
TEST_F(AuctionRunnerTest, NoReportWinUrl) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", kBidder1, kBidder1Name,
                    true /* has_signals */, "k1", "a") +
          kReportWinNoUrl);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", kBidder2, kBidder2Name,
                    true /* has_signals */, "l2", "b") +
          kReportWinNoUrl);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddJsonResponse(
      &url_loader_factory_,
      GURL(kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=k1,k2"),
      R"({"k1":"a", "k2": "b", "extra": "c"})");
  auction_worklet::AddJsonResponse(
      &url_loader_factory_,
      GURL(kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=l1,l2"),
      R"({"l1":"a", "l2": "b", "extra": "c"})");

  Result res = RunStandardAuction();
  EXPECT_EQ("https://ad2.com/", res.ad_url.spec());
  EXPECT_EQ("https://anotheradthing.com", res.interest_group_owner.Serialize());
  EXPECT_EQ("Another Ad Thing", res.interest_group_name);
  EXPECT_EQ("https://reporting.example.com/", res.seller_report_url.spec());
  EXPECT_TRUE(res.bidder_report_url.is_empty());
  EXPECT_THAT(res.errors, testing::ElementsAre());
}

// A successful auction where neither reporting worklets sets a URL.
TEST_F(AuctionRunnerTest, NeitherReportUrl) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", kBidder1, kBidder1Name,
                    true /* has_signals */, "k1", "a") +
          kReportWinNoUrl);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", kBidder2, kBidder2Name,
                    true /* has_signals */, "l2", "b") +
          kReportWinNoUrl);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScriptNoReportUrl());
  auction_worklet::AddJsonResponse(
      &url_loader_factory_,
      GURL(kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=k1,k2"),
      R"({"k1":"a", "k2": "b", "extra": "c"})");
  auction_worklet::AddJsonResponse(
      &url_loader_factory_,
      GURL(kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=l1,l2"),
      R"({"l1":"a", "l2": "b", "extra": "c"})");

  Result res = RunStandardAuction();
  EXPECT_EQ("https://ad2.com/", res.ad_url.spec());
  EXPECT_EQ("https://anotheradthing.com", res.interest_group_owner.Serialize());
  EXPECT_EQ("Another Ad Thing", res.interest_group_name);
  EXPECT_TRUE(res.seller_report_url.is_empty());
  EXPECT_TRUE(res.bidder_report_url.is_empty());
  EXPECT_THAT(res.errors, testing::ElementsAre());
}

TEST_F(AuctionRunnerTest, AllBiddersCrashBeforeBidding) {
  for (bool seller_worklet_loads_first : {false, true}) {
    SCOPED_TRACE(seller_worklet_loads_first);

    StartStandardAuctionWithMockService();
    auto seller_worklet = mock_worklet_service_->TakeSellerWorklet();
    ASSERT_TRUE(seller_worklet);
    auto bidder1_worklet =
        mock_worklet_service_->TakeBidderWorklet(kBidder1, kBidder1Name);
    ASSERT_TRUE(bidder1_worklet);
    auto bidder2_worklet =
        mock_worklet_service_->TakeBidderWorklet(kBidder2, kBidder2Name);
    ASSERT_TRUE(bidder2_worklet);

    if (seller_worklet_loads_first) {
      seller_worklet->CompleteLoading();
      mock_worklet_service_->Flush();
    }
    EXPECT_FALSE(auction_complete_);

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
    // The auction isn't failed until the seller worklet has completed loading.
    if (!seller_worklet_loads_first)
      seller_worklet->CompleteLoading();

    auction_run_loop_->Run();

    EXPECT_FALSE(result_.ad_url.is_valid());
    EXPECT_TRUE(result_.ad_url.is_empty());
    EXPECT_TRUE(result_.interest_group_owner.opaque());
    EXPECT_EQ("", result_.interest_group_name);
    EXPECT_TRUE(result_.seller_report_url.is_empty());
    EXPECT_TRUE(result_.bidder_report_url.is_empty());

    // Reset the service, so callbacks can be destroyed without DCHECKing.
    mock_worklet_service_.reset();
  }
}

// Test the case a single bidder worklet crashes before bidding. The auction
// should continue, without that bidder's bid.
TEST_F(AuctionRunnerTest, BidderCrashBeforeBidding) {
  for (bool other_bidder_finishes_first : {false, true}) {
    SCOPED_TRACE(other_bidder_finishes_first);
    for (bool seller_worklet_loads_first : {false, true}) {
      SCOPED_TRACE(seller_worklet_loads_first);
      StartStandardAuctionWithMockService();
      auto seller_worklet = mock_worklet_service_->TakeSellerWorklet();
      ASSERT_TRUE(seller_worklet);
      auto bidder1_worklet =
          mock_worklet_service_->TakeBidderWorklet(kBidder1, kBidder1Name);
      ASSERT_TRUE(bidder1_worklet);
      auto bidder2_worklet =
          mock_worklet_service_->TakeBidderWorklet(kBidder2, kBidder2Name);
      ASSERT_TRUE(bidder2_worklet);

      ASSERT_FALSE(auction_complete_);
      if (seller_worklet_loads_first)
        seller_worklet->CompleteLoading();
      if (other_bidder_finishes_first) {
        bidder2_worklet->CompleteLoadingAndBid(7 /* bid */,
                                               GURL("https://ad2.com/"));
      }
      mock_worklet_service_->Flush();

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
        bidder2_worklet->CompleteLoadingAndBid(7 /* bid */,
                                               GURL("https://ad2.com/"));
      }
      mock_worklet_service_->Flush();

      // The auction should be scored without waiting on the crashed kBidder1.
      auto score_ad_params = seller_worklet->WaitForScoreAd();
      EXPECT_EQ(kBidder2, score_ad_params->interest_group_owner);
      EXPECT_EQ(7, score_ad_params->bid);
      seller_worklet->InvokeScoreAdCallback(11);

      // Finish the auction.
      seller_worklet->WaitForReportResult();
      seller_worklet->InvokeReportResultCallback();
      bidder2_worklet->WaitForReportWin();
      bidder2_worklet->InvokeReportWinCallback();

      // Bidder2 won.
      auction_run_loop_->Run();
      EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
      EXPECT_EQ(kBidder2, result_.interest_group_owner);
      EXPECT_EQ(kBidder2Name, result_.interest_group_name);
      EXPECT_TRUE(result_.seller_report_url.is_empty());
      EXPECT_TRUE(result_.bidder_report_url.is_empty());

      // Reset the service, so `bidder1_callback` can be destroyed without
      // DCHECKing.
      mock_worklet_service_.reset();
    }
  }
}

// If a losing bidder crashes while scoring, the auction should succeed.
TEST_F(AuctionRunnerTest, LosingBidderCrashWhileScoring) {
  StartStandardAuctionWithMockService();

  auto seller_worklet = mock_worklet_service_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_worklet_service_->TakeBidderWorklet(kBidder1, kBidder1Name);
  ASSERT_TRUE(bidder1_worklet);
  auto bidder2_worklet =
      mock_worklet_service_->TakeBidderWorklet(kBidder2, kBidder2Name);
  ASSERT_TRUE(bidder2_worklet);

  seller_worklet->CompleteLoading();
  bidder1_worklet->CompleteLoadingAndBid(5 /* bid */, GURL("https://ad1.com/"));
  bidder2_worklet->CompleteLoadingAndBid(7 /* bid */, GURL("https://ad2.com/"));

  // Bidder1 crashes while the seller scores its bid.
  auto score_ad_params = seller_worklet->WaitForScoreAd();
  bidder1_worklet.reset();
  EXPECT_EQ(kBidder1, score_ad_params->interest_group_owner);
  EXPECT_EQ(5, score_ad_params->bid);
  seller_worklet->InvokeScoreAdCallback(10 /* score */);

  // Score Bidder2's bid.
  score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder2, score_ad_params->interest_group_owner);
  EXPECT_EQ(7, score_ad_params->bid);
  seller_worklet->InvokeScoreAdCallback(11 /* score */);

  // Finish the auction.
  seller_worklet->WaitForReportResult();
  seller_worklet->InvokeReportResultCallback();
  bidder2_worklet->WaitForReportWin();
  bidder2_worklet->InvokeReportWinCallback();
  auction_run_loop_->Run();

  // Bidder2 won.
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
  EXPECT_EQ(kBidder2, result_.interest_group_owner);
  EXPECT_EQ(kBidder2Name, result_.interest_group_name);
  EXPECT_TRUE(result_.seller_report_url.is_empty());
  EXPECT_TRUE(result_.bidder_report_url.is_empty());
}

// If the winning bidder crashes while scoring, the auction should fail.
TEST_F(AuctionRunnerTest, WinningBidderCrashWhileScoring) {
  StartStandardAuctionWithMockService();

  auto seller_worklet = mock_worklet_service_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_worklet_service_->TakeBidderWorklet(kBidder1, kBidder1Name);
  ASSERT_TRUE(bidder1_worklet);
  auto bidder2_worklet =
      mock_worklet_service_->TakeBidderWorklet(kBidder2, kBidder2Name);
  ASSERT_TRUE(bidder2_worklet);

  seller_worklet->CompleteLoading();
  bidder1_worklet->CompleteLoadingAndBid(7 /* bid */, GURL("https://ad1.com/"));
  bidder2_worklet->CompleteLoadingAndBid(5 /* bid */, GURL("https://ad2.com/"));

  // Bidder1 crashes while scoring its bid.
  auto score_ad_params = seller_worklet->WaitForScoreAd();
  bidder1_worklet.reset();
  EXPECT_EQ(kBidder1, score_ad_params->interest_group_owner);
  EXPECT_EQ(7, score_ad_params->bid);
  seller_worklet->InvokeScoreAdCallback(11 /* score */);

  // Score Bidder2's bid.
  score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder2, score_ad_params->interest_group_owner);
  EXPECT_EQ(5, score_ad_params->bid);
  seller_worklet->InvokeScoreAdCallback(10 /* score */);

  // Finish the auction.
  seller_worklet->WaitForReportResult();
  seller_worklet->InvokeReportResultCallback();
  // AuctionRunner discovered Bidder1 crashed before calling its ReportWin
  // method.
  auction_run_loop_->Run();

  // No bidder won.
  EXPECT_FALSE(result_.ad_url.is_valid());
  EXPECT_TRUE(result_.ad_url.is_empty());
  EXPECT_TRUE(result_.interest_group_owner.opaque());
  EXPECT_EQ("", result_.interest_group_name);
  EXPECT_TRUE(result_.seller_report_url.is_empty());
  EXPECT_TRUE(result_.bidder_report_url.is_empty());
}

// If the winning bidder crashes while coming up with the reporting URL, the
// auction should fail.
TEST_F(AuctionRunnerTest, WinningBidderCrashWhileReporting) {
  StartStandardAuctionWithMockService();

  auto seller_worklet = mock_worklet_service_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_worklet_service_->TakeBidderWorklet(kBidder1, kBidder1Name);
  ASSERT_TRUE(bidder1_worklet);
  auto bidder2_worklet =
      mock_worklet_service_->TakeBidderWorklet(kBidder2, kBidder2Name);
  ASSERT_TRUE(bidder2_worklet);

  seller_worklet->CompleteLoading();
  bidder1_worklet->CompleteLoadingAndBid(7 /* bid */, GURL("https://ad1.com/"));
  bidder2_worklet->CompleteLoadingAndBid(5 /* bid */, GURL("https://ad2.com/"));

  // Bidder1 crashes while scoring its bid.
  auto score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder1, score_ad_params->interest_group_owner);
  EXPECT_EQ(7, score_ad_params->bid);
  seller_worklet->InvokeScoreAdCallback(11 /* score */);

  // Score Bidder2's bid.
  score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder2, score_ad_params->interest_group_owner);
  EXPECT_EQ(5, score_ad_params->bid);
  seller_worklet->InvokeScoreAdCallback(10 /* score */);

  seller_worklet->WaitForReportResult();
  seller_worklet->InvokeReportResultCallback();
  bidder1_worklet->WaitForReportWin();
  bidder1_worklet.reset();
  auction_run_loop_->Run();

  // No bidder won.
  EXPECT_FALSE(result_.ad_url.is_valid());
  EXPECT_TRUE(result_.ad_url.is_empty());
  EXPECT_TRUE(result_.interest_group_owner.opaque());
  EXPECT_EQ("", result_.interest_group_name);
  EXPECT_TRUE(result_.seller_report_url.is_empty());
  EXPECT_TRUE(result_.bidder_report_url.is_empty());
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

    auto seller_worklet = mock_worklet_service_->TakeSellerWorklet();
    ASSERT_TRUE(seller_worklet);
    auto bidder1_worklet =
        mock_worklet_service_->TakeBidderWorklet(kBidder1, kBidder1Name);
    ASSERT_TRUE(bidder1_worklet);
    auto bidder2_worklet =
        mock_worklet_service_->TakeBidderWorklet(kBidder2, kBidder2Name);
    ASSERT_TRUE(bidder2_worklet);

    // While loop to allow breaking when the crash stage is reached.
    while (true) {
      if (crash_phase == CrashPhase::kLoad) {
        // Close the service pipe to avoid a DCHECK when deleting the seller's
        // load callback.
        mock_worklet_service_.reset();
        seller_worklet.reset();
        break;
      }

      bidder1_worklet->CompleteLoadingAndBid(5 /* bid */,
                                             GURL("https://ad1.com/"));
      bidder2_worklet->CompleteLoadingAndBid(7 /* bid */,
                                             GURL("https://ad2.com/"));
      // Wait for bids to be received.
      base::RunLoop().RunUntilIdle();

      if (crash_phase == CrashPhase::kLoadAfterBiddersLoaded) {
        // Close the service pipe to avoid a DCHECK when deleting the seller's
        // load callback.
        mock_worklet_service_.reset();
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

    // No bidder won.
    EXPECT_FALSE(result_.ad_url.is_valid());
    EXPECT_TRUE(result_.ad_url.is_empty());
    EXPECT_TRUE(result_.interest_group_owner.opaque());
    EXPECT_EQ("", result_.interest_group_name);
    EXPECT_TRUE(result_.seller_report_url.is_empty());
    EXPECT_TRUE(result_.bidder_report_url.is_empty());
  }
}

}  // namespace
}  // namespace content
