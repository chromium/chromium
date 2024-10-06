// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_worklet_manager.h"

#include <stdint.h>

#include <array>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "content/browser/interest_group/auction_metrics_recorder.h"
#include "content/browser/interest_group/auction_process_manager.h"
#include "content/browser/interest_group/subresource_url_authorizations.h"
#include "content/browser/interest_group/subresource_url_builder.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/services/auction_worklet/public/mojom/auction_network_events_handler.mojom.h"
#include "content/services/auction_worklet/public/mojom/auction_shared_storage_host.mojom.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/functions.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/interest_group/ad_display_size.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "url/gurl.h"
#include "url/origin.h"

using testing::UnorderedElementsAre;

namespace content {
class ProcessHandleTestPeer {
 public:
  explicit ProcessHandleTestPeer(
      const AuctionProcessManager::ProcessHandle* handle)
      : handle_(handle) {}

  void CallOnLaunchedWithPid() {
    handle_->OnBaseProcessLaunchedForTesting(base::Process::Current());
  }

  std::unique_ptr<AuctionProcessManager::ProcessHandle> CloneHandle(
      AuctionProcessManager& auction_process_manager) {
    auto new_handle = std::make_unique<AuctionProcessManager::ProcessHandle>();
    base::test::TestFuture<void> process_available;
    if (!auction_process_manager.RequestWorkletService(
            handle_->worklet_type_, handle_->origin_,
            /*frame_site_instance=*/nullptr, new_handle.get(),
            process_available.GetCallback())) {
      CHECK(process_available.Wait());
    }
    return new_handle;
  }

 private:
  raw_ptr<const AuctionProcessManager::ProcessHandle> handle_;
};

namespace {

const char kAuction1[] = "a";
const char kAuction2[] = "b";
const char kAuction3[] = "c";
const char kAuction4[] = "d";
const char kAuction5[] = "e";

using BundleSubresourceInfo = SubresourceUrlBuilder::BundleSubresourceInfo;

constexpr char kBuyer1OriginStr[] = "https://origin.test";
constexpr char kBuyer2OriginStr[] = "https://origin2.test";

AuctionWorkletManager::FatalErrorCallback NeverInvokedFatalErrorCallback() {
  return base::BindOnce(
      [](AuctionWorkletManager::FatalErrorType fatal_error_type,
         const std::vector<std::string>& errors) {
        ADD_FAILURE() << "This should not be called";
      });
}

blink::DirectFromSellerSignalsSubresource CreateSubresource(
    const GURL& bundle_url) {
  blink::DirectFromSellerSignalsSubresource subresource;
  subresource.bundle_url = bundle_url;
  return subresource;
}

blink::DirectFromSellerSignals PopulateSubresources() {
  blink::DirectFromSellerSignals result;

  const url::Origin kBuyer1Origin = url::Origin::Create(GURL(kBuyer1OriginStr));
  const url::Origin kBuyer2Origin = url::Origin::Create(GURL(kBuyer2OriginStr));

  const GURL bundle_url1 = GURL("https://seller.test/bundle1");
  const GURL bundle_url2 = GURL("https://seller.test/bundle2");

  result.prefix = GURL("https://seller.test/signals");

  result.per_buyer_signals[kBuyer1Origin] = CreateSubresource(bundle_url1);
  result.per_buyer_signals[kBuyer2Origin] = CreateSubresource(bundle_url2);

  blink::DirectFromSellerSignalsSubresource seller_signals =
      CreateSubresource(bundle_url1);
  result.seller_signals = seller_signals;

  blink::DirectFromSellerSignalsSubresource auction_signals =
      CreateSubresource(bundle_url2);
  result.auction_signals = auction_signals;

  return result;
}

bool PublicKeyEvaluateHelper(
    const auction_worklet::mojom::TrustedSignalsPublicKey* public_key,
    base::expected<BiddingAndAuctionServerKey, std::string> expected_key) {
  if (expected_key.has_value() && public_key) {
    return expected_key->id == public_key->id &&
           expected_key->key == public_key->key;
  } else if (!expected_key.has_value() && !public_key) {
    return true;
  } else {
    return false;
  }
}

// Single-use helper for waiting for a load error and inspecting its data.
class FatalLoadErrorHelper {
 public:
  FatalLoadErrorHelper() = default;
  ~FatalLoadErrorHelper() = default;

  AuctionWorkletManager::FatalErrorCallback Callback() {
    return base::BindOnce(&FatalLoadErrorHelper::OnFatalError,
                          base::Unretained(this));
  }

  void WaitForResult() { run_loop_.Run(); }

  AuctionWorkletManager::FatalErrorType fatal_error_type() const {
    return fatal_error_type_;
  }

  const std::vector<std::string>& errors() const { return errors_; }

 private:
  void OnFatalError(AuctionWorkletManager::FatalErrorType fatal_error_type,
                    const std::vector<std::string>& errors) {
    EXPECT_FALSE(run_loop_.AnyQuitCalled());

    fatal_error_type_ = fatal_error_type;
    errors_ = std::move(errors);
    run_loop_.Quit();
  }

  AuctionWorkletManager::FatalErrorType fatal_error_type_;
  std::vector<std::string> errors_;

  // For use by FatalErrorCallback only.
  base::RunLoop run_loop_;
};

// BidderWorklet that holds onto passed in callbacks, to let the test fixture
// invoke them.
class MockBidderWorklet : public auction_worklet::mojom::BidderWorklet {
 public:
  explicit MockBidderWorklet(
      mojo::PendingReceiver<auction_worklet::mojom::BidderWorklet>
          pending_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_url_loader_factory,
      const GURL& script_source_url,
      const std::optional<GURL>& wasm_url,
      const std::optional<GURL>& trusted_bidding_signals_url,
      const url::Origin& top_window_origin,
      auction_worklet::mojom::TrustedSignalsPublicKeyPtr public_key,
      bool enable_dtor_pending_signals_check)
      : url_loader_factory_(std::move(pending_url_loader_factory)),
        script_source_url_(script_source_url),
        wasm_url_(wasm_url),
        public_key_(std::move(public_key)),
        trusted_bidding_signals_url_(trusted_bidding_signals_url),
        top_window_origin_(top_window_origin),
        enable_dtor_pending_signals_check_(enable_dtor_pending_signals_check),
        receiver_(this, std::move(pending_receiver)) {}

  MockBidderWorklet(const MockBidderWorklet&) = delete;
  const MockBidderWorklet& operator=(const MockBidderWorklet&) = delete;

  ~MockBidderWorklet() override {
    if (enable_dtor_pending_signals_check_) {
      // Process any pending SendPendingSignalsRequests() calls.
      base::RunLoop().RunUntilIdle();
      EXPECT_EQ(expected_num_send_pending_signals_requests_calls_,
                num_send_pending_signals_requests_calls_);
    }
  }

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
          auction_worklet::mojom::GenerateBidFinalizer> bid_finalizer)
      override {
    NOTREACHED_IN_MIGRATION();
  }

  void SendPendingSignalsRequests() override {
    ++num_send_pending_signals_requests_calls_;
    if (num_send_pending_signals_requests_calls_ ==
        expected_num_send_pending_signals_requests_calls_) {
      send_pending_signals_requests_called_loop_->Quit();
    }
  }

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
      ReportWinCallback report_win_callback) override {
    NOTREACHED_IN_MIGRATION();
  }

  void ConnectDevToolsAgent(
      mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent,
      uint32_t thread_index) override {
    ADD_FAILURE()
        << "ConnectDevToolsAgent should not be called on MockBidderWorklet";
  }

  void ClosePipe(const char* error) {
    DCHECK(error);
    receiver_.ResetWithReason(/*custom_reason_code=*/0, error);
  }

  void WaitForSendPendingSignalsRequests(
      int expected_num_send_pending_signals_requests_calls) {
    DCHECK_LT(expected_num_send_pending_signals_requests_calls_,
              expected_num_send_pending_signals_requests_calls);

    expected_num_send_pending_signals_requests_calls_ =
        expected_num_send_pending_signals_requests_calls;
    if (num_send_pending_signals_requests_calls_ <
        expected_num_send_pending_signals_requests_calls_) {
      send_pending_signals_requests_called_loop_ =
          std::make_unique<base::RunLoop>();
      send_pending_signals_requests_called_loop_->Run();
    }

    EXPECT_EQ(expected_num_send_pending_signals_requests_calls_,
              num_send_pending_signals_requests_calls_);
  }

  mojo::Remote<network::mojom::URLLoaderFactory>& url_loader_factory() {
    return url_loader_factory_;
  }

  const GURL& script_source_url() const { return script_source_url_; }
  const std::optional<GURL>& wasm_url() const { return wasm_url_; }
  const auction_worklet::mojom::TrustedSignalsPublicKey* public_key() const {
    return public_key_.get();
  }
  const std::optional<GURL>& trusted_bidding_signals_url() const {
    return trusted_bidding_signals_url_;
  }
  const url::Origin& top_window_origin() const { return top_window_origin_; }

  int num_send_pending_signals_requests_calls() const {
    return num_send_pending_signals_requests_calls_;
  }

 private:
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;

  const GURL script_source_url_;
  const std::optional<GURL> wasm_url_;
  const auction_worklet::mojom::TrustedSignalsPublicKeyPtr public_key_;
  const std::optional<GURL> trusted_bidding_signals_url_;
  const url::Origin top_window_origin_;
  const bool enable_dtor_pending_signals_check_;

  // Number of times SendPendingSignalsRequests() has been invoked. Used to
  // check that calls through Mojo BidderWorklet interfaces make it to the
  // correct MockBidderWorklet.
  int num_send_pending_signals_requests_calls_ = 0;
  // Number of SendPendingSignalsRequests() to wait for. Once this is hit,
  // `send_pending_signals_requests_called_loop_` is invoked. Must match
  // num_send_pending_signals_requests_calls_ on destruction (which catches
  // unexpected extra calls).
  int expected_num_send_pending_signals_requests_calls_ = 0;
  std::unique_ptr<base::RunLoop> send_pending_signals_requests_called_loop_;

  // Receiver is last so that destroying `this` while there's a pending callback
  // over the pipe will not DCHECK.
  mojo::Receiver<auction_worklet::mojom::BidderWorklet> receiver_;
};

// SellerWorklet that holds onto passed in callbacks, to let the test fixture
// invoke them.
class MockSellerWorklet : public auction_worklet::mojom::SellerWorklet {
 public:
  explicit MockSellerWorklet(
      mojo::PendingReceiver<auction_worklet::mojom::SellerWorklet>
          pending_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_url_loader_factory,
      const GURL& script_source_url,
      const std::optional<GURL>& trusted_scoring_signals_url,
      const url::Origin& top_window_origin,
      auction_worklet::mojom::TrustedSignalsPublicKeyPtr public_key)
      : url_loader_factory_(std::move(pending_url_loader_factory)),
        script_source_url_(script_source_url),
        trusted_scoring_signals_url_(trusted_scoring_signals_url),
        top_window_origin_(top_window_origin),
        public_key_(std::move(public_key)),
        receiver_(this, std::move(pending_receiver)) {}

  MockSellerWorklet(const MockSellerWorklet&) = delete;
  const MockSellerWorklet& operator=(const MockSellerWorklet&) = delete;

  ~MockSellerWorklet() override {
    // Process any pending SendPendingSignalsRequests() calls.
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(expected_num_send_pending_signals_requests_calls_,
              num_send_pending_signals_requests_calls_);
  }

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
          score_ad_client) override {
    NOTREACHED_IN_MIGRATION();
  }

  void SendPendingSignalsRequests() override {
    ++num_send_pending_signals_requests_calls_;
    if (num_send_pending_signals_requests_calls_ ==
        expected_num_send_pending_signals_requests_calls_) {
      send_pending_signals_requests_called_loop_->Quit();
    }
  }

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
      ReportResultCallback report_result_callback) override {
    NOTREACHED_IN_MIGRATION();
  }

  void ConnectDevToolsAgent(
      mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent,
      uint32_t thread_index) override {
    ADD_FAILURE()
        << "ConnectDevToolsAgent should not be called on MockSellerWorklet";
  }

  void ClosePipe(const char* error) {
    DCHECK(error);
    receiver_.ResetWithReason(/*custom_reason_code=*/0, error);
  }

  void WaitForSendPendingSignalsRequests(
      int expected_num_send_pending_signals_requests_calls) {
    DCHECK_LT(expected_num_send_pending_signals_requests_calls_,
              expected_num_send_pending_signals_requests_calls);

    expected_num_send_pending_signals_requests_calls_ =
        expected_num_send_pending_signals_requests_calls;
    if (num_send_pending_signals_requests_calls_ <
        expected_num_send_pending_signals_requests_calls_) {
      send_pending_signals_requests_called_loop_ =
          std::make_unique<base::RunLoop>();
      send_pending_signals_requests_called_loop_->Run();
    }

    EXPECT_EQ(expected_num_send_pending_signals_requests_calls_,
              num_send_pending_signals_requests_calls_);
  }

  mojo::Remote<network::mojom::URLLoaderFactory>& url_loader_factory() {
    return url_loader_factory_;
  }

  const GURL& script_source_url() const { return script_source_url_; }
  const std::optional<GURL>& trusted_scoring_signals_url() const {
    return trusted_scoring_signals_url_;
  }
  const url::Origin& top_window_origin() const { return top_window_origin_; }

  const auction_worklet::mojom::TrustedSignalsPublicKey* public_key() const {
    return public_key_.get();
  }

  int num_send_pending_signals_requests_calls() const {
    return num_send_pending_signals_requests_calls_;
  }

 private:
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;

  const GURL script_source_url_;
  const std::optional<GURL> trusted_scoring_signals_url_;
  const url::Origin top_window_origin_;
  const auction_worklet::mojom::TrustedSignalsPublicKeyPtr public_key_;

  // Number of times SendPendingSignalsRequests() has been invoked. Used to
  // check that calls through Mojo SellerWorklet interfaces make it to the
  // correct MockSellerWorklet.
  int num_send_pending_signals_requests_calls_ = 0;
  // Number of SendPendingSignalsRequests() to wait for. Once this is hit,
  // `send_pending_signals_requests_called_loop_` is invoked. Must match
  // num_send_pending_signals_requests_calls_ on destruction (which catches
  // unexpected extra calls).
  int expected_num_send_pending_signals_requests_calls_ = 0;
  std::unique_ptr<base::RunLoop> send_pending_signals_requests_called_loop_;

  // Receiver is last so that destroying `this` while there's a pending callback
  // over the pipe will not DCHECK.
  mojo::Receiver<auction_worklet::mojom::SellerWorklet> receiver_;
};

// AuctionProcessManager and AuctionWorkletService - combining the two with a
// mojo::ReceiverSet makes it easier to track which call came over which
// receiver than using separate classes.
class MockAuctionProcessManager
    : public AuctionProcessManager,
      public auction_worklet::mojom::AuctionWorkletService {
 public:
  MockAuctionProcessManager() = default;
  ~MockAuctionProcessManager() override = default;

  // AuctionProcessManager implementation:
  scoped_refptr<AuctionProcessManager::WorkletProcess> LaunchProcess(
      const ProcessHandle* process_handle,
      const std::string& display_name) override {
    mojo::PendingReceiver<auction_worklet::mojom::AuctionWorkletService>
        pending_receiver;
    auto worklet_process = base::MakeRefCounted<WorkletProcess>(
        this, /*render_process_host=*/nullptr,
        pending_receiver.InitWithNewPipeAndPassRemote(),
        process_handle->worklet_type(), process_handle->origin(),
        /*uses_shared_process=*/false);
    mojo::ReceiverId receiver_id =
        receiver_set_.Add(this, std::move(pending_receiver));

    // Have to flush the receiver set, so that any closed receivers are removed,
    // before searching for duplicate process names.
    receiver_set_.FlushForTesting();

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
    return worklet_process;
  }

  void OnNewProcessAssigned(const ProcessHandle* handle) override {
    if (defer_on_launched_for_handles_) {
      deferred_on_launch_call_handles_.push_back(
          ProcessHandleTestPeer(handle).CloneHandle(*this));
    } else {
      ProcessHandleTestPeer(handle).CallOnLaunchedWithPid();
    }
  }

  void DeferOnLaunchedForHandles() { defer_on_launched_for_handles_ = true; }

  void CallOnLaunchedWithPidForAllHandles() {
    for (auto& handle : deferred_on_launch_call_handles_) {
      ProcessHandleTestPeer(handle.get()).CallOnLaunchedWithPid();
    }
    deferred_on_launch_call_handles_.clear();
  }

  scoped_refptr<SiteInstance> MaybeComputeSiteInstance(
      SiteInstance* frame_site_instance,
      const url::Origin& worklet_origin) override {
    return nullptr;
  }

  bool TryUseSharedProcess(ProcessHandle* process_handle) override {
    return false;
  }

  void DisableBidderWorkletDtorPendingSignalsCheck() {
    enable_bidder_worklet_dtor_pending_signals_check_ = false;
  }

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
      auction_worklet::mojom::TrustedSignalsPublicKeyPtr public_key) override {
    DCHECK(!bidder_worklet_);

    // Make sure this request came over the right pipe.
    EXPECT_EQ(receiver_display_name_map_[receiver_set_.current_receiver()],
              ComputeDisplayName(AuctionProcessManager::WorkletType::kBidder,
                                 url::Origin::Create(script_source_url)));

    bidder_worklet_ = std::make_unique<MockBidderWorklet>(
        std::move(bidder_worklet_receiver),
        std::move(pending_url_loader_factory), script_source_url,
        bidding_wasm_helper_url, trusted_bidding_signals_url, top_window_origin,
        std::move(public_key),
        enable_bidder_worklet_dtor_pending_signals_check_);

    if (bidder_worklet_run_loop_) {
      bidder_worklet_run_loop_->Quit();
    }
  }

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
      auction_worklet::mojom::TrustedSignalsPublicKeyPtr public_key) override {
    DCHECK(!seller_worklet_);

    // Make sure this request came over the right pipe.
    EXPECT_EQ(receiver_display_name_map_[receiver_set_.current_receiver()],
              ComputeDisplayName(AuctionProcessManager::WorkletType::kSeller,
                                 url::Origin::Create(script_source_url)));

    seller_worklet_ = std::make_unique<MockSellerWorklet>(
        std::move(seller_worklet_receiver),
        std::move(pending_url_loader_factory), script_source_url,
        trusted_scoring_signals_url, top_window_origin, std::move(public_key));

    if (seller_worklet_run_loop_) {
      seller_worklet_run_loop_->Quit();
    }
  }

  std::unique_ptr<MockBidderWorklet> WaitForBidderWorklet() {
    if (!bidder_worklet_) {
      bidder_worklet_run_loop_ = std::make_unique<base::RunLoop>();
      bidder_worklet_run_loop_->Run();
    }

    DCHECK(bidder_worklet_);
    return std::move(bidder_worklet_);
  }

  bool HasBidderWorkletRequest() {
    base::RunLoop().RunUntilIdle();
    return bidder_worklet_.get() != nullptr;
  }

  std::unique_ptr<MockSellerWorklet> WaitForSellerWorklet() {
    if (!seller_worklet_) {
      seller_worklet_run_loop_ = std::make_unique<base::RunLoop>();
      seller_worklet_run_loop_->Run();
    }

    DCHECK(seller_worklet_);
    return std::move(seller_worklet_);
  }

  bool HasSellerWorkletRequest() const {
    base::RunLoop().RunUntilIdle();
    return seller_worklet_.get() != nullptr;
  }

 private:
  // The most recently created unclaimed bidder worklet.
  std::unique_ptr<MockBidderWorklet> bidder_worklet_;
  std::unique_ptr<base::RunLoop> bidder_worklet_run_loop_;

  // The most recently created unclaimed seller worklet.
  std::unique_ptr<MockSellerWorklet> seller_worklet_;
  std::unique_ptr<base::RunLoop> seller_worklet_run_loop_;

  // If true, MockBidderWorklet destructor will check to make sure the
  // proper number of trusted signals requests were received.
  bool enable_bidder_worklet_dtor_pending_signals_check_ = true;

  // Map from ReceiverSet IDs to display name when the process was launched.
  // Used to verify that worklets are created in the right process.
  std::map<mojo::ReceiverId, std::string> receiver_display_name_map_;

  // ReceiverSet is last so that destroying `this` while there's a pending
  // callback over the pipe will not DCHECK.
  mojo::ReceiverSet<auction_worklet::mojom::AuctionWorkletService>
      receiver_set_;

  bool defer_on_launched_for_handles_ = false;
  std::vector<std::unique_ptr<AuctionProcessManager::ProcessHandle>>
      deferred_on_launch_call_handles_;
};

