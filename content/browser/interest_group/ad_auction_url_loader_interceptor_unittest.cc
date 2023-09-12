// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64url.h"
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
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

namespace {

constexpr char kLegitimateAdAuctionResponse[] =
    "ungWv48Bz-pBQUDeXa4iI7ADYaOWF3qctBD_YfIAFa0=";

using FollowRedirectParams =
    network::TestURLLoaderFactory::TestURLLoader::FollowRedirectParams;

std::string base64Decode(base::StringPiece input) {
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
      absl::optional<mojo_base::BigBuffer> cached_metadata) override {
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
      const absl::optional<std::string>& ad_auction_result_header_value,
      const absl::optional<std::string>& ad_auction_signals_header_value) {
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
        /*self_if_matches=*/absl::nullopt,
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

  const std::set<std::string>& GetAuctionSignalsForOrigin(
      const url::Origin& origin) {
    Page& page = web_contents()->GetPrimaryPage();

    AdAuctionPageData* ad_auction_page_data =
        PageUserData<AdAuctionPageData>::GetOrCreateForPage(page);

    return ad_auction_page_data->GetAuctionSignalsForOrigin(origin);
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
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse,
          /*ad_auction_signals_header_value=*/absl::nullopt),
      /*body=*/{}, absl::nullopt);
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

  std::string ad_auction_request_header_value;
  bool has_ad_auction_request_header =
      pending_request->request.headers.GetHeader(
          "Sec-Ad-Auction-Fetch", &ad_auction_request_header_value);
  EXPECT_TRUE(has_ad_auction_request_header);
  EXPECT_EQ(ad_auction_request_header_value, "?1");

