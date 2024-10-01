// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/mock_auction_process_manager.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
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
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom-forward.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "content/services/auction_worklet/public/mojom/real_time_reporting.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/interest_group/ad_display_size.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

MockBidderWorklet::MockBidderWorklet(
    mojo::PendingReceiver<auction_worklet::mojom::BidderWorklet>
        pending_receiver,
    const std::map<std::string, base::TimeDelta>& expected_per_buyer_timeouts,
    bool skip_generate_bid)
    : skip_generate_bid_(skip_generate_bid),
      expected_per_buyer_timeouts_(expected_per_buyer_timeouts),
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
    const std::optional<GURL>& direct_from_seller_per_buyer_signals,
    const std::optional<GURL>& direct_from_seller_auction_signals,
    const url::Origin& browser_signal_seller_origin,
    const std::optional<url::Origin>& browser_signal_top_level_seller_origin,
    const base::TimeDelta browser_signal_recency,
    blink::mojom::BiddingBrowserSignalsPtr bidding_browser_signals,
    base::Time auction_start_time,
    const std::optional<blink::AdSize>& requested_ad_size,
    uint16_t multi_bid_limit,
    uint64_t trace_id,
    mojo::PendingAssociatedRemote<auction_worklet::mojom::GenerateBidClient>
        generate_bid_client,
    mojo::PendingAssociatedReceiver<
        auction_worklet::mojom::GenerateBidFinalizer> bid_finalizer) {
  if (skip_generate_bid_) {
    return;
  }

  generate_bid_called_ = true;

  // per_buyer_timeout passed that will be passed to FinishGenerateBid()
  // should not be empty, because auction_config's all_buyers_timeout (which
  // is the key of '*' in perBuyerTimeouts) is set in the AuctionRunnerTest.
  // Figure out what it should expect here (and save it into
  // `finalizer_receiver_set_` as context info) since the bidder name isn't
  // easily available at FinishGenerateBid time.
  auto it =
      expected_per_buyer_timeouts_.find(bidder_worklet_non_shared_params->name);
  CHECK(it != expected_per_buyer_timeouts_.end());
  base::TimeDelta expected_per_buyer_timeout = it->second;

  // Single auctions should invoke all GenerateBid() calls on a worklet
  // before invoking SendPendingSignalsRequests().
  EXPECT_FALSE(send_pending_signals_requests_called_);

  finalizer_receiver_set_.Add(this, std::move(bid_finalizer),
                              expected_per_buyer_timeout);

  mojo::AssociatedRemote<auction_worklet::mojom::GenerateBidClient>
      bound_generate_bid_client;
  bound_generate_bid_client.Bind(std::move(generate_bid_client));
  generate_bid_clients_.push_back(std::move(bound_generate_bid_client));
}

void MockBidderWorklet::SendPendingSignalsRequests() {
  // This allows multiple calls.
  send_pending_signals_requests_called_ = true;
}

void MockBidderWorklet::ReportWin(
    bool is_for_additional_bid,
    const std::optional<std::string>& interest_group_name_reporting_id,
    const std::optional<std::string>& buyer_reporting_id,
    const std::optional<std::string>& buyer_and_seller_reporting_id,
    const std::optional<std::string>& selected_buyer_and_seller_reporting_id,
    const std::optional<std::string>& auction_signals_json,
    const std::optional<std::string>& per_buyer_signals_json,
    const std::optional<GURL>& direct_from_seller_per_buyer_signals,
    const std::optional<std::string>&
        direct_from_seller_per_buyer_signals_header_ad_slot,
    const std::optional<GURL>& direct_from_seller_auction_signals,
    const std::optional<std::string>&
        direct_from_seller_auction_signals_header_ad_slot,
    const std::string& seller_signals_json,
    auction_worklet::mojom::KAnonymityBidMode kanon_mode,
    bool bid_is_kanon,
    const GURL& browser_signal_render_url,
    double browser_signal_bid,
    const std::optional<blink::AdCurrency>& browser_signal_bid_currency,
    double browser_signal_highest_scoring_other_bid,
    const std::optional<blink::AdCurrency>&
        browser_signal_highest_scoring_other_bid_currency,
    bool browser_signal_made_highest_scoring_other_bid,
    std::optional<double> browser_signal_ad_cost,
    std::optional<uint16_t> browser_signal_modeling_signals,
    uint8_t browser_signal_join_count,
    uint8_t browser_signal_recency,
    const url::Origin& browser_signal_seller_origin,
    const std::optional<url::Origin>& browser_signal_top_level_seller_origin,
    const std::optional<base::TimeDelta> browser_signal_reporting_timeout,
    std::optional<uint32_t> bidding_signals_data_version,
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
    mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent,
    uint32_t thread_index) {
  ADD_FAILURE()
      << "ConnectDevToolsAgent should not be called on MockBidderWorklet";
}

