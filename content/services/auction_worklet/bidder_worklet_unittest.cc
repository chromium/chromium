// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/bidder_worklet.h"

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/common/private_aggregation_features.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "content/services/auction_worklet/worklet_devtools_debug_test_util.h"
#include "content/services/auction_worklet/worklet_test_util.h"
#include "content/services/auction_worklet/worklet_v8_debug_test_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "net/http/http_status_code.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

using testing::HasSubstr;
using testing::StartsWith;

namespace auction_worklet {
namespace {

using PrivateAggregationRequests = BidderWorklet::PrivateAggregationRequests;

// This was produced by running wat2wasm on this:
// (module
//  (global (export "test_const") i32 (i32.const 123))
// )
const uint8_t kToyWasm[] = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x06, 0x07, 0x01,
    0x7f, 0x00, 0x41, 0xfb, 0x00, 0x0b, 0x07, 0x0e, 0x01, 0x0a, 0x74,
    0x65, 0x73, 0x74, 0x5f, 0x63, 0x6f, 0x6e, 0x73, 0x74, 0x03, 0x00};

const char kWasmUrl[] = "https://foo.test/helper.wasm";

// Packs kToyWasm into a std::string.
std::string ToyWasm() {
  return std::string(reinterpret_cast<const char*>(kToyWasm),
                     std::size(kToyWasm));
}

// Creates generateBid() scripts with the specified result value, in raw
// Javascript. Allows returning generateBid() arguments, arbitrary values,
// incorrect types, etc.
std::string CreateGenerateBidScript(const std::string& raw_return_value,
                                    const std::string& extra_code = "") {
  constexpr char kGenerateBidScript[] = R"(
    function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                         trustedBiddingSignals, browserSignals,
                         directFromSellerSignals) {
      %s;
      return %s;
    }
  )";
  return base::StringPrintf(kGenerateBidScript, extra_code.c_str(),
                            raw_return_value.c_str());
}

// Returns a working script, primarily for testing failure cases where it
// should not be run.
static std::string CreateBasicGenerateBidScript() {
  return CreateGenerateBidScript(
      R"({ad: ["ad"], bid:1, render:"https://response.test/"})");
}

// Returns a working script which calls forDebuggingOnly.reportAdAuctionLoss()
// and forDebuggingOnly.reportAdAuctionWin() if corresponding url is provided.
static std::string CreateBasicGenerateBidScriptWithDebuggingReport(
    const std::string& extra_code) {
  return CreateGenerateBidScript(
      R"({ad: ["ad"], bid:1, render:"https://response.test/"})", extra_code);
}

// Creates bidder worklet script with a reportWin() function with the specified
// body, and the default generateBid() function.
std::string CreateReportWinScript(const std::string& function_body) {
  constexpr char kReportWinScript[] = R"(
    function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
                       browserSignals, directFromSellerSignals) {
      %s;
    }
  )";
  return CreateBasicGenerateBidScript() +
         base::StringPrintf(kReportWinScript, function_body.c_str());
}

// A GenerateBidClient that takes a callback to call in OnGenerateBid(), and
// optionally one for OnBiddingSignalsReceived(). If no callback for
// OnBiddingSignalsReceived() is provided, just invokes the
// OnBiddingSignalsReceivedCallback immediately.
class GenerateBidClientWithCallbacks : public mojom::GenerateBidClient {
 public:
  using OnBiddingSignalsReceivedCallback = base::OnceCallback<void(
      const base::flat_map<std::string, double>& priority_vector,
      base::TimeDelta trusted_signals_fetch_duration,
      base::OnceClosure callback)>;

  using GenerateBidCallback = base::OnceCallback<void(
      mojom::BidderWorkletBidPtr bid,
      mojom::BidderWorkletKAnonEnforcedBidPtr kanon_bid,
      uint32_t data_version,
      bool has_data_version,
      const absl::optional<GURL>& debug_loss_report_url,
      const absl::optional<GURL>& debug_win_report_url,
      double set_priority,
      bool has_set_priority,
      base::flat_map<std::string,
                     auction_worklet::mojom::PrioritySignalsDoublePtr>
          update_priority_signals_overrides,
      PrivateAggregationRequests pa_requests,
      base::TimeDelta bidding_duration,
      const std::vector<std::string>& errors)>;

  explicit GenerateBidClientWithCallbacks(
      GenerateBidCallback generate_bid_callback,
      OnBiddingSignalsReceivedCallback on_bidding_signals_received_callback =
          OnBiddingSignalsReceivedCallback())
      : on_bidding_signals_received_callback_(
            std::move(on_bidding_signals_received_callback)),
        generate_bid_callback_(std::move(generate_bid_callback)) {
    DCHECK(generate_bid_callback_);
  }

  ~GenerateBidClientWithCallbacks() override = default;

  // Helper that creates a GenerateBidClientWithCallbacks() using a
  // SelfOwnedReceived.
  static mojo::PendingAssociatedRemote<mojom::GenerateBidClient> Create(
      GenerateBidCallback callback,
      OnBiddingSignalsReceivedCallback on_bidding_signals_received_callback =
          OnBiddingSignalsReceivedCallback()) {
    mojo::PendingAssociatedRemote<mojom::GenerateBidClient> client_remote;
    mojo::MakeSelfOwnedAssociatedReceiver(
        std::make_unique<GenerateBidClientWithCallbacks>(
            std::move(callback),
            std::move(on_bidding_signals_received_callback)),
        client_remote.InitWithNewEndpointAndPassReceiver());
    return client_remote;
  }

  // Creates a GenerateBidClient() that expects OnGenerateBidComplete() never to
  // be invoked. Allows OnBiddingSignalsReceived() to be invoked.
  static mojo::PendingAssociatedRemote<mojom::GenerateBidClient>
  CreateNeverCompletes() {
    return Create(GenerateBidNeverInvokedCallback());
  }

  static GenerateBidCallback GenerateBidNeverInvokedCallback() {
    return base::BindOnce(
        [](mojom::BidderWorkletBidPtr bid,
           mojom::BidderWorkletKAnonEnforcedBidPtr kanon_bid,
           uint32_t data_version, bool has_data_version,
           const absl::optional<GURL>& debug_loss_report_url,
           const absl::optional<GURL>& debug_win_report_url,
           double set_priority, bool has_set_priority,
           base::flat_map<std::string,
                          auction_worklet::mojom::PrioritySignalsDoublePtr>
               update_priority_signals_overrides,
           PrivateAggregationRequests pa_requests,
           base::TimeDelta bidding_duration,
           const std::vector<std::string>& errors) {
          ADD_FAILURE() << "OnGenerateBidComplete should not be invoked.";
        });
  }

  // mojom::GenerateBidClient implementation:

  void OnBiddingSignalsReceived(
      const base::flat_map<std::string, double>& priority_vector,
      base::TimeDelta trusted_signals_fetch_duration,
      base::OnceClosure callback) override {
    // May only be called once.
    EXPECT_FALSE(on_bidding_signals_received_invoked_);
    on_bidding_signals_received_invoked_ = true;

    if (on_bidding_signals_received_callback_) {
      std::move(on_bidding_signals_received_callback_)
          .Run(priority_vector, trusted_signals_fetch_duration,
               std::move(callback));
      return;
    }
    std::move(callback).Run();
  }

  void OnGenerateBidComplete(
      mojom::BidderWorkletBidPtr bid,
      mojom::BidderWorkletKAnonEnforcedBidPtr kanon_bid,
      uint32_t data_version,
      bool has_data_version,
      const absl::optional<GURL>& debug_loss_report_url,
      const absl::optional<GURL>& debug_win_report_url,
      double set_priority,
      bool has_set_priority,
      base::flat_map<std::string,
                     auction_worklet::mojom::PrioritySignalsDoublePtr>
          update_priority_signals_overrides,
      PrivateAggregationRequests pa_requests,
      base::TimeDelta bidding_duration,
      const std::vector<std::string>& errors) override {
    // OnBiddingSignalsReceived() must be called first.
    EXPECT_TRUE(on_bidding_signals_received_invoked_);

    std::move(generate_bid_callback_)
        .Run(std::move(bid), std::move(kanon_bid), data_version,
             has_data_version, debug_loss_report_url, debug_win_report_url,
             set_priority, has_set_priority,
             std::move(update_priority_signals_overrides),
             std::move(pa_requests), bidding_duration, errors);
  }

 private:
  bool on_bidding_signals_received_invoked_ = false;
  OnBiddingSignalsReceivedCallback on_bidding_signals_received_callback_;
  GenerateBidCallback generate_bid_callback_;
};

class BidderWorkletTest : public testing::Test {
 public:
  BidderWorkletTest() { SetDefaultParameters(); }

  ~BidderWorkletTest() override = default;

  void SetUp() override {
    // v8_helper_ needs to be created here instead of the constructor, because
    // this test fixture has a subclass that initializes a ScopedFeatureList in
    // their constructor, which needs to be done BEFORE other threads are
    // started in multithreaded test environments so that no other threads use
    // it when it's being initiated.
    // https://source.chromium.org/chromium/chromium/src/+/main:base/test/scoped_feature_list.h;drc=60124005e97ae2716b0fb34187d82da6019b571f;l=37
    v8_helper_ = AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner());
  }

  void TearDown() override {
    // Release the V8 helper and process all pending tasks. This is to make sure
    // there aren't any pending tasks between the V8 thread and the main thread
    // that will result in UAFs. These lines are not necessary for any test to
    // pass. This needs to be done before a subclass resets ScopedFeatureList,
    // so no thread queries it while it's being modified.
    v8_helper_.reset();
    task_environment_.RunUntilIdle();
  }

  // Default values. No test actually depends on these being anything but valid,
  // but test that set these can use this to reset values to default after each
  // test.
  void SetDefaultParameters() {
    interest_group_name_ = "Fred";
    interest_group_enable_bidding_signals_prioritization_ = false;
    interest_group_priority_vector_.reset();
    interest_group_user_bidding_signals_ = absl::nullopt;
    join_origin_ = url::Origin::Create(GURL("https://url.test/"));
    execution_mode_ =
        blink::mojom::InterestGroup::ExecutionMode::kCompatibilityMode;

    interest_group_ads_.clear();
    interest_group_ads_.emplace_back(blink::InterestGroup::Ad(
        GURL("https://response.test/"), /*metadata=*/absl::nullopt));

    interest_group_ad_components_.reset();
    interest_group_ad_components_.emplace();
    interest_group_ad_components_->emplace_back(blink::InterestGroup::Ad(
        GURL("https://ad_component.test/"), /*metadata=*/absl::nullopt));

    kanon_keys_.clear();
    kanon_mode_ = auction_worklet::mojom::KAnonymityBidMode::kNone;
    provide_direct_from_seller_signals_late_ = false;

    daily_update_url_.reset();

    interest_group_trusted_bidding_signals_url_.reset();
    interest_group_trusted_bidding_signals_keys_.reset();

    browser_signal_join_count_ = 2;
    browser_signal_bid_count_ = 3;
    browser_signal_prev_wins_.clear();

    auction_signals_ = "[\"auction_signals\"]";
    per_buyer_signals_ = "[\"per_buyer_signals\"]";
    per_buyer_timeout_ = absl::nullopt;
    top_window_origin_ = url::Origin::Create(GURL("https://top.window.test/"));
    permissions_policy_state_ =
        mojom::AuctionWorkletPermissionsPolicyState::New(
            /*private_aggregation_allowed=*/true,
            /*shared_storage_allowed=*/true);
    experiment_group_id_ = absl::nullopt;
    browser_signal_seller_origin_ =
        url::Origin::Create(GURL("https://browser.signal.seller.test/"));
    browser_signal_top_level_seller_origin_.reset();
    seller_signals_ = "[\"seller_signals\"]";
    browser_signal_render_url_ = GURL("https://render_url.test/");
    browser_signal_bid_ = 1;
    browser_signal_highest_scoring_other_bid_ = 0.5;
    browser_signal_made_highest_scoring_other_bid_ = false;
    data_version_.reset();
  }

  // Helper that creates and runs a script to validate that `expression`
  // evaluates to true when evaluated in a generateBid() script. Does this by
  // evaluating the expression in the content of generateBid() and throwing if
  // it's not true. Otherwise, a bid is generated.
  void RunGenerateBidExpectingExpressionIsTrue(const std::string& expression) {
    std::string script = CreateGenerateBidScript(
        /*raw_return_value=*/R"({bid: 1, render:"https://response.test/"})",
        /*extra_code=*/base::StringPrintf(R"(let val = %s;
                                             if (val !== true)
                                             throw JSON.stringify(val);)",
                                          expression.c_str()));

    RunGenerateBidWithJavascriptExpectingResult(
        script, mojom::BidderWorkletBid::New(
                    /*ad=*/"null",
                    /*bid=*/1, GURL("https://response.test/"),
                    /*ad_components=*/absl::nullopt, base::TimeDelta()));
  }

  // Configures `url_loader_factory_` to return a generateBid() script with the
  // specified return line. Then runs the script, expecting the provided result.
  void RunGenerateBidWithReturnValueExpectingResult(
      const std::string& raw_return_value,
      mojom::BidderWorkletBidPtr expected_bid,
      const absl::optional<uint32_t>& expected_data_version = absl::nullopt,
      std::vector<std::string> expected_errors = std::vector<std::string>(),
      const absl::optional<GURL>& expected_debug_loss_report_url =
          absl::nullopt,
      const absl::optional<GURL>& expected_debug_win_report_url = absl::nullopt,
      const absl::optional<double> expected_set_priority = absl::nullopt,
      const base::flat_map<std::string, absl::optional<double>>&
          expected_update_priority_signals_overrides =
              base::flat_map<std::string, absl::optional<double>>(),
      PrivateAggregationRequests expected_pa_requests = {}) {
    RunGenerateBidWithJavascriptExpectingResult(
        CreateGenerateBidScript(raw_return_value), std::move(expected_bid),
        expected_data_version, expected_errors, expected_debug_loss_report_url,
        expected_debug_win_report_url, expected_set_priority,
        expected_update_priority_signals_overrides,
        std::move(expected_pa_requests));
  }

  // Configures `url_loader_factory_` to return a script with the specified
  // Javascript. Then runs the script, expecting the provided result.
  void RunGenerateBidWithJavascriptExpectingResult(
      const std::string& javascript,
      mojom::BidderWorkletBidPtr expected_bid,
      const absl::optional<uint32_t>& expected_data_version = absl::nullopt,
      std::vector<std::string> expected_errors = std::vector<std::string>(),
      const absl::optional<GURL>& expected_debug_loss_report_url =
          absl::nullopt,
      const absl::optional<GURL>& expected_debug_win_report_url = absl::nullopt,
      const absl::optional<double> expected_set_priority = absl::nullopt,
      const base::flat_map<std::string, absl::optional<double>>&
          expected_update_priority_signals_overrides =
              base::flat_map<std::string, absl::optional<double>>(),
      PrivateAggregationRequests expected_pa_requests = {}) {
    SCOPED_TRACE(javascript);
    AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                          javascript);
    RunGenerateBidExpectingResult(
        std::move(expected_bid), expected_data_version, expected_errors,
        expected_debug_loss_report_url, expected_debug_win_report_url,
        expected_set_priority, expected_update_priority_signals_overrides,
        std::move(expected_pa_requests));
  }

  // Loads and runs a generateBid() script, expecting the provided result.
  //
  // `bid_duration` of `expected_bid` is ignored unless it's non-zero, in which
  // case the duration is expected to be at least `bid_duration` - useful for
  // testing that `bid_duration` at least seems to reflect timeouts.
  void RunGenerateBidExpectingResult(
      mojom::BidderWorkletBidPtr expected_bid,
      const absl::optional<uint32_t>& expected_data_version = absl::nullopt,
      std::vector<std::string> expected_errors = std::vector<std::string>(),
      const absl::optional<GURL>& expected_debug_loss_report_url =
          absl::nullopt,
      const absl::optional<GURL>& expected_debug_win_report_url = absl::nullopt,
      const absl::optional<double> expected_set_priority = absl::nullopt,
      const base::flat_map<std::string, absl::optional<double>>&
          expected_update_priority_signals_overrides =
              base::flat_map<std::string, absl::optional<double>>(),
      PrivateAggregationRequests expected_pa_requests = {}) {
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
      if (!expected_bid->bid_duration.is_zero())
        EXPECT_GE(bid_->bid_duration, expected_bid->bid_duration);
    }
    EXPECT_EQ(expected_data_version, data_version_);
    EXPECT_EQ(expected_debug_loss_report_url, bid_debug_loss_report_url_);
    EXPECT_EQ(expected_debug_win_report_url, bid_debug_win_report_url_);
    EXPECT_EQ(expected_pa_requests, pa_requests_);
    EXPECT_EQ(expected_errors, bid_errors_);
    EXPECT_EQ(expected_set_priority, set_priority_);
    EXPECT_EQ(expected_update_priority_signals_overrides,
              update_priority_signals_overrides_);
  }

  // Configures `url_loader_factory_` to return a reportWin() script with the
  // specified body. Then runs the script, expecting the provided result.
  void RunReportWinWithFunctionBodyExpectingResult(
      const std::string& function_body,
      const absl::optional<GURL>& expected_report_url,
      const base::flat_map<std::string, GURL>& expected_ad_beacon_map =
          base::flat_map<std::string, GURL>(),
      PrivateAggregationRequests expected_pa_requests = {},
      const std::vector<std::string>& expected_errors =
          std::vector<std::string>()) {
    RunReportWinWithJavascriptExpectingResult(
        CreateReportWinScript(function_body), expected_report_url,
        expected_ad_beacon_map, std::move(expected_pa_requests),
        expected_errors);
  }

  // Configures `url_loader_factory_` to return a reportWin() script with the
  // specified Javascript. Then runs the script, expecting the provided result.
  void RunReportWinWithJavascriptExpectingResult(
      const std::string& javascript,
      const absl::optional<GURL>& expected_report_url,
      const base::flat_map<std::string, GURL>& expected_ad_beacon_map =
          base::flat_map<std::string, GURL>(),
      PrivateAggregationRequests expected_pa_requests = {},
      const std::vector<std::string>& expected_errors =
          std::vector<std::string>()) {
    SCOPED_TRACE(javascript);
    AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                          javascript);
    RunReportWinExpectingResult(expected_report_url, expected_ad_beacon_map,
                                std::move(expected_pa_requests),
                                expected_errors);
  }

  // Runs reportWin() on an already loaded worklet,  verifies the return
  // value and invokes `done_closure` when done. Expects something else to
  // spin the event loop.
  void RunReportWinExpectingResultAsync(
      mojom::BidderWorklet* bidder_worklet,
      const absl::optional<GURL>& expected_report_url,
      const base::flat_map<std::string, GURL>& expected_ad_beacon_map,
      PrivateAggregationRequests expected_pa_requests,
      const std::vector<std::string>& expected_errors,
      base::OnceClosure done_closure) {
    bidder_worklet->ReportWin(
        interest_group_name_, auction_signals_, per_buyer_signals_,
        direct_from_seller_per_buyer_signals_,
        direct_from_seller_auction_signals_, seller_signals_,
        browser_signal_render_url_, browser_signal_bid_,
        browser_signal_highest_scoring_other_bid_,
        browser_signal_made_highest_scoring_other_bid_,
        browser_signal_seller_origin_, browser_signal_top_level_seller_origin_,
        data_version_.value_or(0), data_version_.has_value(),
        /*trace_id=*/1,
        base::BindOnce(
            [](const absl::optional<GURL>& expected_report_url,
               const base::flat_map<std::string, GURL>& expected_ad_beacon_map,
               PrivateAggregationRequests expected_pa_requests,
               const std::vector<std::string>& expected_errors,
               base::OnceClosure done_closure,
               const absl::optional<GURL>& report_url,
               const base::flat_map<std::string, GURL>& ad_beacon_map,
               PrivateAggregationRequests pa_requests,
               const std::vector<std::string>& errors) {
              EXPECT_EQ(expected_report_url, report_url);
              EXPECT_EQ(expected_errors, errors);
              EXPECT_EQ(expected_ad_beacon_map, ad_beacon_map);
              EXPECT_EQ(expected_pa_requests, pa_requests);
              std::move(done_closure).Run();
            },
            expected_report_url, expected_ad_beacon_map,
            std::move(expected_pa_requests), expected_errors,
            std::move(done_closure)));
  }

  // Loads and runs a reportWin() with the provided return line, expecting the
  // supplied result.
  void RunReportWinExpectingResult(
      const absl::optional<GURL>& expected_report_url,
      const base::flat_map<std::string, GURL>& expected_ad_beacon_map =
          base::flat_map<std::string, GURL>(),
      PrivateAggregationRequests expected_pa_requests = {},
      const std::vector<std::string>& expected_errors =
          std::vector<std::string>()) {
    auto bidder_worklet = CreateWorklet();
    ASSERT_TRUE(bidder_worklet);

    base::RunLoop run_loop;
    RunReportWinExpectingResultAsync(bidder_worklet.get(), expected_report_url,
                                     expected_ad_beacon_map,
                                     std::move(expected_pa_requests),
                                     expected_errors, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Creates a BidderWorkletNonSharedParams based on test fixture
  // configuration.
  mojom::BidderWorkletNonSharedParamsPtr CreateBidderWorkletNonSharedParams() {
    std::vector<std::pair<auction_worklet::mojom::KAnonKeyPtr, bool>>
        kanon_keys;
    for (const auto& key : kanon_keys_) {
      kanon_keys.emplace_back(key.first.Clone(), key.second);
    }
    return mojom::BidderWorkletNonSharedParams::New(
        interest_group_name_,
        interest_group_enable_bidding_signals_prioritization_,
        interest_group_priority_vector_, execution_mode_, daily_update_url_,
        interest_group_trusted_bidding_signals_keys_,
        interest_group_user_bidding_signals_, interest_group_ads_,
        interest_group_ad_components_, std::move(kanon_keys));
  }

  // Creates a BiddingBrowserSignals based on test fixture configuration.
  mojom::BiddingBrowserSignalsPtr CreateBiddingBrowserSignals() {
    return mojom::BiddingBrowserSignals::New(
        browser_signal_join_count_, browser_signal_bid_count_,
        CloneWinList(browser_signal_prev_wins_));
  }

  // Create a BidderWorklet, returning the remote. If `out_bidder_worklet_impl`
  // is non-null, will also stash the actual implementation pointer there.
  // if `url` is empty, uses `interest_group_bidding_url_`.
  mojo::Remote<mojom::BidderWorklet> CreateWorklet(
      GURL url = GURL(),
      bool pause_for_debugger_on_start = false,
      BidderWorklet** out_bidder_worklet_impl = nullptr,
      bool use_alternate_url_loader_factory = false) {
    CHECK(!load_script_run_loop_);

    mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory;
    if (use_alternate_url_loader_factory) {
      alternate_url_loader_factory_.Clone(
          url_loader_factory.InitWithNewPipeAndPassReceiver());
    } else {
      url_loader_factory_.Clone(
          url_loader_factory.InitWithNewPipeAndPassReceiver());
    }

    auto bidder_worklet_impl = std::make_unique<BidderWorklet>(
        v8_helper_, std::move(shared_storage_host_remote_),
        pause_for_debugger_on_start, std::move(url_loader_factory),
        url.is_empty() ? interest_group_bidding_url_ : url,
        interest_group_wasm_url_, interest_group_trusted_bidding_signals_url_,
        top_window_origin_, permissions_policy_state_.Clone(),
        experiment_group_id_);
    auto* bidder_worklet_ptr = bidder_worklet_impl.get();
    mojo::Remote<mojom::BidderWorklet> bidder_worklet;
    mojo::ReceiverId receiver_id =
        bidder_worklets_.Add(std::move(bidder_worklet_impl),
                             bidder_worklet.BindNewPipeAndPassReceiver());
    bidder_worklet_ptr->set_close_pipe_callback(
        base::BindOnce(&BidderWorkletTest::ClosePipeCallback,
                       base::Unretained(this), receiver_id));
    bidder_worklet.set_disconnect_with_reason_handler(base::BindRepeating(
        &BidderWorkletTest::OnDisconnectWithReason, base::Unretained(this)));

    if (out_bidder_worklet_impl)
      *out_bidder_worklet_impl = bidder_worklet_ptr;
    return bidder_worklet;
  }

  // If no `generate_bid_client` is provided, uses one that invokes
  // GenerateBidCallback().
  void BeginGenerateBid(
      mojom::BidderWorklet* bidder_worklet,
      mojo::PendingAssociatedReceiver<mojom::GenerateBidFinalizer> finalizer,
      mojo::PendingAssociatedRemote<mojom::GenerateBidClient>
          generate_bid_client = mojo::NullAssociatedRemote()) {
    if (!generate_bid_client) {
      generate_bid_client =
          GenerateBidClientWithCallbacks::Create(base::BindOnce(
              &BidderWorkletTest::GenerateBidCallback, base::Unretained(this)));
    }
    bidder_worklet->BeginGenerateBid(
        CreateBidderWorkletNonSharedParams(), kanon_mode_, join_origin_,
        provide_direct_from_seller_signals_late_
            ? absl::nullopt
            : direct_from_seller_per_buyer_signals_,
        provide_direct_from_seller_signals_late_
            ? absl::nullopt
            : direct_from_seller_auction_signals_,
        browser_signal_seller_origin_, browser_signal_top_level_seller_origin_,
        CreateBiddingBrowserSignals(), auction_start_time_,
        /*trace_id=*/1, std::move(generate_bid_client), std::move(finalizer));
    bidder_worklet->SendPendingSignalsRequests();
  }

  // If no `generate_bid_client` is provided, uses one that invokes
  // GenerateBidCallback().
  void GenerateBid(mojom::BidderWorklet* bidder_worklet,
                   mojo::PendingAssociatedRemote<mojom::GenerateBidClient>
                       generate_bid_client = mojo::NullAssociatedRemote()) {
    mojo::AssociatedRemote<auction_worklet::mojom::GenerateBidFinalizer>
        bid_finalizer;
    BeginGenerateBid(bidder_worklet,
                     bid_finalizer.BindNewEndpointAndPassReceiver(),
                     std::move(generate_bid_client));
    bid_finalizer->FinishGenerateBid(auction_signals_, per_buyer_signals_,
                                     per_buyer_timeout_,
                                     provide_direct_from_seller_signals_late_
                                         ? direct_from_seller_per_buyer_signals_
                                         : absl::nullopt,
                                     provide_direct_from_seller_signals_late_
                                         ? direct_from_seller_auction_signals_
                                         : absl::nullopt);
  }

  // Calls BeginGenerateBid()/FinishGenerateBid(), expecting the
  // GenerateBidClient's OnGenerateBidComplete() method never to be invoked.
  void GenerateBidExpectingNeverCompletes(
      mojom::BidderWorklet* bidder_worklet) {
    mojo::AssociatedRemote<auction_worklet::mojom::GenerateBidFinalizer>
        bid_finalizer;
    bidder_worklet->BeginGenerateBid(
        CreateBidderWorkletNonSharedParams(), kanon_mode_, join_origin_,
        direct_from_seller_per_buyer_signals_,
        direct_from_seller_auction_signals_, browser_signal_seller_origin_,
        browser_signal_top_level_seller_origin_, CreateBiddingBrowserSignals(),
        auction_start_time_,
        /*trace_id=*/1, GenerateBidClientWithCallbacks::CreateNeverCompletes(),
        bid_finalizer.BindNewEndpointAndPassReceiver());
    bidder_worklet->SendPendingSignalsRequests();
    bid_finalizer->FinishGenerateBid(
        auction_signals_, per_buyer_signals_, per_buyer_timeout_,
        /*direct_from_seller_per_buyer_signals=*/absl::nullopt,
        /*direct_from_seller_auction_signals=*/absl::nullopt);
  }

  // Create a BidderWorklet and invokes BeginGenerateBid()/FinishGenerateBid(),
  // waiting for the GenerateBid() callback to be invoked. Returns a null
  // Remote on failure.
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

  void GenerateBidCallback(
      mojom::BidderWorkletBidPtr bid,
      mojom::BidderWorkletKAnonEnforcedBidPtr kanon_bid,
      uint32_t data_version,
      bool has_data_version,
      const absl::optional<GURL>& debug_loss_report_url,
      const absl::optional<GURL>& debug_win_report_url,
      double set_priority,
      bool has_set_priority,
      base::flat_map<std::string,
                     auction_worklet::mojom::PrioritySignalsDoublePtr>
          update_priority_signals_overrides,
      PrivateAggregationRequests pa_requests,
      base::TimeDelta bidding_duration,
      const std::vector<std::string>& errors) {
    absl::optional<uint32_t> maybe_data_version;
    if (has_data_version)
      maybe_data_version = data_version;
    absl::optional<double> maybe_set_priority;
    if (has_set_priority)
      maybe_set_priority = set_priority;
    bid_ = std::move(bid);
    kanon_bid_ = std::move(kanon_bid);
    data_version_ = maybe_data_version;
    bid_debug_loss_report_url_ = debug_loss_report_url;
    bid_debug_win_report_url_ = debug_win_report_url;
    set_priority_ = maybe_set_priority;

    update_priority_signals_overrides_.clear();
    for (const auto& override : update_priority_signals_overrides) {
      absl::optional<double> value;
      if (override.second)
        value = override.second->value;
      update_priority_signals_overrides_.emplace(override.first, value);
    }

    pa_requests_ = std::move(pa_requests);
    bid_errors_ = errors;
    load_script_run_loop_->Quit();
  }

  // Waits for OnDisconnectWithReason() to be invoked, if it hasn't been
  // already, and returns the error string it was invoked with.
  std::string WaitForDisconnect() {
    DCHECK(!disconnect_run_loop_);

    if (!disconnect_reason_) {
      disconnect_run_loop_ = std::make_unique<base::RunLoop>();
      disconnect_run_loop_->Run();
      disconnect_run_loop_.reset();
    }

    DCHECK(disconnect_reason_);
    std::string disconnect_reason = std::move(disconnect_reason_).value();
    disconnect_reason_.reset();
    return disconnect_reason;
  }

 protected:
  void ClosePipeCallback(mojo::ReceiverId receiver_id,
                         const std::string& description) {
    bidder_worklets_.RemoveWithReason(receiver_id, /*custom_reason_code=*/0,
                                      description);
  }

  void OnDisconnectWithReason(uint32_t custom_reason,
                              const std::string& description) {
    DCHECK(!disconnect_reason_);

    disconnect_reason_ = description;
    if (disconnect_run_loop_)
      disconnect_run_loop_->Quit();
  }

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
  //
  // NOTE: For each new GURL field, GenerateBidLoadCompletionOrder /
  // ReportWinLoadCompletionOrder should be updated.
  std::string interest_group_name_;
  bool interest_group_enable_bidding_signals_prioritization_;
  absl::optional<base::flat_map<std::string, double>>
      interest_group_priority_vector_;
  GURL interest_group_bidding_url_ = GURL("https://url.test/");
  url::Origin join_origin_;
  blink::mojom::InterestGroup::ExecutionMode execution_mode_ =
      blink::mojom::InterestGroup::ExecutionMode::kCompatibilityMode;
  absl::optional<GURL> interest_group_wasm_url_;
  absl::optional<std::string> interest_group_user_bidding_signals_;
  std::vector<blink::InterestGroup::Ad> interest_group_ads_;
  absl::optional<std::vector<blink::InterestGroup::Ad>>
      interest_group_ad_components_;
  base::flat_map<auction_worklet::mojom::KAnonKeyPtr, bool> kanon_keys_;
  auction_worklet::mojom::KAnonymityBidMode kanon_mode_ =
      auction_worklet::mojom::KAnonymityBidMode::kNone;
  absl::optional<GURL> daily_update_url_;
  absl::optional<GURL> interest_group_trusted_bidding_signals_url_;
  absl::optional<std::vector<std::string>>
      interest_group_trusted_bidding_signals_keys_;
  int browser_signal_join_count_;
  int browser_signal_bid_count_;
  std::vector<mojo::StructPtr<mojom::PreviousWin>> browser_signal_prev_wins_;

  absl::optional<std::string> auction_signals_;
  absl::optional<std::string> per_buyer_signals_;
  absl::optional<GURL> direct_from_seller_per_buyer_signals_;
  absl::optional<GURL> direct_from_seller_auction_signals_;
  absl::optional<base::TimeDelta> per_buyer_timeout_;
  url::Origin top_window_origin_;
  mojom::AuctionWorkletPermissionsPolicyStatePtr permissions_policy_state_;
  absl::optional<uint16_t> experiment_group_id_;
  url::Origin browser_signal_seller_origin_;
  absl::optional<url::Origin> browser_signal_top_level_seller_origin_;

  bool provide_direct_from_seller_signals_late_ = false;

  std::string seller_signals_;
  // Used for both the output GenerateBid(), and the input of ReportWin().
  absl::optional<uint32_t> data_version_;
  GURL browser_signal_render_url_;
  double browser_signal_bid_;
  double browser_signal_highest_scoring_other_bid_;
  bool browser_signal_made_highest_scoring_other_bid_;

  // Use a single constant start time. Only delta times are provided to scripts,
  // relative to the time of the auction, so no need to vary the auction time.
  const base::Time auction_start_time_ = base::Time::Now();

  // Reuseable run loop for loading the script. It's always populated after
  // creating the worklet, to cause a crash if the callback is invoked
  // synchronously.
  std::unique_ptr<base::RunLoop> load_script_run_loop_;

  // Values passed to the GenerateBidCallback().
  mojom::BidderWorkletBidPtr bid_;
  mojom::BidderWorkletKAnonEnforcedBidPtr kanon_bid_;
  absl::optional<GURL> bid_debug_loss_report_url_;
  absl::optional<GURL> bid_debug_win_report_url_;
  absl::optional<double> set_priority_;
  // Uses absl::optional<double> instead of the Mojo type to be more
  // user-friendly.
  base::flat_map<std::string, absl::optional<double>>
      update_priority_signals_overrides_;
  PrivateAggregationRequests pa_requests_;
  std::vector<std::string> bid_errors_;

  network::TestURLLoaderFactory url_loader_factory_;
  network::TestURLLoaderFactory alternate_url_loader_factory_;
  scoped_refptr<AuctionV8Helper> v8_helper_;

  mojo::PendingRemote<mojom::AuctionSharedStorageHost>
      shared_storage_host_remote_;

  // Reuseable run loop for disconnection errors.
  std::unique_ptr<base::RunLoop> disconnect_run_loop_;
  absl::optional<std::string> disconnect_reason_;

  // Owns all created BidderWorklets - having a ReceiverSet allows them to have
  // a ClosePipeCallback which behaves just like the one in
  // AuctionWorkletServiceImpl, to better match production behavior.
  mojo::UniqueReceiverSet<mojom::BidderWorklet> bidder_worklets_;
};

// Test the case the BidderWorklet pipe is closed before invoking the
// GenerateBidCallback. The invocation of the GenerateBidCallback is not
// observed, since the callback is on the pipe that was just closed. There
// should be no Mojo exception due to destroying the creation callback without
// invoking it.
TEST_F(BidderWorkletTest, PipeClosed) {
  auto bidder_worklet = CreateWorklet();
  GenerateBidExpectingNeverCompletes(bidder_worklet.get());
  bidder_worklet.reset();
  EXPECT_FALSE(bidder_worklets_.empty());

  // This should not result in a Mojo crash.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(bidder_worklets_.empty());
}

TEST_F(BidderWorkletTest, NetworkError) {
  url_loader_factory_.AddResponse(interest_group_bidding_url_.spec(),
                                  CreateBasicGenerateBidScript(),
                                  net::HTTP_NOT_FOUND);
  auto bidder_worklet = CreateWorklet();
  GenerateBidExpectingNeverCompletes(bidder_worklet.get());
  EXPECT_EQ("Failed to load https://url.test/ HTTP status = 404 Not Found.",
            WaitForDisconnect());
}

TEST_F(BidderWorkletTest, CompileError) {
  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        "Invalid Javascript");
  auto bidder_worklet = CreateWorklet();
  GenerateBidExpectingNeverCompletes(bidder_worklet.get());

  std::string error = WaitForDisconnect();
  EXPECT_THAT(error, StartsWith("https://url.test/:1 "));
  EXPECT_THAT(error, HasSubstr("SyntaxError"));
}

