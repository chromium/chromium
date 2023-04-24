// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/mock_auction_process_manager.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "content/browser/interest_group/auction_process_manager.h"
#include "content/public/browser/site_instance.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

MockBidderWorklet::MockBidderWorklet(
    mojo::PendingReceiver<auction_worklet::mojom::BidderWorklet>
        pending_receiver,
    const std::map<std::string, base::TimeDelta>& expected_per_buyer_timeouts)
    : expected_per_buyer_timeouts_(expected_per_buyer_timeouts),
      receiver_(this, std::move(pending_receiver)) {
  receiver_.set_disconnect_handler(
      base::BindOnce(&MockBidderWorklet::OnPipeClosed, base::Unretained(this)));
}

MockBidderWorklet::~MockBidderWorklet() {
  // `send_pending_signals_requests_called_` should always be called if any
  // bids are generated, except in the unlikely event that the Mojo pipe is
  // closed before a posted task is executed (this cannot be simulated by
  // closing a pipe in tests, due to vagaries of timing of the two messages).
  if (generate_bid_called_) {
    // Flush the receiver in case the message is pending on the pipe. This
    // doesn't happen when the auction has run successfully, where the auction
    // only completes when all messages have been received, but may happen in
    // failure cases where the message is sent, but the AuctionRunner is torn
    // down early.
    if (receiver_.is_bound()) {
      receiver_.FlushForTesting();
    }
    EXPECT_TRUE(send_pending_signals_requests_called_);
  }
}

void MockBidderWorklet::BeginGenerateBid(
    auction_worklet::mojom::BidderWorkletNonSharedParamsPtr
        bidder_worklet_non_shared_params,
    auction_worklet::mojom::KAnonymityBidMode kanon_mode,
    const url::Origin& interest_group_join_origin,
    const absl::optional<GURL>& direct_from_seller_per_buyer_signals,
    const absl::optional<GURL>& direct_from_seller_auction_signals,
    const url::Origin& browser_signal_seller_origin,
    const absl::optional<url::Origin>& browser_signal_top_level_seller_origin,
    auction_worklet::mojom::BiddingBrowserSignalsPtr bidding_browser_signals,
    base::Time auction_start_time,
    uint64_t trace_id,
    mojo::PendingAssociatedRemote<auction_worklet::mojom::GenerateBidClient>
        generate_bid_client,
    mojo::PendingAssociatedReceiver<
        auction_worklet::mojom::GenerateBidFinalizer> bid_finalizer) {
  generate_bid_called_ = true;
  // While the real BidderWorklet implementation supports multiple pending
  // callbacks, this class does not.
  DCHECK(!generate_bid_client_);

  // per_buyer_timeout passed that will be passed to FinishGenerateBid()
  // should not be empty, because auction_config's all_buyers_timeout (which
  // is the key of '*' in perBuyerTimeouts) is set in the AuctionRunnerTest.
  // Figure out what it should expect here (and save it into the receiver set
  // as context info) since the bidder name isn't easily available at
  // FinishGenerateBid time.
  auto it =
      expected_per_buyer_timeouts_.find(bidder_worklet_non_shared_params->name);
  CHECK(it != expected_per_buyer_timeouts_.end());
  base::TimeDelta expected_per_buyer_timeout = it->second;

  // Single auctions should invoke all GenerateBid() calls on a worklet
  // before invoking SendPendingSignalsRequests().
  EXPECT_FALSE(send_pending_signals_requests_called_);

  finalizer_receiver_set_.Add(this, std::move(bid_finalizer),
                              expected_per_buyer_timeout);

  generate_bid_client_.Bind(std::move(generate_bid_client));
}

void MockBidderWorklet::SendPendingSignalsRequests() {
  // This allows multiple calls.
  send_pending_signals_requests_called_ = true;
}