void MockBidderWorklet::FinishGenerateBid(
    const std::optional<std::string>& auction_signals_json,
    const std::optional<std::string>& per_buyer_signals_json,
    const std::optional<base::TimeDelta> per_buyer_timeout,
    const std::optional<blink::AdCurrency>& per_buyer_currency,
    const std::optional<GURL>& direct_from_seller_per_buyer_signals,
    const std::optional<std::string>&
        direct_from_seller_per_buyer_signals_header_ad_slot,
    const std::optional<GURL>& direct_from_seller_auction_signals,
    const std::optional<std::string>&
        direct_from_seller_auction_signals_header_ad_slot) {
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
  if (generate_bid_clients_.empty()) {
    generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
    generate_bid_run_loop_->Run();
    generate_bid_run_loop_.reset();
    DCHECK(!generate_bid_clients_.empty());
  }
}

void MockBidderWorklet::SetBidderTrustedSignalsFetchLatency(
    base::TimeDelta delta) {
  trusted_signals_fetch_latency_ = delta;
}

void MockBidderWorklet::SetBiddingLatency(base::TimeDelta delta) {
  bidding_latency_ = delta;
}

void MockBidderWorklet::SetCodeFetchLatencies(
    std::optional<base::TimeDelta> js_fetch_latency,
    std::optional<base::TimeDelta> wasm_fetch_latency) {
  js_fetch_latency_ = js_fetch_latency;
  wasm_fetch_latency_ = wasm_fetch_latency;
}