TEST_F(BidderWorkletTest, GenerateBidReturnValueAd) {
  // Missing `ad` field should be treated as null.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New("null", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()));

  // Explicitly setting an undefined ad value acts just like not setting an ad
  // value.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: globalThis.not_defined, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New("null", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()));

  // Make sure "ad" can be of a variety of JS object types.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New("\"ad\"", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          R"(["ad"])", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));
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
      R"({ad: function() {return 1;}, bid:1, render:"https://response.test/"})",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ generateBid() bid has invalid ad value."});

  // Make sure recursive structures aren't allowed in ad field.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(
        function generateBid() {
          var a = [];
          a[0] = a;
          return {ad: a, bid:1, render:"https://response.test/"};
        }
      )",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ generateBid() bid has invalid ad value."});
}

TEST_F(BidderWorkletTest, GenerateBidReturnValueBid) {
  // Undefined / an empty return statement and null are treated as not bidding.
  RunGenerateBidWithReturnValueExpectingResult(
      "",
      /*expected_bid=*/mojom::BidderWorkletBidPtr());
  RunGenerateBidWithReturnValueExpectingResult(
      "null",
      /*expected_bid=*/mojom::BidderWorkletBidPtr());

  // Missing bid value.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", render:"https://response.test/"})",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ generateBid() returned object must have numeric bid "
       "field."});

  // Valid positive bid values.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New("\"ad\"", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()));
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
      /*expected_bid=*/mojom::BidderWorkletBidPtr());
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:-10, render:"https://response.test/"})",
      /*expected_bid=*/mojom::BidderWorkletBidPtr());
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:-1.5, render:"https://response.test/"})",
      /*expected_bid=*/mojom::BidderWorkletBidPtr());

  // Infinite and NaN bid.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1/0, render:"https://response.test/"})",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ generateBid() bid of inf is not a valid bid."});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:-1/0, render:"https://response.test/"})",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ generateBid() bid of -inf is not a valid bid."});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:0/0, render:"https://response.test/"})",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ generateBid() bid of nan is not a valid bid."});

  // Non-numeric bid.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:"1", render:"https://response.test/"})",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ generateBid() returned object must have numeric bid "
       "field."});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:[1], render:"https://response.test/"})",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ generateBid() returned object must have numeric bid "
       "field."});
}

TEST_F(BidderWorkletTest, GenerateBidReturnValueUrl) {
  // Missing value.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1})",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ generateBid() bid has incorrect structure."});

  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          "[\"ad\"]", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));

  // Missing value with bid <= 0 is considered a valid no-bid case.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({bid:0})",
      /*expected_bid=*/mojom::BidderWorkletBidPtr());
  RunGenerateBidWithReturnValueExpectingResult(
      R"({bid:-1})",
      /*expected_bid=*/mojom::BidderWorkletBidPtr());

  // Disallowed render schemes.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:"http://response.test/"})",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ generateBid() bid render URL 'http://response.test/' "
       "isn't a valid https:// URL."});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:"chrome-extension://response.test/"})",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ generateBid() bid render URL "
       "'chrome-extension://response.test/' isn't a valid https:// URL."});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:"about:blank"})",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ generateBid() bid render URL 'about:blank' isn't a "
       "valid https:// URL."});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:"data:,foo"})",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ generateBid() bid render URL 'data:,foo' isn't a "
       "valid https:// URL."});

  // Invalid render URLs.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:"test"})",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ generateBid() bid render URL '' isn't a valid "
       "https:// URL."});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:"http://"})",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ generateBid() bid render URL 'http:' isn't a valid "
       "https:// URL."});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:["http://response.test/"]})",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ generateBid() bid has incorrect structure."});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:9})",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ generateBid() bid has incorrect structure."});
}

TEST_F(BidderWorkletTest, GenerateBidReturnValueAdComponents) {
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
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ generateBid() bid contains adComponents but "
       "InterestGroup has no adComponents."});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad",
        bid:1,
        render:"https://response.test/",
        adComponents:[]})",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ generateBid() bid contains adComponents but "
       "InterestGroup has no adComponents."});

  // Auction should fail if adComponents in return value is an unexpected type.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad",
        bid:1,
        render:"https://response.test/",
        adComponents:5})",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ generateBid() bid contains adComponents but "
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
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ generateBid() bid adComponents value must be an "
       "array."});

  // Unexpected value types in adComponents should fail.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad",
        bid:1,
        render:"https://response.test/",
        adComponents:[{}]})",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ generateBid() bid adComponents value must be an "
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
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ generateBid() bid adComponents with over 20 "
       "items."});
}

TEST_F(BidderWorkletTest, GenerateBidReturnValueInvalid) {
  // Valid JS, but missing function.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(
        function someOtherFunction() {
          return {ad: ["ad"], bid:1, render:"https://response.test/"};
        }
      )",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ `generateBid` is not a function."});
  RunGenerateBidWithJavascriptExpectingResult(
      "", /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ `generateBid` is not a function."});
  RunGenerateBidWithJavascriptExpectingResult(
      "5", /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ `generateBid` is not a function."});

  // Throw exception.
  RunGenerateBidWithJavascriptExpectingResult(
      "shrimp", /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:1 Uncaught ReferenceError: "
       "shrimp is not defined."});
}

// Test parsing of setBid arguments.
TEST_F(BidderWorkletTest, GenerateBidSetBidThrows) {
  // --------
  // Vary ad
  // --------

  // Other values JSON can't represent result in failing instead of null.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({ad: function() {return 1;}, bid:1, render:"https://response.test/"});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:2 Uncaught TypeError: bid has invalid ad value."});

  // Make sure recursive structures aren't allowed in ad field.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(
        function generateBid() {
          var a = [];
          a[0] = a;
          setBid({ad: a, bid:1, render:"https://response.test/"});
          return {};
        }
      )",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:5 Uncaught TypeError: bid has invalid ad value."});

  // --------
  // Vary bid
  // --------

  // Non-numeric bid.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({ad: ["ad"], bid:"1", render:"https://response.test/"});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:2 Uncaught TypeError: returned object must have "
       "numeric bid field."});
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({ad: ["ad"], bid:[1], render:"https://response.test/"});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:2 Uncaught TypeError: returned object must have "
       "numeric bid field."});

  // ---------
  // Vary URL.
  // ---------

  // Disallowed render schemes.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({ad: ["ad"], bid:1, render:"http://response.test/"});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:2 Uncaught TypeError: bid render URL "
       "'http://response.test/' isn't a valid https:// URL."});
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({ad: ["ad"],
                 bid:1,
                 render:"chrome-extension://response.test/"});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:2 Uncaught TypeError: bid render URL "
       "'chrome-extension://response.test/' isn't a valid https:// URL."});
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({ad: ["ad"], bid:1, render:"about:blank"});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:2 Uncaught TypeError: bid render URL 'about:blank' "
       "isn't a valid https:// URL."});
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({ad: ["ad"], bid:1, render:"data:,foo"});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:2 Uncaught TypeError: bid render URL 'data:,foo' "
       "isn't a valid https:// URL."});

  // Invalid render URLs.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({ad: ["ad"], bid:1, render:"test"});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:2 Uncaught TypeError: bid render URL '' isn't a "
       "valid https:// URL."});
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({ad: ["ad"], bid:1, render:"http://"});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:2 Uncaught TypeError: bid render URL 'http:' isn't a "
       "valid https:// URL."});
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({ad: ["ad"], bid:1, render:["http://response.test/"]});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:2 Uncaught TypeError: bid has incorrect structure."});
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({ad: ["ad"], bid:1, render:9});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:2 Uncaught TypeError: bid has incorrect structure."});

  // ----------------------
  // No adComponents in IG.
  // ----------------------

  interest_group_ad_components_ = absl::nullopt;

  // Auction should fail if adComponents in return value is an array.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({ad: "ad",
                 bid:1,
                 render:"https://response.test/",
                 adComponents:["http://response.test/"]});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:2 Uncaught TypeError: bid contains adComponents but "
       "InterestGroup has no adComponents."});
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({ad: "ad",
                 bid:1,
                 render:"https://response.test/",
                 adComponents:[]});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:2 Uncaught TypeError: bid contains adComponents but "
       "InterestGroup has no adComponents."});

  // Auction should fail if adComponents in return value is an unexpected type.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({ad: "ad",
                 bid:1,
                 render:"https://response.test/",
                 adComponents:5});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:2 Uncaught TypeError: bid contains adComponents but "
       "InterestGroup has no adComponents."});

  SetDefaultParameters();

  // -----------------------------
  // Non-empty adComponents in IG.
  // -----------------------------

  // Auction should fail if adComponents in return value is an unexpected type.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({ad: "ad",
                 bid:1,
                 render:"https://response.test/",
                 adComponents:5});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:2 Uncaught TypeError: bid adComponents value must be "
       "an array."});

  // Unexpected value types in adComponents should fail.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({ad: "ad",
                 bid:1,
                 render:"https://response.test/",
                 adComponents:[{}]});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:2 Uncaught TypeError: bid adComponents value must be "
       "an array of strings."});

  // Up to 20 values in the output adComponents output array are allowed (And
  // they can all be the same URL).
  static_assert(blink::kMaxAdAuctionAdComponents == 20,
                "Unexpected value of kMaxAdAuctionAdComponents");

  // Results with 21 or more values are rejected.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({ad: "ad",
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
        ]});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:2 Uncaught TypeError: bid adComponents with over 20 "
       "items."});

  // ------------
  // Other cases.
  // ------------

  // No return value.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid(1);
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:2 Uncaught TypeError: bid not an object."});

  // Missing value.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({bid:"a", render:"https://response.test/"});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:2 Uncaught TypeError: returned object must have "
       "numeric bid field."});
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({ad: ["ad"], render:"https://response.test/"});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:2 Uncaught TypeError: returned object must have "
       "numeric bid field."});
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({ad: ["ad"], bid:"a"});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:2 Uncaught TypeError: returned object must have "
       "numeric bid field."});
  // Setting a valid bid with setBid(), followed by setting an invalid bid that
  // throws, should result in no bid being set.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({bid:1, render:"https://response.test/"});
         setBid({ad: ["ad"], bid:"1", render:"https://response.test/"});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:3 Uncaught TypeError: returned object must have "
       "numeric bid field."});
}

// Make sure Date() is not available when running generateBid().
TEST_F(BidderWorkletTest, GenerateBidDateNotAvailable) {
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: Date().toString(), bid:1, render:"https://response.test/"})",
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:6 Uncaught ReferenceError: Date is not defined."});
}

TEST_F(BidderWorkletTest, GenerateBidInterestGroupOwner) {
  interest_group_bidding_url_ = GURL("https://foo.test/bar");
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: interestGroup.owner, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          R"("https://foo.test")", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));

  interest_group_bidding_url_ = GURL("https://[::1]:40000/");
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: interestGroup.owner, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          R"("https://[::1]:40000")", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));
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

TEST_F(BidderWorkletTest,
       GenerateBidInterestGroupUseBiddingSignalsPrioritization) {
  RunGenerateBidExpectingExpressionIsTrue(
      "interestGroup.useBiddingSignalsPrioritization === false");

  interest_group_enable_bidding_signals_prioritization_ = true;
  RunGenerateBidExpectingExpressionIsTrue(
      "interestGroup.useBiddingSignalsPrioritization === true");
}

TEST_F(BidderWorkletTest, GenerateBidInterestGroupPriorityVector) {
  RunGenerateBidExpectingExpressionIsTrue(
      "interestGroup.priorityVector === undefined");

  interest_group_priority_vector_.emplace();
  RunGenerateBidExpectingExpressionIsTrue(
      "Object.keys(interestGroup.priorityVector).length === 0");

  interest_group_priority_vector_->emplace("foo", 4);
  RunGenerateBidExpectingExpressionIsTrue(
      "Object.keys(interestGroup.priorityVector).length === 1");
  RunGenerateBidExpectingExpressionIsTrue(
      "interestGroup.priorityVector['foo'] === 4");

  interest_group_priority_vector_->emplace("bar", -5);
  RunGenerateBidExpectingExpressionIsTrue(
      "Object.keys(interestGroup.priorityVector).length === 2");
  RunGenerateBidExpectingExpressionIsTrue(
      "interestGroup.priorityVector['foo'] === 4");
  RunGenerateBidExpectingExpressionIsTrue(
      "interestGroup.priorityVector['bar'] === -5");
}

TEST_F(BidderWorkletTest, GenerateBidInterestGroupBiddingLogicUrl) {
  const std::string kGenerateBidBody =
      R"({ad: interestGroup.biddingLogicUrl, bid:1,
        render:"https://response.test/"})";

  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          R"("https://url.test/")", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));

  interest_group_bidding_url_ = GURL("https://url.test/foo");
  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          R"("https://url.test/foo")", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));
}

TEST_F(BidderWorkletTest, GenerateBidInterestGroupBiddingWasmHelperUrl) {
  const std::string kGenerateBidBody =
      R"({ad: "biddingWasmHelperUrl" in interestGroup ?
            interestGroup.biddingWasmHelperUrl : "missing",
        bid:1,
        render:"https://response.test/"})";

  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          R"("missing")", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));

  interest_group_wasm_url_ = GURL(kWasmUrl);
  AddResponse(&url_loader_factory_, GURL(kWasmUrl), kWasmMimeType,
              /*charset=*/absl::nullopt, ToyWasm());
  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(R"("https://foo.test/helper.wasm")", 1,
                                   GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()));
}

TEST_F(BidderWorkletTest, GenerateBidInterestGroupDailyUpdateUrl) {
  const std::string kGenerateBidBody =
      R"({ad: "dailyUpdateUrl" in interestGroup ?
            interestGroup.dailyUpdateUrl : "missing",
        bid:1,
        render:"https://response.test/"})";

  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          R"("missing")", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));

  daily_update_url_ = GURL("https://url.test/daily_update");
  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(R"("https://url.test/daily_update")", 1,
                                   GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()));
}

TEST_F(BidderWorkletTest, GenerateBidInterestGroupTrustedBiddingSignalsUrl) {
  const std::string kGenerateBidBody =
      R"({ad: "trustedBiddingSignalsUrl" in interestGroup ?
            interestGroup.trustedBiddingSignalsUrl : "missing",
        bid:1,
        render:"https://response.test/"})";

  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          R"("missing")", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));

  interest_group_trusted_bidding_signals_url_ =
      GURL("https://signals.test/foo.json");
  // Need trusted signals response for the next test case, to prevent it from
  // hanging.
  AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(interest_group_trusted_bidding_signals_url_->spec() +
           "?hostname=top.window.test&interestGroupNames=Fred"),
      "{}");
  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(R"("https://signals.test/foo.json")", 1,
                                   GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()));
}

TEST_F(BidderWorkletTest, GenerateBidInterestGroupTrustedBiddingSignalsKeys) {
  const std::string kGenerateBidBody =
      R"({ad: "trustedBiddingSignalsKeys" in interestGroup ?
            interestGroup.trustedBiddingSignalsKeys : "missing",
        bid:1,
        render:"https://response.test/"})";

  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          R"("missing")", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));

  // 0-length but non-null key list.
  interest_group_trusted_bidding_signals_keys_.emplace();
  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(R"([])", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()));

  interest_group_trusted_bidding_signals_keys_->push_back("2");
  interest_group_trusted_bidding_signals_keys_->push_back("1");
  interest_group_trusted_bidding_signals_keys_->push_back("3");
  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          R"(["2","1","3"])", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));
}

