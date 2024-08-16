// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/ad_auction_headers_util.h"

#include <functional>
#include <string>
#include <string_view>

#include "base/base64url.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/interest_group/ad_auction_page_data.h"
#include "content/browser/interest_group/header_direct_from_seller_signals.h"
#include "content/browser/interest_group/interest_group_features.h"
#include "content/browser/loader/subresource_proxying_url_loader_service.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_render_frame_host.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_version.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

constexpr char kLegitimateAdAuctionResponse[] =
    "ungWv48Bz-pBQUDeXa4iI7ADYaOWF3qctBD_YfIAFa0=";
constexpr char kLegitimateAdAuctionSignals[] =
    R"([{"adSlot":"slot1", "sellerSignals":{"signal1":"value1"}}])";

std::string Base64UrlDecode(std::string_view input) {
  std::string bytes;
  CHECK(base::Base64UrlDecode(
      input, base::Base64UrlDecodePolicy::IGNORE_PADDING, &bytes));
  return bytes;
}

class InterceptingContentBrowserClient : public ContentBrowserClient {
 public:
  bool IsInterestGroupAPIAllowed(RenderFrameHost* render_frame_host,
                                 InterestGroupApiOperation operation,
                                 const url::Origin& top_frame_origin,
                                 const url::Origin& api_origin) override {
    return interest_group_allowed_by_settings_;
  }

  void set_interest_group_allowed_by_settings(bool allowed) {
    interest_group_allowed_by_settings_ = allowed;
  }

 private:
  bool interest_group_allowed_by_settings_ = false;
};

blink::ParsedPermissionsPolicy CreatePermissivePolicy() {
  blink::ParsedPermissionsPolicy policy;
  policy.emplace_back(
      blink::mojom::PermissionsPolicyFeature::kRunAdAuction,
      /*allowed_origins=*/
      std::vector{*blink::OriginWithPossibleWildcards::FromOrigin(
                      url::Origin::Create(GURL("https://google.com"))),
                  *blink::OriginWithPossibleWildcards::FromOrigin(
                      url::Origin::Create(GURL("https://foo1.com")))},
      /*self_if_matches=*/std::nullopt,
      /*matches_all_origins=*/false,
      /*matches_opaque_src=*/false);
  return policy;
}

blink::ParsedPermissionsPolicy CreateRestrictivePolicy() {
  blink::ParsedPermissionsPolicy policy;
  policy.emplace_back(
      blink::mojom::PermissionsPolicyFeature::kRunAdAuction,
      /*allowed_origins=*/std::vector<blink::OriginWithPossibleWildcards>(),
      /*self_if_matches=*/std::nullopt,
      /*matches_all_origins=*/false,
      /*matches_opaque_src=*/false);
  return policy;
}

}  // namespace

class IsAdAuctionHeadersEligibleTest
    : public RenderViewHostTestHarness,
      public testing::WithParamInterface<bool> {
 public:
  IsAdAuctionHeadersEligibleTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kInterestGroupStorage},
        /*disabled_features=*/{});
  }

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    original_client_ = SetBrowserClientForTesting(&browser_client_);
    browser_client_.set_interest_group_allowed_by_settings(true);
  }

  void TearDown() override {
    SetBrowserClientForTesting(original_client_);

    RenderViewHostTestHarness::TearDown();
  }

  RenderFrameHostImpl& NavigatePage(const GURL& url) {
    auto main_frame_navigation =
        NavigationSimulator::CreateBrowserInitiated(url, web_contents());
    main_frame_navigation->SetPermissionsPolicyHeader(CreatePermissivePolicy());
    main_frame_navigation->Commit();

    if (GetParam()) {
      return static_cast<RenderFrameHostImpl&>(
          *main_frame_navigation->GetFinalRenderFrameHost());
    }

    TestRenderFrameHost* subframe = static_cast<TestRenderFrameHost*>(
        RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
            ->AppendChild("child0"));

    auto subframe_navigation =
        NavigationSimulator::CreateRendererInitiated(url, subframe);
    subframe_navigation->SetPermissionsPolicyHeader(CreatePermissivePolicy());
    subframe_navigation->Commit();

    return static_cast<RenderFrameHostImpl&>(
        *subframe_navigation->GetFinalRenderFrameHost());
  }

  network::ResourceRequest CreateResourceRequest(const GURL& url) {
    network::ResourceRequest resource_request;
    resource_request.url = url;
    resource_request.ad_auction_headers = true;
    return resource_request;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

  InterceptingContentBrowserClient browser_client_;
  raw_ptr<ContentBrowserClient> original_client_ = nullptr;
};

