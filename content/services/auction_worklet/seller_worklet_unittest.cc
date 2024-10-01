// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/services/auction_worklet/seller_worklet.h"

#include <algorithm>
#include <memory>
#include <optional>
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
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/values_test_util.h"
#include "base/test/with_feature_override.h"
#include "base/time/time.h"
#include "components/cbor/writer.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/cpp/cbor_test_util.h"
#include "content/services/auction_worklet/public/cpp/real_time_reporting.h"
#include "content/services/auction_worklet/public/mojom/auction_network_events_handler.mojom.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom-forward.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "content/services/auction_worklet/public/mojom/real_time_reporting.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "content/services/auction_worklet/worklet_devtools_debug_test_util.h"
#include "content/services/auction_worklet/worklet_test_util.h"
#include "content/services/auction_worklet/worklet_v8_debug_test_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "net/http/http_status_code.h"
#include "net/third_party/quiche/src/quiche/oblivious_http/oblivious_http_gateway.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/ad_auction_currencies.h"
#include "third_party/blink/public/common/interest_group/ad_display_size.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

using testing::HasSubstr;
using testing::StartsWith;

namespace auction_worklet {
namespace {

using PrivateAggregationRequests = SellerWorklet::PrivateAggregationRequests;
using RealTimeReportingContributions =
    SellerWorklet::RealTimeReportingContributions;

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

const uint8_t kTestPrivateKey[] = {
    0xff, 0x1f, 0x47, 0xb1, 0x68, 0xb6, 0xb9, 0xea, 0x65, 0xf7, 0x97,
    0x4f, 0xf2, 0x2e, 0xf2, 0x36, 0x94, 0xe2, 0xf6, 0xb6, 0x8d, 0x66,
    0xf3, 0xa7, 0x64, 0x14, 0x28, 0xd4, 0x45, 0x35, 0x01, 0x8f,
};

const uint8_t kTestPublicKey[] = {
    0xa1, 0x5f, 0x40, 0x65, 0x86, 0xfa, 0xc4, 0x7b, 0x99, 0x59, 0x70,
    0xf1, 0x85, 0xd9, 0xd8, 0x91, 0xc7, 0x4d, 0xcf, 0x1e, 0xb9, 0x1a,
    0x7d, 0x50, 0xa5, 0x8b, 0x01, 0x68, 0x3e, 0x60, 0x05, 0x2d,
};

const uint8_t kKeyId = 0xFF;

// Creates a seller script with scoreAd() returning the specified expression.
// Allows using scoreAd() arguments, arbitrary values, incorrect types, etc.
std::string CreateScoreAdScript(const std::string& raw_return_value,
                                const std::string& extra_code = std::string()) {
  constexpr char kSellAdScript[] = R"(
    function scoreAd(adMetadata, bid, auctionConfig, trustedScoringSignals,
        browserSignals, directFromSellerSignals, crossOriginTrustedSignals) {
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
std::string CreateReportToScript(
    const std::string& raw_return_value,
    const std::string& extra_code = std::string()) {
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
      std::optional<double> bid_in_seller_currency,
      std::optional<uint32_t> scoring_signals_data_version,
      const std::optional<GURL>& debug_loss_report_url,
      const std::optional<GURL>& debug_win_report_url,
      PrivateAggregationRequests pa_requests,
      RealTimeReportingContributions real_time_contributions,
      mojom::SellerTimingMetricsPtr score_ad_timing_metrics,
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
      std::optional<double> bid_in_seller_currency,
      std::optional<uint32_t> scoring_signals_data_version,
      const std::optional<GURL>& debug_loss_report_url,
      const std::optional<GURL>& debug_win_report_url,
      PrivateAggregationRequests pa_requests,
      RealTimeReportingContributions real_time_contributions,
      mojom::SellerTimingMetricsPtr score_ad_timing_metrics,
      mojom::ScoreAdDependencyLatenciesPtr score_ad_dependency_latencies,
      const std::vector<std::string>& errors) override {
    std::move(score_ad_complete_callback_)
        .Run(score, reject_reason,
             std::move(component_auction_modified_bid_params),
             std::move(bid_in_seller_currency),
             std::move(scoring_signals_data_version), debug_loss_report_url,
             debug_win_report_url, std::move(pa_requests),
             std::move(real_time_contributions),
             std::move(score_ad_timing_metrics),
             std::move(score_ad_dependency_latencies), errors);
  }

  static ScoreAdCompleteCallback ScoreAdNeverInvokedCallback() {
    return base::BindOnce(
        [](double score, mojom::RejectReason reject_reason,
           mojom::ComponentAuctionModifiedBidParamsPtr
               component_auction_modified_bid_params,
           std::optional<double> bid_in_seller_currency,
           std::optional<uint32_t> scoring_signals_data_version,
           const std::optional<GURL>& debug_loss_report_url,
           const std::optional<GURL>& debug_win_report_url,
           PrivateAggregationRequests pa_requests,
           RealTimeReportingContributions real_time_contributions,
           mojom::SellerTimingMetricsPtr score_ad_timing_metrics,
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
    // The v8_helpers need to be created here instead of the constructor,
    // because this test fixture has a subclass that initializes a
    // ScopedFeatureList in in their constructor, which needs to be done BEFORE
    // other threads are started in multithreaded test environments so that no
    // other threads use it when it's being initiated.
    // https://source.chromium.org/chromium/chromium/src/+/main:base/test/scoped_feature_list.h;drc=60124005e97ae2716b0fb34187d82da6019b571f;l=37
    while (v8_helpers_.size() < NumThreads()) {
      v8_helpers_.push_back(
          AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner()));
    }

    shared_storage_hosts_.resize(NumThreads());
  }

  void TearDown() override {
    // Release the V8 helper and process all pending tasks. This is to make sure
    // there aren't any pending tasks between the V8 thread and the main thread
    // that will result in UAFs. These lines are not necessary for any test to
    // pass. This needs to be done before a subclass resets ScopedFeatureList,
    // so no thread queries it while it's being modified.
    v8_helpers_.clear();
    task_environment_.RunUntilIdle();

    // In all tests where the SellerWorklet receiver is closed before the
    // remote, the disconnect reason should be consumed and validated.
    EXPECT_FALSE(disconnect_reason_);
  }

  virtual size_t NumThreads() { return 1u; }

  // Sets default values for scoreAd() and report_result() arguments. No test
  // actually depends on these being anything but valid, but this does allow
  // tests to reset them to a consistent state.
  void SetDefaultParameters() {
    ad_metadata_ = "[1]";
    bid_ = 1;
    bid_currency_ = std::nullopt;
    decision_logic_url_ = GURL("https://url.test/");
    trusted_scoring_signals_url_.reset();
    auction_ad_config_non_shared_params_ =
        blink::AuctionConfig::NonSharedParams();

    top_window_origin_ = url::Origin::Create(GURL("https://window.test/"));
    permissions_policy_state_ =
        mojom::AuctionWorkletPermissionsPolicyState::New(
            /*private_aggregation_allowed=*/true,
            /*shared_storage_allowed=*/false);
    experiment_group_id_ = std::nullopt;
    public_key_ = nullptr;
    browser_signals_other_seller_.reset();
    component_expect_bid_currency_ = std::nullopt;
    browser_signal_interest_group_owner_ =
        url::Origin::Create(GURL("https://interest.group.owner.test/"));
    browser_signal_buyer_and_seller_reporting_id_ = std::nullopt;
    browser_signal_selected_buyer_and_seller_reporting_id_ = std::nullopt;
    browser_signal_render_url_ = GURL("https://render.url.test/");
    browser_signal_ad_components_.clear();
    browser_signal_bidding_duration_msecs_ = 0;
    browser_signal_render_size_ = std::nullopt;
    browser_signal_for_debugging_only_in_cooldown_or_lockout_ = false;
    browser_signal_desireability_ = 1;
    seller_timeout_ = std::nullopt;
    bidder_joining_origin_ = url::Origin::Create(GURL("https://joining.test/"));
    browser_signal_highest_scoring_other_bid_ = 0;
    browser_signal_highest_scoring_other_bid_currency_ = std::nullopt;
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
      std::optional<uint32_t> expected_data_version = std::nullopt,
      const std::optional<GURL>& expected_debug_loss_report_url = std::nullopt,
      const std::optional<GURL>& expected_debug_win_report_url = std::nullopt,
      mojom::RejectReason expected_reject_reason =
          mojom::RejectReason::kNotAvailable,
      PrivateAggregationRequests expected_pa_requests = {},
      RealTimeReportingContributions expected_real_time_contributions = {},
      std::optional<double> expected_bid_in_seller_currency = std::nullopt) {
    RunScoreAdWithJavascriptExpectingResult(
        CreateScoreAdScript(raw_return_value), expected_score, expected_errors,
        std::move(expected_component_auction_modified_bid_params),
        expected_data_version, expected_debug_loss_report_url,
        expected_debug_win_report_url, expected_reject_reason,
        std::move(expected_pa_requests),
        std::move(expected_real_time_contributions),
        expected_bid_in_seller_currency);
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
      std::optional<uint32_t> expected_data_version = std::nullopt,
      const std::optional<GURL>& expected_debug_loss_report_url = std::nullopt,
      const std::optional<GURL>& expected_debug_win_report_url = std::nullopt,
      mojom::RejectReason expected_reject_reason =
          mojom::RejectReason::kNotAvailable,
      PrivateAggregationRequests expected_pa_requests = {},
      RealTimeReportingContributions expected_real_time_contributions = {},
      std::optional<double> expected_bid_in_seller_currency = std::nullopt) {
    AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                          CreateScoreAdScript(raw_return_value),
                          extra_js_headers_);
    auto seller_worklet = CreateWorklet();

    base::RunLoop run_loop;
    RunScoreAdOnWorkletAsync(
        seller_worklet.get(), expected_score, expected_errors,
        std::move(expected_component_auction_modified_bid_params),
        expected_data_version, expected_debug_loss_report_url,
        expected_debug_win_report_url, expected_reject_reason,
        std::move(expected_pa_requests),
        std::move(expected_real_time_contributions),
        expected_bid_in_seller_currency,
        /*expected_score_ad_timeout=*/false,
        /*expected_signals_fetch_latency=*/std::nullopt,
        /*expected_code_ready_latency=*/std::nullopt, run_loop.QuitClosure());
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
      std::optional<uint32_t> expected_data_version = std::nullopt,
      const std::optional<GURL>& expected_debug_loss_report_url = std::nullopt,
      const std::optional<GURL>& expected_debug_win_report_url = std::nullopt,
      mojom::RejectReason expected_reject_reason =
          mojom::RejectReason::kNotAvailable,
      PrivateAggregationRequests expected_pa_requests = {},
      RealTimeReportingContributions expected_real_time_contributions = {},
      std::optional<double> expected_bid_in_seller_currency = std::nullopt) {
    SCOPED_TRACE(javascript);
    AddJavascriptResponse(&url_loader_factory_, decision_logic_url_, javascript,
                          extra_js_headers_);
    RunScoreAdExpectingResult(
        expected_score, expected_errors,
        std::move(expected_component_auction_modified_bid_params),
        expected_data_version, expected_debug_loss_report_url,
        expected_debug_win_report_url, expected_reject_reason,
        std::move(expected_pa_requests),
        std::move(expected_real_time_contributions),
        expected_bid_in_seller_currency);
  }

  // Runs score_ad() script, checking result and invoking provided closure
  // when done. Something else must spin the event loop.
  void RunScoreAdOnWorkletAsync(
      mojom::SellerWorklet* seller_worklet,
      double expected_score,
      const std::vector<std::string>& expected_errors,
      mojom::ComponentAuctionModifiedBidParamsPtr
          expected_component_auction_modified_bid_params,
      std::optional<uint32_t> expected_data_version,
      const std::optional<GURL>& expected_debug_loss_report_url,
      const std::optional<GURL>& expected_debug_win_report_url,
      mojom::RejectReason expected_reject_reason,
      PrivateAggregationRequests expected_pa_requests,
      RealTimeReportingContributions expected_real_time_contributions,
      std::optional<double> expected_bid_in_seller_currency,
      bool expected_score_ad_timeout,
      std::optional<base::TimeDelta> expected_signals_fetch_latency,
      std::optional<base::TimeDelta> expected_code_ready_latency,
      base::OnceClosure done_closure) {
    seller_worklet->ScoreAd(
        ad_metadata_, bid_, bid_currency_, auction_ad_config_non_shared_params_,
        direct_from_seller_seller_signals_,
        direct_from_seller_seller_signals_header_ad_slot_,
        direct_from_seller_auction_signals_,
        direct_from_seller_auction_signals_header_ad_slot_,
        browser_signals_other_seller_.Clone(), component_expect_bid_currency_,
        browser_signal_interest_group_owner_, browser_signal_render_url_,
        browser_signal_selected_buyer_and_seller_reporting_id_,
        browser_signal_buyer_and_seller_reporting_id_,
        browser_signal_ad_components_, browser_signal_bidding_duration_msecs_,
        browser_signal_render_size_,
        browser_signal_for_debugging_only_in_cooldown_or_lockout_,
        seller_timeout_,
        /*trace_id=*/1, bidder_joining_origin_,
        TestScoreAdClient::Create(base::BindOnce(
            [](double expected_score,
               mojom::RejectReason expected_reject_reason,
               mojom::ComponentAuctionModifiedBidParamsPtr
                   expected_component_auction_modified_bid_params,
               std::optional<uint32_t> expected_data_version,
               const std::optional<GURL>& expected_debug_loss_report_url,
               const std::optional<GURL>& expected_debug_win_report_url,
               PrivateAggregationRequests expected_pa_requests,
               RealTimeReportingContributions expected_real_time_contributions,
               std::optional<double> expected_bid_in_seller_currency,
               std::optional<base::TimeDelta> expected_score_ad_timeout,
               std::optional<base::TimeDelta> expected_signals_fetch_latency,
               std::optional<base::TimeDelta> expected_code_ready_latency,
               std::vector<std::string> expected_errors,
               base::OnceClosure done_closure, double score,
               mojom::RejectReason reject_reason,
               mojom::ComponentAuctionModifiedBidParamsPtr
                   component_auction_modified_bid_params,
               std::optional<double> bid_in_seller_currency,
               std::optional<uint32_t> scoring_signals_data_version,
               const std::optional<GURL>& debug_loss_report_url,
               const std::optional<GURL>& debug_win_report_url,
               PrivateAggregationRequests pa_requests,
               RealTimeReportingContributions real_time_contributions,
               mojom::SellerTimingMetricsPtr score_ad_timing_metrics,
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
                EXPECT_EQ(expected_component_auction_modified_bid_params->bid,
                          component_auction_modified_bid_params->bid);
              }
              EXPECT_EQ(expected_debug_loss_report_url, debug_loss_report_url);
              EXPECT_EQ(expected_debug_win_report_url, debug_win_report_url);
              EXPECT_EQ(expected_data_version, scoring_signals_data_version);
              EXPECT_EQ(expected_pa_requests, pa_requests);
              EXPECT_EQ(expected_real_time_contributions,
                        real_time_contributions);
              EXPECT_EQ(expected_bid_in_seller_currency,
                        bid_in_seller_currency);
              if (expected_score_ad_timeout) {
                // We only know that about the time of the timeout should have
                // elapsed, and there may also be some thread skew.
                EXPECT_GE(score_ad_timing_metrics->script_latency,
                          expected_score_ad_timeout.value() * 0.9);
              }
              EXPECT_EQ(expected_score_ad_timeout.has_value(),
                        score_ad_timing_metrics->script_timed_out);
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
            std::move(expected_real_time_contributions),
            expected_bid_in_seller_currency,
            expected_score_ad_timeout
                ? std::make_optional(
                      seller_timeout_.value_or(AuctionV8Helper::kScriptTimeout))
                : std::nullopt,
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
        browser_signal_selected_buyer_and_seller_reporting_id_,
        browser_signal_buyer_and_seller_reporting_id_,
        browser_signal_ad_components_, browser_signal_bidding_duration_msecs_,
        browser_signal_render_size_,
        browser_signal_for_debugging_only_in_cooldown_or_lockout_,
        seller_timeout_,
        /*trace_id=*/1, bidder_joining_origin_,
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
      std::optional<uint32_t> expected_data_version = std::nullopt,
      const std::optional<GURL>& expected_debug_loss_report_url = std::nullopt,
      const std::optional<GURL>& expected_debug_win_report_url = std::nullopt,
      mojom::RejectReason expected_reject_reason =
          mojom::RejectReason::kNotAvailable,
      PrivateAggregationRequests expected_pa_requests = {},
      RealTimeReportingContributions expected_real_time_contributions = {},
      std::optional<double> expected_bid_in_seller_currency = std::nullopt) {
    base::RunLoop run_loop;
    RunScoreAdOnWorkletAsync(
        seller_worklet, expected_score, expected_errors,
        std::move(expected_component_auction_modified_bid_params),
        expected_data_version, expected_debug_loss_report_url,
        expected_debug_win_report_url, expected_reject_reason,
        std::move(expected_pa_requests),
        std::move(expected_real_time_contributions),
        expected_bid_in_seller_currency,
        /*expected_score_ad_timeout=*/false,
        /*expected_signals_fetch_latency=*/std::nullopt,
        /*expected_code_ready_latency=*/std::nullopt, run_loop.QuitClosure());
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
      std::optional<uint32_t> expected_data_version = std::nullopt,
      const std::optional<GURL>& expected_debug_loss_report_url = std::nullopt,
      const std::optional<GURL>& expected_debug_win_report_url = std::nullopt,
      mojom::RejectReason expected_reject_reason =
          mojom::RejectReason::kNotAvailable,
      PrivateAggregationRequests expected_pa_requests = {},
      RealTimeReportingContributions expected_real_time_contributions = {},
      std::optional<double> expected_bid_in_seller_currency = std::nullopt) {
    auto seller_worklet = CreateWorklet();
    ASSERT_TRUE(seller_worklet);
    RunScoreAdExpectingResultOnWorklet(
        seller_worklet.get(), expected_score, expected_errors,
        std::move(expected_component_auction_modified_bid_params),
        expected_data_version, expected_debug_loss_report_url,
        expected_debug_win_report_url, expected_reject_reason,
        std::move(expected_pa_requests),
        std::move(expected_real_time_contributions),
        expected_bid_in_seller_currency);
  }

  // Configures `url_loader_factory_` to return a report_result() script created
  // with CreateReportToScript(). Then runs the script, expecting the provided
  // result.
  void RunReportResultCreatedScriptExpectingResult(
      const std::string& raw_return_value,
      const std::string& extra_code,
      const std::optional<std::string>& expected_signals_for_winner,
      const std::optional<GURL>& expected_report_url,
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
      const std::optional<std::string>& expected_signals_for_winner,
      const std::optional<GURL>& expected_report_url,
      const base::flat_map<std::string, GURL>& expected_ad_beacon_map =
          base::flat_map<std::string, GURL>(),
      PrivateAggregationRequests expected_pa_requests = {},
      const std::vector<std::string>& expected_errors =
          std::vector<std::string>()) {
    SCOPED_TRACE(javascript);
    AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                          javascript);
    RunReportResultExpectingResult(
        expected_signals_for_winner, expected_report_url,
        expected_ad_beacon_map, std::move(expected_pa_requests),
        /*expected_reporting_latency_timeout=*/false, expected_errors);
  }

  // Loads and runs a report_result() script, expecting the supplied result.
  // Caller is responsible for spinning the event loop at least until
  // `done_closure`.
  void RunReportResultExpectingResultAsync(
      mojom::SellerWorklet* seller_worklet,
      const std::optional<std::string>& expected_signals_for_winner,
      const std::optional<GURL>& expected_report_url,
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
        browser_signal_selected_buyer_and_seller_reporting_id_,
        browser_signal_render_url_, bid_, bid_currency_,
        browser_signal_desireability_,
        browser_signal_highest_scoring_other_bid_,
        browser_signal_highest_scoring_other_bid_currency_,
        browser_signals_component_auction_report_result_params_.Clone(),
        browser_signal_data_version_,
        /*trace_id=*/1,
        base::BindOnce(
            [](const std::optional<std::string>& expected_signals_for_winner,
               const std::optional<GURL>& expected_report_url,
               const base::flat_map<std::string, GURL>& expected_ad_beacon_map,
               PrivateAggregationRequests expected_pa_requests,
               bool expected_reporting_latency_timeout,
               std::optional<base::TimeDelta> reporting_timeout,
               const std::vector<std::string>& expected_errors,
               base::OnceClosure done_closure,
               const std::optional<std::string>& signals_for_winner,
               const std::optional<GURL>& report_url,
               const base::flat_map<std::string, GURL>& ad_beacon_map,
               PrivateAggregationRequests pa_requests,
               auction_worklet::mojom::SellerTimingMetricsPtr timing_metrics,
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
                EXPECT_GE(timing_metrics->script_latency,
                          (reporting_timeout.has_value()
                               ? reporting_timeout.value()
                               : AuctionV8Helper::kScriptTimeout) *
                              0.9);
              }
              EXPECT_EQ(expected_reporting_latency_timeout,
                        timing_metrics->script_timed_out);
              EXPECT_EQ(expected_errors, errors);
              std::move(done_closure).Run();
            },
            expected_signals_for_winner, expected_report_url,
            expected_ad_beacon_map, std::move(expected_pa_requests),
            expected_reporting_latency_timeout,
            auction_ad_config_non_shared_params_.reporting_timeout,
            expected_errors, std::move(done_closure)));
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
        browser_signal_selected_buyer_and_seller_reporting_id_,
        browser_signal_render_url_, bid_, bid_currency_,
        browser_signal_desireability_,
        browser_signal_highest_scoring_other_bid_,
        browser_signal_highest_scoring_other_bid_currency_,
        browser_signals_component_auction_report_result_params_.Clone(),
        browser_signal_data_version_,
        /*trace_id=*/1,
        base::BindOnce(
            [](const std::optional<std::string>& signals_for_winner,
               const std::optional<GURL>& report_url,
               const base::flat_map<std::string, GURL>& ad_beacon_map,
               PrivateAggregationRequests pa_requests,
               auction_worklet::mojom::SellerTimingMetricsPtr timing_metrics,
               const std::vector<std::string>& errors) {
              ADD_FAILURE() << "This should not be invoked";
            }));
  }

  // Loads and runs a report_result() script, expecting the supplied result.
  void RunReportResultExpectingResult(
      const std::optional<std::string>& expected_signals_for_winner,
      const std::optional<GURL>& expected_report_url,
      const base::flat_map<std::string, GURL>& expected_ad_beacon_map =
          base::flat_map<std::string, GURL>(),
      PrivateAggregationRequests expected_pa_requests = {},
      bool expected_reporting_latency_timeout = false,
      const std::vector<std::string>& expected_errors =
          std::vector<std::string>()) {
    auto seller_worklet = CreateWorklet();
    ASSERT_TRUE(seller_worklet);

    base::RunLoop run_loop;
    RunReportResultExpectingResultAsync(
        seller_worklet.get(), expected_signals_for_winner, expected_report_url,
        expected_ad_beacon_map, std::move(expected_pa_requests),
        expected_reporting_latency_timeout, expected_errors,
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

    CHECK_EQ(v8_helpers_.size(), shared_storage_hosts_.size());

    mojo::Remote<mojom::SellerWorklet> seller_worklet;
    auto seller_worklet_impl = std::make_unique<SellerWorklet>(
        v8_helpers_, std::move(shared_storage_hosts_),
        pause_for_debugger_on_start, std::move(url_loader_factory),
        auction_network_events_handler_.CreateRemote(), decision_logic_url_,
        trusted_scoring_signals_url_, top_window_origin_,
        permissions_policy_state_.Clone(), experiment_group_id_,
        public_key_ ? public_key_.Clone() : nullptr,
        base::BindRepeating(&SellerWorkletTest::GetNextThreadIndex,
                            base::Unretained(this)));

    shared_storage_hosts_.resize(NumThreads());

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

  // Use a round-robin scheduling. The state is shared across worklets in the
  // same test case.
  size_t GetNextThreadIndex() { return next_thread_index_++ % NumThreads(); }

  scoped_refptr<AuctionV8Helper> v8_helper() { return v8_helpers_[0]; }

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
    LOG(WARNING) << "Worklet disconnect with reason: " << description;

    disconnect_reason_ = description;
    if (disconnect_run_loop_) {
      disconnect_run_loop_->Quit();
    }
  }

  base::test::TaskEnvironment task_environment_;

  // Extra headers to append to replies for JavaScript resources.
  std::optional<std::string> extra_js_headers_;

  // Arguments passed to score_bid() and report_result(). Arguments common to
  // both of them use the same field.
  //
  // NOTE: For each new GURL field, ScoreAdLoadCompletionOrder /
  // ReportResultLoadCompletionOrder should be updated.
  std::string ad_metadata_;
  // `bid_` is a browser signal for report_result(), but a direct parameter for
  // score_bid().
  double bid_;
  std::optional<blink::AdCurrency> bid_currency_;
  GURL decision_logic_url_;
  std::optional<GURL> trusted_scoring_signals_url_;
  blink::AuctionConfig::NonSharedParams auction_ad_config_non_shared_params_;
  std::optional<GURL> direct_from_seller_seller_signals_;
  std::optional<std::string> direct_from_seller_seller_signals_header_ad_slot_;
  std::optional<GURL> direct_from_seller_auction_signals_;
  std::optional<std::string> direct_from_seller_auction_signals_header_ad_slot_;
  url::Origin top_window_origin_;
  mojom::AuctionWorkletPermissionsPolicyStatePtr permissions_policy_state_;
  std::optional<uint16_t> experiment_group_id_;
  mojom::TrustedSignalsPublicKeyPtr public_key_;
  mojom::ComponentAuctionOtherSellerPtr browser_signals_other_seller_;
  std::optional<blink::AdCurrency> component_expect_bid_currency_;
  url::Origin browser_signal_interest_group_owner_;
  GURL browser_signal_render_url_;
  std::optional<std::string>
      browser_signal_selected_buyer_and_seller_reporting_id_;
  std::optional<std::string> browser_signal_buyer_and_seller_reporting_id_;
  std::vector<GURL> browser_signal_ad_components_;
  uint32_t browser_signal_bidding_duration_msecs_;
  std::optional<blink::AdSize> browser_signal_render_size_;
  bool browser_signal_for_debugging_only_in_cooldown_or_lockout_;
  double browser_signal_desireability_;
  double browser_signal_highest_scoring_other_bid_;
  std::optional<blink::AdCurrency>
      browser_signal_highest_scoring_other_bid_currency_;
  mojom::ComponentAuctionReportResultParamsPtr
      browser_signals_component_auction_report_result_params_;
  std::optional<uint32_t> browser_signal_data_version_;
  std::optional<base::TimeDelta> seller_timeout_;
  url::Origin bidder_joining_origin_;

  // Reuseable run loop for disconnection errors.
  std::unique_ptr<base::RunLoop> disconnect_run_loop_;
  std::optional<std::string> disconnect_reason_;

  network::TestURLLoaderFactory url_loader_factory_;
  network::TestURLLoaderFactory alternate_url_loader_factory_;

  std::vector<scoped_refptr<AuctionV8Helper>> v8_helpers_;

  TestAuctionNetworkEventsHandler auction_network_events_handler_;

  std::vector<mojo::PendingRemote<mojom::AuctionSharedStorageHost>>
      shared_storage_hosts_;

  size_t next_thread_index_ = 0;

  // Owns all created seller worklets - having a ReceiverSet allows them to have
  // a ClosePipeCallback which behaves just like the one in
  // AuctionWorkletServiceImpl, to better match production behavior.
  mojo::UniqueReceiverSet<mojom::SellerWorklet> seller_worklets_;
};

class SellerWorkletCrossOriginTrustedSignalsDisabledTest
    : public SellerWorkletTest {
 public:
  SellerWorkletCrossOriginTrustedSignalsDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(
        blink::features::kFledgePermitCrossOriginTrustedSignals);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class SellerWorkletTwoThreadsTest : public SellerWorkletTest {
 private:
  size_t NumThreads() override { return 2u; }
};

class SellerWorkletMultiThreadingTest
    : public SellerWorkletTest,
      public testing::WithParamInterface<size_t> {
 private:
  size_t NumThreads() override { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         SellerWorkletMultiThreadingTest,
                         testing::Values(1, 2),
                         [](const auto& info) {
                           return base::StrCat({info.param == 2
                                                    ? "TwoThreads"
                                                    : "SingleThread"});
                         });

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
              /*ad=*/"null", /*bid=*/std::nullopt,
              /*bid_currency=*/std::nullopt);

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
          /*ad=*/"null", /*bid=*/std::nullopt, /*bid_currency=*/std::nullopt));
  RunScoreAdWithReturnValueExpectingResult(
      "{ad:null, desirability:1, allowComponentAuction:true}", 1,
      /*expected_errors=*/{},
      mojom::ComponentAuctionModifiedBidParams::New(
          /*ad=*/"null", /*bid=*/std::nullopt, /*bid_currency=*/std::nullopt));
  RunScoreAdWithReturnValueExpectingResult(
      R"({ad:"foo", desirability:1, allowComponentAuction:true})", 1,
      /*expected_errors=*/{},
      mojom::ComponentAuctionModifiedBidParams::New(
          /*ad=*/R"("foo")", /*bid=*/std::nullopt,
          /*bid_currency=*/std::nullopt));
  RunScoreAdWithReturnValueExpectingResult(
      "{ad:[[35]], desirability:1, allowComponentAuction:true}", 1,
      /*expected_errors=*/{},
      mojom::ComponentAuctionModifiedBidParams::New(
          /*ad=*/"[[35]]", /*bid=*/std::nullopt,
          /*bid_currency=*/std::nullopt));
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
        /*expected_data_version=*/std::nullopt,
        /*expected_debug_loss_report_url=*/std::nullopt,
        /*expected_debug_win_report_url=*/std::nullopt, test_case.reason_enum);
  }

  // Default reject reason is mojom::RejectReason::kNotAvailable, if scoreAd()
  // does not return one.
  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:-1}", 0,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      mojom::RejectReason::kNotAvailable);

  // Reject reason is ignored when desirability is positive.
  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:3, rejectReason: 'invalid-bid'}", 3,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
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
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_reject_reason*/ mojom::RejectReason::kNotAvailable);

  // Reject reason returned by seller script must be a string.
  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:-1, rejectReason: 2}", 0,
      /*expected_errors=*/
      {"https://url.test/ scoreAd() returned an invalid reject reason."},
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
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
          /*ad=*/"null", /*bid=*/13, /*bid_currency=*/std::nullopt));
  RunScoreAdWithReturnValueExpectingResult(
      "{ad:null, desirability:1, allowComponentAuction:true, bid:1.2}", 1,
      /*expected_errors=*/{},
      mojom::ComponentAuctionModifiedBidParams::New(
          /*ad=*/"null", /*bid=*/1.2, /*bid_currency=*/std::nullopt));

  // Can also specify the currency.
  RunScoreAdWithReturnValueExpectingResult(
      "{ad:null, desirability:1, allowComponentAuction:true, "
      "bid:1.2, bidCurrency: 'USD'}",
      1,
      /*expected_errors=*/{},
      mojom::ComponentAuctionModifiedBidParams::New(
          /*ad=*/"null", /*bid=*/1.2,
          /*bid_currency=*/blink::AdCurrency::From("USD")));

  // Not providing a bid is fine.
  RunScoreAdWithReturnValueExpectingResult(
      "{ad:null, desirability:1, allowComponentAuction:true}", 1,
      /*expected_errors=*/{},
      mojom::ComponentAuctionModifiedBidParams::New(
          /*ad=*/"null", /*bid=*/std::nullopt, /*bid_currency=*/std::nullopt));

  // JS coercions happen for bid as well.
  RunScoreAdWithReturnValueExpectingResult(
      R"({ad:null, desirability:1, allowComponentAuction:true, bid:"5"})", 1,
      /*expected_errors=*/{},
      mojom::ComponentAuctionModifiedBidParams::New(
          /*ad=*/"null", /*bid=*/5, /*bid_currency=*/std::nullopt));
  RunScoreAdWithReturnValueExpectingResult(
      "{ad:null, desirability:1, allowComponentAuction:true, bid:[4]}", 1,
      /*expected_errors=*/{},
      mojom::ComponentAuctionModifiedBidParams::New(
          /*ad=*/"null", /*bid=*/4, /*bid_currency=*/std::nullopt));

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
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      mojom::RejectReason::kWrongScoreAdCurrency);

  // Special case: if there is a manually specified reject-reason, it goes in
  // and not the currency mismatch.
  RunScoreAdWithReturnValueExpectingResult(
      "{ad:null, desirability:1, allowComponentAuction:true, "
      "bid:1.2, bidCurrency: 'USSD', rejectReason: 'category-exclusions'}",
      0, {"https://url.test/ scoreAd() returned an invalid bidCurrency."},
      /*expected_component_auction_modified_bid_params=*/
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
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
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      mojom::RejectReason::kWrongScoreAdCurrency);

  RunScoreAdWithReturnValueExpectingResult(
      "{ad:null, desirability:1, allowComponentAuction:true, "
      "bid:1.2, bidCurrency: 'CAD'}",
      1,
      /*expected_errors=*/{},
      mojom::ComponentAuctionModifiedBidParams::New(
          /*ad=*/"null", /*bid=*/1.2,
          /*bid_currency=*/blink::AdCurrency::From("CAD")));
  auction_ad_config_non_shared_params_.seller_currency = std::nullopt;

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
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      mojom::RejectReason::kWrongScoreAdCurrency);
  RunScoreAdWithReturnValueExpectingResult(
      "{ad:null, desirability:1, allowComponentAuction:true, "
      "bid:1.2, bidCurrency: 'EUR'}",
      1,
      /*expected_errors=*/{},
      mojom::ComponentAuctionModifiedBidParams::New(
          /*ad=*/"null", /*bid=*/1.2,
          /*bid_currency=*/blink::AdCurrency::From("EUR")));
  component_expect_bid_currency_ = std::nullopt;

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
          /*ad=*/"null", /*bid=*/std::nullopt, /*bid_currency=*/std::nullopt),
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      mojom::RejectReason::kWrongScoreAdCurrency);
}

