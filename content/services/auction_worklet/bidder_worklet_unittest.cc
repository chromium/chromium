// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/bidder_worklet.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/worklet_devtools_debug_test_util.h"
#include "content/services/auction_worklet/worklet_test_util.h"
#include "content/services/auction_worklet/worklet_v8_debug_test_util.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "net/http/http_status_code.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

using testing::HasSubstr;
using testing::StartsWith;

namespace auction_worklet {
namespace {

// Creates generateBid() scripts with the specified result value, in raw
// Javascript. Allows returning generateBid() arguments, arbitrary values,
// incorrect types, etc.
std::string CreateGenerateBidScript(const std::string& raw_return_value) {
  constexpr char kGenerateBidScript[] = R"(
    function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                         trustedBiddingSignals, browserSignals) {
      return %s;
    }
  )";
  return base::StringPrintf(kGenerateBidScript, raw_return_value.c_str());
}

// Returns a working script, primarily for testing failure cases where it
// should not be run.
static std::string CreateBasicGenerateBidScript() {
  return CreateGenerateBidScript(
      R"({ad: ["ad"], bid:1, render:"https://response.test/"})");
}

// Creates bidder worklet script with a reportWin() function with the specified
// body, and the default generateBid() function.
std::string CreateReportWinScript(const std::string& function_body) {
  constexpr char kReportWinScript[] = R"(
    function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
                       browserSignals) {
      %s;
    }
  )";
  return CreateBasicGenerateBidScript() +
         base::StringPrintf(kReportWinScript, function_body.c_str());
}

class BidderWorkletTest : public testing::Test {
 public:
  BidderWorkletTest() {
    SetDefaultParameters();
    v8_helper_ = AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner());
  }

  ~BidderWorkletTest() override { task_environment_.RunUntilIdle(); }

  // Default values. No test actually depends on these being anything but valid,
  // but test that set these can use this to reset values to default after each
  // test.
  void SetDefaultParameters() {
    interest_group_owner_ = url::Origin::Create(GURL("https://foo.test"));
    interest_group_name_ = "Fred";
    interest_group_user_bidding_signals_ = absl::nullopt;

    interest_group_ads_.clear();
    interest_group_ads_.emplace_back(blink::InterestGroup::Ad(
        GURL("https://response.test/"), absl::nullopt /* metadata */));

    interest_group_ad_components_.reset();
    interest_group_ad_components_.emplace();
    interest_group_ad_components_->emplace_back(blink::InterestGroup::Ad(
        GURL("https://ad_component.test/"), absl::nullopt /* metadata */));

    interest_group_trusted_bidding_signals_url_.reset();
    interest_group_trusted_bidding_signals_keys_.reset();

    browser_signal_join_count_ = 2;
    browser_signal_bid_count_ = 3;
    browser_signal_prev_wins_.clear();

    auction_signals_ = "[\"auction_signals\"]";
    per_buyer_signals_ = "[\"per_buyer_signals\"]";
    browser_signal_top_window_origin_ =
        url::Origin::Create(GURL("https://top.window.test/"));
    browser_signal_seller_origin_ =
        url::Origin::Create(GURL("https://browser.signal.seller.test/"));
    seller_signals_ = "[\"seller_signals\"]";
    browser_signal_render_url_ = GURL("https://render_url.test/");
    browser_signal_bid_ = 1;
  }

  // Configures `url_loader_factory_` to return a generateBid() script with the
  // specified return line Then runs the script, expecting the provided result.
  void RunGenerateBidWithReturnValueExpectingResult(
      const std::string& raw_return_value,
      mojom::BidderWorkletBidPtr expected_bid,
      std::vector<std::string> expected_errors = std::vector<std::string>()) {
    RunGenerateBidWithJavascriptExpectingResult(
        CreateGenerateBidScript(raw_return_value), std::move(expected_bid),
        expected_errors);
  }

  // Configures `url_loader_factory_` to return a script with the specified
  // Javascript Then runs the script, expecting the provided result.
  void RunGenerateBidWithJavascriptExpectingResult(
      const std::string& javascript,
      mojom::BidderWorkletBidPtr expected_bid,
      std::vector<std::string> expected_errors = std::vector<std::string>()) {
    SCOPED_TRACE(javascript);
    AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                          javascript);
    RunGenerateBidExpectingResult(std::move(expected_bid), expected_errors);
  }

  // Loads and runs a generateBid() script, expecting the provided result.
  void RunGenerateBidExpectingResult(
      mojom::BidderWorkletBidPtr expected_bid,
      std::vector<std::string> expected_errors = std::vector<std::string>()) {
    auto bidder_worklet = CreateWorkletAndGenerateBid();

    EXPECT_EQ(expected_bid.is_null(), bid_.is_null());
    if (expected_bid && bid_) {
      EXPECT_EQ(expected_bid->ad, bid_->ad);
      EXPECT_EQ(expected_bid->bid, bid_->bid);
      EXPECT_EQ(expected_bid->render_url, bid_->render_url);
      if (!expected_bid->ad_components) {
        EXPECT_FALSE(bid_->ad_components);
      } else {
        EXPECT_THAT(*bid_->ad_components,
                    ::testing::ElementsAreArray(*expected_bid->ad_components));
      }
    }
    EXPECT_EQ(expected_errors, bid_errors_);
  }

  // Configures `url_loader_factory_` to return a reportWin() script with the
  // specified body. Then runs the script, expecting the provided result.
  void RunReportWinWithFunctionBodyExpectingResult(
      const std::string& function_body,
      const absl::optional<GURL>& expected_report_url,
      const std::vector<std::string>& expected_errors =
          std::vector<std::string>()) {
    RunReportWinWithJavascriptExpectingResult(
        CreateReportWinScript(function_body), expected_report_url,
        expected_errors);
  }

  // Configures `url_loader_factory_` to return a reportWin() script with the
  // specified Javascript. Then runs the script, expecting the provided result.
  void RunReportWinWithJavascriptExpectingResult(
      const std::string& javascript,
      const absl::optional<GURL>& expected_report_url,
      const std::vector<std::string>& expected_errors =
          std::vector<std::string>()) {
    SCOPED_TRACE(javascript);
    AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                          javascript);
    RunReportWinExpectingResult(expected_report_url, expected_errors);
  }

  // Runs reportWin() on an already loaded worklet,  verifies the return
  // value and invokes `done_closure` when done. Expects something else to
  // spin the event loop.
  void RunReportWinExpectingResultAsync(
      mojom::BidderWorklet* bidder_worklet,
      const absl::optional<GURL>& expected_report_url,
      const std::vector<std::string>& expected_errors,
      base::OnceClosure done_closure) {
    bidder_worklet->ReportWin(
        auction_signals_, per_buyer_signals_, browser_signal_top_window_origin_,
        seller_signals_, browser_signal_render_url_, browser_signal_bid_,
        base::BindOnce(
            [](const absl::optional<GURL>& expected_report_url,
               const std::vector<std::string>& expected_errors,
               base::OnceClosure done_closure,
               const absl::optional<GURL>& report_url,
               const std::vector<std::string>& errors) {
              EXPECT_EQ(expected_report_url, report_url);
              EXPECT_EQ(expected_errors, errors);
              std::move(done_closure).Run();
            },
            expected_report_url, expected_errors, std::move(done_closure)));
  }

  // Loads and runs a reportWin() with the provided return line, expecting the
  // supplied result.
  void RunReportWinExpectingResult(
      const absl::optional<GURL>& expected_report_url,
      const std::vector<std::string>& expected_errors =
          std::vector<std::string>()) {
    auto bidder_worklet = CreateWorkletAndGenerateBid();
    ASSERT_TRUE(bidder_worklet);

    base::RunLoop run_loop;
    RunReportWinExpectingResultAsync(bidder_worklet.get(), expected_report_url,
                                     expected_errors, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Creates a BiddingInterestGroup based on test fixture configuration.
  mojom::BiddingInterestGroupPtr CreateBiddingInterestGroup(const GURL& url) {
    blink::InterestGroup interest_group;
    interest_group.owner = interest_group_owner_;
    interest_group.name = interest_group_name_;
    interest_group.bidding_url = url;
    interest_group.user_bidding_signals = interest_group_user_bidding_signals_;
    interest_group.trusted_bidding_signals_url =
        interest_group_trusted_bidding_signals_url_;
    interest_group.trusted_bidding_signals_keys =
        interest_group_trusted_bidding_signals_keys_;
    interest_group.ads = interest_group_ads_;
    interest_group.ad_components = interest_group_ad_components_;

    mojom::BiddingBrowserSignalsPtr bidding_browser_signals =
        mojom::BiddingBrowserSignals::New(
            browser_signal_join_count_, browser_signal_bid_count_,
            CloneWinList(browser_signal_prev_wins_));
    mojom::BiddingInterestGroupPtr bidding_interest_group =
        mojom::BiddingInterestGroup::New(std::move(interest_group),
                                         std::move(bidding_browser_signals));
    return bidding_interest_group;
  }

  // Create a BidderWorklet, returning the remote. If `out_bidder_worklet_impl`
  // is non-null, will also stash the actual implementation pointer there.
  // if `url` is empty, uses `interest_group_bidding_url_`.
  mojo::Remote<mojom::BidderWorklet> CreateWorklet(
      GURL url = GURL(),
      bool pause_for_debugger_on_start = false,
      BidderWorklet** out_bidder_worklet_impl = nullptr) {
    CHECK(!load_script_run_loop_);

    mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory;
    url_loader_factory_.Clone(
        url_loader_factory.InitWithNewPipeAndPassReceiver());

    auto bidder_worklet_impl = std::make_unique<BidderWorklet>(
        v8_helper_, pause_for_debugger_on_start, std::move(url_loader_factory),
        CreateBiddingInterestGroup(url.is_empty() ? interest_group_bidding_url_
                                                  : url));
    if (out_bidder_worklet_impl)
      *out_bidder_worklet_impl = bidder_worklet_impl.get();

    mojo::Remote<mojom::BidderWorklet> bidder_worklet;
    mojo::MakeSelfOwnedReceiver(std::move(bidder_worklet_impl),
                                bidder_worklet.BindNewPipeAndPassReceiver());
    return bidder_worklet;
  }

  void GenerateBid(mojom::BidderWorklet* bidder_worklet) {
    bidder_worklet->GenerateBid(
        auction_signals_, per_buyer_signals_, browser_signal_top_window_origin_,
        browser_signal_seller_origin_, auction_start_time_,
        base::BindOnce(&BidderWorkletTest::GenerateBidCallback,
                       base::Unretained(this)));
  }

  // Create a BidderWorklet and invokes GenerateBid(), waiting for the
  // GenerateBid() callback to be invoked. Returns a null Remote on failure.
  mojo::Remote<mojom::BidderWorklet> CreateWorkletAndGenerateBid() {
    mojo::Remote<mojom::BidderWorklet> bidder_worklet = CreateWorklet();
    GenerateBid(bidder_worklet.get());

    load_script_run_loop_ = std::make_unique<base::RunLoop>();
    load_script_run_loop_->Run();
    load_script_run_loop_.reset();
    if (!bid_)
      return mojo::Remote<mojom::BidderWorklet>();
    return bidder_worklet;
  }

  const mojom::BidderWorkletBidPtr& bid() const { return bid_; }
  const std::vector<std::string> bid_errors() const { return bid_errors_; }

  void GenerateBidCallback(mojom::BidderWorkletBidPtr bid,
                           const std::vector<std::string>& errors) {
    bid_ = std::move(bid);
    bid_errors_ = std::move(errors);
    load_script_run_loop_->Quit();
  }

 protected:
  std::vector<mojo::StructPtr<mojom::PreviousWin>> CloneWinList(
      const std::vector<mojo::StructPtr<mojom::PreviousWin>>& prev_win_list) {
    std::vector<mojo::StructPtr<mojom::PreviousWin>> out;
    for (const auto& prev_win : prev_win_list) {
      out.push_back(prev_win->Clone());
    }
    return out;
  }

  base::test::TaskEnvironment task_environment_;

  // Values used to construct the BiddingInterestGroup passed to the
  // BidderWorklet.
  url::Origin interest_group_owner_;
  std::string interest_group_name_;
  const GURL interest_group_bidding_url_ = GURL("https://url.test/");
  absl::optional<std::string> interest_group_user_bidding_signals_;
  std::vector<blink::InterestGroup::Ad> interest_group_ads_;
  absl::optional<std::vector<blink::InterestGroup::Ad>>
      interest_group_ad_components_;
  absl::optional<GURL> interest_group_trusted_bidding_signals_url_;
  absl::optional<std::vector<std::string>>
      interest_group_trusted_bidding_signals_keys_;
  int browser_signal_join_count_;
  int browser_signal_bid_count_;
  std::vector<mojo::StructPtr<mojom::PreviousWin>> browser_signal_prev_wins_;

  absl::optional<std::string> auction_signals_;
  absl::optional<std::string> per_buyer_signals_;
  url::Origin browser_signal_top_window_origin_;
  url::Origin browser_signal_seller_origin_;
  std::string seller_signals_;
  GURL browser_signal_render_url_;
  double browser_signal_bid_;

  // Use a single constant start time. Only delta times are provided to scripts,
  // relative to the time of the auction, so no need to vary the auction time.
  const base::Time auction_start_time_ = base::Time::Now();

  // Reuseable run loop for loading the script. It's always populated after
  // creating the worklet, to cause a crash if the callback is invoked
  // synchronously.
  std::unique_ptr<base::RunLoop> load_script_run_loop_;

  // Values passed to the GenerateBidCallback().
  mojom::BidderWorkletBidPtr bid_;
  std::vector<std::string> bid_errors_;

  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<AuctionV8Helper> v8_helper_;
};

// Test the case the BidderWorklet pipe is closed before invoking the
// GenerateBidCallback. The invocation of the GenerateBidCallback is not
// observed, since the callback is on the pipe that was just closed. There
// should be no Mojo exception due to destroying the creation callback without
// invoking it.
TEST_F(BidderWorkletTest, PipeClosed) {
  auto bidder_worklet = CreateWorklet();
  GenerateBid(bidder_worklet.get());
  bidder_worklet.reset();

  // This should not result in a Mojo crash.
  task_environment_.RunUntilIdle();
}

TEST_F(BidderWorkletTest, NetworkError) {
  url_loader_factory_.AddResponse(interest_group_bidding_url_.spec(),
                                  CreateBasicGenerateBidScript(),
                                  net::HTTP_NOT_FOUND);
  RunGenerateBidExpectingResult(
      mojom::BidderWorkletBidPtr() /* expected_bid */,
      {"Failed to load https://url.test/ HTTP status = 404 Not Found."});
}

TEST_F(BidderWorkletTest, CompileError) {
  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        "Invalid Javascript");
  EXPECT_FALSE(CreateWorkletAndGenerateBid());

  EXPECT_FALSE(bid());
  ASSERT_EQ(1u, bid_errors().size());
  EXPECT_THAT(bid_errors()[0], StartsWith("https://url.test/:1 "));
  EXPECT_THAT(bid_errors()[0], HasSubstr("SyntaxError"));
}