class AuctionWorkletManagerTest : public RenderViewHostTestHarness,
                                  public AuctionWorkletManager::Delegate {
 public:
  AuctionWorkletManagerTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    mojo::SetDefaultProcessErrorHandler(base::BindRepeating(
        &AuctionWorkletManagerTest::OnBadMessage, base::Unretained(this)));
  }

  ~AuctionWorkletManagerTest() override {
    // Any bad message should have been inspected and cleared before the end of
    // the test.
    EXPECT_EQ(std::string(), bad_message_);
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
  }

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    auction_metrics_recorder_manager_ =
        std::make_unique<AuctionMetricsRecorderManager>(
            ukm::AssignNewSourceId());
    auction_worklet_manager_ = std::make_unique<AuctionWorkletManager>(
        &auction_process_manager_, kTopWindowOrigin, kFrameOrigin, this);
  }

  void TearDown() override {
    auction_worklet_manager_.reset();
    RenderViewHostTestHarness::TearDown();
  }

  // AuctionWorkletManager::Delegate implementation:
  network::mojom::URLLoaderFactory* GetFrameURLLoaderFactory() override {
    return &url_loader_factory_;
  }
  network::mojom::URLLoaderFactory* GetTrustedURLLoaderFactory() override {
    return &url_loader_factory_;
  }
  void PreconnectSocket(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key) override {}
  RenderFrameHostImpl* GetFrame() override {
    return static_cast<RenderFrameHostImpl*>(
        web_contents()->GetPrimaryMainFrame());
  }
  scoped_refptr<SiteInstance> GetFrameSiteInstance() override {
    return scoped_refptr<SiteInstance>();
  }
  network::mojom::ClientSecurityStatePtr GetClientSecurityState() override {
    return network::mojom::ClientSecurityState::New();
  }
  std::optional<std::string> GetCookieDeprecationLabel() override {
    return std::nullopt;
  }
  void GetBiddingAndAuctionServerKey(
      const std::optional<url::Origin>& coordinator,
      base::OnceCallback<void(base::expected<BiddingAndAuctionServerKey,
                                             std::string>)> callback) override {
    // Not implemented because this method is not called in this test.
    NOTREACHED();
  }

 protected:
  void OnBadMessage(const std::string& reason) {
    // No test expects multiple bad messages at a time
    EXPECT_EQ(std::string(), bad_message_);
    // Empty bad messages aren't expected. This check allows an empty
    // `bad_message_` field to mean no bad message, avoiding using an optional,
    // which has less helpful output on EXPECT failures.
    EXPECT_FALSE(reason.empty());

    bad_message_ = reason;
  }

  // Gets and clear most recent bad Mojo message.
  std::string TakeBadMessage() { return std::move(bad_message_); }

  const url::Origin kTopWindowOrigin =
      url::Origin::Create(GURL("https://top.window.origin.test/"));
  // Frame origin is passed in buth otherwise ignored by these tests - it's only
  // used by the DevTools hooks, which only have integration tests.
  const url::Origin kFrameOrigin =
      url::Origin::Create(GURL("https://frame.origin.test/"));

  // Defaults used by most tests.
  const GURL kDecisionLogicUrl = GURL("https://origin.test/script");
  const GURL kWasmUrl = GURL("https://origin.test/wasm");
  const GURL kTrustedSignalsUrl = GURL("https://origin.test/trusted_signals");
  const SubresourceUrlBuilder kEmptySubresourceBuilder{std::nullopt};
  const SubresourceUrlBuilder kPopulatedSubresourceBuilder{
      PopulateSubresources()};

  std::string bad_message_;

  network::TestURLLoaderFactory url_loader_factory_;
  MockAuctionProcessManager auction_process_manager_;
  std::unique_ptr<AuctionMetricsRecorderManager>
      auction_metrics_recorder_manager_;
  std::unique_ptr<AuctionWorkletManager> auction_worklet_manager_;
};

TEST_F(AuctionWorkletManagerTest, SingleBidderWorklet) {
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
  base::test::TestFuture<void> worklet_available;
  auction_worklet_manager_->RequestBidderWorklet(
      kAuction1, kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
      /*needs_cors_for_additional_bid=*/false,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_bidding_signals_coordinator=*/std::nullopt,
      worklet_available.GetCallback(), NeverInvokedFatalErrorCallback(), handle,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available.Wait());
  EXPECT_TRUE(handle->GetBidderWorklet());
  EXPECT_THAT(handle->GetDevtoolsAuctionIdsForTesting(),
              UnorderedElementsAre(kAuction1));
  handle->AuthorizeSubresourceUrls(kPopulatedSubresourceBuilder);

  std::unique_ptr<MockBidderWorklet> bidder_worklet =
      auction_process_manager_.WaitForBidderWorklet();
  EXPECT_EQ(kDecisionLogicUrl, bidder_worklet->script_source_url());
  EXPECT_EQ(kWasmUrl, bidder_worklet->wasm_url());
  EXPECT_EQ(kTrustedSignalsUrl, bidder_worklet->trusted_bidding_signals_url());
  EXPECT_EQ(kTopWindowOrigin, bidder_worklet->top_window_origin());

  EXPECT_EQ(0, bidder_worklet->num_send_pending_signals_requests_calls());
  handle->GetBidderWorklet()->SendPendingSignalsRequests();
  bidder_worklet->WaitForSendPendingSignalsRequests(1);

  const url::Origin kBuyer1Origin = url::Origin::Create(GURL(kBuyer1OriginStr));
  const url::Origin kBuyer2Origin = url::Origin::Create(GURL(kBuyer2OriginStr));
  const SubresourceUrlBuilder::BundleSubresourceInfo expected_buyer1_full_info =
      kPopulatedSubresourceBuilder.per_buyer_signals().at(kBuyer1Origin);
  const SubresourceUrlBuilder::BundleSubresourceInfo expected_buyer2_full_info =
      kPopulatedSubresourceBuilder.per_buyer_signals().at(kBuyer2Origin);
  EXPECT_EQ(
      nullptr,
      handle->GetSubresourceUrlAuthorizationsForTesting().GetAuthorizationInfo(
          kPopulatedSubresourceBuilder.seller_signals()->subresource_url));
  EXPECT_EQ(
      kPopulatedSubresourceBuilder.auction_signals(),
      *handle->GetSubresourceUrlAuthorizationsForTesting().GetAuthorizationInfo(
          kPopulatedSubresourceBuilder.auction_signals()->subresource_url));
  EXPECT_EQ(
      expected_buyer1_full_info,
      *handle->GetSubresourceUrlAuthorizationsForTesting().GetAuthorizationInfo(
          expected_buyer1_full_info.subresource_url));
  EXPECT_EQ(
      nullptr,
      handle->GetSubresourceUrlAuthorizationsForTesting().GetAuthorizationInfo(
          expected_buyer2_full_info.subresource_url));
}

TEST_F(AuctionWorkletManagerTest, SingleSellerWorklet) {
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
  base::test::TestFuture<void> worklet_available;
  auction_worklet_manager_->RequestSellerWorklet(
      kAuction1, kDecisionLogicUrl, kTrustedSignalsUrl,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_scoring_signals_coordinator=*/std::nullopt,
      worklet_available.GetCallback(), NeverInvokedFatalErrorCallback(), handle,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available.Wait());
  EXPECT_TRUE(handle->GetSellerWorklet());
  EXPECT_THAT(handle->GetDevtoolsAuctionIdsForTesting(),
              UnorderedElementsAre(kAuction1));
  handle->AuthorizeSubresourceUrls(kPopulatedSubresourceBuilder);

  std::unique_ptr<MockSellerWorklet> seller_worklet =
      auction_process_manager_.WaitForSellerWorklet();
  EXPECT_EQ(kDecisionLogicUrl, seller_worklet->script_source_url());
  EXPECT_EQ(kTrustedSignalsUrl, seller_worklet->trusted_scoring_signals_url());
  EXPECT_EQ(kTopWindowOrigin, seller_worklet->top_window_origin());

  EXPECT_EQ(0, seller_worklet->num_send_pending_signals_requests_calls());
  handle->GetSellerWorklet()->SendPendingSignalsRequests();
  seller_worklet->WaitForSendPendingSignalsRequests(1);

  const url::Origin kBuyer1Origin = url::Origin::Create(GURL(kBuyer1OriginStr));
  const url::Origin kBuyer2Origin = url::Origin::Create(GURL(kBuyer2OriginStr));
  const SubresourceUrlBuilder::BundleSubresourceInfo expected_buyer1_full_info =
      kPopulatedSubresourceBuilder.per_buyer_signals().at(kBuyer1Origin);
  const SubresourceUrlBuilder::BundleSubresourceInfo expected_buyer2_full_info =
      kPopulatedSubresourceBuilder.per_buyer_signals().at(kBuyer2Origin);
  EXPECT_EQ(
      kPopulatedSubresourceBuilder.seller_signals(),
      *handle->GetSubresourceUrlAuthorizationsForTesting().GetAuthorizationInfo(
          kPopulatedSubresourceBuilder.seller_signals()->subresource_url));
  EXPECT_EQ(
      kPopulatedSubresourceBuilder.auction_signals(),
      *handle->GetSubresourceUrlAuthorizationsForTesting().GetAuthorizationInfo(
          kPopulatedSubresourceBuilder.auction_signals()->subresource_url));
  EXPECT_EQ(
      nullptr,
      handle->GetSubresourceUrlAuthorizationsForTesting().GetAuthorizationInfo(
          expected_buyer1_full_info.subresource_url));
  EXPECT_EQ(
      nullptr,
      handle->GetSubresourceUrlAuthorizationsForTesting().GetAuthorizationInfo(
          expected_buyer2_full_info.subresource_url));
}

TEST_F(AuctionWorkletManagerTest,
       SingleBidderWorkletEmptyDirectFromSellerSignals) {
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
  base::test::TestFuture<void> worklet_available;
  auction_worklet_manager_->RequestBidderWorklet(
      kAuction1, kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
      /*needs_cors_for_additional_bid=*/false,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_bidding_signals_coordinator=*/std::nullopt,
      worklet_available.GetCallback(), NeverInvokedFatalErrorCallback(), handle,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available.Wait());
  EXPECT_TRUE(handle->GetBidderWorklet());
  handle->AuthorizeSubresourceUrls(kEmptySubresourceBuilder);

  std::unique_ptr<MockBidderWorklet> bidder_worklet =
      auction_process_manager_.WaitForBidderWorklet();

  EXPECT_TRUE(
      handle->GetSubresourceUrlAuthorizationsForTesting().IsEmptyForTesting());
}

TEST_F(AuctionWorkletManagerTest,
       SingleSellerWorkletEmptyDirectFromSellerSignals) {
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
  base::test::TestFuture<void> worklet_available;
  auction_worklet_manager_->RequestSellerWorklet(
      kAuction1, kDecisionLogicUrl, kTrustedSignalsUrl,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_scoring_signals_coordinator=*/std::nullopt,
      worklet_available.GetCallback(), NeverInvokedFatalErrorCallback(), handle,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available.Wait());
  EXPECT_TRUE(handle->GetSellerWorklet());
  handle->AuthorizeSubresourceUrls(kEmptySubresourceBuilder);

  std::unique_ptr<MockSellerWorklet> seller_worklet =
      auction_process_manager_.WaitForSellerWorklet();

  EXPECT_TRUE(
      handle->GetSubresourceUrlAuthorizationsForTesting().IsEmptyForTesting());
}

