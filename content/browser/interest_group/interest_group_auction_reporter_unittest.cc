// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_auction_reporter.h"

#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "content/browser/fenced_frame/fenced_frame_reporter.h"
#include "content/browser/interest_group/ad_auction_page_data.h"
#include "content/browser/interest_group/auction_worklet_manager.h"
#include "content/browser/interest_group/header_direct_from_seller_signals.h"
#include "content/browser/interest_group/interest_group_k_anonymity_manager.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/interest_group/interest_group_pa_report_util.h"
#include "content/browser/interest_group/mock_auction_process_manager.h"
#include "content/browser/interest_group/subresource_url_builder.h"
#include "content/browser/interest_group/test_interest_group_manager_impl.h"
#include "content/browser/interest_group/test_interest_group_private_aggregation_manager.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/page_user_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "content/services/auction_worklet/public/mojom/real_time_reporting.mojom-forward.h"
#include "content/test/test_content_browser_client.h"
#include "mojo/public/cpp/system/functions.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/fenced_frame/redacted_fenced_frame_config.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/common/interest_group/test_interest_group_builder.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom.h"
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
      /*direct_from_seller_signals=*/std::nullopt);
  // Also must not be null.
  out.direct_from_seller_signals_header_ad_slot =
      base::MakeRefCounted<HeaderDirectFromSellerSignals::Result>();

  // The specific values these are assigned to don't matter for these tests, but
  // they don't have default initializers, so have to set them to placate memory
  // tools.
  out.bid = 1;
  out.rounded_bid = 1;
  out.bid_in_seller_currency = 10;
  out.score = 1;
  out.highest_scoring_other_bid = 0;
  out.trace_id = 0;
  return out;
}

// Helper to avoid excess boilerplate.
template <typename... Ts>
auto ElementsAreRequests(Ts&... requests) {
  static_assert(
      std::conjunction<std::is_same<
          std::remove_const_t<Ts>,
          auction_worklet::mojom::PrivateAggregationRequestPtr>...>::value);
  // Need to use `std::ref` as `mojo::StructPtr`s are move-only.
  return testing::UnorderedElementsAre(testing::Eq(std::ref(requests))...);
}

// Helper to avoid excess boilerplate.
template <typename... Ts>
auto ElementsAreContributions(Ts&... requests) {
  static_assert(
      std::conjunction<std::is_same<
          std::remove_const_t<Ts>,
          auction_worklet::mojom::RealTimeReportingContributionPtr>...>::value);
  // Need to use `std::ref` as `mojo::StructPtr`s are move-only.
  return testing::UnorderedElementsAre(testing::Eq(std::ref(requests))...);
}

class EventReportingAttestationBrowserClient : public TestContentBrowserClient {
 public:
  bool IsPrivacySandboxReportingDestinationAttested(
      content::BrowserContext* browser_context,
      const url::Origin& destination_origin,
      content::PrivacySandboxInvokingAPI invoking_api) override {
    return true;
  }
};

