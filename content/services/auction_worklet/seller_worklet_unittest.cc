// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/seller_worklet.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/mojom/auction_network_events_handler.mojom.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "content/services/auction_worklet/worklet_devtools_debug_test_util.h"
#include "content/services/auction_worklet/worklet_test_util.h"
#include "content/services/auction_worklet/worklet_v8_debug_test_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "net/http/http_status_code.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/ad_auction_currencies.h"
#include "third_party/blink/public/common/interest_group/ad_display_size.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"

using testing::HasSubstr;
using testing::StartsWith;

namespace auction_worklet {
namespace {

using PrivateAggregationRequests =
    std::vector<mojom::PrivateAggregationRequestPtr>;

// Very short time used by some tests that want to wait until just before a
// timer triggers.
constexpr base::TimeDelta kTinyTime = base::Microseconds(1);

// Common trusted scoring signals response.
const char kTrustedScoringSignalsResponse[] = R"(
  {
    "renderUrls": {"https://render.url.test/": 4},
    "adComponentRenderURLs": {
      "https://component1.test/": 1,
      "https://component2.test/": 2
    }
  }
)";

// Creates a seller script with scoreAd() returning the specified expression.
// Allows using scoreAd() arguments, arbitrary values, incorrect types, etc.
std::string CreateScoreAdScript(const std::string& raw_return_value,
                                const std::string& extra_code = "") {
  constexpr char kSellAdScript[] = R"(
    function scoreAd(adMetadata, bid, auctionConfig, trustedScoringSignals,
        browserSignals, directFromSellerSignals) {
      %s;
      return %s;
    }
  )";
  return base::StringPrintf(kSellAdScript, extra_code.c_str(),
                            raw_return_value.c_str());
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
    function reportResult(auctionConfig, browserSignals,
        directFromSellerSignals) {
      %s;
      return %s;
    }
  )";
  return CreateBasicSellAdScript() +
         base::StringPrintf(kBasicSellerScript, extra_code.c_str(),
                            raw_return_value.c_str());
}

// A ScoreAdClient that takes a callback to call in OnScoreAdComplete().
class TestScoreAdClient : public mojom::ScoreAdClient {
 public:
  using ScoreAdCompleteCallback = base::OnceCallback<void(
      double score,
      mojom::RejectReason reject_reason,
      mojom::ComponentAuctionModifiedBidParamsPtr
          component_auction_modified_bid_params,
      absl::optional<double> bid_in_seller_currency,
      absl::optional<uint32_t> scoring_signals_data_version,
      const absl::optional<GURL>& debug_loss_report_url,
      const absl::optional<GURL>& debug_win_report_url,
      PrivateAggregationRequests pa_requests,
      base::TimeDelta scoring_latency,
      mojom::ScoreAdDependencyLatenciesPtr score_ad_dependency_latencies,
      const std::vector<std::string>& errors)>;

  explicit TestScoreAdClient(ScoreAdCompleteCallback score_ad_complete_callback)
      : score_ad_complete_callback_(std::move(score_ad_complete_callback)) {}

  ~TestScoreAdClient() override = default;

  // Helper that creates a TestScoreAdClient owned by a SelfOwnedReceiver.
  static mojo::PendingRemote<mojom::ScoreAdClient> Create(
      ScoreAdCompleteCallback score_ad_complete_callback) {
    mojo::PendingRemote<mojom::ScoreAdClient> client_remote;
    mojo::MakeSelfOwnedReceiver(std::make_unique<TestScoreAdClient>(
                                    std::move(score_ad_complete_callback)),
                                client_remote.InitWithNewPipeAndPassReceiver());
    return client_remote;
  }

  // mojom::ScoreAdClient implementation:
  void OnScoreAdComplete(
      double score,
      mojom::RejectReason reject_reason,
      mojom::ComponentAuctionModifiedBidParamsPtr
          component_auction_modified_bid_params,
      absl::optional<double> bid_in_seller_currency,
      absl::optional<uint32_t> scoring_signals_data_version,
      const absl::optional<GURL>& debug_loss_report_url,
      const absl::optional<GURL>& debug_win_report_url,
      PrivateAggregationRequests pa_requests,
      base::TimeDelta scoring_latency,
      mojom::ScoreAdDependencyLatenciesPtr score_ad_dependency_latencies,
      const std::vector<std::string>& errors) override {
    std::move(score_ad_complete_callback_)
        .Run(score, reject_reason,
             std::move(component_auction_modified_bid_params),
             std::move(bid_in_seller_currency),
             std::move(scoring_signals_data_version), debug_loss_report_url,
             debug_win_report_url, std::move(pa_requests), scoring_latency,
             std::move(score_ad_dependency_latencies), errors);
  }

  static ScoreAdCompleteCallback ScoreAdNeverInvokedCallback() {
    return base::BindOnce(
        [](double score, mojom::RejectReason reject_reason,
           mojom::ComponentAuctionModifiedBidParamsPtr
               component_auction_modified_bid_params,
           absl::optional<double> bid_in_seller_currency,
           absl::optional<uint32_t> scoring_signals_data_version,
           const absl::optional<GURL>& debug_loss_report_url,
           const absl::optional<GURL>& debug_win_report_url,
           PrivateAggregationRequests pa_requests,
           base::TimeDelta scoring_latency,
           mojom::ScoreAdDependencyLatenciesPtr score_ad_dependency_latencies,
           const std::vector<std::string>& errors) {
          ADD_FAILURE() << "Callback should not be invoked";
        });
  }

 private:
  ScoreAdCompleteCallback score_ad_complete_callback_;
};

class SellerWorkletTest : public testing::Test {
 public:
  explicit SellerWorkletTest(
      base::test::TaskEnvironment::TimeSource time_mode =
          base::test::TaskEnvironment::TimeSource::MOCK_TIME)
      : task_environment_(time_mode) {
    SetDefaultParameters();
  }

  ~SellerWorkletTest() override = default;

  void SetUp() override {
    // v8_helper_ needs to be created here instead of the constructor, because
    // this test fixture has a subclass that initializes a ScopedFeatureList in
    // in their constructor, which needs to be done BEFORE other threads are
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

    // In all tests where the SellerWorklet receiver is closed before the
    // remote, the disconnect reason should be consumed and validated.
    EXPECT_FALSE(disconnect_reason_);
  }

  // Sets default values for scoreAd() and report_result() arguments. No test
  // actually depends on these being anything but valid, but this does allow
  // tests to reset them to a consistent state.
  void SetDefaultParameters() {
    ad_metadata_ = "[1]";
    bid_ = 1;
    bid_currency_ = absl::nullopt;
    decision_logic_url_ = GURL("https://url.test/");
    trusted_scoring_signals_url_.reset();
    auction_ad_config_non_shared_params_ =
        blink::AuctionConfig::NonSharedParams();

    top_window_origin_ = url::Origin::Create(GURL("https://window.test/"));
    permissions_policy_state_ =
        mojom::AuctionWorkletPermissionsPolicyState::New(
            /*private_aggregation_allowed=*/true,
            /*shared_storage_allowed=*/false);
    experiment_group_id_ = absl::nullopt;
    browser_signals_other_seller_.reset();
    component_expect_bid_currency_ = absl::nullopt;
    browser_signal_interest_group_owner_ =
        url::Origin::Create(GURL("https://interest.group.owner.test/"));
    browser_signal_buyer_and_seller_reporting_id_ = absl::nullopt;
    browser_signal_render_url_ = GURL("https://render.url.test/");
    browser_signal_ad_components_.clear();
    browser_signal_bidding_duration_msecs_ = 0;
    browser_signal_desireability_ = 1;
    seller_timeout_ = absl::nullopt;
    browser_signal_highest_scoring_other_bid_ = 0;
    browser_signal_highest_scoring_other_bid_currency_ = absl::nullopt;
  }

  // Configures `url_loader_factory_` to return a script with the specified
  // return line, expecting the provided result.
  void RunScoreAdWithReturnValueExpectingResult(
      const std::string& raw_return_value,
      double expected_score,
      const std::vector<std::string>& expected_errors =
          std::vector<std::string>(),
      mojom::ComponentAuctionModifiedBidParamsPtr
          expected_component_auction_modified_bid_params =
              mojom::ComponentAuctionModifiedBidParamsPtr(),
      absl::optional<uint32_t> expected_data_version = {},
      const absl::optional<GURL>& expected_debug_loss_report_url =
          absl::nullopt,
      const absl::optional<GURL>& expected_debug_win_report_url = absl::nullopt,
      mojom::RejectReason expected_reject_reason =
          mojom::RejectReason::kNotAvailable,
      PrivateAggregationRequests expected_pa_requests = {},
      absl::optional<double> expected_bid_in_seller_currency = absl::nullopt) {
    RunScoreAdWithJavascriptExpectingResult(
        CreateScoreAdScript(raw_return_value), expected_score, expected_errors,
        std::move(expected_component_auction_modified_bid_params),
        expected_data_version, expected_debug_loss_report_url,
        expected_debug_win_report_url, expected_reject_reason,
        std::move(expected_pa_requests), expected_bid_in_seller_currency);
  }

  // Behaves just like RunScoreAdWithReturnValueExpectingResult(), but
  // additionally expects the auction to take exactly `expected_duration`, using
  // FastForwardBy() to advance time. Can't just use RunLoop and Time::Now()
  // time, because that can get confused by superfluous events and wait 30
  // seconds too long (perhaps confused by the 30 second download timer, even
  // though the download should complete immediately, and the URLLoader with the
  // timer on it deleted?)
  void RunScoreAdWithReturnValueExpectingResultInExactTime(
      const std::string& raw_return_value,
      double expected_score,
      mojom::ComponentAuctionModifiedBidParamsPtr
          expected_component_auction_modified_bid_params,
      base::TimeDelta expected_duration,
      const std::vector<std::string>& expected_errors = {},
      absl::optional<uint32_t> expected_data_version = {},
      const absl::optional<GURL>& expected_debug_loss_report_url =
          absl::nullopt,
      const absl::optional<GURL>& expected_debug_win_report_url = absl::nullopt,
      mojom::RejectReason expected_reject_reason =
          mojom::RejectReason::kNotAvailable,
      PrivateAggregationRequests expected_pa_requests = {},
      absl::optional<double> expected_bid_in_seller_currency = absl::nullopt) {
    AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                          CreateScoreAdScript(raw_return_value));
    auto seller_worklet = CreateWorklet();

    base::RunLoop run_loop;
    RunScoreAdOnWorkletAsync(
        seller_worklet.get(), expected_score, expected_errors,
        std::move(expected_component_auction_modified_bid_params),
        expected_data_version, expected_debug_loss_report_url,
        expected_debug_win_report_url, expected_reject_reason,
        std::move(expected_pa_requests), expected_bid_in_seller_currency,
        /*expected_score_ad_timeout=*/false,
        /*expected_signals_fetch_latency=*/absl::nullopt,
        /*expected_code_ready_latency=*/absl::nullopt, run_loop.QuitClosure());
    task_environment_.FastForwardBy(expected_duration - kTinyTime);
    EXPECT_FALSE(run_loop.AnyQuitCalled());
    task_environment_.FastForwardBy(kTinyTime);
    EXPECT_TRUE(run_loop.AnyQuitCalled());
  }

  // Configures `url_loader_factory_` to return the provided script, and then
  // runs its score_ad() function, expecting the provided result.
  void RunScoreAdWithJavascriptExpectingResult(
      const std::string& javascript,
      double expected_score,
      const std::vector<std::string>& expected_errors =
          std::vector<std::string>(),
      mojom::ComponentAuctionModifiedBidParamsPtr
          expected_component_auction_modified_bid_params =
              mojom::ComponentAuctionModifiedBidParamsPtr(),
      absl::optional<uint32_t> expected_data_version = {},
      const absl::optional<GURL>& expected_debug_loss_report_url =
          absl::nullopt,
      const absl::optional<GURL>& expected_debug_win_report_url = absl::nullopt,
      mojom::RejectReason expected_reject_reason =
          mojom::RejectReason::kNotAvailable,
      PrivateAggregationRequests expected_pa_requests = {},
      absl::optional<double> expected_bid_in_seller_currency = absl::nullopt) {
    SCOPED_TRACE(javascript);
    AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                          javascript);
    RunScoreAdExpectingResult(
        expected_score, expected_errors,
        std::move(expected_component_auction_modified_bid_params),
        expected_data_version, expected_debug_loss_report_url,
        expected_debug_win_report_url, expected_reject_reason,
        std::move(expected_pa_requests), expected_bid_in_seller_currency);
  }

  // Runs score_ad() script, checking result and invoking provided closure
  // when done. Something else must spin the event loop.
  void RunScoreAdOnWorkletAsync(
      mojom::SellerWorklet* seller_worklet,
      double expected_score,
      const std::vector<std::string>& expected_errors,
      mojom::ComponentAuctionModifiedBidParamsPtr
          expected_component_auction_modified_bid_params,
      absl::optional<uint32_t> expected_data_version,
      const absl::optional<GURL>& expected_debug_loss_report_url,
      const absl::optional<GURL>& expected_debug_win_report_url,
      mojom::RejectReason expected_reject_reason,
      PrivateAggregationRequests expected_pa_requests,
      absl::optional<double> expected_bid_in_seller_currency,
      bool expected_score_ad_timeout,
      absl::optional<base::TimeDelta> expected_signals_fetch_latency,
      absl::optional<base::TimeDelta> expected_code_ready_latency,
      base::OnceClosure done_closure) {
    seller_worklet->ScoreAd(
        ad_metadata_, bid_, bid_currency_, auction_ad_config_non_shared_params_,
        direct_from_seller_seller_signals_,
        direct_from_seller_seller_signals_header_ad_slot_,
        direct_from_seller_auction_signals_,
        direct_from_seller_auction_signals_header_ad_slot_,
        browser_signals_other_seller_.Clone(), component_expect_bid_currency_,
        browser_signal_interest_group_owner_, browser_signal_render_url_,
        browser_signal_ad_components_, browser_signal_bidding_duration_msecs_,
        seller_timeout_,
        /*trace_id=*/1,
        TestScoreAdClient::Create(base::BindOnce(
            [](double expected_score,
               mojom::RejectReason expected_reject_reason,
               mojom::ComponentAuctionModifiedBidParamsPtr
                   expected_component_auction_modified_bid_params,
               absl::optional<uint32_t> expected_data_version,
               const absl::optional<GURL>& expected_debug_loss_report_url,
               const absl::optional<GURL>& expected_debug_win_report_url,
               PrivateAggregationRequests expected_pa_requests,
               absl::optional<double> expected_bid_in_seller_currency,
               absl::optional<base::TimeDelta> expected_score_ad_timeout,
               absl::optional<base::TimeDelta> expected_signals_fetch_latency,
               absl::optional<base::TimeDelta> expected_code_ready_latency,
               std::vector<std::string> expected_errors,
               base::OnceClosure done_closure, double score,
               mojom::RejectReason reject_reason,
               mojom::ComponentAuctionModifiedBidParamsPtr
                   component_auction_modified_bid_params,
               absl::optional<double> bid_in_seller_currency,
               absl::optional<uint32_t> scoring_signals_data_version,
               const absl::optional<GURL>& debug_loss_report_url,
               const absl::optional<GURL>& debug_win_report_url,
               PrivateAggregationRequests pa_requests,
               base::TimeDelta scoring_latency,
               mojom::ScoreAdDependencyLatenciesPtr
                   score_ad_dependency_latencies,
               const std::vector<std::string>& errors) {
              EXPECT_EQ(expected_score, score);
              EXPECT_EQ(static_cast<int>(expected_reject_reason),
                        static_cast<int>(reject_reason));
              EXPECT_EQ(
                  expected_component_auction_modified_bid_params.is_null(),
                  component_auction_modified_bid_params.is_null());
              if (!expected_component_auction_modified_bid_params.is_null() &&
                  !component_auction_modified_bid_params.is_null()) {
                EXPECT_EQ(expected_component_auction_modified_bid_params->ad,
                          component_auction_modified_bid_params->ad);
                EXPECT_EQ(
                    expected_component_auction_modified_bid_params->has_bid,
                    component_auction_modified_bid_params->has_bid);
                if (expected_component_auction_modified_bid_params->has_bid) {
                  EXPECT_EQ(expected_component_auction_modified_bid_params->bid,
                            component_auction_modified_bid_params->bid);
                }
              }
              EXPECT_EQ(expected_debug_loss_report_url, debug_loss_report_url);
              EXPECT_EQ(expected_debug_win_report_url, debug_win_report_url);
              EXPECT_EQ(expected_data_version, scoring_signals_data_version);
              EXPECT_EQ(expected_bid_in_seller_currency,
                        bid_in_seller_currency);
              EXPECT_EQ(expected_pa_requests, pa_requests);
              if (expected_score_ad_timeout) {
                // We only know that about the time of the timeout should have
                // elapsed, and there may also be some thread skew.
                EXPECT_GE(scoring_latency,
                          expected_score_ad_timeout.value() * 0.9);
              }
              if (expected_signals_fetch_latency) {
                EXPECT_EQ(score_ad_dependency_latencies
                              ->trusted_scoring_signals_latency,
                          expected_signals_fetch_latency);
              }
              if (expected_code_ready_latency) {
                EXPECT_EQ(score_ad_dependency_latencies->code_ready_latency,
                          expected_code_ready_latency);
              }
              EXPECT_EQ(expected_errors, errors);
              std::move(done_closure).Run();
            },
            expected_score, expected_reject_reason,
            std::move(expected_component_auction_modified_bid_params),
            expected_data_version, expected_debug_loss_report_url,
            expected_debug_win_report_url, std::move(expected_pa_requests),
            expected_bid_in_seller_currency,
            expected_score_ad_timeout
                ? absl::make_optional(
                      seller_timeout_.value_or(AuctionV8Helper::kScriptTimeout))
                : absl::nullopt,
            expected_signals_fetch_latency, expected_code_ready_latency,
            expected_errors, std::move(done_closure))));
  }

  void RunScoreAdOnWorkletExpectingCallbackNeverInvoked(
      mojom::SellerWorklet* seller_worklet) {
    seller_worklet->ScoreAd(
        ad_metadata_, bid_, bid_currency_, auction_ad_config_non_shared_params_,
        direct_from_seller_seller_signals_,
        direct_from_seller_seller_signals_header_ad_slot_,
        direct_from_seller_auction_signals_,
        direct_from_seller_auction_signals_header_ad_slot_,
        browser_signals_other_seller_.Clone(), component_expect_bid_currency_,
        browser_signal_interest_group_owner_, browser_signal_render_url_,
        browser_signal_ad_components_, browser_signal_bidding_duration_msecs_,
        seller_timeout_,
        /*trace_id=*/1,
        TestScoreAdClient::Create(
            TestScoreAdClient::ScoreAdNeverInvokedCallback()));
  }

  // Loads and runs a scode_ad() script, expecting the supplied result.
  void RunScoreAdExpectingResultOnWorklet(
      mojom::SellerWorklet* seller_worklet,
      double expected_score,
      const std::vector<std::string>& expected_errors =
          std::vector<std::string>(),
      mojom::ComponentAuctionModifiedBidParamsPtr
          expected_component_auction_modified_bid_params =
              mojom::ComponentAuctionModifiedBidParamsPtr(),
      absl::optional<uint32_t> expected_data_version = absl::nullopt,
      const absl::optional<GURL>& expected_debug_loss_report_url =
          absl::nullopt,
      const absl::optional<GURL>& expected_debug_win_report_url = absl::nullopt,
      mojom::RejectReason expected_reject_reason =
          mojom::RejectReason::kNotAvailable,
      PrivateAggregationRequests expected_pa_requests = {},
      absl::optional<double> expected_bid_in_seller_currency = absl::nullopt) {
    base::RunLoop run_loop;
    RunScoreAdOnWorkletAsync(
        seller_worklet, expected_score, expected_errors,
        std::move(expected_component_auction_modified_bid_params),
        expected_data_version, expected_debug_loss_report_url,
        expected_debug_win_report_url, expected_reject_reason,
        std::move(expected_pa_requests), expected_bid_in_seller_currency,
        /*expected_score_ad_timeout=*/false,
        /*expected_signals_fetch_latency=*/absl::nullopt,
        /*expected_code_ready_latency=*/absl::nullopt, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Loads and runs a scode_ad() script, expecting the supplied result.
  void RunScoreAdExpectingResult(
      double expected_score,
      const std::vector<std::string>& expected_errors =
          std::vector<std::string>(),
      mojom::ComponentAuctionModifiedBidParamsPtr
          expected_component_auction_modified_bid_params =
              mojom::ComponentAuctionModifiedBidParamsPtr(),
      absl::optional<uint32_t> expected_data_version = absl::nullopt,
      const absl::optional<GURL>& expected_debug_loss_report_url =
          absl::nullopt,
      const absl::optional<GURL>& expected_debug_win_report_url = absl::nullopt,
      mojom::RejectReason expected_reject_reason =
          mojom::RejectReason::kNotAvailable,
      PrivateAggregationRequests expected_pa_requests = {},
      absl::optional<double> expected_bid_in_seller_currency = absl::nullopt) {
    auto seller_worklet = CreateWorklet();
    ASSERT_TRUE(seller_worklet);
    RunScoreAdExpectingResultOnWorklet(
        seller_worklet.get(), expected_score, expected_errors,
        std::move(expected_component_auction_modified_bid_params),
        expected_data_version, expected_debug_loss_report_url,
        expected_debug_win_report_url, expected_reject_reason,
        std::move(expected_pa_requests), expected_bid_in_seller_currency);
  }

  // Configures `url_loader_factory_` to return a report_result() script created
  // with CreateReportToScript(). Then runs the script, expecting the provided
  // result.
  void RunReportResultCreatedScriptExpectingResult(
      const std::string& raw_return_value,
      const std::string& extra_code,
      const absl::optional<std::string>& expected_signals_for_winner,
      const absl::optional<GURL>& expected_report_url,
      const base::flat_map<std::string, GURL>& expected_ad_beacon_map =
          base::flat_map<std::string, GURL>(),
      PrivateAggregationRequests expected_pa_requests = {},
      const std::vector<std::string>& expected_errors =
          std::vector<std::string>()) {
    RunReportResultWithJavascriptExpectingResult(
        CreateReportToScript(raw_return_value, extra_code),
        expected_signals_for_winner, expected_report_url,
        expected_ad_beacon_map, std::move(expected_pa_requests),
        expected_errors);
  }

  // Configures `url_loader_factory_` to return the provided script, and then
  // runs its report_result() function. Then runs the script, expecting the
  // provided result.
  void RunReportResultWithJavascriptExpectingResult(
      const std::string& javascript,
      const absl::optional<std::string>& expected_signals_for_winner,
      const absl::optional<GURL>& expected_report_url,
      const base::flat_map<std::string, GURL>& expected_ad_beacon_map =
          base::flat_map<std::string, GURL>(),
      PrivateAggregationRequests expected_pa_requests = {},
      const std::vector<std::string>& expected_errors =
          std::vector<std::string>()) {
    SCOPED_TRACE(javascript);
    AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                          javascript);
    RunReportResultExpectingResult(expected_signals_for_winner,
                                   expected_report_url, expected_ad_beacon_map,
                                   std::move(expected_pa_requests),
                                   expected_errors);
  }

  // Loads and runs a report_result() script, expecting the supplied result.
  // Caller is responsible for spinning the event loop at least until
  // `done_closure`.
  void RunReportResultExpectingResultAsync(
      mojom::SellerWorklet* seller_worklet,
      const absl::optional<std::string>& expected_signals_for_winner,
      const absl::optional<GURL>& expected_report_url,
      const base::flat_map<std::string, GURL>& expected_ad_beacon_map,
      PrivateAggregationRequests expected_pa_requests,
      bool expected_reporting_latency_timeout,
      const std::vector<std::string>& expected_errors,
      base::OnceClosure done_closure) {
    seller_worklet->ReportResult(
        auction_ad_config_non_shared_params_,
        direct_from_seller_seller_signals_,
        direct_from_seller_seller_signals_header_ad_slot_,
        direct_from_seller_auction_signals_,
        direct_from_seller_auction_signals_header_ad_slot_,
        browser_signals_other_seller_.Clone(),
        browser_signal_interest_group_owner_,
        browser_signal_buyer_and_seller_reporting_id_,
        browser_signal_render_url_, bid_, bid_currency_,
        browser_signal_desireability_,
        browser_signal_highest_scoring_other_bid_,
        browser_signal_highest_scoring_other_bid_currency_,
        browser_signals_component_auction_report_result_params_.Clone(),
        browser_signal_data_version_.value_or(0),
        browser_signal_data_version_.has_value(),
        /*trace_id=*/1,
        base::BindOnce(
            [](const absl::optional<std::string>& expected_signals_for_winner,
               const absl::optional<GURL>& expected_report_url,
               const base::flat_map<std::string, GURL>& expected_ad_beacon_map,
               PrivateAggregationRequests expected_pa_requests,
               bool expected_reporting_latency_timeout,
               const std::vector<std::string>& expected_errors,
               base::OnceClosure done_closure,
               const absl::optional<std::string>& signals_for_winner,
               const absl::optional<GURL>& report_url,
               const base::flat_map<std::string, GURL>& ad_beacon_map,
               PrivateAggregationRequests pa_requests,
               base::TimeDelta reporting_latency,
               const std::vector<std::string>& errors) {
              if (signals_for_winner && expected_signals_for_winner) {
                // If neither is null, used fancy base::Value comparison, which
                // removes dependencies on JSON serialization order and format,
                // and has better error output.
                EXPECT_THAT(base::test::ParseJson(*signals_for_winner),
                            base::test::IsJson(*expected_signals_for_winner));
              } else {
                // Otherwise, just compare the optional strings directly.
                EXPECT_EQ(expected_signals_for_winner, signals_for_winner);
              }
              EXPECT_EQ(expected_report_url, report_url);
              EXPECT_EQ(expected_ad_beacon_map, ad_beacon_map);
              EXPECT_EQ(expected_pa_requests, pa_requests);
              if (expected_reporting_latency_timeout) {
                // We only know that about the time of the timeout should have
                // elapsed, and there may also be some thread skew.
                EXPECT_GE(reporting_latency,
                          AuctionV8Helper::kScriptTimeout * 0.9);
              }
              EXPECT_EQ(expected_errors, errors);
              std::move(done_closure).Run();
            },
            expected_signals_for_winner, expected_report_url,
            expected_ad_beacon_map, std::move(expected_pa_requests),
            expected_reporting_latency_timeout, expected_errors,
            std::move(done_closure)));
  }

  void RunReportResultExpectingCallbackNeverInvoked(
      mojom::SellerWorklet* seller_worklet) {
    seller_worklet->ReportResult(
        auction_ad_config_non_shared_params_,
        direct_from_seller_seller_signals_,
        direct_from_seller_seller_signals_header_ad_slot_,
        direct_from_seller_auction_signals_,
        direct_from_seller_auction_signals_header_ad_slot_,
        browser_signals_other_seller_.Clone(),
        browser_signal_interest_group_owner_,
        browser_signal_buyer_and_seller_reporting_id_,
        browser_signal_render_url_, bid_, bid_currency_,
        browser_signal_desireability_,
        browser_signal_highest_scoring_other_bid_,
        browser_signal_highest_scoring_other_bid_currency_,
        browser_signals_component_auction_report_result_params_.Clone(),
        browser_signal_data_version_.value_or(0),
        browser_signal_data_version_.has_value(),
        /*trace_id=*/1,
        base::BindOnce(
            [](const absl::optional<std::string>& signals_for_winner,
               const absl::optional<GURL>& report_url,
               const base::flat_map<std::string, GURL>& ad_beacon_map,
               PrivateAggregationRequests pa_requests,
               base::TimeDelta reporting_latency,
               const std::vector<std::string>& errors) {
              ADD_FAILURE() << "This should not be invoked";
            }));
  }

  // Loads and runs a report_result() script, expecting the supplied result.
  void RunReportResultExpectingResult(
      const absl::optional<std::string>& expected_signals_for_winner,
      const absl::optional<GURL>& expected_report_url,
      const base::flat_map<std::string, GURL>& expected_ad_beacon_map =
          base::flat_map<std::string, GURL>(),
      PrivateAggregationRequests expected_pa_requests = {},
      const std::vector<std::string>& expected_errors =
          std::vector<std::string>()) {
    auto seller_worklet = CreateWorklet();
    ASSERT_TRUE(seller_worklet);

    base::RunLoop run_loop;
    RunReportResultExpectingResultAsync(
        seller_worklet.get(), expected_signals_for_winner, expected_report_url,
        expected_ad_beacon_map, std::move(expected_pa_requests),
        /*expected_reporting_latency_timeout=*/false, expected_errors,
        run_loop.QuitClosure());
    run_loop.Run();
  }

  // Create a seller worklet. If out_seller_worklet_impl is non-null, will also
  // the stash an actual implementation pointer there.
  mojo::Remote<mojom::SellerWorklet> CreateWorklet(
      bool pause_for_debugger_on_start = false,
      SellerWorklet** out_seller_worklet_impl = nullptr,
      bool use_alternate_url_loader_factory = false) {
    mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory;
    if (use_alternate_url_loader_factory) {
      alternate_url_loader_factory_.Clone(
          url_loader_factory.InitWithNewPipeAndPassReceiver());
    } else {
      url_loader_factory_.Clone(
          url_loader_factory.InitWithNewPipeAndPassReceiver());
    }

    mojo::Remote<mojom::SellerWorklet> seller_worklet;
    auto seller_worklet_impl = std::make_unique<SellerWorklet>(
        v8_helper_, std::move(shared_storage_host_remote_),
        pause_for_debugger_on_start, std::move(url_loader_factory),
        auction_network_events_handler_.CreateRemote(), decision_logic_url_,
        trusted_scoring_signals_url_, top_window_origin_,
        permissions_policy_state_.Clone(), experiment_group_id_);
    auto* seller_worklet_ptr = seller_worklet_impl.get();
    mojo::ReceiverId receiver_id =
        seller_worklets_.Add(std::move(seller_worklet_impl),
                             seller_worklet.BindNewPipeAndPassReceiver());
    seller_worklet_ptr->set_close_pipe_callback(
        base::BindOnce(&SellerWorkletTest::ClosePipeCallback,
                       base::Unretained(this), receiver_id));
    seller_worklet.set_disconnect_with_reason_handler(base::BindRepeating(
        &SellerWorkletTest::OnDisconnectWithReason, base::Unretained(this)));

    if (out_seller_worklet_impl) {
      *out_seller_worklet_impl = seller_worklet_ptr;
    }
    return seller_worklet;
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
    seller_worklets_.RemoveWithReason(receiver_id, /*custom_reason_code=*/0,
                                      description);
  }

  void OnDisconnectWithReason(uint32_t custom_reason,
                              const std::string& description) {
    DCHECK(!disconnect_reason_);

    disconnect_reason_ = description;
    if (disconnect_run_loop_) {
      disconnect_run_loop_->Quit();
    }
  }

  base::test::TaskEnvironment task_environment_;

  // Arguments passed to score_bid() and report_result(). Arguments common to
  // both of them use the same field.
  //
  // NOTE: For each new GURL field, ScoreAdLoadCompletionOrder /
  // ReportResultLoadCompletionOrder should be updated.
  std::string ad_metadata_;
  // `bid_` is a browser signal for report_result(), but a direct parameter for
  // score_bid().
  double bid_;
  absl::optional<blink::AdCurrency> bid_currency_;
  GURL decision_logic_url_;
  absl::optional<GURL> trusted_scoring_signals_url_;
  blink::AuctionConfig::NonSharedParams auction_ad_config_non_shared_params_;
  absl::optional<GURL> direct_from_seller_seller_signals_;
  absl::optional<std::string> direct_from_seller_seller_signals_header_ad_slot_;
  absl::optional<GURL> direct_from_seller_auction_signals_;
  absl::optional<std::string>
      direct_from_seller_auction_signals_header_ad_slot_;
  url::Origin top_window_origin_;
  mojom::AuctionWorkletPermissionsPolicyStatePtr permissions_policy_state_;
  absl::optional<uint16_t> experiment_group_id_;
  mojom::ComponentAuctionOtherSellerPtr browser_signals_other_seller_;
  absl::optional<blink::AdCurrency> component_expect_bid_currency_;
  url::Origin browser_signal_interest_group_owner_;
  absl::optional<std::string> browser_signal_buyer_and_seller_reporting_id_;
  GURL browser_signal_render_url_;
  std::vector<GURL> browser_signal_ad_components_;
  uint32_t browser_signal_bidding_duration_msecs_;
  double browser_signal_desireability_;
  double browser_signal_highest_scoring_other_bid_;
  absl::optional<blink::AdCurrency>
      browser_signal_highest_scoring_other_bid_currency_;
  mojom::ComponentAuctionReportResultParamsPtr
      browser_signals_component_auction_report_result_params_;
  absl::optional<uint32_t> browser_signal_data_version_;
  absl::optional<base::TimeDelta> seller_timeout_;

  // Reuseable run loop for disconnection errors.
  std::unique_ptr<base::RunLoop> disconnect_run_loop_;
  absl::optional<std::string> disconnect_reason_;

  network::TestURLLoaderFactory url_loader_factory_;
  network::TestURLLoaderFactory alternate_url_loader_factory_;
  scoped_refptr<AuctionV8Helper> v8_helper_;

  TestAuctionNetworkEventsHandler auction_network_events_handler_;

  mojo::PendingRemote<mojom::AuctionSharedStorageHost>
      shared_storage_host_remote_;

  // Owns all created seller worklets - having a ReceiverSet allows them to have
  // a ClosePipeCallback which behaves just like the one in
  // AuctionWorkletServiceImpl, to better match production behavior.
  mojo::UniqueReceiverSet<mojom::SellerWorklet> seller_worklets_;
};

