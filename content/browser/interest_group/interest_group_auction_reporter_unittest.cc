// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_auction_reporter.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "content/browser/interest_group/auction_worklet_manager.h"
#include "content/browser/interest_group/interest_group_k_anonymity_manager.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/interest_group/mock_auction_process_manager.h"
#include "content/browser/interest_group/subresource_url_builder.h"
#include "content/browser/interest_group/test_interest_group_manager_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/system/functions.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/common/interest_group/test_interest_group_builder.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

InterestGroupAuctionReporter::SellerWinningBidInfo CreateSellerWinningBidInfo(
    blink::AuctionConfig* auction_config) {
  InterestGroupAuctionReporter::SellerWinningBidInfo out;
  out.auction_config = auction_config;

  // Must not be null. Passing in a nullopt URL when constructing it makes it
  // return nothing, though.
  out.subresource_url_builder = std::make_unique<SubresourceUrlBuilder>(
      /*direct_from_seller_signals=*/absl::nullopt);

  // The specific values these are assigned to don't matter for these tests, but
  // they don't have default initializers, so have to set them to placate memory
  // tools.
  out.bid = 1;
  out.score = 1;
  out.highest_scoring_other_bid = 0;
  out.trace_id = 0;
  return out;
}

// These tests cover the InterestGroupAuctionReporter state machine with respect
// to auction worklets and sending reports. All tests use mock auction worklets.
// Passing arguments correctly to reporting worklets is not covered by these
// tests, but rather by the AuctionRunner tests.
class InterestGroupAuctionReporterTest
    : public RenderViewHostTestHarness,
      public AuctionWorkletManager::Delegate {
 public:
  InterestGroupAuctionReporterTest() {
    mojo::SetDefaultProcessErrorHandler(
        base::BindRepeating(&InterestGroupAuctionReporterTest::OnBadMessage,
                            base::Unretained(this)));

    // The `frame_client_security_state_` used for testing shouldn't match the
    // default value, so equality checks in `interest_group_manager_impl_` won't
    // pass with a default-constructed state.
    network::mojom::ClientSecurityStatePtr
        default_constructed_client_security_state =
            network::mojom::ClientSecurityState::New();
    EXPECT_FALSE(frame_client_security_state_->Equals(
        *default_constructed_client_security_state));

    // This is shared auction configuration that's the same for both component
    // auctions and single-seller auctions.

    auction_config_->seller = kSellerOrigin;
    auction_config_->decision_logic_url = kSellerScriptUrl;

    winning_bid_info_.storage_interest_group =
        std::make_unique<StorageInterestGroup>();
    winning_bid_info_.storage_interest_group->interest_group =
        blink::TestInterestGroupBuilder(kWinningBidderOrigin,
                                        kWinningBidderName)
            .SetBiddingUrl(kWinningBidderScriptUrl)
            // A non-empty ad list is needed by KAnonKeyForAdBid().
            .SetAds({{{GURL("https://ad.render.url.test/"),
                       "\"This be metadata\""}}})
            .Build();

    // Join the interest group that "won" the auction - this matters for tests
    // that make sure the interest group is updated correctly.
    const blink::InterestGroup& interest_group =
        winning_bid_info_.storage_interest_group->interest_group;
    interest_group_manager_impl_->JoinInterestGroup(
        interest_group,
        /*joining_url=*/kWinningBidderOrigin.GetURL());

    winning_bid_info_.ad_metadata = kWinningAdMetadata;

    // The actual value doesn't matter for tests, but need to set some value as
    // it doesn't have a default one.
    winning_bid_info_.bid = 1;

    seller_winning_bid_info_ =
        CreateSellerWinningBidInfo(auction_config_.get());

    // Populate `k_anon_keys_to_join_` with accurate-looking strings. Could
    // actually use any strings for the sake of these tests, but seems best to
    // use accurate ones.
    std::vector<std::string> k_anon_keys_to_join{
        KAnonKeyForAdBid(interest_group, (*interest_group.ads)[0].render_url),
        KAnonKeyForAdNameReporting(interest_group, (*interest_group.ads)[0]),
    };
    k_anon_keys_to_join_ =
        base::flat_set<std::string>(std::move(k_anon_keys_to_join));
  }

  ~InterestGroupAuctionReporterTest() override = default;

  void TearDown() override {
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
    // All bad Mojo messages should have been validated, which clears them.
    EXPECT_TRUE(bad_message_.empty());

    // The base class (RenderViewHostTestHarness) tears down the task environemt
    // in its TearDown() method instead of in its destructor, so have to delete
    // `interest_group_manager_impl_` before that to avoid any leaks, since it
    // does work on other threads.
    interest_group_auction_reporter_.reset();

    // All reports should have been observed. Not all tests observe all other
    // fields, so don't check any of the others.
    interest_group_manager_impl_->ExpectReports({});

    interest_group_manager_impl_.reset();

    RenderViewHostTestHarness::TearDown();
  }

  // Sets up an auction with one seller (i.e., not a component auction).
  // SetUpComponentAuction() must not also be invoked.
  void SetUpSingleSellerAuction() {
    auction_config_->non_shared_params.interest_group_buyers.emplace()
        .push_back(kWinningBidderOrigin);
  }

  // Sets up a component auction. SetUpComponentAuction() must not also be
  // invoked.
  void SetUpComponentAuction() {
    blink::AuctionConfig& component_auction_config =
        auction_config_->non_shared_params.component_auctions.emplace_back();
    component_auction_config.seller = kComponentSellerOrigin;
    component_auction_config.decision_logic_url = kComponentSellerScriptUrl;

    component_auction_config.non_shared_params.interest_group_buyers.emplace()
        .push_back(kWinningBidderOrigin);

    component_seller_winning_bid_info_ =
        CreateSellerWinningBidInfo(&component_auction_config);
    component_seller_winning_bid_info_->component_auction_modified_bid_params =
        auction_worklet::mojom::ComponentAuctionModifiedBidParams::New(
            /*ad=*/"null", /*bid=*/0, /*has_bid=*/false);
  }

  void SetUpReporterAndStart() {
    interest_group_auction_reporter_ = std::make_unique<
        InterestGroupAuctionReporter>(
        interest_group_manager_impl_.get(), &auction_worklet_manager_,
        std::move(auction_config_), kFrameOrigin,
        frame_client_security_state_.Clone(),
        dummy_report_shared_url_loader_factory_, std::move(winning_bid_info_),
        std::move(seller_winning_bid_info_),
        std::move(component_seller_winning_bid_info_),
        /*interest_groups_that_bid=*/
        blink::InterestGroupSet{{kWinningBidderOrigin, kWinningBidderName},
                                {kLosingBidderOrigin, kLosingBidderName}},
        std::move(debug_win_report_urls_), std::move(debug_loss_report_urls_),
        k_anon_keys_to_join_,
        std::map<url::Origin,
                 InterestGroupAuctionReporter::PrivateAggregationRequests>());
    interest_group_auction_reporter_->Start(
        base::BindOnce(&InterestGroupAuctionReporterTest::OnCompleteCallback,
                       base::Unretained(this)));
  }

  void SetUpAndStartSingleSellerAuction() {
    SetUpSingleSellerAuction();
    SetUpReporterAndStart();
  }

  void SetUpAndStartComponentAuction() {
    SetUpComponentAuction();
    SetUpReporterAndStart();
  }

  // Waits for the worklet with the provided URL to be created and invokes its
  // ReportResult callback with `report_url`. Destroys the seller worklet when
  // done, which should not mater to the InterestGroupAuctionReporter.
  void WaitForReportResultAndRunCallback(
      const GURL& seller_url,
      absl::optional<GURL> report_url,
      base::flat_map<std::string, GURL> ad_beacon_map = {},
      std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>
          pa_requests = {},
      std::vector<std::string> errors = {}) {
    auction_process_manager_.WaitForWinningSellerReload();
    std::unique_ptr<MockSellerWorklet> seller_worklet =
        auction_process_manager_.TakeSellerWorklet(seller_url);
    seller_worklet->set_expect_send_pending_signals_requests_called(false);
    seller_worklet->WaitForReportResult();
    seller_worklet->InvokeReportResultCallback(
        std::move(report_url), std::move(ad_beacon_map), std::move(pa_requests),
        std::move(errors));

    // Note that the seller pipe is flushed when `seller_worklet` is destroyed.
    // Flushing the pipe ensures that the reporter has received the response,
    // and any resulting reports have been queued.
  }

  // Waits for the bidder worklet to be created and invokes its ReportWin
  // callback with `report_url`. Destroys the bidder worklet when done, which
  // should not mater to the InterestGroupAuctionReporter.
  void WaitForReportWinAndRunCallback(
      absl::optional<GURL> report_url,
      base::flat_map<std::string, GURL> ad_beacon_map = {},
      std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>
          pa_requests = {},
      std::vector<std::string> errors = {}) {
    auction_process_manager_.WaitForWinningBidderReload();
    std::unique_ptr<MockBidderWorklet> bidder_worklet =
        auction_process_manager_.TakeBidderWorklet(kWinningBidderScriptUrl);
    bidder_worklet->WaitForReportWin();
    bidder_worklet->InvokeReportWinCallback(
        std::move(report_url), std::move(ad_beacon_map), std::move(pa_requests),
        std::move(errors));

    // Note that the bidder pipe is not automatically flushed on destruction, so
    // need to destroy it manually. Flushing the pipe ensures that the reporter
    // has received the response, and any resulting reports have been queued.
    bidder_worklet->Flush();
  }

  // Checks that the win has not yet been recorded by the InterestGroupManager.
  void ExpectNoWinsRecorded() const {
    absl::optional<StorageInterestGroup> interest_group =
        interest_group_manager_impl_->BlockingGetInterestGroup(
            kWinningBidderOrigin, kWinningBidderName);
    ASSERT_TRUE(interest_group);
    EXPECT_EQ(0u, interest_group->bidding_browser_signals->prev_wins.size());
  }

  // Checks that the win has been recorded once and only once by the
  // InterestGroupManager.
  void ExpectWinRecordedOnce() const {
    absl::optional<StorageInterestGroup> interest_group =
        interest_group_manager_impl_->BlockingGetInterestGroup(
            kWinningBidderOrigin, kWinningBidderName);
    ASSERT_TRUE(interest_group);
    const std::vector<auction_worklet::mojom::PreviousWinPtr>* prev_wins =
        &interest_group->bidding_browser_signals->prev_wins;
    ASSERT_EQ(1u, prev_wins->size());
    EXPECT_EQ((*prev_wins)[0]->ad_json, kWinningAdMetadata);
  }

  // AuctionWorkletManager::Delegate implementation.
  //
  // Note that none of these matter for these tests, as the the mock worklet
  // classes don't make network requests, but a real AuctionWorkletManager is
  // used, which expects most of these methods to return non-null objects.
  network::mojom::URLLoaderFactory* GetFrameURLLoaderFactory() override {
    NOTREACHED();
    return nullptr;
  }
  network::mojom::URLLoaderFactory* GetTrustedURLLoaderFactory() override {
    NOTREACHED();
    return nullptr;
  }
  void PreconnectSocket(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key) override {
    NOTREACHED();
  }
  RenderFrameHostImpl* GetFrame() override {
    return static_cast<RenderFrameHostImpl*>(main_rfh());
  }
  scoped_refptr<SiteInstance> GetFrameSiteInstance() override {
    return scoped_refptr<SiteInstance>();
  }
  network::mojom::ClientSecurityStatePtr GetClientSecurityState() override {
    return frame_client_security_state_.Clone();
  }

  void WaitForCompletion() { WaitForCompletionExpectingErrors({}); }

  void WaitForCompletionExpectingErrors(
      const std::vector<std::string>& expected_errors) {
    reporter_run_loop_.Run();
    EXPECT_THAT(errors_, testing::UnorderedElementsAreArray(expected_errors));
  }

  // Gets and clear most recent bad Mojo message.
  std::string TakeBadMessage() { return std::move(bad_message_); }

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

  void OnCompleteCallback() {
    reporter_run_loop_.Quit();
    errors_ = interest_group_auction_reporter_->errors();
  }

  // Top frame origin - shouldn't matter for these tests.
  const url::Origin kTopFrameOrigin =
      url::Origin::Create(GURL("https://top_frame_origin.test/"));

  const url::Origin kFrameOrigin =
      url::Origin::Create(GURL("https://frame_origin.test/"));

  const GURL kWinningBidderScriptUrl =
      GURL("https://winning.bidder.origin.test/bidder_script.js");
  const url::Origin kWinningBidderOrigin =
      url::Origin::Create(kWinningBidderScriptUrl);
  const std::string kWinningBidderName = "winning interest group name";
  const std::string kWinningAdMetadata = R"({"render_url": "https://foo/"})";

  const url::Origin kLosingBidderOrigin =
      url::Origin::Create(GURL("https://losing.bidder.origin.test/"));
  const std::string kLosingBidderName = "losing interest group name";

  const GURL kSellerScriptUrl =
      GURL("https://seller.origin.test/seller_script.js");
  const url::Origin kSellerOrigin = url::Origin::Create(kSellerScriptUrl);

  const GURL kComponentSellerScriptUrl =
      GURL("https://component.seller.origin.test/component_seller_script.js");
  const url::Origin kComponentSellerOrigin =
      url::Origin::Create(kComponentSellerScriptUrl);

  const std::vector<blink::InterestGroupKey> kExpectedInterestGroupsThatBid{
      {kWinningBidderOrigin, kWinningBidderName},
      {kLosingBidderOrigin, kLosingBidderName}};

  std::vector<GURL> debug_win_report_urls_;
  std::vector<GURL> debug_loss_report_urls_;

  // These values don't matter, one of them should just not match the default
  // value, to make sure it's correctly plumbed through to
  // `interest_group_manager_impl_`.
  network::mojom::ClientSecurityStatePtr frame_client_security_state_{
      network::mojom::ClientSecurityState::New(
          network::CrossOriginEmbedderPolicy(),
          /*is_web_secure_context=*/true,
          /*ip_address_space=*/network::mojom::IPAddressSpace::kPublic,
          /*is_web_secure_context=*/
          network::mojom::PrivateNetworkRequestPolicy::kBlock)};

  const GURL kSellerReportUrl =
      GURL("https://seller.report.test/seller-report");
  const GURL kComponentSellerReportUrl =
      GURL("https://component.seller.report.test/component-seller-report");
  const GURL kBidderReportUrl =
      GURL("https://bidder.report.test/bidder=report");

  // SharedURLLoaderFactory used for reports. Reports are short-circuited by the
  // TestInterestGroupManagerImpl before they make it over the network, so this
  // is only used for equality checks around making sure the right factory is
  // passed to it.
  scoped_refptr<network::SharedURLLoaderFactory>
      dummy_report_shared_url_loader_factory_ =
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              nullptr);

  MockAuctionProcessManager auction_process_manager_;
  AuctionWorkletManager auction_worklet_manager_{
      &auction_process_manager_, kTopFrameOrigin, kFrameOrigin, this};

  std::unique_ptr<blink::AuctionConfig> auction_config_ =
      std::make_unique<blink::AuctionConfig>();
  InterestGroupAuctionReporter::WinningBidInfo winning_bid_info_;
  InterestGroupAuctionReporter::SellerWinningBidInfo seller_winning_bid_info_;
  absl::optional<InterestGroupAuctionReporter::SellerWinningBidInfo>
      component_seller_winning_bid_info_;

  std::unique_ptr<TestInterestGroupManagerImpl> interest_group_manager_impl_ =
      std::make_unique<TestInterestGroupManagerImpl>(
          kFrameOrigin,
          frame_client_security_state_.Clone(),
          dummy_report_shared_url_loader_factory_);

  base::flat_set<std::string> k_anon_keys_to_join_;

  std::unique_ptr<InterestGroupAuctionReporter>
      interest_group_auction_reporter_;

  std::string bad_message_;
  std::vector<std::string> errors_;
  base::RunLoop reporter_run_loop_;
};

