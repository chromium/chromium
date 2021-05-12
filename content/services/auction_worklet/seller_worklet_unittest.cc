// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/seller_worklet.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/worklet_test_util.h"
#include "net/http/http_status_code.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace auction_worklet {
namespace {

// Creates seller a script with scoreAd() returning the specified expression.
// Allows using scoreAd() arguments, arbitrary values, incorrect types, etc.
std::string CreateScoreAdScript(const std::string& raw_return_value) {
  constexpr char kSellAdScript[] = R"(
    function scoreAd(adMetadata, bid, auctionConfig, trustedScoringSignals,
        browserSignals) {
      return %s;
    }
  )";
  return base::StringPrintf(kSellAdScript, raw_return_value.c_str());
}

// Returns a working script, primarily for testing failure cases where it
// should not be run.
std::string CreateBasicSellAdScript() {
  return CreateScoreAdScript("1");
}

// Creates a seller script with report_result() returning the specified
// expression. If |extra_code| is non-empty, it will be added as an additional
// line above the return value. Intended for sendReportTo() calls. In practice,
// these scripts will always include a scoreAd() method, but few tests check
// both methods.
std::string CreateReportToScript(const std::string& raw_return_value,
                                 const std::string& extra_code) {
  constexpr char kBasicSellerScript[] = R"(
    function reportResult(auctionConfig, browserSignals) {
      %s;
      return %s;
    }
  )";
  return base::StringPrintf(kBasicSellerScript, extra_code.c_str(),
                            raw_return_value.c_str());
}

class SellerWorkletTest : public testing::Test {
 public:
  SellerWorkletTest() { SetDefaultParameters(); }

  ~SellerWorkletTest() override = default;

  // Sets default values for scoreAd() and report_result() arguments. No test
  // actually depends on these being anything but valid, but this does allow
  // tests to reset them to a consistent state.
  void SetDefaultParameters() {
    ad_metadata_ = "[1]";
    bid_ = 1;
    auction_config_ = blink::mojom::AuctionAdConfig::New();
    browser_signal_top_window_hostname_ = "top_window_hostname";
    browser_signal_interest_group_owner_ =
        url::Origin::Create(GURL("https://foo.test/"));
    browser_signal_ad_render_fingerprint_ = "ad_render_fingerprint";
    browser_signal_bidding_duration_ = base::TimeDelta();
    browser_signal_render_url_ = GURL("https://render.url.test/");
    browser_signal_desireability_ = 1;
  }

  // Configures `url_loader_factory_` to return a script with the specified
  // return line, expecting the provided result.
  void RunScoreAdWithReturnValueExpectingResult(
      const std::string& raw_return_value,
      double expected_score) {
    RunScoreAdWithJavascriptExpectingResult(
        CreateScoreAdScript(raw_return_value), expected_score);
  }

  // Configures `url_loader_factory_` to return the provided script, and then
  // runs its generate_bid() function. Then runs the script, expecting the
  // provided result.
  void RunScoreAdWithJavascriptExpectingResult(const std::string& javascript,
                                               double expected_score) {
    SCOPED_TRACE(javascript);
    AddJavascriptResponse(&url_loader_factory_, url_, javascript);
    RunScoreAdExpectingResult(expected_score);
  }

  // Loads and runs a scode_ad() script, expecting the supplied result.
  void RunScoreAdExpectingResult(double expected_score) {
    auto seller_worket = CreateWorklet();
    ASSERT_TRUE(seller_worket);

    SellerWorklet::ScoreResult actual_result =
        seller_worket->ScoreAd(ad_metadata_, bid_, *auction_config_,
                               browser_signal_top_window_hostname_,
                               browser_signal_interest_group_owner_,
                               browser_signal_ad_render_fingerprint_,
                               browser_signal_bidding_duration_);
    EXPECT_EQ(expected_score > 0, actual_result.success);
    EXPECT_EQ(expected_score, actual_result.score);
  }