void MockBidderWorklet::InvokeGenerateBidCallback(
    std::optional<double> bid,
    const std::optional<blink::AdCurrency>& bid_currency,
    const blink::AdDescriptor& ad_descriptor,
    auction_worklet::mojom::BidRole bid_role,
    std::vector<auction_worklet::mojom::BidderWorkletBidPtr> further_bids,
    std::optional<std::vector<blink::AdDescriptor>> ad_component_descriptors,
    base::TimeDelta duration,
    const std::optional<uint32_t>& bidding_signals_data_version,
    const std::optional<GURL>& debug_loss_report_url,
    const std::optional<GURL>& debug_win_report_url,
    std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>
        pa_requests,
    std::vector<auction_worklet::mojom::RealTimeReportingContributionPtr>
        real_time_contributions,
    auction_worklet::mojom::GenerateBidDependencyLatenciesPtr
        dependency_latencies,
    auction_worklet::mojom::RejectReason reject_reason) {
  WaitForGenerateBid();

  mojo::AssociatedRemote<auction_worklet::mojom::GenerateBidClient>
      generate_bid_client = std::move(generate_bid_clients_.front());
  generate_bid_clients_.pop_front();

  base::RunLoop run_loop;
  generate_bid_client->OnBiddingSignalsReceived(
      /*priority_vector=*/{},
      /*trusted_signals_fetch_latency=*/trusted_signals_fetch_latency_,
      /*update_if_older_than=*/std::nullopt, run_loop.QuitClosure());
  run_loop.Run();

  if (!dependency_latencies) {
    dependency_latencies =
        auction_worklet::mojom::GenerateBidDependencyLatencies::New(
            /*code_ready_latency=*/std::nullopt,
            /*config_promises_latency=*/std::nullopt,
            /*direct_from_seller_signals_latency=*/std::nullopt,
            /*trusted_bidding_signals_latency=*/std::nullopt,
            /*deps_wait_start_time=*/base::TimeTicks::Now(),
            /*generate_bid_start_time=*/base::TimeTicks::Now(),
            /*generate_bid_finish_time=*/base::TimeTicks::Now());
  }

  std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>
      non_kanon_pa_requests;
  for (const auto& request : non_kanon_pa_requests_) {
    non_kanon_pa_requests.push_back(request->Clone());
  }

  auto bid_metrics = auction_worklet::mojom::BidderTimingMetrics::New(
      /*js_fetch_latency=*/js_fetch_latency_,
      /*wasm_fetch_latency=*/wasm_fetch_latency_,
      /*script_latency=*/bidding_latency_,
      /*script_timed_out=*/script_timed_out_);

  std::vector<auction_worklet::mojom::BidderWorkletBidPtr> bids;
  if (!bid.has_value()) {
    DCHECK(further_bids.empty());
    generate_bid_client->OnGenerateBidComplete(
        /*bids=*/std::move(bids),
        /*bidding_signals_data_version=*/std::nullopt, debug_loss_report_url,
        /*debug_win_report_url=*/std::nullopt,
        /*set_priority=*/std::nullopt,
        /*update_priority_signals_overrides=*/
        base::flat_map<std::string,
                       auction_worklet::mojom::PrioritySignalsDoublePtr>(),
        /*pa_requests=*/std::move(pa_requests),
        /*non_kanon_pa_requests=*/std::move(non_kanon_pa_requests),
        /*real_time_contributions=*/{},
        /*generate_bid_metrics=*/std::move(bid_metrics),
        /*generate_bid_dependency_latencies=*/std::move(dependency_latencies),
        reject_reason,
        /*errors=*/std::vector<std::string>());
    return;
  }

  bids.push_back(auction_worklet::mojom::BidderWorkletBid::New(
      bid_role, "ad", *bid, bid_currency, /*ad_cost=*/std::nullopt,
      std::move(ad_descriptor),
      selected_buyer_and_seller_reporting_id_, ad_component_descriptors,
      /*modeling_signals=*/std::nullopt, duration));
  bids.insert(bids.end(), std::make_move_iterator(further_bids.begin()),
              std::make_move_iterator(further_bids.end()));

  generate_bid_client->OnGenerateBidComplete(
      std::move(bids), bidding_signals_data_version, debug_loss_report_url,
      debug_win_report_url,
      /*set_priority=*/std::nullopt,
      /*update_priority_signals_overrides=*/
      base::flat_map<std::string,
                     auction_worklet::mojom::PrioritySignalsDoublePtr>(),
      /*pa_requests=*/std::move(pa_requests),
      /*non_kanon_pa_requests=*/std::move(non_kanon_pa_requests),
      /*real_time_contributions=*/std::move(real_time_contributions),
      /*generated_bid_metrics=*/std::move(bid_metrics),
      /*generate_bid_dependency_latencies=*/std::move(dependency_latencies),
      reject_reason,
      /*errors=*/std::vector<std::string>());
}

void MockBidderWorklet::WaitForReportWin() {
  DCHECK(generate_bid_clients_.empty());
  DCHECK(!report_win_run_loop_);
  if (!report_win_callback_) {
    report_win_run_loop_ = std::make_unique<base::RunLoop>();
    report_win_run_loop_->Run();
    report_win_run_loop_.reset();
    DCHECK(report_win_callback_);
  }
}