// Test parsing of return values.
TEST_F(BidderWorkletTest, GenerateBidResult) {
  // Base case. Also serves to make sure the script returned by
  // CreateBasicGenerateBidScript() does indeed work.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateBasicGenerateBidScript(),
      mojom::BidderWorkletBid::New(
          "[\"ad\"]", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));

  // --------
  // Vary ad
  // --------

  // Make sure "ad" can be of a variety of JS object types.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New("\"ad\"", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: {a:1,b:null}, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          R"({"a":1,"b":null})", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: [2.5,[]], bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          "[2.5,[]]", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: -5, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New("-5", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()));
  // Some values that can't be represented in JSON become null.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: 0/0, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New("null", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: [globalThis.not_defined], bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New("[null]", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: [function() {return 1;}], bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New("[null]", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()));

  // Other values JSON can't represent result in failing instead of null.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: globalThis.not_defined, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBidPtr() /* expected_bid */,
      {"https://url.test/ generateBid() return value "
       "has incorrect structure."});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: function() {return 1;}, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBidPtr() /* expected_bid */,
      {"https://url.test/ generateBid() return value "
       "has incorrect structure."});

  // Make sure recursive structures aren't allowed in ad field.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(
        function generateBid() {
          var a = [];
          a[0] = a;
          return {ad: a, bid:1, render:"https://response.test/"};
        }
      )",
      mojom::BidderWorkletBidPtr() /* expected_bid */,
      {"https://url.test/ generateBid() return value "
       "has incorrect structure."});

  // --------
  // Vary bid
  // --------

  // Valid positive bid values.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid:1.5, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          "\"ad\"", 1.5, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid:2, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New("\"ad\"", 2, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid:0.001, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          "\"ad\"", 0.001, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));

  // Bids <= 0.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:0, render:"https://response.test/"})",
      mojom::BidderWorkletBidPtr() /* expected_bid */);
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:-10, render:"https://response.test/"})",
      mojom::BidderWorkletBidPtr() /* expected_bid */);
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:-1.5, render:"https://response.test/"})",
      mojom::BidderWorkletBidPtr() /* expected_bid */);

  // Infinite and NaN bid.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1/0, render:"https://response.test/"})",
      mojom::BidderWorkletBidPtr() /* expected_bid */);
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:-1/0, render:"https://response.test/"})",
      mojom::BidderWorkletBidPtr() /* expected_bid */);
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:0/0, render:"https://response.test/"})",
      mojom::BidderWorkletBidPtr() /* expected_bid */);

  // Non-numeric bid.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:"1", render:"https://response.test/"})",
      mojom::BidderWorkletBidPtr() /* expected_bid */,
      {"https://url.test/ generateBid() return value "
       "has incorrect structure."});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:[1], render:"https://response.test/"})",
      mojom::BidderWorkletBidPtr() /* expected_bid */,
      {"https://url.test/ generateBid() return value "
       "has incorrect structure."});

  // ---------
  // Vary URL.
  // ---------

  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          "[\"ad\"]", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));

  // Disallowed render schemes.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:"http://response.test/"})",
      mojom::BidderWorkletBidPtr() /* expected_bid */,
      {"https://url.test/ generateBid() returned "
       "render URL that isn't a valid https:// URL."});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:"chrome-extension://response.test/"})",
      mojom::BidderWorkletBidPtr() /* expected_bid */,
      {"https://url.test/ generateBid() returned "
       "render URL that isn't a valid https:// URL."});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:"about:blank"})",
      mojom::BidderWorkletBidPtr() /* expected_bid */,
      {"https://url.test/ generateBid() returned "
       "render URL that isn't a valid https:// URL."});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:"data:,foo"})",
      mojom::BidderWorkletBidPtr() /* expected_bid */,
      {"https://url.test/ generateBid() returned "
       "render URL that isn't a valid https:// URL."});

  // Invalid render URLs.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:"test"})",
      mojom::BidderWorkletBidPtr() /* expected_bid */,
      {"https://url.test/ generateBid() returned "
       "render URL that isn't a valid https:// URL."});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:"http://"})",
      mojom::BidderWorkletBidPtr() /* expected_bid */,
      {"https://url.test/ generateBid() returned "
       "render URL that isn't a valid https:// URL."});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:["http://response.test/"]})",
      mojom::BidderWorkletBidPtr() /* expected_bid */,
      {"https://url.test/ generateBid() return value "
       "has incorrect structure."});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:9})",
      mojom::BidderWorkletBidPtr() /* expected_bid */,
      {"https://url.test/ generateBid() return value "
       "has incorrect structure."});

  // ----------------------
  // No adComponents in IG.
  // ----------------------

  interest_group_ad_components_ = absl::nullopt;

  // Auction should fail if adComponents in return value is an array.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad",
        bid:1,
        render:"https://response.test/",
        adComponents:["http://response.test/"]})",
      mojom::BidderWorkletBidPtr(),
      {"https://url.test/ generateBid() return value contains adComponents but "
       "InterestGroup has no adComponents."});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad",
        bid:1,
        render:"https://response.test/",
        adComponents:[]})",
      mojom::BidderWorkletBidPtr(),
      {"https://url.test/ generateBid() return value contains adComponents but "
       "InterestGroup has no adComponents."});

  // Auction should fail if adComponents in return value is an unexpected type.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad",
        bid:1,
        render:"https://response.test/",
        adComponents:5})",
      mojom::BidderWorkletBidPtr(),
      {"https://url.test/ generateBid() return value contains adComponents but "
       "InterestGroup has no adComponents."});

  // Not present and null adComponents fields should result in success.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad",
        bid:1,
        render:"https://response.test/"})",
      mojom::BidderWorkletBid::New("\"ad\"", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad",
        bid:1,
        render:"https://response.test/",
        adComponents:null})",
      mojom::BidderWorkletBid::New("\"ad\"", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()));

  SetDefaultParameters();

  // -----------------------------
  // Non-empty adComponents in IG.
  // -----------------------------

  // Empty adComponents in results is allowed.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad",
        bid:1,
        render:"https://response.test/"})",
      mojom::BidderWorkletBid::New("\"ad\"", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()));

  // Auction should fail if adComponents in return value is an unexpected type.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad",
        bid:1,
        render:"https://response.test/",
        adComponents:5})",
      mojom::BidderWorkletBidPtr(),
      {"https://url.test/ generateBid() returned adComponents value must be "
       "an array."});

  // Unexpected value types in adComponents should fail.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad",
        bid:1,
        render:"https://response.test/",
        adComponents:[{}]})",
      mojom::BidderWorkletBidPtr(),
      {"https://url.test/ generateBid() returned adComponents value must be an "
       "array of strings."});

  // Up to 20 values in the output adComponents output array are allowed (And
  // they can all be the same URL).
  static_assert(blink::kMaxAdAuctionAdComponents == 20,
                "Unexpected value of kMaxAdAuctionAdComponents");
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad",
        bid:1,
        render:"https://response.test/",
        adComponents:[
          "https://ad_component.test/" /* 1 */,
          "https://ad_component.test/" /* 2 */,
          "https://ad_component.test/" /* 3 */,
          "https://ad_component.test/" /* 4 */,
          "https://ad_component.test/" /* 5 */,
          "https://ad_component.test/" /* 6 */,
          "https://ad_component.test/" /* 7 */,
          "https://ad_component.test/" /* 8 */,
          "https://ad_component.test/" /* 9 */,
          "https://ad_component.test/" /* 10 */,
          "https://ad_component.test/" /* 11 */,
          "https://ad_component.test/" /* 12 */,
          "https://ad_component.test/" /* 13 */,
          "https://ad_component.test/" /* 14 */,
          "https://ad_component.test/" /* 15 */,
          "https://ad_component.test/" /* 16 */,
          "https://ad_component.test/" /* 17 */,
          "https://ad_component.test/" /* 18 */,
          "https://ad_component.test/" /* 19 */,
          "https://ad_component.test/" /* 20 */,
        ]})",
      mojom::BidderWorkletBid::New(
          "\"ad\"", 1, GURL("https://response.test/"),
          /*ad_components=*/
          std::vector<GURL>{
              GURL("https://ad_component.test/") /* 1 */,
              GURL("https://ad_component.test/") /* 2 */,
              GURL("https://ad_component.test/") /* 3 */,
              GURL("https://ad_component.test/") /* 4 */,
              GURL("https://ad_component.test/") /* 5 */,
              GURL("https://ad_component.test/") /* 6 */,
              GURL("https://ad_component.test/") /* 7 */,
              GURL("https://ad_component.test/") /* 8 */,
              GURL("https://ad_component.test/") /* 9 */,
              GURL("https://ad_component.test/") /* 10 */,
              GURL("https://ad_component.test/") /* 11 */,
              GURL("https://ad_component.test/") /* 12 */,
              GURL("https://ad_component.test/") /* 13 */,
              GURL("https://ad_component.test/") /* 14 */,
              GURL("https://ad_component.test/") /* 15 */,
              GURL("https://ad_component.test/") /* 16 */,
              GURL("https://ad_component.test/") /* 17 */,
              GURL("https://ad_component.test/") /* 18 */,
              GURL("https://ad_component.test/") /* 19 */,
              GURL("https://ad_component.test/") /* 20 */,
          },
          base::TimeDelta()));

  // Results with 21 or more values are rejected.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad",
        bid:1,
        render:"https://response.test/",
        adComponents:[
          "https://ad_component.test/" /* 1 */,
          "https://ad_component.test/" /* 2 */,
          "https://ad_component.test/" /* 3 */,
          "https://ad_component.test/" /* 4 */,
          "https://ad_component.test/" /* 5 */,
          "https://ad_component.test/" /* 6 */,
          "https://ad_component.test/" /* 7 */,
          "https://ad_component.test/" /* 8 */,
          "https://ad_component.test/" /* 9 */,
          "https://ad_component.test/" /* 10 */,
          "https://ad_component.test/" /* 11 */,
          "https://ad_component.test/" /* 12 */,
          "https://ad_component.test/" /* 13 */,
          "https://ad_component.test/" /* 14 */,
          "https://ad_component.test/" /* 15 */,
          "https://ad_component.test/" /* 16 */,
          "https://ad_component.test/" /* 17 */,
          "https://ad_component.test/" /* 18 */,
          "https://ad_component.test/" /* 19 */,
          "https://ad_component.test/" /* 20 */,
          "https://ad_component.test/" /* 21 */,
        ]})",
      mojom::BidderWorkletBidPtr(),
      {"https://url.test/ generateBid() returned adComponents with over 20 "
       "items."});

  // ------------
  // Other cases.
  // ------------

  // No return value.
  RunGenerateBidWithReturnValueExpectingResult(
      "", mojom::BidderWorkletBidPtr() /* expected_bid */,
      {"https://url.test/ generateBid() return value not an object."});

  // Missing value.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({bid:"a", render:"https://response.test/"})",
      mojom::BidderWorkletBidPtr() /* expected_bid */,
      {"https://url.test/ generateBid() return value "
       "has incorrect structure."});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], render:"https://response.test/"})",
      mojom::BidderWorkletBidPtr() /* expected_bid */,
      {"https://url.test/ generateBid() return value "
       "has incorrect structure."});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:"a"})",
      mojom::BidderWorkletBidPtr() /* expected_bid */,
      {"https://url.test/ generateBid() return value "
       "has incorrect structure."});

  // Valid JS, but missing function.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(
        function someOtherFunction() {
          return {ad: ["ad"], bid:1, render:"https://response.test/"};
        }
      )",
      mojom::BidderWorkletBidPtr() /* expected_bid */,
      {"https://url.test/ `generateBid` is not a function."});
  RunGenerateBidWithJavascriptExpectingResult(
      "", mojom::BidderWorkletBidPtr() /* expected_bid */,
      {"https://url.test/ `generateBid` is not a function."});
  RunGenerateBidWithJavascriptExpectingResult(
      "5", mojom::BidderWorkletBidPtr() /* expected_bid */,
      {"https://url.test/ `generateBid` is not a function."});

  // Throw exception.
  RunGenerateBidWithJavascriptExpectingResult(
      "shrimp", mojom::BidderWorkletBidPtr() /* expected_bid */,
      {"https://url.test/:1 Uncaught ReferenceError: "
       "shrimp is not defined."});
}