void MockBidderWorklet::ReportWin(
    const std::string& interest_group_name,
    const absl::optional<std::string>& auction_signals_json,
    const absl::optional<std::string>& per_buyer_signals_json,
    const absl::optional<GURL>& direct_from_seller_per_buyer_signals,
    const absl::optional<GURL>& direct_from_seller_auction_signals,
    const std::string& seller_signals_json,
    const GURL& browser_signal_render_url,
    double browser_signal_bid,
    const absl::optional<blink::AdCurrency>& browser_signal_bid_currency,
    double browser_signal_highest_scoring_other_bid,
    const absl::optional<blink::AdCurrency>&
        browser_signal_highest_scoring_other_bid_currency,
    bool browser_signal_made_highest_scoring_other_bid,
    absl::optional<double> browser_signal_ad_cost,
    absl::optional<uint16_t> browser_signal_modeling_signals,
    uint8_t browser_signal_join_count,
    uint8_t browser_signal_recency,
    const url::Origin& browser_signal_seller_origin,
    const absl::optional<url::Origin>& browser_signal_top_level_seller_origin,
    uint32_t bidding_signals_data_version,
    bool has_bidding_signals_data_version,
    uint64_t trace_id,
    ReportWinCallback report_win_callback) {
  // While the real BidderWorklet implementation supports multiple pending
  // callbacks, this class does not.
  DCHECK(!report_win_callback_);
  report_win_callback_ = std::move(report_win_callback);
  if (report_win_run_loop_) {
    report_win_run_loop_->Quit();
  }
}

void MockBidderWorklet::ConnectDevToolsAgent(
    mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent) {
  ADD_FAILURE()
      << "ConnectDevToolsAgent should not be called on MockBidderWorklet";
}

void MockBidderWorklet::FinishGenerateBid(
    const absl::optional<std::string>& auction_signals_json,
    const absl::optional<std::string>& per_buyer_signals_json,
    const absl::optional<base::TimeDelta> per_buyer_timeout,
    const absl::optional<blink::AdCurrency>& per_buyer_currency,
    const absl::optional<GURL>& direct_from_seller_per_buyer_signals,
    const absl::optional<GURL>& direct_from_seller_auction_signals) {
  // per_buyer_timeout passed to GenerateBid() should not be empty, because
  // auction_config's all_buyers_timeout (which is the key of '*' in
  // perBuyerTimeouts) is set in the AuctionRunnerTest.
  ASSERT_TRUE(per_buyer_timeout.has_value());

  // The actual expected value is stashed by BeginGenerateBid into the
  // context.
  EXPECT_EQ(per_buyer_timeout.value(),
            finalizer_receiver_set_.current_context());

  if (generate_bid_run_loop_) {
    generate_bid_run_loop_->Quit();
  }
}

void MockBidderWorklet::WaitForGenerateBid() {
  if (!generate_bid_client_) {
    generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
    generate_bid_run_loop_->Run();
    generate_bid_run_loop_.reset();
    DCHECK(generate_bid_client_);
  }
}

void MockBidderWorklet::SetBidderTrustedSignalsFetchLatency(
    base::TimeDelta delta) {
  trusted_signals_fetch_latency_ = delta;
}

void MockBidderWorklet::SetBiddingLatency(base::TimeDelta delta) {
  bidding_latency_ = delta;
}

