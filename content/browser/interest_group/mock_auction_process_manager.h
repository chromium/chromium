// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_MOCK_AUCTION_PROCESS_MANAGER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_MOCK_AUCTION_PROCESS_MANAGER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "content/browser/interest_group/auction_process_manager.h"
#include "content/public/browser/site_instance.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/interest_group/ad_auction_currencies.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class RenderProcessHost;
class ProcessHandle;

// This file contains an AuctionProcessManager that creates mock worklets
// that tests can control.

// BidderWorklet that holds onto passed in callbacks, to let the test invoke
// them.
class MockBidderWorklet : public auction_worklet::mojom::BidderWorklet,
                          auction_worklet::mojom::GenerateBidFinalizer {
 public:
  MockBidderWorklet(mojo::PendingReceiver<auction_worklet::mojom::BidderWorklet>
                        pending_receiver,
                    const std::map<std::string, base::TimeDelta>&
                        expected_per_buyer_timeouts);

  MockBidderWorklet(const MockBidderWorklet&) = delete;
  const MockBidderWorklet& operator=(const MockBidderWorklet&) = delete;

  ~MockBidderWorklet() override;

  // auction_worklet::mojom::BidderWorklet implementation:
  void BeginGenerateBid(
      auction_worklet::mojom::BidderWorkletNonSharedParamsPtr
          bidder_worklet_non_shared_params,
      auction_worklet::mojom::KAnonymityBidMode kanon_mode,
      const url::Origin& interest_group_join_origin,
      const absl::optional<GURL>& direct_from_seller_per_buyer_signals,
      const absl::optional<GURL>& direct_from_seller_auction_signals,
      const url::Origin& browser_signal_seller_origin,
      const absl::optional<url::Origin>& browser_signal_top_level_seller_origin,
      const base::TimeDelta browser_signal_recency,
      auction_worklet::mojom::BiddingBrowserSignalsPtr bidding_browser_signals,
      base::Time auction_start_time,
      uint64_t trace_id,
      mojo::PendingAssociatedRemote<auction_worklet::mojom::GenerateBidClient>
          generate_bid_client,
      mojo::PendingAssociatedReceiver<
          auction_worklet::mojom::GenerateBidFinalizer> bid_finalizer) override;
  void SendPendingSignalsRequests() override;
  void ReportWin(
      auction_worklet::mojom::ReportingIdField reporting_id_field,
      const std::string& reporting_id,
      const absl::optional<std::string>& auction_signals_json,
      const absl::optional<std::string>& per_buyer_signals_json,
      const absl::optional<GURL>& direct_from_seller_per_buyer_signals,
      const absl::optional<GURL>& direct_from_seller_auction_signals,
      const std::string& seller_signals_json,
      auction_worklet::mojom::KAnonymityBidMode kanon_mode,
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
      ReportWinCallback report_win_callback) override;
  void ConnectDevToolsAgent(
      mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent)
      override;

  // mojom::GenerateBidFinalizer implementation.
  void FinishGenerateBid(
      const absl::optional<std::string>& auction_signals_json,
      const absl::optional<std::string>& per_buyer_signals_json,
      const absl::optional<base::TimeDelta> per_buyer_timeout,
      const absl::optional<blink::AdCurrency>& per_buyer_currency,
      const absl::optional<GURL>& direct_from_seller_per_buyer_signals,
      const absl::optional<GURL>& direct_from_seller_auction_signals) override;

  // Waits for GenerateBid() to be invoked.
  void WaitForGenerateBid();

  // The below functions alter `trusted_signals_fetch_latency` (from
  // OnBiddingSignalsReceived()) and `bidding_latency` (from
  // OnGenerateBidComplete()), respectively, to return `delta`.
  void SetBidderTrustedSignalsFetchLatency(base::TimeDelta delta);
  void SetBiddingLatency(base::TimeDelta delta);

  // Same for `reporting_latency` for ReportWin()
  void SetReportingLatency(base::TimeDelta delta) {
    reporting_latency_ = delta;
  }

  // Invokes the GenerateBid callback. A bid of base::nullopt means no bid
  // should be offered. Waits for the GenerateBid() call first, if needed.
  void InvokeGenerateBidCallback(
      absl::optional<double> bid,
      const absl::optional<blink::AdCurrency>& bid_currency = absl::nullopt,
      const blink::AdDescriptor& ad_descriptor = blink::AdDescriptor(),
      auction_worklet::mojom::BidderWorkletKAnonEnforcedBidPtr mojo_kanon_bid =
          auction_worklet::mojom::BidderWorkletKAnonEnforcedBidPtr(),
      absl::optional<std::vector<blink::AdDescriptor>>
          ad_component_descriptors = absl::nullopt,
      base::TimeDelta duration = base::TimeDelta(),
      const absl::optional<uint32_t>& bidding_signals_data_version =
          absl::nullopt,
      const absl::optional<GURL>& debug_loss_report_url = absl::nullopt,
      const absl::optional<GURL>& debug_win_report_url = absl::nullopt,
      std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>
          pa_requests = {},
      auction_worklet::mojom::GenerateBidDependencyLatenciesPtr
          dependency_latencies =
              auction_worklet::mojom::GenerateBidDependencyLatenciesPtr(),
      auction_worklet::mojom::RejectReason reject_reason =
          auction_worklet::mojom::RejectReason::kNotAvailable);

  // Waits for ReportWin() to be invoked.
  void WaitForReportWin();

  // Invokes the ReportWin() callback with the provided parameters. Does not
  // wait for ReportWin() to be invoked.
  void InvokeReportWinCallback(
      absl::optional<GURL> report_url = absl::nullopt,
      base::flat_map<std::string, GURL> ad_beacon_map = {},
      std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>
          pa_requests = {},
      std::vector<std::string> errors = {});

  // Flushes the receiver pipe.
  void Flush();

  // Flush the receiver pipe and return whether or not its closed.
  bool PipeIsClosed();

 private:
  void OnPipeClosed() { pipe_closed_ = true; }

  mojo::AssociatedRemote<auction_worklet::mojom::GenerateBidClient>
      generate_bid_client_;
  mojo::AssociatedReceiverSet<auction_worklet::mojom::GenerateBidFinalizer,
                              base::TimeDelta>
      finalizer_receiver_set_;

  bool pipe_closed_ = false;

  std::unique_ptr<base::RunLoop> generate_bid_run_loop_;
  std::unique_ptr<base::RunLoop> report_win_run_loop_;
  ReportWinCallback report_win_callback_;

  bool generate_bid_called_ = false;
  bool send_pending_signals_requests_called_ = false;

  // Expected per-bidder timeout values, indexed by interest group name.
  std::map<std::string, base::TimeDelta> expected_per_buyer_timeouts_;

  // To be fed as `trusted_signals_fetch_latency` (from
  // OnBiddingSignalsReceived()) and `bidding_latency` (from
  // OnGenerateBidComplete()), respectively,
  base::TimeDelta trusted_signals_fetch_latency_;
  base::TimeDelta bidding_latency_;

  // To be fed as `reporting_latency` to ReportWin() callback.
  base::TimeDelta reporting_latency_;

  // Receiver is last so that destroying `this` while there's a pending callback
  // over the pipe will not DCHECK.
  mojo::Receiver<auction_worklet::mojom::BidderWorklet> receiver_;
};

