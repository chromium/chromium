// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/loader/subresource_proxying_url_loader_service.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/back_forward_cache_util.h"
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
#include "third_party/blink/public/mojom/browsing_topics/browsing_topics.mojom.h"

namespace content {

namespace {

using FollowRedirectParams =
    network::TestURLLoaderFactory::TestURLLoader::FollowRedirectParams;

constexpr char kExpectedHeaderForEmptyTopics[] =
    "();p=P0000000000000000000000000000000";

constexpr char kExpectedHeaderForOrigin1[] =
    "(1);v=chrome.1:1:2, ();p=P00000000000";

constexpr char kExpectedHeaderForOrigin2[] =
    "(2);v=chrome.1:1:2, ();p=P00000000000";

class TopicsInterceptingContentBrowserClient : public ContentBrowserClient {
 public:
  bool HandleTopicsWebApi(
      const url::Origin& context_origin,
      content::RenderFrameHost* main_frame,
      browsing_topics::ApiCallerSource caller_source,
      bool get_topics,
      bool observe,
      std::vector<blink::mojom::EpochTopicPtr>& topics) override {
    ++handle_topics_web_api_count_;
    last_get_topics_param_ = get_topics;
    last_observe_param_ = observe;

    if (!topics_eligible_) {
      return false;
    }

    if (get_topics) {
      if (context_origin == url::Origin::Create(GURL("https://foo1.com"))) {
        blink::mojom::EpochTopicPtr result_topic =
            blink::mojom::EpochTopic::New();
        result_topic->topic = 1;
        result_topic->config_version = "chrome.1";
        result_topic->taxonomy_version = "1";
        result_topic->model_version = "2";
        result_topic->version = "chrome.1:1:2";
        topics.push_back(std::move(result_topic));
      } else if (context_origin ==
                 url::Origin::Create(GURL("https://foo2.com"))) {
        blink::mojom::EpochTopicPtr result_topic =
            blink::mojom::EpochTopic::New();
        result_topic->topic = 2;
        result_topic->config_version = "chrome.1";
        result_topic->taxonomy_version = "1";
        result_topic->model_version = "2";
        result_topic->version = "chrome.1:1:2";
        topics.push_back(std::move(result_topic));
      }
    }

    return true;
  }

  int NumVersionsInTopicsEpochs(
      content::RenderFrameHost* main_frame) const override {
    return 1;
  }

  size_t handle_topics_web_api_count() const {
    return handle_topics_web_api_count_;
  }

  void set_topics_eligible(bool eligible) { topics_eligible_ = eligible; }

  bool last_get_topics_param() const { return last_get_topics_param_; }

  bool last_observe_param() const { return last_observe_param_; }

 private:
  size_t handle_topics_web_api_count_ = 0;
  bool last_get_topics_param_ = false;
  bool last_observe_param_ = false;
  bool topics_eligible_ = true;
};

}  // namespace

class BrowsingTopicsURLLoaderTest : public RenderViewHostTestHarness {
 public:
  BrowsingTopicsURLLoaderTest() {
    scoped_feature_list_.InitAndEnableFeature(blink::features::kBrowsingTopics);
  }

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    original_client_ = content::SetBrowserClientForTesting(&browser_client_);
  }

