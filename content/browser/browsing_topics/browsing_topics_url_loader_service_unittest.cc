// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_topics/browsing_topics_url_loader_service.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "third_party/blink/public/mojom/browsing_topics/browsing_topics.mojom.h"

namespace content {

namespace {

using FollowRedirectParams =
    network::TestURLLoaderFactory::TestURLLoader::FollowRedirectParams;

constexpr char kExpectedHeaderForOrigin1[] =
    "1;version=\"chrome.1:1:2\";config_version=\"chrome.1\";model_version="
    "\"2\";taxonomy_version=\"1\"";

constexpr char kExpectedHeaderForOrigin2[] =
    "2;version=\"chrome.3:4:5\";config_version=\"chrome.3\";model_version="
    "\"5\";taxonomy_version=\"4\"";

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
        result_topic->config_version = "chrome.3";
        result_topic->taxonomy_version = "4";
        result_topic->model_version = "5";
        result_topic->version = "chrome.3:4:5";
        topics.push_back(std::move(result_topic));
      }
    }

    return true;
  }

  size_t handle_topics_web_api_count() const {
    return handle_topics_web_api_count_;
  }

  bool last_get_topics_param() const { return last_get_topics_param_; }

  bool last_observe_param() const { return last_observe_param_; }

 private:
  size_t handle_topics_web_api_count_ = 0;
  bool last_get_topics_param_ = false;
  bool last_observe_param_ = false;
};

}  // namespace

class BrowsingTopicsURLLoaderServiceTest : public RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    original_client_ = content::SetBrowserClientForTesting(&browser_client_);
  }

  void TearDown() override {
    SetBrowserClientForTesting(original_client_);

    content::RenderViewHostTestHarness::TearDown();
  }

  const TopicsInterceptingContentBrowserClient& browser_client() const {
    return browser_client_;
  }

  base::WeakPtr<BrowsingTopicsURLLoaderService::BindContext> CreateFactory(
      network::TestURLLoaderFactory& proxied_url_loader_factory,
      mojo::Remote<network::mojom::URLLoaderFactory>&
          remote_url_loader_factory) {
    if (!browsing_topics_url_loader_service_) {
      browsing_topics_url_loader_service_ =
          std::make_unique<BrowsingTopicsURLLoaderService>();
    }

    mojo::Remote<network::mojom::URLLoaderFactory> factory;
    proxied_url_loader_factory.Clone(factory.BindNewPipeAndPassReceiver());
    auto pending_factory =
        std::make_unique<network::WrapperPendingSharedURLLoaderFactory>(
            factory.Unbind());

    return browsing_topics_url_loader_service_->GetFactory(
        remote_url_loader_factory.BindNewPipeAndPassReceiver(),
        std::move(pending_factory));
  }

  network::mojom::URLResponseHeadPtr CreateResponseHead(
      absl::optional<std::string> topics_header_value) {
    auto head = network::mojom::URLResponseHead::New();
    head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");

    if (topics_header_value) {
      head->headers->AddHeader("Observe-Browsing-Topics",
                               topics_header_value.value());
    }

    return head;
  }

  network::ResourceRequest CreateResourceRequest(const GURL& url) {
    network::ResourceRequest request;
    request.url = url;
    return request;
  }

  void NavigatePage(const GURL& url) {
    auto simulator =
        NavigationSimulator::CreateBrowserInitiated(url, web_contents());

    blink::ParsedPermissionsPolicy policy;
    policy.emplace_back(blink::mojom::PermissionsPolicyFeature::kBrowsingTopics,
                        /*allowed_origins=*/
                        std::vector<blink::OriginWithPossibleWildcards>{
                            blink::OriginWithPossibleWildcards(
                                url::Origin::Create(GURL("https://google.com")),
                                /*has_subdomain_wildcard=*/false),
                            blink::OriginWithPossibleWildcards(
                                url::Origin::Create(GURL("https://foo1.com")),
                                /*has_subdomain_wildcard=*/false),
                            blink::OriginWithPossibleWildcards(
                                url::Origin::Create(GURL("https://foo2.com")),
                                /*has_subdomain_wildcard=*/false),
                            blink::OriginWithPossibleWildcards(
                                url::Origin::Create(GURL("https://foo3.com")),
                                /*has_subdomain_wildcard=*/false)},
                        /*matches_all_origins=*/false,
                        /*matches_opaque_src=*/false);

    simulator->SetPermissionsPolicyHeader(std::move(policy));

    simulator->Commit();
  }

 private:
  TopicsInterceptingContentBrowserClient browser_client_;
  raw_ptr<ContentBrowserClient> original_client_ = nullptr;

  std::unique_ptr<BrowsingTopicsURLLoaderService>
      browsing_topics_url_loader_service_;
};