// Test the case the SellerWorklet pipe is closed before any of its methods are
// invoked. Nothing should happen.
TEST_F(SellerWorkletTest, PipeClosed) {
  auto sellet_worklet = CreateWorklet();
  sellet_worklet.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(SellerWorkletTest, NetworkError) {
  url_loader_factory_.AddResponse(decision_logic_url_.spec(),
                                  CreateBasicSellAdScript(),
                                  net::HTTP_NOT_FOUND);
  auto sellet_worklet = CreateWorklet();
  EXPECT_EQ("Failed to load https://url.test/ HTTP status = 404 Not Found.",
            WaitForDisconnect());

  // Wait until idle to ensure all requests have been observed within the
  // `auction_network_events_handler_`.
  task_environment_.RunUntilIdle();
  EXPECT_THAT(
      auction_network_events_handler_.GetObservedRequests(),
      testing::ElementsAre(
          "Sent URL: https://url.test/", "Received URL: https://url.test/",
          "Completion Status: net::ERR_HTTP_RESPONSE_CODE_FAILURE"));
}

TEST_F(SellerWorkletTest, CompileError) {
  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        "Invalid Javascript");
  auto sellet_worklet = CreateWorklet();
  std::string disconnect_error = WaitForDisconnect();
  EXPECT_THAT(disconnect_error, StartsWith("https://url.test/:1 "));
  EXPECT_THAT(disconnect_error, HasSubstr("SyntaxError"));
}

// Test parsing of return values.
TEST_F(SellerWorkletTest, ScoreAd) {
  // Base case. Also serves to make sure the script returned by
  // CreateBasicSellAdScript() does indeed work.
  RunScoreAdWithJavascriptExpectingResult(CreateBasicSellAdScript(), 1);

  // Wait until idle to ensure all requests have been observed within the
  // `auction_network_events_handler_`.
  task_environment_.RunUntilIdle();
  EXPECT_THAT(auction_network_events_handler_.GetObservedRequests(),
              testing::ElementsAre("Sent URL: https://url.test/",
                                   "Received URL: https://url.test/",
                                   "Completion Status: net::OK"));

  // Test returning results with the object format.
  RunScoreAdWithReturnValueExpectingResult("{desirability:3}", 3);
  RunScoreAdWithReturnValueExpectingResult("{desirability:0.5}", 0.5);
  RunScoreAdWithReturnValueExpectingResult("{desirability:0}", 0);
  RunScoreAdWithReturnValueExpectingResult("{desirability:-10}", 0);

  // Test returning a number, which is interpreted as a score.
  RunScoreAdWithReturnValueExpectingResult("3", 3);
  RunScoreAdWithReturnValueExpectingResult("0.5", 0.5);
  RunScoreAdWithReturnValueExpectingResult("0", 0);
  RunScoreAdWithReturnValueExpectingResult("-10", 0);

  // Unknown fields should be ignored.
  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:3, snore:1/0, smore:[15], shore:{desirability:2}}", 3);

  // No return value.
  RunScoreAdWithReturnValueExpectingResult(
      "", 0,
      {"https://url.test/ scoreAd() return: Required field 'desirability' "
       "is undefined."});

  // Wrong return type / invalid values.
  RunScoreAdWithReturnValueExpectingResult(
      "{hats:15}", 0,
      {"https://url.test/ scoreAd() return: Required field 'desirability' "
       "is undefined."});
  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:[15, 16]}", 0,
      {"https://url.test/ scoreAd() return: Converting field 'desirability' to "
       "a Number did not produce a finite double."});
  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:1/0}", 0,
      {"https://url.test/ scoreAd() return: Converting field 'desirability' to "
       "a Number did not produce a finite double."});
  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:0/0}", 0,
      {"https://url.test/ scoreAd() return: Converting field 'desirability' to "
       "a Number did not produce a finite double."});

  // Same tests as the previous block, but returning the value directly instead
  // of in an object.
  RunScoreAdWithReturnValueExpectingResult(
      "[15]", 0,
      {"https://url.test/ scoreAd() return: Required field 'desirability' "
       "is undefined."});
  RunScoreAdWithReturnValueExpectingResult(
      "1/0", 0, {"https://url.test/ scoreAd() returned an invalid score."});
  RunScoreAdWithReturnValueExpectingResult(
      "0/0", 0, {"https://url.test/ scoreAd() returned an invalid score."});
  RunScoreAdWithReturnValueExpectingResult(
      "-1/0", 0, {"https://url.test/ scoreAd() returned an invalid score."});
  RunScoreAdWithReturnValueExpectingResult(
      "true", 0,
      {"https://url.test/ scoreAd() return: Value passed as dictionary is "
       "neither object, null, nor undefined."});

  // Throw exception.
  RunScoreAdWithReturnValueExpectingResult(
      "shrimp", 0,
      {"https://url.test/:5 Uncaught ReferenceError: shrimp is not defined."});

  // JavaScript being itself and doing weird conversions.
  RunScoreAdWithReturnValueExpectingResult("{desirability:[15]}", 15);
  RunScoreAdWithReturnValueExpectingResult("{desirability:true}", 1);
}

TEST_F(SellerWorkletTest, ScoreAdAllowComponentAuction) {
  // Expected errors vector on failure.
  const std::vector<std::string> kExpectedErrorsOnFailure{
      R"(https://url.test/ scoreAd() return value does not have )"
      R"(allowComponentAuction set to true. Ad dropped from component )"
      R"(auction.)"};

  // Expected ComponentAuctionModifiedBidParams, for successful cases when a
  // component auction's `scoreAd()` script is simulated.
  const mojom::ComponentAuctionModifiedBidParamsPtr
      kExpectedComponentAuctionModifiedBidParams =
          mojom::ComponentAuctionModifiedBidParams::New(
              /*ad=*/"null", /*bid=*/0, /*bid_currency=*/absl::nullopt,
              /*has_bid=*/false);

  // With a null `browser_signals_other_seller_`, returning a raw score is
  // allowed, and if returning an object, `allowComponentAuction` doesn't
  // matter.
  browser_signals_other_seller_.reset();
  RunScoreAdWithReturnValueExpectingResult("1", 1);
  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:1, allowComponentAuction:true}", 1);
  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:1, allowComponentAuction:false}", 1);
  RunScoreAdWithReturnValueExpectingResult("{desirability:1}", 1);
  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:1, allowComponentAuction:1}", 1);
  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:1, allowComponentAuction:0}", 1);
  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:1, allowComponentAuction:[32]}", 1);

  // With a top-level seller in `browser_signals_other_seller_`, an object must
  // be returned if the score is positive, and `allowComponentAuction` must be
  // true.
  browser_signals_other_seller_ =
      mojom::ComponentAuctionOtherSeller::NewTopLevelSeller(
          url::Origin::Create(GURL("https://top.seller.test")));
  RunScoreAdWithReturnValueExpectingResult("1", 0, kExpectedErrorsOnFailure);
  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:1, allowComponentAuction:true}", 1,
      /*expected_errors=*/{},
      kExpectedComponentAuctionModifiedBidParams.Clone());
  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:1, allowComponentAuction:false}", 0,
      kExpectedErrorsOnFailure);
  RunScoreAdWithReturnValueExpectingResult("{desirability:1}", 0,
                                           kExpectedErrorsOnFailure);
  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:1, allowComponentAuction:1}", 1,
      /*expected_errors=*/{},
      kExpectedComponentAuctionModifiedBidParams.Clone());
  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:1, allowComponentAuction:0}", 0, kExpectedErrorsOnFailure);
  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:1, allowComponentAuction:[32]}", 1,
      /*expected_errors=*/{},
      kExpectedComponentAuctionModifiedBidParams.Clone());

  // With a desirability <= 0, a false `allowComponentAuction` value is not
  // considered an error.
  RunScoreAdWithReturnValueExpectingResult("0", 0);
  RunScoreAdWithReturnValueExpectingResult("-1", 0);
  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:0, allowComponentAuction:false}", 0);
  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:-1, allowComponentAuction:false}", 0);

  // A missing `allowComponentAuction` field is treated as if it were "false".
  RunScoreAdWithReturnValueExpectingResult("{desirability:1}", 0,
                                           kExpectedErrorsOnFailure);

  // With a component seller in `browser_signals_other_seller_`, an object must
  // be returned, and `allowComponentAuction` must be true.
  browser_signals_other_seller_ =
      mojom::ComponentAuctionOtherSeller::NewComponentSeller(
          url::Origin::Create(GURL("https://component.seller.test")));
  RunScoreAdWithReturnValueExpectingResult("1", 0, kExpectedErrorsOnFailure);
  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:1, allowComponentAuction:true}", 1);
  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:1, allowComponentAuction:false}", 0,
      kExpectedErrorsOnFailure);
  RunScoreAdWithReturnValueExpectingResult("{desirability:1}", 0,
                                           kExpectedErrorsOnFailure);
  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:1, allowComponentAuction:1}", 1);
  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:1, allowComponentAuction:0}", 0, kExpectedErrorsOnFailure);
  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:1, allowComponentAuction:[32]}", 1);
}

// Test the `ad` output field of scoreAd().
TEST_F(SellerWorkletTest, ScoreAdAd) {
  // When `browser_signals_other_seller_` is not top a top-level auction (i.e.
  // scoreAd() is invoked for a top-level seller, so the other seller is empty
  // or a component seller), `ad` field is ignored, and a null
  // ComponentAuctionModifiedBidParamsPtr is returned.
  browser_signals_other_seller_.reset();
  RunScoreAdWithReturnValueExpectingResult(
      "{ad:null, desirability:1}", 1,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr());
  browser_signals_other_seller_ =
      mojom::ComponentAuctionOtherSeller::NewComponentSeller(
          url::Origin::Create(GURL("https://component.seller.test")));
  RunScoreAdWithReturnValueExpectingResult(
      "{ad:null, desirability:1, allowComponentAuction:true}", 1,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr());

  // When `browser_signals_other_seller_` is not top a top-level auction, a
  // ComponentAuctionModifiedBidParamsPtr with an `ad` field is returned.

  browser_signals_other_seller_ =
      mojom::ComponentAuctionOtherSeller::NewTopLevelSeller(
          url::Origin::Create(GURL("https://top.seller.test")));
  // If the ad field isn't present, `ad` is set to "null".
  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:1, allowComponentAuction:true}", 1, /*expected_errors=*/{},
      mojom::ComponentAuctionModifiedBidParams::New(
          /*ad=*/"null", /*bid=*/0, /*bid_currency=*/absl::nullopt,
          /*has_bid=*/false));
  RunScoreAdWithReturnValueExpectingResult(
      "{ad:null, desirability:1, allowComponentAuction:true}", 1,
      /*expected_errors=*/{},
      mojom::ComponentAuctionModifiedBidParams::New(
          /*ad=*/"null", /*bid=*/0, /*bid_currency=*/absl::nullopt,
          /*has_bid=*/false));
  RunScoreAdWithReturnValueExpectingResult(
      R"({ad:"foo", desirability:1, allowComponentAuction:true})", 1,
      /*expected_errors=*/{},
      mojom::ComponentAuctionModifiedBidParams::New(
          /*ad=*/R"("foo")", /*bid=*/0, /*bid_currency=*/absl::nullopt,
          /*has_bid=*/false));
  RunScoreAdWithReturnValueExpectingResult(
      "{ad:[[35]], desirability:1, allowComponentAuction:true}", 1,
      /*expected_errors=*/{},
      mojom::ComponentAuctionModifiedBidParams::New(
          /*ad=*/"[[35]]", /*bid=*/0, /*bid_currency=*/absl::nullopt,
          /*has_bid=*/false));
}

// Test the `rejectReason` output field of scoreAd().
TEST_F(SellerWorkletTest, ScoreAdRejectReason) {
  const struct {
    std::string reason_str;
    mojom::RejectReason reason_enum;
  } kTestCases[] = {
      {"not-available", mojom::RejectReason::kNotAvailable},
      {"invalid-bid", mojom::RejectReason::kInvalidBid},
      {"bid-below-auction-floor", mojom::RejectReason::kBidBelowAuctionFloor},
      {"pending-approval-by-exchange",
       mojom::RejectReason::kPendingApprovalByExchange},
      {"disapproved-by-exchange", mojom::RejectReason::kDisapprovedByExchange},
      {"blocked-by-publisher", mojom::RejectReason::kBlockedByPublisher},
      {"language-exclusions", mojom::RejectReason::kLanguageExclusions},
      {"category-exclusions", mojom::RejectReason::kCategoryExclusions},
  };

  for (const auto& test_case : kTestCases) {
    RunScoreAdWithReturnValueExpectingResult(
        base::StringPrintf(R"({desirability:-1, rejectReason: '%s'})",
                           test_case.reason_str.c_str()),
        0,
        /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
        /*expected_data_version=*/absl::nullopt,
        /*expected_debug_loss_report_url=*/absl::nullopt,
        /*expected_debug_win_report_url=*/absl::nullopt, test_case.reason_enum);
  }

  // Default reject reason is mojom::RejectReason::kNotAvailable, if scoreAd()
  // does not return one.
  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:-1}", 0,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      mojom::RejectReason::kNotAvailable);

  // Reject reason is ignored when desirability is positive.
  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:3, rejectReason: 'invalid-bid'}", 3,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_reject_reason*/ mojom::RejectReason::kNotAvailable);
}

// Invalid `rejectReason` output of scoreAd() results in error.
TEST_F(SellerWorkletTest, ScoreAdInvalidRejectReason) {
  // Reject reason string returned by seller script is case sensitive.
  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:-1, rejectReason: 'INVALID-BID'}", 0,
      /*expected_errors=*/
      {"https://url.test/ scoreAd() returned an invalid reject reason."},
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_reject_reason*/ mojom::RejectReason::kNotAvailable);

  // Reject reason returned by seller script must be a string.
  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:-1, rejectReason: 2}", 0,
      /*expected_errors=*/
      {"https://url.test/ scoreAd() returned an invalid reject reason."},
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_reject_reason*/ mojom::RejectReason::kNotAvailable);
}

// Test the `bid` output field of scoreAd().
TEST_F(SellerWorkletTest, ScoreAdModifiesBid) {
  // When `browser_signals_other_seller_` is not top a top-level auction (i.e.
  // scoreAd() is invoked for a top-level seller), `bid` field is ignored.
  browser_signals_other_seller_.reset();
  RunScoreAdWithReturnValueExpectingResult(
      "{bid:5, desirability:1}", 1,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr());
  browser_signals_other_seller_ =
      mojom::ComponentAuctionOtherSeller::NewComponentSeller(
          url::Origin::Create(GURL("https://component.seller.test")));
  RunScoreAdWithReturnValueExpectingResult(
      "{bid:10, desirability:1, allowComponentAuction:true}", 1,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr());

  // Otherwise, bid field is returned to the caller.
  browser_signals_other_seller_ =
      mojom::ComponentAuctionOtherSeller::NewTopLevelSeller(
          url::Origin::Create(GURL("https://top.level.seller.test")));
  RunScoreAdWithReturnValueExpectingResult(
      "{ad:null, desirability:1, allowComponentAuction:true, bid:13}", 1,
      /*expected_errors=*/{},
      mojom::ComponentAuctionModifiedBidParams::New(
          /*ad=*/"null", /*bid=*/13, /*bid_currency=*/absl::nullopt,
          /*has_bid=*/true));
  RunScoreAdWithReturnValueExpectingResult(
      "{ad:null, desirability:1, allowComponentAuction:true, bid:1.2}", 1,
      /*expected_errors=*/{},
      mojom::ComponentAuctionModifiedBidParams::New(
          /*ad=*/"null", /*bid=*/1.2, /*bid_currency=*/absl::nullopt,
          /*has_bid=*/true));

  // Can also specify the currency.
  RunScoreAdWithReturnValueExpectingResult(
      "{ad:null, desirability:1, allowComponentAuction:true, "
      "bid:1.2, bidCurrency: 'USD'}",
      1,
      /*expected_errors=*/{},
      mojom::ComponentAuctionModifiedBidParams::New(
          /*ad=*/"null", /*bid=*/1.2,
          /*bid_currency=*/blink::AdCurrency::From("USD"),
          /*has_bid=*/true));

  // Not providing a bid is fine.
  RunScoreAdWithReturnValueExpectingResult(
      "{ad:null, desirability:1, allowComponentAuction:true}", 1,
      /*expected_errors=*/{},
      mojom::ComponentAuctionModifiedBidParams::New(
          /*ad=*/"null", /*bid=*/0, /*bid_currency=*/absl::nullopt,
          /*has_bid=*/false));

  // JS coercions happen for bid as well.
  RunScoreAdWithReturnValueExpectingResult(
      R"({ad:null, desirability:1, allowComponentAuction:true, bid:"5"})", 1,
      /*expected_errors=*/{},
      mojom::ComponentAuctionModifiedBidParams::New(
          /*ad=*/"null", /*bid=*/5, /*bid_currency=*/absl::nullopt,
          /*has_bid=*/true));
  RunScoreAdWithReturnValueExpectingResult(
      "{ad:null, desirability:1, allowComponentAuction:true, bid:[4]}", 1,
      /*expected_errors=*/{},
      mojom::ComponentAuctionModifiedBidParams::New(
          /*ad=*/"null", /*bid=*/4, /*bid_currency=*/absl::nullopt,
          /*has_bid=*/true));

  // Invalid bids result in errors.
  RunScoreAdWithReturnValueExpectingResult(
      R"({ad:null, desirability:1, allowComponentAuction:true, bid:0})", 0,
      /*expected_errors=*/
      {"https://url.test/ scoreAd() returned an invalid bid."});
  RunScoreAdWithReturnValueExpectingResult(
      R"({ad:null, desirability:1, allowComponentAuction:true, bid:-1})", 0,
      /*expected_errors=*/
      {"https://url.test/ scoreAd() returned an invalid bid."});
  RunScoreAdWithReturnValueExpectingResult(
      "{ad:null, desirability:1, allowComponentAuction:true, bid:1/0}", 0,
      /*expected_errors=*/
      {"https://url.test/ scoreAd() return: Converting field 'bid' to a Number "
       "did not produce a finite double."});
  RunScoreAdWithReturnValueExpectingResult(
      "{ad:null, desirability:1, allowComponentAuction:true, bid:-1/0}", 0,
      /*expected_errors=*/
      {"https://url.test/ scoreAd() return: Converting field 'bid' to a Number "
       "did not produce a finite double."});
  RunScoreAdWithReturnValueExpectingResult(
      "{ad:null, desirability:1, allowComponentAuction:true, bid:0/0}", 0,
      /*expected_errors=*/
      {"https://url.test/ scoreAd() return: Converting field 'bid' to a Number "
       "did not produce a finite double."});

  // Currency mismatch or invalid currency produce errors, too
  // (and a match doesn't)
  RunScoreAdWithReturnValueExpectingResult(
      "{ad:null, desirability:1, allowComponentAuction:true, "
      "bid:1.2, bidCurrency: 'USSD'}",
      0, {"https://url.test/ scoreAd() returned an invalid bidCurrency."},
      /*expected_component_auction_modified_bid_params=*/
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      mojom::RejectReason::kWrongScoreAdCurrency);

  // Special case: if there is a manually specified reject-reason, it goes in
  // and not the currency mismatch.
  RunScoreAdWithReturnValueExpectingResult(
      "{ad:null, desirability:1, allowComponentAuction:true, "
      "bid:1.2, bidCurrency: 'USSD', rejectReason: 'category-exclusions'}",
      0, {"https://url.test/ scoreAd() returned an invalid bidCurrency."},
      /*expected_component_auction_modified_bid_params=*/
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      mojom::RejectReason::kCategoryExclusions);

  auction_ad_config_non_shared_params_.seller_currency =
      blink::AdCurrency::From("CAD");
  RunScoreAdWithReturnValueExpectingResult(
      "{ad:null, desirability:1, allowComponentAuction:true, "
      "bid:1.2, bidCurrency: 'USD'}",
      0,
      {"https://url.test/ scoreAd() bidCurrency mismatch vs own sellerCurrency,"
       " expected 'CAD' got 'USD'."},
      /*expected_component_auction_modified_bid_params=*/
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      mojom::RejectReason::kWrongScoreAdCurrency);

  RunScoreAdWithReturnValueExpectingResult(
      "{ad:null, desirability:1, allowComponentAuction:true, "
      "bid:1.2, bidCurrency: 'CAD'}",
      1,
      /*expected_errors=*/{},
      mojom::ComponentAuctionModifiedBidParams::New(
          /*ad=*/"null", /*bid=*/1.2,
          /*bid_currency=*/blink::AdCurrency::From("CAD"),
          /*has_bid=*/true));
  auction_ad_config_non_shared_params_.seller_currency = absl::nullopt;

  // In component auctions, there is also verification against parent auction's
  // bidderCurrencies.
  component_expect_bid_currency_ = blink::AdCurrency::From("EUR");
  RunScoreAdWithReturnValueExpectingResult(
      "{ad:null, desirability:1, allowComponentAuction:true, "
      "bid:1.2, bidCurrency: 'USD'}",
      0,
      {"https://url.test/ scoreAd() bidCurrency mismatch in component auction "
       "vs parent auction bidderCurrency, expected 'EUR' got 'USD'."},
      /*expected_component_auction_modified_bid_params=*/
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      mojom::RejectReason::kWrongScoreAdCurrency);
  RunScoreAdWithReturnValueExpectingResult(
      "{ad:null, desirability:1, allowComponentAuction:true, "
      "bid:1.2, bidCurrency: 'EUR'}",
      1,
      /*expected_errors=*/{},
      mojom::ComponentAuctionModifiedBidParams::New(
          /*ad=*/"null", /*bid=*/1.2,
          /*bid_currency=*/blink::AdCurrency::From("EUR"),
          /*has_bid=*/true));
  component_expect_bid_currency_ = absl::nullopt;

  // A 0 bid is normally considered invalid, unless desirability is 0, in which
  // case it is ignored.
  RunScoreAdWithReturnValueExpectingResult(
      "{ad:null, desirability:0, allowComponentAuction:true, bid:0}", 0);
}

