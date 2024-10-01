// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_MOCK_AUCTION_PROCESS_MANAGER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_MOCK_AUCTION_PROCESS_MANAGER_H_

#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "content/browser/interest_group/auction_process_manager.h"
#include "content/public/browser/site_instance.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom-forward.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/real_time_reporting.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/common/interest_group/ad_auction_currencies.h"
#include "third_party/blink/public/common/interest_group/ad_display_size.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class ProcessHandle;

// This file contains an AuctionProcessManager that creates mock worklets
// that tests can control.

// BidderWorklet that holds onto passed in callbacks, to let the test invoke
// them.
class MockBidderWorklet : public auction_worklet::mojom::BidderWorklet,
                          auction_worklet::mojom::GenerateBidFinalizer {
 public:
  MockBidderWorklet(
      mojo::PendingReceiver<auction_worklet::mojom::BidderWorklet>
          pending_receiver,
      const std::map<std::string, base::TimeDelta>& expected_per_buyer_timeouts,
      bool skip_generate_bid);

  MockBidderWorklet(const MockBidderWorklet&) = delete;
  const MockBidderWorklet& operator=(const MockBidderWorklet&) = delete;

  ~MockBidderWorklet() override;

  // auction_worklet::mojom::BidderWorklet implementation:
  void BeginGenerateBid(
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
          auction_worklet::mojom::GenerateBidFinalizer> bid_finalizer) override;
  void SendPendingSignalsRequests() override;
  void ReportWin(
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
      ReportWinCallback report_win_callback) override;
  void ConnectDevToolsAgent(
      mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent,
      uint32_t thread_index) override;

  // mojom::GenerateBidFinalizer implementation.
  void FinishGenerateBid(
      const std::optional<std::string>& auction_signals_json,
      const std::optional<std::string>& per_buyer_signals_json,
      const std::optional<base::TimeDelta> per_buyer_timeout,
      const std::optional<blink::AdCurrency>& per_buyer_currency,
      const std::optional<GURL>& direct_from_seller_per_buyer_signals,
      const std::optional<std::string>&
          direct_from_seller_per_buyer_signals_header_ad_slot,
      const std::optional<GURL>& direct_from_seller_auction_signals,
      const std::optional<std::string>&
          direct_from_seller_auction_signals_header_ad_slot) override;

  // Waits for GenerateBid() to be invoked.
  void WaitForGenerateBid();

  // The below functions alter `trusted_signals_fetch_latency` (from
  // OnBiddingSignalsReceived()) and `bidding_latency` (from
  // OnGenerateBidComplete()), respectively, to return `delta`.
  void SetBidderTrustedSignalsFetchLatency(base::TimeDelta delta);
  void SetBiddingLatency(base::TimeDelta delta);
  void SetCodeFetchLatencies(std::optional<base::TimeDelta> js_fetch_latency,
                             std::optional<base::TimeDelta> wasm_fetch_latency);
  void SetScriptTimedOut(bool val) { script_timed_out_ = val; }

  // Same for `reporting_latency` for ReportWin()
  void SetReportingLatency(base::TimeDelta delta) {
    reporting_latency_ = delta;
  }

  // Controls what's passed to `non_kanon_pa_requests` of the generate bid
  // callback.
  void SetNonKAnonPARequests(
      std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>
          non_kanon_pa_requests) {
    non_kanon_pa_requests_ = std::move(non_kanon_pa_requests);
  }

  // Invokes the GenerateBid callback. A bid of base::nullopt means no bid
  // should be offered. Waits for the GenerateBid() call first, if needed.
  void InvokeGenerateBidCallback(
      std::optional<double> bid,
      const std::optional<blink::AdCurrency>& bid_currency = std::nullopt,
      const blink::AdDescriptor& ad_descriptor = blink::AdDescriptor(),
      auction_worklet::mojom::BidRole bid_role =
          auction_worklet::mojom::BidRole::kUnenforcedKAnon,
      std::vector<auction_worklet::mojom::BidderWorkletBidPtr> further_bids =
          std::vector<auction_worklet::mojom::BidderWorkletBidPtr>(),
      std::optional<std::vector<blink::AdDescriptor>> ad_component_descriptors =
          std::nullopt,
      base::TimeDelta duration = base::TimeDelta(),
      const std::optional<uint32_t>& bidding_signals_data_version =
          std::nullopt,
      const std::optional<GURL>& debug_loss_report_url = std::nullopt,
      const std::optional<GURL>& debug_win_report_url = std::nullopt,
      std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>
          pa_requests = {},
      std::vector<auction_worklet::mojom::RealTimeReportingContributionPtr>
          real_time_contributions = {},
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
      std::optional<GURL> report_url = std::nullopt,
      base::flat_map<std::string, GURL> ad_beacon_map = {},
      base::flat_map<std::string, std::string> ad_macro_map = {},
      std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>
          pa_requests = {},
      std::vector<std::string> errors = {});

  // Flushes the receiver pipe.
  void Flush();

  // Flush the receiver pipe and return whether or not its closed.
  bool PipeIsClosed();

  void SetSelectedBuyerAndSellerReportingId(
      std::optional<std::string> selected);

 private:
  void OnPipeClosed() { pipe_closed_ = true; }

  std::list<mojo::AssociatedRemote<auction_worklet::mojom::GenerateBidClient>>
      generate_bid_clients_;
  mojo::AssociatedReceiverSet<auction_worklet::mojom::GenerateBidFinalizer,
                              base::TimeDelta>
      finalizer_receiver_set_;

  bool pipe_closed_ = false;

  std::optional<std::string> selected_buyer_and_seller_reporting_id_;

  std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>
      non_kanon_pa_requests_;

  std::unique_ptr<base::RunLoop> generate_bid_run_loop_;
  std::unique_ptr<base::RunLoop> report_win_run_loop_;
  ReportWinCallback report_win_callback_;

  // If true, bypass the `BeginGenerateBid()` function. This class does not
  // support testing multiple calls to `BeginGenerateBid()`. This flag is useful
  // for testing other auction functions while disabling that specific code
  // path.
  bool skip_generate_bid_ = false;

  bool generate_bid_called_ = false;
  bool send_pending_signals_requests_called_ = false;

  // Expected per-bidder timeout values, indexed by interest group name.
  std::map<std::string, base::TimeDelta> expected_per_buyer_timeouts_;

  // To be fed as `trusted_signals_fetch_latency` (from
  // OnBiddingSignalsReceived()).
  base::TimeDelta trusted_signals_fetch_latency_;

  // These are fed as part of BidderTimingMetrics. Except for
  // `bidding_latency_` and `reporting_latency_` they are used both for
  // generateBid and reportWin.
  base::TimeDelta bidding_latency_;
  base::TimeDelta reporting_latency_;
  std::optional<base::TimeDelta> js_fetch_latency_;
  std::optional<base::TimeDelta> wasm_fetch_latency_;
  bool script_timed_out_ = false;

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
          score_ad_client) override;
  void SendPendingSignalsRequests() override;
  void ReportResult(
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
      ReportResultCallback report_result_callback) override;
  void ConnectDevToolsAgent(
      mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent,
      uint32_t thread_index) override;

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
      std::optional<GURL> report_url = std::nullopt,
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

  void SetCodeFetchLatency(std::optional<base::TimeDelta> js_fetch_latency) {
    js_fetch_latency_ = js_fetch_latency;
  }

  void SetScriptTimedOut(bool val) { script_timed_out_ = val; }

 private:
  std::unique_ptr<base::RunLoop> score_ad_run_loop_;
  std::list<ScoreAdParams> score_ad_params_;

  std::unique_ptr<base::RunLoop> report_result_run_loop_;
  ReportResultCallback report_result_callback_;

  bool expect_send_pending_signals_requests_called_ = true;
  bool send_pending_signals_requests_called_ = false;

  // To be fed as `reporting_latency` to ReportResult() callback.
  base::TimeDelta reporting_latency_;

  // Used for reporting callback as well.
  std::optional<base::TimeDelta> js_fetch_latency_;
  bool script_timed_out_ = false;

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
  scoped_refptr<WorkletProcess> LaunchProcess(
      const ProcessHandle* process_handle,
      const std::string& display_name) override;
  scoped_refptr<SiteInstance> MaybeComputeSiteInstance(
      SiteInstance* frame_site_instance,
      const url::Origin& worklet_origin) override;
  bool TryUseSharedProcess(ProcessHandle* process_handle) override;

  // auction_worklet::mojom::AuctionWorkletService implementation:
  void LoadBidderWorklet(
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
      auction_worklet::mojom::TrustedSignalsPublicKeyPtr public_key) override;
  void LoadSellerWorklet(
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
      auction_worklet::mojom::TrustedSignalsPublicKeyPtr public_key) override;

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

  void SetSkipGenerateBid() { skip_generate_bid_ = true; }

  size_t load_bidder_worklet_count() const {
    return load_bidder_worklet_count_;
  }

  size_t last_load_bidder_worklet_threads_count() const {
    return last_load_bidder_worklet_threads_count_;
  }

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

  // If true, configure the bidder worklets to bypass the `BeginGenerateBid()`
  // function. The `MockBidderWorklet` does not support testing multiple calls
  // to `BeginGenerateBid()`. This flag is useful for testing other auction
  // functions while disabling that specific code path.
  bool skip_generate_bid_ = false;

  size_t load_bidder_worklet_count_ = 0;
  size_t last_load_bidder_worklet_threads_count_ = 0;

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
