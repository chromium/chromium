// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/ad_auction_url_loader_interceptor.h"

#include <string_view>

#include "base/base64url.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/interest_group/ad_auction_page_data.h"
#include "content/browser/interest_group/header_direct_from_seller_signals.h"
#include "content/browser/loader/subresource_proxying_url_loader_service.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "mojo/public/cpp/system/functions.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/parsed_headers.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

namespace {

constexpr char kLegitimateAdAuctionResponse[] =
    "ungWv48Bz-pBQUDeXa4iI7ADYaOWF3qctBD_YfIAFa0=";
constexpr char kLegitimateAdAuctionSignals[] =
    R"([{"adSlot":"slot1", "sellerSignals":{"signal1":"value1"}}])";

using FollowRedirectParams =
    network::TestURLLoaderFactory::TestURLLoader::FollowRedirectParams;

std::string base64Decode(std::string_view input) {
  std::string bytes;
  CHECK(base::Base64UrlDecode(
      input, base::Base64UrlDecodePolicy::IGNORE_PADDING, &bytes));
  return bytes;
}

class InterceptingContentBrowserClient : public ContentBrowserClient {
 public:
  bool IsInterestGroupAPIAllowed(content::RenderFrameHost* render_frame_host,
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

class TestURLLoaderClient : public network::mojom::URLLoaderClient {
 public:
  TestURLLoaderClient() = default;
  ~TestURLLoaderClient() override = default;

  TestURLLoaderClient(const TestURLLoaderClient&) = delete;
  TestURLLoaderClient& operator=(const TestURLLoaderClient&) = delete;

  mojo::PendingRemote<network::mojom::URLLoaderClient>
  BindURLLoaderClientAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override {
    if (head->headers.get()->HasHeader("Ad-Auction-Signals")) {
      received_ad_auction_signals_header_ = true;
    }

    if (head->headers.get()->HasHeader("Ad-Auction-Result")) {
      received_ad_auction_result_header_ = true;
    }

    if (head->headers.get()->HasHeader("Ad-Auction-Additional-Bid")) {
      received_ad_auction_additional_bid_header_ = true;
    }

    received_response_ = true;
  }

  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override {
    if (head->headers.get()->HasHeader("Ad-Auction-Signals")) {
      received_ad_auction_signals_header_ = true;
    }

    if (head->headers.get()->HasHeader("Ad-Auction-Result")) {
      received_ad_auction_result_header_ = true;
    }

    if (head->headers.get()->HasHeader("Ad-Auction-Additional-Bid")) {
      received_ad_auction_additional_bid_header_ = true;
    }
  }

  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override {
  }
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        base::OnceCallback<void()> callback) override {}
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {}
  void OnComplete(const network::URLLoaderCompletionStatus& status) override {}

  bool received_response() const { return received_response_; }

  bool received_ad_auction_signals_header() const {
    return received_ad_auction_signals_header_;
  }

  bool received_ad_auction_result_header() const {
    return received_ad_auction_result_header_;
  }

  bool received_ad_auction_additional_bid_header() const {
    return received_ad_auction_additional_bid_header_;
  }

 private:
  bool received_response_ = false;
  bool received_ad_auction_result_header_ = false;
  bool received_ad_auction_signals_header_ = false;
  bool received_ad_auction_additional_bid_header_ = false;

  mojo::Receiver<network::mojom::URLLoaderClient> receiver_{this};
};

}  // namespace

class AdAuctionURLLoaderInterceptorTest : public RenderViewHostTestHarness {
 public:
  AdAuctionURLLoaderInterceptorTest() {
    // The `kBrowsingTopics` feature is needed to create a request eligible to
    // be handled via `SubresourceProxyingURLLoaderService` (i.e. sets
    // browsing_topics but not ad_auction_headers).
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kInterestGroupStorage,
                              blink::features::kFledgeBiddingAndAuctionServer,
                              blink::features::kAdAuctionSignals,
                              blink::features::kBrowsingTopics},
        /*disabled_features=*/{});
  }

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    original_client_ = content::SetBrowserClientForTesting(&browser_client_);
    browser_client_.set_interest_group_allowed_by_settings(true);
  }

  void TearDown() override {
    SetBrowserClientForTesting(original_client_);

    subresource_proxying_url_loader_service_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  base::WeakPtr<SubresourceProxyingURLLoaderService::BindContext> CreateFactory(
      network::TestURLLoaderFactory& proxied_url_loader_factory,
      mojo::Remote<network::mojom::URLLoaderFactory>&
          remote_url_loader_factory) {
    if (!subresource_proxying_url_loader_service_) {
      subresource_proxying_url_loader_service_ =
          std::make_unique<SubresourceProxyingURLLoaderService>(
              browser_context());
    }

    return subresource_proxying_url_loader_service_->GetFactory(
        remote_url_loader_factory.BindNewPipeAndPassReceiver(),
        FrameTreeNodeId(), proxied_url_loader_factory.GetSafeWeakWrapper(),
        /*render_frame_host=*/nullptr,
        /*prefetched_signed_exchange_cache=*/nullptr);
  }

  network::mojom::URLResponseHeadPtr CreateResponseHead(
      const std::optional<std::string>& ad_auction_result_header_value,
      const std::optional<std::string>& ad_auction_signals_header_value) {
    auto head = network::mojom::URLResponseHead::New();
    head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");

    if (ad_auction_result_header_value) {
      head->headers->AddHeader("Ad-Auction-Result",
                               ad_auction_result_header_value.value());
    }

    if (ad_auction_signals_header_value) {
      head->headers->AddHeader("Ad-Auction-Signals",
                               ad_auction_signals_header_value.value());
    }

    return head;
  }

  network::mojom::URLResponseHeadPtr CreateResponseHeadWithAdditionalBids(
      const std::vector<std::string>& ad_auction_additional_bid_header_values) {
    auto head = network::mojom::URLResponseHead::New();
    head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");

    for (const std::string& header_value :
         ad_auction_additional_bid_header_values) {
      head->headers->AddHeader("Ad-Auction-Additional-Bid", header_value);
    }

    return head;
  }

  network::ResourceRequest CreateResourceRequest(const GURL& url,
                                                 bool ad_auction_headers = true,
                                                 bool browsing_topics = false) {
    network::ResourceRequest request;
    request.url = url;
    request.ad_auction_headers = ad_auction_headers;
    request.browsing_topics = browsing_topics;
    return request;
  }

  void NavigatePage(const GURL& url) {
    auto simulator =
        NavigationSimulator::CreateBrowserInitiated(url, web_contents());

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

    simulator->SetPermissionsPolicyHeader(std::move(policy));

    simulator->Commit();
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
  data_decoder::test::InProcessDataDecoder data_decoder_;
  base::test::ScopedFeatureList scoped_feature_list_;

  InterceptingContentBrowserClient browser_client_;
  raw_ptr<ContentBrowserClient> original_client_ = nullptr;

  std::unique_ptr<SubresourceProxyingURLLoaderService>
      subresource_proxying_url_loader_service_;
};

TEST_F(AdAuctionURLLoaderInterceptorTest, RequestArrivedBeforeCommit) {
  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory;
  mojo::Remote<network::mojom::URLLoader> remote_loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client;

  CreateFactory(proxied_url_loader_factory, remote_url_loader_factory);

  // This request arrives before commit. It is thus not eligible for ad auction
  // headers.
  remote_url_loader_factory->CreateLoaderAndStart(
      remote_loader.BindNewPipeAndPassReceiver(),
      /*request_id=*/0, /*options=*/0,
      CreateResourceRequest(GURL("https://foo1.com")),
      client.InitWithNewPipeAndPassRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  remote_url_loader_factory.FlushForTesting();

  EXPECT_EQ(1, proxied_url_loader_factory.NumPending());
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      &proxied_url_loader_factory.pending_requests()->back();

  EXPECT_EQ(pending_request->request.headers.GetHeader("Sec-Ad-Auction-Fetch"),
            std::nullopt);

  pending_request->client->OnReceiveResponse(
      CreateResponseHead(
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse,
          /*ad_auction_signals_header_value=*/std::nullopt),
      /*body=*/{}, std::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(WitnessedAuctionResultForOrigin(
      url::Origin::Create(GURL("https://foo1.com")),
      base64Decode(kLegitimateAdAuctionResponse)));
}

TEST_F(AdAuctionURLLoaderInterceptorTest, RequestArrivedAfterCommit) {
  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory;
  mojo::Remote<network::mojom::URLLoader> remote_loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client;

  base::WeakPtr<SubresourceProxyingURLLoaderService::BindContext> bind_context =
      CreateFactory(proxied_url_loader_factory, remote_url_loader_factory);
  bind_context->OnDidCommitNavigation(
      web_contents()->GetPrimaryMainFrame()->GetWeakDocumentPtr());

  // The request to `foo1.com` will add ad auction header value "?1".
  remote_url_loader_factory->CreateLoaderAndStart(
      remote_loader.BindNewPipeAndPassReceiver(),
      /*request_id=*/0, /*options=*/0,
      CreateResourceRequest(GURL("https://foo1.com")),
      client.InitWithNewPipeAndPassRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  remote_url_loader_factory.FlushForTesting();

  EXPECT_EQ(1, proxied_url_loader_factory.NumPending());
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      &proxied_url_loader_factory.pending_requests()->back();

  EXPECT_THAT(
      pending_request->request.headers.GetHeader("Sec-Ad-Auction-Fetch"),
      testing::Optional(std::string("?1")));

  // The ad auction result from the response header will be stored in the page.
  pending_request->client->OnReceiveResponse(
      CreateResponseHead(
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse,
          /*ad_auction_signals_header_value=*/std::nullopt),
      /*body=*/{}, std::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(WitnessedAuctionResultForOrigin(
      url::Origin::Create(GURL("https://foo1.com")),
      base64Decode(kLegitimateAdAuctionResponse)));
}

// Make sure that if a cloned pipe is created before the commit, but is used
// after it, it still works.
TEST_F(AdAuctionURLLoaderInterceptorTest, RequestOnClonedPipeBeforeCommit) {
  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory;
  mojo::Remote<network::mojom::URLLoader> remote_loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client;

  base::WeakPtr<SubresourceProxyingURLLoaderService::BindContext> bind_context =
      CreateFactory(proxied_url_loader_factory, remote_url_loader_factory);

  mojo::Remote<network::mojom::URLLoaderFactory>
      remote_url_loader_factory_clone;
  remote_url_loader_factory->Clone(
      remote_url_loader_factory_clone.BindNewPipeAndPassReceiver());
  // Make sure the clone actually happens before the commit.
  remote_url_loader_factory.FlushForTesting();

  bind_context->OnDidCommitNavigation(
      web_contents()->GetPrimaryMainFrame()->GetWeakDocumentPtr());

  // The request to `foo1.com` will add ad auction header value "?1".
  remote_url_loader_factory_clone->CreateLoaderAndStart(
      remote_loader.BindNewPipeAndPassReceiver(),
      /*request_id=*/0, /*options=*/0,
      CreateResourceRequest(GURL("https://foo1.com")),
      client.InitWithNewPipeAndPassRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  remote_url_loader_factory_clone.FlushForTesting();

  EXPECT_EQ(1, proxied_url_loader_factory.NumPending());
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      &proxied_url_loader_factory.pending_requests()->back();

  EXPECT_THAT(
      pending_request->request.headers.GetHeader("Sec-Ad-Auction-Fetch"),
      testing::Optional(std::string("?1")));

  // The ad auction result from the response header will be stored in the page.
  pending_request->client->OnReceiveResponse(
      CreateResponseHead(
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse,
          /*ad_auction_signals_header_value=*/std::nullopt),
      /*body=*/{}, std::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(WitnessedAuctionResultForOrigin(
      url::Origin::Create(GURL("https://foo1.com")),
      base64Decode(kLegitimateAdAuctionResponse)));
}

TEST_F(AdAuctionURLLoaderInterceptorTest, RequestFromMainFrame) {
  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory;
  mojo::Remote<network::mojom::URLLoader> remote_loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client;

  base::WeakPtr<SubresourceProxyingURLLoaderService::BindContext> bind_context =
      CreateFactory(proxied_url_loader_factory, remote_url_loader_factory);
  bind_context->OnDidCommitNavigation(
      web_contents()->GetPrimaryMainFrame()->GetWeakDocumentPtr());

  // The request to "foo1.com" will add the ad auction header value "?1".
  remote_url_loader_factory->CreateLoaderAndStart(
      remote_loader.BindNewPipeAndPassReceiver(),
      /*request_id=*/0, /*options=*/0,
      CreateResourceRequest(GURL("https://foo1.com")),
      client.InitWithNewPipeAndPassRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  remote_url_loader_factory.FlushForTesting();

  EXPECT_EQ(1, proxied_url_loader_factory.NumPending());
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      &proxied_url_loader_factory.pending_requests()->back();

  EXPECT_THAT(
      pending_request->request.headers.GetHeader("Sec-Ad-Auction-Fetch"),
      testing::Optional(std::string("?1")));

  // The ad auction result from the response header will be stored in the page.
  pending_request->client->OnReceiveResponse(
      CreateResponseHead(
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse,
          /*ad_auction_signals_header_value=*/std::nullopt),
      /*body=*/{}, std::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(WitnessedAuctionResultForOrigin(
      url::Origin::Create(GURL("https://foo1.com")),
      base64Decode(kLegitimateAdAuctionResponse)));
}

TEST_F(AdAuctionURLLoaderInterceptorTest, RequestFromSubframe) {
  NavigatePage(GURL("https://google.com"));

  TestRenderFrameHost* initial_subframe = static_cast<TestRenderFrameHost*>(
      content::RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
          ->AppendChild("child0"));

  auto subframe_navigation = NavigationSimulator::CreateRendererInitiated(
      GURL("https://google.com"), initial_subframe);

  subframe_navigation->Commit();

  RenderFrameHost* final_subframe =
      subframe_navigation->GetFinalRenderFrameHost();

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory;
  mojo::Remote<network::mojom::URLLoader> remote_loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client;

  base::WeakPtr<SubresourceProxyingURLLoaderService::BindContext> bind_context =
      CreateFactory(proxied_url_loader_factory, remote_url_loader_factory);
  bind_context->OnDidCommitNavigation(final_subframe->GetWeakDocumentPtr());

  // The request to "foo1.com" will add the ad auction header value "?1".
  remote_url_loader_factory->CreateLoaderAndStart(
      remote_loader.BindNewPipeAndPassReceiver(),
      /*request_id=*/0, /*options=*/0,
      CreateResourceRequest(GURL("https://foo1.com")),
      client.InitWithNewPipeAndPassRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  remote_url_loader_factory.FlushForTesting();

  EXPECT_EQ(1, proxied_url_loader_factory.NumPending());
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      &proxied_url_loader_factory.pending_requests()->back();

  EXPECT_THAT(
      pending_request->request.headers.GetHeader("Sec-Ad-Auction-Fetch"),
      testing::Optional(std::string("?1")));

  // The ad auction result from the response header will be stored in the page.
  pending_request->client->OnReceiveResponse(
      CreateResponseHead(
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse,
          /*ad_auction_signals_header_value=*/std::nullopt),
      /*body=*/{}, std::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(WitnessedAuctionResultForOrigin(
      url::Origin::Create(GURL("https://foo1.com")),
      base64Decode(kLegitimateAdAuctionResponse)));
}

TEST_F(AdAuctionURLLoaderInterceptorTest,
       RequestNotEligibleForAdAuctionHeadersDueToMissingFetchParam) {
  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory;
  mojo::Remote<network::mojom::URLLoader> remote_loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client;

  base::WeakPtr<SubresourceProxyingURLLoaderService::BindContext> bind_context =
      CreateFactory(proxied_url_loader_factory, remote_url_loader_factory);
  bind_context->OnDidCommitNavigation(
      web_contents()->GetPrimaryMainFrame()->GetWeakDocumentPtr());

  // Make request to `foo1.com` that sets the `browsing_topics` but not the
  // `ad_auction_headers` param, so that it's a valid request type to be proxied
  // but is not eligible for ad auction headers processing.
  remote_url_loader_factory->CreateLoaderAndStart(
      remote_loader.BindNewPipeAndPassReceiver(),
      /*request_id=*/0, /*options=*/0,
      CreateResourceRequest(GURL("https://foo1.com"),
                            /*ad_auction_headers=*/false,
                            /*browsing_topics=*/true),
      client.InitWithNewPipeAndPassRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  remote_url_loader_factory.FlushForTesting();

  EXPECT_EQ(1, proxied_url_loader_factory.NumPending());
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      &proxied_url_loader_factory.pending_requests()->back();

  EXPECT_EQ(pending_request->request.headers.GetHeader("Sec-Ad-Auction-Fetch"),
            std::nullopt);

  pending_request->client->OnReceiveResponse(
      CreateResponseHead(
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse,
          /*ad_auction_signals_header_value=*/kLegitimateAdAuctionSignals),
      /*body=*/{}, std::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(WitnessedAuctionResultForOrigin(
      url::Origin::Create(GURL("https://foo1.com")),
      base64Decode(kLegitimateAdAuctionResponse)));

  const scoped_refptr<HeaderDirectFromSellerSignals::Result> signals =
      ParseAndFindAdAuctionSignals(
          url::Origin::Create(GURL("https://foo1.com")), "slot1");
  EXPECT_EQ(signals, nullptr);
}

TEST_F(AdAuctionURLLoaderInterceptorTest,
       HasRedirect_AdAuctionResultResponseIgnored) {
  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory(
      /*observe_loader_requests=*/true);
  mojo::Remote<network::mojom::URLLoader> remote_loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client;

  base::WeakPtr<SubresourceProxyingURLLoaderService::BindContext> bind_context =
      CreateFactory(proxied_url_loader_factory, remote_url_loader_factory);
  bind_context->OnDidCommitNavigation(
      web_contents()->GetPrimaryMainFrame()->GetWeakDocumentPtr());

  // The request to "foo1.com" will add the ad auction header value "?1".
  remote_url_loader_factory->CreateLoaderAndStart(
      remote_loader.BindNewPipeAndPassReceiver(),
      /*request_id=*/0, /*options=*/0,
      CreateResourceRequest(GURL("https://foo1.com")),
      client.InitWithNewPipeAndPassRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  remote_url_loader_factory.FlushForTesting();

  EXPECT_EQ(1, proxied_url_loader_factory.NumPending());
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      &proxied_url_loader_factory.pending_requests()->back();

  EXPECT_THAT(
      pending_request->request.headers.GetHeader("Sec-Ad-Auction-Fetch"),
      testing::Optional(std::string("?1")));

  // Redirect to `foo2.com`. The ad auction result response for the initial
  // request to `foo1.com` will be ignored, and the redirect request to
  // `foo2.com` is not eligible for the ad auction headers.
  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL("https://foo2.com");

  pending_request->client->OnReceiveRedirect(
      redirect_info,
      CreateResponseHead(
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse,
          /*ad_auction_signals_header_value=*/std::nullopt));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(WitnessedAuctionResultForOrigin(
      url::Origin::Create(GURL("https://foo1.com")),
      base64Decode(kLegitimateAdAuctionResponse)));

  remote_loader->FollowRedirect(/*removed_headers=*/{},
                                /*modified_headers=*/{},
                                /*modified_cors_exempt_headers=*/{},
                                /*new_url=*/std::nullopt);
  base::RunLoop().RunUntilIdle();

  const std::vector<FollowRedirectParams>& follow_redirect_params =
      pending_request->test_url_loader->follow_redirect_params();
  EXPECT_EQ(follow_redirect_params.size(), 1u);
  EXPECT_EQ(follow_redirect_params[0].removed_headers.size(), 1u);
  EXPECT_EQ(follow_redirect_params[0].removed_headers[0],
            "Sec-Ad-Auction-Fetch");

  EXPECT_EQ(follow_redirect_params[0].modified_headers.GetHeader(
                "Sec-Ad-Auction-Fetch"),
            std::nullopt);

  pending_request->client->OnReceiveResponse(
      CreateResponseHead(
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse,
          /*ad_auction_signals_header_value=*/std::nullopt),
      /*body=*/{}, std::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(WitnessedAuctionResultForOrigin(
      url::Origin::Create(GURL("https://foo2.com")),
      base64Decode(kLegitimateAdAuctionResponse)));
}

TEST_F(AdAuctionURLLoaderInterceptorTest, AdAuctionSignalsResponseHeader) {
  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory;
  mojo::Remote<network::mojom::URLLoader> remote_loader;
  TestURLLoaderClient test_client;

  base::WeakPtr<SubresourceProxyingURLLoaderService::BindContext> bind_context =
      CreateFactory(proxied_url_loader_factory, remote_url_loader_factory);
  bind_context->OnDidCommitNavigation(
      web_contents()->GetPrimaryMainFrame()->GetWeakDocumentPtr());

  // The request to "foo1.com" will add the ad auction header value "?1".
  remote_url_loader_factory->CreateLoaderAndStart(
      remote_loader.BindNewPipeAndPassReceiver(),
      /*request_id=*/0, /*options=*/0,
      CreateResourceRequest(GURL("https://foo1.com")),
      test_client.BindURLLoaderClientAndGetRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  remote_url_loader_factory.FlushForTesting();

  EXPECT_EQ(1, proxied_url_loader_factory.NumPending());
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      &proxied_url_loader_factory.pending_requests()->back();

  EXPECT_THAT(
      pending_request->request.headers.GetHeader("Sec-Ad-Auction-Fetch"),
      testing::Optional(std::string("?1")));

  // The ad auction signals from the response header will be stored in the page.
  pending_request->client->OnReceiveResponse(
      CreateResponseHead(
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse,
          /*ad_auction_signals_header_value=*/kLegitimateAdAuctionSignals),
      /*body=*/{}, std::nullopt);
  base::RunLoop().RunUntilIdle();

  // The `Ad-Auction-Signals` header was intercepted and stored in the browser.
  // It was not exposed to the original loader client. In contrast, the
  //  `Ad-Auction-Result` header was exposed to the original loader client.
  EXPECT_TRUE(test_client.received_response());
  EXPECT_TRUE(test_client.received_ad_auction_result_header());
  EXPECT_FALSE(test_client.received_ad_auction_signals_header());

  const scoped_refptr<HeaderDirectFromSellerSignals::Result> signals =
      ParseAndFindAdAuctionSignals(
          url::Origin::Create(GURL("https://foo1.com")), "slot1");
  EXPECT_EQ(signals->seller_signals(), R"({"signal1":"value1"})");
}

TEST_F(AdAuctionURLLoaderInterceptorTest,
       HasRedirect_AdAuctionSignalsResponseIgnored) {
  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory(
      /*observe_loader_requests=*/true);
  mojo::Remote<network::mojom::URLLoader> remote_loader;
  TestURLLoaderClient test_client;

  base::WeakPtr<SubresourceProxyingURLLoaderService::BindContext> bind_context =
      CreateFactory(proxied_url_loader_factory, remote_url_loader_factory);
  bind_context->OnDidCommitNavigation(
      web_contents()->GetPrimaryMainFrame()->GetWeakDocumentPtr());

  // The request to "foo1.com" will add the ad auction header value "?1".
  remote_url_loader_factory->CreateLoaderAndStart(
      remote_loader.BindNewPipeAndPassReceiver(),
      /*request_id=*/0, /*options=*/0,
      CreateResourceRequest(GURL("https://foo1.com")),
      test_client.BindURLLoaderClientAndGetRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  remote_url_loader_factory.FlushForTesting();

  EXPECT_EQ(1, proxied_url_loader_factory.NumPending());
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      &proxied_url_loader_factory.pending_requests()->back();

  EXPECT_THAT(
      pending_request->request.headers.GetHeader("Sec-Ad-Auction-Fetch"),
      testing::Optional(std::string("?1")));

  // Redirect to `foo2.com`. The ad auction signals response for the initial
  // request to `foo1.com` will be ignored, and the redirect request to
  // `foo2.com` is not eligible for the ad auction headers.
  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL("https://foo2.com");

  pending_request->client->OnReceiveRedirect(
      redirect_info,
      CreateResponseHead(
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse,
          /*ad_auction_signals_header_value=*/kLegitimateAdAuctionSignals));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(WitnessedAuctionResultForOrigin(
      url::Origin::Create(GURL("https://foo1.com")),
      base64Decode(kLegitimateAdAuctionResponse)));

  remote_loader->FollowRedirect(/*removed_headers=*/{},
                                /*modified_headers=*/{},
                                /*modified_cors_exempt_headers=*/{},
                                /*new_url=*/std::nullopt);
  base::RunLoop().RunUntilIdle();

  const std::vector<FollowRedirectParams>& follow_redirect_params =
      pending_request->test_url_loader->follow_redirect_params();
  EXPECT_EQ(follow_redirect_params.size(), 1u);
  EXPECT_EQ(follow_redirect_params[0].removed_headers.size(), 1u);
  EXPECT_EQ(follow_redirect_params[0].removed_headers[0],
            "Sec-Ad-Auction-Fetch");

  EXPECT_EQ(follow_redirect_params[0].modified_headers.GetHeader(
                "Sec-Ad-Auction-Fetch"),
            std::nullopt);

  pending_request->client->OnReceiveResponse(
      CreateResponseHead(
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse,
          /*ad_auction_signals_header_value=*/kLegitimateAdAuctionSignals),
      /*body=*/{}, std::nullopt);
  base::RunLoop().RunUntilIdle();

  // The `Ad-Auction-Signals` header was ignored and not exposed to the original
  // loader client.
  EXPECT_TRUE(test_client.received_response());
  EXPECT_TRUE(test_client.received_ad_auction_result_header());
  EXPECT_FALSE(test_client.received_ad_auction_signals_header());

  const scoped_refptr<HeaderDirectFromSellerSignals::Result> signals =
      ParseAndFindAdAuctionSignals(
          url::Origin::Create(GURL("https://foo1.com")), "slot1");
  EXPECT_EQ(signals, nullptr);
}

TEST_F(AdAuctionURLLoaderInterceptorTest, AdditionalBids) {
  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory;
  mojo::Remote<network::mojom::URLLoader> remote_loader;
  TestURLLoaderClient test_client;

  base::WeakPtr<SubresourceProxyingURLLoaderService::BindContext> bind_context =
      CreateFactory(proxied_url_loader_factory, remote_url_loader_factory);
  bind_context->OnDidCommitNavigation(
      web_contents()->GetPrimaryMainFrame()->GetWeakDocumentPtr());

  // The request to "foo1.com" will add the ad auction header value "?1".
  remote_url_loader_factory->CreateLoaderAndStart(
      remote_loader.BindNewPipeAndPassReceiver(),
      /*request_id=*/0, /*options=*/0,
      CreateResourceRequest(GURL("https://foo1.com")),
      test_client.BindURLLoaderClientAndGetRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  remote_url_loader_factory.FlushForTesting();

  EXPECT_EQ(1, proxied_url_loader_factory.NumPending());
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      &proxied_url_loader_factory.pending_requests()->back();

  EXPECT_THAT(
      pending_request->request.headers.GetHeader("Sec-Ad-Auction-Fetch"),
      testing::Optional(std::string("?1")));

  pending_request->client->OnReceiveResponse(
      CreateResponseHeadWithAdditionalBids(
          {"00000000-0000-0000-0000-000000000000:e30=",
           "00000000-0000-0000-0000-000000000001:e30=",
           "00000000-0000-0000-0000-000000000001:e2E6IDF9"}),
      /*body=*/{}, std::nullopt);
  base::RunLoop().RunUntilIdle();

  // The `Ad-Auction-Additional-Bid` header was intercepted and stored in the
  // browser. It was not exposed to the original loader client.
  EXPECT_TRUE(test_client.received_response());
  EXPECT_FALSE(test_client.received_ad_auction_additional_bid_header());

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

// Tests that the Ad-Auction-Additional-Bid header will be removed from the
// final response even when the request isn't eligible for ad auction.
TEST_F(
    AdAuctionURLLoaderInterceptorTest,
    AdAuctionDisallowedBySettings_AdditionalBidResponseHeaderNotHandledAndRemoved) {
  browser_client_.set_interest_group_allowed_by_settings(false);

  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory;
  mojo::Remote<network::mojom::URLLoader> remote_loader;
  TestURLLoaderClient test_client;

  base::WeakPtr<SubresourceProxyingURLLoaderService::BindContext> bind_context =
      CreateFactory(proxied_url_loader_factory, remote_url_loader_factory);
  bind_context->OnDidCommitNavigation(
      web_contents()->GetPrimaryMainFrame()->GetWeakDocumentPtr());

  // The request to `foo1.com` won't be eligible for ad auction.
  remote_url_loader_factory->CreateLoaderAndStart(
      remote_loader.BindNewPipeAndPassReceiver(),
      /*request_id=*/0, /*options=*/0,
      CreateResourceRequest(GURL("https://foo1.com")),
      test_client.BindURLLoaderClientAndGetRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  remote_url_loader_factory.FlushForTesting();

  EXPECT_EQ(1, proxied_url_loader_factory.NumPending());
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      &proxied_url_loader_factory.pending_requests()->back();

  EXPECT_EQ(pending_request->request.headers.GetHeader("Sec-Ad-Auction-Fetch"),
            std::nullopt);

  pending_request->client->OnReceiveResponse(
      CreateResponseHeadWithAdditionalBids(
          {"00000000-0000-0000-0000-000000000000:e30="}),
      /*body=*/{}, std::nullopt);
  base::RunLoop().RunUntilIdle();

  // The `Ad-Auction-Additional-Bid` header was intercepted. It's not stored
  // in the browser since the request wasn't eligible for ad auction. It was not
  // exposed to the original loader client.
  EXPECT_TRUE(test_client.received_response());
  EXPECT_FALSE(test_client.received_ad_auction_additional_bid_header());

  url::Origin request_origin = url::Origin::Create(GURL("https://foo1.com"));
  EXPECT_THAT(TakeAuctionAdditionalBidsForOriginAndNonce(
                  request_origin, "00000000-0000-0000-0000-000000000000"),
              ::testing::IsEmpty());
}

TEST_F(AdAuctionURLLoaderInterceptorTest,
       HasRedirect_AdditionalBidResponseHeaderIgnoredAndRemoved) {
  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory(
      /*observe_loader_requests=*/true);
  mojo::Remote<network::mojom::URLLoader> remote_loader;
  TestURLLoaderClient test_client;

  base::WeakPtr<SubresourceProxyingURLLoaderService::BindContext> bind_context =
      CreateFactory(proxied_url_loader_factory, remote_url_loader_factory);
  bind_context->OnDidCommitNavigation(
      web_contents()->GetPrimaryMainFrame()->GetWeakDocumentPtr());

  // The request to "foo1.com" will add the ad auction header value "?1".
  remote_url_loader_factory->CreateLoaderAndStart(
      remote_loader.BindNewPipeAndPassReceiver(),
      /*request_id=*/0, /*options=*/0,
      CreateResourceRequest(GURL("https://foo1.com")),
      test_client.BindURLLoaderClientAndGetRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  remote_url_loader_factory.FlushForTesting();

  EXPECT_EQ(1, proxied_url_loader_factory.NumPending());
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      &proxied_url_loader_factory.pending_requests()->back();

  EXPECT_THAT(
      pending_request->request.headers.GetHeader("Sec-Ad-Auction-Fetch"),
      testing::Optional(std::string("?1")));

  // Redirect to `foo2.com`. The ad auction additional bid response for the
  // initial request to `foo1.com` will be ignored, and the redirect request to
  // `foo2.com` won't be eligible for the ad auction headers.
  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL("https://foo2.com");

  pending_request->client->OnReceiveRedirect(
      redirect_info, CreateResponseHeadWithAdditionalBids(
                         {"00000000-0000-0000-0000-000000000000:e30="}));
  base::RunLoop().RunUntilIdle();

  remote_loader->FollowRedirect(/*removed_headers=*/{},
                                /*modified_headers=*/{},
                                /*modified_cors_exempt_headers=*/{},
                                /*new_url=*/std::nullopt);
  base::RunLoop().RunUntilIdle();

  const std::vector<FollowRedirectParams>& follow_redirect_params =
      pending_request->test_url_loader->follow_redirect_params();
  EXPECT_EQ(follow_redirect_params.size(), 1u);
  EXPECT_EQ(follow_redirect_params[0].removed_headers.size(), 1u);
  EXPECT_EQ(follow_redirect_params[0].removed_headers[0],
            "Sec-Ad-Auction-Fetch");

  EXPECT_EQ(follow_redirect_params[0].modified_headers.GetHeader(
                "Sec-Ad-Auction-Fetch"),
            std::nullopt);

  pending_request->client->OnReceiveResponse(
      CreateResponseHeadWithAdditionalBids(
          {"00000000-0000-0000-0000-000000000000:e30="}),
      /*body=*/{}, std::nullopt);
  base::RunLoop().RunUntilIdle();

  // The `Ad-Auction-Additional-Bid` header was ignored and not exposed to the
  // original loader client.
  EXPECT_TRUE(test_client.received_response());
  EXPECT_FALSE(test_client.received_ad_auction_additional_bid_header());

  url::Origin request_origin = url::Origin::Create(GURL("https://foo1.com"));
  EXPECT_THAT(TakeAuctionAdditionalBidsForOriginAndNonce(
                  request_origin, "00000000-0000-0000-0000-000000000000"),
              ::testing::IsEmpty());
}

}  // namespace content