TEST_F(InterestGroupAuctionReporterTest, SingleSellerNoReports) {
  SetUpAndStartSingleSellerAuction();
  interest_group_auction_reporter_->OnNavigateToWinningAdCallback().Run();

  WaitForReportResultAndRunCallback(kSellerScriptUrl, absl::nullopt);
  interest_group_manager_impl_->ExpectReports({});
  WaitForReportWinAndRunCallback(absl::nullopt);
  interest_group_manager_impl_->ExpectReports({});

  WaitForCompletion();
}

TEST_F(InterestGroupAuctionReporterTest, ComponentAuctionNoReports) {
  SetUpAndStartComponentAuction();
  interest_group_auction_reporter_->OnNavigateToWinningAdCallback().Run();

  WaitForReportResultAndRunCallback(kSellerScriptUrl, absl::nullopt);
  interest_group_manager_impl_->ExpectReports({});
  WaitForReportResultAndRunCallback(kComponentSellerScriptUrl, absl::nullopt);
  interest_group_manager_impl_->ExpectReports({});
  WaitForReportWinAndRunCallback(absl::nullopt);
  interest_group_manager_impl_->ExpectReports({});

  WaitForCompletion();
}

TEST_F(InterestGroupAuctionReporterTest, SingleSellerReports) {
  SetUpAndStartSingleSellerAuction();
  interest_group_auction_reporter_->OnNavigateToWinningAdCallback().Run();

  WaitForReportResultAndRunCallback(kSellerScriptUrl, kSellerReportUrl);
  interest_group_manager_impl_->ExpectReports(
      {{InterestGroupManagerImpl::ReportType::kSendReportTo,
        kSellerReportUrl}});

  WaitForReportWinAndRunCallback(kBidderReportUrl);
  interest_group_manager_impl_->ExpectReports(
      {{InterestGroupManagerImpl::ReportType::kSendReportTo,
        kBidderReportUrl}});

  WaitForCompletion();
}