// Make sure Date() is not available when running generateBid().
TEST_F(BidderWorkletTest, GenerateBidDateNotAvailable) {
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: Date().toString(), bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBidPtr() /* expected_bid */,
      {"https://url.test/:4 Uncaught ReferenceError: Date is not defined."});
}

TEST_F(BidderWorkletTest, GenerateBidLogAndError) {
  const char kScript[] = R"(
    function generateBid() {
      console.log("Logging");
      return "hello";
    }
  )";

  RunGenerateBidWithJavascriptExpectingResult(
      kScript, mojom::BidderWorkletBidPtr() /* expected_bid */,
      {"https://url.test/ [Log]: Logging",
       "https://url.test/ generateBid() return value not an object."});
}

TEST_F(BidderWorkletTest, GenerateBidInterestGroupName) {
  const std::string kGenerateBidBody =
      R"({ad: interestGroup.name, bid:1, render:"https://response.test/"})";

  interest_group_name_ = "foo";
  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          R"("foo")", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));

  interest_group_name_ = R"("foo")";
  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          R"("\"foo\"")", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));

  interest_group_name_ = "[1]";
  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          R"("[1]")", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));
}

// Test multiple GenerateBid calls on a single worklet, in parallel. Do this
// twice, once before the worklet has loaded its Javascript, and once after, to
// make sure both cases work.
TEST_F(BidderWorkletTest, GenerateBidParallel) {
  // Each GenerateBid call provides a different `auctionSignals` value. Use that
  // in the result for testing.
  const char kBidderScriptReturnValue[] = R"({
    ad: auctionSignals,
    bid: auctionSignals,
    render:"https://response.test/"
  })";

  auto bidder_worklet = CreateWorklet();

  // For the first loop iteration, call GenerateBid repeatedly and only then
  // provide the bidder script. For the second loop iteration, reuse the bidder
  // worklet from the first iteration, so the Javascript is loaded from the
  // start.
  for (bool generate_bid_invoked_before_worklet_script_loaded : {false, true}) {
    SCOPED_TRACE(generate_bid_invoked_before_worklet_script_loaded);

    base::RunLoop run_loop;
    const size_t kNumGenerateBidCalls = 10;
    size_t num_generate_bid_calls = 0;
    for (size_t i = 0; i < kNumGenerateBidCalls; ++i) {
      size_t bid_value = i + 1;
      bidder_worklet->GenerateBid(
          /*auction_signals_json=*/base::NumberToString(bid_value),
          per_buyer_signals_, browser_signal_top_window_origin_,
          browser_signal_seller_origin_, auction_start_time_,
          base::BindLambdaForTesting(
              [&run_loop, &num_generate_bid_calls, bid_value](
                  mojom::BidderWorkletBidPtr bid,
                  const std::vector<std::string>& errors) {
                EXPECT_EQ(bid_value, bid->bid);
                EXPECT_EQ(base::NumberToString(bid_value), bid->ad);
                EXPECT_EQ(GURL("https://response.test/"), bid->render_url);
                EXPECT_TRUE(errors.empty());
                ++num_generate_bid_calls;
                if (num_generate_bid_calls == kNumGenerateBidCalls)
                  run_loop.Quit();
              }));
    }

    // If this is the first loop iteration, wait for all the Mojo calls to
    // settle, and then provide the Javascript response body.
    if (generate_bid_invoked_before_worklet_script_loaded == false) {
      // Since the script hasn't loaded yet, no bids should be generated.
      task_environment_.RunUntilIdle();
      EXPECT_FALSE(run_loop.AnyQuitCalled());
      EXPECT_EQ(0u, num_generate_bid_calls);

      // Load script.
      AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                            CreateGenerateBidScript(kBidderScriptReturnValue));
    }

    run_loop.Run();
    EXPECT_EQ(kNumGenerateBidCalls, num_generate_bid_calls);
  }
}

// Test multiple GenerateBid calls on a single worklet, in parallel, in the case
// the script fails to load. Do this twice, once before the worklet has
// encountered a network error, and once after, to make sure both cases work.
TEST_F(BidderWorkletTest, GenerateBidNetworkErrorParallel) {
  auto bidder_worklet = CreateWorklet();

  // For the first loop iteration, call GenerateBid repeatedly and only then
  // provide the network error. For the second loop iteration, reuse the bidder
  // worklet from the first iteration, so the Javascript is loaded from the
  // start.
  for (bool generate_bid_invoked_before_network_error : {false, true}) {
    SCOPED_TRACE(generate_bid_invoked_before_network_error);

    base::RunLoop run_loop;
    const size_t kNumGenerateBidCalls = 10;
    size_t num_generate_bid_calls = 0;
    for (size_t i = 0; i < kNumGenerateBidCalls; ++i) {
      bidder_worklet->GenerateBid(
          auction_signals_, per_buyer_signals_,
          browser_signal_top_window_origin_, browser_signal_seller_origin_,
          auction_start_time_,
          base::BindLambdaForTesting(
              [&run_loop, &num_generate_bid_calls](
                  mojom::BidderWorkletBidPtr bid,
                  const std::vector<std::string>& errors) {
                EXPECT_FALSE(bid);
                EXPECT_EQ(errors, std::vector<std::string>{
                                      "Failed to load https://url.test/ HTTP "
                                      "status = 404 Not Found."});
                ++num_generate_bid_calls;
                if (num_generate_bid_calls == kNumGenerateBidCalls)
                  run_loop.Quit();
              }));
    }

    // If this is the first loop iteration, wait for all the Mojo calls to
    // settle, and then provide the network error.
    if (generate_bid_invoked_before_network_error == false) {
      // Since the script hasn't loaded yet, or failed to load, no bids should
      // be generated.
      task_environment_.RunUntilIdle();
      EXPECT_FALSE(run_loop.AnyQuitCalled());
      EXPECT_EQ(0u, num_generate_bid_calls);

      // Script fails to load.
      url_loader_factory_.AddResponse(interest_group_bidding_url_.spec(),
                                      CreateBasicGenerateBidScript(),
                                      net::HTTP_NOT_FOUND);
    }

    run_loop.Run();
    EXPECT_EQ(kNumGenerateBidCalls, num_generate_bid_calls);
  }
}