// Test the `incomingBidInSellerCurrency` output field of scoreAd()
TEST_F(SellerWorkletTest, ScoreAdIncomingBidInSellerCurrency) {
  // Configure bid currency to make sure checks for passthrough are done.
  bid_currency_ = blink::AdCurrency::From("USD");

  // If seller currency isn't configured, can't set it.
  auction_ad_config_non_shared_params_.seller_currency = std::nullopt;

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
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_reject_reason=*/
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_real_time_contributions=*/{},
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
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_reject_reason=*/
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_real_time_contributions=*/{},
      /*expected_bid_in_seller_currency=*/1);

  // ...can also have that same-currency bid directly forwarded.
  bid_ = 3.14;
  RunScoreAdWithReturnValueExpectingResult(
      "{desirability:1}", 1,
      /*expected_errors=*/std::vector<std::string>(),
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_reject_reason=*/
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_real_time_contributions=*/{},
      /*expected_bid_in_seller_currency=*/3.14);

  // This should also work if we use the number-only shorthand.
  RunScoreAdWithReturnValueExpectingResult(
      "1", 1,
      /*expected_errors=*/std::vector<std::string>(),
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_reject_reason=*/
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_real_time_contributions=*/{},
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

TEST_F(SellerWorkletTest, ScoreAdSelectedBuyerAndSellerReportingId) {
  browser_signal_selected_buyer_and_seller_reporting_id_ = "foo";

  RunScoreAdWithReturnValueExpectingResult(
      R"(browserSignals.selectedBuyerAndSellerReportingId === "foo" ? 3 : 0)",
      3);
}

TEST_F(SellerWorkletTest, ScoreAdBuyerAndSellerReportingIdPresentWithSelected) {
  browser_signal_selected_buyer_and_seller_reporting_id_ = "foo";
  browser_signal_buyer_and_seller_reporting_id_ = "boo";
  RunScoreAdWithReturnValueExpectingResult(
      R"((browserSignals.selectedBuyerAndSellerReportingId === "foo" &&
      browserSignals.buyerAndSellerReportingId === "boo") ? 3 : 0 )",
      3);
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
          /*ad=*/"null", /*bid=*/std::nullopt, /*bid_currency=*/std::nullopt));

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
          /*ad=*/"null", /*bid=*/std::nullopt, /*bid_currency=*/std::nullopt));

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

// Check that accessing `renderUrl` of browserSignals displays a warning.
//
// TODO(crbug.com/40266734): Remove this test when the field itself is
// removed.
TEST_F(SellerWorkletTest, ScoreAdRenderUrlDeprecationWarning) {
  ScopedInspectorSupport inspector_support(v8_helper().get());
  AddJavascriptResponse(
      &url_loader_factory_, decision_logic_url_,
      CreateScoreAdScript("browserSignals.renderUrl ? 1 : 0"));
  SellerWorklet* worklet_impl = nullptr;
  auto worklet =
      CreateWorklet(/*pause_for_debugger_on_start=*/false, &worklet_impl);
  int id = worklet_impl->context_group_ids_for_testing()[0];
  TestChannel* channel =
      inspector_support.ConnectDebuggerSessionAndRuntimeEnable(id);

  RunScoreAdExpectingResultOnWorklet(worklet_impl, 1);

  channel->WaitForAndValidateConsoleMessage(
      "warning", /*json_args=*/
      "[{\"type\":\"string\", "
      "\"value\":\"browserSignals.renderUrl is deprecated. Please use "
      "browserSignals.renderURL instead.\"}]",
      /*stack_trace_size=*/1, /*function=*/"scoreAd", decision_logic_url_,
      /*line_number=*/4);

  channel->ExpectNoMoreConsoleEvents();
}

// Check that accessing `renderURL` of browserSignals does not display a
// warning.
//
// TODO(crbug.com/40266734): Remove this test when renderUrl is removed.
TEST_F(SellerWorkletTest, ScoreAdRenderUrlNoDeprecationWarning) {
  ScopedInspectorSupport inspector_support(v8_helper().get());
  AddJavascriptResponse(
      &url_loader_factory_, decision_logic_url_,
      CreateScoreAdScript("browserSignals.renderURL ? 1 : 0"));
  SellerWorklet* worklet_impl = nullptr;
  auto worklet =
      CreateWorklet(/*pause_for_debugger_on_start=*/false, &worklet_impl);
  int id = worklet_impl->context_group_ids_for_testing()[0];
  TestChannel* channel =
      inspector_support.ConnectDebuggerSessionAndRuntimeEnable(id);

  RunScoreAdExpectingResultOnWorklet(worklet_impl, 1);
  channel->ExpectNoMoreConsoleEvents();
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

// Check that accessing `decisionLogicUrl` and `trustedScoringSignalsUrl` of
// `auctionConfig` displays a warning.
//
// TODO(crbug.com/40266734): Remove this test when the fields are
// removed.
TEST_F(SellerWorkletTest, ScoreAdAuctionConfigUrlDeprecationWarning) {
  ScopedInspectorSupport inspector_support(v8_helper().get());
  decision_logic_url_ = GURL("https://url.test/");
  trusted_scoring_signals_url_ =
      GURL("https://url.test/trusted_scoring_signals");

  // Include a pair of component auctions, to make sure it works for them as
  // well.
  auto& component_auctions =
      auction_ad_config_non_shared_params_.component_auctions;
  component_auctions.emplace_back();
  component_auctions[0].seller =
      url::Origin::Create(GURL("https://component1.test"));
  component_auctions[0].decision_logic_url =
      GURL("https://component1.test/script.js");
  component_auctions[0].trusted_scoring_signals_url =
      GURL("https://component1.test/signals.js");

  component_auctions.emplace_back(blink::AuctionConfig());
  component_auctions[1].seller =
      url::Origin::Create(GURL("https://component2.test"));
  component_auctions[1].decision_logic_url =
      GURL("https://component2.test/script.js");
  component_auctions[1].trusted_scoring_signals_url =
      GURL("https://component2.test/signals.js");

  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        CreateScoreAdScript(
                            /*raw_return_value=*/
                            R"(auctionConfig.decisionLogicUrl &&
             auctionConfig.trustedScoringSignalsUrl &&
             auctionConfig.componentAuctions[0].decisionLogicUrl &&
             auctionConfig.componentAuctions[0].trustedScoringSignalsUrl &&
             auctionConfig.componentAuctions[1].decisionLogicUrl &&
             auctionConfig.componentAuctions[1].trustedScoringSignalsUrl ?
                 1 : 0)"));

  // This response body doesn't actually matter, but have to set some response
  // for the signals request to avoid a hang.
  AddJsonResponse(
      &url_loader_factory_,
      GURL("https://url.test/trusted_scoring_signals?hostname=window.test"
           "&renderUrls=https%3A%2F%2Frender.url.test%2F"),
      kTrustedScoringSignalsResponse);

  SellerWorklet* worklet_impl = nullptr;
  auto worklet =
      CreateWorklet(/*pause_for_debugger_on_start=*/false, &worklet_impl);
  int id = worklet_impl->context_group_ids_for_testing()[0];
  TestChannel* channel =
      inspector_support.ConnectDebuggerSessionAndRuntimeEnable(id);

  RunScoreAdExpectingResultOnWorklet(worklet_impl, /*expected_score=*/1);

  channel->WaitForAndValidateConsoleMessage(
      "warning", /*json_args=*/
      "[{\"type\":\"string\", "
      "\"value\":\"auctionConfig.decisionLogicUrl is deprecated. Please use "
      "auctionConfig.decisionLogicURL instead.\"}]",
      /*stack_trace_size=*/1, /*function=*/"scoreAd", decision_logic_url_,
      /*line_number=*/4);

  channel->WaitForAndValidateConsoleMessage(
      "warning", /*json_args=*/
      "[{\"type\":\"string\", "
      "\"value\":\"auctionConfig.trustedScoringSignalsUrl is deprecated. "
      "Please use auctionConfig.trustedScoringSignalsURL instead.\"}]",
      /*stack_trace_size=*/1, /*function=*/"scoreAd", decision_logic_url_,
      /*line_number=*/5);

  channel->WaitForAndValidateConsoleMessage(
      "warning", /*json_args=*/
      "[{\"type\":\"string\", "
      "\"value\":\"auctionConfig.decisionLogicUrl is deprecated. Please use "
      "auctionConfig.decisionLogicURL instead.\"}]",
      /*stack_trace_size=*/1, /*function=*/"scoreAd", decision_logic_url_,
      /*line_number=*/6);

  channel->WaitForAndValidateConsoleMessage(
      "warning", /*json_args=*/
      "[{\"type\":\"string\", "
      "\"value\":\"auctionConfig.trustedScoringSignalsUrl is deprecated. "
      "Please use auctionConfig.trustedScoringSignalsURL instead.\"}]",
      /*stack_trace_size=*/1, /*function=*/"scoreAd", decision_logic_url_,
      /*line_number=*/7);

  channel->WaitForAndValidateConsoleMessage(
      "warning", /*json_args=*/
      "[{\"type\":\"string\", "
      "\"value\":\"auctionConfig.decisionLogicUrl is deprecated. Please use "
      "auctionConfig.decisionLogicURL instead.\"}]",
      /*stack_trace_size=*/1, /*function=*/"scoreAd", decision_logic_url_,
      /*line_number=*/8);

  channel->WaitForAndValidateConsoleMessage(
      "warning", /*json_args=*/
      "[{\"type\":\"string\", "
      "\"value\":\"auctionConfig.trustedScoringSignalsUrl is deprecated. "
      "Please use auctionConfig.trustedScoringSignalsURL instead.\"}]",
      /*stack_trace_size=*/1, /*function=*/"scoreAd", decision_logic_url_,
      /*line_number=*/9);

  channel->ExpectNoMoreConsoleEvents();
}

// Check that accessing `decisionLogicURL` and `trustedScoringSignalsURL` of
// `auctionConfig` does not display a warning.
//
// TODO(crbug.com/40266734): Remove this test when `decisionLogicUrl` and
// `trustedScoringSignalsUrl` are removed.
TEST_F(SellerWorkletTest, ScoreAdAuctionConfigUrlNoDeprecationWarning) {
  ScopedInspectorSupport inspector_support(v8_helper().get());
  decision_logic_url_ = GURL("https://url.test/");
  trusted_scoring_signals_url_ =
      GURL("https://url.test/trusted_scoring_signals");

  // Include a pair of component auctions, to make sure they have no warnings as
  // well.
  auto& component_auctions =
      auction_ad_config_non_shared_params_.component_auctions;
  component_auctions.emplace_back(blink::AuctionConfig());
  component_auctions[0].seller =
      url::Origin::Create(GURL("https://component1.test"));
  component_auctions[0].decision_logic_url =
      GURL("https://component1.test/script.js");
  component_auctions[0].trusted_scoring_signals_url =
      GURL("https://component1.test/signals.js");

  component_auctions.emplace_back(blink::AuctionConfig());
  component_auctions[1].seller =
      url::Origin::Create(GURL("https://component2.test"));
  component_auctions[1].decision_logic_url =
      GURL("https://component2.test/script.js");
  component_auctions[1].trusted_scoring_signals_url =
      GURL("https://component2.test/signals.js");

  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        CreateScoreAdScript(
                            /*raw_return_value=*/
                            R"(auctionConfig.decisionLogicURL &&
             auctionConfig.trustedScoringSignalsURL &&
             auctionConfig.componentAuctions[0].decisionLogicURL &&
             auctionConfig.componentAuctions[0].trustedScoringSignalsURL &&
             auctionConfig.componentAuctions[1].decisionLogicURL &&
             auctionConfig.componentAuctions[1].trustedScoringSignalsURL ?
                 1 : 0)"));

  // This response body doesn't actually matter, but have to set some response
  // for the signals request to avoid a hang.
  AddJsonResponse(
      &url_loader_factory_,
      GURL("https://url.test/trusted_scoring_signals?hostname=window.test"
           "&renderUrls=https%3A%2F%2Frender.url.test%2F"),
      kTrustedScoringSignalsResponse);

  SellerWorklet* worklet_impl = nullptr;
  auto worklet =
      CreateWorklet(/*pause_for_debugger_on_start=*/false, &worklet_impl);
  int id = worklet_impl->context_group_ids_for_testing()[0];
  TestChannel* channel =
      inspector_support.ConnectDebuggerSessionAndRuntimeEnable(id);

  RunScoreAdExpectingResultOnWorklet(worklet_impl, /*expected_score=*/1);
  channel->ExpectNoMoreConsoleEvents();
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
TEST_P(SellerWorkletMultiThreadingTest, ScoreAdTrustedScoringSignals) {
  // With no trusted scoring signals URL, `trustedScoringSignals` should be
  // null.
  trusted_scoring_signals_url_ = std::nullopt;
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

  mojom::RealTimeReportingContribution expected_trusted_signal_histogram(
      /*bucket=*/1024 + auction_worklet::RealTimeReportingPlatformError::
                            kTrustedScoringSignalsFailure,
      /*priority_weight=*/1,
      /*latency_threshold=*/std::nullopt);
  RealTimeReportingContributions expected_real_time_contributions;
  expected_real_time_contributions.push_back(
      expected_trusted_signal_histogram.Clone());
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
                          kNoComponentSignalsUrl.spec().c_str())},
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_reject_reason=*/mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{}, std::move(expected_real_time_contributions));

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

// With the cross-origin trusted signals flag off, nothing is passed in to the
// cross-original signals parameter.
TEST_F(SellerWorkletCrossOriginTrustedSignalsDisabledTest, Basic) {
  RunScoreAdWithReturnValueExpectingResult(
      "crossOriginTrustedSignals === undefined ? 1 : 0", 1);
  RunScoreAdWithReturnValueExpectingResult("arguments.length", 6);
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
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_real_time_contributions=*/{},
      /*expected_bid_in_seller_currency=*/std::nullopt,
      /*expected_score_ad_timeout=*/false,
      /*expected_signals_fetch_latency=*/kDelay,
      /*expected_code_ready_latency=*/std::nullopt, run_loop.QuitClosure());
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
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_real_time_contributions=*/{},
      /*expected_bid_in_seller_currency=*/std::nullopt,
      /*expected_score_ad_timeout=*/false,
      /*expected_signals_fetch_latency=*/std::nullopt,
      /*expected_code_ready_latency=*/kDelay, run_loop.QuitClosure());
  task_environment_.RunUntilIdle();
  task_environment_.FastForwardBy(kDelay);
  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        CreateScoreAdScript(/*raw_return_value=*/"1", ""));
  run_loop.Run();
}

