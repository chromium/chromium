// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/bidder_worklet.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/trusted_bidding_signals.h"
#include "content/services/auction_worklet/worklet_test_util.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "net/http/http_status_code.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

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

// Creates reportWin() scripts with the specified body.
std::string CreateReportWinScript(const std::string& function_body) {
  constexpr char kReportWinScript[] = R"(
    function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
                       browserSignals) {
      %s;
    }
  )";
  return base::StringPrintf(kReportWinScript, function_body.c_str());
}

class BidderWorkletTest : public testing::Test {
 public:
  BidderWorkletTest() { SetDefaultParameters(); }

  ~BidderWorkletTest() override = default;

  // Default values. No test actually depends on these being anything but valid,
  // but test that set these can use this to reset values to default after each
  // test.
  void SetDefaultParameters() {
    interest_group_owner_ = url::Origin::Create(GURL("https://foo.test"));
    interest_group_name_ = "Fred";
    interest_group_user_bidding_signals_ = std::string();
    interest_group_ads_.clear();
    interest_group_ads_.push_back(blink::mojom::InterestGroupAd::New(
        GURL("https://response.test/"), base::nullopt /* metadata */));
    auction_signals_ = "[\"auction_signals\"]";
    null_auction_signals_ = false;
    per_buyer_signals_ = "[\"per_buyer_signals\"]";
    null_per_buyer_signals_ = false;
    browser_signal_top_window_hostname_ = "browser_signal_top_window_hostname";
    browser_signal_seller_ = "browser_signal_seller";
    browser_signal_join_count_ = 2;
    browser_signal_bid_count_ = 3;
    browser_signal_prev_wins_.clear();
    seller_signals_ = "[\"seller_signals\"]";
    browser_signal_render_url_ = GURL("https://render_url.test/");
    browser_signal_ad_render_fingerprint_ =
        "browser_signal_ad_render_fingerprint";
    browser_signal_bid_ = 1;
  }

  // Configures `url_loader_factory_` to return a generateBid() script with the
  // specified return line Then runs the script, expecting the provided result.
  void RunGenerateBidWithReturnValueExpectingResult(
      const std::string& raw_return_value,
      const BidderWorklet::BidResult& expected_result) {
    RunGenerateBidWithJavascriptExpectingResult(
        CreateGenerateBidScript(raw_return_value), expected_result);
  }

  // Configures `url_loader_factory_` to return a script with the specified
  // Javascript Then runs the script, expecting the provided result.
  void RunGenerateBidWithJavascriptExpectingResult(
      const std::string& javascript,
      const BidderWorklet::BidResult& expected_result) {
    SCOPED_TRACE(javascript);
    AddJavascriptResponse(&url_loader_factory_, url_, javascript);
    RunGenerateBidExpectingResult(expected_result);
  }

  // Loads and runs a generateBid() script with the provided return line,
  // expecting the provided result.
  void RunGenerateBidExpectingResult(
      const BidderWorklet::BidResult& expected_result) {
    auto bidder_worklet = CreateWorklet();
    ASSERT_TRUE(bidder_worklet);

    BidderWorklet::BidResult actual_result =
        RunGenerateBid(bidder_worklet.get());
    ExpectBidResultsEqual(expected_result, actual_result);
  }

  BidderWorklet::BidResult RunGenerateBid(BidderWorklet* bidder_worket) {
    blink::mojom::InterestGroupPtr interest_group =
        blink::mojom::InterestGroup::New();
    interest_group->owner = interest_group_owner_;
    interest_group->name = interest_group_name_;
    // Convert a string to an optional. Empty string means empty optional value.
    if (!interest_group_user_bidding_signals_.empty()) {
      interest_group->user_bidding_signals =
          interest_group_user_bidding_signals_;
    }
    interest_group->ads = std::vector<blink::mojom::InterestGroupAdPtr>();
    for (const auto& ad : interest_group_ads_) {
      interest_group->ads->emplace_back(ad.Clone());
    }
    return bidder_worket->GenerateBid(
        *interest_group,
        null_auction_signals_
            ? base::nullopt
            : base::make_optional<std::string>(auction_signals_),
        null_per_buyer_signals_
            ? base::nullopt
            : base::make_optional<std::string>(per_buyer_signals_),
        trusted_bidding_signals_keys_, trusted_bidding_signals_.get(),
        browser_signal_top_window_hostname_, browser_signal_seller_,
        browser_signal_join_count_, browser_signal_bid_count_,
        browser_signal_prev_wins_, auction_start_time_);
  }