  void TearDown() override {
    SetBrowserClientForTesting(original_client_);

    subresource_proxying_url_loader_service_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  TopicsInterceptingContentBrowserClient& browser_client() {
    return browser_client_;
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
      bool parsed_header_value) {
    auto head = network::mojom::URLResponseHead::New();

    network::mojom::ParsedHeadersPtr parsed_headers =
        network::mojom::ParsedHeaders::New();
    parsed_headers->observe_browsing_topics = parsed_header_value;
    head->parsed_headers = std::move(parsed_headers);

    return head;
  }

  network::ResourceRequest CreateResourceRequest(const GURL& url,
                                                 bool browsing_topics = true) {
    network::ResourceRequest request;
    request.url = url;
    request.browsing_topics = browsing_topics;
    return request;
  }

  void NavigatePage(const GURL& url) {
    auto simulator =
        NavigationSimulator::CreateBrowserInitiated(url, web_contents());

    blink::ParsedPermissionsPolicy policy;
    policy.emplace_back(
        blink::mojom::PermissionsPolicyFeature::kBrowsingTopics,
        /*allowed_origins=*/
        std::vector{*blink::OriginWithPossibleWildcards::FromOrigin(
                        url::Origin::Create(GURL("https://google.com"))),
                    *blink::OriginWithPossibleWildcards::FromOrigin(
                        url::Origin::Create(GURL("https://foo1.com"))),
                    *blink::OriginWithPossibleWildcards::FromOrigin(
                        url::Origin::Create(GURL("https://foo2.com"))),
                    *blink::OriginWithPossibleWildcards::FromOrigin(
                        url::Origin::Create(GURL("https://foo3.com")))},
        /*self_if_matches=*/std::nullopt,
        /*matches_all_origins=*/false,
        /*matches_opaque_src=*/false);

    simulator->SetPermissionsPolicyHeader(std::move(policy));

    simulator->Commit();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  TopicsInterceptingContentBrowserClient browser_client_;
  raw_ptr<ContentBrowserClient> original_client_ = nullptr;

  std::unique_ptr<SubresourceProxyingURLLoaderService>
      subresource_proxying_url_loader_service_;
};

TEST_F(BrowsingTopicsURLLoaderTest, RequestArrivedBeforeCommit) {
  base::HistogramTester histograms;

  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory;
  mojo::Remote<network::mojom::URLLoader> remote_loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client;

  CreateFactory(proxied_url_loader_factory, remote_url_loader_factory);

  // This request arrives before commit. It is thus not eligible for topics.
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

  EXPECT_EQ(pending_request->request.headers.GetHeader("Sec-Browsing-Topics"),
            std::nullopt);

  pending_request->client->OnReceiveResponse(CreateResponseHead(true),
                                             /*body=*/{}, std::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 0u);

  histograms.ExpectUniqueSample("BrowsingTopics.Fetch.InitialUrlRequest.Result",
                                2 /* kNoInitiatorFrame */, 1);
}

TEST_F(BrowsingTopicsURLLoaderTest, RequestArrivedAfterCommit) {
  base::HistogramTester histograms;

  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory;
  mojo::Remote<network::mojom::URLLoader> remote_loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client;

  base::WeakPtr<SubresourceProxyingURLLoaderService::BindContext> bind_context =
      CreateFactory(proxied_url_loader_factory, remote_url_loader_factory);
  bind_context->OnDidCommitNavigation(
      web_contents()->GetPrimaryMainFrame()->GetWeakDocumentPtr());

  // The request to `foo1.com` will cause the topics header value
  // `kExpectedHeaderForOrigin1` to be added.
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

  EXPECT_THAT(pending_request->request.headers.GetHeader("Sec-Browsing-Topics"),
              testing::Optional(std::string(kExpectedHeaderForOrigin1)));

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 1u);

  // The true topics response header value will cause an observation to be
  // recorded.
  pending_request->client->OnReceiveResponse(CreateResponseHead(true),
                                             /*body=*/{}, std::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 2u);
  EXPECT_FALSE(browser_client().last_get_topics_param());
  EXPECT_TRUE(browser_client().last_observe_param());

  histograms.ExpectUniqueSample("BrowsingTopics.Fetch.InitialUrlRequest.Result",
                                0 /* kSuccess */, 1);
}

TEST_F(BrowsingTopicsURLLoaderTest, RequestArrivedAfterDocumentDestroyed) {
  // The test assumes that the page gets deleted after navigation. Disable
  // back/forward cache to ensure that pages don't get preserved in the cache.
  DisableBackForwardCacheForTesting(web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  base::HistogramTester histograms;

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
  // factory to be destroyed. Thus the request won't be eligible for topics.
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

  EXPECT_EQ(pending_request->request.headers.GetHeader("Sec-Browsing-Topics"),
            std::nullopt);

  pending_request->client->OnReceiveResponse(CreateResponseHead(true),
                                             /*body=*/{}, std::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 0u);

  histograms.ExpectUniqueSample("BrowsingTopics.Fetch.InitialUrlRequest.Result",
                                2 /* kNoInitiatorFrame */, 1);
}

TEST_F(BrowsingTopicsURLLoaderTest, RequestFromSubframe) {
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

  // The request to `foo1.com` will cause the topics header value
  // `kExpectedHeaderForOrigin1` to be added.
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

  EXPECT_THAT(pending_request->request.headers.GetHeader("Sec-Browsing-Topics"),
              testing::Optional(std::string(kExpectedHeaderForOrigin1)));

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 1u);

  // The true topics response header value will cause an observation to be
  // recorded.
  pending_request->client->OnReceiveResponse(CreateResponseHead(true),
                                             /*body=*/{}, std::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 2u);
  EXPECT_FALSE(browser_client().last_get_topics_param());
  EXPECT_TRUE(browser_client().last_observe_param());
}

TEST_F(BrowsingTopicsURLLoaderTest, HasFalseValueObserveResponseHeader) {
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

  EXPECT_THAT(pending_request->request.headers.GetHeader("Sec-Browsing-Topics"),
              testing::Optional(std::string(kExpectedHeaderForOrigin1)));

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 1u);