TEST_F(SellerWorkletTest, ScoreAdJsFetchLatency) {
  auto seller_worklet = CreateWorklet();
  ASSERT_TRUE(seller_worklet);
  base::RunLoop run_loop;
  seller_worklet->ScoreAd(
      ad_metadata_, bid_, bid_currency_, auction_ad_config_non_shared_params_,
      direct_from_seller_seller_signals_,
      direct_from_seller_seller_signals_header_ad_slot_,
      direct_from_seller_auction_signals_,
      direct_from_seller_auction_signals_header_ad_slot_,
      browser_signals_other_seller_.Clone(), component_expect_bid_currency_,
      browser_signal_interest_group_owner_, browser_signal_render_url_,
      browser_signal_selected_buyer_and_seller_reporting_id_,
      browser_signal_buyer_and_seller_reporting_id_,
      browser_signal_ad_components_, browser_signal_bidding_duration_msecs_,
      browser_signal_render_size_,
      browser_signal_for_debugging_only_in_cooldown_or_lockout_,
      seller_timeout_,
      /*trace_id=*/1, bidder_joining_origin_,
      TestScoreAdClient::Create(base::BindLambdaForTesting(
          [&run_loop](double score, mojom::RejectReason reject_reason,
                      mojom::ComponentAuctionModifiedBidParamsPtr
                          component_auction_modified_bid_params,
                      std::optional<double> bid_in_seller_currency,
                      std::optional<uint32_t> scoring_signals_data_version,
                      const std::optional<GURL>& debug_loss_report_url,
                      const std::optional<GURL>& debug_win_report_url,
                      PrivateAggregationRequests pa_requests,
                      RealTimeReportingContributions real_time_contributions,
                      mojom::SellerTimingMetricsPtr score_ad_timing_metrics,
                      mojom::ScoreAdDependencyLatenciesPtr
                          score_ad_dependency_latencies,
                      const std::vector<std::string>& errors) {
            ASSERT_TRUE(score_ad_timing_metrics->js_fetch_latency.has_value());
            EXPECT_EQ(base::Milliseconds(235),
                      *score_ad_timing_metrics->js_fetch_latency);
            run_loop.Quit();
          })));
  task_environment_.FastForwardBy(base::Milliseconds(235));
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
TEST_P(SellerWorkletMultiThreadingTest, ScoreAdParallelBeforeLoadComplete) {
  auto seller_worklet = CreateWorklet(/*pause_for_debugger_on_start=*/false);

  const size_t kNumWorklets = 10;
  size_t num_completed_worklets = 0;
  base::RunLoop run_loop;
  for (size_t i = 0; i < kNumWorklets; ++i) {
    browser_signal_render_url_ = GURL(base::StringPrintf("https://foo/%zu", i));
    RunScoreAdOnWorkletAsync(seller_worklet.get(), /*expected_score=*/i,
                             /*expected_errors=*/std::vector<std::string>(),
                             mojom::ComponentAuctionModifiedBidParamsPtr(),
                             /*expected_data_version=*/std::nullopt,
                             /*expected_debug_loss_report_url=*/std::nullopt,
                             /*expected_debug_win_report_url=*/std::nullopt,
                             /*expected_reject_reason=*/
                             mojom::RejectReason::kNotAvailable,
                             /*expected_pa_requests=*/{},
                             /*expected_real_time_contributions=*/{},
                             /*expected_bid_in_seller_currency=*/std::nullopt,
                             /*expected_score_ad_timeout=*/false,
                             /*expected_signals_fetch_latency=*/std::nullopt,
                             /*expected_code_ready_latency=*/std::nullopt,
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
TEST_P(SellerWorkletMultiThreadingTest, ScoreAdParallelAfterLoadComplete) {
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
                             /*expected_data_version=*/std::nullopt,
                             /*expected_debug_loss_report_url=*/std::nullopt,
                             /*expected_debug_win_report_url=*/std::nullopt,
                             /*expected_reject_reason=*/
                             mojom::RejectReason::kNotAvailable,
                             /*expected_pa_requests=*/{},
                             /*expected_real_time_contributions=*/{},
                             /*expected_bid_in_seller_currency=*/std::nullopt,
                             /*expected_score_ad_timeout=*/false,
                             /*expected_signals_fetch_latency=*/std::nullopt,
                             /*expected_code_ready_latency=*/std::nullopt,
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
TEST_P(SellerWorkletMultiThreadingTest, ScoreAdParallelLoadFails) {
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
TEST_P(SellerWorkletMultiThreadingTest,
       ScoreAdParallelTrustedScoringSignalsNotBatched) {
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
                             /*expected_debug_loss_report_url=*/std::nullopt,
                             /*expected_debug_win_report_url=*/std::nullopt,
                             /*expected_reject_reason=*/
                             mojom::RejectReason::kNotAvailable,
                             /*expected_pa_requests=*/{},
                             /*expected_real_time_contributions=*/{},
                             /*expected_bid_in_seller_currency=*/std::nullopt,
                             /*expected_score_ad_timeout=*/false,
                             /*expected_signals_fetch_latency=*/std::nullopt,
                             /*expected_code_ready_latency=*/std::nullopt,
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
TEST_P(SellerWorkletMultiThreadingTest,
       ScoreAdParallelTrustedScoringSignalsBatched1) {
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
                             /*expected_data_version=*/std::nullopt,
                             /*expected_debug_loss_report_url=*/std::nullopt,
                             /*expected_debug_win_report_url=*/std::nullopt,
                             /*expected_reject_reason=*/
                             mojom::RejectReason::kNotAvailable,
                             /*expected_pa_requests=*/{},
                             /*expected_real_time_contributions=*/{},
                             /*expected_bid_in_seller_currency=*/std::nullopt,
                             /*expected_score_ad_timeout=*/false,
                             /*expected_signals_fetch_latency=*/std::nullopt,
                             /*expected_code_ready_latency=*/std::nullopt,
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
TEST_P(SellerWorkletMultiThreadingTest,
       ScoreAdParallelTrustedScoringSignalsBatched2) {
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
                             /*expected_debug_loss_report_url=*/std::nullopt,
                             /*expected_debug_win_report_url=*/std::nullopt,
                             /*expected_reject_reason=*/
                             mojom::RejectReason::kNotAvailable,
                             /*expected_pa_requests=*/{},
                             /*expected_real_time_contributions=*/{},
                             /*expected_bid_in_seller_currency=*/std::nullopt,
                             /*expected_score_ad_timeout=*/false,
                             /*expected_signals_fetch_latency=*/std::nullopt,
                             /*expected_code_ready_latency=*/std::nullopt,
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
TEST_P(SellerWorkletMultiThreadingTest,
       ScoreAdParallelTrustedScoringSignalsBatched3) {
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
                             /*expected_debug_loss_report_url=*/std::nullopt,
                             /*expected_debug_win_report_url=*/std::nullopt,
                             /*expected_reject_reason=*/
                             mojom::RejectReason::kNotAvailable,
                             /*expected_pa_requests=*/{},
                             /*expected_real_time_contributions=*/{},
                             /*expected_bid_in_seller_currency=*/std::nullopt,
                             /*expected_score_ad_timeout=*/false,
                             /*expected_signals_fetch_latency=*/std::nullopt,
                             /*expected_code_ready_latency=*/std::nullopt,
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
        /*expected_data_version=*/std::nullopt,
        /*expected_debug_loss_report_url=*/std::nullopt,
        /*expected_debug_win_report_url=*/std::nullopt,
        /*expected_reject_reason=*/
        mojom::RejectReason::kNotAvailable,
        /*expected_pa_requests=*/{},
        /*expected_real_time_contributions=*/{},
        /*expected_bid_in_seller_currency=*/std::nullopt,
        /*expected_score_ad_timeout=*/false,
        /*expected_signals_fetch_latency=*/std::nullopt,
        /*expected_code_ready_latency=*/std::nullopt, run_loop->QuitClosure());
    for (size_t i = 0; i < std::size(kResponses); ++i) {
      SCOPED_TRACE(i);
      const Response& response =
          kResponses[(i + offset) % std::size(kResponses)];
      AddResponse(&url_loader_factory_, response.response_url,
                  response.response_type,
                  /*charset=*/std::nullopt, response.content, response.headers);
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
              kJsonMimeType, /*charset=*/std::nullopt, kWorklet1JsonResponse,
              kDirectFromSellerSignalsHeaders);
  AddResponse(&url_loader_factory_, *direct_from_seller_auction_signals_,
              kJsonMimeType, /*charset=*/std::nullopt, kWorklet1JsonResponse,
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
              /*charset=*/std::nullopt, kWorklet2JsonResponse,
              kDirectFromSellerSignalsHeaders);
  AddResponse(&alternate_url_loader_factory_,
              *direct_from_seller_auction_signals_, kJsonMimeType,
              /*charset=*/std::nullopt, kWorklet2JsonResponse,
              kDirectFromSellerSignalsHeaders);
  AddJavascriptResponse(
      &alternate_url_loader_factory_, decision_logic_url_,
      CreateScoreAdScript("1", /*extra_code=*/kWorklet2ExtraCode));
  auto run_loop = std::make_unique<base::RunLoop>();
  RunScoreAdOnWorkletAsync(
      seller_worklet1.get(), /*expected_score=*/1.0,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_reject_reason=*/
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_real_time_contributions=*/{},
      /*expected_bid_in_seller_currency=*/std::nullopt,
      /*expected_score_ad_timeout=*/false,
      /*expected_signals_fetch_latency=*/std::nullopt,
      /*expected_code_ready_latency=*/std::nullopt, run_loop->QuitClosure());
  run_loop->Run();

  run_loop = std::make_unique<base::RunLoop>();
  RunScoreAdOnWorkletAsync(
      seller_worklet2.get(), /*expected_score=*/1.0,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_reject_reason=*/
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_real_time_contributions=*/{},
      /*expected_bid_in_seller_currency=*/std::nullopt,
      /*expected_score_ad_timeout=*/false,
      /*expected_signals_fetch_latency=*/std::nullopt,
      /*expected_code_ready_latency=*/std::nullopt, run_loop->QuitClosure());
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
      /*expected_report_url=*/std::nullopt);
  RunReportResultCreatedScriptExpectingResult(
      R"("  1   ")", /*extra_code=*/std::string(),
      /*expected_signals_for_winner=*/R"("  1   ")",
      /*expected_report_url=*/std::nullopt);
  RunReportResultCreatedScriptExpectingResult(
      "[ null ]", /*extra_code=*/std::string(), "[null]",
      /*expected_report_url=*/std::nullopt);

  // No return value.
  RunReportResultCreatedScriptExpectingResult(
      "", /*extra_code=*/std::string(), "null",
      /*expected_report_url=*/std::nullopt);

  // Throw exception.
  RunReportResultCreatedScriptExpectingResult(
      "shrimp", /*extra_code=*/std::string(),
      /*expected_signals_for_winner=*/std::nullopt,
      /*expected_report_url=*/std::nullopt,
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
      /*expected_signals_for_winner=*/std::nullopt,
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:10 Uncaught TypeError: "
       "sendReportTo must be passed a valid HTTPS url."});
  RunReportResultCreatedScriptExpectingResult(
      "1", R"(sendReportTo("file:///foo/"))",
      /*expected_signals_for_winner=*/std::nullopt,
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:10 Uncaught TypeError: "
       "sendReportTo must be passed a valid HTTPS url."});

  // Multiple calls.
  RunReportResultCreatedScriptExpectingResult(
      "1",
      R"(sendReportTo("https://foo.test/"); sendReportTo("https://foo.test/"))",
      /*expected_signals_for_winner=*/std::nullopt,
      /*expected_report_url=*/std::nullopt,
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
      /*expected_report_url=*/std::nullopt);

  // Not a URL.
  RunReportResultCreatedScriptExpectingResult(
      "1", R"(sendReportTo("France"))",
      /*expected_signals_for_winner=*/std::nullopt,
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:10 Uncaught TypeError: "
       "sendReportTo must be passed a valid HTTPS url."});
  RunReportResultCreatedScriptExpectingResult(
      "1", R"(sendReportTo(null))",
      /*expected_signals_for_winner=*/std::nullopt,
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:10 Uncaught TypeError: "
       "sendReportTo must be passed a valid HTTPS url."});
  RunReportResultCreatedScriptExpectingResult(
      "1", R"(sendReportTo([5]))",
      /*expected_signals_for_winner=*/std::nullopt,
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:10 Uncaught TypeError: "
       "sendReportTo must be passed a valid HTTPS url."});
}

TEST_F(SellerWorkletTest, ReportResultDateNotAvailable) {
  RunReportResultCreatedScriptExpectingResult(
      "1", R"(sendReportTo("https://foo.test/" + Date().toString()))",
      /*expected_signals_for_winner=*/std::nullopt,
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:10 Uncaught ReferenceError: Date is not defined."});
}

TEST_F(SellerWorkletTest, ReportResultTopWindowOrigin) {
  top_window_origin_ = url::Origin::Create(GURL("https://foo.test/"));
  RunReportResultCreatedScriptExpectingResult(
      R"(browserSignals.topWindowHostname == "foo.test" ? 2 : 1)",
      /*extra_code=*/std::string(), "2",
      /*expected_report_url=*/std::nullopt);

  top_window_origin_ = url::Origin::Create(GURL("https://[::1]:40000/"));
  RunReportResultCreatedScriptExpectingResult(
      R"(browserSignals.topWindowHostname == "[::1]" ? 3 : 1)",
      /*extra_code=*/std::string(), "3",
      /*expected_report_url=*/std::nullopt);
}

TEST_F(SellerWorkletTest, ReportResultTopLevelSeller) {
  browser_signals_other_seller_.reset();
  RunReportResultCreatedScriptExpectingResult(
      R"("topLevelSeller" in browserSignals ? 0 : 1)",
      /*extra_code=*/std::string(), /*expected_signals_for_winner=*/"1",
      /*expected_report_url=*/std::nullopt);

  browser_signals_other_seller_ =
      mojom::ComponentAuctionOtherSeller::NewTopLevelSeller(
          url::Origin::Create(GURL("https://top.seller.test")));
  browser_signals_component_auction_report_result_params_ =
      mojom::ComponentAuctionReportResultParams::New(
          /*top_level_seller_signals=*/"null",
          /*modified_bid=*/std::nullopt);
  RunReportResultCreatedScriptExpectingResult(
      R"(browserSignals.topLevelSeller === "https://top.seller.test" ? 2 : 0)",
      /*extra_code=*/std::string(), /*expected_signals_for_winner=*/"2",
      /*expected_report_url=*/std::nullopt);
  browser_signals_component_auction_report_result_params_.reset();

  browser_signals_other_seller_ =
      mojom::ComponentAuctionOtherSeller::NewComponentSeller(
          url::Origin::Create(GURL("https://component.test")));
  RunReportResultCreatedScriptExpectingResult(
      R"("topLevelSeller" in browserSignals ? 0 : 3)",
      /*extra_code=*/std::string(), /*expected_signals_for_winner=*/"3",
      /*expected_report_url=*/std::nullopt);
}

TEST_F(SellerWorkletTest, ReportResultComponentSeller) {
  browser_signals_other_seller_.reset();
  RunReportResultCreatedScriptExpectingResult(
      R"("componentSeller" in browserSignals ? 0 : 1)",
      /*extra_code=*/std::string(), /*expected_signals_for_winner=*/"1",
      /*expected_report_url=*/std::nullopt);

  browser_signals_other_seller_ =
      mojom::ComponentAuctionOtherSeller::NewTopLevelSeller(
          url::Origin::Create(GURL("https://top.seller.test")));
  browser_signals_component_auction_report_result_params_ =
      mojom::ComponentAuctionReportResultParams::New(
          /*top_level_seller_signals=*/"null",
          /*modified_bid=*/std::nullopt);
  RunReportResultCreatedScriptExpectingResult(
      R"("componentSeller" in browserSignals ? 0 : 2)",
      /*extra_code=*/std::string(), /*expected_signals_for_winner=*/"2",
      /*expected_report_url=*/std::nullopt);
  browser_signals_component_auction_report_result_params_.reset();

  browser_signals_other_seller_ =
      mojom::ComponentAuctionOtherSeller::NewComponentSeller(
          url::Origin::Create(GURL("https://component.test")));
  RunReportResultCreatedScriptExpectingResult(
      R"(browserSignals.componentSeller === "https://component.test" ? 3 : 0)",
      /*extra_code=*/std::string(), /*expected_signals_for_winner=*/"3",
      /*expected_report_url=*/std::nullopt);
}

TEST_F(SellerWorkletTest, ReportResultTopLevelSellerSignals) {
  // Top-level auctions should never be passed a `topLevelSellerSignals` field,
  // whether ReportResult() is invoked with a bid from a component auction or
  // the top-level auction.

  browser_signals_other_seller_.reset();
  RunReportResultCreatedScriptExpectingResult(
      "'topLevelSellerSignals' in browserSignals ? 0 : 1",
      /*extra_code=*/std::string(), /*expected_signals_for_winner=*/"1",
      /*expected_report_url=*/std::nullopt);

  browser_signals_other_seller_ =
      mojom::ComponentAuctionOtherSeller::NewComponentSeller(
          url::Origin::Create(GURL("https://component.test")));
  RunReportResultCreatedScriptExpectingResult(
      "'topLevelSellerSignals' in browserSignals ? 0 : 2",
      /*extra_code=*/std::string(), /*expected_signals_for_winner=*/"2",
      /*expected_report_url=*/std::nullopt);

  // Component auctions should take `topLevelSellerSignals` from the
  // ComponentAuctionReportResultParams argument.
  browser_signals_other_seller_ =
      mojom::ComponentAuctionOtherSeller::NewTopLevelSeller(
          url::Origin::Create(GURL("https://top.seller.test")));
  browser_signals_component_auction_report_result_params_ =
      mojom::ComponentAuctionReportResultParams::New(
          /*top_level_seller_signals=*/"null",
          /*modified_bid=*/std::nullopt);
  RunReportResultCreatedScriptExpectingResult(
      "browserSignals.topLevelSellerSignals === null ? 3 : 0",
      /*extra_code=*/std::string(), /*expected_signals_for_winner=*/"3",
      /*expected_report_url=*/std::nullopt);

  browser_signals_component_auction_report_result_params_
      ->top_level_seller_signals = "[4]";
  RunReportResultCreatedScriptExpectingResult(
      "browserSignals.topLevelSellerSignals[0]",
      /*extra_code=*/std::string(), /*expected_signals_for_winner=*/"4",
      /*expected_report_url=*/std::nullopt);
}

TEST_F(SellerWorkletTest, ReportResultModifiedBid) {
  // Top-level auctions should never be passed a `modifiedBid` field, whether
  // ReportResult() is invoked with a bit from a component auction or the
  // top-level auction.

  browser_signals_other_seller_.reset();
  RunReportResultCreatedScriptExpectingResult(
      "'modifiedBid' in browserSignals ? 0 : 1",
      /*extra_code=*/std::string(), /*expected_signals_for_winner=*/"1",
      /*expected_report_url=*/std::nullopt);

  browser_signals_other_seller_ =
      mojom::ComponentAuctionOtherSeller::NewComponentSeller(
          url::Origin::Create(GURL("https://component.test")));
  RunReportResultCreatedScriptExpectingResult(
      "'modifiedBid' in browserSignals ? 0 : 2",
      /*extra_code=*/std::string(), /*expected_signals_for_winner=*/"2",
      /*expected_report_url=*/std::nullopt);

  // Component auctions should only receive a `modifiedBid` field when
  // `modified_bid` has a value.
  browser_signals_other_seller_ =
      mojom::ComponentAuctionOtherSeller::NewTopLevelSeller(
          url::Origin::Create(GURL("https://top.seller.test")));
  browser_signals_component_auction_report_result_params_ =
      mojom::ComponentAuctionReportResultParams::New(
          /*top_level_seller_signals=*/"null",
          /*modified_bid=*/4);
  RunReportResultCreatedScriptExpectingResult(
      "browserSignals.modifiedBid",
      /*extra_code=*/std::string(), /*expected_signals_for_winner=*/"4",
      /*expected_report_url=*/std::nullopt);
  browser_signals_component_auction_report_result_params_->modified_bid =
      std::nullopt;
  RunReportResultCreatedScriptExpectingResult(
      "'modifiedBid' in browserSignals ? 0 : 3",
      /*extra_code=*/std::string(), /*expected_signals_for_winner=*/"3",
      /*expected_report_url=*/std::nullopt);
}

TEST_F(SellerWorkletTest, ReportResultInterestGroupOwner) {
  browser_signal_interest_group_owner_ =
      url::Origin::Create(GURL("https://foo.test/"));
  RunReportResultCreatedScriptExpectingResult(
      R"(browserSignals.interestGroupOwner == "https://foo.test" ? 2 : 1)",
      /*extra_code=*/std::string(), "2",
      /*expected_report_url=*/std::nullopt);

  browser_signal_interest_group_owner_ =
      url::Origin::Create(GURL("https://[::1]:40000/"));
  RunReportResultCreatedScriptExpectingResult(
      R"(browserSignals.interestGroupOwner == "https://[::1]:40000" ? 3 : 1)",
      /*extra_code=*/std::string(), "3",
      /*expected_report_url=*/std::nullopt);
}

TEST_F(SellerWorkletTest, ReportResultBuyerAndSellerReportingId) {
  browser_signal_buyer_and_seller_reporting_id_ = "campaign";
  RunReportResultCreatedScriptExpectingResult(
      R"(browserSignals.buyerAndSellerReportingId === "campaign" ? 2 : 1)",
      /*extra_code=*/std::string(), "2",
      /*expected_report_url=*/std::nullopt);
}

TEST_F(SellerWorkletTest, ReportResultSelectedBuyerAndSellerReportingId) {
  browser_signal_selected_buyer_and_seller_reporting_id_ = "selectable_id1";

  RunReportResultCreatedScriptExpectingResult(
      R"(browserSignals.selectedBuyerAndSellerReportingId === "selectable_id1" ? 2 : 1)",
      /*extra_code=*/std::string(), "2",
      /*expected_report_url=*/std::nullopt);
}

TEST_F(SellerWorkletTest, ReportResultRenderUrl) {
  browser_signal_render_url_ = GURL("https://foo/");
  RunReportResultCreatedScriptExpectingResult(
      "browserSignals.renderURL", "sendReportTo(browserSignals.renderURL)",
      R"("https://foo/")", browser_signal_render_url_);
}

// Check that accessing `renderUrl` of browserSignals displays a warning.
//
// TODO(crbug.com/40266734): Remove this test when the field itself is
// removed.
TEST_F(SellerWorkletTest, ReportResultRenderUrlDeprecationWarning) {
  ScopedInspectorSupport inspector_support(v8_helper().get());
  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        CreateReportToScript("browserSignals.renderUrl"));
  SellerWorklet* worklet_impl = nullptr;
  auto worklet =
      CreateWorklet(/*pause_for_debugger_on_start=*/false, &worklet_impl);
  int id = worklet_impl->context_group_ids_for_testing()[0];
  TestChannel* channel =
      inspector_support.ConnectDebuggerSessionAndRuntimeEnable(id);

  base::RunLoop run_loop;
  RunReportResultExpectingResultAsync(
      worklet_impl, /*expected_signals_for_winner=*/
      base::StringPrintf("\"%s\"", browser_signal_render_url_.spec().c_str()),
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{}, /*expected_pa_requests=*/{},
      /*expected_reporting_latency_timeout=*/false, /*expected_errors=*/{},
      run_loop.QuitClosure());
  run_loop.Run();

  channel->WaitForAndValidateConsoleMessage(
      "warning", /*json_args=*/
      "[{\"type\":\"string\", "
      "\"value\":\"browserSignals.renderUrl is deprecated. Please use "
      "browserSignals.renderURL instead.\"}]",
      /*stack_trace_size=*/1, /*function=*/"reportResult", decision_logic_url_,
      /*line_number=*/10);

  channel->ExpectNoMoreConsoleEvents();
}

// Check that accessing `renderURL` of browserSignals does not display a
// warning.
//
// TODO(crbug.com/40266734): Remove this test when renderUrl is removed.
TEST_F(SellerWorkletTest, ReportResultRenderUrlNoDeprecationWarning) {
  ScopedInspectorSupport inspector_support(v8_helper().get());
  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        CreateReportToScript("browserSignals.renderURL"));
  SellerWorklet* worklet_impl = nullptr;
  auto worklet =
      CreateWorklet(/*pause_for_debugger_on_start=*/false, &worklet_impl);
  int id = worklet_impl->context_group_ids_for_testing()[0];
  TestChannel* channel =
      inspector_support.ConnectDebuggerSessionAndRuntimeEnable(id);

  base::RunLoop run_loop;
  RunReportResultExpectingResultAsync(
      worklet_impl, /*expected_signals_for_winner=*/
      base::StringPrintf("\"%s\"", browser_signal_render_url_.spec().c_str()),
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{}, /*expected_pa_requests=*/{},
      /*expected_reporting_latency_timeout=*/false, /*expected_errors=*/{},
      run_loop.QuitClosure());
  run_loop.Run();
  channel->ExpectNoMoreConsoleEvents();
}

TEST_F(SellerWorkletTest, ReportResultRegisterAdBeacon) {
  bid_ = 5;
  base::flat_map<std::string, GURL> expected_ad_beacon_map = {
      {"click", GURL("https://click.example.test/")},
      {"view", GURL("https://view.example.test/")},
  };
  RunReportResultCreatedScriptExpectingResult(
      R"(5)",
      R"(registerAdBeacon({
        'click': "https://click.example.test/",
        'view': "https://view.example.test/",
      }))",
      /*expected_signals_for_winner=*/"5",
      /*expected_report_url=*/std::nullopt, expected_ad_beacon_map);

  browser_signal_render_url_ = GURL("https://foo/");
  RunReportResultCreatedScriptExpectingResult(
      R"(5)",
      R"(registerAdBeacon({
        'click': "https://click.example.test/",
        'view': "https://view.example.test/",
      });
      sendReportTo(browserSignals.renderURL))",
      /*expected_signals_for_winner=*/"5",
      /*expected_report_url=*/browser_signal_render_url_,
      expected_ad_beacon_map);

  RunReportResultCreatedScriptExpectingResult(
      R"(5)",
      R"(sendReportTo(browserSignals.renderURL);
      registerAdBeacon({
        'click': "https://click.example.test/",
        'view': "https://view.example.test/",
      }))",
      /*expected_signals_for_winner=*/"5",
      /*expected_report_url=*/browser_signal_render_url_,
      expected_ad_beacon_map);

  // Don't call twice.
  RunReportResultCreatedScriptExpectingResult(
      R"(5)",
      R"(registerAdBeacon({
        'click': "https://click.example.test/",
        'view': "https://view.example.test/",
      });
      registerAdBeacon())",
      /*expected_signals_for_winner=*/{},
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:14 Uncaught TypeError: registerAdBeacon may be "
       "called at most once."});

  // If called twice and the error is caught, use the first result.
  RunReportResultCreatedScriptExpectingResult(
      R"(5)",
      R"(registerAdBeacon({
           'click': "https://click.example.test/",
           'view': "https://view.example.test/",
         });
         try { registerAdBeacon() }
         catch (e) {})",
      /*expected_signals_for_winner=*/"5",
      /*expected_report_url=*/std::nullopt, expected_ad_beacon_map);

  // If error on first call, can be called again.
  RunReportResultCreatedScriptExpectingResult(
      R"(5)",
      R"(try { registerAdBeacon() }
         catch (e) {}
         registerAdBeacon({
           'click': "https://click.example.test/",
           'view': "https://view.example.test/",
         }))",
      /*expected_signals_for_winner=*/"5",
      /*expected_report_url=*/std::nullopt, expected_ad_beacon_map);

  // Error if no parameters
  RunReportResultCreatedScriptExpectingResult(
      R"(5)", R"(registerAdBeacon())",
      /*expected_signals_for_winner=*/{},
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:10 Uncaught TypeError: registerAdBeacon(): at least "
       "1 argument(s) are required."});

  // Error if parameter is not an object
  RunReportResultCreatedScriptExpectingResult(
      R"(5)", R"(registerAdBeacon("foo"))",
      /*expected_signals_for_winner=*/{},
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:10 Uncaught TypeError: registerAdBeacon(): Cannot "
       "convert argument 'map' to a record since it's not an Object."});

  // Generally OK if parameter attributes are not strings
  RunReportResultCreatedScriptExpectingResult(
      R"(5)",
      R"(registerAdBeacon({
        'click': "https://click.example.test/",
        1: "https://view.example.test/",
      }))",
      /*expected_signals_for_winner=*/"5",
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/
      {
          {"click", GURL("https://click.example.test/")},
          {"1", GURL("https://view.example.test/")},
      },
      /*expected_pa_requests=*/{}, {});

  // ... but keys must be convertible to strings
  RunReportResultCreatedScriptExpectingResult(
      R"(5)",
      R"(let map = {
           'click': "https://click.example.test/"
         }
         map[Symbol('a')] = "https://view.example.test/";
         registerAdBeacon(map))",
      /*expected_signals_for_winner=*/{},
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:14 Uncaught TypeError: Cannot convert a Symbol value "
       "to a string."});

  // Error if invalid reporting URL
  RunReportResultCreatedScriptExpectingResult(
      R"(5)",
      R"(registerAdBeacon({
        'click': "https://click.example.test/",
        'view': "gopher://view.example.test/",
      }))",
      /*expected_signals_for_winner=*/{},
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:10 Uncaught TypeError: registerAdBeacon(): invalid "
       "reporting url for key 'view': 'gopher://view.example.test/'."});

  // Error if not trustworthy reporting URL
  RunReportResultCreatedScriptExpectingResult(
      R"(5)",
      R"(registerAdBeacon({
        'click': "https://127.0.0.1/",
        'view': "http://view.example.test/",
      }))",
      /*expected_signals_for_winner=*/{},
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:10 Uncaught TypeError: registerAdBeacon(): invalid "
       "reporting url for key 'view': 'http://view.example.test/'."});

  // Error if invalid "reserved.*" reporting event type
  RunReportResultCreatedScriptExpectingResult(
      R"(5)",
      R"(registerAdBeacon({
        'click': "https://127.0.0.1/",
        'reserved.bogus': "https://view.example.test/",
      }))",
      /*expected_signals_for_winner=*/{},
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:10 Uncaught TypeError: registerAdBeacon(): Invalid "
       "reserved type 'reserved.bogus' cannot be used."});

  // Special case for error message if the key has mismatched surrogates.
  RunReportResultCreatedScriptExpectingResult(
      R"(5)",
      R"(registerAdBeacon({
         '\ud835': "http://127.0.0.1/",
      }))",
      /*expected_signals_for_winner=*/{},
      /*expected_report_url=*/std::nullopt,
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
      /*expected_report_url=*/std::nullopt);
}