  // Configures `url_loader_factory_` to return a report_result() script created
  // with CreateReportToScript(). Then runs the script, expecting the provided
  // result.
  void RunReportResultCreatedScriptExpectingResult(
      const std::string& raw_return_value,
      const std::string& extra_code,
      const SellerWorklet::Report& expected_report) {
    RunReportResultWithJavascriptExpectingResult(
        CreateReportToScript(raw_return_value, extra_code), expected_report);
  }

  // Configures `url_loader_factory_` to return the provided script, and then
  // runs its report_result() function. Then runs the script, expecting the
  // provided result.
  void RunReportResultWithJavascriptExpectingResult(
      const std::string& javascript,
      const SellerWorklet::Report& expected_report) {
    SCOPED_TRACE(javascript);
    AddJavascriptResponse(&url_loader_factory_, url_, javascript);
    RunReportResultExpectingResult(expected_report);
  }

  // Loads and runs a report_result() script, expecting the supplied result.
  void RunReportResultExpectingResult(
      const SellerWorklet::Report& expected_report) {
    auto seller_worket = CreateWorklet();
    ASSERT_TRUE(seller_worket);

    SellerWorklet::Report actual_result = seller_worket->ReportResult(
        *auction_config_, browser_signal_top_window_hostname_,
        browser_signal_interest_group_owner_, browser_signal_render_url_,
        browser_signal_ad_render_fingerprint_, bid_,
        browser_signal_desireability_);
    EXPECT_EQ(expected_report.success, actual_result.success);
    EXPECT_EQ(expected_report.signals_for_winner,
              actual_result.signals_for_winner);
    EXPECT_EQ(expected_report.report_url, actual_result.report_url);
  }