  // Expect no further handling for topics as the response header value is
  // false.
  pending_request->client->OnReceiveResponse(CreateResponseHead(false),
                                             /*body=*/{}, std::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 1u);
}

TEST_F(BrowsingTopicsURLLoaderTest, EmptyTopics) {
  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory;
  mojo::Remote<network::mojom::URLLoader> remote_loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client;

  base::WeakPtr<SubresourceProxyingURLLoaderService::BindContext> bind_context =
      CreateFactory(proxied_url_loader_factory, remote_url_loader_factory);
  bind_context->OnDidCommitNavigation(
      web_contents()->GetPrimaryMainFrame()->GetWeakDocumentPtr());

  // The request to `foo3.com` will cause an empty topics header value to be
  // added.
  remote_url_loader_factory->CreateLoaderAndStart(
      remote_loader.BindNewPipeAndPassReceiver(),
      /*request_id=*/0, /*options=*/0,
      CreateResourceRequest(GURL("https://foo3.com")),
      client.InitWithNewPipeAndPassRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  remote_url_loader_factory.FlushForTesting();

  EXPECT_EQ(1, proxied_url_loader_factory.NumPending());
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      &proxied_url_loader_factory.pending_requests()->back();

  EXPECT_THAT(pending_request->request.headers.GetHeader("Sec-Browsing-Topics"),
              testing::Optional(std::string(kExpectedHeaderForEmptyTopics)));

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 1u);

  // The true topics response header value will cause an observation to be
  // recorded.
  pending_request->client->OnReceiveResponse(CreateResponseHead(true),
                                             /*body=*/{}, std::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 2u);
  EXPECT_FALSE(browser_client().last_get_topics_param());
  EXPECT_TRUE(browser_client().last_observe_param());
}

TEST_F(BrowsingTopicsURLLoaderTest, TopicsNotEligibleDueToFromFencedFrame) {
  base::HistogramTester histograms;

  NavigatePage(GURL("https://google.com"));

  RenderFrameHost* initial_fenced_frame =
      RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
          ->AppendFencedFrame();

  auto fenced_frame_navigation = NavigationSimulator::CreateRendererInitiated(
      GURL("https://google.com"), initial_fenced_frame);

  fenced_frame_navigation->Commit();

  RenderFrameHost* final_fenced_frame =
      fenced_frame_navigation->GetFinalRenderFrameHost();

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory;
  mojo::Remote<network::mojom::URLLoader> remote_loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client;

  base::WeakPtr<SubresourceProxyingURLLoaderService::BindContext> bind_context =
      CreateFactory(proxied_url_loader_factory, remote_url_loader_factory);
  bind_context->OnDidCommitNavigation(final_fenced_frame->GetWeakDocumentPtr());

  // The request won't be eligible for topics because it's from a fenced frame.
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

  EXPECT_EQ(pending_request->request.headers.GetHeader("Sec-Browsing-Topics"),
            std::nullopt);

  pending_request->client->OnReceiveResponse(CreateResponseHead(true),
                                             /*body=*/{}, std::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 0u);

  histograms.ExpectUniqueSample("BrowsingTopics.Fetch.InitialUrlRequest.Result",
                                3 /* kFromFencedFrame */, 1);
}