TEST_F(SellerWorkletTest, ReportResultBidCurrency) {
  bid_currency_ = blink::AdCurrency::From("EUR");
  RunReportResultCreatedScriptExpectingResult(
      "browserSignals.bidCurrency + typeof browserSignals.bidCurrency",
      /*extra_code=*/std::string(), R"("EURstring")",
      /*expected_report_url=*/std::nullopt);
}

TEST_F(SellerWorkletTest, ReportResultDesireability) {
  browser_signal_desireability_ = 10;
  RunReportResultCreatedScriptExpectingResult(
      "browserSignals.desirability + typeof browserSignals.desirability",
      /*extra_code=*/std::string(), R"("10number")",
      /*expected_report_url=*/std::nullopt);
}

TEST_F(SellerWorkletTest, ReportResultHighestScoringOtherBid) {
  browser_signal_highest_scoring_other_bid_ = 5;
  RunReportResultCreatedScriptExpectingResult(
      "browserSignals.highestScoringOtherBid + typeof "
      "browserSignals.highestScoringOtherBid",
      /*extra_code=*/std::string(), R"("5number")",
      /*expected_report_url=*/std::nullopt);
}

TEST_F(SellerWorkletTest, ReportResultHighestScoringOtherBidCurrency) {
  browser_signal_highest_scoring_other_bid_currency_ =
      blink::AdCurrency::From("EUR");
  RunReportResultCreatedScriptExpectingResult(
      "browserSignals.highestScoringOtherBidCurrency + typeof "
      "browserSignals.highestScoringOtherBidCurrency",
      /*extra_code=*/std::string(), R"("EURstring")",
      /*expected_report_url=*/std::nullopt);
}

TEST_F(SellerWorkletTest, ReportResultAuctionConfigParam) {
  // Empty AuctionAdConfig, with nothing filled in, except the seller and
  // decision logic URL.
  decision_logic_url_ = GURL("https://example.test/auction.js");
  RunReportResultCreatedScriptExpectingResult(
      "auctionConfig", /*extra_code=*/std::string(),
      base::StringPrintf(
          R"({"seller":"https://example.test",)"
          R"("decisionLogicURL":"https://example.test/auction.js",)"
          R"("decisionLogicUrl":"https://example.test/auction.js",)"
          R"("reportingTimeout": %s})",
          base::NumberToString(AuctionV8Helper::kScriptTimeout.InMilliseconds())
              .c_str()),
      /*expected_report_url=*/std::nullopt);

  // Everything filled in but component auctions (can't include component
  // auctions and non-empty interestGroupBuyers, so test those cases
  // separately).
  decision_logic_url_ = GURL("https://example.test/auction.js");
  trusted_scoring_signals_url_ =
      GURL("https://example.test/scoring_signals.json");
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

  auction_ad_config_non_shared_params_.reporting_timeout =
      base::Milliseconds(200);
  std::vector<blink::AuctionConfig::AdKeywordReplacement> example_replacement =
      {blink::AuctionConfig::AdKeywordReplacement({"${SELLER}", "ExampleSSP"})};

  auction_ad_config_non_shared_params_.deprecated_render_url_replacements =
      blink::AuctionConfig::MaybePromiseDeprecatedRenderURLReplacements::
          FromValue(std::move(example_replacement));

  blink::AuctionConfig::BuyerCurrencies buyer_currencies;
  buyer_currencies.per_buyer_currencies.emplace();
  buyer_currencies.per_buyer_currencies
      .value()[url::Origin::Create(GURL("https://ca.test"))] =
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
      R"({"seller":"https://example.test",
          "decisionLogicURL":"https://example.test/auction.js",
          "decisionLogicUrl":"https://example.test/auction.js",
          "trustedScoringSignalsURL":"https://example.test/scoring_signals.json",
          "trustedScoringSignalsUrl":"https://example.test/scoring_signals.json",
          "interestGroupBuyers":["https://buyer1.com",
                                 "https://another-buyer.com"],
          "auctionSignals":{"is_auction_signals":true},
          "sellerSignals":{"is_seller_signals":true},
          "sellerTimeout":200,
          "perBuyerSignals":{"https://a.com":{"signals_a":"A"},
                             "https://b.com":{"signals_b":"B"}},
          "perBuyerCurrencies":{"*": "USD",
                                "https://ca.test": "CAD"},
          "perBuyerTimeouts":{"https://a.com":100,"*":150},
          "perBuyerCumulativeTimeouts":{"https://a.com":101,"*":151},
          "reportingTimeout":200,
          "perBuyerPrioritySignals":{"https://a.com":{"signals_c":0.5},
                                     "*":            {"signals_d":0}},
          "deprecatedRenderURLReplacements": {"${SELLER}":"ExampleSSP"}
        })";
  RunReportResultCreatedScriptExpectingResult(
      "auctionConfig", /*extra_code=*/std::string(), kExpectedJson1,
      /*expected_report_url=*/std::nullopt);

  // Clear NonSharedParams(), and add and populate two component auctions, each
  // with one the mandatory `seller` and `decision_logic_url` fields filled in,
  // and one extra field: One that's directly a member of the AuctionAdConfig,
  // and one that's in the non-shared params. A default reporting timeout is set
  // when auction config does not have one.
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
  std::string default_reporting_timeout =
      base::NumberToString(AuctionV8Helper::kScriptTimeout.InMilliseconds());
  std::string kExpectedJson2 = base::StringPrintf(
      R"({"seller":"https://example.test",
          "decisionLogicURL":"https://example.test/auction.js",
          "decisionLogicUrl":"https://example.test/auction.js",
          "reportingTimeout":%s,
          "trustedScoringSignalsURL":"https://example.test/scoring_signals.json",
          "trustedScoringSignalsUrl":"https://example.test/scoring_signals.json",
          "componentAuctions":[
              {"seller":"https://component1.com",
               "decisionLogicURL":"https://component1.com/script.js",
               "decisionLogicUrl":"https://component1.com/script.js",
               "reportingTimeout":%s,
               "sellerTimeout":111},
              {"seller":"https://component2.com",
               "decisionLogicURL":"https://component2.com/script.js",
               "decisionLogicUrl":"https://component2.com/script.js",
               "reportingTimeout":%s,
               "trustedScoringSignalsURL":"https://component2.com/signals.json",
               "trustedScoringSignalsUrl":"https://component2.com/signals.json"}
          ]})",
      default_reporting_timeout.c_str(), default_reporting_timeout.c_str(),
      default_reporting_timeout.c_str());
  RunReportResultCreatedScriptExpectingResult(
      "auctionConfig", /*extra_code=*/std::string(), kExpectedJson2,
      /*expected_report_url=*/std::nullopt);
}

// Check that accessing `decisionLogicUrl` and `trustedScoringSignalsUrl` of
// `auctionConfig` displays a warning.
//
// TODO(crbug.com/40266734): Remove this test when the fields are
// removed.
TEST_F(SellerWorkletTest, ReportResultAuctionConfigUrlDeprecationWarning) {
  ScopedInspectorSupport inspector_support(v8_helper().get());
  decision_logic_url_ = GURL("https://url.test/");
  trusted_scoring_signals_url_ =
      GURL("https://url.test/trusted_scoring_signals");

  // Include a pair of component auctions, to make sure it works for them as
  // well.
  auto& component_auctions =
      auction_ad_config_non_shared_params_.component_auctions;
  component_auctions.emplace_back(blink::AuctionConfig());
  component_auctions[0].seller =
      url::Origin::Create(GURL("https://component1.test"));
  component_auctions[0].decision_logic_url =
      GURL("https://component1.test/script.js");
  component_auctions[0].trusted_scoring_signals_url =
      GURL("https://component1.test/signals.js");

  component_auctions.emplace_back(blink::AuctionConfig());
  component_auctions[1].seller =
      url::Origin::Create(GURL("https://component2.test"));
  component_auctions[1].decision_logic_url =
      GURL("https://component2.test/script.js");
  component_auctions[1].trusted_scoring_signals_url =
      GURL("https://component2.test/signals.js");

  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        CreateReportToScript(
                            /*raw_return_value=*/
                            R"(auctionConfig.decisionLogicUrl + '|' +
         auctionConfig.trustedScoringSignalsUrl + '|' +
         auctionConfig.componentAuctions[0].decisionLogicUrl + '|' +
         auctionConfig.componentAuctions[0].trustedScoringSignalsUrl + '|' +
         auctionConfig.componentAuctions[1].decisionLogicUrl + '|' +
         auctionConfig.componentAuctions[1].trustedScoringSignalsUrl)"));

  SellerWorklet* worklet_impl = nullptr;
  auto worklet =
      CreateWorklet(/*pause_for_debugger_on_start=*/false, &worklet_impl);
  int id = worklet_impl->context_group_ids_for_testing()[0];
  TestChannel* channel =
      inspector_support.ConnectDebuggerSessionAndRuntimeEnable(id);

  base::RunLoop run_loop;
  RunReportResultExpectingResultAsync(
      worklet_impl, /*expected_signals_for_winner=*/
      "\"https://url.test/|"
      "https://url.test/trusted_scoring_signals|"
      "https://component1.test/script.js|"
      "https://component1.test/signals.js|"
      "https://component2.test/script.js|"
      "https://component2.test/signals.js\"",
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{}, /*expected_pa_requests=*/{},
      /*expected_reporting_latency_timeout=*/false, /*expected_errors=*/{},
      run_loop.QuitClosure());
  run_loop.Run();

  channel->WaitForAndValidateConsoleMessage(
      "warning", /*json_args=*/
      "[{\"type\":\"string\", "
      "\"value\":\"auctionConfig.decisionLogicUrl is deprecated. Please use "
      "auctionConfig.decisionLogicURL instead.\"}]",
      /*stack_trace_size=*/1, /*function=*/"reportResult", decision_logic_url_,
      /*line_number=*/10);

  channel->WaitForAndValidateConsoleMessage(
      "warning", /*json_args=*/
      "[{\"type\":\"string\", "
      "\"value\":\"auctionConfig.trustedScoringSignalsUrl is deprecated. "
      "Please use auctionConfig.trustedScoringSignalsURL instead.\"}]",
      /*stack_trace_size=*/1, /*function=*/"reportResult", decision_logic_url_,
      /*line_number=*/11);

  channel->WaitForAndValidateConsoleMessage(
      "warning", /*json_args=*/
      "[{\"type\":\"string\", "
      "\"value\":\"auctionConfig.decisionLogicUrl is deprecated. Please use "
      "auctionConfig.decisionLogicURL instead.\"}]",
      /*stack_trace_size=*/1, /*function=*/"reportResult", decision_logic_url_,
      /*line_number=*/12);

  channel->WaitForAndValidateConsoleMessage(
      "warning", /*json_args=*/
      "[{\"type\":\"string\", "
      "\"value\":\"auctionConfig.trustedScoringSignalsUrl is deprecated. "
      "Please use auctionConfig.trustedScoringSignalsURL instead.\"}]",
      /*stack_trace_size=*/1, /*function=*/"reportResult", decision_logic_url_,
      /*line_number=*/13);

  channel->WaitForAndValidateConsoleMessage(
      "warning", /*json_args=*/
      "[{\"type\":\"string\", "
      "\"value\":\"auctionConfig.decisionLogicUrl is deprecated. Please use "
      "auctionConfig.decisionLogicURL instead.\"}]",
      /*stack_trace_size=*/1, /*function=*/"reportResult", decision_logic_url_,
      /*line_number=*/14);

  channel->WaitForAndValidateConsoleMessage(
      "warning", /*json_args=*/
      "[{\"type\":\"string\", "
      "\"value\":\"auctionConfig.trustedScoringSignalsUrl is deprecated. "
      "Please use auctionConfig.trustedScoringSignalsURL instead.\"}]",
      /*stack_trace_size=*/1, /*function=*/"reportResult", decision_logic_url_,
      /*line_number=*/15);

  channel->ExpectNoMoreConsoleEvents();
}

// Check that accessing `decisionLogicURL` and `trustedScoringSignalsURL` of
// `auctionConfig` does not display a warning.
//
// TODO(crbug.com/40266734): Remove this test when `decisionLogicUrl` and
// `trustedScoringSignalsUrl` are removed.
TEST_F(SellerWorkletTest, ReportResultAuctionConfigUrlNoDeprecationWarning) {
  ScopedInspectorSupport inspector_support(v8_helper().get());
  decision_logic_url_ = GURL("https://url.test/");
  trusted_scoring_signals_url_ =
      GURL("https://url.test/trusted_scoring_signals");

  // Include a pair of component auctions, to make sure it works for them as
  // well.
  auto& component_auctions =
      auction_ad_config_non_shared_params_.component_auctions;
  component_auctions.emplace_back(blink::AuctionConfig());
  component_auctions[0].seller =
      url::Origin::Create(GURL("https://component1.test"));
  component_auctions[0].decision_logic_url =
      GURL("https://component1.test/script.js");
  component_auctions[0].trusted_scoring_signals_url =
      GURL("https://component1.test/signals.js");

  component_auctions.emplace_back(blink::AuctionConfig());
  component_auctions[1].seller =
      url::Origin::Create(GURL("https://component2.test"));
  component_auctions[1].decision_logic_url =
      GURL("https://component2.test/script.js");
  component_auctions[1].trusted_scoring_signals_url =
      GURL("https://component2.test/signals.js");

  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        CreateReportToScript(
                            /*raw_return_value=*/
                            R"(auctionConfig.decisionLogicURL + '|' +
         auctionConfig.trustedScoringSignalsURL + '|' +
         auctionConfig.componentAuctions[0].decisionLogicURL + '|' +
         auctionConfig.componentAuctions[0].trustedScoringSignalsURL + '|' +
         auctionConfig.componentAuctions[1].decisionLogicURL + '|' +
         auctionConfig.componentAuctions[1].trustedScoringSignalsURL)"));

  SellerWorklet* worklet_impl = nullptr;
  auto worklet =
      CreateWorklet(/*pause_for_debugger_on_start=*/false, &worklet_impl);
  int id = worklet_impl->context_group_ids_for_testing()[0];
  TestChannel* channel =
      inspector_support.ConnectDebuggerSessionAndRuntimeEnable(id);

  base::RunLoop run_loop;
  RunReportResultExpectingResultAsync(
      worklet_impl, /*expected_signals_for_winner=*/
      "\"https://url.test/|"
      "https://url.test/trusted_scoring_signals|"
      "https://component1.test/script.js|"
      "https://component1.test/signals.js|"
      "https://component2.test/script.js|"
      "https://component2.test/signals.js\"",
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{}, /*expected_pa_requests=*/{},
      /*expected_reporting_latency_timeout=*/false, /*expected_errors=*/{},
      run_loop.QuitClosure());
  run_loop.Run();
  channel->ExpectNoMoreConsoleEvents();
}

TEST_F(SellerWorkletTest,
       ReportResultDirectFromSellerSignalsHeaderAdSlotParam) {
  direct_from_seller_auction_signals_header_ad_slot_ = R"("abcde")";
  direct_from_seller_seller_signals_header_ad_slot_ = R"("abcdefg")";

  const char kExpectedJson[] =
      R"({"auctionSignals":"abcde", "sellerSignals":"abcdefg"})";

  RunReportResultCreatedScriptExpectingResult(
      "directFromSellerSignals", /*extra_code=*/std::string(), kExpectedJson,
      /*expected_report_url=*/std::nullopt);
}

TEST_F(SellerWorkletTest, ReportResultAuctionConfigParamPerBuyerTimeouts) {
  // Empty AuctionAdConfig, with nothing filled in, except the seller and
  // decision logic URL.
  decision_logic_url_ = GURL("https://example.test/auction.js");
  RunReportResultCreatedScriptExpectingResult(
      "auctionConfig", /*extra_code=*/std::string(),
      base::StringPrintf(
          R"({"seller":"https://example.test",)"
          R"("decisionLogicURL":"https://example.test/auction.js",)"
          R"("decisionLogicUrl":"https://example.test/auction.js",)"
          R"("reportingTimeout": %s})",
          base::NumberToString(AuctionV8Helper::kScriptTimeout.InMilliseconds())
              .c_str()),
      /*expected_report_url=*/std::nullopt);

  {
    blink::AuctionConfig::BuyerTimeouts buyer_timeouts;
    buyer_timeouts.per_buyer_timeouts.emplace();
    auction_ad_config_non_shared_params_.buyer_timeouts =
        blink::AuctionConfig::MaybePromiseBuyerTimeouts::FromValue(
            std::move(buyer_timeouts));

    RunReportResultCreatedScriptExpectingResult(
        "auctionConfig", /*extra_code=*/std::string(),
        base::StringPrintf(
            R"({"seller":"https://example.test",)"
            R"("decisionLogicURL":"https://example.test/auction.js",)"
            R"("decisionLogicUrl":"https://example.test/auction.js",)"
            R"("perBuyerTimeouts":{},)"
            R"("reportingTimeout": %s})",
            base::NumberToString(
                AuctionV8Helper::kScriptTimeout.InMilliseconds())
                .c_str()),
        /*expected_report_url=*/std::nullopt);
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
        base::StringPrintf(
            R"({"seller":"https://example.test",)"
            R"("decisionLogicURL":"https://example.test/auction.js",)"
            R"("decisionLogicUrl":"https://example.test/auction.js",)"
            R"("perBuyerTimeouts":{"*":150},)"
            R"("reportingTimeout": %s})",
            base::NumberToString(
                AuctionV8Helper::kScriptTimeout.InMilliseconds())
                .c_str()),
        /*expected_report_url=*/std::nullopt);
  }
}

TEST_F(SellerWorkletTest, ReportResultExperimentGroupIdParam) {
  RunReportResultCreatedScriptExpectingResult(
      R"("experimentGroupId" in auctionConfig ? 1 : 0)",
      /*extra_code=*/std::string(), /*expected_signals_for_winner=*/"0",
      /*expected_report_url=*/std::nullopt);

  experiment_group_id_ = 954u;
  RunReportResultCreatedScriptExpectingResult(
      "auctionConfig.experimentGroupId",
      /*extra_code=*/std::string(), /*expected_signals_for_winner=*/"954",
      /*expected_report_url=*/std::nullopt);
}

TEST_F(SellerWorkletTest, ReportResultDataVersion) {
  browser_signal_data_version_ = 20;
  RunReportResultCreatedScriptExpectingResult(
      "browserSignals.dataVersion", /*extra_code=*/std::string(),
      /*expected_signals_for_winner=*/"20",
      /*expected_report_url=*/std::nullopt);
}

TEST_F(SellerWorkletTest, ReportResultJsFetchLatency) {
  auto seller_worklet = CreateWorklet();
  ASSERT_TRUE(seller_worklet);
  base::RunLoop run_loop;
  seller_worklet->ReportResult(
      auction_ad_config_non_shared_params_, direct_from_seller_seller_signals_,
      direct_from_seller_seller_signals_header_ad_slot_,
      direct_from_seller_auction_signals_,
      direct_from_seller_auction_signals_header_ad_slot_,
      browser_signals_other_seller_.Clone(),
      browser_signal_interest_group_owner_,
      browser_signal_buyer_and_seller_reporting_id_,
      browser_signal_selected_buyer_and_seller_reporting_id_,
      browser_signal_render_url_, bid_, bid_currency_,
      browser_signal_desireability_, browser_signal_highest_scoring_other_bid_,
      browser_signal_highest_scoring_other_bid_currency_,
      browser_signals_component_auction_report_result_params_.Clone(),
      browser_signal_data_version_,
      /*trace_id=*/1,
      base::BindOnce(
          [](base::OnceClosure done_closure,
             const std::optional<std::string>& signals_for_winner,
             const std::optional<GURL>& report_url,
             const base::flat_map<std::string, GURL>& ad_beacon_map,
             PrivateAggregationRequests pa_requests,
             auction_worklet::mojom::SellerTimingMetricsPtr timing_metrics,
             const std::vector<std::string>& errors) {
            ASSERT_TRUE(timing_metrics->js_fetch_latency.has_value());
            EXPECT_EQ(base::Milliseconds(235),
                      *timing_metrics->js_fetch_latency);
            std::move(done_closure).Run();
          },
          run_loop.QuitClosure()));
  task_environment_.FastForwardBy(base::Milliseconds(235));
  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        CreateScoreAdScript(/*raw_return_value=*/"1", ""));
  run_loop.Run();
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
      AddResponse(&url_loader_factory_, response.response_url,
                  response.response_type,
                  /*charset=*/std::nullopt, response.content, response.headers);
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
TEST_P(SellerWorkletMultiThreadingTest, ScriptIsolation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      blink::features::kFledgeAlwaysReuseSellerContext);
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
          browser_signal_selected_buyer_and_seller_reporting_id_,
          browser_signal_buyer_and_seller_reporting_id_,
          browser_signal_ad_components_, browser_signal_bidding_duration_msecs_,
          browser_signal_render_size_,
          browser_signal_for_debugging_only_in_cooldown_or_lockout_,
          seller_timeout_,
          /*trace_id=*/1, bidder_joining_origin_,
          TestScoreAdClient::Create(base::BindLambdaForTesting(
              [&run_loop](
                  double score, mojom::RejectReason reject_reason,
                  mojom::ComponentAuctionModifiedBidParamsPtr
                      component_auction_modified_bid_params,
                  std::optional<double> bid_in_seller_currency,
                  std::optional<uint32_t> scoring_signals_data_version,
                  const std::optional<GURL>& debug_loss_report_url,
                  const std::optional<GURL>& debug_win_report_url,
                  PrivateAggregationRequests pa_requests,
                  RealTimeReportingContributions real_time_contributions,
                  mojom::SellerTimingMetricsPtr score_ad_timing_metrics,
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
          browser_signal_selected_buyer_and_seller_reporting_id_,
          browser_signal_render_url_, bid_, bid_currency_,
          browser_signal_desireability_,
          browser_signal_highest_scoring_other_bid_,
          browser_signal_highest_scoring_other_bid_currency_,
          browser_signals_component_auction_report_result_params_.Clone(),
          browser_signal_data_version_,
          /*trace_id=*/1,
          base::BindLambdaForTesting(
              [&run_loop](
                  const std::optional<std::string>& signals_for_winner,
                  const std::optional<GURL>& report_url,
                  const base::flat_map<std::string, GURL>& ad_beacon_map,
                  PrivateAggregationRequests pa_requests,
                  auction_worklet::mojom::SellerTimingMetricsPtr timing_metrics,
                  const std::vector<std::string>& errors) {
                EXPECT_EQ("2", signals_for_winner);
                EXPECT_TRUE(errors.empty());
                run_loop.Quit();
              }));
      run_loop.Run();
    }
  }
}