TEST_F(InterestGroupAuctionReporterTest, ComponentAuctionReports) {
  SetUpAndStartComponentAuction();

  interest_group_auction_reporter_->OnNavigateToWinningAdCallback().Run();

  WaitForReportResultAndRunCallback(kSellerScriptUrl, kSellerReportUrl);
  interest_group_manager_impl_->ExpectReports(
      {{InterestGroupManagerImpl::ReportType::kSendReportTo,
        kSellerReportUrl}});

  WaitForReportResultAndRunCallback(kComponentSellerScriptUrl,
                                    kComponentSellerReportUrl);
  interest_group_manager_impl_->ExpectReports(
      {{InterestGroupManagerImpl::ReportType::kSendReportTo,
        kComponentSellerReportUrl}});

  WaitForReportWinAndRunCallback(kBidderReportUrl);
  interest_group_manager_impl_->ExpectReports(
      {{InterestGroupManagerImpl::ReportType::kSendReportTo,
        kBidderReportUrl}});

  WaitForCompletion();
}

// Test the case where all worklets return errors. Errors are non-fatal, so
// reports are still sent.
TEST_F(InterestGroupAuctionReporterTest, ComponentAuctionErrors) {
  const char kSellerError[] = "Error: Seller script had an error.";
  const char kComponentSellerError[] =
      "Error: Component seller script had an error.";
  const char kBuyerError[] = "Error: Buyer script had an error.";

  SetUpAndStartComponentAuction();
  interest_group_auction_reporter_->OnNavigateToWinningAdCallback().Run();

  WaitForReportResultAndRunCallback(kSellerScriptUrl, kSellerReportUrl,
                                    /*ad_beacon_map=*/{}, /*pa_requests=*/{},
                                    /*errors=*/{kSellerError});
  interest_group_manager_impl_->ExpectReports(
      {{InterestGroupManagerImpl::ReportType::kSendReportTo,
        kSellerReportUrl}});

  WaitForReportResultAndRunCallback(kComponentSellerScriptUrl,
                                    kComponentSellerReportUrl,
                                    /*ad_beacon_map=*/{}, /*pa_requests=*/{},
                                    /*errors=*/{kComponentSellerError});
  interest_group_manager_impl_->ExpectReports(
      {{InterestGroupManagerImpl::ReportType::kSendReportTo,
        kComponentSellerReportUrl}});

  WaitForReportWinAndRunCallback(kBidderReportUrl, /*ad_beacon_map=*/{},
                                 /*pa_requests=*/{}, /*errors=*/{kBuyerError});
  interest_group_manager_impl_->ExpectReports(
      {{InterestGroupManagerImpl::ReportType::kSendReportTo,
        kBidderReportUrl}});

  WaitForCompletionExpectingErrors(
      {kSellerError, kComponentSellerError, kBuyerError});
}

