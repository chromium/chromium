// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/interest_group/ad_auction_page_data.h"
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
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/parsed_headers.mojom.h"
#include "services/network/test/test_url_loader_factory.h"

namespace content {

namespace {

constexpr char kLegitimateAdAuctionResponse[] =
    "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";

using FollowRedirectParams =
    network::TestURLLoaderFactory::TestURLLoader::FollowRedirectParams;

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

}  // namespace

class AdAuctionURLLoaderInterceptorTest : public RenderViewHostTestHarness {
 public:
  AdAuctionURLLoaderInterceptorTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kInterestGroupStorage,
                              blink::features::kFledgeBiddingAndAuctionServer},
        /*disabled_features=*/{});
  }

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    original_client_ = content::SetBrowserClientForTesting(&browser_client_);
    browser_client_.set_interest_group_allowed_by_settings(true);
  }

  void TearDown() override {
    SetBrowserClientForTesting(original_client_);

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
        /*frame_tree_node_id=*/0,
        proxied_url_loader_factory.GetSafeWeakWrapper(),
        /*render_frame_host=*/nullptr,
        /*prefetched_signed_exchange_cache=*/nullptr);
  }

  network::mojom::URLResponseHeadPtr CreateResponseHead(
      const absl::optional<std::string>& ad_auction_result_header_value) {
    auto head = network::mojom::URLResponseHead::New();
    head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");

    if (ad_auction_result_header_value) {
      head->headers->AddHeader("Ad-Auction-Result",
                               ad_auction_result_header_value.value());
    }

    return head;
  }

  network::ResourceRequest CreateResourceRequest(
      const GURL& url,
      bool ad_auction_headers = true) {
    network::ResourceRequest request;
    request.url = url;
    request.ad_auction_headers = ad_auction_headers;
    return request;
  }

  void NavigatePage(const GURL& url) {
    auto simulator =
        NavigationSimulator::CreateBrowserInitiated(url, web_contents());

    blink::ParsedPermissionsPolicy policy;
    policy.emplace_back(blink::mojom::PermissionsPolicyFeature::kRunAdAuction,
                        /*allowed_origins=*/
                        std::vector<blink::OriginWithPossibleWildcards>{
                            blink::OriginWithPossibleWildcards(
                                url::Origin::Create(GURL("https://google.com")),
                                /*has_subdomain_wildcard=*/false),
                            blink::OriginWithPossibleWildcards(
                                url::Origin::Create(GURL("https://foo1.com")),
                                /*has_subdomain_wildcard=*/false)},
                        /*self_if_matches=*/absl::nullopt,
                        /*matches_all_origins=*/false,
                        /*matches_opaque_src=*/false);

    simulator->SetPermissionsPolicyHeader(std::move(policy));

    simulator->Commit();
  }

  bool WitnessedAuctionResponseForOrigin(const url::Origin& origin,
                                         const std::string& response) {
    Page& page = web_contents()->GetPrimaryPage();

    AdAuctionPageData* ad_auction_page_data =
        PageUserData<AdAuctionPageData>::GetOrCreateForPage(page);

    return ad_auction_page_data->WitnessedAuctionResponseForOrigin(origin,
                                                                   response);
  }

 protected:
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

  std::string ad_auction_request_header_value;
  bool has_ad_auction_request_header =
      pending_request->request.headers.GetHeader(
          "Sec-Ad-Auction-Fetch", &ad_auction_request_header_value);
  EXPECT_FALSE(has_ad_auction_request_header);

  pending_request->client->OnReceiveResponse(
      CreateResponseHead(
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse),
      /*body=*/{}, absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(WitnessedAuctionResponseForOrigin(
      url::Origin::Create(GURL("https://foo1.com")),
      kLegitimateAdAuctionResponse));
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

  // The request to `foo1.com` will cause the ad auction header value "?1" to be
  // added.
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

  std::string ad_auction_request_header_value;
  bool has_ad_auction_request_header =
      pending_request->request.headers.GetHeader(
          "Sec-Ad-Auction-Fetch", &ad_auction_request_header_value);
  EXPECT_TRUE(has_ad_auction_request_header);
  EXPECT_EQ(ad_auction_request_header_value, "?1");

  // The ad auction result from the response header will be stored in the page.
  pending_request->client->OnReceiveResponse(
      CreateResponseHead(
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse),
      /*body=*/{}, absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(WitnessedAuctionResponseForOrigin(
      url::Origin::Create(GURL("https://foo1.com")),
      kLegitimateAdAuctionResponse));
}

TEST_F(AdAuctionURLLoaderInterceptorTest,
       RequestArrivedAfterDocumentDestroyed) {
  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory;
  mojo::Remote<network::mojom::URLLoader> remote_loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client;

  base::WeakPtr<SubresourceProxyingURLLoaderService::BindContext> bind_context =
      CreateFactory(proxied_url_loader_factory, remote_url_loader_factory);
  bind_context->OnDidCommitNavigation(
      web_contents()->GetPrimaryMainFrame()->GetWeakDocumentPtr());

  // This second navigation will cause the initial document referenced by the
  // factory to be destroyed. Thus the request won't be eligible for ad auction
  // headers.
  auto simulator = NavigationSimulator::CreateBrowserInitiated(
      GURL("https://foo1.com"), web_contents());
  simulator->Commit();

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

  std::string ad_auction_request_header_value;
  bool has_ad_auction_request_header =
      pending_request->request.headers.GetHeader(
          "Sec-Ad-Auction-Fetch", &ad_auction_request_header_value);
  EXPECT_FALSE(has_ad_auction_request_header);

  pending_request->client->OnReceiveResponse(
      CreateResponseHead(
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse),
      /*body=*/{}, absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(WitnessedAuctionResponseForOrigin(
      url::Origin::Create(GURL("https://foo1.com")),
      kLegitimateAdAuctionResponse));
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

  // The request to `foo1.com` will cause an empty ad auction header value to be
  // added.
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

  std::string ad_auction_request_header_value;
  bool has_ad_auction_request_header =
      pending_request->request.headers.GetHeader(
          "Sec-Ad-Auction-Fetch", &ad_auction_request_header_value);
  EXPECT_TRUE(has_ad_auction_request_header);
  EXPECT_EQ(ad_auction_request_header_value, "?1");

  // The ad auction result from the response header will be stored in the page.
  pending_request->client->OnReceiveResponse(
      CreateResponseHead(
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse),
      /*body=*/{}, absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(WitnessedAuctionResponseForOrigin(
      url::Origin::Create(GURL("https://foo1.com")),
      kLegitimateAdAuctionResponse));
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

  // The request to `foo1.com` will cause the ad auction header value "?1" to be
  // added.
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

  std::string ad_auction_request_header_value;
  bool has_ad_auction_request_header =
      pending_request->request.headers.GetHeader(
          "Sec-Ad-Auction-Fetch", &ad_auction_request_header_value);
  EXPECT_TRUE(has_ad_auction_request_header);
  EXPECT_EQ(ad_auction_request_header_value, "?1");

  // The ad auction result from the response header will be stored in the page.
  pending_request->client->OnReceiveResponse(
      CreateResponseHead(
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse),
      /*body=*/{}, absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(WitnessedAuctionResponseForOrigin(
      url::Origin::Create(GURL("https://foo1.com")),
      kLegitimateAdAuctionResponse));
}

TEST_F(AdAuctionURLLoaderInterceptorTest,
       RequestNotEligibleForAdAuctionHeadersDueToSettings) {
  browser_client_.set_interest_group_allowed_by_settings(false);

  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory;
  mojo::Remote<network::mojom::URLLoader> remote_loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client;

  base::WeakPtr<SubresourceProxyingURLLoaderService::BindContext> bind_context =
      CreateFactory(proxied_url_loader_factory, remote_url_loader_factory);
  bind_context->OnDidCommitNavigation(
      web_contents()->GetPrimaryMainFrame()->GetWeakDocumentPtr());

  // The request to `foo1.com` won't be eligible for ad auction.
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

  std::string ad_auction_request_header_value;
  bool has_ad_auction_request_header =
      pending_request->request.headers.GetHeader(
          "Sec-Ad-Auction-Fetch", &ad_auction_request_header_value);
  EXPECT_FALSE(has_ad_auction_request_header);

  pending_request->client->OnReceiveResponse(
      CreateResponseHead(
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse),
      /*body=*/{}, absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(WitnessedAuctionResponseForOrigin(
      url::Origin::Create(GURL("https://foo1.com")),
      kLegitimateAdAuctionResponse));
}

TEST_F(AdAuctionURLLoaderInterceptorTest,
       InvalidAdAuctionResultResponseHeader) {
  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory;
  mojo::Remote<network::mojom::URLLoader> remote_loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client;

  base::WeakPtr<SubresourceProxyingURLLoaderService::BindContext> bind_context =
      CreateFactory(proxied_url_loader_factory, remote_url_loader_factory);
  bind_context->OnDidCommitNavigation(
      web_contents()->GetPrimaryMainFrame()->GetWeakDocumentPtr());

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

  std::string ad_auction_request_header_value;
  bool has_ad_auction_request_header =
      pending_request->request.headers.GetHeader(
          "Sec-Ad-Auction-Fetch", &ad_auction_request_header_value);
  EXPECT_TRUE(has_ad_auction_request_header);
  EXPECT_EQ(ad_auction_request_header_value, "?1");

  // Expect no further handling for topics as the response header value is
  // false.
  pending_request->client->OnReceiveResponse(
      CreateResponseHead(
          /*ad_auction_result_header_value=*/"invalid-response-header"),
      /*body=*/{}, absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(WitnessedAuctionResponseForOrigin(
      url::Origin::Create(GURL("https://foo1.com")),
      "invalid-response-header"));
}

TEST_F(AdAuctionURLLoaderInterceptorTest, RequestFromInactiveFrame) {
  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory;
  mojo::Remote<network::mojom::URLLoader> remote_loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client;

  base::WeakPtr<SubresourceProxyingURLLoaderService::BindContext> bind_context =
      CreateFactory(proxied_url_loader_factory, remote_url_loader_factory);
  bind_context->OnDidCommitNavigation(
      web_contents()->GetPrimaryMainFrame()->GetWeakDocumentPtr());

  // Switch the frame to an inactive state. The request won't be eligible for
  // ad auction.
  RenderFrameHostImpl& rfh =
      static_cast<RenderFrameHostImpl&>(*web_contents()->GetPrimaryMainFrame());
  rfh.SetLifecycleState(
      RenderFrameHostImpl::LifecycleStateImpl::kReadyToBeDeleted);

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

  std::string ad_auction_request_header_value;
  bool has_ad_auction_request_header =
      pending_request->request.headers.GetHeader(
          "Sec-Ad-Auction-Fetch", &ad_auction_request_header_value);
  EXPECT_FALSE(has_ad_auction_request_header);

  pending_request->client->OnReceiveResponse(
      CreateResponseHead(
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse),
      /*body=*/{}, absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(WitnessedAuctionResponseForOrigin(
      url::Origin::Create(GURL("https://foo1.com")),
      kLegitimateAdAuctionResponse));
}

TEST_F(AdAuctionURLLoaderInterceptorTest,
       AdAuctionHeadersNotEligibleDueToPermissionsPolicy) {
  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory;
  mojo::Remote<network::mojom::URLLoader> remote_loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client;

  base::WeakPtr<SubresourceProxyingURLLoaderService::BindContext> bind_context =
      CreateFactory(proxied_url_loader_factory, remote_url_loader_factory);
  bind_context->OnDidCommitNavigation(
      web_contents()->GetPrimaryMainFrame()->GetWeakDocumentPtr());

  // The permissions policy disallows `foo2.com`. The request won't be eligible
  // for ad auction headers.
  remote_url_loader_factory->CreateLoaderAndStart(
      remote_loader.BindNewPipeAndPassReceiver(),
      /*request_id=*/0, /*options=*/0,
      CreateResourceRequest(GURL("https://foo2.com")),
      client.InitWithNewPipeAndPassRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  remote_url_loader_factory.FlushForTesting();

  EXPECT_EQ(1, proxied_url_loader_factory.NumPending());
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      &proxied_url_loader_factory.pending_requests()->back();

  std::string ad_auction_request_header_value;
  bool has_ad_auction_request_header =
      pending_request->request.headers.GetHeader(
          "Sec-Ad-Auction-Fetch", &ad_auction_request_header_value);
  EXPECT_FALSE(has_ad_auction_request_header);

  pending_request->client->OnReceiveResponse(
      CreateResponseHead(
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse),
      /*body=*/{}, absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(WitnessedAuctionResponseForOrigin(
      url::Origin::Create(GURL("https://foo2.com")),
      kLegitimateAdAuctionResponse));
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

  // The request to `foo1.com` will cause the ad auction header value "?1" to be
  // added.
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

  std::string ad_auction_request_header_value;
  bool has_ad_auction_request_header =
      pending_request->request.headers.GetHeader(
          "Sec-Ad-Auction-Fetch", &ad_auction_request_header_value);
  EXPECT_TRUE(has_ad_auction_request_header);
  EXPECT_EQ(ad_auction_request_header_value, "?1");

  // Redirect to `foo2.com`. The ad auction result in response for the initial
  // request to `foo1.com` will be ignored, and the redirect request to
  // `foo2.com` is eligible for the ad auction headers either.
  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL("https://foo2.com");

  pending_request->client->OnReceiveRedirect(
      redirect_info,
      CreateResponseHead(
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(WitnessedAuctionResponseForOrigin(
      url::Origin::Create(GURL("https://foo1.com")),
      kLegitimateAdAuctionResponse));

  remote_loader->FollowRedirect(/*removed_headers=*/{},
                                /*modified_headers=*/{},
                                /*modified_cors_exempt_headers=*/{},
                                /*new_url=*/absl::nullopt);
  base::RunLoop().RunUntilIdle();

  const std::vector<FollowRedirectParams>& follow_redirect_params =
      pending_request->test_url_loader->follow_redirect_params();
  EXPECT_EQ(follow_redirect_params.size(), 1u);
  EXPECT_EQ(follow_redirect_params[0].removed_headers.size(), 1u);
  EXPECT_EQ(follow_redirect_params[0].removed_headers[0],
            "Sec-Ad-Auction-Fetch");

  std::string redirect_ad_auction_request_header_value;
  bool redirect_has_ad_auction_request_header =
      follow_redirect_params[0].modified_headers.GetHeader(
          "Sec-Ad-Auction-Fetch", &redirect_ad_auction_request_header_value);
  EXPECT_FALSE(redirect_has_ad_auction_request_header);

  pending_request->client->OnReceiveResponse(
      CreateResponseHead(
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse),
      /*body=*/{}, absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(WitnessedAuctionResponseForOrigin(
      url::Origin::Create(GURL("https://foo2.com")),
      kLegitimateAdAuctionResponse));
}

}  // namespace content