// SellerWorklet that holds onto passed in callbacks, to let the test invoke
// them.
class MockSellerWorklet : public auction_worklet::mojom::SellerWorklet {
 public:
  // Subset of parameters passed to SellerWorklet's ScoreAd method.
  struct ScoreAdParams {
    ScoreAdParams();
    ScoreAdParams(ScoreAdParams&&);
    ~ScoreAdParams();

    ScoreAdParams& operator=(ScoreAdParams&&);

    mojo::PendingRemote<auction_worklet::mojom::ScoreAdClient> score_ad_client;
    double bid;
    url::Origin interest_group_owner;
  };

  explicit MockSellerWorklet(
      mojo::PendingReceiver<auction_worklet::mojom::SellerWorklet>
          pending_receiver);

  MockSellerWorklet(const MockSellerWorklet&) = delete;
  const MockSellerWorklet& operator=(const MockSellerWorklet&) = delete;

  ~MockSellerWorklet() override;

  // auction_worklet::mojom::SellerWorklet implementation:
  void ScoreAd(
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
          score_ad_client) override;
  void SendPendingSignalsRequests() override;
  void ReportResult(
      const blink::AuctionConfig::NonSharedParams&
          auction_ad_config_non_shared_params,
      const absl::optional<GURL>& direct_from_seller_seller_signals,
      const absl::optional<GURL>& direct_from_seller_auction_signals,
      auction_worklet::mojom::ComponentAuctionOtherSellerPtr
          browser_signals_other_seller,
      const url::Origin& browser_signal_interest_group_owner,
      const absl::optional<std::string>&
          browser_signal_buyer_and_seller_reporting_id,
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
      ReportResultCallback report_result_callback) override;
  void ConnectDevToolsAgent(
      mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent)
      override;

  // Closes the receiver pipe with the provided reason.
  void ResetReceiverWithReason(const std::string& reason);

  // Waits until ScoreAd() has been invoked, if it hasn't been already. It's up
  // to the caller to invoke the returned ScoreAdParams::callback to continue
  // the auction.
  ScoreAdParams WaitForScoreAd();

  // Waits until ReportResult() has been invoked, if it hasn't been already.
  void WaitForReportResult();

  // Configures `reporting_latency` passed to ReportResult by
  // InvokeReportResultCallback.
  void SetReportingLatency(base::TimeDelta delta) {
    reporting_latency_ = delta;
  }

  // Invokes the ReportResultCallback for the most recent ScoreAd() call with
  // the provided score. WaitForReportResult() must have been invoked first.
  void InvokeReportResultCallback(
      absl::optional<GURL> report_url = absl::nullopt,
      base::flat_map<std::string, GURL> ad_beacon_map = {},
      std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>
          pa_requests = {},
      std::vector<std::string> errors = {});

  // Flushes the receiver pipe.
  void Flush();