void MockBidderWorklet::InvokeGenerateBidCallback(
    absl::optional<double> bid,
    const absl::optional<blink::AdCurrency>& bid_currency,
    const blink::AdDescriptor& ad_descriptor,
    auction_worklet::mojom::BidderWorkletKAnonEnforcedBidPtr mojo_kanon_bid,
    absl::optional<std::vector<blink::AdDescriptor>> ad_component_descriptors,
    base::TimeDelta duration,
    const absl::optional<uint32_t>& bidding_signals_data_version,
    const absl::optional<GURL>& debug_loss_report_url,
    const absl::optional<GURL>& debug_win_report_url,
    std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>
        pa_requests) {
  WaitForGenerateBid();

  base::RunLoop run_loop;
  generate_bid_client_->OnBiddingSignalsReceived(
      /*priority_vector=*/{},
      /*trusted_signals_fetch_latency=*/trusted_signals_fetch_latency_,
      run_loop.QuitClosure());
  run_loop.Run();

  if (!bid.has_value()) {
    generate_bid_client_->OnGenerateBidComplete(
        /*bid=*/nullptr,
        /*kanon_bid=*/std::move(mojo_kanon_bid),
        /*bidding_signals_data_version=*/0,
        /*has_bidding_signals_data_version=*/false, debug_loss_report_url,
        /*debug_win_report_url=*/absl::nullopt,
        /*set_priority=*/0,
        /*has_set_priority=*/false,
        /*update_priority_signals_overrides=*/
        base::flat_map<std::string,
                       auction_worklet::mojom::PrioritySignalsDoublePtr>(),
        /*pa_requests=*/std::move(pa_requests),
        /*non_kanon_pa_requests=*/{},
        /*bidding_latency=*/bidding_latency_,
        /*errors=*/std::vector<std::string>());
    return;
  }

  generate_bid_client_->OnGenerateBidComplete(
      auction_worklet::mojom::BidderWorkletBid::New(
          "ad", *bid, bid_currency, /*ad_cost=*/absl::nullopt,
          std::move(ad_descriptor), ad_component_descriptors,
          /*modeling_signals=*/absl::nullopt, duration),
      /*kanon_bid=*/std::move(mojo_kanon_bid),
      bidding_signals_data_version.value_or(0),
      bidding_signals_data_version.has_value(), debug_loss_report_url,
      debug_win_report_url,
      /*set_priority=*/0,
      /*has_set_priority=*/false,
      /*update_priority_signals_overrides=*/
      base::flat_map<std::string,
                     auction_worklet::mojom::PrioritySignalsDoublePtr>(),
      /*pa_requests=*/std::move(pa_requests),
      /*non_kanon_pa_requests=*/{},
      /*bidding_latency=*/bidding_latency_,
      /*errors=*/std::vector<std::string>());
}

void MockBidderWorklet::WaitForReportWin() {
  DCHECK(!generate_bid_client_);
  DCHECK(!report_win_run_loop_);
  if (!report_win_callback_) {
    report_win_run_loop_ = std::make_unique<base::RunLoop>();
    report_win_run_loop_->Run();
    report_win_run_loop_.reset();
    DCHECK(report_win_callback_);
  }
}

void MockBidderWorklet::InvokeReportWinCallback(
    absl::optional<GURL> report_url,
    base::flat_map<std::string, GURL> ad_beacon_map,
    std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>
        pa_requests,
    std::vector<std::string> errors) {
  DCHECK(report_win_callback_);
  std::move(report_win_callback_)
      .Run(report_url, ad_beacon_map, std::move(pa_requests),
           std::move(errors));
}

void MockBidderWorklet::Flush() {
  receiver_.FlushForTesting();
}

bool MockBidderWorklet::PipeIsClosed() {
  receiver_.FlushForTesting();
  return pipe_closed_;
}

MockSellerWorklet::ScoreAdParams::ScoreAdParams() = default;
MockSellerWorklet::ScoreAdParams::ScoreAdParams(ScoreAdParams&&) = default;
MockSellerWorklet::ScoreAdParams::~ScoreAdParams() = default;
MockSellerWorklet::ScoreAdParams& MockSellerWorklet::ScoreAdParams::operator=(
    ScoreAdParams&&) = default;

MockSellerWorklet::MockSellerWorklet(
    mojo::PendingReceiver<auction_worklet::mojom::SellerWorklet>
        pending_receiver)
    : receiver_(this, std::move(pending_receiver)) {}

MockSellerWorklet::~MockSellerWorklet() {
  // Flush the receiver in case the message is pending on the pipe. This
  // doesn't happen when the auction has run successfully, where the auction
  // only completes when all messages have been received, but may happen in
  // failure cases where the message is sent, but the AuctionRunner is torn
  // down early.
  if (receiver_.is_bound()) {
    receiver_.FlushForTesting();
  }

  EXPECT_EQ(expect_send_pending_signals_requests_called_,
            send_pending_signals_requests_called_);

  // Every received ScoreAd() call should have been waited for.
  EXPECT_TRUE(score_ad_params_.empty());
}