// Test currency checks when score ad does not modify bid.
TEST_F(SellerWorkletTest, ScoreAdDoesNotModifyBidCurrency) {
  // Set us up as component seller.
  browser_signals_other_seller_ =
      mojom::ComponentAuctionOtherSeller::NewTopLevelSeller(
          url::Origin::Create(GURL("https://top.level.seller.test")));
  bid_currency_ = blink::AdCurrency::From("CAD");
  auction_ad_config_non_shared_params_.seller_currency =
      blink::AdCurrency::From("USD");
  RunScoreAdWithReturnValueExpectingResult(
      "{ad:null, desirability:1, allowComponentAuction:true}", 0,
      {"https://url.test/ scoreAd() bid passthrough mismatch vs own "
       "sellerCurrency, expected 'USD' got 'CAD'."},
      /*expected_component_auction_modified_bid_params=*/
      mojom::ComponentAuctionModifiedBidParams::New(
          /*ad=*/"null", /*bid=*/0, /*bid_currency=*/absl::nullopt,
          /*has_bid=*/false),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      mojom::RejectReason::kWrongScoreAdCurrency);
}

// Test the `incomingBidInSellerCurrency` output field of scoreAd()
TEST_F(SellerWorkletTest, ScoreAdIncomingBidInSellerCurrency) {
  // Configure bid currency to make sure checks for passthrough are done.
  bid_currency_ = blink::AdCurrency::From("USD");

  // If seller currency isn't configured, can't set it.
  auction_ad_config_non_shared_params_.seller_currency = absl::nullopt;

  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:1, incomingBidInSellerCurrency: 4}", 0,
      {"https://url.test/ scoreAd() attempting to set "
       "incomingBidInSellerCurrency without a configured sellerCurrency."});

  auction_ad_config_non_shared_params_.seller_currency =
      blink::AdCurrency::From("CAD");
  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:1, incomingBidInSellerCurrency: 'foo'}", 0,
      {"https://url.test/ scoreAd() return: Converting field "
       "'incomingBidInSellerCurrency' to a Number did not produce a finite "
       "double."});

  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:1, incomingBidInSellerCurrency: -100}", 0,
      {"https://url.test/ scoreAd() incomingBidInSellerCurrency not "
       "a valid bid."});

  // Now pass in a valid one.
  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:1, incomingBidInSellerCurrency: 100}", 1,
      /*expected_errors=*/std::vector<std::string>(),
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_reject_reason=*/
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_bid_in_seller_currency=*/100);

  // When bid currency matches seller currency, incomingBidInSellerCurrency
  // should only be be accepted if it matches the incoming value.
  bid_currency_ = blink::AdCurrency::From("CAD");
  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:1, incomingBidInSellerCurrency: 100}", 0,
      {"https://url.test/ scoreAd() attempting to set "
       "incomingBidInSellerCurrency inconsistent with incoming bid already in "
       "seller currency."});

  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:1, incomingBidInSellerCurrency: 1}", 1,
      /*expected_errors=*/std::vector<std::string>(),
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_reject_reason=*/
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_bid_in_seller_currency=*/1);

  // ...can also have that same-currency bid directly forwarded.
  bid_ = 3.14;
  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:1}", 1,
      /*expected_errors=*/std::vector<std::string>(),
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_reject_reason=*/
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_bid_in_seller_currency=*/3.14);

  // This should also work if we use the number-only shorthand.
  RunScoreAdWithReturnValueExpectingResult(
      "1", 1,
      /*expected_errors=*/std::vector<std::string>(),
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_reject_reason=*/
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_bid_in_seller_currency=*/3.14);
}

TEST_F(SellerWorkletTest, ScoreAdDateNotAvailable) {
  RunScoreAdWithReturnValueExpectingResult(
      "Date.parse(Date().toString())", 0,
      {"https://url.test/:5 Uncaught ReferenceError: Date is not defined."});
}

TEST_F(SellerWorkletTest, ScoreAdMedata) {
  ad_metadata_ = R"("foo")";
  RunScoreAdWithReturnValueExpectingResult(R"(adMetadata === "foo" ? 4 : 0)",
                                           4);

  ad_metadata_ = "[1]";
  RunScoreAdWithReturnValueExpectingResult(R"(adMetadata[0] === 1 ? 4 : 0)", 4);

  // If adMetadata is invalid, score should be 0.
  ad_metadata_ = "{invalid_json";
  RunScoreAdWithReturnValueExpectingResult("1", 0);
}

TEST_F(SellerWorkletTest, ScoreAdTopWindowOrigin) {
  top_window_origin_ = url::Origin::Create(GURL("https://foo.test/"));
  RunScoreAdWithReturnValueExpectingResult(
      R"(browserSignals.topWindowHostname == "foo.test" ? 2 : 0)", 2);

  top_window_origin_ = url::Origin::Create(GURL("https://[::1]:40000/"));
  RunScoreAdWithReturnValueExpectingResult(
      R"(browserSignals.topWindowHostname == "[::1]" ? 3 : 0)", 3);
}

TEST_F(SellerWorkletTest, ScoreAdTopLevelSeller) {
  // `topLevelSeller` should be empty when a top-level seller is scoring a bid
  // from its own auction.
  browser_signals_other_seller_.reset();
  RunScoreAdWithReturnValueExpectingResult(
      R"("topLevelSeller" in browserSignals ? 0 : 1)", 1);

  // `topLevelSeller` should be set when a top-level seller is scoring a bid
  // from a component auction. Must set `allowComponentAuction` to true for any
  // value to be returned.
  browser_signals_other_seller_ =
      mojom::ComponentAuctionOtherSeller::NewTopLevelSeller(
          url::Origin::Create(GURL("https://top.seller.test")));
  RunScoreAdWithReturnValueExpectingResult(
      R"(browserSignals.topLevelSeller === "https://top.seller.test" ?
             {desirability: 2, allowComponentAuction: true} : 0)",
      2, /*expected_errors=*/{},
      mojom::ComponentAuctionModifiedBidParams::New(
          /*ad=*/"null", /*bid=*/0, /*bid_currency=*/absl::nullopt,
          /*has_bid=*/false));

  // `topLevelSeller` should be empty when a component seller is scoring a bid.
  // Must set `allowComponentAuction` to true for any value to be returned.
  browser_signals_other_seller_ =
      mojom::ComponentAuctionOtherSeller::NewComponentSeller(
          url::Origin::Create(GURL("https://component.test")));
  RunScoreAdWithReturnValueExpectingResult(
      R"("topLevelSeller" in browserSignals ?
             0 : {desirability: 3, allowComponentAuction: true})",
      3);
}

TEST_F(SellerWorkletTest, ScoreAdComponentSeller) {
  // `componentSeller` should be empty when a top-level seller is scoring a bid
  // from its own auction.
  browser_signals_other_seller_.reset();
  RunScoreAdWithReturnValueExpectingResult(
      R"("componentSeller" in browserSignals ? 0 : 1)", 1);

  // `componentSeller` should be empty when a top-level seller is scoring a bid
  // from a component auction. Must set `allowComponentAuction` to true for any
  // value to be returned.
  browser_signals_other_seller_ =
      mojom::ComponentAuctionOtherSeller::NewTopLevelSeller(
          url::Origin::Create(GURL("https://top.seller.test")));
  RunScoreAdWithReturnValueExpectingResult(
      R"("componentSeller" in browserSignals ?
             0 : {desirability: 2, allowComponentAuction: true})",
      2, /*expected_errors=*/{},
      mojom::ComponentAuctionModifiedBidParams::New(
          /*ad=*/"null", /*bid=*/0, /*bid_currency=*/absl::nullopt,
          /*has_bid=*/false));

  // `componentSeller` should be set when a component seller is scoring a bid.
  // Must set `allowComponentAuction` to true for any value to be returned.
  browser_signals_other_seller_ =
      mojom::ComponentAuctionOtherSeller::NewComponentSeller(
          url::Origin::Create(GURL("https://component.test")));
  RunScoreAdWithReturnValueExpectingResult(
      R"(browserSignals.componentSeller === "https://component.test" ?
             {desirability: 3, allowComponentAuction: true} : 0)",
      3);
}