TEST_F(BrowsingTopicsURLLoaderTest, TopicsNotEligibleDueToInactiveFrame) {
  base::HistogramTester histograms;

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
  // topics.
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

  EXPECT_EQ(pending_request->request.headers.GetHeader("Sec-Browsing-Topics"),
            std::nullopt);

  pending_request->client->OnReceiveResponse(CreateResponseHead(true),
                                             /*body=*/{}, std::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 0u);

  histograms.ExpectUniqueSample("BrowsingTopics.Fetch.InitialUrlRequest.Result",
                                4 /* kFromNonPrimaryPage */, 1);
}

TEST_F(BrowsingTopicsURLLoaderTest, OpaqueRequestURL) {
  base::HistogramTester histograms;

  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory;
  mojo::Remote<network::mojom::URLLoader> remote_loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client;

  base::WeakPtr<SubresourceProxyingURLLoaderService::BindContext> bind_context =
      CreateFactory(proxied_url_loader_factory, remote_url_loader_factory);
  bind_context->OnDidCommitNavigation(
      web_contents()->GetPrimaryMainFrame()->GetWeakDocumentPtr());

  // Fetch an opaque url. The request won't be eligible for topics.
  remote_url_loader_factory->CreateLoaderAndStart(
      remote_loader.BindNewPipeAndPassReceiver(),
      /*request_id=*/0, /*options=*/0,
      CreateResourceRequest(GURL("data:text/javascript;base64,Ly8gSGVsbG8h")),
      client.InitWithNewPipeAndPassRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  remote_url_loader_factory.FlushForTesting();

  EXPECT_EQ(1, proxied_url_loader_factory.NumPending());
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      &proxied_url_loader_factory.pending_requests()->back();

  EXPECT_EQ(pending_request->request.headers.GetHeader("Sec-Browsing-Topics"),
            std::nullopt);

  pending_request->client->OnReceiveResponse(CreateResponseHead(true),
                                             /*body=*/{}, std::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 0u);

  histograms.ExpectUniqueSample("BrowsingTopics.Fetch.InitialUrlRequest.Result",
                                5 /* kOpaqueCallerOrigin */, 1);
}

TEST_F(BrowsingTopicsURLLoaderTest, TopicsNotEligibleDueToPermissionsPolicy) {
  base::HistogramTester histograms;

  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory;
  mojo::Remote<network::mojom::URLLoader> remote_loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client;

  base::WeakPtr<SubresourceProxyingURLLoaderService::BindContext> bind_context =
      CreateFactory(proxied_url_loader_factory, remote_url_loader_factory);
  bind_context->OnDidCommitNavigation(
      web_contents()->GetPrimaryMainFrame()->GetWeakDocumentPtr());

  // The permissions policy disallows `foo4.com`. The request won't be eligible
  // for topics.
  remote_url_loader_factory->CreateLoaderAndStart(
      remote_loader.BindNewPipeAndPassReceiver(),
      /*request_id=*/0, /*options=*/0,
      CreateResourceRequest(GURL("https://foo4.com")),
      client.InitWithNewPipeAndPassRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  remote_url_loader_factory.FlushForTesting();

  EXPECT_EQ(1, proxied_url_loader_factory.NumPending());
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      &proxied_url_loader_factory.pending_requests()->back();

  EXPECT_EQ(pending_request->request.headers.GetHeader("Sec-Browsing-Topics"),
            std::nullopt);

  pending_request->client->OnReceiveResponse(CreateResponseHead(true),
                                             /*body=*/{}, std::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 0u);

  histograms.ExpectUniqueSample("BrowsingTopics.Fetch.InitialUrlRequest.Result",
                                7 /* kDisallowedByPermissionsPolicy */, 1);
}

TEST_F(BrowsingTopicsURLLoaderTest,
       TopicsNotEligibleDueToContentClientSettings) {
  base::HistogramTester histograms;

  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory;
  mojo::Remote<network::mojom::URLLoader> remote_loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client;

  base::WeakPtr<SubresourceProxyingURLLoaderService::BindContext> bind_context =
      CreateFactory(proxied_url_loader_factory, remote_url_loader_factory);
  bind_context->OnDidCommitNavigation(
      web_contents()->GetPrimaryMainFrame()->GetWeakDocumentPtr());

  browser_client().set_topics_eligible(false);

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

  // When the request is ineligible for topics due to user settings, an empty
  // list of topics will be sent in the header.
  EXPECT_THAT(pending_request->request.headers.GetHeader("Sec-Browsing-Topics"),
              testing::Optional(std::string(kExpectedHeaderForEmptyTopics)));

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 1u);

  // The response header won't be handled even after re-enabling the settings,
  // because the request wasn't eligible for topics.
  browser_client().set_topics_eligible(true);
  pending_request->client->OnReceiveResponse(CreateResponseHead(true),
                                             /*body=*/{}, std::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 1u);

  histograms.ExpectUniqueSample("BrowsingTopics.Fetch.InitialUrlRequest.Result",
                                1 /* kDisallowedByContentClient */, 1);
}

TEST_F(BrowsingTopicsURLLoaderTest, RedirectTopicsUpdated) {
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

  // The request to `foo1.com` will cause the topics header value
  // `kExpectedHeaderForOrigin1` to be added.
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

  EXPECT_THAT(pending_request->request.headers.GetHeader("Sec-Browsing-Topics"),
              testing::Optional(std::string(kExpectedHeaderForOrigin1)));

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 1u);

  // Redirect to `foo2.com` will cause the topics header to be updated to
  // `kExpectedHeaderForOrigin2` for the redirect request.
  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL("https://foo2.com");

  // The true topics response header value for the initial request will cause an
  // observation to be recorded.
  pending_request->client->OnReceiveRedirect(redirect_info,
                                             CreateResponseHead(true));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 2u);
  EXPECT_FALSE(browser_client().last_get_topics_param());
  EXPECT_TRUE(browser_client().last_observe_param());

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
            "Sec-Browsing-Topics");

  EXPECT_THAT(follow_redirect_params[0].modified_headers.GetHeader(
                  "Sec-Browsing-Topics"),
              testing::Optional(std::string(kExpectedHeaderForOrigin2)));

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 3u);

  // The true topics response header value will cause an observation to be
  // recorded.
  pending_request->client->OnReceiveResponse(CreateResponseHead(true),
                                             /*body=*/{}, std::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 4u);
  EXPECT_FALSE(browser_client().last_get_topics_param());
  EXPECT_TRUE(browser_client().last_observe_param());
}