TEST_F(BidderWorkletTest, GenerateBidInterestGroupUserBiddingSignals) {
  const std::string kGenerateBidBody =
      R"({ad: interestGroup.userBiddingSignals, bid:1, render:"https://response.test/"})";

  // Since UserBiddingSignals are in JSON, non-JSON like standalone string
  // literals should not come in, but we are required to hangle such cases
  // gracefully, since it's ultimately data from the renderer. In this case it
  // just turns into null.
  interest_group_user_bidding_signals_ = "foo";
  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New("null", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()));

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
      mojo::AssociatedRemote<auction_worklet::mojom::GenerateBidFinalizer>
          bid_finalizer;
      bidder_worklet->BeginGenerateBid(
          CreateBidderWorkletNonSharedParams(), kanon_mode_, join_origin_,
          direct_from_seller_per_buyer_signals_,
          direct_from_seller_auction_signals_, browser_signal_seller_origin_,
          browser_signal_top_level_seller_origin_,
          CreateBiddingBrowserSignals(), auction_start_time_,
          /*trace_id=*/1,
          GenerateBidClientWithCallbacks::Create(base::BindLambdaForTesting(
              [&run_loop, &num_generate_bid_calls, bid_value](
                  mojom::BidderWorkletBidPtr bid,
                  mojom::BidderWorkletKAnonEnforcedBidPtr kanon_bid,
                  uint32_t data_version, bool has_data_version,
                  const absl::optional<GURL>& debug_loss_report_url,
                  const absl::optional<GURL>& debug_win_report_url,
                  double set_priority, bool has_set_priority,
                  base::flat_map<
                      std::string,
                      auction_worklet::mojom::PrioritySignalsDoublePtr>
                      update_priority_signals_overrides,
                  PrivateAggregationRequests pa_requests,
                  base::TimeDelta bidding_duration,
                  const std::vector<std::string>& errors) {
                EXPECT_EQ(bid_value, bid->bid);
                EXPECT_EQ(base::NumberToString(bid_value), bid->ad);
                EXPECT_EQ(GURL("https://response.test/"), bid->render_url);
                EXPECT_FALSE(kanon_bid);
                EXPECT_FALSE(has_data_version);
                EXPECT_TRUE(errors.empty());
                ++num_generate_bid_calls;
                if (num_generate_bid_calls == kNumGenerateBidCalls)
                  run_loop.Quit();
              })),
          bid_finalizer.BindNewEndpointAndPassReceiver());
      bid_finalizer->FinishGenerateBid(
          /*auction_signals_json=*/base::NumberToString(bid_value),
          per_buyer_signals_, per_buyer_timeout_,
          /*direct_from_seller_per_buyer_signals=*/absl::nullopt,
          /*direct_from_seller_auction_signals=*/absl::nullopt);
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
// the script fails to load.
TEST_F(BidderWorkletTest, GenerateBidParallelLoadFails) {
  auto bidder_worklet = CreateWorklet();

  for (size_t i = 0; i < 10; ++i) {
    GenerateBidExpectingNeverCompletes(bidder_worklet.get());
  }

  // Script fails to load.
  url_loader_factory_.AddResponse(interest_group_bidding_url_.spec(),
                                  CreateBasicGenerateBidScript(),
                                  net::HTTP_NOT_FOUND);

  EXPECT_EQ("Failed to load https://url.test/ HTTP status = 404 Not Found.",
            WaitForDisconnect());
}

// Test multiple GenerateBid calls on a single worklet, in parallel, in the case
// there are trusted bidding signals.
//
// In this test, the ordering is:
// 1) GenerateBid() calls are made.
// 2) The worklet script load completes.
// 3) The trusted bidding signals are loaded.
TEST_F(BidderWorkletTest, GenerateBidTrustedBiddingSignalsParallelBatched1) {
  interest_group_trusted_bidding_signals_url_ = GURL("https://signals.test/");
  interest_group_trusted_bidding_signals_keys_.emplace();

  // Each GenerateBid() call provides a different `auctionSignals` value. The
  // `i` generateBid() call should have trusted scoring signals of {"i":i+1},
  // and this function uses the key as the "ad" parameter, and the value as the
  // bid.
  const char kBidderScriptReturnValue[] = R"({
    ad: Number(Object.keys(trustedBiddingSignals)[0]),
    bid: trustedBiddingSignals[Object.keys(trustedBiddingSignals)[0]],
    render:"https://response.test/"
  })";

  auto bidder_worklet = CreateWorklet();

  // 1) GenerateBid() calls are made.
  base::RunLoop run_loop;
  const size_t kNumGenerateBidCalls = 10;
  size_t num_generate_bid_calls = 0;
  for (size_t i = 0; i < kNumGenerateBidCalls; ++i) {
    // Append a different key for each request.
    auto interest_group_fields = CreateBidderWorkletNonSharedParams();
    interest_group_fields->trusted_bidding_signals_keys->push_back(
        base::NumberToString(i));
    mojo::AssociatedRemote<auction_worklet::mojom::GenerateBidFinalizer>
        bid_finalizer;
    bidder_worklet->BeginGenerateBid(
        std::move(interest_group_fields), kanon_mode_, join_origin_,
        direct_from_seller_per_buyer_signals_,
        direct_from_seller_auction_signals_, browser_signal_seller_origin_,
        browser_signal_top_level_seller_origin_, CreateBiddingBrowserSignals(),
        auction_start_time_,
        /*trace_id=*/1,
        GenerateBidClientWithCallbacks::Create(base::BindLambdaForTesting(
            [&run_loop, &num_generate_bid_calls, i](
                mojom::BidderWorkletBidPtr bid,
                mojom::BidderWorkletKAnonEnforcedBidPtr kanon_bid,
                uint32_t data_version, bool has_data_version,
                const absl::optional<GURL>& debug_loss_report_url,
                const absl::optional<GURL>& debug_win_report_url,
                double set_priority, bool has_set_priority,
                base::flat_map<std::string,
                               auction_worklet::mojom::PrioritySignalsDoublePtr>
                    update_priority_signals_overrides,
                PrivateAggregationRequests pa_requests,
                base::TimeDelta bidding_duration,
                const std::vector<std::string>& errors) {
              EXPECT_EQ(base::NumberToString(i), bid->ad);
              EXPECT_EQ(i + 1, bid->bid);
              EXPECT_EQ(GURL("https://response.test/"), bid->render_url);
              EXPECT_FALSE(kanon_bid);
              EXPECT_EQ(10u, data_version);
              EXPECT_TRUE(has_data_version);
              EXPECT_TRUE(errors.empty());
              ++num_generate_bid_calls;
              if (num_generate_bid_calls == kNumGenerateBidCalls)
                run_loop.Quit();
            })),
        bid_finalizer.BindNewEndpointAndPassReceiver());
    bid_finalizer->FinishGenerateBid(
        auction_signals_, per_buyer_signals_, per_buyer_timeout_,
        /*direct_from_seller_per_buyer_signals=*/absl::nullopt,
        /*direct_from_seller_auction_signals=*/absl::nullopt);
  }
  // This should trigger a single network request for all needed signals.
  bidder_worklet->SendPendingSignalsRequests();

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
  std::string keys;
  base::Value::Dict keys_dict;
  for (size_t i = 0; i < kNumGenerateBidCalls; ++i) {
    if (i != 0)
      keys.append(",");
    keys.append(base::NumberToString(i));
    keys_dict.Set(base::NumberToString(i), static_cast<int>(i + 1));
  }
  base::Value::Dict signals_dict;
  signals_dict.Set("keys", std::move(keys_dict));
  std::string signals_json;
  base::JSONWriter::Write(signals_dict, &signals_json);
  AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(base::StringPrintf(
          "https://signals.test/"
          "?hostname=top.window.test&keys=%s&interestGroupNames=Fred",
          keys.c_str())),
      signals_json, /*data_version=*/10u);

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
TEST_F(BidderWorkletTest, GenerateBidTrustedBiddingSignalsParallelBatched2) {
  interest_group_trusted_bidding_signals_url_ = GURL("https://signals.test/");
  interest_group_trusted_bidding_signals_keys_.emplace();

  // Each GenerateBid() call provides a different `auctionSignals` value. The
  // `i` generateBid() call should have trusted scoring signals of {"i":i+1},
  // and this function uses the key as the "ad" parameter, and the value as the
  // bid.
  const char kBidderScriptReturnValue[] = R"({
    ad: Number(Object.keys(trustedBiddingSignals)[0]),
    bid: trustedBiddingSignals[Object.keys(trustedBiddingSignals)[0]],
    render:"https://response.test/"
  })";

  auto bidder_worklet = CreateWorklet();

  // 1) GenerateBid() calls are made
  base::RunLoop run_loop;
  const size_t kNumGenerateBidCalls = 10;
  size_t num_generate_bid_calls = 0;
  for (size_t i = 0; i < kNumGenerateBidCalls; ++i) {
    // Append a different key for each request.
    auto interest_group_fields = CreateBidderWorkletNonSharedParams();
    interest_group_fields->trusted_bidding_signals_keys->push_back(
        base::NumberToString(i));
    mojo::AssociatedRemote<auction_worklet::mojom::GenerateBidFinalizer>
        bid_finalizer;
    bidder_worklet->BeginGenerateBid(
        std::move(interest_group_fields), kanon_mode_, join_origin_,
        direct_from_seller_per_buyer_signals_,
        direct_from_seller_auction_signals_, browser_signal_seller_origin_,
        browser_signal_top_level_seller_origin_, CreateBiddingBrowserSignals(),
        auction_start_time_,
        /*trace_id=*/1,
        GenerateBidClientWithCallbacks::Create(base::BindLambdaForTesting(
            [&run_loop, &num_generate_bid_calls, i](
                mojom::BidderWorkletBidPtr bid,
                mojom::BidderWorkletKAnonEnforcedBidPtr kanon_bid,
                uint32_t data_version, bool has_data_version,
                const absl::optional<GURL>& debug_loss_report_url,
                const absl::optional<GURL>& debug_win_report_url,
                double set_priority, bool has_set_priority,
                base::flat_map<std::string,
                               auction_worklet::mojom::PrioritySignalsDoublePtr>
                    update_priority_signals_overrides,
                PrivateAggregationRequests pa_requests,
                base::TimeDelta bidding_duration,
                const std::vector<std::string>& errors) {
              EXPECT_EQ(base::NumberToString(i), bid->ad);
              EXPECT_EQ(i + 1, bid->bid);
              EXPECT_EQ(GURL("https://response.test/"), bid->render_url);
              EXPECT_FALSE(kanon_bid);
              EXPECT_EQ(42u, data_version);
              EXPECT_TRUE(has_data_version);
              EXPECT_TRUE(errors.empty());
              ++num_generate_bid_calls;
              if (num_generate_bid_calls == kNumGenerateBidCalls)
                run_loop.Quit();
            })),
        bid_finalizer.BindNewEndpointAndPassReceiver());
    bid_finalizer->FinishGenerateBid(
        auction_signals_, per_buyer_signals_, per_buyer_timeout_,
        /*direct_from_seller_per_buyer_signals=*/absl::nullopt,
        /*direct_from_seller_auction_signals=*/absl::nullopt);
  }
  // This should trigger a single network request for all needed signals.
  bidder_worklet->SendPendingSignalsRequests();

  // Calling GenerateBid() shouldn't cause any callbacks to be invoked - the
  // BidderWorklet is waiting on both the trusted bidding signals and Javascript
  // responses from the network.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(run_loop.AnyQuitCalled());
  EXPECT_EQ(0u, num_generate_bid_calls);

  // 2) The trusted bidding signals are loaded.
  std::string keys;
  base::Value::Dict keys_dict;
  for (size_t i = 0; i < kNumGenerateBidCalls; ++i) {
    if (i != 0)
      keys.append(",");
    keys.append(base::NumberToString(i));
    keys_dict.Set(base::NumberToString(i), static_cast<int>(i + 1));
  }
  base::Value::Dict signals_dict;
  signals_dict.Set("keys", std::move(keys_dict));
  std::string signals_json;
  base::JSONWriter::Write(signals_dict, &signals_json);
  AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(base::StringPrintf(
          "https://signals.test/"
          "?hostname=top.window.test&keys=%s&interestGroupNames=Fred",
          keys.c_str())),
      signals_json, /*data_version=*/42u);

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
TEST_F(BidderWorkletTest, GenerateBidTrustedBiddingSignalsParallelBatched3) {
  interest_group_trusted_bidding_signals_url_ = GURL("https://signals.test/");
  interest_group_trusted_bidding_signals_keys_.emplace();

  // Each GenerateBid() call provides a different `auctionSignals` value. The
  // `i` generateBid() call should have trusted scoring signals of {"i":i+1},
  // and this function uses the key as the "ad" parameter, and the value as the
  // bid.
  const char kBidderScriptReturnValue[] = R"({
    ad: Number(Object.keys(trustedBiddingSignals)[0]),
    bid: trustedBiddingSignals[Object.keys(trustedBiddingSignals)[0]],
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
    // Append a different key for each request.
    auto interest_group_fields = CreateBidderWorkletNonSharedParams();
    interest_group_fields->trusted_bidding_signals_keys->push_back(
        base::NumberToString(i));
    mojo::AssociatedRemote<auction_worklet::mojom::GenerateBidFinalizer>
        bid_finalizer;
    bidder_worklet->BeginGenerateBid(
        std::move(interest_group_fields), kanon_mode_, join_origin_,
        direct_from_seller_per_buyer_signals_,
        direct_from_seller_auction_signals_, browser_signal_seller_origin_,
        browser_signal_top_level_seller_origin_, CreateBiddingBrowserSignals(),
        auction_start_time_,
        /*trace_id=*/1,
        GenerateBidClientWithCallbacks::Create(base::BindLambdaForTesting(
            [&run_loop, &num_generate_bid_calls, i](
                mojom::BidderWorkletBidPtr bid,
                mojom::BidderWorkletKAnonEnforcedBidPtr kanon_bid,
                uint32_t data_version, bool has_data_version,
                const absl::optional<GURL>& debug_loss_report_url,
                const absl::optional<GURL>& debug_win_report_url,
                double set_priority, bool has_set_priority,
                base::flat_map<std::string,
                               auction_worklet::mojom::PrioritySignalsDoublePtr>
                    update_priority_signals_overrides,
                PrivateAggregationRequests pa_requests,
                base::TimeDelta bidding_duration,
                const std::vector<std::string>& errors) {
              EXPECT_EQ(base::NumberToString(i), bid->ad);
              EXPECT_EQ(i + 1, bid->bid);
              EXPECT_FALSE(kanon_bid);
              EXPECT_EQ(22u, data_version);
              EXPECT_TRUE(has_data_version);
              EXPECT_EQ(GURL("https://response.test/"), bid->render_url);
              EXPECT_TRUE(errors.empty());
              ++num_generate_bid_calls;
              if (num_generate_bid_calls == kNumGenerateBidCalls)
                run_loop.Quit();
            })),
        bid_finalizer.BindNewEndpointAndPassReceiver());
    bid_finalizer->FinishGenerateBid(
        auction_signals_, per_buyer_signals_, per_buyer_timeout_,
        /*direct_from_seller_per_buyer_signals=*/absl::nullopt,
        /*direct_from_seller_auction_signals=*/absl::nullopt);
  }
  // This should trigger a single network request for all needed signals.
  bidder_worklet->SendPendingSignalsRequests();

  // No callbacks should have been invoked yet, since the trusted bidding
  // signals haven't loaded yet.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(run_loop.AnyQuitCalled());
  EXPECT_EQ(0u, num_generate_bid_calls);

  // 3) The trusted bidding signals are loaded.
  std::string keys;
  base::Value::Dict keys_dict;
  for (size_t i = 0; i < kNumGenerateBidCalls; ++i) {
    if (i != 0)
      keys.append(",");
    keys.append(base::NumberToString(i));
    keys_dict.Set(base::NumberToString(i), static_cast<int>(i + 1));
  }
  base::Value::Dict signals_dict;
  signals_dict.Set("keys", std::move(keys_dict));
  std::string signals_json;
  base::JSONWriter::Write(signals_dict, &signals_json);
  AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(base::StringPrintf(
          "https://signals.test/"
          "?hostname=top.window.test&keys=%s&interestGroupNames=Fred",
          keys.c_str())),
      signals_json, /*data_version=*/22u);

  // The worklets can now generate bids.
  run_loop.Run();
  EXPECT_EQ(num_generate_bid_calls, kNumGenerateBidCalls);
}

// Same as the first batched test, but without batching requests. No need to
// test all not batched order variations.
TEST_F(BidderWorkletTest, GenerateBidTrustedBiddingSignalsParallelNotBatched) {
  interest_group_trusted_bidding_signals_url_ = GURL("https://signals.test/");
  interest_group_trusted_bidding_signals_keys_.emplace();

  // Each GenerateBid() call provides a different `auctionSignals` value. The
  // `i` generateBid() call should have trusted scoring signals of {"i":i+1},
  // and this function uses the key as the "ad" parameter, and the value as the
  // bid.
  const char kBidderScriptReturnValue[] = R"({
    ad: Number(Object.keys(trustedBiddingSignals)[0]),
    bid: trustedBiddingSignals[Object.keys(trustedBiddingSignals)[0]],
    render:"https://response.test/"
  })";

  auto bidder_worklet = CreateWorklet();

  // 1) BeginGenerateBid()/FinishGenerateBid() calls are made
  base::RunLoop run_loop;
  const size_t kNumGenerateBidCalls = 10;
  size_t num_generate_bid_calls = 0;
  for (size_t i = 0; i < kNumGenerateBidCalls; ++i) {
    // Append a different key for each request.
    auto interest_group_fields = CreateBidderWorkletNonSharedParams();
    interest_group_fields->trusted_bidding_signals_keys->push_back(
        base::NumberToString(i));
    mojo::AssociatedRemote<auction_worklet::mojom::GenerateBidFinalizer>
        bid_finalizer;
    bidder_worklet->BeginGenerateBid(
        std::move(interest_group_fields), kanon_mode_, join_origin_,
        direct_from_seller_per_buyer_signals_,
        direct_from_seller_auction_signals_, browser_signal_seller_origin_,
        browser_signal_top_level_seller_origin_, CreateBiddingBrowserSignals(),
        auction_start_time_,
        /*trace_id=*/1,
        GenerateBidClientWithCallbacks::Create(base::BindLambdaForTesting(
            [&run_loop, &num_generate_bid_calls, i](
                mojom::BidderWorkletBidPtr bid,
                mojom::BidderWorkletKAnonEnforcedBidPtr kanon_bid,
                uint32_t data_version, bool has_data_version,
                const absl::optional<GURL>& debug_loss_report_url,
                const absl::optional<GURL>& debug_win_report_url,
                double set_priority, bool has_set_priority,
                base::flat_map<std::string,
                               auction_worklet::mojom::PrioritySignalsDoublePtr>
                    update_priority_signals_overrides,
                PrivateAggregationRequests pa_requests,
                base::TimeDelta bidding_duration,
                const std::vector<std::string>& errors) {
              EXPECT_EQ(base::NumberToString(i), bid->ad);
              EXPECT_EQ(i + 1, bid->bid);
              EXPECT_EQ(GURL("https://response.test/"), bid->render_url);
              EXPECT_FALSE(kanon_bid);
              EXPECT_EQ(i, data_version);
              EXPECT_TRUE(has_data_version);
              EXPECT_TRUE(errors.empty());
              ++num_generate_bid_calls;
              if (num_generate_bid_calls == kNumGenerateBidCalls)
                run_loop.Quit();
            })),
        bid_finalizer.BindNewEndpointAndPassReceiver());

    // Send one request at a time.
    bidder_worklet->SendPendingSignalsRequests();

    bid_finalizer->FinishGenerateBid(
        auction_signals_, per_buyer_signals_, per_buyer_timeout_,
        /*direct_from_seller_per_buyer_signals=*/absl::nullopt,
        /*direct_from_seller_auction_signals=*/absl::nullopt);
  }

  // Calling FinishGenerateBid() shouldn't cause any callbacks to be invoked -
  // the BidderWorklet is waiting on both the trusted bidding signals and
  // Javascript responses from the network.
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
    AddBidderJsonResponse(
        &url_loader_factory_,
        GURL(base::StringPrintf(
            "https://signals.test/"
            "?hostname=top.window.test&keys=%zu&interestGroupNames=Fred",
            i)),
        base::StringPrintf(R"({"keys":{"%zu":%zu}})", i, i + 1), i);
  }

  // The worklets can now generate bids.
  run_loop.Run();
  EXPECT_EQ(kNumGenerateBidCalls, num_generate_bid_calls);
}

// It shouldn't matter the order in which network fetches complete. For each
// required and optional generateBid() URL load prerequisite, ensure that
// generateBid() completes when that URL is the last loaded URL.
TEST_F(BidderWorkletTest, GenerateBidLoadCompletionOrder) {
  constexpr char kTrustedSignalsResponse[] = R"({"keys":{"1":1}})";
  constexpr char kJsonResponse[] = "{}";
  constexpr char kDirectFromSellerSignalsHeaders[] =
      "X-Allow-FLEDGE: true\nX-FLEDGE-Auction-Only: true";

  direct_from_seller_per_buyer_signals_ =
      GURL("https://url.test/perbuyersignals");
  direct_from_seller_auction_signals_ = GURL("https://url.test/auctionsignals");
  interest_group_trusted_bidding_signals_url_ =
      GURL("https://url.test/trustedsignals");
  interest_group_trusted_bidding_signals_keys_ = {"1"};

  struct Response {
    GURL response_url;
    std::string response_type;
    std::string headers;
    std::string content;
  };

  const Response kResponses[] = {
      {interest_group_bidding_url_, kJavascriptMimeType, kAllowFledgeHeader,
       CreateBasicGenerateBidScript()},
      {*direct_from_seller_per_buyer_signals_, kJsonMimeType,
       kDirectFromSellerSignalsHeaders, kJsonResponse},
      {*direct_from_seller_auction_signals_, kJsonMimeType,
       kDirectFromSellerSignalsHeaders, kJsonResponse},
      {GURL(interest_group_trusted_bidding_signals_url_->spec() +
            "?hostname=top.window.test&keys=1&interestGroupNames=Fred"),
       kJsonMimeType, kAllowFledgeHeader, kTrustedSignalsResponse}};

  // Cycle such that each response in `kResponses` gets to be the last response,
  // like so:
  //
  // 0,1,2
  // 1,2,0
  // 2,0,1
  for (size_t offset = 0; offset < std::size(kResponses); ++offset) {
    SCOPED_TRACE(offset);
    mojo::Remote<mojom::BidderWorklet> bidder_worklet = CreateWorklet();
    url_loader_factory_.ClearResponses();
    load_script_run_loop_ = std::make_unique<base::RunLoop>();
    GenerateBid(bidder_worklet.get());
    for (size_t i = 0; i < std::size(kResponses); ++i) {
      SCOPED_TRACE(i);
      const Response& response =
          kResponses[(i + offset) % std::size(kResponses)];
      AddResponse(
          &url_loader_factory_, response.response_url, response.response_type,
          /*charset=*/absl::nullopt, response.content, response.headers);
      task_environment_.RunUntilIdle();
      if (i < std::size(kResponses) - 1) {
        // Some URLs haven't finished loading -- generateBid() should be
        // blocked.
        EXPECT_FALSE(load_script_run_loop_->AnyQuitCalled());
      }
    }
    // The last URL for this generateBid() call has completed -- check that
    // generateBid() returns.
    load_script_run_loop_->Run();
    load_script_run_loop_.reset();
  }
}

// If multiple worklets request DirectFromSellerSignals, they each get the
// correct signals.
TEST_F(BidderWorkletTest, GenerateBidDirectFromSellerSignalsMultipleWorklets) {
  constexpr char kWorklet1JsonResponse[] = R"({"worklet":1})";
  constexpr char kWorklet2JsonResponse[] = R"({"worklet":2})";
  constexpr char kWorklet1ExtraCode[] = R"(
const perBuyerSignalsJson =
    JSON.stringify(directFromSellerSignals.perBuyerSignals);
if (perBuyerSignalsJson !== '{"worklet":1}') {
  throw 'Wrong directFromSellerSignals.perBuyerSignals ' +
      perBuyerSignalsJson;
}
const auctionSignalsJson =
    JSON.stringify(directFromSellerSignals.auctionSignals);
if (auctionSignalsJson !== '{"worklet":1}') {
  throw 'Wrong directFromSellerSignals.auctionSignals ' +
      auctionSignalsJson;
}
)";
  constexpr char kWorklet2ExtraCode[] = R"(
const perBuyerSignalsJson =
    JSON.stringify(directFromSellerSignals.perBuyerSignals);
if (perBuyerSignalsJson !== '{"worklet":2}') {
  throw 'Wrong directFromSellerSignals.perBuyerSignals ' +
      perBuyerSignalsJson;
}
const auctionSignalsJson =
    JSON.stringify(directFromSellerSignals.auctionSignals);
if (auctionSignalsJson !== '{"worklet":2}') {
  throw 'Wrong directFromSellerSignals.auctionSignals ' +
      auctionSignalsJson;
}
)";
  constexpr char kRawReturnValue[] =
      R"({bid: 1, render:"https://response.test/"})";
  constexpr char kDirectFromSellerSignalsHeaders[] =
      "X-Allow-FLEDGE: true\nX-FLEDGE-Auction-Only: true";

  for (bool late_direct_from_seller_signals : {false, true}) {
    SCOPED_TRACE(late_direct_from_seller_signals);
    provide_direct_from_seller_signals_late_ = late_direct_from_seller_signals;
    direct_from_seller_per_buyer_signals_ =
        GURL("https://url.test/perbuyersignals");
    direct_from_seller_auction_signals_ =
        GURL("https://url.test/auctionsignals");

    mojo::Remote<mojom::BidderWorklet> bidder_worklet1 = CreateWorklet();
    AddResponse(&url_loader_factory_, *direct_from_seller_per_buyer_signals_,
                kJsonMimeType, /*charset=*/absl::nullopt, kWorklet1JsonResponse,
                kDirectFromSellerSignalsHeaders);
    AddResponse(&url_loader_factory_, *direct_from_seller_auction_signals_,
                kJsonMimeType, /*charset=*/absl::nullopt, kWorklet1JsonResponse,
                kDirectFromSellerSignalsHeaders);
    AddJavascriptResponse(
        &url_loader_factory_, interest_group_bidding_url_,
        CreateGenerateBidScript(/*raw_return_value=*/kRawReturnValue,
                                /*extra_code=*/kWorklet1ExtraCode));

    // For the second worklet, use a different `interest_group_bidding_url_` (to
    // set up different expectations), but use the same DirectFromSellerSignals
    // URLs.
    interest_group_bidding_url_ = GURL("https://url2.test/");
    mojo::Remote<mojom::BidderWorklet> bidder_worklet2 =
        CreateWorklet(/*url=*/GURL(),
                      /*pause_for_debugger_on_start=*/false,
                      /*out_bidder_worklet_impl=*/nullptr,
                      /*use_alternate_url_loader_factory=*/true);
    AddResponse(&alternate_url_loader_factory_,
                *direct_from_seller_per_buyer_signals_, kJsonMimeType,
                /*charset=*/absl::nullopt, kWorklet2JsonResponse,
                kDirectFromSellerSignalsHeaders);
    AddResponse(&alternate_url_loader_factory_,
                *direct_from_seller_auction_signals_, kJsonMimeType,
                /*charset=*/absl::nullopt, kWorklet2JsonResponse,
                kDirectFromSellerSignalsHeaders);
    AddJavascriptResponse(
        &alternate_url_loader_factory_, interest_group_bidding_url_,
        CreateGenerateBidScript(/*raw_return_value=*/kRawReturnValue,
                                /*extra_code=*/kWorklet2ExtraCode));
    load_script_run_loop_ = std::make_unique<base::RunLoop>();
    GenerateBid(bidder_worklet1.get());
    load_script_run_loop_->Run();
    EXPECT_THAT(bid_errors_, ::testing::UnorderedElementsAre());

    load_script_run_loop_ = std::make_unique<base::RunLoop>();
    GenerateBid(bidder_worklet2.get());
    load_script_run_loop_->Run();
    EXPECT_THAT(bid_errors_, ::testing::UnorderedElementsAre());
    load_script_run_loop_.reset();
  }
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

  // Since PerBuyerSignals are in JSON, non-JSON strings should result in
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