TEST_F(BrowsingTopicsURLLoaderServiceTest, RequestArrivedBeforeCommit) {
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

  std::string topics_header_value;
  bool has_topics_header = pending_request->request.headers.GetHeader(
      "Sec-Browsing-Topics", &topics_header_value);
  EXPECT_FALSE(has_topics_header);

  pending_request->client->OnReceiveResponse(
      CreateResponseHead(/*topics_header_value=*/"?1"), /*body=*/{},
      absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 0u);
}

TEST_F(BrowsingTopicsURLLoaderServiceTest, RequestArrivedAfterCommit) {
  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory;
  mojo::Remote<network::mojom::URLLoader> remote_loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client;

  base::WeakPtr<BrowsingTopicsURLLoaderService::BindContext> bind_context =
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

  std::string topics_header_value;
  bool has_topics_header = pending_request->request.headers.GetHeader(
      "Sec-Browsing-Topics", &topics_header_value);
  EXPECT_TRUE(has_topics_header);
  EXPECT_EQ(topics_header_value, kExpectedHeaderForOrigin1);

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 1u);

  // The topics response header value "?1" will cause an observation to be
  // recorded.
  pending_request->client->OnReceiveResponse(
      CreateResponseHead(/*topics_header_value=*/"?1"), /*body=*/{},
      absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 2u);
  EXPECT_FALSE(browser_client().last_get_topics_param());
  EXPECT_TRUE(browser_client().last_observe_param());
}

TEST_F(BrowsingTopicsURLLoaderServiceTest,
       RequestArrivedAfterDocumentDestroyed) {
  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory;
  mojo::Remote<network::mojom::URLLoader> remote_loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client;

  base::WeakPtr<BrowsingTopicsURLLoaderService::BindContext> bind_context =
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

  std::string topics_header_value;
  bool has_topics_header = pending_request->request.headers.GetHeader(
      "Sec-Browsing-Topics", &topics_header_value);
  EXPECT_FALSE(has_topics_header);

  pending_request->client->OnReceiveResponse(
      CreateResponseHead(/*topics_header_value=*/"?1"), /*body=*/{},
      absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 0u);
}

TEST_F(BrowsingTopicsURLLoaderServiceTest, RequestFromSubframe) {
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

  base::WeakPtr<BrowsingTopicsURLLoaderService::BindContext> bind_context =
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

  std::string topics_header_value;
  bool has_topics_header = pending_request->request.headers.GetHeader(
      "Sec-Browsing-Topics", &topics_header_value);
  EXPECT_TRUE(has_topics_header);
  EXPECT_EQ(topics_header_value, kExpectedHeaderForOrigin1);

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 1u);

  // The topics response header value "?1" will cause an observation to be
  // recorded.
  pending_request->client->OnReceiveResponse(
      CreateResponseHead(/*topics_header_value=*/"?1"), /*body=*/{},
      absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 2u);
  EXPECT_FALSE(browser_client().last_get_topics_param());
  EXPECT_TRUE(browser_client().last_observe_param());
}

