// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/auction_runner.h"

#include <vector>

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/services/auction_worklet/worklet_test_util.h"
#include "net/http/http_status_code.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace auction_worklet {
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
      return {ig: interestGroupName};
    }
  )";
  return base::StringPrintf(
      kBidScript, bid.c_str(), render_url.c_str(),
      interest_group_owner.Serialize().c_str(), interest_group_name.c_str(),
      has_signals ? "true" : "false", signal_key.c_str(), signal_val.c_str());
}

constexpr char kAuctionScript[] = R"(
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

    return bid * 2;
  }

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

class AuctionRunnerTest : public testing::Test {
 protected:
  struct Result {
    GURL ad_url;
    url::Origin interest_group_owner;
    std::string interest_group_name;
    mojom::WinningBidderReportPtr bidder_report;
    mojom::SellerReportPtr seller_report;
  };

  Result RunAuctionAndWait(const GURL& seller_decision_logic_url,
                           std::vector<mojom::BiddingInterestGroupPtr> bidders,
                           const std::string& auction_signals_json,
                           mojom::BrowserSignalsPtr browser_signals) {
    mojo::Receiver<network::mojom::URLLoaderFactory> factory_receiver{
        &url_loader_factory_};

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

    base::RunLoop run_loop;
    Result result;
    AuctionRunner::CreateAndStart(
        factory_receiver.BindNewPipeAndPassRemote(), std::move(auction_config),
        std::move(bidders), std::move(browser_signals),
        base::BindLambdaForTesting([&](const GURL& ad_url,
                                       const url::Origin& interest_group_owner,
                                       const std::string& interest_group_name,
                                       mojom::WinningBidderReportPtr bid_report,
                                       mojom::SellerReportPtr seller_report) {
          result.ad_url = ad_url;
          result.interest_group_owner = interest_group_owner;
          result.interest_group_name = interest_group_name;
          result.bidder_report = std::move(bid_report);
          result.seller_report = std::move(seller_report);
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  mojom::BiddingInterestGroupPtr MakeInterestGroup(
      const url::Origin& owner,
      const std::string& name,
      const GURL& bidding_url,
      const base::Optional<GURL>& trusted_bidding_signals_url,
      const std::vector<std::string>& trusted_bidding_signals_keys,
      const GURL& ad_url) {
    std::vector<blink::mojom::InterestGroupAdPtr> ads;
    ads.push_back(
        blink::mojom::InterestGroupAd::New(ad_url, R"({"ads": true})"));

    std::vector<mojom::PreviousWinPtr> previous_wins;
    previous_wins.push_back(
        mojom::PreviousWin::New(base::Time::Now(), R"({"winner": 0})"));
    previous_wins.push_back(
        mojom::PreviousWin::New(base::Time::Now(), R"({"winner": -1})"));
    previous_wins.push_back(
        mojom::PreviousWin::New(base::Time::Now(), R"({"winner": -2})"));

    return mojom::BiddingInterestGroup::New(
        blink::mojom::InterestGroup::New(
            base::Time::Max(), owner, name, bidding_url,
            GURL() /* update_url */, trusted_bidding_signals_url,
            trusted_bidding_signals_keys, base::nullopt, std::move(ads)),
        mojom::BiddingBrowserSignals::New(3, 5, std::move(previous_wins)));
  }

  Result RunStandardAuction() {
    std::vector<mojom::BiddingInterestGroupPtr> bidders;
    bidders.push_back(MakeInterestGroup(kBidder1, kBidder1Name, kBidder1Url,
                                        kTrustedSignalsUrl, {"k1", "k2"},
                                        GURL("https://ad1.com")));
    bidders.push_back(MakeInterestGroup(kBidder2, kBidder2Name, kBidder2Url,
                                        kTrustedSignalsUrl, {"l1", "l2"},
                                        GURL("https://ad2.com")));

    return RunAuctionAndWait(
        kSellerUrl, std::move(bidders),
        R"({"isAuctionSignals": true})", /* auction_signals_json */
        mojom::BrowserSignals::New(
            url::Origin::Create(GURL("https://publisher1.com")),
            url::Origin::Create(kSellerUrl)));
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
  network::TestURLLoaderFactory url_loader_factory_;
};

// An auction with two successful bids.
TEST_F(AuctionRunnerTest, Basic) {
  AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", kBidder1, kBidder1Name,
                    true /* has_signals */, "k1", "a"));
  AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", kBidder2, kBidder2Name,
                    true /* has_signals */, "l2", "b"));
  AddJavascriptResponse(&url_loader_factory_, kSellerUrl, kAuctionScript);
  AddJsonResponse(
      &url_loader_factory_,
      GURL(kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=k1,k2"),
      R"({"k1":"a", "k2": "b", "extra": "c"})");
  AddJsonResponse(
      &url_loader_factory_,
      GURL(kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=l1,l2"),
      R"({"l1":"a", "l2": "b", "extra": "c"})");

  Result res = RunStandardAuction();
  EXPECT_EQ("https://ad2.com/", res.ad_url.spec());
  EXPECT_EQ("https://anotheradthing.com", res.interest_group_owner.Serialize());
  EXPECT_EQ("Another Ad Thing", res.interest_group_name);
  EXPECT_TRUE(res.seller_report->report_requested);
  EXPECT_EQ("https://reporting.example.com/",
            res.seller_report->report_url.spec());
  EXPECT_EQ(R"({"topWindowHostname":"publisher1.com",)"
            R"("interestGroupOwner":"https://anotheradthing.com",)"
            R"("renderUrl":"https://ad2.com/",)"
            R"("adRenderFingerprint":"#####",)"
            R"("bid":2,"desirability":4})",
            res.seller_report->signals_for_winner_json);
  EXPECT_TRUE(res.bidder_report->report_requested);
  EXPECT_EQ("https://buyer-reporting.example.com/",
            res.bidder_report->report_url.spec());
}

// An auction where one bid is successful, another's script 404s.
TEST_F(AuctionRunnerTest, OneBidOne404) {
  AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", kBidder1, kBidder1Name,
                    true /* has_signals */, "k1", "a"));
  url_loader_factory_.AddResponse(kBidder2Url.spec(), "", net::HTTP_NOT_FOUND);
  AddJavascriptResponse(&url_loader_factory_, kSellerUrl, kAuctionScript);
  AddJsonResponse(
      &url_loader_factory_,
      GURL(kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=k1,k2"),
      R"({"k1":"a", "k2": "b", "extra": "c"})");
  AddJsonResponse(
      &url_loader_factory_,
      GURL(kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=l1,l2"),
      R"({"l1":"a", "l2": "b", "extra": "c"})");

  Result res = RunStandardAuction();
  EXPECT_EQ("https://ad1.com/", res.ad_url.spec());
  EXPECT_EQ("https://adplatform.com", res.interest_group_owner.Serialize());
  EXPECT_EQ("Ad Platform", res.interest_group_name);
  EXPECT_TRUE(res.seller_report->report_requested);
  EXPECT_EQ("https://reporting.example.com/",
            res.seller_report->report_url.spec());
  EXPECT_EQ(R"({"topWindowHostname":"publisher1.com",)"
            R"("interestGroupOwner":"https://adplatform.com",)"
            R"("renderUrl":"https://ad1.com/",)"
            R"("adRenderFingerprint":"#####",)"
            R"("bid":1,"desirability":2})",
            res.seller_report->signals_for_winner_json);
  EXPECT_TRUE(res.bidder_report->report_requested);
  EXPECT_EQ("https://buyer-reporting.example.com/",
            res.bidder_report->report_url.spec());
}

// An auction where one bid is successful, another's script does not provide a
// bidding function.
TEST_F(AuctionRunnerTest, OneBidOneNotMade) {
  AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", kBidder1, kBidder1Name,
                    true /* has_signals */, "k1", "a"));

  // The auction script doesn't make any bids.
  AddJavascriptResponse(&url_loader_factory_, kBidder2Url, kAuctionScript);
  AddJavascriptResponse(&url_loader_factory_, kSellerUrl, kAuctionScript);
  AddJsonResponse(
      &url_loader_factory_,
      GURL(kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=k1,k2"),
      R"({"k1":"a", "k2": "b", "extra": "c"})");
  AddJsonResponse(
      &url_loader_factory_,
      GURL(kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=l1,l2"),
      R"({"l1":"a", "l2": "b", "extra": "c"})");

  Result res = RunStandardAuction();
  EXPECT_EQ("https://ad1.com/", res.ad_url.spec());
  EXPECT_EQ("https://adplatform.com", res.interest_group_owner.Serialize());
  EXPECT_EQ("Ad Platform", res.interest_group_name);
  EXPECT_TRUE(res.seller_report->report_requested);
  EXPECT_EQ("https://reporting.example.com/",
            res.seller_report->report_url.spec());
  EXPECT_EQ(R"({"topWindowHostname":"publisher1.com",)"
            R"("interestGroupOwner":"https://adplatform.com",)"
            R"("renderUrl":"https://ad1.com/",)"
            R"("adRenderFingerprint":"#####",)"
            R"("bid":1,"desirability":2})",
            res.seller_report->signals_for_winner_json);
  EXPECT_TRUE(res.bidder_report->report_requested);
  EXPECT_EQ("https://buyer-reporting.example.com/",
            res.bidder_report->report_url.spec());
}

// An auction where no bidding scripts load successfully.
TEST_F(AuctionRunnerTest, NoBids) {
  url_loader_factory_.AddResponse(kBidder1Url.spec(), "", net::HTTP_NOT_FOUND);
  url_loader_factory_.AddResponse(kBidder2Url.spec(), "", net::HTTP_NOT_FOUND);
  AddJavascriptResponse(&url_loader_factory_, kSellerUrl, kAuctionScript);
  AddJsonResponse(
      &url_loader_factory_,
      GURL(kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=k1,k2"),
      R"({"k1":"a", "k2":"b", "extra":"c"})");
  AddJsonResponse(
      &url_loader_factory_,
      GURL(kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=l1,l2"),
      R"({"l1":"a", "l2": "b", "extra":"c"})");

  Result res = RunStandardAuction();
  EXPECT_TRUE(res.ad_url.is_empty());
  EXPECT_TRUE(res.interest_group_owner.opaque());
  EXPECT_EQ("", res.interest_group_name);
  EXPECT_FALSE(res.seller_report->report_requested);
  EXPECT_TRUE(res.seller_report->report_url.is_empty());
  EXPECT_EQ("", res.seller_report->signals_for_winner_json);
  EXPECT_FALSE(res.bidder_report->report_requested);
  EXPECT_TRUE(res.bidder_report->report_url.is_empty());
}

// An auction where none of the bidding scripts has a valid bidding function.
TEST_F(AuctionRunnerTest, NoBidMadeByScript) {
  // kAuctionScript is a valid script that doesn't have a bidding function.
  AddJavascriptResponse(&url_loader_factory_, kBidder1Url, kAuctionScript);
  AddJavascriptResponse(&url_loader_factory_, kBidder2Url, kAuctionScript);
  AddJavascriptResponse(&url_loader_factory_, kSellerUrl, kAuctionScript);
  AddJsonResponse(
      &url_loader_factory_,
      GURL(kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=k1,k2"),
      R"({"k1":"a", "k2":"b", "extra":"c"})");
  AddJsonResponse(
      &url_loader_factory_,
      GURL(kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=l1,l2"),
      R"({"l1":"a", "l2": "b", "extra":"c"})");

  Result res = RunStandardAuction();
  EXPECT_TRUE(res.ad_url.is_empty());
  EXPECT_TRUE(res.interest_group_owner.opaque());
  EXPECT_EQ("", res.interest_group_name);
  EXPECT_FALSE(res.seller_report->report_requested);
  EXPECT_TRUE(res.seller_report->report_url.is_empty());
  EXPECT_EQ("", res.seller_report->signals_for_winner_json);
  EXPECT_FALSE(res.bidder_report->report_requested);
  EXPECT_TRUE(res.bidder_report->report_url.is_empty());
}

// An auction where the seller script doesn't have a scoring function.
TEST_F(AuctionRunnerTest, SellerRejectsAll) {
  std::string bid_script1 =
      MakeBidScript("1", "https://ad1.com/", kBidder1, kBidder1Name,
                    true /* has_signals */, "k1", "a");
  AddJavascriptResponse(&url_loader_factory_, kBidder1Url, bid_script1);
  AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", kBidder2, kBidder2Name,
                    true /* has_signals */, "l2", "b"));

  // No seller scoring function in a bid script.
  AddJavascriptResponse(&url_loader_factory_, kSellerUrl, bid_script1);
  AddJsonResponse(
      &url_loader_factory_,
      GURL(kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=k1,k2"),
      R"({"k1":"a", "k2":"b", "extra":"c"})");
  AddJsonResponse(
      &url_loader_factory_,
      GURL(kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=l1,l2"),
      R"({"l1":"a", "l2": "b", "extra":"c"})");

  Result res = RunStandardAuction();
  EXPECT_TRUE(res.ad_url.is_empty());
  EXPECT_TRUE(res.interest_group_owner.opaque());
  EXPECT_EQ("", res.interest_group_name);
  EXPECT_FALSE(res.seller_report->report_requested);
  EXPECT_TRUE(res.seller_report->report_url.is_empty());
  EXPECT_EQ("", res.seller_report->signals_for_winner_json);
  EXPECT_FALSE(res.bidder_report->report_requested);
  EXPECT_TRUE(res.bidder_report->report_url.is_empty());
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
  EXPECT_FALSE(res.seller_report->report_requested);
  EXPECT_TRUE(res.seller_report->report_url.is_empty());
  EXPECT_EQ("", res.seller_report->signals_for_winner_json);
  EXPECT_FALSE(res.bidder_report->report_requested);
  EXPECT_TRUE(res.bidder_report->report_url.is_empty());

  EXPECT_EQ(0, url_loader_factory_.NumPending());
}

// An auction where bidders don't requested trusted bidding signals.
TEST_F(AuctionRunnerTest, NoTrustedBiddingSignals) {
  AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", kBidder1, kBidder1Name,
                    false /* has_signals */, "k1", "a"));
  AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", kBidder2, kBidder2Name,
                    false /* has_signals */, "l2", "b"));
  AddJavascriptResponse(&url_loader_factory_, kSellerUrl, kAuctionScript);

  std::vector<mojom::BiddingInterestGroupPtr> bidders;
  bidders.push_back(MakeInterestGroup(kBidder1, kBidder1Name, kBidder1Url,
                                      base::nullopt, {"k1", "k2"},
                                      GURL("https://ad1.com")));
  bidders.push_back(MakeInterestGroup(kBidder2, kBidder2Name, kBidder2Url,
                                      base::nullopt, {"l1", "l2"},
                                      GURL("https://ad2.com")));

  Result res = RunAuctionAndWait(
      kSellerUrl, std::move(bidders),
      R"({"isAuctionSignals": true})", /* auction_signals_json */
      mojom::BrowserSignals::New(
          url::Origin::Create(GURL("https://publisher1.com")),
          url::Origin::Create(kSellerUrl)));

  EXPECT_EQ("https://ad2.com/", res.ad_url.spec());
  EXPECT_EQ("https://anotheradthing.com", res.interest_group_owner.Serialize());
  EXPECT_EQ("Another Ad Thing", res.interest_group_name);
  EXPECT_TRUE(res.seller_report->report_requested);
  EXPECT_EQ("https://reporting.example.com/",
            res.seller_report->report_url.spec());
  EXPECT_EQ(R"({"topWindowHostname":"publisher1.com",)"
            R"("interestGroupOwner":"https://anotheradthing.com",)"
            R"("renderUrl":"https://ad2.com/",)"
            R"("adRenderFingerprint":"#####",)"
            R"("bid":2,"desirability":4})",
            res.seller_report->signals_for_winner_json);
  EXPECT_TRUE(res.bidder_report->report_requested);
  EXPECT_EQ("https://buyer-reporting.example.com/",
            res.bidder_report->report_url.spec());
}

// An auction where trusted bidding signals are requested, but the fetch 404s.
TEST_F(AuctionRunnerTest, TrustedBiddingSignals404) {
  AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", kBidder1, kBidder1Name,
                    false /* has_signals */, "k1", "a"));
  AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", kBidder2, kBidder2Name,
                    false /* has_signals */, "l2", "b"));
  url_loader_factory_.AddResponse(
      kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=k1,k2", "",
      net::HTTP_NOT_FOUND);
  url_loader_factory_.AddResponse(
      kTrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=l1,l2", "",
      net::HTTP_NOT_FOUND);
  AddJavascriptResponse(&url_loader_factory_, kSellerUrl, kAuctionScript);

  Result res = RunStandardAuction();
  EXPECT_EQ("https://ad2.com/", res.ad_url.spec());
  EXPECT_EQ("https://anotheradthing.com", res.interest_group_owner.Serialize());
  EXPECT_EQ("Another Ad Thing", res.interest_group_name);
  EXPECT_TRUE(res.seller_report->report_requested);
  EXPECT_EQ("https://reporting.example.com/",
            res.seller_report->report_url.spec());
  EXPECT_EQ(R"({"topWindowHostname":"publisher1.com",)"
            R"("interestGroupOwner":"https://anotheradthing.com",)"
            R"("renderUrl":"https://ad2.com/",)"
            R"("adRenderFingerprint":"#####",)"
            R"("bid":2,"desirability":4})",
            res.seller_report->signals_for_winner_json);
  EXPECT_TRUE(res.bidder_report->report_requested);
  EXPECT_EQ("https://buyer-reporting.example.com/",
            res.bidder_report->report_url.spec());
}

}  // namespace
}  // namespace auction_worklet