// Test the case where a process assignment completes asynchronously. This
// only happens when the BidderWorklet process limit has been reached. This test
// also serves to make sure that different bidder origins result in different
// processes.
TEST_F(AuctionWorkletManagerTest, BidderWorkletAsync) {
  // Create `kMaxBidderProcesses` for origins other than https://origin.test.
  //
  // For proper destruction ordering, `handles` should be after
  // `bidder_worklets`. Otherwise, worklet destruction will result in invoking
  // the `handles` fatal error callback, as if they had crashed.
  std::list<std::unique_ptr<MockBidderWorklet>> bidder_worklets;
  std::list<std::unique_ptr<AuctionWorkletManager::WorkletHandle>> handles;
  for (size_t i = 0; i < AuctionProcessManager::kMaxBidderProcesses; ++i) {
    EXPECT_EQ(i, auction_process_manager_.GetBidderProcessCountForTesting());

    GURL decision_logic_url =
        GURL(base::StringPrintf("https://origin%zu.test", i));
    std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
    base::test::TestFuture<void> worklet_available;
    auction_worklet_manager_->RequestBidderWorklet(
        kAuction1, decision_logic_url, /*wasm_url=*/std::nullopt,
        /*trusted_bidding_signals_url=*/std::nullopt,
        /*needs_cors_for_additional_bid=*/false,
        /*experiment_group_id=*/std::nullopt,
        /*trusted_bidding_signals_slot_size_param=*/"",
        /*trusted_bidding_signals_coordinator=*/std::nullopt,
        worklet_available.GetCallback(), NeverInvokedFatalErrorCallback(),
        handle,
        auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
    ASSERT_TRUE(worklet_available.Wait());
    EXPECT_TRUE(handle->GetBidderWorklet());
    EXPECT_EQ(i + 1,
              auction_process_manager_.GetBidderProcessCountForTesting());

    std::unique_ptr<MockBidderWorklet> bidder_worklet =
        auction_process_manager_.WaitForBidderWorklet();
    EXPECT_EQ(decision_logic_url, bidder_worklet->script_source_url());
    EXPECT_EQ(std::nullopt, bidder_worklet->wasm_url());
    EXPECT_EQ(std::nullopt, bidder_worklet->trusted_bidding_signals_url());
    EXPECT_EQ(kTopWindowOrigin, bidder_worklet->top_window_origin());

    EXPECT_EQ(0, bidder_worklet->num_send_pending_signals_requests_calls());
    handle->GetBidderWorklet()->SendPendingSignalsRequests();
    bidder_worklet->WaitForSendPendingSignalsRequests(1);

    handles.emplace_back(std::move(handle));
    bidder_worklets.emplace_back(std::move(bidder_worklet));
  }

  // Should be at the bidder process limit.
  EXPECT_EQ(AuctionProcessManager::kMaxBidderProcesses,
            auction_process_manager_.GetBidderProcessCountForTesting());

  // The next request for a distinct bidder worklet should not be able to
  // complete for now, since there's no available process quota.
  base::test::TestFuture<void> worklet_available2;
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
  auction_worklet_manager_->RequestBidderWorklet(
      kAuction1, kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
      /*needs_cors_for_additional_bid=*/false,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_bidding_signals_coordinator=*/std::nullopt,
      worklet_available2.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  EXPECT_EQ(AuctionProcessManager::kMaxBidderProcesses,
            auction_process_manager_.GetBidderProcessCountForTesting());
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(worklet_available2.IsReady());

  // Freeing a WorkletHandle should result in a new process being
  // available, and the most recent request getting a new worklet.

  handles.pop_front();
  ASSERT_TRUE(worklet_available2.Wait());
  EXPECT_TRUE(handle->GetBidderWorklet());

  std::unique_ptr<MockBidderWorklet> bidder_worklet =
      auction_process_manager_.WaitForBidderWorklet();
  EXPECT_EQ(kDecisionLogicUrl, bidder_worklet->script_source_url());
  EXPECT_EQ(kWasmUrl, bidder_worklet->wasm_url());
  EXPECT_EQ(kTrustedSignalsUrl, bidder_worklet->trusted_bidding_signals_url());
  EXPECT_EQ(kTopWindowOrigin, bidder_worklet->top_window_origin());

  EXPECT_EQ(0, bidder_worklet->num_send_pending_signals_requests_calls());
  handle->GetBidderWorklet()->SendPendingSignalsRequests();
  bidder_worklet->WaitForSendPendingSignalsRequests(1);

  // Should still be at the process limit.
  EXPECT_EQ(AuctionProcessManager::kMaxBidderProcesses,
            auction_process_manager_.GetBidderProcessCountForTesting());
}

// Test the case where  a process assignment completes asynchronously. This
// only happens when the SellerWorklet process limit has been reached. This test
// also serves to make sure that different seller origins result in different
// processes.
TEST_F(AuctionWorkletManagerTest, SellerWorkletAsync) {
  // Create `kMaxSellerProcesses` for origins other than https://origin.test.
  //
  // For proper destruction ordering, `handles` should be after
  // `seller_worklets`. Otherwise, worklet destruction will result in invoking
  // the `handles` fatal error callback, as if they had crashed.
  std::list<std::unique_ptr<MockSellerWorklet>> seller_worklets;
  std::list<std::unique_ptr<AuctionWorkletManager::WorkletHandle>> handles;
  for (size_t i = 0; i < AuctionProcessManager::kMaxSellerProcesses; ++i) {
    EXPECT_EQ(i, auction_process_manager_.GetSellerProcessCountForTesting());

    GURL decision_logic_url =
        GURL(base::StringPrintf("https://origin%zu.test", i));
    std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
    base::test::TestFuture<void> worklet_available;
    auction_worklet_manager_->RequestSellerWorklet(
        kAuction1, decision_logic_url,
        /*trusted_scoring_signals_url=*/std::nullopt,
        /*experiment_group_id=*/std::nullopt,
        /*trusted_scoring_signals_coordinator=*/std::nullopt,
        worklet_available.GetCallback(), NeverInvokedFatalErrorCallback(),
        handle,
        auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
    ASSERT_TRUE(worklet_available.Wait());
    EXPECT_TRUE(handle->GetSellerWorklet());
    EXPECT_EQ(i + 1,
              auction_process_manager_.GetSellerProcessCountForTesting());

    std::unique_ptr<MockSellerWorklet> seller_worklet =
        auction_process_manager_.WaitForSellerWorklet();
    EXPECT_EQ(decision_logic_url, seller_worklet->script_source_url());
    EXPECT_EQ(std::nullopt, seller_worklet->trusted_scoring_signals_url());
    EXPECT_EQ(kTopWindowOrigin, seller_worklet->top_window_origin());

    EXPECT_EQ(0, seller_worklet->num_send_pending_signals_requests_calls());
    handle->GetSellerWorklet()->SendPendingSignalsRequests();
    seller_worklet->WaitForSendPendingSignalsRequests(1);

    handles.emplace_back(std::move(handle));
    seller_worklets.emplace_back(std::move(seller_worklet));
  }

  // Should be at the seller process limit.
  EXPECT_EQ(AuctionProcessManager::kMaxSellerProcesses,
            auction_process_manager_.GetSellerProcessCountForTesting());

  // The next request for a distinct seller worklet should not be able to
  // complete for now, since there's no available process quota.
  base::RunLoop worklet_available_loop;
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
  auction_worklet_manager_->RequestSellerWorklet(
      kAuction1, kDecisionLogicUrl, kTrustedSignalsUrl,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_scoring_signals_coordinator=*/std::nullopt,
      worklet_available_loop.QuitClosure(), NeverInvokedFatalErrorCallback(),
      handle,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  EXPECT_EQ(AuctionProcessManager::kMaxSellerProcesses,
            auction_process_manager_.GetSellerProcessCountForTesting());
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(worklet_available_loop.AnyQuitCalled());

  // Freeing a WorkletHandle should result in a new process being
  // available, and the most recent request getting a new worklet.

  handles.pop_front();
  worklet_available_loop.Run();
  EXPECT_TRUE(handle->GetSellerWorklet());

  std::unique_ptr<MockSellerWorklet> seller_worklet =
      auction_process_manager_.WaitForSellerWorklet();
  EXPECT_EQ(kDecisionLogicUrl, seller_worklet->script_source_url());
  EXPECT_EQ(kTrustedSignalsUrl, seller_worklet->trusted_scoring_signals_url());
  EXPECT_EQ(kTopWindowOrigin, seller_worklet->top_window_origin());

  EXPECT_EQ(0, seller_worklet->num_send_pending_signals_requests_calls());
  handle->GetSellerWorklet()->SendPendingSignalsRequests();
  seller_worklet->WaitForSendPendingSignalsRequests(1);

  // Should still be at the process limit.
  EXPECT_EQ(AuctionProcessManager::kMaxSellerProcesses,
            auction_process_manager_.GetSellerProcessCountForTesting());
}

// Test that requests with the same parameters reuse bidder worklets.
TEST_F(AuctionWorkletManagerTest, ReuseBidderWorklet) {
  // Load a bidder worklet.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle1;
  base::test::TestFuture<void> worklet_available1;
  auction_worklet_manager_->RequestBidderWorklet(
      kAuction1, kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
      /*needs_cors_for_additional_bid=*/false,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_bidding_signals_coordinator=*/std::nullopt,
      worklet_available1.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle1,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available1.Wait());
  EXPECT_TRUE(handle1->GetBidderWorklet());
  std::unique_ptr<MockBidderWorklet> bidder_worklet1 =
      auction_process_manager_.WaitForBidderWorklet();
  EXPECT_EQ(kDecisionLogicUrl, bidder_worklet1->script_source_url());
  EXPECT_EQ(kWasmUrl, bidder_worklet1->wasm_url());
  EXPECT_EQ(kTrustedSignalsUrl, bidder_worklet1->trusted_bidding_signals_url());
  EXPECT_EQ(kTopWindowOrigin, bidder_worklet1->top_window_origin());
  handle1->GetBidderWorklet()->SendPendingSignalsRequests();
  bidder_worklet1->WaitForSendPendingSignalsRequests(1);
  // Should only be one process.
  EXPECT_EQ(1u, auction_process_manager_.GetBidderProcessCountForTesting());
  EXPECT_THAT(handle1->GetDevtoolsAuctionIdsForTesting(),
              UnorderedElementsAre(kAuction1));

  // Load a bidder worklet with the same parameters. The worklet should be
  // reused.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle2;
  base::test::TestFuture<void> worklet_available2;
  auction_worklet_manager_->RequestBidderWorklet(
      kAuction2, kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
      /*needs_cors_for_additional_bid=*/false,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_bidding_signals_coordinator=*/std::nullopt,
      worklet_available2.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle2,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available2.Wait());
  EXPECT_EQ(handle1->GetBidderWorklet(), handle2->GetBidderWorklet());
  EXPECT_FALSE(auction_process_manager_.HasBidderWorkletRequest());
  handle2->GetBidderWorklet()->SendPendingSignalsRequests();
  bidder_worklet1->WaitForSendPendingSignalsRequests(2);
  // Should still only be one process.
  EXPECT_EQ(1u, auction_process_manager_.GetBidderProcessCountForTesting());
  // ... but used by both auctions.
  EXPECT_THAT(handle2->GetDevtoolsAuctionIdsForTesting(),
              UnorderedElementsAre(kAuction1, kAuction2));

  // Close original handle. Worklet should still be alive, and so should its
  // process.
  handle1.reset();
  EXPECT_EQ(1u, auction_process_manager_.GetBidderProcessCountForTesting());
  // We should no longer attribute its work to the first auction, however.
  EXPECT_THAT(handle2->GetDevtoolsAuctionIdsForTesting(),
              UnorderedElementsAre(kAuction2));

  // Load a bidder worklet with the same parameters. The worklet should still be
  // reused again.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle3;
  base::test::TestFuture<void> worklet_available3;
  auction_worklet_manager_->RequestBidderWorklet(
      kAuction3, kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
      /*needs_cors_for_additional_bid=*/false,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_bidding_signals_coordinator=*/std::nullopt,
      worklet_available3.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle3,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available3.Wait());
  EXPECT_EQ(handle2->GetBidderWorklet(), handle3->GetBidderWorklet());
  EXPECT_FALSE(auction_process_manager_.HasBidderWorkletRequest());
  handle3->GetBidderWorklet()->SendPendingSignalsRequests();
  bidder_worklet1->WaitForSendPendingSignalsRequests(3);
  // Should still only be one process.
  EXPECT_EQ(1u, auction_process_manager_.GetBidderProcessCountForTesting());
  EXPECT_THAT(handle3->GetDevtoolsAuctionIdsForTesting(),
              UnorderedElementsAre(kAuction2, kAuction3));

  // Close both remaining handles.
  handle2.reset();
  handle3.reset();

  // Process should be destroyed.
  EXPECT_EQ(0u, auction_process_manager_.GetBidderProcessCountForTesting());

  // Request another bidder worklet. A new BidderWorklet in a new process should
  // be created.
  base::test::TestFuture<void> worklet_available4;
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle4;
  auction_worklet_manager_->RequestBidderWorklet(
      kAuction4, kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
      /*needs_cors_for_additional_bid=*/false,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_bidding_signals_coordinator=*/std::nullopt,
      worklet_available4.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle4,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available4.Wait());
  EXPECT_TRUE(handle4->GetBidderWorklet());
  std::unique_ptr<MockBidderWorklet> bidder_worklet2 =
      auction_process_manager_.WaitForBidderWorklet();
  EXPECT_EQ(kDecisionLogicUrl, bidder_worklet2->script_source_url());
  EXPECT_EQ(kWasmUrl, bidder_worklet2->wasm_url());
  EXPECT_EQ(kTrustedSignalsUrl, bidder_worklet2->trusted_bidding_signals_url());
  EXPECT_EQ(kTopWindowOrigin, bidder_worklet2->top_window_origin());
  handle4->GetBidderWorklet()->SendPendingSignalsRequests();
  bidder_worklet2->WaitForSendPendingSignalsRequests(1);
  EXPECT_EQ(1u, auction_process_manager_.GetBidderProcessCountForTesting());
  EXPECT_THAT(handle4->GetDevtoolsAuctionIdsForTesting(),
              UnorderedElementsAre(kAuction4));
}

// Test that requests with the same parameters reuse seller worklets.
TEST_F(AuctionWorkletManagerTest, ReuseSellerWorklet) {
  // Load a seller worklet.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle1;
  base::test::TestFuture<void> worklet_available1;
  auction_worklet_manager_->RequestSellerWorklet(
      kAuction1, kDecisionLogicUrl, kTrustedSignalsUrl,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_scoring_signals_coordinator=*/std::nullopt,
      worklet_available1.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle1,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available1.Wait());
  EXPECT_TRUE(handle1->GetSellerWorklet());
  std::unique_ptr<MockSellerWorklet> seller_worklet1 =
      auction_process_manager_.WaitForSellerWorklet();
  EXPECT_EQ(kDecisionLogicUrl, seller_worklet1->script_source_url());
  EXPECT_EQ(kTrustedSignalsUrl, seller_worklet1->trusted_scoring_signals_url());
  EXPECT_EQ(kTopWindowOrigin, seller_worklet1->top_window_origin());
  handle1->GetSellerWorklet()->SendPendingSignalsRequests();
  seller_worklet1->WaitForSendPendingSignalsRequests(1);
  // Should only be one process.
  EXPECT_EQ(1u, auction_process_manager_.GetSellerProcessCountForTesting());
  EXPECT_THAT(handle1->GetDevtoolsAuctionIdsForTesting(),
              UnorderedElementsAre(kAuction1));

  // Load a seller worklet with the same parameters. The worklet should be
  // reused.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle2;
  base::test::TestFuture<void> worklet_available2;
  auction_worklet_manager_->RequestSellerWorklet(
      kAuction2, kDecisionLogicUrl, kTrustedSignalsUrl,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_scoring_signals_coordinator=*/std::nullopt,
      worklet_available2.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle2,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available2.Wait());
  EXPECT_EQ(handle1->GetSellerWorklet(), handle2->GetSellerWorklet());
  EXPECT_FALSE(auction_process_manager_.HasSellerWorkletRequest());
  handle2->GetSellerWorklet()->SendPendingSignalsRequests();
  seller_worklet1->WaitForSendPendingSignalsRequests(2);
  // Should still only be one process.
  EXPECT_EQ(1u, auction_process_manager_.GetSellerProcessCountForTesting());
  // ... but used by both auctions.
  EXPECT_THAT(handle2->GetDevtoolsAuctionIdsForTesting(),
              UnorderedElementsAre(kAuction1, kAuction2));

  // Close original handle. Worklet should still be alive, and so should its
  // process.
  handle1.reset();
  EXPECT_EQ(1u, auction_process_manager_.GetSellerProcessCountForTesting());
  // We should no longer attribute its work to the first auction, however.
  EXPECT_THAT(handle2->GetDevtoolsAuctionIdsForTesting(),
              UnorderedElementsAre(kAuction2));

  // Load a seller worklet with the same parameters. The worklet should still be
  // reused again.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle3;
  base::test::TestFuture<void> worklet_available3;
  auction_worklet_manager_->RequestSellerWorklet(
      kAuction3, kDecisionLogicUrl, kTrustedSignalsUrl,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_scoring_signals_coordinator=*/std::nullopt,
      worklet_available3.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle3,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available3.Wait());
  EXPECT_EQ(handle2->GetSellerWorklet(), handle3->GetSellerWorklet());
  EXPECT_FALSE(auction_process_manager_.HasSellerWorkletRequest());
  handle3->GetSellerWorklet()->SendPendingSignalsRequests();
  seller_worklet1->WaitForSendPendingSignalsRequests(3);
  // Should still only be one process.
  EXPECT_EQ(1u, auction_process_manager_.GetSellerProcessCountForTesting());
  EXPECT_THAT(handle2->GetDevtoolsAuctionIdsForTesting(),
              UnorderedElementsAre(kAuction2, kAuction3));

  // Close both remaining handles.
  handle2.reset();
  handle3.reset();

  // Process should be destroyed.
  EXPECT_EQ(0u, auction_process_manager_.GetSellerProcessCountForTesting());

  // Request another seller worklet. A new SellerWorklet in a new process should
  // be created.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle4;
  base::test::TestFuture<void> worklet_available4;
  auction_worklet_manager_->RequestSellerWorklet(
      kAuction4, kDecisionLogicUrl, kTrustedSignalsUrl,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_scoring_signals_coordinator=*/std::nullopt,
      worklet_available4.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle4,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available4.Wait());
  EXPECT_TRUE(handle4->GetSellerWorklet());
  std::unique_ptr<MockSellerWorklet> seller_worklet2 =
      auction_process_manager_.WaitForSellerWorklet();
  EXPECT_EQ(kDecisionLogicUrl, seller_worklet2->script_source_url());
  EXPECT_EQ(kTrustedSignalsUrl, seller_worklet2->trusted_scoring_signals_url());
  EXPECT_EQ(kTopWindowOrigin, seller_worklet2->top_window_origin());
  EXPECT_EQ(0, seller_worklet2->num_send_pending_signals_requests_calls());
  handle4->GetSellerWorklet()->SendPendingSignalsRequests();
  seller_worklet2->WaitForSendPendingSignalsRequests(1);
  EXPECT_EQ(1u, auction_process_manager_.GetSellerProcessCountForTesting());
  EXPECT_THAT(handle4->GetDevtoolsAuctionIdsForTesting(),
              UnorderedElementsAre(kAuction4));
}

// Make sure that worklets are not reused when parameters don't match.
TEST_F(AuctionWorkletManagerTest, DifferentBidderWorklets) {
  // Load a bidder worklet.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle1;
  base::test::TestFuture<void> worklet_available1;
  auction_worklet_manager_->RequestBidderWorklet(
      kAuction1, kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
      /*needs_cors_for_additional_bid=*/false,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_bidding_signals_coordinator=*/std::nullopt,
      worklet_available1.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle1,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available1.Wait());
  EXPECT_TRUE(handle1->GetBidderWorklet());
  std::unique_ptr<MockBidderWorklet> bidder_worklet1 =
      auction_process_manager_.WaitForBidderWorklet();
  EXPECT_EQ(kDecisionLogicUrl, bidder_worklet1->script_source_url());
  EXPECT_EQ(kWasmUrl, bidder_worklet1->wasm_url());
  EXPECT_EQ(kTrustedSignalsUrl, bidder_worklet1->trusted_bidding_signals_url());
  EXPECT_EQ(kTopWindowOrigin, bidder_worklet1->top_window_origin());
  // Should only be one process.
  EXPECT_EQ(1u, auction_process_manager_.GetBidderProcessCountForTesting());
  EXPECT_THAT(handle1->GetDevtoolsAuctionIdsForTesting(),
              UnorderedElementsAre(kAuction1));

  // Load a bidder worklet with a different decision logic URL. A new worklet
  // should be created, using the same process.
  const GURL kDifferentDecisionLogicUrl =
      GURL("https://origin.test/different_script");
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle2;
  base::test::TestFuture<void> worklet_available2;
  auction_worklet_manager_->RequestBidderWorklet(
      kAuction2, kDifferentDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
      /*needs_cors_for_additional_bid=*/false,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_bidding_signals_coordinator=*/std::nullopt,
      worklet_available2.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle2,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available2.Wait());
  EXPECT_TRUE(handle1->GetBidderWorklet());
  EXPECT_NE(handle1->GetBidderWorklet(), handle2->GetBidderWorklet());
  std::unique_ptr<MockBidderWorklet> bidder_worklet2 =
      auction_process_manager_.WaitForBidderWorklet();
  EXPECT_EQ(kDifferentDecisionLogicUrl, bidder_worklet2->script_source_url());
  EXPECT_EQ(kWasmUrl, bidder_worklet2->wasm_url());
  EXPECT_EQ(kTrustedSignalsUrl, bidder_worklet2->trusted_bidding_signals_url());
  EXPECT_EQ(kTopWindowOrigin, bidder_worklet2->top_window_origin());
  // Should still only be one process.
  EXPECT_EQ(1u, auction_process_manager_.GetBidderProcessCountForTesting());
  EXPECT_THAT(handle2->GetDevtoolsAuctionIdsForTesting(),
              UnorderedElementsAre(kAuction2));

  // Load a bidder worklet with a different (null) trusted signals URL. A new
  // worklet should be created, using the same process.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle3;
  base::test::TestFuture<void> worklet_available3;
  auction_worklet_manager_->RequestBidderWorklet(
      kAuction3, kDecisionLogicUrl, kWasmUrl,
      /*trusted_bidding_signals_url=*/std::nullopt,
      /*needs_cors_for_additional_bid=*/false,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_bidding_signals_coordinator=*/std::nullopt,
      worklet_available3.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle3,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available3.Wait());
  EXPECT_TRUE(handle3->GetBidderWorklet());
  EXPECT_NE(handle1->GetBidderWorklet(), handle3->GetBidderWorklet());
  EXPECT_NE(handle2->GetBidderWorklet(), handle3->GetBidderWorklet());
  std::unique_ptr<MockBidderWorklet> bidder_worklet3 =
      auction_process_manager_.WaitForBidderWorklet();
  EXPECT_EQ(kDecisionLogicUrl, bidder_worklet3->script_source_url());
  EXPECT_EQ(kWasmUrl, bidder_worklet3->wasm_url());
  EXPECT_EQ(std::nullopt, bidder_worklet3->trusted_bidding_signals_url());
  EXPECT_EQ(kTopWindowOrigin, bidder_worklet3->top_window_origin());
  // Should still only be one process.
  EXPECT_EQ(1u, auction_process_manager_.GetBidderProcessCountForTesting());
  EXPECT_THAT(handle3->GetDevtoolsAuctionIdsForTesting(),
              UnorderedElementsAre(kAuction3));

  // Load a bidder worklet with a different (null) wasm helper URL. A new
  // worklet should be created, using the same process.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle4;
  base::test::TestFuture<void> worklet_available4;
  auction_worklet_manager_->RequestBidderWorklet(
      kAuction4, kDecisionLogicUrl, /*wasm_url=*/std::nullopt,
      kTrustedSignalsUrl,
      /*needs_cors_for_additional_bid=*/false,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_bidding_signals_coordinator=*/std::nullopt,
      worklet_available4.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle4,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  EXPECT_TRUE(handle4->GetBidderWorklet());
  EXPECT_NE(handle1->GetBidderWorklet(), handle4->GetBidderWorklet());
  EXPECT_NE(handle2->GetBidderWorklet(), handle4->GetBidderWorklet());
  EXPECT_NE(handle3->GetBidderWorklet(), handle4->GetBidderWorklet());
  std::unique_ptr<MockBidderWorklet> bidder_worklet4 =
      auction_process_manager_.WaitForBidderWorklet();
  EXPECT_EQ(kDecisionLogicUrl, bidder_worklet4->script_source_url());
  EXPECT_EQ(std::nullopt, bidder_worklet4->wasm_url());
  EXPECT_EQ(kTrustedSignalsUrl, bidder_worklet4->trusted_bidding_signals_url());
  EXPECT_EQ(kTopWindowOrigin, bidder_worklet4->top_window_origin());
  // Should still only be one process.
  EXPECT_EQ(1u, auction_process_manager_.GetBidderProcessCountForTesting());
  EXPECT_THAT(handle4->GetDevtoolsAuctionIdsForTesting(),
              UnorderedElementsAre(kAuction4));
}

// Test bidder worklet matching with different experiment IDs.
TEST_F(AuctionWorkletManagerTest, BidderWorkletExperimentIDs) {
  const unsigned short kExperiment1 = 123u;
  const unsigned short kExperiment2 = 234u;

  base::test::TestFuture<void> worklet_available1;
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle1;
  auction_worklet_manager_->RequestBidderWorklet(
      kAuction1, kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
      /*needs_cors_for_additional_bid=*/false, kExperiment1,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_bidding_signals_coordinator=*/std::nullopt,
      worklet_available1.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle1,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available1.Wait());
  EXPECT_TRUE(handle1->GetBidderWorklet());
  std::unique_ptr<MockBidderWorklet> bidder_worklet1 =
      auction_process_manager_.WaitForBidderWorklet();

  // Request one with a different experiment ID. Should result in a different
  // worklet.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle2;
  base::test::TestFuture<void> worklet_available2;
  auction_worklet_manager_->RequestBidderWorklet(
      kAuction2, kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
      /*needs_cors_for_additional_bid=*/false, kExperiment2,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_bidding_signals_coordinator=*/std::nullopt,
      worklet_available2.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle2,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available2.Wait());
  EXPECT_TRUE(handle2->GetBidderWorklet());
  std::unique_ptr<MockBidderWorklet> bidder_worklet2 =
      auction_process_manager_.WaitForBidderWorklet();
  EXPECT_NE(handle1->GetBidderWorklet(), handle2->GetBidderWorklet());

  // Now try with different trusted signals URL (using WASM url instead).
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle3;
  base::test::TestFuture<void> worklet_available3;
  auction_worklet_manager_->RequestBidderWorklet(
      kAuction3, kDecisionLogicUrl, kWasmUrl, kWasmUrl,
      /*needs_cors_for_additional_bid=*/false, kExperiment1,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_bidding_signals_coordinator=*/std::nullopt,
      worklet_available3.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle3,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available3.Wait());
  EXPECT_TRUE(handle3->GetBidderWorklet());
  std::unique_ptr<MockBidderWorklet> bidder_worklet3 =
      auction_process_manager_.WaitForBidderWorklet();
  EXPECT_NE(handle1->GetBidderWorklet(), handle3->GetBidderWorklet());
  EXPECT_NE(handle2->GetBidderWorklet(), handle3->GetBidderWorklet());

  // Now test with null trusted signals URL. For bidder worklets this should be
  // as if no experiment was given, since that's the only way they see it.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle4;
  base::test::TestFuture<void> worklet_available4;
  auction_worklet_manager_->RequestBidderWorklet(
      kAuction4, kDecisionLogicUrl, kWasmUrl,
      /*trusted_bidding_signals_url=*/std::nullopt,
      /*needs_cors_for_additional_bid=*/false, kExperiment1,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_bidding_signals_coordinator=*/std::nullopt,
      worklet_available4.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle4,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available4.Wait());
  EXPECT_TRUE(handle4->GetBidderWorklet());
  std::unique_ptr<MockBidderWorklet> bidder_worklet4 =
      auction_process_manager_.WaitForBidderWorklet();
  EXPECT_NE(handle1->GetBidderWorklet(), handle4->GetBidderWorklet());
  EXPECT_NE(handle2->GetBidderWorklet(), handle4->GetBidderWorklet());
  EXPECT_NE(handle3->GetBidderWorklet(), handle4->GetBidderWorklet());

  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle5;
  base::test::TestFuture<void> worklet_available5;
  auction_worklet_manager_->RequestBidderWorklet(
      kAuction5, kDecisionLogicUrl, kWasmUrl,
      /*trusted_bidding_signals_url=*/std::nullopt,
      /*needs_cors_for_additional_bid=*/false,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_bidding_signals_coordinator=*/std::nullopt,
      worklet_available5.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle5,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available5.Wait());
  EXPECT_TRUE(handle5->GetBidderWorklet());
  EXPECT_EQ(handle5->GetBidderWorklet(), handle4->GetBidderWorklet());
  EXPECT_THAT(handle4->GetDevtoolsAuctionIdsForTesting(),
              UnorderedElementsAre(kAuction4, kAuction5));
}

// Test bidder worklet matching with different CORS mode.
TEST_F(AuctionWorkletManagerTest, BidderWorkletCORSForAdditionalBid) {
  base::test::TestFuture<void> worklet_available1;
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle1;
  auction_worklet_manager_->RequestBidderWorklet(
      kAuction1, kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
      /*needs_cors_for_additional_bid=*/false,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_bidding_signals_coordinator=*/std::nullopt,
      worklet_available1.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle1,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available1.Wait());
  EXPECT_TRUE(handle1->GetBidderWorklet());
  std::unique_ptr<MockBidderWorklet> bidder_worklet1 =
      auction_process_manager_.WaitForBidderWorklet();

  // Request one with a different CORS setting. Should result in a different
  // worklet.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle2;
  base::test::TestFuture<void> worklet_available2;
  auction_worklet_manager_->RequestBidderWorklet(
      kAuction1, kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
      /*needs_cors_for_additional_bid=*/true,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_bidding_signals_coordinator=*/std::nullopt,
      worklet_available2.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle2,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available2.Wait());
  EXPECT_TRUE(handle2->GetBidderWorklet());
  std::unique_ptr<MockBidderWorklet> bidder_worklet2 =
      auction_process_manager_.WaitForBidderWorklet();
  EXPECT_NE(handle1->GetBidderWorklet(), handle2->GetBidderWorklet());
}

// Make sure that worklets are not reused when parameters don't match.
TEST_F(AuctionWorkletManagerTest, DifferentSellerWorklets) {
  // Load a seller worklet.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle1;
  base::test::TestFuture<void> worklet_available1;
  auction_worklet_manager_->RequestSellerWorklet(
      kAuction1, kDecisionLogicUrl, kTrustedSignalsUrl,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_scoring_signals_coordinator=*/std::nullopt,
      worklet_available1.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle1,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available1.Wait());
  EXPECT_TRUE(handle1->GetSellerWorklet());
  std::unique_ptr<MockSellerWorklet> seller_worklet1 =
      auction_process_manager_.WaitForSellerWorklet();
  EXPECT_EQ(kDecisionLogicUrl, seller_worklet1->script_source_url());
  EXPECT_EQ(kTrustedSignalsUrl, seller_worklet1->trusted_scoring_signals_url());
  EXPECT_EQ(kTopWindowOrigin, seller_worklet1->top_window_origin());
  // Should only be one process.
  EXPECT_EQ(1u, auction_process_manager_.GetSellerProcessCountForTesting());

  // Load a seller worklet with a different decision logic URL. A new worklet
  // should be created, using the same process.
  const GURL kDifferentDecisionLogicUrl =
      GURL("https://origin.test/different_script");
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle2;
  base::test::TestFuture<void> worklet_available2;
  auction_worklet_manager_->RequestSellerWorklet(
      kAuction1, kDifferentDecisionLogicUrl, kTrustedSignalsUrl,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_scoring_signals_coordinator=*/std::nullopt,
      worklet_available2.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle2,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available2.Wait());
  EXPECT_TRUE(handle1->GetSellerWorklet());
  EXPECT_NE(handle1->GetSellerWorklet(), handle2->GetSellerWorklet());
  std::unique_ptr<MockSellerWorklet> seller_worklet2 =
      auction_process_manager_.WaitForSellerWorklet();
  EXPECT_EQ(kDifferentDecisionLogicUrl, seller_worklet2->script_source_url());
  EXPECT_EQ(kTrustedSignalsUrl, seller_worklet2->trusted_scoring_signals_url());
  EXPECT_EQ(kTopWindowOrigin, seller_worklet2->top_window_origin());
  // Should still only be one process.
  EXPECT_EQ(1u, auction_process_manager_.GetSellerProcessCountForTesting());

  // Load a seller worklet with a different (null) trusted signals URL. A new
  // worklet should be created, using the same process.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle3;
  base::test::TestFuture<void> worklet_available3;
  auction_worklet_manager_->RequestSellerWorklet(
      kAuction1, kDecisionLogicUrl,
      /*trusted_scoring_signals_url=*/std::nullopt,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_scoring_signals_coordinator=*/std::nullopt,
      worklet_available3.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle3,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available3.Wait());
  EXPECT_TRUE(handle3->GetSellerWorklet());
  EXPECT_NE(handle1->GetSellerWorklet(), handle3->GetSellerWorklet());
  EXPECT_NE(handle2->GetSellerWorklet(), handle3->GetSellerWorklet());
  std::unique_ptr<MockSellerWorklet> seller_worklet3 =
      auction_process_manager_.WaitForSellerWorklet();
  EXPECT_EQ(kDecisionLogicUrl, seller_worklet3->script_source_url());
  EXPECT_EQ(std::nullopt, seller_worklet3->trusted_scoring_signals_url());
  EXPECT_EQ(kTopWindowOrigin, seller_worklet3->top_window_origin());
  // Should still only be one process.
  EXPECT_EQ(1u, auction_process_manager_.GetSellerProcessCountForTesting());
}

// Test seller worklet matching with different experiment IDs.
TEST_F(AuctionWorkletManagerTest, SellerWorkletExperimentIDs) {
  const unsigned short kExperiment1 = 123u;
  const unsigned short kExperiment2 = 234u;

  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle1;
  base::test::TestFuture<void> worklet_available1;
  auction_worklet_manager_->RequestSellerWorklet(
      kAuction1, kDecisionLogicUrl, kTrustedSignalsUrl, kExperiment1,
      /*trusted_scoring_signals_coordinator=*/std::nullopt,
      worklet_available1.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle1,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available1.Wait());
  EXPECT_TRUE(handle1->GetSellerWorklet());
  std::unique_ptr<MockSellerWorklet> seller_worklet1 =
      auction_process_manager_.WaitForSellerWorklet();

  // Request one with a different experiment ID. Should result in a different
  // worklet.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle2;
  base::test::TestFuture<void> worklet_available2;
  auction_worklet_manager_->RequestSellerWorklet(
      kAuction1, kDecisionLogicUrl, kTrustedSignalsUrl, kExperiment2,
      /*trusted_scoring_signals_coordinator=*/std::nullopt,
      worklet_available2.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle2,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available2.Wait());
  EXPECT_TRUE(handle2->GetSellerWorklet());
  std::unique_ptr<MockSellerWorklet> seller_worklet2 =
      auction_process_manager_.WaitForSellerWorklet();
  EXPECT_NE(handle1->GetSellerWorklet(), handle2->GetSellerWorklet());

  // Now try with different trusted signals URL (using WASM url instead).
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle3;
  base::test::TestFuture<void> worklet_available3;
  auction_worklet_manager_->RequestSellerWorklet(
      kAuction1, kDecisionLogicUrl, kWasmUrl, kExperiment1,
      /*trusted_scoring_signals_coordinator=*/std::nullopt,
      worklet_available3.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle3,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available3.Wait());
  EXPECT_TRUE(handle3->GetSellerWorklet());
  std::unique_ptr<MockSellerWorklet> seller_worklet3 =
      auction_process_manager_.WaitForSellerWorklet();
  EXPECT_NE(handle1->GetSellerWorklet(), handle3->GetSellerWorklet());
  EXPECT_NE(handle2->GetSellerWorklet(), handle3->GetSellerWorklet());

  // Now test with null trusted signals URL. For seller worklet, we should still
  // distinguish different experiment IDs since the ID shows up in
  // AuctionConfig.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle4;
  base::test::TestFuture<void> worklet_available4;
  auction_worklet_manager_->RequestSellerWorklet(
      kAuction1, kDecisionLogicUrl,
      /*trusted_scoring_signals_url=*/std::nullopt, kExperiment1,
      /*trusted_scoring_signals_coordinator=*/std::nullopt,
      worklet_available4.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle4,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available4.Wait());
  EXPECT_TRUE(handle4->GetSellerWorklet());
  std::unique_ptr<MockSellerWorklet> seller_worklet4 =
      auction_process_manager_.WaitForSellerWorklet();
  EXPECT_NE(handle1->GetSellerWorklet(), handle4->GetSellerWorklet());
  EXPECT_NE(handle2->GetSellerWorklet(), handle4->GetSellerWorklet());
  EXPECT_NE(handle3->GetSellerWorklet(), handle4->GetSellerWorklet());

  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle5;
  base::test::TestFuture<void> worklet_available5;
  auction_worklet_manager_->RequestSellerWorklet(
      kAuction1, kDecisionLogicUrl,
      /*trusted_scoring_signals_url=*/std::nullopt,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_scoring_signals_coordinator=*/std::nullopt,
      worklet_available5.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle5,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available5.Wait());
  EXPECT_TRUE(handle5->GetSellerWorklet());
  std::unique_ptr<MockSellerWorklet> seller_worklet5 =
      auction_process_manager_.WaitForSellerWorklet();
  EXPECT_NE(handle1->GetSellerWorklet(), handle5->GetSellerWorklet());
  EXPECT_NE(handle2->GetSellerWorklet(), handle5->GetSellerWorklet());
  EXPECT_NE(handle3->GetSellerWorklet(), handle5->GetSellerWorklet());
  EXPECT_NE(handle4->GetSellerWorklet(), handle5->GetSellerWorklet());
}

TEST_F(AuctionWorkletManagerTest, BidderWorkletLoadError) {
  const char kErrorText[] = "Goat teleportation error";

  // Load a bidder worklet.
  FatalLoadErrorHelper load_error_helper;
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
  base::test::TestFuture<void> worklet_available;
  auction_worklet_manager_->RequestBidderWorklet(
      kAuction1, kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
      /*needs_cors_for_additional_bid=*/false,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_bidding_signals_coordinator=*/std::nullopt,
      worklet_available.GetCallback(), load_error_helper.Callback(), handle,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available.Wait());
  EXPECT_TRUE(handle->GetBidderWorklet());

  // Return a load error.
  std::unique_ptr<MockBidderWorklet> bidder_worklet =
      auction_process_manager_.WaitForBidderWorklet();
  bidder_worklet->ClosePipe(kErrorText);

  // Wait for the load error, check the parameters.
  load_error_helper.WaitForResult();
  EXPECT_THAT(load_error_helper.errors(), testing::ElementsAre(kErrorText));
  EXPECT_EQ(AuctionWorkletManager::FatalErrorType::kScriptLoadFailed,
            load_error_helper.fatal_error_type());

  // Should be safe to call into the worklet, even after the error. This allows
  // errors to be handled asynchronously.
  handle->GetBidderWorklet()->SendPendingSignalsRequests();
  task_environment()->RunUntilIdle();

  // Another request for the same worklet should trigger creation of a new
  // worklet, even though the old handle for the worklet hasn't been deleted
  // yet.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle2;
  base::test::TestFuture<void> worklet_available2;
  auction_worklet_manager_->RequestBidderWorklet(
      kAuction2, kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
      /*needs_cors_for_additional_bid=*/false,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_bidding_signals_coordinator=*/std::nullopt,
      worklet_available2.GetCallback(), load_error_helper.Callback(), handle2,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available2.Wait());
  EXPECT_TRUE(handle2->GetBidderWorklet());
  EXPECT_NE(handle->GetBidderWorklet(), handle2->GetBidderWorklet());
  std::unique_ptr<MockBidderWorklet> bidder_worklet2 =
      auction_process_manager_.WaitForBidderWorklet();
  EXPECT_THAT(handle2->GetDevtoolsAuctionIdsForTesting(),
              UnorderedElementsAre(kAuction2));
}

// Make sure that errors that occur when some worklets haven't gotten
// their success callbacks yet work right.
TEST_F(AuctionWorkletManagerTest, LoadErrorWithoutProcessAssigned) {
  const size_t kNumWorklets = AuctionWorkletManager::kBatchSize * 3;

  // Normally, ~MockBidderWorklet() spins an event loop in order to wait for all
  // incoming pending signals requests. This causes trouble for this test since
  // it makes a bunch of extra notifications get dispatched from the nested
  // event loop, making it hard to precisely inject a failure. Since this test
  // doesn't care about that, just turn that functionality off.
  auction_process_manager_.DisableBidderWorkletDtorPendingSignalsCheck();

  size_t success_callbacks = 0;
  size_t error_callbacks = 0;
  std::vector<std::unique_ptr<AuctionWorkletManager::WorkletHandle>> handles;
  for (size_t i = 0; i < kNumWorklets; ++i) {
    handles.emplace_back();
    auction_worklet_manager_->RequestBidderWorklet(
        kAuction1, kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
        /*needs_cors_for_additional_bid=*/false,
        /*experiment_group_id=*/std::nullopt,
        /*trusted_bidding_signals_slot_size_param=*/"",
        /*trusted_bidding_signals_coordinator=*/std::nullopt,
        base::BindOnce(
            [](size_t* success_callbacks_ptr, size_t worklet_index) {
              // Successes must be invoked in order, starting from the one for
              // 0th handle.
              EXPECT_EQ(worklet_index, *success_callbacks_ptr);
              ++*success_callbacks_ptr;
            },
            &success_callbacks, i),
        base::BindLambdaForTesting(
            [&](AuctionWorkletManager::FatalErrorType fatal_error_type,
                const std::vector<std::string>& errors) {
              ++error_callbacks;
              EXPECT_EQ(fatal_error_type,
                        AuctionWorkletManager::FatalErrorType::kWorkletCrash);
              EXPECT_THAT(errors, ::testing::ElementsAre(
                                      "https://origin.test/script crashed."));
            }),
        handles.back(),
        auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  }

  // Grab the first worklet to inject a simulated crash.
  std::unique_ptr<MockBidderWorklet> bidder_worklet =
      auction_process_manager_.WaitForBidderWorklet();
  bidder_worklet.reset();

  task_environment()->RunUntilIdle();
  // We expect the notification batch size # of successes, and as many failures
  // as there were requests.
  EXPECT_EQ(AuctionWorkletManager::kBatchSize, success_callbacks);
  EXPECT_EQ(kNumWorklets, error_callbacks);
}

// Make sure that success callbacks are ordered properly.
TEST_F(AuctionWorkletManagerTest, LoadSuccessOrder) {
  const size_t kNumWorklets = AuctionWorkletManager::kBatchSize * 3 + 1;

  size_t success_callbacks = 0;
  std::vector<std::unique_ptr<AuctionWorkletManager::WorkletHandle>> handles;
  base::RunLoop run_loop;
  for (size_t i = 0; i < kNumWorklets; ++i) {
    handles.emplace_back();
    auction_worklet_manager_->RequestBidderWorklet(
        kAuction1, kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
        /*needs_cors_for_additional_bid=*/false,
        /*experiment_group_id=*/std::nullopt,
        /*trusted_bidding_signals_slot_size_param=*/"",
        /*trusted_bidding_signals_coordinator=*/std::nullopt,
        base::BindOnce(
            [](size_t* success_callbacks_ptr, size_t limit,
               base::RunLoop* run_loop, size_t worklet_index) {
              // Successes must be invoked in order, starting from the one for
              // 0th handle.
              EXPECT_EQ(worklet_index, *success_callbacks_ptr);
              ++*success_callbacks_ptr;
              if (*success_callbacks_ptr == limit) {
                run_loop->Quit();
              }
            },
            &success_callbacks, kNumWorklets, &run_loop, i),
        NeverInvokedFatalErrorCallback(), handles.back(),
        auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  }

  run_loop.Run();
  EXPECT_EQ(kNumWorklets, success_callbacks);
  std::unique_ptr<MockBidderWorklet> bidder_worklet =
      auction_process_manager_.WaitForBidderWorklet();
}

TEST_F(AuctionWorkletManagerTest, SellerWorkletLoadError) {
  const char kErrorText[] = "Goat teleportation error";

  // Load a seller worklet.
  FatalLoadErrorHelper load_error_helper;
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
  base::test::TestFuture<void> worklet_available;
  auction_worklet_manager_->RequestSellerWorklet(
      kAuction1, kDecisionLogicUrl, kTrustedSignalsUrl,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_scoring_signals_coordinator=*/std::nullopt,
      worklet_available.GetCallback(), load_error_helper.Callback(), handle,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available.Wait());
  EXPECT_TRUE(handle->GetSellerWorklet());

  // Return a load error.
  std::unique_ptr<MockSellerWorklet> seller_worklet =
      auction_process_manager_.WaitForSellerWorklet();
  seller_worklet->ClosePipe(kErrorText);

  // Wait for the load error, check the parameters.
  load_error_helper.WaitForResult();
  EXPECT_THAT(load_error_helper.errors(), testing::ElementsAre(kErrorText));
  EXPECT_EQ(AuctionWorkletManager::FatalErrorType::kScriptLoadFailed,
            load_error_helper.fatal_error_type());

  // Should be safe to call into the worklet, even after the error. This allows
  // errors to be handled asynchronously.
  handle->GetSellerWorklet()->SendPendingSignalsRequests();
  task_environment()->RunUntilIdle();

  // Another request for the same worklet should trigger creation of a new
  // worklet, even though the old handle for the worklet hasn't been deleted
  // yet.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle2;
  base::test::TestFuture<void> worklet_available2;
  auction_worklet_manager_->RequestSellerWorklet(
      kAuction1, kDecisionLogicUrl, kTrustedSignalsUrl,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_scoring_signals_coordinator=*/std::nullopt,
      worklet_available2.GetCallback(), load_error_helper.Callback(), handle2,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available2.Wait());
  EXPECT_TRUE(handle2->GetSellerWorklet());
  EXPECT_NE(handle->GetSellerWorklet(), handle2->GetSellerWorklet());
  std::unique_ptr<MockSellerWorklet> seller_worklet2 =
      auction_process_manager_.WaitForSellerWorklet();
}

TEST_F(AuctionWorkletManagerTest, BidderWorkletCrash) {
  // Load a bidder worklet.
  FatalLoadErrorHelper load_error_helper;
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
  base::test::TestFuture<void> worklet_available;
  auction_worklet_manager_->RequestBidderWorklet(
      kAuction1, kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
      /*needs_cors_for_additional_bid=*/false,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_bidding_signals_coordinator=*/std::nullopt,
      worklet_available.GetCallback(), load_error_helper.Callback(), handle,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available.Wait());
  EXPECT_TRUE(handle->GetBidderWorklet());

  // Close the worklet pipe, simulating a worklet crash.
  std::unique_ptr<MockBidderWorklet> bidder_worklet =
      auction_process_manager_.WaitForBidderWorklet();
  bidder_worklet.reset();

  // Wait for the error, check the parameters.
  load_error_helper.WaitForResult();
  EXPECT_THAT(load_error_helper.errors(),
              testing::ElementsAre("https://origin.test/script crashed."));
  EXPECT_EQ(AuctionWorkletManager::FatalErrorType::kWorkletCrash,
            load_error_helper.fatal_error_type());

  // Should be safe to call into the worklet, even after the error. This allows
  // errors to be handled asynchronously.
  handle->GetBidderWorklet()->SendPendingSignalsRequests();
  task_environment()->RunUntilIdle();
  handle->GetBidderWorklet()->SendPendingSignalsRequests();

  // Another request for the same worklet should trigger creation of a new
  // worklet, even though the old handle for the worklet hasn't been deleted
  // yet.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle2;
  base::test::TestFuture<void> worklet_available2;
  auction_worklet_manager_->RequestBidderWorklet(
      kAuction1, kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
      /*needs_cors_for_additional_bid=*/false,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_bidding_signals_coordinator=*/std::nullopt,
      worklet_available2.GetCallback(), load_error_helper.Callback(), handle2,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available2.Wait());
  EXPECT_TRUE(handle2->GetBidderWorklet());
  EXPECT_NE(handle->GetBidderWorklet(), handle2->GetBidderWorklet());
  std::unique_ptr<MockBidderWorklet> bidder_worklet2 =
      auction_process_manager_.WaitForBidderWorklet();
}

TEST_F(AuctionWorkletManagerTest, SellerWorkletCrash) {
  // Load a seller worklet.
  FatalLoadErrorHelper load_error_helper;
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
  base::test::TestFuture<void> worklet_available;
  auction_worklet_manager_->RequestSellerWorklet(
      kAuction1, kDecisionLogicUrl, kTrustedSignalsUrl,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_scoring_signals_coordinator=*/std::nullopt,
      worklet_available.GetCallback(), load_error_helper.Callback(), handle,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available.Wait());
  EXPECT_TRUE(handle->GetSellerWorklet());

  // Close the worklet pipe, simulating a worklet crash.
  std::unique_ptr<MockSellerWorklet> seller_worklet =
      auction_process_manager_.WaitForSellerWorklet();
  seller_worklet.reset();

  // Wait for the error, check the parameters.
  load_error_helper.WaitForResult();
  EXPECT_THAT(load_error_helper.errors(),
              testing::ElementsAre("https://origin.test/script crashed."));
  EXPECT_EQ(AuctionWorkletManager::FatalErrorType::kWorkletCrash,
            load_error_helper.fatal_error_type());

  // Should be safe to call into the worklet, even after the error. This allows
  // errors to be handled asynchronously.
  handle->GetSellerWorklet()->SendPendingSignalsRequests();
  task_environment()->RunUntilIdle();
  handle->GetSellerWorklet()->SendPendingSignalsRequests();

  // Another request for the same worklet should trigger creation of a new
  // worklet, even though the old handle for the worklet hasn't been deleted
  // yet.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle2;
  base::test::TestFuture<void> worklet_available2;
  auction_worklet_manager_->RequestSellerWorklet(
      kAuction2, kDecisionLogicUrl, kTrustedSignalsUrl,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_scoring_signals_coordinator=*/std::nullopt,
      worklet_available2.GetCallback(), load_error_helper.Callback(), handle2,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available2.Wait());
  EXPECT_TRUE(handle2->GetSellerWorklet());
  EXPECT_NE(handle->GetSellerWorklet(), handle2->GetSellerWorklet());
  std::unique_ptr<MockSellerWorklet> seller_worklet2 =
      auction_process_manager_.WaitForSellerWorklet();
  EXPECT_THAT(handle2->GetDevtoolsAuctionIdsForTesting(),
              UnorderedElementsAre(kAuction2));
}

// Test reentrant deletion of a WorkletHandle on error.
TEST_F(AuctionWorkletManagerTest, BidderWorkletDeleteOnError) {
  const char kErrorText[] = "Goat teleporation error";

  // Load a bidder worklet.
  base::RunLoop run_loop;
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
  base::test::TestFuture<void> worklet_available;
  auction_worklet_manager_->RequestBidderWorklet(
      kAuction1, kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
      /*needs_cors_for_additional_bid=*/false,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_bidding_signals_coordinator=*/std::nullopt,
      worklet_available.GetCallback(),
      base::BindLambdaForTesting(
          [&](AuctionWorkletManager::FatalErrorType fatal_error_type,
              const std::vector<std::string>& errors) {
            handle.reset();
            run_loop.Quit();
          }),
      handle,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available.Wait());
  EXPECT_TRUE(handle->GetBidderWorklet());

  // Return a load error.
  std::unique_ptr<MockBidderWorklet> bidder_worklet =
      auction_process_manager_.WaitForBidderWorklet();
  bidder_worklet->ClosePipe(kErrorText);

  run_loop.Run();
  // The process should have been deleted, and there should be no crashes.
  EXPECT_EQ(0u, auction_process_manager_.GetBidderProcessCountForTesting());
}

// Test re-entrant deletion of a WorkletHandle on success, and following
// failure on the same worklet.
TEST_F(AuctionWorkletManagerTest, BidderWorkletDeleteOnSuccess) {
  const char kErrorText[] = "Ox undercapacity error";

  int worklets_received = 0;
  int failures_received = 0;
  std::vector<std::unique_ptr<AuctionWorkletManager::WorkletHandle>> handles;

  // This test assumes that the 10 requests it handles all fit within the same
  // notification batch; otherwise it would still pass but not actually exercise
  // what it's meant to exercise.
  ASSERT_LE(10u, AuctionWorkletManager::kBatchSize);

  // Request 10 worklets; on receipt of first one, delete first 4 handles.
  for (int i = 0; i < 10; ++i) {
    handles.emplace_back();
    auction_worklet_manager_->RequestBidderWorklet(
        kAuction1, kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
        /*needs_cors_for_additional_bid=*/false,
        /*experiment_group_id=*/std::nullopt,
        /*trusted_bidding_signals_slot_size_param=*/"",
        /*trusted_bidding_signals_coordinator=*/std::nullopt,
        base::BindOnce(
            [](int worklet_index, int* worklets_received_ptr,
               std::vector<std::unique_ptr<
                   AuctionWorkletManager::WorkletHandle>>* handles_ptr) {
              if (*worklets_received_ptr == 0) {
                EXPECT_EQ(0, worklet_index);
                for (int j = 0; j < 4; ++j) {
                  handles_ptr->erase(handles_ptr->begin());
                }
              } else {
                // After receiving 0, we deleted 0, 1, 2, 3, so we expect 4 and
                // on..
                EXPECT_EQ(3 + *worklets_received_ptr, worklet_index);
              }
              ++*worklets_received_ptr;
            },
            i, &worklets_received, &handles),
        base::BindLambdaForTesting(
            [&](AuctionWorkletManager::FatalErrorType fatal_error_type,
                const std::vector<std::string>& errors) {
              ++failures_received;
              EXPECT_EQ(
                  fatal_error_type,
                  AuctionWorkletManager::FatalErrorType::kScriptLoadFailed);
              EXPECT_THAT(errors, ::testing::ElementsAre(kErrorText));
            }),
        handles.back(),
        auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  }
  task_environment()->RunUntilIdle();
  // We expect the first worklet + 6 that weren't cancelled.
  EXPECT_EQ(7, worklets_received);

  // Return a load error.
  std::unique_ptr<MockBidderWorklet> bidder_worklet =
      auction_process_manager_.WaitForBidderWorklet();
  bidder_worklet->ClosePipe(kErrorText);

  // Only that 6 that weren't cancelled should receive it.
  task_environment()->RunUntilIdle();
  EXPECT_EQ(6, failures_received);
}

// Test reentrant deletion of a WorkletHandle on error.
TEST_F(AuctionWorkletManagerTest, SellerWorkletDeleteOnError) {
  const char kErrorText[] = "Goat teleporation error";

  // Load a seller worklet.
  base::RunLoop run_loop;
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
  base::test::TestFuture<void> worklet_available;
  auction_worklet_manager_->RequestSellerWorklet(
      kAuction1, kDecisionLogicUrl, kTrustedSignalsUrl,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_scoring_signals_coordinator=*/std::nullopt,
      worklet_available.GetCallback(),
      base::BindLambdaForTesting(
          [&](AuctionWorkletManager::FatalErrorType fatal_error_type,
              const std::vector<std::string>& errors) {
            handle.reset();
            run_loop.Quit();
          }),
      handle,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available.Wait());
  EXPECT_TRUE(handle->GetSellerWorklet());

  // Return a load error.
  std::unique_ptr<MockSellerWorklet> seller_worklet =
      auction_process_manager_.WaitForSellerWorklet();
  seller_worklet->ClosePipe(kErrorText);

  run_loop.Run();
  // The process should have been deleted, and there should be no crashes.
  EXPECT_EQ(0u, auction_process_manager_.GetSellerProcessCountForTesting());
}

// Minimal test that bidder worklets' AuctionURLLoaderFactoryProxies are
// correctly configured.
TEST_F(AuctionWorkletManagerTest, BidderWorkletUrlRequestProtection) {
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
  base::test::TestFuture<void> worklet_available;
  auction_worklet_manager_->RequestBidderWorklet(
      kAuction1, kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
      /*needs_cors_for_additional_bid=*/false,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_bidding_signals_coordinator=*/std::nullopt,
      worklet_available.GetCallback(), NeverInvokedFatalErrorCallback(), handle,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available.Wait());
  EXPECT_TRUE(handle->GetBidderWorklet());

  std::unique_ptr<MockBidderWorklet> bidder_worklet =
      auction_process_manager_.WaitForBidderWorklet();

  struct AllowedUrls {
    GURL url;
    const char* mime_type;
  };
  const auto kAllowedUrls = std::to_array<AllowedUrls>({
      {kDecisionLogicUrl, "application/javascript"},
      {kWasmUrl, "application/wasm"},
      {GURL("https://origin.test/"
            "trusted_signals?hostname=top.window.origin.test&render_urls=not_"
            "validated"),
       "application/json"},
  });

  for (size_t i = 0; i < std::size(kAllowedUrls); ++i) {
    network::ResourceRequest request;
    request.url = kAllowedUrls[i].url;
    request.headers.SetHeader(net::HttpRequestHeaders::kAccept,
                              kAllowedUrls[i].mime_type);
    mojo::PendingRemote<network::mojom::URLLoader> receiver;
    mojo::PendingReceiver<network::mojom::URLLoaderClient> client;
    bidder_worklet->url_loader_factory()->CreateLoaderAndStart(
        receiver.InitWithNewPipeAndPassReceiver(), /*request_id=*/0,
        /*options=*/0, request, client.InitWithNewPipeAndPassRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
    bidder_worklet->url_loader_factory().FlushForTesting();
    EXPECT_TRUE(bidder_worklet->url_loader_factory().is_connected());
    ASSERT_EQ(i + 1, url_loader_factory_.pending_requests()->size());
    EXPECT_EQ(kAllowedUrls[i].url,
              (*url_loader_factory_.pending_requests())[i].request.url);
  }

  // Other URLs should be rejected.
  network::ResourceRequest request;
  request.url = GURL("https://origin.test/");
  request.headers.SetHeader(net::HttpRequestHeaders::kAccept,
                            kAllowedUrls[0].mime_type);
  mojo::PendingRemote<network::mojom::URLLoader> receiver;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client;
  bidder_worklet->url_loader_factory()->CreateLoaderAndStart(
      receiver.InitWithNewPipeAndPassReceiver(), /*request_id=*/0,
      /*options=*/0, request, client.InitWithNewPipeAndPassRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  bidder_worklet->url_loader_factory().FlushForTesting();
  EXPECT_FALSE(bidder_worklet->url_loader_factory().is_connected());
  EXPECT_EQ(std::size(kAllowedUrls),
            url_loader_factory_.pending_requests()->size());
  EXPECT_EQ("Unexpected request", TakeBadMessage());
}

// Test a bidder worklet can be correctly requested with a valid coordinator
// when `kFledgeTrustedSignalsKVv2Support` is disabled. It depends on
// `GetBiddingAndAuctionServerKey()` to cause a crash if it is called
// unexpectedly.
TEST_F(AuctionWorkletManagerTest, BidderWorkletWithKVv2FeatureDisabled) {
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
  base::test::TestFuture<void> worklet_available;

  auction_worklet_manager_->RequestBidderWorklet(
      kAuction1, kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
      /*needs_cors_for_additional_bid=*/false,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_bidding_signals_coordinator=*/
      url::Origin::Create(GURL("https://origin.test/")),
      worklet_available.GetCallback(), NeverInvokedFatalErrorCallback(), handle,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());

  ASSERT_TRUE(worklet_available.Wait());
  EXPECT_TRUE(handle->GetBidderWorklet());
  std::unique_ptr<MockBidderWorklet> bidder_worklet =
      auction_process_manager_.WaitForBidderWorklet();
  EXPECT_EQ(kDecisionLogicUrl, bidder_worklet->script_source_url());
  EXPECT_EQ(kTrustedSignalsUrl, bidder_worklet->trusted_bidding_signals_url());
  EXPECT_TRUE(!bidder_worklet->public_key());
}

// Minimal test that seller worklets' AuctionURLLoaderFactoryProxies are
// correctly configured.
TEST_F(AuctionWorkletManagerTest, SellerWorkletUrlRequestProtection) {
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
  base::test::TestFuture<void> worklet_available;
  auction_worklet_manager_->RequestSellerWorklet(
      kAuction1, kDecisionLogicUrl, kTrustedSignalsUrl,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_scoring_signals_coordinator=*/std::nullopt,
      worklet_available.GetCallback(), NeverInvokedFatalErrorCallback(), handle,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available.Wait());
  EXPECT_TRUE(handle->GetSellerWorklet());

  std::unique_ptr<MockSellerWorklet> seller_worklet =
      auction_process_manager_.WaitForSellerWorklet();

  struct AllowedUrlMapping {
    GURL url;
    const char* mime_type;
  };
  const auto kAllowedUrls = std::to_array<AllowedUrlMapping>({
      {
          kDecisionLogicUrl,
          "application/javascript",
      },
      {GURL("https://origin.test/"
            "trusted_signals?hostname=top.window.origin.test&render_urls=not_"
            "validated"),
       "application/json"},
  });

  for (size_t i = 0; i < std::size(kAllowedUrls); ++i) {
    network::ResourceRequest request;
    request.url = kAllowedUrls[i].url;
    request.headers.SetHeader(net::HttpRequestHeaders::kAccept,
                              kAllowedUrls[i].mime_type);
    mojo::PendingRemote<network::mojom::URLLoader> receiver;
    mojo::PendingReceiver<network::mojom::URLLoaderClient> client;
    seller_worklet->url_loader_factory()->CreateLoaderAndStart(
        receiver.InitWithNewPipeAndPassReceiver(), /*request_id=*/0,
        /*options=*/0, request, client.InitWithNewPipeAndPassRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
    seller_worklet->url_loader_factory().FlushForTesting();
    EXPECT_TRUE(seller_worklet->url_loader_factory().is_connected());
    ASSERT_EQ(i + 1, url_loader_factory_.pending_requests()->size());
    EXPECT_EQ(kAllowedUrls[i].url,
              (*url_loader_factory_.pending_requests())[i].request.url);
  }

  // Other URLs should be rejected.
  network::ResourceRequest request;
  request.url = GURL("https://origin.test/");
  request.headers.SetHeader(net::HttpRequestHeaders::kAccept,
                            kAllowedUrls[0].mime_type);
  mojo::PendingRemote<network::mojom::URLLoader> receiver;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client;
  seller_worklet->url_loader_factory()->CreateLoaderAndStart(
      receiver.InitWithNewPipeAndPassReceiver(), /*request_id=*/0,
      /*options=*/0, request, client.InitWithNewPipeAndPassRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  seller_worklet->url_loader_factory().FlushForTesting();
  EXPECT_FALSE(seller_worklet->url_loader_factory().is_connected());
  EXPECT_EQ(std::size(kAllowedUrls),
            url_loader_factory_.pending_requests()->size());
  EXPECT_EQ("Unexpected request", TakeBadMessage());
}

// Test a seller worklet can be correctly requested with a valid coordinator
// when `kFledgeTrustedSignalsKVv2Support` is disabled. It depends on
// `GetBiddingAndAuctionServerKey()` to cause a crash if it is called
// unexpectedly.
TEST_F(AuctionWorkletManagerTest, SellerWorkletWithKVv2FeatureDisabled) {
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
  base::test::TestFuture<void> worklet_available;

  auction_worklet_manager_->RequestSellerWorklet(
      kAuction1, kDecisionLogicUrl, kTrustedSignalsUrl,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_scoring_signals_coordinator=*/
      url::Origin::Create(GURL("https://origin.test/")),
      worklet_available.GetCallback(), NeverInvokedFatalErrorCallback(), handle,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());

  ASSERT_TRUE(worklet_available.Wait());
  EXPECT_TRUE(handle->GetSellerWorklet());
  std::unique_ptr<MockSellerWorklet> seller_worklet =
      auction_process_manager_.WaitForSellerWorklet();
  EXPECT_EQ(kDecisionLogicUrl, seller_worklet->script_source_url());
  EXPECT_EQ(kTrustedSignalsUrl, seller_worklet->trusted_scoring_signals_url());
}

TEST(WorkletKeyTest, HashConsistentForEqualKeys) {
  AuctionWorkletManager::WorkletKey key1(
      AuctionWorkletManager::WorkletType::kBidder,
      GURL("https://example.test/script_url"),
      GURL("https://example.test/wasm_url"),
      GURL("https://example.test/signals_url"),
      /*needs_cors_for_additional_bid=*/
      false, 0x85u,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_signals_coordinator=*/std::nullopt);

  AuctionWorkletManager::WorkletKey key2(
      AuctionWorkletManager::WorkletType::kBidder,
      GURL("https://example.test/script_url"),
      GURL("https://example.test/wasm_url"),
      GURL("https://example.test/signals_url"),
      /*needs_cors_for_additional_bid=*/false, 0x85u,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_signals_coordinator=*/std::nullopt);

  EXPECT_FALSE(key1 < key2);
  EXPECT_FALSE(key2 < key1);
  EXPECT_EQ(key1.GetHash(), key2.GetHash());
}

TEST(WorkletKeyTest, HashIsDifferentForKeysWithDifferentType) {
  AuctionWorkletManager::WorkletKey key1(
      AuctionWorkletManager::WorkletType::kBidder,
      GURL("https://example.test/script_url"),
      GURL("https://example.test/wasm_url"),
      GURL("https://example.test/signals_url"),
      /*needs_cors_for_additional_bid=*/false, 0x85u,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_signals_coordinator=*/std::nullopt);

  AuctionWorkletManager::WorkletKey key2(
      AuctionWorkletManager::WorkletType::kSeller,
      GURL("https://example.test/script_url"),
      GURL("https://example.test/wasm_url"),
      GURL("https://example.test/signals_url"),
      /*needs_cors_for_additional_bid=*/false, 0x85u,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_signals_coordinator=*/std::nullopt);

  EXPECT_TRUE(key1 < key2 || key2 < key1);
  EXPECT_NE(key1.GetHash(), key2.GetHash());
}

TEST(WorkletKeyTest, HashIsDifferentForKeysWithDifferentScriptUrl) {
  AuctionWorkletManager::WorkletKey key1(
      AuctionWorkletManager::WorkletType::kBidder,
      GURL("https://example.test/script_url"),
      GURL("https://example.test/wasm_url"),
      GURL("https://example.test/signals_url"),
      /*needs_cors_for_additional_bid=*/false, 0x85u,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_signals_coordinator=*/std::nullopt);

  AuctionWorkletManager::WorkletKey key2(
      AuctionWorkletManager::WorkletType::kBidder,
      GURL("https://different.example.test/script_url"),
      GURL("https://example.test/wasm_url"),
      GURL("https://example.test/signals_url"),
      /*needs_cors_for_additional_bid=*/false, 0x85u,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_signals_coordinator=*/std::nullopt);

  EXPECT_TRUE(key1 < key2 || key2 < key1);
  EXPECT_NE(key1.GetHash(), key2.GetHash());
}

TEST(WorkletKeyTest, HashIsDifferentForKeysWithDifferentWasmUrl) {
  AuctionWorkletManager::WorkletKey key1(
      AuctionWorkletManager::WorkletType::kBidder,
      GURL("https://example.test/script_url"),
      GURL("https://example.test/wasm_url"),
      GURL("https://example.test/signals_url"),
      /*needs_cors_for_additional_bid=*/false, 0x85u,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_signals_coordinator=*/std::nullopt);

  AuctionWorkletManager::WorkletKey key2(
      AuctionWorkletManager::WorkletType::kBidder,
      GURL("https://example.test/script_url"),
      GURL("https://different.example.test/wasm_url"),
      GURL("https://example.test/signals_url"),
      /*needs_cors_for_additional_bid=*/false, 0x85u,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_signals_coordinator=*/std::nullopt);

  EXPECT_TRUE(key1 < key2 || key2 < key1);
  EXPECT_NE(key1.GetHash(), key2.GetHash());
}

TEST(WorkletKeyTest, HashIsDifferentWhenGivenNullOptWasmUrl) {
  AuctionWorkletManager::WorkletKey key1(
      AuctionWorkletManager::WorkletType::kBidder,
      GURL("https://example.test/script_url"),
      GURL("https://example.test/wasm_url"),
      GURL("https://example.test/signals_url"),
      /*needs_cors_for_additional_bid=*/false, 0x85u,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_signals_coordinator=*/std::nullopt);

  AuctionWorkletManager::WorkletKey key2(
      AuctionWorkletManager::WorkletType::kBidder,
      GURL("https://example.test/script_url"), std::nullopt,
      GURL("https://example.test/signals_url"),
      /*needs_cors_for_additional_bid=*/false, 0x85u,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_signals_coordinator=*/std::nullopt);

  EXPECT_TRUE(key1 < key2 || key2 < key1);
  EXPECT_NE(key1.GetHash(), key2.GetHash());
}

TEST(WorkletKeyTest, HashIsDifferentForKeysWithDifferentSignalsUrl) {
  AuctionWorkletManager::WorkletKey key1(
      AuctionWorkletManager::WorkletType::kBidder,
      GURL("https://example.test/script_url"),
      GURL("https://example.test/wasm_url"),
      GURL("https://example.test/signals_url"),
      /*needs_cors_for_additional_bid=*/false, 0x85u,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_signals_coordinator=*/std::nullopt);

  AuctionWorkletManager::WorkletKey key2(
      AuctionWorkletManager::WorkletType::kBidder,
      GURL("https://example.test/script_url"),
      GURL("https://example.test/wasm_url"),
      GURL("https://different.example.test/signals_url"),
      /*needs_cors_for_additional_bid=*/false, 0x85u,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_signals_coordinator=*/std::nullopt);

  EXPECT_TRUE(key1 < key2 || key2 < key1);
  EXPECT_NE(key1.GetHash(), key2.GetHash());
}

TEST(WorkletKeyTest, HashIsDifferentWhenGivenNullOptSignalsUrl) {
  AuctionWorkletManager::WorkletKey key1(
      AuctionWorkletManager::WorkletType::kBidder,
      GURL("https://example.test/script_url"),
      GURL("https://example.test/wasm_url"),
      GURL("https://example.test/signals_url"),
      /*needs_cors_for_additional_bid=*/false, 0x85u,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_signals_coordinator=*/std::nullopt);

  AuctionWorkletManager::WorkletKey key2(
      AuctionWorkletManager::WorkletType::kBidder,
      GURL("https://example.test/script_url"),
      GURL("https://example.test/wasm_url"), std::nullopt,
      /*needs_cors_for_additional_bid=*/false, 0x85u,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_signals_coordinator=*/std::nullopt);

  EXPECT_TRUE(key1 < key2 || key2 < key1);
  EXPECT_NE(key1.GetHash(), key2.GetHash());
}

TEST(WorkletKeyTest, HashIsDifferentForKeysWithDifferentExperiment) {
  AuctionWorkletManager::WorkletKey key1(
      AuctionWorkletManager::WorkletType::kBidder,
      GURL("https://example.test/script_url"),
      GURL("https://example.test/wasm_url"),
      GURL("https://example.test/signals_url"),
      /*needs_cors_for_additional_bid=*/false, 0x85u,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_signals_coordinator=*/std::nullopt);

  AuctionWorkletManager::WorkletKey key2(
      AuctionWorkletManager::WorkletType::kBidder,
      GURL("https://example.test/script_url"),
      GURL("https://example.test/wasm_url"),
      GURL("https://example.test/signals_url"),
      /*needs_cors_for_additional_bid=*/false, 0x48u,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_signals_coordinator=*/std::nullopt);

  EXPECT_TRUE(key1 < key2 || key2 < key1);
  EXPECT_NE(key1.GetHash(), key2.GetHash());
}

TEST(WorkletKeyTest, HashIsDifferentWhenGivenNullOptExperiment) {
  AuctionWorkletManager::WorkletKey key1(
      AuctionWorkletManager::WorkletType::kBidder,
      GURL("https://example.test/script_url"),
      GURL("https://example.test/wasm_url"),
      GURL("https://example.test/signals_url"),
      /*needs_cors_for_additional_bid=*/false, 0x85u,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_signals_coordinator=*/std::nullopt);

  AuctionWorkletManager::WorkletKey key2(
      AuctionWorkletManager::WorkletType::kBidder,
      GURL("https://example.test/script_url"),
      GURL("https://example.test/wasm_url"),
      GURL("https://example.test/signals_url"),
      /*needs_cors_for_additional_bid=*/false, std::nullopt,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_signals_coordinator=*/std::nullopt);

  EXPECT_TRUE(key1 < key2 || key2 < key1);
  EXPECT_NE(key1.GetHash(), key2.GetHash());
}

TEST(WorkletKeyTest, HashIsDifferentForKeysWithDifferentCORSForAdditionalBid) {
  AuctionWorkletManager::WorkletKey key1(
      AuctionWorkletManager::WorkletType::kBidder,
      GURL("https://example.test/script_url"),
      GURL("https://example.test/wasm_url"),
      GURL("https://example.test/signals_url"),
      /*needs_cors_for_additional_bid=*/false, 0x85u,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_signals_coordinator=*/std::nullopt);

  AuctionWorkletManager::WorkletKey key2(
      AuctionWorkletManager::WorkletType::kBidder,
      GURL("https://example.test/script_url"),
      GURL("https://example.test/wasm_url"),
      GURL("https://example.test/signals_url"),
      /*needs_cors_for_additional_bid=*/true, 0x85u,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_signals_coordinator=*/std::nullopt);

  EXPECT_TRUE(key1 < key2 || key2 < key1);
  EXPECT_NE(key1.GetHash(), key2.GetHash());
}

TEST(WorkletKeyTest, HashIsSameForDifferentSlotSizeParamWhenNoSignalsUrl) {
  AuctionWorkletManager::WorkletKey key1(
      AuctionWorkletManager::WorkletType::kBidder,
      GURL("https://example.test/script_url"),
      GURL("https://example.test/wasm_url"), /*signals_url=*/std::nullopt,
      /*needs_cors_for_additional_bid=*/false, 0x85u,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_signals_coordinator=*/std::nullopt);

  AuctionWorkletManager::WorkletKey key2(
      AuctionWorkletManager::WorkletType::kBidder,
      GURL("https://example.test/script_url"),
      GURL("https://example.test/wasm_url"), /*signals_url=*/std::nullopt,
      /*needs_cors_for_additional_bid=*/true, 0x85u,
      /*trusted_bidding_signals_slot_size_param=*/"foo=bar",
      /*trusted_signals_coordinator=*/std::nullopt);

  EXPECT_TRUE(key1 < key2 || key2 < key1);
  EXPECT_NE(key1.GetHash(), key2.GetHash());
}

TEST(WorkletKeyTest, HashIsDifferentForDifferentSlotSizeParamWithSignalsUrl) {
  AuctionWorkletManager::WorkletKey key1(
      AuctionWorkletManager::WorkletType::kBidder,
      GURL("https://example.test/script_url"),
      GURL("https://example.test/wasm_url"),
      GURL("https://example.test/signals_url"),
      /*needs_cors_for_additional_bid=*/false, 0x85u,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_signals_coordinator=*/std::nullopt);

  AuctionWorkletManager::WorkletKey key2(
      AuctionWorkletManager::WorkletType::kBidder,
      GURL("https://example.test/script_url"),
      GURL("https://example.test/wasm_url"),
      GURL("https://example.test/signals_url"),
      /*needs_cors_for_additional_bid=*/true, 0x85u,
      /*trusted_bidding_signals_slot_size_param=*/"foo=bar",
      /*trusted_signals_coordinator=*/std::nullopt);

  EXPECT_TRUE(key1 < key2 || key2 < key1);
  EXPECT_NE(key1.GetHash(), key2.GetHash());
}

TEST(WorkletKeyTest, HashIsDifferentForKeysWithDifferentCoordinator) {
  AuctionWorkletManager::WorkletKey key1(
      AuctionWorkletManager::WorkletType::kBidder,
      GURL("https://example.test/script_url"),
      GURL("https://example.test/wasm_url"),
      GURL("https://example.test/signals_url"),
      /*needs_cors_for_additional_bid=*/false, 0x85u,
      /*trusted_bidding_signals_slot_size_param=*/"",
      url::Origin::Create(GURL("https://foo.test")));

  AuctionWorkletManager::WorkletKey key2(
      AuctionWorkletManager::WorkletType::kBidder,
      GURL("https://example.test/script_url"),
      GURL("https://example.test/wasm_url"),
      GURL("https://example.test/signals_url"),
      /*needs_cors_for_additional_bid=*/false, 0x85u,
      /*trusted_bidding_signals_slot_size_param=*/"",
      url::Origin::Create(GURL("https://bar.test")));

  EXPECT_TRUE(key1 < key2 || key2 < key1);
  EXPECT_NE(key1.GetHash(), key2.GetHash());
}

TEST(WorkletKeyTest, HashIsDifferentWhenGivenNullOptCoordinator) {
  AuctionWorkletManager::WorkletKey key1(
      AuctionWorkletManager::WorkletType::kBidder,
      GURL("https://example.test/script_url"),
      GURL("https://example.test/wasm_url"),
      GURL("https://example.test/signals_url"),
      /*needs_cors_for_additional_bid=*/false, 0x85u,
      /*trusted_bidding_signals_slot_size_param=*/"",
      url::Origin::Create(GURL("https://foo.test")));

  AuctionWorkletManager::WorkletKey key2(
      AuctionWorkletManager::WorkletType::kBidder,
      GURL("https://example.test/script_url"),
      GURL("https://example.test/wasm_url"),
      GURL("https://example.test/signals_url"),
      /*needs_cors_for_additional_bid=*/false, 0x85u,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_signals_coordinator=*/std::nullopt);

  EXPECT_TRUE(key1 < key2 || key2 < key1);
  EXPECT_NE(key1.GetHash(), key2.GetHash());
}

TEST_F(AuctionWorkletManagerTest,
       DoesNotCrashWhenProcessReadyAfterWorkletDestroyed) {
  auction_process_manager_.DeferOnLaunchedForHandles();

  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
  base::test::TestFuture<void> worklet_available;
  auction_worklet_manager_->RequestBidderWorklet(
      kAuction1, kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
      /*needs_cors_for_additional_bid=*/false,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_bidding_signals_coordinator=*/std::nullopt,
      worklet_available.GetCallback(), NeverInvokedFatalErrorCallback(), handle,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available.Wait());
  EXPECT_TRUE(handle->GetBidderWorklet());
  std::unique_ptr<MockBidderWorklet> bidder_worklet =
      auction_process_manager_.WaitForBidderWorklet();

  bidder_worklet.reset();

  handle.reset();
  auction_process_manager_.CallOnLaunchedWithPidForAllHandles();
}

class AuctionWorkletManagerKVv2Test : public AuctionWorkletManagerTest {
 public:
  AuctionWorkletManagerKVv2Test() {
    feature_list_.InitAndEnableFeature(
        blink::features::kFledgeTrustedSignalsKVv2Support);
  }

  ~AuctionWorkletManagerKVv2Test() override { DCHECK(!fetch_key_callback_); }

  void GetBiddingAndAuctionServerKey(
      const std::optional<url::Origin>& coordinator,
      base::OnceCallback<void(base::expected<BiddingAndAuctionServerKey,
                                             std::string>)> callback) override {
    DCHECK(!fetch_key_callback_);

    if (synchronous_fetch_) {
      std::move(callback).Run(key_);
    } else {
      fetch_key_callback_ = std::move(callback);
    }
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  url::Origin coordinator_{
      url::Origin::Create(GURL("https://coordinator.test/"))};

  base::OnceCallback<void(
      base::expected<BiddingAndAuctionServerKey, std::string>)>
      fetch_key_callback_;

  bool synchronous_fetch_ = true;
  base::expected<BiddingAndAuctionServerKey, std::string> key_{
      BiddingAndAuctionServerKey("public-key", /*id=*/0)};
};

TEST_F(AuctionWorkletManagerKVv2Test,
       SingleBidderWorkletSyncFetchedKeyBeforeProcessAssigned) {
  std::vector<base::expected<BiddingAndAuctionServerKey, std::string>>
      expected_keys = {BiddingAndAuctionServerKey("public-key", /*id=*/0),
                       base::unexpected("Failed to fetch public key.")};

  for (const auto& key : expected_keys) {
    key_ = key;
    std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
    base::test::TestFuture<void> worklet_available;

    auction_worklet_manager_->RequestBidderWorklet(
        kAuction1, kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
        /*needs_cors_for_additional_bid=*/false,
        /*experiment_group_id=*/std::nullopt,
        /*trusted_bidding_signals_slot_size_param=*/"", coordinator_,
        worklet_available.GetCallback(), NeverInvokedFatalErrorCallback(),
        handle,
        auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());

    ASSERT_TRUE(worklet_available.Wait());
    EXPECT_TRUE(handle->GetBidderWorklet());
    std::unique_ptr<MockBidderWorklet> bidder_worklet =
        auction_process_manager_.WaitForBidderWorklet();
    EXPECT_EQ(kDecisionLogicUrl, bidder_worklet->script_source_url());
    EXPECT_EQ(kTrustedSignalsUrl,
              bidder_worklet->trusted_bidding_signals_url());
    EXPECT_TRUE(PublicKeyEvaluateHelper(bidder_worklet->public_key(), key_));
  }
}

TEST_F(AuctionWorkletManagerKVv2Test,
       SingleBidderWorkletAsyncFetchedKeyBeforeProcessAssigned) {
  std::vector<base::expected<BiddingAndAuctionServerKey, std::string>>
      expected_keys = {BiddingAndAuctionServerKey("public-key", /*id=*/0),
                       base::unexpected("Failed to fetch public key.")};
  auction_process_manager_.DeferOnLaunchedForHandles();
  synchronous_fetch_ = false;

  for (const auto& key : expected_keys) {
    key_ = key;
    std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
    base::test::TestFuture<void> worklet_available;

    auction_worklet_manager_->RequestBidderWorklet(
        kAuction1, kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
        /*needs_cors_for_additional_bid=*/false,
        /*experiment_group_id=*/std::nullopt,
        /*trusted_bidding_signals_slot_size_param=*/"", coordinator_,
        worklet_available.GetCallback(), NeverInvokedFatalErrorCallback(),
        handle,
        auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
    std::move(fetch_key_callback_).Run(key_);
    auction_process_manager_.CallOnLaunchedWithPidForAllHandles();

    ASSERT_TRUE(worklet_available.Wait());
    EXPECT_TRUE(handle->GetBidderWorklet());
    std::unique_ptr<MockBidderWorklet> bidder_worklet =
        auction_process_manager_.WaitForBidderWorklet();
    EXPECT_EQ(kDecisionLogicUrl, bidder_worklet->script_source_url());
    EXPECT_EQ(kTrustedSignalsUrl,
              bidder_worklet->trusted_bidding_signals_url());
    EXPECT_TRUE(PublicKeyEvaluateHelper(bidder_worklet->public_key(), key_));
  }
}

TEST_F(AuctionWorkletManagerKVv2Test,
       SingleBidderWorkletAsyncFetchedKeyAfterProcessAssigned) {
  std::vector<base::expected<BiddingAndAuctionServerKey, std::string>>
      expected_keys = {BiddingAndAuctionServerKey("public-key", /*id=*/0),
                       base::unexpected("Failed to fetch public key.")};
  auction_process_manager_.DeferOnLaunchedForHandles();
  synchronous_fetch_ = false;

  for (const auto& key : expected_keys) {
    key_ = key;
    std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
    base::test::TestFuture<void> worklet_available;

    auction_worklet_manager_->RequestBidderWorklet(
        kAuction1, kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
        /*needs_cors_for_additional_bid=*/false,
        /*experiment_group_id=*/std::nullopt,
        /*trusted_bidding_signals_slot_size_param=*/"", coordinator_,
        worklet_available.GetCallback(), NeverInvokedFatalErrorCallback(),
        handle,
        auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
    auction_process_manager_.CallOnLaunchedWithPidForAllHandles();
    std::move(fetch_key_callback_).Run(key_);

    ASSERT_TRUE(worklet_available.Wait());
    EXPECT_TRUE(handle->GetBidderWorklet());
    std::unique_ptr<MockBidderWorklet> bidder_worklet =
        auction_process_manager_.WaitForBidderWorklet();
    EXPECT_EQ(kDecisionLogicUrl, bidder_worklet->script_source_url());
    EXPECT_EQ(kTrustedSignalsUrl,
              bidder_worklet->trusted_bidding_signals_url());
    EXPECT_TRUE(PublicKeyEvaluateHelper(bidder_worklet->public_key(), key_));
  }
}

// Make sure that worklets are not reused when parameters don't match.
TEST_F(AuctionWorkletManagerKVv2Test,
       DifferentBidderWorkletsWithDifferentCoordinators) {
  // Load a KVv2 bidder worklet.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle1;
  base::test::TestFuture<void> worklet_available1;
  auction_worklet_manager_->RequestBidderWorklet(
      kAuction1, kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
      /*needs_cors_for_additional_bid=*/false,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_bidding_signals_coordinator=*/
      url::Origin::Create(GURL("https://a.test/")),
      worklet_available1.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle1,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available1.Wait());
  EXPECT_TRUE(handle1->GetBidderWorklet());
  std::unique_ptr<MockBidderWorklet> bidder_worklet1 =
      auction_process_manager_.WaitForBidderWorklet();
  EXPECT_EQ(kDecisionLogicUrl, bidder_worklet1->script_source_url());
  EXPECT_EQ(kTrustedSignalsUrl, bidder_worklet1->trusted_bidding_signals_url());
  EXPECT_TRUE(PublicKeyEvaluateHelper(bidder_worklet1->public_key(), key_));
  // Should only be one process.
  EXPECT_EQ(1u, auction_process_manager_.GetBidderProcessCountForTesting());
  EXPECT_THAT(handle1->GetDevtoolsAuctionIdsForTesting(),
              UnorderedElementsAre(kAuction1));

  // Load a KVv2 bidder worklet with a different coorinator. A new worklet
  // should be created, using the same process.
  const GURL kDifferentDecisionLogicUrl =
      GURL("https://origin.test/different_script");
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle2;
  base::test::TestFuture<void> worklet_available2;
  auction_worklet_manager_->RequestBidderWorklet(
      kAuction2, kDifferentDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
      /*needs_cors_for_additional_bid=*/false,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_bidding_signals_slot_size_param=*/"",
      url::Origin::Create(GURL("https://b.test/")),
      worklet_available2.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle2,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available2.Wait());
  EXPECT_TRUE(handle1->GetBidderWorklet());
  EXPECT_NE(handle1->GetBidderWorklet(), handle2->GetBidderWorklet());
  std::unique_ptr<MockBidderWorklet> bidder_worklet2 =
      auction_process_manager_.WaitForBidderWorklet();
  EXPECT_EQ(kDifferentDecisionLogicUrl, bidder_worklet2->script_source_url());
  EXPECT_EQ(kTrustedSignalsUrl, bidder_worklet2->trusted_bidding_signals_url());
  EXPECT_TRUE(PublicKeyEvaluateHelper(bidder_worklet2->public_key(), key_));
  // Should still only be one process.
  EXPECT_EQ(1u, auction_process_manager_.GetBidderProcessCountForTesting());
  EXPECT_THAT(handle2->GetDevtoolsAuctionIdsForTesting(),
              UnorderedElementsAre(kAuction2));
}

// Test that requests with the same parameters reuse bidder worklets.
TEST_F(AuctionWorkletManagerKVv2Test, ReuseBidderWorklet) {
  // Load a KVv2 bidder worklet.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle1;
  base::test::TestFuture<void> worklet_available1;

  auction_worklet_manager_->RequestBidderWorklet(
      kAuction1, kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
      /*needs_cors_for_additional_bid=*/false,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_bidding_signals_slot_size_param=*/"", coordinator_,
      worklet_available1.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle1,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());

  ASSERT_TRUE(worklet_available1.Wait());
  EXPECT_TRUE(handle1->GetBidderWorklet());
  std::unique_ptr<MockBidderWorklet> bidder_worklet1 =
      auction_process_manager_.WaitForBidderWorklet();
  EXPECT_EQ(kDecisionLogicUrl, bidder_worklet1->script_source_url());
  EXPECT_EQ(kTrustedSignalsUrl, bidder_worklet1->trusted_bidding_signals_url());
  EXPECT_TRUE(PublicKeyEvaluateHelper(bidder_worklet1->public_key(), key_));
  handle1->GetBidderWorklet()->SendPendingSignalsRequests();
  bidder_worklet1->WaitForSendPendingSignalsRequests(1);
  // Should only be one process.
  EXPECT_EQ(1u, auction_process_manager_.GetBidderProcessCountForTesting());
  EXPECT_THAT(handle1->GetDevtoolsAuctionIdsForTesting(),
              UnorderedElementsAre(kAuction1));

  // Load a KVv2 bidder worklet with the same parameters. The worklet should be
  // reused.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle2;
  base::test::TestFuture<void> worklet_available2;

  auction_worklet_manager_->RequestBidderWorklet(
      kAuction2, kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
      /*needs_cors_for_additional_bid=*/false,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_bidding_signals_slot_size_param=*/"", coordinator_,
      worklet_available2.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle2,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());

  ASSERT_TRUE(worklet_available2.Wait());
  EXPECT_EQ(handle1->GetBidderWorklet(), handle2->GetBidderWorklet());
  EXPECT_FALSE(auction_process_manager_.HasBidderWorkletRequest());
  handle2->GetBidderWorklet()->SendPendingSignalsRequests();
  bidder_worklet1->WaitForSendPendingSignalsRequests(2);
  // Should still only be one process.
  EXPECT_EQ(1u, auction_process_manager_.GetBidderProcessCountForTesting());
  // ... but used by both auctions.
  EXPECT_THAT(handle2->GetDevtoolsAuctionIdsForTesting(),
              UnorderedElementsAre(kAuction1, kAuction2));

  // Close original handle. Worklet should still be alive, and so should its
  // process.
  handle1.reset();
  EXPECT_EQ(1u, auction_process_manager_.GetBidderProcessCountForTesting());
  // We should no longer attribute its work to the first auction, however.
  EXPECT_THAT(handle2->GetDevtoolsAuctionIdsForTesting(),
              UnorderedElementsAre(kAuction2));

  // Load a KVv2 bidder worklet with the same parameters. The worklet should
  // still be reused again.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle3;
  base::test::TestFuture<void> worklet_available3;
  auction_worklet_manager_->RequestBidderWorklet(
      kAuction3, kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
      /*needs_cors_for_additional_bid=*/false,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_bidding_signals_slot_size_param=*/"", coordinator_,
      worklet_available3.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle3,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());

  ASSERT_TRUE(worklet_available3.Wait());
  EXPECT_EQ(handle2->GetBidderWorklet(), handle3->GetBidderWorklet());
  EXPECT_FALSE(auction_process_manager_.HasBidderWorkletRequest());
  handle3->GetBidderWorklet()->SendPendingSignalsRequests();
  bidder_worklet1->WaitForSendPendingSignalsRequests(3);
  // Should still only be one process.
  EXPECT_EQ(1u, auction_process_manager_.GetBidderProcessCountForTesting());
  EXPECT_THAT(handle3->GetDevtoolsAuctionIdsForTesting(),
              UnorderedElementsAre(kAuction2, kAuction3));

  // Close both remaining handles.
  handle2.reset();
  handle3.reset();

  // Process should be destroyed.
  EXPECT_EQ(0u, auction_process_manager_.GetBidderProcessCountForTesting());

  // Request another KVv2 bidder worklet. A new BidderWorklet in a new process
  // should be created.
  base::test::TestFuture<void> worklet_available4;
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle4;
  auction_worklet_manager_->RequestBidderWorklet(
      kAuction4, kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
      /*needs_cors_for_additional_bid=*/false,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_bidding_signals_slot_size_param=*/"", coordinator_,
      worklet_available4.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle4,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available4.Wait());
  EXPECT_TRUE(handle4->GetBidderWorklet());
  std::unique_ptr<MockBidderWorklet> bidder_worklet2 =
      auction_process_manager_.WaitForBidderWorklet();
  EXPECT_EQ(kDecisionLogicUrl, bidder_worklet2->script_source_url());
  EXPECT_EQ(kTrustedSignalsUrl, bidder_worklet2->trusted_bidding_signals_url());
  EXPECT_TRUE(PublicKeyEvaluateHelper(bidder_worklet2->public_key(), key_));
  handle4->GetBidderWorklet()->SendPendingSignalsRequests();
  bidder_worklet2->WaitForSendPendingSignalsRequests(1);
  EXPECT_EQ(1u, auction_process_manager_.GetBidderProcessCountForTesting());
  EXPECT_THAT(handle4->GetDevtoolsAuctionIdsForTesting(),
              UnorderedElementsAre(kAuction4));
}

// Test a bidder worklet can be correctly requested when coordinator is empty
// and `kFledgeTrustedSignalsKVv2Support` is enabled.
TEST_F(AuctionWorkletManagerKVv2Test, BidderWorkletWithoutCoordinator) {
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
  base::test::TestFuture<void> worklet_available;

  auction_worklet_manager_->RequestBidderWorklet(
      kAuction1, kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
      /*needs_cors_for_additional_bid=*/false,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_bidding_signals_slot_size_param=*/"",
      /*trusted_bidding_signals_coordinator=*/std::nullopt,
      worklet_available.GetCallback(), NeverInvokedFatalErrorCallback(), handle,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());

  ASSERT_TRUE(worklet_available.Wait());
  EXPECT_TRUE(handle->GetBidderWorklet());
  std::unique_ptr<MockBidderWorklet> bidder_worklet =
      auction_process_manager_.WaitForBidderWorklet();
  EXPECT_EQ(kDecisionLogicUrl, bidder_worklet->script_source_url());
  EXPECT_EQ(kTrustedSignalsUrl, bidder_worklet->trusted_bidding_signals_url());
  EXPECT_TRUE(!bidder_worklet->public_key());
}

TEST_F(AuctionWorkletManagerKVv2Test,
       SingleSellerWorkletSyncFetchedKeyBeforeProcessAssigned) {
  std::vector<base::expected<BiddingAndAuctionServerKey, std::string>>
      expected_keys = {BiddingAndAuctionServerKey("public-key", /*id=*/0),
                       base::unexpected("Failed to fetch public key.")};

  for (const auto& key : expected_keys) {
    key_ = key;
    std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
    base::test::TestFuture<void> worklet_available;

    auction_worklet_manager_->RequestSellerWorklet(
        kAuction1, kDecisionLogicUrl, kTrustedSignalsUrl,
        /*experiment_group_id=*/std::nullopt,
        /*trusted_scoring_signals_coordinator=*/
        url::Origin::Create(GURL("https://origin.test/")),
        worklet_available.GetCallback(), NeverInvokedFatalErrorCallback(),
        handle,
        auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());

    ASSERT_TRUE(worklet_available.Wait());
    EXPECT_TRUE(handle->GetSellerWorklet());
    std::unique_ptr<MockSellerWorklet> seller_worklet =
        auction_process_manager_.WaitForSellerWorklet();
    EXPECT_EQ(kDecisionLogicUrl, seller_worklet->script_source_url());
    EXPECT_EQ(kTrustedSignalsUrl,
              seller_worklet->trusted_scoring_signals_url());
    EXPECT_TRUE(PublicKeyEvaluateHelper(seller_worklet->public_key(), key_));
  }
}

TEST_F(AuctionWorkletManagerKVv2Test,
       SingleSellerWorkletAsyncFetchedKeyBeforeProcessAssigned) {
  std::vector<base::expected<BiddingAndAuctionServerKey, std::string>>
      expected_keys = {BiddingAndAuctionServerKey("public-key", /*id=*/0),
                       base::unexpected("Failed to fetch public key.")};
  auction_process_manager_.DeferOnLaunchedForHandles();
  synchronous_fetch_ = false;

  for (const auto& key : expected_keys) {
    key_ = key;
    std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
    base::test::TestFuture<void> worklet_available;

    auction_worklet_manager_->RequestSellerWorklet(
        kAuction1, kDecisionLogicUrl, kTrustedSignalsUrl,
        /*experiment_group_id=*/std::nullopt,
        /*trusted_scoring_signals_coordinator=*/
        url::Origin::Create(GURL("https://origin.test/")),
        worklet_available.GetCallback(), NeverInvokedFatalErrorCallback(),
        handle,
        auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
    std::move(fetch_key_callback_).Run(key_);
    auction_process_manager_.CallOnLaunchedWithPidForAllHandles();

    ASSERT_TRUE(worklet_available.Wait());
    EXPECT_TRUE(handle->GetSellerWorklet());
    std::unique_ptr<MockSellerWorklet> seller_worklet =
        auction_process_manager_.WaitForSellerWorklet();
    EXPECT_EQ(kDecisionLogicUrl, seller_worklet->script_source_url());
    EXPECT_EQ(kTrustedSignalsUrl,
              seller_worklet->trusted_scoring_signals_url());
    EXPECT_TRUE(PublicKeyEvaluateHelper(seller_worklet->public_key(), key_));
  }
}

TEST_F(AuctionWorkletManagerKVv2Test,
       SingleSellerWorkletAsyncFetchedKeyAfterProcessAssigned) {
  std::vector<base::expected<BiddingAndAuctionServerKey, std::string>>
      expected_keys = {BiddingAndAuctionServerKey("public-key", /*id=*/0),
                       base::unexpected("Failed to fetch public key.")};
  auction_process_manager_.DeferOnLaunchedForHandles();
  synchronous_fetch_ = false;

  for (const auto& key : expected_keys) {
    key_ = key;
    std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
    base::test::TestFuture<void> worklet_available;

    auction_worklet_manager_->RequestSellerWorklet(
        kAuction1, kDecisionLogicUrl, kTrustedSignalsUrl,
        /*experiment_group_id=*/std::nullopt,
        /*trusted_scoring_signals_coordinator=*/
        url::Origin::Create(GURL("https://origin.test/")),
        worklet_available.GetCallback(), NeverInvokedFatalErrorCallback(),
        handle,
        auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
    auction_process_manager_.CallOnLaunchedWithPidForAllHandles();
    std::move(fetch_key_callback_).Run(key_);

    ASSERT_TRUE(worklet_available.Wait());
    EXPECT_TRUE(handle->GetSellerWorklet());
    std::unique_ptr<MockSellerWorklet> seller_worklet =
        auction_process_manager_.WaitForSellerWorklet();
    EXPECT_EQ(kDecisionLogicUrl, seller_worklet->script_source_url());
    EXPECT_EQ(kTrustedSignalsUrl,
              seller_worklet->trusted_scoring_signals_url());
    EXPECT_TRUE(PublicKeyEvaluateHelper(seller_worklet->public_key(), key_));
  }
}

// Make sure that worklets are not reused when parameters don't match.
TEST_F(AuctionWorkletManagerKVv2Test,
       DifferentSellerWorkletsWithDifferentCoordinators) {
  // Load a KVv2 bidder worklet.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle1;
  base::test::TestFuture<void> worklet_available1;
  auction_worklet_manager_->RequestSellerWorklet(
      kAuction1, kDecisionLogicUrl, kTrustedSignalsUrl,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_scoring_signals_coordinator=*/
      url::Origin::Create(GURL("https://a.test/")),
      worklet_available1.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle1,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available1.Wait());
  EXPECT_TRUE(handle1->GetSellerWorklet());
  std::unique_ptr<MockSellerWorklet> seller_worklet1 =
      auction_process_manager_.WaitForSellerWorklet();
  EXPECT_EQ(kDecisionLogicUrl, seller_worklet1->script_source_url());
  EXPECT_EQ(kTrustedSignalsUrl, seller_worklet1->trusted_scoring_signals_url());
  EXPECT_TRUE(PublicKeyEvaluateHelper(seller_worklet1->public_key(), key_));
  // Should only be one process.
  EXPECT_EQ(1u, auction_process_manager_.GetSellerProcessCountForTesting());
  EXPECT_THAT(handle1->GetDevtoolsAuctionIdsForTesting(),
              UnorderedElementsAre(kAuction1));

  // Load a KVv2 seller worklet with a different coorinator. A new worklet
  // should be created, using the same process.
  const GURL kDifferentDecisionLogicUrl =
      GURL("https://origin.test/different_script");
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle2;
  base::test::TestFuture<void> worklet_available2;
  auction_worklet_manager_->RequestSellerWorklet(
      kAuction2, kDifferentDecisionLogicUrl, kTrustedSignalsUrl,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_scoring_signals_coordinator=*/
      url::Origin::Create(GURL("https://b.test/")),
      worklet_available2.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle2,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available2.Wait());
  EXPECT_TRUE(handle1->GetSellerWorklet());
  EXPECT_NE(handle1->GetSellerWorklet(), handle2->GetSellerWorklet());
  std::unique_ptr<MockSellerWorklet> seller_worklet2 =
      auction_process_manager_.WaitForSellerWorklet();
  EXPECT_EQ(kDifferentDecisionLogicUrl, seller_worklet2->script_source_url());
  EXPECT_EQ(kTrustedSignalsUrl, seller_worklet2->trusted_scoring_signals_url());
  EXPECT_TRUE(PublicKeyEvaluateHelper(seller_worklet2->public_key(), key_));
  // Should still only be one process.
  EXPECT_EQ(1u, auction_process_manager_.GetSellerProcessCountForTesting());
  EXPECT_THAT(handle2->GetDevtoolsAuctionIdsForTesting(),
              UnorderedElementsAre(kAuction2));
}

// Test that requests with the same parameters reuse bidder worklets.
TEST_F(AuctionWorkletManagerKVv2Test, ReuseSellerWorklet) {
  // Load a KVv2 seller worklet.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle1;
  base::test::TestFuture<void> worklet_available1;

  auction_worklet_manager_->RequestSellerWorklet(
      kAuction1, kDecisionLogicUrl, kTrustedSignalsUrl,
      /*experiment_group_id=*/std::nullopt, coordinator_,
      worklet_available1.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle1,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());

  ASSERT_TRUE(worklet_available1.Wait());
  EXPECT_TRUE(handle1->GetSellerWorklet());
  std::unique_ptr<MockSellerWorklet> seller_worklet1 =
      auction_process_manager_.WaitForSellerWorklet();
  EXPECT_EQ(kDecisionLogicUrl, seller_worklet1->script_source_url());
  EXPECT_EQ(kTrustedSignalsUrl, seller_worklet1->trusted_scoring_signals_url());
  EXPECT_TRUE(PublicKeyEvaluateHelper(seller_worklet1->public_key(), key_));
  handle1->GetSellerWorklet()->SendPendingSignalsRequests();
  seller_worklet1->WaitForSendPendingSignalsRequests(1);
  // Should only be one process.
  EXPECT_EQ(1u, auction_process_manager_.GetSellerProcessCountForTesting());
  EXPECT_THAT(handle1->GetDevtoolsAuctionIdsForTesting(),
              UnorderedElementsAre(kAuction1));

  // Load a KVv2 seller worklet with the same parameters. The worklet should be
  // reused.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle2;
  base::test::TestFuture<void> worklet_available2;

  auction_worklet_manager_->RequestSellerWorklet(
      kAuction2, kDecisionLogicUrl, kTrustedSignalsUrl,
      /*experiment_group_id=*/std::nullopt, coordinator_,
      worklet_available2.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle2,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());

  ASSERT_TRUE(worklet_available2.Wait());
  EXPECT_EQ(handle1->GetSellerWorklet(), handle2->GetSellerWorklet());
  EXPECT_FALSE(auction_process_manager_.HasSellerWorkletRequest());
  handle2->GetSellerWorklet()->SendPendingSignalsRequests();
  seller_worklet1->WaitForSendPendingSignalsRequests(2);
  // Should still only be one process.
  EXPECT_EQ(1u, auction_process_manager_.GetSellerProcessCountForTesting());
  // ... but used by both auctions.
  EXPECT_THAT(handle2->GetDevtoolsAuctionIdsForTesting(),
              UnorderedElementsAre(kAuction1, kAuction2));

  // Close original handle. Worklet should still be alive, and so should its
  // process.
  handle1.reset();
  EXPECT_EQ(1u, auction_process_manager_.GetSellerProcessCountForTesting());
  // We should no longer attribute its work to the first auction, however.
  EXPECT_THAT(handle2->GetDevtoolsAuctionIdsForTesting(),
              UnorderedElementsAre(kAuction2));

  // Load a KVv2 seller worklet with the same parameters. The worklet should
  // still be reused again.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle3;
  base::test::TestFuture<void> worklet_available3;
  auction_worklet_manager_->RequestSellerWorklet(
      kAuction3, kDecisionLogicUrl, kTrustedSignalsUrl,
      /*experiment_group_id=*/std::nullopt, coordinator_,
      worklet_available3.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle3,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());

  ASSERT_TRUE(worklet_available3.Wait());
  EXPECT_EQ(handle2->GetSellerWorklet(), handle3->GetSellerWorklet());
  EXPECT_FALSE(auction_process_manager_.HasSellerWorkletRequest());
  handle3->GetSellerWorklet()->SendPendingSignalsRequests();
  seller_worklet1->WaitForSendPendingSignalsRequests(3);
  // Should still only be one process.
  EXPECT_EQ(1u, auction_process_manager_.GetSellerProcessCountForTesting());
  EXPECT_THAT(handle3->GetDevtoolsAuctionIdsForTesting(),
              UnorderedElementsAre(kAuction2, kAuction3));

  // Close both remaining handles.
  handle2.reset();
  handle3.reset();

  // Process should be destroyed.
  EXPECT_EQ(0u, auction_process_manager_.GetSellerProcessCountForTesting());

  // Request another KVv2 seller worklet. A new SellerWorklet in a new process
  // should be created.
  base::test::TestFuture<void> worklet_available4;
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle4;
  auction_worklet_manager_->RequestSellerWorklet(
      kAuction4, kDecisionLogicUrl, kTrustedSignalsUrl,
      /*experiment_group_id=*/std::nullopt, coordinator_,
      worklet_available4.GetCallback(), NeverInvokedFatalErrorCallback(),
      handle4,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());
  ASSERT_TRUE(worklet_available4.Wait());
  EXPECT_TRUE(handle4->GetSellerWorklet());
  std::unique_ptr<MockSellerWorklet> seller_worklet2 =
      auction_process_manager_.WaitForSellerWorklet();
  EXPECT_EQ(kDecisionLogicUrl, seller_worklet2->script_source_url());
  EXPECT_EQ(kTrustedSignalsUrl, seller_worklet2->trusted_scoring_signals_url());
  EXPECT_TRUE(PublicKeyEvaluateHelper(seller_worklet2->public_key(), key_));
  handle4->GetSellerWorklet()->SendPendingSignalsRequests();
  seller_worklet2->WaitForSendPendingSignalsRequests(1);
  EXPECT_EQ(1u, auction_process_manager_.GetSellerProcessCountForTesting());
  EXPECT_THAT(handle4->GetDevtoolsAuctionIdsForTesting(),
              UnorderedElementsAre(kAuction4));
}

// Test a seller worklet can be correctly requested when coordinator is empty
// and `kFledgeTrustedSignalsKVv2Support` is enabled.
TEST_F(AuctionWorkletManagerKVv2Test, SellerWorkletWithoutCoordinator) {
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
  base::test::TestFuture<void> worklet_available;

  auction_worklet_manager_->RequestSellerWorklet(
      kAuction1, kDecisionLogicUrl, kTrustedSignalsUrl,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_scoring_signals_coordinator=*/std::nullopt,
      worklet_available.GetCallback(), NeverInvokedFatalErrorCallback(), handle,
      auction_metrics_recorder_manager_->CreateAuctionMetricsRecorder());

  ASSERT_TRUE(worklet_available.Wait());
  EXPECT_TRUE(handle->GetSellerWorklet());
  std::unique_ptr<MockSellerWorklet> seller_worklet =
      auction_process_manager_.WaitForSellerWorklet();
  EXPECT_EQ(kDecisionLogicUrl, seller_worklet->script_source_url());
  EXPECT_EQ(kTrustedSignalsUrl, seller_worklet->trusted_scoring_signals_url());
  EXPECT_TRUE(!seller_worklet->public_key());
}

}  // namespace
}  // namespace content