TEST_F(BrowsingTopicsURLLoaderServiceTest, HasNoObserveResponseHeader) {
  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory;
  mojo::Remote<network::mojom::URLLoader> remote_loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client;

  base::WeakPtr<BrowsingTopicsURLLoaderService::BindContext> bind_context =
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

  std::string topics_header_value;
  bool has_topics_header = pending_request->request.headers.GetHeader(
      "Sec-Browsing-Topics", &topics_header_value);
  EXPECT_TRUE(has_topics_header);
  EXPECT_EQ(topics_header_value, kExpectedHeaderForOrigin1);

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 1u);

  // Expect no further handling for topics as the response does not contain the
  // topics header.
  pending_request->client->OnReceiveResponse(
      CreateResponseHead(/*topics_header_value=*/absl::nullopt),
      /*body=*/{}, absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 1u);
}

TEST_F(BrowsingTopicsURLLoaderServiceTest, HasFalseValueObserveResponseHeader) {
  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory;
  mojo::Remote<network::mojom::URLLoader> remote_loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client;

  base::WeakPtr<BrowsingTopicsURLLoaderService::BindContext> bind_context =
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

  std::string topics_header_value;
  bool has_topics_header = pending_request->request.headers.GetHeader(
      "Sec-Browsing-Topics", &topics_header_value);
  EXPECT_TRUE(has_topics_header);
  EXPECT_EQ(topics_header_value, kExpectedHeaderForOrigin1);

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 1u);

  // Expect no further handling for topics as the response header value is "?0"
  // (i.e. false).
  pending_request->client->OnReceiveResponse(
      CreateResponseHead(/*topics_header_value=*/"?0"),
      /*body=*/{}, absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 1u);
}

TEST_F(BrowsingTopicsURLLoaderServiceTest, HasMalformedObserveResponseHeader) {
  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory;
  mojo::Remote<network::mojom::URLLoader> remote_loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client;

  base::WeakPtr<BrowsingTopicsURLLoaderService::BindContext> bind_context =
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

  std::string topics_header_value;
  bool has_topics_header = pending_request->request.headers.GetHeader(
      "Sec-Browsing-Topics", &topics_header_value);
  EXPECT_TRUE(has_topics_header);
  EXPECT_EQ(topics_header_value, kExpectedHeaderForOrigin1);

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 1u);

  // Expect no further handling for topics as the response header value is
  // malformed.
  pending_request->client->OnReceiveResponse(
      CreateResponseHead(/*topics_header_value=*/"?1, ?0"),
      /*body=*/{}, absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 1u);
}

TEST_F(BrowsingTopicsURLLoaderServiceTest, EmptyTopics) {
  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory;
  mojo::Remote<network::mojom::URLLoader> remote_loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client;

  base::WeakPtr<BrowsingTopicsURLLoaderService::BindContext> bind_context =
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

  std::string topics_header_value;
  bool has_topics_header = pending_request->request.headers.GetHeader(
      "Sec-Browsing-Topics", &topics_header_value);
  EXPECT_TRUE(has_topics_header);
  EXPECT_TRUE(topics_header_value.empty());

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 1u);

  // The topics response header value "?1" will cause an observation to be
  // recorded.
  pending_request->client->OnReceiveResponse(
      CreateResponseHead(/*topics_header_value=*/"?1"), /*body=*/{},
      absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 2u);
  EXPECT_FALSE(browser_client().last_get_topics_param());
  EXPECT_TRUE(browser_client().last_observe_param());
}

TEST_F(BrowsingTopicsURLLoaderServiceTest,
       TopicsNotEligibleDueToInactiveFrame) {
  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory;
  mojo::Remote<network::mojom::URLLoader> remote_loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client;

  base::WeakPtr<BrowsingTopicsURLLoaderService::BindContext> bind_context =
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

  std::string topics_header_value;
  bool has_topics_header = pending_request->request.headers.GetHeader(
      "Sec-Browsing-Topics", &topics_header_value);
  EXPECT_FALSE(has_topics_header);

  pending_request->client->OnReceiveResponse(
      CreateResponseHead(/*topics_header_value=*/"?1"), /*body=*/{},
      absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 0u);
}

