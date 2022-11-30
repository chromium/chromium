// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_worklet_manager.h"

#include <stdint.h>

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/browser/interest_group/auction_process_manager.h"
#include "content/browser/interest_group/subresource_url_authorizations.h"
#include "content/browser/interest_group/subresource_url_builder.h"
#include "content/public/browser/site_instance.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

using BundleSubresourceInfo = SubresourceUrlBuilder::BundleSubresourceInfo;

constexpr char kBuyer1OriginStr[] = "https://origin.test";
constexpr char kBuyer2OriginStr[] = "https://origin2.test";

base::OnceClosure NeverInvokedWorkletAvailableCallback() {
  return base::BindOnce([]() { ADD_FAILURE() << "This should not be called"; });
}

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
      const absl::optional<GURL>& wasm_url,
      const absl::optional<GURL>& trusted_bidding_signals_url,
      const url::Origin& top_window_origin)
      : url_loader_factory_(std::move(pending_url_loader_factory)),
        script_source_url_(script_source_url),
        wasm_url_(wasm_url),
        trusted_bidding_signals_url_(trusted_bidding_signals_url),
        top_window_origin_(top_window_origin),
        receiver_(this, std::move(pending_receiver)) {}

  MockBidderWorklet(const MockBidderWorklet&) = delete;
  const MockBidderWorklet& operator=(const MockBidderWorklet&) = delete;

  ~MockBidderWorklet() override {
    // Process any pending SendPendingSignalsRequests() calls.
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(expected_num_send_pending_signals_requests_calls_,
              num_send_pending_signals_requests_calls_);
  }

  // auction_worklet::mojom::SellerWorklet implementation:

  void GenerateBid(
      auction_worklet::mojom::BidderWorkletNonSharedParamsPtr
          bidder_worklet_non_shared_params,
      auction_worklet::mojom::KAnonymityBidMode kanon_mode,
      const url::Origin& interest_group_join_origin,
      const absl::optional<std::string>& auction_signals_json,
      const absl::optional<std::string>& per_buyer_signals_json,
      const absl::optional<GURL>& direct_from_seller_per_buyer_signals,
      const absl::optional<GURL>& direct_from_seller_auction_signals,
      const absl::optional<base::TimeDelta> per_buyer_timeout,
      const url::Origin& browser_signal_seller_origin,
      const absl::optional<url::Origin>& browser_signal_top_level_seller_origin,
      auction_worklet::mojom::BiddingBrowserSignalsPtr bidding_browser_signals,
      base::Time auction_start_time,
      uint64_t trace_id,
      mojo::PendingAssociatedRemote<auction_worklet::mojom::GenerateBidClient>
          generate_bid_client) override {
    NOTREACHED();
  }

  void SendPendingSignalsRequests() override {
    ++num_send_pending_signals_requests_calls_;
    if (num_send_pending_signals_requests_calls_ ==
        expected_num_send_pending_signals_requests_calls_) {
      send_pending_signals_requests_called_loop_->Quit();
    }
  }

  void ReportWin(
      const std::string& interest_group_name,
      const absl::optional<std::string>& auction_signals_json,
      const absl::optional<std::string>& per_buyer_signals_json,
      const absl::optional<GURL>& direct_from_seller_per_buyer_signals,
      const absl::optional<GURL>& direct_from_seller_auction_signals,
      const std::string& seller_signals_json,
      const GURL& browser_signal_render_url,
      double browser_signal_bid,
      double browser_signal_highest_scoring_other_bid,
      bool browser_signal_made_highest_scoring_other_bid,
      const url::Origin& browser_signal_seller_origin,
      const absl::optional<url::Origin>& browser_signal_top_level_seller_origin,
      uint32_t browser_signal_data_version,
      bool browser_signal_has_data_version,
      uint64_t trace_id,
      ReportWinCallback report_win_callback) override {
    NOTREACHED();
  }

  void ConnectDevToolsAgent(
      mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent)
      override {
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
  const absl::optional<GURL>& wasm_url() const { return wasm_url_; }
  const absl::optional<GURL>& trusted_bidding_signals_url() const {
    return trusted_bidding_signals_url_;
  }
  const url::Origin& top_window_origin() const { return top_window_origin_; }

  int num_send_pending_signals_requests_calls() const {
    return num_send_pending_signals_requests_calls_;
  }

 private:
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;

  const GURL script_source_url_;
  const absl::optional<GURL> wasm_url_;
  const absl::optional<GURL> trusted_bidding_signals_url_;
  const url::Origin top_window_origin_;

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
      const absl::optional<GURL>& trusted_scoring_signals_url,
      const url::Origin& top_window_origin)
      : url_loader_factory_(std::move(pending_url_loader_factory)),
        script_source_url_(script_source_url),
        trusted_scoring_signals_url_(trusted_scoring_signals_url),
        top_window_origin_(top_window_origin),
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

  void ScoreAd(const std::string& ad_metadata_json,
               double bid,
               const blink::AuctionConfig::NonSharedParams&
                   auction_ad_config_non_shared_params,
               const absl::optional<GURL>& direct_from_seller_seller_signals,
               const absl::optional<GURL>& direct_from_seller_auction_signals,
               auction_worklet::mojom::ComponentAuctionOtherSellerPtr
                   browser_signals_other_seller,
               const url::Origin& browser_signal_interest_group_owner,
               const GURL& browser_signal_render_url,
               const std::vector<GURL>& browser_signal_ad_components,
               uint32_t browser_signal_bidding_duration_msecs,
               const absl::optional<base::TimeDelta> seller_timeout,
               uint64_t trace_id,
               mojo::PendingRemote<auction_worklet::mojom::ScoreAdClient>
                   score_ad_client) override {
    NOTREACHED();
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
      const absl::optional<GURL>& direct_from_seller_seller_signals,
      const absl::optional<GURL>& direct_from_seller_auction_signals,
      auction_worklet::mojom::ComponentAuctionOtherSellerPtr
          browser_signals_other_seller,
      const url::Origin& browser_signal_interest_group_owner,
      const GURL& browser_signal_render_url,
      double browser_signal_bid,
      double browser_signal_desirability,
      double browser_signal_highest_scoring_other_bid,
      auction_worklet::mojom::ComponentAuctionReportResultParamsPtr
          browser_signals_component_auction_report_result_params,
      uint32_t browser_signal_data_version,
      bool browser_signal_has_data_version,
      uint64_t trace_id,
      ReportResultCallback report_result_callback) override {
    NOTREACHED();
  }

  void ConnectDevToolsAgent(
      mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent)
      override {
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
  const absl::optional<GURL>& trusted_scoring_signals_url() const {
    return trusted_scoring_signals_url_;
  }
  const url::Origin& top_window_origin() const { return top_window_origin_; }

  int num_send_pending_signals_requests_calls() const {
    return num_send_pending_signals_requests_calls_;
  }

 private:
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;

  const GURL script_source_url_;
  const absl::optional<GURL> trusted_scoring_signals_url_;
  const url::Origin top_window_origin_;

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
  RenderProcessHost* LaunchProcess(
      mojo::PendingReceiver<auction_worklet::mojom::AuctionWorkletService>
          auction_worklet_service_receiver,
      const ProcessHandle* process_handle,
      const std::string& display_name) override {
    mojo::ReceiverId receiver_id =
        receiver_set_.Add(this, std::move(auction_worklet_service_receiver));

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
      if (receiver_set_.HasReceiver(receiver.first))
        EXPECT_NE(receiver.second, display_name);
    }

    receiver_display_name_map_[receiver_id] = display_name;
    return nullptr;
  }

  scoped_refptr<SiteInstance> MaybeComputeSiteInstance(
      SiteInstance* frame_site_instance,
      const url::Origin& worklet_origin) override {
    return nullptr;
  }

  bool TryUseSharedProcess(ProcessHandle* process_handle) override {
    return false;
  }

  // auction_worklet::mojom::AuctionWorkletService implementation:

  void LoadBidderWorklet(
      mojo::PendingReceiver<auction_worklet::mojom::BidderWorklet>
          bidder_worklet_receiver,
      bool pause_for_debugger_on_start,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_url_loader_factory,
      const GURL& script_source_url,
      const absl::optional<GURL>& bidding_wasm_helper_url,
      const absl::optional<GURL>& trusted_bidding_signals_url,
      const url::Origin& top_window_origin,
      bool has_experiment_group_id,
      uint16_t experiment_group_id) override {
    DCHECK(!bidder_worklet_);

    // Make sure this request came over the right pipe.
    EXPECT_EQ(receiver_display_name_map_[receiver_set_.current_receiver()],
              ComputeDisplayName(AuctionProcessManager::WorkletType::kBidder,
                                 url::Origin::Create(script_source_url)));

    bidder_worklet_ = std::make_unique<MockBidderWorklet>(
        std::move(bidder_worklet_receiver),
        std::move(pending_url_loader_factory), script_source_url,
        bidding_wasm_helper_url, trusted_bidding_signals_url,
        top_window_origin);

    if (bidder_worklet_run_loop_)
      bidder_worklet_run_loop_->Quit();
  }

  void LoadSellerWorklet(
      mojo::PendingReceiver<auction_worklet::mojom::SellerWorklet>
          seller_worklet_receiver,
      bool should_pause_on_start,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_url_loader_factory,
      const GURL& script_source_url,
      const absl::optional<GURL>& trusted_scoring_signals_url,
      const url::Origin& top_window_origin,
      bool has_experiment_group_id,
      uint16_t experiment_group_id) override {
    DCHECK(!seller_worklet_);

    // Make sure this request came over the right pipe.
    EXPECT_EQ(receiver_display_name_map_[receiver_set_.current_receiver()],
              ComputeDisplayName(AuctionProcessManager::WorkletType::kSeller,
                                 url::Origin::Create(script_source_url)));

    seller_worklet_ = std::make_unique<MockSellerWorklet>(
        std::move(seller_worklet_receiver),
        std::move(pending_url_loader_factory), script_source_url,
        trusted_scoring_signals_url, top_window_origin);

    if (seller_worklet_run_loop_)
      seller_worklet_run_loop_->Quit();
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

  // Map from ReceiverSet IDs to display name when the process was launched.
  // Used to verify that worklets are created in the right process.
  std::map<mojo::ReceiverId, std::string> receiver_display_name_map_;

  // ReceiverSet is last so that destroying `this` while there's a pending
  // callback over the pipe will not DCHECK.
  mojo::ReceiverSet<auction_worklet::mojom::AuctionWorkletService>
      receiver_set_;
};