  void ExpectBidResultsEqual(const BidderWorklet::BidResult& expected_result,
                             const BidderWorklet::BidResult& actual_result) {
    EXPECT_EQ(expected_result.success, actual_result.success);
    EXPECT_EQ(expected_result.ad, actual_result.ad);
    EXPECT_EQ(expected_result.bid, actual_result.bid);
    EXPECT_EQ(expected_result.render_url, actual_result.render_url);
  }

  // Configures `url_loader_factory_` to return a reportWin() script with the
  // specified body. Then runs the script, expecting the provided result.
  void RunReportWinWithFunctionBodyExpectingResult(
      const std::string& function_body,
      const GURL& expected_report_url) {
    RunReportWinWithJavascriptExpectingResult(
        CreateReportWinScript(function_body), expected_report_url);
  }

  // Configures `url_loader_factory_` to return a reportWin() script with the
  // specified Javascript. Then runs the script, expecting the provided result.
  void RunReportWinWithJavascriptExpectingResult(
      const std::string& javascript,
      const GURL& expected_report_url) {
    SCOPED_TRACE(javascript);
    AddJavascriptResponse(&url_loader_factory_, url_, javascript);
    RunReportWinExpectingResult(expected_report_url);
  }

  // Loads and runs a reportWin() with the provided return line, expecting the
  // supplied result.
  void RunReportWinExpectingResult(const GURL& expected_report_url) {
    auto bidder_worket = CreateWorklet();
    ASSERT_TRUE(bidder_worket);

    BidderWorklet::ReportWinResult actual_result = bidder_worket->ReportWin(
        null_auction_signals_
            ? base::nullopt
            : base::make_optional<std::string>(auction_signals_),
        null_per_buyer_signals_
            ? base::nullopt
            : base::make_optional<std::string>(per_buyer_signals_),
        seller_signals_, browser_signal_top_window_hostname_,
        interest_group_owner_, interest_group_name_, browser_signal_render_url_,
        browser_signal_ad_render_fingerprint_, browser_signal_bid_);
    EXPECT_EQ(!expected_report_url.is_empty(), actual_result.success);
    EXPECT_EQ(expected_report_url, actual_result.report_url);
  }

  // Create a BidderWorklet, waiting for the URLLoader to complete. Returns
  // nullptr on failure.
  std::unique_ptr<BidderWorklet> CreateWorklet() {
    CHECK(!load_script_run_loop_);

    create_worklet_succeeded_ = false;
    auto bidder_worket = std::make_unique<BidderWorklet>(
        &url_loader_factory_, url_, &v8_helper_,
        base::BindOnce(&BidderWorkletTest::CreateWorkletCallback,
                       base::Unretained(this)));
    load_script_run_loop_ = std::make_unique<base::RunLoop>();
    load_script_run_loop_->Run();
    load_script_run_loop_.reset();
    if (!create_worklet_succeeded_)
      return nullptr;
    return bidder_worket;
  }

 protected:
  void CreateWorkletCallback(bool success) {
    create_worklet_succeeded_ = success;
    load_script_run_loop_->Quit();
  }

  base::test::TaskEnvironment task_environment_;

  const GURL url_ = GURL("https://url.test/");

  // Arguments passed to generateBid() and reportWin(). Arguments that both
  // methods take are shared, as are interest group fields that also appear in
  // `browserSignals`.
  url::Origin interest_group_owner_;
  std::string interest_group_name_;
  // This is actually an option value, but to make testing easier, use a string.
  // An empty string means nullptr.
  std::string interest_group_user_bidding_signals_;
  std::vector<blink::mojom::InterestGroupAdPtr> interest_group_ads_;