// Test multiple GenerateBid calls on a single worklet, in parallel, in the case
// there are trusted bidding signals.
//
// In this test, the ordering is:
// 1) GenerateBid() calls are made.
// 2) The worklet script load completes.
// 3) The trusted bidding signals are loaded.
TEST_F(BidderWorkletTest, GenerateBidTrustedBiddingSignalsParallel1) {
  interest_group_trusted_bidding_signals_url_ = GURL("https://signals.test/");
  interest_group_trusted_bidding_signals_keys_ =
      std::vector<std::string>({"key"});

  // Each GenerateBid call provides a different `auctionSignals` value. Use that
  // in the result for testing.
  const char kBidderScriptReturnValue[] = R"({
    ad: trustedBiddingSignals.key,
    bid: trustedBiddingSignals.key,
    render:"https://response.test/"
  })";

  auto bidder_worklet = CreateWorklet();

  // 1) GenerateBid() calls are made
  base::RunLoop run_loop;
  const size_t kNumGenerateBidCalls = 10;
  size_t num_generate_bid_calls = 0;
  for (size_t i = 0; i < kNumGenerateBidCalls; ++i) {
    size_t bid_value = i + 1;
    // Use a different origin for each GenerateBid call, since it's the only
    // GenerateBid parameter that affects the URL of bidding signals requests.
    url::Origin top_window_origin =
        url::Origin::Create(GURL(base::StringPrintf("https://%zu.test", i)));
    bidder_worklet->GenerateBid(
        auction_signals_, per_buyer_signals_, top_window_origin,
        browser_signal_seller_origin_, auction_start_time_,
        base::BindLambdaForTesting(
            [&run_loop, &num_generate_bid_calls, bid_value](
                mojom::BidderWorkletBidPtr bid,
                const std::vector<std::string>& errors) {
              EXPECT_EQ(bid_value, bid->bid);
              EXPECT_EQ(base::NumberToString(bid_value), bid->ad);
              EXPECT_EQ(GURL("https://response.test/"), bid->render_url);
              EXPECT_TRUE(errors.empty());
              ++num_generate_bid_calls;
              if (num_generate_bid_calls == kNumGenerateBidCalls)
                run_loop.Quit();
            }));
  }

  // Calling GenerateBid() shouldn't cause any callbacks to be invoked - the
  // BidderWorklet is waiting on both the trusted bidding signals and Javascript
  // responses from the network.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(run_loop.AnyQuitCalled());
  EXPECT_EQ(0u, num_generate_bid_calls);

  // 2) The worklet script load completes.
  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        CreateGenerateBidScript(kBidderScriptReturnValue));
  // No callbacks are invoked, as the BidderWorklet is still waiting on the
  // trusted bidding signals responses.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(run_loop.AnyQuitCalled());
  EXPECT_EQ(0u, num_generate_bid_calls);

  // 3) The trusted bidding signals are loaded.
  for (size_t i = 0; i < kNumGenerateBidCalls; ++i) {
    AddJsonResponse(&url_loader_factory_,
                    GURL(base::StringPrintf(
                        "https://signals.test/?hostname=%zu.test&keys=key", i)),
                    base::StringPrintf(R"({"key":%zu})", i + 1));
  }

  // The worklets can now generate bids.
  run_loop.Run();
  EXPECT_EQ(kNumGenerateBidCalls, num_generate_bid_calls);
}

// Test multiple GenerateBid calls on a single worklet, in parallel, in the case
// there are trusted bidding signals.
//
// In this test, the ordering is:
// 1) GenerateBid() calls are made
// 2) The trusted bidding signals are loaded.
// 3) The worklet script load completes.
TEST_F(BidderWorkletTest, GenerateBidTrustedBiddingSignalsParallel2) {
  interest_group_trusted_bidding_signals_url_ = GURL("https://signals.test/");
  interest_group_trusted_bidding_signals_keys_ =
      std::vector<std::string>({"key"});

  // Each GenerateBid call provides a different `auctionSignals` value. Use that
  // in the result for testing.
  const char kBidderScriptReturnValue[] = R"({
    ad: trustedBiddingSignals.key,
    bid: trustedBiddingSignals.key,
    render:"https://response.test/"
  })";

  auto bidder_worklet = CreateWorklet();

  // 1) GenerateBid() calls are made
  base::RunLoop run_loop;
  const size_t kNumGenerateBidCalls = 10;
  size_t num_generate_bid_calls = 0;
  for (size_t i = 0; i < kNumGenerateBidCalls; ++i) {
    size_t bid_value = i + 1;
    // Use a different origin for each GenerateBid call, since it's the only
    // GenerateBid parameter that affects the URL of bidding signals requests.
    url::Origin top_window_origin =
        url::Origin::Create(GURL(base::StringPrintf("https://%zu.test", i)));
    bidder_worklet->GenerateBid(
        auction_signals_, per_buyer_signals_, top_window_origin,
        browser_signal_seller_origin_, auction_start_time_,
        base::BindLambdaForTesting(
            [&run_loop, &num_generate_bid_calls, bid_value](
                mojom::BidderWorkletBidPtr bid,
                const std::vector<std::string>& errors) {
              EXPECT_EQ(bid_value, bid->bid);
              EXPECT_EQ(base::NumberToString(bid_value), bid->ad);
              EXPECT_EQ(GURL("https://response.test/"), bid->render_url);
              EXPECT_TRUE(errors.empty());
              ++num_generate_bid_calls;
              if (num_generate_bid_calls == kNumGenerateBidCalls)
                run_loop.Quit();
            }));
  }

  // Calling GenerateBid() shouldn't cause any callbacks to be invoked - the
  // BidderWorklet is waiting on both the trusted bidding signals and Javascript
  // responses from the network.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(run_loop.AnyQuitCalled());
  EXPECT_EQ(0u, num_generate_bid_calls);

  // 2) The trusted bidding signals are loaded.
  for (size_t i = 0; i < kNumGenerateBidCalls; ++i) {
    AddJsonResponse(&url_loader_factory_,
                    GURL(base::StringPrintf(
                        "https://signals.test/?hostname=%zu.test&keys=key", i)),
                    base::StringPrintf(R"({"key":%zu})", i + 1));
  }

  // No callbacks should have been invoked, since the worklet script hasn't
  // loaded yet.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(run_loop.AnyQuitCalled());
  EXPECT_EQ(0u, num_generate_bid_calls);

  // 3) The worklet script load completes.
  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        CreateGenerateBidScript(kBidderScriptReturnValue));

  // The worklets can now generate bids.
  run_loop.Run();
  EXPECT_EQ(kNumGenerateBidCalls, num_generate_bid_calls);
}

// Test multiple GenerateBid calls on a single worklet, in parallel, in the case
// there are trusted bidding signals.
//
// In this test, the ordering is:
// 1) The worklet script load completes.
// 2) GenerateBid() calls are made.
// 3) The trusted bidding signals are loaded.
TEST_F(BidderWorkletTest, GenerateBidTrustedBiddingSignalsParallel3) {
  interest_group_trusted_bidding_signals_url_ = GURL("https://signals.test/");
  interest_group_trusted_bidding_signals_keys_ =
      std::vector<std::string>({"key"});

  // Each GenerateBid call provides a different `auctionSignals` value. Use that
  // in the result for testing.
  const char kBidderScriptReturnValue[] = R"({
    ad: trustedBiddingSignals.key,
    bid: trustedBiddingSignals.key,
    render:"https://response.test/"
  })";

  auto bidder_worklet = CreateWorklet();

  // 1) The worklet script load completes.
  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        CreateGenerateBidScript(kBidderScriptReturnValue));
  task_environment_.RunUntilIdle();

  // 2) GenerateBid() calls are made.
  base::RunLoop run_loop;
  const size_t kNumGenerateBidCalls = 10;
  size_t num_generate_bid_calls = 0;
  for (size_t i = 0; i < kNumGenerateBidCalls; ++i) {
    size_t bid_value = i + 1;
    // Use a different origin for each GenerateBid call, since it's the only
    // GenerateBid parameter that affects the URL of bidding signals requests.
    url::Origin top_window_origin =
        url::Origin::Create(GURL(base::StringPrintf("https://%zu.test", i)));
    bidder_worklet->GenerateBid(
        auction_signals_, per_buyer_signals_, top_window_origin,
        browser_signal_seller_origin_, auction_start_time_,
        base::BindLambdaForTesting(
            [&run_loop, &num_generate_bid_calls, bid_value](
                mojom::BidderWorkletBidPtr bid,
                const std::vector<std::string>& errors) {
              EXPECT_EQ(bid_value, bid->bid);
              EXPECT_EQ(base::NumberToString(bid_value), bid->ad);
              EXPECT_EQ(GURL("https://response.test/"), bid->render_url);
              EXPECT_TRUE(errors.empty());
              ++num_generate_bid_calls;
              if (num_generate_bid_calls == kNumGenerateBidCalls)
                run_loop.Quit();
            }));
  }

  // No callbacks should have been invoked yet, since the trusted bidding
  // signals haven't loaded yet.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(run_loop.AnyQuitCalled());
  EXPECT_EQ(0u, num_generate_bid_calls);

  // 3) The trusted bidding signals are loaded.
  for (size_t i = 0; i < kNumGenerateBidCalls; ++i) {
    AddJsonResponse(&url_loader_factory_,
                    GURL(base::StringPrintf(
                        "https://signals.test/?hostname=%zu.test&keys=key", i)),
                    base::StringPrintf(R"({"key":%zu})", i + 1));
  }

  // The worklets can now generate bids.
  run_loop.Run();
  EXPECT_EQ(num_generate_bid_calls, kNumGenerateBidCalls);
}

TEST_F(BidderWorkletTest, GenerateBidInterestGroupUserBiddingSignals) {
  const std::string kGenerateBidBody =
      R"({ad: interestGroup.userBiddingSignals, bid:1, render:"https://response.test/"})";

  // Since UserBiddingSignals are in JSON, non-JSON strings should result in
  // failures.
  interest_group_user_bidding_signals_ = "foo";
  RunGenerateBidWithReturnValueExpectingResult(kGenerateBidBody,
                                               mojom::BidderWorkletBidPtr());

  interest_group_user_bidding_signals_ = R"("foo")";
  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          R"("foo")", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));

  interest_group_user_bidding_signals_ = "[1]";
  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New("[1]", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()));

  interest_group_user_bidding_signals_ = absl::nullopt;
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: interestGroup.userBiddingSignals === undefined, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New("true", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()));
}

TEST_F(BidderWorkletTest, GenerateBidAuctionSignals) {
  const std::string kGenerateBidBody =
      R"({ad: auctionSignals, bid:1, render:"https://response.test/"})";

  // Since AuctionSignals are in JSON, non-JSON strings should result in
  // failures.
  auction_signals_ = "foo";
  RunGenerateBidWithReturnValueExpectingResult(kGenerateBidBody,
                                               mojom::BidderWorkletBidPtr());

  auction_signals_ = R"("foo")";
  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          R"("foo")", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));

  auction_signals_ = "[1]";
  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New("[1]", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()));
}