// Test case case where navigation occurs only after all reporting scripts have
// run.
TEST_F(InterestGroupAuctionReporterTest, SingleSellerReportsLateNavigation) {
  SetUpAndStartSingleSellerAuction();

  WaitForReportResultAndRunCallback(kSellerScriptUrl, kSellerReportUrl);
  interest_group_manager_impl_->ExpectReports({});

  WaitForReportWinAndRunCallback(kBidderReportUrl);
  interest_group_manager_impl_->ExpectReports({});

  EXPECT_FALSE(reporter_run_loop_.AnyQuitCalled());

  interest_group_auction_reporter_->OnNavigateToWinningAdCallback().Run();
  WaitForCompletion();
  interest_group_manager_impl_->ExpectReports(
      {{InterestGroupManagerImpl::ReportType::kSendReportTo, kSellerReportUrl},
       {InterestGroupManagerImpl::ReportType::kSendReportTo,
        kBidderReportUrl}});
}

// Test case case where navigation occurs only after all reporting scripts have
// run.
TEST_F(InterestGroupAuctionReporterTest,
       ComponentAuctionReportsLateNavigation) {
  SetUpAndStartComponentAuction();

  WaitForReportResultAndRunCallback(kSellerScriptUrl, kSellerReportUrl);
  interest_group_manager_impl_->ExpectReports({});

  WaitForReportResultAndRunCallback(kComponentSellerScriptUrl,
                                    kComponentSellerReportUrl);
  interest_group_manager_impl_->ExpectReports({});

  WaitForReportWinAndRunCallback(kBidderReportUrl);
  interest_group_manager_impl_->ExpectReports({});

  EXPECT_FALSE(reporter_run_loop_.AnyQuitCalled());

  interest_group_auction_reporter_->OnNavigateToWinningAdCallback().Run();
  WaitForCompletion();
  interest_group_manager_impl_->ExpectReports(
      {{InterestGroupManagerImpl::ReportType::kSendReportTo, kSellerReportUrl},
       {InterestGroupManagerImpl::ReportType::kSendReportTo,
        kComponentSellerReportUrl},
       {InterestGroupManagerImpl::ReportType::kSendReportTo,
        kBidderReportUrl}});
}