  std::string auction_signals_;
  // true to pass nullopt rather than `auction_signals_`.
  bool null_auction_signals_ = false;

  std::string per_buyer_signals_;
  // true to pass nullopt rather than `per_buyer_signals_`.
  bool null_per_buyer_signals_ = false;

  std::string browser_signal_top_window_hostname_;
  std::string browser_signal_seller_;
  int browser_signal_join_count_;
  int browser_signal_bid_count_;
  std::vector<mojo::StructPtr<mojom::PreviousWin>> browser_signal_prev_wins_;
  std::vector<std::string> trusted_bidding_signals_keys_;
  std::unique_ptr<TrustedBiddingSignals> trusted_bidding_signals_;
  std::string seller_signals_;
  GURL browser_signal_render_url_;
  std::string browser_signal_ad_render_fingerprint_;
  double browser_signal_bid_;

  // Use a single constant start time. Only delta times are provided to scripts,
  // relative to the time of the auction, so no need to vary the auction time.
  const base::Time auction_start_time_ = base::Time::Now();

  // Reuseable run loop for loading the script. It's always populated after
  // creating the worklet, to cause a crash if the callback is invoked
  // synchronously.
  std::unique_ptr<base::RunLoop> load_script_run_loop_;
  bool create_worklet_succeeded_ = false;

  network::TestURLLoaderFactory url_loader_factory_;
  AuctionV8Helper v8_helper_;
};

TEST_F(BidderWorkletTest, NetworkError) {
  url_loader_factory_.AddResponse(url_.spec(), CreateBasicGenerateBidScript(),
                                  net::HTTP_NOT_FOUND);
  EXPECT_FALSE(CreateWorklet());
}

TEST_F(BidderWorkletTest, CompileError) {
  AddJavascriptResponse(&url_loader_factory_, url_, "Invalid Javascript");
  EXPECT_FALSE(CreateWorklet());
}