TEST_F(SellerWorkletTest,
       ContextIsReusedIfFledgeAlwaysReuseSellerContextEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      blink::features::kFledgeAlwaysReuseSellerContext);
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
  std::vector<double> expected_scores = {2, 1, 1};
  for (int i = 0; i < 3; ++i) {
    double expected_score = expected_scores[i];
    base::RunLoop run_loop;
    seller_worklet->ScoreAd(
        ad_metadata_, bid_, bid_currency_, auction_ad_config_non_shared_params_,
        direct_from_seller_seller_signals_,
        direct_from_seller_seller_signals_header_ad_slot_,
        direct_from_seller_auction_signals_,
        direct_from_seller_auction_signals_header_ad_slot_,
        browser_signals_other_seller_.Clone(), component_expect_bid_currency_,
        browser_signal_interest_group_owner_, browser_signal_render_url_,
        browser_signal_selected_buyer_and_seller_reporting_id_,
        browser_signal_buyer_and_seller_reporting_id_,
        browser_signal_ad_components_, browser_signal_bidding_duration_msecs_,
        browser_signal_render_size_,
        browser_signal_for_debugging_only_in_cooldown_or_lockout_,
        seller_timeout_,
        /*trace_id=*/1, bidder_joining_origin_,
        TestScoreAdClient::Create(base::BindLambdaForTesting(
            [&run_loop, &expected_score](
                double score, mojom::RejectReason reject_reason,
                mojom::ComponentAuctionModifiedBidParamsPtr
                    component_auction_modified_bid_params,
                std::optional<double> bid_in_seller_currency,
                std::optional<uint32_t> scoring_signals_data_version,
                const std::optional<GURL>& debug_loss_report_url,
                const std::optional<GURL>& debug_win_report_url,
                PrivateAggregationRequests pa_requests,
                RealTimeReportingContributions real_time_contributions,
                mojom::SellerTimingMetricsPtr score_ad_timing_metrics,
                mojom::ScoreAdDependencyLatenciesPtr
                    score_ad_dependency_latencies,
                const std::vector<std::string>& errors) {
              EXPECT_EQ(expected_score, score);
              EXPECT_FALSE(scoring_signals_data_version.has_value());
              EXPECT_TRUE(errors.empty());
              run_loop.Quit();
            })));
    run_loop.Run();
  }

  // The Report worklet should still get a fresh context.
  base::RunLoop run_loop;
  seller_worklet->ReportResult(
      auction_ad_config_non_shared_params_, direct_from_seller_seller_signals_,
      direct_from_seller_seller_signals_header_ad_slot_,
      direct_from_seller_auction_signals_,
      direct_from_seller_auction_signals_header_ad_slot_,
      browser_signals_other_seller_.Clone(),
      browser_signal_interest_group_owner_,
      browser_signal_buyer_and_seller_reporting_id_,
      browser_signal_selected_buyer_and_seller_reporting_id_,
      browser_signal_render_url_, bid_, bid_currency_,
      browser_signal_desireability_, browser_signal_highest_scoring_other_bid_,
      browser_signal_highest_scoring_other_bid_currency_,
      browser_signals_component_auction_report_result_params_.Clone(),
      browser_signal_data_version_,
      /*trace_id=*/1,
      base::BindLambdaForTesting(
          [&run_loop](
              const std::optional<std::string>& signals_for_winner,
              const std::optional<GURL>& report_url,
              const base::flat_map<std::string, GURL>& ad_beacon_map,
              PrivateAggregationRequests pa_requests,
              auction_worklet::mojom::SellerTimingMetricsPtr timing_metrics,
              const std::vector<std::string>& errors) {
            EXPECT_EQ("2", signals_for_winner);
            EXPECT_TRUE(errors.empty());
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(
    SellerWorkletTwoThreadsTest,
    OneWorklet_ContextIsReusedInSameThreadIfFledgeAlwaysReuseSellerContextEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      blink::features::kFledgeAlwaysReuseSellerContext);
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

  // Context is reused within the same thread. Since we use a round-robin
  // scheduling approach and the thread pool has a size of 2, it means the first
  // two ScoreAds should have score=2 and the rest of them should have score=1.
  std::vector<double> expected_scores = {2, 2, 1, 1, 1, 1};
  for (int i = 0; i < 6; ++i) {
    double expected_score = expected_scores[i];
    base::RunLoop run_loop;
    seller_worklet->ScoreAd(
        ad_metadata_, bid_, bid_currency_, auction_ad_config_non_shared_params_,
        direct_from_seller_seller_signals_,
        direct_from_seller_seller_signals_header_ad_slot_,
        direct_from_seller_auction_signals_,
        direct_from_seller_auction_signals_header_ad_slot_,
        browser_signals_other_seller_.Clone(), component_expect_bid_currency_,
        browser_signal_interest_group_owner_, browser_signal_render_url_,
        browser_signal_selected_buyer_and_seller_reporting_id_,
        browser_signal_buyer_and_seller_reporting_id_,
        browser_signal_ad_components_, browser_signal_bidding_duration_msecs_,
        browser_signal_render_size_,
        browser_signal_for_debugging_only_in_cooldown_or_lockout_,
        seller_timeout_,
        /*trace_id=*/1, bidder_joining_origin_,
        TestScoreAdClient::Create(base::BindLambdaForTesting(
            [&run_loop, &expected_score](
                double score, mojom::RejectReason reject_reason,
                mojom::ComponentAuctionModifiedBidParamsPtr
                    component_auction_modified_bid_params,
                std::optional<double> bid_in_seller_currency,
                std::optional<uint32_t> scoring_signals_data_version,
                const std::optional<GURL>& debug_loss_report_url,
                const std::optional<GURL>& debug_win_report_url,
                PrivateAggregationRequests pa_requests,
                RealTimeReportingContributions real_time_contributions,
                mojom::SellerTimingMetricsPtr score_ad_timing_metrics,
                mojom::ScoreAdDependencyLatenciesPtr
                    score_ad_dependency_latencies,
                const std::vector<std::string>& errors) {
              EXPECT_EQ(expected_score, score);
              EXPECT_FALSE(scoring_signals_data_version.has_value());
              EXPECT_TRUE(errors.empty());
              run_loop.Quit();
            })));
    run_loop.Run();
  }

  // The Report worklet should still get a fresh context.
  base::RunLoop run_loop;
  seller_worklet->ReportResult(
      auction_ad_config_non_shared_params_, direct_from_seller_seller_signals_,
      direct_from_seller_seller_signals_header_ad_slot_,
      direct_from_seller_auction_signals_,
      direct_from_seller_auction_signals_header_ad_slot_,
      browser_signals_other_seller_.Clone(),
      browser_signal_interest_group_owner_,
      browser_signal_buyer_and_seller_reporting_id_,
      browser_signal_selected_buyer_and_seller_reporting_id_,
      browser_signal_render_url_, bid_, bid_currency_,
      browser_signal_desireability_, browser_signal_highest_scoring_other_bid_,
      browser_signal_highest_scoring_other_bid_currency_,
      browser_signals_component_auction_report_result_params_.Clone(),
      browser_signal_data_version_,
      /*trace_id=*/1,
      base::BindLambdaForTesting(
          [&run_loop](
              const std::optional<std::string>& signals_for_winner,
              const std::optional<GURL>& report_url,
              const base::flat_map<std::string, GURL>& ad_beacon_map,
              PrivateAggregationRequests pa_requests,
              auction_worklet::mojom::SellerTimingMetricsPtr timing_metrics,
              const std::vector<std::string>& errors) {
            EXPECT_EQ("2", signals_for_winner);
            EXPECT_TRUE(errors.empty());
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(
    SellerWorkletTwoThreadsTest,
    TwoWorklets_ContextIsReusedInSameThreadIfFledgeAlwaysReuseSellerContextEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      blink::features::kFledgeAlwaysReuseSellerContext);
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

  auto seller_worklet1 = CreateWorklet();
  auto seller_worklet2 = CreateWorklet();

  // Context is reused within the same thread. Since we use a round-robin
  // scheduling approach and the thread pool has a size of 2, it means the first
  // two ScoreAds should have score=2 and the rest of them should have score=1.
  //
  // Together with the "OneWorklet_XXX" test variant above, this also shows that
  // the round-robin scheduling state is not local to each SellerWorklet, but
  // all worklets in the same test case will share the same state.
  std::vector<double> expected_scores = {2, 2, 1, 1, 1, 1};
  for (int i = 0; i < 6; ++i) {
    auto* seller_worklet = (i % 2 == 0) ? &seller_worklet1 : &seller_worklet2;

    double expected_score = expected_scores[i];
    base::RunLoop run_loop;
    (*seller_worklet)
        ->ScoreAd(
            ad_metadata_, bid_, bid_currency_,
            auction_ad_config_non_shared_params_,
            direct_from_seller_seller_signals_,
            direct_from_seller_seller_signals_header_ad_slot_,
            direct_from_seller_auction_signals_,
            direct_from_seller_auction_signals_header_ad_slot_,
            browser_signals_other_seller_.Clone(),
            component_expect_bid_currency_,
            browser_signal_interest_group_owner_, browser_signal_render_url_,
            browser_signal_selected_buyer_and_seller_reporting_id_,
            browser_signal_buyer_and_seller_reporting_id_,
            browser_signal_ad_components_,
            browser_signal_bidding_duration_msecs_, browser_signal_render_size_,
            browser_signal_for_debugging_only_in_cooldown_or_lockout_,
            seller_timeout_,
            /*trace_id=*/1, bidder_joining_origin_,
            TestScoreAdClient::Create(base::BindLambdaForTesting(
                [&run_loop, &expected_score](
                    double score, mojom::RejectReason reject_reason,
                    mojom::ComponentAuctionModifiedBidParamsPtr
                        component_auction_modified_bid_params,
                    std::optional<double> bid_in_seller_currency,
                    std::optional<uint32_t> scoring_signals_data_version,
                    const std::optional<GURL>& debug_loss_report_url,
                    const std::optional<GURL>& debug_win_report_url,
                    PrivateAggregationRequests pa_requests,
                    RealTimeReportingContributions real_time_contributions,
                    mojom::SellerTimingMetricsPtr score_ad_timing_metrics,
                    mojom::ScoreAdDependencyLatenciesPtr
                        score_ad_dependency_latencies,
                    const std::vector<std::string>& errors) {
                  EXPECT_EQ(expected_score, score);
                  EXPECT_FALSE(scoring_signals_data_version.has_value());
                  EXPECT_TRUE(errors.empty());
                  run_loop.Quit();
                })));
    run_loop.Run();
  }

  // The Report worklet should still get a fresh context.
  for (auto* seller_worklet : {&seller_worklet1, &seller_worklet2}) {
    base::RunLoop run_loop;
    (*seller_worklet)
        ->ReportResult(
            auction_ad_config_non_shared_params_,
            direct_from_seller_seller_signals_,
            direct_from_seller_seller_signals_header_ad_slot_,
            direct_from_seller_auction_signals_,
            direct_from_seller_auction_signals_header_ad_slot_,
            browser_signals_other_seller_.Clone(),
            browser_signal_interest_group_owner_,
            browser_signal_buyer_and_seller_reporting_id_,
            browser_signal_selected_buyer_and_seller_reporting_id_,
            browser_signal_render_url_, bid_, bid_currency_,
            browser_signal_desireability_,
            browser_signal_highest_scoring_other_bid_,
            browser_signal_highest_scoring_other_bid_currency_,
            browser_signals_component_auction_report_result_params_.Clone(),
            browser_signal_data_version_,
            /*trace_id=*/1,
            base::BindLambdaForTesting(
                [&run_loop](
                    const std::optional<std::string>& signals_for_winner,
                    const std::optional<GURL>& report_url,
                    const base::flat_map<std::string, GURL>& ad_beacon_map,
                    PrivateAggregationRequests pa_requests,
                    auction_worklet::mojom::SellerTimingMetricsPtr
                        timing_metrics,
                    const std::vector<std::string>& errors) {
                  EXPECT_EQ("2", signals_for_winner);
                  EXPECT_TRUE(errors.empty());
                  run_loop.Quit();
                }));
    run_loop.Run();
  }
}

TEST_F(SellerWorkletTwoThreadsTest,
       TrustedScoringSignalsTaskTriggersNextThreadIndexCallback) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      blink::features::kFledgeAlwaysReuseSellerContext);
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

  // Before the second ScoreAd (i == 1), create another worklet with non-empty
  // `trusted_scoring_signals_url_`. The first and the third ScoreAd are
  // expected to have a new context. This implies that creating a worklet with
  // `trusted_scoring_signals_url_` will trigger the
  // `GetNextThreadIndexCallback`.
  std::vector<double> expected_scores = {2, 1, 2, 1, 1, 1};
  for (int i = 0; i < 6; ++i) {
    if (i == 1) {
      trusted_scoring_signals_url_ = GURL("https://url.test/trustedsignals");
      auto new_worklet = CreateWorklet();
    }

    double expected_score = expected_scores[i];
    base::RunLoop run_loop;
    seller_worklet->ScoreAd(
        ad_metadata_, bid_, bid_currency_, auction_ad_config_non_shared_params_,
        direct_from_seller_seller_signals_,
        direct_from_seller_seller_signals_header_ad_slot_,
        direct_from_seller_auction_signals_,
        direct_from_seller_auction_signals_header_ad_slot_,
        browser_signals_other_seller_.Clone(), component_expect_bid_currency_,
        browser_signal_interest_group_owner_, browser_signal_render_url_,
        browser_signal_selected_buyer_and_seller_reporting_id_,
        browser_signal_buyer_and_seller_reporting_id_,
        browser_signal_ad_components_, browser_signal_bidding_duration_msecs_,
        browser_signal_render_size_,
        browser_signal_for_debugging_only_in_cooldown_or_lockout_,
        seller_timeout_,
        /*trace_id=*/1, bidder_joining_origin_,
        TestScoreAdClient::Create(base::BindLambdaForTesting(
            [&run_loop, &expected_score](
                double score, mojom::RejectReason reject_reason,
                mojom::ComponentAuctionModifiedBidParamsPtr
                    component_auction_modified_bid_params,
                std::optional<double> bid_in_seller_currency,
                std::optional<uint32_t> scoring_signals_data_version,
                const std::optional<GURL>& debug_loss_report_url,
                const std::optional<GURL>& debug_win_report_url,
                PrivateAggregationRequests pa_requests,
                RealTimeReportingContributions real_time_contributions,
                mojom::SellerTimingMetricsPtr score_ad_timing_metrics,
                mojom::ScoreAdDependencyLatenciesPtr
                    score_ad_dependency_latencies,
                const std::vector<std::string>& errors) {
              EXPECT_EQ(expected_score, score);
              EXPECT_FALSE(scoring_signals_data_version.has_value());
              EXPECT_TRUE(errors.empty());
              run_loop.Quit();
            })));
    run_loop.Run();
  }
}

TEST_F(SellerWorkletTest, ContextReuseDoesNotCrashLazyFiller) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      blink::features::kFledgeAlwaysReuseSellerContext);
  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        R"(
        scoreAd = function(adMetadata, bid, auctionConfig){
            if (!globalThis.auctionConfig) {
                globalThis.auctionConfig = auctionConfig;
            } else {
                // Access a lazily loaded attribute from a prior run
                // of this function.
                console.log(globalThis.auctionConfig.decisionLogicUrl);
            }
            return 1;
        };
        reportResult = scoreAd;
      )");
  auto seller_worklet = CreateWorklet();
  ASSERT_TRUE(seller_worklet);
  for (int i = 0; i < 3; ++i) {
    double expected_score = 1;
    base::RunLoop run_loop;
    seller_worklet->ScoreAd(
        ad_metadata_, bid_, bid_currency_, auction_ad_config_non_shared_params_,
        direct_from_seller_seller_signals_,
        direct_from_seller_seller_signals_header_ad_slot_,
        direct_from_seller_auction_signals_,
        direct_from_seller_auction_signals_header_ad_slot_,
        browser_signals_other_seller_.Clone(), component_expect_bid_currency_,
        browser_signal_interest_group_owner_, browser_signal_render_url_,
        browser_signal_selected_buyer_and_seller_reporting_id_,
        browser_signal_buyer_and_seller_reporting_id_,
        browser_signal_ad_components_, browser_signal_bidding_duration_msecs_,
        browser_signal_render_size_,
        browser_signal_for_debugging_only_in_cooldown_or_lockout_,
        seller_timeout_,
        /*trace_id=*/1, bidder_joining_origin_,
        TestScoreAdClient::Create(base::BindLambdaForTesting(
            [&run_loop, &expected_score](
                double score, mojom::RejectReason reject_reason,
                mojom::ComponentAuctionModifiedBidParamsPtr
                    component_auction_modified_bid_params,
                std::optional<double> bid_in_seller_currency,
                std::optional<uint32_t> scoring_signals_data_version,
                const std::optional<GURL>& debug_loss_report_url,
                const std::optional<GURL>& debug_win_report_url,
                PrivateAggregationRequests pa_requests,
                RealTimeReportingContributions real_time_contributions,
                mojom::SellerTimingMetricsPtr score_ad_timing_metrics,
                mojom::ScoreAdDependencyLatenciesPtr
                    score_ad_dependency_latencies,
                const std::vector<std::string>& errors) {
              EXPECT_EQ(expected_score, score);
              EXPECT_FALSE(scoring_signals_data_version.has_value());
              EXPECT_TRUE(errors.empty());
              run_loop.Quit();
            })));
    run_loop.Run();
  }
}

TEST_F(SellerWorkletTest, DeleteBeforeScoreAdCallback) {
  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        CreateBasicSellAdScript());
  auto seller_worklet = CreateWorklet();
  ASSERT_TRUE(seller_worklet);

  base::WaitableEvent* event_handle = WedgeV8Thread(v8_helper().get());
  seller_worklet->ScoreAd(
      ad_metadata_, bid_, bid_currency_, auction_ad_config_non_shared_params_,
      direct_from_seller_seller_signals_,
      direct_from_seller_seller_signals_header_ad_slot_,
      direct_from_seller_auction_signals_,
      direct_from_seller_auction_signals_header_ad_slot_,
      browser_signals_other_seller_.Clone(), component_expect_bid_currency_,
      browser_signal_interest_group_owner_, browser_signal_render_url_,
      browser_signal_selected_buyer_and_seller_reporting_id_,
      browser_signal_buyer_and_seller_reporting_id_,
      browser_signal_ad_components_, browser_signal_bidding_duration_msecs_,
      browser_signal_render_size_,
      browser_signal_for_debugging_only_in_cooldown_or_lockout_,
      seller_timeout_,
      /*trace_id=*/1, bidder_joining_origin_,
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

  base::WaitableEvent* event_handle = WedgeV8Thread(v8_helper().get());
  seller_worklet->ReportResult(
      auction_ad_config_non_shared_params_, direct_from_seller_seller_signals_,
      direct_from_seller_seller_signals_header_ad_slot_,
      direct_from_seller_auction_signals_,
      direct_from_seller_auction_signals_header_ad_slot_,
      browser_signals_other_seller_.Clone(),
      browser_signal_interest_group_owner_,
      browser_signal_buyer_and_seller_reporting_id_,
      browser_signal_selected_buyer_and_seller_reporting_id_,
      browser_signal_render_url_, bid_, bid_currency_,
      browser_signal_desireability_, browser_signal_highest_scoring_other_bid_,
      browser_signal_highest_scoring_other_bid_currency_,
      browser_signals_component_auction_report_result_params_.Clone(),
      browser_signal_data_version_,
      /*trace_id=*/1,
      base::BindOnce(
          [](const std::optional<std::string>& signals_for_winner,
             const std::optional<GURL>& report_url,
             const base::flat_map<std::string, GURL>& ad_beacon_map,
             PrivateAggregationRequests pa_requests,
             auction_worklet::mojom::SellerTimingMetricsPtr timing_metrics,
             const std::vector<std::string>& errors) {
            ADD_FAILURE()
                << "Callback should not be invoked since worklet deleted";
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
  int id = worklet_impl->context_group_ids_for_testing()[0];

  // Queue a ScoreAd() call, which should not happen immediately since loading
  // is paused.
  base::RunLoop run_loop;
  RunScoreAdOnWorkletAsync(
      worklet.get(), /*expected_score=*/10,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_reject_reason=*/
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_real_time_contributions=*/{},
      /*expected_bid_in_seller_currency=*/std::nullopt,
      /*expected_score_ad_timeout=*/false,
      /*expected_signals_fetch_latency=*/std::nullopt,
      /*expected_code_ready_latency=*/std::nullopt, run_loop.QuitClosure());

  // Give it a chance to fetch.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(run_loop.AnyQuitCalled());

  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        CreateScoreAdScript("10"));

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(run_loop.AnyQuitCalled());

  // Let the ScoreAd() call run.
  v8_helper()->v8_runner()->PostTask(
      FROM_HERE, base::BindOnce([](scoped_refptr<AuctionV8Helper> v8_helper,
                                   int id) { v8_helper->Resume(id); },
                                v8_helper(), id));

  run_loop.RunUntilIdle();
}

TEST_P(SellerWorkletMultiThreadingTest, PauseOnStartDelete) {
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
  int id = worklet_impl->context_group_ids_for_testing()[0];

  // Delete the worklet.
  worklet.reset();
  task_environment_.RunUntilIdle();

  // Try to resume post-delete. Should not crash
  v8_helper()->v8_runner()->PostTask(
      FROM_HERE, base::BindOnce([](scoped_refptr<AuctionV8Helper> v8_helper,
                                   int id) { v8_helper->Resume(id); },
                                v8_helper(), id));

  task_environment_.RunUntilIdle();
}

TEST_F(SellerWorkletTest, BasicV8Debug) {
  ScopedInspectorSupport inspector_support(v8_helper().get());

  // Helper for looking for scriptParsed events.
  auto is_script_parsed = [](const TestChannel::Event& event) -> bool {
    if (event.type != TestChannel::Event::Type::Notification) {
      return false;
    }

    const std::string* candidate_method =
        event.value.GetDict().FindString("method");
    return (candidate_method && *candidate_method == "Debugger.scriptParsed");
  };

  const GURL kUrl1 = GURL("http://example.test/first.js");
  const GURL kUrl2 = GURL("http://example2.test/second.js");

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
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_reject_reason=*/
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_real_time_contributions=*/{},
      /*expected_bid_in_seller_currency=*/std::nullopt,
      /*expected_score_ad_timeout=*/false,
      /*expected_signals_fetch_latency=*/std::nullopt,
      /*expected_code_ready_latency=*/std::nullopt, run_loop1.QuitClosure());

  decision_logic_url_ = kUrl2;
  SellerWorklet* worklet_impl2 = nullptr;
  auto worklet2 = CreateWorklet(
      /*pause_for_debugger_on_start=*/true, &worklet_impl2);
  base::RunLoop run_loop2;
  RunScoreAdOnWorkletAsync(
      worklet2.get(), /*expected_score=*/2,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_reject_reason=*/
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_real_time_contributions=*/{},
      /*expected_bid_in_seller_currency=*/std::nullopt,
      /*expected_score_ad_timeout=*/false,
      /*expected_signals_fetch_latency=*/std::nullopt,
      /*expected_code_ready_latency=*/std::nullopt, run_loop2.QuitClosure());

  int id1 = worklet_impl1->context_group_ids_for_testing()[0];
  int id2 = worklet_impl2->context_group_ids_for_testing()[0];

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

TEST_F(SellerWorkletTwoThreadsTest, BasicV8Debug) {
  ScopedInspectorSupport inspector_support0(v8_helpers_[0].get());
  ScopedInspectorSupport inspector_support1(v8_helpers_[1].get());

  // Helper for looking for scriptParsed events.
  auto is_script_parsed = [](const TestChannel::Event& event) -> bool {
    if (event.type != TestChannel::Event::Type::Notification) {
      return false;
    }

    const std::string* candidate_method =
        event.value.GetDict().FindString("method");
    return (candidate_method && *candidate_method == "Debugger.scriptParsed");
  };

  const GURL kUrl1 = GURL("http://example.test/first.js");

  AddJavascriptResponse(&url_loader_factory_, kUrl1, CreateScoreAdScript("1"));

  SellerWorklet* worklet_impl = nullptr;
  decision_logic_url_ = kUrl1;
  auto worklet = CreateWorklet(
      /*pause_for_debugger_on_start=*/true, &worklet_impl);
  base::RunLoop run_loop;
  RunScoreAdOnWorkletAsync(
      worklet.get(), /*expected_score=*/1,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_reject_reason=*/
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_real_time_contributions=*/{},
      /*expected_bid_in_seller_currency=*/std::nullopt,
      /*expected_score_ad_timeout=*/false,
      /*expected_signals_fetch_latency=*/std::nullopt,
      /*expected_code_ready_latency=*/std::nullopt, run_loop.QuitClosure());

  std::vector<int> ids = worklet_impl->context_group_ids_for_testing();
  ASSERT_EQ(ids.size(), 2u);

  TestChannel* channel0 = inspector_support0.ConnectDebuggerSession(ids[0]);
  TestChannel* channel1 = inspector_support1.ConnectDebuggerSession(ids[1]);

  channel0->RunCommandAndWaitForResult(
      1, "Runtime.enable", R"({"id":1,"method":"Runtime.enable","params":{}})");
  channel0->RunCommandAndWaitForResult(
      2, "Debugger.enable",
      R"({"id":2,"method":"Debugger.enable","params":{}})");

  channel1->RunCommandAndWaitForResult(
      1, "Runtime.enable", R"({"id":1,"method":"Runtime.enable","params":{}})");
  channel1->RunCommandAndWaitForResult(
      2, "Debugger.enable",
      R"({"id":2,"method":"Debugger.enable","params":{}})");

  // Should not see scriptParsed before resume.
  std::list<TestChannel::Event> events0 = channel0->TakeAllEvents();
  EXPECT_TRUE(base::ranges::none_of(events0, is_script_parsed));
  std::list<TestChannel::Event> events1 = channel1->TakeAllEvents();
  EXPECT_TRUE(base::ranges::none_of(events1, is_script_parsed));

  // Unpause execution for `channel0`. Expect that `run_loop` hasn't quit, as
  // the worklet is waiting on both V8 threads to resume.
  EXPECT_FALSE(run_loop.AnyQuitCalled());
  channel0->RunCommandAndWaitForResult(
      3, "Runtime.runIfWaitingForDebugger",
      R"({"id":3,"method":"Runtime.runIfWaitingForDebugger","params":{}})");
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(run_loop.AnyQuitCalled());

  // Unpause execution for `channel1`. Expect that `run_loop` has quit.
  channel1->RunCommandAndWaitForResult(
      3, "Runtime.runIfWaitingForDebugger",
      R"({"id":3,"method":"Runtime.runIfWaitingForDebugger","params":{}})");
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(run_loop.AnyQuitCalled());

  // `channel0` should have had a parsed notification for kUrl1, as the
  // ScoreAd is executed on the corresponding thread.
  events1 = channel1->TakeAllEvents();
  EXPECT_TRUE(base::ranges::none_of(events1, is_script_parsed));

  TestChannel::Event script_parsed0 =
      channel0->WaitForMethodNotification("Debugger.scriptParsed");
  const std::string* url =
      script_parsed0.value.GetDict().FindStringByDottedPath("params.url");
  ASSERT_TRUE(url);
  EXPECT_EQ(kUrl1.spec(), *url);

  worklet.reset();
  task_environment_.RunUntilIdle();

  // No other scriptParsed events should be on any channel.
  events0 = channel0->TakeAllEvents();
  events1 = channel1->TakeAllEvents();
  EXPECT_TRUE(base::ranges::none_of(events0, is_script_parsed));
  EXPECT_TRUE(base::ranges::none_of(events1, is_script_parsed));
}

TEST_F(SellerWorkletTest, ParseErrorV8Debug) {
  ScopedInspectorSupport inspector_support(v8_helper().get());
  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        "Invalid Javascript");
  SellerWorklet* worklet_impl = nullptr;
  auto worklet =
      CreateWorklet(/*pause_for_debugger_on_start=*/true, &worklet_impl);
  int id = worklet_impl->context_group_ids_for_testing()[0];
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

TEST_F(SellerWorkletTwoThreadsTest, ParseErrorV8Debug) {
  ScopedInspectorSupport inspector_support0(v8_helpers_[0].get());
  ScopedInspectorSupport inspector_support1(v8_helpers_[1].get());

  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        "Invalid Javascript");
  SellerWorklet* worklet_impl = nullptr;
  auto worklet =
      CreateWorklet(/*pause_for_debugger_on_start=*/true, &worklet_impl);

  std::vector<int> ids = worklet_impl->context_group_ids_for_testing();
  EXPECT_EQ(ids.size(), 2u);

  TestChannel* channel0 = inspector_support0.ConnectDebuggerSession(ids[0]);
  TestChannel* channel1 = inspector_support1.ConnectDebuggerSession(ids[1]);

  channel0->RunCommandAndWaitForResult(
      1, "Runtime.enable", R"({"id":1,"method":"Runtime.enable","params":{}})");
  channel0->RunCommandAndWaitForResult(
      2, "Debugger.enable",
      R"({"id":2,"method":"Debugger.enable","params":{}})");

  channel1->RunCommandAndWaitForResult(
      1, "Runtime.enable", R"({"id":1,"method":"Runtime.enable","params":{}})");
  channel1->RunCommandAndWaitForResult(
      2, "Debugger.enable",
      R"({"id":2,"method":"Debugger.enable","params":{}})");

  // Unpause execution and wait for the pipe to be closed with an error.
  channel0->RunCommandAndWaitForResult(
      3, "Runtime.runIfWaitingForDebugger",
      R"({"id":3,"method":"Runtime.runIfWaitingForDebugger","params":{}})");
  channel1->RunCommandAndWaitForResult(
      3, "Runtime.runIfWaitingForDebugger",
      R"({"id":3,"method":"Runtime.runIfWaitingForDebugger","params":{}})");
  EXPECT_FALSE(WaitForDisconnect().empty());

  // Should have gotten a parse error notification for each channel.
  TestChannel::Event parse_error0 =
      channel0->WaitForMethodNotification("Debugger.scriptFailedToParse");
  const std::string* error_url0 =
      parse_error0.value.GetDict().FindStringByDottedPath("params.url");
  ASSERT_TRUE(error_url0);
  EXPECT_EQ(decision_logic_url_.spec(), *error_url0);

  TestChannel::Event parse_error1 =
      channel1->WaitForMethodNotification("Debugger.scriptFailedToParse");
  const std::string* error_url1 =
      parse_error1.value.GetDict().FindStringByDottedPath("params.url");
  ASSERT_TRUE(error_url1);
  EXPECT_EQ(decision_logic_url_.spec(), *error_url1);
}

TEST_F(SellerWorkletTest, BasicDevToolsDebug) {
  const char kScriptResult[] = "this.global_score ? this.global_score : 10";

  const char kUrl1[] = "http://example.test/first.js";
  const char kUrl2[] = "http://example2.test/second.js";

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
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_reject_reason=*/
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_real_time_contributions=*/{},
      /*expected_bid_in_seller_currency=*/std::nullopt,
      /*expected_score_ad_timeout=*/false,
      /*expected_signals_fetch_latency=*/std::nullopt,
      /*expected_code_ready_latency=*/std::nullopt, run_loop1.QuitClosure());

  decision_logic_url_ = GURL(kUrl2);
  auto worklet2 = CreateWorklet(/*pause_for_debugger_on_start=*/true);
  base::RunLoop run_loop2;
  RunScoreAdOnWorkletAsync(
      worklet2.get(), /*expected_score=*/0,
      {"http://example2.test/second.js scoreAd() return: Value passed as "
       "dictionary is neither object, null, nor undefined."},
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_reject_reason=*/
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_real_time_contributions=*/{},
      /*expected_bid_in_seller_currency=*/std::nullopt,
      /*expected_score_ad_timeout=*/false,
      /*expected_signals_fetch_latency=*/std::nullopt,
      /*expected_code_ready_latency=*/std::nullopt, run_loop2.QuitClosure());

  mojo::AssociatedRemote<blink::mojom::DevToolsAgent> agent1, agent2;
  worklet1->ConnectDevToolsAgent(agent1.BindNewEndpointAndPassReceiver(),
                                 /*thread_index=*/0);
  worklet2->ConnectDevToolsAgent(agent2.BindNewEndpointAndPassReceiver(),
                                 /*thread_index=*/0);

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
  EXPECT_EQ("1:2:0:http://example.test/first.js",
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

TEST_F(SellerWorkletTwoThreadsTest, BasicDevToolsDebug) {
  const char kScriptResult[] = "this.global_score ? this.global_score : 10";

  const char kUrl1[] = "http://example.test/first.js";

  AddJavascriptResponse(&url_loader_factory_, GURL(kUrl1),
                        CreateScoreAdScript(kScriptResult));

  decision_logic_url_ = GURL(kUrl1);
  auto worklet = CreateWorklet(/*pause_for_debugger_on_start=*/true);
  base::RunLoop run_loop0;
  RunScoreAdOnWorkletAsync(
      worklet.get(), /*expected_score=*/100.5,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_reject_reason=*/
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_real_time_contributions=*/{},
      /*expected_bid_in_seller_currency=*/std::nullopt,
      /*expected_score_ad_timeout=*/false,
      /*expected_signals_fetch_latency=*/std::nullopt,
      /*expected_code_ready_latency=*/std::nullopt, run_loop0.QuitClosure());

  base::RunLoop run_loop1;
  RunScoreAdOnWorkletAsync(
      worklet.get(), /*expected_score=*/100.6,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_reject_reason=*/
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_real_time_contributions=*/{},
      /*expected_bid_in_seller_currency=*/std::nullopt,
      /*expected_score_ad_timeout=*/false,
      /*expected_signals_fetch_latency=*/std::nullopt,
      /*expected_code_ready_latency=*/std::nullopt, run_loop1.QuitClosure());

  mojo::AssociatedRemote<blink::mojom::DevToolsAgent> agent0, agent1;
  worklet->ConnectDevToolsAgent(agent0.BindNewEndpointAndPassReceiver(),
                                /*thread_index=*/0);
  worklet->ConnectDevToolsAgent(agent1.BindNewEndpointAndPassReceiver(),
                                /*thread_index=*/1);

  TestDevToolsAgentClient debug0(std::move(agent0), "123",
                                 /*use_binary_protocol=*/true);
  TestDevToolsAgentClient debug1(std::move(agent1), "456",
                                 /*use_binary_protocol=*/true);

  debug0.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 1, "Runtime.enable",
      R"({"id":1,"method":"Runtime.enable","params":{}})");
  debug0.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 2, "Debugger.enable",
      R"({"id":2,"method":"Debugger.enable","params":{}})");

  debug1.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 1, "Runtime.enable",
      R"({"id":1,"method":"Runtime.enable","params":{}})");
  debug1.RunCommandAndWaitForResult(
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

  debug0.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 3, "Debugger.setBreakpointByUrl",
      base::StringPrintf(kBreakpointTemplate, kUrl1));
  debug1.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 3, "Debugger.setBreakpointByUrl",
      base::StringPrintf(kBreakpointTemplate, kUrl1));

  // Now resume. We should see a scriptParsed event for `debug0` and `debug1`,
  // as the two ScoreAds are executed on the corresponding thread respectively.
  debug0.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 4,
      "Runtime.runIfWaitingForDebugger",
      R"({"id":4,"method":"Runtime.runIfWaitingForDebugger","params":{}})");
  debug1.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 4,
      "Runtime.runIfWaitingForDebugger",
      R"({"id":4,"method":"Runtime.runIfWaitingForDebugger","params":{}})");

  TestDevToolsAgentClient::Event script_parsed0 =
      debug0.WaitForMethodNotification("Debugger.scriptParsed");
  const std::string* url0 =
      script_parsed0.value.GetDict().FindStringByDottedPath("params.url");
  ASSERT_TRUE(url0);
  EXPECT_EQ(*url0, kUrl1);

  TestDevToolsAgentClient::Event script_parsed1 =
      debug1.WaitForMethodNotification("Debugger.scriptParsed");
  const std::string* url1 =
      script_parsed1.value.GetDict().FindStringByDottedPath("params.url");
  ASSERT_TRUE(url1);
  EXPECT_EQ(*url1, kUrl1);

  // The breakpoints will be hit.
  TestDevToolsAgentClient::Event breakpoint_hit0 =
      debug0.WaitForMethodNotification("Debugger.paused");
  TestDevToolsAgentClient::Event breakpoint_hit1 =
      debug1.WaitForMethodNotification("Debugger.paused");

  base::Value::List* hit_breakpoints0 =
      breakpoint_hit0.value.GetDict().FindDict("params")->FindList(
          "hitBreakpoints");
  ASSERT_TRUE(hit_breakpoints0);
  ASSERT_EQ(1u, hit_breakpoints0->size());
  ASSERT_TRUE((*hit_breakpoints0)[0].is_string());
  EXPECT_EQ("1:2:0:http://example.test/first.js",
            (*hit_breakpoints0)[0].GetString());
  std::string* callframe_id0 = breakpoint_hit0.value.GetDict()
                                   .FindDict("params")
                                   ->FindList("callFrames")
                                   ->front()
                                   .GetDict()
                                   .FindString("callFrameId");

  base::Value::List* hit_breakpoints1 =
      breakpoint_hit1.value.GetDict().FindDict("params")->FindList(
          "hitBreakpoints");
  ASSERT_TRUE(hit_breakpoints1);
  ASSERT_EQ(1u, hit_breakpoints1->size());
  ASSERT_TRUE((*hit_breakpoints1)[0].is_string());
  EXPECT_EQ("1:2:0:http://example.test/first.js",
            (*hit_breakpoints1)[0].GetString());
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

  // Note that ScoreAds are processed in the opposite order they were queued.
  // Thus `debug0` should correspond to the first `RunScoreAdOnWorkletAsync`
  // call, and `debug1` should correspond to the second call.
  debug0.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kIO, 5, "Debugger.evaluateOnCallFrame",
      base::StringPrintf(kCommandTemplate, callframe_id0->c_str(), "100.6"));
  debug1.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kIO, 5, "Debugger.evaluateOnCallFrame",
      base::StringPrintf(kCommandTemplate, callframe_id1->c_str(), "100.5"));

  // Let the thread associated with `debug0` resume.
  EXPECT_FALSE(run_loop1.AnyQuitCalled());
  debug0.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kIO, 6, "Debugger.resume",
      R"({"id":6,"method":"Debugger.resume","params":{}})");
  run_loop1.Run();

  // Let the thread associated with `debug1` resume.
  EXPECT_FALSE(run_loop0.AnyQuitCalled());
  debug1.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kIO, 6, "Debugger.resume",
      R"({"id":6,"method":"Debugger.resume","params":{}})");
  run_loop0.Run();
}