TEST_P(IsAdAuctionHeadersEligibleTest, RequestIsEligible) {
  GURL test_url("https://google.com");
  RenderFrameHostImpl& render_frame_host = NavigatePage(test_url);
  EXPECT_TRUE(IsAdAuctionHeadersEligible(render_frame_host,
                                         CreateResourceRequest(test_url)));
}

TEST_P(IsAdAuctionHeadersEligibleTest, NotEligibleDueToUntrustworthyOrigin) {
  GURL test_url("http://google.com");
  RenderFrameHostImpl& render_frame_host = NavigatePage(test_url);
  EXPECT_FALSE(IsAdAuctionHeadersEligible(render_frame_host,
                                          CreateResourceRequest(test_url)));
}

TEST_P(IsAdAuctionHeadersEligibleTest, NotEligibleDueToOpaqueOrigin) {
  GURL test_url("https://google.com");
  RenderFrameHostImpl& render_frame_host = NavigatePage(test_url);
  network::ResourceRequest resource_request =
      CreateResourceRequest(GURL("data:image/jpeg;base64,UklGRjwCAAA="));
  EXPECT_FALSE(IsAdAuctionHeadersEligible(render_frame_host, resource_request));
}

TEST_P(IsAdAuctionHeadersEligibleTest, NotEligibleDueToInactiveFrame) {
  GURL test_url("https://google.com");
  RenderFrameHostImpl& render_frame_host = NavigatePage(test_url);
  render_frame_host.GetMainFrame()->SetLifecycleState(
      RenderFrameHostImpl::LifecycleStateImpl::kReadyToBeDeleted);
  EXPECT_FALSE(IsAdAuctionHeadersEligible(render_frame_host,
                                          CreateResourceRequest(test_url)));
}

TEST_P(IsAdAuctionHeadersEligibleTest, NotEligibleDueToPermissionsPolicy) {
  GURL test_url("https://google.com");
  RenderFrameHostImpl& render_frame_host = NavigatePage(test_url);

  // The permissions policy disallows `foo2.com`. The request won't be eligible
  // for ad auction headers.
  GURL unpermitted_url("https://foo2.com");
  EXPECT_FALSE(IsAdAuctionHeadersEligible(
      render_frame_host, CreateResourceRequest(unpermitted_url)));
}

TEST_P(IsAdAuctionHeadersEligibleTest,
       NotEligibleDueToInterestGroupAPINotAllowed) {
  browser_client_.set_interest_group_allowed_by_settings(false);

  GURL test_url("https://google.com");
  RenderFrameHostImpl& render_frame_host = NavigatePage(test_url);
  EXPECT_FALSE(IsAdAuctionHeadersEligible(render_frame_host,
                                          CreateResourceRequest(test_url)));
}

INSTANTIATE_TEST_SUITE_P(HasOptionalResourceRequest,
                         IsAdAuctionHeadersEligibleTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "RequestFromMainFrame"
                                             : "RequestFromIFrame";
                         });