TEST_F(BidderWorkletTest, GenerateBidPerBuyerSignals) {
  const std::string kGenerateBidBody =
      R"({ad: perBuyerSignals, bid:1, render:"https://response.test/"})";

  // Since AuctionSignals are in JSON, non-JSON strings should result in
  // failures.
  per_buyer_signals_ = "foo";
  RunGenerateBidWithReturnValueExpectingResult(kGenerateBidBody,
                                               mojom::BidderWorkletBidPtr());

  per_buyer_signals_ = R"("foo")";
  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          R"("foo")", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));

  per_buyer_signals_ = "[1]";
  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New("[1]", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()));

  per_buyer_signals_ = absl::nullopt;
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: perBuyerSignals === null, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New("true", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()));
}

TEST_F(BidderWorkletTest, GenerateBidInterestGroupOwner) {
  interest_group_owner_ = url::Origin::Create(GURL("https://foo.test/"));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: interestGroup.owner, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          R"("https://foo.test")", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));

  interest_group_owner_ = url::Origin::Create(GURL("https://[::1]:40000/"));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: interestGroup.owner, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          R"("https://[::1]:40000")", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));
}

TEST_F(BidderWorkletTest, GenerateBidBrowserSignalSellerOrigin) {
  browser_signal_seller_origin_ =
      url::Origin::Create(GURL("https://foo.test/"));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: browserSignals.seller, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          R"("https://foo.test")", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));

  browser_signal_seller_origin_ =
      url::Origin::Create(GURL("https://[::1]:40000/"));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: browserSignals.seller, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          R"("https://[::1]:40000")", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));
}

TEST_F(BidderWorkletTest, GenerateBidBrowserSignalTopWindowOrigin) {
  browser_signal_top_window_origin_ =
      url::Origin::Create(GURL("https://top.window.test/"));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: browserSignals.topWindowHostname, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          R"("top.window.test")", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));
}

TEST_F(BidderWorkletTest, GenerateBidBrowserSignalJoinCountBidCount) {
  const struct IntegerTestCase {
    // String used in JS to access the parameter.
    const char* name;
    // Pointer to location at which the integer can be modified.
    raw_ptr<int> value_ptr;
  } kIntegerTestCases[] = {
      {"browserSignals.joinCount", &browser_signal_join_count_},
      {"browserSignals.bidCount", &browser_signal_bid_count_}};

  for (const auto& test_case : kIntegerTestCases) {
    SCOPED_TRACE(test_case.name);

    *test_case.value_ptr = 0;
    RunGenerateBidWithReturnValueExpectingResult(
        base::StringPrintf(
            R"({ad: %s, bid:1, render:"https://response.test/"})",
            test_case.name),
        mojom::BidderWorkletBid::New("0", 1, GURL("https://response.test/"),
                                     /*ad_components=*/absl::nullopt,
                                     base::TimeDelta()));

    *test_case.value_ptr = 10;
    RunGenerateBidWithReturnValueExpectingResult(
        base::StringPrintf(
            R"({ad: %s, bid:1, render:"https://response.test/"})",
            test_case.name),
        mojom::BidderWorkletBid::New("10", 1, GURL("https://response.test/"),
                                     /*ad_components=*/absl::nullopt,
                                     base::TimeDelta()));
    SetDefaultParameters();
  }
}

TEST_F(BidderWorkletTest, GenerateBidAds) {
  // A bid URL that's not in the InterestGroup's ads list should fail.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: 0, bid:1, render:"https://response2.test/"})",
      mojom::BidderWorkletBidPtr() /* expected_bid */,
      {"https://url.test/ generateBid() returned render URL that isn't one of "
       "the registered creative URLs."});

  // Adding an ad with a corresponding `renderUrl` should result in success.
  // Also check the `interestGroup.ads` field passed to Javascript.
  interest_group_ads_.emplace_back(blink::InterestGroup::Ad(
      GURL("https://response2.test/"), R"(["metadata"])" /* metadata */));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: interestGroup.ads, bid:1, render:"https://response2.test/"})",
      mojom::BidderWorkletBid::New(
          "[{\"renderUrl\":\"https://response.test/\"},"
          "{\"renderUrl\":\"https://response2.test/"
          "\",\"metadata\":[\"metadata\"]}]",
          1, GURL("https://response2.test/"), /*ad_components=*/absl::nullopt,
          base::TimeDelta()));

  // Make sure `metadata` is treated as an object, instead of a raw string.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: interestGroup.ads[1].metadata[0], bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          "\"metadata\"", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));
}

TEST_F(BidderWorkletTest, GenerateBidAdComponents) {
  // Basic test with an adComponent URL.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: 0, bid:1, render:"https://response.test/", adComponents:["https://ad_component.test/"]})",
      mojom::BidderWorkletBid::New(
          "0", 1, GURL("https://response.test/"),
          std::vector<GURL>{GURL("https://ad_component.test/")},
          base::TimeDelta()));
  // Empty, but non-null, adComponents field.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: 0, bid:1, render:"https://response.test/", adComponents:[]})",
      mojom::BidderWorkletBid::New("0", 1, GURL("https://response.test/"),
                                   std::vector<GURL>(), base::TimeDelta()));

  // An adComponent URL that's not in the InterestGroup's adComponents list
  // should fail.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: 0, bid:1, render:"https://response.test/", adComponents:["https://response.test/"]})",
      mojom::BidderWorkletBidPtr() /* expected_bid */,
      {"https://url.test/ generateBid() returned adComponents URL that isn't "
       "one "
       "of the registered creative URLs."});

  // Add a second ad component URL, this time with metadata.
  // Returning a list with both ads should result in success.
  // Also check the `interestGroup.ads` field passed to Javascript.
  interest_group_ad_components_->emplace_back(blink::InterestGroup::Ad(
      GURL("https://ad_component2.test/"), /*metadata=*/R"(["metadata"])"));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: interestGroup.adComponents,
        bid:1,
        render:"https://response.test/",
        adComponents:["https://ad_component.test/", "https://ad_component2.test/"]})",
      mojom::BidderWorkletBid::New(
          "[{\"renderUrl\":\"https://ad_component.test/\"},"
          "{\"renderUrl\":\"https://ad_component2.test/"
          "\",\"metadata\":[\"metadata\"]}]",
          1, GURL("https://response.test/"),
          std::vector<GURL>{GURL("https://ad_component.test/"),
                            GURL("https://ad_component2.test/")},
          base::TimeDelta()));
}

// Utility method to create a vector of PreviousWin. Needed because StructPtrs
// don't allow copying.
std::vector<mojom::PreviousWinPtr> CreateWinList(
    const mojom::PreviousWinPtr& win1,
    const mojom::PreviousWinPtr& win2 = mojom::PreviousWinPtr(),
    const mojom::PreviousWinPtr& win3 = mojom::PreviousWinPtr()) {
  std::vector<mojo::StructPtr<mojom::PreviousWin>> out;
  out.emplace_back(win1.Clone());
  if (win2)
    out.emplace_back(win2.Clone());
  if (win3)
    out.emplace_back(win3.Clone());
  return out;
}

TEST_F(BidderWorkletTest, GenerateBidPrevWins) {
  base::TimeDelta delta = base::Seconds(100);
  base::TimeDelta tiny_delta = base::Milliseconds(500);

  base::Time time1 = auction_start_time_ - delta - delta;
  base::Time time2 = auction_start_time_ - delta - tiny_delta;
  base::Time future_time = auction_start_time_ + delta;

  auto win1 = mojom::PreviousWin::New(time1, R"("ad1")");
  auto win2 = mojom::PreviousWin::New(time2, R"(["ad2"])");
  auto future_win = mojom::PreviousWin::New(future_time, R"("future_ad")");
  struct TestCase {
    std::vector<mojo::StructPtr<mojom::PreviousWin>> prev_wins;
    // Value to output as the ad data.
    const char* ad;
    // Expected output in the `ad` field of the result.
    const char* expected_ad;
  } test_cases[] = {
      {
          {},
          "browserSignals.prevWins",
          "[]",
      },
      {
          CreateWinList(win1),
          "browserSignals.prevWins",
          R"([[200,"ad1"]])",
      },
      // Make sure it's passed on as an object and not a string.
      {
          CreateWinList(win1),
          "browserSignals.prevWins[0]",
          R"([200,"ad1"])",
      },
      // Test rounding.
      {
          CreateWinList(win2),
          "browserSignals.prevWins",
          R"([[100,["ad2"]]])",
      },
      // Multiple previous wins.
      {
          CreateWinList(win1, win2),
          "browserSignals.prevWins",
          R"([[200,"ad1"],[100,["ad2"]]])",
      },
      // Times are trimmed at 0.
      {
          CreateWinList(future_win),
          "browserSignals.prevWins",
          R"([[0,"future_ad"]])",
      },
      // Out of order wins should be sorted.
      {
          CreateWinList(win2, future_win, win1),
          "browserSignals.prevWins",
          R"([[200,"ad1"],[100,["ad2"]],[0,"future_ad"]])",
      },
  };

  for (auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.ad);
    // StructPtrs aren't copiable, so this effectively destroys each test case.
    browser_signal_prev_wins_ = std::move(test_case.prev_wins);
    RunGenerateBidWithReturnValueExpectingResult(
        base::StringPrintf(
            R"({ad: %s, bid:1, render:"https://response.test/"})",
            test_case.ad),
        mojom::BidderWorkletBid::New(
            test_case.expected_ad, 1, GURL("https://response.test/"),
            /*ad_components=*/absl::nullopt, base::TimeDelta()));
  }
}

TEST_F(BidderWorkletTest, GenerateBidTrustedBiddingSignals) {
  const GURL kBaseSignalsUrl("https://signals.test/");
  const GURL kFullSignalsUrl(
      "https://signals.test/?hostname=top.window.test&keys=key1,key2");

  const char kJson[] = R"(
    {
      "key1": 1,
      "key2": [2]
    }
  )";

  // Request with null TrustedBiddingSignals keys and URL. No request should be
  // made.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: trustedBiddingSignals, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New("null", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()));

  // Request with TrustedBiddingSignals keys and null URL. No request should be
  // made.
  interest_group_trusted_bidding_signals_keys_ =
      std::vector<std::string>({"key1", "key2"});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: trustedBiddingSignals, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New("null", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()));

  // Request with TrustedBiddingSignals URL and null keys. No request should be
  // made.
  interest_group_trusted_bidding_signals_url_ = kBaseSignalsUrl;
  interest_group_trusted_bidding_signals_keys_.reset();
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: trustedBiddingSignals, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New("null", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()));

  // Request with TrustedBiddingSignals URL and empty keys. No request should be
  // made.
  interest_group_trusted_bidding_signals_keys_ = std::vector<std::string>();
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: trustedBiddingSignals, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New("null", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()));

  // Request with valid TrustedBiddingSignals URL and non-empty keys. Request
  // should be made. The request fails.
  interest_group_trusted_bidding_signals_keys_ =
      std::vector<std::string>({"key1", "key2"});
  url_loader_factory_.AddResponse(kFullSignalsUrl.spec(), kJson,
                                  net::HTTP_NOT_FOUND);
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: trustedBiddingSignals, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New("null", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()),
      {"Failed to load "
       "https://signals.test/?hostname=top.window.test&keys=key1,key2 HTTP "
       "status = 404 Not Found."});

  // Request with valid TrustedBiddingSignals URL and non-empty keys. Request
  // should be made. The request succeeds.
  AddJsonResponse(&url_loader_factory_, kFullSignalsUrl, kJson);
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: trustedBiddingSignals, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          R"({"key1":1,"key2":[2]})", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));
}