void MockBidderWorklet::InvokeReportWinCallback(
    std::optional<GURL> report_url,
    base::flat_map<std::string, GURL> ad_beacon_map,
    base::flat_map<std::string, std::string> ad_macro_map,
    std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>
        pa_requests,
    std::vector<std::string> errors) {
  DCHECK(report_win_callback_);
  std::move(report_win_callback_)
      .Run(report_url, std::move(ad_beacon_map), std::move(ad_macro_map),
           std::move(pa_requests),
           auction_worklet::mojom::BidderTimingMetrics::New(
               /*js_fetch_latency=*/js_fetch_latency_,
               /*wasm_fetch_latency=*/wasm_fetch_latency_,
               /*script_latency=*/reporting_latency_,
               /*script_timed_out=*/script_timed_out_),
           std::move(errors));
}

void MockBidderWorklet::Flush() {
  receiver_.FlushForTesting();
}

bool MockBidderWorklet::PipeIsClosed() {
  receiver_.FlushForTesting();
  return pipe_closed_;
}

void MockBidderWorklet::SetSelectedBuyerAndSellerReportingId(
    std::optional<std::string> selected) {
  selected_buyer_and_seller_reporting_id_ = std::move(selected);
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
    const std::optional<blink::AdCurrency>& bid_currency,
    const blink::AuctionConfig::NonSharedParams&
        auction_ad_config_non_shared_params,
    const std::optional<GURL>& direct_from_seller_seller_signals,
    const std::optional<std::string>&
        direct_from_seller_seller_signals_header_ad_slot,
    const std::optional<GURL>& direct_from_seller_auction_signals,
    const std::optional<std::string>&
        direct_from_seller_auction_signals_header_ad_slot,
    auction_worklet::mojom::ComponentAuctionOtherSellerPtr
        browser_signals_other_seller,
    const std::optional<blink::AdCurrency>& component_expect_bid_currency,
    const url::Origin& browser_signal_interest_group_owner,
    const GURL& browser_signal_render_url,
    const std::optional<std::string>&
        browser_signal_selected_buyer_and_seller_reporting_id,
    const std::optional<std::string>&
        browser_signal_buyer_and_seller_reporting_id,
    const std::vector<GURL>& browser_signal_ad_components,
    uint32_t browser_signal_bidding_duration_msecs,
    const std::optional<blink::AdSize>& browser_signal_render_size,
    bool browser_signal_for_debugging_only_in_cooldown_or_lockout,
    const std::optional<base::TimeDelta> seller_timeout,
    uint64_t trace_id,
    const url::Origin& bidder_joining_origin,
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
    const std::optional<GURL>& direct_from_seller_seller_signals,
    const std::optional<std::string>&
        direct_from_seller_seller_signals_header_ad_slot,
    const std::optional<GURL>& direct_from_seller_auction_signals,
    const std::optional<std::string>&
        direct_from_seller_auction_signals_header_ad_slot,
    auction_worklet::mojom::ComponentAuctionOtherSellerPtr
        browser_signals_other_seller,
    const url::Origin& browser_signal_interest_group_owner,
    const std::optional<std::string>&
        browser_signal_buyer_and_seller_reporting_id,
    const std::optional<std::string>&
        browser_signal_selected_buyer_and_seller_reporting_id,
    const GURL& browser_signal_render_url,
    double browser_signal_bid,
    const std::optional<blink::AdCurrency>& browser_signal_bid_currency,
    double browser_signal_desirability,
    double browser_signal_highest_scoring_other_bid,
    const std::optional<blink::AdCurrency>&
        browser_signal_highest_scoring_other_bid_currency,
    auction_worklet::mojom::ComponentAuctionReportResultParamsPtr
        browser_signals_component_auction_report_result_params,
    std::optional<uint32_t> browser_signal_data_version,
    uint64_t trace_id,
    ReportResultCallback report_result_callback) {
  report_result_callback_ = std::move(report_result_callback);
  if (report_result_run_loop_) {
    report_result_run_loop_->Quit();
  }
}