class IsAdAuctionHeadersEligibleForNavigationTest
    : public RenderViewHostTestHarness {
 public:
  IsAdAuctionHeadersEligibleForNavigationTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kInterestGroupStorage},
        /*disabled_features=*/{});
  }

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    original_client_ = SetBrowserClientForTesting(&browser_client_);
    browser_client_.set_interest_group_allowed_by_settings(true);
  }

  void TearDown() override {
    SetBrowserClientForTesting(original_client_);

    RenderViewHostTestHarness::TearDown();
  }

  const FrameTreeNode& NavigatePage(const GURL& url) {
    auto main_frame_navigation =
        NavigationSimulator::CreateBrowserInitiated(url, web_contents());
    main_frame_navigation->SetPermissionsPolicyHeader(CreatePermissivePolicy());
    main_frame_navigation->Commit();

    TestRenderFrameHost* subframe = static_cast<TestRenderFrameHost*>(
        RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
            ->AppendChild("child0"));

    auto subframe_navigation =
        NavigationSimulator::CreateRendererInitiated(url, subframe);

    // IFrame navigations should use its parent frame's permission policy, and
    // so this iframe's permission policy should be restrictive.
    subframe_navigation->SetPermissionsPolicyHeader(CreateRestrictivePolicy());
    subframe_navigation->Commit();

    return *static_cast<RenderFrameHostImpl*>(
                subframe_navigation->GetFinalRenderFrameHost())
                ->frame_tree_node();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

  InterceptingContentBrowserClient browser_client_;
  raw_ptr<ContentBrowserClient> original_client_ = nullptr;
};

TEST_F(IsAdAuctionHeadersEligibleForNavigationTest, RequestIsEligible) {
  GURL test_url("https://google.com");
  const FrameTreeNode& frame = NavigatePage(test_url);
  EXPECT_TRUE(IsAdAuctionHeadersEligibleForNavigation(
      frame, url::Origin::Create(test_url)));
}

TEST_F(IsAdAuctionHeadersEligibleForNavigationTest, DisabledByFeature) {
  base::test::ScopedFeatureList disable_feature;
  disable_feature.InitAndDisableFeature(
      features::kEnableIFrameAdAuctionHeaders);

  GURL test_url("https://google.com");
  const FrameTreeNode& frame = NavigatePage(test_url);
  EXPECT_FALSE(IsAdAuctionHeadersEligibleForNavigation(
      frame, url::Origin::Create(test_url)));
}

TEST_F(IsAdAuctionHeadersEligibleForNavigationTest,
       NotEligibleDueToUntrustworthyOrigin) {
  GURL test_url("http://google.com");
  const FrameTreeNode& frame = NavigatePage(test_url);
  EXPECT_FALSE(IsAdAuctionHeadersEligibleForNavigation(
      frame, url::Origin::Create(test_url)));
}

TEST_F(IsAdAuctionHeadersEligibleForNavigationTest,
       NotEligibleDueToNonChildFrameProvided) {
  GURL test_url("https://google.com");
  auto main_frame_navigation =
      NavigationSimulator::CreateBrowserInitiated(test_url, web_contents());
  main_frame_navigation->SetPermissionsPolicyHeader(CreatePermissivePolicy());
  main_frame_navigation->Commit();
  const FrameTreeNode& frame =
      *static_cast<RenderFrameHostImpl*>(
           main_frame_navigation->GetFinalRenderFrameHost())
           ->frame_tree_node();
  EXPECT_FALSE(IsAdAuctionHeadersEligibleForNavigation(
      frame, url::Origin::Create(test_url)));
}

TEST_F(IsAdAuctionHeadersEligibleForNavigationTest,
       NotEligibleDueToOpaqueOrigin) {
  GURL test_url("https://google.com");
  const FrameTreeNode& frame = NavigatePage(test_url);
  EXPECT_FALSE(IsAdAuctionHeadersEligibleForNavigation(frame, url::Origin()));
}

TEST_F(IsAdAuctionHeadersEligibleForNavigationTest,
       NotEligibleDueToInactiveFrame) {
  GURL test_url("https://google.com");
  const FrameTreeNode& frame = NavigatePage(test_url);
  frame.GetParentOrOuterDocument()->SetLifecycleState(
      RenderFrameHostImpl::LifecycleStateImpl::kReadyToBeDeleted);
  EXPECT_FALSE(IsAdAuctionHeadersEligibleForNavigation(
      frame, url::Origin::Create(test_url)));
}