TEST_F(BrowsingTopicsURLLoaderServiceTest,
       TopicsNotEligibleDueToPermissionsPolicy) {
  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory;
  mojo::Remote<network::mojom::URLLoader> remote_loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client;

  base::WeakPtr<BrowsingTopicsURLLoaderService::BindContext> bind_context =
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

  std::string topics_header_value;
  bool has_topics_header = pending_request->request.headers.GetHeader(
      "Sec-Browsing-Topics", &topics_header_value);
  EXPECT_FALSE(has_topics_header);

  pending_request->client->OnReceiveResponse(
      CreateResponseHead(/*topics_header_value=*/"?1"), /*body=*/{},
      absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 0u);
}

TEST_F(BrowsingTopicsURLLoaderServiceTest, RedirectTopicsUpdated) {
  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory(
      /*observe_loader_requests=*/true);
  mojo::Remote<network::mojom::URLLoader> remote_loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client;

  base::WeakPtr<BrowsingTopicsURLLoaderService::BindContext> bind_context =
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

  std::string topics_header_value;
  bool has_topics_header = pending_request->request.headers.GetHeader(
      "Sec-Browsing-Topics", &topics_header_value);
  EXPECT_TRUE(has_topics_header);
  EXPECT_EQ(topics_header_value, kExpectedHeaderForOrigin1);

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 1u);

  // Redirect to `foo2.com` will cause the topics header to be updated to
  // `kExpectedHeaderForOrigin2` for the redirect request.
  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL("https://foo2.com");

  // The topics response header value "?1" for the initial request will cause an
  // observation to be recorded.
  pending_request->client->OnReceiveRedirect(
      redirect_info, CreateResponseHead(/*topics_header_value=*/"?1"));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 2u);
  EXPECT_FALSE(browser_client().last_get_topics_param());
  EXPECT_TRUE(browser_client().last_observe_param());

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
            "Sec-Browsing-Topics");

  std::string redirect_topics_header_value;
  bool redirect_has_topics_header =
      follow_redirect_params[0].modified_headers.GetHeader(
          "Sec-Browsing-Topics", &redirect_topics_header_value);
  EXPECT_TRUE(redirect_has_topics_header);
  EXPECT_EQ(redirect_topics_header_value, kExpectedHeaderForOrigin2);

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 3u);

  // The topics response header value "?1" will cause an observation to be
  // recorded.
  pending_request->client->OnReceiveResponse(
      CreateResponseHead(/*topics_header_value=*/"?1"), /*body=*/{},
      absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 4u);
  EXPECT_FALSE(browser_client().last_get_topics_param());
  EXPECT_TRUE(browser_client().last_observe_param());
}

TEST_F(BrowsingTopicsURLLoaderServiceTest, RedirectNotEligibleForTopics) {
  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory(
      /*observe_loader_requests=*/true);
  mojo::Remote<network::mojom::URLLoader> remote_loader;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client;

  base::WeakPtr<BrowsingTopicsURLLoaderService::BindContext> bind_context =
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

  std::string topics_header_value;
  bool has_topics_header = pending_request->request.headers.GetHeader(
      "Sec-Browsing-Topics", &topics_header_value);
  EXPECT_TRUE(has_topics_header);
  EXPECT_EQ(topics_header_value, kExpectedHeaderForOrigin1);

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 1u);

  // The permissions policy disallows `foo4.com`. The redirect is thus not
  // eligible for topics.
  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL("https://foo4.com");

  // The topics response header value "?1" for the initial request will cause an
  // observation to be recorded.
  pending_request->client->OnReceiveRedirect(
      redirect_info, CreateResponseHead(/*topics_header_value=*/"?1"));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 2u);
  EXPECT_FALSE(browser_client().last_get_topics_param());
  EXPECT_TRUE(browser_client().last_observe_param());

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
            "Sec-Browsing-Topics");

  std::string redirect_topics_header_value;
  bool redirect_has_topics_header =
      follow_redirect_params[0].modified_headers.GetHeader(
          "Sec-Browsing-Topics", &redirect_topics_header_value);
  EXPECT_FALSE(redirect_has_topics_header);

  pending_request->client->OnReceiveResponse(
      CreateResponseHead(/*topics_header_value=*/"?1"), /*body=*/{},
      absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 2u);
}