// Test the case where the top-level seller script process crashes while
// reporting a component auction, making sure the bidder reporting script is
// still run, despite the crash.
TEST_F(InterestGroupAuctionReporterTest, SingleSellerSellerCrash) {
  SetUpAndStartSingleSellerAuction();

  interest_group_auction_reporter_->OnNavigateToWinningAdCallback().Run();

  auction_process_manager_.WaitForWinningSellerReload();
  std::unique_ptr<MockSellerWorklet> seller_worklet =
      auction_process_manager_.TakeSellerWorklet(kSellerScriptUrl);
  seller_worklet->set_expect_send_pending_signals_requests_called(false);
  // Simulate a seller worklet crash.
  seller_worklet.reset();

  WaitForReportWinAndRunCallback(kBidderReportUrl);
  interest_group_manager_impl_->ExpectReports(
      {{InterestGroupManagerImpl::ReportType::kSendReportTo,
        kBidderReportUrl}});

  WaitForCompletionExpectingErrors(
      {"https://seller.origin.test/seller_script.js crashed."});
}

// Test the case where the buyer worklet crashes while reporting a component
// auction. The reporter should complete, but with an error.
TEST_F(InterestGroupAuctionReporterTest, SingleSellerBuyerCrash) {
  SetUpAndStartSingleSellerAuction();

  interest_group_auction_reporter_->OnNavigateToWinningAdCallback().Run();

  WaitForReportResultAndRunCallback(kSellerScriptUrl, kSellerReportUrl);
  interest_group_manager_impl_->ExpectReports(
      {{InterestGroupManagerImpl::ReportType::kSendReportTo,
        kSellerReportUrl}});

  auction_process_manager_.WaitForWinningBidderReload();
  std::unique_ptr<MockBidderWorklet> bidder_worklet =
      auction_process_manager_.TakeBidderWorklet(kWinningBidderScriptUrl);
  // Simulate a bidder worklet crash.
  bidder_worklet.reset();

  WaitForCompletionExpectingErrors(
      {"https://winning.bidder.origin.test/bidder_script.js crashed."});
}

// Test the case where the top-level seller script process crashes while
// reporting a component auction, making sure the component seller and bidder
// reporting scripts are still run, despite the crash.
TEST_F(InterestGroupAuctionReporterTest, ComponentAuctionTopLevelSellerCrash) {
  SetUpAndStartComponentAuction();

  interest_group_auction_reporter_->OnNavigateToWinningAdCallback().Run();

  auction_process_manager_.WaitForWinningSellerReload();
  std::unique_ptr<MockSellerWorklet> seller_worklet =
      auction_process_manager_.TakeSellerWorklet(kSellerScriptUrl);
  seller_worklet->set_expect_send_pending_signals_requests_called(false);
  // Simulate a seller worklet crash.
  seller_worklet.reset();

  WaitForReportResultAndRunCallback(kComponentSellerScriptUrl,
                                    kComponentSellerReportUrl);
  interest_group_manager_impl_->ExpectReports(
      {{InterestGroupManagerImpl::ReportType::kSendReportTo,
        kComponentSellerReportUrl}});

  WaitForReportWinAndRunCallback(kBidderReportUrl);
  interest_group_manager_impl_->ExpectReports(
      {{InterestGroupManagerImpl::ReportType::kSendReportTo,
        kBidderReportUrl}});

  WaitForCompletionExpectingErrors(
      {"https://seller.origin.test/seller_script.js crashed."});
}