// These tests cover the InterestGroupAuctionReporter state machine with respect
// to auction worklets and sending reports. All tests use mock auction worklets.
// Passing arguments correctly to reporting worklets is not covered by these
// tests, but rather by the AuctionRunner tests.
// TODO(crbug.com/334053709): Add test with reporting IDs.
class InterestGroupAuctionReporterTest
    : public RenderViewHostTestHarness,
      public AuctionWorkletManager::Delegate {
 public:
  InterestGroupAuctionReporterTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        winning_bid_info_(GetWinningBidInfo()) {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{blink::features::kPrivateAggregationApi,
                               {{"fledge_extensions_enabled", "true"}}},
                              {blink::features::
                                   kPrivateAggregationApiFilteringIds,
                               {}}},
        /*disabled_features=*/{});

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

    // Join the interest groups that "won" and "lost" the auction - this matters
    // for tests that make sure the interest group is updated correctly.
    const blink::InterestGroup& interest_group =
        winning_bid_info_.storage_interest_group->interest_group;
    interest_group_manager_impl_->JoinInterestGroup(
        interest_group,
        /*joining_url=*/kWinningBidderOrigin.GetURL());

    auto losing_interest_group =
        blink::TestInterestGroupBuilder(kLosingBidderOrigin, kLosingBidderName)
            .SetBiddingUrl(kLosingBidderScriptUrl)
            // An interest group needs at least one ad to participate in an
            // auction.
            .SetAds({{{GURL("https://ad.render.url.test/"), "null"}}})
            .Build();
    interest_group_manager_impl_->JoinInterestGroup(
        losing_interest_group,
        /*joining_url=*/kLosingBidderOrigin.GetURL());

    winning_bid_info_.render_url = GURL((*interest_group.ads)[0].render_url());
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
        HashedKAnonKeyForAdBid(interest_group,
                               (*interest_group.ads)[0].render_url()),
        HashedKAnonKeyForAdNameReporting(
            interest_group, (*interest_group.ads)[0],
            /*selected_buyer_and_seller_reporting_id=*/std::nullopt),
    };
    k_anon_keys_to_join_ =
        base::flat_set<std::string>(std::move(k_anon_keys_to_join));
  }

  ~InterestGroupAuctionReporterTest() override = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    auction_worklet_manager_ = std::make_unique<AuctionWorkletManager>(
        &auction_process_manager_, kTopFrameOrigin, kFrameOrigin, this);
  }

  void TearDown() override {
    // All private aggregation requests should have been accounted for.
    EXPECT_THAT(private_aggregation_manager_.TakePrivateAggregationRequests(),
                testing::UnorderedElementsAre());

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
    auction_worklet_manager_.reset();

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
            /*ad=*/"null", /*bid=*/std::nullopt, /*bid_currency=*/std::nullopt);
  }

  void SetUpReporterAndStart() {
    interest_group_auction_reporter_ =
        std::make_unique<InterestGroupAuctionReporter>(
            interest_group_manager_impl_.get(), auction_worklet_manager_.get(),
            /*browser_context=*/browser_context(),
            &private_aggregation_manager_,
            private_aggregation_manager_
                .GetLogPrivateAggregationRequestsCallback(),
            base::BindRepeating(
                &InterestGroupAuctionReporterTest::GetAdAuctionPageDataCallback,
                base::Unretained(this)),
            std::move(auction_config_), kDevtoolsAuctionId, kTopFrameOrigin,
            kFrameOrigin, frame_client_security_state_.Clone(),
            dummy_report_shared_url_loader_factory_,
            auction_worklet::mojom::KAnonymityBidMode::kNone, false,
            std::move(winning_bid_info_), std::move(seller_winning_bid_info_),
            std::move(component_seller_winning_bid_info_),
            /*interest_groups_that_bid=*/
            blink::InterestGroupSet{{kWinningBidderOrigin, kWinningBidderName},
                                    {kLosingBidderOrigin, kLosingBidderName}},
            std::move(debug_win_report_urls_),
            std::move(debug_loss_report_urls_), k_anon_keys_to_join_,
            std::move(private_aggregation_requests_reserved_),
            std::move(private_aggregation_event_map_),
            std::move(all_participanta_data_),
            std::move(real_time_contributions_));
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
      std::optional<GURL> report_url,
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
      std::optional<GURL> report_url,
      base::flat_map<std::string, GURL> ad_beacon_map = {},
      base::flat_map<std::string, std::string> ad_macro_map = {},
      std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>
          pa_requests = {},
      std::vector<std::string> errors = {}) {
    auction_process_manager_.WaitForWinningBidderReload();
    std::unique_ptr<MockBidderWorklet> bidder_worklet =
        auction_process_manager_.TakeBidderWorklet(kWinningBidderScriptUrl);
    bidder_worklet->WaitForReportWin();
    bidder_worklet->InvokeReportWinCallback(
        std::move(report_url), std::move(ad_beacon_map),
        std::move(ad_macro_map), std::move(pa_requests), std::move(errors));

    // Note that the bidder pipe is not automatically flushed on destruction, so
    // need to destroy it manually. Flushing the pipe ensures that the reporter
    // has received the response, and any resulting reports have been queued.
    bidder_worklet->Flush();
  }

  // Helper to make a std::vector from PrivateAggregationRequestPtrs.
  // Initializer lists for vectors can't have move-only types, so this works
  // around that.
  std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>
  MakeRequestPtrVector(
      auction_worklet::mojom::PrivateAggregationRequestPtr request1,
      auction_worklet::mojom::PrivateAggregationRequestPtr request2 = nullptr) {
    std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr> out;
    out.emplace_back(std::move(request1));
    if (request2) {
      out.emplace_back(std::move(request2));
    }
    return out;
  }

  // Checks that the win has not yet been recorded by the InterestGroupManager.
  void ExpectNoWinsRecorded() const {
    std::optional<SingleStorageInterestGroup> interest_group =
        interest_group_manager_impl_->BlockingGetInterestGroup(
            kWinningBidderOrigin, kWinningBidderName);
    ASSERT_TRUE(interest_group);
    EXPECT_EQ(
        0u, interest_group.value()->bidding_browser_signals->prev_wins.size());
  }

  // Checks that the win has been recorded once and only once by the
  // InterestGroupManager.
  void ExpectWinRecordedOnce() const {
    std::optional<SingleStorageInterestGroup> interest_group =
        interest_group_manager_impl_->BlockingGetInterestGroup(
            kWinningBidderOrigin, kWinningBidderName);
    ASSERT_TRUE(interest_group);
    const std::vector<blink::mojom::PreviousWinPtr>* prev_wins =
        &interest_group.value()->bidding_browser_signals->prev_wins;
    ASSERT_EQ(1u, prev_wins->size());
    EXPECT_EQ((*prev_wins)[0]->ad_json, kWinningAdMetadata);
  }

  void ExpectBidsForKey(const url::Origin& origin,
                        const std::string& name,
                        int expected_bids) {
    std::optional<SingleStorageInterestGroup> interest_group =
        interest_group_manager_impl_->BlockingGetInterestGroup(origin, name);
    ASSERT_TRUE(interest_group);
    EXPECT_EQ(expected_bids,
              interest_group.value()->bidding_browser_signals->bid_count)
        << origin << "," << name;
  }

  void ExpectNoBidsRecorded() {
    ExpectBidsForKey(kWinningBidderOrigin, kWinningBidderName, 0);
    ExpectBidsForKey(kLosingBidderOrigin, kLosingBidderName, 0);
  }

  void ExpectBidsRecordedOnce() {
    ExpectBidsForKey(kWinningBidderOrigin, kWinningBidderName, 1);
    ExpectBidsForKey(kLosingBidderOrigin, kLosingBidderName, 1);
  }

  // AuctionWorkletManager::Delegate implementation.
  //
  // Note that none of these matter for these tests, as the the mock worklet
  // classes don't make network requests, but a real AuctionWorkletManager is
  // used, which expects most of these methods to return non-null objects.
  network::mojom::URLLoaderFactory* GetFrameURLLoaderFactory() override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }
  network::mojom::URLLoaderFactory* GetTrustedURLLoaderFactory() override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }
  void PreconnectSocket(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key) override {
    NOTREACHED_IN_MIGRATION();
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

  void WaitForCompletion() { WaitForCompletionExpectingErrors({}); }

  void WaitForCompletionExpectingErrors(
      const std::vector<std::string>& expected_errors) {
    reporter_run_loop_.Run();
    EXPECT_THAT(errors_, testing::UnorderedElementsAreArray(expected_errors));
  }

  // Gets and clear most recent bad Mojo message.
  std::string TakeBadMessage() { return std::move(bad_message_); }

 protected:
  InterestGroupAuctionReporter::WinningBidInfo GetWinningBidInfo() {
    StorageInterestGroup storage_interest_group;
    storage_interest_group.interest_group =
        blink::TestInterestGroupBuilder(kWinningBidderOrigin,
                                        kWinningBidderName)
            .SetBiddingUrl(kWinningBidderScriptUrl)
            // An interest group needs at least one ad to participate in an
            // auction.
            .SetAds({{{GURL("https://ad.render.url.test/"),
                       "\"This be metadata\""}}})
            .Build();
    std::vector<StorageInterestGroup> vec;
    vec.push_back(std::move(storage_interest_group));
    auto owner_igs =
        base::MakeRefCounted<StorageInterestGroups>(std::move(vec));
    InterestGroupAuctionReporter::WinningBidInfo winning_bid_info(
        owner_igs->GetInterestGroups()[0]);
    return winning_bid_info;
  }

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

  AdAuctionPageData* GetAdAuctionPageDataCallback() {
    return PageUserData<AdAuctionPageData>::GetOrCreateForPage(
        web_contents()->GetPrimaryPage());
  }

  base::test::ScopedFeatureList feature_list_;

  EventReportingAttestationBrowserClient browser_client_;
  ScopedContentBrowserClientSetting browser_client_setting_{&browser_client_};

  const std::string kDevtoolsAuctionId = "123-456";
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
  const GURL kWinningBidderRealTimeReportingUrl = GURL(
      "https://winning.bidder.origin.test/.well-known/interest-group/"
      "real-time-report");

  const GURL kLosingBidderScriptUrl =
      GURL("https://losing.bidder.origin.test/bidder_script.js");
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

  // Private aggregation requests. Their values don't matter, beyond that
  // they're different from each other.
  const auction_worklet::mojom::PrivateAggregationRequestPtr
      kWinningBidderGenerateBidPrivateAggregationRequest =
          auction_worklet::mojom::PrivateAggregationRequest::New(
              auction_worklet::mojom::AggregatableReportContribution::
                  NewHistogramContribution(
                      blink::mojom::AggregatableReportHistogramContribution::
                          New(/*bucket=*/1,
                              /*value=*/2,
                              /*filtering_id=*/std::nullopt)),
              blink::mojom::AggregationServiceMode::kDefault,
              blink::mojom::DebugModeDetails::New());
  const auction_worklet::mojom::PrivateAggregationRequestPtr
      kReportWinPrivateAggregationRequest =
          auction_worklet::mojom::PrivateAggregationRequest::New(
              auction_worklet::mojom::AggregatableReportContribution::
                  NewHistogramContribution(
                      blink::mojom::AggregatableReportHistogramContribution::
                          New(/*bucket=*/3,
                              /*value=*/4,
                              /*filtering_id=*/0)),
              blink::mojom::AggregationServiceMode::kDefault,
              blink::mojom::DebugModeDetails::New());
  const auction_worklet::mojom::PrivateAggregationRequestPtr
      kLosingBidderGenerateBidPrivateAggregationRequest =
          auction_worklet::mojom::PrivateAggregationRequest::New(
              auction_worklet::mojom::AggregatableReportContribution::
                  NewHistogramContribution(
                      blink::mojom::AggregatableReportHistogramContribution::
                          New(/*bucket=*/5,
                              /*value=*/6,
                              /*filtering_id=*/1)),
              blink::mojom::AggregationServiceMode::kDefault,
              blink::mojom::DebugModeDetails::New());
  const auction_worklet::mojom::PrivateAggregationRequestPtr
      kScoreAdPrivateAggregationRequest =
          auction_worklet::mojom::PrivateAggregationRequest::New(
              auction_worklet::mojom::AggregatableReportContribution::
                  NewHistogramContribution(
                      blink::mojom::AggregatableReportHistogramContribution::
                          New(/*bucket=*/7,
                              /*value=*/8,
                              /*filtering_id=*/255)),
              blink::mojom::AggregationServiceMode::kDefault,
              blink::mojom::DebugModeDetails::New());
  const auction_worklet::mojom::PrivateAggregationRequestPtr
      kReportResultPrivateAggregationRequest =
          auction_worklet::mojom::PrivateAggregationRequest::New(
              auction_worklet::mojom::AggregatableReportContribution::
                  NewHistogramContribution(
                      blink::mojom::AggregatableReportHistogramContribution::
                          New(/*bucket=*/9,
                              /*value=*/10,
                              /*filtering_id=*/std::nullopt)),
              blink::mojom::AggregationServiceMode::kDefault,
              blink::mojom::DebugModeDetails::New());
  const auction_worklet::mojom::PrivateAggregationRequestPtr
      kBonusPrivateAggregationRequest =
          auction_worklet::mojom::PrivateAggregationRequest::New(
              auction_worklet::mojom::AggregatableReportContribution::
                  NewHistogramContribution(
                      blink::mojom::AggregatableReportHistogramContribution::
                          New(/*bucket=*/42,
                              /*value=*/24,
                              /*filtering_id=*/std::nullopt)),
              blink::mojom::AggregationServiceMode::kDefault,
              blink::mojom::DebugModeDetails::New());
  const auction_worklet::mojom::PrivateAggregationRequestPtr
      kWinningBidderGenerateBidNonReservedPrivateAggregationRequest =
          auction_worklet::mojom::PrivateAggregationRequest::New(
              auction_worklet::mojom::AggregatableReportContribution::
                  NewForEventContribution(
                      auction_worklet::mojom::
                          AggregatableReportForEventContribution::New(
                              auction_worklet::mojom::ForEventSignalBucket::
                                  NewIdBucket(1),
                              auction_worklet::mojom::ForEventSignalValue::
                                  NewIntValue(2),
                              /*filtering_id=*/std::nullopt,
                              auction_worklet::mojom::EventType::NewNonReserved(
                                  "event_type"))),
              blink::mojom::AggregationServiceMode::kDefault,
              blink::mojom::DebugModeDetails::New());
  const auction_worklet::mojom::PrivateAggregationRequestPtr
      kReservedOncePrivateAggregationRequest =
          auction_worklet::mojom::PrivateAggregationRequest::New(
              auction_worklet::mojom::AggregatableReportContribution::
                  NewForEventContribution(
                      auction_worklet::mojom::
                          AggregatableReportForEventContribution::New(
                              auction_worklet::mojom::ForEventSignalBucket::
                                  NewIdBucket(1),
                              auction_worklet::mojom::ForEventSignalValue::
                                  NewIntValue(2),
                              /*filtering_id=*/std::nullopt,
                              auction_worklet::mojom::EventType::NewReserved(
                                  auction_worklet::mojom::ReservedEventType::
                                      kReservedOnce))),
              blink::mojom::AggregationServiceMode::kDefault,
              blink::mojom::DebugModeDetails::New());
  const auction_worklet::mojom::PrivateAggregationRequestPtr
      kBonusNonReservedPrivateAggregationRequest =
          auction_worklet::mojom::PrivateAggregationRequest::New(
              auction_worklet::mojom::AggregatableReportContribution::
                  NewForEventContribution(
                      auction_worklet::mojom::
                          AggregatableReportForEventContribution::New(
                              auction_worklet::mojom::ForEventSignalBucket::
                                  NewIdBucket(42),
                              auction_worklet::mojom::ForEventSignalValue::
                                  NewIntValue(24),
                              /*filtering_id=*/std::nullopt,
                              auction_worklet::mojom::EventType::NewNonReserved(
                                  "event_type2"))),
              blink::mojom::AggregationServiceMode::kDefault,
              blink::mojom::DebugModeDetails::New());
  const auction_worklet::mojom::PrivateAggregationRequestPtr
      kReportWinNonReservedPrivateAggregationRequest =
          auction_worklet::mojom::PrivateAggregationRequest::New(
              auction_worklet::mojom::AggregatableReportContribution::
                  NewForEventContribution(
                      auction_worklet::mojom::
                          AggregatableReportForEventContribution::New(
                              auction_worklet::mojom::ForEventSignalBucket::
                                  NewIdBucket(3),
                              auction_worklet::mojom::ForEventSignalValue::
                                  NewIntValue(4),
                              /*filtering_id=*/0,
                              auction_worklet::mojom::EventType::NewNonReserved(
                                  "event_type"))),
              blink::mojom::AggregationServiceMode::kDefault,
              blink::mojom::DebugModeDetails::New());

  const auction_worklet::mojom::RealTimeReportingContributionPtr
      kRealTimeReportingContribution =
          auction_worklet::mojom::RealTimeReportingContribution::New(
              /*bucket=*/100,
              /*priority_weight=*/0.5,
              /*latency_threshold=*/std::nullopt);

  const auction_worklet::mojom::RealTimeReportingContributionPtr
      kRealTimeReportingLatencyContribution =
          auction_worklet::mojom::RealTimeReportingContribution::New(
              /*bucket=*/200,
              /*priority_weight=*/2,
              /*latency_threshold=*/1);

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
          network::mojom::PrivateNetworkRequestPolicy::kBlock,
          network::DocumentIsolationPolicy())};

  const GURL kSellerReportUrl =
      GURL("https://seller.report.test/seller-report");
  const GURL kComponentSellerReportUrl =
      GURL("https://component.seller.report.test/component-seller-report");
  const GURL kBidderReportUrl =
      GURL("https://bidder.report.test/bidder-report");

  // These are used in ad beacon tests. Not used by default.
  const FencedFrameReporter::ReportingUrlMap kSellerBeaconMap = {
      {"click", GURL("https://seller.click.test/")},
      {"clock", GURL("https://seller.clock.test/")},
  };
  const FencedFrameReporter::ReportingUrlMap kComponentSellerBeaconMap = {
      {"click", GURL("https://component.seller.click.test/")},
  };
  const FencedFrameReporter::ReportingUrlMap kBuyerBeaconMap = {
      {"click", GURL("https://buyer.click.test/")},
      {"clack", GURL("https://buyer.clack.test/")},
      {"cluck", GURL("https://buyer.cluck.test/")},
  };

  const base::flat_map<std::string, std::string> kAdMacroMap = {
      {"campaign", "123"},
  };

  // The converted `kAdMacroMap` that is passed to fenced frame reporter.
  const FencedFrameReporter::ReportingMacros kAdMacros = {
      {"${campaign}", "123"},
  };

  base::HistogramTester histogram_tester_;

  network::TestURLLoaderFactory dummy_test_url_loader_factory_;

  // SharedURLLoaderFactory used for reports. Reports are short-circuited by the
  // TestInterestGroupManagerImpl before they make it over the network, so this
  // is only used for equality checks around making sure the right factory is
  // passed to it.
  scoped_refptr<network::SharedURLLoaderFactory>
      dummy_report_shared_url_loader_factory_ =
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &dummy_test_url_loader_factory_);

  MockAuctionProcessManager auction_process_manager_;
  std::unique_ptr<AuctionWorkletManager> auction_worklet_manager_;

  std::unique_ptr<blink::AuctionConfig> auction_config_ =
      std::make_unique<blink::AuctionConfig>();
  InterestGroupAuctionReporter::WinningBidInfo winning_bid_info_;
  InterestGroupAuctionReporter::SellerWinningBidInfo seller_winning_bid_info_;
  std::optional<InterestGroupAuctionReporter::SellerWinningBidInfo>
      component_seller_winning_bid_info_;
  // The private aggregation requests passed in to the constructor.
  std::map<PrivateAggregationKey,
           InterestGroupAuctionReporter::PrivateAggregationRequests>
      private_aggregation_requests_reserved_;

  // The non-reserved private aggregation requests passed in to the constructor.
  std::map<std::string,
           InterestGroupAuctionReporter::PrivateAggregationRequests>
      private_aggregation_event_map_;

  InterestGroupAuctionReporter::PrivateAggregationAllParticipantsData
      all_participanta_data_;

  // The real time reporting histograms passed in to the constructor.
  std::map<url::Origin,
           InterestGroupAuctionReporter::RealTimeReportingContributions>
      real_time_contributions_;

  std::unique_ptr<TestInterestGroupManagerImpl> interest_group_manager_impl_ =
      std::make_unique<TestInterestGroupManagerImpl>(
          kFrameOrigin,
          frame_client_security_state_.Clone(),
          dummy_report_shared_url_loader_factory_);

  base::flat_set<std::string> k_anon_keys_to_join_;

  TestInterestGroupPrivateAggregationManager private_aggregation_manager_{
      kTopFrameOrigin};
  std::unique_ptr<InterestGroupAuctionReporter>
      interest_group_auction_reporter_;

  std::string bad_message_;
  std::vector<std::string> errors_;
  base::RunLoop reporter_run_loop_;
};