TEST_F(SellerWorkletTest, ScoreAdInterestGroupOwner) {
  browser_signal_interest_group_owner_ =
      url::Origin::Create(GURL("https://foo.test/"));
  RunScoreAdWithReturnValueExpectingResult(
      R"(browserSignals.interestGroupOwner == "https://foo.test" ? 2 : 0)", 2);

  browser_signal_interest_group_owner_ =
      url::Origin::Create(GURL("https://[::1]:40000/"));
  RunScoreAdWithReturnValueExpectingResult(
      R"(browserSignals.interestGroupOwner == "https://[::1]:40000" ? 3 : 0)",
      3);
}

TEST_F(SellerWorkletTest, ScoreAdRenderUrl) {
  browser_signal_render_url_ = GURL("https://bar.test/path");
  RunScoreAdWithReturnValueExpectingResult(
      R"(browserSignals.renderURL === "https://bar.test/path" ? 3 : 0)", 3);
  RunScoreAdWithReturnValueExpectingResult(
      R"(browserSignals.renderURL === "https://bar.test/" ? 3 : 0)", 0);
}

TEST_F(SellerWorkletTest, ScoreAdAdComponents) {
  browser_signal_ad_components_.clear();
  RunScoreAdWithReturnValueExpectingResult(
      R"(browserSignals.adComponents === undefined ? 3 : 0)", 3);

  browser_signal_ad_components_ = {GURL("https://bar.test/path")};
  RunScoreAdWithReturnValueExpectingResult(
      R"(browserSignals.adComponents.length)", 1);
  RunScoreAdWithReturnValueExpectingResult(
      R"(browserSignals.adComponents[0] === "https://bar.test/path" ? 3 : 0)",
      3);

  // These are not in lexical order to make sure ordering is preserved.
  browser_signal_ad_components_ = {GURL("https://2.test/"),
                                   GURL("https://1.test/"),
                                   GURL("https://3.test/")};
  RunScoreAdWithReturnValueExpectingResult(
      R"(browserSignals.adComponents.length)", 3);
  RunScoreAdWithReturnValueExpectingResult(
      R"(browserSignals.adComponents[0] === "https://2.test/" ? 3 : 0)", 3);
  RunScoreAdWithReturnValueExpectingResult(
      R"(browserSignals.adComponents[1] === "https://1.test/" ? 3 : 0)", 3);
  RunScoreAdWithReturnValueExpectingResult(
      R"(browserSignals.adComponents[2] === "https://3.test/" ? 3 : 0)", 3);
}

TEST_F(SellerWorkletTest, ScoreAdBid) {
  bid_ = 5;
  RunScoreAdWithReturnValueExpectingResult("bid", 5);
  bid_ = 0.5;
  RunScoreAdWithReturnValueExpectingResult("bid", 0.5);
  bid_ = -1;
  RunScoreAdWithReturnValueExpectingResult("bid", 0);
}

TEST_F(SellerWorkletTest, ScoreAdBidCurrency) {
  RunScoreAdWithReturnValueExpectingResult(
      "browserSignals.bidCurrency === '???' ? 2 : 0", 2);

  bid_currency_ = blink::AdCurrency::From("USD");
  RunScoreAdWithReturnValueExpectingResult(
      "browserSignals.bidCurrency === 'USD' ? 2 : 0", 2);
}

TEST_F(SellerWorkletTest, ScoreAdBiddingDuration) {
  // Test browserSignals.bidding_duration_msec.
  browser_signal_bidding_duration_msecs_ = 0;
  RunScoreAdWithReturnValueExpectingResult("browserSignals.biddingDurationMsec",
                                           0);
  browser_signal_bidding_duration_msecs_ = 100;
  RunScoreAdWithReturnValueExpectingResult("browserSignals.biddingDurationMsec",
                                           100);
}

// Test that auction config gets into scoreAd. More detailed handling of
// (shared) construction of actual object is in ReportResultAuctionConfigParam,
// as that worklet is easier to get things out of.
TEST_F(SellerWorkletTest, ScoreAdAuctionConfigParam) {
  decision_logic_url_ = GURL("https://url.test/");
  RunScoreAdWithReturnValueExpectingResult(
      "auctionConfig.decisionLogicURL.length",
      decision_logic_url_.spec().length());

  decision_logic_url_ = GURL("https://url.test/longer/url");
  RunScoreAdWithReturnValueExpectingResult(
      "auctionConfig.decisionLogicURL.length",
      decision_logic_url_.spec().length());

  direct_from_seller_auction_signals_header_ad_slot_ = R"("abcde")";
  RunScoreAdWithReturnValueExpectingResult(
      "directFromSellerSignals.auctionSignals.length",
      direct_from_seller_auction_signals_header_ad_slot_->length() -
          std::string(R"("")").length());

  direct_from_seller_seller_signals_header_ad_slot_ = R"("abcdefg")";
  RunScoreAdWithReturnValueExpectingResult(
      "directFromSellerSignals.sellerSignals.length",
      direct_from_seller_seller_signals_header_ad_slot_->length() -
          std::string(R"("")").length());
}

TEST_F(SellerWorkletTest, ScoreAdExperimentGroupIdParam) {
  RunScoreAdWithReturnValueExpectingResult(
      R"("experimentGroupId" in auctionConfig ? 1 : 0)", 0);

  experiment_group_id_ = 954u;
  RunScoreAdWithReturnValueExpectingResult("auctionConfig.experimentGroupId",
                                           954);
}

// Tests that trusted scoring signals are correctly passed to scoreAd(). Each
// request is sent individually, without calling SendPendingSignalsRequests() -
// instead, the test advances the mock clock by
// TrustedSignalsRequestManager::kAutoSendDelay, triggering each request to
// automatically be sent.
TEST_F(SellerWorkletTest, ScoreAdTrustedScoringSignals) {
  // With no trusted scoring signals URL, `trustedScoringSignals` should be
  // null.
  trusted_scoring_signals_url_ = absl::nullopt;
  RunScoreAdWithReturnValueExpectingResult(
      "trustedScoringSignals === null ? 1 : 0", 1);

  trusted_scoring_signals_url_ =
      GURL("https://url.test/trusted_scoring_signals");
  // Trusted scoring signals URL without any component ads.
  const GURL kNoComponentSignalsUrl = GURL(
      "https://url.test/trusted_scoring_signals?hostname=window.test"
      "&renderUrls=https%3A%2F%2Frender.url.test%2F");

  // Successful download case.

  AddVersionedJsonResponse(&url_loader_factory_, kNoComponentSignalsUrl,
                           kTrustedScoringSignalsResponse, /*data_version=*/1);

  // Each call should cause the clock to advance exactly `kAutoSendDelay`
  // milliseconds before the request is send over the wire, waiting for other
  // requests.
  RunScoreAdWithReturnValueExpectingResultInExactTime(
      "trustedScoringSignals.renderURL['https://render.url.test/']",
      4 /* Magic value in trustedScoringSignals */,
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      TrustedSignalsRequestManager::kAutoSendDelay, /*expected_errors=*/{},
      /*expected_data_version=*/1);
  RunScoreAdWithReturnValueExpectingResultInExactTime(
      "trustedScoringSignals.adComponentRenderURLs === undefined ? 1 : 0", 1,
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      TrustedSignalsRequestManager::kAutoSendDelay, /*expected_errors=*/{},
      /*expected_data_version=*/1);

  // A network error when fetching the scoring signals results in null
  // `trustedScoringSignals`. This case is just before the component ad test
  // case so that its error response for `kNoComponentSignalsUrl` makes a
  // failure if that URL is incorrectly requested in the component ad test case.
  url_loader_factory_.AddResponse(kNoComponentSignalsUrl.spec(),
                                  /*content=*/std::string(),
                                  net::HTTP_NOT_FOUND);
  RunScoreAdWithReturnValueExpectingResultInExactTime(
      "trustedScoringSignals === null ? 1 : 0", 1,
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      TrustedSignalsRequestManager::kAutoSendDelay,
      /*expected_errors=*/
      {base::StringPrintf("Failed to load %s HTTP status = 404 Not Found.",
                          kNoComponentSignalsUrl.spec().c_str())});

  browser_signal_ad_components_ = {GURL("https://component1.test/"),
                                   GURL("https://component2.test/")};
  AddVersionedJsonResponse(
      &url_loader_factory_,
      GURL("https://url.test/trusted_scoring_signals?hostname=window.test"
           "&renderUrls=https%3A%2F%2Frender.url.test%2F"
           "&adComponentRenderUrls=https%3A%2F%2Fcomponent1.test%2F,"
           "https%3A%2F%2Fcomponent2.test%2F"),
      kTrustedScoringSignalsResponse, /*data_version=*/5);

  RunScoreAdWithReturnValueExpectingResultInExactTime(
      "trustedScoringSignals.renderURL['https://render.url.test/']",
      4 /* Magic value in trustedScoringSignals */,
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      TrustedSignalsRequestManager::kAutoSendDelay, /*expected_errors=*/{},
      /*expected_data_version=*/5);
  RunScoreAdWithReturnValueExpectingResultInExactTime(
      "trustedScoringSignals.adComponentRenderURLs['https://component1.test/']",
      1 /* Magic value in trustedScoringSignals */,
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      TrustedSignalsRequestManager::kAutoSendDelay, /*expected_errors=*/{},
      /*expected_data_version=*/5);
  RunScoreAdWithReturnValueExpectingResultInExactTime(
      "trustedScoringSignals.adComponentRenderURLs['https://component2.test/']",
      2 /* Magic value in trustedScoringSignals */,
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      TrustedSignalsRequestManager::kAutoSendDelay, /*expected_errors=*/{},
      /*expected_data_version=*/5);
}

TEST_F(SellerWorkletTest, ScoreAdTrustedScoringSignalsLatency) {
  const base::TimeDelta kDelay = base::Milliseconds(135);
  trusted_scoring_signals_url_ =
      GURL("https://url.test/trusted_scoring_signals");
  // Trusted scoring signals URL without any component ads.
  const GURL kNoComponentSignalsUrl = GURL(
      "https://url.test/trusted_scoring_signals?hostname=window.test"
      "&renderUrls=https%3A%2F%2Frender.url.test%2F");

  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        CreateScoreAdScript(/*raw_return_value=*/"1", ""));
  auto seller_worklet = CreateWorklet();
  ASSERT_TRUE(seller_worklet);
  base::RunLoop run_loop;
  RunScoreAdOnWorkletAsync(
      seller_worklet.get(), /*expected_score=*/1,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/1,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_bid_in_seller_currency=*/absl::nullopt,
      /*expected_score_ad_timeout=*/false,
      /*expected_signals_fetch_latency=*/kDelay,
      /*expected_code_ready_latency=*/absl::nullopt, run_loop.QuitClosure());
  task_environment_.RunUntilIdle();
  task_environment_.FastForwardBy(kDelay);
  AddVersionedJsonResponse(&url_loader_factory_, kNoComponentSignalsUrl,
                           kTrustedScoringSignalsResponse, /*data_version=*/1);
  run_loop.Run();
}

TEST_F(SellerWorkletTest, ScoreAdCodeReadyLatency) {
  const base::TimeDelta kDelay = base::Milliseconds(235);
  trusted_scoring_signals_url_ =
      GURL("https://url.test/trusted_scoring_signals");
  // Trusted scoring signals URL without any component ads.
  const GURL kNoComponentSignalsUrl = GURL(
      "https://url.test/trusted_scoring_signals?hostname=window.test"
      "&renderUrls=https%3A%2F%2Frender.url.test%2F");
  AddVersionedJsonResponse(&url_loader_factory_, kNoComponentSignalsUrl,
                           kTrustedScoringSignalsResponse, /*data_version=*/1);

  auto seller_worklet = CreateWorklet();
  ASSERT_TRUE(seller_worklet);
  base::RunLoop run_loop;
  RunScoreAdOnWorkletAsync(
      seller_worklet.get(), /*expected_score=*/1,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/1,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_bid_in_seller_currency=*/absl::nullopt,
      /*expected_score_ad_timeout=*/false,
      /*expected_signals_fetch_latency=*/absl::nullopt,
      /*expected_code_ready_latency=*/kDelay, run_loop.QuitClosure());
  task_environment_.RunUntilIdle();
  task_environment_.FastForwardBy(kDelay);
  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        CreateScoreAdScript(/*raw_return_value=*/"1", ""));
  run_loop.Run();
}

TEST_F(SellerWorkletTest, ScoreAdDataVersion) {
  trusted_scoring_signals_url_ =
      GURL("https://url.test/trusted_scoring_signals");
  // Trusted scoring signals URL without any component ads.
  const GURL kNoComponentSignalsUrl = GURL(
      "https://url.test/trusted_scoring_signals?hostname=window.test"
      "&renderUrls=https%3A%2F%2Frender.url.test%2F");

  // Successful download case.
  AddVersionedJsonResponse(&url_loader_factory_, kNoComponentSignalsUrl,
                           kTrustedScoringSignalsResponse,
                           /*data_version=*/100);
  RunScoreAdWithReturnValueExpectingResult(
      "browserSignals.dataVersion", 100,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/100);
}

TEST_F(SellerWorkletTest, ScoreAdExperimentGroupId) {
  experiment_group_id_ = 3948u;
  trusted_scoring_signals_url_ =
      GURL("https://url.test/trusted_scoring_signals");
  // Trusted scoring signals URL without any component ads and the above
  // experiment group id.
  const GURL kSignalsUrl = GURL(
      "https://url.test/trusted_scoring_signals?hostname=window.test"
      "&renderUrls=https%3A%2F%2Frender.url.test%2F"
      "&experimentGroupId=3948");

  // The experiment ID is also passed in in auction config.
  AddJsonResponse(&url_loader_factory_, kSignalsUrl,
                  kTrustedScoringSignalsResponse);
  RunScoreAdWithReturnValueExpectingResult("auctionConfig.experimentGroupId",
                                           3948,
                                           /*expected_errors=*/{});
}

// Test the case of a bunch of ScoreAd() calls in parallel, all started before
// the worklet script has loaded.
TEST_F(SellerWorkletTest, ScoreAdParallelBeforeLoadComplete) {
  auto seller_worklet = CreateWorklet(/*pause_for_debugger_on_start=*/false);

  const size_t kNumWorklets = 10;
  size_t num_completed_worklets = 0;
  base::RunLoop run_loop;
  for (size_t i = 0; i < kNumWorklets; ++i) {
    browser_signal_render_url_ = GURL(base::StringPrintf("https://foo/%zu", i));
    RunScoreAdOnWorkletAsync(seller_worklet.get(), /*expected_score=*/i,
                             /*expected_errors=*/std::vector<std::string>(),
                             mojom::ComponentAuctionModifiedBidParamsPtr(),
                             /*expected_data_version=*/absl::nullopt,
                             /*expected_debug_loss_report_url=*/absl::nullopt,
                             /*expected_debug_win_report_url=*/absl::nullopt,
                             /*expected_reject_reason=*/
                             mojom::RejectReason::kNotAvailable,
                             /*expected_pa_requests=*/{},
                             /*expected_bid_in_seller_currency=*/absl::nullopt,
                             /*expected_score_ad_timeout=*/false,
                             /*expected_signals_fetch_latency=*/absl::nullopt,
                             /*expected_code_ready_latency=*/absl::nullopt,
                             base::BindLambdaForTesting([&]() {
                               ++num_completed_worklets;
                               if (num_completed_worklets == kNumWorklets) {
                                 run_loop.Quit();
                               }
                             }));
  }

  // No calls should complete, since the script hasn't loaded yet.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0u, num_completed_worklets);

  // Load a seller script that uses the last character of `renderURL` as the
  // score. The worklet should report a successful load.
  AddJavascriptResponse(
      &url_loader_factory_, decision_logic_url_,
      CreateScoreAdScript("parseInt(browserSignals.renderURL.slice(-1))"));

  // All scripts should complete successfully.
  run_loop.Run();
}

// Test the case of a bunch of ScoreAd() calls in parallel, all started after
// the worklet script has loaded.
TEST_F(SellerWorkletTest, ScoreAdParallelAfterLoadComplete) {
  // Seller script that uses the last character of `renderURL` as the score.
  AddJavascriptResponse(
      &url_loader_factory_, decision_logic_url_,
      CreateScoreAdScript("parseInt(browserSignals.renderURL.slice(-1))"));
  auto seller_worklet = CreateWorklet();

  const size_t kNumWorklets = 10;
  size_t num_completed_worklets = 0;
  base::RunLoop run_loop;
  for (size_t i = 0; i < kNumWorklets; ++i) {
    browser_signal_render_url_ = GURL(base::StringPrintf("https://foo/%zu", i));
    RunScoreAdOnWorkletAsync(seller_worklet.get(), /*expected_score=*/i,
                             /*expected_errors=*/std::vector<std::string>(),
                             mojom::ComponentAuctionModifiedBidParamsPtr(),
                             /*expected_data_version=*/absl::nullopt,
                             /*expected_debug_loss_report_url=*/absl::nullopt,
                             /*expected_debug_win_report_url=*/absl::nullopt,
                             /*expected_reject_reason=*/
                             mojom::RejectReason::kNotAvailable,
                             /*expected_pa_requests=*/{},
                             /*expected_bid_in_seller_currency=*/absl::nullopt,
                             /*expected_score_ad_timeout=*/false,
                             /*expected_signals_fetch_latency=*/absl::nullopt,
                             /*expected_code_ready_latency=*/absl::nullopt,
                             base::BindLambdaForTesting([&]() {
                               ++num_completed_worklets;
                               if (num_completed_worklets == kNumWorklets) {
                                 run_loop.Quit();
                               }
                             }));
  }
  run_loop.Run();
}

// Test the case of a bunch of ScoreAd() calls in parallel, all started before
// the worklet script fails to load.
TEST_F(SellerWorkletTest, ScoreAdParallelLoadFails) {
  auto seller_worklet = CreateWorklet();

  for (size_t i = 0; i < 10; ++i) {
    browser_signal_render_url_ = GURL(base::StringPrintf("https://foo/%zu", i));
    RunScoreAdOnWorkletExpectingCallbackNeverInvoked(seller_worklet.get());
  }

  // No calls should complete, since the script hasn't loaded yet.
  task_environment_.RunUntilIdle();

  // Script fails to load.
  url_loader_factory_.AddResponse(decision_logic_url_.spec(),
                                  /*content=*/std::string(),
                                  net::HTTP_NOT_FOUND);

  // The worklet should fail to load.
  EXPECT_EQ("Failed to load https://url.test/ HTTP status = 404 Not Found.",
            WaitForDisconnect());
  // The worklet script callbacks should not be invoked.
  task_environment_.RunUntilIdle();
}

// Test the case of a bunch of ScoreAd() calls in parallel, in the case trusted
// scoring signals is non-null. In this case, call AllBidsGenerated() between
// scoring each bid, which should result in requests being sent individually.
TEST_F(SellerWorkletTest, ScoreAdParallelTrustedScoringSignalsNotBatched) {
  base::Time start_time = base::Time::Now();

  // Seller script that gets the score from the `trustedScoringSignals` value of
  // the passed in `renderURL`.
  AddJavascriptResponse(
      &url_loader_factory_, decision_logic_url_,
      CreateScoreAdScript(
          "trustedScoringSignals.renderURL[browserSignals.renderURL]"));
  trusted_scoring_signals_url_ =
      GURL("https://url.test/trusted_scoring_signals");
  auto seller_worklet = CreateWorklet();

  // Start scoring a bunch of worklets. Don't provide JSON responses, to make
  // sure they all reside in the worklet's task list at once.
  const size_t kNumWorklets = 10;
  size_t num_completed_worklets = 0;
  base::RunLoop run_loop;
  for (size_t i = 0; i < kNumWorklets; ++i) {
    browser_signal_render_url_ = GURL(base::StringPrintf("https://foo/%zu", i));
    RunScoreAdOnWorkletAsync(seller_worklet.get(), /*expected_score=*/2 * i,
                             /*expected_errors=*/std::vector<std::string>(),
                             mojom::ComponentAuctionModifiedBidParamsPtr(),
                             /*expected_data_version=*/i,
                             /*expected_debug_loss_report_url=*/absl::nullopt,
                             /*expected_debug_win_report_url=*/absl::nullopt,
                             /*expected_reject_reason=*/
                             mojom::RejectReason::kNotAvailable,
                             /*expected_pa_requests=*/{},
                             /*expected_bid_in_seller_currency=*/absl::nullopt,
                             /*expected_score_ad_timeout=*/false,
                             /*expected_signals_fetch_latency=*/absl::nullopt,
                             /*expected_code_ready_latency=*/absl::nullopt,
                             base::BindLambdaForTesting([&]() {
                               ++num_completed_worklets;
                               if (num_completed_worklets == kNumWorklets) {
                                 run_loop.Quit();
                               }
                             }));
    seller_worklet->SendPendingSignalsRequests();
  }

  // Spin run loop so all requests reach the scoring worklet.
  run_loop.RunUntilIdle();
  EXPECT_EQ(0u, num_completed_worklets);

  // Provide all JSON responses.
  for (size_t i = 0; i < kNumWorklets; ++i) {
    GURL trusted_scoring_signals = GURL(base::StringPrintf(
        "%s?hostname=%s&renderUrls=https%%3A%%2F%%2Ffoo%%2F%zu",
        trusted_scoring_signals_url_->spec().c_str(),
        top_window_origin_.host().c_str(), i));
    std::string response_body = base::StringPrintf(
        R"({"renderUrls": {"https://foo/%zu": %zu}})", i, 2 * i);
    AddVersionedJsonResponse(&url_loader_factory_, trusted_scoring_signals,
                             response_body, /*data_version=*/i);
  }
  run_loop.Run();

  // No time should have passed during this test, since the
  // SendPendingSignalsRequests() calls ensure requests are send immediately,
  // without waiting on a timer. Using a mock time ensures that the passage of
  // wall clock time doesn't impact the current time, only delayed tasks and
  // timers do.
  EXPECT_EQ(base::Time::Now(), start_time);
}

// Test the case of a bunch of ScoreAd() calls in parallel, in the case trusted
// scoring signals is non-null. In this case, don't call AllBidsGenerated()
// between scoring each bid, which should result in all requests being sent as a
// single request.
//
// In this test, the ordering is:
// 1) The worklet script load completes.
// 2) ScoreAd() calls are made.
// 3) The trusted bidding signals are loaded.
TEST_F(SellerWorkletTest, ScoreAdParallelTrustedScoringSignalsBatched1) {
  // Seller script that gets the score from the `trustedScoringSignals` value of
  // the passed in `renderURL`.
  AddJavascriptResponse(
      &url_loader_factory_, decision_logic_url_,
      CreateScoreAdScript(
          "trustedScoringSignals.renderURL[browserSignals.renderURL]"));
  trusted_scoring_signals_url_ =
      GURL("https://url.test/trusted_scoring_signals");
  auto seller_worklet = CreateWorklet();

  // Start scoring a bunch of worklets. Don't provide JSON responses, to make
  // sure they all reside in the worklet's task list at once.
  const size_t kNumWorklets = 10;
  size_t num_completed_worklets = 0;
  base::RunLoop run_loop;
  for (size_t i = 0; i < kNumWorklets; ++i) {
    browser_signal_render_url_ = GURL(base::StringPrintf("https://foo/%zu", i));
    RunScoreAdOnWorkletAsync(seller_worklet.get(), /*expected_score=*/2 * i,
                             /*expected_errors=*/std::vector<std::string>(),
                             mojom::ComponentAuctionModifiedBidParamsPtr(),
                             /*expected_data_version=*/absl::nullopt,
                             /*expected_debug_loss_report_url=*/absl::nullopt,
                             /*expected_debug_win_report_url=*/absl::nullopt,
                             /*expected_reject_reason=*/
                             mojom::RejectReason::kNotAvailable,
                             /*expected_pa_requests=*/{},
                             /*expected_bid_in_seller_currency=*/absl::nullopt,
                             /*expected_score_ad_timeout=*/false,
                             /*expected_signals_fetch_latency=*/absl::nullopt,
                             /*expected_code_ready_latency=*/absl::nullopt,
                             base::BindLambdaForTesting([&]() {
                               ++num_completed_worklets;
                               if (num_completed_worklets == kNumWorklets) {
                                 run_loop.Quit();
                               }
                             }));
  }

  // Spin run loop so all requests reach the scoring worklet.
  run_loop.RunUntilIdle();
  EXPECT_EQ(0u, num_completed_worklets);

  // Provide a single response for the merged URL request.
  std::string request_url =
      base::StringPrintf("%s?hostname=%s&renderUrls=",
                         trusted_scoring_signals_url_->spec().c_str(),
                         top_window_origin_.host().c_str());
  std::string response_body;
  for (size_t i = 0; i < kNumWorklets; ++i) {
    if (i > 0) {
      request_url += ",";
      response_body += ",";
    }
    request_url += base::StringPrintf("https%%3A%%2F%%2Ffoo%%2F%zu", i);
    response_body += base::StringPrintf(R"("https://foo/%zu": %zu)", i, 2 * i);
  }
  response_body =
      base::StringPrintf(R"({"renderUrls": {%s}})", response_body.c_str());
  AddJsonResponse(&url_loader_factory_, GURL(request_url), response_body);

  // All ScoreAd() calls should succeed with the expected scores.
  run_loop.Run();
}

// Same as above, but with different ordering.
//
// In this test, the ordering is:
// 1) ScoreAd() calls are made.
// 2) The worklet script load completes.
// 3) The trusted bidding signals are loaded.
TEST_F(SellerWorkletTest, ScoreAdParallelTrustedScoringSignalsBatched2) {
  trusted_scoring_signals_url_ =
      GURL("https://url.test/trusted_scoring_signals");
  auto seller_worklet = CreateWorklet();

  // Start scoring a bunch of worklets. Don't provide JSON responses, to make
  // sure they all reside in the worklet's task list at once.
  const size_t kNumWorklets = 10;
  size_t num_completed_worklets = 0;
  base::RunLoop run_loop;
  for (size_t i = 0; i < kNumWorklets; ++i) {
    browser_signal_render_url_ = GURL(base::StringPrintf("https://foo/%zu", i));
    RunScoreAdOnWorkletAsync(seller_worklet.get(), /*expected_score=*/2 * i,
                             /*expected_errors=*/std::vector<std::string>(),
                             mojom::ComponentAuctionModifiedBidParamsPtr(),
                             /*expected_data_version=*/10,
                             /*expected_debug_loss_report_url=*/absl::nullopt,
                             /*expected_debug_win_report_url=*/absl::nullopt,
                             /*expected_reject_reason=*/
                             mojom::RejectReason::kNotAvailable,
                             /*expected_pa_requests=*/{},
                             /*expected_bid_in_seller_currency=*/absl::nullopt,
                             /*expected_score_ad_timeout=*/false,
                             /*expected_signals_fetch_latency=*/absl::nullopt,
                             /*expected_code_ready_latency=*/absl::nullopt,
                             base::BindLambdaForTesting([&]() {
                               ++num_completed_worklets;
                               if (num_completed_worklets == kNumWorklets) {
                                 run_loop.Quit();
                               }
                             }));
  }

  // Spin run loop so all requests reach the scoring worklet.
  run_loop.RunUntilIdle();
  EXPECT_EQ(0u, num_completed_worklets);

  // Return seller script that gets the score from the `trustedScoringSignals`
  // value of the passed in `renderURL`, and wait for it to finish loading.
  AddJavascriptResponse(
      &url_loader_factory_, decision_logic_url_,
      CreateScoreAdScript(
          "trustedScoringSignals.renderURL[browserSignals.renderURL]"));
  task_environment_.RunUntilIdle();

  // Provide a single response for the merged URL request.
  std::string request_url =
      base::StringPrintf("%s?hostname=%s&renderUrls=",
                         trusted_scoring_signals_url_->spec().c_str(),
                         top_window_origin_.host().c_str());
  std::string response_body;
  for (size_t i = 0; i < kNumWorklets; ++i) {
    if (i > 0) {
      request_url += ",";
      response_body += ",";
    }
    request_url += base::StringPrintf("https%%3A%%2F%%2Ffoo%%2F%zu", i);
    response_body += base::StringPrintf(R"("https://foo/%zu": %zu)", i, 2 * i);
  }
  response_body =
      base::StringPrintf(R"({"renderUrls": {%s}})", response_body.c_str());
  AddVersionedJsonResponse(&url_loader_factory_, GURL(request_url),
                           response_body, /*data_version=*/10);

  // All ScoreAd() calls should succeed with the expected scores.
  run_loop.Run();
}

// Same as above, but with different ordering.
//
// In this test, the ordering is:
// 1) ScoreAd() calls are made.
// 2) The trusted bidding signals are loaded.
// 3) The worklet script load completes.
TEST_F(SellerWorkletTest, ScoreAdParallelTrustedScoringSignalsBatched3) {
  trusted_scoring_signals_url_ =
      GURL("https://url.test/trusted_scoring_signals");
  auto seller_worklet = CreateWorklet();

  // Start scoring a bunch of worklets. Don't provide JSON responses, to make
  // sure they all reside in the worklet's task list at once.
  const size_t kNumWorklets = 10;
  size_t num_completed_worklets = 0;
  base::RunLoop run_loop;
  for (size_t i = 0; i < kNumWorklets; ++i) {
    browser_signal_render_url_ = GURL(base::StringPrintf("https://foo/%zu", i));
    RunScoreAdOnWorkletAsync(seller_worklet.get(), /*expected_score=*/2 * i,
                             /*expected_errors=*/std::vector<std::string>(),
                             mojom::ComponentAuctionModifiedBidParamsPtr(),
                             /*expected_data_version=*/10,
                             /*expected_debug_loss_report_url=*/absl::nullopt,
                             /*expected_debug_win_report_url=*/absl::nullopt,
                             /*expected_reject_reason=*/
                             mojom::RejectReason::kNotAvailable,
                             /*expected_pa_requests=*/{},
                             /*expected_bid_in_seller_currency=*/absl::nullopt,
                             /*expected_score_ad_timeout=*/false,
                             /*expected_signals_fetch_latency=*/absl::nullopt,
                             /*expected_code_ready_latency=*/absl::nullopt,
                             base::BindLambdaForTesting([&]() {
                               ++num_completed_worklets;
                               if (num_completed_worklets == kNumWorklets) {
                                 run_loop.Quit();
                               }
                             }));
  }

  // Spin run loop so all requests reach the scoring worklet.
  run_loop.RunUntilIdle();
  EXPECT_EQ(0u, num_completed_worklets);

  // Provide a single response for the merged URL request.
  std::string request_url =
      base::StringPrintf("%s?hostname=%s&renderUrls=",
                         trusted_scoring_signals_url_->spec().c_str(),
                         top_window_origin_.host().c_str());
  std::string response_body;
  for (size_t i = 0; i < kNumWorklets; ++i) {
    if (i > 0) {
      request_url += ",";
      response_body += ",";
    }
    request_url += base::StringPrintf("https%%3A%%2F%%2Ffoo%%2F%zu", i);
    response_body += base::StringPrintf(R"("https://foo/%zu": %zu)", i, 2 * i);
  }
  response_body =
      base::StringPrintf(R"({"renderUrls": {%s}})", response_body.c_str());
  AddVersionedJsonResponse(&url_loader_factory_, GURL(request_url),
                           response_body, /*data_version=*/10);

  // Spin run loop so the response is handled. No ScoreAdCalls should complete
  // yet.
  run_loop.RunUntilIdle();
  EXPECT_EQ(0u, num_completed_worklets);

  // Return seller script that gets the score from the `trustedScoringSignals`
  // value of the passed in `renderURL`, and wait for it to finish loading.
  AddJavascriptResponse(
      &url_loader_factory_, decision_logic_url_,
      CreateScoreAdScript(
          "trustedScoringSignals.renderURL[browserSignals.renderURL]"));

  // All ScoreAd() calls should succeed with the expected scores.
  run_loop.Run();
}

// It shouldn't matter the order in which network fetches complete. For each
// required and optional scoreAd() URL load prerequisite, ensure that
// scoreAd() completes when that URL is the last loaded URL.
TEST_F(SellerWorkletTest, ScoreAdLoadCompletionOrder) {
  constexpr char kJsonResponse[] = "{}";
  constexpr char kDirectFromSellerSignalsHeaders[] =
      "Ad-Auction-Allowed: true\nAd-Auction-Only: true";

  direct_from_seller_seller_signals_ = GURL("https://url.test/sellersignals");
  direct_from_seller_auction_signals_ = GURL("https://url.test/auctionsignals");
  trusted_scoring_signals_url_ = GURL("https://url.test/trustedsignals");

  struct Response {
    GURL response_url;
    std::string response_type;
    std::string headers;
    std::string content;
  };

  const Response kResponses[] = {
      {decision_logic_url_, kJavascriptMimeType, kAllowFledgeHeader,
       CreateScoreAdScript("1")},
      {*direct_from_seller_seller_signals_, kJsonMimeType,
       kDirectFromSellerSignalsHeaders, kJsonResponse},
      {*direct_from_seller_auction_signals_, kJsonMimeType,
       kDirectFromSellerSignalsHeaders, kJsonResponse},
      {GURL(trusted_scoring_signals_url_->spec() +
            "?hostname=window.test"
            "&renderUrls=https%3A%2F%2Frender.url.test%2F"),
       kJsonMimeType, kAllowFledgeHeader, kTrustedScoringSignalsResponse}};

  // Cycle such that each response in `kResponses` gets to be the last response,
  // like so:
  //
  // 0,1,2
  // 1,2,0
  // 2,0,1
  for (size_t offset = 0; offset < std::size(kResponses); ++offset) {
    SCOPED_TRACE(offset);
    mojo::Remote<mojom::SellerWorklet> seller_worklet = CreateWorklet();
    url_loader_factory_.ClearResponses();
    auto run_loop = std::make_unique<base::RunLoop>();
    RunScoreAdOnWorkletAsync(
        seller_worklet.get(), /*expected_score=*/1.0,
        /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
        /*expected_data_version=*/absl::nullopt,
        /*expected_debug_loss_report_url=*/absl::nullopt,
        /*expected_debug_win_report_url=*/absl::nullopt,
        /*expected_reject_reason=*/
        mojom::RejectReason::kNotAvailable,
        /*expected_pa_requests=*/{},
        /*expected_bid_in_seller_currency=*/absl::nullopt,
        /*expected_score_ad_timeout=*/false,
        /*expected_signals_fetch_latency=*/absl::nullopt,
        /*expected_code_ready_latency=*/absl::nullopt, run_loop->QuitClosure());
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

// If multiple worklets request DirectFromSellerSignals, they each get the
// correct signals.
TEST_F(SellerWorkletTest, ScoreAdDirectFromSellerSignalsMultipleWorklets) {
  constexpr char kWorklet1JsonResponse[] = R"({"worklet":1})";
  constexpr char kWorklet2JsonResponse[] = R"({"worklet":2})";
  constexpr char kWorklet1ExtraCode[] = R"(
const sellerSignalsJson =
    JSON.stringify(directFromSellerSignals.sellerSignals);
if (sellerSignalsJson !== '{"worklet":1}') {
  throw 'Wrong directFromSellerSignals.sellerSignals ' +
      sellerSignalsJson;
}
const auctionSignalsJson =
    JSON.stringify(directFromSellerSignals.auctionSignals);
if (auctionSignalsJson !== '{"worklet":1}') {
  throw 'Wrong directFromSellerSignals.auctionSignals ' +
      auctionSignalsJson;
}
)";
  constexpr char kWorklet2ExtraCode[] = R"(
const sellerSignalsJson =
    JSON.stringify(directFromSellerSignals.sellerSignals);
if (sellerSignalsJson !== '{"worklet":2}') {
  throw 'Wrong directFromSellerSignals.sellerSignals ' +
      sellerSignalsJson;
}
const auctionSignalsJson =
    JSON.stringify(directFromSellerSignals.auctionSignals);
if (auctionSignalsJson !== '{"worklet":2}') {
  throw 'Wrong directFromSellerSignals.auctionSignals ' +
      auctionSignalsJson;
}
)";
  constexpr char kDirectFromSellerSignalsHeaders[] =
      "Ad-Auction-Allowed: true\nAd-Auction-Only: true";

  direct_from_seller_seller_signals_ = GURL("https://url.test/sellersignals");
  direct_from_seller_auction_signals_ = GURL("https://url.test/auctionsignals");

  mojo::Remote<mojom::SellerWorklet> seller_worklet1 = CreateWorklet();
  AddResponse(&url_loader_factory_, *direct_from_seller_seller_signals_,
              kJsonMimeType, /*charset=*/absl::nullopt, kWorklet1JsonResponse,
              kDirectFromSellerSignalsHeaders);
  AddResponse(&url_loader_factory_, *direct_from_seller_auction_signals_,
              kJsonMimeType, /*charset=*/absl::nullopt, kWorklet1JsonResponse,
              kDirectFromSellerSignalsHeaders);
  AddJavascriptResponse(
      &url_loader_factory_, decision_logic_url_,
      CreateScoreAdScript("1", /*extra_code=*/kWorklet1ExtraCode));

  // For the second worklet, use a different `decision_logic_url_` (to set up
  // different expectations), but use the same DirectFromSellerSignals URLs.
  decision_logic_url_ = GURL("https://url2.test/");
  mojo::Remote<mojom::SellerWorklet> seller_worklet2 = CreateWorklet(
      /*pause_for_debugger_on_start=*/false,
      /*out_seller_worklet_impl=*/nullptr,
      /*use_alternate_url_loader_factory=*/true);
  AddResponse(&alternate_url_loader_factory_,
              *direct_from_seller_seller_signals_, kJsonMimeType,
              /*charset=*/absl::nullopt, kWorklet2JsonResponse,
              kDirectFromSellerSignalsHeaders);
  AddResponse(&alternate_url_loader_factory_,
              *direct_from_seller_auction_signals_, kJsonMimeType,
              /*charset=*/absl::nullopt, kWorklet2JsonResponse,
              kDirectFromSellerSignalsHeaders);
  AddJavascriptResponse(
      &alternate_url_loader_factory_, decision_logic_url_,
      CreateScoreAdScript("1", /*extra_code=*/kWorklet2ExtraCode));
  auto run_loop = std::make_unique<base::RunLoop>();
  RunScoreAdOnWorkletAsync(
      seller_worklet1.get(), /*expected_score=*/1.0,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_reject_reason=*/
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_bid_in_seller_currency=*/absl::nullopt,
      /*expected_score_ad_timeout=*/false,
      /*expected_signals_fetch_latency=*/absl::nullopt,
      /*expected_code_ready_latency=*/absl::nullopt, run_loop->QuitClosure());
  run_loop->Run();

  run_loop = std::make_unique<base::RunLoop>();
  RunScoreAdOnWorkletAsync(
      seller_worklet2.get(), /*expected_score=*/1.0,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_reject_reason=*/
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_bid_in_seller_currency=*/absl::nullopt,
      /*expected_score_ad_timeout=*/false,
      /*expected_signals_fetch_latency=*/absl::nullopt,
      /*expected_code_ready_latency=*/absl::nullopt, run_loop->QuitClosure());
  run_loop->Run();
}

// Test multiple ReportWin() calls on a single worklet, in parallel. Do this
// twice, once before the worklet has loaded its Javascript, and once after, to
// make sure both cases work.
TEST_F(SellerWorkletTest, ReportResultParallel) {
  auto seller_worklet = CreateWorklet();

  // For the first loop iteration, call ReportResult() repeatedly before
  // providing the seller script, then provide the seller script. For the second
  // loop iteration, reuse the seller worklet from the first iteration, so the
  // Javascript is loaded from the start.
  for (bool report_result_invoked_before_worklet_script_loaded :
       {false, true}) {
    SCOPED_TRACE(report_result_invoked_before_worklet_script_loaded);

    base::RunLoop run_loop;
    const size_t kNumReportResultCalls = 10;
    size_t num_report_result_calls = 0;
    for (size_t i = 0; i < kNumReportResultCalls; ++i) {
      // Differentiate each call based on the bid.
      bid_ = i + 1;
      RunReportResultExpectingResultAsync(
          seller_worklet.get(),
          /*expected_signals_for_winner=*/base::NumberToString(bid_),
          /*expected_report_url=*/
          GURL("https://" + base::NumberToString(bid_)),
          /*expected_ad_beacon_map=*/{},
          /*expected_pa_requests=*/{},
          /*expected_reporting_latency_timeout=*/false,
          /*expected_errors=*/{},
          base::BindLambdaForTesting([&run_loop, &num_report_result_calls]() {
            ++num_report_result_calls;
            if (num_report_result_calls == kNumReportResultCalls) {
              run_loop.Quit();
            }
          }));
    }

    // If this is the first loop iteration, wait for all the Mojo calls to
    // settle, and then provide the Javascript response body.
    if (report_result_invoked_before_worklet_script_loaded == false) {
      task_environment_.RunUntilIdle();
      EXPECT_FALSE(run_loop.AnyQuitCalled());
      AddJavascriptResponse(
          &url_loader_factory_, decision_logic_url_,
          CreateReportToScript(
              /*raw_return_value=*/"browserSignals.bid",
              /*extra_code=*/
              R"(sendReportTo("https://" + browserSignals.bid))"));
    }

    run_loop.Run();
    EXPECT_EQ(kNumReportResultCalls, num_report_result_calls);
  }
}

// Test multiple ReportResult() calls on a single worklet, in parallel, in the
// case the worklet script fails to load.
TEST_F(SellerWorkletTest, ReportResultParallelLoadFails) {
  auto seller_worklet = CreateWorklet();

  for (size_t i = 0; i < 10; ++i) {
    RunReportResultExpectingCallbackNeverInvoked(seller_worklet.get());
  }

  url_loader_factory_.AddResponse(decision_logic_url_.spec(), "Response body",
                                  net::HTTP_NOT_FOUND);

  EXPECT_EQ("Failed to load https://url.test/ HTTP status = 404 Not Found.",
            WaitForDisconnect());
}

// Tests parsing of return values.
TEST_F(SellerWorkletTest, ReportResult) {
  RunReportResultCreatedScriptExpectingResult(
      "1", /*extra_code=*/std::string(),
      /*expected_signals_for_winner=*/"1",
      /*expected_report_url=*/absl::nullopt);
  RunReportResultCreatedScriptExpectingResult(
      R"("  1   ")", /*extra_code=*/std::string(),
      /*expected_signals_for_winner=*/R"("  1   ")",
      /*expected_report_url=*/absl::nullopt);
  RunReportResultCreatedScriptExpectingResult(
      "[ null ]", /*extra_code=*/std::string(), "[null]",
      /*expected_report_url=*/absl::nullopt);

  // No return value.
  RunReportResultCreatedScriptExpectingResult(
      "", /*extra_code=*/std::string(), "null",
      /*expected_report_url=*/absl::nullopt);

  // Throw exception.
  RunReportResultCreatedScriptExpectingResult(
      "shrimp", /*extra_code=*/std::string(),
      /*expected_signals_for_winner=*/absl::nullopt,
      /*expected_report_url=*/absl::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:11 Uncaught ReferenceError: "
       "shrimp is not defined."});
}

// Tests reporting URLs.
TEST_F(SellerWorkletTest, ReportResultSendReportTo) {
  RunReportResultCreatedScriptExpectingResult(
      "1", R"(sendReportTo("https://foo.test"))",
      /*expected_signals_for_winner=*/"1", GURL("https://foo.test/"));
  RunReportResultCreatedScriptExpectingResult(
      "1", R"(sendReportTo("https://foo.test/bar"))",
      /*expected_signals_for_winner=*/"1", GURL("https://foo.test/bar"));

  // Disallowed schemes.
  RunReportResultCreatedScriptExpectingResult(
      "1", R"(sendReportTo("http://foo.test/"))",
      /*expected_signals_for_winner=*/absl::nullopt,
      /*expected_report_url=*/absl::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:10 Uncaught TypeError: "
       "sendReportTo must be passed a valid HTTPS url."});
  RunReportResultCreatedScriptExpectingResult(
      "1", R"(sendReportTo("file:///foo/"))",
      /*expected_signals_for_winner=*/absl::nullopt,
      /*expected_report_url=*/absl::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:10 Uncaught TypeError: "
       "sendReportTo must be passed a valid HTTPS url."});

  // Multiple calls.
  RunReportResultCreatedScriptExpectingResult(
      "1",
      R"(sendReportTo("https://foo.test/"); sendReportTo("https://foo.test/"))",
      /*expected_signals_for_winner=*/absl::nullopt,
      /*expected_report_url=*/absl::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:10 Uncaught TypeError: "
       "sendReportTo may be called at most once."});

  // No message if caught, but still no URL.
  RunReportResultCreatedScriptExpectingResult(
      "1",
      R"(try {
        sendReportTo("https://foo.test/");
        sendReportTo("https://foo.test/")} catch(e) {})",
      /*expected_signals_for_winner=*/"1",
      /*expected_report_url=*/absl::nullopt);

  // Not a URL.
  RunReportResultCreatedScriptExpectingResult(
      "1", R"(sendReportTo("France"))",
      /*expected_signals_for_winner=*/absl::nullopt,
      /*expected_report_url=*/absl::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:10 Uncaught TypeError: "
       "sendReportTo must be passed a valid HTTPS url."});
  RunReportResultCreatedScriptExpectingResult(
      "1", R"(sendReportTo(null))",
      /*expected_signals_for_winner=*/absl::nullopt,
      /*expected_report_url=*/absl::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:10 Uncaught TypeError: "
       "sendReportTo must be passed a valid HTTPS url."});
  RunReportResultCreatedScriptExpectingResult(
      "1", R"(sendReportTo([5]))",
      /*expected_signals_for_winner=*/absl::nullopt,
      /*expected_report_url=*/absl::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:10 Uncaught TypeError: "
       "sendReportTo must be passed a valid HTTPS url."});
}

TEST_F(SellerWorkletTest, ReportResultDateNotAvailable) {
  RunReportResultCreatedScriptExpectingResult(
      "1", R"(sendReportTo("https://foo.test/" + Date().toString()))",
      /*expected_signals_for_winner=*/absl::nullopt,
      /*expected_report_url=*/absl::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:10 Uncaught ReferenceError: Date is not defined."});
}

TEST_F(SellerWorkletTest, ReportResultTopWindowOrigin) {
  top_window_origin_ = url::Origin::Create(GURL("https://foo.test/"));
  RunReportResultCreatedScriptExpectingResult(
      R"(browserSignals.topWindowHostname == "foo.test" ? 2 : 1)",
      /*extra_code=*/std::string(), "2",
      /*expected_report_url=*/absl::nullopt);

  top_window_origin_ = url::Origin::Create(GURL("https://[::1]:40000/"));
  RunReportResultCreatedScriptExpectingResult(
      R"(browserSignals.topWindowHostname == "[::1]" ? 3 : 1)",
      /*extra_code=*/std::string(), "3",
      /*expected_report_url=*/absl::nullopt);
}

TEST_F(SellerWorkletTest, ReportResultTopLevelSeller) {
  browser_signals_other_seller_.reset();
  RunReportResultCreatedScriptExpectingResult(
      R"("topLevelSeller" in browserSignals ? 0 : 1)",
      /*extra_code=*/std::string(), /*expected_signals_for_winner=*/"1",
      /*expected_report_url=*/absl::nullopt);

  browser_signals_other_seller_ =
      mojom::ComponentAuctionOtherSeller::NewTopLevelSeller(
          url::Origin::Create(GURL("https://top.seller.test")));
  browser_signals_component_auction_report_result_params_ =
      mojom::ComponentAuctionReportResultParams::New(
          /*top_level_seller_signals=*/"null",
          /*modified_bid=*/0,
          /*has_modified_bid=*/false);
  RunReportResultCreatedScriptExpectingResult(
      R"(browserSignals.topLevelSeller === "https://top.seller.test" ? 2 : 0)",
      /*extra_code=*/std::string(), /*expected_signals_for_winner=*/"2",
      /*expected_report_url=*/absl::nullopt);
  browser_signals_component_auction_report_result_params_.reset();

  browser_signals_other_seller_ =
      mojom::ComponentAuctionOtherSeller::NewComponentSeller(
          url::Origin::Create(GURL("https://component.test")));
  RunReportResultCreatedScriptExpectingResult(
      R"("topLevelSeller" in browserSignals ? 0 : 3)",
      /*extra_code=*/std::string(), /*expected_signals_for_winner=*/"3",
      /*expected_report_url=*/absl::nullopt);
}

TEST_F(SellerWorkletTest, ReportResultComponentSeller) {
  browser_signals_other_seller_.reset();
  RunReportResultCreatedScriptExpectingResult(
      R"("componentSeller" in browserSignals ? 0 : 1)",
      /*extra_code=*/std::string(), /*expected_signals_for_winner=*/"1",
      /*expected_report_url=*/absl::nullopt);

  browser_signals_other_seller_ =
      mojom::ComponentAuctionOtherSeller::NewTopLevelSeller(
          url::Origin::Create(GURL("https://top.seller.test")));
  browser_signals_component_auction_report_result_params_ =
      mojom::ComponentAuctionReportResultParams::New(
          /*top_level_seller_signals=*/"null",
          /*modified_bid=*/0,
          /*has_modified_bid=*/false);
  RunReportResultCreatedScriptExpectingResult(
      R"("componentSeller" in browserSignals ? 0 : 2)",
      /*extra_code=*/std::string(), /*expected_signals_for_winner=*/"2",
      /*expected_report_url=*/absl::nullopt);
  browser_signals_component_auction_report_result_params_.reset();

  browser_signals_other_seller_ =
      mojom::ComponentAuctionOtherSeller::NewComponentSeller(
          url::Origin::Create(GURL("https://component.test")));
  RunReportResultCreatedScriptExpectingResult(
      R"(browserSignals.componentSeller === "https://component.test" ? 3 : 0)",
      /*extra_code=*/std::string(), /*expected_signals_for_winner=*/"3",
      /*expected_report_url=*/absl::nullopt);
}

TEST_F(SellerWorkletTest, ReportResultTopLevelSellerSignals) {
  // Top-level auctions should never be passed a `topLevelSellerSignals` field,
  // whether ReportResult() is invoked with a bid from a component auction or
  // the top-level auction.

  browser_signals_other_seller_.reset();
  RunReportResultCreatedScriptExpectingResult(
      "'topLevelSellerSignals' in browserSignals ? 0 : 1",
      /*extra_code=*/std::string(), /*expected_signals_for_winner=*/"1",
      /*expected_report_url=*/absl::nullopt);

  browser_signals_other_seller_ =
      mojom::ComponentAuctionOtherSeller::NewComponentSeller(
          url::Origin::Create(GURL("https://component.test")));
  RunReportResultCreatedScriptExpectingResult(
      "'topLevelSellerSignals' in browserSignals ? 0 : 2",
      /*extra_code=*/std::string(), /*expected_signals_for_winner=*/"2",
      /*expected_report_url=*/absl::nullopt);

  // Component auctions should take `topLevelSellerSignals` from the
  // ComponentAuctionReportResultParams argument.
  browser_signals_other_seller_ =
      mojom::ComponentAuctionOtherSeller::NewTopLevelSeller(
          url::Origin::Create(GURL("https://top.seller.test")));
  browser_signals_component_auction_report_result_params_ =
      mojom::ComponentAuctionReportResultParams::New(
          /*top_level_seller_signals=*/"null",
          /*modified_bid=*/0,
          /*has_modified_bid=*/false);
  RunReportResultCreatedScriptExpectingResult(
      "browserSignals.topLevelSellerSignals === null ? 3 : 0",
      /*extra_code=*/std::string(), /*expected_signals_for_winner=*/"3",
      /*expected_report_url=*/absl::nullopt);

  browser_signals_component_auction_report_result_params_
      ->top_level_seller_signals = "[4]";
  RunReportResultCreatedScriptExpectingResult(
      "browserSignals.topLevelSellerSignals[0]",
      /*extra_code=*/std::string(), /*expected_signals_for_winner=*/"4",
      /*expected_report_url=*/absl::nullopt);
}

TEST_F(SellerWorkletTest, ReportResultModifiedBid) {
  // Top-level auctions should never be passed a `modifiedBid` field, whether
  // ReportResult() is invoked with a bit from a component auction or the
  // top-level auction.

  browser_signals_other_seller_.reset();
  RunReportResultCreatedScriptExpectingResult(
      "'modifiedBid' in browserSignals ? 0 : 1",
      /*extra_code=*/std::string(), /*expected_signals_for_winner=*/"1",
      /*expected_report_url=*/absl::nullopt);

  browser_signals_other_seller_ =
      mojom::ComponentAuctionOtherSeller::NewComponentSeller(
          url::Origin::Create(GURL("https://component.test")));
  RunReportResultCreatedScriptExpectingResult(
      "'modifiedBid' in browserSignals ? 0 : 2",
      /*extra_code=*/std::string(), /*expected_signals_for_winner=*/"2",
      /*expected_report_url=*/absl::nullopt);

  // Component auctions should only receive a `modifiedBid` field when
  // `has_modified_bid` is true.
  browser_signals_other_seller_ =
      mojom::ComponentAuctionOtherSeller::NewTopLevelSeller(
          url::Origin::Create(GURL("https://top.seller.test")));
  browser_signals_component_auction_report_result_params_ =
      mojom::ComponentAuctionReportResultParams::New(
          /*top_level_seller_signals=*/"null",
          /*modified_bid=*/4,
          /*has_modified_bid=*/true);
  RunReportResultCreatedScriptExpectingResult(
      "browserSignals.modifiedBid",
      /*extra_code=*/std::string(), /*expected_signals_for_winner=*/"4",
      /*expected_report_url=*/absl::nullopt);
  browser_signals_component_auction_report_result_params_->has_modified_bid =
      false;
  RunReportResultCreatedScriptExpectingResult(
      "'modifiedBid' in browserSignals ? 0 : 3",
      /*extra_code=*/std::string(), /*expected_signals_for_winner=*/"3",
      /*expected_report_url=*/absl::nullopt);
}

TEST_F(SellerWorkletTest, ReportResultInterestGroupOwner) {
  browser_signal_interest_group_owner_ =
      url::Origin::Create(GURL("https://foo.test/"));
  RunReportResultCreatedScriptExpectingResult(
      R"(browserSignals.interestGroupOwner == "https://foo.test" ? 2 : 1)",
      /*extra_code=*/std::string(), "2",
      /*expected_report_url=*/absl::nullopt);

  browser_signal_interest_group_owner_ =
      url::Origin::Create(GURL("https://[::1]:40000/"));
  RunReportResultCreatedScriptExpectingResult(
      R"(browserSignals.interestGroupOwner == "https://[::1]:40000" ? 3 : 1)",
      /*extra_code=*/std::string(), "3",
      /*expected_report_url=*/absl::nullopt);
}

TEST_F(SellerWorkletTest, ReportResultBuyerAndSellerReportingId) {
  browser_signal_buyer_and_seller_reporting_id_ = "campaign";
  RunReportResultCreatedScriptExpectingResult(
      R"(browserSignals.buyerAndSellerReportingId === "campaign" ? 2 : 1)",
      /*extra_code=*/std::string(), "2",
      /*expected_report_url=*/absl::nullopt);
}

TEST_F(SellerWorkletTest, ReportResultRenderUrl) {
  browser_signal_render_url_ = GURL("https://foo/");
  RunReportResultCreatedScriptExpectingResult(
      "browserSignals.renderURL", "sendReportTo(browserSignals.renderURL)",
      R"("https://foo/")", browser_signal_render_url_);
}

TEST_F(SellerWorkletTest, ReportResultRegisterAdBeacon) {
  bid_ = 5;
  base::flat_map<std::string, GURL> expected_ad_beacon_map = {
      {"click", GURL("https://click.example.com/")},
      {"view", GURL("https://view.example.com/")},
  };
  RunReportResultCreatedScriptExpectingResult(
      R"(5)",
      R"(registerAdBeacon({
        'click': "https://click.example.com/",
        'view': "https://view.example.com/",
      }))",
      /*expected_signals_for_winner=*/"5",
      /*expected_report_url=*/absl::nullopt, expected_ad_beacon_map);

  browser_signal_render_url_ = GURL("https://foo/");
  RunReportResultCreatedScriptExpectingResult(
      R"(5)",
      R"(registerAdBeacon({
        'click': "https://click.example.com/",
        'view': "https://view.example.com/",
      });
      sendReportTo(browserSignals.renderURL))",
      /*expected_signals_for_winner=*/"5",
      /*expected_report_url=*/browser_signal_render_url_,
      expected_ad_beacon_map);

  RunReportResultCreatedScriptExpectingResult(
      R"(5)",
      R"(sendReportTo(browserSignals.renderURL);
      registerAdBeacon({
        'click': "https://click.example.com/",
        'view': "https://view.example.com/",
      }))",
      /*expected_signals_for_winner=*/"5",
      /*expected_report_url=*/browser_signal_render_url_,
      expected_ad_beacon_map);

  // Don't call twice.
  RunReportResultCreatedScriptExpectingResult(
      R"(5)",
      R"(registerAdBeacon({
        'click': "https://click.example.com/",
        'view': "https://view.example.com/",
      });
      registerAdBeacon())",
      /*expected_signals_for_winner=*/{},
      /*expected_report_url=*/absl::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:14 Uncaught TypeError: registerAdBeacon may be "
       "called at most once."});

  // If called twice and the error is caught, use the first result.
  RunReportResultCreatedScriptExpectingResult(
      R"(5)",
      R"(registerAdBeacon({
           'click': "https://click.example.com/",
           'view': "https://view.example.com/",
         });
         try { registerAdBeacon() }
         catch (e) {})",
      /*expected_signals_for_winner=*/"5",
      /*expected_report_url=*/absl::nullopt, expected_ad_beacon_map);

  // If error on first call, can be called again.
  RunReportResultCreatedScriptExpectingResult(
      R"(5)",
      R"(try { registerAdBeacon() }
         catch (e) {}
         registerAdBeacon({
           'click': "https://click.example.com/",
           'view': "https://view.example.com/",
         }))",
      /*expected_signals_for_winner=*/"5",
      /*expected_report_url=*/absl::nullopt, expected_ad_beacon_map);

  // Error if no parameters
  RunReportResultCreatedScriptExpectingResult(
      R"(5)", R"(registerAdBeacon())",
      /*expected_signals_for_winner=*/{},
      /*expected_report_url=*/absl::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:10 Uncaught TypeError: registerAdBeacon(): at least "
       "1 argument(s) are required."});

  // Error if parameter is not an object
  RunReportResultCreatedScriptExpectingResult(
      R"(5)", R"(registerAdBeacon("foo"))",
      /*expected_signals_for_winner=*/{},
      /*expected_report_url=*/absl::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:10 Uncaught TypeError: registerAdBeacon(): Cannot "
       "convert argument 'map' to a record since it's not an Object."});

  // Generally OK if parameter attributes are not strings
  RunReportResultCreatedScriptExpectingResult(
      R"(5)",
      R"(registerAdBeacon({
        'click': "https://click.example.com/",
        1: "https://view.example.com/",
      }))",
      /*expected_signals_for_winner=*/"5",
      /*expected_report_url=*/absl::nullopt,
      /*expected_ad_beacon_map=*/
      {
          {"click", GURL("https://click.example.com/")},
          {"1", GURL("https://view.example.com/")},
      },
      /*expected_pa_requests=*/{}, {});

  // ... but keys must be convertible to strings
  RunReportResultCreatedScriptExpectingResult(
      R"(5)",
      R"(let map = {
           'click': "https://click.example.com/"
         }
         map[Symbol('a')] = "https://view.example.com/";
         registerAdBeacon(map))",
      /*expected_signals_for_winner=*/{},
      /*expected_report_url=*/absl::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:14 Uncaught TypeError: Cannot convert a Symbol value "
       "to a string."});

  // Error if invalid reporting URL
  RunReportResultCreatedScriptExpectingResult(
      R"(5)",
      R"(registerAdBeacon({
        'click': "https://click.example.com/",
        'view': "gopher://view.example.com/",
      }))",
      /*expected_signals_for_winner=*/{},
      /*expected_report_url=*/absl::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:10 Uncaught TypeError: registerAdBeacon(): invalid "
       "reporting url for key 'view': 'gopher://view.example.com/'."});

  // Error if not trustworthy reporting URL
  RunReportResultCreatedScriptExpectingResult(
      R"(5)",
      R"(registerAdBeacon({
        'click': "https://127.0.0.1/",
        'view': "http://view.example.com/",
      }))",
      /*expected_signals_for_winner=*/{},
      /*expected_report_url=*/absl::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:10 Uncaught TypeError: registerAdBeacon(): invalid "
       "reporting url for key 'view': 'http://view.example.com/'."});

  // Special case for error message if the key has mismatched surrogates.
  RunReportResultCreatedScriptExpectingResult(
      R"(5)",
      R"(registerAdBeacon({
         '\ud835': "http://127.0.0.1/",
      }))",
      /*expected_signals_for_winner=*/{},
      /*expected_report_url=*/absl::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:10 Uncaught TypeError: registerAdBeacon(): invalid "
       "reporting url."});
}