TEST_F(BrowsingTopicsURLLoaderServiceTest, TwoRequests) {
  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  network::TestURLLoaderFactory proxied_url_loader_factory;

  mojo::Remote<network::mojom::URLLoader> remote_loader1;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client1;

  mojo::Remote<network::mojom::URLLoader> remote_loader2;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client2;

  base::WeakPtr<BrowsingTopicsURLLoaderService::BindContext> bind_context =
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

  {
    std::string topics_header_value;
    bool has_topics_header = pending_request1->request.headers.GetHeader(
        "Sec-Browsing-Topics", &topics_header_value);
    EXPECT_TRUE(has_topics_header);
    EXPECT_EQ(topics_header_value, kExpectedHeaderForOrigin1);

    EXPECT_EQ(browser_client().handle_topics_web_api_count(), 1u);
  }

  pending_request1->client->OnReceiveResponse(
      CreateResponseHead(/*topics_header_value=*/"?1"), /*body=*/{},
      absl::nullopt);
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

  {
    std::string topics_header_value;
    bool has_topics_header = pending_request2->request.headers.GetHeader(
        "Sec-Browsing-Topics", &topics_header_value);

    // Topics not eligible due to permissions policy.
    EXPECT_FALSE(has_topics_header);

    EXPECT_EQ(browser_client().handle_topics_web_api_count(), 2u);
  }

  pending_request2->client->OnReceiveResponse(
      CreateResponseHead(/*topics_header_value=*/"?1"), /*body=*/{},
      absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 2u);
}

TEST_F(BrowsingTopicsURLLoaderServiceTest, TwoFactories) {
  NavigatePage(GURL("https://google.com"));

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory1;
  network::TestURLLoaderFactory proxied_url_loader_factory1;
  mojo::Remote<network::mojom::URLLoader> remote_loader1;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client1;

  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory2;
  network::TestURLLoaderFactory proxied_url_loader_factory2;
  mojo::Remote<network::mojom::URLLoader> remote_loader2;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client2;

  base::WeakPtr<BrowsingTopicsURLLoaderService::BindContext> bind_context1 =
      CreateFactory(proxied_url_loader_factory1, remote_url_loader_factory1);
  bind_context1->OnDidCommitNavigation(
      web_contents()->GetPrimaryMainFrame()->GetWeakDocumentPtr());

  base::WeakPtr<BrowsingTopicsURLLoaderService::BindContext> bind_context2 =
      CreateFactory(proxied_url_loader_factory2, remote_url_loader_factory2);
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

  {
    std::string topics_header_value;
    bool has_topics_header = pending_request1->request.headers.GetHeader(
        "Sec-Browsing-Topics", &topics_header_value);
    EXPECT_TRUE(has_topics_header);
    EXPECT_EQ(topics_header_value, kExpectedHeaderForOrigin1);

    EXPECT_EQ(browser_client().handle_topics_web_api_count(), 1u);
  }

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

  {
    std::string topics_header_value;
    bool has_topics_header = pending_request2->request.headers.GetHeader(
        "Sec-Browsing-Topics", &topics_header_value);

    // Topics not eligible due to permissions policy.
    EXPECT_FALSE(has_topics_header);

    EXPECT_EQ(browser_client().handle_topics_web_api_count(), 1u);
  }

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 1u);

  pending_request1->client->OnReceiveResponse(
      CreateResponseHead(/*topics_header_value=*/"?1"), /*body=*/{},
      absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 2u);
  EXPECT_FALSE(browser_client().last_get_topics_param());
  EXPECT_TRUE(browser_client().last_observe_param());

  pending_request2->client->OnReceiveResponse(
      CreateResponseHead(/*topics_header_value=*/"?1"), /*body=*/{},
      absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(browser_client().handle_topics_web_api_count(), 2u);
}

TEST_F(BrowsingTopicsURLLoaderServiceTest, BindContextClearedDueToDisconnect) {
  NavigatePage(GURL("https://google.com"));

  base::WeakPtr<BrowsingTopicsURLLoaderService::BindContext> bind_context;

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

}  // namespace content