// Test parsing of return values.
TEST_F(BidderWorkletTest, GenerateBidResult) {
  // Base case. Also serves to make sure the script returned by
  // CreateBasicGenerateBidScript() does indeed work.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateBasicGenerateBidScript(),
      BidderWorklet::BidResult("[\"ad\"]", 1, GURL("https://response.test/")));

  // --------
  // Vary ad
  // --------

  // Make sure "ad" can be of a variety of JS object types.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid:1, render:"https://response.test/"})",
      BidderWorklet::BidResult("\"ad\"", 1, GURL("https://response.test/")));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: {a:1,b:null}, bid:1, render:"https://response.test/"})",
      BidderWorklet::BidResult(R"({"a":1,"b":null})", 1,
                               GURL("https://response.test/")));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: [2.5,[]], bid:1, render:"https://response.test/"})",
      BidderWorklet::BidResult("[2.5,[]]", 1, GURL("https://response.test/")));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: -5, bid:1, render:"https://response.test/"})",
      BidderWorklet::BidResult("-5", 1, GURL("https://response.test/")));
  // Some values that can't be represented in JSON become null.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: 0/0, bid:1, render:"https://response.test/"})",
      BidderWorklet::BidResult("null", 1, GURL("https://response.test/")));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: [globalThis.not_defined], bid:1, render:"https://response.test/"})",
      BidderWorklet::BidResult("[null]", 1, GURL("https://response.test/")));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: [function() {return 1;}], bid:1, render:"https://response.test/"})",
      BidderWorklet::BidResult("[null]", 1, GURL("https://response.test/")));

  // Other values JSON can't represent result in failing instead of null.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: globalThis.not_defined, bid:1, render:"https://response.test/"})",
      BidderWorklet::BidResult());
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: function() {return 1;}, bid:1, render:"https://response.test/"})",
      BidderWorklet::BidResult());

  // Make sure recursive structures aren't allowed in ad field.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(
        function generateBid() {
          var a = [];
          a[0] = a;
          return {ad: a, bid:1, render:"https://response.test/"};
        }
      )",
      BidderWorklet::BidResult());

  // --------
  // Vary bid
  // --------

  // Valid positive bid values.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid:1.5, render:"https://response.test/"})",
      BidderWorklet::BidResult("\"ad\"", 1.5, GURL("https://response.test/")));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid:2, render:"https://response.test/"})",
      BidderWorklet::BidResult("\"ad\"", 2, GURL("https://response.test/")));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid:0.001, render:"https://response.test/"})",
      BidderWorklet::BidResult("\"ad\"", 0.001,
                               GURL("https://response.test/")));

  // Bids <= 0.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:0, render:"https://response.test/"})",
      BidderWorklet::BidResult());
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:-10, render:"https://response.test/"})",
      BidderWorklet::BidResult());
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:-1.5, render:"https://response.test/"})",
      BidderWorklet::BidResult());

  // Infinite and NaN bid.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1/0, render:"https://response.test/"})",
      BidderWorklet::BidResult());
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:-1/0, render:"https://response.test/"})",
      BidderWorklet::BidResult());
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:0/0, render:"https://response.test/"})",
      BidderWorklet::BidResult());

  // Non-numeric bid.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:"1", render:"https://response.test/"})",
      BidderWorklet::BidResult());
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:[1], render:"https://response.test/"})",
      BidderWorklet::BidResult());

  // ---------
  // Vary URL.
  // ---------

  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:"https://response.test/"})",
      BidderWorklet::BidResult("[\"ad\"]", 1, GURL("https://response.test/")));

  // Disallowed schemes.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:"http://response.test/"})",
      BidderWorklet::BidResult());
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:"chrome-extension://response.test/"})",
      BidderWorklet::BidResult());
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:"about:blank"})",
      BidderWorklet::BidResult());
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:"data:,foo"})", BidderWorklet::BidResult());

  // Invalid URLs.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:"test"})", BidderWorklet::BidResult());
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:"http://"})", BidderWorklet::BidResult());
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:["http://response.test/"]})",
      BidderWorklet::BidResult());
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:9})", BidderWorklet::BidResult());

  // ------------
  // Other cases.
  // ------------

  // No return value.
  RunGenerateBidWithReturnValueExpectingResult("", BidderWorklet::BidResult());

  // Missing value.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({bid:"a", render:"https://response.test/"})",
      BidderWorklet::BidResult());
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], render:"https://response.test/"})",
      BidderWorklet::BidResult());
  RunGenerateBidWithReturnValueExpectingResult(R"({ad: ["ad"], bid:"a"})",
                                               BidderWorklet::BidResult());

  // Valid JS, but missing function.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(
        function someOtherFunction() {
          return {ad: ["ad"], bid:1, render:"https://response.test/"};
        }
      )",
      BidderWorklet::BidResult());
  RunGenerateBidWithJavascriptExpectingResult("", BidderWorklet::BidResult());
  RunGenerateBidWithJavascriptExpectingResult("5", BidderWorklet::BidResult());
  RunGenerateBidWithJavascriptExpectingResult("shrimp",
                                              BidderWorklet::BidResult());

  // Throw exception.
  RunGenerateBidWithReturnValueExpectingResult("shrimp",
                                               BidderWorklet::BidResult());
}

// Make sure Date() is not available when running generateBid().
TEST_F(BidderWorkletTest, GenerateBidDateNotAvailable) {
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: Date().toString(), bid:1, render:"https://response.test/"})",
      BidderWorklet::BidResult());
}