void MockSellerWorklet::ScoreAd(
    const std::string& ad_metadata_json,
    double bid,
    const absl::optional<blink::AdCurrency>& bid_currency,
    const blink::AuctionConfig::NonSharedParams&
        auction_ad_config_non_shared_params,
    const absl::optional<GURL>& direct_from_seller_seller_signals,
    const absl::optional<GURL>& direct_from_seller_auction_signals,
    auction_worklet::mojom::ComponentAuctionOtherSellerPtr
        browser_signals_other_seller,
    const absl::optional<blink::AdCurrency>& component_expect_bid_currency,
    const url::Origin& browser_signal_interest_group_owner,
    const GURL& browser_signal_render_url,
    const std::vector<GURL>& browser_signal_ad_components,
    uint32_t browser_signal_bidding_duration_msecs,
    const absl::optional<base::TimeDelta> seller_timeout,
    uint64_t trace_id,
    mojo::PendingRemote<auction_worklet::mojom::ScoreAdClient>
        score_ad_client) {
  // SendPendingSignalsRequests() should only be called once all ads are
  // scored.
  EXPECT_FALSE(send_pending_signals_requests_called_);

  ASSERT_TRUE(seller_timeout.has_value());
  // seller_timeout in auction_config higher than 500 ms should be clamped to
  // 500 ms by the AuctionRunner before passed to ScoreAd(), and
  // auction_config's seller_timeout is 1000 ms so it should be 500 ms here.
  EXPECT_EQ(seller_timeout.value(), base::Milliseconds(500));

  ScoreAdParams score_ad_params;
  score_ad_params.score_ad_client = std::move(score_ad_client);
  score_ad_params.bid = bid;
  score_ad_params.interest_group_owner = browser_signal_interest_group_owner;
  score_ad_params_.emplace_front(std::move(score_ad_params));
  if (score_ad_run_loop_) {
    score_ad_run_loop_->Quit();
  }
}

void MockSellerWorklet::SendPendingSignalsRequests() {
  // SendPendingSignalsRequests() should only be called once by a single
  // AuctionRunner.
  EXPECT_FALSE(send_pending_signals_requests_called_);

  send_pending_signals_requests_called_ = true;
}

void MockSellerWorklet::ReportResult(
    const blink::AuctionConfig::NonSharedParams&
        auction_ad_config_non_shared_params,
    const absl::optional<GURL>& direct_from_seller_seller_signals,
    const absl::optional<GURL>& direct_from_seller_auction_signals,
    auction_worklet::mojom::ComponentAuctionOtherSellerPtr
        browser_signals_other_seller,
    const url::Origin& browser_signal_interest_group_owner,
    const GURL& browser_signal_render_url,
    double browser_signal_bid,
    const absl::optional<blink::AdCurrency>& browser_signal_bid_currency,
    double browser_signal_desirability,
    double browser_signal_highest_scoring_other_bid,
    const absl::optional<blink::AdCurrency>&
        browser_signal_highest_scoring_other_bid_currency,
    auction_worklet::mojom::ComponentAuctionReportResultParamsPtr
        browser_signals_component_auction_report_result_params,
    uint32_t browser_signal_data_version,
    bool browser_signal_has_data_version,
    uint64_t trace_id,
    ReportResultCallback report_result_callback) {
  report_result_callback_ = std::move(report_result_callback);
  if (report_result_run_loop_) {
    report_result_run_loop_->Quit();
  }
}

void MockSellerWorklet::ConnectDevToolsAgent(
    mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent) {
  ADD_FAILURE()
      << "ConnectDevToolsAgent should not be called on MockSellerWorklet";
}

void MockSellerWorklet::ResetReceiverWithReason(const std::string& reason) {
  receiver_.ResetWithReason(/*custom_reason_code=*/0, reason);
}

// Waits until ScoreAd() has been invoked, if it hasn't been already. It's up
// to the caller to invoke the returned ScoreAdParams::callback to continue
// the auction.
MockSellerWorklet::ScoreAdParams MockSellerWorklet::WaitForScoreAd() {
  DCHECK(!score_ad_run_loop_);
  if (score_ad_params_.empty()) {
    score_ad_run_loop_ = std::make_unique<base::RunLoop>();
    score_ad_run_loop_->Run();
    score_ad_run_loop_.reset();
    DCHECK(!score_ad_params_.empty());
  }
  ScoreAdParams out = std::move(score_ad_params_.front());
  score_ad_params_.pop_front();
  return out;
}