TEST_F(BrowsingTopicsURLLoaderTest, RedirectNotEligibleForTopics) {
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

  // The request to `foo1.com` will cause the topics header value
  // `kExpectedHeaderForOrigin1` to be added.
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

  EXPECT_THAT(pending_request->request.headers.GetHeader("Sec-Browsing-Topics"),
              testing::Optional(std::string(kExpectedHeaderForOrigin1)));

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 1u);

  // The permissions policy disallows `foo4.com`. The redirect is thus not
  // eligible for topics.
  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL("https://foo4.com");

  // The true topics response header value for the initial request will cause an
  // observation to be recorded.
  pending_request->client->OnReceiveRedirect(redirect_info,
                                             CreateResponseHead(true));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 2u);
  EXPECT_FALSE(browser_client().last_get_topics_param());
  EXPECT_TRUE(browser_client().last_observe_param());

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
            "Sec-Browsing-Topics");

  EXPECT_EQ(follow_redirect_params[0].modified_headers.GetHeader(
                "Sec-Browsing-Topics"),
            std::nullopt);

  pending_request->client->OnReceiveResponse(CreateResponseHead(true),
                                             /*body=*/{}, std::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 2u);
}

TEST_F(BrowsingTopicsURLLoaderTest, TwoRequests) {
  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory;

  mojo::Remote<network::mojom::URLLoader> remote_loader1;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client1;

  mojo::Remote<network::mojom::URLLoader> remote_loader2;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client2;

  base::WeakPtr<SubresourceProxyingURLLoaderService::BindContext> bind_context =
      CreateFactory(proxied_url_loader_factory, remote_url_loader_factory);
  bind_context->OnDidCommitNavigation(
      web_contents()->GetPrimaryMainFrame()->GetWeakDocumentPtr());

  remote_url_loader_factory->CreateLoaderAndStart(
      remote_loader1.BindNewPipeAndPassReceiver(),
      /*request_id=*/1, /*options=*/0,
      CreateResourceRequest(GURL("https://foo1.com")),
      client1.InitWithNewPipeAndPassRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  remote_url_loader_factory.FlushForTesting();

  EXPECT_EQ(1, proxied_url_loader_factory.NumPending());
  network::TestURLLoaderFactory::PendingRequest* pending_request1 =
      &proxied_url_loader_factory.pending_requests()->back();

  EXPECT_THAT(
      pending_request1->request.headers.GetHeader("Sec-Browsing-Topics"),
      testing::Optional(std::string(kExpectedHeaderForOrigin1)));

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 1u);

  pending_request1->client->OnReceiveResponse(CreateResponseHead(true),
                                              /*body=*/{}, std::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 2u);
  EXPECT_FALSE(browser_client().last_get_topics_param());
  EXPECT_TRUE(browser_client().last_observe_param());

  remote_url_loader_factory->CreateLoaderAndStart(
      remote_loader2.BindNewPipeAndPassReceiver(),
      /*request_id=*/2, /*options=*/0,
      CreateResourceRequest(GURL("https://foo4.com")),
      client2.InitWithNewPipeAndPassRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  remote_url_loader_factory.FlushForTesting();

  EXPECT_EQ(2, proxied_url_loader_factory.NumPending());
  network::TestURLLoaderFactory::PendingRequest* pending_request2 =
      &proxied_url_loader_factory.pending_requests()->back();

  EXPECT_TRUE(remote_url_loader_factory.is_connected());

  // Topics not eligible due to permissions policy.
  EXPECT_EQ(pending_request2->request.headers.GetHeader("Sec-Browsing-Topics"),
            std::nullopt);

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 2u);

  pending_request2->client->OnReceiveResponse(CreateResponseHead(true),
                                              /*body=*/{}, std::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 2u);
}