// Checks that most input parameters are correctly passed in, and each is parsed
// as JSON or not, depending on the parameter. Does not test `previousWins` or
// `trustedBiddingSignals`.
TEST_F(BidderWorkletTest, GenerateBidBasicInputParameters) {
  // Parameters that are C++ strings, including JSON strings.
  const struct StringTestCase {
    // String used in JS to access the parameter.
    const char* name;
    bool is_json;
    // Pointer to location at which the string can be modified.
    std::string* value_ptr;
  } kStringTestCases[] = {
      {
          "interestGroup.name",
          false /* is_json */,
          &interest_group_name_,
      },
      {
          "interestGroup.userBiddingSignals",
          true /* is_json */,
          &interest_group_user_bidding_signals_,
      },
      {
          "auctionSignals",
          true /* is_json */,
          &auction_signals_,
      },
      {
          "perBuyerSignals",
          true /* is_json */,
          &per_buyer_signals_,
      },
      {
          "browserSignals.topWindowHostname",
          false /* is_json */,
          &browser_signal_top_window_hostname_,
      },
      {
          "browserSignals.seller",
          false /* is_json */,
          &browser_signal_seller_,
      },
  };

  for (const auto& test_case : kStringTestCases) {
    SCOPED_TRACE(test_case.name);

    *test_case.value_ptr = "foo";
    RunGenerateBidWithReturnValueExpectingResult(
        base::StringPrintf(
            R"({ad: %s, bid:1, render:"https://response.test/"})",
            test_case.name),
        test_case.is_json ? BidderWorklet::BidResult()
                          : BidderWorklet::BidResult(
                                R"("foo")", 1, GURL("https://response.test/")));

    *test_case.value_ptr = R"("foo")";
    RunGenerateBidWithReturnValueExpectingResult(
        base::StringPrintf(
            R"({ad: %s, bid:1, render:"https://response.test/"})",
            test_case.name),
        test_case.is_json
            ? BidderWorklet::BidResult(R"("foo")", 1,
                                       GURL("https://response.test/"))
            : BidderWorklet::BidResult(R"("\"foo\"")", 1,
                                       GURL("https://response.test/")));

    *test_case.value_ptr = "[1]";
    RunGenerateBidWithReturnValueExpectingResult(
        base::StringPrintf(
            R"({ad: %s[0], bid:1, render:"https://response.test/"})",
            test_case.name),
        test_case.is_json
            ? BidderWorklet::BidResult("1", 1, GURL("https://response.test/"))
            : BidderWorklet::BidResult(R"("[")", 1,
                                       GURL("https://response.test/")));
    SetDefaultParameters();
  }

  interest_group_owner_ = url::Origin::Create(GURL("https://foo.test/"));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: interestGroup.owner, bid:1, render:"https://response.test/"})",
      BidderWorklet::BidResult(R"("https://foo.test")", 1,
                               GURL("https://response.test/")));

  interest_group_owner_ = url::Origin::Create(GURL("https://[::1]:40000/"));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: interestGroup.owner, bid:1, render:"https://response.test/"})",
      BidderWorklet::BidResult(R"("https://[::1]:40000")", 1,
                               GURL("https://response.test/")));
  SetDefaultParameters();

  // Test the empty `userBiddingSignals` case, too. It's actually an optional
  // unlike the other values. Setting it to the empty string makes the optional
  // nullptr. This results in interestGroup.userBiddingSignals not being
  // populated (so undefined, rather than null).
  interest_group_user_bidding_signals_ = "";
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad:typeof interestGroup.userBiddingSignals, bid:1, render:"https://response.test/"})",
      BidderWorklet::BidResult(R"("undefined")", 1,
                               GURL("https://response.test/")));
  SetDefaultParameters();

  const struct IntegerTestCase {
    // String used in JS to access the parameter.
    const char* name;
    // Pointer to location at which the integer can be modified.
    int* value_ptr;
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
        BidderWorklet::BidResult("0", 1, GURL("https://response.test/")));

    *test_case.value_ptr = 10;
    RunGenerateBidWithReturnValueExpectingResult(
        base::StringPrintf(
            R"({ad: %s, bid:1, render:"https://response.test/"})",
            test_case.name),
        BidderWorklet::BidResult("10", 1, GURL("https://response.test/")));
    SetDefaultParameters();
  }

  // Test InterestGroup.ads field.

  // A bid URL that's not in the InterestGroup's ads list should fail.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: 0, bid:1, render:"https://response2.test/"})",
      BidderWorklet::BidResult());

  // Adding an ad with a corresponding `renderUrl` should result in success.
  // Also check the `interestGroup.ads` field passed to Javascript.
  interest_group_ads_.push_back(blink::mojom::InterestGroupAd::New(
      GURL("https://response2.test/"), R"(["metadata"])" /* metadata */));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: interestGroup.ads, bid:1, render:"https://response2.test/"})",
      BidderWorklet::BidResult("[{\"renderUrl\":\"https://response.test/\"},"
                               "{\"renderUrl\":\"https://response2.test/"
                               "\",\"metadata\":[\"metadata\"]}]",
                               1, GURL("https://response2.test/")));

  // Make sure `metadata` is treated as an object, instead of a raw string.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: interestGroup.ads[1].metadata[0], bid:1, render:"https://response.test/"})",
      BidderWorklet::BidResult("\"metadata\"", 1,
                               GURL("https://response.test/")));
}