  // Create a SellerWorklet, waiting for the URLLoader to complete. Returns
  // nullptr on failure.
  std::unique_ptr<SellerWorklet> CreateWorklet() {
    CHECK(!load_script_run_loop_);

    create_worklet_succeeded_ = false;
    auto bidder_worket = std::make_unique<SellerWorklet>(
        &url_loader_factory_, url_, &v8_helper_,
        base::BindOnce(&SellerWorkletTest::CreateWorkletCallback,
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

  // Arguments passed to score_bid() and report_result(). Arguments common to
  // both of them use the same field.
  std::string ad_metadata_;
  // This is a browser signal for report_result(), but a direct parameter for
  // score_bid().
  double bid_;
  blink::mojom::AuctionAdConfigPtr auction_config_;
  std::string browser_signal_top_window_hostname_;
  url::Origin browser_signal_interest_group_owner_;
  std::string browser_signal_ad_render_fingerprint_;
  base::TimeDelta browser_signal_bidding_duration_;
  GURL browser_signal_render_url_;
  double browser_signal_desireability_;

  // Reuseable run loop for loading the script. It's always populated after
  // creating the worklet, to cause a crash if the callback is invoked
  // synchronously.
  std::unique_ptr<base::RunLoop> load_script_run_loop_;
  bool create_worklet_succeeded_ = false;

  network::TestURLLoaderFactory url_loader_factory_;
  AuctionV8Helper v8_helper_;
};

TEST_F(SellerWorkletTest, NetworkError) {
  url_loader_factory_.AddResponse(url_.spec(), CreateBasicSellAdScript(),
                                  net::HTTP_NOT_FOUND);
  EXPECT_FALSE(CreateWorklet());
}

TEST_F(SellerWorkletTest, CompileError) {
  AddJavascriptResponse(&url_loader_factory_, url_, "Invalid Javascript");
  EXPECT_FALSE(CreateWorklet());
}

// Test parsing of return values.
TEST_F(SellerWorkletTest, ScoreAd) {
  // Base case. Also serves to make sure the script returned by
  // CreateBasicSellAdScript() does indeed work.
  RunScoreAdWithJavascriptExpectingResult(CreateBasicSellAdScript(), 1);

  RunScoreAdWithReturnValueExpectingResult("3", 3);
  RunScoreAdWithReturnValueExpectingResult("0.5", 0.5);
  RunScoreAdWithReturnValueExpectingResult("0", 0);
  RunScoreAdWithReturnValueExpectingResult("-10", 0);

  // No return value.
  RunScoreAdWithReturnValueExpectingResult("", 0);

  // Wrong return type / invalid values.
  RunScoreAdWithReturnValueExpectingResult("[15]", 0);
  RunScoreAdWithReturnValueExpectingResult("1/0", 0);
  RunScoreAdWithReturnValueExpectingResult("0/0", 0);
  RunScoreAdWithReturnValueExpectingResult("-1/0", 0);
  RunScoreAdWithReturnValueExpectingResult("true", 0);

  // Throw exception.
  RunScoreAdWithReturnValueExpectingResult("shrimp", 0);
}

TEST_F(SellerWorkletTest, ScoreAdDateNotAvailable) {
  RunScoreAdWithReturnValueExpectingResult("Date.parse(Date().toString())", 0);
}

// Checks that input parameters are correctly passed in.
TEST_F(SellerWorkletTest, ScoreAdParameters) {
  // Parameters that are C++ strings, including JSON strings.
  const struct StringTestCase {
    // String used in JS to access the parameter.
    const char* name;
    bool is_json;
    // Pointer to location at which the string can be modified.
    std::string* value_ptr;
  } kStringTestCases[] = {
      {
          "adMetadata",
          true /* is_json */,
          &ad_metadata_,
      },
      {
          "browserSignals.topWindowHostname",
          false /* is_json */,
          &browser_signal_top_window_hostname_,
      },
      {
          "browserSignals.adRenderFingerprint",
          false /* is_json */,
          &browser_signal_ad_render_fingerprint_,
      },
  };

  for (const auto& test_case : kStringTestCases) {
    SCOPED_TRACE(test_case.name);

    *test_case.value_ptr = "foo";
    RunScoreAdWithReturnValueExpectingResult(
        base::StringPrintf(R"(%s == "foo" ? 1 : 2)", test_case.name),
        test_case.is_json ? 0 : 1);

    *test_case.value_ptr = R"("foo")";
    RunScoreAdWithReturnValueExpectingResult(
        base::StringPrintf(R"(%s == "foo" ? 1 : 2)", test_case.name),
        test_case.is_json ? 1 : 2);

    *test_case.value_ptr = "[1]";
    RunScoreAdWithReturnValueExpectingResult(
        base::StringPrintf(R"(%s[0] == 1 ? 4 : %s=="[1]" ? 3 : 0)",
                           test_case.name, test_case.name),
        test_case.is_json ? 4 : 3);
    SetDefaultParameters();
  }

  browser_signal_interest_group_owner_ =
      url::Origin::Create(GURL("https://foo.test/"));
  RunScoreAdWithReturnValueExpectingResult(
      R"(browserSignals.interestGroupOwner == "https://foo.test" ? 2 : 0)", 2);

  browser_signal_interest_group_owner_ =
      url::Origin::Create(GURL("https://[::1]:40000/"));
  RunScoreAdWithReturnValueExpectingResult(
      R"(browserSignals.interestGroupOwner == "https://[::1]:40000" ? 3 : 0)",
      3);
  SetDefaultParameters();

  // Test bid parameter.
  bid_ = 5;
  RunScoreAdWithReturnValueExpectingResult(base::StringPrintf("bid"), 5);
  bid_ = 0.5;
  RunScoreAdWithReturnValueExpectingResult(base::StringPrintf("bid"), 0.5);
  bid_ = -1;
  RunScoreAdWithReturnValueExpectingResult(base::StringPrintf("bid"), 0);
  SetDefaultParameters();

  // Test browserSignals.bidding_duration_msec.
  browser_signal_bidding_duration_ = base::TimeDelta();
  RunScoreAdWithReturnValueExpectingResult(
      base::StringPrintf("browserSignals.biddingDurationMsec"), 0);
  browser_signal_bidding_duration_ = base::TimeDelta::FromMilliseconds(100);
  RunScoreAdWithReturnValueExpectingResult(
      base::StringPrintf("browserSignals.biddingDurationMsec"), 100);

  // Make sure that submillisecond resolution is not available.
  browser_signal_bidding_duration_ = base::TimeDelta::FromMicroseconds(2400);
  RunScoreAdWithReturnValueExpectingResult(
      base::StringPrintf("browserSignals.biddingDurationMsec"), 2);
}

// Test that auction config gets into scoreAd. More detailed handling of
// (shared) construction of actual object is in ReportResultAuctionConfigParam,
// as that worklet is easier to get things out of.
TEST_F(SellerWorkletTest, ScoreAdAuctionConfigParam) {
  // Default value, no URL
  RunScoreAdWithReturnValueExpectingResult(
      "auctionConfig.decisionLogicUrl.length", 0);

  std::string url = "https://example.com/auction.js";
  auction_config_ = blink::mojom::AuctionAdConfig::New();
  auction_config_->seller = url::Origin::Create(GURL("https://example.com"));
  auction_config_->decision_logic_url = GURL(url);
  RunScoreAdWithReturnValueExpectingResult(
      "auctionConfig.decisionLogicUrl.length", url.length());
}

// Tests parsing of return values.
TEST_F(SellerWorkletTest, ReportResult) {
  RunReportResultCreatedScriptExpectingResult(
      "1", std::string() /* extra_code */, SellerWorklet::Report("1", GURL()));
  RunReportResultCreatedScriptExpectingResult(
      R"("  1   ")", std::string() /* extra_code */,
      SellerWorklet::Report(R"("  1   ")", GURL()));
  RunReportResultCreatedScriptExpectingResult(
      "[ null ]", std::string() /* extra_code */,
      SellerWorklet::Report("[null]", GURL()));

  // No return value.
  RunReportResultCreatedScriptExpectingResult(
      "", std::string() /* extra_code */,
      SellerWorklet::Report("null", GURL()));

  // Throw exception.
  RunReportResultCreatedScriptExpectingResult(
      "shrimp", std::string() /* extra_code */, SellerWorklet::Report());
}

// Tests reporting URLs.
TEST_F(SellerWorkletTest, ReportResultSendReportTo) {
  RunReportResultCreatedScriptExpectingResult(
      "1", R"(sendReportTo("https://foo.test"))",
      SellerWorklet::Report("1", GURL("https://foo.test/")));
  RunReportResultCreatedScriptExpectingResult(
      "1", R"(sendReportTo("https://foo.test/bar"))",
      SellerWorklet::Report("1", GURL("https://foo.test/bar")));

  // Disallowed schemes.
  RunReportResultCreatedScriptExpectingResult(
      "1", R"(sendReportTo("http://foo.test/"))", SellerWorklet::Report());
  RunReportResultCreatedScriptExpectingResult(
      "1", R"(sendReportTo("file:///foo/"))", SellerWorklet::Report());

  // Multiple calls.
  RunReportResultCreatedScriptExpectingResult(
      "1",
      R"(sendReportTo("https://foo.test/"); sendReportTo("https://foo.test/"))",
      SellerWorklet::Report());

  // Not a URL.
  RunReportResultCreatedScriptExpectingResult("1", R"(sendReportTo("France"))",
                                              SellerWorklet::Report());
  RunReportResultCreatedScriptExpectingResult("1", R"(sendReportTo(null))",
                                              SellerWorklet::Report());
  RunReportResultCreatedScriptExpectingResult("1", R"(sendReportTo([5]))",
                                              SellerWorklet::Report());
}

TEST_F(SellerWorkletTest, ReportResultDateNotAvailable) {
  RunReportResultCreatedScriptExpectingResult(
      "1", R"(sendReportTo("https://foo.test/" + Date().toString()))",
      SellerWorklet::Report());
}

TEST_F(SellerWorkletTest, ReportResultParameters) {
  // Parameters that are C++ strings, including JSON strings.
  const struct StringTestCase {
    // String used in JS to access the parameter.
    const char* name;
    bool is_json;
    // Pointer to location at which the string can be modified.
    std::string* value_ptr;
  } kStringTestCases[] = {
      {
          "browserSignals.topWindowHostname",
          false /* is_json */,
          &browser_signal_top_window_hostname_,
      },
      {
          "browserSignals.adRenderFingerprint",
          false /* is_json */,
          &browser_signal_ad_render_fingerprint_,
      },
      {
          "browserSignals.topWindowHostname",
          false /* is_json */,
          &browser_signal_top_window_hostname_,
      },
  };

  for (const auto& test_case : kStringTestCases) {
    SCOPED_TRACE(test_case.name);

    *test_case.value_ptr = "foo";
    RunReportResultCreatedScriptExpectingResult(
        base::StringPrintf(R"(%s == "foo" ? 2 : 1)", test_case.name),
        std::string() /* extra_code */,
        test_case.is_json ? SellerWorklet::Report()
                          : SellerWorklet::Report("2", GURL()));

    *test_case.value_ptr = R"("foo")";
    RunReportResultCreatedScriptExpectingResult(
        base::StringPrintf(R"(%s == "foo" ? 1 : 2)", test_case.name),
        std::string() /* extra_code */,
        test_case.is_json ? SellerWorklet::Report("1", GURL())
                          : SellerWorklet::Report("2", GURL()));

    *test_case.value_ptr = "[1]";
    RunReportResultCreatedScriptExpectingResult(
        base::StringPrintf(R"(%s[0] == 1 ? 4 : %s=="[1]" ? 3 : 0)",
                           test_case.name, test_case.name),
        std::string() /* extra_code */,
        test_case.is_json ? SellerWorklet::Report("4", GURL())
                          : SellerWorklet::Report("3", GURL()));

    SetDefaultParameters();
  }

  browser_signal_interest_group_owner_ =
      url::Origin::Create(GURL("https://foo.test/"));
  RunReportResultCreatedScriptExpectingResult(
      R"(browserSignals.interestGroupOwner == "https://foo.test" ? 2 : 1)",
      std::string() /* extra_code */, SellerWorklet::Report("2", GURL()));

  browser_signal_interest_group_owner_ =
      url::Origin::Create(GURL("https://[::1]:40000/"));
  RunReportResultCreatedScriptExpectingResult(
      R"(browserSignals.interestGroupOwner == "https://[::1]:40000" ? 3 : 1)",
      std::string() /* extra_code */, SellerWorklet::Report("3", GURL()));

  browser_signal_render_url_ = GURL("https://foo/");
  RunReportResultCreatedScriptExpectingResult(
      "browserSignals.renderUrl", "sendReportTo(browserSignals.renderUrl)",
      SellerWorklet::Report(R"("https://foo/")", browser_signal_render_url_));
  SetDefaultParameters();

  bid_ = 5;
  RunReportResultCreatedScriptExpectingResult(
      "browserSignals.bid + typeof browserSignals.bid",
      std::string() /* extra_code */,
      SellerWorklet::Report(R"("5number")", GURL()));
  SetDefaultParameters();

  browser_signal_desireability_ = 10;
  RunReportResultCreatedScriptExpectingResult(
      "browserSignals.desirability + typeof browserSignals.desirability",
      std::string() /* extra_code */,
      SellerWorklet::Report(R"("10number")", GURL()));
  SetDefaultParameters();
}

TEST_F(SellerWorkletTest, ReportResultAuctionConfigParam) {
  // Empty AuctionAdConfig, with nothing filled in.
  RunReportResultCreatedScriptExpectingResult(
      "auctionConfig", std::string() /* extra_code */,
      SellerWorklet::Report(R"({"seller":"null","decisionLogicUrl":""})",
                            GURL()));

  // Everything filled in.
  auction_config_ = blink::mojom::AuctionAdConfig::New();
  auction_config_->seller = url::Origin::Create(GURL("https://example.com"));
  auction_config_->decision_logic_url = GURL("https://example.com/auction.js");
  auction_config_->interest_group_buyers =
      blink::mojom::InterestGroupBuyers::NewAllBuyers(
          blink::mojom::AllBuyers::New());
  auction_config_->auction_signals = R"({"is_auction_signals": true})";
  auction_config_->seller_signals = R"({"is_seller_signals": true})";
  base::flat_map<url::Origin, std::string> per_buyer_signals;
  per_buyer_signals[url::Origin::Create(GURL("https://a.com"))] =
      R"({"signals_a": "A"})";
  per_buyer_signals[url::Origin::Create(GURL("https://b.com"))] =
      R"({"signals_b": "B"})";
  auction_config_->per_buyer_signals = std::move(per_buyer_signals);

  const char kExpectedJson[] =
      R"({"seller":"https://example.com",)"
      R"("decisionLogicUrl":"https://example.com/auction.js",)"
      R"("interestGroupBuyers":"*",)"
      R"("auctionSignals":{"is_auction_signals":true},)"
      R"("sellerSignals":{"is_seller_signals":true},)"
      R"("perBuyerSignals":{"a.com":{"signals_a":"A"},)"
      R"("b.com":{"signals_b":"B"}}})";
  RunReportResultCreatedScriptExpectingResult(
      "auctionConfig", std::string() /* extra_code */,
      SellerWorklet::Report(kExpectedJson, GURL()));

  // Array option for interest_group_buyers. Everything else optional
  // unpopulated.
  std::vector<url::Origin> buyers;
  buyers.push_back(url::Origin::Create(GURL("https://buyer1.com")));
  buyers.push_back(url::Origin::Create(GURL("https://another-buyer.com")));
  auction_config_ = blink::mojom::AuctionAdConfig::New();
  auction_config_->seller = url::Origin::Create(GURL("https://example.com"));
  auction_config_->decision_logic_url = GURL("https://example.com/auction.js");
  auction_config_->interest_group_buyers =
      blink::mojom::InterestGroupBuyers::NewBuyers(std::move(buyers));
  const char kExpectedJson2[] =
      R"({"seller":"https://example.com",)"
      R"("decisionLogicUrl":"https://example.com/auction.js",)"
      R"("interestGroupBuyers":["buyer1.com","another-buyer.com"]})";
  RunReportResultCreatedScriptExpectingResult(
      "auctionConfig", std::string() /* extra_code */,
      SellerWorklet::Report(kExpectedJson2, GURL()));
}

// Subsequent runs of the same script should not affect each other. Same is true
// for different scripts, but it follows from the single script case.
TEST_F(SellerWorkletTest, ScriptIsolation) {
  // Use arrays so that all values are references, to catch both the case where
  // variables are persisted, and the case where what they refer to is
  // persisted, but variables are overwritten between runs.
  AddJavascriptResponse(&url_loader_factory_, url_,
                        R"(
        // Globally scoped variable.
        if (!globalThis.var1)
          globalThis.var1 = [1];
        scoreAd = function() {
          // Value only visible within this closure.
          var var2 = [2];
          return function() {
            if (2 == ++globalThis.var1[0] && 3 == ++var2[0])
              return 2;
            return 1;
          }
        }();

        reportResult = scoreAd;
      )");
  auto seller_worket = CreateWorklet();
  ASSERT_TRUE(seller_worket);

  for (int i = 0; i < 3; ++i) {
    // Run each script twice in a row, to cover both cases where the same
    // function is run sequentially, and when one function is run after the
    // other.
    for (int j = 0; j < 2; ++j) {
      SellerWorklet::ScoreResult score_result =
          seller_worket->ScoreAd(ad_metadata_, bid_, *auction_config_,
                                 browser_signal_top_window_hostname_,
                                 browser_signal_interest_group_owner_,
                                 browser_signal_ad_render_fingerprint_,
                                 browser_signal_bidding_duration_);
      EXPECT_TRUE(score_result.success);
      EXPECT_EQ(2, score_result.score);
    }

    for (int j = 0; j < 2; ++j) {
      SellerWorklet::Report report = seller_worket->ReportResult(
          *auction_config_, browser_signal_top_window_hostname_,
          browser_signal_interest_group_owner_, browser_signal_render_url_,
          browser_signal_ad_render_fingerprint_, bid_,
          browser_signal_desireability_);
      EXPECT_TRUE(report.success);
      EXPECT_EQ("2", report.signals_for_winner);
    }
  }
}

}  // namespace
}  // namespace auction_worklet