TEST_F(IsAdAuctionHeadersEligibleForNavigationTest,
       NotEligibleDueToPermissionsPolicy) {
  GURL test_url("https://google.com");
  const FrameTreeNode& frame = NavigatePage(test_url);

  // The permissions policy disallows `foo2.com`. The request won't be eligible
  // for ad auction headers.
  GURL unpermitted_url("https://foo2.com");
  EXPECT_FALSE(IsAdAuctionHeadersEligibleForNavigation(
      frame, url::Origin::Create(unpermitted_url)));
}

TEST_F(IsAdAuctionHeadersEligibleForNavigationTest,
       NotEligibleDueToInterestGroupAPINotAllowed) {
  browser_client_.set_interest_group_allowed_by_settings(false);

  GURL test_url("https://google.com");
  const FrameTreeNode& frame = NavigatePage(test_url);
  EXPECT_FALSE(IsAdAuctionHeadersEligibleForNavigation(
      frame, url::Origin::Create(test_url)));
}

class ProcessAdAuctionResponseHeadersTest : public RenderViewHostTestHarness {
 public:
  ProcessAdAuctionResponseHeadersTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kFledgeBiddingAndAuctionServer,
                              blink::features::kAdAuctionSignals},
        /*disabled_features=*/{});
  }

  bool WitnessedAuctionResultForOrigin(const url::Origin& origin,
                                       const std::string& response) {
    Page& page = web_contents()->GetPrimaryPage();

    AdAuctionPageData* ad_auction_page_data =
        PageUserData<AdAuctionPageData>::GetOrCreateForPage(page);

    return ad_auction_page_data->WitnessedAuctionResultForOrigin(origin,
                                                                 response);
  }

  const scoped_refptr<HeaderDirectFromSellerSignals::Result>
  ParseAndFindAdAuctionSignals(const url::Origin& origin,
                               const std::string& ad_slot) {
    Page& page = web_contents()->GetPrimaryPage();

    AdAuctionPageData* ad_auction_page_data =
        PageUserData<AdAuctionPageData>::GetOrCreateForPage(page);

    scoped_refptr<HeaderDirectFromSellerSignals::Result> my_result;
    base::RunLoop run_loop;
    ad_auction_page_data->ParseAndFindAdAuctionSignals(
        origin, ad_slot,
        base::BindLambdaForTesting(
            [&my_result, &run_loop](
                scoped_refptr<HeaderDirectFromSellerSignals::Result> result) {
              my_result = std::move(result);
              run_loop.Quit();
            }));
    run_loop.Run();

    return my_result;
  }

  std::vector<std::string> TakeAuctionAdditionalBidsForOriginAndNonce(
      const url::Origin& origin,
      const std::string& nonce) {
    Page& page = web_contents()->GetPrimaryPage();

    AdAuctionPageData* ad_auction_page_data =
        PageUserData<AdAuctionPageData>::GetOrCreateForPage(page);

    return ad_auction_page_data->TakeAuctionAdditionalBidsForOriginAndNonce(
        origin, nonce);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ProcessAdAuctionResponseHeadersTest,
       MultipleAdAuctionResults_AllWitnessed) {
  const char kLegitimateAdAuctionResponse2[] =
      "8oX0szl-BNWitSuE3ZK5Npt05t83A1wrl94oBtlZHFs=";
  const char kLegitimateAdAuctionResponse3[] =
      "lIcI37kQp_ArBk_1JfdEjyQ0suSLUpYDIKO906THBdk=";

  net::HttpResponseHeaders::Builder headers_builder({1, 1}, "200 OK");
  headers_builder.AddHeader(kAdAuctionResultResponseHeaderKey,
                            kLegitimateAdAuctionResponse);
  std::string concatenated_header =
      base::StrCat({kLegitimateAdAuctionResponse2, ",", "invalid", ",",
                    kLegitimateAdAuctionResponse3});
  headers_builder.AddHeader(kAdAuctionResultResponseHeaderKey,
                            concatenated_header);
  headers_builder.AddHeader(kAdAuctionResultResponseHeaderKey, "alsoInvalid");
  scoped_refptr<net::HttpResponseHeaders> headers = headers_builder.Build();

  // Unlike the signals and additional bid headers, the result header is not
  // removed when it's stored in the browser.
  EXPECT_TRUE(headers->HasHeader(kAdAuctionResultResponseHeaderKey));

  ProcessAdAuctionResponseHeaders(url::Origin::Create(GURL("https://foo1.com")),
                                  web_contents()->GetPrimaryPage(), headers);

  EXPECT_TRUE(WitnessedAuctionResultForOrigin(
      url::Origin::Create(GURL("https://foo1.com")),
      Base64UrlDecode(kLegitimateAdAuctionResponse)));

  EXPECT_TRUE(WitnessedAuctionResultForOrigin(
      url::Origin::Create(GURL("https://foo1.com")),
      Base64UrlDecode(kLegitimateAdAuctionResponse2)));

  EXPECT_TRUE(WitnessedAuctionResultForOrigin(
      url::Origin::Create(GURL("https://foo1.com")),
      Base64UrlDecode(kLegitimateAdAuctionResponse3)));
}