TEST_F(InterestGroupAuctionReporterTest, SingleSellerNoReports) {
  SetUpAndStartSingleSellerAuction();
  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();

  WaitForReportResultAndRunCallback(kSellerScriptUrl, std::nullopt);
  interest_group_manager_impl_->ExpectReports({});
  WaitForReportWinAndRunCallback(std::nullopt);
  interest_group_manager_impl_->ExpectReports({});

  WaitForCompletion();
}

TEST_F(InterestGroupAuctionReporterTest, ComponentAuctionNoReports) {
  SetUpAndStartComponentAuction();
  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();

  WaitForReportResultAndRunCallback(kSellerScriptUrl, std::nullopt);
  interest_group_manager_impl_->ExpectReports({});
  WaitForReportResultAndRunCallback(kComponentSellerScriptUrl, std::nullopt);
  interest_group_manager_impl_->ExpectReports({});
  WaitForReportWinAndRunCallback(std::nullopt);
  interest_group_manager_impl_->ExpectReports({});

  WaitForCompletion();
}

TEST_F(InterestGroupAuctionReporterTest, SingleSellerReports) {
  SetUpAndStartSingleSellerAuction();
  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();

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

// Test the case where we are in the middle of reporting and the frametree node
// is destroyed, we want to ensure that there is no crash.
TEST_F(InterestGroupAuctionReporterTest,
       SingleSellerReportsWhenFrameTreeNodeIsDestroyed) {
  const base::TimeDelta reporting_interval = base::Milliseconds(50);
  interest_group_manager_impl_->set_use_real_enqueue_reports(true);
  interest_group_manager_impl_->set_max_active_report_requests_for_testing(1);
  interest_group_manager_impl_->set_reporting_interval_for_testing(
      reporting_interval);

  dummy_test_url_loader_factory_.AddResponse(
      "https://seller.report.test/seller-report", "");
  dummy_test_url_loader_factory_.AddResponse(
      "https://bidder.report.test/bidder-report", "");

  SetUpAndStartSingleSellerAuction();

  WaitForReportResultAndRunCallback(kSellerScriptUrl, kSellerReportUrl);
  WaitForReportWinAndRunCallback(kBidderReportUrl);

  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(main_rfh()->GetFrameTreeNodeId())
      .Run();

  WaitForCompletion();
  DeleteContents();
  task_environment()->RunUntilIdle();
  task_environment()->AdvanceClock(reporting_interval);
}

TEST_F(InterestGroupAuctionReporterTest, ComponentAuctionReports) {
  SetUpAndStartComponentAuction();

  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();

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
  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();

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
                                 /*ad_macro_map=*/{},
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

  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();
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

  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();
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

  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();

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

  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();

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

  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();

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

  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();

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

  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();

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

  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();

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

  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();

  WaitForReportResultAndRunCallback(kSellerScriptUrl, kSellerReportUrl);
  interest_group_manager_impl_->ExpectReports(
      {{InterestGroupManagerImpl::ReportType::kSendReportTo,
        kSellerReportUrl}});

  WaitForReportWinAndRunCallback(GURL("http://not.https.test/"));
  interest_group_manager_impl_->ExpectReports({});
  EXPECT_EQ("Invalid bidder report URL", TakeBadMessage());

  WaitForCompletion();
}

TEST_F(InterestGroupAuctionReporterTest, SingleSellerReportBeaconMap) {
  SetUpAndStartSingleSellerAuction();
  // The component seller list should be empty from the start, for a single
  // seller auction.
  EXPECT_THAT(interest_group_auction_reporter_->fenced_frame_reporter()
                  ->GetAdBeaconMapForTesting(),
              testing::UnorderedElementsAre(testing::Pair(
                  blink::FencedFrame::ReportingDestination::kComponentSeller,
                  testing::UnorderedElementsAre())));

  WaitForReportResultAndRunCallback(
      kSellerScriptUrl, /*report_url=*/std::nullopt, kSellerBeaconMap);
  EXPECT_THAT(
      interest_group_auction_reporter_->fenced_frame_reporter()
          ->GetAdBeaconMapForTesting(),
      testing::UnorderedElementsAre(
          testing::Pair(blink::FencedFrame::ReportingDestination::kSeller,
                        testing::UnorderedElementsAreArray(kSellerBeaconMap)),
          testing::Pair(
              blink::FencedFrame::ReportingDestination::kComponentSeller,
              testing::UnorderedElementsAre())));

  WaitForReportWinAndRunCallback(/*report_url=*/std::nullopt, kBuyerBeaconMap);
  EXPECT_THAT(
      interest_group_auction_reporter_->fenced_frame_reporter()
          ->GetAdBeaconMapForTesting(),
      testing::UnorderedElementsAre(
          testing::Pair(blink::FencedFrame::ReportingDestination::kSeller,
                        testing::UnorderedElementsAreArray(kSellerBeaconMap)),
          testing::Pair(
              blink::FencedFrame::ReportingDestination::kComponentSeller,
              testing::UnorderedElementsAre()),
          testing::Pair(blink::FencedFrame::ReportingDestination::kBuyer,
                        testing::UnorderedElementsAreArray(kBuyerBeaconMap))));

  // Invoking the callback has no effect on per-destination reporting maps.
  // Fenced frames navigated to the winning ad use them to trigger reports, so
  // no need to hold them back until a fenced frame is navigated to the winning
  // ad.
  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();

  WaitForCompletion();
  EXPECT_THAT(
      interest_group_auction_reporter_->fenced_frame_reporter()
          ->GetAdBeaconMapForTesting(),
      testing::UnorderedElementsAre(
          testing::Pair(blink::FencedFrame::ReportingDestination::kSeller,
                        testing::UnorderedElementsAreArray(kSellerBeaconMap)),
          testing::Pair(
              blink::FencedFrame::ReportingDestination::kComponentSeller,
              testing::UnorderedElementsAre()),
          testing::Pair(blink::FencedFrame::ReportingDestination::kBuyer,
                        testing::UnorderedElementsAreArray(kBuyerBeaconMap))));
}

TEST_F(InterestGroupAuctionReporterTest, ComponentAuctionReportBeaconMap) {
  SetUpAndStartComponentAuction();
  EXPECT_THAT(interest_group_auction_reporter_->fenced_frame_reporter()
                  ->GetAdBeaconMapForTesting(),
              testing::UnorderedElementsAre());

  WaitForReportResultAndRunCallback(
      kSellerScriptUrl, /*report_url=*/std::nullopt, kSellerBeaconMap);
  EXPECT_THAT(interest_group_auction_reporter_->fenced_frame_reporter()
                  ->GetAdBeaconMapForTesting(),
              testing::UnorderedElementsAre(testing::Pair(
                  blink::FencedFrame::ReportingDestination::kSeller,
                  testing::UnorderedElementsAreArray(kSellerBeaconMap))));

  WaitForReportResultAndRunCallback(kComponentSellerScriptUrl,
                                    /*report_url=*/std::nullopt,
                                    kComponentSellerBeaconMap);
  EXPECT_THAT(
      interest_group_auction_reporter_->fenced_frame_reporter()
          ->GetAdBeaconMapForTesting(),
      testing::UnorderedElementsAre(
          testing::Pair(blink::FencedFrame::ReportingDestination::kSeller,
                        testing::UnorderedElementsAreArray(kSellerBeaconMap)),
          testing::Pair(
              blink::FencedFrame::ReportingDestination::kComponentSeller,
              testing::UnorderedElementsAreArray(kComponentSellerBeaconMap))));

  WaitForReportWinAndRunCallback(/*report_url=*/std::nullopt, kBuyerBeaconMap);
  EXPECT_THAT(
      interest_group_auction_reporter_->fenced_frame_reporter()
          ->GetAdBeaconMapForTesting(),
      testing::UnorderedElementsAre(
          testing::Pair(blink::FencedFrame::ReportingDestination::kSeller,
                        testing::UnorderedElementsAreArray(kSellerBeaconMap)),
          testing::Pair(
              blink::FencedFrame::ReportingDestination::kComponentSeller,
              testing::UnorderedElementsAreArray(kComponentSellerBeaconMap)),
          testing::Pair(blink::FencedFrame::ReportingDestination::kBuyer,
                        testing::UnorderedElementsAreArray(kBuyerBeaconMap))));

  // Invoking the callback has no effect on per-destination reporting maps.
  // Fenced frames navigated to the winning ad use them to trigger reports, so
  // no need to hold them back until a fenced frame is navigated to the winning
  // ad.
  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();

  WaitForCompletion();
  EXPECT_THAT(
      interest_group_auction_reporter_->fenced_frame_reporter()
          ->GetAdBeaconMapForTesting(),
      testing::UnorderedElementsAre(
          testing::Pair(blink::FencedFrame::ReportingDestination::kSeller,
                        testing::UnorderedElementsAreArray(kSellerBeaconMap)),
          testing::Pair(
              blink::FencedFrame::ReportingDestination::kComponentSeller,
              testing::UnorderedElementsAreArray(kComponentSellerBeaconMap)),
          testing::Pair(blink::FencedFrame::ReportingDestination::kBuyer,
                        testing::UnorderedElementsAreArray(kBuyerBeaconMap))));
}

// Test case where a bad report URL is received over Mojo from the seller
// worklet. Bad report URLs should be rejected in the Mojo process, so this
// results in reporting a bad Mojo message, though the reporting phase is
// allowed to continue.
TEST_F(InterestGroupAuctionReporterTest,
       ComponentAuctionReportBeaconMapBadSellerUrl) {
  SetUpAndStartComponentAuction();
  EXPECT_THAT(interest_group_auction_reporter_->fenced_frame_reporter()
                  ->GetAdBeaconMapForTesting(),
              testing::UnorderedElementsAre());

  WaitForReportResultAndRunCallback(
      kSellerScriptUrl, /*report_url=*/std::nullopt,
      /*ad_beacon_map=*/
      {{"click", GURL("https://seller.click.test/")},
       {"clock", GURL("http://http.not.allowed.test/")}});
  EXPECT_EQ("Invalid seller beacon URL for 'clock'", TakeBadMessage());
  EXPECT_THAT(interest_group_auction_reporter_->fenced_frame_reporter()
                  ->GetAdBeaconMapForTesting(),
              testing::UnorderedElementsAre(testing::Pair(
                  blink::FencedFrame::ReportingDestination::kSeller,
                  testing::UnorderedElementsAre())));

  WaitForReportResultAndRunCallback(kComponentSellerScriptUrl,
                                    /*report_url=*/std::nullopt,
                                    kComponentSellerBeaconMap);
  EXPECT_THAT(
      interest_group_auction_reporter_->fenced_frame_reporter()
          ->GetAdBeaconMapForTesting(),
      testing::UnorderedElementsAre(
          testing::Pair(blink::FencedFrame::ReportingDestination::kSeller,
                        testing::UnorderedElementsAre()),
          testing::Pair(
              blink::FencedFrame::ReportingDestination::kComponentSeller,
              testing::UnorderedElementsAreArray(kComponentSellerBeaconMap))));

  WaitForReportWinAndRunCallback(/*report_url=*/std::nullopt, kBuyerBeaconMap);
  EXPECT_THAT(
      interest_group_auction_reporter_->fenced_frame_reporter()
          ->GetAdBeaconMapForTesting(),
      testing::UnorderedElementsAre(
          testing::Pair(blink::FencedFrame::ReportingDestination::kSeller,
                        testing::UnorderedElementsAre()),
          testing::Pair(
              blink::FencedFrame::ReportingDestination::kComponentSeller,
              testing::UnorderedElementsAreArray(kComponentSellerBeaconMap)),
          testing::Pair(blink::FencedFrame::ReportingDestination::kBuyer,
                        testing::UnorderedElementsAreArray(kBuyerBeaconMap))));

  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();
  WaitForCompletion();
}

// Test case where a bad report URL is received over Mojo from the bidder
// worklet. Bad report URLs should be rejected in the Mojo process, so this
// results in reporting a bad Mojo message, though the reporting phase is
// allowed to continue.
TEST_F(InterestGroupAuctionReporterTest,
       ComponentAuctionReportBeaconMapBadBidderUrl) {
  SetUpAndStartComponentAuction();
  EXPECT_THAT(interest_group_auction_reporter_->fenced_frame_reporter()
                  ->GetAdBeaconMapForTesting(),
              testing::UnorderedElementsAre());

  WaitForReportResultAndRunCallback(
      kSellerScriptUrl, /*report_url=*/std::nullopt, kSellerBeaconMap);
  EXPECT_THAT(interest_group_auction_reporter_->fenced_frame_reporter()
                  ->GetAdBeaconMapForTesting(),
              testing::UnorderedElementsAre(testing::Pair(
                  blink::FencedFrame::ReportingDestination::kSeller,
                  testing::UnorderedElementsAreArray(kSellerBeaconMap))));

  WaitForReportResultAndRunCallback(kComponentSellerScriptUrl,
                                    /*report_url=*/std::nullopt,
                                    kComponentSellerBeaconMap);
  EXPECT_THAT(
      interest_group_auction_reporter_->fenced_frame_reporter()
          ->GetAdBeaconMapForTesting(),
      testing::UnorderedElementsAre(
          testing::Pair(blink::FencedFrame::ReportingDestination::kSeller,
                        testing::UnorderedElementsAreArray(kSellerBeaconMap)),
          testing::Pair(
              blink::FencedFrame::ReportingDestination::kComponentSeller,
              testing::UnorderedElementsAreArray(kComponentSellerBeaconMap))));

  WaitForReportWinAndRunCallback(
      /*report_url=*/std::nullopt, /*ad_beacon_map=*/{
          {"click", GURL()}, {"clack", GURL("http://buyer.clack.test/")}});
  EXPECT_EQ("Invalid bidder beacon URL for 'clack'", TakeBadMessage());
  EXPECT_THAT(
      interest_group_auction_reporter_->fenced_frame_reporter()
          ->GetAdBeaconMapForTesting(),
      testing::UnorderedElementsAre(
          testing::Pair(blink::FencedFrame::ReportingDestination::kSeller,
                        testing::UnorderedElementsAreArray(kSellerBeaconMap)),
          testing::Pair(
              blink::FencedFrame::ReportingDestination::kComponentSeller,
              testing::UnorderedElementsAreArray(kComponentSellerBeaconMap)),
          testing::Pair(blink::FencedFrame::ReportingDestination::kBuyer,
                        testing::UnorderedElementsAre())));

  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();
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
  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();
  interest_group_manager_impl_->ExpectReports(
      {{InterestGroupManagerImpl::ReportType::kDebugWin, kDebugWinReport1},
       {InterestGroupManagerImpl::ReportType::kDebugWin, kDebugWinReport2},
       {InterestGroupManagerImpl::ReportType::kDebugLoss, kDebugLossReport1},
       {InterestGroupManagerImpl::ReportType::kDebugLoss, kDebugLossReport2}});

  WaitForReportResultAndRunCallback(kSellerScriptUrl, std::nullopt);
  interest_group_manager_impl_->ExpectReports({});
  WaitForReportWinAndRunCallback(std::nullopt);
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

  WaitForReportResultAndRunCallback(kSellerScriptUrl, std::nullopt);
  interest_group_manager_impl_->ExpectReports({});
  WaitForReportWinAndRunCallback(std::nullopt);
  interest_group_manager_impl_->ExpectReports({});

  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();
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
  ExpectNoBidsRecorded();

  // The win and bids should be recorded immediately upon navigation.
  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();
  ExpectWinRecordedOnce();
  ExpectBidsRecordedOnce();

  WaitForReportResultAndRunCallback(kSellerScriptUrl, std::nullopt);
  WaitForReportWinAndRunCallback(std::nullopt);
  WaitForCompletion();

  // The win and bids should have been recorded only once.
  ExpectWinRecordedOnce();
  ExpectBidsRecordedOnce();
}

// Check that the winning interest group and bids are reported to the
// InterestGroupManager, in the case where the fenced frame is navigated to only
// after all reporting scripts have been run.
TEST_F(InterestGroupAuctionReporterTest, RecordWinAndBidsLateNavigation) {
  SetUpAndStartSingleSellerAuction();
  ExpectNoBidsRecorded();

  WaitForReportResultAndRunCallback(kSellerScriptUrl, std::nullopt);
  WaitForReportWinAndRunCallback(std::nullopt);

  // Running reporting scripts should not cause the win or any bids to be
  // recorded.
  ExpectNoWinsRecorded();
  ExpectNoBidsRecorded();

  // The bids should be recorded immediately upon navigation.
  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();
  ExpectWinRecordedOnce();
  ExpectBidsRecordedOnce();

  WaitForCompletion();

  // The win and bids should have been recorded only once.
  ExpectWinRecordedOnce();
  ExpectBidsRecordedOnce();
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
  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();
  task_environment()->RunUntilIdle();
  EXPECT_THAT(interest_group_manager_impl_->TakeJoinedKAnonSets(),
              testing::UnorderedElementsAreArray(k_anon_keys_to_join_));

  WaitForReportResultAndRunCallback(kSellerScriptUrl, std::nullopt);
  WaitForReportWinAndRunCallback(std::nullopt);
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

  WaitForReportResultAndRunCallback(kSellerScriptUrl, std::nullopt);
  WaitForReportWinAndRunCallback(std::nullopt);

  // Running reporting scripts should not cause the k-anon keys to be recorded.
  ExpectNoWinsRecorded();
  // Have to spin all message loops to flush any k-anon set join events.
  task_environment()->RunUntilIdle();
  EXPECT_THAT(interest_group_manager_impl_->TakeJoinedKAnonSets(),
              testing::UnorderedElementsAre());

  // The k-anon keys recorded immediately upon navigation.
  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();
  task_environment()->RunUntilIdle();
  EXPECT_THAT(interest_group_manager_impl_->TakeJoinedKAnonSets(),
              testing::UnorderedElementsAreArray(k_anon_keys_to_join_));

  WaitForCompletion();

  // The k-anon keys should have been recorded only once.
  task_environment()->RunUntilIdle();
  EXPECT_THAT(interest_group_manager_impl_->TakeJoinedKAnonSets(),
              testing::UnorderedElementsAre());
}

// Check that private aggregation requests are passed along as expected. This
// creates an auction which is both passed aggregation reports from the bidding
// and scoring phase of the auction, and receives more from each reporting
// worklet that's invoked. This covers the case where a navigation occurs before
// the seller's reporting script completes.
TEST_F(InterestGroupAuctionReporterTest, PrivateAggregationRequests) {
  private_aggregation_requests_reserved_[PrivateAggregationKey(kSellerOrigin,
                                                               std::nullopt)]
      .push_back(kScoreAdPrivateAggregationRequest.Clone());
  private_aggregation_requests_reserved_[PrivateAggregationKey(
                                             kWinningBidderOrigin,
                                             std::nullopt)]
      .push_back(kWinningBidderGenerateBidPrivateAggregationRequest.Clone());
  private_aggregation_requests_reserved_[PrivateAggregationKey(
                                             kLosingBidderOrigin, std::nullopt)]
      .push_back(kLosingBidderGenerateBidPrivateAggregationRequest.Clone());

  SetUpAndStartSingleSellerAuction();

  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();

  WaitForReportResultAndRunCallback(
      kSellerScriptUrl, /*report_url=*/std::nullopt, /*ad_beacon_map=*/{},
      MakeRequestPtrVector(kReportResultPrivateAggregationRequest.Clone(),
                           kBonusPrivateAggregationRequest.Clone()));

  // No requests should be sent until all phases are complete.
  EXPECT_THAT(private_aggregation_manager_.TakePrivateAggregationRequests(),
              testing::UnorderedElementsAre());

  // All reserved aggregation requests should be immediately passed along once
  // the auction is complete.
  WaitForReportWinAndRunCallback(
      /*report_url=*/std::nullopt, /*ad_beacon_map=*/{}, /*ad_macro_map=*/{},
      MakeRequestPtrVector(kReportWinPrivateAggregationRequest.Clone(),
                           kBonusPrivateAggregationRequest.Clone()));
  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
      testing::UnorderedElementsAre(
          testing::Pair(
              kSellerOrigin,
              ElementsAreRequests(kScoreAdPrivateAggregationRequest,
                                  kReportResultPrivateAggregationRequest,
                                  kBonusPrivateAggregationRequest)),
          testing::Pair(kWinningBidderOrigin,
                        ElementsAreRequests(
                            kWinningBidderGenerateBidPrivateAggregationRequest,
                            kReportWinPrivateAggregationRequest,
                            kBonusPrivateAggregationRequest)),
          testing::Pair(
              kLosingBidderOrigin,
              ElementsAreRequests(
                  kLosingBidderGenerateBidPrivateAggregationRequest))));

  WaitForCompletion();
}