TEST_F(BidderWorkletTest, GenerateBidBrowserSignalTopLevelSellerOrigin) {
  browser_signal_top_level_seller_origin_ = absl::nullopt;
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "topLevelSeller" in browserSignals,
          bid:1,
          render:"https://response.test/"})",
      mojom::BidderWorkletBid::New("false", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()));

  // Need to set `allowComponentAuction` to true for a bid to be created when
  // topLevelSeller is non-null.
  browser_signal_top_level_seller_origin_ =
      url::Origin::Create(GURL("https://foo.test"));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: browserSignals.topLevelSeller, bid:1, render:"https://response.test/",
          allowComponentAuction: true})",
      mojom::BidderWorkletBid::New(
          R"("https://foo.test")", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));
}

TEST_F(BidderWorkletTest, GenerateBidBrowserSignalTopWindowOrigin) {
  top_window_origin_ = url::Origin::Create(GURL("https://top.window.test/"));
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
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ generateBid() bid render URL "
       "'https://response2.test/' isn't one of the registered creative URLs."});

  // Adding an ad with a corresponding `renderUrl` should result in success.
  // Also check the `interestGroup.ads` field passed to Javascript.
  interest_group_ads_.emplace_back(blink::InterestGroup::Ad(
      GURL("https://response2.test/"), /*metadata=*/R"(["metadata"])"));
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
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ generateBid() bid adComponents URL "
       "'https://response.test/' isn't one of the registered creative URLs."});

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

// Test behavior of the `allowComponentAuction` output field, which can block
// bids when not set to true and `topLevelSellerOrigin` is non-null.
TEST_F(BidderWorkletTest, GenerateBidAllowComponentAuction) {
  // In all success cases, this is the returned bid.
  const auto kBidOnSuccess = mojom::BidderWorkletBid::New(
      "null", 1, GURL("https://response.test/"),
      /*ad_components=*/absl::nullopt, base::TimeDelta());

  // Use a null `topLevelSellerOrigin`. `allowComponentAuction` value should be
  // ignored.
  browser_signal_top_level_seller_origin_ = absl::nullopt;
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: null, bid:1, render:"https://response.test/", allowComponentAuction: true})",
      kBidOnSuccess.Clone());
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: null, bid:1, render:"https://response.test/", allowComponentAuction: false})",
      kBidOnSuccess.Clone());
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: null, bid:1, render:"https://response.test/"})",
      kBidOnSuccess.Clone());
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: null, bid:1, render:"https://response.test/", allowComponentAuction: 0})",
      kBidOnSuccess.Clone());
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: null, bid:1, render:"https://response.test/", allowComponentAuction: 1})",
      kBidOnSuccess.Clone());
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: null, bid:1, render:"https://response.test/", allowComponentAuction: "OnTuesdays"})",
      kBidOnSuccess.Clone());

  // Use a non-null `topLevelSellerOrigin`. `allowComponentAuction` value must
  // be "true" for a bid to be generated. This uses the standard Javascript
  // behavior for how to convert non-bools to a bool.
  browser_signal_top_level_seller_origin_ =
      url::Origin::Create(GURL("https://foo.test"));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: null, bid:1, render:"https://response.test/", allowComponentAuction: true})",
      kBidOnSuccess.Clone());
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: null, bid:1, render:"https://response.test/", allowComponentAuction: false})",
      mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt, /*expected_errors=*/
      {"https://url.test/ generateBid() bid does not have "
       "allowComponentAuction set to true. Bid dropped from component "
       "auction."});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: null, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt, /*expected_errors=*/
      {"https://url.test/ generateBid() bid does not have "
       "allowComponentAuction set to true. Bid dropped from component "
       "auction."});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: null, bid:1, render:"https://response.test/", allowComponentAuction: 0})",
      mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt, /*expected_errors=*/
      {"https://url.test/ generateBid() bid does not have "
       "allowComponentAuction set to true. Bid dropped from component "
       "auction."});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: null, bid:1, render:"https://response.test/", allowComponentAuction: 1})",
      kBidOnSuccess.Clone());
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: null, bid:1, render:"https://response.test/", allowComponentAuction: "OnTuesdays"})",
      kBidOnSuccess.Clone());
}

TEST_F(BidderWorkletTest, GenerateBidWasm404) {
  interest_group_wasm_url_ = GURL(kWasmUrl);
  // Have the WASM URL 404.
  AddResponse(&url_loader_factory_, interest_group_wasm_url_.value(),
              kWasmMimeType,
              /*charset=*/absl::nullopt, "Error 404", kAllowFledgeHeader,
              net::HTTP_NOT_FOUND);

  // The Javascript request receives valid JS.
  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        CreateBasicGenerateBidScript());

  auto bidder_worklet = CreateWorklet();
  GenerateBidExpectingNeverCompletes(bidder_worklet.get());
  EXPECT_EQ(
      "Failed to load https://foo.test/helper.wasm "
      "HTTP status = 404 Not Found.",
      WaitForDisconnect());
}

TEST_F(BidderWorkletTest, GenerateBidWasmFailure) {
  interest_group_wasm_url_ = GURL(kWasmUrl);
  // Instead of WASM have JS, but with WASM mimetype.
  AddResponse(&url_loader_factory_, interest_group_wasm_url_.value(),
              kWasmMimeType,
              /*charset=*/absl::nullopt, CreateBasicGenerateBidScript());

  // The Javascript request receives valid JS.
  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        CreateBasicGenerateBidScript());

  auto bidder_worklet = CreateWorklet();
  GenerateBidExpectingNeverCompletes(bidder_worklet.get());
  EXPECT_EQ(
      "https://foo.test/helper.wasm Uncaught CompileError: "
      "WasmModuleObject::Compile(): expected magic word 00 61 73 6d, found "
      "0a 20 20 20 @+0.",
      WaitForDisconnect());
}

TEST_F(BidderWorkletTest, GenerateBidWasm) {
  std::string bid_script = CreateGenerateBidScript(
      R"({ad: WebAssembly.Module.exports(browserSignals.wasmHelper), bid: 1,
          render:"https://response.test/"})");

  interest_group_wasm_url_ = GURL(kWasmUrl);
  AddResponse(&url_loader_factory_, GURL(kWasmUrl), kWasmMimeType,
              /*charset=*/absl::nullopt, ToyWasm());

  RunGenerateBidWithJavascriptExpectingResult(
      bid_script, mojom::BidderWorkletBid::New(
                      R"([{"name":"test_const","kind":"global"}])", 1,
                      GURL("https://response.test/"),
                      /*ad_components=*/absl::nullopt, base::TimeDelta()));
}

TEST_F(BidderWorkletTest, WasmReportWin) {
  // Regression test for state machine bug during development, with
  // double-execution of report win tasks when waiting on WASM.
  AddJavascriptResponse(
      &url_loader_factory_, interest_group_bidding_url_,
      CreateReportWinScript(R"(sendReportTo("https://foo.test"))"));
  interest_group_wasm_url_ = GURL(kWasmUrl);

  auto bidder_worklet = CreateWorklet();
  ASSERT_TRUE(bidder_worklet);

  // Wedge the V8 thread so that completed first instance of reportWin doesn't
  // fully wrap up and clean up the task by time the WASM is delivered.
  base::WaitableEvent* event_handle = WedgeV8Thread(v8_helper_.get());

  base::RunLoop run_loop;
  bidder_worklet->ReportWin(
      interest_group_name_, /*auction_signals_json=*/"0", per_buyer_signals_,
      direct_from_seller_per_buyer_signals_,
      direct_from_seller_auction_signals_, seller_signals_,
      browser_signal_render_url_, browser_signal_bid_,
      browser_signal_highest_scoring_other_bid_,
      browser_signal_made_highest_scoring_other_bid_,
      browser_signal_seller_origin_, browser_signal_top_level_seller_origin_,
      data_version_.value_or(0), data_version_.has_value(),
      /*trace_id=*/1,
      base::BindLambdaForTesting(
          [&run_loop](const absl::optional<GURL>& report_url,
                      const base::flat_map<std::string, GURL>& ad_beacon_map,
                      PrivateAggregationRequests pa_requests,
                      const std::vector<std::string>& errors) {
            run_loop.Quit();
          }));
  base::RunLoop().RunUntilIdle();
  AddResponse(&url_loader_factory_, GURL(kWasmUrl), kWasmMimeType,
              /*charset=*/absl::nullopt, ToyWasm());
  base::RunLoop().RunUntilIdle();
  event_handle->Signal();
  run_loop.Run();
  // Make sure there isn't a second attempt to complete lurking.
  task_environment_.RunUntilIdle();
}

TEST_F(BidderWorkletTest, WasmReportWin2) {
  // Regression test for https://crbug.com/1345219 --- JS loaded but WASM not
  // by time of reportWin() causing it to not get run.
  AddJavascriptResponse(
      &url_loader_factory_, interest_group_bidding_url_,
      CreateReportWinScript(R"(sendReportTo("https://foo.test"))"));
  interest_group_wasm_url_ = GURL(kWasmUrl);

  auto bidder_worklet = CreateWorklet();
  ASSERT_TRUE(bidder_worklet);

  // Get the JS a chance to load.
  task_environment_.RunUntilIdle();

  base::RunLoop run_loop;
  RunReportWinExpectingResultAsync(
      bidder_worklet.get(), GURL("https://foo.test"),
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      /*expected_errors=*/{},
      base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }));
  task_environment_.RunUntilIdle();
  AddResponse(&url_loader_factory_, GURL(kWasmUrl), kWasmMimeType,
              /*charset=*/absl::nullopt, ToyWasm());
  task_environment_.RunUntilIdle();

  run_loop.Run();
}

TEST_F(BidderWorkletTest, WasmOrdering) {
  enum Event { kWasmSuccess, kJsSuccess, kWasmFailure, kJsFailure };

  struct Test {
    std::vector<Event> events;
    bool expect_success;
  };

  const Test tests[] = {
      {{kWasmSuccess, kJsSuccess}, /*expect_success=*/true},
      {{kJsSuccess, kWasmSuccess}, /*expect_success=*/true},
      {{kWasmFailure, kJsSuccess}, /*expect_success=*/false},
      {{kJsSuccess, kWasmFailure}, /*expect_success=*/false},
      {{kWasmSuccess, kJsFailure}, /*expect_success=*/false},
      {{kJsFailure, kWasmSuccess}, /*expect_success=*/false},
      {{kJsFailure, kWasmFailure}, /*expect_success=*/false},
      {{kWasmFailure, kJsFailure}, /*expect_success=*/false},
  };

  interest_group_wasm_url_ = GURL(kWasmUrl);

  for (const Test& test : tests) {
    url_loader_factory_.ClearResponses();

    mojo::Remote<mojom::BidderWorklet> bidder_worklet = CreateWorklet();
    if (test.expect_success) {
      // On success, callback should be invoked.
      load_script_run_loop_ = std::make_unique<base::RunLoop>();
      GenerateBid(bidder_worklet.get());
    } else {
      // On error, the pipe is closed without invoking the callback.
      GenerateBidExpectingNeverCompletes(bidder_worklet.get());
    }

    for (Event ev : test.events) {
      switch (ev) {
        case kWasmSuccess:
          AddResponse(&url_loader_factory_, GURL(kWasmUrl),
                      std::string(kWasmMimeType),
                      /*charset=*/absl::nullopt, ToyWasm());
          break;

        case kJsSuccess:
          AddJavascriptResponse(&url_loader_factory_,
                                interest_group_bidding_url_,
                                CreateBasicGenerateBidScript());
          break;

        case kWasmFailure:
          url_loader_factory_.AddResponse(kWasmUrl, "", net::HTTP_NOT_FOUND);
          break;

        case kJsFailure:
          url_loader_factory_.AddResponse(interest_group_bidding_url_.spec(),
                                          "", net::HTTP_NOT_FOUND);

          break;
      };
      task_environment_.RunUntilIdle();
    }

    if (test.expect_success) {
      // On success, the callback is invoked.
      load_script_run_loop_->Run();
      load_script_run_loop_.reset();
      EXPECT_TRUE(bid_);
    } else {
      // On failure, the pipe is closed with a non-empty error message, without
      // invoking the callback.
      EXPECT_FALSE(WaitForDisconnect().empty());
    }
  }
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
  const GURL kNoKeysSignalsUrl(
      "https://signals.test/"
      "?hostname=top.window.test&interestGroupNames=Fred");
  const GURL kFullSignalsUrl(
      "https://signals.test/"
      "?hostname=top.window.test&keys=key1,key2&interestGroupNames=Fred");

  const char kJson[] = R"(
    {
      "keys": {
        "key1": 1,
        "key2": [2]
      }
    }
  )";

  size_t observed_requests = 0;

  // Request with null TrustedBiddingSignals keys and URL. No request should be
  // made.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: trustedBiddingSignals, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New("null", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()));
  EXPECT_EQ(observed_requests += 1, url_loader_factory_.total_requests());

  // Request with TrustedBiddingSignals keys and null URL. No request should be
  // made.
  interest_group_trusted_bidding_signals_keys_ =
      std::vector<std::string>({"key1", "key2"});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: trustedBiddingSignals, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New("null", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()));
  EXPECT_EQ(observed_requests += 1, url_loader_factory_.total_requests());

  // Request with TrustedBiddingSignals URL and null keys. Request should be
  // made without any keys, and nothing from the response passed to
  // generateBid().
  interest_group_trusted_bidding_signals_url_ = kBaseSignalsUrl;
  interest_group_trusted_bidding_signals_keys_.reset();
  AddBidderJsonResponse(&url_loader_factory_, kNoKeysSignalsUrl, kJson);
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: trustedBiddingSignals, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New("null", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()));
  EXPECT_EQ(observed_requests += 2, url_loader_factory_.total_requests());

  // Request with TrustedBiddingSignals URL and empty keys. Request should be
  // made without any keys, and nothing from the response passed to
  // generateBid().
  interest_group_trusted_bidding_signals_keys_ = std::vector<std::string>();
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: trustedBiddingSignals, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New("null", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()));
  EXPECT_EQ(observed_requests += 2, url_loader_factory_.total_requests());
  url_loader_factory_.ClearResponses();

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
      /*expected_data_version=*/absl::nullopt,
      {"Failed to load "
       "https://signals.test/"
       "?hostname=top.window.test&keys=key1,key2&interestGroupNames=Fred HTTP "
       "status = 404 Not Found."});
  EXPECT_EQ(observed_requests += 2, url_loader_factory_.total_requests());

  // Request with valid TrustedBiddingSignals URL and non-empty keys. Request
  // should be made. The request succeeds.
  AddBidderJsonResponse(&url_loader_factory_, kFullSignalsUrl, kJson);
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: trustedBiddingSignals, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          R"({"key1":1,"key2":[2]})", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));
  EXPECT_EQ(observed_requests += 2, url_loader_factory_.total_requests());
}

// Test that when no trusted signals are fetched, generating bids is delayed
// until the OnBiddingSignalsReceivedCallback is invoked.
TEST_F(BidderWorkletTest, GenerateBidOnBiddingSignalsReceivedNoTrustedSignals) {
  auto bidder_worklet = CreateWorklet();
  AddJavascriptResponse(
      &url_loader_factory_, interest_group_bidding_url_,
      CreateGenerateBidScript(CreateBasicGenerateBidScript()));

  base::RunLoop on_bidding_signals_received_run_loop;
  base::OnceClosure on_bidding_signals_received_continue_callback;
  auto on_bidding_signals_received_callback = base::BindLambdaForTesting(
      [&](const base::flat_map<std::string, double>& priority_vector,
          base::TimeDelta trusted_signals_fetch_duration,
          base::OnceClosure callback) {
        EXPECT_TRUE(priority_vector.empty());
        EXPECT_EQ(base::TimeDelta(), trusted_signals_fetch_duration);
        on_bidding_signals_received_run_loop.Quit();
        on_bidding_signals_received_continue_callback = std::move(callback);
      });
  load_script_run_loop_ = std::make_unique<base::RunLoop>();
  GenerateBid(bidder_worklet.get(),
              GenerateBidClientWithCallbacks::Create(
                  base::BindOnce(&BidderWorkletTest::GenerateBidCallback,
                                 base::Unretained(this)),
                  std::move(on_bidding_signals_received_callback)));

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(on_bidding_signals_received_run_loop.AnyQuitCalled());
  EXPECT_FALSE(load_script_run_loop_->AnyQuitCalled());
  ASSERT_TRUE(on_bidding_signals_received_continue_callback);

  std::move(on_bidding_signals_received_continue_callback).Run();
  load_script_run_loop_->Run();
}

// Test that when signals fail to be feteched, OnBiddingSignalsReceived() is
// only invoked after the failed fetch completes, and generating bids is delayed
// until the OnBiddingSignalsReceivedCallback is invoked.
TEST_F(BidderWorkletTest, GenerateBidOnBiddingSignalsReceivedFetchFails) {
  interest_group_trusted_bidding_signals_url_ = GURL("https://signals.test/");
  const GURL kFullSignalsUrl(
      "https://signals.test/?hostname=top.window.test&interestGroupNames=Fred");

  auto bidder_worklet = CreateWorklet();
  AddJavascriptResponse(
      &url_loader_factory_, interest_group_bidding_url_,
      CreateGenerateBidScript(CreateBasicGenerateBidScript()));

  base::RunLoop on_bidding_signals_received_run_loop;
  base::OnceClosure on_bidding_signals_received_continue_callback;
  auto on_bidding_signals_received_callback = base::BindLambdaForTesting(
      [&](const base::flat_map<std::string, double>& priority_vector,
          base::TimeDelta trusted_signals_fetch_duration,
          base::OnceClosure callback) {
        EXPECT_TRUE(priority_vector.empty());
        on_bidding_signals_received_run_loop.Quit();
        on_bidding_signals_received_continue_callback = std::move(callback);
      });
  load_script_run_loop_ = std::make_unique<base::RunLoop>();
  GenerateBid(bidder_worklet.get(),
              GenerateBidClientWithCallbacks::Create(
                  base::BindOnce(&BidderWorkletTest::GenerateBidCallback,
                                 base::Unretained(this)),
                  std::move(on_bidding_signals_received_callback)));

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(on_bidding_signals_received_run_loop.AnyQuitCalled());
  EXPECT_FALSE(load_script_run_loop_->AnyQuitCalled());

  url_loader_factory_.AddResponse(kFullSignalsUrl.spec(), /*content=*/"",
                                  net::HTTP_NOT_FOUND);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(on_bidding_signals_received_run_loop.AnyQuitCalled());
  EXPECT_FALSE(load_script_run_loop_->AnyQuitCalled());
  ASSERT_TRUE(on_bidding_signals_received_continue_callback);

  std::move(on_bidding_signals_received_continue_callback).Run();
  load_script_run_loop_->Run();
}

// Test that when signals are successfully fetched, OnBiddingSignalsReceived()
// is only invoked after the fetch completes, and generating bids is delayed
// until the OnBiddingSignalsReceivedCallback is invoked.
TEST_F(BidderWorkletTest,
       GenerateBidOnBiddingSignalsReceivedNoPriorityVectorRequested) {
  interest_group_trusted_bidding_signals_url_ = GURL("https://signals.test/");
  interest_group_trusted_bidding_signals_keys_ = {{"key1"}};
  const GURL kFullSignalsUrl(
      "https://signals.test/"
      "?hostname=top.window.test&keys=key1&interestGroupNames=Fred");

  const char kJson[] = R"({"keys": {"key1": 1}})";

  auto bidder_worklet = CreateWorklet();
  AddJavascriptResponse(
      &url_loader_factory_, interest_group_bidding_url_,
      CreateGenerateBidScript(CreateBasicGenerateBidScript()));

  base::RunLoop on_bidding_signals_received_run_loop;
  base::OnceClosure on_bidding_signals_received_continue_callback;
  auto on_bidding_signals_received_callback = base::BindLambdaForTesting(
      [&](const base::flat_map<std::string, double>& priority_vector,
          base::TimeDelta trusted_signals_fetch_duration,
          base::OnceClosure callback) {
        EXPECT_TRUE(priority_vector.empty());
        on_bidding_signals_received_run_loop.Quit();
        on_bidding_signals_received_continue_callback = std::move(callback);
      });
  load_script_run_loop_ = std::make_unique<base::RunLoop>();
  GenerateBid(bidder_worklet.get(),
              GenerateBidClientWithCallbacks::Create(
                  base::BindOnce(&BidderWorkletTest::GenerateBidCallback,
                                 base::Unretained(this)),
                  std::move(on_bidding_signals_received_callback)));

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(on_bidding_signals_received_run_loop.AnyQuitCalled());
  EXPECT_FALSE(load_script_run_loop_->AnyQuitCalled());

  AddBidderJsonResponse(&url_loader_factory_, kFullSignalsUrl, kJson);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(on_bidding_signals_received_run_loop.AnyQuitCalled());
  EXPECT_FALSE(load_script_run_loop_->AnyQuitCalled());
  ASSERT_TRUE(on_bidding_signals_received_continue_callback);

  std::move(on_bidding_signals_received_continue_callback).Run();
  load_script_run_loop_->Run();
}

// Test that when signals are successfully fetched, but the response includes no
// priority vector. OnBiddingSignalsReceived() is invoked with a empty priority
// vector after the fetch completes, and generating bids is delayed until the
// OnBiddingSignalsReceivedCallback is invoked.
TEST_F(BidderWorkletTest,
       GenerateBidOnBiddingSignalsReceivedNoPriorityVectorReceived) {
  interest_group_trusted_bidding_signals_url_ = GURL("https://signals.test/");
  const GURL kFullSignalsUrl(
      "https://signals.test/?hostname=top.window.test&interestGroupNames=Fred");

  const char kJson[] = "{}";

  auto bidder_worklet = CreateWorklet();
  AddJavascriptResponse(
      &url_loader_factory_, interest_group_bidding_url_,
      CreateGenerateBidScript(CreateBasicGenerateBidScript()));

  base::RunLoop on_bidding_signals_received_run_loop;
  base::OnceClosure on_bidding_signals_received_continue_callback;
  auto on_bidding_signals_received_callback = base::BindLambdaForTesting(
      [&](const base::flat_map<std::string, double>& priority_vector,
          base::TimeDelta trusted_signals_fetch_duration,
          base::OnceClosure callback) {
        EXPECT_TRUE(priority_vector.empty());
        on_bidding_signals_received_run_loop.Quit();
        on_bidding_signals_received_continue_callback = std::move(callback);
      });
  load_script_run_loop_ = std::make_unique<base::RunLoop>();
  GenerateBid(bidder_worklet.get(),
              GenerateBidClientWithCallbacks::Create(
                  base::BindOnce(&BidderWorkletTest::GenerateBidCallback,
                                 base::Unretained(this)),
                  std::move(on_bidding_signals_received_callback)));

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(on_bidding_signals_received_run_loop.AnyQuitCalled());
  EXPECT_FALSE(load_script_run_loop_->AnyQuitCalled());

  AddBidderJsonResponse(&url_loader_factory_, kFullSignalsUrl, kJson);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(on_bidding_signals_received_run_loop.AnyQuitCalled());
  EXPECT_FALSE(load_script_run_loop_->AnyQuitCalled());
  ASSERT_TRUE(on_bidding_signals_received_continue_callback);

  std::move(on_bidding_signals_received_continue_callback).Run();
  load_script_run_loop_->Run();
}

// Test that cancelling a GenerateBid() call by deleting the GenerateBidClient
// aborts a pending trusted signals fetch.
TEST_F(BidderWorkletTest, GenerateBidCancelAbortsSignalsFetch) {
  interest_group_trusted_bidding_signals_url_ = GURL("https://signals.test/");
  const GURL kFullSignalsUrl(
      "https://signals.test/?hostname=top.window.test&interestGroupNames=Fred");

  auto bidder_worklet = CreateWorklet();
  // This is not strictly needed for this test, but should ensure that only the
  // JSON fetch is pending.
  AddJavascriptResponse(
      &url_loader_factory_, interest_group_bidding_url_,
      CreateGenerateBidScript(CreateBasicGenerateBidScript()));

  GenerateBidClientWithCallbacks client(
      GenerateBidClientWithCallbacks::GenerateBidNeverInvokedCallback(),
      GenerateBidClientWithCallbacks::OnBiddingSignalsReceivedCallback());
  mojo::AssociatedReceiver<mojom::GenerateBidClient> client_receiver(&client);

  GenerateBid(bidder_worklet.get(),
              client_receiver.BindNewEndpointAndPassRemote());

  // Wait for the JSON request to be made, and validate it's URL and that the
  // URLLoaderClient pipe is still connected.
  task_environment_.RunUntilIdle();
  ASSERT_EQ(1, url_loader_factory_.NumPending());
  EXPECT_EQ(kFullSignalsUrl,
            url_loader_factory_.GetPendingRequest(0)->request.url);
  ASSERT_TRUE(url_loader_factory_.GetPendingRequest(0)->client.is_connected());

  // Destroy the GenerateBidClient pipe. This should result in the generate bid
  // call being aborted, and the fetch being cancelled, since only the cancelled
  // GenerateBid() call was depending on the response.
  client_receiver.reset();
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(url_loader_factory_.GetPendingRequest(0)->client.is_connected());
}