TEST_F(SellerWorkletTest, InstrumentationBreakpoints) {
  const char kUrl[] = "http://example.test/script.js";

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
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_reject_reason=*/
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_real_time_contributions=*/{},
      /*expected_bid_in_seller_currency=*/std::nullopt,
      /*expected_score_ad_timeout=*/false,
      /*expected_signals_fetch_latency=*/std::nullopt,
      /*expected_code_ready_latency=*/std::nullopt, run_loop.QuitClosure());

  mojo::AssociatedRemote<blink::mojom::DevToolsAgent> agent;
  worklet->ConnectDevToolsAgent(agent.BindNewEndpointAndPassReceiver(),
                                /*thread_index=*/0);

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
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_reject_reason=*/
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_real_time_contributions=*/{},
      /*expected_bid_in_seller_currency=*/std::nullopt,
      /*expected_score_ad_timeout=*/false,
      /*expected_signals_fetch_latency=*/std::nullopt,
      /*expected_code_ready_latency=*/std::nullopt, run_loop3.QuitClosure());

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
  const char kUrl[] = "http://example.test/script.js";

  std::string script_body =
      CreateBasicSellAdScript() +
      CreateReportToScript("1", R"(sendReportTo("https://foo.test"))");
  AddJavascriptResponse(&url_loader_factory_, GURL(kUrl), script_body);

  decision_logic_url_ = GURL(kUrl);
  auto worklet = CreateWorklet(/*pause_for_debugger_on_start=*/true);
  RunScoreAdOnWorkletExpectingCallbackNeverInvoked(worklet.get());

  mojo::AssociatedRemote<blink::mojom::DevToolsAgent> agent;
  worklet->ConnectDevToolsAgent(agent.BindNewEndpointAndPassReceiver(),
                                /*thread_index=*/0);

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
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_reject_reason=*/
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_real_time_contributions=*/{},
      /*expected_bid_in_seller_currency=*/std::nullopt,
      /*expected_score_ad_timeout=*/false,
      /*expected_signals_fetch_latency=*/std::nullopt,
      /*expected_code_ready_latency=*/std::nullopt, base::BindOnce([]() {
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
  base::WaitableEvent* event_handle = WedgeV8Thread(v8_helper().get());

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
      browser_signal_selected_buyer_and_seller_reporting_id_,
      browser_signal_buyer_and_seller_reporting_id_,
      browser_signal_ad_components_, browser_signal_bidding_duration_msecs_,
      browser_signal_render_size_,
      browser_signal_for_debugging_only_in_cooldown_or_lockout_,
      seller_timeout_,
      /*trace_id=*/1, bidder_joining_origin_,
      client_receiver.BindNewPipeAndPassRemote());

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
  v8_helper()->v8_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<AuctionV8Helper> v8_helper) {
            v8_helper->set_script_timeout_for_testing(base::Days(360));
          },
          v8_helper()));

  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        "while(true) {}");
  mojo::Remote<mojom::SellerWorklet> seller_worklet = CreateWorklet();
  // Let the script load.
  task_environment_.RunUntilIdle();

  // Now we no longer need it for parsing JS, wedge the V8 thread so we get a
  // chance to cancel the script *before* it actually tries running.
  base::WaitableEvent* event_handle = WedgeV8Thread(v8_helper().get());

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
      browser_signal_selected_buyer_and_seller_reporting_id_,
      browser_signal_buyer_and_seller_reporting_id_,
      browser_signal_ad_components_, browser_signal_bidding_duration_msecs_,
      browser_signal_render_size_,
      browser_signal_for_debugging_only_in_cooldown_or_lockout_,
      seller_timeout_,
      /*trace_id=*/1, bidder_joining_origin_,
      client_receiver.BindNewPipeAndPassRemote());
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
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt);

  RunScoreAdWithJavascriptExpectingResult(
      CreateScoreAdScript(
          "1", R"(forDebuggingOnly.reportAdAuctionWin("https://win.url"))"),
      1, /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt);
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
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt);
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
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt);
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
      /*expected_report_url=*/std::nullopt);
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
      /*expected_report_url=*/std::nullopt);
}

TEST_F(SellerWorkletTest,
       ScoreAdBrowserSignalForDebuggingOnlyInCooldownOrLockout) {
  RunScoreAdWithReturnValueExpectingResult(
      R"(browserSignals.forDebuggingOnlyInCooldownOrLockout === false ? 3 : 0)",
      3);

  browser_signal_for_debugging_only_in_cooldown_or_lockout_ = true;
  RunScoreAdWithReturnValueExpectingResult(
      R"(browserSignals.forDebuggingOnlyInCooldownOrLockout === true ? 3 : 0)",
      3);
}

class ScoreAdBrowserSignalRenderSizeTest
    : public base::test::WithFeatureOverride,
      public SellerWorkletTest {
 public:
  ScoreAdBrowserSignalRenderSizeTest()
      : base::test::WithFeatureOverride(
            blink::features::kRenderSizeInScoreAdBrowserSignals) {}
};

TEST_P(ScoreAdBrowserSignalRenderSizeTest, ScoreAdBrowserSignalRenderSize) {
  RunScoreAdWithReturnValueExpectingResult(
      R"(browserSignals.hasOwnProperty('renderSize') ? 3 : 0)", 0);

  browser_signal_render_size_ =
      blink::AdSize(100, blink::AdSize::LengthUnit::kScreenWidth, 50,
                    blink::AdSize::LengthUnit::kPixels);
  RunScoreAdWithReturnValueExpectingResult(
      R"((browserSignals.hasOwnProperty('renderSize') &&
          browserSignals.renderSize.width === '100sw' &&
          browserSignals.renderSize.height === '50px') ? 3 : 0)",
      IsParamFeatureEnabled() ? 3 : 0);
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(ScoreAdBrowserSignalRenderSizeTest);

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
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_reject_reason=*/mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{});

  RunReportResultCreatedScriptExpectingResult(
      R"(5)",
      R"(
        sharedStorage.clear();
      )",
      /*expected_signals_for_winner=*/std::nullopt,
      /*expected_report_url=*/std::nullopt, /*expected_ad_beacon_map=*/{},
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
    shared_storage_hosts_[0] = receiver.BindNewPipeAndPassRemote();

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
        /*expected_data_version=*/std::nullopt,
        /*expected_debug_loss_report_url=*/std::nullopt,
        /*expected_debug_win_report_url=*/std::nullopt,
        /*expected_reject_reason=*/mojom::RejectReason::kNotAvailable,
        /*expected_pa_requests=*/{});

    // Make sure the shared storage mojom methods are invoked as they use a
    // dedicated pipe.
    task_environment_.RunUntilIdle();

    using RequestType =
        auction_worklet::TestAuctionSharedStorageHost::RequestType;
    using Request = auction_worklet::TestAuctionSharedStorageHost::Request;

    EXPECT_THAT(
        test_shared_storage_host.observed_requests(),
        testing::ElementsAre(
            Request{.type = RequestType::kSet,
                    .key = u"a",
                    .value = u"b",
                    .ignore_if_present = false,
                    .source_auction_worklet_function =
                        mojom::AuctionWorkletFunction::kSellerScoreAd},
            Request{.type = RequestType::kSet,
                    .key = u"a",
                    .value = u"b",
                    .ignore_if_present = true,
                    .source_auction_worklet_function =
                        mojom::AuctionWorkletFunction::kSellerScoreAd},
            Request{.type = RequestType::kAppend,
                    .key = u"a",
                    .value = u"b",
                    .ignore_if_present = false,
                    .source_auction_worklet_function =
                        mojom::AuctionWorkletFunction::kSellerScoreAd},
            Request{.type = RequestType::kDelete,
                    .key = u"a",
                    .value = std::u16string(),
                    .ignore_if_present = false,
                    .source_auction_worklet_function =
                        mojom::AuctionWorkletFunction::kSellerScoreAd},
            Request{.type = RequestType::kClear,
                    .key = std::u16string(),
                    .value = std::u16string(),
                    .ignore_if_present = false,
                    .source_auction_worklet_function =
                        mojom::AuctionWorkletFunction::kSellerScoreAd}));
  }

  {
    shared_storage_hosts_[0] =
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
        /*expected_data_version=*/std::nullopt,
        /*expected_debug_loss_report_url=*/std::nullopt,
        /*expected_debug_win_report_url=*/std::nullopt,
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
    shared_storage_hosts_[0] = receiver.BindNewPipeAndPassRemote();

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
        /*expected_report_url=*/std::nullopt, /*expected_ad_beacon_map=*/{},
        /*expected_pa_requests=*/{},
        /*expected_errors=*/{});

    // Make sure the shared storage mojom methods are invoked as they use a
    // dedicated pipe.
    task_environment_.RunUntilIdle();

    using RequestType =
        auction_worklet::TestAuctionSharedStorageHost::RequestType;
    using Request = auction_worklet::TestAuctionSharedStorageHost::Request;

    EXPECT_THAT(
        test_shared_storage_host.observed_requests(),
        testing::ElementsAre(
            Request{.type = RequestType::kSet,
                    .key = u"a",
                    .value = u"b",
                    .ignore_if_present = false,
                    .source_auction_worklet_function =
                        mojom::AuctionWorkletFunction::kSellerReportResult},
            Request{.type = RequestType::kSet,
                    .key = u"a",
                    .value = u"b",
                    .ignore_if_present = true,
                    .source_auction_worklet_function =
                        mojom::AuctionWorkletFunction::kSellerReportResult},
            Request{.type = RequestType::kAppend,
                    .key = u"a",
                    .value = u"b",
                    .ignore_if_present = false,
                    .source_auction_worklet_function =
                        mojom::AuctionWorkletFunction::kSellerReportResult},
            Request{.type = RequestType::kDelete,
                    .key = u"a",
                    .value = std::u16string(),
                    .ignore_if_present = false,
                    .source_auction_worklet_function =
                        mojom::AuctionWorkletFunction::kSellerReportResult},
            Request{.type = RequestType::kClear,
                    .key = std::u16string(),
                    .value = std::u16string(),
                    .ignore_if_present = false,
                    .source_auction_worklet_function =
                        mojom::AuctionWorkletFunction::kSellerReportResult}));
  }

  {
    shared_storage_hosts_[0] =
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
        /*expected_signals_for_winner=*/std::nullopt,
        /*expected_report_url=*/std::nullopt, /*expected_ad_beacon_map=*/{},
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

class SellerWorkletKVv2Test : public SellerWorkletTest {
 public:
  SellerWorkletKVv2Test() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kFledgeTrustedSignalsKVv2Support}, {});

    public_key_ = mojom::TrustedSignalsPublicKey::New(
        std::string(reinterpret_cast<const char*>(&kTestPublicKey[0]),
                    sizeof(kTestPublicKey)),
        kKeyId);
  }

 protected:
  static std::string GenerateResponseBody(
      const std::string& request_body,
      const std::string& response_json_content) {
    // Decrypt the request.
    auto response_key_config = quiche::ObliviousHttpHeaderKeyConfig::Create(
        kKeyId, EVP_HPKE_DHKEM_X25519_HKDF_SHA256, EVP_HPKE_HKDF_SHA256,
        EVP_HPKE_AES_256_GCM);
    CHECK(response_key_config.ok()) << response_key_config.status();

    auto ohttp_gateway =
        quiche::ObliviousHttpGateway::Create(
            std::string(reinterpret_cast<const char*>(&kTestPrivateKey[0]),
                        sizeof(kTestPrivateKey)),
            response_key_config.value())
            .value();

    auto received_request = ohttp_gateway.DecryptObliviousHttpRequest(
        request_body, kTrustedSignalsKVv2EncryptionRequestMediaType);
    CHECK(received_request.ok()) << received_request.status();

    // Build response body.
    cbor::Value::MapValue compression_group;
    compression_group.try_emplace(cbor::Value("compressionGroupId"),
                                  cbor::Value(0));
    compression_group.try_emplace(cbor::Value("ttlMs"), cbor::Value(100));
    compression_group.try_emplace(
        cbor::Value("content"),
        cbor::Value(test::ToCborVector(response_json_content)));

    cbor::Value::ArrayValue compression_groups;
    compression_groups.emplace_back(std::move(compression_group));

    cbor::Value::MapValue body_map;
    body_map.try_emplace(cbor::Value("compressionGroups"),
                         cbor::Value(std::move(compression_groups)));

    cbor::Value body_value(std::move(body_map));
    std::optional<std::vector<uint8_t>> maybe_body_bytes =
        cbor::Writer::Write(body_value);
    CHECK(maybe_body_bytes);

    std::string response_body = test::CreateKVv2ResponseBody(
        base::as_string_view(maybe_body_bytes.value()));
    auto response_context =
        std::move(received_request).value().ReleaseContext();

    // Encrypt the response body.
    auto maybe_response = ohttp_gateway.CreateObliviousHttpResponse(
        response_body, response_context,
        kTrustedSignalsKVv2EncryptionResponseMediaType);
    CHECK(maybe_response.ok()) << maybe_response.status();

    return maybe_response->EncapsulateAndSerialize();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SellerWorkletKVv2Test, ScoreAdTrustedScoringSignals) {
  const char kJson[] =
      R"([{
          "id": 0,
          "dataVersion": 101,
          "keyGroupOutputs": [
            {
              "tags": [
                "renderUrls"
              ],
              "keyValues": {
                "https://bar.test/": {
                  "value": "1"
                }
              }
            },
            {
              "tags": [
                "adComponentRenderUrls"
              ],
              "keyValues": {
                "https://barsub.test/": {
                  "value": "2"
                },
                "https://foosub.test/": {
                  "value": "[3]"
                }
              }
            }
          ]
        }])";

  const char kValidate[] = R"(
    const expected = '{"renderURL":{"https://bar.test/":1},' +
    '"renderUrl":{"https://bar.test/":1},"adComponentRenderURLs":' +
    '{"https://barsub.test/":2,"https://foosub.test/":[3]},' +
    '"adComponentRenderUrls":{"https://barsub.test/":2,' +
    '"https://foosub.test/":[3]}}'
    const actual = JSON.stringify(trustedScoringSignals);
    if (actual === expected)
      return 2;
    throw actual + "!" + expected;
  )";

  const base::TimeDelta kDelay = base::Milliseconds(135);
  trusted_scoring_signals_url_ =
      GURL("https://url.test/trusted_scoring_signals");
  browser_signal_render_url_ = GURL("https://bar.test/");
  browser_signal_ad_components_ = {GURL("https://barsub.test/"),
                                   GURL("https://foosub.test/")};

  AddJavascriptResponse(
      &url_loader_factory_, decision_logic_url_,
      CreateScoreAdScript(/*raw_return_value=*/"1", kValidate));
  auto seller_worklet = CreateWorklet();
  ASSERT_TRUE(seller_worklet);
  base::RunLoop run_loop;
  RunScoreAdOnWorkletAsync(
      seller_worklet.get(), /*expected_score=*/2,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/101,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_real_time_contributions=*/{},
      /*expected_bid_in_seller_currency=*/std::nullopt,
      /*expected_score_ad_timeout=*/false,
      /*expected_signals_fetch_latency=*/kDelay,
      /*expected_code_ready_latency=*/std::nullopt, run_loop.QuitClosure());
  task_environment_.FastForwardBy(kDelay);

  // Decrypt request and encrypt response.
  ASSERT_EQ(1, url_loader_factory_.NumPending());
  const network::ResourceRequest* pending_request;
  ASSERT_TRUE(url_loader_factory_.IsPending(
      trusted_scoring_signals_url_->spec(), &pending_request));

  std::string request_body =
      std::string(pending_request->request_body->elements()
                      ->at(0)
                      .As<network::DataElementBytes>()
                      .AsStringPiece());
  std::string response_body = GenerateResponseBody(request_body, kJson);
  std::string headers =
      base::StringPrintf("%s\nContent-Type: %s", kAllowFledgeHeader,
                         "message/ad-auction-trusted-signals-request");
  AddResponse(&url_loader_factory_, trusted_scoring_signals_url_.value(),
              kAdAuctionTrustedSignalsMimeType,
              /*charset=*/std::nullopt, response_body, headers);

  run_loop.Run();
}

class SellerWorkletTwoThreadsSharedStorageAPIEnabledTest
    : public SellerWorkletSharedStorageAPIEnabledTest {
 public:
  size_t NumThreads() override { return 2u; }
};