  // The ad auction result from the response header will be stored in the page.
  pending_request->client->OnReceiveResponse(
      CreateResponseHead(
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse,
          /*ad_auction_signals_header_value=*/absl::nullopt),
      /*body=*/{}, absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(WitnessedAuctionResultForOrigin(
      url::Origin::Create(GURL("https://foo1.com")),
      base64Decode(kLegitimateAdAuctionResponse)));
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
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse,
          /*ad_auction_signals_header_value=*/absl::nullopt),
      /*body=*/{}, absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(WitnessedAuctionResultForOrigin(
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

  std::string ad_auction_request_header_value;
  bool has_ad_auction_request_header =
      pending_request->request.headers.GetHeader(
          "Sec-Ad-Auction-Fetch", &ad_auction_request_header_value);
  EXPECT_TRUE(has_ad_auction_request_header);
  EXPECT_EQ(ad_auction_request_header_value, "?1");

  // The ad auction result from the response header will be stored in the page.
  pending_request->client->OnReceiveResponse(
      CreateResponseHead(
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse,
          /*ad_auction_signals_header_value=*/absl::nullopt),
      /*body=*/{}, absl::nullopt);
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

  std::string ad_auction_request_header_value;
  bool has_ad_auction_request_header =
      pending_request->request.headers.GetHeader(
          "Sec-Ad-Auction-Fetch", &ad_auction_request_header_value);
  EXPECT_TRUE(has_ad_auction_request_header);
  EXPECT_EQ(ad_auction_request_header_value, "?1");

  // The ad auction result from the response header will be stored in the page.
  pending_request->client->OnReceiveResponse(
      CreateResponseHead(
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse,
          /*ad_auction_signals_header_value=*/absl::nullopt),
      /*body=*/{}, absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(WitnessedAuctionResultForOrigin(
      url::Origin::Create(GURL("https://foo1.com")),
      base64Decode(kLegitimateAdAuctionResponse)));
}

TEST_F(AdAuctionURLLoaderInterceptorTest, MultipleResults) {
  const char kLegitimateAdAuctionResponse2[] =
      "8oX0szl-BNWitSuE3ZK5Npt05t83A1wrl94oBtlZHFs=";
  const char kLegitimateAdAuctionResponse3[] =
      "lIcI37kQp_ArBk_1JfdEjyQ0suSLUpYDIKO906THBdk=";
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

  std::string ad_auction_request_header_value;
  bool has_ad_auction_request_header =
      pending_request->request.headers.GetHeader(
          "Sec-Ad-Auction-Fetch", &ad_auction_request_header_value);
  EXPECT_TRUE(has_ad_auction_request_header);
  EXPECT_EQ(ad_auction_request_header_value, "?1");

  auto head = CreateResponseHead(
      /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse,
      /*ad_auction_signals_header_value=*/absl::nullopt);

  head->headers->AddHeader(
      "Ad-Auction-Result",
      base::StrCat({kLegitimateAdAuctionResponse2, ",", "invalid", ",",
                    kLegitimateAdAuctionResponse3}));
  head->headers->AddHeader("Ad-Auction-Result", "alsonotValid");
  // The ad auction result from the response header will be stored in the page.
  pending_request->client->OnReceiveResponse(std::move(head), /*body=*/{},
                                             absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(WitnessedAuctionResultForOrigin(
      url::Origin::Create(GURL("https://foo1.com")),
      base64Decode(kLegitimateAdAuctionResponse)));

  EXPECT_TRUE(WitnessedAuctionResultForOrigin(
      url::Origin::Create(GURL("https://foo1.com")),
      base64Decode(kLegitimateAdAuctionResponse2)));

  EXPECT_TRUE(WitnessedAuctionResultForOrigin(
      url::Origin::Create(GURL("https://foo1.com")),
      base64Decode(kLegitimateAdAuctionResponse3)));
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

  std::string ad_auction_request_header_value;
  bool has_ad_auction_request_header =
      pending_request->request.headers.GetHeader(
          "Sec-Ad-Auction-Fetch", &ad_auction_request_header_value);
  EXPECT_FALSE(has_ad_auction_request_header);

  pending_request->client->OnReceiveResponse(
      CreateResponseHead(
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse,
          /*ad_auction_signals_header_value=*/"{}"),
      /*body=*/{}, absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(WitnessedAuctionResultForOrigin(
      url::Origin::Create(GURL("https://foo1.com")),
      base64Decode(kLegitimateAdAuctionResponse)));

  const std::set<std::string>& signals =
      GetAuctionSignalsForOrigin(url::Origin::Create(GURL("https://foo1.com")));
  EXPECT_THAT(signals, ::testing::IsEmpty());
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
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse,
          /*ad_auction_signals_header_value=*/"{}"),
      /*body=*/{}, absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(WitnessedAuctionResultForOrigin(
      url::Origin::Create(GURL("https://foo1.com")),
      base64Decode(kLegitimateAdAuctionResponse)));

  const std::set<std::string>& signals =
      GetAuctionSignalsForOrigin(url::Origin::Create(GURL("https://foo1.com")));
  EXPECT_THAT(signals, ::testing::IsEmpty());
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
          /*ad_auction_result_header_value=*/"invalid-response-header",
          /*ad_auction_signals_header_value=*/absl::nullopt),
      /*body=*/{}, absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(WitnessedAuctionResultForOrigin(
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
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse,
          /*ad_auction_signals_header_value=*/absl::nullopt),
      /*body=*/{}, absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(WitnessedAuctionResultForOrigin(
      url::Origin::Create(GURL("https://foo1.com")),
      base64Decode(kLegitimateAdAuctionResponse)));
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
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse,
          /*ad_auction_signals_header_value=*/absl::nullopt),
      /*body=*/{}, absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(WitnessedAuctionResultForOrigin(
      url::Origin::Create(GURL("https://foo2.com")),
      base64Decode(kLegitimateAdAuctionResponse)));
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

  std::string ad_auction_request_header_value;
  bool has_ad_auction_request_header =
      pending_request->request.headers.GetHeader(
          "Sec-Ad-Auction-Fetch", &ad_auction_request_header_value);
  EXPECT_TRUE(has_ad_auction_request_header);
  EXPECT_EQ(ad_auction_request_header_value, "?1");

  // Redirect to `foo2.com`. The ad auction result response for the initial
  // request to `foo1.com` will be ignored, and the redirect request to
  // `foo2.com` is not eligible for the ad auction headers.
  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL("https://foo2.com");

  pending_request->client->OnReceiveRedirect(
      redirect_info,
      CreateResponseHead(
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse,
          /*ad_auction_signals_header_value=*/absl::nullopt));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(WitnessedAuctionResultForOrigin(
      url::Origin::Create(GURL("https://foo1.com")),
      base64Decode(kLegitimateAdAuctionResponse)));

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
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse,
          /*ad_auction_signals_header_value=*/absl::nullopt),
      /*body=*/{}, absl::nullopt);
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

  std::string ad_auction_request_header_value;
  bool has_ad_auction_request_header =
      pending_request->request.headers.GetHeader(
          "Sec-Ad-Auction-Fetch", &ad_auction_request_header_value);
  EXPECT_TRUE(has_ad_auction_request_header);
  EXPECT_EQ(ad_auction_request_header_value, "?1");

  // The ad auction signals from the response header will be stored in the page.
  pending_request->client->OnReceiveResponse(
      CreateResponseHead(
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse,
          /*ad_auction_signals_header_value=*/"{}"),
      /*body=*/{}, absl::nullopt);
  base::RunLoop().RunUntilIdle();

  // The `Ad-Auction-Signals` header was intercepted and stored in the browser.
  // It was not exposed to the original loader client. In contrast, the
  //  `Ad-Auction-Result` header was exposed to the original loader client.
  EXPECT_TRUE(test_client.received_response());
  EXPECT_TRUE(test_client.received_ad_auction_result_header());
  EXPECT_FALSE(test_client.received_ad_auction_signals_header());

  const std::set<std::string>& signals =
      GetAuctionSignalsForOrigin(url::Origin::Create(GURL("https://foo1.com")));
  EXPECT_THAT(signals, ::testing::UnorderedElementsAre("{}"));
}

// Tests that the Ad-Auction-Signals header will be removed from the final
// response regardless of whether it's an auction eligible request.
TEST_F(AdAuctionURLLoaderInterceptorTest,
       AdAuctionSignalsResponseHeaderRemoved) {
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

  std::string ad_auction_request_header_value;
  bool has_ad_auction_request_header =
      pending_request->request.headers.GetHeader(
          "Sec-Ad-Auction-Fetch", &ad_auction_request_header_value);
  EXPECT_FALSE(has_ad_auction_request_header);

  // The ad auction signals from the response header won't be stored in the
  // page.
  pending_request->client->OnReceiveResponse(
      CreateResponseHead(
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse,
          /*ad_auction_signals_header_value=*/"{}"),
      /*body=*/{}, absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_client.received_response());
  EXPECT_TRUE(test_client.received_ad_auction_result_header());
  EXPECT_FALSE(test_client.received_ad_auction_signals_header());

  const std::set<std::string>& signals =
      GetAuctionSignalsForOrigin(url::Origin::Create(GURL("https://foo1.com")));
  EXPECT_THAT(signals, ::testing::IsEmpty());
}

TEST_F(AdAuctionURLLoaderInterceptorTest,
       AdAuctionSignalsResponseHeaderTooLong) {
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

  std::string ad_auction_request_header_value;
  bool has_ad_auction_request_header =
      pending_request->request.headers.GetHeader(
          "Sec-Ad-Auction-Fetch", &ad_auction_request_header_value);
  EXPECT_TRUE(has_ad_auction_request_header);
  EXPECT_EQ(ad_auction_request_header_value, "?1");

  // The ad auction signals from the response header won't be stored in the
  // page.
  pending_request->client->OnReceiveResponse(
      CreateResponseHead(
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse,
          /*ad_auction_signals_header_value=*/std::string(10001, '0')),
      /*body=*/{}, absl::nullopt);
  base::RunLoop().RunUntilIdle();

  const std::set<std::string>& signals =
      GetAuctionSignalsForOrigin(url::Origin::Create(GURL("https://foo1.com")));
  EXPECT_THAT(signals, ::testing::IsEmpty());
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

  std::string ad_auction_request_header_value;
  bool has_ad_auction_request_header =
      pending_request->request.headers.GetHeader(
          "Sec-Ad-Auction-Fetch", &ad_auction_request_header_value);
  EXPECT_TRUE(has_ad_auction_request_header);
  EXPECT_EQ(ad_auction_request_header_value, "?1");

  // Redirect to `foo2.com`. The ad auction signals response for the initial
  // request to `foo1.com` will be ignored, and the redirect request to
  // `foo2.com` is not eligible for the ad auction headers.
  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL("https://foo2.com");

  pending_request->client->OnReceiveRedirect(
      redirect_info,
      CreateResponseHead(
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse,
          /*ad_auction_signals_header_value=*/"{}"));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(WitnessedAuctionResultForOrigin(
      url::Origin::Create(GURL("https://foo1.com")),
      base64Decode(kLegitimateAdAuctionResponse)));

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
          /*ad_auction_result_header_value=*/kLegitimateAdAuctionResponse,
          /*ad_auction_signals_header_value=*/"{}"),
      /*body=*/{}, absl::nullopt);
  base::RunLoop().RunUntilIdle();

  // The `Ad-Auction-Signals` header was ignored and not exposed to the original
  // loader client.
  EXPECT_TRUE(test_client.received_response());
  EXPECT_TRUE(test_client.received_ad_auction_result_header());
  EXPECT_FALSE(test_client.received_ad_auction_signals_header());

  const std::set<std::string>& signals =
      GetAuctionSignalsForOrigin(url::Origin::Create(GURL("https://foo1.com")));
  EXPECT_THAT(signals, ::testing::IsEmpty());
}