TEST_F(SellerWorkletTest, ReportResultBid) {
  bid_ = 5;
  RunReportResultCreatedScriptExpectingResult(
      "browserSignals.bid + typeof browserSignals.bid",
      /*extra_code=*/std::string(), R"("5number")",
      /*expected_report_url=*/absl::nullopt);
}

TEST_F(SellerWorkletTest, ReportResultBidCurrency) {
  bid_currency_ = blink::AdCurrency::From("EUR");
  RunReportResultCreatedScriptExpectingResult(
      "browserSignals.bidCurrency + typeof browserSignals.bidCurrency",
      /*extra_code=*/std::string(), R"("EURstring")",
      /*expected_report_url=*/absl::nullopt);
}

TEST_F(SellerWorkletTest, ReportResultDesireability) {
  browser_signal_desireability_ = 10;
  RunReportResultCreatedScriptExpectingResult(
      "browserSignals.desirability + typeof browserSignals.desirability",
      /*extra_code=*/std::string(), R"("10number")",
      /*expected_report_url=*/absl::nullopt);
}

TEST_F(SellerWorkletTest, ReportResultHighestScoringOtherBid) {
  browser_signal_highest_scoring_other_bid_ = 5;
  RunReportResultCreatedScriptExpectingResult(
      "browserSignals.highestScoringOtherBid + typeof "
      "browserSignals.highestScoringOtherBid",
      /*extra_code=*/std::string(), R"("5number")",
      /*expected_report_url=*/absl::nullopt);
}

TEST_F(SellerWorkletTest, ReportResultHighestScoringOtherBidCurrency) {
  browser_signal_highest_scoring_other_bid_currency_ =
      blink::AdCurrency::From("EUR");
  RunReportResultCreatedScriptExpectingResult(
      "browserSignals.highestScoringOtherBidCurrency + typeof "
      "browserSignals.highestScoringOtherBidCurrency",
      /*extra_code=*/std::string(), R"("EURstring")",
      /*expected_report_url=*/absl::nullopt);
}

TEST_F(SellerWorkletTest, ReportResultAuctionConfigParam) {
  // Empty AuctionAdConfig, with nothing filled in, except the seller and
  // decision logic URL.
  decision_logic_url_ = GURL("https://example.com/auction.js");
  RunReportResultCreatedScriptExpectingResult(
      "auctionConfig", /*extra_code=*/std::string(),
      R"({"seller":"https://example.com",)"
      R"("decisionLogicURL":"https://example.com/auction.js",)"
      R"("decisionLogicUrl":"https://example.com/auction.js"})",
      /*expected_report_url=*/absl::nullopt);

  // Everything filled in but component auctions (can't include component
  // auctions and non-empty interestGroupBuyers, so test those cases
  // separately).
  decision_logic_url_ = GURL("https://example.com/auction.js");
  trusted_scoring_signals_url_ =
      GURL("https://example.com/scoring_signals.json");
  auction_ad_config_non_shared_params_.interest_group_buyers = {
      url::Origin::Create(GURL("https://buyer1.com")),
      url::Origin::Create(GURL("https://another-buyer.com"))};
  auction_ad_config_non_shared_params_.auction_signals =
      blink::AuctionConfig::MaybePromiseJson::FromValue(
          R"({"is_auction_signals": true})");
  auction_ad_config_non_shared_params_.seller_signals =
      blink::AuctionConfig::MaybePromiseJson::FromValue(
          R"({"is_seller_signals": true})");
  auction_ad_config_non_shared_params_.seller_timeout = base::Milliseconds(200);
  base::flat_map<url::Origin, std::string> per_buyer_signals;
  per_buyer_signals[url::Origin::Create(GURL("https://a.com"))] =
      R"({"signals_a": "A"})";
  per_buyer_signals[url::Origin::Create(GURL("https://b.com"))] =
      R"({"signals_b": "B"})";
  auction_ad_config_non_shared_params_.per_buyer_signals =
      blink::AuctionConfig::MaybePromisePerBuyerSignals::FromValue(
          std::move(per_buyer_signals));

  blink::AuctionConfig::BuyerTimeouts buyer_timeouts;
  buyer_timeouts.per_buyer_timeouts.emplace();
  buyer_timeouts.per_buyer_timeouts
      .value()[url::Origin::Create(GURL("https://a.com"))] =
      base::Milliseconds(100);
  buyer_timeouts.all_buyers_timeout = base::Milliseconds(150);
  auction_ad_config_non_shared_params_.buyer_timeouts =
      blink::AuctionConfig::MaybePromiseBuyerTimeouts::FromValue(
          std::move(buyer_timeouts));

  blink::AuctionConfig::BuyerTimeouts buyer_cumulative_timeouts;
  buyer_cumulative_timeouts.per_buyer_timeouts.emplace();
  buyer_cumulative_timeouts.per_buyer_timeouts
      .value()[url::Origin::Create(GURL("https://a.com"))] =
      base::Milliseconds(101);
  buyer_cumulative_timeouts.all_buyers_timeout = base::Milliseconds(151);
  auction_ad_config_non_shared_params_.buyer_cumulative_timeouts =
      blink::AuctionConfig::MaybePromiseBuyerTimeouts::FromValue(
          std::move(buyer_cumulative_timeouts));

  blink::AuctionConfig::BuyerCurrencies buyer_currencies;
  buyer_currencies.per_buyer_currencies.emplace();
  buyer_currencies.per_buyer_currencies
      .value()[url::Origin::Create(GURL("https://example.ca"))] =
      blink::AdCurrency::From("CAD");
  buyer_currencies.all_buyers_currency = blink::AdCurrency::From("USD");
  auction_ad_config_non_shared_params_.buyer_currencies =
      blink::AuctionConfig::MaybePromiseBuyerCurrencies::FromValue(
          std::move(buyer_currencies));

  auction_ad_config_non_shared_params_.per_buyer_priority_signals = {
      {url::Origin::Create(GURL("https://a.com")), {{"signals_c", 0.5}}}};
  auction_ad_config_non_shared_params_.all_buyers_priority_signals = {
      {"signals_d", 0}};

  const char kExpectedJson1[] =
      R"({"seller":"https://example.com",
          "decisionLogicURL":"https://example.com/auction.js",
          "decisionLogicUrl":"https://example.com/auction.js",
          "trustedScoringSignalsURL":"https://example.com/scoring_signals.json",
          "trustedScoringSignalsUrl":"https://example.com/scoring_signals.json",
          "interestGroupBuyers":["https://buyer1.com",
                                 "https://another-buyer.com"],
          "auctionSignals":{"is_auction_signals":true},
          "sellerSignals":{"is_seller_signals":true},
          "sellerTimeout":200,
          "perBuyerSignals":{"https://a.com":{"signals_a":"A"},
                             "https://b.com":{"signals_b":"B"}},
          "perBuyerCurrencies":{"*": "USD",
                                "https://example.ca": "CAD"},
          "perBuyerTimeouts":{"https://a.com":100,"*":150},
          "perBuyerCumulativeTimeouts":{"https://a.com":101,"*":151},
          "perBuyerPrioritySignals":{"https://a.com":{"signals_c":0.5},
                                     "*":            {"signals_d":0}}
        })";
  RunReportResultCreatedScriptExpectingResult(
      "auctionConfig", /*extra_code=*/std::string(), kExpectedJson1,
      /*expected_report_url=*/absl::nullopt);

  // Clear NonSharedParams(), and add and populate two component auctions, each
  // with one the mandatory `seller` and `decision_logic_url` fields filled in,
  // and one extra field: One that's directly a member of the AuctionAdConfig,
  // and one that's in the non-shared params.
  auction_ad_config_non_shared_params_ =
      blink::AuctionConfig::NonSharedParams();
  auto& component_auctions =
      auction_ad_config_non_shared_params_.component_auctions;

  component_auctions.emplace_back(blink::AuctionConfig());
  component_auctions[0].seller =
      url::Origin::Create(GURL("https://component1.com"));
  component_auctions[0].decision_logic_url =
      GURL("https://component1.com/script.js");
  component_auctions[0].non_shared_params.seller_timeout =
      base::Milliseconds(111);

  component_auctions.emplace_back(blink::AuctionConfig());
  component_auctions[1].seller =
      url::Origin::Create(GURL("https://component2.com"));
  component_auctions[1].decision_logic_url =
      GURL("https://component2.com/script.js");
  component_auctions[1].trusted_scoring_signals_url =
      GURL("https://component2.com/signals.json");

  const char kExpectedJson2[] =
      R"({"seller":"https://example.com",
          "decisionLogicURL":"https://example.com/auction.js",
          "decisionLogicUrl":"https://example.com/auction.js",
          "trustedScoringSignalsURL":"https://example.com/scoring_signals.json",
          "trustedScoringSignalsUrl":"https://example.com/scoring_signals.json",
          "componentAuctions":[
              {"seller":"https://component1.com",
               "decisionLogicURL":"https://component1.com/script.js",
               "decisionLogicUrl":"https://component1.com/script.js",
               "sellerTimeout":111},
              {"seller":"https://component2.com",
               "decisionLogicURL":"https://component2.com/script.js",
               "decisionLogicUrl":"https://component2.com/script.js",
               "trustedScoringSignalsURL":"https://component2.com/signals.json",
               "trustedScoringSignalsUrl":"https://component2.com/signals.json"}
          ]})";
  RunReportResultCreatedScriptExpectingResult(
      "auctionConfig", /*extra_code=*/std::string(), kExpectedJson2,
      /*expected_report_url=*/absl::nullopt);
}

