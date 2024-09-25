// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/bidder_worklet.h"

#include <stdint.h>

#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "components/cbor/writer.h"
#include "content/common/features.h"
#include "content/public/common/content_features.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/cpp/cbor_test_util.h"
#include "content/services/auction_worklet/public/cpp/real_time_reporting.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom-forward.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom-forward.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "content/services/auction_worklet/public/mojom/real_time_reporting.mojom.h"
#include "content/services/auction_worklet/worklet_devtools_debug_test_util.h"
#include "content/services/auction_worklet/worklet_test_util.h"
#include "content/services/auction_worklet/worklet_v8_debug_test_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "net/http/http_status_code.h"
#include "net/third_party/quiche/src/quiche/oblivious_http/oblivious_http_gateway.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "third_party/blink/public/common/interest_group/ad_auction_currencies.h"
#include "third_party/blink/public/common/interest_group/ad_display_size.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "third_party/googletest/src/googlemock/include/gmock/gmock-more-matchers.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

using testing::HasSubstr;
using testing::IsEmpty;
using testing::StartsWith;
using testing::UnorderedElementsAre;

namespace auction_worklet {
namespace {

using base::test::TaskEnvironment;
using PrivateAggregationRequests = BidderWorklet::PrivateAggregationRequests;
using RealTimeReportingContributions =
    BidderWorklet::RealTimeReportingContributions;

// This was produced by running wat2wasm on this:
// (module
//  (global (export "test_const") i32 (i32.const 123))
// )
const uint8_t kToyWasm[] = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x06, 0x07, 0x01,
    0x7f, 0x00, 0x41, 0xfb, 0x00, 0x0b, 0x07, 0x0e, 0x01, 0x0a, 0x74,
    0x65, 0x73, 0x74, 0x5f, 0x63, 0x6f, 0x6e, 0x73, 0x74, 0x03, 0x00};

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

const char kWasmUrl[] = "https://foo.test/helper.wasm";
const uint8_t kKeyId = 0xFF;

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
                         directFromSellerSignals, crossOriginTrustedSignals) {
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

// Returns a working script which executes given `extra_code` as well.
static std::string CreateBasicGenerateBidScriptWithExtraCode(
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
      base::TimeDelta trusted_signals_fetch_latency,
      std::optional<base::TimeDelta> update_if_older_than,
      base::OnceClosure callback)>;

  using GenerateBidCallback = base::OnceCallback<void(
      std::vector<mojom::BidderWorkletBidPtr> bid,
      std::optional<uint32_t> data_version,
      const std::optional<GURL>& debug_loss_report_url,
      const std::optional<GURL>& debug_win_report_url,
      std::optional<double> set_priority,
      base::flat_map<std::string,
                     auction_worklet::mojom::PrioritySignalsDoublePtr>
          update_priority_signals_overrides,
      PrivateAggregationRequests pa_requests,
      PrivateAggregationRequests non_kanon_pa_requests,
      RealTimeReportingContributions real_time_contributions,
      mojom::BidderTimingMetricsPtr generate_bid_metrics,
      mojom::GenerateBidDependencyLatenciesPtr
          generate_bid_dependency_latencies,
      mojom::RejectReason reject_reason,
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
        [](std::vector<mojom::BidderWorkletBidPtr> bids,
           std::optional<uint32_t> data_version,
           const std::optional<GURL>& debug_loss_report_url,
           const std::optional<GURL>& debug_win_report_url,
           std::optional<double> set_priority,
           base::flat_map<std::string,
                          auction_worklet::mojom::PrioritySignalsDoublePtr>
               update_priority_signals_overrides,
           PrivateAggregationRequests pa_requests,
           PrivateAggregationRequests non_kanon_pa_requests,
           RealTimeReportingContributions real_time_contributions,
           mojom::BidderTimingMetricsPtr generate_bid_metrics,
           mojom::GenerateBidDependencyLatenciesPtr
               generate_bid_dependency_latencies,
           mojom::RejectReason reject_reason,
           const std::vector<std::string>& errors) {
          ADD_FAILURE() << "OnGenerateBidComplete should not be invoked.";
        });
  }

  // mojom::GenerateBidClient implementation:

  void OnBiddingSignalsReceived(
      const base::flat_map<std::string, double>& priority_vector,
      base::TimeDelta trusted_signals_fetch_latency,
      std::optional<base::TimeDelta> update_if_older_than,
      base::OnceClosure callback) override {
    // May only be called once.
    EXPECT_FALSE(on_bidding_signals_received_invoked_);
    on_bidding_signals_received_invoked_ = true;

    if (on_bidding_signals_received_callback_) {
      std::move(on_bidding_signals_received_callback_)
          .Run(priority_vector, trusted_signals_fetch_latency,
               update_if_older_than, std::move(callback));
      return;
    }
    std::move(callback).Run();
  }

  void OnGenerateBidComplete(
      std::vector<mojom::BidderWorkletBidPtr> bids,
      std::optional<uint32_t> data_version,
      const std::optional<GURL>& debug_loss_report_url,
      const std::optional<GURL>& debug_win_report_url,
      std::optional<double> set_priority,
      base::flat_map<std::string,
                     auction_worklet::mojom::PrioritySignalsDoublePtr>
          update_priority_signals_overrides,
      PrivateAggregationRequests pa_requests,
      PrivateAggregationRequests non_kanon_pa_requests,
      RealTimeReportingContributions real_time_contributions,
      mojom::BidderTimingMetricsPtr generate_bid_metrics,
      mojom::GenerateBidDependencyLatenciesPtr
          generate_bid_dependency_latencies,
      mojom::RejectReason reject_reason,
      const std::vector<std::string>& errors) override {
    // OnBiddingSignalsReceived() must be called first.
    EXPECT_TRUE(on_bidding_signals_received_invoked_);

    std::move(generate_bid_callback_)
        .Run(std::move(bids), data_version, debug_loss_report_url,
             debug_win_report_url, set_priority,
             std::move(update_priority_signals_overrides),
             std::move(pa_requests), std::move(non_kanon_pa_requests),
             std::move(real_time_contributions),
             std::move(generate_bid_metrics),
             std::move(generate_bid_dependency_latencies), reject_reason,
             errors);
  }

 private:
  bool on_bidding_signals_received_invoked_ = false;
  OnBiddingSignalsReceivedCallback on_bidding_signals_received_callback_;
  GenerateBidCallback generate_bid_callback_;
};

class BidderWorkletTest : public testing::Test {
 public:
  BidderWorkletTest(TaskEnvironment::TimeSource time_source =
                        TaskEnvironment::TimeSource::SYSTEM_TIME)
      : task_environment_(time_source) {
    SetDefaultParameters();
    feature_list_.InitAndEnableFeature(
        features::kInterestGroupUpdateIfOlderThan);
  }

  ~BidderWorkletTest() override = default;

  void SetUp() override {
    // The v8_helpers_ need to be created here instead of the constructor,
    // because this test fixture has a subclass that initializes a
    // ScopedFeatureList in their constructor, which needs to be done BEFORE
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
  }

  virtual size_t NumThreads() { return 1u; }

  // Default values. No test actually depends on these being anything but valid,
  // but test that set these can use this to reset values to default after each
  // test.
  void SetDefaultParameters() {
    interest_group_name_ = "Fred";
    is_for_additional_bid_ = false;
    reporting_id_field_ = mojom::ReportingIdField::kInterestGroupName;
    interest_group_name_reporting_id_ = interest_group_name_;
    interest_group_enable_bidding_signals_prioritization_ = false;
    interest_group_priority_vector_.reset();
    interest_group_user_bidding_signals_ = std::nullopt;
    join_origin_ = url::Origin::Create(GURL("https://url.test/"));
    execution_mode_ =
        blink::mojom::InterestGroup::ExecutionMode::kCompatibilityMode;

    interest_group_ads_.clear();
    interest_group_ads_.emplace_back(GURL("https://response.test/"),
                                     /*metadata=*/std::nullopt);

    interest_group_ad_components_.reset();
    interest_group_ad_components_.emplace();
    interest_group_ad_components_->emplace_back(
        GURL("https://ad_component.test/"), /*metadata=*/std::nullopt);

    kanon_keys_.clear();
    kanon_mode_ = auction_worklet::mojom::KAnonymityBidMode::kNone;
    bid_is_kanon_ = false;
    provide_direct_from_seller_signals_late_ = false;

    update_url_.reset();

    interest_group_trusted_bidding_signals_url_.reset();
    interest_group_trusted_bidding_signals_keys_.reset();

    browser_signal_join_count_ = 2;
    browser_signal_bid_count_ = 3;
    browser_signal_for_debugging_only_in_cooldown_or_lockout_ = false;
    browser_signal_prev_wins_.clear();

    auction_signals_ = "[\"auction_signals\"]";
    per_buyer_signals_ = "[\"per_buyer_signals\"]";
    per_buyer_timeout_ = std::nullopt;
    per_buyer_currency_ = std::nullopt;
    top_window_origin_ = url::Origin::Create(GURL("https://top.window.test/"));
    permissions_policy_state_ =
        mojom::AuctionWorkletPermissionsPolicyState::New(
            /*private_aggregation_allowed=*/true,
            /*shared_storage_allowed=*/false);
    experiment_group_id_ = std::nullopt;
    public_key_ = nullptr;

    browser_signal_seller_origin_ =
        url::Origin::Create(GURL("https://browser.signal.seller.test/"));
    browser_signal_top_level_seller_origin_.reset();
    seller_signals_ = "[\"seller_signals\"]";
    browser_signal_render_url_ = GURL("https://render_url.test/");
    browser_signal_bid_ = 1;
    browser_signal_bid_currency_ = std::nullopt;
    browser_signal_highest_scoring_other_bid_ = 0.5;
    browser_signal_highest_scoring_other_bid_currency_ = std::nullopt;
    browser_signal_made_highest_scoring_other_bid_ = false;
    browser_signal_ad_cost_.reset();
    browser_signal_modeling_signals_.reset();
    browser_signal_join_count_ = 2;
    browser_signal_recency_report_win_ = 5;
    data_version_.reset();
  }

  // Because making a vector of move-only values is unwieldy, the test helpers
  // take this variant that's easily constructible from a single bid object.
  using OneOrManyBids = absl::variant<mojom::BidderWorkletBidPtr,
                                      std::vector<mojom::BidderWorkletBidPtr>>;

  // Helper that creates and runs a script to validate that `expression`
  // evaluates to true when evaluated in a generateBid() script. Does this by
  // evaluating the expression in the content of generateBid() and throwing if
  // it's not true. Otherwise, a bid is generated.
  void RunGenerateBidExpectingExpressionIsTrue(
      const std::string& expression,
      const std::optional<uint32_t>& expected_data_version = std::nullopt) {
    std::string script = CreateGenerateBidScript(
        /*raw_return_value=*/R"({bid: 1, render:"https://response.test/"})",
        /*extra_code=*/base::StringPrintf(R"(let val = %s;
                                             if (val !== true)
                                             throw JSON.stringify(val);)",
                                          expression.c_str()));

    RunGenerateBidWithJavascriptExpectingResult(
        script,
        mojom::BidderWorkletBid::New(
            auction_worklet::mojom::BidRole::kUnenforcedKAnon,
            /*ad=*/"null",
            /*bid=*/1, /*bid_currency=*/std::nullopt,
            /*ad_cost=*/std::nullopt,
            blink::AdDescriptor(GURL("https://response.test/")),
            /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
            /*ad_component_descriptors=*/std::nullopt,
            /*modeling_signals=*/std::nullopt, base::TimeDelta()),
        expected_data_version);
  }

  // Configures `url_loader_factory_` to return a generateBid() script with the
  // specified return line. Then runs the script, expecting the provided result
  // to be the first returned bid.
  void RunGenerateBidWithReturnValueExpectingResult(
      const std::string& raw_return_value,
      OneOrManyBids expected_bids,
      const std::optional<uint32_t>& expected_data_version = std::nullopt,
      std::vector<std::string> expected_errors = std::vector<std::string>(),
      const std::optional<GURL>& expected_debug_loss_report_url = std::nullopt,
      const std::optional<GURL>& expected_debug_win_report_url = std::nullopt,
      const std::optional<double> expected_set_priority = std::nullopt,
      const base::flat_map<std::string, std::optional<double>>&
          expected_update_priority_signals_overrides =
              base::flat_map<std::string, std::optional<double>>(),
      PrivateAggregationRequests expected_pa_requests = {},
      PrivateAggregationRequests expected_non_kanon_pa_requests = {},
      RealTimeReportingContributions expected_real_time_contributions = {}) {
    RunGenerateBidWithJavascriptExpectingResult(
        CreateGenerateBidScript(raw_return_value), std::move(expected_bids),
        expected_data_version, expected_errors, expected_debug_loss_report_url,
        expected_debug_win_report_url, expected_set_priority,
        expected_update_priority_signals_overrides,
        std::move(expected_pa_requests),
        std::move(expected_non_kanon_pa_requests),
        std::move(expected_real_time_contributions));
  }

  // Configures `url_loader_factory_` to return a script with the specified
  // Javascript. Then runs the script, expecting the provided result to be the
  // first returned bid.
  void RunGenerateBidWithJavascriptExpectingResult(
      const std::string& javascript,
      OneOrManyBids expected_bids,
      const std::optional<uint32_t>& expected_data_version = std::nullopt,
      std::vector<std::string> expected_errors = std::vector<std::string>(),
      const std::optional<GURL>& expected_debug_loss_report_url = std::nullopt,
      const std::optional<GURL>& expected_debug_win_report_url = std::nullopt,
      const std::optional<double> expected_set_priority = std::nullopt,
      const base::flat_map<std::string, std::optional<double>>&
          expected_update_priority_signals_overrides =
              base::flat_map<std::string, std::optional<double>>(),
      PrivateAggregationRequests expected_pa_requests = {},
      PrivateAggregationRequests expected_non_kanon_pa_requests = {},
      RealTimeReportingContributions expected_real_time_contributions = {}) {
    SCOPED_TRACE(javascript);
    AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                          javascript);
    RunGenerateBidExpectingResult(
        std::move(expected_bids), expected_data_version, expected_errors,
        expected_debug_loss_report_url, expected_debug_win_report_url,
        expected_set_priority, expected_update_priority_signals_overrides,
        std::move(expected_pa_requests),
        std::move(expected_non_kanon_pa_requests),
        std::move(expected_real_time_contributions));
  }

  // Loads and runs a generateBid() script, expecting the provided result to
  // be the first returned bid.
  //
  // `bid_duration` of `expected_bid` is ignored unless it's non-zero, in which
  // case the duration is expected to be at least `bid_duration` - useful for
  // testing that `bid_duration` at least seems to reflect timeouts.
  void RunGenerateBidExpectingResult(
      OneOrManyBids expected_bid_or_bids,
      const std::optional<uint32_t>& expected_data_version = std::nullopt,
      std::vector<std::string> expected_errors = std::vector<std::string>(),
      const std::optional<GURL>& expected_debug_loss_report_url = std::nullopt,
      const std::optional<GURL>& expected_debug_win_report_url = std::nullopt,
      const std::optional<double> expected_set_priority = std::nullopt,
      const base::flat_map<std::string, std::optional<double>>&
          expected_update_priority_signals_overrides =
              base::flat_map<std::string, std::optional<double>>(),
      PrivateAggregationRequests expected_pa_requests = {},
      PrivateAggregationRequests expected_non_kanon_pa_requests = {},
      RealTimeReportingContributions expected_real_time_contributions = {}) {
    std::vector<mojom::BidderWorkletBidPtr> expected_bids;
    if (absl::holds_alternative<mojom::BidderWorkletBidPtr>(
            expected_bid_or_bids)) {
      mojom::BidderWorkletBidPtr expected_bid =
          absl::get<mojom::BidderWorkletBidPtr>(
              std::move(expected_bid_or_bids));
      if (expected_bid) {
        expected_bids.push_back(std::move(expected_bid));
      }
    } else {
      expected_bids = absl::get<std::vector<mojom::BidderWorkletBidPtr>>(
          std::move(expected_bid_or_bids));
    }

    auto bidder_worklet = CreateWorkletAndGenerateBid();

    EXPECT_EQ(expected_errors, bid_errors_);
    ASSERT_EQ(expected_bids.size(), bids_.size());
    for (size_t i = 0; i < bids_.size(); ++i) {
      const mojom::BidderWorkletBidPtr& expected_bid = expected_bids[i];
      EXPECT_EQ(expected_bid->bid_role, bids_[i]->bid_role);
      EXPECT_EQ(expected_bid->ad, bids_[i]->ad);
      EXPECT_EQ(expected_bid->selected_buyer_and_seller_reporting_id,
                bids_[i]->selected_buyer_and_seller_reporting_id);
      EXPECT_EQ(expected_bid->bid, bids_[i]->bid);
      EXPECT_EQ(blink::PrintableAdCurrency(expected_bid->bid_currency),
                blink::PrintableAdCurrency(bids_[i]->bid_currency));
      EXPECT_EQ(expected_bid->ad_descriptor.url, bids_[i]->ad_descriptor.url);
      EXPECT_EQ(expected_bid->ad_descriptor.size, bids_[i]->ad_descriptor.size);
      if (!expected_bid->ad_component_descriptors) {
        EXPECT_FALSE(bids_[i]->ad_component_descriptors);
      } else {
        EXPECT_THAT(*bids_[i]->ad_component_descriptors,
                    ::testing::ElementsAreArray(
                        *expected_bid->ad_component_descriptors));
      }
      if (!expected_bid->bid_duration.is_zero()) {
        EXPECT_GE(bids_[i]->bid_duration, expected_bid->bid_duration);
      }
    }
    EXPECT_EQ(expected_data_version, data_version_);
    EXPECT_EQ(expected_debug_loss_report_url, bid_debug_loss_report_url_);
    EXPECT_EQ(expected_debug_win_report_url, bid_debug_win_report_url_);
    EXPECT_EQ(expected_pa_requests, pa_requests_);
    EXPECT_EQ(expected_non_kanon_pa_requests, non_kanon_pa_requests_);
    EXPECT_EQ(expected_real_time_contributions, real_time_contributions_);
    EXPECT_EQ(expected_set_priority, set_priority_);
    EXPECT_EQ(expected_update_priority_signals_overrides,
              update_priority_signals_overrides_);
  }

  // Configures `url_loader_factory_` to return a reportWin() script with the
  // specified body. Then runs the script, expecting the provided result.
  void RunReportWinWithFunctionBodyExpectingResult(
      const std::string& function_body,
      const std::optional<GURL>& expected_report_url,
      const base::flat_map<std::string, GURL>& expected_ad_beacon_map =
          base::flat_map<std::string, GURL>(),
      const base::flat_map<std::string, std::string>& expected_ad_macro_map =
          base::flat_map<std::string, std::string>(),
      PrivateAggregationRequests expected_pa_requests = {},
      const std::vector<std::string>& expected_errors =
          std::vector<std::string>()) {
    RunReportWinWithJavascriptExpectingResult(
        CreateReportWinScript(function_body), expected_report_url,
        expected_ad_beacon_map, std::move(expected_ad_macro_map),
        std::move(expected_pa_requests), expected_errors);
  }

  // Configures `url_loader_factory_` to return a reportWin() script with the
  // specified Javascript. Then runs the script, expecting the provided result.
  void RunReportWinWithJavascriptExpectingResult(
      const std::string& javascript,
      const std::optional<GURL>& expected_report_url,
      const base::flat_map<std::string, GURL>& expected_ad_beacon_map =
          base::flat_map<std::string, GURL>(),
      const base::flat_map<std::string, std::string>& expected_ad_macro_map =
          {},
      PrivateAggregationRequests expected_pa_requests = {},
      const std::vector<std::string>& expected_errors =
          std::vector<std::string>()) {
    SCOPED_TRACE(javascript);
    AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                          javascript);
    RunReportWinExpectingResult(
        expected_report_url, expected_ad_beacon_map,
        std::move(expected_ad_macro_map), std::move(expected_pa_requests),
        /*expected_reporting_latency_timeout=*/false, expected_errors);
  }

  // Runs reportWin() on an already loaded worklet,  verifies the return
  // value and invokes `done_closure` when done. Expects something else to
  // spin the event loop. If `expected_reporting_latency_timeout` is true,
  // the timeout will be (extremely loosely) compared against normal script
  // timeout.
  void RunReportWinExpectingResultAsync(
      mojom::BidderWorklet* bidder_worklet,
      const std::optional<GURL>& expected_report_url,
      const base::flat_map<std::string, GURL>& expected_ad_beacon_map,
      const base::flat_map<std::string, std::string>& expected_ad_macro_map,
      PrivateAggregationRequests expected_pa_requests,
      bool expected_reporting_latency_timeout,
      const std::vector<std::string>& expected_errors,
      base::OnceClosure done_closure) {
    bidder_worklet->ReportWin(
        is_for_additional_bid_, interest_group_name_reporting_id_,
        buyer_reporting_id_, buyer_and_seller_reporting_id_,
        selected_buyer_and_seller_reporting_id_, auction_signals_,
        per_buyer_signals_, direct_from_seller_per_buyer_signals_,
        direct_from_seller_per_buyer_signals_header_ad_slot_,
        direct_from_seller_auction_signals_,
        direct_from_seller_auction_signals_header_ad_slot_, seller_signals_,
        kanon_mode_, bid_is_kanon_, browser_signal_render_url_,
        browser_signal_bid_, browser_signal_bid_currency_,
        browser_signal_highest_scoring_other_bid_,
        browser_signal_highest_scoring_other_bid_currency_,
        browser_signal_made_highest_scoring_other_bid_, browser_signal_ad_cost_,
        browser_signal_modeling_signals_, browser_signal_join_count_,
        browser_signal_recency_report_win_, browser_signal_seller_origin_,
        browser_signal_top_level_seller_origin_,
        browser_signal_reporting_timeout_, data_version_,

        /*trace_id=*/1,
        base::BindOnce(
            [](const std::optional<GURL>& expected_report_url,
               const base::flat_map<std::string, GURL>& expected_ad_beacon_map,
               const base::flat_map<std::string, std::string>&
                   expected_ad_macro_map,
               PrivateAggregationRequests expected_pa_requests,
               bool expected_reporting_latency_timeout,
               std::optional<base::TimeDelta> reporting_timeout,
               const std::vector<std::string>& expected_errors,
               base::OnceClosure done_closure,
               const std::optional<GURL>& report_url,
               const base::flat_map<std::string, GURL>& ad_beacon_map,
               const base::flat_map<std::string, std::string>& ad_macro_map,
               PrivateAggregationRequests pa_requests,
               auction_worklet::mojom::BidderTimingMetricsPtr timing_metrics,
               const std::vector<std::string>& errors) {
              EXPECT_EQ(expected_report_url, report_url);
              EXPECT_EQ(expected_errors, errors);
              EXPECT_EQ(expected_ad_beacon_map, ad_beacon_map);
              EXPECT_EQ(expected_ad_macro_map, ad_macro_map);
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
              std::move(done_closure).Run();
            },
            expected_report_url, expected_ad_beacon_map,
            std::move(expected_ad_macro_map), std::move(expected_pa_requests),
            expected_reporting_latency_timeout,
            browser_signal_reporting_timeout_, expected_errors,
            std::move(done_closure)));
  }

  // Loads and runs a reportWin() with the provided return line, expecting the
  // supplied result.
  void RunReportWinExpectingResult(
      const std::optional<GURL>& expected_report_url,
      const base::flat_map<std::string, GURL>& expected_ad_beacon_map =
          base::flat_map<std::string, GURL>(),
      const base::flat_map<std::string, std::string>& expected_ad_macro_map =
          base::flat_map<std::string, std::string>(),
      PrivateAggregationRequests expected_pa_requests = {},
      bool expected_reporting_latency_timeout = false,
      const std::vector<std::string>& expected_errors =
          std::vector<std::string>()) {
    auto bidder_worklet = CreateWorklet();
    ASSERT_TRUE(bidder_worklet);

    base::RunLoop run_loop;
    RunReportWinExpectingResultAsync(
        bidder_worklet.get(), expected_report_url, expected_ad_beacon_map,
        std::move(expected_ad_macro_map), std::move(expected_pa_requests),
        expected_reporting_latency_timeout, expected_errors,
        run_loop.QuitClosure());
    run_loop.Run();
  }

  // Creates a BidderWorkletNonSharedParams based on test fixture
  // configuration.
  mojom::BidderWorkletNonSharedParamsPtr CreateBidderWorkletNonSharedParams() {
    std::vector<auction_worklet::mojom::KAnonKeyPtr> kanon_keys;
    for (const auto& key : kanon_keys_) {
      kanon_keys.emplace_back(key.Clone());
    }
    return mojom::BidderWorkletNonSharedParams::New(
        interest_group_name_,
        blink::InterestGroup::TrustedBiddingSignalsSlotSizeMode::kNone,
        interest_group_enable_bidding_signals_prioritization_,
        interest_group_priority_vector_, execution_mode_, update_url_,
        interest_group_trusted_bidding_signals_keys_,
        /*max_trusted_bidding_signals_url_length=*/0,
        interest_group_user_bidding_signals_, interest_group_ads_,
        interest_group_ad_components_, std::move(kanon_keys));
  }

  // Creates a BiddingBrowserSignals based on test fixture configuration.
  blink::mojom::BiddingBrowserSignalsPtr CreateBiddingBrowserSignals() {
    return blink::mojom::BiddingBrowserSignals::New(
        browser_signal_join_count_, browser_signal_bid_count_,
        CloneWinList(browser_signal_prev_wins_),
        browser_signal_for_debugging_only_in_cooldown_or_lockout_);
  }

  // Create a BidderWorklet, returning the remote. If `out_bidder_worklet_impl`
  // is non-null, will also stash the actual implementation pointer there.
  // if `url` is empty, uses `interest_group_bidding_url_`.
  mojo::Remote<mojom::BidderWorklet> CreateWorklet(
      GURL url = GURL(),
      bool pause_for_debugger_on_start = false,
      BidderWorklet** out_bidder_worklet_impl = nullptr,
      bool use_alternate_url_loader_factory = false) {
    CHECK(!generate_bid_run_loop_);

    mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory;
    if (use_alternate_url_loader_factory) {
      alternate_url_loader_factory_.Clone(
          url_loader_factory.InitWithNewPipeAndPassReceiver());
    } else {
      url_loader_factory_.Clone(
          url_loader_factory.InitWithNewPipeAndPassReceiver());
    }

    CHECK_EQ(v8_helpers_.size(), shared_storage_hosts_.size());

    auto bidder_worklet_impl = std::make_unique<BidderWorklet>(
        v8_helpers_, std::move(shared_storage_hosts_),
        pause_for_debugger_on_start, std::move(url_loader_factory),
        auction_network_events_handler_.CreateRemote(),
        url.is_empty() ? interest_group_bidding_url_ : url,
        interest_group_wasm_url_, interest_group_trusted_bidding_signals_url_,
        /*trusted_bidding_signals_slot_size_param=*/"", top_window_origin_,
        permissions_policy_state_.Clone(), experiment_group_id_,
        public_key_ ? public_key_.Clone() : nullptr);

    shared_storage_hosts_.resize(NumThreads());

    last_bidder_join_origin_hash_salt_ =
        bidder_worklet_impl->join_origin_hash_salt_for_testing();

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

    if (out_bidder_worklet_impl) {
      *out_bidder_worklet_impl = bidder_worklet_ptr;
    }
    return bidder_worklet;
  }

  scoped_refptr<AuctionV8Helper> v8_helper() { return v8_helpers_[0]; }

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
            ? std::nullopt
            : direct_from_seller_per_buyer_signals_,
        provide_direct_from_seller_signals_late_
            ? std::nullopt
            : direct_from_seller_auction_signals_,
        browser_signal_seller_origin_, browser_signal_top_level_seller_origin_,
        browser_signal_recency_generate_bid_, CreateBiddingBrowserSignals(),
        auction_start_time_, requested_ad_size_, multi_bid_limit_,
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
    bid_finalizer->FinishGenerateBid(
        auction_signals_, per_buyer_signals_, per_buyer_timeout_,
        per_buyer_currency_,
        provide_direct_from_seller_signals_late_
            ? direct_from_seller_per_buyer_signals_
            : std::nullopt,
        direct_from_seller_per_buyer_signals_header_ad_slot_,
        provide_direct_from_seller_signals_late_
            ? direct_from_seller_auction_signals_
            : std::nullopt,
        direct_from_seller_auction_signals_header_ad_slot_);
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
        browser_signal_top_level_seller_origin_,
        browser_signal_recency_generate_bid_, CreateBiddingBrowserSignals(),
        auction_start_time_, requested_ad_size_, multi_bid_limit_,
        /*trace_id=*/1, GenerateBidClientWithCallbacks::CreateNeverCompletes(),
        bid_finalizer.BindNewEndpointAndPassReceiver());
    bidder_worklet->SendPendingSignalsRequests();
    bid_finalizer->FinishGenerateBid(
        auction_signals_, per_buyer_signals_, per_buyer_timeout_,
        per_buyer_currency_,
        /*direct_from_seller_per_buyer_signals=*/std::nullopt,
        /*direct_from_seller_per_buyer_signals_header_ad_slot=*/std::nullopt,
        /*direct_from_seller_auction_signals=*/std::nullopt,
        /*direct_from_seller_auction_signals_header_ad_slot=*/std::nullopt);
  }

  // Create a BidderWorklet and invokes BeginGenerateBid()/FinishGenerateBid(),
  // waiting for the GenerateBid() callback to be invoked. Returns a null
  // Remote on failure.
  mojo::Remote<mojom::BidderWorklet> CreateWorkletAndGenerateBid() {
    mojo::Remote<mojom::BidderWorklet> bidder_worklet = CreateWorklet();
    GenerateBid(bidder_worklet.get());
    generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
    generate_bid_run_loop_->Run();
    generate_bid_run_loop_.reset();
    if (bids_.empty()) {
      return mojo::Remote<mojom::BidderWorklet>();
    }
    return bidder_worklet;
  }

  bool OnlyBidsForKAnonUpdate() {
    if (kanon_mode_ != auction_worklet::mojom::KAnonymityBidMode::kEnforce) {
      return false;
    }
    for (const auto& bid : bids_) {
      if (bid->bid_role != auction_worklet::mojom::BidRole::kUnenforcedKAnon) {
        return false;
      }
    }
    return true;
  }

  void GenerateBidCallback(
      std::vector<mojom::BidderWorkletBidPtr> bids,
      std::optional<uint32_t> data_version,
      const std::optional<GURL>& debug_loss_report_url,
      const std::optional<GURL>& debug_win_report_url,
      std::optional<double> set_priority,
      base::flat_map<std::string,
                     auction_worklet::mojom::PrioritySignalsDoublePtr>
          update_priority_signals_overrides,
      PrivateAggregationRequests pa_requests,
      PrivateAggregationRequests non_kanon_pa_requests,
      RealTimeReportingContributions real_time_contributions,
      mojom::BidderTimingMetricsPtr generate_bid_metrics,
      mojom::GenerateBidDependencyLatenciesPtr
          generate_bid_dependency_latencies,
      mojom::RejectReason reject_reason,
      const std::vector<std::string>& errors) {
    bids_ = std::move(bids);
    data_version_ = data_version;
    bid_debug_loss_report_url_ = debug_loss_report_url;
    bid_debug_win_report_url_ = debug_win_report_url;
    set_priority_ = set_priority;

    update_priority_signals_overrides_.clear();
    for (const auto& override : update_priority_signals_overrides) {
      std::optional<double> value;
      if (override.second) {
        value = override.second->value;
      }
      update_priority_signals_overrides_.emplace(override.first, value);
    }

    pa_requests_ = std::move(pa_requests);
    non_kanon_pa_requests_ = std::move(non_kanon_pa_requests);
    real_time_contributions_ = std::move(real_time_contributions);
    generate_bid_metrics_ = std::move(generate_bid_metrics);
    generate_bid_dependency_latencies_ =
        std::move(generate_bid_dependency_latencies);
    reject_reason_ = reject_reason;
    // Shouldn't have a reject reason if we have bids, unless they are all
    // non-k-anon bids only there to update k-anon data and not determine the
    // actual winner.
    if (!bids_.empty() && !OnlyBidsForKAnonUpdate()) {
      EXPECT_EQ(reject_reason_, mojom::RejectReason::kNotAvailable);
    }
    bid_errors_ = errors;
    generate_bid_run_loop_->Quit();
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

  void AuthorizeKAnonForReporting(
      const GURL& ad_render_url,
      std::optional<std::string> buyer_reporting_id,
      std::optional<std::string> buyer_and_seller_reporting_id,
      std::optional<std::string> selected_buyer_and_seller_reporting_id,
      mojom::BidderWorkletNonSharedParamsPtr& params) {
    params->kanon_keys.emplace_back(auction_worklet::mojom::KAnonKey::New(
        blink::HashedKAnonKeyForAdNameReportingWithoutInterestGroup(
            url::Origin::Create(interest_group_bidding_url_),
            interest_group_name_, interest_group_bidding_url_,
            ad_render_url.spec(), buyer_reporting_id,
            buyer_and_seller_reporting_id,
            selected_buyer_and_seller_reporting_id)));
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
    LOG(WARNING) << "Worklet disconnect with reason: " << description;

    disconnect_reason_ = description;
    if (disconnect_run_loop_) {
      disconnect_run_loop_->Quit();
    }
  }

  std::vector<mojo::StructPtr<blink::mojom::PreviousWin>> CloneWinList(
      const std::vector<mojo::StructPtr<blink::mojom::PreviousWin>>&
          prev_win_list) {
    std::vector<mojo::StructPtr<blink::mojom::PreviousWin>> out;
    for (const auto& prev_win : prev_win_list) {
      out.push_back(prev_win->Clone());
    }
    return out;
  }

  TaskEnvironment task_environment_;

  // Values used to construct the BiddingInterestGroup passed to the
  // BidderWorklet.
  //
  // NOTE: For each new GURL field, GenerateBidLoadCompletionOrder /
  // ReportWinLoadCompletionOrder should be updated.
  std::string interest_group_name_;
  bool interest_group_enable_bidding_signals_prioritization_;
  std::optional<base::flat_map<std::string, double>>
      interest_group_priority_vector_;
  GURL interest_group_bidding_url_ = GURL("https://url.test/");
  url::Origin join_origin_;
  blink::mojom::InterestGroup::ExecutionMode execution_mode_ =
      blink::mojom::InterestGroup::ExecutionMode::kCompatibilityMode;
  std::optional<GURL> interest_group_wasm_url_;
  std::optional<std::string> interest_group_user_bidding_signals_;
  std::vector<blink::InterestGroup::Ad> interest_group_ads_;
  std::optional<std::vector<blink::InterestGroup::Ad>>
      interest_group_ad_components_;
  base::flat_set<auction_worklet::mojom::KAnonKeyPtr> kanon_keys_;
  auction_worklet::mojom::KAnonymityBidMode kanon_mode_ =
      auction_worklet::mojom::KAnonymityBidMode::kNone;
  bool bid_is_kanon_;
  std::optional<GURL> update_url_;
  std::optional<GURL> interest_group_trusted_bidding_signals_url_;
  std::optional<std::vector<std::string>>
      interest_group_trusted_bidding_signals_keys_;
  int browser_signal_join_count_;
  int browser_signal_bid_count_;
  bool browser_signal_for_debugging_only_in_cooldown_or_lockout_;
  base::TimeDelta browser_signal_recency_generate_bid_;
  std::vector<mojo::StructPtr<blink::mojom::PreviousWin>>
      browser_signal_prev_wins_;

  std::optional<std::string> auction_signals_;
  std::optional<std::string> per_buyer_signals_;
  std::optional<GURL> direct_from_seller_per_buyer_signals_;
  std::optional<std::string>
      direct_from_seller_per_buyer_signals_header_ad_slot_;
  std::optional<GURL> direct_from_seller_auction_signals_;
  std::optional<std::string> direct_from_seller_auction_signals_header_ad_slot_;
  std::optional<base::TimeDelta> per_buyer_timeout_;
  std::optional<blink::AdCurrency> per_buyer_currency_;
  url::Origin top_window_origin_;
  mojom::AuctionWorkletPermissionsPolicyStatePtr permissions_policy_state_;
  std::optional<uint16_t> experiment_group_id_;
  mojom::TrustedSignalsPublicKeyPtr public_key_;
  url::Origin browser_signal_seller_origin_;
  std::optional<url::Origin> browser_signal_top_level_seller_origin_;

  bool provide_direct_from_seller_signals_late_ = false;

  std::string seller_signals_;
  // Used for both the output GenerateBid(), and the input of ReportWin().
  std::optional<uint32_t> data_version_;
  GURL browser_signal_render_url_;
  double browser_signal_bid_;
  std::optional<blink::AdCurrency> browser_signal_bid_currency_;
  double browser_signal_highest_scoring_other_bid_;
  std::optional<blink::AdCurrency>
      browser_signal_highest_scoring_other_bid_currency_;
  bool browser_signal_made_highest_scoring_other_bid_;
  std::optional<double> browser_signal_ad_cost_;
  std::optional<uint16_t> browser_signal_modeling_signals_;
  uint8_t browser_signal_recency_report_win_;

  // Used for reportWin();
  std::optional<base::TimeDelta> browser_signal_reporting_timeout_ =
      std::nullopt;
  bool is_for_additional_bid_ = false;
  auction_worklet::mojom::ReportingIdField reporting_id_field_ =
      auction_worklet::mojom::ReportingIdField::kInterestGroupName;
  std::optional<std::string> interest_group_name_reporting_id_;
  std::optional<std::string> buyer_reporting_id_;
  std::optional<std::string> buyer_and_seller_reporting_id_;
  std::optional<std::string> selected_buyer_and_seller_reporting_id_;

  // Use a single constant start time. Only delta times are provided to scripts,
  // relative to the time of the auction, so no need to vary the auction time.
  const base::Time auction_start_time_ = base::Time::Now();

  // This is the requested ad size provided to BeginGenerateBid() by the auction
  // logic. It's piped through to the browserSignals JS object in the
  // buyer's generateBid() function if it is present.
  std::optional<blink::AdSize> requested_ad_size_;

  // How many bids can be returned from multi bid (if on).
  uint16_t multi_bid_limit_ = 1;

  // Reusable run loop for waiting until the GenerateBid() callback has been
  // invoked. It's populated and later cleared by the
  // CreateWorkletAndGenerateBid() series of methods, which wait for a bid to be
  // generated. Callers that need to spin the loop themselves need to populate
  // this directly.
  std::unique_ptr<base::RunLoop> generate_bid_run_loop_;

  // Values passed to the GenerateBidCallback().
  std::vector<mojom::BidderWorkletBidPtr> bids_;
  std::optional<GURL> bid_debug_loss_report_url_;
  std::optional<GURL> bid_debug_win_report_url_;
  std::optional<double> set_priority_;
  // Uses std::optional<double> instead of the Mojo type to be more
  // user-friendly.
  base::flat_map<std::string, std::optional<double>>
      update_priority_signals_overrides_;
  PrivateAggregationRequests pa_requests_;
  PrivateAggregationRequests non_kanon_pa_requests_;
  RealTimeReportingContributions real_time_contributions_;
  mojom::BidderTimingMetricsPtr generate_bid_metrics_;
  mojom::GenerateBidDependencyLatenciesPtr generate_bid_dependency_latencies_;
  mojom::RejectReason reject_reason_ = mojom::RejectReason::kNotAvailable;
  std::vector<std::string> bid_errors_;

  network::TestURLLoaderFactory url_loader_factory_;
  network::TestURLLoaderFactory alternate_url_loader_factory_;

  std::vector<scoped_refptr<AuctionV8Helper>> v8_helpers_;

  std::string last_bidder_join_origin_hash_salt_;

  TestAuctionNetworkEventsHandler auction_network_events_handler_;

  std::vector<mojo::PendingRemote<mojom::AuctionSharedStorageHost>>
      shared_storage_hosts_;

  // Reuseable run loop for disconnection errors.
  std::unique_ptr<base::RunLoop> disconnect_run_loop_;
  std::optional<std::string> disconnect_reason_;

  // Owns all created BidderWorklets - having a ReceiverSet allows them to have
  // a ClosePipeCallback which behaves just like the one in
  // AuctionWorkletServiceImpl, to better match production behavior.
  mojo::UniqueReceiverSet<mojom::BidderWorklet> bidder_worklets_;

  base::test::ScopedFeatureList feature_list_;
};

class BidderWorkletTwoThreadsTest : public BidderWorkletTest {
 private:
  size_t NumThreads() override { return 2u; }
};

class BidderWorkletMultiThreadingTest
    : public BidderWorkletTest,
      public testing::WithParamInterface<size_t> {
 private:
  size_t NumThreads() override { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         BidderWorkletMultiThreadingTest,
                         testing::Values(1, 2),
                         [](const auto& info) {
                           return base::StrCat({info.param == 2
                                                    ? "TwoThreads"
                                                    : "SingleThread"});
                         });

class BidderWorkletCustomAdComponentLimitTest : public BidderWorkletTest {
 public:
  BidderWorkletCustomAdComponentLimitTest() {
    const std::map<std::string, std::string> params = {
        {"FledgeAdComponentLimit", "25"}};
    feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFledgeCustomMaxAuctionAdComponents, params);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

class BidderWorkletMultiBidDisabledTest : public BidderWorkletTest {
 public:
  BidderWorkletMultiBidDisabledTest() {
    feature_list_.InitAndDisableFeature(blink::features::kFledgeMultiBid);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

// kCookieDeprecationFacilitatedTesting disables kFledgeMultiBid.
class BidderWorkletMultiBidAndCookieDeprecationTest : public BidderWorkletTest {
 public:
  BidderWorkletMultiBidAndCookieDeprecationTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kFledgeMultiBid,
                              features::kCookieDeprecationFacilitatedTesting},
        /*disabled_features=*/{});
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

class BidderWorkletCrossOriginTrustedSignalsDisabledTest
    : public BidderWorkletTest {
 public:
  BidderWorkletCrossOriginTrustedSignalsDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(
        blink::features::kFledgePermitCrossOriginTrustedSignals);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test the case the BidderWorklet pipe is closed before invoking the
// GenerateBidCallback. The invocation of the GenerateBidCallback is not
// observed, since the callback is on the pipe that was just closed. There
// should be no Mojo exception due to destroying the creation callback without
// invoking it.
TEST_F(BidderWorkletTest, PipeClosed) {
  base::HistogramTester histogram_tester;
  auto bidder_worklet = CreateWorklet();
  GenerateBidExpectingNeverCompletes(bidder_worklet.get());
  bidder_worklet.reset();
  EXPECT_FALSE(bidder_worklets_.empty());

  // This should not result in a Mojo crash.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(bidder_worklets_.empty());

  // These metrics should get recorded when a bidder worklet is destroyed.
  histogram_tester.ExpectTotalCount(
      "Ads.InterestGroup.Auction.BidderWorkletIsolateTotalHeapSizeKilobytes",
      1);
  histogram_tester.ExpectTotalCount(
      "Ads.InterestGroup.Auction.BidderWorkletIsolateUsedHeapSizeKilobytes", 1);
}

TEST_F(BidderWorkletTest, NetworkError) {
  url_loader_factory_.AddResponse(interest_group_bidding_url_.spec(),
                                  CreateBasicGenerateBidScript(),
                                  net::HTTP_NOT_FOUND);
  auto bidder_worklet = CreateWorklet();
  GenerateBidExpectingNeverCompletes(bidder_worklet.get());
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
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "null", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  // Wait until idle to ensure all requests have been observed within the
  // `auction_network_events_handler_`.
  task_environment_.RunUntilIdle();
  EXPECT_THAT(auction_network_events_handler_.GetObservedRequests(),
              testing::ElementsAre("Sent URL: https://url.test/",
                                   "Received URL: https://url.test/",
                                   "Completion Status: net::OK"));

  // Explicitly setting an undefined ad value acts just like not setting an ad
  // value.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: globalThis.not_defined, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "null", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  // Make sure "ad" can be of a variety of JS object types.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, R"(["ad"])", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: {a:1,b:null}, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon,
          R"({"a":1,"b":null})", 1, /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: [2.5,[]], bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "[2.5,[]]", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: -5, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "-5", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
  // Some values that can't be represented in JSON become null.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: 0/0, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "null", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: [globalThis.not_defined], bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "[null]", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: [function() {return 1;}], bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "[null]", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  // Other values JSON can't represent result in failing instead of null.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: function() {return 1;}, bid:1, render:"https://response.test/"})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ generateBid() bid has invalid ad value."});

  // JSON extraction failing to terminate is also handled.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: {get field() { while(true); } },
          bid:1, render:"https://response.test/"})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ generateBid() serializing bid 'ad' value to JSON "
       "timed out."});

  // Make sure recursive structures aren't allowed in ad field.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(
        function generateBid() {
          var a = [];
          a[0] = a;
          return {ad: a, bid:1, render:"https://response.test/"};
        }
      )",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ generateBid() bid has invalid ad value."});
}

TEST_F(BidderWorkletTest, GenerateBidReturnValueBid) {
  // Undefined / an empty return statement and null are treated as not bidding.
  RunGenerateBidWithReturnValueExpectingResult(
      "",
      /*expected_bids=*/mojom::BidderWorkletBidPtr());
  EXPECT_FALSE(generate_bid_metrics_->script_timed_out);
  RunGenerateBidWithReturnValueExpectingResult(
      "null",
      /*expected_bids=*/mojom::BidderWorkletBidPtr());
  EXPECT_FALSE(generate_bid_metrics_->script_timed_out);

  // Missing bid value is also treated as not bidding, since setBid(null)
  // is basically the same as setBid({}) in WebIDL.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", render:"https://response.test/"})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr());
  EXPECT_FALSE(generate_bid_metrics_->script_timed_out);

  // Valid positive bid values.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
  EXPECT_FALSE(generate_bid_metrics_->script_timed_out);
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid:1.5, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 1.5,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid:2, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon,

          "\"ad\"", 2, /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid:0.001, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon,

          "\"ad\"", 0.001, /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  // Bids <= 0.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:0, render:"https://response.test/"})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr());
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:-10, render:"https://response.test/"})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr());
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:-1.5, render:"https://response.test/"})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr());

  // Infinite and NaN bid.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1/0, render:"https://response.test/"})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ generateBid() Converting field 'bid' to a Number did "
       "not produce a finite double."});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:-1/0, render:"https://response.test/"})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ generateBid() Converting field 'bid' to a Number did "
       "not produce a finite double."});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:0/0, render:"https://response.test/"})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ generateBid() Converting field 'bid' to a Number did "
       "not produce a finite double."});

  // Non-numeric bid. JS just makes these into numbers. At least [1,2] turns
  // into NaN...
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:"1", render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "[\"ad\"]", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:true, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "[\"ad\"]", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:[1], render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "[\"ad\"]", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:[1,2], render:"https://response.test/"})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ generateBid() Converting field 'bid' to a Number did "
       "not produce a finite double."});
  EXPECT_FALSE(generate_bid_metrics_->script_timed_out);

  // Test with bid with a non-terminating conversion.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          /*raw_return_value=*/"",
          /*extra_code=*/R"(
            return {ad: "ad", bid:{valueOf:() => {while(true) {} } },
                    render:"https://response.test/"};
          )"),
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ generateBid() Converting field 'bid' to Number timed"
       " out."});
  EXPECT_TRUE(generate_bid_metrics_->script_timed_out);

  // Test with bid with a non-terminating getter.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          /*raw_return_value=*/"",
          /*extra_code=*/R"(
            return {ad: "ad", get bid() {while(true) {} },
                    render:"https://response.test/"};
          )"),
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ generateBid() Execution timed out trying to access "
       "field 'bid'."});
  EXPECT_TRUE(generate_bid_metrics_->script_timed_out);
}

TEST_F(BidderWorkletTest, GenerateBidReturnBidCurrencyExpectUnspecified) {
  // Tests of returning various currency and comparing against unspecifed
  // expectation.

  // Explicitly specified correct one.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid:1, bidCurrency: "USD",
          render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 1,
          blink::AdCurrency::From("USD"),
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
  EXPECT_EQ(reject_reason_, mojom::RejectReason::kNotAvailable);

  // Not specifying currency explicitly results in nullopt tag.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid:1,
          render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
  EXPECT_EQ(reject_reason_, mojom::RejectReason::kNotAvailable);

  // Trying to explicitly specify kUnspecifiedAdCurrency fails.
  RunGenerateBidWithReturnValueExpectingResult(
      base::StringPrintf(
          R"({ad: "ad", bid:1, bidCurrency:"%s",
          render:"https://response.test/"})",
          blink::kUnspecifiedAdCurrency),
      mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ generateBid() bidCurrency of '???' "
       "is not a currency code."});
  EXPECT_EQ(reject_reason_, mojom::RejectReason::kWrongGenerateBidCurrency);

  // Expect currency codes to be 3 characters.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid:1, bidCurrency:"USSD",
          render:"https://response.test/"})",
      mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ generateBid() bidCurrency of 'USSD' "
       "is not a currency code."});
  EXPECT_EQ(reject_reason_, mojom::RejectReason::kWrongGenerateBidCurrency);

  // Expect currency code to be 3 uppercase characters.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid:1, bidCurrency:"usd",
          render:"https://response.test/"})",
      mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ generateBid() bidCurrency of 'usd' "
       "is not a currency code."});
  EXPECT_EQ(reject_reason_, mojom::RejectReason::kWrongGenerateBidCurrency);
}

TEST_F(BidderWorkletTest, GenerateBidReturnBidCurrencyExpectCAD) {
  // Tests of returning various currency and comparing against expected `CAD`.
  //
  per_buyer_currency_ = blink::AdCurrency::From("CAD");

  // Explicitly specified correct one.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid:1, bidCurrency: "CAD",
          render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 1,
          blink::AdCurrency::From("CAD"),
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
  EXPECT_EQ(reject_reason_, mojom::RejectReason::kNotAvailable);

  // Explicitly specified incorrect one.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid:1, bidCurrency: "USD",
          render:"https://response.test/"})",
      mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ generateBid() bidCurrency mismatch; "
       "returned 'USD', expected 'CAD'."});
  EXPECT_EQ(reject_reason_, mojom::RejectReason::kWrongGenerateBidCurrency);

  // Not specifying currency explicitly results in std::nullopt, which matches
  // for compatibility reasons.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid:1,
          render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
  EXPECT_EQ(reject_reason_, mojom::RejectReason::kNotAvailable);
}

TEST_F(BidderWorkletTest, GenerateBidReturnValueUrl) {
  // Missing render field.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ generateBid() 'render' is required when making a "
       "bid."});

  // Missing ad and render fields.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({bid:1})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ generateBid() 'render' is required when making a "
       "bid."});

  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "[\"ad\"]", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  // Missing value with bid <= 0 is considered a valid no-bid case.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({bid:0})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr());
  RunGenerateBidWithReturnValueExpectingResult(
      R"({bid:-1})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr());

  // Disallowed render schemes.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:"http://response.test/"})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ generateBid() bid render URL 'http://response.test/' "
       "isn't a valid https:// URL."});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:"chrome-extension://response.test/"})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ generateBid() bid render URL "
       "'chrome-extension://response.test/' isn't a valid https:// URL."});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:"about:blank"})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ generateBid() bid render URL 'about:blank' isn't a "
       "valid https:// URL."});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:"data:,foo"})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ generateBid() bid render URL 'data:,foo' isn't a "
       "valid https:// URL."});

  // Invalid render URLs.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:"test"})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ generateBid() bid render URL '' isn't a valid "
       "https:// URL."});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:"http://"})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ generateBid() bid render URL 'http:' isn't a valid "
       "https:// URL."});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:["http://response.test/"]})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ generateBid() 'render': Required field 'url' "
       "is undefined."});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: ["ad"], bid:1, render:9})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ generateBid() 'render': Value passed as dictionary "
       "is neither object, null, nor undefined."});
}

// Check that accessing `renderUrl` of an entry in the ads array displays a
// warning. Also checks that renderUrl works as expected.
//
// TODO(crbug.com/40266734): Remove this test when the field itself is
// removed.
TEST_F(BidderWorkletTest, AdsRenderUrlDeprecationWarning) {
  ScopedInspectorSupport inspector_support(v8_helper().get());

  interest_group_ads_.emplace_back(GURL("https://response2.test/"),
                                   /*metadata=*/std::nullopt);

  AddJavascriptResponse(
      &url_loader_factory_, interest_group_bidding_url_,
      CreateGenerateBidScript(
          R"({ad: ["ad"], bid:1, render:"https://response.test/"})",
          R"(if (interestGroup.ads[0].renderUrl !== "https://response.test/" ||
             interestGroup.ads[1].renderUrl !== "https://response2.test/") {
           return;
         })"));

  interest_group_enable_bidding_signals_prioritization_ = true;

  BidderWorklet* worklet_impl;
  auto worklet =
      CreateWorklet(interest_group_bidding_url_,
                    /*pause_for_debugger_on_start=*/false, &worklet_impl);

  int id = worklet_impl->context_group_ids_for_testing()[0];
  TestChannel* channel =
      inspector_support.ConnectDebuggerSessionAndRuntimeEnable(id);

  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  GenerateBid(worklet.get());
  generate_bid_run_loop_->Run();
  // Each access of a different `renderUrl` value should have generated a
  // separate warning.
  channel->WaitForAndValidateConsoleMessage(
      "warning", /*json_args=*/
      "[{\"type\":\"string\", "
      "\"value\":\"AuctionAd.renderUrl is deprecated. Please use "
      "AuctionAd.renderURL instead.\"}]",
      /*stack_trace_size=*/1, /*function=*/"generateBid",
      interest_group_bidding_url_, /*line_number=*/4);

  channel->WaitForAndValidateConsoleMessage(
      "warning", /*json_args=*/
      "[{\"type\":\"string\", "
      "\"value\":\"AuctionAd.renderUrl is deprecated. Please use "
      "AuctionAd.renderURL instead.\"}]",
      /*stack_trace_size=*/1, /*function=*/"generateBid",
      interest_group_bidding_url_, /*line_number=*/5);

  channel->ExpectNoMoreConsoleEvents();

  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(1, bids_[0]->bid);
  bids_.clear();
  generate_bid_run_loop_.reset();
}

// Check that accessing `renderURL` of an entry in the ads array does not
// display a warning.
//
// TODO(crbug.com/40266734): Remove this test when renderUrl is removed.
TEST_F(BidderWorkletTest, AdsRenderUrlNoDeprecationWarning) {
  ScopedInspectorSupport inspector_support(v8_helper().get());

  AddJavascriptResponse(
      &url_loader_factory_, interest_group_bidding_url_,
      CreateGenerateBidScript(
          R"({ad: ["ad"], bid:1, render:"https://response.test/"})",
          "if (!interestGroup.ads[0].renderURL) return;"));

  interest_group_enable_bidding_signals_prioritization_ = true;

  BidderWorklet* worklet_impl;
  auto worklet =
      CreateWorklet(interest_group_bidding_url_,
                    /*pause_for_debugger_on_start=*/false, &worklet_impl);

  int id = worklet_impl->context_group_ids_for_testing()[0];
  TestChannel* channel =
      inspector_support.ConnectDebuggerSessionAndRuntimeEnable(id);

  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  GenerateBid(worklet.get());
  generate_bid_run_loop_->Run();

  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(1, bids_[0]->bid);

  channel->ExpectNoMoreConsoleEvents();
}

TEST_F(BidderWorkletTest, GenerateBidReturnValueAdComponents) {
  // ----------------------
  // No adComponents in IG.
  // ----------------------

  interest_group_ad_components_ = std::nullopt;

  // Auction should fail if adComponents in return value is an array.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad",
        bid:1,
        render:"https://response.test/",
        adComponents:["http://response.test/"]})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ generateBid() bid contains adComponents but "
       "InterestGroup has no adComponents."});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad",
        bid:1,
        render:"https://response.test/",
        adComponents:[]})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ generateBid() bid contains adComponents but "
       "InterestGroup has no adComponents."});

  // Auction should fail if adComponents in return value is an unexpected type.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad",
        bid:1,
        render:"https://response.test/",
        adComponents:5})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ generateBid() Sequence field 'adComponents' must be "
       "an Object."});

  // Not present adComponents field should result in success; null is a type
  // error.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad",
        bid:1,
        render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad",
        bid:1,
        render:"https://response.test/",
        adComponents:null})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ generateBid() Sequence field 'adComponents' must be "
       "an Object."});

  SetDefaultParameters();

  // -----------------------------
  // Non-empty adComponents in IG.
  // -----------------------------

  // Empty adComponents in results is allowed.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad",
        bid:1,
        render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  // Auction should fail if adComponents in return value is an unexpected type.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad",
        bid:1,
        render:"https://response.test/",
        adComponents:5})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ generateBid() Sequence field 'adComponents' must be "
       "an Object."});

  // Unexpected value types in adComponents should fail.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad",
        bid:1,
        render:"https://response.test/",
        adComponents:[{}]})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ generateBid() adComponents entry: Required field "
       "'url' is undefined."});

  // By default up to 40 values in the output adComponents output array are
  // allowed (And they can all be the same URL).
  ASSERT_EQ(blink::MaxAdAuctionAdComponents(), 40u)
      << "Unexpected value of MaxAdAuctionAdComponents()";
  std::vector<blink::AdDescriptor> expected_descriptors(
      40u, blink::AdDescriptor(GURL("https://ad_component.test/")));
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
          "https://ad_component.test/" /* 22 */,
          "https://ad_component.test/" /* 23 */,
          "https://ad_component.test/" /* 24 */,
          "https://ad_component.test/" /* 25 */,
          "https://ad_component.test/" /* 26 */,
          "https://ad_component.test/" /* 27 */,
          "https://ad_component.test/" /* 28 */,
          "https://ad_component.test/" /* 29 */,
          "https://ad_component.test/" /* 30 */,
          "https://ad_component.test/" /* 31 */,
          "https://ad_component.test/" /* 32 */,
          "https://ad_component.test/" /* 33 */,
          "https://ad_component.test/" /* 34 */,
          "https://ad_component.test/" /* 35 */,
          "https://ad_component.test/" /* 36 */,
          "https://ad_component.test/" /* 37 */,
          "https://ad_component.test/" /* 38 */,
          "https://ad_component.test/" /* 39 */,
          "https://ad_component.test/" /* 40 */,
        ]})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          std::move(expected_descriptors),
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  // Results with 41 or more values are rejected.
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
          "https://ad_component.test/" /* 22 */,
          "https://ad_component.test/" /* 23 */,
          "https://ad_component.test/" /* 24 */,
          "https://ad_component.test/" /* 25 */,
          "https://ad_component.test/" /* 26 */,
          "https://ad_component.test/" /* 27 */,
          "https://ad_component.test/" /* 28 */,
          "https://ad_component.test/" /* 29 */,
          "https://ad_component.test/" /* 30 */,
          "https://ad_component.test/" /* 31 */,
          "https://ad_component.test/" /* 32 */,
          "https://ad_component.test/" /* 33 */,
          "https://ad_component.test/" /* 34 */,
          "https://ad_component.test/" /* 35 */,
          "https://ad_component.test/" /* 36 */,
          "https://ad_component.test/" /* 37 */,
          "https://ad_component.test/" /* 38 */,
          "https://ad_component.test/" /* 39 */,
          "https://ad_component.test/" /* 40 */,
          "https://ad_component.test/" /* 41 */,
        ]})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ generateBid() bid adComponents with over 40 "
       "items."});
}

TEST_F(BidderWorkletCustomAdComponentLimitTest, AdComponentsLimit) {
  // The fixture sets the limit to max of 25 output adComponents.
  ASSERT_EQ(blink::MaxAdAuctionAdComponents(), 25u)
      << "Unexpected value of MaxAdAuctionAdComponents()";
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
          "https://ad_component.test/" /* 22 */,
          "https://ad_component.test/" /* 23 */,
          "https://ad_component.test/" /* 24 */,
          "https://ad_component.test/" /* 25 */
        ]})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/
          std::vector<blink::AdDescriptor>{
              blink::AdDescriptor(GURL("https://ad_component.test/")) /* 1 */,
              blink::AdDescriptor(GURL("https://ad_component.test/")) /* 2 */,
              blink::AdDescriptor(GURL("https://ad_component.test/")) /* 3 */,
              blink::AdDescriptor(GURL("https://ad_component.test/")) /* 4 */,
              blink::AdDescriptor(GURL("https://ad_component.test/")) /* 5 */,
              blink::AdDescriptor(GURL("https://ad_component.test/")) /* 6 */,
              blink::AdDescriptor(GURL("https://ad_component.test/")) /* 7 */,
              blink::AdDescriptor(GURL("https://ad_component.test/")) /* 8 */,
              blink::AdDescriptor(GURL("https://ad_component.test/")) /* 9 */,
              blink::AdDescriptor(GURL("https://ad_component.test/")) /* 10 */,
              blink::AdDescriptor(GURL("https://ad_component.test/")) /* 11 */,
              blink::AdDescriptor(GURL("https://ad_component.test/")) /* 12 */,
              blink::AdDescriptor(GURL("https://ad_component.test/")) /* 13 */,
              blink::AdDescriptor(GURL("https://ad_component.test/")) /* 14 */,
              blink::AdDescriptor(GURL("https://ad_component.test/")) /* 15 */,
              blink::AdDescriptor(GURL("https://ad_component.test/")) /* 16 */,
              blink::AdDescriptor(GURL("https://ad_component.test/")) /* 17 */,
              blink::AdDescriptor(GURL("https://ad_component.test/")) /* 18 */,
              blink::AdDescriptor(GURL("https://ad_component.test/")) /* 19 */,
              blink::AdDescriptor(GURL("https://ad_component.test/")) /* 20 */,
              blink::AdDescriptor(GURL("https://ad_component.test/")) /* 21 */,
              blink::AdDescriptor(GURL("https://ad_component.test/")) /* 22 */,
              blink::AdDescriptor(GURL("https://ad_component.test/")) /* 23 */,
              blink::AdDescriptor(GURL("https://ad_component.test/")) /* 24 */,
              blink::AdDescriptor(GURL("https://ad_component.test/")) /* 25 */,
          },
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  // Results with 26 or more values are rejected.
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
          "https://ad_component.test/" /* 21 */,
          "https://ad_component.test/" /* 22 */,
          "https://ad_component.test/" /* 23 */,
          "https://ad_component.test/" /* 24 */,
          "https://ad_component.test/" /* 25 */,
          "https://ad_component.test/" /* 26 */
        ]})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ generateBid() bid adComponents with over 25 "
       "items."});
}

// Check that accessing `renderUrl` of an entry in the adComponents array
// displays a warning. Also checks that renderUrl works as expected.
//
// TODO(crbug.com/40266734): Remove this test when the field itself is
// removed.
TEST_F(BidderWorkletTest, AdComponentsRenderUrlDeprecationWarning) {
  ScopedInspectorSupport inspector_support(v8_helper().get());

  interest_group_ad_components_.emplace();
  interest_group_ad_components_->emplace_back(GURL("https://component1.test/"),
                                              /*metadata=*/std::nullopt);
  interest_group_ad_components_->emplace_back(GURL("https://component2.test/"),
                                              /*metadata=*/std::nullopt);

  AddJavascriptResponse(
      &url_loader_factory_, interest_group_bidding_url_,
      CreateGenerateBidScript(
          R"({ad: ["ad"], bid:1, render:"https://response.test/"})",
          R"(if (interestGroup.adComponents[0].renderUrl !==
                 "https://component1.test/" ||
             interestGroup.adComponents[1].renderUrl !==
                 "https://component2.test/") {
           return;
         })"));

  interest_group_enable_bidding_signals_prioritization_ = true;

  BidderWorklet* worklet_impl;
  auto worklet =
      CreateWorklet(interest_group_bidding_url_,
                    /*pause_for_debugger_on_start=*/false, &worklet_impl);

  int id = worklet_impl->context_group_ids_for_testing()[0];
  TestChannel* channel =
      inspector_support.ConnectDebuggerSessionAndRuntimeEnable(id);

  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  GenerateBid(worklet.get());
  generate_bid_run_loop_->Run();
  // Each access of a different `renderUrl` value should have generated a
  // separate warning.
  channel->WaitForAndValidateConsoleMessage(
      "warning", /*json_args=*/
      "[{\"type\":\"string\", "
      "\"value\":\"AuctionAd.renderUrl is deprecated. Please use "
      "AuctionAd.renderURL instead.\"}]",
      /*stack_trace_size=*/1, /*function=*/"generateBid",
      interest_group_bidding_url_, /*line_number=*/4);

  channel->WaitForAndValidateConsoleMessage(
      "warning", /*json_args=*/
      "[{\"type\":\"string\", "
      "\"value\":\"AuctionAd.renderUrl is deprecated. Please use "
      "AuctionAd.renderURL instead.\"}]",
      /*stack_trace_size=*/1, /*function=*/"generateBid",
      interest_group_bidding_url_, /*line_number=*/6);

  channel->ExpectNoMoreConsoleEvents();

  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(1, bids_[0]->bid);
  bids_.clear();
  generate_bid_run_loop_.reset();
}

// Check that accessing `renderURL` of an entry in the ads array does not
// display a warning.
//
// TODO(crbug.com/40266734): Remove this test when renderUrl is removed.
TEST_F(BidderWorkletTest, AdComponentsRenderUrlNoDeprecationWarning) {
  ScopedInspectorSupport inspector_support(v8_helper().get());

  interest_group_ad_components_.emplace();
  interest_group_ad_components_->emplace_back(GURL("https://component1.test/"),
                                              /*metadata=*/std::nullopt);

  AddJavascriptResponse(
      &url_loader_factory_, interest_group_bidding_url_,
      CreateGenerateBidScript(
          R"({ad: ["ad"], bid:1, render:"https://response.test/"})",
          "if (!interestGroup.adComponents[0].renderURL) return;"));

  interest_group_enable_bidding_signals_prioritization_ = true;

  BidderWorklet* worklet_impl;
  auto worklet =
      CreateWorklet(interest_group_bidding_url_,
                    /*pause_for_debugger_on_start=*/false, &worklet_impl);

  int id = worklet_impl->context_group_ids_for_testing()[0];
  TestChannel* channel =
      inspector_support.ConnectDebuggerSessionAndRuntimeEnable(id);

  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  GenerateBid(worklet.get());
  generate_bid_run_loop_->Run();

  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(1, bids_[0]->bid);
  channel->ExpectNoMoreConsoleEvents();
}

TEST_F(BidderWorkletTest, GenerateBidModelingSignals) {
  const std::string kGenerateBidBody =
      R"({bid:1, render:"https://response.test/", modelingSignals: 123})";

  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "null", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/123u, base::TimeDelta()));
}

TEST_F(BidderWorkletTest, GenerateBidModelingSignalsNaN) {
  const std::string kGenerateBidBody =
      R"({bid:1, render:"https://response.test/", modelingSignals: NaN})";

  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "null", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
}

TEST_F(BidderWorkletTest, GenerateBidModelingSignalsInfinity) {
  const std::string kGenerateBidBody =
      R"({bid:1, render:"https://response.test/", modelingSignals: Infinity})";

  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "null", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
}

TEST_F(BidderWorkletTest, GenerateBidModelingSignalsNegative) {
  const std::string kGenerateBidBody =
      R"({bid:1, render:"https://response.test/", modelingSignals: -1})";

  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "null", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
}

TEST_F(BidderWorkletTest, GenerateBidModelingSignalsNegativeInfinity) {
  const std::string kGenerateBidBody =
      R"({bid:1, render:"https://response.test/", modelingSignals: -Infinity})";

  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "null", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
}

TEST_F(BidderWorkletTest, GenerateBidModelingSignalsNegativeZero) {
  const std::string kGenerateBidBody =
      R"({bid:1, render:"https://response.test/", modelingSignals: -0})";

  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "null", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
}

TEST_F(BidderWorkletTest, GenerateBidModelingSignalsPositiveZero) {
  const std::string kGenerateBidBody =
      R"({bid:1, render:"https://response.test/", modelingSignals: 0})";

  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "null", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/0u, base::TimeDelta()));
}

TEST_F(BidderWorkletTest, GenerateBidModelingSignalsNonInteger) {
  const std::string kGenerateBidBody =
      R"({bid:1, render:"https://response.test/", modelingSignals: 123.5})";

  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "null", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/123u, base::TimeDelta()));
}

TEST_F(BidderWorkletTest, GenerateBidModelingSignalsTooLarge) {
  const std::string kGenerateBidBody =
      R"({bid:1, render:"https://response.test/", modelingSignals: 4096})";

  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "null", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
}

TEST_F(BidderWorkletTest, GenerateBidModelingSignalsAlmostTooLarge) {
  const std::string kGenerateBidBody =
      R"({bid:1, render:"https://response.test/", modelingSignals: 4095.9})";

  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "null", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/4095, base::TimeDelta()));
}

TEST_F(BidderWorkletTest, GenerateBidReturnValueInvalid) {
  // Valid JS, but missing function.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(
        function someOtherFunction() {
          return {ad: ["ad"], bid:1, render:"https://response.test/"};
        }
      )",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ `generateBid` is not a function."});
  RunGenerateBidWithJavascriptExpectingResult(
      "", /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ `generateBid` is not a function."});
  RunGenerateBidWithJavascriptExpectingResult(
      "5", /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ `generateBid` is not a function."});

  // Throw exception.
  RunGenerateBidWithJavascriptExpectingResult(
      "shrimp", /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
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
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
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
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/:5 Uncaught TypeError: bid has invalid ad value."});

  // Timeouts can also happen when serializing ad field to json.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(
        function generateBid() {
          var a = {
            get field() { while(true); }
          }
          setBid({ad: a, bid:1, render:"https://response.test/"});
          return {};
        }
      )",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ execution of `generateBid` timed out."});

  // --------
  // Vary bid
  // --------

  // Non-numeric bid.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({ad: ["ad"], bid:"boo", render:"https://response.test/"});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/:2 Uncaught TypeError: Converting field 'bid' to a "
       "Number did not produce a finite double."});
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({ad: ["ad"], bid:[1,2], render:"https://response.test/"});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/:2 Uncaught TypeError: Converting field 'bid' to a "
       "Number did not produce a finite double."});

  // ---------
  // Vary URL.
  // ---------

  // Disallowed render schemes.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({ad: ["ad"], bid:1, render:"http://response.test/"});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/:2 Uncaught TypeError: bid render URL "
       "'http://response.test/' isn't a valid https:// URL."});
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({ad: ["ad"],
                 bid:1,
                 render:"chrome-extension://response.test/"});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/:2 Uncaught TypeError: bid render URL "
       "'chrome-extension://response.test/' isn't a valid https:// URL."});
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({ad: ["ad"], bid:1, render:"about:blank"});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/:2 Uncaught TypeError: bid render URL 'about:blank' "
       "isn't a valid https:// URL."});
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({ad: ["ad"], bid:1, render:"data:,foo"});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/:2 Uncaught TypeError: bid render URL 'data:,foo' "
       "isn't a valid https:// URL."});

  // Invalid render URLs.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({ad: ["ad"], bid:1, render:"test"});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/:2 Uncaught TypeError: bid render URL '' isn't a "
       "valid https:// URL."});
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({ad: ["ad"], bid:1, render:"http://"});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/:2 Uncaught TypeError: bid render URL 'http:' isn't a "
       "valid https:// URL."});
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({ad: ["ad"], bid:1, render:["http://response.test/"]});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/:2 Uncaught TypeError: 'render': Required field 'url' "
       "is undefined."});
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({ad: ["ad"], bid:1, render:9});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/:2 Uncaught TypeError: 'render': Value passed as "
       "dictionary is neither object, null, nor undefined."});

  // ----------------------
  // No adComponents in IG.
  // ----------------------

  interest_group_ad_components_ = std::nullopt;

  // Auction should fail if adComponents in return value is an array.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({ad: "ad",
                 bid:1,
                 render:"https://response.test/",
                 adComponents:["http://response.test/"]});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
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
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
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
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/:2 Uncaught TypeError: Sequence field 'adComponents' "
       "must be an Object."});

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
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/:2 Uncaught TypeError: Sequence field 'adComponents' "
       "must be an Object."});

  // Unexpected value types in adComponents should fail.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({ad: "ad",
                 bid:1,
                 render:"https://response.test/",
                 adComponents:[{}]});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/:2 Uncaught TypeError: adComponents entry: Required "
       "field 'url' is undefined."});

  // Up to 20 values in the output adComponents output array are allowed (And
  // they can all be the same URL).
  ASSERT_EQ(blink::MaxAdAuctionAdComponents(), 40u)
      << "Unexpected value of MaxAdAuctionAdComponents";

  // Results with 41 or more values are rejected.
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
                   "https://ad_component.test/" /* 22 */,
                   "https://ad_component.test/" /* 23 */,
                   "https://ad_component.test/" /* 24 */,
                   "https://ad_component.test/" /* 25 */,
                   "https://ad_component.test/" /* 26 */,
                   "https://ad_component.test/" /* 27 */,
                   "https://ad_component.test/" /* 28 */,
                   "https://ad_component.test/" /* 29 */,
                   "https://ad_component.test/" /* 30 */,
                   "https://ad_component.test/" /* 31 */,
                   "https://ad_component.test/" /* 32 */,
                   "https://ad_component.test/" /* 33 */,
                   "https://ad_component.test/" /* 34 */,
                   "https://ad_component.test/" /* 35 */,
                   "https://ad_component.test/" /* 36 */,
                   "https://ad_component.test/" /* 37 */,
                   "https://ad_component.test/" /* 38 */,
                   "https://ad_component.test/" /* 39 */,
                   "https://ad_component.test/" /* 40 */,
                   "https://ad_component.test/" /* 41 */,
        ]});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/:2 Uncaught TypeError: bid adComponents with over 40 "
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
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/:2 Uncaught TypeError: Value passed as dictionary "
       "is neither object, null, nor undefined."});

  // Missing or invalid bid value.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({bid:"a", render:"https://response.test/"});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/:2 Uncaught TypeError: Converting field 'bid' to a "
       "Number did not produce a finite double."});
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({ad: ["ad"], render:"https://response.test/"});
         return {ad: "actually_reached", bid: 4,
                 render:"https://response.test/2"};
       })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ generateBid() bid render URL "
       "'https://response.test/2' isn't one of the registered creative URLs."});
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({ad: ["ad"], bid:"a"});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/:2 Uncaught TypeError: Converting field 'bid' to a "
       "Number did not produce a finite double."});
  // Setting a valid bid with setBid(), followed by setting an invalid bid that
  // throws, should result in no bid being set.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({bid:1, render:"https://response.test/"});
         setBid({ad: ["ad"], bid:"1/0", render:"https://response.test/"});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/:3 Uncaught TypeError: Converting field 'bid' to a "
       "Number did not produce a finite double."});

  // An exception in a coercion invoked by setBid().
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         let o = { a: 123 };
         try {
           setBid({bid:{valueOf: () => { throw o }},
                  render:"https://response.test/"});
         } catch (e) {
           if (e === o) {
             setBid({bid:1, render:"https://response.test/"});
           }
           throw "something";
         }
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bids=*/
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "null", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/:10 Uncaught something."});
}

TEST_F(BidderWorkletTest, GenerateBidSetBidNonTermConversion) {
  // A setBid() that hits a timeout in value conversion.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({bid:{valueOf: () => { while(true) {} } },
                 render:"https://response.test/"});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ execution of `generateBid` timed out."});

  // Variant that tries to catch it.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         try { setBid({bid:{valueOf: () => { while(true) {} } },
               render:"https://response.test/"});
         } catch (e) {}
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ execution of `generateBid` timed out."});

  // A setBid() that hits a timeout in a field getter.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({ get bid() { while(true) {} },
                 render:"https://response.test/"});
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ execution of `generateBid` timed out."});

  // Non-terminating conversion inside a field of the render dictionary.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid({bid: 1,
                 render: { get url() { while(true) {} } } });
         return {ad: "not_reached", bid: 4, render:"https://response.test/2"};
       })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ execution of `generateBid` timed out."});
}

TEST_F(BidderWorkletMultiBidDisabledTest, GenerateBidMultiBid) {
  RunGenerateBidWithReturnValueExpectingResult(
      R"([{ad: "ad", bid: 1,
          render: "https://response.test/"}])",
      /*expected_bids=*/mojom::BidderWorkletBidPtr());
}

// Cookie disabling trial forces multibid off.
TEST_F(BidderWorkletMultiBidAndCookieDeprecationTest, GenerateBidMultiBid) {
  RunGenerateBidWithReturnValueExpectingResult(
      R"([{ad: "ad", bid: 1,
          render: "https://response.test/"}])",
      /*expected_bids=*/mojom::BidderWorkletBidPtr());
}

// Make sure that fields that are only available with multibid on aren't
// read when its off.
TEST_F(BidderWorkletMultiBidDisabledTest, ComponentTargetFieldsOnlyMultiBid) {
  auto expected_bid = mojom::BidderWorkletBid::New(
      auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 5,
      /*bid_currency=*/std::nullopt,
      /*ad_cost=*/std::nullopt,
      blink::AdDescriptor(GURL("https://response.test/")),
      /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
      /*ad_component_descriptors=*/std::nullopt,
      /*modeling_signals=*/std::nullopt, base::TimeDelta());

  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid: 5,
          render: {url: "https://response.test/"},
          get numMandatoryAdComponents() {
            throw 'used numMandatoryAdComponents';
          }
      })",
      expected_bid->Clone());

  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid: 5,
          render: {url: "https://response.test/"},
          get targetNumAdComponents() {
            throw 'used targetNumAdComponents';
          }
      })",
      expected_bid->Clone());
}

// Make sure that fields that are only available with multibid on aren't
// read when its forced off by kCookieDeprecationFacilitatedTesting.
TEST_F(BidderWorkletMultiBidAndCookieDeprecationTest,
       ComponentTargetFieldsOnlyMultiBid) {
  auto expected_bid = mojom::BidderWorkletBid::New(
      auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 5,
      /*bid_currency=*/std::nullopt,
      /*ad_cost=*/std::nullopt,
      blink::AdDescriptor(GURL("https://response.test/")),
      /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
      /*ad_component_descriptors=*/std::nullopt,
      /*modeling_signals=*/std::nullopt, base::TimeDelta());

  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid: 5,
          render: {url: "https://response.test/"},
          get numMandatoryAdComponents() {
            throw 'used numMandatoryAdComponents';
          }
      })",
      expected_bid->Clone());

  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid: 5,
          render: {url: "https://response.test/"},
          get targetNumAdComponents() {
            throw 'used targetNumAdComponents';
          }
      })",
      expected_bid->Clone());
}

// Make sure that fields that are only available with multibid on are read
// when it's on. This is mostly meant to validate the multibid=off
// version of the testcase; the actual functionality of the fields is tested
// separately.
TEST_F(BidderWorkletTest, ComponentTargetFieldsOnlyMultiBid) {
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid: 5,
          render: {url: "https://response.test/"},
          get numMandatoryAdComponents() {
            throw 'used numMandatoryAdComponents';
          }
      })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/
      {"https://url.test/:9 Uncaught used numMandatoryAdComponents."});

  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid: 5,
          render: {url: "https://response.test/"},
          get targetNumAdComponents() {
            throw 'used targetNumAdComponents';
          }
      })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/
      {"https://url.test/:9 Uncaught used targetNumAdComponents."});
}

TEST_F(BidderWorkletTest, TargetNumAdComponents) {
  interest_group_ad_components_->emplace_back(
      GURL("https://ad_component2.test/"), /*metadata=*/std::nullopt);
  interest_group_ad_components_->emplace_back(
      GURL("https://ad_component3.test/"), /*metadata=*/std::nullopt);

  // Can't target 0 component ads (just don't return any).
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid: 5,
          render: {url: "https://response.test/"},
          targetNumAdComponents: 0
      })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/
      {"https://url.test/ generateBid() targetNumAdComponents must be "
       "positive."});

  // Still an error if some are included.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid: 5,
          render: {url: "https://response.test/"},
          adComponents: [
            "https://ad_component.test",
            "https://ad_component.test",
            "https://ad_component.test",
          ],
          targetNumAdComponents: 0
      })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/
      {"https://url.test/ generateBid() targetNumAdComponents must be "
       "positive."});

  // Can't target more than permitted.
  const char kLotsProvidedAndTargeted[] = R"(
    let componentsArray = [];
    componentsArray.length = 60;
    componentsArray.fill("https://ad_component.test", 0);

    function generateBid() {
      return {
          ad: "ad", bid: 5,
          render: {url: "https://response.test/"},
          targetNumAdComponents: 50,
          adComponents: componentsArray
      }
    }
  )";
  RunGenerateBidWithJavascriptExpectingResult(
      kLotsProvidedAndTargeted,
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/
      {"https://url.test/ generateBid() bid targetNumAdComponents larger than "
       "component ad limit of 40."});

  // Must provide at least as much as target.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid: 5,
          render: {url: "https://response.test/"},
          targetNumAdComponents: 1
      })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/
      {"https://url.test/ generateBid() adComponents list smaller than "
       "targetNumAdComponents."});

  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid: 5,
          render: {url: "https://response.test/"},
          targetNumAdComponents: 2,
          adComponents: [
            "https://ad_component.test",
          ]
      })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/
      {"https://url.test/ generateBid() adComponents list smaller than "
       "targetNumAdComponents."});

  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid: 5,
          render: {url: "https://response.test/"},
          adComponents: [
            "https://ad_component.test",
            "https://ad_component.test",
            "https://ad_component.test",
            "https://ad_component.test",
          ],
          targetNumAdComponents: 5
      })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/
      {"https://url.test/ generateBid() adComponents list smaller than "
       "targetNumAdComponents."});

  // Provided exactly what's targeted.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid: 5,
          render: {url: "https://response.test/"},
          adComponents: [
            "https://ad_component.test",
            "https://ad_component2.test",
            "https://ad_component3.test",
          ],
          targetNumAdComponents: 3,
          numMandatoryAdComponents: 2
      })",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 5,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/
          std::vector<blink::AdDescriptor>{
              blink::AdDescriptor(GURL("https://ad_component.test/")),
              blink::AdDescriptor(GURL("https://ad_component2.test/")),
              blink::AdDescriptor(GURL("https://ad_component3.test/"))},
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  // Can't say that more is mandatory than target.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid: 5,
          render: {url: "https://response.test/"},
          adComponents: [
            "https://ad_component.test",
            "https://ad_component.test",
            "https://ad_component.test",
            "https://ad_component.test",
            "https://ad_component.test",
            "https://ad_component.test",
            "https://ad_component.test",
          ],
          targetNumAdComponents: 5,
          numMandatoryAdComponents: 6
      })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/
      {"https://url.test/ generateBid() numMandatoryAdComponents cannot exceed "
       "targetNumAdComponents."});

  // Providing invalid (as opposed to non-k-anon) component ads beyond the limit
  // is still an error.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid: 5,
          render: {url: "https://response.test/"},
          adComponents: [
            "https://ad_component.test",
            "https://ad_component.test",
            "https://ad_component.test",
            "https://ad_component.test",
            "https://ad_component.test",
            "https://ad_component.test",
            "https://not_ad_component.test",
          ],
          targetNumAdComponents: 5,
      })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/
      {"https://url.test/ generateBid() bid adComponents URL "
       "'https://not_ad_component.test/' isn't one of the registered creative "
       "URLs."});

  // It's actually OK to provide more adComponents than limit as long as the
  // target is below it. It will get reduced to the target.
  const char kLotsProvided[] = R"(
    let componentsArray = [];
    componentsArray.length = 50;
    componentsArray[0] = "https://ad_component.test/";
    componentsArray[1] = "https://ad_component2.test/";
    componentsArray.fill("https://ad_component3.test", 2);

    function generateBid() {
      return {
          ad: "ad", bid: 5,
          render: {url: "https://response.test/"},
          targetNumAdComponents: 2,
          adComponents: componentsArray
      }
    }
  )";

  RunGenerateBidWithJavascriptExpectingResult(
      kLotsProvided,
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 5,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/
          std::vector<blink::AdDescriptor>{
              blink::AdDescriptor(GURL("https://ad_component.test/")),
              blink::AdDescriptor(GURL("https://ad_component2.test/"))},
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
}

TEST_F(BidderWorkletTest, TargetNumAdComponentsKAnon) {
  const char kBid[] = R"({
    ad: "ad",
    bid: 5,
    render: "https://response.test/",
    targetNumAdComponents: 2,
    adComponents: [
      "https://ad_component.test",
      "https://ad_component2.test",
      "https://ad_component3.test",
      "https://ad_component4.test",
    ]
  })";

  auto non_k_anon_bid = mojom::BidderWorkletBid::New(
      auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 5,
      /*bid_currency=*/std::nullopt,
      /*ad_cost=*/std::nullopt,
      blink::AdDescriptor(GURL("https://response.test/")),
      /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
      /*ad_component_descriptors=*/
      std::vector<blink::AdDescriptor>{
          blink::AdDescriptor(GURL("https://ad_component.test/")),
          blink::AdDescriptor(GURL("https://ad_component2.test/"))},
      /*modeling_signals=*/std::nullopt, base::TimeDelta());

  interest_group_ad_components_->emplace_back(
      GURL("https://ad_component2.test/"), /*metadata=*/std::nullopt);
  interest_group_ad_components_->emplace_back(
      GURL("https://ad_component3.test/"), /*metadata=*/std::nullopt);
  interest_group_ad_components_->emplace_back(
      GURL("https://ad_component4.test/"), /*metadata=*/std::nullopt);

  kanon_mode_ = auction_worklet::mojom::KAnonymityBidMode::kNone;

  // With no k-anon enforcement, the requested 2 component ads are just the
  // first two component ads.
  RunGenerateBidWithReturnValueExpectingResult(kBid, non_k_anon_bid->Clone());

  // Turn on enforcement, but don't authorize anything. This is still going to
  // be a non-k-anon bid only.
  kanon_mode_ = auction_worklet::mojom::KAnonymityBidMode::kEnforce;
  RunGenerateBidWithReturnValueExpectingResult(
      kBid, non_k_anon_bid->Clone(),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/{});

  // Just authorizing the main bid still produces the same effect, but the
  // reason for re-run failure is different.
  kanon_keys_.emplace(
      auction_worklet::mojom::KAnonKey::New(blink::HashedKAnonKeyForAdBid(
          url::Origin::Create(interest_group_bidding_url_),
          interest_group_bidding_url_, "https://response.test/")));
  RunGenerateBidWithReturnValueExpectingResult(
      kBid, non_k_anon_bid->Clone(),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/
      {"https://url.test/ generateBid() bid adComponents URL "
       "'https://ad_component.test/' isn't one of the registered creative "
       "URLs."});

  // Authorizing ad components 2 and 4, they should be used for k-anon bid but
  // there should still be the non-k-anon bid.
  kanon_keys_.emplace(auction_worklet::mojom::KAnonKey::New(
      blink::HashedKAnonKeyForAdComponentBid(
          GURL("https://ad_component2.test/"))));
  kanon_keys_.emplace(auction_worklet::mojom::KAnonKey::New(
      blink::HashedKAnonKeyForAdComponentBid(
          GURL("https://ad_component4.test/"))));

  {
    std::vector<mojom::BidderWorkletBidPtr> expected;
    expected.push_back(mojom::BidderWorkletBid::New(
        auction_worklet::mojom::BidRole::kEnforcedKAnon, "\"ad\"", 5,
        /*bid_currency=*/std::nullopt,
        /*ad_cost=*/std::nullopt,
        blink::AdDescriptor(GURL("https://response.test/")),
        /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
        /*ad_component_descriptors=*/
        std::vector<blink::AdDescriptor>{
            blink::AdDescriptor(GURL("https://ad_component2.test/")),
            blink::AdDescriptor(GURL("https://ad_component4.test/"))},
        /*modeling_signals=*/std::nullopt, base::TimeDelta()));
    expected.push_back(non_k_anon_bid->Clone());
    RunGenerateBidWithReturnValueExpectingResult(kBid, std::move(expected));
  }

  // Authorizing 1 as well makes the bid suitable for both auctions.
  kanon_keys_.emplace(auction_worklet::mojom::KAnonKey::New(
      blink::HashedKAnonKeyForAdComponentBid(
          GURL("https://ad_component.test/"))));
  RunGenerateBidWithReturnValueExpectingResult(
      kBid, mojom::BidderWorkletBid::New(
                auction_worklet::mojom::BidRole::kBothKAnonModes, "\"ad\"", 5,
                /*bid_currency=*/std::nullopt,
                /*ad_cost=*/std::nullopt,
                blink::AdDescriptor(GURL("https://response.test/")),
                /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
                /*ad_component_descriptors=*/
                std::vector<blink::AdDescriptor>{
                    blink::AdDescriptor(GURL("https://ad_component.test/")),
                    blink::AdDescriptor(GURL("https://ad_component2.test/"))},
                /*modeling_signals=*/std::nullopt, base::TimeDelta()));
}

TEST_F(BidderWorkletTest, TargetAndMandatoryAdComponentsKAnon) {
  const char kBid[] = R"({
    ad: "ad",
    bid: 5,
    render: "https://response.test/",
    targetNumAdComponents: 2,
    numMandatoryAdComponents: 1,
    adComponents: [
      "https://ad_component.test",
      "https://ad_component2.test",
      {url: "https://ad_component3.test", requiredComponent: true},
      "https://ad_component4.test",
    ]
  })";

  auto non_k_anon_bid = mojom::BidderWorkletBid::New(
      auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 5,
      /*bid_currency=*/std::nullopt,
      /*ad_cost=*/std::nullopt,
      blink::AdDescriptor(GURL("https://response.test/")),
      /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
      /*ad_component_descriptors=*/
      std::vector<blink::AdDescriptor>{
          blink::AdDescriptor(GURL("https://ad_component.test/")),
          blink::AdDescriptor(GURL("https://ad_component2.test/"))},
      /*modeling_signals=*/std::nullopt, base::TimeDelta());

  interest_group_ad_components_->emplace_back(
      GURL("https://ad_component2.test/"), /*metadata=*/std::nullopt);
  interest_group_ad_components_->emplace_back(
      GURL("https://ad_component3.test/"), /*metadata=*/std::nullopt);
  interest_group_ad_components_->emplace_back(
      GURL("https://ad_component4.test/"), /*metadata=*/std::nullopt);

  kanon_mode_ = auction_worklet::mojom::KAnonymityBidMode::kNone;

  // With no k-anon enforcement, the requested 2 component ads are just the
  // first two component ads.
  RunGenerateBidWithReturnValueExpectingResult(kBid, non_k_anon_bid->Clone());

  // Turn on enforcement, but don't authorize anything. This is still going to
  // be a non-k-anon bid only.
  kanon_mode_ = auction_worklet::mojom::KAnonymityBidMode::kEnforce;
  RunGenerateBidWithReturnValueExpectingResult(
      kBid, non_k_anon_bid->Clone(),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/{});

  // Authorize the main bid. This is still going to be a non-k-anon bid only;
  // there will be an error-message from a failed re-run, though.
  kanon_keys_.emplace(
      auction_worklet::mojom::KAnonKey::New(blink::HashedKAnonKeyForAdBid(
          url::Origin::Create(interest_group_bidding_url_),
          interest_group_bidding_url_, "https://response.test/")));
  RunGenerateBidWithReturnValueExpectingResult(
      kBid, non_k_anon_bid->Clone(),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/
      {"https://url.test/ generateBid() bid adComponents URL "
       "'https://ad_component.test/' isn't one of the registered creative "
       "URLs."});

  // Authorizing ad components 3 and 4 isn't enough since absence of 1 prevents
  // it from being accepted.
  kanon_keys_.emplace(auction_worklet::mojom::KAnonKey::New(
      blink::HashedKAnonKeyForAdComponentBid(
          GURL("https://ad_component3.test/"))));
  kanon_keys_.emplace(auction_worklet::mojom::KAnonKey::New(
      blink::HashedKAnonKeyForAdComponentBid(
          GURL("https://ad_component4.test/"))));
  RunGenerateBidWithReturnValueExpectingResult(
      kBid, non_k_anon_bid->Clone(),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/
      {"https://url.test/ generateBid() bid adComponents URL "
       "'https://ad_component.test/' isn't one of the registered creative "
       "URLs."});

  // Now authorize 1 as well. Should get 1 and 3 as k-anon bid.
  kanon_keys_.emplace(auction_worklet::mojom::KAnonKey::New(
      blink::HashedKAnonKeyForAdComponentBid(
          GURL("https://ad_component.test/"))));
  {
    std::vector<mojom::BidderWorkletBidPtr> expected;
    expected.push_back(mojom::BidderWorkletBid::New(
        auction_worklet::mojom::BidRole::kEnforcedKAnon, "\"ad\"", 5,
        /*bid_currency=*/std::nullopt,
        /*ad_cost=*/std::nullopt,
        blink::AdDescriptor(GURL("https://response.test/")),
        /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
        /*ad_component_descriptors=*/
        std::vector<blink::AdDescriptor>{
            blink::AdDescriptor(GURL("https://ad_component.test/")),
            blink::AdDescriptor(GURL("https://ad_component3.test/"))},
        /*modeling_signals=*/std::nullopt, base::TimeDelta()));
    expected.push_back(non_k_anon_bid->Clone());
    RunGenerateBidWithReturnValueExpectingResult(kBid, std::move(expected));
  }

  // Authorizing 2 as well makes the bid suitable for both auctions.
  kanon_keys_.emplace(auction_worklet::mojom::KAnonKey::New(
      blink::HashedKAnonKeyForAdComponentBid(
          GURL("https://ad_component2.test/"))));
  RunGenerateBidWithReturnValueExpectingResult(
      kBid, mojom::BidderWorkletBid::New(
                auction_worklet::mojom::BidRole::kBothKAnonModes, "\"ad\"", 5,
                /*bid_currency=*/std::nullopt,
                /*ad_cost=*/std::nullopt,
                blink::AdDescriptor(GURL("https://response.test/")),
                /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
                /*ad_component_descriptors=*/
                std::vector<blink::AdDescriptor>{
                    blink::AdDescriptor(GURL("https://ad_component.test/")),
                    blink::AdDescriptor(GURL("https://ad_component2.test/"))},
                /*modeling_signals=*/std::nullopt, base::TimeDelta()));
}

TEST_F(BidderWorkletTest, GenerateBidMultiBid) {
  multi_bid_limit_ = 2;

  auto expected_bid = mojom::BidderWorkletBid::New(
      auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 2,
      /*bid_currency=*/std::nullopt,
      /*ad_cost=*/std::nullopt,
      blink::AdDescriptor(GURL("https://response.test/")),
      /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
      /*ad_component_descriptors=*/std::nullopt,
      /*modeling_signals=*/std::nullopt, base::TimeDelta());

  // Empty array means no-bid.
  RunGenerateBidWithReturnValueExpectingResult(
      "[]", /*expected_bids=*/mojom::BidderWorkletBidPtr());
  EXPECT_EQ(reject_reason_, mojom::RejectReason::kNotAvailable);

  // Single bid as an array member.
  RunGenerateBidWithReturnValueExpectingResult(
      R"([{ad: "ad", bid: 2,
          render: "https://response.test/"}])",
      expected_bid->Clone());
  EXPECT_EQ(reject_reason_, mojom::RejectReason::kNotAvailable);

  // Actually multiple bids.
  {
    std::vector<mojom::BidderWorkletBidPtr> expected;
    expected.push_back(expected_bid->Clone());
    expected.push_back(expected_bid->Clone());
    expected[1]->bid = 3;
    expected[1]->ad = "\"ad2\"";
    RunGenerateBidWithReturnValueExpectingResult(
        R"([{ad: "ad", bid: 2,
          render: "https://response.test/"},
          {ad: "ad2", bid: 3,
          render: "https://response.test/"},
         ])",
        std::move(expected));
    EXPECT_EQ(reject_reason_, mojom::RejectReason::kNotAvailable);
  }

  // Too many bids.
  RunGenerateBidWithReturnValueExpectingResult(
      R"([{ad: "ad", bid: 2,
          render: "https://response.test/"},
          {ad: "ad2", bid: 3,
          render: "https://response.test/"},
          {ad: "ad3", bid: 4,
          render: "https://response.test/"},
         ])",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/
      {"https://url.test/ generateBid() more bids provided than permitted by "
       "auction configuration."});
  EXPECT_EQ(reject_reason_, mojom::RejectReason::kMultiBidLimitExceeded);

  // The bid limit looks at the length of the sequence provided; some entries
  // being dropped as non-bids doesn't change that.
  RunGenerateBidWithReturnValueExpectingResult(
      R"([{ad: "ad", bid: 2,
          render: "https://response.test/"},
          {ad: "ad2", bid: -5,
          render: "https://response.test/"},
          {ad: "ad3", bid: 4,
          render: "https://response.test/"},
         ])",
      /*expected_bids=*/nullptr,
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/
      {"https://url.test/ generateBid() more bids provided than permitted by "
       "auction configuration."});
  EXPECT_EQ(reject_reason_, mojom::RejectReason::kMultiBidLimitExceeded);

  // Catches errors in individual entries.
  RunGenerateBidWithReturnValueExpectingResult(
      R"([{ad: "ad", bid: 10}])",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/
      {"https://url.test/ generateBid() bids sequence entry: 'render' is "
       "required when making a bid."});
  EXPECT_EQ(reject_reason_, mojom::RejectReason::kNotAvailable);

  // Some more complicated error messages.
  RunGenerateBidWithReturnValueExpectingResult(
      R"([{ad: "ad", bid: 10, render: {}}])",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/
      {"https://url.test/ generateBid() bids sequence entry: 'render': "
       "Required field 'url' is undefined."});
  EXPECT_EQ(reject_reason_, mojom::RejectReason::kNotAvailable);

  RunGenerateBidWithReturnValueExpectingResult(
      R"([{ad: "ad", bid: 10, render: "https://example.org/",
           adComponents: [{}]
      }])",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/
      {"https://url.test/ generateBid() bids sequence entry: adComponents "
       "entry: Required field 'url' is undefined."});
  EXPECT_EQ(reject_reason_, mojom::RejectReason::kNotAvailable);

  RunGenerateBidWithReturnValueExpectingResult(
      R"([{ad: "ad", bid: 2,
          render: "https://response.test/"},
          {ad: "ad", bid: 10}])",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/
      {"https://url.test/ generateBid() bids sequence entry: 'render' is "
       "required when making a bid."});
  EXPECT_EQ(reject_reason_, mojom::RejectReason::kNotAvailable);

  RunGenerateBidWithReturnValueExpectingResult(
      R"([{ad: "ad", bid: 2,
          render: "https://response.test/"},
          10])",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/
      {"https://url.test/ generateBid() bids sequence entry: Value passed as "
       "dictionary is neither object, null, nor undefined."});
  EXPECT_EQ(reject_reason_, mojom::RejectReason::kNotAvailable);

  // A non-bid is still ignored.
  RunGenerateBidWithReturnValueExpectingResult(
      R"([{ad: "ad",
          render: "https://response.test/"}])",
      /*expected_bids=*/mojom::BidderWorkletBidPtr());
  EXPECT_EQ(reject_reason_, mojom::RejectReason::kNotAvailable);

  // A non-bid among real bids in an array is also ignored.
  RunGenerateBidWithReturnValueExpectingResult(
      R"([{ad: "ad", bid: 2,
          render: "https://response.test/"},
          {ad: "ad2",
          render: "https://response.test/"},
         ])",
      expected_bid->Clone());
  EXPECT_EQ(reject_reason_, mojom::RejectReason::kNotAvailable);

  // Make sure the IDL checks happen before semantic checks. This has 4 bids,
  // but 4th one isn't an object, so the IDL error about that is what we should
  // report, not the semantic error on size or on lack of URL on first one. This
  // is mostly important in that looking at the 4th one could have side-effects.
  RunGenerateBidWithReturnValueExpectingResult(
      R"([{ad: "ad", bid: 2},
          {ad: "ad2", bid: 3,
          render:"https://response.test/"},
          {ad: "ad3", bid: 4,
          render:"https://response.test/"},
          4
         ])",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/
      {"https://url.test/ generateBid() bids sequence entry: Value passed as "
       "dictionary is neither object, null, nor undefined."});
  EXPECT_EQ(reject_reason_, mojom::RejectReason::kNotAvailable);
}

// If multibid is off, even passing an array with 1 element to SetBid() isn't
// available.
TEST_F(BidderWorkletMultiBidDisabledTest, SetBidMultiBid) {
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
        setBid([{ad: "ad", bid:2, render: "https://response.test"}]);
        throw "oh no";
      })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/{"https://url.test/:3 Uncaught oh no."});
}

TEST_F(BidderWorkletTest, SetBidMultiBid) {
  multi_bid_limit_ = 2;
  interest_group_ads_.emplace_back(GURL("https://response2.test/"),
                                   /*metadata=*/std::nullopt);
  interest_group_ads_.emplace_back(GURL("https://response3.test/"),
                                   /*metadata=*/std::nullopt);

  auto expected_bid = mojom::BidderWorkletBid::New(
      auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 2,
      /*bid_currency=*/std::nullopt,
      /*ad_cost=*/std::nullopt,
      blink::AdDescriptor(GURL("https://response.test/")),
      /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
      /*ad_component_descriptors=*/std::nullopt,
      /*modeling_signals=*/std::nullopt, base::TimeDelta());

  auto expected_bid3 = mojom::BidderWorkletBid::New(
      auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"lad\"", 8,
      /*bid_currency=*/std::nullopt,
      /*ad_cost=*/std::nullopt,
      blink::AdDescriptor(GURL("https://response3.test/")),
      /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
      /*ad_component_descriptors=*/std::nullopt,
      /*modeling_signals=*/std::nullopt, base::TimeDelta());

  // Can set an array with one entry.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid([{ad: "ad", bid:2, render: "https://response.test"}]);
         throw "boo";
       })",
      /*expected_bids=*/expected_bid->Clone(),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/{"https://url.test/:3 Uncaught boo."});

  // Or two.
  {
    std::vector<mojom::BidderWorkletBidPtr> expected;
    expected.push_back(expected_bid->Clone());
    expected.push_back(expected_bid->Clone());
    expected[1]->ad_descriptor =
        blink::AdDescriptor(GURL("https://response2.test"));
    expected[1]->ad = "\"bad\"";
    expected[1]->bid = 4;
    RunGenerateBidWithJavascriptExpectingResult(
        R"(function generateBid() {
         setBid([{ad: "ad", bid:2, render: "https://response.test"},
                 {ad: "bad", bid:4, render: "https://response2.test"}]);
         throw "boo";
       })",
        std::move(expected),
        /*expected_data_version=*/std::nullopt,
        /*expected_errors=*/{"https://url.test/:4 Uncaught boo."});
  }

  // ..but given our `multi_bid_limit_` is 2, not 3.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid([{ad: "ad", bid:2, render: "https://response.test"},
                 {ad: "bad", bid:4, render: "https://response2.test"},
                 {ad: "lad", bid:8, render: "https://response3.test"}]);
         throw "boo";
       })",
      /*expected_bids=*/nullptr, /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/
      {"https://url.test/:2 Uncaught TypeError: more bids provided than "
       "permitted by auction configuration."});

  // SetBid() with arrays overwrites.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid([{ad: "ad", bid:2, render: "https://response.test"},
                 {ad: "bad", bid:4, render: "https://response2.test"}]);
         setBid([{ad: "lad", bid:8, render: "https://response3.test"}]);
         throw "boo";
       })",
      /*expected_bids=*/expected_bid3->Clone(),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/{"https://url.test/:5 Uncaught boo."});

  // Can overwrite with an empty array as well.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid([{ad: "ad", bid:2, render: "https://response.test"},
                 {ad: "bad", bid:4, render: "https://response2.test"}]);
         setBid([]);
         throw "boo";
       })",
      /*expected_bids=*/nullptr, /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/{"https://url.test/:5 Uncaught boo."});

  // Individual checks do happen.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         setBid([{ad: "ad", bid:2, render: "https://response.test"},
                 {ad: "bad", bid:4, render: "https://response4.test"}]);
         throw "boo";
       })",
      /*expected_bids=*/nullptr, /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/
      {"https://url.test/:2 Uncaught TypeError: bids sequence entry: bid "
       "render URL 'https://response4.test/' isn't one of the registered "
       "creative URLs."});

  // Can recover from a set that throws an exception.
  RunGenerateBidWithJavascriptExpectingResult(
      R"(function generateBid() {
         try {
           setBid([{ad: "ad", bid:2, render: "https://response.test"},
                  {ad: "bad", bid:4, render: "https://response4.test"}]);
         } catch (e) {
           setBid([{ad: "lad", bid:8, render: "https://response3.test"}]);
         }
         throw "boo";
       })",
      /*expected_bids=*/expected_bid3->Clone(),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/{"https://url.test/:8 Uncaught boo."});
}

// Make sure Date() is not available when running generateBid().
TEST_F(BidderWorkletTest, GenerateBidDateNotAvailable) {
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: Date().toString(), bid:1, render:"https://response.test/"})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/:6 Uncaught ReferenceError: Date is not defined."});
}

TEST_F(BidderWorkletTest, GenerateBidInterestGroupOwner) {
  interest_group_bidding_url_ = GURL("https://foo.test/bar");
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: interestGroup.owner, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon,
          R"("https://foo.test")", 1, /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  interest_group_bidding_url_ = GURL("https://[::1]:40000/");
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: interestGroup.owner, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon,
          R"("https://[::1]:40000")", 1, /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
}

TEST_F(BidderWorkletTest, GenerateBidInterestGroupName) {
  const std::string kGenerateBidBody =
      R"({ad: interestGroup.name, bid:1, render:"https://response.test/"})";

  interest_group_name_ = "foo";
  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, R"("foo")", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  interest_group_name_ = R"("foo")";
  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, R"("\"foo\"")", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  interest_group_name_ = "[1]";
  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, R"("[1]")", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
}

TEST_F(BidderWorkletTest,
       GenerateBidInterestGroupEnableBiddingSignalsPrioritization) {
  RunGenerateBidExpectingExpressionIsTrue(
      "interestGroup.enableBiddingSignalsPrioritization === false");

  interest_group_enable_bidding_signals_prioritization_ = true;
  RunGenerateBidExpectingExpressionIsTrue(
      "interestGroup.enableBiddingSignalsPrioritization === true");
}

// This is deprecated and slated to be removed, but should work in the meantime.
//
// TODO(crbug.com/41490104): Remove this test when the field itself is
// removed.
TEST_F(BidderWorkletTest,
       GenerateBidInterestGroupUseBiddingSignalsPrioritization) {
  RunGenerateBidExpectingExpressionIsTrue(
      "interestGroup.useBiddingSignalsPrioritization === false");

  interest_group_enable_bidding_signals_prioritization_ = true;
  RunGenerateBidExpectingExpressionIsTrue(
      "interestGroup.useBiddingSignalsPrioritization === true");
}

// Check that accessing `useBiddingSignalsPrioritization` displays a warning.
//
// TODO(crbug.com/41490104): Remove this test when the field itself is
// removed.
TEST_F(BidderWorkletTest, UseBiddingSignalsPrioritizationDeprecationWarning) {
  ScopedInspectorSupport inspector_support(v8_helper().get());

  AddJavascriptResponse(
      &url_loader_factory_, interest_group_bidding_url_,
      CreateGenerateBidScript(
          R"({ad: ["ad"], bid:1, render:"https://response.test/"})",
          "if (!interestGroup.useBiddingSignalsPrioritization) return;"));

  interest_group_enable_bidding_signals_prioritization_ = true;

  BidderWorklet* worklet_impl;
  auto worklet =
      CreateWorklet(interest_group_bidding_url_,
                    /*pause_for_debugger_on_start=*/false, &worklet_impl);

  int id = worklet_impl->context_group_ids_for_testing()[0];
  TestChannel* channel =
      inspector_support.ConnectDebuggerSessionAndRuntimeEnable(id);

  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  GenerateBid(worklet.get());
  generate_bid_run_loop_->Run();
  channel->WaitForAndValidateConsoleMessage(
      "warning", /*json_args=*/
      "[{\"type\":\"string\", "
      "\"value\":\"interestGroup.useBiddingSignalsPrioritization is "
      "deprecated. Please use interestGroup.enableBiddingSignalsPrioritization "
      "instead.\"}]",
      /*stack_trace_size=*/1, /*function=*/"generateBid",
      interest_group_bidding_url_, /*line_number=*/4);
  channel->ExpectNoMoreConsoleEvents();

  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(1, bids_[0]->bid);
  bids_.clear();
  generate_bid_run_loop_.reset();
}

// Check that accessing `enableBiddingSignalsPrioritization` does not display a
// warning.
//
// TODO(crbug.com/41490104): Remove this test when
// useBiddingSignalsPrioritization is removed.
TEST_F(BidderWorkletTest,
       EnableBiddingSignalsPrioritizationNoDeprecationWarning) {
  ScopedInspectorSupport inspector_support(v8_helper().get());

  AddJavascriptResponse(
      &url_loader_factory_, interest_group_bidding_url_,
      CreateGenerateBidScript(
          R"({ad: ["ad"], bid:1, render:"https://response.test/"})",
          "if (!interestGroup.enableBiddingSignalsPrioritization) return;"));

  interest_group_enable_bidding_signals_prioritization_ = true;

  BidderWorklet* worklet_impl;
  auto worklet =
      CreateWorklet(interest_group_bidding_url_,
                    /*pause_for_debugger_on_start=*/false, &worklet_impl);

  int id = worklet_impl->context_group_ids_for_testing()[0];
  TestChannel* channel =
      inspector_support.ConnectDebuggerSessionAndRuntimeEnable(id);

  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  GenerateBid(worklet.get());
  generate_bid_run_loop_->Run();

  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(1, bids_[0]->bid);
  channel->ExpectNoMoreConsoleEvents();
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
      R"({ad: interestGroup.biddingLogicURL, bid:1,
        render:"https://response.test/"})";

  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon,
          R"("https://url.test/")", 1, /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  interest_group_bidding_url_ = GURL("https://url.test/foo");
  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon,
          R"("https://url.test/foo")", 1, /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
}

// Check that accessing `biddingLogicUrl` displays a warning.
//
// TODO(crbug.com/40264073): Remove this test when the field itself is
// removed.
TEST_F(BidderWorkletTest, BiddingLogicUrlDeprecationWarning) {
  ScopedInspectorSupport inspector_support(v8_helper().get());

  AddJavascriptResponse(
      &url_loader_factory_, interest_group_bidding_url_,
      CreateGenerateBidScript(
          R"({ad: ["ad"], bid:1, render:"https://response.test/"})",
          "if (!interestGroup.biddingLogicUrl) return;"));

  BidderWorklet* worklet_impl;
  auto worklet =
      CreateWorklet(interest_group_bidding_url_,
                    /*pause_for_debugger_on_start=*/false, &worklet_impl);

  int id = worklet_impl->context_group_ids_for_testing()[0];
  TestChannel* channel =
      inspector_support.ConnectDebuggerSessionAndRuntimeEnable(id);

  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  GenerateBid(worklet.get());
  generate_bid_run_loop_->Run();
  channel->WaitForAndValidateConsoleMessage(
      "warning", /*json_args=*/
      "[{\"type\":\"string\", "
      "\"value\":\"interestGroup.biddingLogicUrl is deprecated. Please use "
      "interestGroup.biddingLogicURL instead.\"}]",
      /*stack_trace_size=*/1, /*function=*/"generateBid",
      interest_group_bidding_url_, /*line_number=*/4);
  channel->ExpectNoMoreConsoleEvents();

  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(1, bids_[0]->bid);
  bids_.clear();
  generate_bid_run_loop_.reset();
}

// Check that accessing `biddingLogicURL` does not display a warning.
//
// TODO(crbug.com/40264073): Remove this test when `biddingLogicUrl` is
// removed.
TEST_F(BidderWorkletTest, BiddingLogicUrlNoDeprecationWarning) {
  ScopedInspectorSupport inspector_support(v8_helper().get());

  AddJavascriptResponse(
      &url_loader_factory_, interest_group_bidding_url_,
      CreateGenerateBidScript(
          R"({ad: ["ad"], bid:1, render:"https://response.test/"})",
          "if (!interestGroup.biddingLogicURL) return;"));

  BidderWorklet* worklet_impl;
  auto worklet =
      CreateWorklet(interest_group_bidding_url_,
                    /*pause_for_debugger_on_start=*/false, &worklet_impl);

  int id = worklet_impl->context_group_ids_for_testing()[0];
  TestChannel* channel =
      inspector_support.ConnectDebuggerSessionAndRuntimeEnable(id);

  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  GenerateBid(worklet.get());
  generate_bid_run_loop_->Run();

  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(1, bids_[0]->bid);
  channel->ExpectNoMoreConsoleEvents();
}

TEST_F(BidderWorkletTest, GenerateBidInterestGroupBiddingWasmHelperUrl) {
  const std::string kGenerateBidBody =
      R"({ad: "biddingWasmHelperURL" in interestGroup ?
            interestGroup.biddingWasmHelperURL : "missing",
        bid:1,
        render:"https://response.test/"})";

  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, R"("missing")", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  interest_group_wasm_url_ = GURL(kWasmUrl);
  AddResponse(&url_loader_factory_, GURL(kWasmUrl), kWasmMimeType,
              /*charset=*/std::nullopt, ToyWasm());
  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon,
          R"("https://foo.test/helper.wasm")", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
}

// Check that accessing `biddingWasmHelperUrl` displays a warning.
//
// TODO(crbug.com/40264073): Remove this test when the field itself is
// removed.
TEST_F(BidderWorkletTest, BiddingWasmHelperUrlDeprecationWarning) {
  ScopedInspectorSupport inspector_support(v8_helper().get());

  AddJavascriptResponse(
      &url_loader_factory_, interest_group_bidding_url_,
      CreateGenerateBidScript(
          R"({ad: ["ad"], bid:1, render:"https://response.test/"})",
          "if (!interestGroup.biddingWasmHelperUrl) return;"));

  interest_group_wasm_url_ = GURL(kWasmUrl);
  // Need a valid WASM response to avoid a hang, and for the script to run.
  AddResponse(&url_loader_factory_, GURL(kWasmUrl), kWasmMimeType,
              /*charset=*/std::nullopt, ToyWasm());

  BidderWorklet* worklet_impl;
  auto worklet =
      CreateWorklet(interest_group_bidding_url_,
                    /*pause_for_debugger_on_start=*/false, &worklet_impl);

  int id = worklet_impl->context_group_ids_for_testing()[0];
  TestChannel* channel =
      inspector_support.ConnectDebuggerSessionAndRuntimeEnable(id);

  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  GenerateBid(worklet.get());
  generate_bid_run_loop_->Run();
  channel->WaitForAndValidateConsoleMessage(
      "warning", /*json_args=*/
      "[{\"type\":\"string\", "
      "\"value\":\"interestGroup.biddingWasmHelperUrl is deprecated. Please "
      "use interestGroup.biddingWasmHelperURL instead.\"}]",
      /*stack_trace_size=*/1, /*function=*/"generateBid",
      interest_group_bidding_url_, /*line_number=*/4);
  channel->ExpectNoMoreConsoleEvents();

  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(1, bids_[0]->bid);
  bids_.clear();
  generate_bid_run_loop_.reset();
}

// Check that accessing `biddingWasmHelperURL` does not display a warning.
//
// TODO(crbug.com/40264073): Remove this test when `biddingWasmHelperUrl`
// is removed.
TEST_F(BidderWorkletTest, BiddingWasmHelperUrlNoDeprecationWarning) {
  ScopedInspectorSupport inspector_support(v8_helper().get());

  AddJavascriptResponse(
      &url_loader_factory_, interest_group_bidding_url_,
      CreateGenerateBidScript(
          R"({ad: ["ad"], bid:1, render:"https://response.test/"})",
          "if (!interestGroup.biddingWasmHelperURL) return;"));

  interest_group_wasm_url_ = GURL(kWasmUrl);
  // Need a valid WASM response to avoid a hang, and for the script to run.
  AddResponse(&url_loader_factory_, GURL(kWasmUrl), kWasmMimeType,
              /*charset=*/std::nullopt, ToyWasm());

  BidderWorklet* worklet_impl;
  auto worklet =
      CreateWorklet(interest_group_bidding_url_,
                    /*pause_for_debugger_on_start=*/false, &worklet_impl);

  int id = worklet_impl->context_group_ids_for_testing()[0];
  TestChannel* channel =
      inspector_support.ConnectDebuggerSessionAndRuntimeEnable(id);

  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  GenerateBid(worklet.get());
  generate_bid_run_loop_->Run();

  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(1, bids_[0]->bid);
  channel->ExpectNoMoreConsoleEvents();
}

TEST_F(BidderWorkletTest, GenerateBidInterestGroupUpdateUrl) {
  const std::string kGenerateBidBody =
      R"({ad: "updateURL" in interestGroup ?
            interestGroup.updateURL : "missing",
        bid:1,
        render:"https://response.test/"})";
  // TODO(crbug.com/40258629): Remove this and tests that use it when
  // removing support for the deprecated `dailyUpdateUrl` field, in favor of
  // `updateURL`.
  const std::string kGenerateBidBodyUsingDeprecatedDailyUpdateUrl =
      R"({ad: "dailyUpdateUrl" in interestGroup ?
            interestGroup.updateURL : "missing",
        bid:1,
        render:"https://response.test/"})";

  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, R"("missing")", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBodyUsingDeprecatedDailyUpdateUrl,
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, R"("missing")", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  update_url_ = GURL("https://url.test/daily_update");
  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon,
          R"("https://url.test/daily_update")", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBodyUsingDeprecatedDailyUpdateUrl,
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon,
          R"("https://url.test/daily_update")", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
}

// Check that accessing `updateUrl` displays a warning.
//
// TODO(crbug.com/40264073): Remove this test when the field itself is
// removed.
TEST_F(BidderWorkletTest, UpdateUrlDeprecationWarning) {
  ScopedInspectorSupport inspector_support(v8_helper().get());

  AddJavascriptResponse(
      &url_loader_factory_, interest_group_bidding_url_,
      CreateGenerateBidScript(
          R"({ad: ["ad"], bid:1, render:"https://response.test/"})",
          "if (!interestGroup.updateUrl) return;"));

  update_url_ = GURL("https://url.test/update");

  BidderWorklet* worklet_impl;
  auto worklet =
      CreateWorklet(interest_group_bidding_url_,
                    /*pause_for_debugger_on_start=*/false, &worklet_impl);

  int id = worklet_impl->context_group_ids_for_testing()[0];
  TestChannel* channel =
      inspector_support.ConnectDebuggerSessionAndRuntimeEnable(id);

  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  GenerateBid(worklet.get());
  generate_bid_run_loop_->Run();
  channel->WaitForAndValidateConsoleMessage(
      "warning", /*json_args=*/
      "[{\"type\":\"string\", "
      "\"value\":\"interestGroup.updateUrl is deprecated. Please use "
      "interestGroup.updateURL instead.\"}]",
      /*stack_trace_size=*/1, /*function=*/"generateBid",
      interest_group_bidding_url_, /*line_number=*/4);
  channel->ExpectNoMoreConsoleEvents();

  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(1, bids_[0]->bid);
  bids_.clear();
  generate_bid_run_loop_.reset();
}

// Check that accessing `dailyUpdateUrl` displays a warning.
//
// TODO(crbug.com/40258629): Remove this test when the field itself is
// removed.
TEST_F(BidderWorkletTest, DailyUpdateUrlDeprecationWarning) {
  ScopedInspectorSupport inspector_support(v8_helper().get());

  AddJavascriptResponse(
      &url_loader_factory_, interest_group_bidding_url_,
      CreateGenerateBidScript(
          R"({ad: ["ad"], bid:1, render:"https://response.test/"})",
          "if (!interestGroup.dailyUpdateUrl) return;"));

  update_url_ = GURL("https://url.test/update");

  BidderWorklet* worklet_impl;
  auto worklet =
      CreateWorklet(interest_group_bidding_url_,
                    /*pause_for_debugger_on_start=*/false, &worklet_impl);

  int id = worklet_impl->context_group_ids_for_testing()[0];
  TestChannel* channel =
      inspector_support.ConnectDebuggerSessionAndRuntimeEnable(id);

  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  GenerateBid(worklet.get());
  generate_bid_run_loop_->Run();
  channel->WaitForAndValidateConsoleMessage(
      "warning", /*json_args=*/
      "[{\"type\":\"string\", "
      "\"value\":\"interestGroup.dailyUpdateUrl is deprecated. Please use "
      "interestGroup.updateURL instead.\"}]",
      /*stack_trace_size=*/1, /*function=*/"generateBid",
      interest_group_bidding_url_, /*line_number=*/4);
  channel->ExpectNoMoreConsoleEvents();

  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(1, bids_[0]->bid);
  bids_.clear();
  generate_bid_run_loop_.reset();
}

// Check that accessing `updateURL` does not display a warning.
//
// TODO(crbug.com/40264073) and TODO(crbug.com/40258629):
// Remove this test when `dailyUpdateUrl` and `updateUrl` are removed.
TEST_F(BidderWorkletTest, UpdateUrlNoDeprecationWarning) {
  ScopedInspectorSupport inspector_support(v8_helper().get());

  AddJavascriptResponse(
      &url_loader_factory_, interest_group_bidding_url_,
      CreateGenerateBidScript(
          R"({ad: ["ad"], bid:1, render:"https://response.test/"})",
          "if (!interestGroup.updateURL) return;"));

  update_url_ = GURL("https://url.test/update");

  BidderWorklet* worklet_impl;
  auto worklet =
      CreateWorklet(interest_group_bidding_url_,
                    /*pause_for_debugger_on_start=*/false, &worklet_impl);

  int id = worklet_impl->context_group_ids_for_testing()[0];
  TestChannel* channel =
      inspector_support.ConnectDebuggerSessionAndRuntimeEnable(id);

  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  GenerateBid(worklet.get());
  generate_bid_run_loop_->Run();

  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(1, bids_[0]->bid);
  channel->ExpectNoMoreConsoleEvents();
}

TEST_F(BidderWorkletTest, GenerateBidInterestGroupTrustedBiddingSignalsUrl) {
  const std::string kGenerateBidBody =
      R"({ad: "trustedBiddingSignalsURL" in interestGroup ?
            interestGroup.trustedBiddingSignalsURL : "missing",
        bid:1,
        render:"https://response.test/"})";

  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, R"("missing")", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

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
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon,
          R"("https://signals.test/foo.json")", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
}

// Check that accessing `trustedBiddingSignalsUrl` displays a warning.
//
// TODO(crbug.com/40264073): Remove this test when the field itself is
// removed.
TEST_F(BidderWorkletTest, TrustedBiddingSignalsUrlDeprecationWarning) {
  ScopedInspectorSupport inspector_support(v8_helper().get());

  AddJavascriptResponse(
      &url_loader_factory_, interest_group_bidding_url_,
      CreateGenerateBidScript(
          R"({ad: ["ad"], bid:1, render:"https://response.test/"})",
          "if (!interestGroup.trustedBiddingSignalsUrl) return;"));

  interest_group_trusted_bidding_signals_url_ =
      GURL("https://url.test/trusted_signals");
  // Need trusted signals response to prevent a hang.
  AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(interest_group_trusted_bidding_signals_url_->spec() +
           "?hostname=top.window.test&interestGroupNames=Fred"),
      "{}");

  BidderWorklet* worklet_impl;
  auto worklet =
      CreateWorklet(interest_group_bidding_url_,
                    /*pause_for_debugger_on_start=*/false, &worklet_impl);

  int id = worklet_impl->context_group_ids_for_testing()[0];
  TestChannel* channel =
      inspector_support.ConnectDebuggerSessionAndRuntimeEnable(id);

  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  GenerateBid(worklet.get());
  generate_bid_run_loop_->Run();
  channel->WaitForAndValidateConsoleMessage(
      "warning", /*json_args=*/
      "[{\"type\":\"string\", "
      "\"value\":\"interestGroup.trustedBiddingSignalsUrl is deprecated. "
      "Please use interestGroup.trustedBiddingSignalsURL instead.\"}]",
      /*stack_trace_size=*/1, /*function=*/"generateBid",
      interest_group_bidding_url_, /*line_number=*/4);
  channel->ExpectNoMoreConsoleEvents();

  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(1, bids_[0]->bid);
  bids_.clear();
  generate_bid_run_loop_.reset();
}

// Check that accessing `TrustedBiddingSignalsURL` does not display a warning.
//
// TODO(crbug.com/40264073): Remove this test when
// `trustedBiddingSignalsUrl` is removed.
TEST_F(BidderWorkletTest, TrustedBiddingSignalsUrlNoDeprecationWarning) {
  ScopedInspectorSupport inspector_support(v8_helper().get());

  AddJavascriptResponse(
      &url_loader_factory_, interest_group_bidding_url_,
      CreateGenerateBidScript(
          R"({ad: ["ad"], bid:1, render:"https://response.test/"})",
          "if (!interestGroup.trustedBiddingSignalsURL) return;"));

  interest_group_trusted_bidding_signals_url_ =
      GURL("https://url.test/trusted_signals");
  // Need trusted signals response to prevent a hang.
  AddBidderJsonResponse(
      &url_loader_factory_,
      GURL(interest_group_trusted_bidding_signals_url_->spec() +
           "?hostname=top.window.test&interestGroupNames=Fred"),
      "{}");

  BidderWorklet* worklet_impl;
  auto worklet =
      CreateWorklet(interest_group_bidding_url_,
                    /*pause_for_debugger_on_start=*/false, &worklet_impl);

  int id = worklet_impl->context_group_ids_for_testing()[0];
  TestChannel* channel =
      inspector_support.ConnectDebuggerSessionAndRuntimeEnable(id);

  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  GenerateBid(worklet.get());
  generate_bid_run_loop_->Run();

  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(1, bids_[0]->bid);
  channel->ExpectNoMoreConsoleEvents();
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
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, R"("missing")", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  // 0-length but non-null key list.
  interest_group_trusted_bidding_signals_keys_.emplace();
  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, R"([])", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  interest_group_trusted_bidding_signals_keys_->push_back("2");
  interest_group_trusted_bidding_signals_keys_->push_back("1");
  interest_group_trusted_bidding_signals_keys_->push_back("3");
  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, R"(["2","1","3"])",
          1, /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
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
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "null", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  interest_group_user_bidding_signals_ = R"("foo")";
  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, R"("foo")", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  interest_group_user_bidding_signals_ = "[1]";
  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "[1]", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  interest_group_user_bidding_signals_ = std::nullopt;
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: interestGroup.userBiddingSignals === undefined, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "true", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
}

// Test multiple GenerateBid calls on a single worklet, in parallel. Do this
// twice, once before the worklet has loaded its Javascript, and once after, to
// make sure both cases work.
TEST_P(BidderWorkletMultiThreadingTest, GenerateBidParallel) {
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
          browser_signal_recency_generate_bid_, CreateBiddingBrowserSignals(),
          auction_start_time_, requested_ad_size_, multi_bid_limit_,
          /*trace_id=*/1,
          GenerateBidClientWithCallbacks::Create(base::BindLambdaForTesting(
              [&run_loop, &num_generate_bid_calls, bid_value](
                  std::vector<mojom::BidderWorkletBidPtr> bids,
                  std::optional<uint32_t> data_version,
                  const std::optional<GURL>& debug_loss_report_url,
                  const std::optional<GURL>& debug_win_report_url,
                  std::optional<double> set_priority,
                  base::flat_map<
                      std::string,
                      auction_worklet::mojom::PrioritySignalsDoublePtr>
                      update_priority_signals_overrides,
                  PrivateAggregationRequests pa_requests,
                  PrivateAggregationRequests non_kanon_pa_requests,
                  RealTimeReportingContributions real_time_contributions,
                  mojom::BidderTimingMetricsPtr generate_bid_metrics,
                  mojom::GenerateBidDependencyLatenciesPtr
                      generate_bid_dependency_latencies,
                  mojom::RejectReason reject_reason,
                  const std::vector<std::string>& errors) {
                ASSERT_EQ(1u, bids.size());
                const mojom::BidderWorkletBid* bid = bids[0].get();
                EXPECT_EQ(bid_value, bid->bid);
                EXPECT_EQ(base::NumberToString(bid_value), bid->ad);
                EXPECT_EQ(GURL("https://response.test/"),
                          bid->ad_descriptor.url);
                EXPECT_FALSE(data_version.has_value());
                EXPECT_TRUE(errors.empty());
                ++num_generate_bid_calls;
                if (num_generate_bid_calls == kNumGenerateBidCalls) {
                  run_loop.Quit();
                }
              })),
          bid_finalizer.BindNewEndpointAndPassReceiver());
      bid_finalizer->FinishGenerateBid(
          /*auction_signals_json=*/base::NumberToString(bid_value),
          per_buyer_signals_, per_buyer_timeout_, per_buyer_currency_,
          /*direct_from_seller_per_buyer_signals=*/std::nullopt,
          /*direct_from_seller_per_buyer_signals_header_ad_slot=*/std::nullopt,
          /*direct_from_seller_auction_signals=*/std::nullopt,
          /*direct_from_seller_auction_signals_header_ad_slot=*/std::nullopt);
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
TEST_P(BidderWorkletMultiThreadingTest, GenerateBidParallelLoadFails) {
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
TEST_P(BidderWorkletMultiThreadingTest,
       GenerateBidTrustedBiddingSignalsParallelBatched1) {
  interest_group_trusted_bidding_signals_url_ = GURL("https://signals.test/");
  interest_group_bidding_url_ = *interest_group_trusted_bidding_signals_url_;
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
        browser_signal_top_level_seller_origin_,
        browser_signal_recency_generate_bid_, CreateBiddingBrowserSignals(),
        auction_start_time_, requested_ad_size_, multi_bid_limit_,
        /*trace_id=*/1,
        GenerateBidClientWithCallbacks::Create(base::BindLambdaForTesting(
            [&run_loop, &num_generate_bid_calls, i](
                std::vector<mojom::BidderWorkletBidPtr> bids,
                std::optional<uint32_t> data_version,
                const std::optional<GURL>& debug_loss_report_url,
                const std::optional<GURL>& debug_win_report_url,
                std::optional<double> set_priority,
                base::flat_map<std::string,
                               auction_worklet::mojom::PrioritySignalsDoublePtr>
                    update_priority_signals_overrides,
                PrivateAggregationRequests pa_requests,
                PrivateAggregationRequests non_kanon_pa_requests,
                RealTimeReportingContributions real_time_contributions,
                mojom::BidderTimingMetricsPtr generate_bid_metrics,
                mojom::GenerateBidDependencyLatenciesPtr
                    generate_bid_dependency_latencies,
                mojom::RejectReason reject_reason,
                const std::vector<std::string>& errors) {
              ASSERT_EQ(1u, bids.size());
              const mojom::BidderWorkletBid* bid = bids[0].get();
              EXPECT_EQ(base::NumberToString(i), bid->ad);
              EXPECT_EQ(i + 1, bid->bid);
              EXPECT_EQ(GURL("https://response.test/"), bid->ad_descriptor.url);
              ASSERT_TRUE(data_version.has_value());
              EXPECT_EQ(10u, data_version.value());
              EXPECT_TRUE(errors.empty());
              ++num_generate_bid_calls;
              if (num_generate_bid_calls == kNumGenerateBidCalls) {
                run_loop.Quit();
              }
            })),
        bid_finalizer.BindNewEndpointAndPassReceiver());
    bid_finalizer->FinishGenerateBid(
        auction_signals_, per_buyer_signals_, per_buyer_timeout_,
        per_buyer_currency_,
        /*direct_from_seller_per_buyer_signals=*/std::nullopt,
        /*direct_from_seller_per_buyer_signals_header_ad_slot=*/std::nullopt,
        /*direct_from_seller_auction_signals=*/std::nullopt,
        /*direct_from_seller_auction_signals_header_ad_slot=*/std::nullopt);
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
    if (i != 0) {
      keys.append(",");
    }
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
TEST_P(BidderWorkletMultiThreadingTest,
       GenerateBidTrustedBiddingSignalsParallelBatched2) {
  interest_group_trusted_bidding_signals_url_ = GURL("https://signals.test/");
  interest_group_bidding_url_ = *interest_group_trusted_bidding_signals_url_;
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
        browser_signal_top_level_seller_origin_,
        browser_signal_recency_generate_bid_, CreateBiddingBrowserSignals(),
        auction_start_time_, requested_ad_size_, multi_bid_limit_,
        /*trace_id=*/1,
        GenerateBidClientWithCallbacks::Create(base::BindLambdaForTesting(
            [&run_loop, &num_generate_bid_calls, i](
                std::vector<mojom::BidderWorkletBidPtr> bids,
                std::optional<uint32_t> data_version,
                const std::optional<GURL>& debug_loss_report_url,
                const std::optional<GURL>& debug_win_report_url,
                std::optional<double> set_priority,
                base::flat_map<std::string,
                               auction_worklet::mojom::PrioritySignalsDoublePtr>
                    update_priority_signals_overrides,
                PrivateAggregationRequests pa_requests,
                PrivateAggregationRequests non_kanon_pa_requests,
                RealTimeReportingContributions real_time_contributions,
                mojom::BidderTimingMetricsPtr generate_bid_metrics,
                mojom::GenerateBidDependencyLatenciesPtr
                    generate_bid_dependency_latencies,
                mojom::RejectReason reject_reason,
                const std::vector<std::string>& errors) {
              ASSERT_EQ(1u, bids.size());
              const mojom::BidderWorkletBid* bid = bids[0].get();
              EXPECT_EQ(base::NumberToString(i), bid->ad);
              EXPECT_EQ(i + 1, bid->bid);
              EXPECT_EQ(GURL("https://response.test/"), bid->ad_descriptor.url);
              ASSERT_TRUE(data_version.has_value());
              EXPECT_EQ(42u, data_version.value());
              EXPECT_TRUE(errors.empty());
              ++num_generate_bid_calls;
              if (num_generate_bid_calls == kNumGenerateBidCalls) {
                run_loop.Quit();
              }
            })),
        bid_finalizer.BindNewEndpointAndPassReceiver());
    bid_finalizer->FinishGenerateBid(
        auction_signals_, per_buyer_signals_, per_buyer_timeout_,
        per_buyer_currency_,
        /*direct_from_seller_per_buyer_signals=*/std::nullopt,
        /*direct_from_seller_per_buyer_signals_header_ad_slot=*/std::nullopt,
        /*direct_from_seller_auction_signals=*/std::nullopt,
        /*direct_from_seller_auction_signals_header_ad_slot=*/std::nullopt);
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
    if (i != 0) {
      keys.append(",");
    }
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
TEST_P(BidderWorkletMultiThreadingTest,
       GenerateBidTrustedBiddingSignalsParallelBatched3) {
  interest_group_trusted_bidding_signals_url_ = GURL("https://signals.test/");
  interest_group_bidding_url_ = *interest_group_trusted_bidding_signals_url_;
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
        browser_signal_top_level_seller_origin_,
        browser_signal_recency_generate_bid_, CreateBiddingBrowserSignals(),
        auction_start_time_, requested_ad_size_, multi_bid_limit_,
        /*trace_id=*/1,
        GenerateBidClientWithCallbacks::Create(base::BindLambdaForTesting(
            [&run_loop, &num_generate_bid_calls, i](
                std::vector<mojom::BidderWorkletBidPtr> bids,
                std::optional<uint32_t> data_version,
                const std::optional<GURL>& debug_loss_report_url,
                const std::optional<GURL>& debug_win_report_url,
                std::optional<double> set_priority,
                base::flat_map<std::string,
                               auction_worklet::mojom::PrioritySignalsDoublePtr>
                    update_priority_signals_overrides,
                PrivateAggregationRequests pa_requests,
                PrivateAggregationRequests non_kanon_pa_requests,
                RealTimeReportingContributions real_time_contributions,
                mojom::BidderTimingMetricsPtr generate_bid_metrics,
                mojom::GenerateBidDependencyLatenciesPtr
                    generate_bid_dependency_latencies,
                mojom::RejectReason reject_reason,
                const std::vector<std::string>& errors) {
              ASSERT_EQ(1u, bids.size());
              const mojom::BidderWorkletBid* bid = bids[0].get();
              EXPECT_EQ(base::NumberToString(i), bid->ad);
              EXPECT_EQ(i + 1, bid->bid);
              ASSERT_TRUE(data_version.has_value());
              EXPECT_EQ(22u, data_version.value());
              EXPECT_EQ(GURL("https://response.test/"), bid->ad_descriptor.url);
              EXPECT_TRUE(errors.empty());
              ++num_generate_bid_calls;
              if (num_generate_bid_calls == kNumGenerateBidCalls) {
                run_loop.Quit();
              }
            })),
        bid_finalizer.BindNewEndpointAndPassReceiver());
    bid_finalizer->FinishGenerateBid(
        auction_signals_, per_buyer_signals_, per_buyer_timeout_,
        per_buyer_currency_,
        /*direct_from_seller_per_buyer_signals=*/std::nullopt,
        /*direct_from_seller_per_buyer_signals_header_ad_slot=*/std::nullopt,
        /*direct_from_seller_auction_signals=*/std::nullopt,
        /*direct_from_seller_auction_signals_header_ad_slot=*/std::nullopt);
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
    if (i != 0) {
      keys.append(",");
    }
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
TEST_P(BidderWorkletMultiThreadingTest,
       GenerateBidTrustedBiddingSignalsParallelNotBatched) {
  interest_group_trusted_bidding_signals_url_ = GURL("https://signals.test/");
  interest_group_bidding_url_ = *interest_group_trusted_bidding_signals_url_;
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
        browser_signal_top_level_seller_origin_,
        browser_signal_recency_generate_bid_, CreateBiddingBrowserSignals(),
        auction_start_time_, requested_ad_size_, multi_bid_limit_,
        /*trace_id=*/1,
        GenerateBidClientWithCallbacks::Create(base::BindLambdaForTesting(
            [&run_loop, &num_generate_bid_calls, i](
                std::vector<mojom::BidderWorkletBidPtr> bids,
                std::optional<uint32_t> data_version,
                const std::optional<GURL>& debug_loss_report_url,
                const std::optional<GURL>& debug_win_report_url,
                std::optional<double> set_priority,
                base::flat_map<std::string,
                               auction_worklet::mojom::PrioritySignalsDoublePtr>
                    update_priority_signals_overrides,
                PrivateAggregationRequests pa_requests,
                PrivateAggregationRequests non_kanon_pa_requests,
                RealTimeReportingContributions real_time_contributions,
                mojom::BidderTimingMetricsPtr generate_bid_metrics,
                mojom::GenerateBidDependencyLatenciesPtr
                    generate_bid_dependency_latencies,
                mojom::RejectReason reject_reason,
                const std::vector<std::string>& errors) {
              ASSERT_EQ(1u, bids.size());
              const mojom::BidderWorkletBid* bid = bids[0].get();
              EXPECT_EQ(base::NumberToString(i), bid->ad);
              EXPECT_EQ(i + 1, bid->bid);
              EXPECT_EQ(GURL("https://response.test/"), bid->ad_descriptor.url);
              ASSERT_TRUE(data_version.has_value());
              EXPECT_EQ(i, data_version.value());
              EXPECT_TRUE(errors.empty());
              ++num_generate_bid_calls;
              if (num_generate_bid_calls == kNumGenerateBidCalls) {
                run_loop.Quit();
              }
            })),
        bid_finalizer.BindNewEndpointAndPassReceiver());

    // Send one request at a time.
    bidder_worklet->SendPendingSignalsRequests();

    bid_finalizer->FinishGenerateBid(
        auction_signals_, per_buyer_signals_, per_buyer_timeout_,
        per_buyer_currency_,
        /*direct_from_seller_per_buyer_signals=*/std::nullopt,
        /*direct_from_seller_per_buyer_signals_header_ad_slot=*/std::nullopt,
        /*direct_from_seller_auction_signals=*/std::nullopt,
        /*direct_from_seller_auction_signals_header_ad_slot=*/std::nullopt);
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
TEST_P(BidderWorkletMultiThreadingTest, GenerateBidLoadCompletionOrder) {
  constexpr char kTrustedSignalsResponse[] = R"({"keys":{"1":1}})";
  constexpr char kJsonResponse[] = "{}";
  constexpr char kDirectFromSellerSignalsHeaders[] =
      "Ad-Auction-Allowed: true\nAd-Auction-Only: true";

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

  const auto kResponses = std::to_array<Response>({
      {
          interest_group_bidding_url_,
          kJavascriptMimeType,
          kAllowFledgeHeader,
          CreateBasicGenerateBidScript(),
      },
      {
          *direct_from_seller_per_buyer_signals_,
          kJsonMimeType,
          kDirectFromSellerSignalsHeaders,
          kJsonResponse,
      },
      {
          *direct_from_seller_auction_signals_,
          kJsonMimeType,
          kDirectFromSellerSignalsHeaders,
          kJsonResponse,
      },
      {
          GURL(interest_group_trusted_bidding_signals_url_->spec() +
               "?hostname=top.window.test&keys=1&interestGroupNames=Fred"),
          kJsonMimeType,
          kAllowFledgeHeader,
          kTrustedSignalsResponse,
      },
  });

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
    generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
    GenerateBid(bidder_worklet.get());
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
        EXPECT_FALSE(generate_bid_run_loop_->AnyQuitCalled());
      }
    }
    // The last URL for this generateBid() call has completed -- check that
    // generateBid() returns.
    generate_bid_run_loop_->Run();
    generate_bid_run_loop_.reset();
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
      "Ad-Auction-Allowed: true\nAd-Auction-Only: true";

  for (bool late_direct_from_seller_signals : {false, true}) {
    SCOPED_TRACE(late_direct_from_seller_signals);
    provide_direct_from_seller_signals_late_ = late_direct_from_seller_signals;
    direct_from_seller_per_buyer_signals_ =
        GURL("https://url.test/perbuyersignals");
    direct_from_seller_auction_signals_ =
        GURL("https://url.test/auctionsignals");

    mojo::Remote<mojom::BidderWorklet> bidder_worklet1 = CreateWorklet();
    AddResponse(&url_loader_factory_, *direct_from_seller_per_buyer_signals_,
                kJsonMimeType, /*charset=*/std::nullopt, kWorklet1JsonResponse,
                kDirectFromSellerSignalsHeaders);
    AddResponse(&url_loader_factory_, *direct_from_seller_auction_signals_,
                kJsonMimeType, /*charset=*/std::nullopt, kWorklet1JsonResponse,
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
                /*charset=*/std::nullopt, kWorklet2JsonResponse,
                kDirectFromSellerSignalsHeaders);
    AddResponse(&alternate_url_loader_factory_,
                *direct_from_seller_auction_signals_, kJsonMimeType,
                /*charset=*/std::nullopt, kWorklet2JsonResponse,
                kDirectFromSellerSignalsHeaders);
    AddJavascriptResponse(
        &alternate_url_loader_factory_, interest_group_bidding_url_,
        CreateGenerateBidScript(/*raw_return_value=*/kRawReturnValue,
                                /*extra_code=*/kWorklet2ExtraCode));
    generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
    GenerateBid(bidder_worklet1.get());
    generate_bid_run_loop_->Run();
    EXPECT_THAT(bid_errors_, ::testing::UnorderedElementsAre());

    generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
    GenerateBid(bidder_worklet2.get());
    generate_bid_run_loop_->Run();
    EXPECT_THAT(bid_errors_, ::testing::UnorderedElementsAre());
    generate_bid_run_loop_.reset();
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
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, R"("foo")", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  auction_signals_ = "[1]";
  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "[1]", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
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
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, R"("foo")", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  per_buyer_signals_ = "[1]";
  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "[1]", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  per_buyer_signals_ = std::nullopt;
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: perBuyerSignals === null, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "true", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
}

TEST_F(BidderWorkletTest,
       GenerateBidDirectFromSellerSignalsHeaderAdSlotAuctionSignals) {
  const std::string kGenerateBidBody =
      R"({ad: directFromSellerSignals.auctionSignals,
           bid:1, render:"https://response.test/"})";

  direct_from_seller_auction_signals_header_ad_slot_ = R"("foo")";
  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, R"("foo")", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  direct_from_seller_auction_signals_header_ad_slot_ = "[1]";
  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "[1]", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
}

TEST_F(BidderWorkletTest,
       GenerateBidDirectFromSellerSignalsHeaderAdSlotPerBuyerSignals) {
  const std::string kGenerateBidBody =
      R"({ad: directFromSellerSignals.perBuyerSignals,
           bid:1, render:"https://response.test/"})";

  direct_from_seller_per_buyer_signals_header_ad_slot_ = R"("foo")";
  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, R"("foo")", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  direct_from_seller_per_buyer_signals_header_ad_slot_ = "[1]";
  RunGenerateBidWithReturnValueExpectingResult(
      kGenerateBidBody,
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "[1]", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
}

TEST_F(BidderWorkletTest, GenerateBidBrowserSignalSellerOrigin) {
  browser_signal_seller_origin_ =
      url::Origin::Create(GURL("https://foo.test/"));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: browserSignals.seller, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon,
          R"("https://foo.test")", 1, /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  browser_signal_seller_origin_ =
      url::Origin::Create(GURL("https://[::1]:40000/"));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: browserSignals.seller, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon,
          R"("https://[::1]:40000")", 1, /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
}

TEST_F(BidderWorkletTest, GenerateBidBrowserSignalsAdComponentsLimit) {
  // Default is now limit of 40.
  RunGenerateBidExpectingExpressionIsTrue(
      "browserSignals.adComponentsLimit === 40");
}

TEST_F(BidderWorkletCustomAdComponentLimitTest,
       GenerateBidBrowserSignalsAdComponentsLimit) {
  RunGenerateBidExpectingExpressionIsTrue(
      "browserSignals.adComponentsLimit === 25");
}

TEST_F(BidderWorkletMultiBidDisabledTest, GenerateBidMultiBidLimit) {
  // If feature not enabled.
  RunGenerateBidExpectingExpressionIsTrue(
      "!('multiBidLimit' in browserSignals)");
}

TEST_F(BidderWorkletTest, GenerateBidMultiBidLimit) {
  multi_bid_limit_ = 143;
  RunGenerateBidExpectingExpressionIsTrue(
      "browserSignals.multiBidLimit === 143");

  multi_bid_limit_ = 10;
  RunGenerateBidExpectingExpressionIsTrue(
      "browserSignals.multiBidLimit === 10");
}

TEST_F(BidderWorkletTest, GenerateBidBrowserSignalTopLevelSellerOrigin) {
  browser_signal_top_level_seller_origin_ = std::nullopt;
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "topLevelSeller" in browserSignals,
          bid:1,
          render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "false", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  // Need to set `allowComponentAuction` to true for a bid to be created when
  // topLevelSeller is non-null.
  browser_signal_top_level_seller_origin_ =
      url::Origin::Create(GURL("https://foo.test"));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: browserSignals.topLevelSeller, bid:1, render:"https://response.test/",
          allowComponentAuction: true})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon,
          R"("https://foo.test")", 1, /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
}

TEST_F(BidderWorkletTest, GenerateBidBrowserSignalTopWindowOrigin) {
  top_window_origin_ = url::Origin::Create(GURL("https://top.window.test/"));
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: browserSignals.topWindowHostname, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon,
          R"("top.window.test")", 1, /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
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
        mojom::BidderWorkletBid::New(
            auction_worklet::mojom::BidRole::kUnenforcedKAnon, "0", 1,
            /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
            blink::AdDescriptor(GURL("https://response.test/")),
            /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
            /*ad_component_descriptors=*/std::nullopt,
            /*modeling_signals=*/std::nullopt, base::TimeDelta()));

    *test_case.value_ptr = 10;
    RunGenerateBidWithReturnValueExpectingResult(
        base::StringPrintf(
            R"({ad: %s, bid:1, render:"https://response.test/"})",
            test_case.name),
        mojom::BidderWorkletBid::New(
            auction_worklet::mojom::BidRole::kUnenforcedKAnon, "10", 1,
            /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
            blink::AdDescriptor(GURL("https://response.test/")),
            /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
            /*ad_component_descriptors=*/std::nullopt,
            /*modeling_signals=*/std::nullopt, base::TimeDelta()));
    SetDefaultParameters();
  }
}

TEST_F(BidderWorkletTest,
       GenerateBidBrowserSignalForDebuggingOnlyInCooldownOrLockout) {
  RunGenerateBidExpectingExpressionIsTrue(R"(
    browserSignals.forDebuggingOnlyInCooldownOrLockout === false;
  )");

  browser_signal_for_debugging_only_in_cooldown_or_lockout_ = true;
  RunGenerateBidExpectingExpressionIsTrue(R"(
    browserSignals.forDebuggingOnlyInCooldownOrLockout === true;
  )");
}

TEST_F(BidderWorkletTest, GenerateBidAds) {
  // A bid URL that's not in the InterestGroup's ads list should fail.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: 0, bid:1, render:"https://response2.test/"})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ generateBid() bid render URL "
       "'https://response2.test/' isn't one of the registered creative URLs."});

  // Adding an ad with a corresponding `renderURL` should result in success.
  // Also check the `interestGroup.ads` field passed to Javascript.
  interest_group_ads_.emplace_back(GURL("https://response2.test/"),
                                   /*metadata=*/R"(["metadata"])");
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: interestGroup.ads, bid:1, render:"https://response2.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon,
          "[{\"renderURL\":\"https://response.test/\","
          "\"renderUrl\":\"https://response.test/\"},"
          "{\"renderURL\":\"https://response2.test/\","
          "\"renderUrl\":\"https://response2.test/\","
          "\"metadata\":[\"metadata\"]}]",
          1, /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response2.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  // Make sure `metadata` is treated as an object, instead of a raw string.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: interestGroup.ads[1].metadata[0], bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"metadata\"", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
}

// Verify generateBid can see the reporting Ids when
// `selectable_buyer_and_seller_reporting_ids` is present.
TEST_F(BidderWorkletTest, GenerateBidAdsWithAllReportingIds) {
  // Ad with all reporting ids.
  interest_group_ads_.clear();
  interest_group_ads_.emplace_back(
      GURL("https://response2.test/"),
      /*metadata=*/std::nullopt, /*size_group=*/std::nullopt,
      /*buyer_reporting_id=*/"buyer_reporting_id",
      /*buyer_and_seller_reporting_id=*/"buyer_and_seller_reporting_id",
      /*selectable_buyer_and_seller_reporting_ids=*/
      std::vector<std::string>{"selectable_id1", "selectable_id2"});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: interestGroup.ads, bid:1, render:"https://response2.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon,
          "[{\"renderURL\":\"https://response2.test/\","
          "\"renderUrl\":\"https://response2.test/\","
          "\"buyerReportingId\":\"buyer_reporting_id\","
          "\"buyerAndSellerReportingId\":\"buyer_and_seller_reporting_id\","
          "\"selectableBuyerAndSellerReportingIds\":[\"selectable_id1\","
          "\"selectable_id2\"]}]",
          1, /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response2.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  // Ad with only selectable reporting ids.
  interest_group_ads_.clear();
  interest_group_ads_.emplace_back(
      GURL("https://response2.test/"),
      /*metadata=*/std::nullopt, /*size_group=*/std::nullopt,
      /*buyer_reporting_id=*/std::nullopt,
      /*buyer_and_seller_reporting_id=*/std::nullopt,
      /*selectable_buyer_and_seller_reporting_ids=*/
      std::vector<std::string>{"selectable_id1", "selectable_id2"});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: interestGroup.ads, bid:1, render:"https://response2.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon,
          "[{\"renderURL\":\"https://response2.test/\","
          "\"renderUrl\":\"https://response2.test/\","
          "\"selectableBuyerAndSellerReportingIds\":[\"selectable_id1\","
          "\"selectable_id2\"]}]",
          1, /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response2.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
}

TEST_F(BidderWorkletTest,
       GenerateBidDoesNotContainSelectedReportingIdsWhenFlagDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      blink::features::kFledgeAuctionDealSupport);

  interest_group_ads_.clear();
  interest_group_ads_.emplace_back(
      GURL("https://response2.test/"),
      /*metadata=*/std::nullopt, /*size_group=*/std::nullopt,
      /*buyer_reporting_id=*/std::nullopt,
      /*buyer_and_seller_reporting_id=*/std::nullopt,
      /*selectable_buyer_and_seller_reporting_ids=*/
      std::vector<std::string>{"selectable_id1", "selectable_id2"});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: interestGroup.ads, bid:1,
      render:"https://response2.test/",
      selectedBuyerAndSellerReportingId: "selectable_id1"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon,
          "[{\"renderURL\":\"https://response2.test/\","
          "\"renderUrl\":\"https://response2.test/\","
          "\"selectableBuyerAndSellerReportingIds\":[\"selectable_id1\","
          "\"selectable_id2\"]}]",
          1, /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response2.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
}

TEST_F(BidderWorkletTest, GenerateBidReturnsSelectedReportingId) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      blink::features::kFledgeAuctionDealSupport);
  interest_group_ads_.clear();
  interest_group_ads_.emplace_back(
      GURL("https://response2.test/"),
      /*metadata=*/std::nullopt, /*size_group=*/std::nullopt,
      /*buyer_reporting_id=*/std::nullopt,
      /*buyer_and_seller_reporting_id=*/std::nullopt,
      /*selectable_buyer_and_seller_reporting_ids=*/
      std::vector<std::string>{"selectable_id1", "selectable_id2"});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: interestGroup.ads, bid:1, render:"https://response2.test/", selectedBuyerAndSellerReportingId: "selectable_id1"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon,
          "[{\"renderURL\":\"https://response2.test/\","
          "\"renderUrl\":\"https://response2.test/\","
          "\"selectableBuyerAndSellerReportingIds\":[\"selectable_id1\","
          "\"selectable_id2\"]}]",
          1, /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response2.test/")),
          /*selected_buyer_and_seller_reporting_id=*/"selectable_id1",
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
}

TEST_F(BidderWorkletTest, GenerateBidWithInvalidSelectedReportingId) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      blink::features::kFledgeAuctionDealSupport);
  interest_group_ads_.clear();
  interest_group_ads_.emplace_back(
      GURL("https://response2.test/"),
      /*metadata=*/std::nullopt, /*size_group=*/std::nullopt,
      /*buyer_reporting_id=*/std::nullopt,
      /*buyer_and_seller_reporting_id=*/std::nullopt,
      /*selectable_buyer_and_seller_reporting_ids=*/
      std::vector<std::string>{"selectable_id1", "selectable_id2"});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: interestGroup.ads, bid:1, render:"https://response2.test/",
          selectedBuyerAndSellerReportingId: "invalid_id_not_in_array"})",
      mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt, /*expected_errors=*/
      {"https://url.test/ generateBid() Invalid selected buyer and seller "
       "reporting id"});
}

// Verify generateBid cannot see the reporting Ids when
// `selectable_buyer_and_seller_reporting_ids` is not present.
TEST_F(BidderWorkletTest, GenerateBidAdsWithoutReportingIds) {
  // Ads without selectable should not see any reporting ids in generate bid.
  interest_group_ads_.clear();
  interest_group_ads_.emplace_back(
      GURL("https://response2.test/"),
      /*metadata=*/std::nullopt, /*size_group=*/std::nullopt,
      /*buyer_reporting_id=*/"buyer_reporting_id",
      /*buyer_and_seller_reporting_id=*/"buyer_and_seller_reporting_id",
      /*selectable_buyer_and_seller_reporting_ids=*/std::nullopt);
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: interestGroup.ads, bid:1, render:"https://response2.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon,
          "[{\"renderURL\":\"https://response2.test/\","
          "\"renderUrl\":\"https://response2.test/\"}]",
          1, /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response2.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
}

TEST_F(BidderWorkletTest, GenerateBidAdComponents) {
  // Basic test with an adComponent URL.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: 0, bid:1, render:"https://response.test/", adComponents:["https://ad_component.test/"]})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "0", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          std::vector<blink::AdDescriptor>{
              blink::AdDescriptor(GURL("https://ad_component.test/"))},
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
  // Empty, but non-null, adComponents field.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: 0, bid:1, render:"https://response.test/", adComponents:[]})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "0", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          std::vector<blink::AdDescriptor>(),
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  // An adComponent URL that's not in the InterestGroup's adComponents list
  // should fail.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: 0, bid:1, render:"https://response.test/", adComponents:["https://response.test/"]})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ generateBid() bid adComponents URL "
       "'https://response.test/' isn't one of the registered creative URLs."});

  // Add a second ad component URL, this time with metadata.
  // Returning a list with both ads should result in success.
  // Also check the `interestGroup.ads` field passed to Javascript.
  interest_group_ad_components_->emplace_back(
      GURL("https://ad_component2.test/"), /*metadata=*/R"(["metadata"])");
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: interestGroup.adComponents,
        bid:1,
        render:"https://response.test/",
        adComponents:["https://ad_component.test/", "https://ad_component2.test/"]})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon,
          "[{\"renderURL\":\"https://ad_component.test/\","
          "\"renderUrl\":\"https://ad_component.test/\"},"
          "{\"renderURL\":\"https://ad_component2.test/\","
          "\"renderUrl\":\"https://ad_component2.test/\","
          "\"metadata\":[\"metadata\"]}]",
          1, /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          std::vector<blink::AdDescriptor>{
              blink::AdDescriptor(GURL("https://ad_component.test/")),
              blink::AdDescriptor(GURL("https://ad_component2.test/"))},
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
}

// Test behavior of the `allowComponentAuction` output field, which can block
// bids when not set to true and `topLevelSellerOrigin` is non-null.
TEST_F(BidderWorkletTest, GenerateBidAllowComponentAuction) {
  // In all success cases, this is the returned bid.
  const auto kBidOnSuccess = mojom::BidderWorkletBid::New(
      auction_worklet::mojom::BidRole::kUnenforcedKAnon, "null", 1,
      /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
      blink::AdDescriptor(GURL("https://response.test/")),
      /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
      /*ad_component_descriptors=*/std::nullopt,
      /*modeling_signals=*/std::nullopt, base::TimeDelta());

  // Use a null `topLevelSellerOrigin`. `allowComponentAuction` value should be
  // ignored.
  browser_signal_top_level_seller_origin_ = std::nullopt;
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
      /*expected_data_version=*/std::nullopt, /*expected_errors=*/
      {"https://url.test/ generateBid() bid does not have "
       "allowComponentAuction set to true. Bid dropped from component "
       "auction."});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: null, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt, /*expected_errors=*/
      {"https://url.test/ generateBid() bid does not have "
       "allowComponentAuction set to true. Bid dropped from component "
       "auction."});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: null, bid:1, render:"https://response.test/", allowComponentAuction: 0})",
      mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt, /*expected_errors=*/
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
              /*charset=*/std::nullopt, "Error 404", kAllowFledgeHeader,
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
              /*charset=*/std::nullopt, CreateBasicGenerateBidScript());

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
              /*charset=*/std::nullopt, ToyWasm());

  RunGenerateBidWithJavascriptExpectingResult(
      bid_script, mojom::BidderWorkletBid::New(
                      auction_worklet::mojom::BidRole::kUnenforcedKAnon,
                      R"([{"name":"test_const","kind":"global"}])", 1,
                      /*bid_currency=*/std::nullopt,
                      /*ad_cost=*/std::nullopt,
                      blink::AdDescriptor(GURL("https://response.test/")),
                      /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
                      /*ad_component_descriptors=*/std::nullopt,
                      /*modeling_signals=*/std::nullopt, base::TimeDelta()));
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
  base::WaitableEvent* event_handle = WedgeV8Thread(v8_helper().get());

  base::RunLoop run_loop;
  bidder_worklet->ReportWin(
      is_for_additional_bid_, interest_group_name_reporting_id_,
      buyer_reporting_id_, buyer_and_seller_reporting_id_,
      selected_buyer_and_seller_reporting_id_,
      /*auction_signals_json=*/"0", per_buyer_signals_,
      direct_from_seller_per_buyer_signals_,
      direct_from_seller_per_buyer_signals_header_ad_slot_,
      direct_from_seller_auction_signals_,
      direct_from_seller_auction_signals_header_ad_slot_, seller_signals_,
      kanon_mode_, bid_is_kanon_, browser_signal_render_url_,
      browser_signal_bid_, browser_signal_bid_currency_,
      browser_signal_highest_scoring_other_bid_,
      browser_signal_highest_scoring_other_bid_currency_,
      browser_signal_made_highest_scoring_other_bid_, browser_signal_ad_cost_,
      browser_signal_modeling_signals_, browser_signal_join_count_,
      browser_signal_recency_report_win_, browser_signal_seller_origin_,
      browser_signal_top_level_seller_origin_,
      browser_signal_reporting_timeout_, data_version_,
      /*trace_id=*/1,
      base::BindLambdaForTesting(
          [&run_loop](
              const std::optional<GURL>& report_url,
              const base::flat_map<std::string, GURL>& ad_beacon_map,
              const base::flat_map<std::string, std::string>& ad_macro_map,
              PrivateAggregationRequests pa_requests,
              auction_worklet::mojom::BidderTimingMetricsPtr timing_metrics,
              const std::vector<std::string>& errors) { run_loop.Quit(); }));
  base::RunLoop().RunUntilIdle();
  AddResponse(&url_loader_factory_, GURL(kWasmUrl), kWasmMimeType,
              /*charset=*/std::nullopt, ToyWasm());
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
      /*expected_ad_macro_map=*/{},
      /*expected_pa_requests=*/{},
      /*expected_reporting_latency_timeout=*/false,
      /*expected_errors=*/{},
      base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }));
  task_environment_.RunUntilIdle();
  AddResponse(&url_loader_factory_, GURL(kWasmUrl), kWasmMimeType,
              /*charset=*/std::nullopt, ToyWasm());
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
      generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
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
                      /*charset=*/std::nullopt, ToyWasm());
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
      }
      task_environment_.RunUntilIdle();
    }

    if (test.expect_success) {
      // On success, the callback is invoked.
      generate_bid_run_loop_->Run();
      generate_bid_run_loop_.reset();
      EXPECT_TRUE(!bids_.empty());
    } else {
      // On failure, the pipe is closed with a non-empty error message, without
      // invoking the callback.
      EXPECT_FALSE(WaitForDisconnect().empty());
    }
  }
}

// Utility method to create a vector of PreviousWin. Needed because StructPtrs
// don't allow copying.
std::vector<blink::mojom::PreviousWinPtr> CreateWinList(
    const blink::mojom::PreviousWinPtr& win1,
    const blink::mojom::PreviousWinPtr& win2 = blink::mojom::PreviousWinPtr(),
    const blink::mojom::PreviousWinPtr& win3 = blink::mojom::PreviousWinPtr()) {
  std::vector<mojo::StructPtr<blink::mojom::PreviousWin>> out;
  out.emplace_back(win1.Clone());
  if (win2) {
    out.emplace_back(win2.Clone());
  }
  if (win3) {
    out.emplace_back(win3.Clone());
  }
  return out;
}

TEST_F(BidderWorkletTest, GenerateBidPrevWins) {
  base::TimeDelta delta = base::Seconds(100);
  base::TimeDelta tiny_delta = base::Milliseconds(500);

  base::Time time1 = auction_start_time_ - delta - delta;
  base::Time time2 = auction_start_time_ - delta - tiny_delta;
  base::Time future_time = auction_start_time_ + delta;

  auto win1 = blink::mojom::PreviousWin::New(time1, R"({"renderURL":"ad1"})");
  auto win2 = blink::mojom::PreviousWin::New(
      time2, R"({"renderURL":"ad2", "metadata":"{\"key\":\"value\"}"})");
  auto future_win = blink::mojom::PreviousWin::New(
      future_time, R"({"renderURL":"future_ad"})");
  struct TestCase {
    std::vector<mojo::StructPtr<blink::mojom::PreviousWin>> prev_wins;
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
          R"([[200,{"renderURL":"ad1","render_url":"ad1"}]])",
      },
      // Make sure it's passed on as an object and not a string.
      {
          CreateWinList(win1),
          "browserSignals.prevWins[0]",
          R"([200,{"renderURL":"ad1","render_url":"ad1"}])",
      },
      // Test rounding.
      {
          CreateWinList(win2),
          "browserSignals.prevWins",
          R"([[100,{"renderURL":"ad2",)"
          R"("metadata":{"key":"value"},"render_url":"ad2"}]])",
      },
      // Multiple previous wins.
      {
          CreateWinList(win1, win2),
          "browserSignals.prevWins",
          R"([[200,{"renderURL":"ad1","render_url":"ad1"}],)"
          R"([100,{"renderURL":"ad2",)"
          R"("metadata":{"key":"value"},"render_url":"ad2"}]])",
      },
      // Times are trimmed at 0.
      {
          CreateWinList(future_win),
          "browserSignals.prevWins",
          R"([[0,{"renderURL":"future_ad","render_url":"future_ad"}]])",
      },
      // Out of order wins should be sorted.
      {
          CreateWinList(win2, future_win, win1),
          "browserSignals.prevWins",
          R"([[200,{"renderURL":"ad1","render_url":"ad1"}],)"
          R"([100,{"renderURL":"ad2",)"
          R"("metadata":{"key":"value"},"render_url":"ad2"}],)"
          R"([0,{"renderURL":"future_ad","render_url":"future_ad"}]])",
      },
      // Same as above, but for prevWinsMs.
      {
          {},
          "browserSignals.prevWinsMs",
          "[]",
      },
      {
          CreateWinList(win1),
          "browserSignals.prevWinsMs",
          R"([[200000,{"renderURL":"ad1","render_url":"ad1"}]])",
      },
      // Make sure it's passed on as an object and not a string.
      {
          CreateWinList(win1),
          "browserSignals.prevWinsMs[0]",
          R"([200000,{"renderURL":"ad1","render_url":"ad1"}])",
      },
      // Test rounding.
      {
          CreateWinList(win2),
          "browserSignals.prevWinsMs",
          R"([[100000,{"renderURL":"ad2",)"
          R"("metadata":{"key":"value"},"render_url":"ad2"}]])",
      },
      // Multiple previous wins.
      {
          CreateWinList(win1, win2),
          "browserSignals.prevWinsMs",
          R"([[200000,{"renderURL":"ad1","render_url":"ad1"}],)"
          R"([100000,{"renderURL":"ad2",)"
          R"("metadata":{"key":"value"},"render_url":"ad2"}]])",
      },
      // Times are trimmed at 0.
      {
          CreateWinList(future_win),
          "browserSignals.prevWinsMs",
          R"([[0,{"renderURL":"future_ad","render_url":"future_ad"}]])",
      },
      // Out of order wins should be sorted.
      {
          CreateWinList(win2, future_win, win1),
          "browserSignals.prevWinsMs",
          R"([[200000,{"renderURL":"ad1","render_url":"ad1"}],)"
          R"([100000,{"renderURL":"ad2",)"
          R"("metadata":{"key":"value"},"render_url":"ad2"}],)"
          R"([0,{"renderURL":"future_ad","render_url":"future_ad"}]])",
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
            auction_worklet::mojom::BidRole::kUnenforcedKAnon,
            test_case.expected_ad, 1, /*bid_currency=*/std::nullopt,
            /*ad_cost=*/std::nullopt,
            blink::AdDescriptor(GURL("https://response.test/")),
            /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
            /*ad_component_descriptors=*/std::nullopt,
            /*modeling_signals=*/std::nullopt, base::TimeDelta()));
  }
}

TEST_F(BidderWorkletTest, GenerateBidTrustedBiddingSignals) {
  const GURL kBaseSignalsUrl("https://signals.test/");
  interest_group_bidding_url_ = kBaseSignalsUrl;
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
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "null", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
  EXPECT_EQ(observed_requests += 1, url_loader_factory_.total_requests());

  // Request with TrustedBiddingSignals keys and null URL. No request should be
  // made.
  interest_group_trusted_bidding_signals_keys_ =
      std::vector<std::string>({"key1", "key2"});
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: trustedBiddingSignals, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "null", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
  EXPECT_EQ(observed_requests += 1, url_loader_factory_.total_requests());

  // Request with TrustedBiddingSignals URL and null keys. Request should be
  // made without any keys, and nothing from the response passed to
  // generateBid().
  interest_group_trusted_bidding_signals_url_ = kBaseSignalsUrl;
  interest_group_trusted_bidding_signals_keys_.reset();
  AddBidderJsonResponse(&url_loader_factory_, kNoKeysSignalsUrl, kJson);
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: trustedBiddingSignals, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "null", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
  EXPECT_EQ(observed_requests += 2, url_loader_factory_.total_requests());

  // Request with TrustedBiddingSignals URL and empty keys. Request should be
  // made without any keys, and nothing from the response passed to
  // generateBid().
  interest_group_trusted_bidding_signals_keys_ = std::vector<std::string>();
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: trustedBiddingSignals, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "null", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
  EXPECT_EQ(observed_requests += 2, url_loader_factory_.total_requests());
  url_loader_factory_.ClearResponses();

  // Request with valid TrustedBiddingSignals URL and non-empty keys. Request
  // should be made. The request fails.
  interest_group_trusted_bidding_signals_keys_ =
      std::vector<std::string>({"key1", "key2"});
  url_loader_factory_.AddResponse(kFullSignalsUrl.spec(), kJson,
                                  net::HTTP_NOT_FOUND);
  mojom::RealTimeReportingContribution expected_trusted_signal_histogram(
      /*bucket=*/1024 + auction_worklet::RealTimeReportingPlatformError::
                            kTrustedBiddingSignalsFailure,
      /*priority_weight=*/1,
      /*latency_threshold=*/std::nullopt);
  RealTimeReportingContributions expected_real_time_contributions;
  expected_real_time_contributions.push_back(
      expected_trusted_signal_histogram.Clone());
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: trustedBiddingSignals, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "null", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()),
      /*expected_data_version=*/std::nullopt,
      {"Failed to load "
       "https://signals.test/"
       "?hostname=top.window.test&keys=key1,key2&interestGroupNames=Fred HTTP "
       "status = 404 Not Found."},
      std::nullopt, std::nullopt,
      /*expected_set_priority=*/std::nullopt,
      /*expected_update_priority_signals_overrides=*/{},
      /*expected_pa_requests=*/{},
      /*expected_non_kanon_pa_requests=*/{},
      std::move(expected_real_time_contributions));
  EXPECT_EQ(observed_requests += 2, url_loader_factory_.total_requests());

  // Request with valid TrustedBiddingSignals URL and non-empty keys. Request
  // should be made. The request succeeds.
  AddBidderJsonResponse(&url_loader_factory_, kFullSignalsUrl, kJson);
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: trustedBiddingSignals, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon,
          R"({"key1":1,"key2":[2]})", 1, /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
  EXPECT_EQ(observed_requests += 2, url_loader_factory_.total_requests());
}

// With the cross-origin trusted signals flag off, nothing is passed in to the
// cross-original signals parameter.
TEST_F(BidderWorkletCrossOriginTrustedSignalsDisabledTest, Basic) {
  RunGenerateBidExpectingExpressionIsTrue(
      "crossOriginTrustedSignals === undefined");
  RunGenerateBidExpectingExpressionIsTrue("arguments.length === 6");
}

TEST_F(BidderWorkletTest, GenerateBidTrustedBiddingSignalsV1) {
  const GURL kFullSignalsUrl(
      "https://signals.test/"
      "?hostname=top.window.test&keys=key1,key2&interestGroupNames=Fred");

  const char kJson[] = R"(
    {
      "key1": 1,
      "key2": [2]
    }
  )";

  interest_group_trusted_bidding_signals_url_ = GURL("https://signals.test/");
  interest_group_bidding_url_ = *interest_group_trusted_bidding_signals_url_;
  interest_group_trusted_bidding_signals_keys_ =
      std::vector<std::string>({"key1", "key2"});

  // Request with valid TrustedBiddingSignals URL and non-empty keys. Request
  // should be made. The request succeeds.
  AddBidderJsonResponse(&url_loader_factory_, kFullSignalsUrl, kJson,
                        /*data_version=*/std::nullopt,
                        /*format_version_string=*/std::nullopt);
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: trustedBiddingSignals, bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon,
          R"({"key1":1,"key2":[2]})", 1, /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/
      {"Bidding signals URL https://signals.test/ is using outdated bidding "
       "signals format. Consumers should be updated to use bidding signals "
       "format version 2"});
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
          base::TimeDelta trusted_signals_fetch_latency,
          std::optional<base::TimeDelta> update_if_older_than,
          base::OnceClosure callback) {
        EXPECT_TRUE(priority_vector.empty());
        EXPECT_EQ(base::TimeDelta(), trusted_signals_fetch_latency);
        on_bidding_signals_received_run_loop.Quit();
        on_bidding_signals_received_continue_callback = std::move(callback);
      });
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  GenerateBid(bidder_worklet.get(),
              GenerateBidClientWithCallbacks::Create(
                  base::BindOnce(&BidderWorkletTest::GenerateBidCallback,
                                 base::Unretained(this)),
                  std::move(on_bidding_signals_received_callback)));

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(on_bidding_signals_received_run_loop.AnyQuitCalled());
  EXPECT_FALSE(generate_bid_run_loop_->AnyQuitCalled());
  ASSERT_TRUE(on_bidding_signals_received_continue_callback);

  std::move(on_bidding_signals_received_continue_callback).Run();
  generate_bid_run_loop_->Run();
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
          base::TimeDelta trusted_signals_fetch_latency,
          std::optional<base::TimeDelta> update_if_older_than,
          base::OnceClosure callback) {
        EXPECT_TRUE(priority_vector.empty());
        on_bidding_signals_received_run_loop.Quit();
        on_bidding_signals_received_continue_callback = std::move(callback);
      });
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  GenerateBid(bidder_worklet.get(),
              GenerateBidClientWithCallbacks::Create(
                  base::BindOnce(&BidderWorkletTest::GenerateBidCallback,
                                 base::Unretained(this)),
                  std::move(on_bidding_signals_received_callback)));

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(on_bidding_signals_received_run_loop.AnyQuitCalled());
  EXPECT_FALSE(generate_bid_run_loop_->AnyQuitCalled());

  url_loader_factory_.AddResponse(kFullSignalsUrl.spec(), /*content=*/"",
                                  net::HTTP_NOT_FOUND);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(on_bidding_signals_received_run_loop.AnyQuitCalled());
  EXPECT_FALSE(generate_bid_run_loop_->AnyQuitCalled());
  ASSERT_TRUE(on_bidding_signals_received_continue_callback);

  std::move(on_bidding_signals_received_continue_callback).Run();
  generate_bid_run_loop_->Run();
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
          base::TimeDelta trusted_signals_fetch_latency,
          std::optional<base::TimeDelta> update_if_older_than,
          base::OnceClosure callback) {
        EXPECT_TRUE(priority_vector.empty());
        on_bidding_signals_received_run_loop.Quit();
        on_bidding_signals_received_continue_callback = std::move(callback);
      });
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  GenerateBid(bidder_worklet.get(),
              GenerateBidClientWithCallbacks::Create(
                  base::BindOnce(&BidderWorkletTest::GenerateBidCallback,
                                 base::Unretained(this)),
                  std::move(on_bidding_signals_received_callback)));

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(on_bidding_signals_received_run_loop.AnyQuitCalled());
  EXPECT_FALSE(generate_bid_run_loop_->AnyQuitCalled());

  AddBidderJsonResponse(&url_loader_factory_, kFullSignalsUrl, kJson);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(on_bidding_signals_received_run_loop.AnyQuitCalled());
  EXPECT_FALSE(generate_bid_run_loop_->AnyQuitCalled());
  ASSERT_TRUE(on_bidding_signals_received_continue_callback);

  std::move(on_bidding_signals_received_continue_callback).Run();
  generate_bid_run_loop_->Run();
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
          base::TimeDelta trusted_signals_fetch_latency,
          std::optional<base::TimeDelta> update_if_older_than,
          base::OnceClosure callback) {
        EXPECT_TRUE(priority_vector.empty());
        EXPECT_EQ(std::nullopt, update_if_older_than);
        on_bidding_signals_received_run_loop.Quit();
        on_bidding_signals_received_continue_callback = std::move(callback);
      });
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  GenerateBid(bidder_worklet.get(),
              GenerateBidClientWithCallbacks::Create(
                  base::BindOnce(&BidderWorkletTest::GenerateBidCallback,
                                 base::Unretained(this)),
                  std::move(on_bidding_signals_received_callback)));

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(on_bidding_signals_received_run_loop.AnyQuitCalled());
  EXPECT_FALSE(generate_bid_run_loop_->AnyQuitCalled());

  AddBidderJsonResponse(&url_loader_factory_, kFullSignalsUrl, kJson);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(on_bidding_signals_received_run_loop.AnyQuitCalled());
  EXPECT_FALSE(generate_bid_run_loop_->AnyQuitCalled());
  ASSERT_TRUE(on_bidding_signals_received_continue_callback);

  std::move(on_bidding_signals_received_continue_callback).Run();
  generate_bid_run_loop_->Run();
}

// Same as GenerateBidOnBiddingSignalsReceivedNoPriorityVectorReceived, but
// the priority vector is received, and verified in the signals received
// callback.
TEST_F(BidderWorkletTest,
       GenerateBidOnBiddingSignalsReceivedPriorityVectorReceived) {
  interest_group_trusted_bidding_signals_url_ = GURL("https://signals.test/");
  const GURL kFullSignalsUrl(
      "https://signals.test/?hostname=top.window.test&interestGroupNames=Fred");

  const char kJson[] = R"({"perInterestGroupData":
                            {"Fred": {"priorityVector": {"foo": 1.0}}}
                          })";

  auto bidder_worklet = CreateWorklet();
  AddJavascriptResponse(
      &url_loader_factory_, interest_group_bidding_url_,
      CreateGenerateBidScript(CreateBasicGenerateBidScript()));

  base::RunLoop on_bidding_signals_received_run_loop;
  base::OnceClosure on_bidding_signals_received_continue_callback;
  auto on_bidding_signals_received_callback = base::BindLambdaForTesting(
      [&](const base::flat_map<std::string, double>& priority_vector,
          base::TimeDelta trusted_signals_fetch_latency,
          std::optional<base::TimeDelta> update_if_older_than,
          base::OnceClosure callback) {
        EXPECT_THAT(priority_vector,
                    UnorderedElementsAre(std::make_pair("foo", 1.0)));
        EXPECT_EQ(std::nullopt, update_if_older_than);
        on_bidding_signals_received_run_loop.Quit();
        on_bidding_signals_received_continue_callback = std::move(callback);
      });
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  GenerateBid(bidder_worklet.get(),
              GenerateBidClientWithCallbacks::Create(
                  base::BindOnce(&BidderWorkletTest::GenerateBidCallback,
                                 base::Unretained(this)),
                  std::move(on_bidding_signals_received_callback)));

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(on_bidding_signals_received_run_loop.AnyQuitCalled());
  EXPECT_FALSE(generate_bid_run_loop_->AnyQuitCalled());

  AddBidderJsonResponse(&url_loader_factory_, kFullSignalsUrl, kJson);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(on_bidding_signals_received_run_loop.AnyQuitCalled());
  EXPECT_FALSE(generate_bid_run_loop_->AnyQuitCalled());
  ASSERT_TRUE(on_bidding_signals_received_continue_callback);

  std::move(on_bidding_signals_received_continue_callback).Run();
  generate_bid_run_loop_->Run();
}

// Same as GenerateBidOnBiddingSignalsReceivedNoPriorityVectorReceived, but the
// updateIfOlderThanMs field is present, but priorityVector is not -- this is
// verified in the signals received callback.
TEST_F(
    BidderWorkletTest,
    GenerateBidOnBiddingSignalsReceivedNoPriorityVectorYesUpdateIfOlderThanMsReceived) {
  interest_group_trusted_bidding_signals_url_ = GURL("https://signals.test/");
  const GURL kFullSignalsUrl(
      "https://signals.test/?hostname=top.window.test&interestGroupNames=Fred");

  const char kJson[] = R"({"perInterestGroupData":
                            {"Fred": {"updateIfOlderThanMs": 3600000}}
                          })";

  auto bidder_worklet = CreateWorklet();
  AddJavascriptResponse(
      &url_loader_factory_, interest_group_bidding_url_,
      CreateGenerateBidScript(CreateBasicGenerateBidScript()));

  base::RunLoop on_bidding_signals_received_run_loop;
  base::OnceClosure on_bidding_signals_received_continue_callback;
  auto on_bidding_signals_received_callback = base::BindLambdaForTesting(
      [&](const base::flat_map<std::string, double>& priority_vector,
          base::TimeDelta trusted_signals_fetch_latency,
          std::optional<base::TimeDelta> update_if_older_than,
          base::OnceClosure callback) {
        EXPECT_THAT(priority_vector, IsEmpty());
        EXPECT_EQ(base::Milliseconds(3600000), update_if_older_than);
        on_bidding_signals_received_run_loop.Quit();
        on_bidding_signals_received_continue_callback = std::move(callback);
      });
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  GenerateBid(bidder_worklet.get(),
              GenerateBidClientWithCallbacks::Create(
                  base::BindOnce(&BidderWorkletTest::GenerateBidCallback,
                                 base::Unretained(this)),
                  std::move(on_bidding_signals_received_callback)));

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(on_bidding_signals_received_run_loop.AnyQuitCalled());
  EXPECT_FALSE(generate_bid_run_loop_->AnyQuitCalled());

  AddBidderJsonResponse(&url_loader_factory_, kFullSignalsUrl, kJson);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(on_bidding_signals_received_run_loop.AnyQuitCalled());
  EXPECT_FALSE(generate_bid_run_loop_->AnyQuitCalled());
  ASSERT_TRUE(on_bidding_signals_received_continue_callback);

  std::move(on_bidding_signals_received_continue_callback).Run();
  generate_bid_run_loop_->Run();
}

// Same as GenerateBidOnBiddingSignalsReceivedNoPriorityVectorReceived, but the
// priorityVector and updateIfOlderThanMs fields are present -- this is verified
// in the signals received callback.
TEST_F(
    BidderWorkletTest,
    GenerateBidOnBiddingSignalsReceivedYesPriorityVectorYesUpdateIfOlderThanMsReceived) {
  interest_group_trusted_bidding_signals_url_ = GURL("https://signals.test/");
  const GURL kFullSignalsUrl(
      "https://signals.test/?hostname=top.window.test&interestGroupNames=Fred");

  const char kJson[] = R"({"perInterestGroupData": {
                              "Fred": {
                                "priorityVector": {"foo": 1.0},
                                "updateIfOlderThanMs": 3600000
                              }
                            }
                          })";

  auto bidder_worklet = CreateWorklet();
  AddJavascriptResponse(
      &url_loader_factory_, interest_group_bidding_url_,
      CreateGenerateBidScript(CreateBasicGenerateBidScript()));

  base::RunLoop on_bidding_signals_received_run_loop;
  base::OnceClosure on_bidding_signals_received_continue_callback;
  auto on_bidding_signals_received_callback = base::BindLambdaForTesting(
      [&](const base::flat_map<std::string, double>& priority_vector,
          base::TimeDelta trusted_signals_fetch_latency,
          std::optional<base::TimeDelta> update_if_older_than,
          base::OnceClosure callback) {
        EXPECT_THAT(priority_vector,
                    UnorderedElementsAre(std::make_pair("foo", 1.0)));
        EXPECT_EQ(base::Milliseconds(3600000), update_if_older_than);
        on_bidding_signals_received_run_loop.Quit();
        on_bidding_signals_received_continue_callback = std::move(callback);
      });
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  GenerateBid(bidder_worklet.get(),
              GenerateBidClientWithCallbacks::Create(
                  base::BindOnce(&BidderWorkletTest::GenerateBidCallback,
                                 base::Unretained(this)),
                  std::move(on_bidding_signals_received_callback)));

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(on_bidding_signals_received_run_loop.AnyQuitCalled());
  EXPECT_FALSE(generate_bid_run_loop_->AnyQuitCalled());

  AddBidderJsonResponse(&url_loader_factory_, kFullSignalsUrl, kJson);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(on_bidding_signals_received_run_loop.AnyQuitCalled());
  EXPECT_FALSE(generate_bid_run_loop_->AnyQuitCalled());
  ASSERT_TRUE(on_bidding_signals_received_continue_callback);

  std::move(on_bidding_signals_received_continue_callback).Run();
  generate_bid_run_loop_->Run();
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
          base::TimeDelta trusted_signals_fetch_latency,
          std::optional<base::TimeDelta> update_if_older_than,
          base::OnceClosure callback) {
        EXPECT_TRUE(priority_vector.empty());
        on_bidding_signals_received_continue_callback = std::move(callback);
      });

  GenerateBidClientWithCallbacks client(
      GenerateBidClientWithCallbacks::GenerateBidNeverInvokedCallback(),
      std::move(on_bidding_signals_received_callback));
  mojo::AssociatedReceiver<mojom::GenerateBidClient> client_receiver(&client);

  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
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
  interest_group_bidding_url_ = *interest_group_trusted_bidding_signals_url_;
  interest_group_trusted_bidding_signals_keys_.emplace();
  interest_group_trusted_bidding_signals_keys_->push_back("key1");
  AddBidderJsonResponse(
      &url_loader_factory_,
      GURL("https://signals.test/"
           "?hostname=top.window.test&keys=key1&interestGroupNames=Fred"),
      R"({"keys":{"key1":1}})", /*data_version=*/7u);
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid:browserSignals.dataVersion, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, R"("ad")", 7,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()),
      /*expected_data_version=*/7u);
}

// Even with no trustedBiddingSignalsKeys, the data version should be available.
TEST_F(BidderWorkletTest, GenerateBidDataVersionNoKeys) {
  interest_group_trusted_bidding_signals_url_ = GURL("https://signals.test/");
  interest_group_bidding_url_ = *interest_group_trusted_bidding_signals_url_;
  AddBidderJsonResponse(
      &url_loader_factory_,
      GURL("https://signals.test/"
           "?hostname=top.window.test&interestGroupNames=Fred"),
      R"({})", /*data_version=*/7u);
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid:browserSignals.dataVersion, render:"https://response.test/"})",
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, R"("ad")", 7,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()),
      /*expected_data_version=*/7u);
}

// Even though the script had set an intermediate result with setBid, the
// returned value should be used instead.
TEST_F(BidderWorkletTest, GenerateBidWithSetBid) {
  interest_group_ads_.emplace_back(GURL("https://response.test/replacement"),
                                   /*metadata=*/std::nullopt);

  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          /*raw_return_value=*/
          R"({ad: "returned", bid:2, render:"https://response.test/replacement" })",
          /*extra_code=*/R"(
            setBid({ad: "ad", bid:1, render:"https://response.test/"})
          )"),
      /*expected_bids=*/
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"returned\"", 2,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/replacement")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  // Test the no-bid version as well.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          /*raw_return_value=*/
          R"({ad: "returned", bid:0, render:"https://response.test/" })",
          /*extra_code=*/R"(
            setBid({ad: "ad", bid:1, render:"https://response.test/"})
          )"),
      /*expected_bids=*/
      mojom::BidderWorkletBidPtr());

  // Test which ends by doing a return; (with no value specified).
  RunGenerateBidWithJavascriptExpectingResult(CreateGenerateBidScript(
                                                  /*raw_return_value=*/"",
                                                  /*extra_code=*/R"(
            setBid({ad: "ad", bid:1, render:"https://response.test/"})
          )"),
                                              /*expected_bids=*/
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
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, R"("ad")", 123,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
}

TEST_F(BidderWorkletTest, GenerateBidTimedOut) {
  // The bidding script has an endless while loop. It will time out due to
  // AuctionV8Helper's default script timeout (50 ms).
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(/*raw_return_value=*/"", R"(while (1))"),
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ execution of `generateBid` timed out."});
  EXPECT_TRUE(generate_bid_metrics_->script_timed_out);
}

TEST_F(BidderWorkletTest, GenerateBidTopLevelTimeout) {
  // The bidding script has an endless while loop. It will time out due to
  // AuctionV8Helper's default script timeout (50 ms).
  RunGenerateBidWithJavascriptExpectingResult(
      "while (1) {}",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ top-level execution timed out."});
  EXPECT_TRUE(generate_bid_metrics_->script_timed_out);
}

TEST_F(BidderWorkletTest, GenerateBidPerBuyerTimeOut) {
  // Use a very long default script timeout, and a short per buyer timeout, so
  // that if the bidder script with endless loop times out, we know that the per
  // buyer timeout overwrote the default script timeout and worked.
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

  per_buyer_timeout_ = base::Milliseconds(20);
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(/*raw_return_value=*/"", R"(while (1))"),
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ execution of `generateBid` timed out."});
  EXPECT_TRUE(generate_bid_metrics_->script_timed_out);
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
      /*expected_bids=*/
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, AuctionV8Helper::kScriptTimeout),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ execution of `generateBid` timed out."});
  EXPECT_TRUE(generate_bid_metrics_->script_timed_out);

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
      /*expected_bids=*/
      mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ execution of `generateBid` timed out."});
  EXPECT_TRUE(generate_bid_metrics_->script_timed_out);
}

// Test that per-buyer timeout of zero results in no bid produced.
TEST_F(BidderWorkletTest, PerBuyerTimeoutZero) {
  per_buyer_timeout_ = base::Seconds(0);
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid:1, render:"https://response.test/"})",
      mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/{"generateBid() aborted due to zero timeout."});
  EXPECT_TRUE(generate_bid_metrics_->script_timed_out);
}

// Test that in the case of multiple setBid() calls, the most recent call takes
// precedence. Note that this test sets `bid_duration` to
// `AuctionV8Helper::kScriptTimeout`, to make sure the full timeout time is
// included in the duration.
TEST_F(BidderWorkletTest, GenerateBidTimedOutWithSetBidTwice) {
  interest_group_ads_.emplace_back(GURL("https://response.test/replacement"),
                                   /*metadata=*/std::nullopt);

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
      /*expected_bids=*/
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad2\"", 2,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/replacement")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, AuctionV8Helper::kScriptTimeout),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ execution of `generateBid` timed out."});
  EXPECT_TRUE(generate_bid_metrics_->script_timed_out);

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
      /*expected_bids=*/
      mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ execution of `generateBid` timed out."});
  EXPECT_TRUE(generate_bid_metrics_->script_timed_out);

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
      /*expected_bids=*/
      mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ execution of `generateBid` timed out."});
  EXPECT_TRUE(generate_bid_metrics_->script_timed_out);
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
      /*expected_bids=*/
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()),
      /*expected_data_version=*/std::nullopt,
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
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/:6 Uncaught TypeError: setPriority(): at least 1 "
       "argument(s) are required."});
  // priority not a double
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          R"({ad: "ad", bid:1, render:"https://response.test/" })",
          /*extra_code=*/R"(
            setPriority("string");
          )"),
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/:6 Uncaught TypeError: setPriority(): Converting "
       "argument 'priority' to a Number did not produce a finite double."});
  // priority not finite
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          R"({ad: "ad", bid:1, render:"https://response.test/" })",
          /*extra_code=*/R"(
            setPriority(0.0/0.0);
          )"),
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/:6 Uncaught TypeError: setPriority(): Converting "
       "argument 'priority' to a Number did not produce a finite double."});
  // priority called twice
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          R"({ad: "ad", bid:1, render:"https://response.test/" })",
          /*extra_code=*/R"(
            setPriority(4);
            setPriority(4);
          )"),
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/:7 Uncaught TypeError: setPriority may be called at "
       "most once."});
  // success
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          R"({ad: "ad", bid:1, render:"https://response.test/" })",
          /*extra_code=*/R"(
            setPriority(9.0);
          )"),
      /*expected_bids=*/
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_set_priority=*/9.0);
  // set priority with no bid should work.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(R"({bid:0, render:"https://response.test/" })",
                              /*extra_code=*/R"(
            setPriority(9.0);
          )"),
      /*expected_bids=*/
      mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_set_priority=*/9.0);
  // set priority with an invalid bid should work.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(R"({bid:1/0, render:"https://response.test/" })",
                              /*extra_code=*/R"(
            setPriority(9.0);
          )"),
      /*expected_bids=*/
      mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/
      {"https://url.test/ generateBid() Converting field 'bid' to a Number did "
       "not produce a finite double."},
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_set_priority=*/9.0);

  // priority argument doesn't terminate
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          R"({ad: "ad", bid:1, render:"https://response.test/" })",
          /*extra_code=*/R"(
            setPriority({
              valueOf:() => {while(true) {}}
            });
          )"),
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ execution of `generateBid` timed out."});
}

TEST_F(BidderWorkletTest, GenerateBidSetPrioritySignalsOverrides) {
  // Not enough args.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(R"({bid:1, render:"https://response.test/" })",
                              /*extra_code=*/R"(
            setPrioritySignalsOverride();
          )"),
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/:6 Uncaught TypeError: setPrioritySignalsOverride(): "
       "at least 1 argument(s) are required."});

  // Key not a string, so it gets turned into one.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(R"({bid:1, render:"https://response.test/" })",
                              /*extra_code=*/R"(
            setPrioritySignalsOverride(15, 15);
          )"),
      /*expected_bids=*/
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "null", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_set_priority=*/std::nullopt,
      /*expected_update_priority_signals_overrides=*/{{"15", 15}});

  // Key can't be turned into a string.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(R"({bid:1, render:"https://response.test/" })",
                              /*extra_code=*/R"(
            setPrioritySignalsOverride({
              toString: () => { return {} }
            }, 15);
          )"),
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/:6 Uncaught TypeError: Cannot convert object to "
       "primitive value."});

  // Value not a double.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(R"({bid:1, render:"https://response.test/" })",
                              /*extra_code=*/R"(
            setPrioritySignalsOverride("key", "value");
          )"),
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/:6 Uncaught TypeError: setPrioritySignalsOverride(): "
       "Converting argument 'priority' to a Number did not produce a finite "
       "double."});

  // Value not finite.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(R"({bid:1, render:"https://response.test/" })",
                              /*extra_code=*/R"(
            setPrioritySignalsOverride("key", 0.0/0.0);
          )"),
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/:6 Uncaught TypeError: setPrioritySignalsOverride(): "
       "Converting argument 'priority' to a Number did not produce a finite "
       "double."});

  // Value conversion doesn't terminate.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(R"({bid:1, render:"https://response.test/" })",
                              /*extra_code=*/R"(
            setPrioritySignalsOverride("key", {
              valueOf:() => {while(true) {}}
            });
          )"),
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ execution of `generateBid` timed out."});

  // A key with no value means the value should be removed.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(R"({bid:1, render:"https://response.test/" })",
                              /*extra_code=*/R"(
            setPrioritySignalsOverride("key");
          )"),
      /*expected_bids=*/
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "null", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_set_priority=*/std::nullopt,
      /*expected_update_priority_signals_overrides=*/{{"key", std::nullopt}});

  // An undefined value means the value should be removed.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(R"({bid:1, render:"https://response.test/" })",
                              /*extra_code=*/R"(
            setPrioritySignalsOverride("key", undefined);
          )"),
      /*expected_bids=*/
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "null", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_set_priority=*/std::nullopt,
      /*expected_update_priority_signals_overrides=*/{{"key", std::nullopt}});

  // A null value means the value should be removed.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(R"({bid:1, render:"https://response.test/" })",
                              /*extra_code=*/R"(
            setPrioritySignalsOverride("key", null);
          )"),
      /*expected_bids=*/
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "null", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_set_priority=*/std::nullopt,
      /*expected_update_priority_signals_overrides=*/{{"key", std::nullopt}});

  // Set a number.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(R"({bid:1, render:"https://response.test/" })",
                              /*extra_code=*/R"(
            setPrioritySignalsOverride("key", 0);
          )"),
      /*expected_bids=*/
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "null", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_set_priority=*/std::nullopt,
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
      /*expected_bids=*/
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "null", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_set_priority=*/std::nullopt,
      /*expected_update_priority_signals_overrides=*/{{"key", 6}});

  // Set multiple values.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(R"({bid:1, render:"https://response.test/" })",
                              /*extra_code=*/R"(
            setPrioritySignalsOverride("key1", 1);
            setPrioritySignalsOverride("key2");
            setPrioritySignalsOverride("key3", -6);
          )"),
      /*expected_bids=*/
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "null", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_set_priority=*/std::nullopt,
      /*expected_update_priority_signals_overrides=*/
      {{"key1", 1}, {"key2", std::nullopt}, {"key3", -6}});

  // Overrides should be respected when there's no bid.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(R"({bid:0, render:"https://response.test/" })",
                              /*extra_code=*/R"(
            setPrioritySignalsOverride("key1", 1);
          )"),
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_set_priority=*/std::nullopt,
      /*expected_update_priority_signals_overrides=*/{{"key1", 1}});

  // Overrides should be respected when there's an invalid bid.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(R"({bid:1/0, render:"https://response.test/" })",
                              /*extra_code=*/R"(
            setPrioritySignalsOverride("key1", 1);
          )"),
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/
      {"https://url.test/ generateBid() Converting field 'bid' to a Number did "
       "not produce a finite double."},
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_set_priority=*/std::nullopt,
      /*expected_update_priority_signals_overrides=*/{{"key1", 1}});
}

TEST_F(BidderWorkletTest, ReportWin) {
  RunReportWinWithFunctionBodyExpectingResult(
      "", /*expected_report_url=*/std::nullopt);
  RunReportWinWithFunctionBodyExpectingResult(
      R"(return "https://ignored.test/")",
      /*expected_report_url=*/std::nullopt);

  RunReportWinWithFunctionBodyExpectingResult(
      R"(sendReportTo("https://foo.test"))", GURL("https://foo.test/"));
  RunReportWinWithFunctionBodyExpectingResult(
      R"(sendReportTo("https://foo.test/bar"))", GURL("https://foo.test/bar"));

  RunReportWinWithFunctionBodyExpectingResult(
      R"(sendReportTo("http://http.not.allowed.test"))",
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_ad_macro_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:11 Uncaught TypeError: sendReportTo must be passed a "
       "valid HTTPS url."});
  RunReportWinWithFunctionBodyExpectingResult(
      R"(sendReportTo("file:///file.not.allowed.test"))",
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_ad_macro_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:11 Uncaught TypeError: sendReportTo must be passed a "
       "valid HTTPS url."});

  RunReportWinWithFunctionBodyExpectingResult(
      R"(sendReportTo(""))", /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_ad_macro_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:11 Uncaught TypeError: sendReportTo must be passed a "
       "valid HTTPS url."});

  RunReportWinWithFunctionBodyExpectingResult(
      R"(sendReportTo("https://foo.test");sendReportTo("https://foo.test"))",
      /*expected_report_url=*/std::nullopt, /*expected_ad_beacon_map=*/{},
      /*expected_ad_macro_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:11 Uncaught TypeError: sendReportTo may be called at "
       "most once."});
}

TEST_F(BidderWorkletTest, ReportWinTimeout) {
  const char kBody[] = R"(
    sendReportTo({
      toString:() => {while(true) {}}
    }))";
  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        CreateReportWinScript(kBody));
  RunReportWinExpectingResult(
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_ad_macro_map=*/{},
      /*expected_pa_requests=*/{},
      /*expected_reporting_latency_timeout=*/true,
      /*expected_errors=*/
      {"https://url.test/ execution of `reportWin` timed out."});
}

TEST_F(BidderWorkletTest, ReportWinTopLevelTimeout) {
  const char kScript[] = "while (1) {}";

  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        kScript);
  RunReportWinExpectingResult(
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_ad_macro_map=*/{},
      /*expected_pa_requests=*/{},
      /*expected_reporting_latency_timeout=*/true,
      /*expected_errors=*/
      {"https://url.test/ top-level execution timed out."});
}

TEST_F(BidderWorkletTest, SendReportToLongUrl) {
  // Copying large URLs can cause flaky generateBid() timeouts with the default
  // value, even on the standard debug bots.
  browser_signal_reporting_timeout_ = TestTimeouts::action_max_timeout();

  GURL long_report_url("https://long.test/" +
                       std::string(url::kMaxURLChars, '1'));

  AddJavascriptResponse(
      &url_loader_factory_, interest_group_bidding_url_,
      CreateReportWinScript(base::StringPrintf(
          R"(sendReportTo("%s"))", long_report_url.spec().c_str())));

  ScopedInspectorSupport inspector_support(v8_helper().get());
  BidderWorklet* worklet_impl;
  auto worklet =
      CreateWorklet(interest_group_bidding_url_,
                    /*pause_for_debugger_on_start=*/false, &worklet_impl);

  int id = worklet_impl->context_group_ids_for_testing()[0];
  TestChannel* channel =
      inspector_support.ConnectDebuggerSessionAndRuntimeEnable(id);

  base::RunLoop run_loop;
  RunReportWinExpectingResultAsync(
      worklet_impl, /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_ad_macro_map=*/{},
      /*expected_pa_requests=*/{},
      /*expected_reporting_latency_timeout=*/false,
      /*expected_errors=*/{}, run_loop.QuitClosure());
  run_loop.Run();

  channel->WaitForAndValidateConsoleMessage(
      "warning", /*json_args=*/
      base::StringPrintf("[{\"type\":\"string\", \"value\":\"sendReportTo "
                         "passed URL of length %" PRIuS
                         " but accepts URLs of at most length %" PRIuS ".\"}]",
                         long_report_url.spec().size(), url::kMaxURLChars),
      /*stack_trace_size=*/1, /*function=*/"reportWin",
      interest_group_bidding_url_, /*line_number=*/10);

  channel->ExpectNoMoreConsoleEvents();
}

// Turn enforcement of permission policy on contributeToHistogramOnEvent on.
TEST_F(BidderWorkletTest, ContributeToHistogramOnEventPermissionEnforced) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      blink::features::kFledgeEnforcePermissionPolicyContributeOnEvent);

  base::HistogramTester histogram_tester;
  permissions_policy_state_ = mojom::AuctionWorkletPermissionsPolicyState::New(
      /*private_aggregation_allowed=*/false,
      /*shared_storage_allowed=*/false);

  const char kScriptBody[] = R"(
      privateAggregation.contributeToHistogramOnEvent(
          "reserved.win", {bucket: 234n, value: 56});
  )";

  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        CreateReportWinScript(kScriptBody));

  ScopedInspectorSupport inspector_support(v8_helper().get());
  BidderWorklet* worklet_impl;
  auto worklet =
      CreateWorklet(interest_group_bidding_url_,
                    /*pause_for_debugger_on_start=*/false, &worklet_impl);

  int id = worklet_impl->context_group_ids_for_testing()[0];
  TestChannel* channel =
      inspector_support.ConnectDebuggerSessionAndRuntimeEnable(id);

  base::RunLoop run_loop;
  RunReportWinExpectingResultAsync(
      worklet_impl, /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_ad_macro_map=*/{},
      /*expected_pa_requests=*/{},
      /*expected_reporting_latency_timeout=*/false,
      /*expected_errors=*/
      {"https://url.test/:12 Uncaught TypeError: The \"private-aggregation\" "
       "Permissions Policy denied the method contributeToHistogramOnEvent on "
       "privateAggregation."},
      run_loop.QuitClosure());
  run_loop.Run();

  channel->ExpectNoMoreConsoleEvents();
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.ContributeToHistogramOnEventPermissionPolicy",
      false, 1);
}

// Not enforcing permission policy on contributeToHistogramOnEvent.
// Legacy compatibility mode.
TEST_F(BidderWorkletTest, ContributeToHistogramOnEventPermissionNotEnforced) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      blink::features::kFledgeEnforcePermissionPolicyContributeOnEvent);

  base::HistogramTester histogram_tester;
  permissions_policy_state_ = mojom::AuctionWorkletPermissionsPolicyState::New(
      /*private_aggregation_allowed=*/false,
      /*shared_storage_allowed=*/false);

  const char kScriptBody[] = R"(
      privateAggregation.contributeToHistogramOnEvent(
          "reserved.win", {bucket: 234n, value: 56});
  )";

  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        CreateReportWinScript(kScriptBody));

  ScopedInspectorSupport inspector_support(v8_helper().get());
  BidderWorklet* worklet_impl;
  auto worklet =
      CreateWorklet(interest_group_bidding_url_,
                    /*pause_for_debugger_on_start=*/false, &worklet_impl);

  int id = worklet_impl->context_group_ids_for_testing()[0];
  TestChannel* channel =
      inspector_support.ConnectDebuggerSessionAndRuntimeEnable(id);

  base::RunLoop run_loop;
  PrivateAggregationRequests incorrectly_expected_pa_requests;
  incorrectly_expected_pa_requests.push_back(
      mojom::PrivateAggregationRequest::New(
          mojom::AggregatableReportContribution::NewForEventContribution(
              mojom::AggregatableReportForEventContribution::New(
                  /*bucket=*/mojom::ForEventSignalBucket::NewIdBucket(234),
                  /*value=*/mojom::ForEventSignalValue::NewIntValue(56),
                  /*filtering_id=*/std::nullopt,
                  /*event_type=*/
                  mojom::EventType::NewReserved(
                      mojom::ReservedEventType::kReservedWin))),
          blink::mojom::AggregationServiceMode::kDefault,
          blink::mojom::DebugModeDetails::New()));
  RunReportWinExpectingResultAsync(
      worklet_impl, /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_ad_macro_map=*/{},
      /*expected_pa_requests=*/std::move(incorrectly_expected_pa_requests),
      /*expected_reporting_latency_timeout=*/false,
      /*expected_errors=*/{}, run_loop.QuitClosure());
  run_loop.Run();

  const char kWarning[] =
      "privateAggregation.contributeToHistogramOnEvent called without "
      "appropriate \\\"private-aggregation\\\" Permissions Policy approval; "
      "accepting for backwards compatibility but this will be shortly throwing "
      "an exception";

  channel->WaitForAndValidateConsoleMessage(
      "warning", /*json_args=*/
      base::StrCat({"[{\"type\":\"string\", \"value\":\"", kWarning, "\"}]"}),
      /*stack_trace_size=*/1, /*function=*/"reportWin",
      interest_group_bidding_url_, /*line_number=*/11);

  channel->ExpectNoMoreConsoleEvents();
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.ContributeToHistogramOnEventPermissionPolicy",
      false, 1);
}

// Debug win/loss reporting APIs should do nothing when feature
// kBiddingAndScoringDebugReportingAPI is not enabled. It will not fail
// generateBid().
TEST_F(BidderWorkletTest, ForDebuggingOnlyReportsWithDebugFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      blink::features::kBiddingAndScoringDebugReportingAPI);

  RunGenerateBidWithJavascriptExpectingResult(
      CreateBasicGenerateBidScriptWithExtraCode(
          R"(forDebuggingOnly.reportAdAuctionLoss("https://loss.url"))"),
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "[\"ad\"]", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  RunGenerateBidWithJavascriptExpectingResult(
      CreateBasicGenerateBidScriptWithExtraCode(
          R"(forDebuggingOnly.reportAdAuctionWin("https://win.url"))"),
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "[\"ad\"]", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));
}

TEST_F(BidderWorkletTest, DeleteBeforeReportWinCallback) {
  AddJavascriptResponse(
      &url_loader_factory_, interest_group_bidding_url_,
      CreateReportWinScript(R"(sendReportTo("https://foo.test"))"));
  auto bidder_worklet = CreateWorklet();
  ASSERT_TRUE(bidder_worklet);

  base::WaitableEvent* event_handle = WedgeV8Thread(v8_helper().get());
  bidder_worklet->ReportWin(
      is_for_additional_bid_, interest_group_name_reporting_id_,
      buyer_reporting_id_, buyer_and_seller_reporting_id_,
      selected_buyer_and_seller_reporting_id_, auction_signals_,
      per_buyer_signals_, direct_from_seller_per_buyer_signals_,
      direct_from_seller_per_buyer_signals_header_ad_slot_,
      direct_from_seller_auction_signals_,
      direct_from_seller_auction_signals_header_ad_slot_, seller_signals_,
      kanon_mode_, bid_is_kanon_, browser_signal_render_url_,
      browser_signal_bid_, browser_signal_bid_currency_,
      browser_signal_highest_scoring_other_bid_,
      browser_signal_highest_scoring_other_bid_currency_,
      browser_signal_made_highest_scoring_other_bid_, browser_signal_ad_cost_,
      browser_signal_modeling_signals_, browser_signal_join_count_,
      browser_signal_recency_report_win_, browser_signal_seller_origin_,
      browser_signal_top_level_seller_origin_,
      browser_signal_reporting_timeout_, data_version_,
      /*trace_id=*/1,
      base::BindOnce(
          [](const std::optional<GURL>& report_url,
             const base::flat_map<std::string, GURL>& ad_beacon_map,
             const base::flat_map<std::string, std::string>& ad_macro_map,
             PrivateAggregationRequests pa_requests,
             auction_worklet::mojom::BidderTimingMetricsPtr timing_metrics,
             const std::vector<std::string>& errors) {
            ADD_FAILURE()
                << "Callback should not be invoked since worklet deleted";
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
          is_for_additional_bid_, interest_group_name_reporting_id_,
          buyer_reporting_id_, buyer_and_seller_reporting_id_,
          selected_buyer_and_seller_reporting_id_,
          /*auction_signals_json=*/base::NumberToString(i), per_buyer_signals_,
          direct_from_seller_per_buyer_signals_,
          direct_from_seller_per_buyer_signals_header_ad_slot_,
          direct_from_seller_auction_signals_,
          direct_from_seller_auction_signals_header_ad_slot_, seller_signals_,
          kanon_mode_, bid_is_kanon_, browser_signal_render_url_,
          browser_signal_bid_, browser_signal_bid_currency_,
          browser_signal_highest_scoring_other_bid_,
          browser_signal_highest_scoring_other_bid_currency_,
          browser_signal_made_highest_scoring_other_bid_,
          browser_signal_ad_cost_, browser_signal_modeling_signals_,
          browser_signal_join_count_, browser_signal_recency_report_win_,
          browser_signal_seller_origin_,
          browser_signal_top_level_seller_origin_,
          browser_signal_reporting_timeout_, data_version_,
          /*trace_id=*/1,
          base::BindLambdaForTesting(
              [&run_loop, &num_report_win_calls, i](
                  const std::optional<GURL>& report_url,
                  const base::flat_map<std::string, GURL>& ad_beacon_map,
                  const base::flat_map<std::string, std::string>& ad_macro_map,
                  PrivateAggregationRequests pa_requests,
                  auction_worklet::mojom::BidderTimingMetricsPtr timing_metrics,
                  const std::vector<std::string>& errors) {
                EXPECT_EQ(GURL(base::StringPrintf("https://foo.test/%zu", i)),
                          report_url);
                EXPECT_TRUE(errors.empty());
                ++num_report_win_calls;
                if (num_report_win_calls == kNumReportWinCalls) {
                  run_loop.Quit();
                }
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
        is_for_additional_bid_, interest_group_name_reporting_id_,
        buyer_reporting_id_, buyer_and_seller_reporting_id_,
        selected_buyer_and_seller_reporting_id_,
        /*auction_signals_json=*/base::NumberToString(i), per_buyer_signals_,
        direct_from_seller_per_buyer_signals_,
        direct_from_seller_per_buyer_signals_header_ad_slot_,
        direct_from_seller_auction_signals_,
        direct_from_seller_auction_signals_header_ad_slot_, seller_signals_,
        kanon_mode_, bid_is_kanon_, browser_signal_render_url_,
        browser_signal_bid_, browser_signal_bid_currency_,
        browser_signal_highest_scoring_other_bid_,
        browser_signal_highest_scoring_other_bid_currency_,
        browser_signal_made_highest_scoring_other_bid_, browser_signal_ad_cost_,
        browser_signal_modeling_signals_, browser_signal_join_count_,
        browser_signal_recency_report_win_, browser_signal_seller_origin_,
        browser_signal_top_level_seller_origin_,
        browser_signal_reporting_timeout_, data_version_,
        /*trace_id=*/1,
        base::BindOnce(
            [](const std::optional<GURL>& report_url,
               const base::flat_map<std::string, GURL>& ad_beacon_map,
               const base::flat_map<std::string, std::string>& ad_macro_map,
               PrivateAggregationRequests pa_requests,
               auction_worklet::mojom::BidderTimingMetricsPtr timing_metrics,
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
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_ad_macro_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:11 Uncaught ReferenceError: Date is not defined."});
}

TEST_F(BidderWorkletTest, ReportWinIsForAdditionalBid) {
  const char kScript[] = R"(
    function reportWin() {
      sendReportTo("https://report-win.test/");
    }

    function reportAdditionalBidWin() {
      sendReportTo("https://report-additional-bid-win.test/");
    }
  )";

  is_for_additional_bid_ = false;
  RunReportWinWithJavascriptExpectingResult(kScript,
                                            GURL("https://report-win.test/"));

  is_for_additional_bid_ = true;
  RunReportWinWithJavascriptExpectingResult(
      kScript, GURL("https://report-additional-bid-win.test/"));
}

TEST_F(BidderWorkletTest, ReportWinContainsInterestGroupName) {
  const char kScriptBody[] = R"(
    sendReportTo("https://example.test/?" +
                 browserSignals.interestGroupName);
  )";

  RunReportWinWithFunctionBodyExpectingResult(
      kScriptBody, GURL("https://example.test/?Fred"));
}

TEST_F(BidderWorkletTest, ReportWinContainsBuyerReportingId) {
  const char kScriptBody[] = R"(
    sendReportTo("https://example.test/?" +
                 browserSignals.buyerReportingId);
  )";

  buyer_reporting_id_ = "reporting_id";
  RunReportWinWithFunctionBodyExpectingResult(
      kScriptBody, GURL("https://example.test/?reporting_id"));
}

TEST_F(BidderWorkletTest, ReportWinContainsBuyerAndSellerReportingId) {
  const char kScriptBody[] = R"(
    sendReportTo("https://example.test/?" +
                 browserSignals.buyerAndSellerReportingId);
  )";

  buyer_and_seller_reporting_id_ = "reporting_id";
  RunReportWinWithFunctionBodyExpectingResult(
      kScriptBody, GURL("https://example.test/?reporting_id"));
}

TEST_F(BidderWorkletTest, ReportWinContainsSelectedBuyerAndSellerReportingId) {
  const char kScriptBody[] = R"(
    sendReportTo("https://example.test/?" +
                 browserSignals.selectedBuyerAndSellerReportingId);
  )";

  selected_buyer_and_seller_reporting_id_ = "reporting_id";
  RunReportWinWithFunctionBodyExpectingResult(
      kScriptBody, GURL("https://example.test/?reporting_id"));
}

TEST_F(BidderWorkletTest, ReportWinContainsNoReportingId) {
  const char kScriptBody[] = R"(
    sendReportTo("https://example.test/?" +
                 browserSignals.interestGroupName + "/" +
                 browserSignals.buyerReportingId + "/" +
                 browserSignals.buyerAndSellerReportingId + "/" +
                 browserSignals.selectedBuyerAndSellerReportingId);
  )";

  interest_group_name_reporting_id_ = std::nullopt;
  RunReportWinWithFunctionBodyExpectingResult(
      kScriptBody,
      GURL("https://example.test/?undefined/undefined/undefined/undefined"));
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
      "Ad-Auction-Allowed: true\nAd-Auction-Only: true";

  direct_from_seller_per_buyer_signals_ =
      GURL("https://url.test/perbuyersignals");
  direct_from_seller_auction_signals_ = GURL("https://url.test/auctionsignals");

  struct Response {
    GURL response_url;
    std::string response_type;
    std::string headers;
    std::string content;
  };

  const auto kResponses = std::to_array<const Response>({
      {
          interest_group_bidding_url_,
          kJavascriptMimeType,
          kAllowFledgeHeader,
          CreateReportWinScript(R"(sendReportTo("https://foo.test"))"),
      },
      {
          *direct_from_seller_per_buyer_signals_,
          kJsonMimeType,
          kDirectFromSellerSignalsHeaders,
          kJsonResponse,
      },
      {
          *direct_from_seller_auction_signals_,
          kJsonMimeType,
          kDirectFromSellerSignalsHeaders,
          kJsonResponse,
      },
  });

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
    RunReportWinExpectingResultAsync(
        bidder_worklet.get(), GURL("https://foo.test/"), {}, {}, {},
        /*expected_reporting_latency_timeout=*/false, {},
        run_loop->QuitClosure());
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

  auction_signals_ = std::nullopt;
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

  per_buyer_signals_ = std::nullopt;
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
      "sendReportTo(sellerSignals)", /*expected_report_url=*/std::nullopt);

  seller_signals_ = R"("https://interest.group.name.test/")";
  RunReportWinWithFunctionBodyExpectingResult(
      "sendReportTo(sellerSignals)", GURL("https://interest.group.name.test/"));
}

TEST_F(BidderWorkletTest,
       ReportWinDirectFromSellerSignalsHeaderAdSlotAuctionSignals) {
  direct_from_seller_auction_signals_header_ad_slot_ =
      R"("https://interest.group.name.test/")";
  RunReportWinWithFunctionBodyExpectingResult(
      "sendReportTo(directFromSellerSignals.auctionSignals)",
      GURL("https://interest.group.name.test/"));

  direct_from_seller_auction_signals_header_ad_slot_ = std::nullopt;
  RunReportWinWithFunctionBodyExpectingResult(
      R"(sendReportTo("https://" +
          (directFromSellerSignals.auctionSignals === null)))",
      GURL("https://true/"));
}

TEST_F(BidderWorkletTest,
       ReportWinDirectFromSellerSignalsHeaderAdSlotPerBuyerSignals) {
  direct_from_seller_per_buyer_signals_header_ad_slot_ =
      R"("https://interest.group.name.test/")";
  RunReportWinWithFunctionBodyExpectingResult(
      "sendReportTo(directFromSellerSignals.perBuyerSignals)",
      GURL("https://interest.group.name.test/"));

  direct_from_seller_per_buyer_signals_header_ad_slot_ = std::nullopt;
  RunReportWinWithFunctionBodyExpectingResult(
      R"(sendReportTo("https://" +
          (directFromSellerSignals.perBuyerSignals === null)))",
      GURL("https://true/"));
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
      "sendReportTo(browserSignals.renderURL)", browser_signal_render_url_);
}

// Check that accessing `renderUrl` of browserSignals displays a warning.
//
// TODO(crbug.com/40266734): Remove this test when the field itself is
// removed.
TEST_F(BidderWorkletTest, ReportWinBrowserSignalRenderUrlDeprecationWarning) {
  ScopedInspectorSupport inspector_support(v8_helper().get());

  AddJavascriptResponse(
      &url_loader_factory_, interest_group_bidding_url_,
      CreateReportWinScript("sendReportTo(browserSignals.renderUrl);"));

  BidderWorklet* worklet_impl;
  auto worklet =
      CreateWorklet(interest_group_bidding_url_,
                    /*pause_for_debugger_on_start=*/false, &worklet_impl);

  int id = worklet_impl->context_group_ids_for_testing()[0];
  TestChannel* channel =
      inspector_support.ConnectDebuggerSessionAndRuntimeEnable(id);

  base::RunLoop run_loop;
  RunReportWinExpectingResultAsync(worklet_impl, browser_signal_render_url_,
                                   /*expected_ad_beacon_map=*/{},
                                   /*expected_ad_macro_map=*/{},
                                   /*expected_pa_requests=*/{},
                                   /*expected_reporting_latency_timeout=*/false,
                                   /*expected_errors=*/{},
                                   run_loop.QuitClosure());
  run_loop.Run();

  channel->WaitForAndValidateConsoleMessage(
      "warning", /*json_args=*/
      "[{\"type\":\"string\", "
      "\"value\":\"browserSignals.renderUrl is deprecated. Please use "
      "browserSignals.renderURL instead.\"}]",
      /*stack_trace_size=*/1, /*function=*/"reportWin",
      interest_group_bidding_url_, /*line_number=*/10);

  channel->ExpectNoMoreConsoleEvents();
}

// Check that accessing `renderURL` of browserSignals does not display a
// warning.
//
// TODO(crbug.com/40266734): Remove this test when renderUrl is removed.
TEST_F(BidderWorkletTest, ReportWinBrowserSignalRenderUrlNoDeprecationWarning) {
  ScopedInspectorSupport inspector_support(v8_helper().get());

  AddJavascriptResponse(
      &url_loader_factory_, interest_group_bidding_url_,
      CreateReportWinScript("sendReportTo(browserSignals.renderURL);"));

  BidderWorklet* worklet_impl;
  auto worklet =
      CreateWorklet(interest_group_bidding_url_,
                    /*pause_for_debugger_on_start=*/false, &worklet_impl);

  int id = worklet_impl->context_group_ids_for_testing()[0];
  TestChannel* channel =
      inspector_support.ConnectDebuggerSessionAndRuntimeEnable(id);

  base::RunLoop run_loop;
  RunReportWinExpectingResultAsync(worklet_impl, browser_signal_render_url_,
                                   /*expected_ad_beacon_map=*/{},
                                   /*expected_ad_macro_map=*/{},
                                   /*expected_pa_requests=*/{},
                                   /*expected_reporting_latency_timeout=*/false,
                                   /*expected_errors=*/{},
                                   run_loop.QuitClosure());
  run_loop.Run();
  channel->ExpectNoMoreConsoleEvents();
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

TEST_F(BidderWorkletTest, ReportWinBrowserSignalBidCurrency) {
  browser_signal_bid_currency_ = std::nullopt;
  RunReportWinWithFunctionBodyExpectingResult(
      R"(if (browserSignals.bidCurrency === "???")
        sendReportTo("https://jumboshrimp.test"))",
      GURL("https://jumboshrimp.test"));

  browser_signal_bid_currency_ = blink::AdCurrency::From("USD");
  RunReportWinWithFunctionBodyExpectingResult(
      R"(if (browserSignals.bidCurrency === "USD")
        sendReportTo("https://jumboshrimp.test"))",
      GURL("https://jumboshrimp.test"));
}

TEST_F(BidderWorkletTest,
       ReportWinBrowserSignalHighestScoringOtherBidCurrency) {
  browser_signal_highest_scoring_other_bid_currency_ =
      blink::AdCurrency::From("CAD");
  RunReportWinWithFunctionBodyExpectingResult(
      R"(if (browserSignals.highestScoringOtherBidCurrency === "CAD")
        sendReportTo("https://jumboshrimp.test"))",
      GURL("https://jumboshrimp.test"));

  browser_signal_highest_scoring_other_bid_currency_ = std::nullopt;
  RunReportWinWithFunctionBodyExpectingResult(
      R"(if (browserSignals.highestScoringOtherBidCurrency === "???")
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
  browser_signal_top_level_seller_origin_ = std::nullopt;
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

TEST_F(BidderWorkletTest, ReportWinBrowserSignalModelingSignals) {
  browser_signal_modeling_signals_ = std::nullopt;
  RunReportWinWithFunctionBodyExpectingResult(
      R"(if (!("modelingSignals" in browserSignals))
            sendReportTo('https://pass.test');)",
      /*expected_report_url=*/GURL("https://pass.test"));

  browser_signal_modeling_signals_ = 123u;
  RunReportWinWithFunctionBodyExpectingResult(
      R"(if (browserSignals.modelingSignals === 123)
        sendReportTo("https://jumboshrimp.test"))",
      GURL("https://jumboshrimp.test"));
}

TEST_F(BidderWorkletTest, ReportWinBrowserSignalJoinCount) {
  browser_signal_join_count_ = 7;
  RunReportWinWithFunctionBodyExpectingResult(
      R"(if (browserSignals.joinCount === 7)
        sendReportTo("https://jumboshrimp.test"))",
      GURL("https://jumboshrimp.test"));
}

TEST_F(BidderWorkletTest, ReportWinBrowserSignalRecency) {
  browser_signal_recency_report_win_ = 19u;
  RunReportWinWithFunctionBodyExpectingResult(
      R"(if (browserSignals.recency === 19)
        sendReportTo("https://jumboshrimp.test"))",
      GURL("https://jumboshrimp.test"));
}

TEST_F(BidderWorkletTest, ReportWinNoBrowserSignalRecencyForAdditionalBid) {
  is_for_additional_bid_ = true;
  browser_signal_recency_report_win_ = 19u;
  const char kScript[] = R"(
    function reportAdditionalBidWin(
          auctionSignals, perBuyerSignals, sellerSignals,
          browserSignals, directFromSellerSignals) {
      if ('recency' in browserSignals)
        throw 'Should not have recency in reportAdditionalBidWin';
      sendReportTo("https://report-additional-bid-win.test/");
    }
  )";

  RunReportWinWithJavascriptExpectingResult(
      kScript, GURL("https://report-additional-bid-win.test/"));
}

TEST_F(BidderWorkletTest, KAnonStatusExposesInReportWinBrowserSignals) {
  kanon_mode_ = auction_worklet::mojom::KAnonymityBidMode::kEnforce;
  bid_is_kanon_ = true;
  RunReportWinWithFunctionBodyExpectingResult(
      R"(if (browserSignals.kAnonStatus === "passedAndEnforced")
        sendReportTo("https://passedAndEnforced.test"))",
      GURL("https://passedAndEnforced.test"));

  kanon_mode_ = auction_worklet::mojom::KAnonymityBidMode::kEnforce;
  bid_is_kanon_ = false;
  RunReportWinWithFunctionBodyExpectingResult(
      R"(if (browserSignals.kAnonStatus === "passedAndEnforced")
        sendReportTo("https://passedAndEnforced.test"))",
      GURL("https://passedAndEnforced.test"));

  kanon_mode_ = auction_worklet::mojom::KAnonymityBidMode::kSimulate;
  bid_is_kanon_ = true;
  RunReportWinWithFunctionBodyExpectingResult(
      R"(if (browserSignals.kAnonStatus === "passedNotEnforced")
        sendReportTo("https://passedNotEnforced.test"))",
      GURL("https://passedNotEnforced.test"));

  kanon_mode_ = auction_worklet::mojom::KAnonymityBidMode::kSimulate;
  bid_is_kanon_ = false;
  RunReportWinWithFunctionBodyExpectingResult(
      R"(if (browserSignals.kAnonStatus === "belowThreshold")
        sendReportTo("https://belowThreshold.test"))",
      GURL("https://belowThreshold.test"));

  kanon_mode_ = auction_worklet::mojom::KAnonymityBidMode::kNone;
  bid_is_kanon_ = true;
  RunReportWinWithFunctionBodyExpectingResult(
      R"(if (browserSignals.kAnonStatus === "notCalculated")
        sendReportTo("https://notCalculated.test"))",
      GURL("https://notCalculated.test"));

  kanon_mode_ = auction_worklet::mojom::KAnonymityBidMode::kNone;
  bid_is_kanon_ = false;
  RunReportWinWithFunctionBodyExpectingResult(
      R"(if (browserSignals.kAnonStatus === "notCalculated")
        sendReportTo("https://notCalculated.test"))",
      GURL("https://notCalculated.test"));
}

// Subsequent runs of the same script should not affect each other. Same is true
// for different scripts, but it follows from the single script case.
//
// TODO(mmenke): The current API only allows each generateBid() method to be
// called once, but each ReportWin() to be called multiple times. When the API
// is updated to allow multiple calls to generateBid(), update this method to
// invoke it multiple times.
TEST_P(BidderWorkletMultiThreadingTest, ScriptIsolation) {
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
        is_for_additional_bid_, interest_group_name_reporting_id_,
        buyer_reporting_id_, buyer_and_seller_reporting_id_,
        selected_buyer_and_seller_reporting_id_, auction_signals_,
        per_buyer_signals_, direct_from_seller_per_buyer_signals_,
        direct_from_seller_per_buyer_signals_header_ad_slot_,
        direct_from_seller_auction_signals_,
        direct_from_seller_auction_signals_header_ad_slot_, seller_signals_,
        kanon_mode_, bid_is_kanon_, browser_signal_render_url_,
        browser_signal_bid_, browser_signal_bid_currency_,
        browser_signal_highest_scoring_other_bid_,
        browser_signal_highest_scoring_other_bid_currency_,
        browser_signal_made_highest_scoring_other_bid_, browser_signal_ad_cost_,
        browser_signal_modeling_signals_, browser_signal_join_count_,
        browser_signal_recency_report_win_, browser_signal_seller_origin_,
        browser_signal_top_level_seller_origin_,
        browser_signal_reporting_timeout_, data_version_, /*trace_id=*/1,
        base::BindLambdaForTesting(
            [&run_loop](
                const std::optional<GURL>& report_url,
                const base::flat_map<std::string, GURL>& ad_beacon_map,
                const base::flat_map<std::string, std::string>& ad_macro_map,
                PrivateAggregationRequests pa_requests,
                auction_worklet::mojom::BidderTimingMetricsPtr timing_metrics,
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
  int id = worklet_impl->context_group_ids_for_testing()[0];

  // Give it a chance to fetch.
  task_environment_.RunUntilIdle();

  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        CreateBasicGenerateBidScript());

  // Set up the event loop for the standard callback.
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();

  // Let this run.
  v8_helper()->v8_runner()->PostTask(
      FROM_HERE, base::BindOnce([](scoped_refptr<AuctionV8Helper> v8_helper,
                                   int id) { v8_helper->Resume(id); },
                                v8_helper(), id));

  generate_bid_run_loop_->Run();
  generate_bid_run_loop_.reset();

  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ("[\"ad\"]", bids_[0]->ad);
  EXPECT_EQ(1, bids_[0]->bid);
  EXPECT_EQ(GURL("https://response.test/"), bids_[0]->ad_descriptor.url);
  EXPECT_THAT(bid_errors_, ::testing::UnorderedElementsAre());
}

TEST_F(BidderWorkletTwoThreadsTest, PauseOnStart) {
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

  // Grab the context group IDs to be able to resume.
  std::vector<int> ids = worklet_impl->context_group_ids_for_testing();
  ASSERT_EQ(ids.size(), 2u);

  // Give it a chance to fetch.
  task_environment_.RunUntilIdle();

  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        CreateBasicGenerateBidScript());

  // Set up the event loop for the standard callback.
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();

  // Let this run.
  v8_helpers_[0]->v8_runner()->PostTask(
      FROM_HERE, base::BindOnce([](scoped_refptr<AuctionV8Helper> v8_helper,
                                   int id) { v8_helper->Resume(id); },
                                v8_helpers_[0], ids[0]));
  v8_helpers_[1]->v8_runner()->PostTask(
      FROM_HERE, base::BindOnce([](scoped_refptr<AuctionV8Helper> v8_helper,
                                   int id) { v8_helper->Resume(id); },
                                v8_helpers_[1], ids[1]));

  generate_bid_run_loop_->Run();
  generate_bid_run_loop_.reset();

  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ("[\"ad\"]", bids_[0]->ad);
  EXPECT_EQ(1, bids_[0]->bid);
  EXPECT_EQ(GURL("https://response.test/"), bids_[0]->ad_descriptor.url);
  EXPECT_THAT(bid_errors_, ::testing::UnorderedElementsAre());
}

TEST_P(BidderWorkletMultiThreadingTest, PauseOnStartDelete) {
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
  int id = worklet_impl->context_group_ids_for_testing()[0];

  // Delete the worklet. No callback should be invoked.
  worklet.reset();
  task_environment_.RunUntilIdle();

  // Try to resume post-delete. Should not crash.
  v8_helper()->v8_runner()->PostTask(
      FROM_HERE, base::BindOnce([](scoped_refptr<AuctionV8Helper> v8_helper,
                                   int id) { v8_helper->Resume(id); },
                                v8_helper(), id));
  task_environment_.RunUntilIdle();
}

TEST_F(BidderWorkletTest, BasicV8Debug) {
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

  const char kUrl1[] = "http://example.test/first.js";
  const char kUrl2[] = "http://example2.test/second.js";

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
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  channel1->RunCommandAndWaitForResult(
      3, "Runtime.runIfWaitingForDebugger",
      R"({"id":3,"method":"Runtime.runIfWaitingForDebugger","params":{}})");
  generate_bid_run_loop_->Run();

  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(1, bids_[0]->bid);
  bids_.clear();
  generate_bid_run_loop_.reset();

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
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  channel2->RunCommandAndWaitForResult(
      3, "Runtime.runIfWaitingForDebugger",
      R"({"id":3,"method":"Runtime.runIfWaitingForDebugger","params":{}})");
  generate_bid_run_loop_->Run();

  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(1, bids_[0]->bid);
  bids_.clear();
  generate_bid_run_loop_.reset();

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

TEST_F(BidderWorkletTwoThreadsTest, BasicV8Debug) {
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

  const char kUrl[] = "http://example.test/first.js";

  AddJavascriptResponse(&url_loader_factory_, GURL(kUrl),
                        CreateBasicGenerateBidScript());

  BidderWorklet* worklet_impl;
  auto worklet = CreateWorklet(GURL(kUrl), /*pause_for_debugger_on_start=*/true,
                               &worklet_impl);
  GenerateBid(worklet.get());

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

  // Unpause execution for #0. Expect that no bid is generated yet, as the
  // worklet is waiting on both V8 threads to resume.
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  channel0->RunCommandAndWaitForResult(
      3, "Runtime.runIfWaitingForDebugger",
      R"({"id":3,"method":"Runtime.runIfWaitingForDebugger","params":{}})");
  generate_bid_run_loop_->RunUntilIdle();

  EXPECT_TRUE(bids_.empty());

  // Unpause execution for #1. Expect that one bid is generated.
  channel1->RunCommandAndWaitForResult(
      3, "Runtime.runIfWaitingForDebugger",
      R"({"id":3,"method":"Runtime.runIfWaitingForDebugger","params":{}})");
  generate_bid_run_loop_->Run();

  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(1, bids_[0]->bid);
  bids_.clear();
  generate_bid_run_loop_.reset();

  // channel0 should have had a parsed notification for kUrl, as the GenerateBid
  // is executed on the corresponding thread.
  TestChannel::Event script_parsed0 =
      channel0->WaitForMethodNotification("Debugger.scriptParsed");
  const std::string* url =
      script_parsed0.value.GetDict().FindStringByDottedPath("params.url");
  ASSERT_TRUE(url);
  EXPECT_EQ(kUrl, *url);

  // There shouldn't be a parsed notification on channel1, however.
  events1 = channel1->TakeAllEvents();
  EXPECT_TRUE(base::ranges::none_of(events1, is_script_parsed));

  worklet.reset();
  task_environment_.RunUntilIdle();

  // No other scriptParsed events should be on either channel.
  events0 = channel0->TakeAllEvents();
  events1 = channel1->TakeAllEvents();
  EXPECT_TRUE(base::ranges::none_of(events0, is_script_parsed));
  EXPECT_TRUE(base::ranges::none_of(events1, is_script_parsed));
}

TEST_F(BidderWorkletTest, ParseErrorV8Debug) {
  ScopedInspectorSupport inspector_support(v8_helper().get());
  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        "Invalid Javascript");

  BidderWorklet* worklet_impl = nullptr;
  auto worklet =
      CreateWorklet(interest_group_bidding_url_,
                    /*pause_for_debugger_on_start=*/true, &worklet_impl);
  GenerateBidExpectingNeverCompletes(worklet.get());
  int id = worklet_impl->context_group_ids_for_testing()[0];
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

TEST_F(BidderWorkletTwoThreadsTest, ParseErrorV8Debug) {
  ScopedInspectorSupport inspector_support0(v8_helpers_[0].get());
  ScopedInspectorSupport inspector_support1(v8_helpers_[1].get());
  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        "Invalid Javascript");

  BidderWorklet* worklet_impl = nullptr;
  auto worklet =
      CreateWorklet(interest_group_bidding_url_,
                    /*pause_for_debugger_on_start=*/true, &worklet_impl);
  GenerateBidExpectingNeverCompletes(worklet.get());
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

  // Unpause execution.
  channel0->RunCommandAndWaitForResult(
      3, "Runtime.runIfWaitingForDebugger",
      R"({"id":3,"method":"Runtime.runIfWaitingForDebugger","params":{}})");
  channel1->RunCommandAndWaitForResult(
      3, "Runtime.runIfWaitingForDebugger",
      R"({"id":3,"method":"Runtime.runIfWaitingForDebugger","params":{}})");

  // Worklet should disconnect with an error message.
  EXPECT_FALSE(WaitForDisconnect().empty());

  // Should have gotten a parse error notification for each channel.
  TestChannel::Event parse_error0 =
      channel0->WaitForMethodNotification("Debugger.scriptFailedToParse");
  const std::string* error_url0 =
      parse_error0.value.GetDict().FindStringByDottedPath("params.url");
  ASSERT_TRUE(error_url0);
  EXPECT_EQ(interest_group_bidding_url_.spec(), *error_url0);

  TestChannel::Event parse_error1 =
      channel1->WaitForMethodNotification("Debugger.scriptFailedToParse");
  const std::string* error_url1 =
      parse_error1.value.GetDict().FindStringByDottedPath("params.url");
  ASSERT_TRUE(error_url1);
  EXPECT_EQ(interest_group_bidding_url_.spec(), *error_url1);
}

TEST_F(BidderWorkletTest, BasicDevToolsDebug) {
  std::string bid_script = CreateGenerateBidScript(
      R"({ad: ["ad"], bid: this.global_bid ? this.global_bid : 1,
          render:"https://response.test/"})");
  const char kUrl1[] = "http://example.test/first.js";
  const char kUrl2[] = "http://example2.test/second.js";

  AddJavascriptResponse(&url_loader_factory_, GURL(kUrl1), bid_script);
  AddJavascriptResponse(&url_loader_factory_, GURL(kUrl2), bid_script);

  auto worklet1 =
      CreateWorklet(GURL(kUrl1), /*pause_for_debugger_on_start=*/true);
  GenerateBid(worklet1.get());
  auto worklet2 =
      CreateWorklet(GURL(kUrl2), /*pause_for_debugger_on_start=*/true);
  GenerateBid(worklet2.get());

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

  // Next there is the breakpoint.
  TestDevToolsAgentClient::Event breakpoint_hit1 =
      debug1.WaitForMethodNotification("Debugger.paused");

  base::Value::List* hit_breakpoints1 =
      breakpoint_hit1.value.GetDict().FindListByDottedPath(
          "params.hitBreakpoints");
  ASSERT_TRUE(hit_breakpoints1);
  ASSERT_EQ(1u, hit_breakpoints1->size());
  ASSERT_TRUE((*hit_breakpoints1)[0].is_string());
  EXPECT_EQ("1:0:0:http://example.test/first.js",
            (*hit_breakpoints1)[0].GetString());
  std::string* callframe_id1 = breakpoint_hit1.value.GetDict()
                                   .FindDict("params")
                                   ->FindList("callFrames")
                                   ->front()
                                   .GetDict()
                                   .FindString("callFrameId");

  // Override the bid value.
  const char kCommandTemplate[] = R"({
    "id": 5,
    "method": "Debugger.evaluateOnCallFrame",
    "params": {
      "callFrameId": "%s",
      "expression": "global_bid = 42"
    }
  })";

  debug1.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kIO, 5, "Debugger.evaluateOnCallFrame",
      base::StringPrintf(kCommandTemplate, callframe_id1->c_str()));

  // Resume, setting up event loop for fixture first.
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  debug1.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kIO, 6, "Debugger.resume",
      R"({"id":6,"method":"Debugger.resume","params":{}})");
  generate_bid_run_loop_->Run();

  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(42, bids_[0]->bid);
  bids_.clear();

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
  EXPECT_EQ("1:0:0:http://example2.test/second.js",
            (*hit_breakpoints2)[0].GetString());

  // Go ahead and resume w/o messing with anything.
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  debug2.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kIO, 5, "Debugger.resume",
      R"({"id":5,"method":"Debugger.resume","params":{}})");
  generate_bid_run_loop_->Run();

  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(1, bids_[0]->bid);
}

TEST_F(BidderWorkletTwoThreadsTest, BasicDevToolsDebug) {
  std::string bid_script = CreateGenerateBidScript(
      R"({ad: ["ad"], bid: this.global_bid ? this.global_bid : 1,
          render:"https://response.test/"})");
  const char kUrl1[] = "http://example.test/first.js";

  AddJavascriptResponse(&url_loader_factory_, GURL(kUrl1), bid_script);

  auto worklet =
      CreateWorklet(GURL(kUrl1), /*pause_for_debugger_on_start=*/true);
  GenerateBid(worklet.get());
  GenerateBid(worklet.get());

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
          "lineNumber": 0,
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
  // as the two GenerateBids are executed on the corresponding thread
  // respectively.
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
      breakpoint_hit0.value.GetDict().FindListByDottedPath(
          "params.hitBreakpoints");
  ASSERT_TRUE(hit_breakpoints0);
  ASSERT_EQ(1u, hit_breakpoints0->size());
  ASSERT_TRUE((*hit_breakpoints0)[0].is_string());
  EXPECT_EQ("1:0:0:http://example.test/first.js",
            (*hit_breakpoints0)[0].GetString());
  std::string* callframe_id0 = breakpoint_hit0.value.GetDict()
                                   .FindDict("params")
                                   ->FindList("callFrames")
                                   ->front()
                                   .GetDict()
                                   .FindString("callFrameId");

  base::Value::List* hit_breakpoints1 =
      breakpoint_hit1.value.GetDict().FindListByDottedPath(
          "params.hitBreakpoints");
  ASSERT_TRUE(hit_breakpoints1);
  ASSERT_EQ(1u, hit_breakpoints1->size());
  ASSERT_TRUE((*hit_breakpoints1)[0].is_string());
  EXPECT_EQ("1:0:0:http://example.test/first.js",
            (*hit_breakpoints1)[0].GetString());
  std::string* callframe_id1 = breakpoint_hit1.value.GetDict()
                                   .FindDict("params")
                                   ->FindList("callFrames")
                                   ->front()
                                   .GetDict()
                                   .FindString("callFrameId");

  // Override the bid value.
  const char kCommandTemplate[] = R"({
    "id": 5,
    "method": "Debugger.evaluateOnCallFrame",
    "params": {
      "callFrameId": "%s",
      "expression": "global_bid = 42"
    }
  })";

  debug0.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kIO, 5, "Debugger.evaluateOnCallFrame",
      base::StringPrintf(kCommandTemplate, callframe_id0->c_str()));
  debug1.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kIO, 5, "Debugger.evaluateOnCallFrame",
      base::StringPrintf(kCommandTemplate, callframe_id1->c_str()));

  // Let the thread associated with `debug0` resume.
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  debug0.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kIO, 6, "Debugger.resume",
      R"({"id":6,"method":"Debugger.resume","params":{}})");
  generate_bid_run_loop_->Run();

  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(42, bids_[0]->bid);
  bids_.clear();

  // Let the thread associated with `debug1` resume.
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  debug1.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kIO, 6, "Debugger.resume",
      R"({"id":6,"method":"Debugger.resume","params":{}})");
  generate_bid_run_loop_->Run();

  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(42, bids_[0]->bid);
  bids_.clear();
}

TEST_F(BidderWorkletTest, InstrumentationBreakpoints) {
  const char kUrl[] = "http://example.test/bid.js";

  AddJavascriptResponse(
      &url_loader_factory_, GURL(kUrl),
      CreateReportWinScript(R"(sendReportTo("https://foo.test"))"));

  auto worklet =
      CreateWorklet(GURL(kUrl), /*pause_for_debugger_on_start=*/true);
  GenerateBid(worklet.get());

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
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  debug.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kIO, 6, "Debugger.resume",
      R"({"id":6,"method":"Debugger.resume","params":{}})");
  generate_bid_run_loop_->Run();

  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(1, bids_[0]->bid);
  bids_.clear();

  // Now ask for reporting. This should hit the other breakpoint.
  base::RunLoop run_loop;
  RunReportWinExpectingResultAsync(
      worklet.get(), GURL("https://foo.test/"), {}, {}, {},
      /*expected_reporting_latency_timeout=*/false, {}, run_loop.QuitClosure());

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
  const char kUrl[] = "http://example.test/bid.js";

  AddJavascriptResponse(
      &url_loader_factory_, GURL(kUrl),
      CreateReportWinScript(R"(sendReportTo("https://foo.test"))"));

  auto worklet =
      CreateWorklet(GURL(kUrl), /*pause_for_debugger_on_start=*/true);
  GenerateBid(worklet.get());

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
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  generate_bid_run_loop_->Run();
  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(1, bids_[0]->bid);

  // Run 2, same group.
  GenerateBid(bidder_worklet.get());
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  generate_bid_run_loop_->Run();
  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(2, bids_[0]->bid);

  // Run 3, not in group.
  execution_mode_ =
      blink::mojom::InterestGroup::ExecutionMode::kCompatibilityMode;
  join_origin_ = url::Origin::Create(GURL("https://url2.test/"));
  GenerateBid(bidder_worklet.get());
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  generate_bid_run_loop_->Run();
  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(1, bids_[0]->bid);

  // Run 4, back to group.
  execution_mode_ =
      blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode;
  join_origin_ = url::Origin::Create(GURL("https://url.test/"));
  GenerateBid(bidder_worklet.get());
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  generate_bid_run_loop_->Run();
  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(3, bids_[0]->bid);

  // Run 5, different group.
  join_origin_ = url::Origin::Create(GURL("https://url2.test/"));
  GenerateBid(bidder_worklet.get());
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  generate_bid_run_loop_->Run();
  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(1, bids_[0]->bid);

  // Run 5, different group cont'd.
  GenerateBid(bidder_worklet.get());
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  generate_bid_run_loop_->Run();
  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(2, bids_[0]->bid);
}

TEST_F(BidderWorkletTest, ExecutionModeGroupByOriginSaveMultipleGroups) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      blink::features::kFledgeNumberBidderWorkletGroupByOriginContextsToKeep,
      {{"GroupByOriginContextLimit", "2"},
       {"IncludeFacilitatedTestingGroups", "true"}});

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

  // Save origin 1 context.
  execution_mode_ =
      blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode;
  join_origin_ = url::Origin::Create(GURL("https://url.test/"));
  GenerateBid(bidder_worklet.get());
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  generate_bid_run_loop_->Run();
  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(1, bids_[0]->bid);

  // Save origin 2 context.
  join_origin_ = url::Origin::Create(GURL("https://url2.test/"));
  GenerateBid(bidder_worklet.get());
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  generate_bid_run_loop_->Run();
  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(1, bids_[0]->bid);

  // Save origin 3 context. This will overwrite origin 1's context.
  join_origin_ = url::Origin::Create(GURL("https://url3.test/"));
  GenerateBid(bidder_worklet.get());
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  generate_bid_run_loop_->Run();
  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(1, bids_[0]->bid);

  // Access origin 2 context which should still be saved.
  join_origin_ = url::Origin::Create(GURL("https://url2.test/"));
  GenerateBid(bidder_worklet.get());
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  generate_bid_run_loop_->Run();
  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(2, bids_[0]->bid);

  // Access origin 3 context which should still be saved.
  join_origin_ = url::Origin::Create(GURL("https://url3.test/"));
  GenerateBid(bidder_worklet.get());
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  generate_bid_run_loop_->Run();
  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(2, bids_[0]->bid);

  // Origin 1's context is not still saved. This will save it and overwrite
  // origin 2's context.
  join_origin_ = url::Origin::Create(GURL("https://url.test/"));
  GenerateBid(bidder_worklet.get());
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  generate_bid_run_loop_->Run();
  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(1, bids_[0]->bid);

  // Access origin 3 context which should still be saved.
  join_origin_ = url::Origin::Create(GURL("https://url3.test/"));
  GenerateBid(bidder_worklet.get());
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  generate_bid_run_loop_->Run();
  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(3, bids_[0]->bid);

  // Access origin 2 context which is no longer saved.
  join_origin_ = url::Origin::Create(GURL("https://url2.test/"));
  GenerateBid(bidder_worklet.get());
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  generate_bid_run_loop_->Run();
  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(1, bids_[0]->bid);
}
TEST_F(BidderWorkletTest, ExecutionModeFrozenContext) {
  const char kScript[] = R"(
    if (!('count' in globalThis))
      globalThis.count = 1;
    function generateBid() {
      ++count;  // This increment is a no-op if the context is frozen.
      return {
        ad: ["ad"],
        bid: count,
        render:"https://response.test/?" + Object.isFrozen(globalThis)};
    }
  )";

  std::string frozen_url = "https://response.test/?true";
  std::string not_frozen_url = "https://response.test/?false";

  interest_group_ads_.emplace_back(GURL(frozen_url),
                                   /*metadata=*/std::nullopt);
  interest_group_ads_.emplace_back(GURL(not_frozen_url),
                                   /*metadata=*/std::nullopt);
  mojo::Remote<mojom::BidderWorklet> bidder_worklet = CreateWorklet();
  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        kScript);

  // Run 1, frozen.
  execution_mode_ = blink::mojom::InterestGroup::ExecutionMode::kFrozenContext;
  join_origin_ = url::Origin::Create(GURL("https://url.test/"));
  GenerateBid(bidder_worklet.get());
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  generate_bid_run_loop_->Run();
  EXPECT_THAT(bid_errors_, testing::ElementsAre());
  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(1, bids_[0]->bid);
  EXPECT_EQ(frozen_url, bids_[0]->ad_descriptor.url);

  // Run 2, frozen.
  GenerateBid(bidder_worklet.get());
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  generate_bid_run_loop_->Run();
  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(1, bids_[0]->bid);
  EXPECT_EQ(frozen_url, bids_[0]->ad_descriptor.url);

  // Run 3, grouped.
  execution_mode_ =
      blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode;
  GenerateBid(bidder_worklet.get());
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  generate_bid_run_loop_->Run();
  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(2, bids_[0]->bid);
  EXPECT_EQ(not_frozen_url, bids_[0]->ad_descriptor.url);

  // Run 4, grouped.
  GenerateBid(bidder_worklet.get());
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  generate_bid_run_loop_->Run();
  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(3, bids_[0]->bid);
  EXPECT_EQ(not_frozen_url, bids_[0]->ad_descriptor.url);

  // Run 5, frozen.
  execution_mode_ = blink::mojom::InterestGroup::ExecutionMode::kFrozenContext;
  GenerateBid(bidder_worklet.get());
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  generate_bid_run_loop_->Run();
  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(1, bids_[0]->bid);
  EXPECT_EQ(frozen_url, bids_[0]->ad_descriptor.url);

  // Run 6, compatibility
  execution_mode_ =
      blink::mojom::InterestGroup::ExecutionMode::kCompatibilityMode;
  GenerateBid(bidder_worklet.get());
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  generate_bid_run_loop_->Run();
  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(2, bids_[0]->bid);
  EXPECT_EQ(not_frozen_url, bids_[0]->ad_descriptor.url);

  // Run 7, frozen
  execution_mode_ = blink::mojom::InterestGroup::ExecutionMode::kFrozenContext;
  GenerateBid(bidder_worklet.get());
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  generate_bid_run_loop_->Run();
  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(1, bids_[0]->bid);
  EXPECT_EQ(frozen_url, bids_[0]->ad_descriptor.url);
}

TEST_F(BidderWorkletTest, ExecutionModeFrozenContextFails) {
  const char kScript[] = R"(
    const incrementer = (function() {
           let a = 1;
           return function() { a += 1; return a; };
         })();
    function generateBid() {
      return {ad: ["ad"], bid:incrementer(), render:"https://response.test/"};
    }
  )";

  mojo::Remote<mojom::BidderWorklet> bidder_worklet = CreateWorklet();
  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        kScript);

  execution_mode_ = blink::mojom::InterestGroup::ExecutionMode::kFrozenContext;
  join_origin_ = url::Origin::Create(GURL("https://url.test/"));
  GenerateBid(bidder_worklet.get());
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  generate_bid_run_loop_->Run();
  EXPECT_THAT(bid_errors_,
              testing::ElementsAre("undefined:0 Uncaught TypeError: Cannot "
                                   "DeepFreeze non-const value a."));
  EXPECT_TRUE(bids_.empty());
}

TEST_F(BidderWorkletTest, AlwaysReuseBidderContext) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      blink::features::kFledgeAlwaysReuseBidderContext);
  const char kScript[] = R"(
    const incrementer = (function() {
           let a = 1;
           return function() { a += 1; return a; };
         })();
    function generateBid() {
      return {ad: ["ad"], bid:incrementer(), render:"https://response.test/"};
    }
  )";

  mojo::Remote<mojom::BidderWorklet> bidder_worklet = CreateWorklet();
  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        kScript);

  // This will not fail because the execution mode is ignored. A frozen context
  // is not actually used.
  execution_mode_ = blink::mojom::InterestGroup::ExecutionMode::kFrozenContext;
  join_origin_ = url::Origin::Create(GURL("https://url.test/"));
  GenerateBid(bidder_worklet.get());
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  generate_bid_run_loop_->Run();
  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(2, bids_[0]->bid);

  // The context will still be reused when we switch to a different mode.
  execution_mode_ =
      blink::mojom::InterestGroup::ExecutionMode::kCompatibilityMode;
  GenerateBid(bidder_worklet.get());
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  generate_bid_run_loop_->Run();
  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(3, bids_[0]->bid);
  execution_mode_ =
      blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode;
  GenerateBid(bidder_worklet.get());
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  generate_bid_run_loop_->Run();
  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(4, bids_[0]->bid);

  // The context will still be reused when using a different origin in
  // kGroupedByOriginMode.
  join_origin_ = url::Origin::Create(GURL("https://url2.test/"));
  GenerateBid(bidder_worklet.get());
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  generate_bid_run_loop_->Run();
  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(5, bids_[0]->bid);
}

// Test that when `kFledgeAlwaysReuseBidderContext` is enabled, odd and even
// `GenerateBid()` calls consistently use separate, dedicated v8 contexts if the
// execution mode is not 'group-by-origin'. This indicates a round-robin thread
// selection. When the execution mode is 'group-by-origin', a fixed thread is
// used.
TEST_F(BidderWorkletTwoThreadsTest, AlwaysReuseBidderContext) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      blink::features::kFledgeAlwaysReuseBidderContext);
  const char kScript[] = R"(
    const incrementer = (function() {
           let a = 1;
           return function() { a += 1; return a; };
         })();
    function generateBid() {
      return {ad: ["ad"], bid:incrementer(), render:"https://response.test/"};
    }
  )";

  mojo::Remote<mojom::BidderWorklet> bidder_worklet = CreateWorklet();
  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        kScript);

  // This will not fail because the execution mode is ignored. A frozen context
  // is not actually used.
  execution_mode_ = blink::mojom::InterestGroup::ExecutionMode::kFrozenContext;
  join_origin_ = url::Origin::Create(GURL("https://url.test/"));
  GenerateBid(bidder_worklet.get());
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  generate_bid_run_loop_->Run();
  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(2, bids_[0]->bid);

  // The second GenerateBid will run on the second thread that owns a separate
  // context.
  GenerateBid(bidder_worklet.get());
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  generate_bid_run_loop_->Run();
  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(2, bids_[0]->bid);

  // The context will still be reused when we switch to a different mode. This
  // will run on the first thread.
  execution_mode_ =
      blink::mojom::InterestGroup::ExecutionMode::kCompatibilityMode;
  GenerateBid(bidder_worklet.get());
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  generate_bid_run_loop_->Run();
  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(3, bids_[0]->bid);

  // This may run on either thread depending on the hash of the join_origin.
  // The decision is non-persistent as FastHash is non-persistent. The
  // assertion accommodates both possible scenarios.
  execution_mode_ =
      blink::mojom::InterestGroup ::ExecutionMode::kGroupedByOriginMode;
  GenerateBid(bidder_worklet.get());
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  generate_bid_run_loop_->Run();
  ASSERT_EQ(1u, bids_.size());

  int generate_bid_therad =
      base::FastHash(last_bidder_join_origin_hash_salt_ + "https://url.test") %
      2;
  int expected_bid = (generate_bid_therad == 0) ? 4 : 3;
  EXPECT_EQ(expected_bid, bids_[0]->bid);
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
  base::WaitableEvent* event_handle = WedgeV8Thread(v8_helper().get());

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
  v8_helper()->v8_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<AuctionV8Helper> v8_helper) {
            v8_helper->set_script_timeout_for_testing(base::Days(360));
          },
          v8_helper()));

  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        "while(true) {}");

  mojo::Remote<mojom::BidderWorklet> bidder_worklet = CreateWorklet();
  // Let the script load.
  task_environment_.RunUntilIdle();

  // Now we no longer need it for parsing JS, wedge the V8 thread so we get a
  // chance to cancel the script *before* it actually tries running.
  base::WaitableEvent* event_handle = WedgeV8Thread(v8_helper().get());

  GenerateBid(bidder_worklet.get());
  bidder_worklet->ReportWin(
      is_for_additional_bid_, interest_group_name_reporting_id_,
      buyer_reporting_id_, buyer_and_seller_reporting_id_,
      selected_buyer_and_seller_reporting_id_, auction_signals_,
      per_buyer_signals_, direct_from_seller_per_buyer_signals_,
      direct_from_seller_per_buyer_signals_header_ad_slot_,
      direct_from_seller_auction_signals_,
      direct_from_seller_auction_signals_header_ad_slot_, seller_signals_,
      kanon_mode_, bid_is_kanon_, browser_signal_render_url_,
      browser_signal_bid_, browser_signal_bid_currency_,
      browser_signal_highest_scoring_other_bid_,
      browser_signal_highest_scoring_other_bid_currency_,
      browser_signal_made_highest_scoring_other_bid_, browser_signal_ad_cost_,
      browser_signal_modeling_signals_, browser_signal_join_count_,
      browser_signal_recency_report_win_, browser_signal_seller_origin_,
      browser_signal_top_level_seller_origin_,
      browser_signal_reporting_timeout_, data_version_,
      /*trace_id=*/1,
      base::BindOnce(
          [](const std::optional<GURL>& report_url,
             const base::flat_map<std::string, GURL>& ad_beacon_map,
             const base::flat_map<std::string, std::string>& ad_macro_map,
             PrivateAggregationRequests pa_requests,
             auction_worklet::mojom::BidderTimingMetricsPtr timing_metrics,
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
      CreateBasicGenerateBidScriptWithExtraCode(
          R"(forDebuggingOnly.reportAdAuctionLoss("https://loss.url");
            forDebuggingOnly.reportAdAuctionWin("https://win.url"))"),
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "[\"ad\"]", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/{}, GURL("https://loss.url"),
      GURL("https://win.url"));

  // It's OK to call one API but not the other.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateBasicGenerateBidScriptWithExtraCode(
          R"(forDebuggingOnly.reportAdAuctionLoss("https://loss.url"))"),
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "[\"ad\"]", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/{}, GURL("https://loss.url"),
      /*expected_debug_win_report_url=*/std::nullopt);
  RunGenerateBidWithJavascriptExpectingResult(
      CreateBasicGenerateBidScriptWithExtraCode(
          R"(forDebuggingOnly.reportAdAuctionWin("https://win.url"))"),
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "[\"ad\"]", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/{}, /*expected_debug_loss_report_url=*/std::nullopt,
      GURL("https://win.url"));

  // forDebuggingOnly binding errors are collected by bidder worklets.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateBasicGenerateBidScriptWithExtraCode(
          R"(forDebuggingOnly.reportAdAuctionLoss())"),
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/:5 Uncaught TypeError: "
       "reportAdAuctionLoss(): at least 1 argument(s) are required."},
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt);
}

// Test the case the debugging report URLs are exactly match and are just above
// the max URL length. When the length is hit, no errors are produced, but the
// debug report URLs are ignored.
TEST_F(BidderWorkletBiddingAndScoringDebugReportingAPIEnabledTest,
       ForDebuggingOnlyReportsLengthLimit) {
  std::string almost_too_long_loss_report_url = "https://loss.url/";
  almost_too_long_loss_report_url += std::string(
      url::kMaxURLChars - almost_too_long_loss_report_url.size(), '1');
  std::string too_long_loss_report_url = almost_too_long_loss_report_url + "2";

  std::string almost_too_long_win_report_url = "https://win.url/";
  almost_too_long_win_report_url += std::string(
      url::kMaxURLChars - almost_too_long_win_report_url.size(), '1');
  std::string too_long_win_report_url = almost_too_long_win_report_url + "2";

  ScopedInspectorSupport inspector_support(v8_helper().get());
  // Copying large URLs can cause flaky generateBid() timeouts with the default
  // value, even on the standard debug bots.
  per_buyer_timeout_ = TestTimeouts::action_max_timeout();

  // Almost too long loss report URL and too long win report URL. Mixing the too
  // long / not-too-long cases makes sure that we don't clear both URLs when one
  // is too longer, or check the length of the wrong URL.
  {
    AddJavascriptResponse(
        &url_loader_factory_, interest_group_bidding_url_,
        CreateBasicGenerateBidScriptWithExtraCode(
            base::StringPrintf(R"(forDebuggingOnly.reportAdAuctionLoss("%s");
                                  forDebuggingOnly.reportAdAuctionWin("%s"))",
                               almost_too_long_loss_report_url.c_str(),
                               too_long_win_report_url.c_str())));

    BidderWorklet* worklet_impl;
    auto worklet =
        CreateWorklet(interest_group_bidding_url_,
                      /*pause_for_debugger_on_start=*/false, &worklet_impl);

    int id = worklet_impl->context_group_ids_for_testing()[0];
    TestChannel* channel =
        inspector_support.ConnectDebuggerSessionAndRuntimeEnable(id);

    generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
    GenerateBid(worklet.get());
    generate_bid_run_loop_->Run();
    generate_bid_run_loop_.reset();

    EXPECT_THAT(bid_errors_, testing::ElementsAre());
    ASSERT_EQ(1u, bids_.size());
    EXPECT_EQ(bid_debug_loss_report_url_, almost_too_long_loss_report_url);
    EXPECT_FALSE(bid_debug_win_report_url_.has_value());

    channel->WaitForAndValidateConsoleMessage(
        "warning", /*json_args=*/
        "[{\"type\":\"string\", "
        "\"value\":\"reportAdAuctionWin accepts URLs of at most length "
        "2097152.\"}]",
        /*stack_trace_size=*/1, /*function=*/"generateBid",
        interest_group_bidding_url_, /*line_number=*/5);

    channel->ExpectNoMoreConsoleEvents();
  }

  // Too long loss report URL and almost too long win report URL.
  {
    AddJavascriptResponse(
        &url_loader_factory_, interest_group_bidding_url_,
        CreateBasicGenerateBidScriptWithExtraCode(
            base::StringPrintf(R"(forDebuggingOnly.reportAdAuctionLoss("%s");
                                  forDebuggingOnly.reportAdAuctionWin("%s"))",
                               too_long_loss_report_url.c_str(),
                               almost_too_long_win_report_url.c_str())));

    BidderWorklet* worklet_impl;
    auto worklet =
        CreateWorklet(interest_group_bidding_url_,
                      /*pause_for_debugger_on_start=*/false, &worklet_impl);

    int id = worklet_impl->context_group_ids_for_testing()[0];
    TestChannel* channel =
        inspector_support.ConnectDebuggerSessionAndRuntimeEnable(id);

    generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
    GenerateBid(worklet.get());
    generate_bid_run_loop_->Run();
    generate_bid_run_loop_.reset();

    EXPECT_THAT(bid_errors_, testing::ElementsAre());
    ASSERT_EQ(1u, bids_.size());
    EXPECT_FALSE(bid_debug_loss_report_url_.has_value());
    EXPECT_EQ(bid_debug_win_report_url_, almost_too_long_win_report_url);

    channel->WaitForAndValidateConsoleMessage(
        "warning", /*json_args=*/
        "[{\"type\":\"string\", "
        "\"value\":\"reportAdAuctionLoss accepts URLs of at most length "
        "2097152.\"}]",
        /*stack_trace_size=*/1, /*function=*/"generateBid",
        interest_group_bidding_url_, /*line_number=*/4);

    channel->ExpectNoMoreConsoleEvents();
  }
}

TEST_F(BidderWorkletBiddingAndScoringDebugReportingAPIEnabledTest,
       ForDebuggingOnlyArgumentTimeout) {
  RunGenerateBidWithJavascriptExpectingResult(
      CreateBasicGenerateBidScriptWithExtraCode(
          R"(forDebuggingOnly.reportAdAuctionLoss({
              toString:() => {while(true) {}}
          }))"),
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ execution of `generateBid` timed out."});

  RunGenerateBidWithJavascriptExpectingResult(
      CreateBasicGenerateBidScriptWithExtraCode(
          R"(forDebuggingOnly.reportAdAuctionWin({
              toString:() => {while(true) {}}
          }))"),
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ execution of `generateBid` timed out."});
}

// Debugging loss/win report URLs should be nullopt if generateBid() parameters
// are invalid.
TEST_F(BidderWorkletBiddingAndScoringDebugReportingAPIEnabledTest,
       ForDebuggingOnlyReportsInvalidGenerateBidParameter) {
  auction_signals_ = "invalid json";
  RunGenerateBidWithJavascriptExpectingResult(
      CreateBasicGenerateBidScriptWithExtraCode(
          R"(forDebuggingOnly.reportAdAuctionLoss("https://loss.url");
            forDebuggingOnly.reportAdAuctionWin("https://win.url"))"),
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt);
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
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/:6 Uncaught ReferenceError: error is not defined."},
      GURL("https://loss.url1"));
}

TEST_F(BidderWorkletBiddingAndScoringDebugReportingAPIEnabledTest,
       GenerateBidInvalidReturnValue) {
  // Keep debugging loss report URLs when generateBid() returns invalid value
  // type.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          R"({ad: ["ad"], bid:"invalid", render:"https://response.test/"})",
          R"(forDebuggingOnly.reportAdAuctionLoss("https://loss.url");
            forDebuggingOnly.reportAdAuctionWin("https://win.url"))"),
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ generateBid() Converting field 'bid' to a Number did "
       "not produce a finite double."},
      GURL("https://loss.url"),
      /*expected_debug_win_report_url=*/std::nullopt);
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
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://url.test/ execution of `generateBid` timed out."},
      GURL("https://loss.url1"));
}

TEST_F(BidderWorkletTest, ReportWinRegisterAdBeacon) {
  base::flat_map<std::string, GURL> expected_ad_beacon_map = {
      {"click", GURL("https://click.example.test/")},
      {"view", GURL("https://view.example.test/")},
  };
  RunReportWinWithFunctionBodyExpectingResult(
      R"(registerAdBeacon({
        'click': "https://click.example.test/",
        'view': "https://view.example.test/",
      }))",
      /*expected_report_url=*/std::nullopt, expected_ad_beacon_map);

  // Don't call twice.
  RunReportWinWithFunctionBodyExpectingResult(
      R"(registerAdBeacon({
        'click': "https://click.example.test/",
        'view': "https://view.example.test/",
      });
      registerAdBeacon())",
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_ad_macro_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:15 Uncaught TypeError: registerAdBeacon may be "
       "called at most once."});

  // If called twice and the error is caught, use the first result.
  RunReportWinWithFunctionBodyExpectingResult(
      R"(registerAdBeacon({
           'click': "https://click.example.test/",
           'view': "https://view.example.test/",
         });
         try { registerAdBeacon() }
         catch (e) {})",
      /*expected_report_url=*/std::nullopt, expected_ad_beacon_map);

  // If error on first call, can be called again.
  RunReportWinWithFunctionBodyExpectingResult(
      R"(try { registerAdBeacon() }
         catch (e) {}
         registerAdBeacon({
           'click': "https://click.example.test/",
           'view': "https://view.example.test/",
         }))",
      /*expected_report_url=*/std::nullopt, expected_ad_beacon_map);

  // Error if no parameters
  RunReportWinWithFunctionBodyExpectingResult(
      R"(registerAdBeacon())",
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_ad_macro_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:11 Uncaught TypeError: registerAdBeacon(): at least "
       "1 argument(s) are required."});

  // Error if parameter is not an object
  RunReportWinWithFunctionBodyExpectingResult(
      R"(registerAdBeacon("foo"))",
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_ad_macro_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:11 Uncaught TypeError: registerAdBeacon(): Cannot "
       "convert argument 'map' to a record since it's not an Object."});

  // OK if parameter attributes are not strings
  RunReportWinWithFunctionBodyExpectingResult(
      R"(registerAdBeacon({
        'click': "https://click.example.test/",
        1: "https://view.example.test/",
      }))",
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/
      {{"click", GURL("https://click.example.test/")},
       {"1", GURL("https://view.example.test/")}},
      /*expected_ad_macro_map=*/{},
      /*expected_pa_requests=*/{}, {});

  // ... but keys must be convertible to strings
  RunReportWinWithFunctionBodyExpectingResult(
      R"(let map = {
           'click': "https://click.example.test/"
         }
         map[Symbol('a')] = "https://view.example.test/";
         registerAdBeacon(map))",
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_ad_macro_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:15 Uncaught TypeError: Cannot convert a Symbol value "
       "to a string."});

  // Error if invalid reporting URL
  RunReportWinWithFunctionBodyExpectingResult(
      R"(registerAdBeacon({
        'click': "https://click.example.test/",
        'view': "gopher://view.example.test/",
      }))",
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_ad_macro_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:11 Uncaught TypeError: registerAdBeacon(): invalid "
       "reporting url for key 'view': 'gopher://view.example.test/'."});

  // Error if not trustworthy reporting URL
  RunReportWinWithFunctionBodyExpectingResult(
      R"(registerAdBeacon({
        'click': "https://127.0.0.1/",
        'view': "http://view.example.test/",
      }))",
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_ad_macro_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:11 Uncaught TypeError: registerAdBeacon(): invalid "
       "reporting url for key 'view': 'http://view.example.test/'."});

  // Error if invalid "reserved.*" reporting event type
  RunReportWinWithFunctionBodyExpectingResult(
      R"(registerAdBeacon({
        'click': "https://click.example.test/",
        'reserved.bogus': "https://view.example.test/",
      }))",
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_ad_macro_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:11 Uncaught TypeError: registerAdBeacon(): Invalid "
       "reserved type 'reserved.bogus' cannot be used."});

  // Special case for error message if the key has mismatched surrogates.
  RunReportWinWithFunctionBodyExpectingResult(
      R"(registerAdBeacon({
        '\ud835': "http://127.0.0.1/",
      }))",
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_ad_macro_map=*/{},
      /*expected_pa_requests=*/{},
      {"https://url.test/:11 Uncaught TypeError: registerAdBeacon(): invalid "
       "reporting url."});
}

TEST_F(BidderWorkletTest, ReportWinRegisterAdBeaconLongUrl) {
  // Copying large URLs can cause flaky generateBid() timeouts with the default
  // value, even on the standard debug bots.
  browser_signal_reporting_timeout_ = TestTimeouts::action_max_timeout();

  std::string almost_too_long_beacon_url = "https://long.url.test/";
  almost_too_long_beacon_url +=
      std::string(url::kMaxURLChars - almost_too_long_beacon_url.size(), '1');
  std::string too_long_beacon_url = almost_too_long_beacon_url + "2";

  ScopedInspectorSupport inspector_support(v8_helper().get());

  // A single beacon URL that is too long.
  {
    AddJavascriptResponse(
        &url_loader_factory_, interest_group_bidding_url_,
        CreateReportWinScript(base::StringPrintf(
            R"(registerAdBeacon({a:"%s"}))", too_long_beacon_url.c_str())));

    BidderWorklet* worklet_impl;
    auto worklet =
        CreateWorklet(interest_group_bidding_url_,
                      /*pause_for_debugger_on_start=*/false, &worklet_impl);

    int id = worklet_impl->context_group_ids_for_testing()[0];
    TestChannel* channel =
        inspector_support.ConnectDebuggerSessionAndRuntimeEnable(id);

    base::RunLoop run_loop;
    RunReportWinExpectingResultAsync(
        worklet_impl, /*expected_report_url=*/std::nullopt,
        /*expected_ad_beacon_map=*/{},
        /*expected_ad_macro_map=*/{},
        /*expected_pa_requests=*/{},
        /*expected_reporting_latency_timeout=*/false,
        /*expected_errors=*/{}, run_loop.QuitClosure());
    run_loop.Run();

    channel->WaitForAndValidateConsoleMessage(
        "warning", /*json_args=*/
        base::StringPrintf(
            "[{\"type\":\"string\", \"value\":\"registerAdBeacon(): passed URL "
            "of length %" PRIuS " but accepts URLs of at most length %" PRIuS
            ".\"}]",
            too_long_beacon_url.size(), url::kMaxURLChars),
        /*stack_trace_size=*/1, /*function=*/"reportWin",
        interest_group_bidding_url_, /*line_number=*/10);

    channel->ExpectNoMoreConsoleEvents();
  }

  // 3 beacon URLs, two of which are too long. The other one should make it
  // through, while the two that are too long should each display a separate
  // warning.
  {
    AddJavascriptResponse(
        &url_loader_factory_, interest_group_bidding_url_,
        CreateReportWinScript(base::StringPrintf(
            R"(registerAdBeacon({a:"%s", b:"%s", c:"%s"}))",
            too_long_beacon_url.c_str(), almost_too_long_beacon_url.c_str(),
            (too_long_beacon_url + "3").c_str())));

    BidderWorklet* worklet_impl;
    auto worklet =
        CreateWorklet(interest_group_bidding_url_,
                      /*pause_for_debugger_on_start=*/false, &worklet_impl);

    int id = worklet_impl->context_group_ids_for_testing()[0];
    TestChannel* channel =
        inspector_support.ConnectDebuggerSessionAndRuntimeEnable(id);

    base::RunLoop run_loop;
    RunReportWinExpectingResultAsync(
        worklet_impl, /*expected_report_url=*/std::nullopt,
        /*expected_ad_beacon_map=*/{{"b", GURL(almost_too_long_beacon_url)}},
        /*expected_ad_macro_map=*/{},
        /*expected_pa_requests=*/{},
        /*expected_reporting_latency_timeout=*/false,
        /*expected_errors=*/{}, run_loop.QuitClosure());
    run_loop.Run();

    channel->WaitForAndValidateConsoleMessage(
        "warning", /*json_args=*/
        base::StringPrintf(
            "[{\"type\":\"string\", \"value\":\"registerAdBeacon(): passed URL "
            "of length %" PRIuS " but accepts URLs of at most length %" PRIuS
            ".\"}]",
            too_long_beacon_url.size(), url::kMaxURLChars),
        /*stack_trace_size=*/1, /*function=*/"reportWin",
        interest_group_bidding_url_, /*line_number=*/10);

    channel->WaitForAndValidateConsoleMessage(
        "warning", /*json_args=*/
        base::StringPrintf(
            "[{\"type\":\"string\", \"value\":\"registerAdBeacon(): passed URL "
            "of length %" PRIuS " but accepts URLs of at most length %" PRIuS
            ".\"}]",
            too_long_beacon_url.size() + 1, url::kMaxURLChars),
        /*stack_trace_size=*/1, /*function=*/"reportWin",
        interest_group_bidding_url_, /*line_number=*/10);

    channel->ExpectNoMoreConsoleEvents();
  }
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
      /*expected_bids=*/nullptr,
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/
      {"https://url.test/:6 Uncaught ReferenceError: sharedStorage is not "
       "defined."},
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_set_priority=*/std::nullopt,
      /*expected_update_priority_signals_overrides=*/{},
      /*expected_pa_requests=*/{});

  RunReportWinWithFunctionBodyExpectingResult(
      R"(
        sharedStorage.clear();
      )",
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{}, /*expected_ad_macro_map=*/{},
      /*expected_pa_requests=*/{},
      /*expected_errors=*/
      {"https://url.test/:12 Uncaught ReferenceError: sharedStorage is not "
       "defined."});
}

class BidderWorkletSharedStorageAPIEnabledTest : public BidderWorkletTest {
 public:
  BidderWorkletSharedStorageAPIEnabledTest() {
    feature_list_.InitAndEnableFeature(blink::features::kSharedStorageAPI);

    // Set the shared-storage permissions policy to allowed.
    permissions_policy_state_ =
        mojom::AuctionWorkletPermissionsPolicyState::New(
            /*private_aggregation_allowed=*/true,
            /*shared_storage_allowed=*/true);
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
    shared_storage_hosts_[0] = receiver.BindNewPipeAndPassRemote();

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
        /*expected_bids=*/
        mojom::BidderWorkletBid::New(
            auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 1,
            /*bid_currency=*/std::nullopt,
            /*ad_cost=*/std::nullopt,
            blink::AdDescriptor(GURL("https://response.test/")),
            /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
            /*ad_component_descriptors=*/std::nullopt,
            /*modeling_signals=*/std::nullopt, base::TimeDelta()),
        /*expected_data_version=*/std::nullopt,
        /*expected_errors=*/{},
        /*expected_debug_loss_report_url=*/std::nullopt,
        /*expected_debug_win_report_url=*/std::nullopt,
        /*expected_set_priority=*/std::nullopt,
        /*expected_update_priority_signals_overrides=*/{},
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
                        mojom::AuctionWorkletFunction::kBidderGenerateBid},
            Request{.type = RequestType::kSet,
                    .key = u"a",
                    .value = u"b",
                    .ignore_if_present = true,
                    .source_auction_worklet_function =
                        mojom::AuctionWorkletFunction::kBidderGenerateBid},
            Request{.type = RequestType::kAppend,
                    .key = u"a",
                    .value = u"b",
                    .ignore_if_present = false,
                    .source_auction_worklet_function =
                        mojom::AuctionWorkletFunction::kBidderGenerateBid},
            Request{.type = RequestType::kDelete,
                    .key = u"a",
                    .value = std::u16string(),
                    .ignore_if_present = false,
                    .source_auction_worklet_function =
                        mojom::AuctionWorkletFunction::kBidderGenerateBid},
            Request{.type = RequestType::kClear,
                    .key = std::u16string(),
                    .value = std::u16string(),
                    .ignore_if_present = false,
                    .source_auction_worklet_function =
                        mojom::AuctionWorkletFunction::kBidderGenerateBid}));
  }

  {
    shared_storage_hosts_[0] =
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
        /*expected_bids=*/nullptr,
        /*expected_data_version=*/std::nullopt,
        /*expected_errors=*/
        {"https://url.test/:6 Uncaught TypeError: The \"shared-storage\" "
         "Permissions Policy denied the method on sharedStorage."},
        /*expected_debug_loss_report_url=*/std::nullopt,
        /*expected_debug_win_report_url=*/std::nullopt,
        /*expected_set_priority=*/std::nullopt,
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
    shared_storage_hosts_[0] = receiver.BindNewPipeAndPassRemote();

    RunReportWinWithFunctionBodyExpectingResult(
        R"(
          sharedStorage.set('a', 'b');
          sharedStorage.set('a', 'b', {ignoreIfPresent: true});
          sharedStorage.append('a', 'b');
          sharedStorage.delete('a');
          sharedStorage.clear();
        )",
        /*expected_report_url=*/std::nullopt,
        /*expected_ad_beacon_map=*/{}, /*expected_ad_macro_map=*/{},
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
                        mojom::AuctionWorkletFunction::kBidderReportWin},
            Request{.type = RequestType::kSet,
                    .key = u"a",
                    .value = u"b",
                    .ignore_if_present = true,
                    .source_auction_worklet_function =
                        mojom::AuctionWorkletFunction::kBidderReportWin},
            Request{.type = RequestType::kAppend,
                    .key = u"a",
                    .value = u"b",
                    .ignore_if_present = false,
                    .source_auction_worklet_function =
                        mojom::AuctionWorkletFunction::kBidderReportWin},
            Request{.type = RequestType::kDelete,
                    .key = u"a",
                    .value = std::u16string(),
                    .ignore_if_present = false,
                    .source_auction_worklet_function =
                        mojom::AuctionWorkletFunction::kBidderReportWin},
            Request{.type = RequestType::kClear,
                    .key = std::u16string(),
                    .value = std::u16string(),
                    .ignore_if_present = false,
                    .source_auction_worklet_function =
                        mojom::AuctionWorkletFunction::kBidderReportWin}));
  }

  {
    shared_storage_hosts_[0] =
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
        /*expected_report_url=*/std::nullopt,
        /*expected_ad_beacon_map=*/{}, /*expected_ad_macro_map=*/{},
        /*expected_pa_requests=*/{},
        /*expected_errors=*/
        {"https://url.test/:12 Uncaught TypeError: The \"shared-storage\" "
         "Permissions Policy denied the method on sharedStorage."});

    permissions_policy_state_ =
        mojom::AuctionWorkletPermissionsPolicyState::New(
            /*private_aggregation_allowed=*/true,
            /*shared_storage_allowed=*/true);
  }

  {
    mojo::Receiver<auction_worklet::mojom::AuctionSharedStorageHost> receiver(
        &test_shared_storage_host);
    shared_storage_hosts_[0] = receiver.BindNewPipeAndPassRemote();

    const char kBody[] = R"(
      sharedStorage.delete({toString: () => {
          while(true) {}
        }
      });
    )";
    AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                          CreateReportWinScript(kBody));

    RunReportWinExpectingResult(
        /*expected_report_url=*/std::nullopt,
        /*expected_ad_beacon_map=*/{},
        /*expected_ad_macro_map=*/{},
        /*expected_pa_requests=*/{},
        /*expected_reporting_latency_timeout=*/true,
        /*expected_errors=*/
        {"https://url.test/ execution of `reportWin` timed out."});
  }
}

class BidderWorkletTwoThreadsSharedStorageAPIEnabledTest
    : public BidderWorkletSharedStorageAPIEnabledTest {
 public:
  size_t NumThreads() override { return 2u; }
};

TEST_F(BidderWorkletTwoThreadsSharedStorageAPIEnabledTest,
       SharedStorageWriteInGenerateBid) {
  auction_worklet::TestAuctionSharedStorageHost test_shared_storage_host0;
  auction_worklet::TestAuctionSharedStorageHost test_shared_storage_host1;

  mojo::Receiver<auction_worklet::mojom::AuctionSharedStorageHost> receiver0(
      &test_shared_storage_host0);
  shared_storage_hosts_[0] = receiver0.BindNewPipeAndPassRemote();

  mojo::Receiver<auction_worklet::mojom::AuctionSharedStorageHost> receiver1(
      &test_shared_storage_host0);
  shared_storage_hosts_[1] = receiver1.BindNewPipeAndPassRemote();

  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          R"({ad: "ad", bid:1, render:"https://response.test/" })",
          /*extra_code=*/R"(
          sharedStorage.set('a', 'b');
        )"),
      /*expected_bids=*/
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_set_priority=*/std::nullopt,
      /*expected_update_priority_signals_overrides=*/{},
      /*expected_pa_requests=*/{});

  // Make sure the shared storage mojom methods are invoked as they use a
  // dedicated pipe.
  task_environment_.RunUntilIdle();

  // Expect that only the shared storage host corresponding to the thread
  // handling the GenerateBid has observed the requests.
  EXPECT_TRUE(test_shared_storage_host1.observed_requests().empty());

  using RequestType =
      auction_worklet::TestAuctionSharedStorageHost::RequestType;
  using Request = auction_worklet::TestAuctionSharedStorageHost::Request;

  EXPECT_THAT(test_shared_storage_host0.observed_requests(),
              testing::ElementsAre(Request{
                  .type = RequestType::kSet,
                  .key = u"a",
                  .value = u"b",
                  .ignore_if_present = false,
                  .source_auction_worklet_function =
                      mojom::AuctionWorkletFunction::kBidderGenerateBid}));
}

class BidderWorkletPrivateAggregationEnabledTest : public BidderWorkletTest {
 public:
  BidderWorkletPrivateAggregationEnabledTest() {
    feature_list_.InitAndEnableFeature(blink::features::kPrivateAggregationApi);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(BidderWorkletPrivateAggregationEnabledTest, GenerateBid) {
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
              /*bucket=*/absl::MakeInt128(/*high=*/1,
                                          /*low=*/0),
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

  // Only contributeToHistogram() is called.
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(kExpectedRequest1.Clone());

    RunGenerateBidWithJavascriptExpectingResult(
        CreateGenerateBidScript(
            R"({ad: "ad", bid:1, render:"https://response.test/" })",
            /*extra_code=*/R"(
            privateAggregation.contributeToHistogram({bucket: 123n, value: 45});
          )"),
        /*expected_bids=*/
        mojom::BidderWorkletBid::New(
            auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 1,
            /*bid_currency=*/std::nullopt,
            /*ad_cost=*/std::nullopt,
            blink::AdDescriptor(GURL("https://response.test/")),
            /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
            /*ad_component_descriptors=*/std::nullopt,
            /*modeling_signals=*/std::nullopt, base::TimeDelta()),
        /*expected_data_version=*/std::nullopt,
        /*expected_errors=*/{},
        /*expected_debug_loss_report_url=*/std::nullopt,
        /*expected_debug_win_report_url=*/std::nullopt,
        /*expected_set_priority=*/std::nullopt,
        /*expected_update_priority_signals_overrides=*/{},
        std::move(expected_pa_requests));
  }

  // Only contributeToHistogramOnEvent() is called.
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(kExpectedForEventRequest1.Clone());

    RunGenerateBidWithJavascriptExpectingResult(
        CreateGenerateBidScript(
            R"({ad: "ad", bid:1, render:"https://response.test/" })",
            /*extra_code=*/R"(
            privateAggregation.contributeToHistogramOnEvent(
                "reserved.win", {bucket: 234n, value: 56});
          )"),
        /*expected_bids=*/
        mojom::BidderWorkletBid::New(
            auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 1,
            /*bid_currency=*/std::nullopt,
            /*ad_cost=*/std::nullopt,
            blink::AdDescriptor(GURL("https://response.test/")),
            /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
            /*ad_component_descriptors=*/std::nullopt,
            /*modeling_signals=*/std::nullopt, base::TimeDelta()),
        /*expected_data_version=*/std::nullopt,
        /*expected_errors=*/{},
        /*expected_debug_loss_report_url=*/std::nullopt,
        /*expected_debug_win_report_url=*/std::nullopt,
        /*expected_set_priority=*/std::nullopt,
        /*expected_update_priority_signals_overrides=*/{},
        std::move(expected_pa_requests));
  }

  // Both contributeToHistogram() and contributeToHistogramOnEvent() are called.
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(kExpectedRequest1.Clone());
    expected_pa_requests.push_back(kExpectedForEventRequest1.Clone());

    RunGenerateBidWithJavascriptExpectingResult(
        CreateGenerateBidScript(
            R"({ad: "ad", bid:1, render:"https://response.test/" })",
            /*extra_code=*/R"(
            privateAggregation.contributeToHistogram({bucket: 123n, value: 45});
            privateAggregation.contributeToHistogramOnEvent(
                "reserved.win", {bucket: 234n, value: 56});
          )"),
        /*expected_bids=*/
        mojom::BidderWorkletBid::New(
            auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 1,
            /*bid_currency=*/std::nullopt,
            /*ad_cost=*/std::nullopt,
            blink::AdDescriptor(GURL("https://response.test/")),
            /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
            /*ad_component_descriptors=*/std::nullopt,
            /*modeling_signals=*/std::nullopt, base::TimeDelta()),
        /*expected_data_version=*/std::nullopt,
        /*expected_errors=*/{},
        /*expected_debug_loss_report_url=*/std::nullopt,
        /*expected_debug_win_report_url=*/std::nullopt,
        /*expected_set_priority=*/std::nullopt,
        /*expected_update_priority_signals_overrides=*/{},
        std::move(expected_pa_requests));
  }

  // Set the private-aggregation permissions policy to disallowed.
  {
    permissions_policy_state_ =
        mojom::AuctionWorkletPermissionsPolicyState::New(
            /*private_aggregation_allowed=*/false,
            /*shared_storage_allowed=*/false);

    RunGenerateBidWithJavascriptExpectingResult(
        CreateGenerateBidScript(
            R"({ad: "ad", bid:1, render:"https://response.test/" })",
            /*extra_code=*/R"(
            privateAggregation.contributeToHistogram({bucket: 123n, value: 45});
          )"),
        /*expected_bids=*/nullptr,
        /*expected_data_version=*/std::nullopt,
        /*expected_errors=*/
        {"https://url.test/:6 Uncaught TypeError: The \"private-aggregation\" "
         "Permissions Policy denied the method on privateAggregation."},
        /*expected_debug_loss_report_url=*/std::nullopt,
        /*expected_debug_win_report_url=*/std::nullopt,
        /*expected_set_priority=*/std::nullopt,
        /*expected_update_priority_signals_overrides=*/{},
        /*expected_pa_requests=*/{});

    // Currently this goes through with default flags, while it should throw.
    // See the ContributeToHistogramOnEventPermission* tests for the story on
    // getting it fixed.
    PrivateAggregationRequests incorrectly_expected_pa_requests;
    incorrectly_expected_pa_requests.push_back(
        kExpectedForEventRequest1.Clone());
    RunGenerateBidWithJavascriptExpectingResult(
        CreateGenerateBidScript(
            R"({ad: "ad", bid:1, render:"https://response.test/" })",
            /*extra_code=*/R"(
            privateAggregation.contributeToHistogramOnEvent(
                "reserved.win", {bucket: 234n, value: 56});
          )"),
        /*expected_bids=*/
        mojom::BidderWorkletBid::New(
            auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 1,
            /*bid_currency=*/std::nullopt,
            /*ad_cost=*/std::nullopt,
            blink::AdDescriptor(GURL("https://response.test/")),
            /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
            /*ad_component_descriptors=*/std::nullopt,
            /*modeling_signals=*/std::nullopt, base::TimeDelta()),
        /*expected_data_version=*/std::nullopt,
        /*expected_errors=*/{},
        /*expected_debug_loss_report_url=*/std::nullopt,
        /*expected_debug_win_report_url=*/std::nullopt,
        /*expected_set_priority=*/std::nullopt,
        /*expected_update_priority_signals_overrides=*/{},
        /*expected_pa_requests=*/std::move(incorrectly_expected_pa_requests));

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

    RunGenerateBidWithJavascriptExpectingResult(
        CreateGenerateBidScript(
            R"({ad: "ad", bid:1, render:"https://response.test/" })",
            /*extra_code=*/R"(
            privateAggregation.contributeToHistogram(
                {bucket: 18446744073709551616n, value: 1});
            privateAggregation.contributeToHistogramOnEvent(
                "reserved.win", {bucket: 18446744073709551616n, value: 2});
          )"),
        /*expected_bids=*/
        mojom::BidderWorkletBid::New(
            auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 1,
            /*bid_currency=*/std::nullopt,
            /*ad_cost=*/std::nullopt,
            blink::AdDescriptor(GURL("https://response.test/")),
            /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
            /*ad_component_descriptors=*/std::nullopt,
            /*modeling_signals=*/std::nullopt, base::TimeDelta()),
        /*expected_data_version=*/std::nullopt,
        /*expected_errors=*/{},
        /*expected_debug_loss_report_url=*/std::nullopt,
        /*expected_debug_win_report_url=*/std::nullopt,
        /*expected_set_priority=*/std::nullopt,
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
            privateAggregation.contributeToHistogram({bucket: 123n, value: 45});
            privateAggregation.contributeToHistogram(
                {bucket: 18446744073709551616n, value: 1});
            privateAggregation.contributeToHistogramOnEvent(
                "reserved.win", {bucket: 234n, value: 56});
            privateAggregation.contributeToHistogramOnEvent(
                "reserved.win", {bucket: 18446744073709551616n, value: 2});
          )"),
        /*expected_bids=*/
        mojom::BidderWorkletBid::New(
            auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 1,
            /*bid_currency=*/std::nullopt,
            /*ad_cost=*/std::nullopt,
            blink::AdDescriptor(GURL("https://response.test/")),
            /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
            /*ad_component_descriptors=*/std::nullopt,
            /*modeling_signals=*/std::nullopt, base::TimeDelta()),
        /*expected_data_version=*/std::nullopt,
        /*expected_errors=*/{},
        /*expected_debug_loss_report_url=*/std::nullopt,
        /*expected_debug_win_report_url=*/std::nullopt,
        /*expected_set_priority=*/std::nullopt,
        /*expected_update_priority_signals_overrides=*/{},
        std::move(expected_pa_requests));
  }

  // An unrelated exception after contributeToHistogram and
  // contributeToHistogramOnEvent shouldn't block the reports.
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(kExpectedRequest1.Clone());
    expected_pa_requests.push_back(kExpectedForEventRequest1.Clone());

    RunGenerateBidWithJavascriptExpectingResult(
        CreateGenerateBidScript(
            R"({ad: "ad", bid:1, render:"https://response.test/" })",
            /*extra_code=*/R"(
            privateAggregation.contributeToHistogram({bucket: 123n, value: 45});
            privateAggregation.contributeToHistogramOnEvent(
                "reserved.win", {bucket: 234n, value: 56});
            error;
          )"),
        /*expected_bids=*/mojom::BidderWorkletBidPtr(),
        /*expected_data_version=*/std::nullopt,
        /*expected_errors=*/
        {"https://url.test/:9 Uncaught ReferenceError: error is not defined."},
        /*expected_debug_loss_report_url=*/std::nullopt,
        /*expected_debug_win_report_url=*/std::nullopt,
        /*expected_set_priority=*/std::nullopt,
        /*expected_update_priority_signals_overrides=*/{},
        std::move(expected_pa_requests));
  }

  // Debug mode enabled with debug key
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(
        auction_worklet::mojom::PrivateAggregationRequest::New(
            kExpectedRequest1.contribution->Clone(),
            blink::mojom::AggregationServiceMode::kDefault,
            blink::mojom::DebugModeDetails::New(
                /*is_enabled=*/true, blink::mojom::DebugKey::New(1234u))));
    expected_pa_requests.push_back(
        auction_worklet::mojom::PrivateAggregationRequest::New(
            kExpectedForEventRequest1.contribution->Clone(),
            blink::mojom::AggregationServiceMode::kDefault,
            blink::mojom::DebugModeDetails::New(
                /*is_enabled=*/true, blink::mojom::DebugKey::New(1234u))));

    RunGenerateBidWithJavascriptExpectingResult(
        CreateGenerateBidScript(
            R"({ad: "ad", bid:1, render:"https://response.test/" })",
            /*extra_code=*/R"(
            privateAggregation.enableDebugMode({debugKey: 1234n});
            privateAggregation.contributeToHistogram({bucket: 123n, value: 45});
            privateAggregation.contributeToHistogramOnEvent(
                "reserved.win", {bucket: 234n, value: 56});
          )"),
        /*expected_bids=*/
        mojom::BidderWorkletBid::New(
            auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 1,
            /*bid_currency=*/std::nullopt,
            /*ad_cost=*/std::nullopt,
            blink::AdDescriptor(GURL("https://response.test/")),
            /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
            /*ad_component_descriptors=*/std::nullopt,
            /*modeling_signals=*/std::nullopt, base::TimeDelta()),
        /*expected_data_version=*/std::nullopt,
        /*expected_errors=*/{},
        /*expected_debug_loss_report_url=*/std::nullopt,
        /*expected_debug_win_report_url=*/std::nullopt,
        /*expected_set_priority=*/std::nullopt,
        /*expected_update_priority_signals_overrides=*/{},
        std::move(expected_pa_requests));
  }

  // Debug mode enabled without debug key, but with multiple requests
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(
        auction_worklet::mojom::PrivateAggregationRequest::New(
            kExpectedRequest1.contribution->Clone(),
            blink::mojom::AggregationServiceMode::kDefault,
            blink::mojom::DebugModeDetails::New(
                /*is_enabled=*/true, /*debug_key=*/nullptr)));
    expected_pa_requests.push_back(
        auction_worklet::mojom::PrivateAggregationRequest::New(
            kExpectedRequest2.contribution->Clone(),
            blink::mojom::AggregationServiceMode::kDefault,
            blink::mojom::DebugModeDetails::New(
                /*is_enabled=*/true, /*debug_key=*/nullptr)));

    RunGenerateBidWithJavascriptExpectingResult(
        CreateGenerateBidScript(
            R"({ad: "ad", bid:1, render:"https://response.test/" })",
            /*extra_code=*/R"(
            privateAggregation.enableDebugMode();
            privateAggregation.contributeToHistogram({bucket: 123n, value: 45});
            privateAggregation.contributeToHistogram(
                {bucket: 18446744073709551616n, value: 1});
          )"),
        /*expected_bids=*/
        mojom::BidderWorkletBid::New(
            auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 1,
            /*bid_currency=*/std::nullopt,
            /*ad_cost=*/std::nullopt,
            blink::AdDescriptor(GURL("https://response.test/")),
            /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
            /*ad_component_descriptors=*/std::nullopt,
            /*modeling_signals=*/std::nullopt, base::TimeDelta()),
        /*expected_data_version=*/std::nullopt,
        /*expected_errors=*/{},
        /*expected_debug_loss_report_url=*/std::nullopt,
        /*expected_debug_win_report_url=*/std::nullopt,
        /*expected_set_priority=*/std::nullopt,
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
        /*expected_bids=*/
        mojom::BidderWorkletBidPtr(),
        /*expected_data_version=*/std::nullopt,
        /*expected_errors=*/
        {"https://url.test/:7 Uncaught TypeError: enableDebugMode may be "
         "called at most once."},
        /*expected_debug_loss_report_url=*/std::nullopt,
        /*expected_debug_win_report_url=*/std::nullopt,
        /*expected_set_priority=*/std::nullopt,
        /*expected_update_priority_signals_overrides=*/{},
        /*expected_pa_requests=*/{});
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

    RunGenerateBidWithJavascriptExpectingResult(
        CreateGenerateBidScript(
            R"({ad: "ad", bid:1, render:"https://response.test/" })",
            /*extra_code=*/R"(
            privateAggregation.contributeToHistogram(
                {bucket: 123n, value: 45, filteringId: 0n});
            privateAggregation.contributeToHistogramOnEvent(
                "reserved.win", {bucket: 234n, value: 56, filteringId: 255n});
          )"),
        /*expected_bid=*/
        mojom::BidderWorkletBid::New(
            auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 1,
            /*bid_currency=*/std::nullopt,
            /*ad_cost=*/std::nullopt,
            blink::AdDescriptor(GURL("https://response.test/")),
            /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
            /*ad_component_descriptors=*/std::nullopt,
            /*modeling_signals=*/std::nullopt, base::TimeDelta()),
        /*expected_data_version=*/std::nullopt,
        /*expected_errors=*/{},
        /*expected_debug_loss_report_url=*/std::nullopt,
        /*expected_debug_win_report_url=*/std::nullopt,
        /*expected_set_priority=*/std::nullopt,
        /*expected_update_priority_signals_overrides=*/{},
        std::move(expected_pa_requests));
  }

  // Requests where reject reason is specified and k-anonymity enforcement is
  // active.
  kanon_mode_ = mojom::KAnonymityBidMode::kEnforce;
  {
    PrivateAggregationRequests expected_non_kanon_pa_requests;
    expected_non_kanon_pa_requests.push_back(
        mojom::PrivateAggregationRequest::New(
            mojom::AggregatableReportContribution::NewForEventContribution(
                mojom::AggregatableReportForEventContribution::New(
                    /*bucket=*/mojom::ForEventSignalBucket::NewSignalBucket(
                        mojom::SignalBucket::New(
                            /*baseValue=*/mojom::BaseValue::kWinningBid,
                            /*scale=*/1.0,
                            /*offset=*/nullptr)),
                    /*value=*/
                    mojom::ForEventSignalValue::NewSignalValue(
                        mojom::SignalValue::New(
                            /*baseValue=*/mojom::BaseValue::kBidRejectReason,
                            /*scale=*/1.0,
                            /*offset=*/0)),
                    /*filtering_id=*/std::nullopt,
                    /*event_type=*/
                    mojom::EventType::NewReserved(
                        mojom::ReservedEventType::kReservedLoss))),
            blink::mojom::AggregationServiceMode::kDefault,
            blink::mojom::DebugModeDetails::New()));

    RunGenerateBidWithJavascriptExpectingResult(
        CreateGenerateBidScript(
            R"({ad: "ad", bid:1, render:"https://response.test/" })",
            /*extra_code=*/R"(
            if (interestGroup.ads.length > 0) {
              privateAggregation.contributeToHistogram(
                  {bucket: 18446744073709551616n, value: 1});
              privateAggregation.contributeToHistogramOnEvent(
                  "reserved.win", {bucket: 18446744073709551616n, value: 2});
              privateAggregation.contributeToHistogramOnEvent("reserved.loss", {
                bucket: {baseValue: "highest-scoring-other-bid", offset: 0n},
                value: 1,
              });
              privateAggregation.contributeToHistogramOnEvent("reserved.loss", {
                bucket: {baseValue: "winning-bid"},
                value: {baseValue: "bid-reject-reason"},
              });
            }
          )"),
        /*expected_bids=*/
        mojom::BidderWorkletBid::New(
            auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 1,
            /*bid_currency=*/std::nullopt,
            /*ad_cost=*/std::nullopt,
            blink::AdDescriptor(GURL("https://response.test/")),
            /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
            /*ad_component_descriptors=*/std::nullopt,
            /*modeling_signals=*/std::nullopt, base::TimeDelta()),
        /*expected_data_version=*/std::nullopt,
        /*expected_errors=*/{},
        /*expected_debug_loss_report_url=*/std::nullopt,
        /*expected_debug_win_report_url=*/std::nullopt,
        /*expected_set_priority=*/std::nullopt,
        /*expected_update_priority_signals_overrides=*/{},
        /*expected_pa_requests=*/{},
        /*expected_non_kanon_pa_requests=*/
        std::move(expected_non_kanon_pa_requests));
  }
}

TEST_F(BidderWorkletPrivateAggregationEnabledTest, ReportWin) {
  auction_worklet::mojom::PrivateAggregationRequest kExpectedRequest1(
      mojom::AggregatableReportContribution::NewHistogramContribution(
          blink::mojom::AggregatableReportHistogramContribution::New(
              /*bucket=*/123,
              /*value=*/45,
              /*filtering_id=*/std::nullopt)),
      blink::mojom::AggregationServiceMode::kDefault,
      blink::mojom::DebugModeDetails::New());
  auction_worklet::mojom::PrivateAggregationRequest kExpectedRequest2(
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

    RunReportWinWithFunctionBodyExpectingResult(
        R"(
          privateAggregation.contributeToHistogram({bucket: 123n, value: 45});
        )",
        /*expected_report_url=*/std::nullopt,
        /*expected_ad_beacon_map=*/{}, /*expected_ad_macro_map=*/{},
        std::move(expected_pa_requests),
        /*expected_errors=*/{});
  }

  // Set the private-aggregation permissions policy to disallowed.
  {
    permissions_policy_state_ =
        mojom::AuctionWorkletPermissionsPolicyState::New(
            /*private_aggregation_allowed=*/false,
            /*shared_storage_allowed=*/false);

    RunReportWinWithFunctionBodyExpectingResult(
        R"(
          privateAggregation.contributeToHistogram({bucket: 123n, value: 45});
        )",
        /*expected_report_url=*/std::nullopt,
        /*expected_ad_beacon_map=*/{}, /*expected_ad_macro_map=*/{},
        /*expected_pa_requests=*/{},
        /*expected_errors=*/
        {"https://url.test/:12 Uncaught TypeError: The \"private-aggregation\" "
         "Permissions Policy denied the method on privateAggregation."});

    permissions_policy_state_ =
        mojom::AuctionWorkletPermissionsPolicyState::New(
            /*private_aggregation_allowed=*/true,
            /*shared_storage_allowed=*/false);
  }

  // Large bucket
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(kExpectedRequest2.Clone());

    RunReportWinWithFunctionBodyExpectingResult(
        R"(
          privateAggregation.contributeToHistogram(
              {bucket: 18446744073709551616n, value: 1});
        )",
        /*expected_report_url=*/std::nullopt,
        /*expected_ad_beacon_map=*/{}, /*expected_ad_macro_map=*/{},
        std::move(expected_pa_requests),
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
          privateAggregation.contributeToHistogram({bucket: 123n, value: 45});
          privateAggregation.contributeToHistogram(
              {bucket: 18446744073709551616n, value: 1});
          privateAggregation.contributeToHistogramOnEvent(
              "reserved.win", {bucket: 234n, value: 56});
          privateAggregation.contributeToHistogramOnEvent(
              "reserved.win", {bucket: 18446744073709551616n, value: 2});
        )",
        /*expected_report_url=*/std::nullopt,
        /*expected_ad_beacon_map=*/{}, /*expected_ad_macro_map=*/{},
        std::move(expected_pa_requests),
        /*expected_errors=*/{});
  }

  // An unrelated exception after contributeToHistogram shouldn't block the
  // report
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(kExpectedRequest1.Clone());

    RunReportWinWithFunctionBodyExpectingResult(
        R"(
          privateAggregation.contributeToHistogram({bucket: 123n, value: 45});
          error;
        )",
        /*expected_report_url=*/std::nullopt,
        /*expected_ad_beacon_map=*/{}, /*expected_ad_macro_map=*/{},
        std::move(expected_pa_requests),
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
            blink::mojom::AggregationServiceMode::kDefault,
            blink::mojom::DebugModeDetails::New(
                /*is_enabled=*/true, blink::mojom::DebugKey::New(1234u))));
    expected_pa_requests.push_back(
        auction_worklet::mojom::PrivateAggregationRequest::New(
            kExpectedForEventRequest1.contribution->Clone(),
            blink::mojom::AggregationServiceMode::kDefault,
            blink::mojom::DebugModeDetails::New(
                /*is_enabled=*/true, blink::mojom::DebugKey::New(1234u))));

    RunReportWinWithFunctionBodyExpectingResult(
        R"(
            privateAggregation.enableDebugMode({debugKey: 1234n});
            privateAggregation.contributeToHistogram({bucket: 123n, value: 45});
            privateAggregation.contributeToHistogramOnEvent(
                "reserved.win", {bucket: 234n, value: 56});
        )",
        /*expected_report_url=*/std::nullopt,
        /*expected_ad_beacon_map=*/{}, /*expected_ad_macro_map=*/{},
        std::move(expected_pa_requests),
        /*expected_errors=*/{});
  }

  // Debug mode enabled without debug key, but with multiple requests
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(
        auction_worklet::mojom::PrivateAggregationRequest::New(
            kExpectedRequest1.contribution->Clone(),
            blink::mojom::AggregationServiceMode::kDefault,
            blink::mojom::DebugModeDetails::New(
                /*is_enabled=*/true, /*debug_key=*/nullptr)));
    expected_pa_requests.push_back(
        auction_worklet::mojom::PrivateAggregationRequest::New(
            kExpectedRequest2.contribution->Clone(),
            blink::mojom::AggregationServiceMode::kDefault,
            blink::mojom::DebugModeDetails::New(
                /*is_enabled=*/true, /*debug_key=*/nullptr)));

    RunReportWinWithFunctionBodyExpectingResult(
        R"(
            privateAggregation.enableDebugMode();
            privateAggregation.contributeToHistogram({bucket: 123n, value: 45});
            privateAggregation.contributeToHistogram(
                {bucket: 18446744073709551616n, value: 1});
        )",
        /*expected_report_url=*/std::nullopt,
        /*expected_ad_beacon_map=*/{}, /*expected_ad_macro_map=*/{},
        std::move(expected_pa_requests),
        /*expected_errors=*/{});
  }

  // For-event report and histogram report are reported.
  {
    PrivateAggregationRequests expected_pa_requests;
    expected_pa_requests.push_back(kExpectedRequest1.Clone());
    expected_pa_requests.push_back(kExpectedForEventRequest1.Clone());

    RunReportWinWithFunctionBodyExpectingResult(
        R"(
          privateAggregation.contributeToHistogram({bucket: 123n, value: 45});
          privateAggregation.contributeToHistogramOnEvent(
              "reserved.win", {bucket: 234n, value: 56});
        )",
        /*expected_report_url=*/std::nullopt,
        /*expected_ad_beacon_map=*/{}, /*expected_ad_macro_map=*/{},
        std::move(expected_pa_requests),
        /*expected_errors=*/{});
  }

  // Filtering IDs specified
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

    RunReportWinWithFunctionBodyExpectingResult(
        R"(
          privateAggregation.contributeToHistogram(
              {bucket: 123n, value: 45, filteringId: 0n});
          privateAggregation.contributeToHistogramOnEvent(
              "reserved.win", {bucket: 234n, value: 56, filteringId: 255n});
        )",
        /*expected_report_url=*/std::nullopt,
        /*expected_ad_beacon_map=*/{}, /*expected_ad_macro_map=*/{},
        std::move(expected_pa_requests),
        /*expected_errors=*/{});
  }
}

class BidderWorkletPrivateAggregationDisabledTest : public BidderWorkletTest {
 public:
  BidderWorkletPrivateAggregationDisabledTest() {
    feature_list_.InitAndDisableFeature(
        blink::features::kPrivateAggregationApi);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(BidderWorkletPrivateAggregationDisabledTest, GenerateBid) {
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          R"({ad: "ad", bid:1, render:"https://response.test/" })",
          /*extra_code=*/R"(
            privateAggregation.contributeToHistogram({bucket: 123n, value: 45});
          )"),
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/
      {"https://url.test/:6 Uncaught ReferenceError: privateAggregation is not "
       "defined."},
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_set_priority=*/std::nullopt,
      /*expected_pa_requests=*/{});
}

TEST_F(BidderWorkletPrivateAggregationDisabledTest, ReportWin) {
  RunReportWinWithFunctionBodyExpectingResult(
      R"(
          privateAggregation.contributeToHistogram({bucket: 123n, value: 45});
        )",
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{}, /*expected_ad_macro_map=*/{},
      /*expected_pa_requests=*/{},
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
      /*expected_bids=*/mojom::BidderWorkletBidPtr());

  // Sole bid is unauthorized. The non-enforced bid is there, kanon-bid isn't.
  // Since this is simulation mode, set_priority and errors should come from the
  // unrestricted run.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          R"({ad: ["ad"], bid:1, render:"https://response.test/"})",
          kSideEffectScript),
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, R"(["ad"])", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_set_priority=*/11);

  // Now authorize it.
  kanon_keys_.emplace(
      auction_worklet::mojom::KAnonKey::New(blink::HashedKAnonKeyForAdBid(
          url::Origin::Create(interest_group_bidding_url_),
          interest_group_bidding_url_, "https://response.test/")));
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          R"({ad: ["ad"], bid:1, render:"https://response.test/"})",
          kSideEffectScript),
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kBothKAnonModes, R"(["ad"])", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_set_priority=*/11);

  // Add a second ad, not authorized yet, with script that it will try it
  // if it's in the ad vector.
  interest_group_ads_.emplace_back(GURL("https://response2.test/"),
                                   /*metadata=*/std::nullopt);
  // Non-enforced bid will be 2. Since this is simulated mode, other things are
  // from the same run, so expected_set_priority is 12.
  {
    std::vector<mojom::BidderWorkletBidPtr> expected;
    expected.push_back(mojom::BidderWorkletBid::New(
        auction_worklet::mojom::BidRole::kUnenforcedKAnon, R"(["ad"])", 2,
        /*bid_currency=*/std::nullopt,
        /*ad_cost=*/std::nullopt,
        blink::AdDescriptor(GURL("https://response2.test/")),
        /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
        /*ad_component_descriptors=*/std::nullopt,
        /*modeling_signals=*/std::nullopt, base::TimeDelta()));
    expected.push_back(expected[0]->Clone());
    // k-anon-enforced bid will be 1.
    expected[1]->bid_role = auction_worklet::mojom::BidRole::kEnforcedKAnon;
    expected[1]->bid = 1;
    expected[1]->ad_descriptor =
        blink::AdDescriptor(GURL("https://response.test/"));

    RunGenerateBidWithJavascriptExpectingResult(
        CreateGenerateBidScript(
            R"({ad: ["ad"], bid:interestGroup.ads.length,
          render:interestGroup.ads[interestGroup.ads.length - 1].renderURL})",
            kSideEffectScript),
        std::move(expected),
        /*expected_data_version=*/std::nullopt,
        /*expected_errors=*/{},
        /*expected_debug_loss_report_url=*/std::nullopt,
        /*expected_debug_win_report_url=*/std::nullopt,
        /*expected_set_priority=*/12);
  }

  // Authorize it.
  kanon_keys_.emplace(
      auction_worklet::mojom::KAnonKey::New(blink::HashedKAnonKeyForAdBid(
          url::Origin::Create(interest_group_bidding_url_),
          interest_group_bidding_url_, "https://response2.test/")));
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          R"({ad: ["ad"], bid:interestGroup.ads.length,
          render:interestGroup.ads[interestGroup.ads.length - 1].renderURL})",
          kSideEffectScript),
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kBothKAnonModes, R"(["ad"])", 2,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response2.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_set_priority=*/12);
}

TEST_F(BidderWorkletTest, KAnonEnforce) {
  kanon_mode_ = auction_worklet::mojom::KAnonymityBidMode::kEnforce;

  const char kSideEffectScript[] = R"(
    setPriority(interestGroup.ads.length + 10);
  )";

  // Nothing returned regardless of filtering.
  RunGenerateBidWithReturnValueExpectingResult(
      "",
      /*expected_bids=*/mojom::BidderWorkletBidPtr());

  // Sole bid is unauthorized. The non-enforced bid is there, kanon-bid isn't.
  // Since this is enforcement mode there is no restricted run.
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          R"({ad: ["ad"], bid:1, render:"https://response.test/"})",
          kSideEffectScript),
      /*expected_bids=*/
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, R"(["ad"])", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_set_priority=*/std::nullopt);

  // Now authorize it.
  kanon_keys_.emplace(
      auction_worklet::mojom::KAnonKey::New(blink::HashedKAnonKeyForAdBid(
          url::Origin::Create(interest_group_bidding_url_),
          interest_group_bidding_url_, "https://response.test/")));
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          R"({ad: ["ad"], bid:1, render:"https://response.test/"})",
          kSideEffectScript),
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kBothKAnonModes, R"(["ad"])", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_set_priority=*/11);

  // Add a second ad, not authorized yet, with script that it will try it
  // if it's in the ad vector.
  interest_group_ads_.emplace_back(GURL("https://response2.test/"),
                                   /*metadata=*/std::nullopt);
  // Non-enforced bid will be 2,  k-anon-enforced bid will be 1. Since this is
  // enforced mode, other things are  from the restricted run, so
  // expected_set_priority is 11.
  std::vector<mojom::BidderWorkletBidPtr> expected;
  expected.push_back(mojom::BidderWorkletBid::New(
      auction_worklet::mojom::BidRole::kUnenforcedKAnon, R"(["ad"])", 2,
      /*bid_currency=*/std::nullopt,
      /*ad_cost=*/std::nullopt,
      blink::AdDescriptor(GURL("https://response2.test/")),
      /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
      /*ad_component_descriptors=*/std::nullopt,
      /*modeling_signals=*/std::nullopt, base::TimeDelta()));
  expected.push_back(mojom::BidderWorkletBid::New(
      auction_worklet::mojom::BidRole::kEnforcedKAnon, R"(["ad"])", 1,
      /*bid_currency=*/std::nullopt,
      /*ad_cost=*/std::nullopt,
      blink::AdDescriptor(GURL("https://response.test/")),
      /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
      /*ad_component_descriptors=*/std::nullopt,
      /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          R"({ad: ["ad"], bid:interestGroup.ads.length,
          render:interestGroup.ads[interestGroup.ads.length - 1].renderURL})",
          kSideEffectScript),
      std::move(expected),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_set_priority=*/11);

  // Authorize it.
  kanon_keys_.emplace(
      auction_worklet::mojom::KAnonKey::New(blink::HashedKAnonKeyForAdBid(
          url::Origin::Create(interest_group_bidding_url_),
          interest_group_bidding_url_, "https://response2.test/")));
  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          R"({ad: ["ad"], bid:interestGroup.ads.length,
          render:interestGroup.ads[interestGroup.ads.length - 1].renderURL})",
          kSideEffectScript),
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kBothKAnonModes, R"(["ad"])", 2,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response2.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/{},
      /*expected_debug_loss_report_url=*/std::nullopt,
      /*expected_debug_win_report_url=*/std::nullopt,
      /*expected_set_priority=*/12);
}

// Test of multi-bid and k-anon: the bids are annotated with their roles
// properly.
TEST_F(BidderWorkletTest, KAnonClassify) {
  kanon_mode_ = auction_worklet::mojom::KAnonymityBidMode::kEnforce;
  multi_bid_limit_ = 3;
  interest_group_ads_.emplace_back(GURL("https://response2.test/"),
                                   /*metadata=*/std::nullopt);
  interest_group_ads_.emplace_back(GURL("https://response3.test/"),
                                   /*metadata=*/std::nullopt);

  // Note: don't want an empty line in the beginning, or semi-colon insertion
  // will be trouble.
  const char kBids[] =
      R"([{bid: 1, render: "https://response.test/"},
          {bid: 2, render: "https://response2.test/"},
          {bid: 3, render: "https://response3.test/"}]
  )";

  auto bid1 = mojom::BidderWorkletBid::New(
      auction_worklet::mojom::BidRole::kUnenforcedKAnon, "null", 1,
      /*bid_currency=*/std::nullopt,
      /*ad_cost=*/std::nullopt,
      blink::AdDescriptor(GURL("https://response.test/")),
      /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
      /*ad_component_descriptors=*/std::nullopt,
      /*modeling_signals=*/std::nullopt, base::TimeDelta());

  // 3 bids none of which are k-anon. This triggers a re-run, which is skipped
  // since there are no k-anon ads.
  {
    std::vector<mojom::BidderWorkletBidPtr> expected;
    expected.push_back(bid1.Clone());
    expected.push_back(bid1.Clone());
    expected.push_back(bid1.Clone());
    expected[1]->bid_role = auction_worklet::mojom::BidRole::kUnenforcedKAnon;
    expected[1]->bid = 2;
    expected[1]->ad_descriptor =
        blink::AdDescriptor(GURL("https://response2.test/"));
    expected[2]->bid_role = auction_worklet::mojom::BidRole::kUnenforcedKAnon;
    expected[2]->bid = 3;
    expected[2]->ad_descriptor =
        blink::AdDescriptor(GURL("https://response3.test/"));

    RunGenerateBidWithJavascriptExpectingResult(
        CreateGenerateBidScript(kBids), std::move(expected),
        /*expected_data_version=*/std::nullopt,
        /*expected_errors=*/{});
  }

  // Authorize the second one. No re-run in this case since one ad is usable.
  kanon_keys_.emplace(
      auction_worklet::mojom::KAnonKey::New(blink::HashedKAnonKeyForAdBid(
          url::Origin::Create(interest_group_bidding_url_),
          interest_group_bidding_url_, "https://response2.test/")));
  {
    std::vector<mojom::BidderWorkletBidPtr> expected;
    expected.push_back(bid1.Clone());
    expected.push_back(bid1.Clone());
    expected.push_back(bid1.Clone());
    expected[1]->bid_role = auction_worklet::mojom::BidRole::kBothKAnonModes;
    expected[1]->bid = 2;
    expected[1]->ad_descriptor =
        blink::AdDescriptor(GURL("https://response2.test/"));
    expected[2]->bid_role = auction_worklet::mojom::BidRole::kUnenforcedKAnon;
    expected[2]->bid = 3;
    expected[2]->ad_descriptor =
        blink::AdDescriptor(GURL("https://response3.test/"));
    RunGenerateBidWithJavascriptExpectingResult(CreateGenerateBidScript(kBids),
                                                std::move(expected));
  }
}

// Test for doing a re-run in multi-bid mode: returning only non-k-anon
// bids forces a re-run.
TEST_F(BidderWorkletTest, KAnonRerunMultiBid) {
  kanon_mode_ = auction_worklet::mojom::KAnonymityBidMode::kEnforce;
  multi_bid_limit_ = 3;
  interest_group_ads_.emplace_back(GURL("https://response2.test/"),
                                   /*metadata=*/std::nullopt);
  interest_group_ads_.emplace_back(GURL("https://response3.test/"),
                                   /*metadata=*/std::nullopt);
  interest_group_ads_.emplace_back(GURL("https://response4.test/"),
                                   /*metadata=*/std::nullopt);

  // Authorize ad 4.
  kanon_keys_.emplace(
      auction_worklet::mojom::KAnonKey::New(blink::HashedKAnonKeyForAdBid(
          url::Origin::Create(interest_group_bidding_url_),
          interest_group_bidding_url_, "https://response4.test/")));

  const char kScript[] = R"(
    function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                         trustedBiddingSignals, browserSignals) {
      if (interestGroup.ads.length == 4) {
        // First run. Return first 3 ads.
        if (browserSignals.multiBidLimit !== 3)
            throw "Bad limit on first run";
        return [{bid: 10, render: "https://response.test/"},
                {bid: 9, render: "https://response2.test/"},
                {bid: 8, render: "https://response3.test/"}]
      } else {
        // k-anon re-run.
        if (browserSignals.multiBidLimit !== 1)
            throw "Bad limit on re-run";
        return [{bid: 7, render: "https://response4.test/"}];
      }
    }
  )";

  std::vector<mojom::BidderWorkletBidPtr> expected;
  expected.push_back(mojom::BidderWorkletBid::New(
      auction_worklet::mojom::BidRole::kUnenforcedKAnon, "null", 10,
      /*bid_currency=*/std::nullopt,
      /*ad_cost=*/std::nullopt,
      blink::AdDescriptor(GURL("https://response.test/")),
      /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
      /*ad_component_descriptors=*/std::nullopt,
      /*modeling_signals=*/std::nullopt, base::TimeDelta()));
  expected.push_back(mojom::BidderWorkletBid::New(
      auction_worklet::mojom::BidRole::kUnenforcedKAnon, "null", 9,
      /*bid_currency=*/std::nullopt,
      /*ad_cost=*/std::nullopt,
      blink::AdDescriptor(GURL("https://response2.test/")),
      /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
      /*ad_component_descriptors=*/std::nullopt,
      /*modeling_signals=*/std::nullopt, base::TimeDelta()));
  expected.push_back(mojom::BidderWorkletBid::New(
      auction_worklet::mojom::BidRole::kUnenforcedKAnon, "null", 8,
      /*bid_currency=*/std::nullopt,
      /*ad_cost=*/std::nullopt,
      blink::AdDescriptor(GURL("https://response3.test/")),
      /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
      /*ad_component_descriptors=*/std::nullopt,
      /*modeling_signals=*/std::nullopt, base::TimeDelta()));
  expected.push_back(mojom::BidderWorkletBid::New(
      auction_worklet::mojom::BidRole::kEnforcedKAnon, "null", 7,
      /*bid_currency=*/std::nullopt,
      /*ad_cost=*/std::nullopt,
      blink::AdDescriptor(GURL("https://response4.test/")),
      /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
      /*ad_component_descriptors=*/std::nullopt,
      /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  RunGenerateBidWithJavascriptExpectingResult(kScript, std::move(expected));
}

// Test for context re-use for k-anon rerun.
TEST_F(BidderWorkletTest, KAnonRerun) {
  const char kScript[] = R"(
    if (!('count' in globalThis))
      globalThis.count = 0;
    function generateBid(interestGroup) {
      ++count;
      return {ad: ["ad"], bid:count,
      render:interestGroup.ads[interestGroup.ads.length - 1].renderURL};
    }
  )";

  kanon_mode_ = auction_worklet::mojom::KAnonymityBidMode::kEnforce;

  interest_group_ads_.emplace_back(GURL("https://response2.test/"),
                                   /*metadata=*/std::nullopt);
  kanon_keys_.emplace(
      auction_worklet::mojom::KAnonKey::New(blink::HashedKAnonKeyForAdBid(
          url::Origin::Create(interest_group_bidding_url_),
          interest_group_bidding_url_, "https://response.test/")));

  for (auto execution_mode :
       {blink::mojom::InterestGroup::ExecutionMode::kCompatibilityMode,
        blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode}) {
    execution_mode_ = execution_mode;
    SCOPED_TRACE(execution_mode_);
    std::vector<mojom::BidderWorkletBidPtr> expected;
    expected.push_back(mojom::BidderWorkletBid::New(
        auction_worklet::mojom::BidRole::kUnenforcedKAnon, R"(["ad"])", 1,
        /*bid_currency=*/std::nullopt,
        /*ad_cost=*/std::nullopt,
        blink::AdDescriptor(GURL("https://response2.test/")),
        /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
        /*ad_component_descriptors=*/std::nullopt,
        /*modeling_signals=*/std::nullopt, base::TimeDelta()));
    expected.push_back(mojom::BidderWorkletBid::New(
        auction_worklet::mojom::BidRole::kEnforcedKAnon, R"(["ad"])", 2,
        /*bid_currency=*/std::nullopt,
        /*ad_cost=*/std::nullopt,
        blink::AdDescriptor(GURL("https://response.test/")),
        /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
        /*ad_component_descriptors=*/std::nullopt,
        /*modeling_signals=*/std::nullopt, base::TimeDelta()));
    RunGenerateBidWithJavascriptExpectingResult(kScript, std::move(expected));
  }
}

TEST_F(BidderWorkletTest,
       BidderWorkletOnlyPassesKAnonSelectableReportingIdsOnRestrictedRun) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      blink::features::kFledgeAuctionDealSupport);

  // The first time, this selects "non-k-anon-selected-id" because it's the
  // first `selectableBuyerAndSellerReportingId`; the second time, it should
  // select "k-anon-selected-id" because "non-k-anon-selected-id" will have been
  // removed from that field.
  const char kScript[] = R"(
    function generateBid(interestGroup) {
      return {
          ad: ["ad"],
          bid: interestGroup.ads[0].selectableBuyerAndSellerReportingIds.length,
          render: interestGroup.ads[0].renderURL,
          selectedBuyerAndSellerReportingId:
              interestGroup.ads[0].selectableBuyerAndSellerReportingIds[0],
      };
    }
  )";

  kanon_mode_ = auction_worklet::mojom::KAnonymityBidMode::kEnforce;

  interest_group_ads_.clear();
  interest_group_ads_.emplace_back(
      GURL("https://response.test/"),
      /*metadata=*/std::nullopt, /*size_group=*/std::nullopt,
      /*buyer_reporting_id=*/"buyer1",
      /*buyer_and_seller_reporting_id=*/"common1",
      /*selectable_buyer_and_seller_reporting_ids=*/
      std::vector<std::string>{"non-k-anon-selected-id", "k-anon-selected-id"});

  kanon_keys_.emplace(
      auction_worklet::mojom::KAnonKey::New(blink::HashedKAnonKeyForAdBid(
          url::Origin::Create(interest_group_bidding_url_),
          interest_group_bidding_url_, "https://response.test/")));
  kanon_keys_.emplace(auction_worklet::mojom::KAnonKey::New(
      blink::HashedKAnonKeyForAdNameReportingWithoutInterestGroup(
          url::Origin::Create(interest_group_bidding_url_),
          interest_group_name_, interest_group_bidding_url_,
          "https://response.test/", std::string("buyer1"),
          std::string("common1"), std::string("k-anon-selected-id"))));

  std::vector<mojom::BidderWorkletBidPtr> expected;
  expected.push_back(mojom::BidderWorkletBid::New(
      auction_worklet::mojom::BidRole::kUnenforcedKAnon, R"(["ad"])", 2,
      /*bid_currency=*/std::nullopt,
      /*ad_cost=*/std::nullopt,
      blink::AdDescriptor(GURL("https://response.test/")),
      /*selected_buyer_and_seller_reporting_id=*/"non-k-anon-selected-id",
      /*ad_component_descriptors=*/std::nullopt,
      /*modeling_signals=*/std::nullopt, base::TimeDelta()));
  expected.push_back(mojom::BidderWorkletBid::New(
      auction_worklet::mojom::BidRole::kEnforcedKAnon, R"(["ad"])", 1,
      /*bid_currency=*/std::nullopt,
      /*ad_cost=*/std::nullopt,
      blink::AdDescriptor(GURL("https://response.test/")),
      /*selected_buyer_and_seller_reporting_id=*/"k-anon-selected-id",
      /*ad_component_descriptors=*/std::nullopt,
      /*modeling_signals=*/std::nullopt, base::TimeDelta()));
  RunGenerateBidWithJavascriptExpectingResult(kScript, std::move(expected));
}

TEST_F(BidderWorkletTest,
       BidderWorkletRejectsNonKAnonSelectableReportingIdOnRestrictedRun) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      blink::features::kFledgeAuctionDealSupport);

  // Here, we select the same "non-k-anon-selected-id" on both calls to
  // `generateBid()`, which shouldn't happen because it's not not provided as
  // one of the `selectableBuyerAndSellerReportingIds` passed in on the call to
  // `generateBid()`. This makes the bid not k-anon for reporting, and because
  // this bid has returned a `selectedBuyerAndSellerReportingId`, that makes
  // this bid not k-anon, which makes it an invalid output for the k-anon
  // restricted call. The BidderWorklet recognizes that and rejects this bid as
  // invalid, emitting an error.
  const char kScript[] = R"(
    function generateBid(interestGroup) {
      return {
          ad: ["ad"],
          bid: interestGroup.ads[0].selectableBuyerAndSellerReportingIds.length,
          render: interestGroup.ads[0].renderURL,
          selectedBuyerAndSellerReportingId: "non-k-anon-selected-id",
      };
    }
  )";

  kanon_mode_ = auction_worklet::mojom::KAnonymityBidMode::kEnforce;

  interest_group_ads_.clear();
  interest_group_ads_.emplace_back(
      GURL("https://response.test/"),
      /*metadata=*/std::nullopt, /*size_group=*/std::nullopt,
      /*buyer_reporting_id=*/"buyer1",
      /*buyer_and_seller_reporting_id=*/"common1",
      /*selectable_buyer_and_seller_reporting_ids=*/
      std::vector<std::string>{"non-k-anon-selected-id", "k-anon-selected-id"});

  kanon_keys_.emplace(
      auction_worklet::mojom::KAnonKey::New(blink::HashedKAnonKeyForAdBid(
          url::Origin::Create(interest_group_bidding_url_),
          interest_group_bidding_url_, "https://response.test/")));
  kanon_keys_.emplace(auction_worklet::mojom::KAnonKey::New(
      blink::HashedKAnonKeyForAdNameReportingWithoutInterestGroup(
          url::Origin::Create(interest_group_bidding_url_),
          interest_group_name_, interest_group_bidding_url_,
          "https://response.test/", std::string("buyer1"),
          std::string("common1"), std::string("k-anon-selected-id"))));

  std::vector<mojom::BidderWorkletBidPtr> expected;
  expected.push_back(mojom::BidderWorkletBid::New(
      auction_worklet::mojom::BidRole::kUnenforcedKAnon, R"(["ad"])", 2,
      /*bid_currency=*/std::nullopt,
      /*ad_cost=*/std::nullopt,
      blink::AdDescriptor(GURL("https://response.test/")),
      /*selected_buyer_and_seller_reporting_id=*/"non-k-anon-selected-id",
      /*ad_component_descriptors=*/std::nullopt,
      /*modeling_signals=*/std::nullopt, base::TimeDelta()));
  RunGenerateBidWithJavascriptExpectingResult(
      kScript, std::move(expected),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/
      {"https://url.test/ generateBid() Invalid selected buyer and seller "
       "reporting id"});
}

TEST_F(BidderWorkletTest, IsKAnonURL) {
  const GURL kUrl1("https://example.test/1");
  const GURL kUrl2("https://example2.test/2");
  const GURL kUrl3("https://example3.test/3");
  const GURL kUrl4("https://example3.test/4");
  mojom::BidderWorkletNonSharedParamsPtr params =
      mojom::BidderWorkletNonSharedParams::New();
  url::Origin owner = url::Origin::Create(interest_group_bidding_url_);
  const std::string kUrl1KAnonKey = blink::HashedKAnonKeyForAdBid(
      owner, interest_group_bidding_url_, kUrl1.spec());
  const std::string kUrl2KAnonKey = blink::HashedKAnonKeyForAdBid(
      owner, interest_group_bidding_url_, kUrl2.spec());
  const std::string kUrl3KAnonKey = blink::HashedKAnonKeyForAdBid(
      owner, interest_group_bidding_url_, kUrl3.spec());
  const std::string kUrl4KAnonKey = blink::HashedKAnonKeyForAdBid(
      owner, interest_group_bidding_url_, kUrl4.spec());

  params->kanon_keys.emplace_back(
      auction_worklet::mojom::KAnonKey::New(kUrl1KAnonKey));
  params->kanon_keys.emplace_back(
      auction_worklet::mojom::KAnonKey::New(kUrl2KAnonKey));

  EXPECT_TRUE(BidderWorklet::IsKAnon(params.get(), kUrl1KAnonKey));
  EXPECT_TRUE(BidderWorklet::IsKAnon(params.get(), kUrl2KAnonKey));
  EXPECT_FALSE(BidderWorklet::IsKAnon(params.get(), kUrl3KAnonKey));
  EXPECT_FALSE(BidderWorklet::IsKAnon(params.get(), kUrl4KAnonKey));
}

TEST_F(BidderWorkletTest, IsKAnonResult) {
  const GURL kUrl1("https://example.test/1");
  const GURL kUrl2("https://example2.test/2");
  const GURL kUrl3("https://example3.test/3");
  const GURL kUrl4("https://example3.test/4");
  mojom::BidderWorkletNonSharedParamsPtr params =
      mojom::BidderWorkletNonSharedParams::New();
  url::Origin owner = url::Origin::Create(interest_group_bidding_url_);
  params->kanon_keys.emplace_back(
      auction_worklet::mojom::KAnonKey::New(blink::HashedKAnonKeyForAdBid(
          owner, interest_group_bidding_url_, kUrl1.spec())));
  params->kanon_keys.emplace_back(auction_worklet::mojom::KAnonKey::New(
      blink::HashedKAnonKeyForAdComponentBid(kUrl2)));

  SetBidBindings::BidAndWorkletOnlyMetadata bid_with_metadata;
  bid_with_metadata.bid = mojom::BidderWorkletBid::New();
  const mojom::BidderWorkletBidPtr& bid = bid_with_metadata.bid;

  // k-anon ad URL.
  bid->ad_descriptor.url = kUrl1;
  EXPECT_TRUE(BidderWorklet::IsMainAdKAnon(
      params.get(), interest_group_bidding_url_, bid_with_metadata));

  // k-anon ad URL and component.
  bid->ad_component_descriptors.emplace();
  bid->ad_component_descriptors->emplace_back(kUrl2);
  EXPECT_TRUE(BidderWorklet::IsMainAdKAnon(
      params.get(), interest_group_bidding_url_, bid_with_metadata));
  EXPECT_TRUE(BidderWorklet::IsComponentAdKAnon(
      params.get(), bid->ad_component_descriptors.value()[0]));

  // Non k-anon ad URL, k-anon component.
  bid->ad_descriptor.url = kUrl4;
  EXPECT_FALSE(BidderWorklet::IsMainAdKAnon(
      params.get(), interest_group_bidding_url_, bid_with_metadata));
  EXPECT_TRUE(BidderWorklet::IsComponentAdKAnon(
      params.get(), bid->ad_component_descriptors.value()[0]));

  // k-anon ad URL, one of the components non-k-anon.
  bid->ad_descriptor.url = kUrl1;
  bid->ad_component_descriptors->emplace_back(kUrl3);
  EXPECT_TRUE(BidderWorklet::IsMainAdKAnon(
      params.get(), interest_group_bidding_url_, bid_with_metadata));
  EXPECT_TRUE(BidderWorklet::IsComponentAdKAnon(
      params.get(), bid->ad_component_descriptors.value()[0]));
  EXPECT_FALSE(BidderWorklet::IsComponentAdKAnon(
      params.get(), bid->ad_component_descriptors.value()[1]));
}

TEST_F(BidderWorkletTest, IsMainAdKAnonResultWithSelectedReportingId) {
  const GURL kUrl1("https://example.test/1");

  // Every combination of these reporting IDs is k-anon.
  constexpr char kBuyerReportingId1[] = "buyer1";
  constexpr char kBuyerAndSellerReportingId1[] = "buyerAndSeller1";
  constexpr char kSelectedBuyerAndSellerReportingId1[] = "selected1";

  // Only "selected2" without any other reporting IDs is k-anon.
  constexpr char kSelectedBuyerAndSellerReportingId2[] = "selected2";

  // "selected3" is never k-anon.
  constexpr char kSelectedBuyerAndSellerReportingId3[] = "selected3";

  mojom::BidderWorkletNonSharedParamsPtr params =
      mojom::BidderWorkletNonSharedParams::New();
  url::Origin owner = url::Origin::Create(interest_group_bidding_url_);

  params->kanon_keys.emplace_back(
      auction_worklet::mojom::KAnonKey::New(blink::HashedKAnonKeyForAdBid(
          owner, interest_group_bidding_url_, kUrl1.spec())));
  AuthorizeKAnonForReporting(kUrl1, kBuyerReportingId1,
                             kBuyerAndSellerReportingId1,
                             kSelectedBuyerAndSellerReportingId1, params);
  AuthorizeKAnonForReporting(kUrl1, std::nullopt, kBuyerAndSellerReportingId1,
                             kSelectedBuyerAndSellerReportingId1, params);
  AuthorizeKAnonForReporting(kUrl1, kBuyerReportingId1, std::nullopt,
                             kSelectedBuyerAndSellerReportingId1, params);
  AuthorizeKAnonForReporting(kUrl1, std::nullopt, std::nullopt,
                             kSelectedBuyerAndSellerReportingId1, params);
  AuthorizeKAnonForReporting(kUrl1, std::nullopt, std::nullopt,
                             kSelectedBuyerAndSellerReportingId2, params);

  SetBidBindings::BidAndWorkletOnlyMetadata bid_with_metadata;
  bid_with_metadata.bid = mojom::BidderWorkletBid::New();
  const mojom::BidderWorkletBidPtr& bid = bid_with_metadata.bid;
  bid->ad_descriptor.url = kUrl1;

  bid->selected_buyer_and_seller_reporting_id =
      kSelectedBuyerAndSellerReportingId1;

  bid_with_metadata.buyer_reporting_id = kBuyerReportingId1;
  bid_with_metadata.buyer_and_seller_reporting_id = kBuyerAndSellerReportingId1;
  EXPECT_TRUE(BidderWorklet::IsMainAdKAnon(
      params.get(), interest_group_bidding_url_, bid_with_metadata));

  bid_with_metadata.buyer_reporting_id = std::nullopt;
  bid_with_metadata.buyer_and_seller_reporting_id = kBuyerAndSellerReportingId1;
  EXPECT_TRUE(BidderWorklet::IsMainAdKAnon(
      params.get(), interest_group_bidding_url_, bid_with_metadata));

  bid_with_metadata.buyer_reporting_id = kBuyerReportingId1;
  bid_with_metadata.buyer_and_seller_reporting_id = std::nullopt;
  EXPECT_TRUE(BidderWorklet::IsMainAdKAnon(
      params.get(), interest_group_bidding_url_, bid_with_metadata));

  bid_with_metadata.buyer_reporting_id = std::nullopt;
  bid_with_metadata.buyer_and_seller_reporting_id = std::nullopt;
  EXPECT_TRUE(BidderWorklet::IsMainAdKAnon(
      params.get(), interest_group_bidding_url_, bid_with_metadata));

  bid->selected_buyer_and_seller_reporting_id =
      kSelectedBuyerAndSellerReportingId2;

  bid_with_metadata.buyer_reporting_id = kBuyerReportingId1;
  bid_with_metadata.buyer_and_seller_reporting_id = kBuyerAndSellerReportingId1;
  EXPECT_FALSE(BidderWorklet::IsMainAdKAnon(
      params.get(), interest_group_bidding_url_, bid_with_metadata));

  bid_with_metadata.buyer_reporting_id = std::nullopt;
  bid_with_metadata.buyer_and_seller_reporting_id = kBuyerAndSellerReportingId1;
  EXPECT_FALSE(BidderWorklet::IsMainAdKAnon(
      params.get(), interest_group_bidding_url_, bid_with_metadata));

  bid_with_metadata.buyer_reporting_id = kBuyerReportingId1;
  bid_with_metadata.buyer_and_seller_reporting_id = std::nullopt;
  EXPECT_FALSE(BidderWorklet::IsMainAdKAnon(
      params.get(), interest_group_bidding_url_, bid_with_metadata));

  bid_with_metadata.buyer_reporting_id = std::nullopt;
  bid_with_metadata.buyer_and_seller_reporting_id = std::nullopt;
  EXPECT_TRUE(BidderWorklet::IsMainAdKAnon(
      params.get(), interest_group_bidding_url_, bid_with_metadata));

  bid->selected_buyer_and_seller_reporting_id =
      kSelectedBuyerAndSellerReportingId3;
  EXPECT_FALSE(BidderWorklet::IsMainAdKAnon(
      params.get(), interest_group_bidding_url_, bid_with_metadata));
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
  EXPECT_TRUE(bids_.empty());

  // Add trusted signals, too.
  AddBidderJsonResponse(&url_loader_factory_,
                        GURL("https://url.test/"
                             "trustedsignals?hostname=top.window.test&keys=1&"
                             "interestGroupNames=Fred"),
                        R"({"keys": {"1":123}})");
  // Not enough yet.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(bids_.empty());

  // Now feed in the rest of the arguments.
  bid_finalizer->FinishGenerateBid(
      auction_signals_, per_buyer_signals_, per_buyer_timeout_,
      per_buyer_currency_,
      /*direct_from_seller_per_buyer_signals=*/std::nullopt,
      /*direct_from_seller_per_buyer_signals_header_ad_slot=*/std::nullopt,
      /*direct_from_seller_auction_signals=*/std::nullopt,
      /*direct_from_seller_auction_signals_header_ad_slot=*/std::nullopt);
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  generate_bid_run_loop_->Run();
  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(R"([["auction_signals"],{"1":123},["per_buyer_signals"]])",
            bids_[0]->ad);
  EXPECT_EQ(1, bids_[0]->bid);
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
  EXPECT_TRUE(bids_.empty());

  // Feed in the rest of the arguments.
  bid_finalizer->FinishGenerateBid(
      auction_signals_, per_buyer_signals_, per_buyer_timeout_,
      per_buyer_currency_,
      /*direct_from_seller_per_buyer_signals=*/std::nullopt,
      /*direct_from_seller_per_buyer_signals_header_ad_slot=*/std::nullopt,
      /*direct_from_seller_auction_signals=*/std::nullopt,
      /*direct_from_seller_auction_signals_header_ad_slot=*/std::nullopt);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(bids_.empty());

  // Add trusted signals, too.
  AddBidderJsonResponse(&url_loader_factory_,
                        GURL("https://url.test/"
                             "trustedsignals?hostname=top.window.test&keys=1&"
                             "interestGroupNames=Fred"),
                        R"({"keys": {"1":123}})");
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  generate_bid_run_loop_->Run();
  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(R"([["auction_signals"],{"1":123},["per_buyer_signals"]])",
            bids_[0]->ad);
  EXPECT_EQ(1, bids_[0]->bid);
  EXPECT_THAT(bid_errors_, testing::ElementsAre());
}

class BidderWorkletLatenciesTest : public BidderWorkletTest {
 public:
  // We use MockTime to control the reported latencies.
  BidderWorkletLatenciesTest()
      : BidderWorkletTest(TaskEnvironment::TimeSource::MOCK_TIME) {}
};

TEST_F(BidderWorkletLatenciesTest, GenerateBidLatenciesAreReturned) {
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
  EXPECT_TRUE(bids_.empty());

  task_environment_.FastForwardBy(base::Milliseconds(250));

  // Add trusted signals, too.
  AddBidderJsonResponse(&url_loader_factory_,
                        GURL("https://url.test/"
                             "trustedsignals?hostname=top.window.test&keys=1&"
                             "interestGroupNames=Fred"),
                        R"({"keys": {"1":123}})");
  // Not enough yet.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(bids_.empty());

  task_environment_.FastForwardBy(base::Milliseconds(250));

  // Now feed in the rest of the arguments.
  bid_finalizer->FinishGenerateBid(
      auction_signals_, per_buyer_signals_, per_buyer_timeout_,
      per_buyer_currency_,
      /*direct_from_seller_per_buyer_signals=*/std::nullopt,
      /*direct_from_seller_per_buyer_signals_header_ad_slot=*/std::nullopt,
      /*direct_from_seller_auction_signals=*/std::nullopt,
      /*direct_from_seller_auction_signals_header_ad_slot=*/std::nullopt);
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  generate_bid_run_loop_->Run();
  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ(R"([["auction_signals"],{"1":123},["per_buyer_signals"]])",
            bids_[0]->ad);
  EXPECT_EQ(1, bids_[0]->bid);
  EXPECT_THAT(bid_errors_, testing::ElementsAre());

  ASSERT_TRUE(generate_bid_dependency_latencies_);
  // Any zero latency is replaced with nullopt.
  EXPECT_FALSE(generate_bid_dependency_latencies_->code_ready_latency);
  EXPECT_EQ(base::Milliseconds(500),
            *generate_bid_dependency_latencies_->config_promises_latency);
  EXPECT_FALSE(
      generate_bid_dependency_latencies_->direct_from_seller_signals_latency);
  EXPECT_EQ(
      base::Milliseconds(250),
      *generate_bid_dependency_latencies_->trusted_bidding_signals_latency);
}

TEST_F(BidderWorkletLatenciesTest, GenerateBidFetchMetrics) {
  interest_group_wasm_url_ = GURL(kWasmUrl);
  mojo::Remote<mojom::BidderWorklet> bidder_worklet = CreateWorklet();
  mojo::AssociatedRemote<auction_worklet::mojom::GenerateBidFinalizer>
      bid_finalizer;
  BeginGenerateBid(bidder_worklet.get(),
                   bid_finalizer.BindNewEndpointAndPassReceiver());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(bids_.empty());

  task_environment_.FastForwardBy(base::Milliseconds(250));
  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        CreateBasicGenerateBidScript());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(bids_.empty());

  task_environment_.FastForwardBy(base::Milliseconds(240));
  AddResponse(&url_loader_factory_, GURL(kWasmUrl), kWasmMimeType,
              /*charset=*/std::nullopt, ToyWasm());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(bids_.empty());

  task_environment_.FastForwardBy(base::Milliseconds(230));

  // Now feed in the rest of the arguments.
  bid_finalizer->FinishGenerateBid(
      auction_signals_, per_buyer_signals_, per_buyer_timeout_,
      per_buyer_currency_,
      /*direct_from_seller_per_buyer_signals=*/std::nullopt,
      /*direct_from_seller_per_buyer_signals_header_ad_slot=*/std::nullopt,
      /*direct_from_seller_auction_signals=*/std::nullopt,
      /*direct_from_seller_auction_signals_header_ad_slot=*/std::nullopt);
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  generate_bid_run_loop_->Run();
  ASSERT_EQ(1u, bids_.size());
  EXPECT_EQ("[\"ad\"]", bids_[0]->ad);
  EXPECT_EQ(1, bids_[0]->bid);
  EXPECT_THAT(bid_errors_, testing::ElementsAre());

  ASSERT_TRUE(generate_bid_metrics_->js_fetch_latency.has_value());
  EXPECT_EQ(base::Milliseconds(250), *generate_bid_metrics_->js_fetch_latency);
  ASSERT_TRUE(generate_bid_metrics_->wasm_fetch_latency.has_value());
  EXPECT_EQ(base::Milliseconds(490),
            *generate_bid_metrics_->wasm_fetch_latency);

  generate_bid_metrics_.reset();

  // Do another call, metrics should be the same.
  mojo::AssociatedRemote<auction_worklet::mojom::GenerateBidFinalizer>
      bid_finalizer2;
  BeginGenerateBid(bidder_worklet.get(),
                   bid_finalizer2.BindNewEndpointAndPassReceiver());
  bid_finalizer2->FinishGenerateBid(
      auction_signals_, per_buyer_signals_, per_buyer_timeout_,
      per_buyer_currency_,
      /*direct_from_seller_per_buyer_signals=*/std::nullopt,
      /*direct_from_seller_per_buyer_signals_header_ad_slot=*/std::nullopt,
      /*direct_from_seller_auction_signals=*/std::nullopt,
      /*direct_from_seller_auction_signals_header_ad_slot=*/std::nullopt);
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  generate_bid_run_loop_->Run();

  ASSERT_TRUE(generate_bid_metrics_->js_fetch_latency.has_value());
  EXPECT_EQ(base::Milliseconds(250), *generate_bid_metrics_->js_fetch_latency);
  ASSERT_TRUE(generate_bid_metrics_->wasm_fetch_latency.has_value());
  EXPECT_EQ(base::Milliseconds(490),
            *generate_bid_metrics_->wasm_fetch_latency);
}

TEST_F(BidderWorkletLatenciesTest, ReportWinFetchMetrics) {
  interest_group_wasm_url_ = GURL(kWasmUrl);
  mojo::Remote<mojom::BidderWorklet> bidder_worklet = CreateWorklet();

  base::RunLoop run_loop;
  bidder_worklet->ReportWin(
      is_for_additional_bid_, interest_group_name_reporting_id_,
      buyer_reporting_id_, buyer_and_seller_reporting_id_,
      selected_buyer_and_seller_reporting_id_, auction_signals_,
      per_buyer_signals_, direct_from_seller_per_buyer_signals_,
      direct_from_seller_per_buyer_signals_header_ad_slot_,
      direct_from_seller_auction_signals_,
      direct_from_seller_auction_signals_header_ad_slot_, seller_signals_,
      kanon_mode_, bid_is_kanon_, browser_signal_render_url_,
      browser_signal_bid_, browser_signal_bid_currency_,
      browser_signal_highest_scoring_other_bid_,
      browser_signal_highest_scoring_other_bid_currency_,
      browser_signal_made_highest_scoring_other_bid_, browser_signal_ad_cost_,
      browser_signal_modeling_signals_, browser_signal_join_count_,
      browser_signal_recency_report_win_, browser_signal_seller_origin_,
      browser_signal_top_level_seller_origin_,
      browser_signal_reporting_timeout_, data_version_,

      /*trace_id=*/1,
      base::BindOnce(
          [](base::OnceClosure done_closure,
             const std::optional<GURL>& report_url,
             const base::flat_map<std::string, GURL>& ad_beacon_map,
             const base::flat_map<std::string, std::string>& ad_macro_map,
             PrivateAggregationRequests pa_requests,
             auction_worklet::mojom::BidderTimingMetricsPtr timing_metrics,
             const std::vector<std::string>& errors) {
            ASSERT_TRUE(timing_metrics->js_fetch_latency.has_value());
            EXPECT_EQ(base::Milliseconds(250),
                      *timing_metrics->js_fetch_latency);
            ASSERT_TRUE(timing_metrics->wasm_fetch_latency.has_value());
            EXPECT_EQ(base::Milliseconds(490),
                      *timing_metrics->wasm_fetch_latency);
            std::move(done_closure).Run();
          },
          run_loop.QuitClosure()));

  task_environment_.FastForwardBy(base::Milliseconds(250));
  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        CreateBasicGenerateBidScript());
  task_environment_.RunUntilIdle();

  task_environment_.FastForwardBy(base::Milliseconds(240));
  AddResponse(&url_loader_factory_, GURL(kWasmUrl), kWasmMimeType,
              /*charset=*/std::nullopt, ToyWasm());
  task_environment_.RunUntilIdle();
  task_environment_.FastForwardBy(base::Milliseconds(230));
  run_loop.Run();
}

// Tests both reporting latency, and default reporting timeout.
TEST_F(BidderWorkletTest, ReportWinLatency) {
  // We use an infinite loop since we have some notion of how long a timeout
  // should take.
  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        CreateReportWinScript("while (true) {}"));

  RunReportWinExpectingResult(
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_ad_macro_map=*/{},
      /*expected_pa_requests=*/{},
      /*expected_reporting_latency_timeout=*/true,
      /*expected_errors=*/
      {"https://url.test/ execution of `reportWin` timed out."});
}

TEST_F(BidderWorkletTest, ReportWinZeroTimeout) {
  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        CreateReportWinScript("throw 'something'"));

  browser_signal_reporting_timeout_ = base::TimeDelta();
  RunReportWinExpectingResult(
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_ad_macro_map=*/{},
      /*expected_pa_requests=*/{},
      /*expected_reporting_latency_timeout=*/true,
      /*expected_errors=*/
      {"reportWin() aborted due to zero timeout."});
}

TEST_F(BidderWorkletTest, ReportWinTimeoutFromAuctionConfig) {
  // Use a very long default script timeout, and a short reporting timeout, so
  // that if the reportWin() script with endless loop times out, we know that
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

  browser_signal_reporting_timeout_ = base::Milliseconds(50);
  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        CreateReportWinScript("while (true) {}"));
  RunReportWinExpectingResult(
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      /*expected_ad_macro_map=*/{},
      /*expected_pa_requests=*/{},
      /*expected_reporting_latency_timeout=*/true,
      /*expected_errors=*/
      {"https://url.test/ execution of `reportWin` timed out."});
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

TEST_F(BidderWorkletTest, GenerateBidRenderUrlWithSize) {
  // The 'render' field corresponds to an object that contains the url string.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid: 1, render: {url: "https://response.test/"}})",
      /*expected_bids=*/mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  // The 'render' field corresponds to an object that contains the url string
  // and size with units.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({
          ad: "ad",
          bid: 1,
          render: {
            url: "https://response.test/",
            width: "100sw",
            height: "50px"
          }
        })",
      /*expected_bids=*/mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(
              GURL("https://response.test/"),
              blink::AdSize(100, blink::AdSize::LengthUnit::kScreenWidth, 50,
                            blink::AdSize::LengthUnit::kPixels)),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  // The 'render' field corresponds to an object that contains the url string
  // and size without units.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({
          ad: "ad",
          bid: 1,
          render: {
            url: "https://response.test/",
            width: "100",
            height: "50"
          }
        })",
      /*expected_bids=*/mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(
              GURL("https://response.test/"),
              blink::AdSize(100, blink::AdSize::LengthUnit::kPixels, 50,
                            blink::AdSize::LengthUnit::kPixels)),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  // The 'render' field corresponds to an object with an invalid field.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid: 1, render: {foo: "https://response.test/"}})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt, /*expected_errors=*/
      {"https://url.test/ generateBid() 'render': Required field 'url' "
       "is undefined."});

  // The 'render' field corresponds to an object with an invalid field.
  // Extra fields are just ignored.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({
          ad: "ad",
          bid: 1,
          render: {
            url: "https://response.test/",
            width: "100sw",
            height: "50px",
            foo: 42
          }
        })",
      /*expected_bids=*/mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(
              GURL("https://response.test/"),
              blink::AdSize(100, blink::AdSize::LengthUnit::kScreenWidth, 50,
                            blink::AdSize::LengthUnit::kPixels)),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  // The 'render' field corresponds to an empty object.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid: 1, render: {}})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt, /*expected_errors=*/
      {"https://url.test/ generateBid() 'render': Required field 'url' "
       "is undefined."});

  // The 'render' field corresponds to an object without url string field.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({ad: "ad", bid: 1, render: {width: "100sw", height: "50px"}})",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt, /*expected_errors=*/
      {"https://url.test/ generateBid() 'render': Required field 'url' "
       "is undefined."});

  // Size is not of string type, but numbers just get stringified, so count
  // as pixels.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({
          ad: "ad",
          bid: 1,
          render: {
            url: "https://response.test/",
            width: 100,
            height: 50
          }
        })",
      /*expected_bids=*/mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "\"ad\"", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(
              GURL("https://response.test/"),
              blink::AdSize(100, blink::AdSize::LengthUnit::kPixels, 50,
                            blink::AdSize::LengthUnit::kPixels)),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  // Size is not convertible to string.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({
          ad: "ad",
          bid: 1,
          render: {
            url: "https://response.test/",
            width: {toString:() => { return {}; } },
            height: 50
          }
        })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt, /*expected_errors=*/
      {"undefined:0 Uncaught TypeError: Cannot convert object to primitive "
       "value."});

  // Only width is specified.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({
          ad: "ad",
          bid: 1,
          render: {
            url: "https://response.test/",
            width: "100sw"
          }
        })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt, /*expected_errors=*/
      {"https://url.test/ generateBid() 'render': ads that specify dimensions "
       "must specify both."});

  // Only height is specified.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({
          ad: "ad",
          bid: 1,
          render: {
            url: "https://response.test/",
            height: "100sw"
          }
        })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt, /*expected_errors=*/
      {"https://url.test/ generateBid() 'render': ads that specify dimensions "
       "must specify both."});

  // Size has invalid units.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({
          ad: "ad",
          bid: 1,
          render: {
            url: "https://response.test/",
            width: "100in",
            height: "100px"
          }
        })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt, /*expected_errors=*/
      {"https://url.test/ generateBid() bid has invalid size for render ad."});

  // Size doesn't have numbers.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({
          ad: "ad",
          bid: 1,
          render: {
            url: "https://response.test/",
            width: "sw",
            height: "px"
          }
        })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt, /*expected_errors=*/
      {"https://url.test/ generateBid() bid has invalid size for render ad."});

  // Size is empty.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({
          ad: "ad",
          bid: 1,
          render: {
            url: "https://response.test/",
            width: "",
            height: ""
          }
        })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt, /*expected_errors=*/
      {"https://url.test/ generateBid() bid has invalid size for render ad."});

  // Size has zero value.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({
          ad: "ad",
          bid: 1,
          render: {
            url: "https://response.test/",
            width: "0px",
            height: "10px"
          }
        })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt, /*expected_errors=*/
      {"https://url.test/ generateBid() bid has invalid size for render ad."});

  // Size has negative value.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({
          ad: "ad",
          bid: 1,
          render: {
            url: "https://response.test/",
            width: "-1px",
            height: "10px"
          }
        })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt, /*expected_errors=*/
      {"https://url.test/ generateBid() bid has invalid size for render ad."});
}

TEST_F(BidderWorkletTest, GenerateBidAdComponentsWithSize) {
  // The 'adComponents' field corresponds to an array of object that has the url
  // string.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({
          ad: 0,
          bid: 1,
          render: "https://response.test/",
          adComponents: [
            {url: "https://ad_component.test/"}
          ]
        })",
      /*expected_bids=*/mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "0", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          std::vector<blink::AdDescriptor>{
              blink::AdDescriptor(GURL("https://ad_component.test/"))},
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  // The 'adComponents' field corresponds to an array of object that has the url
  // string and size with units.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({
          ad: 0,
          bid: 1,
          render: "https://response.test/",
          adComponents: [
            {
              url: "https://ad_component.test/",
              width: "100sw",
              height: "50px"
            }
          ]
        })",
      /*expected_bids=*/mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "0", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          std::vector<blink::AdDescriptor>{blink::AdDescriptor(
              GURL("https://ad_component.test/"),
              blink::AdSize(100, blink::AdSize::LengthUnit::kScreenWidth, 50,
                            blink::AdSize::LengthUnit::kPixels))},
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  // The 'adComponents' field corresponds to an array of object that has the url
  // string and size without units.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({
          ad: 0,
          bid: 1,
          render: "https://response.test/",
          adComponents: [
            {
              url: "https://ad_component.test/",
              width: "100",
              height: "50"
            }
          ]
        })",
      /*expected_bids=*/mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "0", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          std::vector<blink::AdDescriptor>{blink::AdDescriptor(
              GURL("https://ad_component.test/"),
              blink::AdSize(100, blink::AdSize::LengthUnit::kPixels, 50,
                            blink::AdSize::LengthUnit::kPixels))},
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  // The 'adComponents' field corresponds to an array of objects.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({
          ad: 0,
          bid: 1,
          render: "https://response.test/",
          adComponents: [
            {
              url: "https://ad_component.test/",
              width: "100sw",
              height: "50px"
            },
            {
              url: "https://ad_component.test/"
            }
          ]
        })",
      /*expected_bids=*/mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "0", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          std::vector<blink::AdDescriptor>{
              blink::AdDescriptor(
                  GURL("https://ad_component.test/"),
                  blink::AdSize(100, blink::AdSize::LengthUnit::kScreenWidth,
                                50, blink::AdSize::LengthUnit::kPixels)),
              blink::AdDescriptor(GURL("https://ad_component.test/"))},
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  // The 'adComponents' field corresponds to an array of url string and objects.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({
          ad: 0,
          bid: 1,
          render: "https://response.test/",
          adComponents: [
            {
              url: "https://ad_component.test/",
              width: "100sw",
              height: "50px"
            },
            {
              url: "https://ad_component.test/"
            },
            "https://ad_component.test/"
          ]
        })",
      /*expected_bids=*/mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "0", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          std::vector<blink::AdDescriptor>{
              blink::AdDescriptor(
                  GURL("https://ad_component.test/"),
                  blink::AdSize(100, blink::AdSize::LengthUnit::kScreenWidth,
                                50, blink::AdSize::LengthUnit::kPixels)),
              blink::AdDescriptor(GURL("https://ad_component.test/")),
              blink::AdDescriptor(GURL("https://ad_component.test/"))},
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  // The 'adComponents' field corresponds to an array of an object that has an
  // invalid field.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({
          ad: 0,
          bid: 1,
          render: "https://response.test/",
          adComponents: [
            {
              foo: "https://ad_component.test/"
            }
          ]
        })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt, /*expected_errors=*/
      {"https://url.test/ generateBid() adComponents entry: Required field "
       "'url' is undefined."});

  // The 'adComponents' field corresponds to an array of an object that has an
  // invalid field. That's OK.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({
          ad: 0,
          bid: 1,
          render:"https://response.test/",
          adComponents: [
            {
              url: "https://ad_component.test/",
              width: "100sw",
              height: "50px",
              foo: 42
            }
          ]
        })",
      /*expected_bids=*/mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "0", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          std::vector<blink::AdDescriptor>{blink::AdDescriptor(
              GURL("https://ad_component.test/"),
              blink::AdSize(100, blink::AdSize::LengthUnit::kScreenWidth, 50,
                            blink::AdSize::LengthUnit::kPixels))},
          /*modeling_signals=*/std::nullopt, base::TimeDelta()));

  // The 'adComponents' field corresponds to an array of an empty object.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({
          ad: 0,
          bid: 1,
          render: "https://response.test/",
          adComponents: [
            {}
          ]
        })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt, /*expected_errors=*/
      {"https://url.test/ generateBid() adComponents entry: Required field "
       "'url' is undefined."});

  // The 'adComponents' field corresponds to an array of an object without url
  // string field.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({
          ad: 0,
          bid: 1,
          render:"https://response.test/",
          adComponents: [
            {
              width: "100sw",
              height: "50px"
            }
          ]
        })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt, /*expected_errors=*/
      {"https://url.test/ generateBid() adComponents entry: Required field "
       "'url' is undefined."});

  // Size is not covertible to a string type.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({
          ad: 0,
          bid: 1,
          render: "https://response.test/",
          adComponents: [
            {
              url: "https://ad_component.test/",
              width: {toString:() => { throw "oops" } },
              height: {toString:() => { throw "oh no" } }
            }
          ]
        })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt, /*expected_errors=*/
      {"https://url.test/:14 Uncaught oh no."});

  // Only width is specified.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({
          ad: 0,
          bid: 1,
          render: "https://response.test/",
          adComponents: [
            {
              url: "https://ad_component.test/",
              width: "100sw"
            }
          ]
        })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt, /*expected_errors=*/
      {"https://url.test/ generateBid() adComponents entry: ads that specify "
       "dimensions must specify both."});

  // Only height is specified.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({
          ad: 0,
          bid: 1,
          render: "https://response.test/",
          adComponents: [
            {
              url: "https://ad_component.test/",
              height: "50px"
            }
          ]
        })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt, /*expected_errors=*/
      {"https://url.test/ generateBid() adComponents entry: ads that specify "
       "dimensions must specify both."});

  // Size has invalid units.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({
          ad: 0,
          bid: 1,
          render: "https://response.test/",
          adComponents: [
            {
              url: "https://ad_component.test/",
              width: "100in",
              height: "100px"
            }
          ]
        })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt, /*expected_errors=*/
      {"https://url.test/ generateBid() bid adComponents have invalid size for "
       "ad component."});

  // Size doesn't have numbers.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({
          ad: 0,
          bid: 1,
          render: "https://response.test/",
          adComponents:
          [
            {
              url: "https://ad_component.test/",
              width: "sw",
              height: "px"
            }
          ]
        })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt, /*expected_errors=*/
      {"https://url.test/ generateBid() bid adComponents have invalid size for "
       "ad component."});

  // Size is empty.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({
          ad: 0,
          bid: 1,
          render: "https://response.test/",
          adComponents: [
            {
              url: "https://ad_component.test/",
              width: "",
              height: ""
            }
          ]
        })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt, /*expected_errors=*/
      {"https://url.test/ generateBid() bid adComponents have invalid size for "
       "ad component."});

  // Size has zero value.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({
          ad: 0,
          bid: 1,
          render: "https://response.test/",
          adComponents: [
            {
              url: "https://ad_component.test/",
              width: "0px",
              height: "1px"
            }
          ]
        })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt, /*expected_errors=*/
      {"https://url.test/ generateBid() bid adComponents have invalid size for "
       "ad component."});

  // Size has negative value.
  RunGenerateBidWithReturnValueExpectingResult(
      R"({
          ad: 0,
          bid: 1,
          render: "https://response.test/",
          adComponents: [
            {
              url: "https://ad_component.test/",
              width: "-1px",
              height: "5px"
            }
          ]
        })",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt, /*expected_errors=*/
      {"https://url.test/ generateBid() bid adComponents have invalid size for "
       "ad component."});
}

// The requested_ad_size argument to BeginGeneratingBid, which originates from
// the auction config, should be propagated into the bidding logic JS via the
// browserSignals property.
TEST_F(BidderWorkletTest, AuctionRequestedSizeIsPresentInBiddingLogic) {
  requested_ad_size_ = blink::AdSize(
      /*width=*/1920,
      /*width_units=*/blink::mojom::AdSize_LengthUnit::kPixels,
      /*height=*/100,
      /*height_units*/ blink::mojom::AdSize_LengthUnit::kScreenHeight);

  RunGenerateBidExpectingExpressionIsTrue(R"(
   (browserSignals.requestedSize.width === "1920px" &&
      browserSignals.requestedSize.height === "100sh");
  )");
}

TEST_F(BidderWorkletTest,
       AuctionRequestedSizeIsAbsentFromInBiddingLogicWhenNotProvided) {
  RunGenerateBidExpectingExpressionIsTrue(R"(
    !browserSignals.hasOwnProperty('requestedSize');
  )");
}

class BidderWorkletAdMacroReportingEnabledTest : public BidderWorkletTest {
 public:
  BidderWorkletAdMacroReportingEnabledTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kAdAuctionReportingWithMacroApi},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(BidderWorkletAdMacroReportingEnabledTest, ReportWinRegisterAdMacro) {
  RunReportWinWithFunctionBodyExpectingResult(
      R"(registerAdMacro('campaign', '111');
        registerAdMacro('', '111');
        registerAdMacro('empty_value', '');
        )",
      /*expected_report_url=*/std::nullopt, /*expected_ad_beacon_map=*/{},
      {{"campaign", "111"}, {"", "111"}, {"empty_value", ""}});

  // Any additional arguments after the first 2 are ignored.
  RunReportWinWithFunctionBodyExpectingResult(
      R"(registerAdMacro('campaign', '111', ['https://test.example'], 'a');)",
      /*expected_report_url=*/std::nullopt, /*expected_ad_beacon_map=*/{},
      {{"campaign", "111"}});

  // Type conversions happen.
  RunReportWinWithFunctionBodyExpectingResult(
      R"(registerAdMacro('campaign', 111);)",
      /*expected_report_url=*/std::nullopt, /*expected_ad_beacon_map=*/{},
      {{"campaign", "111"}});

  RunReportWinWithFunctionBodyExpectingResult(
      R"(registerAdMacro(null, 234);)",
      /*expected_report_url=*/std::nullopt, /*expected_ad_beacon_map=*/{},
      {{"null", "234"}});

  // Key is case sensitive.
  RunReportWinWithFunctionBodyExpectingResult(
      R"(registerAdMacro('campaign', '111');
        registerAdMacro('CAMPAIGN', '111');
        )",
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      {{"campaign", "111"}, {"CAMPAIGN", "111"}});

  // Value is case sensitive.
  RunReportWinWithFunctionBodyExpectingResult(
      R"(registerAdMacro('uppercase', 'ABC');
        registerAdMacro('lowercase', 'abc');
      )",
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      {{"uppercase", "ABC"}, {"lowercase", "abc"}});

  // URL-encoded strings should be accepted.
  RunReportWinWithFunctionBodyExpectingResult(
      R"(registerAdMacro('URL_ENC_KEY', 'http%3A%2F%2Fpub%2Eexample%2Fpage');
        registerAdMacro('http%3A%2F%2Fpub%2Eexample%2Fpage', 'URL_ENC_VAL');
        registerAdMacro('URL_ENC_KEY_http%3A%2F', 'URL_ENC_VAL_http%3A%2F');
      )",
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      {{"URL_ENC_KEY", "http%3A%2F%2Fpub%2Eexample%2Fpage"},
       {"http%3A%2F%2Fpub%2Eexample%2Fpage", "URL_ENC_VAL"},
       {"URL_ENC_KEY_http%3A%2F", "URL_ENC_VAL_http%3A%2F"}});

  // When called multiple times for a macro name, use the last valid call's
  // value.
  RunReportWinWithFunctionBodyExpectingResult(
      R"(registerAdMacro('campaign', '123');
        registerAdMacro('campaign', '111');
        registerAdMacro('publisher', 'abc');
        )",
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      {{"campaign", "111"}, {"publisher", "abc"}});

  // If the error is caught, do not clear other successful calls data.
  RunReportWinWithFunctionBodyExpectingResult(
      R"(registerAdMacro('campaign', '111');
        try { registerAdMacro('campaign', {toString: {}}) }
        catch (e) {}
        registerAdMacro('publisher', 'abc');)",
      /*expected_report_url=*/std::nullopt,
      /*expected_ad_beacon_map=*/{},
      {{"campaign", "111"}, {"publisher", "abc"}});
}

TEST_F(BidderWorkletAdMacroReportingEnabledTest,
       ReportWinRegisterAdMacroInvalidArgs) {
  struct TestCase {
    const char* call;
    const char* expected_error;
  } kTestCases[] = {
      // Less than 2 parameters.
      {R"(registerAdMacro();)",
       "https://url.test/:11 Uncaught TypeError: registerAdMacro(): at least 2 "
       "argument(s) are required."},
      {R"(registerAdMacro('123');)",
       "https://url.test/:11 Uncaught TypeError: registerAdMacro(): at least 2 "
       "argument(s) are required."},
      // Invalid first argument.
      {R"(registerAdMacro({toString:{}}, '456');)",
       "https://url.test/:11 Uncaught TypeError: Cannot convert object to "
       "primitive value."},
      // Invalid second argument.
      {R"(registerAdMacro('123', {toString:{}});)",
       "https://url.test/:11 Uncaught TypeError: Cannot convert object to "
       "primitive value."},
      // Invalid characters in macro key.
      {R"(registerAdMacro('}${FOO', 'foo');)",
       "https://url.test/:11 Uncaught TypeError: registerAdMacro macro key and "
       "value must be URL-encoded."},
      // Invalid characters in macro value.
      {R"(registerAdMacro('MACRO_KEY', 'baz&foo=bar');)",
       "https://url.test/:11 Uncaught TypeError: registerAdMacro macro key and "
       "value must be URL-encoded."},
  };
  for (const auto& test_case : kTestCases) {
    RunReportWinWithFunctionBodyExpectingResult(
        test_case.call,
        /*expected_report_url=*/std::nullopt,
        /*expected_ad_beacon_map=*/{},
        /*expected_ad_macro_map=*/{},
        /*expected_pa_requests=*/{}, {test_case.expected_error});
  }
}

class BidderWorkletSampleDebugReportsDisabledTest : public BidderWorkletTest {
 public:
  BidderWorkletSampleDebugReportsDisabledTest() {
    feature_list_.InitAndDisableFeature(
        blink::features::kFledgeSampleDebugReports);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(BidderWorkletSampleDebugReportsDisabledTest,
       GenerateBidBrowserSignalForDebuggingOnlyInCooldownOrLockout) {
  RunGenerateBidExpectingExpressionIsTrue(R"(
    !browserSignals.hasOwnProperty('forDebuggingOnlyInCooldownOrLockout');
  )");
}

class BidderWorkletCrossOriginTrustedSignalsTest : public BidderWorkletTest {
 public:
  BidderWorkletCrossOriginTrustedSignalsTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kFledgePermitCrossOriginTrustedSignals);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// With the feature on, same-origin trusted signals still come in the same,
// only there is an extra null param.
TEST_F(BidderWorkletCrossOriginTrustedSignalsTest, SameOrigin) {
  base::HistogramTester histogram_tester;

  const GURL kBaseSignalsUrl("https://signals.test/");
  interest_group_bidding_url_ = kBaseSignalsUrl;
  interest_group_trusted_bidding_signals_url_ = kBaseSignalsUrl;
  interest_group_trusted_bidding_signals_keys_ =
      std::vector<std::string>({"key1", "key2"});
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

  AddBidderJsonResponse(&url_loader_factory_, kFullSignalsUrl, kJson,
                        /*data_version=*/5);

  RunGenerateBidExpectingExpressionIsTrue("crossOriginTrustedSignals === null",
                                          /*expected_data_version=*/5);
  RunGenerateBidExpectingExpressionIsTrue("arguments.length === 7",
                                          /*expected_data_version=*/5);
  RunGenerateBidExpectingExpressionIsTrue("browserSignals.dataVersion === 5",
                                          /*expected_data_version=*/5);
  RunGenerateBidExpectingExpressionIsTrue(
      "!('crossOriginDataVersion' in browserSignals)",
      /*expected_data_version=*/5);

  RunGenerateBidExpectingExpressionIsTrue("trustedBiddingSignals['key1'] === 1",
                                          /*expected_data_version=*/5);

  // Should have one sample for each test.
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.TrustedBidderSignalsOriginRelation",
      BidderWorklet::SignalsOriginRelation::kSameOriginSignals, 5);
}

// Cross-origin signals (and their version) come in as different parameters
// and fields.
TEST_F(BidderWorkletCrossOriginTrustedSignalsTest, CrossOrigin) {
  base::HistogramTester histogram_tester;
  const GURL kBaseSignalsUrl("https://signals.test/");
  interest_group_bidding_url_ = GURL("https://url.test/");
  interest_group_trusted_bidding_signals_url_ = kBaseSignalsUrl;
  interest_group_trusted_bidding_signals_keys_ =
      std::vector<std::string>({"key1", "key2"});
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

  AddBidderJsonResponse(&url_loader_factory_, kFullSignalsUrl, kJson,
                        /*data_version=*/5);

  const char kValidate[] = R"(
    function() {
      const expected = '{"https://signals.test":{"key1":1,"key2":[2]}}';
      const actual = JSON.stringify(crossOriginTrustedSignals);
      if (actual === expected)
        return true;
      return actual + "!" + expected;
    }();
  )";

  RunGenerateBidExpectingExpressionIsTrue(kValidate,
                                          /*expected_data_version=*/5);
  RunGenerateBidExpectingExpressionIsTrue("arguments.length === 7",
                                          /*expected_data_version=*/5);
  RunGenerateBidExpectingExpressionIsTrue("!('dataVersion' in browserSignals)",
                                          /*expected_data_version=*/5);
  RunGenerateBidExpectingExpressionIsTrue(
      "browserSignals.crossOriginDataVersion === 5",
      /*expected_data_version=*/5);

  RunGenerateBidExpectingExpressionIsTrue("trustedBiddingSignals === null",
                                          /*expected_data_version=*/5);
  // Should have one sample for each test.
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.TrustedBidderSignalsOriginRelation",
      BidderWorklet::SignalsOriginRelation::kCrossOriginSignals, 5);
}

class BidderWorkletRealTimeReportingEnabledTest : public BidderWorkletTest {
 public:
  BidderWorkletRealTimeReportingEnabledTest() {
    feature_list_.InitAndEnableFeature(
        blink::features::kFledgeRealTimeReporting);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(BidderWorkletRealTimeReportingEnabledTest, RealTimeReporting) {
  auction_worklet::mojom::RealTimeReportingContribution expected_histogram(
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

  RunGenerateBidWithJavascriptExpectingResult(
      CreateBasicGenerateBidScriptWithExtraCode(kExtraCode),
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "[\"ad\"]", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/{}, std::nullopt, std::nullopt,
      /*expected_set_priority=*/std::nullopt,
      /*expected_update_priority_signals_overrides=*/{},
      /*expected_pa_requests=*/{},
      /*expected_non_kanon_pa_requests=*/{},
      std::move(expected_real_time_contributions));
}

// Real time reporting contributions are allowed when an IG does not bid.
TEST_F(BidderWorkletRealTimeReportingEnabledTest, NoBid) {
  auction_worklet::mojom::RealTimeReportingContribution expected_histogram(
      /*bucket=*/100, /*priority_weight=*/0.5,
      /*latency_threshold=*/std::nullopt);

  constexpr char kExtraCode[] = R"(
realTimeReporting.contributeToHistogram({bucket: 100, priorityWeight: 0.5});
)";

  RealTimeReportingContributions expected_real_time_contributions;
  expected_real_time_contributions.push_back(expected_histogram.Clone());

  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(R"({bid: 0})", kExtraCode),
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/{}, std::nullopt, std::nullopt,
      /*expected_set_priority=*/std::nullopt,
      /*expected_update_priority_signals_overrides=*/{},
      /*expected_pa_requests=*/{},
      /*expected_non_kanon_pa_requests=*/{},
      std::move(expected_real_time_contributions));
}

// Real time reporting contributions registered before script timeout are kept.
TEST_F(BidderWorkletRealTimeReportingEnabledTest, ScriptTimeout) {
  // Set timeout to a small number, and use a while loop in the script to let it
  // timeout. Then the execution time would be higher than the latency threshold
  // of 1ms thus the latency contribution will be kept.
  per_buyer_timeout_ = base::Milliseconds(3);

  auction_worklet::mojom::RealTimeReportingContribution expected_histogram(
      /*bucket=*/100, /*priority_weight=*/0.5,
      /*latency_threshold=*/std::nullopt);
  auction_worklet::mojom::RealTimeReportingContribution
      expected_latency_histogram(
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

  RunGenerateBidWithJavascriptExpectingResult(
      CreateBasicGenerateBidScriptWithExtraCode(kExtraCode),
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/
      {"https://url.test/ execution of `generateBid` timed out."}, std::nullopt,
      std::nullopt,
      /*expected_set_priority=*/std::nullopt,
      /*expected_update_priority_signals_overrides=*/{},
      /*expected_pa_requests=*/{},
      /*expected_non_kanon_pa_requests=*/{},
      std::move(expected_real_time_contributions));
}

// contributeToHistogram's is dropped when the script's latency does not
// exceed the threshold.
TEST_F(BidderWorkletRealTimeReportingEnabledTest,
       NotExceedingLatencyThreshold) {
  auction_worklet::mojom::RealTimeReportingContribution expected_histogram(
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

  RunGenerateBidWithJavascriptExpectingResult(
      CreateBasicGenerateBidScriptWithExtraCode(kExtraCode),
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "[\"ad\"]", 1,
          /*bid_currency=*/std::nullopt,
          /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/{}, std::nullopt, std::nullopt,
      /*expected_set_priority=*/std::nullopt,
      /*expected_update_priority_signals_overrides=*/{},
      /*expected_pa_requests=*/{},
      /*expected_non_kanon_pa_requests=*/{},
      std::move(expected_real_time_contributions));
}

// A platform contribution is added when trusted bidding signals server returned
// a non-2xx HTTP response code.
TEST_F(BidderWorkletRealTimeReportingEnabledTest,
       TrustedBiddingSignalNetworkError) {
  const GURL kBaseSignalsUrl("https://signals.test/");
  interest_group_bidding_url_ = kBaseSignalsUrl;
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

  // Request with valid TrustedBiddingSignals URL and non-empty keys. Request
  // should be made. The request fails.
  interest_group_trusted_bidding_signals_url_ = kBaseSignalsUrl;
  interest_group_trusted_bidding_signals_keys_ =
      std::vector<std::string>({"key1", "key2"});
  url_loader_factory_.AddResponse(kFullSignalsUrl.spec(), kJson,
                                  net::HTTP_NOT_FOUND);

  auction_worklet::mojom::RealTimeReportingContribution expected_histogram(
      /*bucket=*/100, /*priority_weight=*/0.5,
      /*latency_threshold=*/std::nullopt);
  mojom::RealTimeReportingContribution expected_trusted_signal_histogram(
      /*bucket=*/1024 + auction_worklet::RealTimeReportingPlatformError::
                            kTrustedBiddingSignalsFailure,
      /*priority_weight=*/1,
      /*latency_threshold=*/std::nullopt);

  constexpr char kExtraCode[] = R"(
realTimeReporting.contributeToHistogram({bucket: 100, priorityWeight: 0.5})
)";

  RealTimeReportingContributions expected_real_time_contributions;
  expected_real_time_contributions.push_back(expected_histogram.Clone());
  expected_real_time_contributions.push_back(
      expected_trusted_signal_histogram.Clone());

  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          /*raw_return_value=*/
          R"({ad: trustedBiddingSignals, bid: 1, render:"https://response.test/"})",
          kExtraCode),
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, "null", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()),
      /*expected_data_version=*/std::nullopt,
      {"Failed to load https://signals.test/"
       "?hostname=top.window.test&keys=key1,key2&interestGroupNames=Fred HTTP "
       "status = 404 Not Found."},
      std::nullopt, std::nullopt,
      /*expected_set_priority=*/std::nullopt,
      /*expected_update_priority_signals_overrides=*/{},
      /*expected_pa_requests=*/{},
      /*expected_non_kanon_pa_requests=*/{},
      std::move(expected_real_time_contributions));
}

// A platform contribution is added when trusted bidding signals server returned
// a non-2xx HTTP response code, even though generateBid() failed.
TEST_F(BidderWorkletRealTimeReportingEnabledTest,
       TrustedBiddingSignalNetworkErrorGenerateBidFailed) {
  const GURL kBaseSignalsUrl("https://signals.test/");
  interest_group_bidding_url_ = kBaseSignalsUrl;
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

  // Request with valid TrustedBiddingSignals URL and non-empty keys. Request
  // should be made. The request fails.
  interest_group_trusted_bidding_signals_url_ = kBaseSignalsUrl;
  interest_group_trusted_bidding_signals_keys_ =
      std::vector<std::string>({"key1", "key2"});
  url_loader_factory_.AddResponse(kFullSignalsUrl.spec(), kJson,
                                  net::HTTP_NOT_FOUND);

  mojom::RealTimeReportingContribution expected_trusted_signal_histogram(
      /*bucket=*/1024 + auction_worklet::RealTimeReportingPlatformError::
                            kTrustedBiddingSignalsFailure,
      /*priority_weight=*/1,
      /*latency_threshold=*/std::nullopt);

  RealTimeReportingContributions expected_real_time_contributions;
  expected_real_time_contributions.push_back(
      expected_trusted_signal_histogram.Clone());

  RunGenerateBidWithJavascriptExpectingResult(
      "shrimp",
      /*expected_bids=*/mojom::BidderWorkletBidPtr(),
      /*expected_data_version=*/std::nullopt,
      {"https://signals.test/:1 Uncaught ReferenceError: "
       "shrimp is not defined.",
       "Failed to load https://signals.test/"
       "?hostname=top.window.test&keys=key1,key2&interestGroupNames=Fred HTTP "
       "status = 404 Not Found."},
      std::nullopt, std::nullopt,
      /*expected_set_priority=*/std::nullopt,
      /*expected_update_priority_signals_overrides=*/{},
      /*expected_pa_requests=*/{},
      /*expected_non_kanon_pa_requests=*/{},
      std::move(expected_real_time_contributions));
}

// No platform contribution for trusted bidding signals failure when getting the
// signal succeeded.
TEST_F(BidderWorkletRealTimeReportingEnabledTest,
       TrustedBiddingSignalSucceedsNoContributionAdded) {
  const GURL kBaseSignalsUrl("https://signals.test/");
  interest_group_bidding_url_ = kBaseSignalsUrl;
  interest_group_trusted_bidding_signals_url_ = kBaseSignalsUrl;
  interest_group_trusted_bidding_signals_keys_.emplace();
  interest_group_trusted_bidding_signals_keys_->push_back("key1");
  AddBidderJsonResponse(
      &url_loader_factory_,
      GURL("https://signals.test/"
           "?hostname=top.window.test&keys=key1&interestGroupNames=Fred"),
      R"({"keys":{"key1":1}})");

  constexpr char kExtraCode[] = R"(
realTimeReporting.contributeToHistogram({bucket: 100, priorityWeight: 0.5})
)";
  auction_worklet::mojom::RealTimeReportingContribution expected_histogram(
      /*bucket=*/100, /*priority_weight=*/0.5,
      /*latency_threshold=*/std::nullopt);

  // Only expects the API call's contribution. No platform contribution is
  // added.
  RealTimeReportingContributions expected_real_time_contributions;
  expected_real_time_contributions.push_back(expected_histogram.Clone());

  RunGenerateBidWithJavascriptExpectingResult(
      CreateGenerateBidScript(
          /*raw_return_value=*/
          R"({ad: trustedBiddingSignals, bid: 1, render:"https://response.test/"})",
          kExtraCode),
      mojom::BidderWorkletBid::New(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon, R"({"key1":1})", 1,
          /*bid_currency=*/std::nullopt, /*ad_cost=*/std::nullopt,
          blink::AdDescriptor(GURL("https://response.test/")),
          /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
          /*ad_component_descriptors=*/std::nullopt,
          /*modeling_signals=*/std::nullopt, base::TimeDelta()),
      /*expected_data_version=*/std::nullopt,
      /*expected_errors=*/{}, std::nullopt, std::nullopt,
      /*expected_set_priority=*/std::nullopt,
      /*expected_update_priority_signals_overrides=*/{},
      /*expected_pa_requests=*/{},
      /*expected_non_kanon_pa_requests=*/{},
      std::move(expected_real_time_contributions));
}

class BidderWorkletKVv2Test : public BidderWorkletTest {
 public:
  BidderWorkletKVv2Test() {
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

TEST_F(BidderWorkletKVv2Test, GenerateBidTrustedBiddingSignals) {
  const char kJson[] =
      R"([{
          "id": 1,
          "dataVersion": 101,
          "keyGroupOutputs": [
            {
              "tags": [
                "interestGroupNames"
              ],
              "keyValues": {
                "Fred": {
                  "value": "{\"priorityVector\":{\"foo\":1}}"
                }
              }
            },
            {
              "tags": [
                "keys"
              ],
              "keyValues": {
                "key1": {
                  "value": "1"
                },
                "key2": {
                  "value": "[2]"
                }
              }
            }
          ]
        }])";

  interest_group_trusted_bidding_signals_url_ =
      GURL("https://url.test/kvv2-test/");
  interest_group_trusted_bidding_signals_keys_ =
      std::vector<std::string>({"key1", "key2"});

  auto bidder_worklet = CreateWorklet();
  AddJavascriptResponse(&url_loader_factory_, interest_group_bidding_url_,
                        CreateGenerateBidScript(
                            R"({ad: trustedBiddingSignals, bid:1,
            render:"https://response.test/"})"));
  generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
  GenerateBid(bidder_worklet.get());

  // Decrypt request and encrypt response.
  task_environment_.RunUntilIdle();
  ASSERT_EQ(1, url_loader_factory_.NumPending());
  const network::ResourceRequest* pending_request;
  ASSERT_TRUE(url_loader_factory_.IsPending(
      interest_group_trusted_bidding_signals_url_->spec(), &pending_request));

  std::string request_body =
      std::string(pending_request->request_body->elements()
                      ->at(0)
                      .As<network::DataElementBytes>()
                      .AsStringPiece());
  std::string response_body = GenerateResponseBody(request_body, kJson);

  std::string headers =
      base::StringPrintf("%s\nContent-Type: %s", kAllowFledgeHeader,
                         "message/ad-auction-trusted-signals-request");
  AddResponse(&url_loader_factory_,
              interest_group_trusted_bidding_signals_url_.value(),
              kAdAuctionTrustedSignalsMimeType,
              /*charset=*/std::nullopt, response_body, headers);

  generate_bid_run_loop_->Run();
  generate_bid_run_loop_.reset();

  ASSERT_EQ(0UL, bid_errors_.size());
  ASSERT_EQ(1UL, bids_.size());

  mojom::BidderWorkletBidPtr expected_bid = mojom::BidderWorkletBid::New(
      auction_worklet::mojom::BidRole::kUnenforcedKAnon,
      R"({"key1":1,"key2":[2]})", 1, /*bid_currency=*/std::nullopt,
      /*ad_cost=*/std::nullopt,
      blink::AdDescriptor(GURL("https://response.test/")),
      /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
      /*ad_component_descriptors=*/std::nullopt,
      /*modeling_signals=*/std::nullopt, base::TimeDelta());

  auto actual_bid = std::move(bids_[0]);
  EXPECT_EQ(expected_bid->bid_role, actual_bid->bid_role);
  EXPECT_EQ(expected_bid->ad, actual_bid->ad);
  EXPECT_EQ(expected_bid->selected_buyer_and_seller_reporting_id,
            actual_bid->selected_buyer_and_seller_reporting_id);
  EXPECT_EQ(expected_bid->bid, actual_bid->bid);
  EXPECT_EQ(blink::PrintableAdCurrency(expected_bid->bid_currency),
            blink::PrintableAdCurrency(actual_bid->bid_currency));
  EXPECT_EQ(expected_bid->ad_descriptor.url, actual_bid->ad_descriptor.url);
  EXPECT_EQ(expected_bid->ad_descriptor.size, actual_bid->ad_descriptor.size);
  if (!expected_bid->ad_component_descriptors) {
    EXPECT_FALSE(actual_bid->ad_component_descriptors);
  } else {
    EXPECT_THAT(
        *actual_bid->ad_component_descriptors,
        ::testing::ElementsAreArray(*expected_bid->ad_component_descriptors));
  }
  if (!expected_bid->bid_duration.is_zero()) {
    EXPECT_GE(actual_bid->bid_duration, expected_bid->bid_duration);
  }
  EXPECT_EQ(101, data_version_);
}

}  // namespace
}  // namespace auction_worklet