// Test handling of null auctionSignals and perBuyerSignals to generateBid.
TEST_F(BidderWorkletTest, GenerateBidParametersOptionalString) {
  constexpr char kRetVal[] = R"({
    ad: "metadata",
    bid: (auctionSignals === null ? 10 : 0) +
         (perBuyerSignals === null ? 2 : 1),
    render: "https://response.test/"
})";

  SetDefaultParameters();
  null_auction_signals_ = false;
  null_per_buyer_signals_ = false;
  RunGenerateBidWithReturnValueExpectingResult(
      kRetVal, BidderWorklet::BidResult("\"metadata\"", 1,
                                        GURL("https://response.test/")));

  SetDefaultParameters();
  null_auction_signals_ = false;
  null_per_buyer_signals_ = true;
  RunGenerateBidWithReturnValueExpectingResult(
      kRetVal, BidderWorklet::BidResult("\"metadata\"", 2,
                                        GURL("https://response.test/")));

  SetDefaultParameters();
  null_auction_signals_ = true;
  null_per_buyer_signals_ = false;
  RunGenerateBidWithReturnValueExpectingResult(
      kRetVal, BidderWorklet::BidResult("\"metadata\"", 11,
                                        GURL("https://response.test/")));

  SetDefaultParameters();
  null_auction_signals_ = true;
  null_per_buyer_signals_ = true;
  RunGenerateBidWithReturnValueExpectingResult(
      kRetVal, BidderWorklet::BidResult("\"metadata\"", 12,
                                        GURL("https://response.test/")));
}

// Utility methods to create vectors of PreviousWin. Needed because StructPtr's
// don't allow copying.

std::vector<mojo::StructPtr<mojom::PreviousWin>> CreateWinList(
    const mojo::StructPtr<mojom::PreviousWin>& win1) {
  std::vector<mojo::StructPtr<mojom::PreviousWin>> out;
  out.emplace_back(win1.Clone());
  return out;
}

std::vector<mojo::StructPtr<mojom::PreviousWin>> CreateWinList(
    const mojo::StructPtr<mojom::PreviousWin>& win1,
    const mojo::StructPtr<mojom::PreviousWin>& win2) {
  std::vector<mojo::StructPtr<mojom::PreviousWin>> out;
  out.emplace_back(win1.Clone());
  out.emplace_back(win2.Clone());
  return out;
}

TEST_F(BidderWorkletTest, GenerateBidPrevWins) {
  base::TimeDelta delta = base::TimeDelta::FromSeconds(100);
  base::TimeDelta tiny_delta = base::TimeDelta::FromMilliseconds(500);

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
      // Out of order times.
      {
          CreateWinList(future_win, win1),
          "browserSignals.prevWins",
          R"([[0,"future_ad"],[200,"ad1"]])",
      },
  };

  for (auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.ad);
    // StructPtrs aren't copiable, so this effectively destroys each test case.
    browser_signal_prev_wins_ = std::move(test_case.prev_wins);
    BidderWorklet::BidResult expected_result;
    RunGenerateBidWithReturnValueExpectingResult(
        base::StringPrintf(
            R"({ad: %s, bid:1, render:"https://response.test/"})",
            test_case.ad),
        BidderWorklet::BidResult(test_case.expected_ad, 1,
                                 GURL("https://response.test/")));
  }
}