// Test the case where the component seller script process crashes while
// reporting a component auction, making sure the bidder reporting script is
// still run, despite the crash.
TEST_F(InterestGroupAuctionReporterTest, ComponentAuctionComponentSellerCrash) {
  SetUpAndStartComponentAuction();

  interest_group_auction_reporter_->OnNavigateToWinningAdCallback().Run();

  WaitForReportResultAndRunCallback(kSellerScriptUrl, kSellerReportUrl);
  interest_group_manager_impl_->ExpectReports(
      {{InterestGroupManagerImpl::ReportType::kSendReportTo,
        kSellerReportUrl}});

  auction_process_manager_.WaitForWinningSellerReload();
  std::unique_ptr<MockSellerWorklet> component_seller_worklet =
      auction_process_manager_.TakeSellerWorklet(kComponentSellerScriptUrl);
  component_seller_worklet->set_expect_send_pending_signals_requests_called(
      false);
  // Simulate a seller worklet crash.
  component_seller_worklet.reset();

  WaitForReportWinAndRunCallback(kBidderReportUrl);
  interest_group_manager_impl_->ExpectReports(
      {{InterestGroupManagerImpl::ReportType::kSendReportTo,
        kBidderReportUrl}});

  WaitForCompletionExpectingErrors(
      {"https://component.seller.origin.test/component_seller_script.js "
       "crashed."});
}

// Test case where a bad report URL is received over Mojo from the seller
// worklet. Bad report URLs should be rejected in the Mojo process, so this
// results in reporting a bad Mojo message, though the reporting phase is
// allowed to continue.
TEST_F(InterestGroupAuctionReporterTest, SingleSellerBadSellerReportUrl) {
  SetUpAndStartSingleSellerAuction();

  interest_group_auction_reporter_->OnNavigateToWinningAdCallback().Run();

  WaitForReportResultAndRunCallback(kSellerScriptUrl,
                                    GURL("http://not.https.test/"));
  interest_group_manager_impl_->ExpectReports({});
  EXPECT_EQ("Invalid seller report URL", TakeBadMessage());

  WaitForReportWinAndRunCallback(kBidderReportUrl);
  interest_group_manager_impl_->ExpectReports(
      {{InterestGroupManagerImpl::ReportType::kSendReportTo,
        kBidderReportUrl}});

  WaitForCompletion();
}

// Test case where a bad report URL is received over Mojo from the component
// seller worklet. Bad report URLs should be rejected in the Mojo process, so
// this results in reporting a bad Mojo message, though the reporting phase is
// allowed to continue.
TEST_F(InterestGroupAuctionReporterTest, ComponentAuctionBadSellerReportUrl) {
  SetUpAndStartComponentAuction();

  interest_group_auction_reporter_->OnNavigateToWinningAdCallback().Run();

  WaitForReportResultAndRunCallback(kSellerScriptUrl, kSellerReportUrl);
  interest_group_manager_impl_->ExpectReports(
      {{InterestGroupManagerImpl::ReportType::kSendReportTo,
        kSellerReportUrl}});

  WaitForReportResultAndRunCallback(kComponentSellerScriptUrl,
                                    GURL("http://not.https.test/"));
  interest_group_manager_impl_->ExpectReports({});
  EXPECT_EQ("Invalid seller report URL", TakeBadMessage());

  WaitForReportWinAndRunCallback(kBidderReportUrl);
  interest_group_manager_impl_->ExpectReports(
      {{InterestGroupManagerImpl::ReportType::kSendReportTo,
        kBidderReportUrl}});

  WaitForCompletion();
}

// Test case where a bad report URL is received over Mojo from the bidder
// worklet. Bad report URLs should be rejected in the Mojo process, so this
// results in reporting a bad Mojo message, though the reporting phase is
// allowed to continue.
TEST_F(InterestGroupAuctionReporterTest, SingleSellerBadBidderReportUrl) {
  SetUpAndStartSingleSellerAuction();

  interest_group_auction_reporter_->OnNavigateToWinningAdCallback().Run();

  WaitForReportResultAndRunCallback(kSellerScriptUrl, kSellerReportUrl);
  interest_group_manager_impl_->ExpectReports(
      {{InterestGroupManagerImpl::ReportType::kSendReportTo,
        kSellerReportUrl}});

  WaitForReportWinAndRunCallback(GURL("http://not.https.test/"));
  interest_group_manager_impl_->ExpectReports({});
  EXPECT_EQ("Invalid bidder report URL", TakeBadMessage());

  WaitForCompletion();
}

TEST_F(InterestGroupAuctionReporterTest, DebugReportsEarlyNavigation) {
  const GURL kDebugWinReport1("https://debug.win.report.test/report-1");
  const GURL kDebugWinReport2("https://debug.win.report.test/report-2");
  debug_win_report_urls_ = {kDebugWinReport1, kDebugWinReport2};

  const GURL kDebugLossReport1("https://debug.loss.report.test/report-1");
  const GURL kDebugLossReport2("https://debug.loss.report.test/report-2");
  debug_loss_report_urls_ = {kDebugLossReport1, kDebugLossReport2};

  SetUpAndStartSingleSellerAuction();
  interest_group_auction_reporter_->OnNavigateToWinningAdCallback().Run();
  interest_group_manager_impl_->ExpectReports(
      {{InterestGroupManagerImpl::ReportType::kDebugWin, kDebugWinReport1},
       {InterestGroupManagerImpl::ReportType::kDebugWin, kDebugWinReport2},
       {InterestGroupManagerImpl::ReportType::kDebugLoss, kDebugLossReport1},
       {InterestGroupManagerImpl::ReportType::kDebugLoss, kDebugLossReport2}});

  WaitForReportResultAndRunCallback(kSellerScriptUrl, absl::nullopt);
  interest_group_manager_impl_->ExpectReports({});
  WaitForReportWinAndRunCallback(absl::nullopt);
  interest_group_manager_impl_->ExpectReports({});

  WaitForCompletion();
}