TEST_F(InterestGroupAuctionReporterTest, InvalidPrivateAggregationRequests) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      blink::features::
          kPrivateAggregationApiProtectedAudienceAdditionalExtensions);
  SetUpAndStartSingleSellerAuction();

  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();

  WaitForReportResultAndRunCallback(
      kSellerScriptUrl, /*report_url=*/std::nullopt, /*ad_beacon_map=*/{},
      MakeRequestPtrVector(kReportResultPrivateAggregationRequest.Clone(),
                           kReservedOncePrivateAggregationRequest.Clone()));

  // No requests should be sent until all phases are complete.
  EXPECT_THAT(private_aggregation_manager_.TakePrivateAggregationRequests(),
              testing::UnorderedElementsAre());
  EXPECT_EQ("Private Aggregation request using disabled features",
            TakeBadMessage());

  // All reserved aggregation requests should be immediately passed along once
  // the auction is complete.
  WaitForReportWinAndRunCallback(
      /*report_url=*/std::nullopt, /*ad_beacon_map=*/{}, /*ad_macro_map=*/{},
      MakeRequestPtrVector(kReportWinPrivateAggregationRequest.Clone(),
                           kReservedOncePrivateAggregationRequest.Clone()));
  EXPECT_EQ("Private Aggregation request using disabled features",
            TakeBadMessage());

  // Due to the invalid messages, incoming PA stuff got discarded.
  EXPECT_THAT(private_aggregation_manager_.TakePrivateAggregationRequests(),
              testing::UnorderedElementsAre());

  WaitForCompletion();
}