TEST_F(BidderWorkletTest, GenerateBidTrustedBiddingSignals) {
  const GURL kBaseSignalsUrl("https://signals.test/");
  const GURL kFullSignalsUrl(
      "https://signals.test/?hostname=hostname&keys=key1,key2");

  const char kJson[] = R"(
    {
      "key1": 1,
      "key2": [2]
    }
  )";

  AddJsonResponse(&url_loader_factory_, kFullSignalsUrl, kJson);

  // Request with null TrustedBiddingSignals. This results
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: trustedBiddingSignals, bid:1, render:"https://response.test/"})",
      BidderWorklet::BidResult("null", 1, GURL("https://response.test/")));

  base::RunLoop run_loop;
  bool signals_loaded_successfully = false;
  trusted_bidding_signals_ = std::make_unique<TrustedBiddingSignals>(
      &url_loader_factory_, std::vector<std::string>({"key1", "key2"}),
      "hostname", kBaseSignalsUrl, &v8_helper_,
      base::BindLambdaForTesting([&](bool success) {
        signals_loaded_successfully = success;
        run_loop.Quit();
      }));
  run_loop.Run();
  ASSERT_TRUE(signals_loaded_successfully);

  // Request with no keys, but non-empty `trustedBiddingSignals`. Probably
  // best not to load the signals if it happens, but whether or not that's done,
  // `trustedBiddingSignals` should be null.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: trustedBiddingSignals, bid:1, render:"https://response.test/"})",
      BidderWorklet::BidResult("null", 1, GURL("https://response.test/")));

  trusted_bidding_signals_keys_ = {"key1"};
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: trustedBiddingSignals["key1"], bid:1, render:"https://response.test/"})",
      BidderWorklet::BidResult("1", 1, GURL("https://response.test/")));

  trusted_bidding_signals_keys_ = {"key2"};
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: trustedBiddingSignals, bid:1, render:"https://response.test/"})",
      BidderWorklet::BidResult(R"({"key2":[2]})", 1,
                               GURL("https://response.test/")));

  trusted_bidding_signals_keys_ = {"key1", "key2"};
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: trustedBiddingSignals, bid:1, render:"https://response.test/"})",
      BidderWorklet::BidResult(R"({"key1":1,"key2":[2]})", 1,
                               GURL("https://response.test/")));
}