TEST_F(ProcessAdAuctionResponseHeadersTest,
       InvalidAdAuctionResultResponseHeader) {
  net::HttpResponseHeaders::Builder headers_builder({1, 1}, "200 OK");
  headers_builder.AddHeader(kAdAuctionResultResponseHeaderKey,
                            "invalid-response-header");
  scoped_refptr<net::HttpResponseHeaders> headers = headers_builder.Build();

  // Unlike the signals and additional bid headers, the result header is not
  // removed when it's stored in the browser.
  EXPECT_TRUE(headers->HasHeader(kAdAuctionResultResponseHeaderKey));

  ProcessAdAuctionResponseHeaders(url::Origin::Create(GURL("https://foo1.com")),
                                  web_contents()->GetPrimaryPage(), headers);

  EXPECT_FALSE(WitnessedAuctionResultForOrigin(
      url::Origin::Create(GURL("https://foo1.com")),
      "invalid-response-header"));
}

TEST_F(ProcessAdAuctionResponseHeadersTest, AdAuctionSignalsResponseHeader) {
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;

  net::HttpResponseHeaders::Builder headers_builder({1, 1}, "200 OK");
  headers_builder.AddHeader(kAdAuctionSignalsResponseHeaderKey,
                            kLegitimateAdAuctionSignals);
  scoped_refptr<net::HttpResponseHeaders> headers = headers_builder.Build();

  ProcessAdAuctionResponseHeaders(url::Origin::Create(GURL("https://foo1.com")),
                                  web_contents()->GetPrimaryPage(), headers);

  // The `Ad-Auction-Signals` header was removed from the headers and
  // stored in the browser.
  EXPECT_FALSE(headers->HasHeader(kAdAuctionSignalsResponseHeaderKey));

  const scoped_refptr<HeaderDirectFromSellerSignals::Result> signals =
      ParseAndFindAdAuctionSignals(
          url::Origin::Create(GURL("https://foo1.com")), "slot1");
  EXPECT_EQ(*signals->seller_signals(), R"({"signal1":"value1"})");
}