TEST_F(InterestGroupAuctionReporterTest, DebugReportsLateNavigation) {
  const GURL kDebugWinReport1("https://debug.win.report.test/report-1");
  const GURL kDebugWinReport2("https://debug.win.report.test/report-2");
  debug_win_report_urls_ = {kDebugWinReport1, kDebugWinReport2};

  const GURL kDebugLossReport1("https://debug.loss.report.test/report-1");
  const GURL kDebugLossReport2("https://debug.loss.report.test/report-2");
  debug_loss_report_urls_ = {kDebugLossReport1, kDebugLossReport2};

  SetUpAndStartSingleSellerAuction();

  WaitForReportResultAndRunCallback(kSellerScriptUrl, absl::nullopt);
  interest_group_manager_impl_->ExpectReports({});
  WaitForReportWinAndRunCallback(absl::nullopt);
  interest_group_manager_impl_->ExpectReports({});

  interest_group_auction_reporter_->OnNavigateToWinningAdCallback().Run();
  interest_group_manager_impl_->ExpectReports(
      {{InterestGroupManagerImpl::ReportType::kDebugWin, kDebugWinReport1},
       {InterestGroupManagerImpl::ReportType::kDebugWin, kDebugWinReport2},
       {InterestGroupManagerImpl::ReportType::kDebugLoss, kDebugLossReport1},
       {InterestGroupManagerImpl::ReportType::kDebugLoss, kDebugLossReport2}});

  WaitForCompletion();
}

// Check that the winning interest group and bids are reported to the
// InterestGroupManager, in the case where the fenced frame is navigated to
// before any reporting scripts have run.
TEST_F(InterestGroupAuctionReporterTest, RecordWinAndBids) {
  SetUpAndStartSingleSellerAuction();
  ExpectNoWinsRecorded();
  EXPECT_THAT(interest_group_manager_impl_->TakeInterestGroupsThatBid(),
              testing::UnorderedElementsAre());

  // The win and bids should be recorded immediately upon navigation.
  interest_group_auction_reporter_->OnNavigateToWinningAdCallback().Run();
  ExpectWinRecordedOnce();
  EXPECT_THAT(
      interest_group_manager_impl_->TakeInterestGroupsThatBid(),
      testing::UnorderedElementsAreArray(kExpectedInterestGroupsThatBid));

  WaitForReportResultAndRunCallback(kSellerScriptUrl, absl::nullopt);
  WaitForReportWinAndRunCallback(absl::nullopt);
  WaitForCompletion();

  // The win and bids should have been recorded only once.
  ExpectWinRecordedOnce();
  EXPECT_THAT(interest_group_manager_impl_->TakeInterestGroupsThatBid(),
              testing::UnorderedElementsAre());
}

// Check that the winning interest group and bids are reported to the
// InterestGroupManager, in the case where the fenced frame is navigated to only
// after all reporting scripts have been run.
TEST_F(InterestGroupAuctionReporterTest, RecordWinAndBidsLateNavigation) {
  SetUpAndStartSingleSellerAuction();
  EXPECT_THAT(interest_group_manager_impl_->TakeInterestGroupsThatBid(),
              testing::UnorderedElementsAre());

  WaitForReportResultAndRunCallback(kSellerScriptUrl, absl::nullopt);
  WaitForReportWinAndRunCallback(absl::nullopt);

  // Running reporting scripts should not cause the win or any bids to be
  // recorded.
  ExpectNoWinsRecorded();
  EXPECT_THAT(interest_group_manager_impl_->TakeInterestGroupsThatBid(),
              testing::UnorderedElementsAre());

  // The bids should be recorded immediately upon navigation.
  interest_group_auction_reporter_->OnNavigateToWinningAdCallback().Run();
  ExpectWinRecordedOnce();
  EXPECT_THAT(
      interest_group_manager_impl_->TakeInterestGroupsThatBid(),
      testing::UnorderedElementsAreArray(kExpectedInterestGroupsThatBid));

  WaitForCompletion();

  // The win and bids should have been recorded only once.
  ExpectWinRecordedOnce();
  EXPECT_THAT(interest_group_manager_impl_->TakeInterestGroupsThatBid(),
              testing::UnorderedElementsAre());
}