TEST_F(InterestGroupAuctionReporterTest, InvalidPrivateAggregationRequests2) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      blink::features::
          kPrivateAggregationApiProtectedAudienceAdditionalExtensions);
  SetUpAndStartSingleSellerAuction();

  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();

  WaitForReportResultAndRunCallback(
      kSellerScriptUrl, /*report_url=*/std::nullopt, /*ad_beacon_map=*/{},
      MakeRequestPtrVector(kReportResultPrivateAggregationRequest.Clone(),
                           kReservedOncePrivateAggregationRequest.Clone()));

  // No requests should be sent until all phases are complete.
  EXPECT_THAT(private_aggregation_manager_.TakePrivateAggregationRequests(),
              testing::UnorderedElementsAre());
  EXPECT_EQ("Reporting Private Aggregation request using reserved.once",
            TakeBadMessage());

  // All reserved aggregation requests should be immediately passed along once
  // the auction is complete.
  WaitForReportWinAndRunCallback(
      /*report_url=*/std::nullopt, /*ad_beacon_map=*/{}, /*ad_macro_map=*/{},
      MakeRequestPtrVector(kReportWinPrivateAggregationRequest.Clone(),
                           kReservedOncePrivateAggregationRequest.Clone()));
  EXPECT_EQ("Reporting Private Aggregation request using reserved.once",
            TakeBadMessage());

  // The invalid PA stuff got discarded.
  EXPECT_THAT(private_aggregation_manager_.TakePrivateAggregationRequests(),
              testing::UnorderedElementsAre());

  WaitForCompletion();
}

// Check that private aggregation requests are passed along as expected. This
// creates an auction which is both passed aggregation reports from the bidding
// and scoring phase of the auction, and receives more from each reporting
// worklet that's invoked. This covers the case where a navigation occurs after
// all reporting scripts have completed.
TEST_F(InterestGroupAuctionReporterTest,
       PrivateAggregationRequestsLateNavigation) {
  private_aggregation_requests_reserved_[PrivateAggregationKey(kSellerOrigin,
                                                               std::nullopt)]
      .push_back(kScoreAdPrivateAggregationRequest.Clone());
  private_aggregation_requests_reserved_[PrivateAggregationKey(
                                             kWinningBidderOrigin,
                                             std::nullopt)]
      .push_back(kWinningBidderGenerateBidPrivateAggregationRequest.Clone());
  private_aggregation_requests_reserved_[PrivateAggregationKey(
                                             kLosingBidderOrigin, std::nullopt)]
      .push_back(kLosingBidderGenerateBidPrivateAggregationRequest.Clone());

  SetUpAndStartSingleSellerAuction();
  EXPECT_THAT(private_aggregation_manager_.TakePrivateAggregationRequests(),
              testing::UnorderedElementsAre());

  WaitForReportResultAndRunCallback(
      kSellerScriptUrl, /*report_url=*/std::nullopt, /*ad_beacon_map=*/{},
      MakeRequestPtrVector(kReportResultPrivateAggregationRequest.Clone(),
                           kBonusPrivateAggregationRequest.Clone()));
  EXPECT_THAT(private_aggregation_manager_.TakePrivateAggregationRequests(),
              testing::UnorderedElementsAre());

  WaitForReportWinAndRunCallback(
      /*report_url=*/std::nullopt, /*ad_beacon_map=*/{}, /*ad_macro_map=*/{},
      MakeRequestPtrVector(kReportWinPrivateAggregationRequest.Clone(),
                           kBonusPrivateAggregationRequest.Clone()));
  EXPECT_THAT(private_aggregation_manager_.TakePrivateAggregationRequests(),
              testing::UnorderedElementsAre());

  // When the navigation finally occurs, all previously queued aggregated
  // requests should be passed along.
  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();
  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
      testing::UnorderedElementsAre(
          testing::Pair(
              kSellerOrigin,
              ElementsAreRequests(kScoreAdPrivateAggregationRequest,
                                  kReportResultPrivateAggregationRequest,
                                  kBonusPrivateAggregationRequest)),
          testing::Pair(kWinningBidderOrigin,
                        ElementsAreRequests(
                            kWinningBidderGenerateBidPrivateAggregationRequest,
                            kReportWinPrivateAggregationRequest,
                            kBonusPrivateAggregationRequest)),
          testing::Pair(
              kLosingBidderOrigin,
              ElementsAreRequests(
                  kLosingBidderGenerateBidPrivateAggregationRequest))));

  WaitForCompletion();
}