TEST_F(BidderWorkletTest, ReportWin) {
  RunReportWinWithFunctionBodyExpectingResult(
      "", absl::nullopt /* expected_report_url */);
  RunReportWinWithFunctionBodyExpectingResult(
      R"(return "https://ignored.test/")",
      absl::nullopt /* expected_report_url */);

  RunReportWinWithFunctionBodyExpectingResult(
      R"(sendReportTo("https://foo.test"))", GURL("https://foo.test/"));
  RunReportWinWithFunctionBodyExpectingResult(
      R"(sendReportTo("https://foo.test/bar"))", GURL("https://foo.test/bar"));

  RunReportWinWithFunctionBodyExpectingResult(
      R"(sendReportTo("http://http.not.allowed.test"))",
      absl::nullopt /* expected_report_url */,
      {"https://url.test/:9 Uncaught TypeError: sendReportTo must be passed a "
       "valid HTTPS url."});
  RunReportWinWithFunctionBodyExpectingResult(
      R"(sendReportTo("file:///file.not.allowed.test"))",
      absl::nullopt /* expected_report_url */,
      {"https://url.test/:9 Uncaught TypeError: sendReportTo must be passed a "
       "valid HTTPS url."});

  RunReportWinWithFunctionBodyExpectingResult(
      R"(sendReportTo(""))", absl::nullopt /* expected_report_url */,
      {"https://url.test/:9 Uncaught TypeError: sendReportTo must be passed a "
       "valid HTTPS url."});

  RunReportWinWithFunctionBodyExpectingResult(
      R"(sendReportTo("https://foo.test");sendReportTo("https://foo.test"))",
      absl::nullopt /* expected_report_url */,
      {"https://url.test/:9 Uncaught TypeError: sendReportTo may be called at "
       "most once."});
}

TEST_F(BidderWorkletTest, DeleteBeforeReportWinCallback) {
  AddJavascriptResponse(
      &url_loader_factory_, interest_group_bidding_url_,
      CreateReportWinScript(R"(sendReportTo("https://foo.test"))"));
  auto bidder_worklet = CreateWorklet();
  ASSERT_TRUE(bidder_worklet);

  base::WaitableEvent* event_handle = WedgeV8Thread(v8_helper_.get());
  bidder_worklet->ReportWin(
      auction_signals_, per_buyer_signals_, browser_signal_top_window_origin_,
      seller_signals_, browser_signal_render_url_, browser_signal_bid_,
      base::BindOnce([](const absl::optional<GURL>& report_url,
                        const std::vector<std::string>& errors) {
        ADD_FAILURE() << "Callback should not be invoked since worklet deleted";
      }));
  base::RunLoop().RunUntilIdle();
  bidder_worklet.reset();
  event_handle->Signal();
}

// Test multiple ReportWin calls on a single worklet, in parallel. Do this
// twice, once before the worklet has loaded its Javascript, and once after, to
// make sure both cases work.
TEST_F(BidderWorkletTest, ReportWinParallel) {
  // Each ReportWin call provides a different `auctionSignals` value. Use that
  // in the report to verify that each call's values are plumbed through
  // correctly.
  const char kReportWinScript[] =
      R"(sendReportTo("https://foo.test/" + auctionSignals))";

  auto bidder_worklet = CreateWorklet();

  // For the first loop iteration, call ReportWin repeatedly before providing
  // the bidder script, then provide the bidder script. For the second loop
  // iteration, reuse the bidder worklet from the first iteration, so the
  // Javascript is loaded from the start.
  for (bool report_win_invoked_before_worklet_script_loaded : {false, true}) {
    SCOPED_TRACE(report_win_invoked_before_worklet_script_loaded);

    base::RunLoop run_loop;
    const size_t kNumReportWinCalls = 10;
    size_t num_report_win_calls = 0;
    for (size_t i = 0; i < kNumReportWinCalls; ++i) {
      bidder_worklet->ReportWin(
          /*auction_signals_json=*/base::NumberToString(i), per_buyer_signals_,
          browser_signal_top_window_origin_, seller_signals_,
          browser_signal_render_url_, browser_signal_bid_,
          base::BindLambdaForTesting(
              [&run_loop, &num_report_win_calls, i](
                  const absl::optional<GURL>& report_url,
                  const std::vector<std::string>& errors) {
                EXPECT_EQ(GURL(base::StringPrintf("https://foo.test/%zu", i)),
                          report_url);
                EXPECT_TRUE(errors.empty());
                ++num_report_win_calls;
                if (num_report_win_calls == kNumReportWinCalls)
                  run_loop.Quit();
              }));
    }

    // If this is the first loop iteration, wait for all the Mojo calls to
    // settle, and then provide the Javascript response body.
    if (report_win_invoked_before_worklet_script_loaded == false) {
      task_environment_.RunUntilIdle();
      EXPECT_FALSE(run_loop.AnyQuitCalled());
      AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                            CreateReportWinScript(kReportWinScript));
    }

    run_loop.Run();
    EXPECT_EQ(kNumReportWinCalls, num_report_win_calls);
  }
}

// Test multiple ReportWin calls on a single worklet, in parallel, in the case
// the worklet script fails to load. Do this twice, once before the worklet
// script has received an error, and once after, to make sure both cases work.
TEST_F(BidderWorkletTest, ReportWinNetworkErrorParallel) {
  auto bidder_worklet = CreateWorklet();

  // For the first loop iteration, call ReportWin repeatedly before providing
  // the network error script, then provide the network error. For the second
  // loop iteration, reuse the bidder worklet from the first iteration, so the
  // network error is present from the start.
  for (bool report_win_invoked_before_worklet_script_loaded : {false, true}) {
    SCOPED_TRACE(report_win_invoked_before_worklet_script_loaded);

    base::RunLoop run_loop;
    const size_t kNumReportWinCalls = 10;
    size_t num_report_win_calls = 0;
    for (size_t i = 0; i < kNumReportWinCalls; ++i) {
      bidder_worklet->ReportWin(
          /*auction_signals_json=*/base::NumberToString(i), per_buyer_signals_,
          browser_signal_top_window_origin_, seller_signals_,
          browser_signal_render_url_, browser_signal_bid_,
          base::BindLambdaForTesting(
              [&run_loop, &num_report_win_calls](
                  const absl::optional<GURL>& report_url,
                  const std::vector<std::string>& errors) {
                EXPECT_FALSE(report_url);
                EXPECT_EQ(errors, std::vector<std::string>{
                                      "Failed to load https://url.test/ "
                                      "HTTP status = 404 Not Found."});
                ++num_report_win_calls;
                if (num_report_win_calls == kNumReportWinCalls)
                  run_loop.Quit();
              }));
    }

    // If this is the first loop iteration, wait for all the Mojo calls to
    // settle, and then provide the Javascript response body.
    if (report_win_invoked_before_worklet_script_loaded == false) {
      task_environment_.RunUntilIdle();
      EXPECT_FALSE(run_loop.AnyQuitCalled());
      url_loader_factory_.AddResponse(interest_group_bidding_url_.spec(),
                                      CreateBasicGenerateBidScript(),
                                      net::HTTP_NOT_FOUND);
    }

    run_loop.Run();
    EXPECT_EQ(kNumReportWinCalls, num_report_win_calls);
  }
}

// Make sure Date() is not available when running reportWin().
TEST_F(BidderWorkletTest, ReportWinDateNotAvailable) {
  RunReportWinWithFunctionBodyExpectingResult(
      R"(sendReportTo("https://foo.test/" + Date().toString()))",
      absl::nullopt /* expected_report_url */,
      {"https://url.test/:9 Uncaught ReferenceError: Date is not defined."});
}

TEST_F(BidderWorkletTest, ReportWinInterestGroupName) {
  interest_group_name_ = "https://interest.group.name.test/";
  RunReportWinWithFunctionBodyExpectingResult(
      "sendReportTo(browserSignals.interestGroupName)",
      GURL(interest_group_name_));
}

TEST_F(BidderWorkletTest, ReportWinAuctionSignals) {
  // Non-JSON strings should silently result in failure generating the bid,
  // before the result can be scored.
  auction_signals_ = "https://interest.group.name.test/";
  RunGenerateBidWithJavascriptExpectingResult(CreateBasicGenerateBidScript(),
                                              mojom::BidderWorkletBidPtr());

  auction_signals_ = R"("https://interest.group.name.test/")";
  RunReportWinWithFunctionBodyExpectingResult(
      "sendReportTo(auctionSignals)",
      GURL("https://interest.group.name.test/"));

  auction_signals_ = absl::nullopt;
  RunReportWinWithFunctionBodyExpectingResult(
      R"(sendReportTo("https://" + (auctionSignals === null)))",
      GURL("https://true/"));
}

TEST_F(BidderWorkletTest, ReportWinPerBuyerSignals) {
  // Non-JSON strings should silently result in failure generating the bid,
  // before the result can be scored.
  per_buyer_signals_ = "https://interest.group.name.test/";
  RunGenerateBidWithJavascriptExpectingResult(CreateBasicGenerateBidScript(),
                                              mojom::BidderWorkletBidPtr());

  per_buyer_signals_ = R"("https://interest.group.name.test/")";
  RunReportWinWithFunctionBodyExpectingResult(
      "sendReportTo(perBuyerSignals)",
      GURL("https://interest.group.name.test/"));

  per_buyer_signals_ = absl::nullopt;
  RunReportWinWithFunctionBodyExpectingResult(
      R"(sendReportTo("https://" + (perBuyerSignals === null)))",
      GURL("https://true/"));
}

TEST_F(BidderWorkletTest, ReportWinSellerSignals) {
  // Non-JSON values should silently result in failures. This shouldn't happen,
  // except in the case of a compromised seller worklet process, so not worth
  // having an error message.
  seller_signals_ = "https://interest.group.name.test/";
  RunReportWinWithFunctionBodyExpectingResult(
      "sendReportTo(sellerSignals)",
      /*expected_report_url=*/absl::nullopt);

  seller_signals_ = R"("https://interest.group.name.test/")";
  RunReportWinWithFunctionBodyExpectingResult(
      "sendReportTo(sellerSignals)", GURL("https://interest.group.name.test/"));
}

TEST_F(BidderWorkletTest, ReportWinInterestGroupOwner) {
  interest_group_owner_ = url::Origin::Create(GURL("https://foo.test/"));
  // Add an extra ".test" because origin's shouldn't have a terminal slash,
  // unlike URLs. If an extra slash were added to the origin, this would end up
  // as https://foo.test/.test, instead.
  RunReportWinWithFunctionBodyExpectingResult(
      R"(sendReportTo(browserSignals.interestGroupOwner + ".test"))",
      GURL("https://foo.test.test/"));

  interest_group_owner_ = url::Origin::Create(GURL("https://[::1]:40000/"));
  RunReportWinWithFunctionBodyExpectingResult(
      R"(sendReportTo(browserSignals.interestGroupOwner))",
      GURL("https://[::1]:40000/"));
}

TEST_F(BidderWorkletTest, ReportWinBrowserSignalTopWindowOrigin) {
  browser_signal_top_window_origin_ =
      url::Origin::Create(GURL("https://top.window.test/"));
  RunReportWinWithFunctionBodyExpectingResult(
      R"(sendReportTo("https://" + browserSignals.topWindowHostname))",
      GURL("https://top.window.test/"));
}

TEST_F(BidderWorkletTest, ReportWinBrowserSignalRenderUrl) {
  browser_signal_render_url_ = GURL("https://shrimp.test/");
  RunReportWinWithFunctionBodyExpectingResult(
      "sendReportTo(browserSignals.renderUrl)", browser_signal_render_url_);
}

TEST_F(BidderWorkletTest, ReportWinBrowserSignalBid) {
  browser_signal_bid_ = 4;
  RunReportWinWithFunctionBodyExpectingResult(
      R"(if (browserSignals.bid === 4)
        sendReportTo("https://jumboshrimp.test"))",
      GURL("https://jumboshrimp.test"));
}

// Subsequent runs of the same script should not affect each other. Same is true
// for different scripts, but it follows from the single script case.
//
// TODO(mmenke): The current API only allows each generateBid() method to be
// called once, but each ReportWin() to be called multiple times. When the API
// is updated to allow multiple calls to generateBid(), update this method to
// invoke it multiple times.
TEST_F(BidderWorkletTest, ScriptIsolation) {
  // Use arrays so that all values are references, to catch both the case where
  // variables are persisted, and the case where what they refer to is
  // persisted, but variables are overwritten between runs.
  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        R"(
        // Globally scoped variable.
        if (!globalThis.var1)
          globalThis.var1 = [1];
        generateBid = function() {
          // Value only visible within this closure.
          var var2 = [2];
          return function() {
            return {
                // Expose values in "ad" object.
                ad: [++globalThis.var1[0], ++var2[0]],
                bid: 1,
                render:"https://response.test/"
            }
          };
        }();

        function reportWin() {
          // Reuse generateBid() to check same potential cases for leaks between
          // successive calls.
          var ad = generateBid().ad;
          sendReportTo("https://" + ad[0] + ad[1] + ".test/");
        }
      )");
  auto bidder_worklet = CreateWorkletAndGenerateBid();
  ASSERT_TRUE(bidder_worklet);

  for (int i = 0; i < 3; ++i) {
    base::RunLoop run_loop;
    bidder_worklet->ReportWin(
        auction_signals_, per_buyer_signals_, browser_signal_top_window_origin_,
        seller_signals_, browser_signal_render_url_, browser_signal_bid_,
        base::BindLambdaForTesting(
            [&run_loop](const absl::optional<GURL>& report_url,
                        const std::vector<std::string>& errors) {
              EXPECT_EQ(GURL("https://23.test/"), report_url);
              EXPECT_TRUE(errors.empty());
              run_loop.Quit();
            }));
    run_loop.Run();
  }
}

TEST_F(BidderWorkletTest, PauseOnStart) {
  // If pause isn't working, this will be used and not the right script.
  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        "nonsense;");

  // No trusted signals to simplify spying on URL loading.
  interest_group_trusted_bidding_signals_keys_.reset();

  BidderWorklet* worklet_impl = nullptr;
  auto worklet =
      CreateWorklet(interest_group_bidding_url_,
                    /* pause_for_debugger_on_start=*/true, &worklet_impl);
  GenerateBid(worklet.get());
  // Grab the context group ID to be able to resume.
  int id = worklet_impl->context_group_id_for_testing();

  // Give it a chance to fetch.
  task_environment_.RunUntilIdle();

  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        CreateBasicGenerateBidScript());

  // Set up the event loop for the standard callback.
  load_script_run_loop_ = std::make_unique<base::RunLoop>();

  // Let this run.
  v8_helper_->v8_runner()->PostTask(
      FROM_HERE, base::BindOnce([](scoped_refptr<AuctionV8Helper> v8_helper,
                                   int id) { v8_helper->Resume(id); },
                                v8_helper_, id));

  load_script_run_loop_->Run();
  load_script_run_loop_.reset();

  ASSERT_TRUE(bid_);
  EXPECT_EQ("[\"ad\"]", bid_->ad);
  EXPECT_EQ(1, bid_->bid);
  EXPECT_EQ(GURL("https://response.test/"), bid_->render_url);
  EXPECT_THAT(bid_errors_, ::testing::UnorderedElementsAre());
}