// Test that cancelling a GenerateBid() call by deleting the GenerateBidClient
// when Javascript is running doesn't result in a crash.
TEST_F(BidderWorkletTest, GenerateBidCancelWhileRunningJavascript) {
  auto bidder_worklet = CreateWorklet();

  // Use a hanging GenerateBid() script so this it (most likely) completes only
  // after the close pipe message has been received.
  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        CreateGenerateBidScript("while (true);"));
  // Reduce the script timeout so the test doesn't take too long to run, while
  // waiting for the loop in the above script to timeout.
  per_buyer_timeout_ = base::Milliseconds(10);

  base::OnceClosure on_bidding_signals_received_continue_callback;
  auto on_bidding_signals_received_callback = base::BindLambdaForTesting(
      [&](const base::flat_map<std::string, double>& priority_vector,
          base::TimeDelta trusted_signals_fetch_duration,
          base::OnceClosure callback) {
        EXPECT_TRUE(priority_vector.empty());
        on_bidding_signals_received_continue_callback = std::move(callback);
      });

  GenerateBidClientWithCallbacks client(
      GenerateBidClientWithCallbacks::GenerateBidNeverInvokedCallback(),
      std::move(on_bidding_signals_received_callback));
  mojo::AssociatedReceiver<mojom::GenerateBidClient> client_receiver(&client);

  load_script_run_loop_ = std::make_unique<base::RunLoop>();
  GenerateBid(bidder_worklet.get(),
              client_receiver.BindNewEndpointAndPassRemote());

  // Wait for for the Javascript to be fetched and OnSignalsReceived to be
  // invoked.
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(on_bidding_signals_received_continue_callback);

  // Invoking the callback passed to OnBiddingSignalsReceived() will cause a
  // task to be posted to run the Javascript generateBid() method.
  std::move(on_bidding_signals_received_continue_callback).Run();
  // This deletes the BidderWorklet's GenerateBidTask for the already running
  // Javascript. Once the Javascript completes running and posts a task back to
  // the main thread, that task should not dereference the deleted
  // GenerateBidTask, because it uses a weak pointer.
  client_receiver.reset();

  // Wait until the script is finished. There should be no crash.
  task_environment_.RunUntilIdle();
}

TEST_F(BidderWorkletTest, GenerateBidDataVersion) {
  interest_group_trusted_bidding_signals_url_ = GURL("https://signals.test/");
  interest_group_trusted_bidding_signals_keys_.emplace();
  interest_group_trusted_bidding_signals_keys_->push_back("key1");
  AddBidderJsonResponse(
      &url_loader_factory_,
      GURL("https://signals.test/"
           "?hostname=top.window.test&keys=key1&interestGroupNames=Fred"),
      R"({"keys":{"key1":1}})", /*data_version=*/7u);
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid:browserSignals.dataVersion, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(R"("ad")", 7, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()),
      7u);
}

// Even though the script had set an intermediate result with setBid, the
// returned value should be used instead.
TEST_F(BidderWorkletTest, GenerateBidWithSetBid) {
  interest_group_ads_.emplace_back(GURL("https://response.test/replacement"),
                                   /*metadata=*/absl::nullopt);

  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          /*raw_return_value=*/
          R"({ad: "returned", bid:2, render:"https://response.test/replacement" })",
          /*extra_code=*/R"(
            setBid({ad: "ad", bid:1, render:"https://response.test/"})
          )"),
      /*expected_bid=*/
      mojom::BidderWorkletBid::New(
          "\"returned\"", 2, GURL("https://response.test/replacement"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));

  // Test the no-bid version as well.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          /*raw_return_value=*/
          R"({ad: "returned", bid:0, render:"https://response.test/" })",
          /*extra_code=*/R"(
            setBid({ad: "ad", bid:1, render:"https://response.test/"})
          )"),
      /*expected_bid=*/
      mojom::BidderWorkletBidPtr());
}

TEST_F(BidderWorkletTest, GenerateBidExperimentGroupId) {
  experiment_group_id_ = 48384u;
  interest_group_trusted_bidding_signals_url_ = GURL("https://signals.test/");
  interest_group_trusted_bidding_signals_keys_.emplace();
  interest_group_trusted_bidding_signals_keys_->push_back("key1");
  AddBidderJsonResponse(
      &url_loader_factory_,
      GURL("https://signals.test/"
           "?hostname=top.window.test&keys=key1&interestGroupNames=Fred"
           "&experimentGroupId=48384"),
      R"({"keys":{"key1":1}})");
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid:123, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          R"("ad")", 123, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));
}

TEST_F(BidderWorkletTest, GenerateBidTimedOut) {
  // The bidding script has an endless while loop. It will time out due to
  // AuctionV8Helper's default script timeout (50 ms).
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(/*raw_return_value=*/"", R"(while (1))"),
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ execution of `generateBid` timed out."});
}

TEST_F(BidderWorkletTest, GenerateBidPerBuyerTimeOut) {
  // Use a very long default script timeout, and a short per buyer timeout, so
  // that if the bidder script with endless loop times out, we know that the per
  // buyer timeout overwrote the default script timeout and worked.
  const base::TimeDelta kScriptTimeout = base::Days(360);
  v8_helper_->v8_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<AuctionV8Helper> v8_helper,
             const base::TimeDelta script_timeout) {
            v8_helper->set_script_timeout_for_testing(script_timeout);
          },
          v8_helper_, kScriptTimeout));
  // Make sure set_script_timeout_for_testing is called.
  task_environment_.RunUntilIdle();

  per_buyer_timeout_ = base::Milliseconds(20);
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(/*raw_return_value=*/"", R"(while (1))"),
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ execution of `generateBid` timed out."});
}

// Even though the script timed out, it had set an intermediate result with
// setBid, so we should use that. Note that this test sets `bid_duration` to
// `AuctionV8Helper::kScriptTimeout`, to make sure the full timeout time is
// included in the duration.
TEST_F(BidderWorkletTest, GenerateBidTimedOutWithSetBid) {
  // The bidding script has an endless while loop.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          /*raw_return_value=*/
          R"({ad: "not_reached", bid:2, render:"https://response.test/2" })",
          /*extra_code=*/R"(
            setBid({ad: "ad", bid:1, render:"https://response.test/"});
            while (1)
          )"),
      /*expected_bid=*/
      mojom::BidderWorkletBid::New("\"ad\"", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   AuctionV8Helper::kScriptTimeout),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ execution of `generateBid` timed out."});

  // Test the case where setBid() sets no bid. There should be no bid, and
  // no error, other than the timeout.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          /*raw_return_value=*/
          R"({ad: "not_reached", bid:2, render:"https://response.test/2" })",
          /*extra_code=*/R"(
            setBid();
            while (1)
          )"),
      /*expected_bid=*/
      mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ execution of `generateBid` timed out."});
}

// Test that per-buyer timeout of zero results in no bid produced.
TEST_F(BidderWorkletTest, TimeoutZero) {
  per_buyer_timeout_ = base::Seconds(0);
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBidPtr());
}

// Test that in the case of multiple setBid() calls, the most recent call takes
// precedence. Note that this test sets `bid_duration` to
// `AuctionV8Helper::kScriptTimeout`, to make sure the full timeout time is
// included in the duration.
TEST_F(BidderWorkletTest, GenerateBidTimedOutWithSetBidTwice) {
  interest_group_ads_.emplace_back(GURL("https://response.test/replacement"),
                                   /*metadata=*/absl::nullopt);

  // The bidding script has an endless while loop.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          /*raw_return_value=*/
          R"({ad: "not_reached", bid:2, render:"https://response.test/2" })",
          /*extra_code=*/R"(
            setBid({ad: "ad", bid:1, render:"https://response.test/"});
            setBid({ad: "ad2", bid:2, render:"https://response.test/replacement"});
            while (1)
          )"),
      /*expected_bid=*/
      mojom::BidderWorkletBid::New(
          "\"ad2\"", 2, GURL("https://response.test/replacement"),
          /*ad_components=*/absl::nullopt, AuctionV8Helper::kScriptTimeout),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ execution of `generateBid` timed out."});

  // Test the case where the second setBid() call clears the bid from the first
  // call without setting a new bid, by passing in no bid. There should be no
  // bid, and no error, other than the timeout.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          /*raw_return_value=*/
          R"({ad: "not_reached", bid:2, render:"https://response.test/2" })",
          /*extra_code=*/R"(
            setBid({ad: "ad", bid:1, render:"https://response.test/"});
            setBid(null);
            while (1)
          )"),
      /*expected_bid=*/
      mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ execution of `generateBid` timed out."});

  // Test the case where the second setBid() call clears the bid from the first
  // call without setting a new bid, by passing in null. There should be no bid,
  // and no error, other than the timeout.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          /*raw_return_value=*/
          R"({ad: "not_reached", bid:2, render:"https://response.test/2" })",
          /*extra_code=*/R"(
            setBid({ad: "ad", bid:1, render:"https://response.test/"});
            setBid(null);
            while (1)
          )"),
      /*expected_bid=*/
      mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ execution of `generateBid` timed out."});
}

// Even though the script timed out, it had set an intermediate result with
// setBid, so we should use that instead. The bid value should not change if we
// mutate the object passed to setBid after it returns.
TEST_F(BidderWorkletTest, GenerateBidTimedOutWithSetBidMutateAfter) {
  // The bidding script has an endless while loop.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          /*raw_return_value=*/
          R"({ad: "not_reached", bid:2, render:"https://response.test/2" })",
          /*extra_code=*/R"(
            let result = {ad: "ad", bid:1, render:"https://response.test/"};
            setBid(result);
            result.ad = "ad2";
            result.bid = 3;
            result.render = "https://response.test/3";
            while (1)
          )"),
      /*expected_bid=*/
      mojom::BidderWorkletBid::New("\"ad\"", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ execution of `generateBid` timed out."});
}

TEST_F(BidderWorkletTest, GenerateBidSetPriority) {
  // not enough args
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          R"({ad: "ad", bid:1, render:"https://response.test/" })",
          /*extra_code=*/R"(
            setPriority();
          )"),
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:6 Uncaught TypeError: setPriority requires 1 double "
       "parameter."});
  // priority not a double
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          R"({ad: "ad", bid:1, render:"https://response.test/" })",
          /*extra_code=*/R"(
            setPriority("string");
          )"),
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:6 Uncaught TypeError: setPriority requires 1 double "
       "parameter."});
  // priority not finite
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          R"({ad: "ad", bid:1, render:"https://response.test/" })",
          /*extra_code=*/R"(
            setPriority(0.0/0.0);
          )"),
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:6 Uncaught TypeError: setPriority requires 1 finite "
       "double parameter."});
  // priority called twice
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          R"({ad: "ad", bid:1, render:"https://response.test/" })",
          /*extra_code=*/R"(
            setPriority(4);
            setPriority(4);
          )"),
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:7 Uncaught TypeError: setPriority may be called at "
       "most once."});
  // success
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          R"({ad: "ad", bid:1, render:"https://response.test/" })",
          /*extra_code=*/R"(
            setPriority(9.0);
          )"),
      /*expected_bid=*/
      mojom::BidderWorkletBid::New("\"ad\"", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()),
      /*expected_data_version=*/absl::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_set_priority=*/9.0);
  // set priority with no bid should work.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(R"({bid:0, render:"https://response.test/" })",
                              /*extra_code=*/R"(
            setPriority(9.0);
          )"),
      /*expected_bid=*/
      mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_set_priority=*/9.0);
  // set priority with an invalid bid should work.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(R"({bid:1/0, render:"https://response.test/" })",
                              /*extra_code=*/R"(
            setPriority(9.0);
          )"),
      /*expected_bid=*/
      mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_errors=*/
      {"https://url.test/ generateBid() bid of inf is not a valid bid."},
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_set_priority=*/9.0);
}

TEST_F(BidderWorkletTest, GenerateBidSetPrioritySignalsOverrides) {
  // Not enough args.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(R"({bid:1, render:"https://response.test/" })",
                              /*extra_code=*/R"(
            setPrioritySignalsOverride();
          )"),
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:6 Uncaught TypeError: setPrioritySignalsOverride "
       "requires at least 1 parameter."});

  // Key not a string.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(R"({bid:1, render:"https://response.test/" })",
                              /*extra_code=*/R"(
            setPrioritySignalsOverride(15, 15);
          )"),
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:6 Uncaught TypeError: First argument to "
       "setPrioritySignalsOverride must be a String."});

  // Value not a double.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(R"({bid:1, render:"https://response.test/" })",
                              /*extra_code=*/R"(
            setPrioritySignalsOverride("key", "value");
          )"),
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:6 Uncaught TypeError: Second argument to "
       "setPrioritySignalsOverride must be a finite Number or null."});

  // Value not finite.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(R"({bid:1, render:"https://response.test/" })",
                              /*extra_code=*/R"(
            setPrioritySignalsOverride("key", 0.0/0.0);
          )"),
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:6 Uncaught TypeError: Second argument to "
       "setPrioritySignalsOverride must be a finite Number or null."});

  // A key with no value means the value should be removed.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(R"({bid:1, render:"https://response.test/" })",
                              /*extra_code=*/R"(
            setPrioritySignalsOverride("key");
          )"),
      /*expected_bid=*/
      mojom::BidderWorkletBid::New("null", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()),
      /*expected_data_version=*/absl::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_set_priority=*/absl::nullopt,
      /*expected_update_priority_signals_overrides=*/{{"key", absl::nullopt}});

  // An undefined value means the value should be removed.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(R"({bid:1, render:"https://response.test/" })",
                              /*extra_code=*/R"(
            setPrioritySignalsOverride("key", undefined);
          )"),
      /*expected_bid=*/
      mojom::BidderWorkletBid::New("null", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()),
      /*expected_data_version=*/absl::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_set_priority=*/absl::nullopt,
      /*expected_update_priority_signals_overrides=*/{{"key", absl::nullopt}});

  // A null value means the value should be removed.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(R"({bid:1, render:"https://response.test/" })",
                              /*extra_code=*/R"(
            setPrioritySignalsOverride("key", null);
          )"),
      /*expected_bid=*/
      mojom::BidderWorkletBid::New("null", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()),
      /*expected_data_version=*/absl::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_set_priority=*/absl::nullopt,
      /*expected_update_priority_signals_overrides=*/{{"key", absl::nullopt}});

  // Set a number.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(R"({bid:1, render:"https://response.test/" })",
                              /*extra_code=*/R"(
            setPrioritySignalsOverride("key", 0);
          )"),
      /*expected_bid=*/
      mojom::BidderWorkletBid::New("null", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()),
      /*expected_data_version=*/absl::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_set_priority=*/absl::nullopt,
      /*expected_update_priority_signals_overrides=*/{{"key", 0}});

  // Set a value multiple times.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(R"({bid:1, render:"https://response.test/" })",
                              /*extra_code=*/R"(
            setPrioritySignalsOverride("key", 4);
            setPrioritySignalsOverride("key", null);
            setPrioritySignalsOverride("key", 5);
            setPrioritySignalsOverride("key", 6);
          )"),
      /*expected_bid=*/
      mojom::BidderWorkletBid::New("null", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()),
      /*expected_data_version=*/absl::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_set_priority=*/absl::nullopt,
      /*expected_update_priority_signals_overrides=*/{{"key", 6}});

  // Set multiple values.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(R"({bid:1, render:"https://response.test/" })",
                              /*extra_code=*/R"(
            setPrioritySignalsOverride("key1", 1);
            setPrioritySignalsOverride("key2");
            setPrioritySignalsOverride("key3", -6);
          )"),
      /*expected_bid=*/
      mojom::BidderWorkletBid::New("null", 1, GURL("https://response.test/"),
                                   /*ad_components=*/absl::nullopt,
                                   base::TimeDelta()),
      /*expected_data_version=*/absl::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_set_priority=*/absl::nullopt,
      /*expected_update_priority_signals_overrides=*/
      {{"key1", 1}, {"key2", absl::nullopt}, {"key3", -6}});

  // Overrides should be respected when there's no bid.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(R"({bid:0, render:"https://response.test/" })",
                              /*extra_code=*/R"(
            setPrioritySignalsOverride("key1", 1);
          )"),
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_set_priority=*/absl::nullopt,
      /*expected_update_priority_signals_overrides=*/{{"key1", 1}});

  // Overrides should be respected when there's an invalid bid.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(R"({bid:1/0, render:"https://response.test/" })",
                              /*extra_code=*/R"(
            setPrioritySignalsOverride("key1", 1);
          )"),
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_errors=*/
      {"https://url.test/ generateBid() bid of inf is not a valid bid."},
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_set_priority=*/absl::nullopt,
      /*expected_update_priority_signals_overrides=*/{{"key1", 1}});
}

TEST_F(BidderWorkletTest, ReportWin) {
  RunReportWinWithFunctionBodyExpectingResult(
      "", /*expected_report_url =*/absl::nullopt);
  RunReportWinWithFunctionBodyExpectingResult(
      R"(return "https://ignored.test/")",
      /*expected_report_url =*/absl::nullopt);

  RunReportWinWithFunctionBodyExpectingResult(
      R"(sendReportTo("https://foo.test"))", GURL("https://foo.test/"));
  RunReportWinWithFunctionBodyExpectingResult(
      R"(sendReportTo("https://foo.test/bar"))", GURL("https://foo.test/bar"));

  RunReportWinWithFunctionBodyExpectingResult(
      R"(sendReportTo("http://http.not.allowed.test"))",
      /*expected_report_url =*/absl::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:11 Uncaught TypeError: sendReportTo must be passed a "
       "valid HTTPS url."});
  RunReportWinWithFunctionBodyExpectingResult(
      R"(sendReportTo("file:///file.not.allowed.test"))",
      /*expected_report_url =*/absl::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:11 Uncaught TypeError: sendReportTo must be passed a "
       "valid HTTPS url."});

  RunReportWinWithFunctionBodyExpectingResult(
      R"(sendReportTo(""))", /*expected_report_url =*/absl::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:11 Uncaught TypeError: sendReportTo must be passed a "
       "valid HTTPS url."});

  RunReportWinWithFunctionBodyExpectingResult(
      R"(sendReportTo("https://foo.test");sendReportTo("https://foo.test"))",
      /*expected_report_url =*/absl::nullopt, /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:11 Uncaught TypeError: sendReportTo may be called at "
       "most once."});
}