// Check that private aggregation requests of non-reserved event types are
// passed along as expected. This creates an auction which is both passed
// aggregation reports from the bidding and scoring phase of the auction, and
// receives more from each reporting worklet that's invoked. This covers the
// case where a navigation occurs before the seller's reporting script
// completes.
TEST_F(InterestGroupAuctionReporterTest,
       PrivateAggregationRequestsNonReserved) {
  private_aggregation_event_map_["event_type"].push_back(
      kWinningBidderGenerateBidPrivateAggregationRequest.Clone());
  private_aggregation_event_map_["event_type2"].push_back(
      kBonusPrivateAggregationRequest.Clone());

  SetUpAndStartSingleSellerAuction();

  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();

  // Nothing should be sent until all phases are complete.
  EXPECT_TRUE(interest_group_auction_reporter_->fenced_frame_reporter()
                  ->GetPrivateAggregationEventMapForTesting()
                  .empty());

  WaitForReportResultAndRunCallback(kSellerScriptUrl,
                                    /*report_url=*/std::nullopt);

  // The non-reserved aggregation requests should be passed along right after
  // the reporting phase.
  WaitForReportWinAndRunCallback(
      /*report_url=*/std::nullopt, /*ad_beacon_map=*/{}, /*ad_macro_map=*/{},
      MakeRequestPtrVector(
          kReportWinNonReservedPrivateAggregationRequest.Clone()));
  EXPECT_THAT(
      interest_group_auction_reporter_->fenced_frame_reporter()
          ->GetPrivateAggregationEventMapForTesting(),
      testing::UnorderedElementsAre(
          testing::Pair("event_type",
                        ElementsAreRequests(
                            kWinningBidderGenerateBidPrivateAggregationRequest,
                            kReportWinPrivateAggregationRequest)),
          testing::Pair("event_type2",
                        ElementsAreRequests(kBonusPrivateAggregationRequest))));

  WaitForCompletion();
}

// Check that private aggregation requests of non-reserved event types are
// passed along as expected. This creates an auction which is both passed
// aggregation reports from the bidding and scoring phase of the auction, and
// receives more from each reporting worklet that's invoked. This covers the
// case where a navigation occurs after all reporting scripts have completed.
TEST_F(InterestGroupAuctionReporterTest,
       PrivateAggregationRequestsNonReservedLateNavigation) {
  private_aggregation_event_map_["event_type"].push_back(
      kWinningBidderGenerateBidPrivateAggregationRequest.Clone());
  private_aggregation_event_map_["event_type2"].push_back(
      kBonusPrivateAggregationRequest.Clone());

  SetUpAndStartSingleSellerAuction();
  EXPECT_TRUE(interest_group_auction_reporter_->fenced_frame_reporter()
                  ->GetPrivateAggregationEventMapForTesting()
                  .empty());

  WaitForReportResultAndRunCallback(kSellerScriptUrl,
                                    /*report_url=*/std::nullopt);

  WaitForReportWinAndRunCallback(
      /*report_url=*/std::nullopt, /*ad_beacon_map=*/{}, /*ad_macro_map=*/{},
      MakeRequestPtrVector(
          kReportWinNonReservedPrivateAggregationRequest.Clone()));
  EXPECT_TRUE(interest_group_auction_reporter_->fenced_frame_reporter()
                  ->GetPrivateAggregationEventMapForTesting()
                  .empty());

  // When the navigation finally occurs, all previously queued aggregated
  // requests should be passed along.
  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();
  EXPECT_THAT(
      interest_group_auction_reporter_->fenced_frame_reporter()
          ->GetPrivateAggregationEventMapForTesting(),
      testing::UnorderedElementsAre(
          testing::Pair("event_type",
                        ElementsAreRequests(
                            kWinningBidderGenerateBidPrivateAggregationRequest,
                            kReportWinPrivateAggregationRequest)),
          testing::Pair("event_type2",
                        ElementsAreRequests(kBonusPrivateAggregationRequest))));

  WaitForCompletion();
}

// Check that real time reporting contributions are passed along as expected.
// This covers the case where a navigation occurs before the seller's reporting
// script completes.
TEST_F(InterestGroupAuctionReporterTest, RealTimeReporting) {
  real_time_contributions_[kWinningBidderOrigin].push_back(
      kRealTimeReportingContribution.Clone());
  real_time_contributions_[kWinningBidderOrigin].push_back(
      kRealTimeReportingLatencyContribution.Clone());

  SetUpAndStartSingleSellerAuction();
  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();

  EXPECT_THAT(
      interest_group_manager_impl_->TakeRealTimeContributions(),
      testing::UnorderedElementsAre(testing::Pair(
          kWinningBidderOrigin,
          ElementsAreContributions(kRealTimeReportingContribution,
                                   kRealTimeReportingLatencyContribution))));

  WaitForReportResultAndRunCallback(kSellerScriptUrl,
                                    /*report_url=*/std::nullopt);
  interest_group_manager_impl_->ExpectReports({});
  WaitForReportWinAndRunCallback(/*report_url=*/std::nullopt);
  interest_group_manager_impl_->ExpectReports({});

  WaitForCompletion();
}

// Check that real time reporting histograms are passed along as expected. This
// covers the case where a navigation occurs after all reporting scripts have
// completed.
TEST_F(InterestGroupAuctionReporterTest, RealTimeReportingLateNavigation) {
  real_time_contributions_[kWinningBidderOrigin].push_back(
      kRealTimeReportingContribution.Clone());
  real_time_contributions_[kWinningBidderOrigin].push_back(
      kRealTimeReportingLatencyContribution.Clone());

  SetUpAndStartSingleSellerAuction();
  interest_group_manager_impl_->ExpectReports({});
  EXPECT_TRUE(
      interest_group_manager_impl_->TakeRealTimeContributions().empty());

  WaitForReportResultAndRunCallback(kSellerScriptUrl,
                                    /*report_url=*/std::nullopt);
  interest_group_manager_impl_->ExpectReports({});
  EXPECT_TRUE(
      interest_group_manager_impl_->TakeRealTimeContributions().empty());
  WaitForReportWinAndRunCallback(/*report_url=*/std::nullopt);
  interest_group_manager_impl_->ExpectReports({});
  EXPECT_TRUE(
      interest_group_manager_impl_->TakeRealTimeContributions().empty());

  // When the navigation finally occurs, all previously queued requests should
  // be passed along.
  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();

  EXPECT_THAT(
      interest_group_manager_impl_->TakeRealTimeContributions(),
      testing::UnorderedElementsAre(testing::Pair(
          kWinningBidderOrigin,
          ElementsAreContributions(kRealTimeReportingContribution,
                                   kRealTimeReportingLatencyContribution))));

  WaitForCompletion();
}

// Check that private aggregation requests are passed along to trigger use
// counter logging as appropriate.
TEST_F(InterestGroupAuctionReporterTest,
       PrivateAggregationLoggingForUseCounter) {
  SetUpAndStartSingleSellerAuction();
  WaitForReportResultAndRunCallback(
      kSellerScriptUrl,
      /*report_url=*/std::nullopt, /*ad_beacon_map=*/{},
      MakeRequestPtrVector(kReportResultPrivateAggregationRequest.Clone()));
  WaitForReportWinAndRunCallback(
      /*report_url=*/std::nullopt, /*ad_beacon_map=*/{}, /*ad_macro_map=*/{},
      MakeRequestPtrVector(
          kReportWinNonReservedPrivateAggregationRequest.Clone()));

  // Requests encountered in reportResult() and reportWin() are passed along.
  EXPECT_THAT(
      private_aggregation_manager_.TakeLoggedPrivateAggregationRequests(),
      ElementsAreRequests(kReportResultPrivateAggregationRequest,
                          kReportWinNonReservedPrivateAggregationRequest));
}

// Check that no private aggregation requests are passed along to trigger use
// counter logging if the API was not used.
TEST_F(InterestGroupAuctionReporterTest,
       PrivateAggregationLoggingForUseCounterNotUsed) {
  SetUpAndStartSingleSellerAuction();
  WaitForReportResultAndRunCallback(kSellerScriptUrl,
                                    /*report_url=*/std::nullopt);
  WaitForReportWinAndRunCallback(
      /*report_url=*/std::nullopt);
  EXPECT_TRUE(
      private_aggregation_manager_.TakeLoggedPrivateAggregationRequests()
          .empty());
}

// Test the case that the InterestGroupAutionReporter is destroyed while calling
// the top-level seller's reportResult() method, before navigation. This
// primarily serves to test UMA.
TEST_F(InterestGroupAuctionReporterTest,
       DestroyedDuringSellerReportResultBeforeNavigation) {
  SetUpAndStartComponentAuction();

  auction_process_manager_.WaitForWinningSellerReload();
  std::unique_ptr<MockSellerWorklet> seller_worklet =
      auction_process_manager_.TakeSellerWorklet(kSellerScriptUrl);
  seller_worklet->set_expect_send_pending_signals_requests_called(false);
  interest_group_auction_reporter_.reset();

  interest_group_manager_impl_->ExpectReports({});
  histogram_tester_.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.FinalReporterState",
      InterestGroupAuctionReporter::ReporterState::kAdNotUsed, 1);
}

// Test the case that the InterestGroupAutionReporter is destroyed while calling
// the top-level seller's reportResult() method, after navigation. This
// primarily serves to test UMA.
TEST_F(InterestGroupAuctionReporterTest, DestroyedDuringSellerReportResult) {
  SetUpAndStartComponentAuction();
  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();

  auction_process_manager_.WaitForWinningSellerReload();
  std::unique_ptr<MockSellerWorklet> seller_worklet =
      auction_process_manager_.TakeSellerWorklet(kSellerScriptUrl);
  seller_worklet->set_expect_send_pending_signals_requests_called(false);
  interest_group_auction_reporter_.reset();

  interest_group_manager_impl_->ExpectReports({});
  histogram_tester_.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.FinalReporterState",
      InterestGroupAuctionReporter::ReporterState::kSellerReportResult, 1);
}