TEST_F(BrowsingTopicsURLLoaderTest, TwoFactories) {
  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory1;
  network::TestURLLoaderFactory proxied_url_loader_factory1;
  mojo::Remote<network::mojom::URLLoader> remote_loader1;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client1;

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory2;
  network::TestURLLoaderFactory proxied_url_loader_factory2;
  mojo::Remote<network::mojom::URLLoader> remote_loader2;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client2;

  base::WeakPtr<SubresourceProxyingURLLoaderService::BindContext>
      bind_context1 = CreateFactory(proxied_url_loader_factory1,
                                    remote_url_loader_factory1);
  bind_context1->OnDidCommitNavigation(
      web_contents()->GetPrimaryMainFrame()->GetWeakDocumentPtr());

  base::WeakPtr<SubresourceProxyingURLLoaderService::BindContext>
      bind_context2 = CreateFactory(proxied_url_loader_factory2,
                                    remote_url_loader_factory2);
  bind_context2->OnDidCommitNavigation(
      web_contents()->GetPrimaryMainFrame()->GetWeakDocumentPtr());

  remote_url_loader_factory1->CreateLoaderAndStart(
      remote_loader1.BindNewPipeAndPassReceiver(),
      /*request_id=*/0, /*options=*/0,
      CreateResourceRequest(GURL("https://foo1.com")),
      client1.InitWithNewPipeAndPassRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  remote_url_loader_factory1.FlushForTesting();

  EXPECT_EQ(1, proxied_url_loader_factory1.NumPending());
  network::TestURLLoaderFactory::PendingRequest* pending_request1 =
      &proxied_url_loader_factory1.pending_requests()->back();

  EXPECT_THAT(
      pending_request1->request.headers.GetHeader("Sec-Browsing-Topics"),
      testing::Optional(std::string(kExpectedHeaderForOrigin1)));

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 1u);

  remote_url_loader_factory2->CreateLoaderAndStart(
      remote_loader2.BindNewPipeAndPassReceiver(),
      /*request_id=*/0, /*options=*/0,
      CreateResourceRequest(GURL("https://foo4.com")),
      client2.InitWithNewPipeAndPassRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  remote_url_loader_factory2.FlushForTesting();

  EXPECT_EQ(1, proxied_url_loader_factory2.NumPending());
  network::TestURLLoaderFactory::PendingRequest* pending_request2 =
      &proxied_url_loader_factory2.pending_requests()->back();

  // Topics not eligible due to permissions policy.
  EXPECT_EQ(pending_request2->request.headers.GetHeader("Sec-Browsing-Topics"),
            std::nullopt);

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 1u);

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 1u);

  pending_request1->client->OnReceiveResponse(CreateResponseHead(true),
                                              /*body=*/{}, std::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 2u);
  EXPECT_FALSE(browser_client().last_get_topics_param());
  EXPECT_TRUE(browser_client().last_observe_param());

  pending_request2->client->OnReceiveResponse(CreateResponseHead(true),
                                              /*body=*/{}, std::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 2u);
}