// Debug win/loss reporting APIs should do nothing when feature
// kBiddingAndScoringDebugReportingAPI is not enabled. It will not fail
// generateBid().
TEST_F(BidderWorkletTest, ForDebuggingOnlyReports) {
  RunGenerateBidWithJavascriptExpectingResult(
      CreateBasicGenerateBidScriptWithDebuggingReport(
          R"(forDebuggingOnly.reportAdAuctionLoss("https://loss.url"))"),
      mojom::BidderWorkletBid::New(
          "[\"ad\"]", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));

  RunGenerateBidWithJavascriptExpectingResult(
      CreateBasicGenerateBidScriptWithDebuggingReport(
          R"(forDebuggingOnly.reportAdAuctionWin("https://win.url"))"),
      mojom::BidderWorkletBid::New(
          "[\"ad\"]", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()));
}

TEST_F(BidderWorkletTest, DeleteBeforeReportWinCallback) {
  AddJavascriptResponse(
      &url_loader_factory_, interest_group_bidding_url_,
      CreateReportWinScript(R"(sendReportTo("https://foo.test"))"));
  auto bidder_worklet = CreateWorklet();
  ASSERT_TRUE(bidder_worklet);

  base::WaitableEvent* event_handle = WedgeV8Thread(v8_helper_.get());
  bidder_worklet->ReportWin(
      interest_group_name_, auction_signals_, per_buyer_signals_,
      direct_from_seller_per_buyer_signals_,
      direct_from_seller_auction_signals_, seller_signals_,
      browser_signal_render_url_, browser_signal_bid_,
      browser_signal_highest_scoring_other_bid_,
      browser_signal_made_highest_scoring_other_bid_,
      browser_signal_seller_origin_, browser_signal_top_level_seller_origin_,
      data_version_.value_or(0), data_version_.has_value(),
      /*trace_id=*/1,
      base::BindOnce([](const absl::optional<GURL>& report_url,
                        const base::flat_map<std::string, GURL>& ad_beacon_map,
                        PrivateAggregationRequests pa_requests,
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
          interest_group_name_,
          /*auction_signals_json=*/base::NumberToString(i), per_buyer_signals_,
          direct_from_seller_per_buyer_signals_,
          direct_from_seller_auction_signals_, seller_signals_,
          browser_signal_render_url_, browser_signal_bid_,
          browser_signal_highest_scoring_other_bid_,
          browser_signal_made_highest_scoring_other_bid_,
          browser_signal_seller_origin_,
          browser_signal_top_level_seller_origin_, data_version_.value_or(0),
          data_version_.has_value(),
          /*trace_id=*/1,
          base::BindLambdaForTesting(
              [&run_loop, &num_report_win_calls, i](
                  const absl::optional<GURL>& report_url,
                  const base::flat_map<std::string, GURL>& ad_beacon_map,
                  PrivateAggregationRequests pa_requests,
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
// the worklet script fails to load.
TEST_F(BidderWorkletTest, ReportWinParallelLoadFails) {
  auto bidder_worklet = CreateWorklet();

  for (size_t i = 0; i < 10; ++i) {
    bidder_worklet->ReportWin(
        interest_group_name_,
        /*auction_signals_json=*/base::NumberToString(i), per_buyer_signals_,
        direct_from_seller_per_buyer_signals_,
        direct_from_seller_auction_signals_, seller_signals_,
        browser_signal_render_url_, browser_signal_bid_,
        browser_signal_highest_scoring_other_bid_,
        browser_signal_made_highest_scoring_other_bid_,
        browser_signal_seller_origin_, browser_signal_top_level_seller_origin_,
        data_version_.value_or(0), data_version_.has_value(),
        /*trace_id=*/1,
        base::BindOnce(
            [](const absl::optional<GURL>& report_url,
               const base::flat_map<std::string, GURL>& ad_beacon_map,
               PrivateAggregationRequests pa_requests,
               const std::vector<std::string>& errors) {
              ADD_FAILURE() << "Callback should not be invoked.";
            }));
  }

  url_loader_factory_.AddResponse(interest_group_bidding_url_.spec(),
                                  CreateBasicGenerateBidScript(),
                                  net::HTTP_NOT_FOUND);

  EXPECT_EQ("Failed to load https://url.test/ HTTP status = 404 Not Found.",
            WaitForDisconnect());
}

// Make sure Date() is not available when running reportWin().
TEST_F(BidderWorkletTest, ReportWinDateNotAvailable) {
  RunReportWinWithFunctionBodyExpectingResult(
      R"(sendReportTo("https://foo.test/" + Date().toString()))",
      /*expected_report_url =*/absl::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:11 Uncaught ReferenceError: Date is not defined."});
}

TEST_F(BidderWorkletTest, ReportWinInterestGroupName) {
  interest_group_name_ = "https://interest.group.name.test/";
  RunReportWinWithFunctionBodyExpectingResult(
      "sendReportTo(browserSignals.interestGroupName)",
      GURL(interest_group_name_));
}

TEST_F(BidderWorkletTest, ReportWinDataVersion) {
  data_version_ = 5u;
  RunReportWinWithFunctionBodyExpectingResult(
      "sendReportTo('https://dataVersion/'+browserSignals.dataVersion)",
      GURL("https://dataVersion/5"));
}

// It shouldn't matter the order in which network fetches complete. For each
// required and optional reportWin() URL load prerequisite, ensure that
// reportWin() completes when that URL is the last loaded URL.
TEST_F(BidderWorkletTest, ReportWinLoadCompletionOrder) {
  constexpr char kJsonResponse[] = "{}";
  constexpr char kDirectFromSellerSignalsHeaders[] =
      "X-Allow-FLEDGE: true\nX-FLEDGE-Auction-Only: true";

  direct_from_seller_per_buyer_signals_ =
      GURL("https://url.test/perbuyersignals");
  direct_from_seller_auction_signals_ = GURL("https://url.test/auctionsignals");

  struct Response {
    GURL response_url;
    std::string response_type;
    std::string headers;
    std::string content;
  };

  const Response kResponses[] = {
      {interest_group_bidding_url_, kJavascriptMimeType, kAllowFledgeHeader,
       CreateReportWinScript(R"(sendReportTo("https://foo.test"))")},
      {*direct_from_seller_per_buyer_signals_, kJsonMimeType,
       kDirectFromSellerSignalsHeaders, kJsonResponse},
      {*direct_from_seller_auction_signals_, kJsonMimeType,
       kDirectFromSellerSignalsHeaders, kJsonResponse}};

  // Cycle such that each response in `kResponses` gets to be the last response,
  // like so:
  //
  // 0,1,2
  // 1,2,0
  // 2,0,1
  for (size_t offset = 0; offset < std::size(kResponses); ++offset) {
    SCOPED_TRACE(offset);
    mojo::Remote<mojom::BidderWorklet> bidder_worklet = CreateWorklet();
    url_loader_factory_.ClearResponses();
    auto run_loop = std::make_unique<base::RunLoop>();
    RunReportWinExpectingResultAsync(bidder_worklet.get(),
                                     GURL("https://foo.test/"), {}, {}, {},
                                     run_loop->QuitClosure());
    for (size_t i = 0; i < std::size(kResponses); ++i) {
      SCOPED_TRACE(i);
      const Response& response =
          kResponses[(i + offset) % std::size(kResponses)];
      AddResponse(
          &url_loader_factory_, response.response_url, response.response_type,
          /*charset=*/absl::nullopt, response.content, response.headers);
      task_environment_.RunUntilIdle();
      if (i < std::size(kResponses) - 1) {
        // Some URLs haven't finished loading -- generateBid() should be
        // blocked.
        EXPECT_FALSE(run_loop->AnyQuitCalled());
      }
    }
    // The last URL for this generateBid() call has completed -- check that
    // generateBid() returns.
    run_loop->Run();
  }
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
      "sendReportTo(sellerSignals)", /*expected_report_url=*/absl::nullopt);

  seller_signals_ = R"("https://interest.group.name.test/")";
  RunReportWinWithFunctionBodyExpectingResult(
      "sendReportTo(sellerSignals)", GURL("https://interest.group.name.test/"));
}

TEST_F(BidderWorkletTest, ReportWinInterestGroupOwner) {
  interest_group_bidding_url_ = GURL("https://foo.test/bar");
  // Add an extra ".test" because origin's shouldn't have a terminal slash,
  // unlike URLs. If an extra slash were added to the origin, this would end up
  // as https://foo.test/.test, instead.
  RunReportWinWithFunctionBodyExpectingResult(
      R"(sendReportTo(browserSignals.interestGroupOwner + ".test"))",
      GURL("https://foo.test.test/"));

  interest_group_bidding_url_ = GURL("https://[::1]:40000/");
  RunReportWinWithFunctionBodyExpectingResult(
      R"(sendReportTo(browserSignals.interestGroupOwner))",
      GURL("https://[::1]:40000/"));
}

TEST_F(BidderWorkletTest, ReportWinBrowserSignalTopWindowOrigin) {
  top_window_origin_ = url::Origin::Create(GURL("https://top.window.test/"));
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

TEST_F(BidderWorkletTest, ReportWinBrowserSignalHighestScoringOtherBid) {
  browser_signal_highest_scoring_other_bid_ = 3;
  RunReportWinWithFunctionBodyExpectingResult(
      R"(if (browserSignals.highestScoringOtherBid === 3)
        sendReportTo("https://jumboshrimp.test"))",
      GURL("https://jumboshrimp.test"));
}

TEST_F(BidderWorkletTest, ReportWinBrowserSignalIsHighestScoringOtherBidMe) {
  browser_signal_made_highest_scoring_other_bid_ = true;
  RunReportWinWithFunctionBodyExpectingResult(
      R"(if (browserSignals.madeHighestScoringOtherBid)
        sendReportTo("https://jumboshrimp.test"))",
      GURL("https://jumboshrimp.test"));
}

TEST_F(BidderWorkletTest, ReportWinBrowserSignalSeller) {
  GURL seller_raw_url = GURL("https://seller.origin.test");
  browser_signal_seller_origin_ = url::Origin::Create(seller_raw_url);
  RunReportWinWithFunctionBodyExpectingResult(
      "sendReportTo(browserSignals.seller)",
      /*expected_report_url=*/seller_raw_url);
}

TEST_F(BidderWorkletTest, ReportWinBrowserSignalTopLevelSeller) {
  browser_signal_top_level_seller_origin_ = absl::nullopt;
  RunReportWinWithFunctionBodyExpectingResult(
      R"(if (!("topLevelSeller" in browserSignals))
            sendReportTo('https://pass.test');)",
      /*expected_report_url=*/GURL("https://pass.test"));

  GURL top_level_seller_raw_url = GURL("https://top.level.seller.origin.test");
  browser_signal_top_level_seller_origin_ =
      url::Origin::Create(top_level_seller_raw_url);
  RunReportWinWithFunctionBodyExpectingResult(
      "sendReportTo(browserSignals.topLevelSeller)",
      /*expected_report_url=*/top_level_seller_raw_url);
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
        interest_group_name_, auction_signals_, per_buyer_signals_,
        direct_from_seller_per_buyer_signals_,
        direct_from_seller_auction_signals_, seller_signals_,
        browser_signal_render_url_, browser_signal_bid_,
        browser_signal_highest_scoring_other_bid_,
        browser_signal_made_highest_scoring_other_bid_,
        browser_signal_seller_origin_, browser_signal_top_level_seller_origin_,
        data_version_.value_or(0), data_version_.has_value(),
        /*trace_id=*/1,
        base::BindLambdaForTesting(
            [&run_loop](const absl::optional<GURL>& report_url,
                        const base::flat_map<std::string, GURL>& ad_beacon_map,
                        PrivateAggregationRequests pa_requests,
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

    const std::string* candidate_method =
        event.value.GetDict().FindString("method");
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
  EXPECT_TRUE(base::ranges::none_of(events1, is_script_parsed));

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
  const std::string* url1 =
      script_parsed1.value.GetDict().FindStringByDottedPath("params.url");
  ASSERT_TRUE(url1);
  EXPECT_EQ(kUrl1, *url1);

  // There shouldn't be a parsed notification on channel 2, however.
  std::list<TestChannel::Event> events2 = channel2->TakeAllEvents();
  EXPECT_TRUE(base::ranges::none_of(events2, is_script_parsed));

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
  const std::string* url2 =
      script_parsed2.value.GetDict().FindStringByDottedPath("params.url");
  ASSERT_TRUE(url2);
  EXPECT_EQ(kUrl2, *url2);

  worklet1.reset();
  worklet2.reset();
  task_environment_.RunUntilIdle();

  // No other scriptParsed events should be on either channel.
  events1 = channel1->TakeAllEvents();
  events2 = channel2->TakeAllEvents();
  EXPECT_TRUE(base::ranges::none_of(events1, is_script_parsed));
  EXPECT_TRUE(base::ranges::none_of(events2, is_script_parsed));
}

TEST_F(BidderWorkletTest, ParseErrorV8Debug) {
  ScopedInspectorSupport inspector_support(v8_helper_.get());
  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        "Invalid Javascript");

  BidderWorklet* worklet_impl = nullptr;
  auto worklet =
      CreateWorklet(interest_group_bidding_url_,
                    /*pause_for_debugger_on_start=*/true, &worklet_impl);
  GenerateBidExpectingNeverCompletes(worklet.get());
  int id = worklet_impl->context_group_id_for_testing();
  TestChannel* channel = inspector_support.ConnectDebuggerSession(id);

  channel->RunCommandAndWaitForResult(
      1, "Runtime.enable", R"({"id":1,"method":"Runtime.enable","params":{}})");
  channel->RunCommandAndWaitForResult(
      2, "Debugger.enable",
      R"({"id":2,"method":"Debugger.enable","params":{}})");

  // Unpause execution.
  channel->RunCommandAndWaitForResult(
      3, "Runtime.runIfWaitingForDebugger",
      R"({"id":3,"method":"Runtime.runIfWaitingForDebugger","params":{}})");

  // Worklet should disconnect with an error message.
  EXPECT_FALSE(WaitForDisconnect().empty());

  // Should have gotten a parse error notification.
  TestChannel::Event parse_error =
      channel->WaitForMethodNotification("Debugger.scriptFailedToParse");
  const std::string* error_url =
      parse_error.value.GetDict().FindStringByDottedPath("params.url");
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
      CreateWorklet(GURL(kUrl1), /*pause_for_debugger_on_start=*/true);
  GenerateBid(worklet1.get());
  auto worklet2 =
      CreateWorklet(GURL(kUrl2), /*pause_for_debugger_on_start=*/true);
  GenerateBid(worklet2.get());

  mojo::AssociatedRemote<blink::mojom::DevToolsAgent> agent1, agent2;
  worklet1->ConnectDevToolsAgent(agent1.BindNewEndpointAndPassReceiver());
  worklet2->ConnectDevToolsAgent(agent2.BindNewEndpointAndPassReceiver());

  TestDevToolsAgentClient debug1(std::move(agent1), "123",
                                 /*use_binary_protocol=*/true);
  TestDevToolsAgentClient debug2(std::move(agent2), "456",
                                 /*use_binary_protocol=*/true);

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
  const std::string* url1 =
      script_parsed1.value.GetDict().FindStringByDottedPath("params.url");
  ASSERT_TRUE(url1);
  EXPECT_EQ(*url1, kUrl1);
  absl::optional<int> context_id1 =
      script_parsed1.value.GetDict().FindIntByDottedPath(
          "params.executionContextId");
  ASSERT_TRUE(context_id1.has_value());

  // Next there is the breakpoint.
  TestDevToolsAgentClient::Event breakpoint_hit1 =
      debug1.WaitForMethodNotification("Debugger.paused");

  base::Value::List* hit_breakpoints1 =
      breakpoint_hit1.value.GetDict().FindListByDottedPath(
          "params.hitBreakpoints");
  ASSERT_TRUE(hit_breakpoints1);
  ASSERT_EQ(1u, hit_breakpoints1->size());
  ASSERT_TRUE((*hit_breakpoints1)[0].is_string());
  EXPECT_EQ("1:0:0:http://example.com/first.js",
            (*hit_breakpoints1)[0].GetString());

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

  base::Value::List* hit_breakpoints2 =
      breakpoint_hit2.value.GetDict().FindListByDottedPath(
          "params.hitBreakpoints");
  ASSERT_TRUE(hit_breakpoints2);
  ASSERT_EQ(1u, hit_breakpoints2->size());
  ASSERT_TRUE((*hit_breakpoints2)[0].is_string());
  EXPECT_EQ("1:0:0:http://example.org/second.js",
            (*hit_breakpoints2)[0].GetString());

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
      CreateWorklet(GURL(kUrl), /*pause_for_debugger_on_start=*/true);
  GenerateBid(worklet.get());

  mojo::AssociatedRemote<blink::mojom::DevToolsAgent> agent;
  worklet->ConnectDevToolsAgent(agent.BindNewEndpointAndPassReceiver());

  TestDevToolsAgentClient debug(std::move(agent), "123",
                                /*use_binary_protocol=*/true);

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
      breakpoint_hit1.value.GetDict().FindStringByDottedPath(
          "params.data.eventName");
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
                                   {}, {}, run_loop.QuitClosure());

  TestDevToolsAgentClient::Event breakpoint_hit2 =
      debug.WaitForMethodNotification("Debugger.paused");
  const std::string* breakpoint2 =
      breakpoint_hit2.value.GetDict().FindStringByDottedPath(
          "params.data.eventName");
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

  mojo::AssociatedRemote<blink::mojom::DevToolsAgent> agent;
  worklet->ConnectDevToolsAgent(agent.BindNewEndpointAndPassReceiver());

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

TEST_F(BidderWorkletTest, ExecutionModeGroupByOrigin) {
  const char kScript[] = R"(
    if (!('count' in globalThis))
      globalThis.count = 0;
    function generateBid() {
      ++count;
      return {ad: ["ad"], bid:count, render:"https://response.test/"};
    }
  )";

  mojo::Remote<mojom::BidderWorklet> bidder_worklet = CreateWorklet();
  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        kScript);

  // Run 1, start group.
  execution_mode_ =
      blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode;
  join_origin_ = url::Origin::Create(GURL("https://url.test/"));
  GenerateBid(bidder_worklet.get());
  load_script_run_loop_ = std::make_unique<base::RunLoop>();
  load_script_run_loop_->Run();
  ASSERT_TRUE(bid_);
  EXPECT_EQ(1, bid_->bid);

  // Run 2, same group.
  GenerateBid(bidder_worklet.get());
  load_script_run_loop_ = std::make_unique<base::RunLoop>();
  load_script_run_loop_->Run();
  ASSERT_TRUE(bid_);
  EXPECT_EQ(2, bid_->bid);

  // Run 3, not in group.
  execution_mode_ =
      blink::mojom::InterestGroup::ExecutionMode::kCompatibilityMode;
  join_origin_ = url::Origin::Create(GURL("https://url2.test/"));
  GenerateBid(bidder_worklet.get());
  load_script_run_loop_ = std::make_unique<base::RunLoop>();
  load_script_run_loop_->Run();
  ASSERT_TRUE(bid_);
  EXPECT_EQ(1, bid_->bid);

  // Run 4, back to group.
  execution_mode_ =
      blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode;
  join_origin_ = url::Origin::Create(GURL("https://url.test/"));
  GenerateBid(bidder_worklet.get());
  load_script_run_loop_ = std::make_unique<base::RunLoop>();
  load_script_run_loop_->Run();
  ASSERT_TRUE(bid_);
  EXPECT_EQ(3, bid_->bid);

  // Run 5, different group.
  join_origin_ = url::Origin::Create(GURL("https://url2.test/"));
  GenerateBid(bidder_worklet.get());
  load_script_run_loop_ = std::make_unique<base::RunLoop>();
  load_script_run_loop_->Run();
  ASSERT_TRUE(bid_);
  EXPECT_EQ(1, bid_->bid);

  // Run 5, different group cont'd.
  GenerateBid(bidder_worklet.get());
  load_script_run_loop_ = std::make_unique<base::RunLoop>();
  load_script_run_loop_->Run();
  ASSERT_TRUE(bid_);
  EXPECT_EQ(2, bid_->bid);
}

// Test that cancelling the worklet before it runs but after the execution was
// queued actually cancels the execution. This is done by trying to run a
// while(true) {} script with a timeout that's bigger than the test timeout, so
// if it doesn't get cancelled the *test* will timeout.
TEST_F(BidderWorkletTest, Cancelation) {
  per_buyer_timeout_ = base::Days(360);

  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        "while(true) {}");

  mojo::Remote<mojom::BidderWorklet> bidder_worklet = CreateWorklet();
  // Let the script load.
  task_environment_.RunUntilIdle();

  // Now we no longer need it for parsing JS, wedge the V8 thread so we get a
  // chance to cancel the script *before* it actually tries running.
  base::WaitableEvent* event_handle = WedgeV8Thread(v8_helper_.get());

  GenerateBidClientWithCallbacks client(
      GenerateBidClientWithCallbacks::GenerateBidNeverInvokedCallback());
  mojo::AssociatedReceiver<mojom::GenerateBidClient> client_receiver(&client);
  GenerateBid(bidder_worklet.get(),
              client_receiver.BindNewEndpointAndPassRemote());

  // Spin the event loop to let the signals negotiation go through. This is
  // for this thread only since the V8 thread is wedged, but it's enough since
  // all mojo objects live here.  This should queue the V8 thread task for
  // execution.
  base::RunLoop().RunUntilIdle();

  // Cancel and then unwedge.
  client_receiver.reset();
  base::RunLoop().RunUntilIdle();
  event_handle->Signal();

  // Make sure cancellation happens before ~BidderWorklet.
  task_environment_.RunUntilIdle();
}

// Test that queued tasks get cancelled at worklet destruction.
TEST_F(BidderWorkletTest, CancelationDtor) {
  per_buyer_timeout_ = base::Days(360);
  // ReportWin timeout isn't configurable the way generateBid is.
  v8_helper_->v8_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<AuctionV8Helper> v8_helper) {
            v8_helper->set_script_timeout_for_testing(base::Days(360));
          },
          v8_helper_));

  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        "while(true) {}");

  mojo::Remote<mojom::BidderWorklet> bidder_worklet = CreateWorklet();
  // Let the script load.
  task_environment_.RunUntilIdle();

  // Now we no longer need it for parsing JS, wedge the V8 thread so we get a
  // chance to cancel the script *before* it actually tries running.
  base::WaitableEvent* event_handle = WedgeV8Thread(v8_helper_.get());

  GenerateBid(bidder_worklet.get());
  bidder_worklet->ReportWin(
      interest_group_name_, auction_signals_, per_buyer_signals_,
      direct_from_seller_per_buyer_signals_,
      direct_from_seller_auction_signals_, seller_signals_,
      browser_signal_render_url_, browser_signal_bid_,
      browser_signal_highest_scoring_other_bid_,
      browser_signal_made_highest_scoring_other_bid_,
      browser_signal_seller_origin_, browser_signal_top_level_seller_origin_,
      data_version_.value_or(0), data_version_.has_value(),
      /*trace_id=*/1,
      base::BindOnce([](const absl::optional<GURL>& report_url,
                        const base::flat_map<std::string, GURL>& ad_beacon_map,
                        PrivateAggregationRequests pa_requests,
                        const std::vector<std::string>& errors) {
        ADD_FAILURE() << "Callback should not be invoked.";
      }));

  // Spin the event loop to let the signals negotiation go through. This is
  // for this thread only since the V8 thread is wedged, but it's enough since
  // all mojo objects live here.  This should queue the V8 thread task for
  // execution.
  base::RunLoop().RunUntilIdle();

  // Destroy the worklet, then unwedge.
  bidder_worklet.reset();
  base::RunLoop().RunUntilIdle();
  event_handle->Signal();
}

// Test that cancelling execution before the script is fetched doesn't run it.
TEST_F(BidderWorkletTest, CancelBeforeFetch) {
  per_buyer_timeout_ = base::Days(360);

  mojo::Remote<mojom::BidderWorklet> bidder_worklet = CreateWorklet();
  GenerateBidClientWithCallbacks client(
      GenerateBidClientWithCallbacks::GenerateBidNeverInvokedCallback());
  mojo::AssociatedReceiver<mojom::GenerateBidClient> client_receiver(&client);
  GenerateBid(bidder_worklet.get(),
              client_receiver.BindNewEndpointAndPassRemote());

  // Let them talk about trusted signals.
  task_environment_.RunUntilIdle();

  // Cancel and then make the script available.
  client_receiver.reset();
  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        "while (true) {}");

  // Make sure cancellation happens before ~BidderWorklet.
  task_environment_.RunUntilIdle();
}

class BidderWorkletBiddingAndScoringDebugReportingAPIEnabledTest
    : public BidderWorkletTest {
 public:
  BidderWorkletBiddingAndScoringDebugReportingAPIEnabledTest() {
    feature_list_.InitAndEnableFeature(
        blink::features::kBiddingAndScoringDebugReportingAPI);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

// Test forDebuggingOnly.reportAdAuctionLoss() and
// forDebuggingOnly.reportAdAuctionWin() called in generateBid().
TEST_F(BidderWorkletBiddingAndScoringDebugReportingAPIEnabledTest,
       ForDebuggingOnlyReports) {
  RunGenerateBidWithJavascriptExpectingResult(
      CreateBasicGenerateBidScriptWithDebuggingReport(
          R"(forDebuggingOnly.reportAdAuctionLoss("https://loss.url");
            forDebuggingOnly.reportAdAuctionWin("https://win.url"))"),
      mojom::BidderWorkletBid::New(
          "[\"ad\"]", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()),
      /*expected_data_version=*/absl::nullopt,
      /*expected_errors=*/{}, GURL("https://loss.url"),
      GURL("https://win.url"));

  // It's OK to call one API but not the other.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateBasicGenerateBidScriptWithDebuggingReport(
          R"(forDebuggingOnly.reportAdAuctionLoss("https://loss.url"))"),
      mojom::BidderWorkletBid::New(
          "[\"ad\"]", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()),
      /*expected_data_version=*/absl::nullopt,
      /*expected_errors=*/{}, GURL("https://loss.url"),
      /*expected_debug_win_report_url=*/absl::nullopt);
  RunGenerateBidWithJavascriptExpectingResult(
      CreateBasicGenerateBidScriptWithDebuggingReport(
          R"(forDebuggingOnly.reportAdAuctionWin("https://win.url"))"),
      mojom::BidderWorkletBid::New(
          "[\"ad\"]", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()),
      /*expected_data_version=*/absl::nullopt,
      /*expected_errors=*/{}, /*expected_debug_loss_report_url=*/absl::nullopt,
      GURL("https://win.url"));
}

TEST_F(BidderWorkletBiddingAndScoringDebugReportingAPIEnabledTest,
       ForDebuggingOnlyReportsInvalidParameter) {
  RunGenerateBidWithJavascriptExpectingResult(
      CreateBasicGenerateBidScriptWithDebuggingReport(
          R"(forDebuggingOnly.reportAdAuctionLoss(null))"),
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:5 Uncaught TypeError: "
       "reportAdAuctionLoss requires 1 string parameter."});

  RunGenerateBidWithJavascriptExpectingResult(
      CreateBasicGenerateBidScriptWithDebuggingReport(
          R"(forDebuggingOnly.reportAdAuctionWin([5]))"),
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:5 Uncaught TypeError: "
       "reportAdAuctionWin requires 1 string parameter."});

  std::vector<std::string> non_https_urls = {"http://report.url",
                                             "file:///foo/", "Not a URL"};
  for (const auto& url : non_https_urls) {
    RunGenerateBidWithJavascriptExpectingResult(
        CreateBasicGenerateBidScriptWithDebuggingReport(base::StringPrintf(
            R"(forDebuggingOnly.reportAdAuctionLoss("%s"))", url.c_str())),
        /*expected_bid=*/mojom::BidderWorkletBidPtr(),
        /*expected_data_version=*/absl::nullopt,
        {"https://url.test/:5 Uncaught TypeError: "
         "reportAdAuctionLoss must be passed a valid HTTPS url."});

    RunGenerateBidWithJavascriptExpectingResult(
        CreateBasicGenerateBidScriptWithDebuggingReport(base::StringPrintf(
            R"(forDebuggingOnly.reportAdAuctionWin("%s"))", url.c_str())),
        /*expected_bid=*/mojom::BidderWorkletBidPtr(),
        /*expected_data_version=*/absl::nullopt,
        {"https://url.test/:5 Uncaught TypeError: "
         "reportAdAuctionWin must be passed a valid HTTPS url."});
  }

  // No message if caught, but still no debug report URLs.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateBasicGenerateBidScriptWithDebuggingReport(
          R"(try {forDebuggingOnly.reportAdAuctionLoss("http://loss.url")}
            catch (e) {})"),
      mojom::BidderWorkletBid::New(
          "[\"ad\"]", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()),
      /*expected_data_version=*/absl::nullopt,
      /*expected_errors=*/{}, /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt);
}

TEST_F(BidderWorkletBiddingAndScoringDebugReportingAPIEnabledTest,
       ForDebuggingOnlyReportsMultiCallsAllowed) {
  RunGenerateBidWithJavascriptExpectingResult(
      CreateBasicGenerateBidScriptWithDebuggingReport(
          R"(forDebuggingOnly.reportAdAuctionLoss("https://loss.url");
            forDebuggingOnly.reportAdAuctionLoss("https://loss.url2"))"),
      mojom::BidderWorkletBid::New(
          "[\"ad\"]", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()),
      /*expected_data_version=*/absl::nullopt,
      /*expected_errors=*/{}, GURL("https://loss.url2"),
      /*expected_debug_win_report_url=*/absl::nullopt);

  RunGenerateBidWithJavascriptExpectingResult(
      CreateBasicGenerateBidScriptWithDebuggingReport(
          R"(forDebuggingOnly.reportAdAuctionWin("https://win.url");
            forDebuggingOnly.reportAdAuctionWin("https://win.url2"))"),
      mojom::BidderWorkletBid::New(
          "[\"ad\"]", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()),
      /*expected_data_version=*/absl::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/absl::nullopt,
      GURL("https://win.url2"));
}

TEST_F(BidderWorkletBiddingAndScoringDebugReportingAPIEnabledTest,
       GenerateBidHasError) {
  // The bidding script has an error statement and the script will fail. But
  // th loss report URLs before bidding script encounters error should be kept.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          /*raw_return_value=*/"[\"ad\"]",
          R"(forDebuggingOnly.reportAdAuctionLoss("https://loss.url1");
            error;
            forDebuggingOnly.reportAdAuctionLoss("https://loss.url2"))"),
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/:6 Uncaught ReferenceError: error is not defined."},
      GURL("https://loss.url1"));
}

TEST_F(BidderWorkletBiddingAndScoringDebugReportingAPIEnabledTest,
       GenerateBidInvalidReturnValue) {
  // Keep debugging loss report URLs when generateBid() returns invalid
  // value type.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          R"({ad: ["ad"], bid:"invalid", render:"https://response.test/"})",
          R"(forDebuggingOnly.reportAdAuctionLoss("https://loss.url");
            forDebuggingOnly.reportAdAuctionWin("https://win.url"))"),
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ generateBid() returned object must have numeric bid "
       "field."},
      GURL("https://loss.url"),
      /*expected_debug_win_report_url=*/absl::nullopt);
}

// Loss report URLs before bidding script times out should be kept.
TEST_F(BidderWorkletBiddingAndScoringDebugReportingAPIEnabledTest,
       GenerateBidTimedOut) {
  // The bidding script has an endless while loop. It will time out due to
  // AuctionV8Helper's default script timeout (50 ms).
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          /*raw_return_value=*/"[\"ad\"]",
          R"(forDebuggingOnly.reportAdAuctionLoss("https://loss.url1");
            while (1);
            forDebuggingOnly.reportAdAuctionLoss("https://loss.url2"))"),
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      {"https://url.test/ execution of `generateBid` timed out."},
      GURL("https://loss.url1"));
}