// Test the case that the InterestGroupAutionReporter is destroyed while calling
// the component seller's reportResult() method, after navigation. This
// primarily serves to test UMA.
TEST_F(InterestGroupAuctionReporterTest,
       DestroyedDuringComponentSellerReportResult) {
  SetUpAndStartComponentAuction();
  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();

  WaitForReportResultAndRunCallback(kSellerScriptUrl, kSellerReportUrl);

  auction_process_manager_.WaitForWinningSellerReload();
  std::unique_ptr<MockSellerWorklet> seller_worklet =
      auction_process_manager_.TakeSellerWorklet(kComponentSellerScriptUrl);
  seller_worklet->set_expect_send_pending_signals_requests_called(false);
  interest_group_auction_reporter_.reset();

  interest_group_manager_impl_->ExpectReports(
      {{InterestGroupManagerImpl::ReportType::kSendReportTo,
        kSellerReportUrl}});
  histogram_tester_.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.FinalReporterState",
      InterestGroupAuctionReporter::ReporterState::kComponentSellerReportResult,
      1);
}

// Test the case that the InterestGroupAutionReporter is destroyed while calling
// the buyer's reportWin() method, after navigation. This primarily serves to
// test UMA.
TEST_F(InterestGroupAuctionReporterTest, DestroyedDuringReportWin) {
  SetUpAndStartComponentAuction();
  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();

  WaitForReportResultAndRunCallback(kSellerScriptUrl, kSellerReportUrl);
  WaitForReportResultAndRunCallback(kComponentSellerScriptUrl,
                                    kComponentSellerReportUrl);

  auction_process_manager_.WaitForWinningBidderReload();
  std::unique_ptr<MockBidderWorklet> bidder_worklet =
      auction_process_manager_.TakeBidderWorklet(kWinningBidderScriptUrl);
  interest_group_auction_reporter_.reset();

  interest_group_manager_impl_->ExpectReports(
      {{InterestGroupManagerImpl::ReportType::kSendReportTo, kSellerReportUrl},
       {InterestGroupManagerImpl::ReportType::kSendReportTo,
        kComponentSellerReportUrl}});
  histogram_tester_.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.FinalReporterState",
      InterestGroupAuctionReporter::ReporterState::kBuyerReportWin, 1);
}

// Test that nothing is recorded and no reports are sent in the case that the
// reporting scripts are successfully run, but the frame is never navigated to.
TEST_F(InterestGroupAuctionReporterTest, NoNavigation) {
  private_aggregation_requests_reserved_[PrivateAggregationKey(
                                             kWinningBidderOrigin,
                                             std::nullopt)]
      .push_back(kWinningBidderGenerateBidPrivateAggregationRequest.Clone());
  private_aggregation_event_map_["event_type"].push_back(
      kWinningBidderGenerateBidPrivateAggregationRequest.Clone());

  auction_worklet::mojom::RealTimeReportingContribution expected_contribution(
      /*bucket=*/100, /*priority_weight=*/0.5,
      /*latency_threshold=*/std::nullopt);
  auction_worklet::mojom::RealTimeReportingContribution
      expected_latency_contribution(
          /*bucket=*/200, /*priority_weight=*/2, /*latency_threshold=*/1);
  real_time_contributions_[kWinningBidderOrigin].push_back(
      expected_contribution.Clone());
  real_time_contributions_[kWinningBidderOrigin].push_back(
      expected_latency_contribution.Clone());

  SetUpAndStartSingleSellerAuction();
  scoped_refptr<FencedFrameReporter> fenced_frame_reporter =

      interest_group_auction_reporter_->fenced_frame_reporter();
  WaitForReportResultAndRunCallback(
      kSellerScriptUrl, kSellerReportUrl, /*ad_beacon_map=*/{},
      MakeRequestPtrVector(kReportResultPrivateAggregationRequest.Clone()));
  WaitForReportWinAndRunCallback(
      kBidderReportUrl, /*ad_beacon_map=*/{}, /*ad_macro_map=*/{},
      MakeRequestPtrVector(
          kReportWinPrivateAggregationRequest.Clone(),
          kReportWinNonReservedPrivateAggregationRequest.Clone()));
  interest_group_auction_reporter_.reset();

  // Have to spin all message loops to flush any k-anon set join events.
  task_environment()->RunUntilIdle();
  EXPECT_THAT(interest_group_manager_impl_->TakeJoinedKAnonSets(),
              testing::UnorderedElementsAre());
  ExpectNoWinsRecorded();
  ExpectNoBidsRecorded();
  EXPECT_THAT(private_aggregation_manager_.TakePrivateAggregationRequests(),
              testing::UnorderedElementsAre());
  EXPECT_TRUE(
      fenced_frame_reporter->GetPrivateAggregationEventMapForTesting().empty());
  interest_group_manager_impl_->ExpectReports({});
  histogram_tester_.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.FinalReporterState",
      InterestGroupAuctionReporter::ReporterState::kAdNotUsed, 1);
}

// Test multiple navigations result in only a single set of reports, and
// metadata being recorded exactly once once by the InterestGroupManager.
TEST_F(InterestGroupAuctionReporterTest, MultipleNavigations) {
  private_aggregation_requests_reserved_[PrivateAggregationKey(
                                             kWinningBidderOrigin,
                                             std::nullopt)]
      .push_back(kWinningBidderGenerateBidPrivateAggregationRequest.Clone());
  private_aggregation_event_map_["event_type"].push_back(
      kWinningBidderGenerateBidPrivateAggregationRequest.Clone());

  real_time_contributions_[kWinningBidderOrigin].push_back(
      kRealTimeReportingContribution.Clone());
  real_time_contributions_[kWinningBidderOrigin].push_back(
      kRealTimeReportingLatencyContribution.Clone());

  SetUpAndStartSingleSellerAuction();
  base::RepeatingClosure callback =
      interest_group_auction_reporter_->OnNavigateToWinningAdCallback(
          FrameTreeNodeId());
  callback.Run();
  callback.Run();

  WaitForReportResultAndRunCallback(
      kSellerScriptUrl, kSellerReportUrl, /*ad_beacon_map=*/{},
      MakeRequestPtrVector(kReportResultPrivateAggregationRequest.Clone()));
  callback.Run();
  callback.Run();

  WaitForReportWinAndRunCallback(
      kBidderReportUrl, /*ad_beacon_map=*/{}, /*ad_macro_map=*/{},
      MakeRequestPtrVector(
          kReportWinPrivateAggregationRequest.Clone(),
          kReportWinNonReservedPrivateAggregationRequest.Clone()));
  callback.Run();
  callback.Run();

  WaitForCompletion();
  // It should be safe to invoke the callback after completion.
  callback.Run();
  callback.Run();

  // Non reserved private aggregation requests should have been passed along
  // only once.
  EXPECT_THAT(
      interest_group_auction_reporter_->fenced_frame_reporter()
          ->GetPrivateAggregationEventMapForTesting(),
      testing::UnorderedElementsAre(testing::Pair(
          "event_type", ElementsAreRequests(
                            kWinningBidderGenerateBidPrivateAggregationRequest,
                            kReportWinPrivateAggregationRequest))));
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
  ExpectBidsRecordedOnce();

  // Private aggregation data should have been passed along only once.
  EXPECT_THAT(
      private_aggregation_manager_.TakePrivateAggregationRequests(),
      testing::UnorderedElementsAre(
          testing::Pair(
              kSellerOrigin,
              ElementsAreRequests(kReportResultPrivateAggregationRequest)),
          testing::Pair(kWinningBidderOrigin,
                        ElementsAreRequests(
                            kWinningBidderGenerateBidPrivateAggregationRequest,
                            kReportWinPrivateAggregationRequest))));

  // Real time reporting data should have been passed along only once.
  EXPECT_THAT(
      interest_group_manager_impl_->TakeRealTimeContributions(),
      testing::UnorderedElementsAre(testing::Pair(
          kWinningBidderOrigin,
          ElementsAreContributions(kRealTimeReportingContribution,
                                   kRealTimeReportingLatencyContribution))));

  // Reports should also have been sent only once.
  interest_group_manager_impl_->ExpectReports(
      {{InterestGroupManagerImpl::ReportType::kSendReportTo, kSellerReportUrl},
       {InterestGroupManagerImpl::ReportType::kSendReportTo,
        kBidderReportUrl}});
}