class AuctionWorkletManagerTest : public testing::Test,
                                  public AuctionWorkletManager::Delegate {
 public:
  AuctionWorkletManagerTest()
      : auction_worklet_manager_(&auction_process_manager_,
                                 kTopWindowOrigin,
                                 kFrameOrigin,
                                 this) {
    mojo::SetDefaultProcessErrorHandler(base::BindRepeating(
        &AuctionWorkletManagerTest::OnBadMessage, base::Unretained(this)));
  }

  ~AuctionWorkletManagerTest() override {
    // Any bad message should have been inspected and cleared before the end of
    // the test.
    EXPECT_EQ(std::string(), bad_message_);
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
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
  RenderFrameHostImpl* GetFrame() override { return nullptr; }
  scoped_refptr<SiteInstance> GetFrameSiteInstance() override {
    return scoped_refptr<SiteInstance>();
  }
  network::mojom::ClientSecurityStatePtr GetClientSecurityState() override {
    return network::mojom::ClientSecurityState::New();
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
  const SubresourceUrlBuilder kEmptySubresourceBuilder{absl::nullopt};
  const SubresourceUrlBuilder kPopulatedSubresourceBuilder{
      PopulateSubresources()};

  base::test::TaskEnvironment task_environment_;

  std::string bad_message_;

  network::TestURLLoaderFactory url_loader_factory_;
  MockAuctionProcessManager auction_process_manager_;
  AuctionWorkletManager auction_worklet_manager_;
};

TEST_F(AuctionWorkletManagerTest, SingleBidderWorklet) {
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
  ASSERT_TRUE(auction_worklet_manager_.RequestBidderWorklet(
      kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
      kPopulatedSubresourceBuilder,
      /*experiment_group_id=*/absl::nullopt,
      NeverInvokedWorkletAvailableCallback(), NeverInvokedFatalErrorCallback(),
      handle));
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
  ASSERT_TRUE(auction_worklet_manager_.RequestSellerWorklet(
      kDecisionLogicUrl, kTrustedSignalsUrl, kPopulatedSubresourceBuilder,
      /*experiment_group_id=*/absl::nullopt,
      NeverInvokedWorkletAvailableCallback(), NeverInvokedFatalErrorCallback(),
      handle));
  EXPECT_TRUE(handle->GetSellerWorklet());

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
  ASSERT_TRUE(auction_worklet_manager_.RequestBidderWorklet(
      kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl, kEmptySubresourceBuilder,
      /*experiment_group_id=*/absl::nullopt,
      NeverInvokedWorkletAvailableCallback(), NeverInvokedFatalErrorCallback(),
      handle));
  EXPECT_TRUE(handle->GetBidderWorklet());

  std::unique_ptr<MockBidderWorklet> bidder_worklet =
      auction_process_manager_.WaitForBidderWorklet();

  EXPECT_TRUE(
      handle->GetSubresourceUrlAuthorizationsForTesting().IsEmptyForTesting());
}

TEST_F(AuctionWorkletManagerTest,
       SingleSellerWorkletEmptyDirectFromSellerSignals) {
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
  ASSERT_TRUE(auction_worklet_manager_.RequestSellerWorklet(
      kDecisionLogicUrl, kTrustedSignalsUrl, kEmptySubresourceBuilder,
      /*experiment_group_id=*/absl::nullopt,
      NeverInvokedWorkletAvailableCallback(), NeverInvokedFatalErrorCallback(),
      handle));
  EXPECT_TRUE(handle->GetSellerWorklet());

  std::unique_ptr<MockSellerWorklet> seller_worklet =
      auction_process_manager_.WaitForSellerWorklet();

  EXPECT_TRUE(
      handle->GetSubresourceUrlAuthorizationsForTesting().IsEmptyForTesting());
}

// Test the case where a bidder worklet request completes asynchronously. This
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
    ASSERT_TRUE(auction_worklet_manager_.RequestBidderWorklet(
        decision_logic_url, /*wasm_url=*/absl::nullopt,
        /*trusted_bidding_signals_url=*/absl::nullopt, kEmptySubresourceBuilder,
        /*experiment_group_id=*/absl::nullopt,
        NeverInvokedWorkletAvailableCallback(),
        NeverInvokedFatalErrorCallback(), handle));
    EXPECT_TRUE(handle->GetBidderWorklet());
    EXPECT_EQ(i + 1,
              auction_process_manager_.GetBidderProcessCountForTesting());

    std::unique_ptr<MockBidderWorklet> bidder_worklet =
        auction_process_manager_.WaitForBidderWorklet();
    EXPECT_EQ(decision_logic_url, bidder_worklet->script_source_url());
    EXPECT_EQ(absl::nullopt, bidder_worklet->wasm_url());
    EXPECT_EQ(absl::nullopt, bidder_worklet->trusted_bidding_signals_url());
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

  // The next request for a distinct bidder worklet should not complete
  // synchronously, since there's no available process quota.
  base::RunLoop worklet_available_loop;
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
  ASSERT_FALSE(auction_worklet_manager_.RequestBidderWorklet(
      kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl, kEmptySubresourceBuilder,
      /*experiment_group_id=*/absl::nullopt,
      worklet_available_loop.QuitClosure(), NeverInvokedFatalErrorCallback(),
      handle));
  EXPECT_EQ(AuctionProcessManager::kMaxBidderProcesses,
            auction_process_manager_.GetBidderProcessCountForTesting());
  EXPECT_FALSE(worklet_available_loop.AnyQuitCalled());

  // Freeing a WorkletHandle should result in a new process being
  // available, and the most recent request getting a new worklet.

  handles.pop_front();
  worklet_available_loop.Run();
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

// Test the case where a seller worklet request completes asynchronously. This
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
    ASSERT_TRUE(auction_worklet_manager_.RequestSellerWorklet(
        decision_logic_url, /*trusted_scoring_signals_url=*/absl::nullopt,
        kEmptySubresourceBuilder,
        /*experiment_group_id=*/absl::nullopt,
        NeverInvokedWorkletAvailableCallback(),
        NeverInvokedFatalErrorCallback(), handle));
    EXPECT_TRUE(handle->GetSellerWorklet());
    EXPECT_EQ(i + 1,
              auction_process_manager_.GetSellerProcessCountForTesting());

    std::unique_ptr<MockSellerWorklet> seller_worklet =
        auction_process_manager_.WaitForSellerWorklet();
    EXPECT_EQ(decision_logic_url, seller_worklet->script_source_url());
    EXPECT_EQ(absl::nullopt, seller_worklet->trusted_scoring_signals_url());
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

  // The next request for a distinct seller worklet should not complete
  // synchronously, since there's no available process quota.
  base::RunLoop worklet_available_loop;
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
  ASSERT_FALSE(auction_worklet_manager_.RequestSellerWorklet(
      kDecisionLogicUrl, kTrustedSignalsUrl, kEmptySubresourceBuilder,
      /*experiment_group_id=*/absl::nullopt,
      worklet_available_loop.QuitClosure(), NeverInvokedFatalErrorCallback(),
      handle));
  EXPECT_EQ(AuctionProcessManager::kMaxSellerProcesses,
            auction_process_manager_.GetSellerProcessCountForTesting());
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
  ASSERT_TRUE(auction_worklet_manager_.RequestBidderWorklet(
      kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl, kEmptySubresourceBuilder,
      /*experiment_group_id=*/absl::nullopt,
      NeverInvokedWorkletAvailableCallback(), NeverInvokedFatalErrorCallback(),
      handle1));
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

  // Load a bidder worklet with the same parameters. The worklet should be
  // reused.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle2;
  ASSERT_TRUE(auction_worklet_manager_.RequestBidderWorklet(
      kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl, kEmptySubresourceBuilder,
      /*experiment_group_id=*/absl::nullopt,
      NeverInvokedWorkletAvailableCallback(), NeverInvokedFatalErrorCallback(),
      handle2));
  EXPECT_EQ(handle1->GetBidderWorklet(), handle2->GetBidderWorklet());
  EXPECT_FALSE(auction_process_manager_.HasBidderWorkletRequest());
  handle2->GetBidderWorklet()->SendPendingSignalsRequests();
  bidder_worklet1->WaitForSendPendingSignalsRequests(2);
  // Should still only be one process.
  EXPECT_EQ(1u, auction_process_manager_.GetBidderProcessCountForTesting());

  // Close original handle. Worklet should still be alive, and so should its
  // process.
  handle1.reset();
  EXPECT_EQ(1u, auction_process_manager_.GetBidderProcessCountForTesting());

  // Load a bidder worklet with the same parameters. The worklet should still be
  // reused again.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle3;
  ASSERT_TRUE(auction_worklet_manager_.RequestBidderWorklet(
      kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl, kEmptySubresourceBuilder,
      /*experiment_group_id=*/absl::nullopt,
      NeverInvokedWorkletAvailableCallback(), NeverInvokedFatalErrorCallback(),
      handle3));
  EXPECT_EQ(handle2->GetBidderWorklet(), handle3->GetBidderWorklet());
  EXPECT_FALSE(auction_process_manager_.HasBidderWorkletRequest());
  handle3->GetBidderWorklet()->SendPendingSignalsRequests();
  bidder_worklet1->WaitForSendPendingSignalsRequests(3);
  // Should still only be one process.
  EXPECT_EQ(1u, auction_process_manager_.GetBidderProcessCountForTesting());

  // Close both remaining handles.
  handle2.reset();
  handle3.reset();

  // Process should be destroyed.
  EXPECT_EQ(0u, auction_process_manager_.GetBidderProcessCountForTesting());

  // Request another bidder worklet. A new BidderWorklet in a new process should
  // be created.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle4;
  ASSERT_TRUE(auction_worklet_manager_.RequestBidderWorklet(
      kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl, kEmptySubresourceBuilder,
      /*experiment_group_id=*/absl::nullopt,
      NeverInvokedWorkletAvailableCallback(), NeverInvokedFatalErrorCallback(),
      handle4));
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
}

// Test that requests with the same parameters reuse seller worklets.
TEST_F(AuctionWorkletManagerTest, ReuseSellerWorklet) {
  // Load a seller worklet.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle1;
  ASSERT_TRUE(auction_worklet_manager_.RequestSellerWorklet(
      kDecisionLogicUrl, kTrustedSignalsUrl, kEmptySubresourceBuilder,
      /*experiment_group_id=*/absl::nullopt,
      NeverInvokedWorkletAvailableCallback(), NeverInvokedFatalErrorCallback(),
      handle1));
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

  // Load a seller worklet with the same parameters. The worklet should be
  // reused.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle2;
  ASSERT_TRUE(auction_worklet_manager_.RequestSellerWorklet(
      kDecisionLogicUrl, kTrustedSignalsUrl, kEmptySubresourceBuilder,
      /*experiment_group_id=*/absl::nullopt,
      NeverInvokedWorkletAvailableCallback(), NeverInvokedFatalErrorCallback(),
      handle2));
  EXPECT_EQ(handle1->GetSellerWorklet(), handle2->GetSellerWorklet());
  EXPECT_FALSE(auction_process_manager_.HasSellerWorkletRequest());
  handle2->GetSellerWorklet()->SendPendingSignalsRequests();
  seller_worklet1->WaitForSendPendingSignalsRequests(2);
  // Should still only be one process.
  EXPECT_EQ(1u, auction_process_manager_.GetSellerProcessCountForTesting());

  // Close original handle. Worklet should still be alive, and so should its
  // process.
  handle1.reset();
  EXPECT_EQ(1u, auction_process_manager_.GetSellerProcessCountForTesting());

  // Load a seller worklet with the same parameters. The worklet should still be
  // reused again.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle3;
  ASSERT_TRUE(auction_worklet_manager_.RequestSellerWorklet(
      kDecisionLogicUrl, kTrustedSignalsUrl, kEmptySubresourceBuilder,
      /*experiment_group_id=*/absl::nullopt,
      NeverInvokedWorkletAvailableCallback(), NeverInvokedFatalErrorCallback(),
      handle3));
  EXPECT_EQ(handle2->GetSellerWorklet(), handle3->GetSellerWorklet());
  EXPECT_FALSE(auction_process_manager_.HasSellerWorkletRequest());
  handle3->GetSellerWorklet()->SendPendingSignalsRequests();
  seller_worklet1->WaitForSendPendingSignalsRequests(3);
  // Should still only be one process.
  EXPECT_EQ(1u, auction_process_manager_.GetSellerProcessCountForTesting());

  // Close both remaining handles.
  handle2.reset();
  handle3.reset();

  // Process should be destroyed.
  EXPECT_EQ(0u, auction_process_manager_.GetSellerProcessCountForTesting());

  // Request another seller worklet. A new SellerWorklet in a new process should
  // be created.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle4;
  ASSERT_TRUE(auction_worklet_manager_.RequestSellerWorklet(
      kDecisionLogicUrl, kTrustedSignalsUrl, kEmptySubresourceBuilder,
      /*experiment_group_id=*/absl::nullopt,
      NeverInvokedWorkletAvailableCallback(), NeverInvokedFatalErrorCallback(),
      handle4));
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
}

// Make sure that worklets are not reused when parameters don't match.
TEST_F(AuctionWorkletManagerTest, DifferentBidderWorklets) {
  // Load a bidder worklet.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle1;
  ASSERT_TRUE(auction_worklet_manager_.RequestBidderWorklet(
      kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl, kEmptySubresourceBuilder,
      /*experiment_group_id=*/absl::nullopt,
      NeverInvokedWorkletAvailableCallback(), NeverInvokedFatalErrorCallback(),
      handle1));
  EXPECT_TRUE(handle1->GetBidderWorklet());
  std::unique_ptr<MockBidderWorklet> bidder_worklet1 =
      auction_process_manager_.WaitForBidderWorklet();
  EXPECT_EQ(kDecisionLogicUrl, bidder_worklet1->script_source_url());
  EXPECT_EQ(kWasmUrl, bidder_worklet1->wasm_url());
  EXPECT_EQ(kTrustedSignalsUrl, bidder_worklet1->trusted_bidding_signals_url());
  EXPECT_EQ(kTopWindowOrigin, bidder_worklet1->top_window_origin());
  // Should only be one process.
  EXPECT_EQ(1u, auction_process_manager_.GetBidderProcessCountForTesting());

  // Load a bidder worklet with a different decision logic URL. A new worklet
  // should be created, using the same process.
  const GURL kDifferentDecisionLogicUrl =
      GURL("https://origin.test/different_script");
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle2;
  ASSERT_TRUE(auction_worklet_manager_.RequestBidderWorklet(
      kDifferentDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl,
      kEmptySubresourceBuilder,
      /*experiment_group_id=*/absl::nullopt,
      NeverInvokedWorkletAvailableCallback(), NeverInvokedFatalErrorCallback(),
      handle2));
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

  // Load a bidder worklet with a different (null) trusted signals URL. A new
  // worklet should be created, using the same process.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle3;
  ASSERT_TRUE(auction_worklet_manager_.RequestBidderWorklet(
      kDecisionLogicUrl, kWasmUrl,
      /*trusted_bidding_signals_url=*/absl::nullopt, kEmptySubresourceBuilder,
      /*experiment_group_id=*/absl::nullopt,
      NeverInvokedWorkletAvailableCallback(), NeverInvokedFatalErrorCallback(),
      handle3));
  EXPECT_TRUE(handle3->GetBidderWorklet());
  EXPECT_NE(handle1->GetBidderWorklet(), handle3->GetBidderWorklet());
  EXPECT_NE(handle2->GetBidderWorklet(), handle3->GetBidderWorklet());
  std::unique_ptr<MockBidderWorklet> bidder_worklet3 =
      auction_process_manager_.WaitForBidderWorklet();
  EXPECT_EQ(kDecisionLogicUrl, bidder_worklet3->script_source_url());
  EXPECT_EQ(kWasmUrl, bidder_worklet3->wasm_url());
  EXPECT_EQ(absl::nullopt, bidder_worklet3->trusted_bidding_signals_url());
  EXPECT_EQ(kTopWindowOrigin, bidder_worklet3->top_window_origin());
  // Should still only be one process.
  EXPECT_EQ(1u, auction_process_manager_.GetBidderProcessCountForTesting());

  // Load a bidder worklet with a different (null) wasm helper URL. A new
  // worklet should be created, using the same process.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle4;
  ASSERT_TRUE(auction_worklet_manager_.RequestBidderWorklet(
      kDecisionLogicUrl, /*wasm_url=*/absl::nullopt, kTrustedSignalsUrl,
      kEmptySubresourceBuilder,
      /*experiment_group_id=*/absl::nullopt,
      NeverInvokedWorkletAvailableCallback(), NeverInvokedFatalErrorCallback(),
      handle4));
  EXPECT_TRUE(handle4->GetBidderWorklet());
  EXPECT_NE(handle1->GetBidderWorklet(), handle4->GetBidderWorklet());
  EXPECT_NE(handle2->GetBidderWorklet(), handle4->GetBidderWorklet());
  EXPECT_NE(handle3->GetBidderWorklet(), handle4->GetBidderWorklet());
  std::unique_ptr<MockBidderWorklet> bidder_worklet4 =
      auction_process_manager_.WaitForBidderWorklet();
  EXPECT_EQ(kDecisionLogicUrl, bidder_worklet4->script_source_url());
  EXPECT_EQ(absl::nullopt, bidder_worklet4->wasm_url());
  EXPECT_EQ(kTrustedSignalsUrl, bidder_worklet4->trusted_bidding_signals_url());
  EXPECT_EQ(kTopWindowOrigin, bidder_worklet4->top_window_origin());
  // Should still only be one process.
  EXPECT_EQ(1u, auction_process_manager_.GetBidderProcessCountForTesting());
}

// Test bidder worklet matching with different experiment IDs.
TEST_F(AuctionWorkletManagerTest, BidderWorkletExperimentIDs) {
  const unsigned short kExperiment1 = 123u;
  const unsigned short kExperiment2 = 234u;

  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle1;
  ASSERT_TRUE(auction_worklet_manager_.RequestBidderWorklet(
      kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl, kEmptySubresourceBuilder,
      kExperiment1, NeverInvokedWorkletAvailableCallback(),
      NeverInvokedFatalErrorCallback(), handle1));
  EXPECT_TRUE(handle1->GetBidderWorklet());
  std::unique_ptr<MockBidderWorklet> bidder_worklet1 =
      auction_process_manager_.WaitForBidderWorklet();

  // Request one with a different experiment ID. Should result in a different
  // worklet.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle2;
  ASSERT_TRUE(auction_worklet_manager_.RequestBidderWorklet(
      kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl, kEmptySubresourceBuilder,
      kExperiment2, NeverInvokedWorkletAvailableCallback(),
      NeverInvokedFatalErrorCallback(), handle2));
  EXPECT_TRUE(handle2->GetBidderWorklet());
  std::unique_ptr<MockBidderWorklet> bidder_worklet2 =
      auction_process_manager_.WaitForBidderWorklet();
  EXPECT_NE(handle1->GetBidderWorklet(), handle2->GetBidderWorklet());

  // Now try with different trusted signals URL (using WASM url instead).
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle3;
  ASSERT_TRUE(auction_worklet_manager_.RequestBidderWorklet(
      kDecisionLogicUrl, kWasmUrl, kWasmUrl, kEmptySubresourceBuilder,
      kExperiment1, NeverInvokedWorkletAvailableCallback(),
      NeverInvokedFatalErrorCallback(), handle3));
  EXPECT_TRUE(handle3->GetBidderWorklet());
  std::unique_ptr<MockBidderWorklet> bidder_worklet3 =
      auction_process_manager_.WaitForBidderWorklet();
  EXPECT_NE(handle1->GetBidderWorklet(), handle3->GetBidderWorklet());
  EXPECT_NE(handle2->GetBidderWorklet(), handle3->GetBidderWorklet());

  // Now test with null trusted signals URL. For bidder worklets this should be
  // as if no experiment was given, since that's the only way they see it.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle4;
  ASSERT_TRUE(auction_worklet_manager_.RequestBidderWorklet(
      kDecisionLogicUrl, kWasmUrl,
      /*trusted_bidding_signals_url=*/absl::nullopt, kEmptySubresourceBuilder,
      kExperiment1, NeverInvokedWorkletAvailableCallback(),
      NeverInvokedFatalErrorCallback(), handle4));
  EXPECT_TRUE(handle4->GetBidderWorklet());
  std::unique_ptr<MockBidderWorklet> bidder_worklet4 =
      auction_process_manager_.WaitForBidderWorklet();
  EXPECT_NE(handle1->GetBidderWorklet(), handle4->GetBidderWorklet());
  EXPECT_NE(handle2->GetBidderWorklet(), handle4->GetBidderWorklet());
  EXPECT_NE(handle3->GetBidderWorklet(), handle4->GetBidderWorklet());

  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle5;
  ASSERT_TRUE(auction_worklet_manager_.RequestBidderWorklet(
      kDecisionLogicUrl, kWasmUrl,
      /*trusted_bidding_signals_url=*/absl::nullopt, kEmptySubresourceBuilder,
      /*experiment_group_id=*/absl::nullopt,
      NeverInvokedWorkletAvailableCallback(), NeverInvokedFatalErrorCallback(),
      handle5));
  EXPECT_TRUE(handle5->GetBidderWorklet());
  EXPECT_EQ(handle5->GetBidderWorklet(), handle4->GetBidderWorklet());
}

// Make sure that worklets are not reused when parameters don't match.
TEST_F(AuctionWorkletManagerTest, DifferentSellerWorklets) {
  // Load a seller worklet.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle1;
  ASSERT_TRUE(auction_worklet_manager_.RequestSellerWorklet(
      kDecisionLogicUrl, kTrustedSignalsUrl, kEmptySubresourceBuilder,
      /*experiment_group_id=*/absl::nullopt,
      NeverInvokedWorkletAvailableCallback(), NeverInvokedFatalErrorCallback(),
      handle1));
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
  ASSERT_TRUE(auction_worklet_manager_.RequestSellerWorklet(
      kDifferentDecisionLogicUrl, kTrustedSignalsUrl, kEmptySubresourceBuilder,
      /*experiment_group_id=*/absl::nullopt,
      NeverInvokedWorkletAvailableCallback(), NeverInvokedFatalErrorCallback(),
      handle2));
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
  ASSERT_TRUE(auction_worklet_manager_.RequestSellerWorklet(
      kDecisionLogicUrl, /*trusted_scoring_signals_url=*/absl::nullopt,
      kEmptySubresourceBuilder,
      /*experiment_group_id=*/absl::nullopt,
      NeverInvokedWorkletAvailableCallback(), NeverInvokedFatalErrorCallback(),
      handle3));
  EXPECT_TRUE(handle3->GetSellerWorklet());
  EXPECT_NE(handle1->GetSellerWorklet(), handle3->GetSellerWorklet());
  EXPECT_NE(handle2->GetSellerWorklet(), handle3->GetSellerWorklet());
  std::unique_ptr<MockSellerWorklet> seller_worklet3 =
      auction_process_manager_.WaitForSellerWorklet();
  EXPECT_EQ(kDecisionLogicUrl, seller_worklet3->script_source_url());
  EXPECT_EQ(absl::nullopt, seller_worklet3->trusted_scoring_signals_url());
  EXPECT_EQ(kTopWindowOrigin, seller_worklet3->top_window_origin());
  // Should still only be one process.
  EXPECT_EQ(1u, auction_process_manager_.GetSellerProcessCountForTesting());
}

// Test seller worklet matching with different experiment IDs.
TEST_F(AuctionWorkletManagerTest, SellerWorkletExperimentIDs) {
  const unsigned short kExperiment1 = 123u;
  const unsigned short kExperiment2 = 234u;

  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle1;
  ASSERT_TRUE(auction_worklet_manager_.RequestSellerWorklet(
      kDecisionLogicUrl, kTrustedSignalsUrl, kEmptySubresourceBuilder,
      kExperiment1, NeverInvokedWorkletAvailableCallback(),
      NeverInvokedFatalErrorCallback(), handle1));
  EXPECT_TRUE(handle1->GetSellerWorklet());
  std::unique_ptr<MockSellerWorklet> seller_worklet1 =
      auction_process_manager_.WaitForSellerWorklet();

  // Request one with a different experiment ID. Should result in a different
  // worklet.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle2;
  ASSERT_TRUE(auction_worklet_manager_.RequestSellerWorklet(
      kDecisionLogicUrl, kTrustedSignalsUrl, kEmptySubresourceBuilder,
      kExperiment2, NeverInvokedWorkletAvailableCallback(),
      NeverInvokedFatalErrorCallback(), handle2));
  EXPECT_TRUE(handle2->GetSellerWorklet());
  std::unique_ptr<MockSellerWorklet> seller_worklet2 =
      auction_process_manager_.WaitForSellerWorklet();
  EXPECT_NE(handle1->GetSellerWorklet(), handle2->GetSellerWorklet());

  // Now try with different trusted signals URL (using WASM url instead).
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle3;
  ASSERT_TRUE(auction_worklet_manager_.RequestSellerWorklet(
      kDecisionLogicUrl, kWasmUrl, kEmptySubresourceBuilder, kExperiment1,
      NeverInvokedWorkletAvailableCallback(), NeverInvokedFatalErrorCallback(),
      handle3));
  EXPECT_TRUE(handle3->GetSellerWorklet());
  std::unique_ptr<MockSellerWorklet> seller_worklet3 =
      auction_process_manager_.WaitForSellerWorklet();
  EXPECT_NE(handle1->GetSellerWorklet(), handle3->GetSellerWorklet());
  EXPECT_NE(handle2->GetSellerWorklet(), handle3->GetSellerWorklet());

  // Now test with null trusted signals URL. For seller worklet, we should still
  // distinguish different experiment IDs since the ID shows up in
  // AuctionConfig.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle4;
  ASSERT_TRUE(auction_worklet_manager_.RequestSellerWorklet(
      kDecisionLogicUrl, /*trusted_scoring_signals_url=*/absl::nullopt,
      kEmptySubresourceBuilder, kExperiment1,
      NeverInvokedWorkletAvailableCallback(), NeverInvokedFatalErrorCallback(),
      handle4));
  EXPECT_TRUE(handle4->GetSellerWorklet());
  std::unique_ptr<MockSellerWorklet> seller_worklet4 =
      auction_process_manager_.WaitForSellerWorklet();
  EXPECT_NE(handle1->GetSellerWorklet(), handle4->GetSellerWorklet());
  EXPECT_NE(handle2->GetSellerWorklet(), handle4->GetSellerWorklet());
  EXPECT_NE(handle3->GetSellerWorklet(), handle4->GetSellerWorklet());

  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle5;
  ASSERT_TRUE(auction_worklet_manager_.RequestSellerWorklet(
      kDecisionLogicUrl, /*trusted_scoring_signals_url=*/absl::nullopt,
      kEmptySubresourceBuilder,
      /*experiment_group_id=*/absl::nullopt,
      NeverInvokedWorkletAvailableCallback(), NeverInvokedFatalErrorCallback(),
      handle5));
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
  ASSERT_TRUE(auction_worklet_manager_.RequestBidderWorklet(
      kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl, kEmptySubresourceBuilder,
      /*experiment_group_id=*/absl::nullopt,
      NeverInvokedWorkletAvailableCallback(), load_error_helper.Callback(),
      handle));
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
  task_environment_.RunUntilIdle();

  // Another request for the same worklet should trigger creation of a new
  // worklet, even though the old handle for the worklet hasn't been deleted
  // yet.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle2;
  ASSERT_TRUE(auction_worklet_manager_.RequestBidderWorklet(
      kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl, kEmptySubresourceBuilder,
      /*experiment_group_id=*/absl::nullopt,
      NeverInvokedWorkletAvailableCallback(), load_error_helper.Callback(),
      handle2));
  EXPECT_TRUE(handle2->GetBidderWorklet());
  EXPECT_NE(handle->GetBidderWorklet(), handle2->GetBidderWorklet());
  std::unique_ptr<MockBidderWorklet> bidder_worklet2 =
      auction_process_manager_.WaitForBidderWorklet();
}

TEST_F(AuctionWorkletManagerTest, SellerWorkletLoadError) {
  const char kErrorText[] = "Goat teleportation error";

  // Load a seller worklet.
  FatalLoadErrorHelper load_error_helper;
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
  ASSERT_TRUE(auction_worklet_manager_.RequestSellerWorklet(
      kDecisionLogicUrl, kTrustedSignalsUrl, kEmptySubresourceBuilder,
      /*experiment_group_id=*/absl::nullopt,
      NeverInvokedWorkletAvailableCallback(), load_error_helper.Callback(),
      handle));
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
  task_environment_.RunUntilIdle();

  // Another request for the same worklet should trigger creation of a new
  // worklet, even though the old handle for the worklet hasn't been deleted
  // yet.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle2;
  ASSERT_TRUE(auction_worklet_manager_.RequestSellerWorklet(
      kDecisionLogicUrl, kTrustedSignalsUrl, kEmptySubresourceBuilder,
      /*experiment_group_id=*/absl::nullopt,
      NeverInvokedWorkletAvailableCallback(), load_error_helper.Callback(),
      handle2));
  EXPECT_TRUE(handle2->GetSellerWorklet());
  EXPECT_NE(handle->GetSellerWorklet(), handle2->GetSellerWorklet());
  std::unique_ptr<MockSellerWorklet> seller_worklet2 =
      auction_process_manager_.WaitForSellerWorklet();
}

TEST_F(AuctionWorkletManagerTest, BidderWorkletCrash) {
  // Load a bidder worklet.
  FatalLoadErrorHelper load_error_helper;
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
  ASSERT_TRUE(auction_worklet_manager_.RequestBidderWorklet(
      kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl, kEmptySubresourceBuilder,
      /*experiment_group_id=*/absl::nullopt,
      NeverInvokedWorkletAvailableCallback(), load_error_helper.Callback(),
      handle));
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
  task_environment_.RunUntilIdle();
  handle->GetBidderWorklet()->SendPendingSignalsRequests();

  // Another request for the same worklet should trigger creation of a new
  // worklet, even though the old handle for the worklet hasn't been deleted
  // yet.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle2;
  ASSERT_TRUE(auction_worklet_manager_.RequestBidderWorklet(
      kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl, kEmptySubresourceBuilder,
      /*experiment_group_id=*/absl::nullopt,
      NeverInvokedWorkletAvailableCallback(), load_error_helper.Callback(),
      handle2));
  EXPECT_TRUE(handle2->GetBidderWorklet());
  EXPECT_NE(handle->GetBidderWorklet(), handle2->GetBidderWorklet());
  std::unique_ptr<MockBidderWorklet> bidder_worklet2 =
      auction_process_manager_.WaitForBidderWorklet();
}

TEST_F(AuctionWorkletManagerTest, SellerWorkletCrash) {
  // Load a seller worklet.
  FatalLoadErrorHelper load_error_helper;
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
  ASSERT_TRUE(auction_worklet_manager_.RequestSellerWorklet(
      kDecisionLogicUrl, kTrustedSignalsUrl, kEmptySubresourceBuilder,
      /*experiment_group_id=*/absl::nullopt,
      NeverInvokedWorkletAvailableCallback(), load_error_helper.Callback(),
      handle));
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
  task_environment_.RunUntilIdle();
  handle->GetSellerWorklet()->SendPendingSignalsRequests();

  // Another request for the same worklet should trigger creation of a new
  // worklet, even though the old handle for the worklet hasn't been deleted
  // yet.
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle2;
  ASSERT_TRUE(auction_worklet_manager_.RequestSellerWorklet(
      kDecisionLogicUrl, kTrustedSignalsUrl, kEmptySubresourceBuilder,
      /*experiment_group_id=*/absl::nullopt,
      NeverInvokedWorkletAvailableCallback(), load_error_helper.Callback(),
      handle2));
  EXPECT_TRUE(handle2->GetSellerWorklet());
  EXPECT_NE(handle->GetSellerWorklet(), handle2->GetSellerWorklet());
  std::unique_ptr<MockSellerWorklet> seller_worklet2 =
      auction_process_manager_.WaitForSellerWorklet();
}

// Test reentrant deletion of a WorkletHandle on error.
TEST_F(AuctionWorkletManagerTest, BidderWorkletDeleteOnError) {
  const char kErrorText[] = "Goat teleporation error";

  // Load a bidder worklet.
  base::RunLoop run_loop;
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
  ASSERT_TRUE(auction_worklet_manager_.RequestBidderWorklet(
      kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl, kEmptySubresourceBuilder,
      /*experiment_group_id=*/absl::nullopt,
      NeverInvokedWorkletAvailableCallback(),
      base::BindLambdaForTesting(
          [&](AuctionWorkletManager::FatalErrorType fatal_error_type,
              const std::vector<std::string>& errors) {
            handle.reset();
            run_loop.Quit();
          }),
      handle));
  EXPECT_TRUE(handle->GetBidderWorklet());

  // Return a load error.
  std::unique_ptr<MockBidderWorklet> bidder_worklet =
      auction_process_manager_.WaitForBidderWorklet();
  bidder_worklet->ClosePipe(kErrorText);

  run_loop.Run();
  // The process should have been deleted, and there should be no crashes.
  EXPECT_EQ(0u, auction_process_manager_.GetBidderProcessCountForTesting());
}

// Test reentrant deletion of a WorkletHandle on error.
TEST_F(AuctionWorkletManagerTest, SellerWorkletDeleteOnError) {
  const char kErrorText[] = "Goat teleporation error";

  // Load a seller worklet.
  base::RunLoop run_loop;
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
  ASSERT_TRUE(auction_worklet_manager_.RequestSellerWorklet(
      kDecisionLogicUrl, kTrustedSignalsUrl, kEmptySubresourceBuilder,
      /*experiment_group_id=*/absl::nullopt,
      NeverInvokedWorkletAvailableCallback(),
      base::BindLambdaForTesting(
          [&](AuctionWorkletManager::FatalErrorType fatal_error_type,
              const std::vector<std::string>& errors) {
            handle.reset();
            run_loop.Quit();
          }),
      handle));
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
  ASSERT_TRUE(auction_worklet_manager_.RequestBidderWorklet(
      kDecisionLogicUrl, kWasmUrl, kTrustedSignalsUrl, kEmptySubresourceBuilder,
      /*experiment_group_id=*/absl::nullopt,
      NeverInvokedWorkletAvailableCallback(), NeverInvokedFatalErrorCallback(),
      handle));
  EXPECT_TRUE(handle->GetBidderWorklet());

  std::unique_ptr<MockBidderWorklet> bidder_worklet =
      auction_process_manager_.WaitForBidderWorklet();

  const struct {
    GURL url;
    const char* mime_type;
  } kAllowedUrls[] = {
      {kDecisionLogicUrl, "application/javascript"},
      {kWasmUrl, "application/wasm"},
      {GURL("https://origin.test/"
            "trusted_signals?hostname=top.window.origin.test&render_urls=not_"
            "validated"),
       "application/json"},
  };

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

// Minimal test that seller worklets' AuctionURLLoaderFactoryProxies are
// correctly configured.
TEST_F(AuctionWorkletManagerTest, SellerWorkletUrlRequestProtection) {
  std::unique_ptr<AuctionWorkletManager::WorkletHandle> handle;
  ASSERT_TRUE(auction_worklet_manager_.RequestSellerWorklet(
      kDecisionLogicUrl, kTrustedSignalsUrl, kEmptySubresourceBuilder,
      /*experiment_group_id=*/absl::nullopt,
      NeverInvokedWorkletAvailableCallback(), NeverInvokedFatalErrorCallback(),
      handle));
  EXPECT_TRUE(handle->GetSellerWorklet());

  std::unique_ptr<MockSellerWorklet> seller_worklet =
      auction_process_manager_.WaitForSellerWorklet();

  const struct {
    GURL url;
    const char* mime_type;
  } kAllowedUrls[] = {
      {kDecisionLogicUrl, "application/javascript"},
      {GURL("https://origin.test/"
            "trusted_signals?hostname=top.window.origin.test&render_urls=not_"
            "validated"),
       "application/json"},
  };

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

}  // namespace
}  // namespace content