void MockSellerWorklet::ConnectDevToolsAgent(
    mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent,
    uint32_t thread_index) {
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
    std::optional<GURL> report_url,
    base::flat_map<std::string, GURL> ad_beacon_map,
    std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>
        pa_requests,
    std::vector<std::string> errors) {
  DCHECK(report_result_callback_);
  std::move(report_result_callback_)
      .Run(/*signals_for_winner=*/std::nullopt, std::move(report_url),
           ad_beacon_map, std::move(pa_requests),
           auction_worklet::mojom::SellerTimingMetrics::New(
               /*js_fetch_latency=*/js_fetch_latency_,
               /*script_latency=*/reporting_latency_,
               /*script_timed_out=*/script_timed_out_),
           errors);
}

void MockSellerWorklet::Flush() {
  receiver_.FlushForTesting();
}

MockAuctionProcessManager::MockAuctionProcessManager() = default;
MockAuctionProcessManager::~MockAuctionProcessManager() = default;

scoped_refptr<AuctionProcessManager::WorkletProcess>
MockAuctionProcessManager::LaunchProcess(const ProcessHandle* process_handle,
                                         const std::string& display_name) {
  mojo::PendingRemote<auction_worklet::mojom::AuctionWorkletService> service;
  mojo::ReceiverId receiver_id =
      receiver_set_.Add(this, service.InitWithNewPipeAndPassReceiver());

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
  return base::MakeRefCounted<WorkletProcess>(
      this, /*render_process_host=*/nullptr, std::move(service),
      process_handle->worklet_type(), process_handle->origin(),
      /*uses_shared_process=*/false);
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
    std::vector<
        mojo::PendingRemote<auction_worklet::mojom::AuctionSharedStorageHost>>
        shared_storage_hosts,
    bool pause_for_debugger_on_start,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_url_loader_factory,
    mojo::PendingRemote<auction_worklet::mojom::AuctionNetworkEventsHandler>
        auction_network_events_handler,
    const GURL& script_source_url,
    const std::optional<GURL>& bidding_wasm_helper_url,
    const std::optional<GURL>& trusted_bidding_signals_url,
    const std::string& trusted_bidding_signals_slot_size_param,
    const url::Origin& top_window_origin,
    auction_worklet::mojom::AuctionWorkletPermissionsPolicyStatePtr
        permissions_policy_state,
    std::optional<uint16_t> experiment_group_id,
    auction_worklet::mojom::TrustedSignalsPublicKeyPtr public_key) {
  load_bidder_worklet_count_++;
  last_load_bidder_worklet_threads_count_ = shared_storage_hosts.size();

  // Make sure this request came over the right pipe.
  url::Origin owner = url::Origin::Create(script_source_url);
  EXPECT_EQ(receiver_display_name_map_[receiver_set_.current_receiver()],
            ComputeDisplayName(AuctionProcessManager::WorkletType::kBidder,
                               url::Origin::Create(script_source_url)));

  EXPECT_EQ(0u, bidder_worklets_.count(script_source_url));
  bidder_worklets_.emplace(
      script_source_url, std::make_unique<MockBidderWorklet>(
                             std::move(bidder_worklet_receiver),
                             expected_per_buyer_timeouts_, skip_generate_bid_));
  MaybeQuitWaitForWorkletsRunLoop();
}

void MockAuctionProcessManager::LoadSellerWorklet(
    mojo::PendingReceiver<auction_worklet::mojom::SellerWorklet>
        seller_worklet_receiver,
    std::vector<
        mojo::PendingRemote<auction_worklet::mojom::AuctionSharedStorageHost>>
        shared_storage_hosts,
    bool should_pause_on_start,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_url_loader_factory,
    mojo::PendingRemote<auction_worklet::mojom::AuctionNetworkEventsHandler>
        auction_network_events_handler,
    const GURL& script_source_url,
    const std::optional<GURL>& trusted_scoring_signals_url,
    const url::Origin& top_window_origin,
    auction_worklet::mojom::AuctionWorkletPermissionsPolicyStatePtr
        permissions_policy_state,
    std::optional<uint16_t> experiment_group_id,
    auction_worklet::mojom::TrustedSignalsPublicKeyPtr public_key) {
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