TEST_F(BidderWorkletTest, ReportWin) {
  RunReportWinWithFunctionBodyExpectingResult("", GURL());
  RunReportWinWithFunctionBodyExpectingResult(
      R"(return "https://ignored.test/")", GURL());

  RunReportWinWithFunctionBodyExpectingResult(
      R"(sendReportTo("https://foo.test"))", GURL("https://foo.test/"));
  RunReportWinWithFunctionBodyExpectingResult(
      R"(sendReportTo("https://foo.test/bar"))", GURL("https://foo.test/bar"));

  RunReportWinWithFunctionBodyExpectingResult(
      R"(sendReportTo("http://http.not.allowed.test"))", GURL());
  RunReportWinWithFunctionBodyExpectingResult(
      R"(sendReportTo("file:///file.not.allowed.test"))", GURL());

  RunReportWinWithFunctionBodyExpectingResult(R"(sendReportTo(""))", GURL());

  RunReportWinWithFunctionBodyExpectingResult(
      R"(sendReportTo("https://foo.test");sendReportTo("https://foo.test"))",
      GURL());
}

// Make sure Date() is not available when running reportWin().
TEST_F(BidderWorkletTest, ReportWinDateNotAvailable) {
  RunReportWinWithFunctionBodyExpectingResult(
      R"(sendReportTo("https://foo.test/" + Date().toString()))", GURL());
}

TEST_F(BidderWorkletTest, ReportWinParameters) {
  // Parameters that are C++ strings, including JSON strings.
  const struct StringTestCase {
    // String used in JS to access the parameter.
    const char* name;
    bool is_json;
    // Pointer to location at which the string can be modified.
    std::string* value_ptr;
  } kStringTestCases[] = {
      {
          "auctionSignals",
          true /* is_json */,
          &auction_signals_,
      },
      {
          "perBuyerSignals",
          true /* is_json */,
          &per_buyer_signals_,
      },
      {
          "sellerSignals",
          true /* is_json */,
          &seller_signals_,
      },
      {
          "browserSignals.topWindowHostname",
          false /* is_json */,
          &browser_signal_top_window_hostname_,
      },
      {
          "browserSignals.interestGroupName",
          false /* is_json */,
          &interest_group_name_,
      },
      {
          "browserSignals.adRenderFingerprint",
          false /* is_json */,
          &browser_signal_ad_render_fingerprint_,
      },
  };

  for (const auto& test_case : kStringTestCases) {
    SCOPED_TRACE(test_case.name);

    *test_case.value_ptr = "https://foo.test/";
    RunReportWinWithFunctionBodyExpectingResult(
        base::StringPrintf("sendReportTo(%s)", test_case.name),
        test_case.is_json ? GURL() : GURL("https://foo.test/"));

    *test_case.value_ptr = R"(["https://foo.test/"])";
    RunReportWinWithFunctionBodyExpectingResult(
        base::StringPrintf("sendReportTo(%s[0])", test_case.name),
        test_case.is_json ? GURL("https://foo.test/") : GURL());

    SetDefaultParameters();
  }

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
  SetDefaultParameters();

  browser_signal_render_url_ = GURL("https://shrimp.test/");
  RunReportWinWithFunctionBodyExpectingResult(
      "sendReportTo(browserSignals.renderUrl)", browser_signal_render_url_);
  SetDefaultParameters();

  browser_signal_bid_ = 4;
  RunReportWinWithFunctionBodyExpectingResult(
      R"(if (browserSignals.bid == 4)
        sendReportTo("https://jumboshrimp.test"))",
      GURL("https://jumboshrimp.test"));
}

// Test handling of null auctionSignals and perBuyerSignals to reportWin.
TEST_F(BidderWorkletTest, ReportWinParametersOptionalString) {
  constexpr char kBody[] = R"(
    let url = "https://reporter.com/?" +
                (auctionSignals === null  ? "aN" : "aP") +
                (perBuyerSignals === null ? "pN" : "pP");
    sendReportTo(url);
  )";

  SetDefaultParameters();
  null_auction_signals_ = false;
  null_per_buyer_signals_ = false;
  RunReportWinWithFunctionBodyExpectingResult(
      kBody, GURL("https://reporter.com/?aPpP"));

  SetDefaultParameters();
  null_auction_signals_ = false;
  null_per_buyer_signals_ = true;
  RunReportWinWithFunctionBodyExpectingResult(
      kBody, GURL("https://reporter.com/?aPpN"));

  SetDefaultParameters();
  null_auction_signals_ = true;
  null_per_buyer_signals_ = false;
  RunReportWinWithFunctionBodyExpectingResult(
      kBody, GURL("https://reporter.com/?aNpP"));

  SetDefaultParameters();
  null_auction_signals_ = true;
  null_per_buyer_signals_ = true;
  RunReportWinWithFunctionBodyExpectingResult(
      kBody, GURL("https://reporter.com/?aNpN"));
}

// Subsequent runs of the same script should not affect each other. Same is true
// for different scripts, but it follows from the single script case.
TEST_F(BidderWorkletTest, ScriptIsolation) {
  // Use arrays so that all values are references, to catch both the case where
  // variables are persisted, and the case where what they refer to is
  // persisted, but variables are overwritten between runs.
  AddJavascriptResponse(&url_loader_factory_, url_,
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
            };
          }
        }();
      )");
  auto bidder_worket = CreateWorklet();
  ASSERT_TRUE(bidder_worket);

  for (int i = 0; i < 3; ++i) {
    BidderWorklet::BidResult actual_result =
        RunGenerateBid(bidder_worket.get());
    // "ad" value should be the same every time the script is run.
    ExpectBidResultsEqual(
        BidderWorklet::BidResult("[2,3]", 1, GURL("https://response.test/")),
        actual_result);
  }
}

}  // namespace
}  // namespace auction_worklet