TEST_F(AdAuctionURLLoaderInterceptorTest, AdditionalBid) {
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

  std::string ad_auction_request_header_value;
  bool has_ad_auction_request_header =
      pending_request->request.headers.GetHeader(
          "Sec-Ad-Auction-Fetch", &ad_auction_request_header_value);
  EXPECT_TRUE(has_ad_auction_request_header);
  EXPECT_EQ(ad_auction_request_header_value, "?1");

  pending_request->client->OnReceiveResponse(
      CreateResponseHeadWithAdditionalBids(
          {"00000000-0000-0000-0000-000000000000:e30="}),
      /*body=*/{}, absl::nullopt);
  base::RunLoop().RunUntilIdle();

  // The `Ad-Auction-Additional-Bid` header was intercepted and stored in the
  // browser. It was not exposed to the original loader client.
  EXPECT_TRUE(test_client.received_response());
  EXPECT_FALSE(test_client.received_ad_auction_additional_bid_header());

  url::Origin request_origin = url::Origin::Create(GURL("https://foo1.com"));
  EXPECT_THAT(TakeAuctionAdditionalBidsForOriginAndNonce(
                  request_origin, "00000000-0000-0000-0000-000000000000"),
              ::testing::ElementsAre("e30="));
}

TEST_F(AdAuctionURLLoaderInterceptorTest,
       AdditionalBid_MultipleNoncesAndMultipleBidsPerNonce) {
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

  std::string ad_auction_request_header_value;
  bool has_ad_auction_request_header =
      pending_request->request.headers.GetHeader(
          "Sec-Ad-Auction-Fetch", &ad_auction_request_header_value);
  EXPECT_TRUE(has_ad_auction_request_header);
  EXPECT_EQ(ad_auction_request_header_value, "?1");

  pending_request->client->OnReceiveResponse(
      CreateResponseHeadWithAdditionalBids(
          {"00000000-0000-0000-0000-000000000000:e30=",
           "00000000-0000-0000-0000-000000000001:e30=",
           "00000000-0000-0000-0000-000000000001:e2E6IDF9"}),
      /*body=*/{}, absl::nullopt);
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

  std::string ad_auction_request_header_value;
  bool has_ad_auction_request_header =
      pending_request->request.headers.GetHeader(
          "Sec-Ad-Auction-Fetch", &ad_auction_request_header_value);
  EXPECT_FALSE(has_ad_auction_request_header);

  pending_request->client->OnReceiveResponse(
      CreateResponseHeadWithAdditionalBids(
          {"00000000-0000-0000-0000-000000000000:e30="}),
      /*body=*/{}, absl::nullopt);
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

TEST_F(AdAuctionURLLoaderInterceptorTest, AdditionalBid_InvalidHeaderSkipped) {
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

  std::string ad_auction_request_header_value;
  bool has_ad_auction_request_header =
      pending_request->request.headers.GetHeader(
          "Sec-Ad-Auction-Fetch", &ad_auction_request_header_value);
  EXPECT_TRUE(has_ad_auction_request_header);
  EXPECT_EQ(ad_auction_request_header_value, "?1");

  // Entries with invalid nonce (i.e. doesn't have 36 characters) will be
  // skipped.
  pending_request->client->OnReceiveResponse(
      CreateResponseHeadWithAdditionalBids(
          {"00000000-0000-0000-0000-00000000000:e30=",
           "00000000-0000-0000-0000-0000000000001:e30=",
           "00000000-0000-0000-0000-000000000001:e2E6IDF9"}),
      /*body=*/{}, absl::nullopt);
  base::RunLoop().RunUntilIdle();

  // The `Ad-Auction-Additional-Bid` header was intercepted and stored in the
  // browser. It was not exposed to the original loader client.
  EXPECT_TRUE(test_client.received_response());
  EXPECT_FALSE(test_client.received_ad_auction_additional_bid_header());

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

  std::string ad_auction_request_header_value;
  bool has_ad_auction_request_header =
      pending_request->request.headers.GetHeader(
          "Sec-Ad-Auction-Fetch", &ad_auction_request_header_value);
  EXPECT_TRUE(has_ad_auction_request_header);
  EXPECT_EQ(ad_auction_request_header_value, "?1");

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
      CreateResponseHeadWithAdditionalBids(
          {"00000000-0000-0000-0000-000000000000:e30="}),
      /*body=*/{}, absl::nullopt);
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