TEST_F(BidderWorkletTest, PauseOnStartDelete) {
  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        CreateBasicGenerateBidScript());
  // No trusted signals to simplify things.
  interest_group_trusted_bidding_signals_keys_.reset();

  BidderWorklet* worklet_impl = nullptr;
  auto worklet =
      CreateWorklet(interest_group_bidding_url_,
                    /*pause_for_debugger_on_start=*/true, &worklet_impl);
  GenerateBid(worklet.get());

  // Give it a chance to fetch.
  task_environment_.RunUntilIdle();

  // Grab the context group ID.
  int id = worklet_impl->context_group_id_for_testing();

  // Delete the worklet. No callback should be invoked.
  worklet.reset();
  task_environment_.RunUntilIdle();

  // Try to resume post-delete. Should not crash.
  v8_helper_->v8_runner()->PostTask(
      FROM_HERE, base::BindOnce([](scoped_refptr<AuctionV8Helper> v8_helper,
                                   int id) { v8_helper->Resume(id); },
                                v8_helper_, id));
  task_environment_.RunUntilIdle();
}

TEST_F(BidderWorkletTest, BasicV8Debug) {
  ScopedInspectorSupport inspector_support(v8_helper_.get());

  // Helper for looking for scriptParsed events.
  auto is_script_parsed = [](const TestChannel::Event& event) -> bool {
    if (event.type != TestChannel::Event::Type::Notification)
      return false;

    const std::string* candidate_method = event.value.FindStringKey("method");
    return (candidate_method && *candidate_method == "Debugger.scriptParsed");
  };

  const char kUrl1[] = "http://example.com/first.js";
  const char kUrl2[] = "http://example.org/second.js";

  AddJavascriptResponse(&url_loader_factory_, GURL(kUrl1),
                        CreateBasicGenerateBidScript());
  AddJavascriptResponse(&url_loader_factory_, GURL(kUrl2),
                        CreateBasicGenerateBidScript());

  BidderWorklet* worklet_impl1;
  auto worklet1 = CreateWorklet(
      GURL(kUrl1), /*pause_for_debugger_on_start=*/true, &worklet_impl1);
  GenerateBid(worklet1.get());

  BidderWorklet* worklet_impl2;
  auto worklet2 = CreateWorklet(
      GURL(kUrl2), /*pause_for_debugger_on_start=*/true, &worklet_impl2);
  GenerateBid(worklet2.get());

  int id1 = worklet_impl1->context_group_id_for_testing();
  int id2 = worklet_impl2->context_group_id_for_testing();

  TestChannel* channel1 = inspector_support.ConnectDebuggerSession(id1);
  TestChannel* channel2 = inspector_support.ConnectDebuggerSession(id2);

  channel1->RunCommandAndWaitForResult(
      1, "Runtime.enable", R"({"id":1,"method":"Runtime.enable","params":{}})");
  channel1->RunCommandAndWaitForResult(
      2, "Debugger.enable",
      R"({"id":2,"method":"Debugger.enable","params":{}})");

  channel2->RunCommandAndWaitForResult(
      1, "Runtime.enable", R"({"id":1,"method":"Runtime.enable","params":{}})");
  channel2->RunCommandAndWaitForResult(
      2, "Debugger.enable",
      R"({"id":2,"method":"Debugger.enable","params":{}})");

  // Should not see scriptParsed before resume.
  std::list<TestChannel::Event> events1 = channel1->TakeAllEvents();
  EXPECT_TRUE(std::none_of(events1.begin(), events1.end(), is_script_parsed));

  // Unpause execution for #1.
  load_script_run_loop_ = std::make_unique<base::RunLoop>();
  channel1->RunCommandAndWaitForResult(
      3, "Runtime.runIfWaitingForDebugger",
      R"({"id":3,"method":"Runtime.runIfWaitingForDebugger","params":{}})");
  load_script_run_loop_->Run();

  ASSERT_TRUE(bid_);
  EXPECT_EQ(1, bid_->bid);
  bid_.reset();
  load_script_run_loop_.reset();

  // channel1 should have had a parsed notification for kUrl1.
  TestChannel::Event script_parsed1 =
      channel1->WaitForMethodNotification("Debugger.scriptParsed");
  const std::string* url1 = script_parsed1.value.FindStringPath("params.url");
  ASSERT_TRUE(url1);
  EXPECT_EQ(kUrl1, *url1);

  // There shouldn't be a parsed notification on channel 2, however.
  std::list<TestChannel::Event> events2 = channel2->TakeAllEvents();
  EXPECT_TRUE(std::none_of(events2.begin(), events2.end(), is_script_parsed));

  // Unpause execution for #2.
  load_script_run_loop_ = std::make_unique<base::RunLoop>();
  channel2->RunCommandAndWaitForResult(
      3, "Runtime.runIfWaitingForDebugger",
      R"({"id":3,"method":"Runtime.runIfWaitingForDebugger","params":{}})");
  load_script_run_loop_->Run();

  ASSERT_TRUE(bid_);
  EXPECT_EQ(1, bid_->bid);
  bid_.reset();
  load_script_run_loop_.reset();

  // channel2 should have had a parsed notification for kUrl2.
  TestChannel::Event script_parsed2 =
      channel2->WaitForMethodNotification("Debugger.scriptParsed");
  const std::string* url2 = script_parsed2.value.FindStringPath("params.url");
  ASSERT_TRUE(url2);
  EXPECT_EQ(kUrl2, *url2);

  worklet1.reset();
  worklet2.reset();
  task_environment_.RunUntilIdle();

  // No other scriptParsed events should be on either channel.
  events1 = channel1->TakeAllEvents();
  events2 = channel2->TakeAllEvents();
  EXPECT_TRUE(std::none_of(events1.begin(), events1.end(), is_script_parsed));
  EXPECT_TRUE(std::none_of(events2.begin(), events2.end(), is_script_parsed));
}

TEST_F(BidderWorkletTest, ParseErrorV8Debug) {
  ScopedInspectorSupport inspector_support(v8_helper_.get());
  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        "Invalid Javascript");

  BidderWorklet* worklet_impl = nullptr;
  auto worklet =
      CreateWorklet(interest_group_bidding_url_,
                    /*pause_for_debugger_on_start=*/true, &worklet_impl);
  GenerateBid(worklet.get());
  int id = worklet_impl->context_group_id_for_testing();
  TestChannel* channel = inspector_support.ConnectDebuggerSession(id);

  channel->RunCommandAndWaitForResult(
      1, "Runtime.enable", R"({"id":1,"method":"Runtime.enable","params":{}})");
  channel->RunCommandAndWaitForResult(
      2, "Debugger.enable",
      R"({"id":2,"method":"Debugger.enable","params":{}})");

  // Unpause execution.
  load_script_run_loop_ = std::make_unique<base::RunLoop>();
  channel->RunCommandAndWaitForResult(
      3, "Runtime.runIfWaitingForDebugger",
      R"({"id":3,"method":"Runtime.runIfWaitingForDebugger","params":{}})");
  load_script_run_loop_->Run();
  load_script_run_loop_.reset();

  EXPECT_FALSE(bid_);
  // Should have gotten a parse error notification.
  TestChannel::Event parse_error =
      channel->WaitForMethodNotification("Debugger.scriptFailedToParse");
  const std::string* error_url = parse_error.value.FindStringPath("params.url");
  ASSERT_TRUE(error_url);
  EXPECT_EQ(interest_group_bidding_url_.spec(), *error_url);
}