void MockSellerWorklet::WaitForReportResult() {
  DCHECK(!report_result_run_loop_);
  if (!report_result_callback_) {
    report_result_run_loop_ = std::make_unique<base::RunLoop>();
    report_result_run_loop_->Run();
    report_result_run_loop_.reset();
    DCHECK(report_result_callback_);
  }
}

void MockSellerWorklet::InvokeReportResultCallback(
    absl::optional<GURL> report_url,
    base::flat_map<std::string, GURL> ad_beacon_map,
    std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>
        pa_requests,
    std::vector<std::string> errors) {
  DCHECK(report_result_callback_);
  std::move(report_result_callback_)
      .Run(/*signals_for_winner=*/absl::nullopt, std::move(report_url),
           ad_beacon_map, std::move(pa_requests), errors);
}

void MockSellerWorklet::Flush() {
  receiver_.FlushForTesting();
}

MockAuctionProcessManager::MockAuctionProcessManager() = default;
MockAuctionProcessManager::~MockAuctionProcessManager() = default;

RenderProcessHost* MockAuctionProcessManager::LaunchProcess(
    mojo::PendingReceiver<auction_worklet::mojom::AuctionWorkletService>
        auction_worklet_service_receiver,
    const ProcessHandle* handle,
    const std::string& display_name) {
  mojo::ReceiverId receiver_id =
      receiver_set_.Add(this, std::move(auction_worklet_service_receiver));

  // Each receiver should get a unique display name. This check serves to help
  // ensure that processes are correctly reused.
  EXPECT_EQ(0u, receiver_display_name_map_.count(receiver_id));
  for (auto receiver : receiver_display_name_map_) {
    // Ignore closed receivers. ReportWin() will result in re-loading a
    // worklet, after closing the original worklet, which may require
    // re-creating the AuctionWorkletService.
    if (receiver_set_.HasReceiver(receiver.first)) {
      EXPECT_NE(receiver.second, display_name);
    }
  }

  receiver_display_name_map_[receiver_id] = display_name;
  return nullptr;
}

scoped_refptr<SiteInstance> MockAuctionProcessManager::MaybeComputeSiteInstance(
    SiteInstance* frame_site_instance,
    const url::Origin& worklet_origin) {
  return nullptr;
}

bool MockAuctionProcessManager::TryUseSharedProcess(
    ProcessHandle* process_handle) {
  return false;
}

void MockAuctionProcessManager::LoadBidderWorklet(
    mojo::PendingReceiver<auction_worklet::mojom::BidderWorklet>
        bidder_worklet_receiver,
    mojo::PendingRemote<auction_worklet::mojom::AuctionSharedStorageHost>
        shared_storage_host_remote,
    bool pause_for_debugger_on_start,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_url_loader_factory,
    const GURL& script_source_url,
    const absl::optional<GURL>& bidding_wasm_helper_url,
    const absl::optional<GURL>& trusted_bidding_signals_url,
    const url::Origin& top_window_origin,
    auction_worklet::mojom::AuctionWorkletPermissionsPolicyStatePtr
        permissions_policy_state,
    bool has_experiment_group_id,
    uint16_t experiment_group_id) {
  // Make sure this request came over the right pipe.
  url::Origin owner = url::Origin::Create(script_source_url);
  EXPECT_EQ(receiver_display_name_map_[receiver_set_.current_receiver()],
            ComputeDisplayName(AuctionProcessManager::WorkletType::kBidder,
                               url::Origin::Create(script_source_url)));

  EXPECT_EQ(0u, bidder_worklets_.count(script_source_url));
  bidder_worklets_.emplace(
      script_source_url,
      std::make_unique<MockBidderWorklet>(std::move(bidder_worklet_receiver),
                                          expected_per_buyer_timeouts_));
  MaybeQuitWaitForWorkletsRunLoop();
}