TEST_F(ProcessAdAuctionResponseHeadersTest,
       AdAuctionSignalsResponseHeaderTooLong) {
  net::HttpResponseHeaders::Builder headers_builder({1, 1}, "200 OK");

  // This test value for the `Ad-Auction-Signals` header doesn't have to be
  // valid JSON, since it doesn't get as far as JSON parsing.
  // `ProcessAdAuctionResponseHeaders` discards this excessively long header
  // before involving `AdAuctionPageData`.
  std::string very_long_header_value(10001, '0');
  headers_builder.AddHeader(kAdAuctionSignalsResponseHeaderKey,
                            very_long_header_value);
  scoped_refptr<net::HttpResponseHeaders> headers = headers_builder.Build();

  ProcessAdAuctionResponseHeaders(url::Origin::Create(GURL("https://foo1.com")),
                                  web_contents()->GetPrimaryPage(), headers);

  // The `Ad-Auction-Signals` header was removed from the headers, even though
  // it wasn't stored in the browser.
  EXPECT_FALSE(headers->HasHeader(kAdAuctionSignalsResponseHeaderKey));

  const scoped_refptr<HeaderDirectFromSellerSignals::Result> signals =
      ParseAndFindAdAuctionSignals(
          url::Origin::Create(GURL("https://foo1.com")), "slot1");
  EXPECT_EQ(signals, nullptr);
}

TEST_F(ProcessAdAuctionResponseHeadersTest, AdditionalBid) {
  net::HttpResponseHeaders::Builder headers_builder({1, 1}, "200 OK");
  headers_builder.AddHeader(kAdAuctionAdditionalBidResponseHeaderKey,
                            "00000000-0000-0000-0000-000000000000:e30=");
  scoped_refptr<net::HttpResponseHeaders> headers = headers_builder.Build();

  ProcessAdAuctionResponseHeaders(url::Origin::Create(GURL("https://foo1.com")),
                                  web_contents()->GetPrimaryPage(), headers);

  // The `Ad-Auction-Additional-Bid` header was removed from the headers and
  // stored in the browser.
  EXPECT_FALSE(headers->HasHeader(kAdAuctionAdditionalBidResponseHeaderKey));

  url::Origin request_origin = url::Origin::Create(GURL("https://foo1.com"));
  EXPECT_THAT(TakeAuctionAdditionalBidsForOriginAndNonce(
                  request_origin, "00000000-0000-0000-0000-000000000000"),
              ::testing::ElementsAre("e30="));

  // Future calls to `TakeAuctionAdditionalBidsForOriginAndNonce` on the same
  // origin and nonce should return nothing. Ideally this should be tested
  // separately as a unitest for `AdAuctionPageData`.
  EXPECT_THAT(TakeAuctionAdditionalBidsForOriginAndNonce(
                  request_origin, "00000000-0000-0000-0000-000000000000"),
              ::testing::IsEmpty());
}

TEST_F(ProcessAdAuctionResponseHeadersTest,
       AdditionalBid_MultipleNoncesAndMultipleBidsPerNonce) {
  net::HttpResponseHeaders::Builder headers_builder({1, 1}, "200 OK");
  headers_builder.AddHeader(kAdAuctionAdditionalBidResponseHeaderKey,
                            "00000000-0000-0000-0000-000000000000:e30=,"
                            "00000000-0000-0000-0000-000000000001:e30=");
  headers_builder.AddHeader(kAdAuctionAdditionalBidResponseHeaderKey,
                            "00000000-0000-0000-0000-000000000001:e2E6IDF9");
  scoped_refptr<net::HttpResponseHeaders> headers = headers_builder.Build();

  ProcessAdAuctionResponseHeaders(url::Origin::Create(GURL("https://foo1.com")),
                                  web_contents()->GetPrimaryPage(), headers);

  // The `Ad-Auction-Additional-Bid` header was removed from the headers and
  // stored in the browser.
  EXPECT_FALSE(headers->HasHeader(kAdAuctionAdditionalBidResponseHeaderKey));

  url::Origin request_origin = url::Origin::Create(GURL("https://foo1.com"));
  EXPECT_THAT(TakeAuctionAdditionalBidsForOriginAndNonce(
                  request_origin, "00000000-0000-0000-0000-000000000000"),
              ::testing::ElementsAre("e30="));
  EXPECT_THAT(TakeAuctionAdditionalBidsForOriginAndNonce(
                  request_origin, "00000000-0000-0000-0000-000000000001"),
              ::testing::ElementsAre("e30=", "e2E6IDF9"));

  // Future calls to `TakeAuctionAdditionalBidsForOriginAndNonce` on the same
  // origin and nonce should return nothing. Ideally this should be tested
  // separately as a unitest for `AdAuctionPageData`.
  EXPECT_THAT(TakeAuctionAdditionalBidsForOriginAndNonce(
                  request_origin, "00000000-0000-0000-0000-000000000000"),
              ::testing::IsEmpty());
  EXPECT_THAT(TakeAuctionAdditionalBidsForOriginAndNonce(
                  request_origin, "00000000-0000-0000-0000-000000000001"),
              ::testing::IsEmpty());
}