TEST_F(SellerWorkletTwoThreadsSharedStorageAPIEnabledTest,
       SharedStorageWriteInScoreAd) {
  auction_worklet::TestAuctionSharedStorageHost test_shared_storage_host0;
  auction_worklet::TestAuctionSharedStorageHost test_shared_storage_host1;

  mojo::Receiver<auction_worklet::mojom::AuctionSharedStorageHost> receiver0(
      &test_shared_storage_host0);
  shared_storage_hosts_[0] = receiver0.BindNewPipeAndPassRemote();

  mojo::Receiver<auction_worklet::mojom::AuctionSharedStorageHost> receiver1(
      &test_shared_storage_host0);
  shared_storage_hosts_[1] = receiver1.BindNewPipeAndPassRemote();

  AddJavascriptResponse(
      &url_loader_factory_, decision_logic_url_,
      /*javascript=*/CreateScoreAdScript("5", /*extra_code=*/R"(
        sharedStorage.set('a', 'b');
      )"));

  auto seller_worklet = CreateWorklet();

  RunScoreAdExpectingResultOnWorklet(seller_worklet.get(), 5);

  // Make sure the shared storage mojom methods are invoked as they use a
  // dedicated pipe.
  task_environment_.RunUntilIdle();

  // Expect that only the shared storage host corresponding to the thread
  // handling the ScoreAd has observed the requests.
  EXPECT_TRUE(test_shared_storage_host1.observed_requests().empty());

  using RequestType =
      auction_worklet::TestAuctionSharedStorageHost::RequestType;
  using Request = auction_worklet::TestAuctionSharedStorageHost::Request;

  EXPECT_THAT(test_shared_storage_host0.observed_requests(),
              testing::ElementsAre(
                  Request{.type = RequestType::kSet,
                          .key = u"a",
                          .value = u"b",
                          .ignore_if_present = false,
                          .source_auction_worklet_function =
                              mojom::AuctionWorkletFunction::kSellerScoreAd}));
}

class SellerWorkletRealTimeTest : public SellerWorkletTest {
 public:
  SellerWorkletRealTimeTest()
      : SellerWorkletTest(
            base::test::TaskEnvironment::TimeSource::SYSTEM_TIME) {}
};

// `scoreAd` should time out due to AuctionV8Helper's default script timeout (50
// ms).
TEST_F(SellerWorkletRealTimeTest, ScoreAdDefaultTimeout) {
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
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_real_time_contributions=*/{},
      /*expected_bid_in_seller_currency=*/std::nullopt,
      /*expected_score_ad_timeout=*/true,
      /*expected_signals_fetch_latency=*/std::nullopt,
      /*expected_code_ready_latency=*/std::nullopt, run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(SellerWorkletRealTimeTest, ScoreAdTopLevelTimeout) {
  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        "while (1) {}");
  auto seller_worklet = CreateWorklet();
  ASSERT_TRUE(seller_worklet);
  base::RunLoop run_loop;
  RunScoreAdOnWorkletAsync(seller_worklet.get(), /*expected_score=*/0,
                           /*expected_errors=*/
                           {"https://url.test/ top-level execution timed out."},
                           mojom::ComponentAuctionModifiedBidParamsPtr(),
                           /*expected_data_version=*/std::nullopt,
                           /*expected_debug_loss_report_url=*/std::nullopt,
                           /*expected_debug_win_report_url=*/std::nullopt,
                           mojom::RejectReason::kNotAvailable,
                           /*expected_pa_requests=*/{},
                           /*expected_real_time_contributions=*/{},
                           /*expected_bid_in_seller_currency=*/std::nullopt,
                           /*expected_score_ad_timeout=*/true,
                           /*expected_signals_fetch_latency=*/std::nullopt,
                           /*expected_code_ready_latency=*/std::nullopt,
                           run_loop.QuitClosure());
  run_loop.Run();
}

// Test that seller timeout zero results in no score produced.
TEST_F(SellerWorkletRealTimeTest, ScoreAdZeroTimeout) {
  seller_timeout_ = base::TimeDelta();

  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        CreateScoreAdScript("10"));

  auto seller_worklet = CreateWorklet();
  ASSERT_TRUE(seller_worklet);
  base::RunLoop run_loop;
  RunScoreAdOnWorkletAsync(seller_worklet.get(), /*expected_score=*/0,
                           /*expected_errors=*/
                           {"scoreAd() aborted due to zero timeout."},
                           mojom::ComponentAuctionModifiedBidParamsPtr(),
                           /*expected_data_version=*/std::nullopt,
                           /*expected_debug_loss_report_url=*/std::nullopt,
                           /*expected_debug_win_report_url=*/std::nullopt,
                           mojom::RejectReason::kNotAvailable,
                           /*expected_pa_requests=*/{},
                           /*expected_real_time_contributions=*/{},
                           /*expected_bid_in_seller_currency=*/std::nullopt,
                           /*expected_score_ad_timeout=*/true,
                           /*expected_signals_fetch_latency=*/std::nullopt,
                           /*expected_code_ready_latency=*/std::nullopt,
                           run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(SellerWorkletRealTimeTest, ScoreAdSellerTimeoutFromAuctionConfig) {
  // Use a very long default script timeout, and a short seller timeout, so
  // that if the seller script with endless loop times out, we know that the
  // seller timeout overwrote the default script timeout and worked.
  const base::TimeDelta kScriptTimeout = base::Days(360);
  v8_helper()->v8_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<AuctionV8Helper> v8_helper,
             const base::TimeDelta script_timeout) {
            v8_helper->set_script_timeout_for_testing(script_timeout);
          },
          v8_helper(), kScriptTimeout));
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
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_real_time_contributions=*/{},
      /*expected_bid_in_seller_currency=*/std::nullopt,
      /*expected_score_ad_timeout=*/true,
      /*expected_signals_fetch_latency=*/std::nullopt,
      /*expected_code_ready_latency=*/std::nullopt, run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(SellerWorkletRealTimeTest, ScoreAdJsonTimeout) {
  seller_timeout_ = base::Milliseconds(20);
  const char kReturnVal[] = R"({
    allowComponentAuction: true,
    desirability: 5,
    ad: {
      get field() { while(true) {} }
    }
  })";
  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        CreateScoreAdScript(kReturnVal));
  auto seller_worklet = CreateWorklet();
  ASSERT_TRUE(seller_worklet);
  base::RunLoop run_loop;
  browser_signals_other_seller_ =
      mojom::ComponentAuctionOtherSeller::NewTopLevelSeller(
          url::Origin::Create(GURL("https://top.seller.test")));
  RunScoreAdOnWorkletAsync(seller_worklet.get(), /*expected_score=*/0,
                           /*expected_errors=*/
                           {"https://url.test/ timeout serializing `ad` field "
                            "of scoreAd() return value."},
                           mojom::ComponentAuctionModifiedBidParamsPtr(),
                           /*expected_data_version=*/std::nullopt,
                           /*expected_debug_loss_report_url=*/std::nullopt,
                           /*expected_debug_win_report_url=*/std::nullopt,
                           mojom::RejectReason::kNotAvailable,
                           /*expected_pa_requests=*/{},
                           /*expected_real_time_contributions=*/{},
                           /*expected_bid_in_seller_currency=*/std::nullopt,
                           /*expected_score_ad_timeout=*/true,
                           /*expected_signals_fetch_latency=*/std::nullopt,
                           /*expected_code_ready_latency=*/std::nullopt,
                           run_loop.QuitClosure());
  run_loop.Run();
}

// Tests both reporting latency, and default reporting timeout.
TEST_F(SellerWorkletRealTimeTest, ReportResultLatency) {
  // We use an infinite loop since we have some notion of how long a timeout
  // should take.
  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        CreateReportToScript("1", "while (true) {}"));

  RunReportResultExpectingResult(
      /*expected_signals_for_winner=*/std::nullopt,
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      /*expected_reporting_latency_timeout=*/true,
      /*expected_errors=*/
      {"https://url.test/ execution of `reportResult` timed out."});
}

TEST_F(SellerWorkletRealTimeTest, ReportResultZeroTimeout) {
  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        CreateReportToScript("1", "throw 'something'"));

  auction_ad_config_non_shared_params_.reporting_timeout = base::TimeDelta();
  RunReportResultExpectingResult(
      /*expected_signals_for_winner=*/std::nullopt,
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      /*expected_reporting_latency_timeout=*/true,
      /*expected_errors=*/
      {"reportResult() aborted due to zero timeout."});
}

TEST_F(SellerWorkletRealTimeTest, ReportResultTimeoutFromAuctionConfig) {
  // Use a very long default script timeout, and a short reporting timeout, so
  // that if the reportResult() script with endless loop times out, we know that
  // the reporting timeout overwrote the default script timeout and worked.
  const base::TimeDelta kScriptTimeout = base::Days(360);
  v8_helper()->v8_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<AuctionV8Helper> v8_helper,
             const base::TimeDelta script_timeout) {
            v8_helper->set_script_timeout_for_testing(script_timeout);
          },
          v8_helper(), kScriptTimeout));
  // Make sure set_script_timeout_for_testing is called.
  task_environment_.RunUntilIdle();

  auction_ad_config_non_shared_params_.reporting_timeout =
      base::Milliseconds(50);
  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        CreateReportToScript("1", "while (true) {}"));
  RunReportResultExpectingResult(
      /*expected_signals_for_winner=*/std::nullopt,
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      /*expected_reporting_latency_timeout=*/true,
      /*expected_errors=*/
      {"https://url.test/ execution of `reportResult` timed out."});
}

TEST_F(SellerWorkletRealTimeTest, ReportResultJsonTimeout) {
  const char kReturnVal[] = R"({
    desirability: 5,
    ad: {
      get field() { while(true) {} }
    }
  })";
  auction_ad_config_non_shared_params_.reporting_timeout =
      base::Milliseconds(30);
  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        CreateReportToScript(kReturnVal));
  RunReportResultExpectingResult(
      /*expected_signals_for_winner=*/std::nullopt,
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      /*expected_reporting_latency_timeout=*/true,
      /*expected_errors=*/
      {"https://url.test/ timeout serializing reportResult() return value."});
}

TEST_F(SellerWorkletRealTimeTest, ReportResultTopLevelTimeout) {
  auction_ad_config_non_shared_params_.reporting_timeout =
      base::Milliseconds(30);
  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        "while (true) {}");
  RunReportResultExpectingResult(
      /*expected_signals_for_winner=*/std::nullopt,
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      /*expected_reporting_latency_timeout=*/true,
      /*expected_errors=*/
      {"https://url.test/ top-level execution timed out."});
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
      /*expected_data_version=*/std::nullopt, GURL("https://loss.url"),
      GURL("https://win.url"));

  // Should keep debug report URLs when score <= 0.
  RunScoreAdWithJavascriptExpectingResult(
      CreateScoreAdScript(
          "-1",
          R"(forDebuggingOnly.reportAdAuctionLoss("https://loss.url");
            forDebuggingOnly.reportAdAuctionWin("https://win.url"))"),
      0, /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/std::nullopt, GURL("https://loss.url"),
      GURL("https://win.url"));

  // It's OK to call one API but not the other.
  RunScoreAdWithJavascriptExpectingResult(
      CreateScoreAdScript(
          "1", R"(forDebuggingOnly.reportAdAuctionLoss("https://loss.url"))"),
      1, /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/std::nullopt, GURL("https://loss.url"));
  RunScoreAdWithJavascriptExpectingResult(
      CreateScoreAdScript(
          "1", R"(forDebuggingOnly.reportAdAuctionWin("https://win.url"))"),
      1, /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt, GURL("https://win.url"));

  // forDebuggingOnly binding errors are collected by seller worklets.
  RunScoreAdWithJavascriptExpectingResult(
      CreateScoreAdScript("1", R"(forDebuggingOnly.reportAdAuctionLoss(null))"),
      0,
      {"https://url.test/:4 Uncaught TypeError: "
       "reportAdAuctionLoss must be passed a valid HTTPS url."},
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt);

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
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt);
}

// Debugging loss/win report URLs should be nullopt if scoreAd() parameters are
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
      0, /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt);
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
  AddJavascriptResponse(
      &url_loader_factory_, decision_logic_url_,
      CreateScoreAdScript(
          /*raw_return_value=*/"",
          R"(forDebuggingOnly.reportAdAuctionLoss("https://loss.url1");
            while (1);
            forDebuggingOnly.reportAdAuctionLoss("https://loss.url2"))"));

  auto seller_worklet = CreateWorklet();
  ASSERT_TRUE(seller_worklet);
  base::RunLoop run_loop;
  RunScoreAdOnWorkletAsync(
      seller_worklet.get(), /*expected_score=*/0,
      /*expected_errors=*/
      {"https://url.test/ execution of `scoreAd` timed out."},
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/GURL("https://loss.url1"),
      /*expected_debug_win_report_url=*/std::nullopt,
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_real_time_contributions=*/{},
      /*expected_bid_in_seller_currency=*/std::nullopt,
      /*expected_score_ad_timeout=*/true,
      /*expected_signals_fetch_latency=*/std::nullopt,
      /*expected_code_ready_latency=*/std::nullopt, run_loop.QuitClosure());
  run_loop.Run();
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
        browser_signal_selected_buyer_and_seller_reporting_id_,
        browser_signal_buyer_and_seller_reporting_id_,
        browser_signal_ad_components_, browser_signal_bidding_duration_msecs_,
        browser_signal_render_size_,
        browser_signal_for_debugging_only_in_cooldown_or_lockout_,
        seller_timeout_,
        /*trace_id=*/1, bidder_joining_origin_,
        TestScoreAdClient::Create(base::BindLambdaForTesting(
            [&run_loop](double score, mojom::RejectReason reject_reason,
                        mojom::ComponentAuctionModifiedBidParamsPtr
                            component_auction_modified_bid_params,
                        std::optional<double> bid_in_seller_currency,
                        std::optional<uint32_t> scoring_signals_data_version,
                        const std::optional<GURL>& debug_loss_report_url,
                        const std::optional<GURL>& debug_win_report_url,
                        PrivateAggregationRequests pa_requests,
                        RealTimeReportingContributions real_time_contributions,
                        mojom::SellerTimingMetricsPtr score_ad_timing_metrics,
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
                EXPECT_EQ(std::nullopt, debug_loss_report_url);
                EXPECT_EQ(std::nullopt, debug_win_report_url);
              }
              run_loop.Quit();
            })));
    run_loop.Run();
  }
}

class SellerWorkletSampleDebugReportsDisabledTest : public SellerWorkletTest {
 public:
  SellerWorkletSampleDebugReportsDisabledTest() {
    feature_list_.InitAndDisableFeature(
        blink::features::kFledgeSampleDebugReports);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(SellerWorkletSampleDebugReportsDisabledTest,
       ScoreAdBrowserSignalForDebuggingOnlyInCooldownOrLockout) {
  RunScoreAdWithReturnValueExpectingResult(
      R"(browserSignals.hasOwnProperty('forDebuggingOnlyInCooldownOrLockout') ?
            3 : 0)",
      0);
}

class SellerWorkletPrivateAggregationEnabledTest : public SellerWorkletTest {
 public:
  SellerWorkletPrivateAggregationEnabledTest() {
    feature_list_.InitAndEnableFeature(blink::features::kPrivateAggregationApi);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(SellerWorkletPrivateAggregationEnabledTest, ScoreAd) {
  mojom::PrivateAggregationRequest kExpectedRequest1(
      mojom::AggregatableReportContribution::NewHistogramContribution(
          blink::mojom::AggregatableReportHistogramContribution::New(
              /*bucket=*/123,
              /*value=*/45,
              /*filtering_id=*/std::nullopt)),
      blink::mojom::AggregationServiceMode::kDefault,
      blink::mojom::DebugModeDetails::New());
  mojom::PrivateAggregationRequest kExpectedRequest2(
      mojom::AggregatableReportContribution::NewHistogramContribution(
          blink::mojom::AggregatableReportHistogramContribution::New(
              /*bucket=*/absl::MakeInt128(/*high=*/1, /*low=*/0),
              /*value=*/1,
              /*filtering_id=*/std::nullopt)),
      blink::mojom::AggregationServiceMode::kDefault,
      blink::mojom::DebugModeDetails::New());

  mojom::PrivateAggregationRequest kExpectedForEventRequest1(
      mojom::AggregatableReportContribution::NewForEventContribution(
          mojom::AggregatableReportForEventContribution::New(
              /*bucket=*/mojom::ForEventSignalBucket::NewIdBucket(234),
              /*value=*/mojom::ForEventSignalValue::NewIntValue(56),
              /*filtering_id=*/std::nullopt,
              /*event_type=*/
              mojom::EventType::NewReserved(
                  mojom::ReservedEventType::kReservedWin))),
      blink::mojom::AggregationServiceMode::kDefault,
      blink::mojom::DebugModeDetails::New());
  mojom::PrivateAggregationRequest kExpectedForEventRequest2(
      mojom::AggregatableReportContribution::NewForEventContribution(
          mojom::AggregatableReportForEventContribution::New(
              /*bucket=*/mojom::ForEventSignalBucket::NewIdBucket(
                  absl::MakeInt128(/*high=*/1,
                                   /*low=*/0)),
              /*value=*/mojom::ForEventSignalValue::NewIntValue(2),
              /*filtering_id=*/std::nullopt,
              /*event_type=*/
              mojom::EventType::NewReserved(
                  mojom::ReservedEventType::kReservedWin))),
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
        /*expected_data_version=*/std::nullopt,
        /*expected_debug_loss_report_url=*/std::nullopt,
        /*expected_debug_win_report_url=*/std::nullopt,
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
        /*expected_data_version=*/std::nullopt,
        /*expected_debug_loss_report_url=*/std::nullopt,
        /*expected_debug_win_report_url=*/std::nullopt,
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
        /*expected_data_version=*/std::nullopt,
        /*expected_debug_loss_report_url=*/std::nullopt,
        /*expected_debug_win_report_url=*/std::nullopt,
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
        /*expected_data_version=*/std::nullopt,
        /*expected_debug_loss_report_url=*/std::nullopt,
        /*expected_debug_win_report_url=*/std::nullopt,
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
        /*expected_data_version=*/std::nullopt,
        /*expected_debug_loss_report_url=*/std::nullopt,
        /*expected_debug_win_report_url=*/std::nullopt,
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
        /*expected_data_version=*/std::nullopt,
        /*expected_debug_loss_report_url=*/std::nullopt,
        /*expected_debug_win_report_url=*/std::nullopt,
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
        /*expected_data_version=*/std::nullopt,
        /*expected_debug_loss_report_url=*/std::nullopt,
        /*expected_debug_win_report_url=*/std::nullopt,
        /*expected_reject_reason=*/mojom::RejectReason::kNotAvailable,
        std::move(expected_pa_requests));
  }

  // Filtering IDs are specified
  {
    base::test::ScopedFeatureList scoped_feature_list{
        blink::features::kPrivateAggregationApiFilteringIds};

    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(mojom::PrivateAggregationRequest::New(
        mojom::AggregatableReportContribution::NewHistogramContribution(
            blink::mojom::AggregatableReportHistogramContribution::New(
                /*bucket=*/123,
                /*value=*/45,
                /*filtering_id=*/0)),
        blink::mojom::AggregationServiceMode::kDefault,
        blink::mojom::DebugModeDetails::New()));
    expected_pa_requests.push_back(mojom::PrivateAggregationRequest::New(
        mojom::AggregatableReportContribution::NewForEventContribution(
            mojom::AggregatableReportForEventContribution::New(
                /*bucket=*/mojom::ForEventSignalBucket::NewIdBucket(234),
                /*value=*/mojom::ForEventSignalValue::NewIntValue(56),
                /*filtering_id=*/255,
                /*event_type=*/
                mojom::EventType::NewReserved(
                    mojom::ReservedEventType::kReservedWin))),
        blink::mojom::AggregationServiceMode::kDefault,
        blink::mojom::DebugModeDetails::New()));

    RunScoreAdWithJavascriptExpectingResult(
        CreateScoreAdScript("5", R"(
            privateAggregation.contributeToHistogram(
                {bucket: 123n, value: 45, filteringId: 0n});
            privateAggregation.contributeToHistogramOnEvent(
                "reserved.win", {bucket: 234n, value: 56, filteringId: 255n});
        )"),
        5, /*expected_errors=*/{},
        mojom::ComponentAuctionModifiedBidParamsPtr(),
        /*expected_data_version=*/std::nullopt,
        /*expected_debug_loss_report_url=*/std::nullopt,
        /*expected_debug_win_report_url=*/std::nullopt,
        /*expected_reject_reason=*/mojom::RejectReason::kNotAvailable,
        std::move(expected_pa_requests));
  }
}

TEST_F(SellerWorkletPrivateAggregationEnabledTest, ReportResult) {
  mojom::PrivateAggregationRequest kExpectedRequest1(
      mojom::AggregatableReportContribution::NewHistogramContribution(
          blink::mojom::AggregatableReportHistogramContribution::New(
              /*bucket=*/123,
              /*value=*/45,
              /*filtering_id=*/std::nullopt)),
      blink::mojom::AggregationServiceMode::kDefault,
      blink::mojom::DebugModeDetails::New());
  mojom::PrivateAggregationRequest kExpectedRequest2(
      mojom::AggregatableReportContribution::NewHistogramContribution(
          blink::mojom::AggregatableReportHistogramContribution::New(
              /*bucket=*/absl::MakeInt128(/*high=*/1, /*low=*/0),
              /*value=*/1,
              /*filtering_id=*/std::nullopt)),
      blink::mojom::AggregationServiceMode::kDefault,
      blink::mojom::DebugModeDetails::New());
  mojom::PrivateAggregationRequest kExpectedForEventRequest(
      mojom::AggregatableReportContribution::NewForEventContribution(
          mojom::AggregatableReportForEventContribution::New(
              /*bucket=*/mojom::ForEventSignalBucket::NewIdBucket(234),
              /*value=*/mojom::ForEventSignalValue::NewIntValue(56),
              /*filtering_id=*/std::nullopt,
              /*event_type=*/
              mojom::EventType::NewReserved(
                  mojom::ReservedEventType::kReservedWin))),
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
        /*expected_report_url=*/std::nullopt, /*expected_ad_beacon_map=*/{},
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
        /*expected_report_url=*/std::nullopt, /*expected_ad_beacon_map=*/{},
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
        /*expected_report_url=*/std::nullopt, /*expected_ad_beacon_map=*/{},
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
        /*expected_signals_for_winner=*/std::nullopt,
        /*expected_report_url=*/std::nullopt, /*expected_ad_beacon_map=*/{},
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
        /*expected_report_url=*/std::nullopt, /*expected_ad_beacon_map=*/{},
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
        /*expected_report_url=*/std::nullopt, /*expected_ad_beacon_map=*/{},
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
        /*expected_report_url=*/std::nullopt, /*expected_ad_beacon_map=*/{},
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
        /*expected_signals_for_winner=*/std::nullopt,
        /*expected_report_url=*/std::nullopt, /*expected_ad_beacon_map=*/{},
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
        /*expected_report_url=*/std::nullopt, /*expected_ad_beacon_map=*/{},
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
        /*expected_report_url=*/std::nullopt, /*expected_ad_beacon_map=*/{},
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
        /*expected_signals_for_winner=*/std::nullopt,
        /*expected_report_url=*/std::nullopt, /*expected_ad_beacon_map=*/{},
        /*expected_pa_requests=*/{},
        /*expected_errors=*/
        {"https://url.test/:12 Uncaught TypeError: enableDebugMode may be "
         "called at most once."});
  }

  // Filtering IDs are specified
  {
    base::test::ScopedFeatureList scoped_feature_list{
        blink::features::kPrivateAggregationApiFilteringIds};

    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(mojom::PrivateAggregationRequest::New(
        mojom::AggregatableReportContribution::NewHistogramContribution(
            blink::mojom::AggregatableReportHistogramContribution::New(
                /*bucket=*/123,
                /*value=*/45,
                /*filtering_id=*/0)),
        blink::mojom::AggregationServiceMode::kDefault,
        blink::mojom::DebugModeDetails::New()));
    expected_pa_requests.push_back(mojom::PrivateAggregationRequest::New(
        mojom::AggregatableReportContribution::NewForEventContribution(
            mojom::AggregatableReportForEventContribution::New(
                /*bucket=*/mojom::ForEventSignalBucket::NewIdBucket(234),
                /*value=*/mojom::ForEventSignalValue::NewIntValue(56),
                /*filtering_id=*/255,
                /*event_type=*/
                mojom::EventType::NewReserved(
                    mojom::ReservedEventType::kReservedWin))),
        blink::mojom::AggregationServiceMode::kDefault,
        blink::mojom::DebugModeDetails::New()));

    RunReportResultCreatedScriptExpectingResult(
        "5",
        R"(
            privateAggregation.contributeToHistogram(
                {bucket: 123n, value: 45, filteringId: 0n});
            privateAggregation.contributeToHistogramOnEvent(
                "reserved.win", {bucket: 234n, value: 56, filteringId: 255n});
        )",
        /*expected_signals_for_winner=*/"5",
        /*expected_report_url=*/std::nullopt, /*expected_ad_beacon_map=*/{},
        std::move(expected_pa_requests),
        /*expected_errors=*/{});
  }
}

class SellerWorkletPrivateAggregationDisabledTest : public SellerWorkletTest {
 public:
  SellerWorkletPrivateAggregationDisabledTest() {
    feature_list_.InitAndDisableFeature(
        blink::features::kPrivateAggregationApi);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
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
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_reject_reason=*/mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{});
}

TEST_F(SellerWorkletPrivateAggregationDisabledTest, ReportResult) {
  RunReportResultCreatedScriptExpectingResult(
      R"(5)",
      R"(privateAggregation.contributeToHistogram({bucket: 123n, value: 45});)",
      /*expected_signals_for_winner=*/std::nullopt,
      /*expected_report_url=*/std::nullopt, /*expected_ad_beacon_map=*/{},
      /*expected_pa_requests=*/{},
      /*expected_errors=*/
      {"https://url.test/:10 Uncaught ReferenceError: privateAggregation is "
       "not defined."});
}