TEST_F(BidderWorkletTest, ReportWinRegisterAdBeacon) {
  base::flat_map<std::string, GURL> expected_ad_beacon_map = {
      {"click", GURL("https://click.example.com/")},
      {"view", GURL("https://view.example.com/")},
  };
  RunReportWinWithFunctionBodyExpectingResult(
      R"(registerAdBeacon({
        'click': "https://click.example.com/",
        'view': "https://view.example.com/",
      }))",
      /*expected_report_url =*/absl::nullopt, expected_ad_beacon_map);

  // Don't call twice.
  RunReportWinWithFunctionBodyExpectingResult(
      R"(registerAdBeacon({
        'click': "https://click.example.com/",
        'view': "https://view.example.com/",
      });
      registerAdBeacon())",
      /*expected_report_url =*/absl::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:15 Uncaught TypeError: registerAdBeacon may be "
       "called at most once."});

  // If called twice and the error is caught, use the first result.
  RunReportWinWithFunctionBodyExpectingResult(
      R"(registerAdBeacon({
           'click': "https://click.example.com/",
           'view': "https://view.example.com/",
         });
         try { registerAdBeacon() }
         catch (e) {})",
      /*expected_report_url =*/absl::nullopt, expected_ad_beacon_map);

  // If error on first call, can be called again.
  RunReportWinWithFunctionBodyExpectingResult(
      R"(try { registerAdBeacon() }
         catch (e) {}
         registerAdBeacon({
           'click': "https://click.example.com/",
           'view': "https://view.example.com/",
         }))",
      /*expected_report_url =*/absl::nullopt, expected_ad_beacon_map);

  // Error if no parameters
  RunReportWinWithFunctionBodyExpectingResult(
      R"(registerAdBeacon())",
      /*expected_report_url =*/absl::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:11 Uncaught TypeError: registerAdBeacon requires 1 "
       "object parameter."});

  // Error if parameter is not an object
  RunReportWinWithFunctionBodyExpectingResult(
      R"(registerAdBeacon("foo"))",
      /*expected_report_url =*/absl::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:11 Uncaught TypeError: registerAdBeacon requires 1 "
       "object parameter."});

  // Error if parameter is not an object
  RunReportWinWithFunctionBodyExpectingResult(
      R"(registerAdBeacon("foo"))",
      /*expected_report_url =*/absl::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:11 Uncaught TypeError: registerAdBeacon requires 1 "
       "object parameter."});

  // Error if parameter attributes are not strings
  RunReportWinWithFunctionBodyExpectingResult(
      R"(registerAdBeacon({
        'click': "https://click.example.com/",
        1: "https://view.example.com/",
      }))",
      /*expected_report_url =*/absl::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:11 Uncaught TypeError: registerAdBeacon object "
       "attributes must be strings."});

  // Error if invalid reporting URL
  RunReportWinWithFunctionBodyExpectingResult(
      R"(registerAdBeacon({
        'click': "https://click.example.com/",
        'view': "gopher://view.example.com/",
      }))",
      /*expected_report_url =*/absl::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:11 Uncaught TypeError: registerAdBeacon invalid "
       "reporting url for key 'view': 'gopher://view.example.com/'."});

  // Error if not trustworthy reporting URL
  RunReportWinWithFunctionBodyExpectingResult(
      R"(registerAdBeacon({
        'click': "https://127.0.0.1/",
        'view': "http://view.example.com/",
      }))",
      /*expected_report_url =*/absl::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:11 Uncaught TypeError: registerAdBeacon invalid "
       "reporting url for key 'view': 'http://view.example.com/'."});
}

class BidderWorkletSharedStorageAPIDisabledTest : public BidderWorkletTest {
 public:
  BidderWorkletSharedStorageAPIDisabledTest() {
    feature_list_.InitAndDisableFeature(blink::features::kSharedStorageAPI);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(BidderWorkletSharedStorageAPIDisabledTest, SharedStorageNotExposed) {
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          R"({ad: "ad", bid:1, render:"https://response.test/" })",
          /*extra_code=*/R"(
          sharedStorage.clear();
        )"),
      /*expected_bid=*/nullptr,
      /*expected_data_version=*/absl::nullopt,
      /*expected_errors=*/
      {"https://url.test/:6 Uncaught ReferenceError: sharedStorage is not "
       "defined."},
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_set_priority=*/absl::nullopt,
      /*expected_update_priority_signals_overrides=*/{},
      /*expected_pa_requests=*/{});

  RunReportWinWithFunctionBodyExpectingResult(
      R"(
        sharedStorage.clear();
      )",
      /*expected_report_url =*/absl::nullopt,
      /*expected_ad_beacon_map=*/{}, /*expected_pa_requests=*/{},
      /*expected_errors=*/
      {"https://url.test/:12 Uncaught ReferenceError: sharedStorage is not "
       "defined."});
}

class BidderWorkletSharedStorageAPIEnabledTest : public BidderWorkletTest {
 public:
  BidderWorkletSharedStorageAPIEnabledTest() {
    feature_list_.InitAndEnableFeature(blink::features::kSharedStorageAPI);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(BidderWorkletSharedStorageAPIEnabledTest,
       SharedStorageWriteInGenerateBid) {
  auction_worklet::TestAuctionSharedStorageHost test_shared_storage_host;

  {
    mojo::Receiver<auction_worklet::mojom::AuctionSharedStorageHost> receiver(
        &test_shared_storage_host);
    shared_storage_host_remote_ = receiver.BindNewPipeAndPassRemote();

    RunGenerateBidWithJavascriptExpectingResult(
        CreateGenerateBidScript(
            R"({ad: "ad", bid:1, render:"https://response.test/" })",
            /*extra_code=*/R"(
          sharedStorage.set('a', 'b');
          sharedStorage.set('a', 'b', {ignoreIfPresent: true});
          sharedStorage.append('a', 'b');
          sharedStorage.delete('a');
          sharedStorage.clear();
        )"),
        /*expected_bid=*/
        mojom::BidderWorkletBid::New(
            "\"ad\"", 1, GURL("https://response.test/"),
            /*ad_components=*/absl::nullopt, base::TimeDelta()),
        /*expected_data_version=*/absl::nullopt,
        /*expected_errors=*/{},
        /*expected_debug_loss_report_url=*/absl::nullopt,
        /*expected_debug_win_report_url=*/absl::nullopt,
        /*expected_set_priority=*/absl::nullopt,
        /*expected_update_priority_signals_overrides=*/{},
        /*expected_pa_requests=*/{});

    // Make sure the shared storage mojom methods are invoked as they use a
    // dedicated pipe.
    task_environment_.RunUntilIdle();

    using RequestType =
        auction_worklet::TestAuctionSharedStorageHost::RequestType;
    using Request = auction_worklet::TestAuctionSharedStorageHost::Request;

    EXPECT_THAT(test_shared_storage_host.observed_requests(),
                testing::ElementsAre(Request{.type = RequestType::kSet,
                                             .key = u"a",
                                             .value = u"b",
                                             .ignore_if_present = false},
                                     Request{.type = RequestType::kSet,
                                             .key = u"a",
                                             .value = u"b",
                                             .ignore_if_present = true},
                                     Request{.type = RequestType::kAppend,
                                             .key = u"a",
                                             .value = u"b",
                                             .ignore_if_present = false},
                                     Request{.type = RequestType::kDelete,
                                             .key = u"a",
                                             .value = std::u16string(),
                                             .ignore_if_present = false},
                                     Request{.type = RequestType::kClear,
                                             .key = std::u16string(),
                                             .value = std::u16string(),
                                             .ignore_if_present = false}));
  }

  {
    shared_storage_host_remote_ =
        mojo::PendingRemote<mojom::AuctionSharedStorageHost>();

    // Set the shared-storage permissions policy to disallowed.
    permissions_policy_state_ =
        mojom::AuctionWorkletPermissionsPolicyState::New(
            /*private_aggregation_allowed=*/true,
            /*shared_storage_allowed=*/false);

    RunGenerateBidWithJavascriptExpectingResult(
        CreateGenerateBidScript(
            R"({ad: "ad", bid:1, render:"https://response.test/" })",
            /*extra_code=*/R"(
            sharedStorage.clear();
          )"),
        /*expected_bid=*/nullptr,
        /*expected_data_version=*/absl::nullopt,
        /*expected_errors=*/
        {"https://url.test/:6 Uncaught TypeError: The \"shared-storage\" "
         "Permissions Policy denied the method on sharedStorage."},
        /*expected_debug_loss_report_url=*/absl::nullopt,
        /*expected_debug_win_report_url=*/absl::nullopt,
        /*expected_set_priority=*/absl::nullopt,
        /*expected_update_priority_signals_overrides=*/{},
        /*expected_pa_requests=*/{});

    permissions_policy_state_ =
        mojom::AuctionWorkletPermissionsPolicyState::New(
            /*private_aggregation_allowed=*/true,
            /*shared_storage_allowed=*/true);
  }
}

TEST_F(BidderWorkletSharedStorageAPIEnabledTest,
       SharedStorageWriteInReportWin) {
  auction_worklet::TestAuctionSharedStorageHost test_shared_storage_host;

  {
    mojo::Receiver<auction_worklet::mojom::AuctionSharedStorageHost> receiver(
        &test_shared_storage_host);
    shared_storage_host_remote_ = receiver.BindNewPipeAndPassRemote();

    RunReportWinWithFunctionBodyExpectingResult(
        R"(
          sharedStorage.set('a', 'b');
          sharedStorage.set('a', 'b', {ignoreIfPresent: true});
          sharedStorage.append('a', 'b');
          sharedStorage.delete('a');
          sharedStorage.clear();
        )",
        /*expected_report_url =*/absl::nullopt,
        /*expected_ad_beacon_map=*/{}, /*expected_pa_requests=*/{},
        /*expected_errors=*/{});

    // Make sure the shared storage mojom methods are invoked as they use a
    // dedicated pipe.
    task_environment_.RunUntilIdle();

    using RequestType =
        auction_worklet::TestAuctionSharedStorageHost::RequestType;
    using Request = auction_worklet::TestAuctionSharedStorageHost::Request;

    EXPECT_THAT(test_shared_storage_host.observed_requests(),
                testing::ElementsAre(Request{.type = RequestType::kSet,
                                             .key = u"a",
                                             .value = u"b",
                                             .ignore_if_present = false},
                                     Request{.type = RequestType::kSet,
                                             .key = u"a",
                                             .value = u"b",
                                             .ignore_if_present = true},
                                     Request{.type = RequestType::kAppend,
                                             .key = u"a",
                                             .value = u"b",
                                             .ignore_if_present = false},
                                     Request{.type = RequestType::kDelete,
                                             .key = u"a",
                                             .value = std::u16string(),
                                             .ignore_if_present = false},
                                     Request{.type = RequestType::kClear,
                                             .key = std::u16string(),
                                             .value = std::u16string(),
                                             .ignore_if_present = false}));
  }

  {
    shared_storage_host_remote_ =
        mojo::PendingRemote<mojom::AuctionSharedStorageHost>();

    // Set the shared-storage permissions policy to disallowed.
    permissions_policy_state_ =
        mojom::AuctionWorkletPermissionsPolicyState::New(
            /*private_aggregation_allowed=*/true,
            /*shared_storage_allowed=*/false);

    RunReportWinWithFunctionBodyExpectingResult(
        R"(
          sharedStorage.clear();
        )",
        /*expected_report_url =*/absl::nullopt,
        /*expected_ad_beacon_map=*/{}, /*expected_pa_requests=*/{},
        /*expected_errors=*/
        {"https://url.test/:12 Uncaught TypeError: The \"shared-storage\" "
         "Permissions Policy denied the method on sharedStorage."});

    permissions_policy_state_ =
        mojom::AuctionWorkletPermissionsPolicyState::New(
            /*private_aggregation_allowed=*/true,
            /*shared_storage_allowed=*/true);
  }
}

class BidderWorkletPrivateAggregationEnabledTest : public BidderWorkletTest {
 public:
  BidderWorkletPrivateAggregationEnabledTest() {
    scoped_feature_list_.InitWithFeatures(
        {content::kPrivateAggregationApi,
         blink::features::kPrivateAggregationApiFledgeExtensions},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(BidderWorkletPrivateAggregationEnabledTest, GenerateBid) {
  mojom::PrivateAggregationRequest kExpectedRequest1(
      mojom::AggregatableReportContribution::NewHistogramContribution(
          content::mojom::AggregatableReportHistogramContribution::New(
              /*bucket=*/123,
              /*value=*/45)),
      content::mojom::AggregationServiceMode::kDefault,
      content::mojom::DebugModeDetails::New());
  mojom::PrivateAggregationRequest kExpectedRequest2(
      mojom::AggregatableReportContribution::NewHistogramContribution(
          content::mojom::AggregatableReportHistogramContribution::New(
              /*bucket=*/absl::MakeInt128(/*high=*/1,
                                          /*low=*/0),
              /*value=*/1)),
      content::mojom::AggregationServiceMode::kDefault,
      content::mojom::DebugModeDetails::New());

  mojom::PrivateAggregationRequest kExpectedForEventRequest1(
      mojom::AggregatableReportContribution::NewForEventContribution(
          mojom::AggregatableReportForEventContribution::New(
              /*bucket=*/mojom::ForEventSignalBucket::NewIdBucket(234),
              /*value=*/mojom::ForEventSignalValue::NewIntValue(56),
              /*event_type=*/"reserved.win")),
      content::mojom::AggregationServiceMode::kDefault,
      content::mojom::DebugModeDetails::New());
  mojom::PrivateAggregationRequest kExpectedForEventRequest2(
      mojom::AggregatableReportContribution::NewForEventContribution(
          mojom::AggregatableReportForEventContribution::New(
              /*bucket=*/mojom::ForEventSignalBucket::NewIdBucket(
                  absl::MakeInt128(/*high=*/1,
                                   /*low=*/0)),
              /*value=*/mojom::ForEventSignalValue::NewIntValue(2),
              /*event_type=*/"reserved.win")),
      content::mojom::AggregationServiceMode::kDefault,
      content::mojom::DebugModeDetails::New());

  // Only sendHistogramReport() is called.
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(kExpectedRequest1.Clone());

    RunGenerateBidWithJavascriptExpectingResult(
        CreateGenerateBidScript(
            R"({ad: "ad", bid:1, render:"https://response.test/" })",
            /*extra_code=*/R"(
            privateAggregation.sendHistogramReport({bucket: 123n, value: 45});
          )"),
        /*expected_bid=*/
        mojom::BidderWorkletBid::New(
            "\"ad\"", 1, GURL("https://response.test/"),
            /*ad_components=*/absl::nullopt, base::TimeDelta()),
        /*expected_data_version=*/absl::nullopt,
        /*expected_errors=*/{},
        /*expected_debug_loss_report_url=*/absl::nullopt,
        /*expected_debug_win_report_url=*/absl::nullopt,
        /*expected_set_priority=*/absl::nullopt,
        /*expected_update_priority_signals_overrides=*/{},
        std::move(expected_pa_requests));
  }

  // Only reportContributionForEvent() is called.
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(kExpectedForEventRequest1.Clone());

    RunGenerateBidWithJavascriptExpectingResult(
        CreateGenerateBidScript(
            R"({ad: "ad", bid:1, render:"https://response.test/" })",
            /*extra_code=*/R"(
            privateAggregation.reportContributionForEvent(
                "reserved.win", {bucket: 234n, value: 56});
          )"),
        /*expected_bid=*/
        mojom::BidderWorkletBid::New(
            "\"ad\"", 1, GURL("https://response.test/"),
            /*ad_components=*/absl::nullopt, base::TimeDelta()),
        /*expected_data_version=*/absl::nullopt,
        /*expected_errors=*/{},
        /*expected_debug_loss_report_url=*/absl::nullopt,
        /*expected_debug_win_report_url=*/absl::nullopt,
        /*expected_set_priority=*/absl::nullopt,
        /*expected_update_priority_signals_overrides=*/{},
        std::move(expected_pa_requests));
  }

  // Both sendHistogramReport() and reportContributionForEvent() are called.
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(kExpectedRequest1.Clone());
    expected_pa_requests.push_back(kExpectedForEventRequest1.Clone());

    RunGenerateBidWithJavascriptExpectingResult(
        CreateGenerateBidScript(
            R"({ad: "ad", bid:1, render:"https://response.test/" })",
            /*extra_code=*/R"(
            privateAggregation.sendHistogramReport({bucket: 123n, value: 45});
            privateAggregation.reportContributionForEvent(
                "reserved.win", {bucket: 234n, value: 56});
          )"),
        /*expected_bid=*/
        mojom::BidderWorkletBid::New(
            "\"ad\"", 1, GURL("https://response.test/"),
            /*ad_components=*/absl::nullopt, base::TimeDelta()),
        /*expected_data_version=*/absl::nullopt,
        /*expected_errors=*/{},
        /*expected_debug_loss_report_url=*/absl::nullopt,
        /*expected_debug_win_report_url=*/absl::nullopt,
        /*expected_set_priority=*/absl::nullopt,
        /*expected_update_priority_signals_overrides=*/{},
        std::move(expected_pa_requests));
  }

  // Set the private-aggregation permissions policy to disallowed.
  {
    permissions_policy_state_ =
        mojom::AuctionWorkletPermissionsPolicyState::New(
            /*private_aggregation_allowed=*/false,
            /*shared_storage_allowed=*/true);

    RunGenerateBidWithJavascriptExpectingResult(
        CreateGenerateBidScript(
            R"({ad: "ad", bid:1, render:"https://response.test/" })",
            /*extra_code=*/R"(
            privateAggregation.sendHistogramReport({bucket: 123n, value: 45});
          )"),
        /*expected_bid=*/nullptr,
        /*expected_data_version=*/absl::nullopt,
        /*expected_errors=*/
        {"https://url.test/:6 Uncaught TypeError: The \"private-aggregation\" "
         "Permissions Policy denied the method on privateAggregation."},
        /*expected_debug_loss_report_url=*/absl::nullopt,
        /*expected_debug_win_report_url=*/absl::nullopt,
        /*expected_set_priority=*/absl::nullopt,
        /*expected_update_priority_signals_overrides=*/{},
        /*expected_pa_requests=*/{});

    permissions_policy_state_ =
        mojom::AuctionWorkletPermissionsPolicyState::New(
            /*private_aggregation_allowed=*/true,
            /*shared_storage_allowed=*/true);
  }

  // Large bucket
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(kExpectedRequest2.Clone());
    expected_pa_requests.push_back(kExpectedForEventRequest2.Clone());

    RunGenerateBidWithJavascriptExpectingResult(
        CreateGenerateBidScript(
            R"({ad: "ad", bid:1, render:"https://response.test/" })",
            /*extra_code=*/R"(
            privateAggregation.sendHistogramReport(
                {bucket: 18446744073709551616n, value: 1});
            privateAggregation.reportContributionForEvent(
                "reserved.win", {bucket: 18446744073709551616n, value: 2});
          )"),
        /*expected_bid=*/
        mojom::BidderWorkletBid::New(
            "\"ad\"", 1, GURL("https://response.test/"),
            /*ad_components=*/absl::nullopt, base::TimeDelta()),
        /*expected_data_version=*/absl::nullopt,
        /*expected_errors=*/{},
        /*expected_debug_loss_report_url=*/absl::nullopt,
        /*expected_debug_win_report_url=*/absl::nullopt,
        /*expected_set_priority=*/absl::nullopt,
        /*expected_update_priority_signals_overrides=*/{},
        std::move(expected_pa_requests));
  }

  // Multiple requests
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(kExpectedRequest1.Clone());
    expected_pa_requests.push_back(kExpectedRequest2.Clone());
    expected_pa_requests.push_back(kExpectedForEventRequest1.Clone());
    expected_pa_requests.push_back(kExpectedForEventRequest2.Clone());

    RunGenerateBidWithJavascriptExpectingResult(
        CreateGenerateBidScript(
            R"({ad: "ad", bid:1, render:"https://response.test/" })",
            /*extra_code=*/R"(
            privateAggregation.sendHistogramReport({bucket: 123n, value: 45});
            privateAggregation.sendHistogramReport(
                {bucket: 18446744073709551616n, value: 1});
            privateAggregation.reportContributionForEvent(
                "reserved.win", {bucket: 234n, value: 56});
            privateAggregation.reportContributionForEvent(
                "reserved.win", {bucket: 18446744073709551616n, value: 2});
          )"),
        /*expected_bid=*/
        mojom::BidderWorkletBid::New(
            "\"ad\"", 1, GURL("https://response.test/"),
            /*ad_components=*/absl::nullopt, base::TimeDelta()),
        /*expected_data_version=*/absl::nullopt,
        /*expected_errors=*/{},
        /*expected_debug_loss_report_url=*/absl::nullopt,
        /*expected_debug_win_report_url=*/absl::nullopt,
        /*expected_set_priority=*/absl::nullopt,
        /*expected_update_priority_signals_overrides=*/{},
        std::move(expected_pa_requests));
  }

  // An unrelated exception after sendHistogramReport and
  // reportContributionForEvent shouldn't block the reports.
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(kExpectedRequest1.Clone());
    expected_pa_requests.push_back(kExpectedForEventRequest1.Clone());

    RunGenerateBidWithJavascriptExpectingResult(
        CreateGenerateBidScript(
            R"({ad: "ad", bid:1, render:"https://response.test/" })",
            /*extra_code=*/R"(
            privateAggregation.sendHistogramReport({bucket: 123n, value: 45});
            privateAggregation.reportContributionForEvent(
                "reserved.win", {bucket: 234n, value: 56});
            error;
          )"),
        /*expected_bid=*/mojom::BidderWorkletBidPtr(),
        /*expected_data_version=*/absl::nullopt,
        /*expected_errors=*/
        {"https://url.test/:9 Uncaught ReferenceError: error is not defined."},
        /*expected_debug_loss_report_url=*/absl::nullopt,
        /*expected_debug_win_report_url=*/absl::nullopt,
        /*expected_set_priority=*/absl::nullopt,
        /*expected_update_priority_signals_overrides=*/{},
        std::move(expected_pa_requests));
  }

  // Debug mode enabled with debug key
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(
        auction_worklet::mojom::PrivateAggregationRequest::New(
            kExpectedRequest1.contribution->Clone(),
            content::mojom::AggregationServiceMode::kDefault,
            content::mojom::DebugModeDetails::New(
                /*is_enabled=*/true, content::mojom::DebugKey::New(1234u))));
    expected_pa_requests.push_back(
        auction_worklet::mojom::PrivateAggregationRequest::New(
            kExpectedForEventRequest1.contribution->Clone(),
            content::mojom::AggregationServiceMode::kDefault,
            content::mojom::DebugModeDetails::New(
                /*is_enabled=*/true, content::mojom::DebugKey::New(1234u))));

    RunGenerateBidWithJavascriptExpectingResult(
        CreateGenerateBidScript(
            R"({ad: "ad", bid:1, render:"https://response.test/" })",
            /*extra_code=*/R"(
            privateAggregation.enableDebugMode({debug_key: 1234n});
            privateAggregation.sendHistogramReport({bucket: 123n, value: 45});
            privateAggregation.reportContributionForEvent(
                "reserved.win", {bucket: 234n, value: 56});
          )"),
        /*expected_bid=*/
        mojom::BidderWorkletBid::New(
            "\"ad\"", 1, GURL("https://response.test/"),
            /*ad_components=*/absl::nullopt, base::TimeDelta()),
        /*expected_data_version=*/absl::nullopt,
        /*expected_errors=*/{},
        /*expected_debug_loss_report_url=*/absl::nullopt,
        /*expected_debug_win_report_url=*/absl::nullopt,
        /*expected_set_priority=*/absl::nullopt,
        /*expected_update_priority_signals_overrides=*/{},
        std::move(expected_pa_requests));
  }

  // Debug mode enabled without debug key, but with multiple requests
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(
        auction_worklet::mojom::PrivateAggregationRequest::New(
            kExpectedRequest1.contribution->Clone(),
            content::mojom::AggregationServiceMode::kDefault,
            content::mojom::DebugModeDetails::New(
                /*is_enabled=*/true, /*debug_key=*/nullptr)));
    expected_pa_requests.push_back(
        auction_worklet::mojom::PrivateAggregationRequest::New(
            kExpectedRequest2.contribution->Clone(),
            content::mojom::AggregationServiceMode::kDefault,
            content::mojom::DebugModeDetails::New(
                /*is_enabled=*/true, /*debug_key=*/nullptr)));

    RunGenerateBidWithJavascriptExpectingResult(
        CreateGenerateBidScript(
            R"({ad: "ad", bid:1, render:"https://response.test/" })",
            /*extra_code=*/R"(
            privateAggregation.enableDebugMode();
            privateAggregation.sendHistogramReport({bucket: 123n, value: 45});
            privateAggregation.sendHistogramReport(
                {bucket: 18446744073709551616n, value: 1});
          )"),
        /*expected_bid=*/
        mojom::BidderWorkletBid::New(
            "\"ad\"", 1, GURL("https://response.test/"),
            /*ad_components=*/absl::nullopt, base::TimeDelta()),
        /*expected_data_version=*/absl::nullopt,
        /*expected_errors=*/{},
        /*expected_debug_loss_report_url=*/absl::nullopt,
        /*expected_debug_win_report_url=*/absl::nullopt,
        /*expected_set_priority=*/absl::nullopt,
        /*expected_update_priority_signals_overrides=*/{},
        std::move(expected_pa_requests));
  }

  // Debug mode enabled twice
  {
    RunGenerateBidWithJavascriptExpectingResult(
        CreateGenerateBidScript(
            R"({ad: "ad", bid:1, render:"https://response.test/" })",
            /*extra_code=*/R"(
            privateAggregation.enableDebugMode();
            privateAggregation.enableDebugMode();
          )"),
        /*expected_bid=*/
        mojom::BidderWorkletBidPtr(),
        /*expected_data_version=*/absl::nullopt,
        /*expected_errors=*/
        {"https://url.test/:7 Uncaught TypeError: enableDebugMode may be "
         "called at most once."},
        /*expected_debug_loss_report_url=*/absl::nullopt,
        /*expected_debug_win_report_url=*/absl::nullopt,
        /*expected_set_priority=*/absl::nullopt,
        /*expected_pa_requests=*/{});
  }
}