// Disable feature kPrivateAggregationApi.
class InterestGroupAuctionReporterPrivateAggregationDisabledTest
    : public InterestGroupAuctionReporterTest {
 public:
  InterestGroupAuctionReporterPrivateAggregationDisabledTest() {
    feature_list_.InitAndDisableFeature(
        blink::features::kPrivateAggregationApi);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(InterestGroupAuctionReporterPrivateAggregationDisabledTest,
       PrivateAggregationRequestsNonReserved) {
  // This is possible currently because we're not checking the feature flags
  // when collecting PA requests and sending to InterestGroupAuctionReporter,
  // and a compromised worklet can send PA requests to browser process when
  // feature kPrivateAggregationApi is disabled.
  private_aggregation_event_map_["event_type"].push_back(
      kWinningBidderGenerateBidPrivateAggregationRequest.Clone());
  private_aggregation_event_map_["event_type2"].push_back(
      kBonusPrivateAggregationRequest.Clone());

  SetUpAndStartSingleSellerAuction();

  // Nothing should be sent when the auction is started.
  EXPECT_TRUE(interest_group_auction_reporter_->fenced_frame_reporter()
                  ->GetPrivateAggregationEventMapForTesting()
                  .empty());

  // The requests from the bidding and scoring phase of the auction should not
  // be passed along.
  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();
  EXPECT_TRUE(interest_group_auction_reporter_->fenced_frame_reporter()
                  ->GetPrivateAggregationEventMapForTesting()
                  .empty());

  WaitForReportResultAndRunCallback(kSellerScriptUrl,
                                    /*report_url=*/std::nullopt);

  // The non-reserved aggregation requests from the bidder's reportWin() method
  // should not be passed along neither. reportWin() could only return PA
  // requests if the worklet is compromised when feature kPrivateAggregationApi
  // is disabled.
  WaitForReportWinAndRunCallback(
      /*report_url=*/std::nullopt, /*ad_beacon_map=*/{}, /*ad_macro_map=*/{},
      MakeRequestPtrVector(
          kReportWinNonReservedPrivateAggregationRequest.Clone()));
  EXPECT_TRUE(interest_group_auction_reporter_->fenced_frame_reporter()
                  ->GetPrivateAggregationEventMapForTesting()
                  .empty());

  WaitForCompletion();
}

// Disable FLEDGE-specific extensions of Private Aggregation API.
class InterestGroupAuctionReporterPrivateAggregationFledgeExtensionDisabledTest
    : public InterestGroupAuctionReporterTest {
 public:
  InterestGroupAuctionReporterPrivateAggregationFledgeExtensionDisabledTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kPrivateAggregationApi,
        {{"fledge_extensions_enabled", "false"}});
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(
    InterestGroupAuctionReporterPrivateAggregationFledgeExtensionDisabledTest,
    PrivateAggregationRequestsNonReserved) {
  // This is possible currently because we're not checking the feature flags
  // when collecting PA requests and sending to InterestGroupAuctionReporter,
  // and a compromised worklet can send PA requests to browser process when
  // feature param
  // `blink::features::kPrivateAggregationApiProtectedAudienceExtensionsEnabled`
  // is false.
  private_aggregation_event_map_["event_type"].push_back(
      kWinningBidderGenerateBidPrivateAggregationRequest.Clone());
  private_aggregation_event_map_["event_type2"].push_back(
      kBonusPrivateAggregationRequest.Clone());

  SetUpAndStartSingleSellerAuction();

  // Nothing should be sent when the auction is started.
  EXPECT_TRUE(interest_group_auction_reporter_->fenced_frame_reporter()
                  ->GetPrivateAggregationEventMapForTesting()
                  .empty());

  // The requests from the bidding and scoring phase of the auction should not
  // be passed along.
  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();
  EXPECT_TRUE(interest_group_auction_reporter_->fenced_frame_reporter()
                  ->GetPrivateAggregationEventMapForTesting()
                  .empty());

  WaitForReportResultAndRunCallback(kSellerScriptUrl,
                                    /*report_url=*/std::nullopt);

  // The non-reserved aggregation requests from the bidder's reportWin() method
  // should not be passed along neither. reportWin() could only return PA
  // requests if the worklet is compromised when feature param
  // `blink::features::kPrivateAggregationApiProtectedAudienceExtensionsEnabled`
  // is false.
  WaitForReportWinAndRunCallback(
      /*report_url=*/std::nullopt, /*ad_beacon_map=*/{}, /*ad_macro_map=*/{},
      MakeRequestPtrVector(
          kReportWinNonReservedPrivateAggregationRequest.Clone()));
  EXPECT_TRUE(interest_group_auction_reporter_->fenced_frame_reporter()
                  ->GetPrivateAggregationEventMapForTesting()
                  .empty());

  WaitForCompletion();
}

TEST(InterestGroupAuctionReporterStochasticRounding, MatchesTable) {
  struct {
    double input;
    unsigned k;
    double expected_output;
  } test_cases[] = {
      {0, 8, 0},
      {1, 8, 1},
      {-1, 8, -1},
      // infinity passes through
      {std::numeric_limits<double>::infinity(), 7,
       std::numeric_limits<double>::infinity()},
      {-std::numeric_limits<double>::infinity(), 7,
       -std::numeric_limits<double>::infinity()},
      // not clipped
      {255, 8, 255},
      // positive overflow
      {2e38, 8, std::numeric_limits<double>::infinity()},
      // positive underflow
      {1e-39, 8, 0},
      // negative overflow
      {-2e38, 8, -std::numeric_limits<double>::infinity()},
      // negative underflow
      {-1e-39, 8, -0},
  };

  for (const auto& test_case : test_cases) {
    EXPECT_EQ(test_case.expected_output,
              InterestGroupAuctionReporter::RoundStochasticallyToKBits(
                  test_case.input, test_case.k))
        << "with " << test_case.input << " and " << test_case.k;
  }
}

TEST(InterestGroupAuctionReporterStochasticRounding, PassesNaN) {
  EXPECT_TRUE(
      std::isnan(InterestGroupAuctionReporter::RoundStochasticallyToKBits(
          std::numeric_limits<double>::quiet_NaN(), 8)));
  EXPECT_TRUE(
      std::isnan(InterestGroupAuctionReporter::RoundStochasticallyToKBits(
          std::numeric_limits<double>::signaling_NaN(), 8)));
}

TEST(InterestGroupAuctionReporterStochasticRounding, IsNonDeterministic) {
  // Since 0.3 can't be represented with 8 bits of precision, this value will be
  // clipped to either the nearest lower number or nearest higher number.
  const double kInput = 0.3;
  base::flat_set<double> seen;
  while (seen.size() < 2) {
    double result =
        InterestGroupAuctionReporter::RoundStochasticallyToKBits(kInput, 8);
    EXPECT_THAT(result, testing::AnyOf(0.298828125, 0.30078125));
    seen.insert(result);
  }
}

TEST(InterestGroupAuctionReporterStochasticRounding, RoundsUpAndDown) {
  const double inputs[] = {129.3, 129.8};
  for (auto input : inputs) {
    SCOPED_TRACE(input);
    base::flat_set<double> seen;
    while (seen.size() < 2) {
      double result =
          InterestGroupAuctionReporter::RoundStochasticallyToKBits(input, 8);
      ASSERT_THAT(result, testing::AnyOf(129.0, 130.0));
      seen.insert(result);
    }
  }
}

TEST(InterestGroupAuctionReporterStochasticRounding, HandlesOverflow) {
  double max_value =
      std::ldexp(0.998046875, std::numeric_limits<int8_t>::max());
  double expected_value =
      std::ldexp(0.99609375, std::numeric_limits<int8_t>::max());
  const double inputs[] = {
      max_value,
      -max_value,
  };
  for (auto input : inputs) {
    SCOPED_TRACE(input);
    base::flat_set<double> seen;
    while (seen.size() < 2) {
      double result =
          InterestGroupAuctionReporter::RoundStochasticallyToKBits(input, 8);
      ASSERT_THAT(
          result,
          testing::AnyOf(
              std::copysign(expected_value, input),
              std::copysign(std::numeric_limits<double>::infinity(), input)));
      seen.insert(result);
    }
  }
}

// Test that random rounding allows mean to approximate the true value.
TEST(InterestGroupAuctionReporterStochasticRounding, ApproximatesTrueSum) {
  // Since 0.3 can't be represented with 8 bits of precision, this value will be
  // clipped randomly. Because 0.3 is 60% of the way from the nearest
  // representable number smaller than it and 40% of the way to the nearest
  // representable number larger than it, the value should be rounded down to
  // 0.2988... 60% of the time and rounded up to 0.30078... 40% of the time.
  // This ensures that if you add the result N times you roughly get 0.3 * N.
  const size_t kIterations = 10000;
  const double kInput = 0.3;
  double total = 0;

  for (size_t idx = 0; idx < kIterations; idx++) {
    total +=
        InterestGroupAuctionReporter::RoundStochasticallyToKBits(kInput, 8);
  }

  EXPECT_GT(total, 0.9 * kInput * kIterations);
  EXPECT_LT(total, 1.1 * kInput * kIterations);
}

class InterestGroupAuctionReporterAdMacroReportingEnabledTest
    : public InterestGroupAuctionReporterTest {
 public:
  InterestGroupAuctionReporterAdMacroReportingEnabledTest() {
    feature_list_.InitAndEnableFeature(
        blink::features::kAdAuctionReportingWithMacroApi);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(InterestGroupAuctionReporterAdMacroReportingEnabledTest,
       SingleSellerReportMacros) {
  SetUpAndStartSingleSellerAuction();
  // The macros should be empty from the start.
  EXPECT_THAT(interest_group_auction_reporter_->fenced_frame_reporter()
                  ->GetAdMacrosForTesting(),
              testing::UnorderedElementsAre());

  WaitForReportResultAndRunCallback(kSellerScriptUrl,
                                    /*report_url=*/std::nullopt);

  WaitForReportWinAndRunCallback(/*report_url=*/std::nullopt,
                                 /*ad_beacon_map=*/{}, kAdMacroMap);
  EXPECT_THAT(interest_group_auction_reporter_->fenced_frame_reporter()
                  ->GetAdMacrosForTesting(),
              testing::UnorderedElementsAre(testing::Pair(
                  blink::FencedFrame::ReportingDestination::kBuyer,
                  testing::UnorderedElementsAreArray(kAdMacros))));

  // Invoking the callback has no effect on macro maps. Fenced frames navigated
  // to the winning ad use them to trigger reports, so no need to hold them back
  // until a fenced frame is navigated to the winning ad.
  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();

  WaitForCompletion();
  EXPECT_THAT(interest_group_auction_reporter_->fenced_frame_reporter()
                  ->GetAdMacrosForTesting(),
              testing::UnorderedElementsAre(testing::Pair(
                  blink::FencedFrame::ReportingDestination::kBuyer,
                  testing::UnorderedElementsAreArray(kAdMacros))));
}

TEST_F(InterestGroupAuctionReporterAdMacroReportingEnabledTest,
       ComponentAuctionReportMacros) {
  SetUpAndStartComponentAuction();
  EXPECT_THAT(interest_group_auction_reporter_->fenced_frame_reporter()
                  ->GetAdMacrosForTesting(),
              testing::UnorderedElementsAre());

  WaitForReportResultAndRunCallback(kSellerScriptUrl,
                                    /*report_url=*/std::nullopt);

  WaitForReportResultAndRunCallback(kComponentSellerScriptUrl,
                                    /*report_url=*/std::nullopt);

  WaitForReportWinAndRunCallback(/*report_url=*/std::nullopt,
                                 /*ad_beacon_map=*/{}, kAdMacroMap);
  EXPECT_THAT(interest_group_auction_reporter_->fenced_frame_reporter()
                  ->GetAdMacrosForTesting(),
              testing::UnorderedElementsAre(testing::Pair(
                  blink::FencedFrame::ReportingDestination::kBuyer,
                  testing::UnorderedElementsAreArray(kAdMacros))));

  // Invoking the callback has no effect on per-destination reporting maps.
  // Fenced frames navigated to the winning ad use them to trigger reports, so
  // no need to hold them back until a fenced frame is navigated to the winning
  // ad.
  interest_group_auction_reporter_
      ->OnNavigateToWinningAdCallback(FrameTreeNodeId())
      .Run();

  WaitForCompletion();
  EXPECT_THAT(interest_group_auction_reporter_->fenced_frame_reporter()
                  ->GetAdMacrosForTesting(),
              testing::UnorderedElementsAre(testing::Pair(
                  blink::FencedFrame::ReportingDestination::kBuyer,
                  testing::UnorderedElementsAreArray(kAdMacros))));
}

}  // namespace
}  // namespace content