TEST_F(BidderWorkletTest, BasicDevToolsDebug) {
  std::string bid_script = CreateGenerateBidScript(
      R"({ad: ["ad"], bid: this.global_bid ? this.global_bid : 1,
          render:"https://response.test/"})");
  const char kUrl1[] = "http://example.com/first.js";
  const char kUrl2[] = "http://example.org/second.js";

  AddJavascriptResponse(&url_loader_factory_, GURL(kUrl1), bid_script);
  AddJavascriptResponse(&url_loader_factory_, GURL(kUrl2), bid_script);

  auto worklet1 =
      CreateWorklet(GURL(kUrl1), true /* pause_for_debugger_on_start */);
  GenerateBid(worklet1.get());
  auto worklet2 =
      CreateWorklet(GURL(kUrl2), true /* pause_for_debugger_on_start */);
  GenerateBid(worklet2.get());

  mojo::Remote<blink::mojom::DevToolsAgent> agent1, agent2;
  worklet1->ConnectDevToolsAgent(agent1.BindNewPipeAndPassReceiver());
  worklet2->ConnectDevToolsAgent(agent2.BindNewPipeAndPassReceiver());

  TestDevToolsAgentClient debug1(std::move(agent1), "123",
                                 true /* use_binary_protocol */);
  TestDevToolsAgentClient debug2(std::move(agent2), "456",
                                 true /* use_binary_protocol */);

  debug1.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 1, "Runtime.enable",
      R"({"id":1,"method":"Runtime.enable","params":{}})");
  debug1.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 2, "Debugger.enable",
      R"({"id":2,"method":"Debugger.enable","params":{}})");

  debug2.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 1, "Runtime.enable",
      R"({"id":1,"method":"Runtime.enable","params":{}})");
  debug2.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 2, "Debugger.enable",
      R"({"id":2,"method":"Debugger.enable","params":{}})");

  const char kBreakpointTemplate[] = R"({
        "id":3,
        "method":"Debugger.setBreakpointByUrl",
        "params": {
          "lineNumber": 0,
          "url": "%s",
          "columnNumber": 0,
          "condition": ""
        }})";

  debug1.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 3, "Debugger.setBreakpointByUrl",
      base::StringPrintf(kBreakpointTemplate, kUrl1));
  debug2.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 3, "Debugger.setBreakpointByUrl",
      base::StringPrintf(kBreakpointTemplate, kUrl2));

  // Now start #1. We should see a scriptParsed event.
  debug1.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 4,
      "Runtime.runIfWaitingForDebugger",
      R"({"id":4,"method":"Runtime.runIfWaitingForDebugger","params":{}})");

  TestDevToolsAgentClient::Event script_parsed1 =
      debug1.WaitForMethodNotification("Debugger.scriptParsed");
  const std::string* url1 = script_parsed1.value.FindStringPath("params.url");
  ASSERT_TRUE(url1);
  EXPECT_EQ(*url1, kUrl1);
  absl::optional<int> context_id1 =
      script_parsed1.value.FindIntPath("params.executionContextId");
  ASSERT_TRUE(context_id1.has_value());

  // Next there is the breakpoint.
  TestDevToolsAgentClient::Event breakpoint_hit1 =
      debug1.WaitForMethodNotification("Debugger.paused");

  base::Value* hit_breakpoints1 =
      breakpoint_hit1.value.FindListPath("params.hitBreakpoints");
  ASSERT_TRUE(hit_breakpoints1);
  base::Value::ConstListView hit_breakpoints_list1 =
      hit_breakpoints1->GetList();
  ASSERT_EQ(1u, hit_breakpoints_list1.size());
  ASSERT_TRUE(hit_breakpoints_list1[0].is_string());
  EXPECT_EQ("1:0:0:http://example.com/first.js",
            hit_breakpoints_list1[0].GetString());

  // Override the bid value.
  const char kCommandTemplate[] = R"({
    "id": 5,
    "method": "Runtime.evaluate",
    "params": {
      "expression": "global_bid = 42",
      "contextId": %d
    }
  })";

  debug1.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kIO, 5, "Runtime.evaluate",
      base::StringPrintf(kCommandTemplate, context_id1.value()));

  // Resume, setting up event loop for fixture first.
  load_script_run_loop_ = std::make_unique<base::RunLoop>();
  debug1.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kIO, 6, "Debugger.resume",
      R"({"id":6,"method":"Debugger.resume","params":{}})");
  load_script_run_loop_->Run();

  ASSERT_TRUE(bid_);
  EXPECT_EQ(42, bid_->bid);
  bid_.reset();

  // Start #2, see that it hit its breakpoint.
  debug2.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 4,
      "Runtime.runIfWaitingForDebugger",
      R"({"id":4,"method":"Runtime.runIfWaitingForDebugger","params":{}})");

  TestDevToolsAgentClient::Event breakpoint_hit2 =
      debug2.WaitForMethodNotification("Debugger.paused");

  base::Value* hit_breakpoints2 =
      breakpoint_hit2.value.FindListPath("params.hitBreakpoints");
  ASSERT_TRUE(hit_breakpoints2);
  base::Value::ConstListView hit_breakpoints_list2 =
      hit_breakpoints2->GetList();
  ASSERT_EQ(1u, hit_breakpoints_list2.size());
  ASSERT_TRUE(hit_breakpoints_list2[0].is_string());
  EXPECT_EQ("1:0:0:http://example.org/second.js",
            hit_breakpoints_list2[0].GetString());

  // Go ahead and resume w/o messing with anything.
  load_script_run_loop_ = std::make_unique<base::RunLoop>();
  debug2.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kIO, 5, "Debugger.resume",
      R"({"id":5,"method":"Debugger.resume","params":{}})");
  load_script_run_loop_->Run();

  ASSERT_TRUE(bid_);
  EXPECT_EQ(1, bid_->bid);
}

TEST_F(BidderWorkletTest, InstrumentationBreakpoints) {
  const char kUrl[] = "http://example.com/bid.js";

  AddJavascriptResponse(
      &url_loader_factory_, GURL(kUrl),
      CreateReportWinScript(R"(sendReportTo("https://foo.test"))"));

  auto worklet =
      CreateWorklet(GURL(kUrl), true /* pause_for_debugger_on_start */);
  GenerateBid(worklet.get());

  mojo::Remote<blink::mojom::DevToolsAgent> agent;
  worklet->ConnectDevToolsAgent(agent.BindNewPipeAndPassReceiver());

  TestDevToolsAgentClient debug(std::move(agent), "123",
                                true /* use_binary_protocol */);

  debug.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 1, "Runtime.enable",
      R"({"id":1,"method":"Runtime.enable","params":{}})");
  debug.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 2, "Debugger.enable",
      R"({"id":2,"method":"Debugger.enable","params":{}})");

  // Set the instrumentation breakpoints.
  debug.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 3,
      "EventBreakpoints.setInstrumentationBreakpoint",
      MakeInstrumentationBreakpointCommand(3, "set",
                                           "beforeBidderWorkletBiddingStart"));
  debug.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 4,
      "EventBreakpoints.setInstrumentationBreakpoint",
      MakeInstrumentationBreakpointCommand(
          4, "set", "beforeBidderWorkletReportingStart"));

  // Unpause the execution. We should immediately hit the breakpoint right
  // after since BidderWorklet does bidding on creation.
  debug.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 5,
      "Runtime.runIfWaitingForDebugger",
      R"({"id":5,"method":"Runtime.runIfWaitingForDebugger","params":{}})");

  TestDevToolsAgentClient::Event breakpoint_hit1 =
      debug.WaitForMethodNotification("Debugger.paused");
  const std::string* breakpoint1 =
      breakpoint_hit1.value.FindStringPath("params.data.eventName");
  ASSERT_TRUE(breakpoint1);
  EXPECT_EQ("instrumentation:beforeBidderWorkletBiddingStart", *breakpoint1);

  // Resume and wait for bid result.
  load_script_run_loop_ = std::make_unique<base::RunLoop>();
  debug.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kIO, 6, "Debugger.resume",
      R"({"id":6,"method":"Debugger.resume","params":{}})");
  load_script_run_loop_->Run();

  ASSERT_TRUE(bid_);
  EXPECT_EQ(1, bid_->bid);
  bid_.reset();

  // Now ask for reporting. This should hit the other breakpoint.
  base::RunLoop run_loop;
  RunReportWinExpectingResultAsync(worklet.get(), GURL("https://foo.test/"), {},
                                   run_loop.QuitClosure());

  TestDevToolsAgentClient::Event breakpoint_hit2 =
      debug.WaitForMethodNotification("Debugger.paused");
  const std::string* breakpoint2 =
      breakpoint_hit2.value.FindStringPath("params.data.eventName");
  ASSERT_TRUE(breakpoint2);
  EXPECT_EQ("instrumentation:beforeBidderWorkletReportingStart", *breakpoint2);

  // Resume and wait for reporting to finish.
  debug.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kIO, 7, "Debugger.resume",
      R"({"id":7,"method":"Debugger.resume","params":{}})");
  run_loop.Run();
}

TEST_F(BidderWorkletTest, UnloadWhilePaused) {
  // Make sure things are cleaned up properly if the worklet is destroyed while
  // paused on a breakpoint.
  const char kUrl[] = "http://example.com/bid.js";

  AddJavascriptResponse(
      &url_loader_factory_, GURL(kUrl),
      CreateReportWinScript(R"(sendReportTo("https://foo.test"))"));

  auto worklet =
      CreateWorklet(GURL(kUrl), /*pause_for_debugger_on_start=*/true);
  GenerateBid(worklet.get());

  mojo::Remote<blink::mojom::DevToolsAgent> agent;
  worklet->ConnectDevToolsAgent(agent.BindNewPipeAndPassReceiver());

  TestDevToolsAgentClient debug(std::move(agent), "123",
                                /*use_binary_protocol=*/true);

  debug.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 1, "Runtime.enable",
      R"({"id":1,"method":"Runtime.enable","params":{}})");
  debug.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 2, "Debugger.enable",
      R"({"id":2,"method":"Debugger.enable","params":{}})");

  // Set an instrumentation breakpoint to get it debugger paused.
  debug.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 3,
      "EventBreakpoints.setInstrumentationBreakpoint",
      MakeInstrumentationBreakpointCommand(3, "set",
                                           "beforeBidderWorkletBiddingStart"));
  debug.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 4,
      "Runtime.runIfWaitingForDebugger",
      R"({"id":4,"method":"Runtime.runIfWaitingForDebugger","params":{}})");

  // Wait for breakpoint to hit.
  debug.WaitForMethodNotification("Debugger.paused");

  // ... and destroy the worklet
  worklet.reset();

  // This won't terminate if the V8 thread is still blocked in debugger.
  task_environment_.RunUntilIdle();
}

}  // namespace
}  // namespace auction_worklet