TEST_F(BrowsingTopicsURLLoaderTest, BindContextClearedDueToDisconnect) {
  NavigatePage(GURL("https://google.com"));

  base::WeakPtr<SubresourceProxyingURLLoaderService::BindContext> bind_context;

  {
    mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
    network::TestURLLoaderFactory proxied_url_loader_factory;

    bind_context =
        CreateFactory(proxied_url_loader_factory, remote_url_loader_factory);

    EXPECT_TRUE(bind_context);
  }

  // Destroying `remote_url_loader_factory` would reset `bind_context`.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(bind_context);
}

TEST_F(BrowsingTopicsURLLoaderTest, ReportBadMessageOnInvalidRequest) {
  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory;
  mojo::Remote<network::mojom::URLLoader> remote_loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client;

  base::WeakPtr<SubresourceProxyingURLLoaderService::BindContext> bind_context =
      CreateFactory(proxied_url_loader_factory, remote_url_loader_factory);
  bind_context->OnDidCommitNavigation(
      web_contents()->GetPrimaryMainFrame()->GetWeakDocumentPtr());

  std::string received_error;
  mojo::SetDefaultProcessErrorHandler(base::BindLambdaForTesting(
      [&](const std::string& error) { received_error = error; }));

  // Invoke CreateLoaderAndStart() with a ResourceRequest invalid for this
  // factory. This will trigger a ReportBadMessage.
  remote_url_loader_factory->CreateLoaderAndStart(
      remote_loader.BindNewPipeAndPassReceiver(),
      /*request_id=*/0, /*options=*/0,
      CreateResourceRequest(GURL("https://foo1.com"),
                            /*browsing_topics=*/false),
      client.InitWithNewPipeAndPassRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  remote_url_loader_factory.FlushForTesting();

  EXPECT_FALSE(remote_url_loader_factory.is_connected());
  EXPECT_EQ(0, proxied_url_loader_factory.NumPending());
  EXPECT_THAT(
      received_error,
      testing::HasSubstr(
          "Unexpected `resource_request_in` in "
          "SubresourceProxyingURLLoaderService::CreateLoaderAndStart()"));
  mojo::SetDefaultProcessErrorHandler(base::NullCallback());
}

}  // namespace content