// Check that the passed in `k_anon_keys_to_join` are reported to the
// InterestGroupManager, in the case where the fenced frame is navigated to
// before any reporting scripts have run.
TEST_F(InterestGroupAuctionReporterTest, RecordKAnonKeysToJoin) {
  SetUpAndStartSingleSellerAuction();

  // Have to spin all message loops to flush any k-anon set join events.
  task_environment()->RunUntilIdle();
  EXPECT_THAT(interest_group_manager_impl_->TakeJoinedKAnonSets(),
              testing::UnorderedElementsAre());

  // The k-anon keys be recorded immediately upon navigation.
  interest_group_auction_reporter_->OnNavigateToWinningAdCallback().Run();
  task_environment()->RunUntilIdle();
  EXPECT_THAT(interest_group_manager_impl_->TakeJoinedKAnonSets(),
              testing::UnorderedElementsAreArray(k_anon_keys_to_join_));

  WaitForReportResultAndRunCallback(kSellerScriptUrl, absl::nullopt);
  WaitForReportWinAndRunCallback(absl::nullopt);
  WaitForCompletion();

  // The k-anon keys should have been recorded only once.
  task_environment()->RunUntilIdle();
  EXPECT_THAT(interest_group_manager_impl_->TakeJoinedKAnonSets(),
              testing::UnorderedElementsAre());
}

// Check that the passed in `k_anon_keys_to_join` are reported to the
// InterestGroupManager, in the case where the fenced frame is navigated to only
// after all reporting scripts have been run.
TEST_F(InterestGroupAuctionReporterTest, RecordKAnonKeysToJoinLateNavigation) {
  SetUpAndStartSingleSellerAuction();
  EXPECT_THAT(interest_group_manager_impl_->TakeJoinedKAnonSets(),
              testing::UnorderedElementsAre());

  WaitForReportResultAndRunCallback(kSellerScriptUrl, absl::nullopt);
  WaitForReportWinAndRunCallback(absl::nullopt);

  // Running reporting scripts should not cause the k-anon keys to be recorded.
  ExpectNoWinsRecorded();
  // Have to spin all message loops to flush any k-anon set join events.
  task_environment()->RunUntilIdle();
  EXPECT_THAT(interest_group_manager_impl_->TakeJoinedKAnonSets(),
              testing::UnorderedElementsAre());

  // The k-anon keys recorded immediately upon navigation.
  interest_group_auction_reporter_->OnNavigateToWinningAdCallback().Run();
  task_environment()->RunUntilIdle();
  EXPECT_THAT(interest_group_manager_impl_->TakeJoinedKAnonSets(),
              testing::UnorderedElementsAreArray(k_anon_keys_to_join_));

  WaitForCompletion();

  // The k-anon keys should have been recorded only once.
  task_environment()->RunUntilIdle();
  EXPECT_THAT(interest_group_manager_impl_->TakeJoinedKAnonSets(),
              testing::UnorderedElementsAre());
}

// Test that nothing is recorded and no reports are sent in the case that the
// reporting scripts are successfully run, but the frame is never navigated to.
TEST_F(InterestGroupAuctionReporterTest, NoNavigation) {
  SetUpAndStartSingleSellerAuction();
  WaitForReportResultAndRunCallback(kSellerScriptUrl, kSellerReportUrl);
  WaitForReportWinAndRunCallback(kBidderReportUrl);
  interest_group_auction_reporter_.reset();

  // Have to spin all message loops to flush any k-anon set join events.
  task_environment()->RunUntilIdle();
  EXPECT_THAT(interest_group_manager_impl_->TakeJoinedKAnonSets(),
              testing::UnorderedElementsAre());
  ExpectNoWinsRecorded();
  EXPECT_THAT(interest_group_manager_impl_->TakeInterestGroupsThatBid(),
              testing::UnorderedElementsAre());
  interest_group_manager_impl_->ExpectReports({});
}

// Test multiple navigations result in only a single set of reports, and
// metadata being recorded exactly once once by the InterestGroupManager.
TEST_F(InterestGroupAuctionReporterTest, MultipleNavigations) {
  SetUpAndStartSingleSellerAuction();
  base::RepeatingClosure callback =
      interest_group_auction_reporter_->OnNavigateToWinningAdCallback();
  callback.Run();
  callback.Run();

  WaitForReportResultAndRunCallback(kSellerScriptUrl, kSellerReportUrl);
  callback.Run();
  callback.Run();

  WaitForReportWinAndRunCallback(kBidderReportUrl);
  callback.Run();
  callback.Run();

  WaitForCompletion();
  // It should be safe to invoke the callback after completion.
  callback.Run();
  callback.Run();

  interest_group_auction_reporter_.reset();
  // It should be safe to invoke the callback after the reporter has been
  // destroyed.
  callback.Run();
  callback.Run();

  // Have to spin all message loops to flush any k-anon set join events.
  task_environment()->RunUntilIdle();

  // The InterestGroupManager should have recorded metadata from the reporter
  // exactly once.
  EXPECT_THAT(interest_group_manager_impl_->TakeJoinedKAnonSets(),
              testing::UnorderedElementsAreArray(k_anon_keys_to_join_));
  ExpectWinRecordedOnce();
  EXPECT_THAT(
      interest_group_manager_impl_->TakeInterestGroupsThatBid(),
      testing::UnorderedElementsAreArray(kExpectedInterestGroupsThatBid));

  // Reports should also have been sent only once.
  interest_group_manager_impl_->ExpectReports(
      {{InterestGroupManagerImpl::ReportType::kSendReportTo, kSellerReportUrl},
       {InterestGroupManagerImpl::ReportType::kSendReportTo,
        kBidderReportUrl}});
}

}  // namespace
}  // namespace content