TEST_F(SellerWorkletTest,
       ReportResultDirectFromSellerSignalsHeaderAdSlotParam) {
  direct_from_seller_auction_signals_header_ad_slot_ = R"("abcde")";
  direct_from_seller_seller_signals_header_ad_slot_ = R"("abcdefg")";

  const char kExpectedJson[] =
      R"({"auctionSignals":"abcde", "sellerSignals":"abcdefg"})";

  RunReportResultCreatedScriptExpectingResult(
      "directFromSellerSignals", /*extra_code=*/std::string(), kExpectedJson,
      /*expected_report_url=*/absl::nullopt);
}

TEST_F(SellerWorkletTest, ReportResultAuctionConfigParamPerBuyerTimeouts) {
  // Empty AuctionAdConfig, with nothing filled in, except the seller and
  // decision logic URL.
  decision_logic_url_ = GURL("https://example.com/auction.js");
  RunReportResultCreatedScriptExpectingResult(
      "auctionConfig", /*extra_code=*/std::string(),
      R"({"seller":"https://example.com",)"
      R"("decisionLogicURL":"https://example.com/auction.js",)"
      R"("decisionLogicUrl":"https://example.com/auction.js"})",
      /*expected_report_url=*/absl::nullopt);

  {
    blink::AuctionConfig::BuyerTimeouts buyer_timeouts;
    buyer_timeouts.per_buyer_timeouts.emplace();
    auction_ad_config_non_shared_params_.buyer_timeouts =
        blink::AuctionConfig::MaybePromiseBuyerTimeouts::FromValue(
            std::move(buyer_timeouts));

    RunReportResultCreatedScriptExpectingResult(
        "auctionConfig", /*extra_code=*/std::string(),
        R"({"seller":"https://example.com",)"
        R"("decisionLogicURL":"https://example.com/auction.js",)"
        R"("decisionLogicUrl":"https://example.com/auction.js",)"
        R"("perBuyerTimeouts":{}})",
        /*expected_report_url=*/absl::nullopt);
  }

  {
    blink::AuctionConfig::BuyerTimeouts buyer_timeouts;
    buyer_timeouts.per_buyer_timeouts.emplace();
    buyer_timeouts.all_buyers_timeout = base::Milliseconds(150);
    auction_ad_config_non_shared_params_.buyer_timeouts =
        blink::AuctionConfig::MaybePromiseBuyerTimeouts::FromValue(
            std::move(buyer_timeouts));

    RunReportResultCreatedScriptExpectingResult(
        "auctionConfig", /*extra_code=*/std::string(),
        R"({"seller":"https://example.com",)"
        R"("decisionLogicURL":"https://example.com/auction.js",)"
        R"("decisionLogicUrl":"https://example.com/auction.js",)"
        R"("perBuyerTimeouts":{"*":150}})",
        /*expected_report_url=*/absl::nullopt);
  }
}

TEST_F(SellerWorkletTest, ReportResultExperimentGroupIdParam) {
  RunReportResultCreatedScriptExpectingResult(
      R"("experimentGroupId" in auctionConfig ? 1 : 0)",
      /*extra_code=*/std::string(), /*expected_signals_for_winner=*/"0",
      /*expected_report_url=*/absl::nullopt);

  experiment_group_id_ = 954u;
  RunReportResultCreatedScriptExpectingResult(
      "auctionConfig.experimentGroupId",
      /*extra_code=*/std::string(), /*expected_signals_for_winner=*/"954",
      /*expected_report_url=*/absl::nullopt);
}

TEST_F(SellerWorkletTest, ReportResultDataVersion) {
  browser_signal_data_version_ = 20;
  RunReportResultCreatedScriptExpectingResult(
      "browserSignals.dataVersion", /*extra_code=*/std::string(),
      /*expected_signals_for_winner=*/"20",
      /*expected_report_url=*/absl::nullopt);
}

// It shouldn't matter the order in which network fetches complete. For each
// required and optional reportResult() URL load prerequisite, ensure that
// reportResult() completes when that URL is the last loaded URL.
TEST_F(SellerWorkletTest, ReportResultLoadCompletionOrder) {
  constexpr char kJsonResponse[] = "{}";
  constexpr char kDirectFromSellerSignalsHeaders[] =
      "Ad-Auction-Allowed: true\nAd-Auction-Only: true";

  direct_from_seller_seller_signals_ = GURL("https://url.test/sellersignals");
  direct_from_seller_auction_signals_ = GURL("https://url.test/auctionsignals");

  struct Response {
    GURL response_url;
    std::string response_type;
    std::string headers;
    std::string content;
  };

  const Response kResponses[] = {
      {decision_logic_url_, kJavascriptMimeType, kAllowFledgeHeader,
       CreateReportToScript(
           "1",
           /*extra_code=*/R"(sendReportTo("https://foo.test"))")},
      {*direct_from_seller_seller_signals_, kJsonMimeType,
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
    mojo::Remote<mojom::SellerWorklet> seller_worklet = CreateWorklet();
    url_loader_factory_.ClearResponses();
    auto run_loop = std::make_unique<base::RunLoop>();
    RunReportResultExpectingResultAsync(
        seller_worklet.get(), "1", GURL("https://foo.test/"),
        /*expected_ad_beacon_map=*/{}, /*expected_pa_requests=*/{},
        /*expected_reporting_latency_timeout=*/false,
        /*expected_errors=*/{}, run_loop->QuitClosure());
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

// Subsequent runs of the same script should not affect each other. Same is true
// for different scripts, but it follows from the single script case.
TEST_F(SellerWorkletTest, ScriptIsolation) {
  // Use arrays so that all values are references, to catch both the case where
  // variables are persisted, and the case where what they refer to is
  // persisted, but variables are overwritten between runs.
  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
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
  auto seller_worklet = CreateWorklet();
  ASSERT_TRUE(seller_worklet);

  for (int i = 0; i < 3; ++i) {
    // Run each script twice in a row, to cover both cases where the same
    // function is run sequentially, and when one function is run after the
    // other.
    for (int j = 0; j < 2; ++j) {
      base::RunLoop run_loop;
      seller_worklet->ScoreAd(
          ad_metadata_, bid_, bid_currency_,
          auction_ad_config_non_shared_params_,
          direct_from_seller_seller_signals_,
          direct_from_seller_seller_signals_header_ad_slot_,
          direct_from_seller_auction_signals_,
          direct_from_seller_auction_signals_header_ad_slot_,
          browser_signals_other_seller_.Clone(), component_expect_bid_currency_,
          browser_signal_interest_group_owner_, browser_signal_render_url_,
          browser_signal_ad_components_, browser_signal_bidding_duration_msecs_,
          seller_timeout_,
          /*trace_id=*/1,
          TestScoreAdClient::Create(base::BindLambdaForTesting(
              [&run_loop](double score, mojom::RejectReason reject_reason,
                          mojom::ComponentAuctionModifiedBidParamsPtr
                              component_auction_modified_bid_params,
                          absl::optional<double> bid_in_seller_currency,
                          absl::optional<uint32_t> scoring_signals_data_version,
                          const absl::optional<GURL>& debug_loss_report_url,
                          const absl::optional<GURL>& debug_win_report_url,
                          PrivateAggregationRequests pa_requests,
                          base::TimeDelta scoring_latency,
                          mojom::ScoreAdDependencyLatenciesPtr
                              score_ad_dependency_latencies,
                          const std::vector<std::string>& errors) {
                EXPECT_EQ(2, score);
                EXPECT_FALSE(scoring_signals_data_version.has_value());
                EXPECT_TRUE(errors.empty());
                run_loop.Quit();
              })));
      run_loop.Run();
    }

    for (int j = 0; j < 2; ++j) {
      base::RunLoop run_loop;
      seller_worklet->ReportResult(
          auction_ad_config_non_shared_params_,
          direct_from_seller_seller_signals_,
          direct_from_seller_seller_signals_header_ad_slot_,
          direct_from_seller_auction_signals_,
          direct_from_seller_auction_signals_header_ad_slot_,
          browser_signals_other_seller_.Clone(),
          browser_signal_interest_group_owner_,
          browser_signal_buyer_and_seller_reporting_id_,
          browser_signal_render_url_, bid_, bid_currency_,
          browser_signal_desireability_,
          browser_signal_highest_scoring_other_bid_,
          browser_signal_highest_scoring_other_bid_currency_,
          browser_signals_component_auction_report_result_params_.Clone(),
          browser_signal_data_version_.value_or(0),
          browser_signal_data_version_.has_value(),
          /*trace_id=*/1,
          base::BindLambdaForTesting(
              [&run_loop](
                  const absl::optional<std::string>& signals_for_winner,
                  const absl::optional<GURL>& report_url,
                  const base::flat_map<std::string, GURL>& ad_beacon_map,
                  PrivateAggregationRequests pa_requests,
                  base::TimeDelta reporting_latency,
                  const std::vector<std::string>& errors) {
                EXPECT_EQ("2", signals_for_winner);
                EXPECT_TRUE(errors.empty());
                run_loop.Quit();
              }));
      run_loop.Run();
    }
  }
}

TEST_F(SellerWorkletTest, DeleteBeforeScoreAdCallback) {
  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        CreateBasicSellAdScript());
  auto seller_worklet = CreateWorklet();
  ASSERT_TRUE(seller_worklet);

  base::WaitableEvent* event_handle = WedgeV8Thread(v8_helper_.get());
  seller_worklet->ScoreAd(
      ad_metadata_, bid_, bid_currency_, auction_ad_config_non_shared_params_,
      direct_from_seller_seller_signals_,
      direct_from_seller_seller_signals_header_ad_slot_,
      direct_from_seller_auction_signals_,
      direct_from_seller_auction_signals_header_ad_slot_,
      browser_signals_other_seller_.Clone(), component_expect_bid_currency_,
      browser_signal_interest_group_owner_, browser_signal_render_url_,
      browser_signal_ad_components_, browser_signal_bidding_duration_msecs_,
      seller_timeout_,
      /*trace_id=*/1,
      TestScoreAdClient::Create(
          // Callback should not be invoked since worklet deleted
          TestScoreAdClient::ScoreAdNeverInvokedCallback()));
  base::RunLoop().RunUntilIdle();
  seller_worklet.reset();
  event_handle->Signal();
}

TEST_F(SellerWorkletTest, DeleteBeforeReportResultCallback) {
  AddJavascriptResponse(
      &url_loader_factory_, decision_logic_url_,
      CreateReportToScript("1", R"(sendReportTo("https://foo.test"))"));
  auto seller_worklet = CreateWorklet();
  ASSERT_TRUE(seller_worklet);
  // Need to call ScoreAd() calling ReportResult().
  RunScoreAdExpectingResultOnWorklet(seller_worklet.get(), 1);

  base::WaitableEvent* event_handle = WedgeV8Thread(v8_helper_.get());
  seller_worklet->ReportResult(
      auction_ad_config_non_shared_params_, direct_from_seller_seller_signals_,
      direct_from_seller_seller_signals_header_ad_slot_,
      direct_from_seller_auction_signals_,
      direct_from_seller_auction_signals_header_ad_slot_,
      browser_signals_other_seller_.Clone(),
      browser_signal_interest_group_owner_,
      browser_signal_buyer_and_seller_reporting_id_, browser_signal_render_url_,
      bid_, bid_currency_, browser_signal_desireability_,
      browser_signal_highest_scoring_other_bid_,
      browser_signal_highest_scoring_other_bid_currency_,
      browser_signals_component_auction_report_result_params_.Clone(),
      browser_signal_data_version_.value_or(0),
      browser_signal_data_version_.has_value(),
      /*trace_id=*/1,
      base::BindOnce([](const absl::optional<std::string>& signals_for_winner,
                        const absl::optional<GURL>& report_url,
                        const base::flat_map<std::string, GURL>& ad_beacon_map,
                        PrivateAggregationRequests pa_requests,
                        base::TimeDelta reporting_latency,
                        const std::vector<std::string>& errors) {
        ADD_FAILURE() << "Callback should not be invoked since worklet deleted";
      }));
  base::RunLoop().RunUntilIdle();
  seller_worklet.reset();
  event_handle->Signal();
}

TEST_F(SellerWorkletTest, PauseOnStart) {
  // If pause isn't working, this will be used and not the right script.
  url_loader_factory_.AddResponse(decision_logic_url_.spec(), "",
                                  net::HTTP_NOT_FOUND);

  SellerWorklet* worklet_impl = nullptr;
  auto worklet =
      CreateWorklet(/*pause_for_debugger_on_start=*/true, &worklet_impl);
  // Grab the context ID to be able to resume.
  int id = worklet_impl->context_group_id_for_testing();

  // Queue a ScoreAd() call, which should not happen immediately since loading
  // is paused.
  base::RunLoop run_loop;
  RunScoreAdOnWorkletAsync(
      worklet.get(), /*expected_score=*/10,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_reject_reason=*/
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_bid_in_seller_currency=*/absl::nullopt,
      /*expected_score_ad_timeout=*/false,
      /*expected_signals_fetch_latency=*/absl::nullopt,
      /*expected_code_ready_latency=*/absl::nullopt, run_loop.QuitClosure());

  // Give it a chance to fetch.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(run_loop.AnyQuitCalled());

  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        CreateScoreAdScript("10"));

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(run_loop.AnyQuitCalled());

  // Let the ScoreAd() call run.
  v8_helper_->v8_runner()->PostTask(
      FROM_HERE, base::BindOnce([](scoped_refptr<AuctionV8Helper> v8_helper,
                                   int id) { v8_helper->Resume(id); },
                                v8_helper_, id));

  run_loop.RunUntilIdle();
}

TEST_F(SellerWorkletTest, PauseOnStartDelete) {
  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        CreateScoreAdScript("10"));

  SellerWorklet* worklet_impl = nullptr;
  auto worklet =
      CreateWorklet(/*pause_for_debugger_on_start=*/true, &worklet_impl);

  // Queue a ScoreAd() call, which should start paused and will never be run.
  base::RunLoop run_loop;
  RunScoreAdOnWorkletExpectingCallbackNeverInvoked(worklet.get());

  // Give it a chance to fetch.
  task_environment_.RunUntilIdle();

  // Grab the context ID.
  int id = worklet_impl->context_group_id_for_testing();

  // Delete the worklet.
  worklet.reset();
  task_environment_.RunUntilIdle();

  // Try to resume post-delete. Should not crash
  v8_helper_->v8_runner()->PostTask(
      FROM_HERE, base::BindOnce([](scoped_refptr<AuctionV8Helper> v8_helper,
                                   int id) { v8_helper->Resume(id); },
                                v8_helper_, id));

  task_environment_.RunUntilIdle();
}

TEST_F(SellerWorkletTest, BasicV8Debug) {
  ScopedInspectorSupport inspector_support(v8_helper_.get());

  // Helper for looking for scriptParsed events.
  auto is_script_parsed = [](const TestChannel::Event& event) -> bool {
    if (event.type != TestChannel::Event::Type::Notification) {
      return false;
    }

    const std::string* candidate_method =
        event.value.GetDict().FindString("method");
    return (candidate_method && *candidate_method == "Debugger.scriptParsed");
  };

  const GURL kUrl1 = GURL("http://example.com/first.js");
  const GURL kUrl2 = GURL("http://example.org/second.js");

  AddJavascriptResponse(&url_loader_factory_, kUrl1, CreateScoreAdScript("1"));
  AddJavascriptResponse(&url_loader_factory_, kUrl2, CreateScoreAdScript("2"));

  SellerWorklet* worklet_impl1 = nullptr;
  decision_logic_url_ = kUrl1;
  auto worklet1 = CreateWorklet(
      /*pause_for_debugger_on_start=*/true, &worklet_impl1);
  base::RunLoop run_loop1;
  RunScoreAdOnWorkletAsync(
      worklet1.get(), /*expected_score=*/1,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_reject_reason=*/
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_bid_in_seller_currency=*/absl::nullopt,
      /*expected_score_ad_timeout=*/false,
      /*expected_signals_fetch_latency=*/absl::nullopt,
      /*expected_code_ready_latency=*/absl::nullopt, run_loop1.QuitClosure());

  decision_logic_url_ = kUrl2;
  SellerWorklet* worklet_impl2 = nullptr;
  auto worklet2 = CreateWorklet(
      /*pause_for_debugger_on_start=*/true, &worklet_impl2);
  base::RunLoop run_loop2;
  RunScoreAdOnWorkletAsync(
      worklet2.get(), /*expected_score=*/2,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_reject_reason=*/
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_bid_in_seller_currency=*/absl::nullopt,
      /*expected_score_ad_timeout=*/false,
      /*expected_signals_fetch_latency=*/absl::nullopt,
      /*expected_code_ready_latency=*/absl::nullopt, run_loop2.QuitClosure());

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
  EXPECT_FALSE(run_loop1.AnyQuitCalled());
  channel1->RunCommandAndWaitForResult(
      3, "Runtime.runIfWaitingForDebugger",
      R"({"id":3,"method":"Runtime.runIfWaitingForDebugger","params":{}})");
  run_loop1.Run();

  // channel1 should have had a parsed notification for kUrl1.
  TestChannel::Event script_parsed1 =
      channel1->WaitForMethodNotification("Debugger.scriptParsed");
  const std::string* url1 =
      script_parsed1.value.GetDict().FindStringByDottedPath("params.url");
  ASSERT_TRUE(url1);
  EXPECT_EQ(kUrl1.spec(), *url1);

  // There shouldn't be a parsed notification on channel 2, however.
  std::list<TestChannel::Event> events2 = channel2->TakeAllEvents();
  EXPECT_TRUE(base::ranges::none_of(events2, is_script_parsed));

  // Unpause execution for #2.
  EXPECT_FALSE(run_loop2.AnyQuitCalled());
  channel2->RunCommandAndWaitForResult(
      3, "Runtime.runIfWaitingForDebugger",
      R"({"id":3,"method":"Runtime.runIfWaitingForDebugger","params":{}})");
  run_loop2.Run();

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

TEST_F(SellerWorkletTest, ParseErrorV8Debug) {
  ScopedInspectorSupport inspector_support(v8_helper_.get());
  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        "Invalid Javascript");
  SellerWorklet* worklet_impl = nullptr;
  auto worklet =
      CreateWorklet(/*pause_for_debugger_on_start=*/true, &worklet_impl);
  int id = worklet_impl->context_group_id_for_testing();
  TestChannel* channel = inspector_support.ConnectDebuggerSession(id);

  channel->RunCommandAndWaitForResult(
      1, "Runtime.enable", R"({"id":1,"method":"Runtime.enable","params":{}})");
  channel->RunCommandAndWaitForResult(
      2, "Debugger.enable",
      R"({"id":2,"method":"Debugger.enable","params":{}})");

  // Unpause execution and wait for the pipe to be closed with an error.
  channel->RunCommandAndWaitForResult(
      3, "Runtime.runIfWaitingForDebugger",
      R"({"id":3,"method":"Runtime.runIfWaitingForDebugger","params":{}})");
  EXPECT_FALSE(WaitForDisconnect().empty());

  // Should have gotten a parse error notification.
  TestChannel::Event parse_error =
      channel->WaitForMethodNotification("Debugger.scriptFailedToParse");
  const std::string* error_url =
      parse_error.value.GetDict().FindStringByDottedPath("params.url");
  ASSERT_TRUE(error_url);
  EXPECT_EQ(decision_logic_url_.spec(), *error_url);
}

TEST_F(SellerWorkletTest, BasicDevToolsDebug) {
  const char kScriptResult[] = "this.global_score ? this.global_score : 10";

  const char kUrl1[] = "http://example.com/first.js";
  const char kUrl2[] = "http://example.org/second.js";

  AddJavascriptResponse(&url_loader_factory_, GURL(kUrl1),
                        CreateScoreAdScript(kScriptResult));
  AddJavascriptResponse(&url_loader_factory_, GURL(kUrl2),
                        CreateScoreAdScript(kScriptResult));

  decision_logic_url_ = GURL(kUrl1);
  auto worklet1 = CreateWorklet(/*pause_for_debugger_on_start=*/true);
  base::RunLoop run_loop1;
  RunScoreAdOnWorkletAsync(
      worklet1.get(), /*expected_score=*/100.5,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_reject_reason=*/
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_bid_in_seller_currency=*/absl::nullopt,
      /*expected_score_ad_timeout=*/false,
      /*expected_signals_fetch_latency=*/absl::nullopt,
      /*expected_code_ready_latency=*/absl::nullopt, run_loop1.QuitClosure());

  decision_logic_url_ = GURL(kUrl2);
  auto worklet2 = CreateWorklet(/*pause_for_debugger_on_start=*/true);
  base::RunLoop run_loop2;
  RunScoreAdOnWorkletAsync(
      worklet2.get(), /*expected_score=*/0,
      {"http://example.org/second.js scoreAd() return: Value passed as "
       "dictionary is neither object, null, nor undefined."},
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_reject_reason=*/
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_bid_in_seller_currency=*/absl::nullopt,
      /*expected_score_ad_timeout=*/false,
      /*expected_signals_fetch_latency=*/absl::nullopt,
      /*expected_code_ready_latency=*/absl::nullopt, run_loop2.QuitClosure());

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
          "lineNumber": 2,
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

  // Next there is the breakpoint.
  TestDevToolsAgentClient::Event breakpoint_hit1 =
      debug1.WaitForMethodNotification("Debugger.paused");

  base::Value::List* hit_breakpoints =
      breakpoint_hit1.value.GetDict().FindDict("params")->FindList(
          "hitBreakpoints");
  ASSERT_TRUE(hit_breakpoints);
  ASSERT_EQ(1u, hit_breakpoints->size());
  ASSERT_TRUE((*hit_breakpoints)[0].is_string());
  EXPECT_EQ("1:2:0:http://example.com/first.js",
            (*hit_breakpoints)[0].GetString());
  std::string* callframe_id1 = breakpoint_hit1.value.GetDict()
                                   .FindDict("params")
                                   ->FindList("callFrames")
                                   ->front()
                                   .GetDict()
                                   .FindString("callFrameId");

  // Override the score value.
  const char kCommandTemplate[] = R"({
    "id": 5,
    "method": "Debugger.evaluateOnCallFrame",
    "params": {
      "callFrameId": "%s",
      "expression": "global_score = %s"
    }
  })";

  debug1.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kIO, 5, "Debugger.evaluateOnCallFrame",
      base::StringPrintf(kCommandTemplate, callframe_id1->c_str(), "100.5"));

  // Let worklet 1 finish. The callback set by RunScoreAdOnWorkletAsync() will
  // verify the result.
  EXPECT_FALSE(run_loop1.AnyQuitCalled());
  debug1.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kIO, 6, "Debugger.resume",
      R"({"id":6,"method":"Debugger.resume","params":{}})");
  run_loop1.Run();

  // Start #2, see that it parses the script.
  debug2.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 4,
      "Runtime.runIfWaitingForDebugger",
      R"({"id":4,"method":"Runtime.runIfWaitingForDebugger","params":{}})");

  TestDevToolsAgentClient::Event script_parsed2 =
      debug2.WaitForMethodNotification("Debugger.scriptParsed");
  const std::string* url2 =
      script_parsed2.value.GetDict().FindStringByDottedPath("params.url");
  ASSERT_TRUE(url2);
  EXPECT_EQ(*url2, kUrl2);

  // Wait for breakpoint, and then change the result to be trouble.
  TestDevToolsAgentClient::Event breakpoint_hit2 =
      debug2.WaitForMethodNotification("Debugger.paused");
  std::string* callframe_id2 = breakpoint_hit2.value.GetDict()
                                   .FindDict("params")
                                   ->FindList("callFrames")
                                   ->front()
                                   .GetDict()
                                   .FindString("callFrameId");
  debug2.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kIO, 5, "Debugger.evaluateOnCallFrame",
      base::StringPrintf(kCommandTemplate, callframe_id2->c_str(),
                         R"(\"not a score\")"));

  // Let worklet 2 finish. The callback set by RunScoreAdOnWorkletAsync() will
  // verify the result.
  debug2.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kIO, 6, "Debugger.resume",
      R"({"id":6,"method":"Debugger.resume","params":{}})");
  run_loop2.Run();
}