  // `expect_send_pending_signals_requests_called_` needs to be set to false in
  // the case a SellerWorklet is destroyed before it receives a request to score
  // the final bid.
  void set_expect_send_pending_signals_requests_called(bool value) {
    expect_send_pending_signals_requests_called_ = value;
  }

 private:
  std::unique_ptr<base::RunLoop> score_ad_run_loop_;
  std::list<ScoreAdParams> score_ad_params_;

  std::unique_ptr<base::RunLoop> report_result_run_loop_;
  ReportResultCallback report_result_callback_;

  bool expect_send_pending_signals_requests_called_ = true;
  bool send_pending_signals_requests_called_ = false;

  // To be fed as `reporting_latency` to ReportResult() callback.
  base::TimeDelta reporting_latency_;

  // Receiver is last so that destroying `this` while there's a pending callback
  // over the pipe will not DCHECK.
  mojo::Receiver<auction_worklet::mojom::SellerWorklet> receiver_;
};

// AuctionWorkletService that creates MockBidderWorklets and MockSellerWorklets
// to hold onto passed in PendingReceivers and Callbacks.
//
// MockAuctionProcessManager implements AuctionProcessManager and
// AuctionWorkletService - combining the two with a mojo::ReceiverSet makes it
// easier to track which call came over which receiver than using separate
// classes.
class MockAuctionProcessManager
    : public AuctionProcessManager,
      public auction_worklet::mojom::AuctionWorkletService {
 public:
  MockAuctionProcessManager();
  ~MockAuctionProcessManager() override;

  // AuctionProcessManager implementation:
  RenderProcessHost* LaunchProcess(
      mojo::PendingReceiver<auction_worklet::mojom::AuctionWorkletService>
          auction_worklet_service_receiver,
      const ProcessHandle* handle,
      const std::string& display_name) override;
  scoped_refptr<SiteInstance> MaybeComputeSiteInstance(
      SiteInstance* frame_site_instance,
      const url::Origin& worklet_origin) override;
  bool TryUseSharedProcess(ProcessHandle* process_handle) override;

  // auction_worklet::mojom::AuctionWorkletService implementation:
  void LoadBidderWorklet(
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
      uint16_t experiment_group_id) override;
  void LoadSellerWorklet(
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
      uint16_t experiment_group_id) override;

  // Set the expected timeout for an interest group with the specified name,
  // when it's received by a bidder worklet's FinishGenerateBid() method. Must
  // be set for all bidders that are given a chance to bid.
  void SetExpectedBuyerBidTimeout(const std::string& name,
                                  base::TimeDelta value);

  // Waits for `num_bidders` bidder worklets and `num_sellers` seller worklets
  // to be created.
  void WaitForWorklets(int num_bidders, int num_sellers = 1);

  // Waits for a single script to be loaded of the specified type. Intended to
  // be used to wait for the winning bidder or seller script to be reloaded.
  // WaitForWorklets() should be used when waiting for worklets to be loaded at
  // the start of an auction.
  void WaitForWinningBidderReload();
  void WaitForWinningSellerReload();

  // Returns the MockBidderWorklet created for the specified script URL, if
  // there is one.
  std::unique_ptr<MockBidderWorklet> TakeBidderWorklet(
      const GURL& script_source_url);

  // Returns the MockSellerWorklet created for the specified script URL, if
  // there is one. If no URL is provided, and there's only one pending seller
  // worklet, returns that seller worklet.
  std::unique_ptr<MockSellerWorklet> TakeSellerWorklet(
      GURL script_source_url = GURL());

  // Flushes the receiver set.
  void Flush();

 private:
  void MaybeQuitWaitForWorkletsRunLoop();

  // Expected per-bidder timeout values, indexed by interest group name. Applied
  // across all bidder origins.
  std::map<std::string, base::TimeDelta> expected_per_buyer_timeouts_;

  // Maps of script URLs to worklets.
  std::map<GURL, std::unique_ptr<MockBidderWorklet>> bidder_worklets_;
  std::map<GURL, std::unique_ptr<MockSellerWorklet>> seller_worklets_;

  // Used to wait for the worklets to be loaded.
  std::unique_ptr<base::RunLoop> wait_for_worklets_run_loop_;

  // Number of seller and bidder worklets that `wait_for_worklets_run_loop_` is
  // waiting for. These are compared to the size of the worklet maps above. Note
  // that those only include cumulative worklets not claimed by
  // TakeBidderWorklet() and TakeSellerWorklet()
  // - once a worklet has been claimed by the consumer, it no longer counts
  // towards these totals.
  size_t waiting_for_num_bidders_ = 0;
  size_t waiting_for_num_sellers_ = 0;

  // Map from ReceiverSet IDs to display name when the process was launched.
  // Used to verify that worklets are created in the right process.
  std::map<mojo::ReceiverId, std::string> receiver_display_name_map_;

  // ReceiverSet is last so that destroying `this` while there's a pending
  // callback over the pipe will not DCHECK.
  mojo::ReceiverSet<auction_worklet::mojom::AuctionWorkletService>
      receiver_set_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_MOCK_AUCTION_PROCESS_MANAGER_H_