TEST_F(ProcessAdAuctionResponseHeadersTest,
       AdditionalBid_InvalidHeaderSkipped) {
  net::HttpResponseHeaders::Builder headers_builder({1, 1}, "200 OK");

  // Entries with invalid nonce (i.e. doesn't have 36 characters) will be
  // skipped.
  headers_builder.AddHeader(kAdAuctionAdditionalBidResponseHeaderKey,
                            "00000000-0000-0000-0000-00000000000:e30=");
  headers_builder.AddHeader(kAdAuctionAdditionalBidResponseHeaderKey,
                            "00000000-0000-0000-0000-0000000000001:e30=,"
                            "00000000-0000-0000-0000-000000000001:e2E6IDF9");
  scoped_refptr<net::HttpResponseHeaders> headers = headers_builder.Build();

  ProcessAdAuctionResponseHeaders(url::Origin::Create(GURL("https://foo1.com")),
                                  web_contents()->GetPrimaryPage(), headers);

  // The `Ad-Auction-Additional-Bid` header was removed from the headers and
  // stored in the browser.
  EXPECT_FALSE(headers->HasHeader(kAdAuctionAdditionalBidResponseHeaderKey));

  url::Origin request_origin = url::Origin::Create(GURL("https://foo1.com"));
  EXPECT_THAT(TakeAuctionAdditionalBidsForOriginAndNonce(
                  request_origin, "00000000-0000-0000-0000-00000000000"),
              ::testing::IsEmpty());
  EXPECT_THAT(TakeAuctionAdditionalBidsForOriginAndNonce(
                  request_origin, "00000000-0000-0000-0000-0000000000001"),
              ::testing::IsEmpty());
  EXPECT_THAT(TakeAuctionAdditionalBidsForOriginAndNonce(
                  request_origin, "00000000-0000-0000-0000-000000000001"),
              ::testing::ElementsAre("e2E6IDF9"));
}

TEST(RemoveAdAuctionResponseHeadersTest,
     SignalsAndAdditionalBidHeadersAreRemoved) {
  net::HttpResponseHeaders::Builder headers_builder({1, 1}, "200 OK");
  headers_builder.AddHeader(kAdAuctionResultResponseHeaderKey,
                            kLegitimateAdAuctionResponse);
  headers_builder.AddHeader(kAdAuctionSignalsResponseHeaderKey,
                            R"([{"adSlot":"slot1"}])");
  headers_builder.AddHeader(kAdAuctionAdditionalBidResponseHeaderKey,
                            "00000000-0000-0000-0000-000000000000:e30=");
  scoped_refptr<net::HttpResponseHeaders> headers = headers_builder.Build();

  RemoveAdAuctionResponseHeaders(headers);

  // Only the signals and additional bid headers are removed.
  EXPECT_TRUE(headers->HasHeader(kAdAuctionResultResponseHeaderKey));
  EXPECT_FALSE(headers->HasHeader(kAdAuctionSignalsResponseHeaderKey));
  EXPECT_FALSE(headers->HasHeader(kAdAuctionAdditionalBidResponseHeaderKey));
}

}  // namespace content