TEST_F(SellerWorkletTest, InstrumentationBreakpoints) {
  const char kUrl[] = "http://example.com/script.js";

  std::string script_body =
      CreateBasicSellAdScript() +
      CreateReportToScript("1", R"(sendReportTo("https://foo.test"))");
  AddJavascriptResponse(&url_loader_factory_, GURL(kUrl), script_body);

  decision_logic_url_ = GURL(kUrl);
  auto worklet = CreateWorklet(/*pause_for_debugger_on_start=*/true);
  base::RunLoop run_loop;
  RunScoreAdOnWorkletAsync(
      worklet.get(), /*expected_score=*/1.0,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_reject_reason=*/
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_bid_in_seller_currency=*/absl::nullopt,
      /*expected_score_ad_timeout=*/false,
      /*expected_signals_fetch_latency=*/absl::nullopt,
      /*expected_code_ready_latency=*/absl::nullopt, run_loop.QuitClosure());

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
                                           "beforeSellerWorkletScoringStart"));
  debug.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 4,
      "EventBreakpoints.setInstrumentationBreakpoint",
      MakeInstrumentationBreakpointCommand(
          4, "set", "beforeSellerWorkletReportingStart"));

  // Resume creation, ScoreAd() call should hit a breakpoint.
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
  EXPECT_EQ("instrumentation:beforeSellerWorkletScoringStart", *breakpoint1);

  // Let scoring finish.
  EXPECT_FALSE(run_loop.AnyQuitCalled());
  debug.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kIO, 6, "Debugger.resume",
      R"({"id":6,"method":"Debugger.resume","params":{}})");
  run_loop.Run();

  // Now try reporting, should hit the other breakpoint.
  base::RunLoop run_loop2;
  RunReportResultExpectingResultAsync(
      worklet.get(), "1", GURL("https://foo.test/"),
      /*expected_ad_beacon_map=*/{}, /*expected_pa_requests=*/{},
      /*expected_reporting_latency_timeout=*/false,
      /*expected_errors=*/{}, run_loop2.QuitClosure());
  TestDevToolsAgentClient::Event breakpoint_hit2 =
      debug.WaitForMethodNotification("Debugger.paused");
  const std::string* breakpoint2 =
      breakpoint_hit2.value.GetDict().FindStringByDottedPath(
          "params.data.eventName");
  ASSERT_TRUE(breakpoint2);
  EXPECT_EQ("instrumentation:beforeSellerWorkletReportingStart", *breakpoint2);

  // Let reporting finish.
  debug.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kIO, 7, "Debugger.resume",
      R"({"id":7,"method":"Debugger.resume","params":{}})");
  run_loop2.Run();

  // Running another scoreAd will trigger the breakpoint again, since we didn't
  // remove it.
  base::RunLoop run_loop3;
  RunScoreAdOnWorkletAsync(
      worklet.get(), /*expected_score=*/1.0,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_reject_reason=*/
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_bid_in_seller_currency=*/absl::nullopt,
      /*expected_score_ad_timeout=*/false,
      /*expected_signals_fetch_latency=*/absl::nullopt,
      /*expected_code_ready_latency=*/absl::nullopt, run_loop3.QuitClosure());

  TestDevToolsAgentClient::Event breakpoint_hit3 =
      debug.WaitForMethodNotification("Debugger.paused");

  const std::string* breakpoint3 =
      breakpoint_hit1.value.GetDict().FindStringByDottedPath(
          "params.data.eventName");
  ASSERT_TRUE(breakpoint3);
  EXPECT_EQ("instrumentation:beforeSellerWorkletScoringStart", *breakpoint3);

  // Let this round of scoring finish, too.
  debug.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kIO, 8, "Debugger.resume",
      R"({"id":8,"method":"Debugger.resume","params":{}})");
  run_loop3.Run();
}

TEST_F(SellerWorkletTest, UnloadWhilePaused) {
  // Make sure things are cleaned up properly if the worklet is destroyed while
  // paused on a breakpoint.
  const char kUrl[] = "http://example.com/script.js";

  std::string script_body =
      CreateBasicSellAdScript() +
      CreateReportToScript("1", R"(sendReportTo("https://foo.test"))");
  AddJavascriptResponse(&url_loader_factory_, GURL(kUrl), script_body);

  decision_logic_url_ = GURL(kUrl);
  auto worklet = CreateWorklet(/*pause_for_debugger_on_start=*/true);
  RunScoreAdOnWorkletExpectingCallbackNeverInvoked(worklet.get());

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

  // Set the instrumentation breakpoint.
  debug.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 3,
      "EventBreakpoints.setInstrumentationBreakpoint",
      MakeInstrumentationBreakpointCommand(3, "set",
                                           "beforeSellerWorkletScoringStart"));
  // Resume execution of create. Should hit corresponding breakpoint.
  debug.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 4,
      "Runtime.runIfWaitingForDebugger",
      R"({"id":4,"method":"Runtime.runIfWaitingForDebugger","params":{}})");

  RunScoreAdOnWorkletAsync(
      worklet.get(), /*expected_score=*/1.0, /*expected_errors=*/{},
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_reject_reason=*/
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_bid_in_seller_currency=*/absl::nullopt,
      /*expected_score_ad_timeout=*/false,
      /*expected_signals_fetch_latency=*/absl::nullopt,
      /*expected_code_ready_latency=*/absl::nullopt, base::BindOnce([]() {
        ADD_FAILURE() << "scoreAd shouldn't actually get to finish.";
      }));

  debug.WaitForMethodNotification("Debugger.paused");

  // Destroy the worklet
  worklet.reset();

  // This won't terminate if the V8 thread is still blocked in debugger.
  task_environment_.RunUntilIdle();
}

// Test that cancelling the worklet before it runs but after the execution was
// queued actually cancels the execution. This is done by trying to run a
// while(true) {} script with a timeout that's bigger than the test timeout, so
// if it doesn't get cancelled the *test* will timeout.
TEST_F(SellerWorkletTest, Cancelation) {
  seller_timeout_ = base::Days(360);
  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        "while(true) {}");
  mojo::Remote<mojom::SellerWorklet> seller_worklet = CreateWorklet();
  // Let the script load.
  task_environment_.RunUntilIdle();

  // Now we no longer need it for parsing JS, wedge the V8 thread so we get a
  // chance to cancel the script *before* it actually tries running.
  base::WaitableEvent* event_handle = WedgeV8Thread(v8_helper_.get());

  TestScoreAdClient client(TestScoreAdClient::ScoreAdNeverInvokedCallback());
  mojo::Receiver<mojom::ScoreAdClient> client_receiver(&client);

  seller_worklet->ScoreAd(
      ad_metadata_, bid_, bid_currency_, auction_ad_config_non_shared_params_,
      direct_from_seller_seller_signals_,
      direct_from_seller_seller_signals_header_ad_slot_,
      direct_from_seller_auction_signals_,
      direct_from_seller_auction_signals_header_ad_slot_,
      browser_signals_other_seller_.Clone(), component_expect_bid_currency_,
      browser_signal_interest_group_owner_, browser_signal_render_url_,
      browser_signal_ad_components_, browser_signal_bidding_duration_msecs_,
      seller_timeout_,
      /*trace_id=*/1, client_receiver.BindNewPipeAndPassRemote());

  // Cancel and then unwedge.
  client_receiver.reset();
  base::RunLoop().RunUntilIdle();
  event_handle->Signal();

  // Make sure cancellation happens before ~SellerWorklet.
  task_environment_.RunUntilIdle();
}

// Test that queued tasks get cancelled at worklet destruction.
TEST_F(SellerWorkletTest, CancelationDtor) {
  seller_timeout_ = base::Days(360);

  // ReportResult timeout isn't configurable the way scoreAd is.
  v8_helper_->v8_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<AuctionV8Helper> v8_helper) {
            v8_helper->set_script_timeout_for_testing(base::Days(360));
          },
          v8_helper_));

  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        "while(true) {}");
  mojo::Remote<mojom::SellerWorklet> seller_worklet = CreateWorklet();
  // Let the script load.
  task_environment_.RunUntilIdle();

  // Now we no longer need it for parsing JS, wedge the V8 thread so we get a
  // chance to cancel the script *before* it actually tries running.
  base::WaitableEvent* event_handle = WedgeV8Thread(v8_helper_.get());

  RunScoreAdOnWorkletExpectingCallbackNeverInvoked(seller_worklet.get());
  RunReportResultExpectingCallbackNeverInvoked(seller_worklet.get());

  // Destroy the worklet, then unwedge.
  seller_worklet.reset();
  base::RunLoop().RunUntilIdle();
  event_handle->Signal();
}

// Test that cancelling execution before the script is fetched doesn't run it.
TEST_F(SellerWorkletTest, CancelBeforeFetch) {
  seller_timeout_ = base::Days(360);

  mojo::Remote<mojom::SellerWorklet> seller_worklet = CreateWorklet();
  TestScoreAdClient client(TestScoreAdClient::ScoreAdNeverInvokedCallback());
  mojo::Receiver<mojom::ScoreAdClient> client_receiver(&client);

  seller_worklet->ScoreAd(
      ad_metadata_, bid_, bid_currency_, auction_ad_config_non_shared_params_,
      direct_from_seller_seller_signals_,
      direct_from_seller_seller_signals_header_ad_slot_,
      direct_from_seller_auction_signals_,
      direct_from_seller_auction_signals_header_ad_slot_,
      browser_signals_other_seller_.Clone(), component_expect_bid_currency_,
      browser_signal_interest_group_owner_, browser_signal_render_url_,
      browser_signal_ad_components_, browser_signal_bidding_duration_msecs_,
      seller_timeout_,
      /*trace_id=*/1, client_receiver.BindNewPipeAndPassRemote());
  task_environment_.RunUntilIdle();
  // Cancel and then make the script available.
  client_receiver.reset();
  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        "while (true) {}");

  // Make sure cancellation happens before ~SellerWorklet.
  task_environment_.RunUntilIdle();
}

TEST_F(SellerWorkletTest, ForDebuggingOnlyReportsWithDebugFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      blink::features::kBiddingAndScoringDebugReportingAPI);

  RunScoreAdWithJavascriptExpectingResult(
      CreateScoreAdScript(
          "1", R"(forDebuggingOnly.reportAdAuctionLoss("https://loss.url"))"),
      1, /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt);

  RunScoreAdWithJavascriptExpectingResult(
      CreateScoreAdScript(
          "1", R"(forDebuggingOnly.reportAdAuctionWin("https://win.url"))"),
      1, /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt);
}

TEST_F(SellerWorkletTest, AuctionRequestedSizeIsPresentInScoreAdJavascript) {
  auction_ad_config_non_shared_params_.requested_size = blink::AdSize(
      /*width=*/1920,
      /*width_units=*/blink::mojom::AdSize_LengthUnit::kPixels,
      /*height=*/100,
      /*height_units*/ blink::mojom::AdSize_LengthUnit::kScreenHeight);

  std::string requested_size_validator =
      R"(if (!(auctionConfig.requestedSize.width === '1920px' &&
               auctionConfig.requestedSize.height === '100sh')) {
          throw new Error('Requested size is incorrect or missing.');
        })";

  RunScoreAdWithJavascriptExpectingResult(
      CreateScoreAdScript("1", requested_size_validator), 1,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt);
}

TEST_F(SellerWorkletTest,
       AuctionRequestedSizeIsMissingFromScoreAdJavascriptWhenNotProvided) {
  // Because we didn't modify auction_ad_config_non_shared_params_,
  // requestedSize should be empty.
  std::string requested_size_validator =
      R"(if (auctionConfig.hasOwnProperty('requestedSize')) {
          throw new Error('Requested size is present but should be missing.');
        })";

  RunScoreAdWithJavascriptExpectingResult(
      CreateScoreAdScript("1", requested_size_validator), 1,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt);
}

TEST_F(SellerWorkletTest, AuctionRequestedSizeIsPresentReportResultJavascript) {
  auction_ad_config_non_shared_params_.requested_size = blink::AdSize(
      /*width=*/1920,
      /*width_units=*/blink::mojom::AdSize_LengthUnit::kPixels,
      /*height=*/100,
      /*height_units*/ blink::mojom::AdSize_LengthUnit::kScreenHeight);

  std::string requested_size_validator =
      R"(if (!(auctionConfig.requestedSize.width === '1920px' &&
               auctionConfig.requestedSize.height === '100sh')) {
          throw new Error('Requested size is incorrect or missing.');
        })";

  RunReportResultCreatedScriptExpectingResult(
      "1", requested_size_validator,
      /*expected_signals_for_winner=*/"1",
      /*expected_report_url=*/absl::nullopt);
}

TEST_F(SellerWorkletTest,
       AuctionRequestedSizeIsMissingFromReportResultJavascriptWhenNotProvided) {
  // Because we didn't modify auction_ad_config_non_shared_params_,
  // requestedSize should be empty.
  std::string requested_size_validator =
      R"(if (auctionConfig.hasOwnProperty('requestedSize')) {
          throw new Error('Requested size is present but should be missing.');
        })";

  RunReportResultCreatedScriptExpectingResult(
      "1", requested_size_validator,
      /*expected_signals_for_winner=*/"1",
      /*expected_report_url=*/absl::nullopt);
}