void MockAuctionProcessManager::LoadSellerWorklet(
    mojo::PendingReceiver<auction_worklet::mojom::SellerWorklet>
        seller_worklet_receiver,
    mojo::PendingRemote<auction_worklet::mojom::AuctionSharedStorageHost>
        shared_storage_host_remote,
    bool should_pause_on_start,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_url_loader_factory,
    const GURL& script_source_url,
    const absl::optional<GURL>& trusted_scoring_signals_url,
    const url::Origin& top_window_origin,
    auction_worklet::mojom::AuctionWorkletPermissionsPolicyStatePtr
        permissions_policy_state,
    bool has_experiment_group_id,
    uint16_t experiment_group_id) {
  EXPECT_EQ(0u, seller_worklets_.count(script_source_url));

  // Make sure this request came over the right pipe.
  EXPECT_EQ(receiver_display_name_map_[receiver_set_.current_receiver()],
            ComputeDisplayName(AuctionProcessManager::WorkletType::kSeller,
                               url::Origin::Create(script_source_url)));

  seller_worklets_.emplace(
      script_source_url,
      std::make_unique<MockSellerWorklet>(std::move(seller_worklet_receiver)));

  MaybeQuitWaitForWorkletsRunLoop();
}

void MockAuctionProcessManager::SetExpectedBuyerBidTimeout(
    const std::string& name,
    base::TimeDelta value) {
  expected_per_buyer_timeouts_[name] = value;
}

void MockAuctionProcessManager::WaitForWorklets(int num_bidders,
                                                int num_sellers) {
  DCHECK(!wait_for_worklets_run_loop_);

  waiting_for_num_bidders_ = num_bidders;
  waiting_for_num_sellers_ = num_sellers;
  wait_for_worklets_run_loop_ = std::make_unique<base::RunLoop>();
  MaybeQuitWaitForWorkletsRunLoop();
  wait_for_worklets_run_loop_->Run();
  wait_for_worklets_run_loop_.reset();

  EXPECT_EQ(waiting_for_num_bidders_, bidder_worklets_.size());
  EXPECT_EQ(waiting_for_num_sellers_, seller_worklets_.size());
  waiting_for_num_bidders_ = 0;
  waiting_for_num_sellers_ = 0;
}

void MockAuctionProcessManager::WaitForWinningBidderReload() {
  WaitForWorklets(/*num_bidders=*/1, /*num_sellers=*/0);
}

void MockAuctionProcessManager::WaitForWinningSellerReload() {
  WaitForWorklets(/*num_bidders=*/0, /*num_sellers=*/1);
}

std::unique_ptr<MockBidderWorklet> MockAuctionProcessManager::TakeBidderWorklet(
    const GURL& script_source_url) {
  auto it = bidder_worklets_.find(script_source_url);
  if (it == bidder_worklets_.end()) {
    return nullptr;
  }
  std::unique_ptr<MockBidderWorklet> out = std::move(it->second);
  bidder_worklets_.erase(it);
  return out;
}

std::unique_ptr<MockSellerWorklet> MockAuctionProcessManager::TakeSellerWorklet(
    GURL script_source_url) {
  if (seller_worklets_.empty()) {
    return nullptr;
  }

  if (script_source_url.is_empty()) {
    CHECK_EQ(1u, seller_worklets_.size());
    script_source_url = seller_worklets_.begin()->first;
  }

  auto it = seller_worklets_.find(script_source_url);
  if (it == seller_worklets_.end()) {
    return nullptr;
  }
  std::unique_ptr<MockSellerWorklet> out = std::move(it->second);
  seller_worklets_.erase(it);
  return out;
}

void MockAuctionProcessManager::Flush() {
  receiver_set_.FlushForTesting();
}

void MockAuctionProcessManager::MaybeQuitWaitForWorkletsRunLoop() {
  if (wait_for_worklets_run_loop_ &&
      bidder_worklets_.size() >= waiting_for_num_bidders_ &&
      seller_worklets_.size() >= waiting_for_num_sellers_) {
    wait_for_worklets_run_loop_->Quit();
  }
}

}  // namespace content