TEST_F(BidderWorkletPrivateAggregationEnabledTest, ReportWin) {
  auction_worklet::mojom::PrivateAggregationRequest kExpectedRequest1(
      mojom::AggregatableReportContribution::NewHistogramContribution(
          content::mojom::AggregatableReportHistogramContribution::New(
              /*bucket=*/123,
              /*value=*/45)),
      content::mojom::AggregationServiceMode::kDefault,
      content::mojom::DebugModeDetails::New());
  auction_worklet::mojom::PrivateAggregationRequest kExpectedRequest2(
      mojom::AggregatableReportContribution::NewHistogramContribution(
          content::mojom::AggregatableReportHistogramContribution::New(
              /*bucket=*/absl::MakeInt128(/*high=*/1, /*low=*/0),
              /*value=*/1)),
      content::mojom::AggregationServiceMode::kDefault,
      content::mojom::DebugModeDetails::New());
  mojom::PrivateAggregationRequest kExpectedForEventRequest1(
      mojom::AggregatableReportContribution::NewForEventContribution(
          mojom::AggregatableReportForEventContribution::New(
              /*bucket=*/mojom::ForEventSignalBucket::NewIdBucket(234),
              /*value=*/mojom::ForEventSignalValue::NewIntValue(56),
              /*event_type=*/"reserved.win")),
      content::mojom::AggregationServiceMode::kDefault,
      content::mojom::DebugModeDetails::New());
  mojom::PrivateAggregationRequest kExpectedForEventRequest2(
      mojom::AggregatableReportContribution::NewForEventContribution(
          mojom::AggregatableReportForEventContribution::New(
              /*bucket=*/mojom::ForEventSignalBucket::NewIdBucket(
                  absl::MakeInt128(/*high=*/1,
                                   /*low=*/0)),
              /*value=*/mojom::ForEventSignalValue::NewIntValue(2),
              /*event_type=*/"reserved.win")),
      content::mojom::AggregationServiceMode::kDefault,
      content::mojom::DebugModeDetails::New());

  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(kExpectedRequest1.Clone());

    RunReportWinWithFunctionBodyExpectingResult(
        R"(
          privateAggregation.sendHistogramReport({bucket: 123n, value: 45});
        )",
        /*expected_report_url =*/absl::nullopt,
        /*expected_ad_beacon_map=*/{}, std::move(expected_pa_requests),
        /*expected_errors=*/{});
  }

  // Set the private-aggregation permissions policy to disallowed.
  {
    permissions_policy_state_ =
        mojom::AuctionWorkletPermissionsPolicyState::New(
            /*private_aggregation_allowed=*/false,
            /*shared_storage_allowed=*/true);

    RunReportWinWithFunctionBodyExpectingResult(
        R"(
          privateAggregation.sendHistogramReport({bucket: 123n, value: 45});
        )",
        /*expected_report_url =*/absl::nullopt,
        /*expected_ad_beacon_map=*/{}, /*expected_pa_requests=*/{},
        /*expected_errors=*/
        {"https://url.test/:12 Uncaught TypeError: The \"private-aggregation\" "
         "Permissions Policy denied the method on privateAggregation."});

    permissions_policy_state_ =
        mojom::AuctionWorkletPermissionsPolicyState::New(
            /*private_aggregation_allowed=*/true,
            /*shared_storage_allowed=*/true);
  }

  // Large bucket
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(kExpectedRequest2.Clone());

    RunReportWinWithFunctionBodyExpectingResult(
        R"(
          privateAggregation.sendHistogramReport({bucket: 18446744073709551616n,
                                                  value: 1});
        )",
        /*expected_report_url =*/absl::nullopt,
        /*expected_ad_beacon_map=*/{}, std::move(expected_pa_requests),
        /*expected_errors=*/{});
  }

  // Multiple requests
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(kExpectedRequest1.Clone());
    expected_pa_requests.push_back(kExpectedRequest2.Clone());
    expected_pa_requests.push_back(kExpectedForEventRequest1.Clone());
    expected_pa_requests.push_back(kExpectedForEventRequest2.Clone());

    RunReportWinWithFunctionBodyExpectingResult(
        R"(
          privateAggregation.sendHistogramReport({bucket: 123n, value: 45});
          privateAggregation.sendHistogramReport({bucket: 18446744073709551616n,
                                                  value: 1});
          privateAggregation.reportContributionForEvent(
              "reserved.win", {bucket: 234n, value: 56});
          privateAggregation.reportContributionForEvent(
              "reserved.win", {bucket: 18446744073709551616n, value: 2});
        )",
        /*expected_report_url =*/absl::nullopt,
        /*expected_ad_beacon_map=*/{}, std::move(expected_pa_requests),
        /*expected_errors=*/{});
  }

  // An unrelated exception after sendHistogramReport shouldn't block the report
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(kExpectedRequest1.Clone());

    RunReportWinWithFunctionBodyExpectingResult(
        R"(
          privateAggregation.sendHistogramReport({bucket: 123n, value: 45});
          error;
        )",
        /*expected_report_url =*/absl::nullopt,
        /*expected_ad_beacon_map=*/{}, std::move(expected_pa_requests),
        /*expected_errors=*/
        {"https://url.test/:13 Uncaught ReferenceError: error is not "
         "defined."});
  }

  // Debug mode enabled with debug key
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(
        auction_worklet::mojom::PrivateAggregationRequest::New(
            kExpectedRequest1.contribution->Clone(),
            content::mojom::AggregationServiceMode::kDefault,
            content::mojom::DebugModeDetails::New(
                /*is_enabled=*/true, content::mojom::DebugKey::New(1234u))));
    expected_pa_requests.push_back(
        auction_worklet::mojom::PrivateAggregationRequest::New(
            kExpectedForEventRequest1.contribution->Clone(),
            content::mojom::AggregationServiceMode::kDefault,
            content::mojom::DebugModeDetails::New(
                /*is_enabled=*/true, content::mojom::DebugKey::New(1234u))));

    RunReportWinWithFunctionBodyExpectingResult(
        R"(
            privateAggregation.enableDebugMode({debug_key: 1234n});
            privateAggregation.sendHistogramReport({bucket: 123n, value: 45});
            privateAggregation.reportContributionForEvent(
                "reserved.win", {bucket: 234n, value: 56});
        )",
        /*expected_report_url=*/absl::nullopt,
        /*expected_ad_beacon_map=*/{}, std::move(expected_pa_requests),
        /*expected_errors=*/{});
  }

  // Debug mode enabled without debug key, but with multiple requests
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(
        auction_worklet::mojom::PrivateAggregationRequest::New(
            kExpectedRequest1.contribution->Clone(),
            content::mojom::AggregationServiceMode::kDefault,
            content::mojom::DebugModeDetails::New(
                /*is_enabled=*/true, /*debug_key=*/nullptr)));
    expected_pa_requests.push_back(
        auction_worklet::mojom::PrivateAggregationRequest::New(
            kExpectedRequest2.contribution->Clone(),
            content::mojom::AggregationServiceMode::kDefault,
            content::mojom::DebugModeDetails::New(
                /*is_enabled=*/true, /*debug_key=*/nullptr)));

    RunReportWinWithFunctionBodyExpectingResult(
        R"(
            privateAggregation.enableDebugMode();
            privateAggregation.sendHistogramReport({bucket: 123n, value: 45});
            privateAggregation.sendHistogramReport(
                {bucket: 18446744073709551616n, value: 1});
        )",
        /*expected_report_url=*/absl::nullopt,
        /*expected_ad_beacon_map=*/{}, std::move(expected_pa_requests),
        /*expected_errors=*/{});
  }

  // For-event report and histogram report are reported.
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(kExpectedRequest1.Clone());
    expected_pa_requests.push_back(kExpectedForEventRequest1.Clone());

    RunReportWinWithFunctionBodyExpectingResult(
        R"(
          privateAggregation.sendHistogramReport({bucket: 123n, value: 45});
          privateAggregation.reportContributionForEvent(
              "reserved.win", {bucket: 234n, value: 56});
        )",
        /*expected_report_url =*/absl::nullopt,
        /*expected_ad_beacon_map=*/{}, std::move(expected_pa_requests),
        /*expected_errors=*/{});
  }
}

class BidderWorkletPrivateAggregationDisabledTest : public BidderWorkletTest {
 public:
  BidderWorkletPrivateAggregationDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(content::kPrivateAggregationApi);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(BidderWorkletPrivateAggregationDisabledTest, GenerateBid) {
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          R"({ad: "ad", bid:1, render:"https://response.test/" })",
          /*extra_code=*/R"(
            privateAggregation.sendHistogramReport({bucket: 123n, value: 45});
          )"),
      /*expected_bid=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_errors=*/
      {"https://url.test/:6 Uncaught ReferenceError: privateAggregation is not "
       "defined."},
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_set_priority=*/absl::nullopt,
      /*expected_pa_requests=*/{});
}

TEST_F(BidderWorkletPrivateAggregationDisabledTest, ReportWin) {
  RunReportWinWithFunctionBodyExpectingResult(
      R"(
          privateAggregation.sendHistogramReport({bucket: 123n, value: 45});
        )",
      /*expected_report_url =*/absl::nullopt,
      /*expected_ad_beacon_map=*/{}, /*expected_pa_requests=*/{},
      /*expected_errors=*/
      {"https://url.test/:12 Uncaught ReferenceError: privateAggregation is "
       "not defined."});
}

TEST_F(BidderWorkletTest, KAnonSimulate) {
  const char kSideEffectScript[] = R"(
    setPriority(interestGroup.ads.length + 10);
  )";

  kanon_mode_ = auction_worklet::mojom::KAnonymityBidMode::kSimulate;

  // Nothing returned regardless of filtering.
  RunGenerateBidWithReturnValueExpectingResult(
      "",
      /*expected_bid=*/mojom::BidderWorkletBidPtr());
  EXPECT_FALSE(kanon_bid_);

  // Sole bid is unauthorized. The non-enforced bid is there, kanon-bid isn't.
  // Since this is simulation mode, set_priority and errors should come from the
  // unrestricted run.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          R"({ad: ["ad"], bid:1, render:"https://response.test/"})",
          kSideEffectScript),
      mojom::BidderWorkletBid::New(
          R"(["ad"])", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()),
      /*expected_data_version=*/absl::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_set_priority=*/11);
  ASSERT_FALSE(kanon_bid_);

  // Now authorize it.
  kanon_keys_.emplace(
      auction_worklet::mojom::KAnonKey::New(blink::KAnonKeyForAdBid(
          url::Origin::Create(interest_group_bidding_url_),
          interest_group_bidding_url_, GURL("https://response.test/"))),
      true);
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          R"({ad: ["ad"], bid:1, render:"https://response.test/"})",
          kSideEffectScript),
      mojom::BidderWorkletBid::New(
          R"(["ad"])", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()),
      /*expected_data_version=*/absl::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_set_priority=*/11);
  ASSERT_TRUE(kanon_bid_);
  EXPECT_TRUE(kanon_bid_->is_same_as_non_enforced());

  // Add a second ad, not authorized yet, with script that it will try it
  // if it's in the ad vector.
  interest_group_ads_.emplace_back(GURL("https://response2.test/"),
                                   /*metadata=*/absl::nullopt);
  // Non-enforced bid will be 2. Since this is simulated mode, other things are
  // from the same run, so expected_set_priority is 12.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          R"({ad: ["ad"], bid:interestGroup.ads.length,
          render:interestGroup.ads[interestGroup.ads.length - 1].renderUrl})",
          kSideEffectScript),
      mojom::BidderWorkletBid::New(
          R"(["ad"])", 2, GURL("https://response2.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()),
      /*expected_data_version=*/absl::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_set_priority=*/12);
  // k-anon-enforced bid will be 1.
  ASSERT_TRUE(kanon_bid_);
  ASSERT_FALSE(kanon_bid_->is_same_as_non_enforced());
  EXPECT_EQ(kanon_bid_->get_bid()->ad, R"(["ad"])");
  EXPECT_EQ(kanon_bid_->get_bid()->bid, 1);
  EXPECT_EQ(kanon_bid_->get_bid()->render_url, GURL("https://response.test/"));

  // Authorize it.
  kanon_keys_.emplace(
      auction_worklet::mojom::KAnonKey::New(blink::KAnonKeyForAdBid(
          url::Origin::Create(interest_group_bidding_url_),
          interest_group_bidding_url_, GURL("https://response2.test/"))),
      true);
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          R"({ad: ["ad"], bid:interestGroup.ads.length,
          render:interestGroup.ads[interestGroup.ads.length - 1].renderUrl})",
          kSideEffectScript),
      mojom::BidderWorkletBid::New(
          R"(["ad"])", 2, GURL("https://response2.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()),
      /*expected_data_version=*/absl::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_set_priority=*/12);
  ASSERT_TRUE(kanon_bid_);
  EXPECT_TRUE(kanon_bid_->is_same_as_non_enforced());
}

TEST_F(BidderWorkletTest, KAnonEnforce) {
  kanon_mode_ = auction_worklet::mojom::KAnonymityBidMode::kEnforce;

  const char kSideEffectScript[] = R"(
    setPriority(interestGroup.ads.length + 10);
  )";

  // Nothing returned regardless of filtering.
  RunGenerateBidWithReturnValueExpectingResult(
      "",
      /*expected_bid=*/mojom::BidderWorkletBidPtr());
  EXPECT_FALSE(kanon_bid_);

  // Sole bid is unauthorized. The non-enforced bid is there, kanon-bid isn't.
  // Since this is enforcement mode, set_priority and errors should come from
  // the restricted run.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          R"({ad: ["ad"], bid:1, render:"https://response.test/"})",
          kSideEffectScript),
      /*expected_bid=*/
      mojom::BidderWorkletBid::New(
          R"(["ad"])", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()),
      /*expected_data_version=*/absl::nullopt,
      /*expected_errors=*/
      {"https://url.test/ generateBid() bid render URL 'https://response.test/'"
       " isn't one of the registered creative URLs."},
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_set_priority=*/10);
  ASSERT_FALSE(kanon_bid_);

  // Now authorize it.
  kanon_keys_.emplace(
      auction_worklet::mojom::KAnonKey::New(blink::KAnonKeyForAdBid(
          url::Origin::Create(interest_group_bidding_url_),
          interest_group_bidding_url_, GURL("https://response.test/"))),
      true);
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          R"({ad: ["ad"], bid:1, render:"https://response.test/"})",
          kSideEffectScript),
      mojom::BidderWorkletBid::New(
          R"(["ad"])", 1, GURL("https://response.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()),
      /*expected_data_version=*/absl::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_set_priority=*/11);
  ASSERT_TRUE(kanon_bid_);
  EXPECT_TRUE(kanon_bid_->is_same_as_non_enforced());

  // Add a second ad, not authorized yet, with script that it will try it
  // if it's in the ad vector.
  interest_group_ads_.emplace_back(GURL("https://response2.test/"),
                                   /*metadata=*/absl::nullopt);
  // Non-enforced bid will be 2. Since this is enforced mode, other things are
  // from the restricted run, so expected_set_priority is 11.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          R"({ad: ["ad"], bid:interestGroup.ads.length,
          render:interestGroup.ads[interestGroup.ads.length - 1].renderUrl})",
          kSideEffectScript),
      mojom::BidderWorkletBid::New(
          R"(["ad"])", 2, GURL("https://response2.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()),
      /*expected_data_version=*/absl::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_set_priority=*/11);
  // k-anon-enforced bid will be 1.
  ASSERT_TRUE(kanon_bid_);
  ASSERT_FALSE(kanon_bid_->is_same_as_non_enforced());
  EXPECT_EQ(kanon_bid_->get_bid()->ad, R"(["ad"])");
  EXPECT_EQ(kanon_bid_->get_bid()->bid, 1);
  EXPECT_EQ(kanon_bid_->get_bid()->render_url, GURL("https://response.test/"));

  // Authorize it.
  kanon_keys_.emplace(
      auction_worklet::mojom::KAnonKey::New(blink::KAnonKeyForAdBid(
          url::Origin::Create(interest_group_bidding_url_),
          interest_group_bidding_url_, GURL("https://response2.test/"))),
      true);
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          R"({ad: ["ad"], bid:interestGroup.ads.length,
          render:interestGroup.ads[interestGroup.ads.length - 1].renderUrl})",
          kSideEffectScript),
      mojom::BidderWorkletBid::New(
          R"(["ad"])", 2, GURL("https://response2.test/"),
          /*ad_components=*/absl::nullopt, base::TimeDelta()),
      /*expected_data_version=*/absl::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_set_priority=*/12);
  ASSERT_TRUE(kanon_bid_);
  EXPECT_TRUE(kanon_bid_->is_same_as_non_enforced());
}

// Test for context re-use for k-anon rerun.
TEST_F(BidderWorkletTest, KAnonRerun) {
  const char kScript[] = R"(
    if (!('count' in globalThis))
      globalThis.count = 0;
    function generateBid(interestGroup) {
      ++count;
      return {ad: ["ad"], bid:count,
      render:interestGroup.ads[interestGroup.ads.length - 1].renderUrl};
    }
  )";

  kanon_mode_ = auction_worklet::mojom::KAnonymityBidMode::kEnforce;

  interest_group_ads_.emplace_back(GURL("https://response2.test/"),
                                   /*metadata=*/absl::nullopt);
  kanon_keys_.emplace(
      auction_worklet::mojom::KAnonKey::New(blink::KAnonKeyForAdBid(
          url::Origin::Create(interest_group_bidding_url_),
          interest_group_bidding_url_, GURL("https://response.test/"))),
      true);

  for (auto execution_mode :
       {blink::mojom::InterestGroup::ExecutionMode::kCompatibilityMode,
        blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode}) {
    execution_mode_ = execution_mode;
    SCOPED_TRACE(execution_mode_);
    RunGenerateBidWithJavascriptExpectingResult(
        kScript, mojom::BidderWorkletBid::New(
                     R"(["ad"])", 1, GURL("https://response2.test/"),
                     /*ad_components=*/absl::nullopt, base::TimeDelta()));
    ASSERT_TRUE(kanon_bid_);
    ASSERT_TRUE(kanon_bid_->is_bid());
    EXPECT_EQ(R"(["ad"])", kanon_bid_->get_bid()->ad);
    EXPECT_EQ(2, kanon_bid_->get_bid()->bid);
    EXPECT_EQ(GURL("https://response.test/"),
              kanon_bid_->get_bid()->render_url);
  }
}

TEST_F(BidderWorkletTest, IsKAnonURL) {
  const GURL kUrl1("https://example.com/1");
  const GURL kUrl2("https://example.org/2");
  const GURL kUrl3("https://example.gov/3");
  const GURL kUrl4("https://example.gov/4");
  mojom::BidderWorkletNonSharedParamsPtr params =
      mojom::BidderWorkletNonSharedParams::New();
  url::Origin owner = url::Origin::Create(interest_group_bidding_url_);
  const std::string kUrl1KAnonKey =
      blink::KAnonKeyForAdBid(owner, interest_group_bidding_url_, kUrl1);
  const std::string kUrl2KAnonKey =
      blink::KAnonKeyForAdBid(owner, interest_group_bidding_url_, kUrl2);
  const std::string kUrl3KAnonKey =
      blink::KAnonKeyForAdBid(owner, interest_group_bidding_url_, kUrl3);
  const std::string kUrl4KAnonKey =
      blink::KAnonKeyForAdBid(owner, interest_group_bidding_url_, kUrl4);

  params->kanon_keys.emplace(
      auction_worklet::mojom::KAnonKey::New(kUrl1KAnonKey), true);
  params->kanon_keys.emplace(
      auction_worklet::mojom::KAnonKey::New(kUrl2KAnonKey), true);
  params->kanon_keys.emplace(
      auction_worklet::mojom::KAnonKey::New(kUrl3KAnonKey), false);

  EXPECT_TRUE(BidderWorklet::IsKAnon(params.get(), kUrl1KAnonKey));
  EXPECT_TRUE(BidderWorklet::IsKAnon(params.get(), kUrl2KAnonKey));
  EXPECT_FALSE(BidderWorklet::IsKAnon(params.get(), kUrl3KAnonKey));
  EXPECT_FALSE(BidderWorklet::IsKAnon(params.get(), kUrl4KAnonKey));
}

TEST_F(BidderWorkletTest, IsKAnonResult) {
  const GURL kUrl1("https://example.com/1");
  const GURL kUrl2("https://example.org/2");
  const GURL kUrl3("https://example.gov/3");
  const GURL kUrl4("https://example.gov/4");
  mojom::BidderWorkletNonSharedParamsPtr params =
      mojom::BidderWorkletNonSharedParams::New();
  url::Origin owner = url::Origin::Create(interest_group_bidding_url_);
  params->kanon_keys.emplace(
      auction_worklet::mojom::KAnonKey::New(
          blink::KAnonKeyForAdBid(owner, interest_group_bidding_url_, kUrl1)),
      true);
  params->kanon_keys.emplace(auction_worklet::mojom::KAnonKey::New(
                                 blink::KAnonKeyForAdComponentBid(kUrl2)),
                             true);
  params->kanon_keys.emplace(auction_worklet::mojom::KAnonKey::New(
                                 blink::KAnonKeyForAdComponentBid(kUrl3)),
                             false);

  mojom::BidderWorkletBidPtr bid = mojom::BidderWorkletBid::New();

  // k-anon ad URL.
  bid->render_url = kUrl1;
  EXPECT_TRUE(
      BidderWorklet::IsKAnon(params.get(), interest_group_bidding_url_, bid));

  // k-anon ad URL and component.
  bid->ad_components.emplace();
  bid->ad_components->push_back(kUrl2);
  EXPECT_TRUE(
      BidderWorklet::IsKAnon(params.get(), interest_group_bidding_url_, bid));

  // Non k-anon ad URL, k-anon component.
  bid->render_url = kUrl4;
  EXPECT_FALSE(
      BidderWorklet::IsKAnon(params.get(), interest_group_bidding_url_, bid));

  // k-anon ad URL, one of the components non-k-anon.
  bid->render_url = kUrl1;
  bid->ad_components->push_back(kUrl3);
  EXPECT_FALSE(
      BidderWorklet::IsKAnon(params.get(), interest_group_bidding_url_, bid));
}

// Test of handling of FinalizeGenerateBid that comes in after the trusted
// signals.
TEST_F(BidderWorkletTest, AsyncFinalizeGenerateBid) {
  interest_group_trusted_bidding_signals_url_ =
      GURL("https://url.test/trustedsignals");
  interest_group_trusted_bidding_signals_keys_ = {"1"};

  const char kSerializeParams[] =
      R"({ad: [auctionSignals, trustedBiddingSignals,
               perBuyerSignals], bid:1,
          render:"https://response.test/"})";

  // Add script, but not trusted signals yet.
  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        CreateGenerateBidScript(kSerializeParams));

  mojo::Remote<mojom::BidderWorklet> bidder_worklet = CreateWorklet();
  mojo::AssociatedRemote<auction_worklet::mojom::GenerateBidFinalizer>
      bid_finalizer;
  BeginGenerateBid(bidder_worklet.get(),
                   bid_finalizer.BindNewEndpointAndPassReceiver());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(bid_);

  // Add trusted signals, too.
  AddBidderJsonResponse(&url_loader_factory_,
                        GURL("https://url.test/"
                             "trustedsignals?hostname=top.window.test&keys=1&"
                             "interestGroupNames=Fred"),
                        R"({"keys": {"1":123}})");
  // Not enough yet.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(bid_);

  // Now feed in the rest of the arguments.
  bid_finalizer->FinishGenerateBid(
      auction_signals_, per_buyer_signals_, per_buyer_timeout_,
      /*direct_from_seller_per_buyer_signals=*/absl::nullopt,
      /*direct_from_seller_auction_signals=*/absl::nullopt);
  load_script_run_loop_ = std::make_unique<base::RunLoop>();
  load_script_run_loop_->Run();
  ASSERT_TRUE(bid_);
  EXPECT_EQ(R"([["auction_signals"],{"1":123},["per_buyer_signals"]])",
            bid_->ad);
  EXPECT_EQ(1, bid_->bid);
  EXPECT_THAT(bid_errors_, testing::ElementsAre());
}

// Test of handling of FinalizeGenerateBid that comes in before the trusted
// signals.
TEST_F(BidderWorkletTest, AsyncFinalizeGenerateBid2) {
  interest_group_trusted_bidding_signals_url_ =
      GURL("https://url.test/trustedsignals");
  interest_group_trusted_bidding_signals_keys_ = {"1"};

  const char kSerializeParams[] =
      R"({ad: [auctionSignals, trustedBiddingSignals,
               perBuyerSignals], bid:1,
          render:"https://response.test/"})";

  // Add script, but not trusted signals yet.
  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        CreateGenerateBidScript(kSerializeParams));

  mojo::Remote<mojom::BidderWorklet> bidder_worklet = CreateWorklet();
  mojo::AssociatedRemote<auction_worklet::mojom::GenerateBidFinalizer>
      bid_finalizer;
  BeginGenerateBid(bidder_worklet.get(),
                   bid_finalizer.BindNewEndpointAndPassReceiver());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(bid_);

  // Feed in the rest of the arguments.
  bid_finalizer->FinishGenerateBid(
      auction_signals_, per_buyer_signals_, per_buyer_timeout_,
      /*direct_from_seller_per_buyer_signals=*/absl::nullopt,
      /*direct_from_seller_auction_signals=*/absl::nullopt);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(bid_);

  // Add trusted signals, too.
  AddBidderJsonResponse(&url_loader_factory_,
                        GURL("https://url.test/"
                             "trustedsignals?hostname=top.window.test&keys=1&"
                             "interestGroupNames=Fred"),
                        R"({"keys": {"1":123}})");
  load_script_run_loop_ = std::make_unique<base::RunLoop>();
  load_script_run_loop_->Run();
  ASSERT_TRUE(bid_);
  EXPECT_EQ(R"([["auction_signals"],{"1":123},["per_buyer_signals"]])",
            bid_->ad);
  EXPECT_EQ(1, bid_->bid);
  EXPECT_THAT(bid_errors_, testing::ElementsAre());
}

// The sequence when GenerateBidClient gets destroyed w/o getting to
// FinalizeGenerateBid() needs to do some extra cleaning up, so exercise it.
TEST_F(BidderWorkletTest, CloseGenerateBidClientBeforeFinalize) {
  mojo::Remote<mojom::BidderWorklet> bidder_worklet = CreateWorklet();
  mojo::AssociatedRemote<auction_worklet::mojom::GenerateBidFinalizer>
      bid_finalizer;
  mojo::PendingAssociatedRemote<mojom::GenerateBidClient> generate_bid_client;
  auto generate_bid_client_impl =
      std::make_unique<GenerateBidClientWithCallbacks>(
          GenerateBidClientWithCallbacks::GenerateBidNeverInvokedCallback());

  mojo::AssociatedReceiver<mojom::GenerateBidClient>
      generate_bid_client_receiver(
          generate_bid_client_impl.get(),
          generate_bid_client.InitWithNewEndpointAndPassReceiver());

  BeginGenerateBid(bidder_worklet.get(),
                   bid_finalizer.BindNewEndpointAndPassReceiver(),
                   std::move(generate_bid_client));
  task_environment_.RunUntilIdle();

  // Drop this end of generate_bid_client pipe w/o getting to
  // FinalizeGenerateBid.
  generate_bid_client_receiver.reset();
  task_environment_.RunUntilIdle();

  // The finalizer pipe must have been closed, too.
  EXPECT_FALSE(bid_finalizer.is_connected());
}

}  // namespace
}  // namespace auction_worklet