class SellerWorkletDeprecatedRenderURLReplacementsEnabledTest
    : public SellerWorkletTest {
 public:
  SellerWorkletDeprecatedRenderURLReplacementsEnabledTest() {
    feature_list_.InitAndEnableFeature(
        blink::features::kFledgeDeprecatedRenderURLReplacements);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(SellerWorkletDeprecatedRenderURLReplacementsEnabledTest,
       DeprecatedRenderURLReplacementsArePresentInScoreAdJavascript) {
  const std::vector<blink::AuctionConfig::AdKeywordReplacement>
      example_replacement = {blink::AuctionConfig::AdKeywordReplacement(
          {"${SELLER}", "ExampleSSP"})};

  auction_ad_config_non_shared_params_.deprecated_render_url_replacements =
      blink::AuctionConfig::MaybePromiseDeprecatedRenderURLReplacements::
          FromValue(std::move(example_replacement));

  std::string render_url_replacements_validator =
      R"(
        const replacementsJson =
    JSON.stringify(auctionConfig.deprecatedRenderURLReplacements);
        if (!(replacementsJson === "{\"${SELLER}\":\"ExampleSSP\"}")) {
          throw new Error('deprecatedRenderURLReplacements is incorrect' +
          'or missing.');
        })";

  RunScoreAdWithJavascriptExpectingResult(
      CreateScoreAdScript("1", render_url_replacements_validator), 1,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt);
}

class SellerWorkletCrossOriginTrustedSignalsTest : public SellerWorkletTest {
 public:
  SellerWorkletCrossOriginTrustedSignalsTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kFledgePermitCrossOriginTrustedSignals);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// With the feature on, same-origin trusted signals still come in the same,
// only there is an extra null param.
TEST_F(SellerWorkletCrossOriginTrustedSignalsTest, SameOrigin) {
  trusted_scoring_signals_url_ =
      GURL("https://url.test/trusted_scoring_signals");
  const GURL kNoComponentSignalsUrl(
      "https://url.test/trusted_scoring_signals?hostname=window.test"
      "&renderUrls=https%3A%2F%2Frender.url.test%2F");

  AddVersionedJsonResponse(&url_loader_factory_, kNoComponentSignalsUrl,
                           kTrustedScoringSignalsResponse, /*data_version=*/5);
  RunScoreAdWithReturnValueExpectingResult(
      "crossOriginTrustedSignals === null ? 1 : 0", 1,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/5);
  RunScoreAdWithReturnValueExpectingResult(
      "arguments.length", 7,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/5);
  RunScoreAdWithReturnValueExpectingResult(
      "browserSignals.dataVersion", 5,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/5);
  RunScoreAdWithReturnValueExpectingResult(
      "'crossOriginDataVersion' in browserSignals ? 3 : 2", 2,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/5);
  RunScoreAdWithReturnValueExpectingResult(
      "trustedScoringSignals.renderURL['https://render.url.test/']", 4,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/5);
}

// Cross-origin signals need explicit header to work; so if it's not there,
// or doesn't permit the origin, they will get blocked including the version.
TEST_F(SellerWorkletCrossOriginTrustedSignalsTest, ForbiddenCrossOrigin) {
  trusted_scoring_signals_url_ =
      GURL("https://other.test/trusted_scoring_signals");
  const GURL kNoComponentSignalsUrl(
      "https://other.test/trusted_scoring_signals?hostname=window.test"
      "&renderUrls=https%3A%2F%2Frender.url.test%2F");

  std::vector<std::string> expected_errors = {
      "https://url.test/ disregarding trusted scoring signals since origin "
      "'https://other.test' is different from script's origin but not "
      "authorized by script's Ad-Auction-Allow-Trusted-Scoring-Signals-From."};

  AddVersionedJsonResponse(&url_loader_factory_, kNoComponentSignalsUrl,
                           kTrustedScoringSignalsResponse, /*data_version=*/5);

  for (bool provide_header : {false, true}) {
    SCOPED_TRACE(provide_header);
    if (provide_header) {
      extra_js_headers_ =
          "Ad-Auction-Allow-Trusted-Scoring-Signals-From: "
          "\"http://other.test/\", \"https://url.test/\"";
    } else {
      extra_js_headers_ = std::nullopt;
    }

    RunScoreAdWithReturnValueExpectingResult(
        "crossOriginTrustedSignals === null ? 1 : 0", 1, expected_errors);
    RunScoreAdWithReturnValueExpectingResult("arguments.length", 7,
                                             expected_errors);
    RunScoreAdWithReturnValueExpectingResult(
        "trustedScoringSignals === null ? 1 : 0", 1, expected_errors);

    // No version in browserSignals... or passed out of worklet.
    RunScoreAdWithReturnValueExpectingResult(
        "'dataVersion' in browserSignals ? 0 : 1", 1, expected_errors,
        mojom::ComponentAuctionModifiedBidParamsPtr(),
        /*expected_data_version=*/std::nullopt);
    RunScoreAdWithReturnValueExpectingResult(
        "'crossOriginDataVersion' in browserSignals ? 0 : 1", 1,
        expected_errors, mojom::ComponentAuctionModifiedBidParamsPtr(),
        /*expected_data_version=*/std::nullopt);
  }
}

// Not allowed cross-origin trusted seller signals do not get fetched
TEST_F(SellerWorkletCrossOriginTrustedSignalsTest,
       ForbiddenCrossOriginNoFetch) {
  trusted_scoring_signals_url_ =
      GURL("https://other.test/trusted_scoring_signals");
  const char kScoreExpr[] = "crossOriginTrustedSignals === null ? 1 : 0";
  const char kError[] =
      "https://url.test/ disregarding trusted scoring signals since origin "
      "'https://other.test' is different from script's origin but not "
      "authorized by script's Ad-Auction-Allow-Trusted-Scoring-Signals-From.";

  std::vector<GURL> saw_urls;
  url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        saw_urls.push_back(request.url);
      }));

  for (bool provide_header : {false, true}) {
    base::HistogramTester histogram_tester;
    SCOPED_TRACE(provide_header);
    url_loader_factory_.ClearResponses();
    saw_urls.clear();
    if (provide_header) {
      extra_js_headers_ =
          "Ad-Auction-Allow-Trusted-Scoring-Signals-From: "
          "\"http://other.test/\", \"https://url.test/\"";
    } else {
      extra_js_headers_ = std::nullopt;
    }

    base::RunLoop run_loop;
    auto seller_worklet = CreateWorklet();
    RunScoreAdOnWorkletAsync(seller_worklet.get(),
                             /*expected_score=*/1.0,
                             /*expected_errors=*/{kError},
                             mojom::ComponentAuctionModifiedBidParamsPtr(),
                             /*expected_data_version=*/std::nullopt,
                             /*expected_debug_loss_report_url=*/std::nullopt,
                             /*expected_debug_win_report_url=*/std::nullopt,
                             mojom::RejectReason::kNotAvailable,
                             /*expected_pa_requests=*/{},
                             /*expected_real_time_contributions=*/{},
                             /*expected_bid_in_seller_currency=*/std::nullopt,
                             /*expected_score_ad_timeout=*/false,
                             /*expected_signals_fetch_latency=*/std::nullopt,
                             /*expected_code_ready_latency=*/std::nullopt,
                             run_loop.QuitClosure());

    // Only the script fetch must have started, even if we give enough time that
    // seller signals would normally be flushed.
    task_environment_.FastForwardBy(
        TrustedSignalsRequestManager::kAutoSendDelay);
    EXPECT_EQ(1, url_loader_factory_.NumPending());
    EXPECT_TRUE(url_loader_factory_.IsPending(decision_logic_url_.spec()));

    AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                          CreateScoreAdScript(kScoreExpr), extra_js_headers_);
    run_loop.Run();
    // We didn't just time out the signals fetch, it didn't happen at all.
    EXPECT_THAT(saw_urls, testing::ElementsAre(decision_logic_url_));

    // Also check the histograms.
    histogram_tester.ExpectTotalCount(
        "Ads.InterestGroup.Auction."
        "TrustedSellerSignalsCrossOriginPermissionWait",
        0);
    histogram_tester.ExpectUniqueSample(
        "Ads.InterestGroup.Auction.TrustedSellerSignalsOriginRelation",
        SellerWorklet::SignalsOriginRelation::kForbiddenCrossOriginSignals, 1);
  }
}

// Note that the fetch also involves CORS, but this isn't really a good place
// to test it since we're using a TestURLLoaderFactory so all the CORS code is
// bypassed.
TEST_F(SellerWorkletCrossOriginTrustedSignalsTest, AllowedCrossOrigin) {
  trusted_scoring_signals_url_ =
      GURL("https://other.test/trusted_scoring_signals");
  const GURL kNoComponentSignalsUrl(
      "https://other.test/trusted_scoring_signals?hostname=window.test"
      "&renderUrls=https%3A%2F%2Frender.url.test%2F");

  extra_js_headers_ =
      "Ad-Auction-Allow-Trusted-Scoring-Signals-From: "
      "\"https://more.test\", \"https://other.test/ignored\"";

  AddVersionedJsonResponse(&url_loader_factory_, kNoComponentSignalsUrl,
                           kTrustedScoringSignalsResponse, /*data_version=*/5);

  RunScoreAdWithReturnValueExpectingResult(
      "arguments.length", 7, /*expected_errors=*/{},
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/5);
  RunScoreAdWithReturnValueExpectingResult(
      "trustedScoringSignals === null ? 1 : 0", 1,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/5);
  const char kValidate[] = R"(
    const expected = '{"https://other.test":{' +
      '"renderURL":{"https://render.url.test/":4},' +
      '"renderUrl":{"https://render.url.test/":4}}}'
    const actual = JSON.stringify(crossOriginTrustedSignals);
    if (actual === expected)
      return 7;
    throw actual + "!" + expected;
  )";

  RunScoreAdWithJavascriptExpectingResult(
      CreateScoreAdScript("3", kValidate), 7,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/5);

  // Versions in crossOriginDataVersion, and passed out of worklet.
  RunScoreAdWithReturnValueExpectingResult(
      "'dataVersion' in browserSignals ? 0 : 1", 1,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/5);
  RunScoreAdWithReturnValueExpectingResult(
      "browserSignals.crossOriginDataVersion", 5,
      /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/5);
}

// When cross-origin trusted seller signals are allowed, they must happen
// after the script headers complete (approximated as script /fetch/
// completes in this test, since TestURLLoaderFactory isn't that fine-grained).
TEST_F(SellerWorkletCrossOriginTrustedSignalsTest, AllowedCrossOriginTiming) {
  trusted_scoring_signals_url_ =
      GURL("https://other.test/trusted_scoring_signals");
  const GURL kNoComponentSignalsUrl(
      "https://other.test/trusted_scoring_signals?hostname=window.test"
      "&renderUrls=https%3A%2F%2Frender.url.test%2F");

  extra_js_headers_ =
      "Ad-Auction-Allow-Trusted-Scoring-Signals-From: "
      "\"https://more.test\", \"https://other.test/ignored\"";

  const char kValidate[] = R"(
    const expected = '{"https://other.test":{' +
      '"renderURL":{"https://render.url.test/":4},' +
      '"renderUrl":{"https://render.url.test/":4}}}'
    const actual = JSON.stringify(crossOriginTrustedSignals);
    if (actual === expected)
      return 7;
    throw actual + "!" + expected;
  )";

  for (base::TimeDelta delay :
       {base::Seconds(0), TrustedSignalsRequestManager::kAutoSendDelay,
        base::Milliseconds(50)}) {
    base::HistogramTester histogram_tester;
    base::RunLoop run_loop;
    auto seller_worklet = CreateWorklet();
    RunScoreAdOnWorkletAsync(
        seller_worklet.get(),
        /*expected_score=*/7,
        /*expected_errors=*/{}, mojom::ComponentAuctionModifiedBidParamsPtr(),
        /*expected_data_version=*/5,
        /*expected_debug_loss_report_url=*/std::nullopt,
        /*expected_debug_win_report_url=*/std::nullopt,
        mojom::RejectReason::kNotAvailable,
        /*expected_pa_requests=*/{},
        /*expected_real_time_contributions=*/{},
        /*expected_bid_in_seller_currency=*/std::nullopt,
        /*expected_score_ad_timeout=*/false,
        /*expected_signals_fetch_latency=*/std::nullopt,
        /*expected_code_ready_latency=*/std::nullopt, run_loop.QuitClosure());

    // Only the script fetch must have started now... and after any (or no)
    // delay.
    if (delay != base::TimeDelta()) {
      task_environment_.FastForwardBy(delay);
    } else {
      // RunUntilIdle() does not advance the mock clock.
      task_environment_.RunUntilIdle();
    }
    EXPECT_EQ(1, url_loader_factory_.NumPending());
    EXPECT_TRUE(url_loader_factory_.IsPending(decision_logic_url_.spec()));

    AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                          CreateScoreAdScript("3", kValidate),
                          extra_js_headers_);
    // Run pending tasks without advancing time. The queued signals fetch should
    // be requested immediately, regardless of whether "kAutoSendDelay" has
    // passed or not.
    task_environment_.RunUntilIdle();

    // Now the trusted signals fetch must be pending, too.
    EXPECT_EQ(1, url_loader_factory_.NumPending());
    EXPECT_TRUE(url_loader_factory_.IsPending(kNoComponentSignalsUrl.spec()));

    // Pretend it took 100ms.
    task_environment_.AdvanceClock(base::Milliseconds(100));
    AddVersionedJsonResponse(&url_loader_factory_, kNoComponentSignalsUrl,
                             kTrustedScoringSignalsResponse,
                             /*data_version=*/5);
    run_loop.Run();
    url_loader_factory_.ClearResponses();

    // The delay we report on trusted signals is just how long the script
    // fetch took.
    histogram_tester.ExpectUniqueSample(
        "Ads.InterestGroup.Auction."
        "TrustedSellerSignalsCrossOriginPermissionWait",
        delay.InMilliseconds(), 1);
    histogram_tester.ExpectUniqueSample(
        "Ads.InterestGroup.Auction.TrustedSellerSignalsOriginRelation",
        SellerWorklet::SignalsOriginRelation::kPermittedCrossOriginSignals, 1);
  }
}

// Handling of errors in trusted signals other than the cross-origin permission;
// it should look identical except for the error message test.
TEST_F(SellerWorkletCrossOriginTrustedSignalsTest, ErrorCrossOrigin) {
  trusted_scoring_signals_url_ =
      GURL("https://other.test/trusted_scoring_signals");
  const GURL kNoComponentSignalsUrl(
      "https://other.test/trusted_scoring_signals?hostname=window.test"
      "&renderUrls=https%3A%2F%2Frender.url.test%2F");

  std::vector<std::string> expected_errors = {
      "https://other.test/trusted_scoring_signals Unable to parse as a JSON "
      "object."};

  extra_js_headers_ =
      "Ad-Auction-Allow-Trusted-Scoring-Signals-From: "
      "\"https://more.test\", \"https://other.test/ignored\"";

  AddVersionedJsonResponse(&url_loader_factory_, kNoComponentSignalsUrl, "{",
                           /*data_version=*/5);

  const char* kTestCases[] = {
      "crossOriginTrustedSignals === null ? 1 : 0",
      "arguments.length === 7 ? 1 : 0",
      "trustedScoringSignals === null ? 1 : 0",
      "'dataVersion' in browserSignals ? 0 : 1",
      "'crossOriginDataVersion' in browserSignals ? 0 : 1"};

  for (const char* test_case : kTestCases) {
    SCOPED_TRACE(test_case);
    mojom::RealTimeReportingContribution expected_trusted_signal_histogram(
        /*bucket=*/1024 + auction_worklet::RealTimeReportingPlatformError::
                              kTrustedScoringSignalsFailure,
        /*priority_weight=*/1,
        /*latency_threshold=*/std::nullopt);
    RealTimeReportingContributions expected_real_time_contributions;
    expected_real_time_contributions.push_back(
        expected_trusted_signal_histogram.Clone());
    RunScoreAdWithReturnValueExpectingResult(
        /*raw_return_value=*/test_case, /*expected_score=*/1, expected_errors,
        mojom::ComponentAuctionModifiedBidParamsPtr(),
        /*expected_data_version=*/std::nullopt,
        /*expected_debug_loss_report_url=*/std::nullopt,
        /*expected_debug_win_report_url=*/std::nullopt,
        /*expected_reject_reason=*/mojom::RejectReason::kNotAvailable,
        /*expected_pa_requests=*/{},
        std::move(expected_real_time_contributions));
  }
}

// Need to use SYSTEM_TIME, because scoring latency uses elapsed_timer, which is
// always 0 if not using SYSTEM_TIME.
class SellerWorkletRealTimeReportingEnabledTest : public SellerWorkletTest {
 public:
  SellerWorkletRealTimeReportingEnabledTest()
      : SellerWorkletTest(
            base::test::TaskEnvironment::TimeSource::SYSTEM_TIME) {
    feature_list_.InitAndEnableFeature(
        blink::features::kFledgeRealTimeReporting);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(SellerWorkletRealTimeReportingEnabledTest, RealTimeReporting) {
  mojom::RealTimeReportingContribution expected_histogram(
      /*bucket=*/100, /*priority_weight=*/0.5,
      /*latency_threshold=*/std::nullopt);

  // Worklet latency contribution is tested in ScriptTimeout.
  // Cannot reliably test worklet latency contribution here since the script
  // takes 0ms to run some times, which is not higher than the smallest allowed
  // latency threshold (0ms), in which case the contribution will be dropped.
  constexpr char kExtraCode[] = R"(
realTimeReporting.contributeToHistogram({bucket: 100, priorityWeight: 0.5})
)";

  RealTimeReportingContributions expected_real_time_contributions;
  expected_real_time_contributions.push_back(expected_histogram.Clone());

  RunScoreAdWithJavascriptExpectingResult(
      CreateScoreAdScript("5", kExtraCode), 5, /*expected_errors=*/{},
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_reject_reason=*/mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{}, std::move(expected_real_time_contributions));
}

TEST_F(SellerWorkletRealTimeReportingEnabledTest, InvalidScore) {
  mojom::RealTimeReportingContribution expected_histogram(
      /*bucket=*/100, /*priority_weight=*/0.5,
      /*latency_threshold=*/std::nullopt);

  constexpr char kExtraCode[] = R"(
realTimeReporting.contributeToHistogram({bucket: 100, priorityWeight: 0.5})
)";

  RealTimeReportingContributions expected_real_time_contributions;
  expected_real_time_contributions.push_back(expected_histogram.Clone());

  RunScoreAdWithJavascriptExpectingResult(
      CreateScoreAdScript("\"invalid_score\"", kExtraCode), 0,
      /*expected_errors=*/
      {"https://url.test/ scoreAd() return: Value passed as dictionary is "
       "neither object, null, nor undefined."},
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_reject_reason=*/mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{}, std::move(expected_real_time_contributions));
}

TEST_F(SellerWorkletRealTimeReportingEnabledTest, ScriptTimeout) {
  // Set timeout to a small number, and use a while loop in the script to let it
  // timeout. Then the execution time would be higher than the latency threshold
  // of 1ms thus the latency contribution will be kept.
  seller_timeout_ = base::Milliseconds(3);

  mojom::RealTimeReportingContribution expected_histogram(
      /*bucket=*/100, /*priority_weight=*/0.5,
      /*latency_threshold=*/std::nullopt);
  mojom::RealTimeReportingContribution expected_latency_histogram(
      /*bucket=*/200, /*priority_weight=*/2, /*latency_threshold=*/1);
  constexpr char kExtraCode[] = R"(
realTimeReporting.contributeToHistogram({bucket: 100, priorityWeight: 0.5});
realTimeReporting.contributeToHistogram(
    {bucket: 200, priorityWeight: 2, latencyThreshold: 1});
while (1);
)";

  RealTimeReportingContributions expected_real_time_contributions;
  expected_real_time_contributions.push_back(expected_histogram.Clone());
  expected_real_time_contributions.push_back(
      expected_latency_histogram.Clone());

  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        CreateScoreAdScript("5", kExtraCode));

  auto seller_worklet = CreateWorklet();
  ASSERT_TRUE(seller_worklet);
  base::RunLoop run_loop;
  RunScoreAdOnWorkletAsync(
      seller_worklet.get(), /*expected_score=*/0,
      /*expected_errors=*/
      {"https://url.test/ execution of `scoreAd` timed out."},
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{},
      /*expected_real_time_contributions=*/
      std::move(expected_real_time_contributions),
      /*expected_bid_in_seller_currency=*/std::nullopt,
      /*expected_score_ad_timeout=*/true,
      /*expected_signals_fetch_latency=*/std::nullopt,
      /*expected_code_ready_latency=*/std::nullopt, run_loop.QuitClosure());
  run_loop.Run();
}

// contributeToHistogram's is dropped when the script's latency does not
// exceed the threshold.
TEST_F(SellerWorkletRealTimeReportingEnabledTest,
       NotExceedingLatencyThreshold) {
  mojom::RealTimeReportingContribution expected_histogram(
      /*bucket=*/100, /*priority_weight=*/0.5,
      /*latency_threshold=*/std::nullopt);
  constexpr char kExtraCode[] = R"(
realTimeReporting.contributeToHistogram({bucket: 100, priorityWeight: 0.5});
realTimeReporting.contributeToHistogram(
    {bucket: 200, priorityWeight: 2, latencyThreshold: 10000000})
)";

  // Only contributeToHistogram's contribution is kept.
  // contributeToHistogram's is filtered out since the script's latency
  // didn't exceed the threshold.
  RealTimeReportingContributions expected_real_time_contributions;
  expected_real_time_contributions.push_back(expected_histogram.Clone());

  RunScoreAdWithJavascriptExpectingResult(
      CreateScoreAdScript("5", kExtraCode), 5, /*expected_errors=*/{},
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_reject_reason=*/mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{}, std::move(expected_real_time_contributions));
}

// A platform contribution is added when trusted scoring signals server returned
// a non-2xx HTTP response code.
TEST_F(SellerWorkletRealTimeReportingEnabledTest,
       TrustedScoringSignalNetworkError) {
  trusted_scoring_signals_url_ =
      GURL("https://url.test/trusted_scoring_signals");
  // Trusted scoring signals URL without any component ads.
  const GURL kNoComponentSignalsUrl = GURL(
      "https://url.test/trusted_scoring_signals?hostname=window.test"
      "&renderUrls=https%3A%2F%2Frender.url.test%2F");

  // A network error when fetching the scoring signals results in null
  // `trustedScoringSignals`.
  url_loader_factory_.AddResponse(kNoComponentSignalsUrl.spec(),
                                  /*content=*/std::string(),
                                  net::HTTP_NOT_FOUND);

  auction_worklet::mojom::RealTimeReportingContribution expected_histogram(
      /*bucket=*/100, /*priority_weight=*/0.5,
      /*latency_threshold=*/std::nullopt);
  mojom::RealTimeReportingContribution expected_trusted_signal_histogram(
      /*bucket=*/1024 + auction_worklet::RealTimeReportingPlatformError::
                            kTrustedScoringSignalsFailure,
      /*priority_weight=*/1,
      /*latency_threshold=*/std::nullopt);

  constexpr char kExtraCode[] = R"(
realTimeReporting.contributeToHistogram({bucket: 100, priorityWeight: 0.5})
)";

  RealTimeReportingContributions expected_real_time_contributions;
  expected_real_time_contributions.push_back(expected_histogram.Clone());
  expected_real_time_contributions.push_back(
      expected_trusted_signal_histogram.Clone());

  RunScoreAdWithJavascriptExpectingResult(
      CreateScoreAdScript("trustedScoringSignals === null ? 1 : 0", kExtraCode),
      1, /*expected_errors=*/
      {base::StringPrintf("Failed to load %s HTTP status = 404 Not Found.",
                          kNoComponentSignalsUrl.spec().c_str())},
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_reject_reason=*/mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{}, std::move(expected_real_time_contributions));
}

// A platform contribution is added when trusted scoring signals server returned
// a non-2xx HTTP response code, even though scoreAd() failed.
TEST_F(SellerWorkletRealTimeReportingEnabledTest,
       TrustedScoringSignalNetworkErrorScoreAdFailed) {
  trusted_scoring_signals_url_ =
      GURL("https://url.test/trusted_scoring_signals");
  // Trusted scoring signals URL without any component ads.
  const GURL kNoComponentSignalsUrl = GURL(
      "https://url.test/trusted_scoring_signals?hostname=window.test"
      "&renderUrls=https%3A%2F%2Frender.url.test%2F");

  // A network error when fetching the scoring signals results in null
  // `trustedScoringSignals`.
  url_loader_factory_.AddResponse(kNoComponentSignalsUrl.spec(),
                                  /*content=*/std::string(),
                                  net::HTTP_NOT_FOUND);

  mojom::RealTimeReportingContribution expected_trusted_signal_histogram(
      /*bucket=*/1024 + auction_worklet::RealTimeReportingPlatformError::
                            kTrustedScoringSignalsFailure,
      /*priority_weight=*/1,
      /*latency_threshold=*/std::nullopt);

  RealTimeReportingContributions expected_real_time_contributions;
  expected_real_time_contributions.push_back(
      expected_trusted_signal_histogram.Clone());

  // scoreAd() failed due to no return value.
  RunScoreAdWithJavascriptExpectingResult(
      CreateScoreAdScript(""),
      /*expected_score=*/0, /*expected_errors=*/
      {base::StringPrintf("Failed to load %s HTTP status = 404 Not Found.",
                          kNoComponentSignalsUrl.spec().c_str()),
       "https://url.test/ scoreAd() return: Required field 'desirability' "
       "is undefined."},
      mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_reject_reason=*/mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{}, std::move(expected_real_time_contributions));
}

// No platform contribution for trusted scoring signals failure when getting the
// signal succeeded.
TEST_F(SellerWorkletRealTimeReportingEnabledTest,
       TrustedScoringSignalSucceedsNoContributionAdded) {
  trusted_scoring_signals_url_ =
      GURL("https://url.test/trusted_scoring_signals");
  const GURL kSignalsUrl = GURL(
      "https://url.test/trusted_scoring_signals?hostname=window.test"
      "&renderUrls=https%3A%2F%2Frender.url.test%2F");

  // Successful download case.
  AddJsonResponse(&url_loader_factory_, kSignalsUrl,
                  kTrustedScoringSignalsResponse);

  auction_worklet::mojom::RealTimeReportingContribution expected_histogram(
      /*bucket=*/100, /*priority_weight=*/0.5,
      /*latency_threshold=*/std::nullopt);
  constexpr char kExtraCode[] = R"(
realTimeReporting.contributeToHistogram({bucket: 100, priorityWeight: 0.5})
)";

  RealTimeReportingContributions expected_real_time_contributions;
  expected_real_time_contributions.push_back(expected_histogram.Clone());

  RunScoreAdWithJavascriptExpectingResult(
      CreateScoreAdScript("trustedScoringSignals === null ? 1 : 0", kExtraCode),
      0, /*expected_errors=*/
      {}, mojom::ComponentAuctionModifiedBidParamsPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_reject_reason=*/mojom::RejectReason::kNotAvailable,
      /*expected_pa_requests=*/{}, std::move(expected_real_time_contributions));
}

}  // namespace
}  // namespace auction_worklet