class SellerWorkletSharedStorageAPIDisabledTest : public SellerWorkletTest {
 public:
  SellerWorkletSharedStorageAPIDisabledTest() {
    feature_list_.InitAndDisableFeature(blink::features::kSharedStorageAPI);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(SellerWorkletSharedStorageAPIDisabledTest, SharedStorageNotExposed) {
  RunScoreAdWithJavascriptExpectingResult(
      CreateScoreAdScript("5", /*extra_code=*/R"(
        sharedStorage.clear();
      )"),
      /*expected_score=*/0, /*expected_errors=*/
      {"https://url.test/:5 Uncaught ReferenceError: sharedStorage is not "
       "defined."},
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_reject_reason=*/mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{});

  RunReportResultCreatedScriptExpectingResult(
      R"(5)",
      R"(
        sharedStorage.clear();
      )",
      /*expected_signals_for_winner=*/absl::nullopt,
      /*expected_report_url=*/absl::nullopt, /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      /*expected_errors=*/
      {"https://url.test/:11 Uncaught ReferenceError: sharedStorage is not "
       "defined."});
}

class SellerWorkletSharedStorageAPIEnabledTest : public SellerWorkletTest {
 public:
  SellerWorkletSharedStorageAPIEnabledTest() {
    feature_list_.InitAndEnableFeature(blink::features::kSharedStorageAPI);
    permissions_policy_state_ =
        mojom::AuctionWorkletPermissionsPolicyState::New(
            /*private_aggregation_allowed=*/true,
            /*shared_storage_allowed=*/true);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(SellerWorkletSharedStorageAPIEnabledTest, SharedStorageWriteInScoreAd) {
  auction_worklet::TestAuctionSharedStorageHost test_shared_storage_host;

  {
    mojo::Receiver<auction_worklet::mojom::AuctionSharedStorageHost> receiver(
        &test_shared_storage_host);
    shared_storage_host_remote_ = receiver.BindNewPipeAndPassRemote();

    RunScoreAdWithJavascriptExpectingResult(
        CreateScoreAdScript("5", /*extra_code=*/R"(
          sharedStorage.set('a', 'b');
          sharedStorage.set('a', 'b', {ignoreIfPresent: true});
          sharedStorage.append('a', 'b');
          sharedStorage.delete('a');
          sharedStorage.clear();
        )"),
        5, /*expected_errors=*/
        {}, mojom::ComponentAuctionModifiedBidParamsPtr(),
        /*expected_data_version=*/absl::nullopt,
        /*expected_debug_loss_report_url=*/absl::nullopt,
        /*expected_debug_win_report_url=*/absl::nullopt,
        /*expected_reject_reason=*/mojom::RejectReason::kNotAvailable,
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

    RunScoreAdWithJavascriptExpectingResult(
        CreateScoreAdScript("5", /*extra_code=*/R"(
          sharedStorage.clear();
        )"),
        /*expected_score=*/0, /*expected_errors=*/
        {"https://url.test/:5 Uncaught TypeError: The \"shared-storage\" "
         "Permissions Policy denied the method on sharedStorage."},
        mojom::ComponentAuctionModifiedBidParamsPtr(),
        /*expected_data_version=*/absl::nullopt,
        /*expected_debug_loss_report_url=*/absl::nullopt,
        /*expected_debug_win_report_url=*/absl::nullopt,
        /*expected_reject_reason=*/mojom::RejectReason::kNotAvailable,
        /*expected_pa_requests=*/{});

    permissions_policy_state_ =
        mojom::AuctionWorkletPermissionsPolicyState::New(
            /*private_aggregation_allowed=*/true,
            /*shared_storage_allowed=*/true);
  }
}

TEST_F(SellerWorkletSharedStorageAPIEnabledTest,
       SharedStorageWriteInReportResult) {
  auction_worklet::TestAuctionSharedStorageHost test_shared_storage_host;

  {
    mojo::Receiver<auction_worklet::mojom::AuctionSharedStorageHost> receiver(
        &test_shared_storage_host);
    shared_storage_host_remote_ = receiver.BindNewPipeAndPassRemote();

    RunReportResultCreatedScriptExpectingResult(
        R"(5)",
        R"(
          sharedStorage.set('a', 'b');
          sharedStorage.set('a', 'b', {ignoreIfPresent: true});
          sharedStorage.append('a', 'b');
          sharedStorage.delete('a');
          sharedStorage.clear();
        )",
        /*expected_signals_for_winner=*/"5",
        /*expected_report_url=*/absl::nullopt, /*expected_ad_beacon_map=*/{},
        /*expected_pa_requests=*/{},
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

    RunReportResultCreatedScriptExpectingResult(
        R"(5)",
        R"(
          sharedStorage.clear();
        )",
        /*expected_signals_for_winner=*/absl::nullopt,
        /*expected_report_url=*/absl::nullopt, /*expected_ad_beacon_map=*/{},
        /*expected_pa_requests=*/{},
        /*expected_errors=*/
        {"https://url.test/:11 Uncaught TypeError: The \"shared-storage\" "
         "Permissions Policy denied the method on sharedStorage."});

    permissions_policy_state_ =
        mojom::AuctionWorkletPermissionsPolicyState::New(
            /*private_aggregation_allowed=*/true,
            /*shared_storage_allowed=*/true);
  }
}

class SellerWorkletRealTimeTest : public SellerWorkletTest {
 public:
  SellerWorkletRealTimeTest()
      : SellerWorkletTest(
            base::test::TaskEnvironment::TimeSource::SYSTEM_TIME) {}
};

// `scoreAd` should time out due to AuctionV8Helper's default script timeout (50
// ms).
TEST_F(SellerWorkletRealTimeTest, ScoreAdTimedOut) {
  AddJavascriptResponse(
      &url_loader_factory_, decision_logic_url_,
      CreateScoreAdScript(/*raw_return_value=*/"", R"(while (1))"));
  auto seller_worklet = CreateWorklet();
  ASSERT_TRUE(seller_worklet);
  base::RunLoop run_loop;
  RunScoreAdOnWorkletAsync(
      seller_worklet.get(), /*expected_score=*/0,
      /*expected_errors=*/
      {"https://url.test/ execution of `scoreAd` timed out."},
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_bid_in_seller_currency=*/absl::nullopt,
      /*expected_score_ad_timeout=*/true,
      /*expected_signals_fetch_latency=*/absl::nullopt,
      /*expected_code_ready_latency=*/absl::nullopt, run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(SellerWorkletRealTimeTest, ScoreAdSellerTimeoutFromAuctionConfig) {
  // Use a very long default script timeout, and a short seller timeout, so
  // that if the seller script with endless loop times out, we know that the
  // seller timeout overwrote the default script timeout and worked.
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

  seller_timeout_ = base::Milliseconds(20);
  AddJavascriptResponse(
      &url_loader_factory_, decision_logic_url_,
      CreateScoreAdScript(/*raw_return_value=*/"", R"(while (1))"));
  auto seller_worklet = CreateWorklet();
  ASSERT_TRUE(seller_worklet);
  base::RunLoop run_loop;
  RunScoreAdOnWorkletAsync(
      seller_worklet.get(), /*expected_score=*/0,
      /*expected_errors=*/
      {"https://url.test/ execution of `scoreAd` timed out."},
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_bid_in_seller_currency=*/absl::nullopt,
      /*expected_score_ad_timeout=*/true,
      /*expected_signals_fetch_latency=*/absl::nullopt,
      /*expected_code_ready_latency=*/absl::nullopt, run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(SellerWorkletRealTimeTest, ReportResultLatency) {
  // We use an infinite loop since we have some notion of how long a timeout
  // should take.
  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        CreateReportToScript("1", "while (true) {}"));

  mojo::Remote<mojom::SellerWorklet> seller_worklet = CreateWorklet();

  base::RunLoop run_loop;
  RunReportResultExpectingResultAsync(
      seller_worklet.get(),
      /*expected_signals_for_winner=*/absl::nullopt,
      /*expected_report_url=*/absl::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      /*expected_reporting_latency_timeout=*/true,
      /*expected_errors=*/
      {"https://url.test/ execution of `reportResult` timed out."},
      run_loop.QuitClosure());
  run_loop.Run();
}

class SellerWorkletBiddingAndScoringDebugReportingAPIEnabledTest
    : public SellerWorkletRealTimeTest {
 public:
  SellerWorkletBiddingAndScoringDebugReportingAPIEnabledTest() {
    feature_list_.InitAndEnableFeature(
        blink::features::kBiddingAndScoringDebugReportingAPI);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

// Test forDebuggingOnly.reportAdAuctionLoss() and
// forDebuggingOnly.reportAdAuctionWin() called in scoreAd().
TEST_F(SellerWorkletBiddingAndScoringDebugReportingAPIEnabledTest,
       ForDebuggingOnlyReports) {
  RunScoreAdWithJavascriptExpectingResult(
      CreateScoreAdScript(
          "1",
          R"(forDebuggingOnly.reportAdAuctionLoss("https://loss.url");
            forDebuggingOnly.reportAdAuctionWin("https://win.url"))"),
      1, /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt, GURL("https://loss.url"),
      GURL("https://win.url"));

  // Should keep debug report URLs when score <= 0.
  RunScoreAdWithJavascriptExpectingResult(
      CreateScoreAdScript(
          "-1",
          R"(forDebuggingOnly.reportAdAuctionLoss("https://loss.url");
            forDebuggingOnly.reportAdAuctionWin("https://win.url"))"),
      0, /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt, GURL("https://loss.url"),
      GURL("https://win.url"));

  // It's OK to call one API but not the other.
  RunScoreAdWithJavascriptExpectingResult(
      CreateScoreAdScript(
          "1", R"(forDebuggingOnly.reportAdAuctionLoss("https://loss.url"))"),
      1, /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt, GURL("https://loss.url"));
  RunScoreAdWithJavascriptExpectingResult(
      CreateScoreAdScript(
          "1", R"(forDebuggingOnly.reportAdAuctionWin("https://win.url"))"),
      1, /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      GURL("https://win.url"));

  // There should be no debugging report URLs when scoreAd() returns invalid
  // value type.
  RunScoreAdWithJavascriptExpectingResult(
      CreateScoreAdScript(
          "\"invalid_score\"",
          R"(forDebuggingOnly.reportAdAuctionLoss("https://loss.url");
            forDebuggingOnly.reportAdAuctionWin("https://win.url"))"),
      0,
      {"https://url.test/ scoreAd() return: Value passed as dictionary is "
       "neither object, null, nor undefined."},
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt);
}

// Debugging loss/win report URLs should be nullopt if scoreAd() pareamters are
// invalid.
TEST_F(SellerWorkletBiddingAndScoringDebugReportingAPIEnabledTest,
       ForDebuggingOnlyReportsInvalidScoreAdParameter) {
  // Auction config param is invalid.
  auction_ad_config_non_shared_params_.auction_signals =
      blink::AuctionConfig::MaybePromiseJson::FromValue("{invalid json");
  RunScoreAdWithJavascriptExpectingResult(
      CreateScoreAdScript(
          "1",
          R"(forDebuggingOnly.reportAdAuctionLoss("https://loss.url");
            forDebuggingOnly.reportAdAuctionWin("https://win.url"))"),
      0);
  // Setting it back to default value to avoid affecting following tests.
  auction_ad_config_non_shared_params_.auction_signals =
      blink::AuctionConfig::MaybePromiseJson::FromValue(
          R"({"is_auction_signals": true})");

  // `ad_metadata_` is an invalid json.
  ad_metadata_ = "{invalid_json";
  RunScoreAdWithJavascriptExpectingResult(
      CreateScoreAdScript(
          "1",
          R"(forDebuggingOnly.reportAdAuctionLoss("https://loss.url");
            forDebuggingOnly.reportAdAuctionWin("https://win.url"))"),
      0);
}

TEST_F(SellerWorkletBiddingAndScoringDebugReportingAPIEnabledTest,
       ForDebuggingOnlyReportsInvalidParameter) {
  RunScoreAdWithJavascriptExpectingResult(
      CreateScoreAdScript("1", R"(forDebuggingOnly.reportAdAuctionLoss(null))"),
      0,
      {"https://url.test/:4 Uncaught TypeError: "
       "reportAdAuctionLoss must be passed a valid HTTPS url."});

  RunScoreAdWithJavascriptExpectingResult(
      CreateScoreAdScript("1", R"(forDebuggingOnly.reportAdAuctionWin([5]))"),
      0,
      {"https://url.test/:4 Uncaught TypeError: "
       "reportAdAuctionWin must be passed a valid HTTPS url."});

  std::vector<std::string> non_https_urls = {"http://report.url",
                                             "file:///foo/", "Not a URL"};
  for (const auto& url : non_https_urls) {
    RunScoreAdWithJavascriptExpectingResult(
        CreateScoreAdScript(
            "1",
            base::StringPrintf(R"(forDebuggingOnly.reportAdAuctionLoss("%s"))",
                               url.c_str())),
        0,
        {"https://url.test/:4 Uncaught TypeError: "
         "reportAdAuctionLoss must be passed a valid HTTPS url."});

    RunScoreAdWithJavascriptExpectingResult(
        CreateScoreAdScript(
            "1",
            base::StringPrintf(R"(forDebuggingOnly.reportAdAuctionWin("%s"))",
                               url.c_str())),
        0,
        {"https://url.test/:4 Uncaught TypeError: "
         "reportAdAuctionWin must be passed a valid HTTPS url."});
  }

  // No message if caught, but still no debug report URLs.
  RunScoreAdWithJavascriptExpectingResult(
      CreateScoreAdScript(
          "1",
          R"(try {forDebuggingOnly.reportAdAuctionLoss("http://loss.url")}
            catch (e) {})"),
      1, /*expected_errors=*/{});
}

TEST_F(SellerWorkletBiddingAndScoringDebugReportingAPIEnabledTest,
       ForDebuggingOnlyReportsMultiCallsAllowed) {
  RunScoreAdWithJavascriptExpectingResult(
      CreateScoreAdScript(
          "1",
          R"(forDebuggingOnly.reportAdAuctionLoss("https://loss.url");
            forDebuggingOnly.reportAdAuctionLoss("https://loss.url2"))"),
      1, /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/GURL("https://loss.url2"),
      /*expected_debug_win_report_url=*/absl::nullopt);

  // Test that the first URL is preserved when the second call throws.
  RunScoreAdWithJavascriptExpectingResult(
      CreateScoreAdScript(
          "1",
          R"(forDebuggingOnly.reportAdAuctionLoss("https://loss.url");
             try {
               forDebuggingOnly.reportAdAuctionLoss("http://invalidloss.url");
             } catch (e) {})"),
      1, /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/GURL("https://loss.url"),
      /*expected_debug_win_report_url=*/absl::nullopt);

  RunScoreAdWithJavascriptExpectingResult(
      CreateScoreAdScript(
          "1",
          R"(forDebuggingOnly.reportAdAuctionWin("https://win.url");
            forDebuggingOnly.reportAdAuctionWin("https://win.url2"))"),
      1, /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/GURL("https://win.url2"));

  // Test that the first URL is preserved when the second call throws.
  RunScoreAdWithJavascriptExpectingResult(
      CreateScoreAdScript(
          "1",
          R"(forDebuggingOnly.reportAdAuctionWin("https://win.url");
             try {
              forDebuggingOnly.reportAdAuctionWin("http://invalidwin.url");
             } catch (e) {})"),
      1, /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/GURL("https://win.url"));
}

// Loss report URLs before seller script times out should be kept.
TEST_F(SellerWorkletBiddingAndScoringDebugReportingAPIEnabledTest,
       ScoreAdHasError) {
  // The seller script has an endless while loop. It will time out due to
  // AuctionV8Helper's default script timeout (50 ms).
  RunScoreAdWithJavascriptExpectingResult(
      CreateScoreAdScript(
          /*raw_return_value=*/"",
          R"(forDebuggingOnly.reportAdAuctionLoss("https://loss.url1");
            error;
            forDebuggingOnly.reportAdAuctionLoss("https://loss.url2"))"),
      0, {"https://url.test/:5 Uncaught ReferenceError: error is not defined."},
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/{}, GURL("https://loss.url1"));
}

// Loss report URLs before seller script times out should be kept.
TEST_F(SellerWorkletBiddingAndScoringDebugReportingAPIEnabledTest,
       ScoreAdTimedOut) {
  // The seller script has an endless while loop. It will time out due to
  // AuctionV8Helper's default script timeout (50 ms).
  RunScoreAdWithJavascriptExpectingResult(
      CreateScoreAdScript(
          /*raw_return_value=*/"",
          R"(forDebuggingOnly.reportAdAuctionLoss("https://loss.url1");
            while (1);
            forDebuggingOnly.reportAdAuctionLoss("https://loss.url2"))"),
      0, {"https://url.test/ execution of `scoreAd` timed out."},
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/{}, GURL("https://loss.url1"));
}

// Subsequent runs of the same script should not affect each other.
TEST_F(SellerWorkletBiddingAndScoringDebugReportingAPIEnabledTest,
       ForDebuggingOnlyReportsScriptIsolation) {
  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        R"(
        function scoreAd(adMetadata, bid, auctionConfig, trustedScoringSignals,
            browserSignals) {
          if (bid === 1) {
            forDebuggingOnly.reportAdAuctionLoss("https://loss.url");
            forDebuggingOnly.reportAdAuctionWin("https://win.url");
          }
          return bid;
        }

        function reportResult() {}
      )");
  auto seller_worklet = CreateWorklet();
  ASSERT_TRUE(seller_worklet);

  // Run the same script twice, and only call debugging report in the first run.
  // Only the first run will have debugging report URLs.
  for (int i = 0; i < 2; ++i) {
    base::RunLoop run_loop;
    seller_worklet->ScoreAd(
        ad_metadata_, i + 1, bid_currency_,
        auction_ad_config_non_shared_params_,
        direct_from_seller_seller_signals_,
        direct_from_seller_seller_signals_header_ad_slot_,
        direct_from_seller_auction_signals_,
        direct_from_seller_auction_signals_header_ad_slot_,
        browser_signals_other_seller_.Clone(), component_expect_bid_currency_,
        browser_signal_interest_group_owner_, browser_signal_render_url_,
        browser_signal_ad_components_, browser_signal_bidding_duration_msecs_,
        seller_timeout_,
        /*trace_id=*/1,
        TestScoreAdClient::Create(base::BindLambdaForTesting(
            [&run_loop](double score, mojom::RejectReason reject_reason,
                        mojom::ComponentAuctionModifiedBidParamsPtr
                            component_auction_modified_bid_params,
                        absl::optional<double> bid_in_seller_currency,
                        absl::optional<uint32_t> scoring_signals_data_version,
                        const absl::optional<GURL>& debug_loss_report_url,
                        const absl::optional<GURL>& debug_win_report_url,
                        PrivateAggregationRequests pa_requests,
                        base::TimeDelta scoring_latency,
                        mojom::ScoreAdDependencyLatenciesPtr
                            score_ad_dependency_latencies,
                        const std::vector<std::string>& errors) {
              if (score == 1) {
                EXPECT_TRUE(debug_loss_report_url.has_value());
                EXPECT_TRUE(debug_win_report_url.has_value());
                EXPECT_EQ(GURL("https://loss.url"),
                          debug_loss_report_url.value());
                EXPECT_EQ(GURL("https://win.url"),
                          debug_win_report_url.value());
              } else {
                EXPECT_EQ(absl::nullopt, debug_loss_report_url);
                EXPECT_EQ(absl::nullopt, debug_win_report_url);
              }
              run_loop.Quit();
            })));
    run_loop.Run();
  }
}

class SellerWorkletPrivateAggregationEnabledTest : public SellerWorkletTest {
 public:
  SellerWorkletPrivateAggregationEnabledTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kPrivateAggregationApi);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SellerWorkletPrivateAggregationEnabledTest, ScoreAd) {
  mojom::PrivateAggregationRequest kExpectedRequest1(
      mojom::AggregatableReportContribution::NewHistogramContribution(
          blink::mojom::AggregatableReportHistogramContribution::New(
              /*bucket=*/123,
              /*value=*/45)),
      blink::mojom::AggregationServiceMode::kDefault,
      blink::mojom::DebugModeDetails::New());
  mojom::PrivateAggregationRequest kExpectedRequest2(
      mojom::AggregatableReportContribution::NewHistogramContribution(
          blink::mojom::AggregatableReportHistogramContribution::New(
              /*bucket=*/absl::MakeInt128(/*high=*/1, /*low=*/0),
              /*value=*/1)),
      blink::mojom::AggregationServiceMode::kDefault,
      blink::mojom::DebugModeDetails::New());

  mojom::PrivateAggregationRequest kExpectedForEventRequest1(
      mojom::AggregatableReportContribution::NewForEventContribution(
          mojom::AggregatableReportForEventContribution::New(
              /*bucket=*/mojom::ForEventSignalBucket::NewIdBucket(234),
              /*value=*/mojom::ForEventSignalValue::NewIntValue(56),
              /*event_type=*/"reserved.win")),
      blink::mojom::AggregationServiceMode::kDefault,
      blink::mojom::DebugModeDetails::New());
  mojom::PrivateAggregationRequest kExpectedForEventRequest2(
      mojom::AggregatableReportContribution::NewForEventContribution(
          mojom::AggregatableReportForEventContribution::New(
              /*bucket=*/mojom::ForEventSignalBucket::NewIdBucket(
                  absl::MakeInt128(/*high=*/1,
                                   /*low=*/0)),
              /*value=*/mojom::ForEventSignalValue::NewIntValue(2),
              /*event_type=*/"reserved.win")),
      blink::mojom::AggregationServiceMode::kDefault,
      blink::mojom::DebugModeDetails::New());

  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(kExpectedRequest1.Clone());
    expected_pa_requests.push_back(kExpectedForEventRequest1.Clone());

    RunScoreAdWithJavascriptExpectingResult(
        CreateScoreAdScript("5", R"(
          privateAggregation.contributeToHistogram({bucket: 123n, value: 45});
          privateAggregation.contributeToHistogramOnEvent(
              "reserved.win", {bucket: 234n, value: 56});
        )"),
        5, /*expected_errors=*/{},
        mojom::ComponentAuctionModifiedBidParamsPtr(),
        /*expected_data_version=*/absl::nullopt,
        /*expected_debug_loss_report_url=*/absl::nullopt,
        /*expected_debug_win_report_url=*/absl::nullopt,
        /*expected_reject_reason=*/mojom::RejectReason::kNotAvailable,
        std::move(expected_pa_requests));
  }

  // Set the private-aggregation permissions policy to disallowed.
  {
    permissions_policy_state_ =
        mojom::AuctionWorkletPermissionsPolicyState::New(
            /*private_aggregation_allowed=*/false,
            /*shared_storage_allowed=*/false);

    RunScoreAdWithJavascriptExpectingResult(
        CreateScoreAdScript("5",
                            "privateAggregation.contributeToHistogram({bucket: "
                            "123n, value: 45})"),
        /*expected_score=*/0, /*expected_errors=*/
        {"https://url.test/:4 Uncaught TypeError: The \"private-aggregation\" "
         "Permissions Policy denied the method on privateAggregation."},
        mojom::ComponentAuctionModifiedBidParamsPtr(),
        /*expected_data_version=*/absl::nullopt,
        /*expected_debug_loss_report_url=*/absl::nullopt,
        /*expected_debug_win_report_url=*/absl::nullopt,
        /*expected_reject_reason=*/mojom::RejectReason::kNotAvailable,
        /*expected_pa_requests=*/{});

    permissions_policy_state_ =
        mojom::AuctionWorkletPermissionsPolicyState::New(
            /*private_aggregation_allowed=*/true,
            /*shared_storage_allowed=*/false);
  }

  // Large bucket
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(kExpectedRequest2.Clone());
    expected_pa_requests.push_back(kExpectedForEventRequest2.Clone());

    RunScoreAdWithJavascriptExpectingResult(
        CreateScoreAdScript("5", R"(
          privateAggregation.contributeToHistogram(
              {bucket: 18446744073709551616n, value: 1});
          privateAggregation.contributeToHistogramOnEvent(
              "reserved.win", {bucket: 18446744073709551616n, value: 2});
        )"),
        5, /*expected_errors=*/{},
        mojom::ComponentAuctionModifiedBidParamsPtr(),
        /*expected_data_version=*/absl::nullopt,
        /*expected_debug_loss_report_url=*/absl::nullopt,
        /*expected_debug_win_report_url=*/absl::nullopt,
        /*expected_reject_reason=*/mojom::RejectReason::kNotAvailable,
        std::move(expected_pa_requests));
  }

  // Multiple requests
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(kExpectedRequest1.Clone());
    expected_pa_requests.push_back(kExpectedRequest2.Clone());
    expected_pa_requests.push_back(kExpectedForEventRequest1.Clone());
    expected_pa_requests.push_back(kExpectedForEventRequest2.Clone());

    RunScoreAdWithJavascriptExpectingResult(
        CreateScoreAdScript("5", R"(
          privateAggregation.contributeToHistogram({bucket: 123n, value: 45});
          privateAggregation.contributeToHistogram(
              {bucket: 18446744073709551616n, value: 1});
          privateAggregation.contributeToHistogramOnEvent(
              "reserved.win", {bucket: 234n, value: 56});
          privateAggregation.contributeToHistogramOnEvent(
              "reserved.win", {bucket: 18446744073709551616n, value: 2});
        )"),
        5, /*expected_errors=*/{},
        mojom::ComponentAuctionModifiedBidParamsPtr(),
        /*expected_data_version=*/absl::nullopt,
        /*expected_debug_loss_report_url=*/absl::nullopt,
        /*expected_debug_win_report_url=*/absl::nullopt,
        /*expected_reject_reason=*/mojom::RejectReason::kNotAvailable,
        std::move(expected_pa_requests));
  }

  // An unrelated exception after contributeToHistogram and
  // contributeToHistogramOnEvent shouldn't block the reports.
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(kExpectedRequest1.Clone());
    expected_pa_requests.push_back(kExpectedForEventRequest1.Clone());

    RunScoreAdWithJavascriptExpectingResult(
        CreateScoreAdScript("5", R"(
          privateAggregation.contributeToHistogram({bucket: 123n, value: 45});
          privateAggregation.contributeToHistogramOnEvent(
              "reserved.win", {bucket: 234n, value: 56});
          error;
        )"),
        0, /*expected_errors=*/
        {"https://url.test/:8 Uncaught ReferenceError: error is not defined."},
        mojom::ComponentAuctionModifiedBidParamsPtr(),
        /*expected_data_version=*/absl::nullopt,
        /*expected_debug_loss_report_url=*/absl::nullopt,
        /*expected_debug_win_report_url=*/absl::nullopt,
        /*expected_reject_reason=*/mojom::RejectReason::kNotAvailable,
        std::move(expected_pa_requests));
  }

  // Debug mode enabled with debug key
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(mojom::PrivateAggregationRequest::New(
        kExpectedRequest1.contribution->Clone(),
        blink::mojom::AggregationServiceMode::kDefault,
        blink::mojom::DebugModeDetails::New(
            /*is_enabled=*/true, blink::mojom::DebugKey::New(1234u))));
    expected_pa_requests.push_back(mojom::PrivateAggregationRequest::New(
        kExpectedForEventRequest1.contribution->Clone(),
        blink::mojom::AggregationServiceMode::kDefault,
        blink::mojom::DebugModeDetails::New(
            /*is_enabled=*/true, blink::mojom::DebugKey::New(1234u))));

    RunScoreAdWithJavascriptExpectingResult(
        CreateScoreAdScript("5",
                            R"(
            privateAggregation.enableDebugMode({debugKey: 1234n});
            privateAggregation.contributeToHistogram({bucket: 123n, value: 45});
            privateAggregation.contributeToHistogramOnEvent(
              "reserved.win", {bucket: 234n, value: 56});
          )"),
        5, /*expected_errors=*/{},
        mojom::ComponentAuctionModifiedBidParamsPtr(),
        /*expected_data_version=*/absl::nullopt,
        /*expected_debug_loss_report_url=*/absl::nullopt,
        /*expected_debug_win_report_url=*/absl::nullopt,
        /*expected_reject_reason=*/mojom::RejectReason::kNotAvailable,
        std::move(expected_pa_requests));
  }

  // Debug mode enabled without debug key, but with multiple requests
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(mojom::PrivateAggregationRequest::New(
        kExpectedRequest1.contribution->Clone(),
        blink::mojom::AggregationServiceMode::kDefault,
        blink::mojom::DebugModeDetails::New(
            /*is_enabled=*/true, /*debug_key=*/nullptr)));
    expected_pa_requests.push_back(mojom::PrivateAggregationRequest::New(
        kExpectedRequest2.contribution->Clone(),
        blink::mojom::AggregationServiceMode::kDefault,
        blink::mojom::DebugModeDetails::New(
            /*is_enabled=*/true, /*debug_key=*/nullptr)));

    RunScoreAdWithJavascriptExpectingResult(
        CreateScoreAdScript("5",
                            R"(
            privateAggregation.enableDebugMode();
            privateAggregation.contributeToHistogram({bucket: 123n, value: 45});
            privateAggregation.contributeToHistogram(
                {bucket: 18446744073709551616n, value: 1});
          )"),
        5, /*expected_errors=*/{},
        mojom::ComponentAuctionModifiedBidParamsPtr(),
        /*expected_data_version=*/absl::nullopt,
        /*expected_debug_loss_report_url=*/absl::nullopt,
        /*expected_debug_win_report_url=*/absl::nullopt,
        /*expected_reject_reason=*/mojom::RejectReason::kNotAvailable,
        std::move(expected_pa_requests));
  }
}

TEST_F(SellerWorkletPrivateAggregationEnabledTest, ReportResult) {
  mojom::PrivateAggregationRequest kExpectedRequest1(
      mojom::AggregatableReportContribution::NewHistogramContribution(
          blink::mojom::AggregatableReportHistogramContribution::New(
              /*bucket=*/123,
              /*value=*/45)),
      blink::mojom::AggregationServiceMode::kDefault,
      blink::mojom::DebugModeDetails::New());
  mojom::PrivateAggregationRequest kExpectedRequest2(
      mojom::AggregatableReportContribution::NewHistogramContribution(
          blink::mojom::AggregatableReportHistogramContribution::New(
              /*bucket=*/absl::MakeInt128(/*high=*/1, /*low=*/0),
              /*value=*/1)),
      blink::mojom::AggregationServiceMode::kDefault,
      blink::mojom::DebugModeDetails::New());
  mojom::PrivateAggregationRequest kExpectedForEventRequest(
      mojom::AggregatableReportContribution::NewForEventContribution(
          mojom::AggregatableReportForEventContribution::New(
              /*bucket=*/mojom::ForEventSignalBucket::NewIdBucket(234),
              /*value=*/mojom::ForEventSignalValue::NewIntValue(56),
              /*event_type=*/"reserved.win")),
      blink::mojom::AggregationServiceMode::kDefault,
      blink::mojom::DebugModeDetails::New());

  // Only contributeToHistogram() is called.
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(kExpectedRequest1.Clone());

    RunReportResultCreatedScriptExpectingResult(
        R"(5)",
        R"(
          privateAggregation.contributeToHistogram({bucket: 123n, value: 45});
        )",
        /*expected_signals_for_winner=*/"5",
        /*expected_report_url=*/absl::nullopt, /*expected_ad_beacon_map=*/{},
        std::move(expected_pa_requests),
        /*expected_errors=*/{});
  }

  // Only contributeToHistogramOnEvent() is called.
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(kExpectedForEventRequest.Clone());

    RunReportResultCreatedScriptExpectingResult(
        "5",
        R"(
          privateAggregation.contributeToHistogramOnEvent(
              "reserved.win", {bucket: 234n, value: 56});
        )",
        /*expected_signals_for_winner=*/"5",
        /*expected_report_url=*/absl::nullopt, /*expected_ad_beacon_map=*/{},
        std::move(expected_pa_requests),
        /*expected_errors=*/{});
  }

  // Both contributeToHistogram() and contributeToHistogramOnEvent() are called.
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(kExpectedRequest1.Clone());
    expected_pa_requests.push_back(kExpectedForEventRequest.Clone());

    RunReportResultCreatedScriptExpectingResult(
        "5",
        R"(
          privateAggregation.contributeToHistogram({bucket: 123n, value: 45});
          privateAggregation.contributeToHistogramOnEvent(
              "reserved.win", {bucket: 234n, value: 56});
        )",
        /*expected_signals_for_winner=*/"5",
        /*expected_report_url=*/absl::nullopt, /*expected_ad_beacon_map=*/{},
        std::move(expected_pa_requests),
        /*expected_errors=*/{});
  }

  // Set the private-aggregation permissions policy to disallowed.
  {
    permissions_policy_state_ =
        mojom::AuctionWorkletPermissionsPolicyState::New(
            /*private_aggregation_allowed=*/false,
            /*shared_storage_allowed=*/false);

    RunReportResultCreatedScriptExpectingResult(
        R"(5)",
        R"(
          privateAggregation.contributeToHistogram({bucket: 123n, value: 45});
        )",
        /*expected_signals_for_winner=*/absl::nullopt,
        /*expected_report_url=*/absl::nullopt, /*expected_ad_beacon_map=*/{},
        /*expected_pa_requests=*/{},
        /*expected_errors=*/
        {"https://url.test/:11 Uncaught TypeError: The \"private-aggregation\" "
         "Permissions Policy denied the method on privateAggregation."});

    permissions_policy_state_ =
        mojom::AuctionWorkletPermissionsPolicyState::New(
            /*private_aggregation_allowed=*/true,
            /*shared_storage_allowed=*/false);
  }

  // BigInt bucket
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(kExpectedRequest1.Clone());

    RunReportResultCreatedScriptExpectingResult(
        R"(5)",
        R"(
          privateAggregation.contributeToHistogram({bucket: 123n, value: 45});
        )",
        /*expected_signals_for_winner=*/"5",
        /*expected_report_url=*/absl::nullopt, /*expected_ad_beacon_map=*/{},
        std::move(expected_pa_requests),
        /*expected_errors=*/{});
  }

  // Large bucket
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(kExpectedRequest2.Clone());

    RunReportResultCreatedScriptExpectingResult(
        R"(5)",
        R"(privateAggregation.contributeToHistogram(
            {bucket: 18446744073709551616n, value: 1});)",
        /*expected_signals_for_winner=*/"5",
        /*expected_report_url=*/absl::nullopt, /*expected_ad_beacon_map=*/{},
        std::move(expected_pa_requests),
        /*expected_errors=*/{});
  }

  // Multiple requests
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(kExpectedRequest1.Clone());
    expected_pa_requests.push_back(kExpectedRequest2.Clone());

    RunReportResultCreatedScriptExpectingResult(
        R"(5)",
        R"(
          privateAggregation.contributeToHistogram({bucket: 123n, value: 45});
          privateAggregation.contributeToHistogram(
              {bucket: 18446744073709551616n, value: 1});
        )",
        /*expected_signals_for_winner=*/"5",
        /*expected_report_url=*/absl::nullopt, /*expected_ad_beacon_map=*/{},
        std::move(expected_pa_requests),
        /*expected_errors=*/{});
  }

  // An unrelated exception after contributeToHistogram shouldn't block the
  // report
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(kExpectedRequest1.Clone());

    RunReportResultCreatedScriptExpectingResult(
        R"(5)",
        R"(
          privateAggregation.contributeToHistogram({bucket: 123n, value: 45});
          error;
        )",
        /*expected_signals_for_winner=*/absl::nullopt,
        /*expected_report_url=*/absl::nullopt, /*expected_ad_beacon_map=*/{},
        std::move(expected_pa_requests),
        /*expected_errors=*/
        {"https://url.test/:12 Uncaught ReferenceError: error is not "
         "defined."});
  }

  // Debug mode enabled with debug key
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(mojom::PrivateAggregationRequest::New(
        kExpectedRequest1.contribution->Clone(),
        blink::mojom::AggregationServiceMode::kDefault,
        blink::mojom::DebugModeDetails::New(
            /*is_enabled=*/true, blink::mojom::DebugKey::New(1234u))));

    RunReportResultCreatedScriptExpectingResult(
        "5",
        R"(
            privateAggregation.enableDebugMode({debugKey: 1234n});
            privateAggregation.contributeToHistogram({bucket: 123n, value: 45});
        )",
        /*expected_signals_for_winner=*/"5",
        /*expected_report_url=*/absl::nullopt, /*expected_ad_beacon_map=*/{},
        std::move(expected_pa_requests),
        /*expected_errors=*/{});
  }

  // Debug mode enabled without debug key, but with multiple requests
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(mojom::PrivateAggregationRequest::New(
        kExpectedRequest1.contribution->Clone(),
        blink::mojom::AggregationServiceMode::kDefault,
        blink::mojom::DebugModeDetails::New(
            /*is_enabled=*/true, /*debug_key=*/nullptr)));
    expected_pa_requests.push_back(mojom::PrivateAggregationRequest::New(
        kExpectedRequest2.contribution->Clone(),
        blink::mojom::AggregationServiceMode::kDefault,
        blink::mojom::DebugModeDetails::New(
            /*is_enabled=*/true, /*debug_key=*/nullptr)));

    RunReportResultCreatedScriptExpectingResult(
        "5",
        R"(
            privateAggregation.enableDebugMode();
            privateAggregation.contributeToHistogram({bucket: 123n, value: 45});
            privateAggregation.contributeToHistogram(
                {bucket: 18446744073709551616n, value: 1});
        )",
        /*expected_signals_for_winner=*/"5",
        /*expected_report_url=*/absl::nullopt, /*expected_ad_beacon_map=*/{},
        std::move(expected_pa_requests),
        /*expected_errors=*/{});
  }

  // Debug mode enabled twice
  {
    RunReportResultCreatedScriptExpectingResult(
        "5",
        R"(
            privateAggregation.enableDebugMode();
            privateAggregation.enableDebugMode();
        )",
        /*expected_signals_for_winner=*/absl::nullopt,
        /*expected_report_url=*/absl::nullopt, /*expected_ad_beacon_map=*/{},
        /*expected_pa_requests=*/{},
        /*expected_errors=*/
        {"https://url.test/:12 Uncaught TypeError: enableDebugMode may be "
         "called at most once."});
  }
}

class SellerWorkletPrivateAggregationDisabledTest : public SellerWorkletTest {
 public:
  SellerWorkletPrivateAggregationDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(
        blink::features::kPrivateAggregationApi);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SellerWorkletPrivateAggregationDisabledTest, ScoreAd) {
  RunScoreAdWithJavascriptExpectingResult(
      CreateScoreAdScript("5",
                          "privateAggregation.contributeToHistogram({bucket: "
                          "123n, value: 45})"),
      0, /*expected_errors=*/
      {"https://url.test/:4 Uncaught ReferenceError: privateAggregation is not "
       "defined."},
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/absl::nullopt,
      /*expected_debug_loss_report_url=*/absl::nullopt,
      /*expected_debug_win_report_url=*/absl::nullopt,
      /*expected_reject_reason=*/mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{});
}

TEST_F(SellerWorkletPrivateAggregationDisabledTest, ReportResult) {
  RunReportResultCreatedScriptExpectingResult(
      R"(5)",
      R"(privateAggregation.contributeToHistogram({bucket: 123n, value: 45});)",
      /*expected_signals_for_winner=*/absl::nullopt,
      /*expected_report_url=*/absl::nullopt, /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      /*expected_errors=*/
      {"https://url.test/:10 Uncaught ReferenceError: privateAggregation is "
       "not defined."});
}

}  // namespace
}  // namespace auction_worklet
