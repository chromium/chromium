// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/cors_url_loader_factory.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/fake_message_dispatch_context.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/features.h"
#include "net/base/load_flags.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/disk_cache/buildflags.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "services/network/cors/cors_url_loader_test_util.h"
#include "services/network/is_browser_initiated.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/prefetch_matching_url_loader_factory.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/resource_scheduler/resource_scheduler.h"
#include "services/network/resource_scheduler/resource_scheduler_client.h"
#include "services/network/test/fake_test_cert_verifier_params_factory.h"
#include "services/network/test/test_url_loader_client.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network::cors {

namespace {

const auto kProcessId = OriginatingProcessId::renderer(RendererProcessId(123));
constexpr int kRequestId = 456;

}  // namespace

class CorsURLLoaderFactoryTest : public testing::Test,
                                 public net::WithTaskEnvironment {
 public:
  CorsURLLoaderFactoryTest() {
    net::URLRequestContextBuilder context_builder;
    context_builder.set_proxy_resolution_service(
        net::ConfiguredProxyResolutionService::CreateDirect());
    url_request_context_ = context_builder.Build();
  }

  CorsURLLoaderFactoryTest(const CorsURLLoaderFactoryTest&) = delete;
  CorsURLLoaderFactoryTest& operator=(const CorsURLLoaderFactoryTest&) = delete;

 protected:
  // testing::Test implementation.

  void BaseSetup(mojom::URLLoaderFactoryParamsPtr factory_params,
                 mojom::NetworkContextParamsPtr context_params,
                 OriginatingProcessId process_id = kProcessId) {
    test_server_.AddDefaultHandlers();
    ASSERT_TRUE(test_server_.Start());

    network_service_ = NetworkService::CreateForTesting();

    // Use a dummy CertVerifier that always passes cert verification, since
    // these unittests don't need to test CertVerifier behavior.
    context_params->cert_verifier_params =
        FakeTestCertVerifierParamsFactory::GetCertVerifierParams();
    // Use a fixed proxy config, to avoid dependencies on local network
    // configuration.
    context_params->initial_proxy_config =
        net::ProxyConfigWithAnnotation::CreateDirect();

    network_context_ = std::make_unique<NetworkContext>(
        network_service_.get(),
        network_context_remote_.BindNewPipeAndPassReceiver(),
        std::move(context_params));

    factory_params->process_id = process_id;
    factory_params->request_initiator_origin_lock =
        url::Origin::Create(test_server_.base_url());
    auto resource_scheduler_client =
        base::MakeRefCounted<ResourceSchedulerClient>(
            ResourceScheduler::ClientId::Create(),
            IsBrowserInitiated(process_id.is_browser()), &resource_scheduler_,
            url_request_context_->network_quality_estimator());
    factory_owner_ = std::make_unique<PrefetchMatchingURLLoaderFactory>(
        network_context_.get(), std::move(factory_params),
        resource_scheduler_client,
        cors_url_loader_factory_remote_.BindNewPipeAndPassReceiver(),
        &origin_access_list_, nullptr);
    cors_url_loader_factory_ =
        factory_owner_->GetCorsURLLoaderFactoryForTesting();
  }

  void SetUp() override {
    BaseSetup(network::mojom::URLLoaderFactoryParams::New(),
              mojom::NetworkContextParams::New());
  }

  void CreateLoaderAndStart(const ResourceRequest& request) {
    CreateLoaderAndStart(request, mojom::kURLLoadOptionNone);
  }

  void CreateLoaderAndStart(const ResourceRequest& request, uint32_t options) {
    CreateLoaderAndStart(
        request, options,
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  }

  void CreateLoaderAndStart(
      const ResourceRequest& request,
      uint32_t options,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
    url_loaders_.emplace_back();
    test_cors_loader_clients_.emplace_back(
        std::make_unique<TestURLLoaderClient>());
    ResourceRequest request_copy(request);
    cors_url_loader_factory_->CreateLoaderAndStart(
        url_loaders_.back().BindNewPipeAndPassReceiver(), kRequestId, options,
        request_copy, test_cors_loader_clients_.back()->CreateRemote(),
        traffic_annotation);
  }

  bool CreateLoaderAndStartAndReturnIsOutermostMainFrame(
      const ResourceRequest& request) {
    url_loaders_.emplace_back();
    test_cors_loader_clients_.emplace_back(
        std::make_unique<TestURLLoaderClient>());
    ResourceRequest request_copy(request);
    cors_url_loader_factory_->CreateLoaderAndStart(
        url_loaders_.back().BindNewPipeAndPassReceiver(), kRequestId,
        mojom::kURLLoadOptionNone, request_copy,
        test_cors_loader_clients_.back()->CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
    return request_copy.is_outermost_main_frame;
  }

  void ResetFactory() {
    cors_url_loader_factory_ = nullptr;
    factory_owner_.reset();
  }

  const CorsURLLoaderFactory* GetCorsURLLoaderFactory() const {
    return factory_owner_->GetCorsURLLoaderFactoryForTesting();
  }

  net::test_server::EmbeddedTestServer* test_server() { return &test_server_; }

  NetworkContext* network_context() { return network_context_.get(); }

  std::vector<mojo::Remote<mojom::URLLoader>>& url_loaders() {
    return url_loaders_;
  }

  net::test::MockNetworkChangeNotifier* mock_network_change_notifier() {
    return scoped_mock_network_change_notifier_->mock_network_change_notifier();
  }

  std::vector<std::unique_ptr<TestURLLoaderClient>>&
  test_cors_loader_clients() {
    return test_cors_loader_clients_;
  }

  void FlushFactoryForTesting() {
    cors_url_loader_factory_remote_.FlushForTesting();
  }

  bool IsFactoryConnected() const {
    return cors_url_loader_factory_remote_.is_connected();
  }

  OriginAccessList* origin_access_list() { return &origin_access_list_; }

 private:
  mojo::FakeMessageDispatchContext mojo_context_;
  // This is required by NetworkBoundCorsURLLoaderFactoryTest but has to live
  // here to destruct things in the right order (it must outlive
  // url_request_context_).
  std::unique_ptr<net::test::ScopedMockNetworkChangeNotifier>
      scoped_mock_network_change_notifier_ =
          std::make_unique<net::test::ScopedMockNetworkChangeNotifier>();
  std::unique_ptr<net::URLRequestContext> url_request_context_;
  ResourceScheduler resource_scheduler_;
  std::unique_ptr<NetworkService> network_service_;
  std::unique_ptr<NetworkContext> network_context_;
  mojo::Remote<mojom::NetworkContext> network_context_remote_;

  net::test_server::EmbeddedTestServer test_server_;

  // Holder for the CorsURLLoaderFactory.
  std::unique_ptr<PrefetchMatchingURLLoaderFactory> factory_owner_;

  // CorsURLLoaderFactory instance under tests.
  raw_ptr<mojom::URLLoaderFactory> cors_url_loader_factory_;
  mojo::Remote<mojom::URLLoaderFactory> cors_url_loader_factory_remote_;

  // Holds the URLLoaders that CreateLoaderAndStart() creates.
  std::vector<mojo::Remote<mojom::URLLoader>> url_loaders_;

  // TestURLLoaderClients that record callback activities.
  std::vector<std::unique_ptr<TestURLLoaderClient>> test_cors_loader_clients_;

  // Holds for allowed origin access lists.
  OriginAccessList origin_access_list_;
};

// Regression test for https://crbug.com/906305.
TEST_F(CorsURLLoaderFactoryTest, DestructionOrder) {
  ResourceRequest request;
  GURL url = test_server()->GetURL("/hung");
  request.mode = mojom::RequestMode::kNoCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = url;
  request.request_initiator = url::Origin::Create(url);

  // As of r609458 setting `keepalive` to true was triggerring a dereference of
  // `factory_params_` in the destructor of network::URLLoader.  This
  // dereference assumes that the network::URLLoaderFactory (which keeps
  // `factory_params_` alive) lives longer than the network::URLLoaders created
  // via the factory (which necessitates being careful with the destruction
  // order of fields of network::cors::CorsURLLoaderFactory which owns both
  // network::URLLoaderFactory and the network::URLLoaders it creates).
  request.keepalive = true;

  // Create a loader and immediately (while the loader is still stored in
  // CorsURLLoaderFactory::loaders_ / not released via test_cors_loader_client_)
  // destroy the factory.  If ASAN doesn't complain then the test passes.
  CreateLoaderAndStart(request);
  ResetFactory();
}

TEST_F(CorsURLLoaderFactoryTest, CleanupWithSharedCacheObjectInUse) {
  // Create a loader for a response that hangs after receiving headers, and run
  // it until headers are received.
  ResourceRequest request;
  GURL url = test_server()->GetURL("/hung-after-headers");
  request.mode = mojom::RequestMode::kNoCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = url;
  request.request_initiator = url::Origin::Create(url);
  CreateLoaderAndStart(request);
  test_cors_loader_clients().back()->RunUntilResponseReceived();

  // Read only requests will fail synchonously on destruction of the request
  // they're waiting on if they're in the `done_headers_queue` when the other
  // request fails. Make a large number of such requests, spin the message loop
  // so they end up blocked on the hung request, and then destroy all loads. A
  // large number of loaders is needed because they're stored in a set, indexed
  // by address, so teardown order is random.
  request.load_flags =
      net::LOAD_ONLY_FROM_CACHE | net::LOAD_SKIP_CACHE_VALIDATION;
  for (int i = 0; i < 10; ++i)
    CreateLoaderAndStart(request);
  base::RunLoop().RunUntilIdle();

  // This should result in a crash if tearing down one URLLoaderFactory
  // resulting in a another one failing causes a crash during teardown. See
  // https://crbug.com/1209769.
  ResetFactory();
}

TEST_F(CorsURLLoaderFactoryTest, DisallowedLoadFlagToUntrustedLoader) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kNoCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = test_server()->GetURL("/echoall");
  request.load_flags = net::LOAD_DISABLE_CERT_NETWORK_FETCHES;
  request.request_initiator = url::Origin::Create(request.url);
  mojo::test::BadMessageObserver bad_message_observer;
  CreateLoaderAndStart(request);
  EXPECT_EQ("CorsURLLoaderFactory: Untrusted caller using restricted load flag",
            bad_message_observer.WaitForBadMessage());
}

TEST_F(CorsURLLoaderFactoryTest, DocumentDestinationRequiresNavigateMode) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kNoCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = test_server()->GetURL("/echoall");
  request.destination = mojom::RequestDestination::kDocument;
  request.request_initiator = url::Origin::Create(request.url);
  mojo::test::BadMessageObserver bad_message_observer;
  CreateLoaderAndStart(request);
  EXPECT_EQ("CorsURLLoaderFactory: frame destination requires kNavigate mode",
            bad_message_observer.WaitForBadMessage());
}

TEST_F(CorsURLLoaderFactoryTest,
       NavigationFromRendererWithBadRequestURLOrigin) {
  ResourceRequest request;
  GURL url = test_server()->GetURL("/echoall");
  request.mode = mojom::RequestMode::kNavigate;
  request.redirect_mode = mojom::RedirectMode::kManual;
  request.destination = mojom::RequestDestination::kEmpty;
  request.method = net::HttpRequestHeaders::kPostMethod;
  request.url = GURL("https://some.other.origin/echoall");
  request.navigation_redirect_chain.push_back(request.url);
  request.request_initiator = url::Origin::Create(url);
  mojo::test::BadMessageObserver bad_message_observer;
  CreateLoaderAndStart(request);
  EXPECT_EQ("CorsURLLoaderFactory: lock VS initiator mismatch",
            bad_message_observer.WaitForBadMessage());
}

TEST_F(CorsURLLoaderFactoryTest, NavigationFromRendererWithBadRedirectMode) {
  ResourceRequest request;
  GURL url = test_server()->GetURL("/echoall");
  request.mode = mojom::RequestMode::kNavigate;
  request.redirect_mode = mojom::RedirectMode::kFollow;
  request.destination = mojom::RequestDestination::kEmpty;
  request.method = net::HttpRequestHeaders::kPostMethod;
  request.url = url;
  request.navigation_redirect_chain.push_back(request.url);
  request.request_initiator = url::Origin::Create(url).DeriveNewOpaqueOrigin();
  mojo::test::BadMessageObserver bad_message_observer;
  CreateLoaderAndStart(request);
  EXPECT_EQ(
      "CorsURLLoaderFactory: navigate from non-browser-process with "
      "redirect_mode set to 'follow'",
      bad_message_observer.WaitForBadMessage());
}

TEST_F(CorsURLLoaderFactoryTest,
       NavigationFromRendererWithBadRequestNavigationRedirectChain) {
  ResourceRequest request;
  GURL url = test_server()->GetURL("/echoall");
  request.mode = mojom::RequestMode::kNavigate;
  request.redirect_mode = mojom::RedirectMode::kManual;
  request.destination = mojom::RequestDestination::kEmpty;
  request.method = net::HttpRequestHeaders::kPostMethod;
  request.url = url;
  // Do not add url to navigation_redirect_chain
  request.request_initiator = url::Origin::Create(url);
  mojo::test::BadMessageObserver bad_message_observer;
  CreateLoaderAndStart(request);
  EXPECT_EQ(
      "CorsURLLoaderFactory: navigate from non-browser-process without "
      "a redirect chain provided",
      bad_message_observer.WaitForBadMessage());
}

TEST_F(CorsURLLoaderFactoryTest, NavigationRedirectChainWithBadMode) {
  ResourceRequest request;
  GURL url = test_server()->GetURL("/echoall");
  request.mode = mojom::RequestMode::kCors;
  request.redirect_mode = mojom::RedirectMode::kFollow;
  request.destination = mojom::RequestDestination::kEmpty;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = url;
  request.navigation_redirect_chain.push_back(request.url);
  request.request_initiator = url::Origin::Create(url);
  mojo::test::BadMessageObserver bad_message_observer;
  CreateLoaderAndStart(request);
  EXPECT_EQ(
      "CorsURLLoaderFactory: navigation redirect chain set for a "
      "non-navigation",
      bad_message_observer.WaitForBadMessage());
}

TEST_F(CorsURLLoaderFactoryTest, OriginalDestinationIsDocumentWithBadMode) {
  ResourceRequest request;
  GURL url = test_server()->GetURL("/echoall");
  request.mode = mojom::RequestMode::kCors;
  request.redirect_mode = mojom::RedirectMode::kFollow;
  request.destination = mojom::RequestDestination::kEmpty;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = url;
  request.navigation_redirect_chain.push_back(request.url);
  request.request_initiator =
      url::Origin::Create(GURL("https://some.other.origin"));
  request.original_destination = mojom::RequestDestination::kDocument;
  mojo::test::BadMessageObserver bad_message_observer;
  CreateLoaderAndStart(request);
  EXPECT_EQ(
      "CorsURLLoaderFactory: original_destination is unexpectedly set to "
      "kDocument",
      bad_message_observer.WaitForBadMessage());
}

TEST_F(CorsURLLoaderFactoryTest,
       OriginalDestinationIsDocumentWithBadDestination) {
  ResourceRequest request;
  GURL url = test_server()->GetURL("/echoall");
  request.mode = mojom::RequestMode::kNavigate;
  request.redirect_mode = mojom::RedirectMode::kManual;
  request.destination = mojom::RequestDestination::kIframe;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = url;
  request.navigation_redirect_chain.push_back(request.url);
  request.request_initiator =
      url::Origin::Create(GURL("https://some.other.origin"));
  request.original_destination = mojom::RequestDestination::kDocument;
  mojo::test::BadMessageObserver bad_message_observer;
  CreateLoaderAndStart(request);
  EXPECT_EQ(
      "CorsURLLoaderFactory: original_destination is unexpectedly set to "
      "kDocument",
      bad_message_observer.WaitForBadMessage());
}

TEST_F(CorsURLLoaderFactoryTest, DisallowedDestinationFromRendererWebIdentity) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kNoCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = test_server()->GetURL("/echoall");
  request.destination = network::mojom::RequestDestination::kWebIdentity;
  request.request_initiator = url::Origin::Create(request.url);
  mojo::test::BadMessageObserver bad_message_observer;
  CreateLoaderAndStart(request);
  EXPECT_EQ(
      "CorsURLLoaderFactory: attempt to use forbidden destination from "
      "renderer",
      bad_message_observer.WaitForBadMessage());
}

TEST_F(CorsURLLoaderFactoryTest,
       DisallowedDestinationFromRendererEmailVerification) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kNoCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = test_server()->GetURL("/echoall");
  request.destination = network::mojom::RequestDestination::kEmailVerification;
  request.request_initiator = url::Origin::Create(request.url);
  mojo::test::BadMessageObserver bad_message_observer;
  CreateLoaderAndStart(request);
  EXPECT_EQ(
      "CorsURLLoaderFactory: attempt to use forbidden destination from "
      "renderer",
      bad_message_observer.WaitForBadMessage());
}

TEST_F(CorsURLLoaderFactoryTest, DataURLTrafficAnnotationBadMessageTest) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kNoCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = GURL("data:text/plain,foo");

  constexpr char kTestId[] = "test_id";
  net::NetworkTrafficAnnotationTag tag =
      net::DefineNetworkTrafficAnnotation(kTestId, "nothing");
  net::MutableNetworkTrafficAnnotationTag mutable_tag(tag);

  mojo::test::BadMessageObserver bad_message_observer;
  CreateLoaderAndStart(request, mojom::kURLLoadOptionNone, mutable_tag);

  EXPECT_EQ(
      "CorsURLLoaderFactory: data: URL is not supported. "
      "net-traffic_annotation_hash=" +
          base::NumberToString(tag.unique_id_hash_code),
      bad_message_observer.WaitForBadMessage());
}

class RequireCrossSiteRequestForCookiesCorsURLLoaderFactoryTest
    : public CorsURLLoaderFactoryTest {
  void SetUp() override {
    auto factory_params = network::mojom::URLLoaderFactoryParams::New();
    factory_params->require_cross_site_request_for_cookies = true;
    auto context_params = mojom::NetworkContextParams::New();
    BaseSetup(std::move(factory_params), std::move(context_params));
  }
};

TEST_F(RequireCrossSiteRequestForCookiesCorsURLLoaderFactoryTest,
       NavigationWithSameSiteForCookies) {
  ResourceRequest request;
  GURL url = test_server()->GetURL("/echoall");
  request.mode = mojom::RequestMode::kNavigate;
  request.redirect_mode = mojom::RedirectMode::kManual;
  request.destination = mojom::RequestDestination::kEmpty;
  request.method = net::HttpRequestHeaders::kPostMethod;
  request.url = url;
  request.navigation_redirect_chain.push_back(request.url);
  request.request_initiator = url::Origin::Create(url);
  request.site_for_cookies = net::SiteForCookies::FromUrl(url);
  mojo::test::BadMessageObserver bad_message_observer;
  CreateLoaderAndStart(request);
  EXPECT_EQ(
      "CorsURLLoaderFactory: all requests in this context must be cross-site",
      bad_message_observer.WaitForBadMessage());
}

TEST_F(RequireCrossSiteRequestForCookiesCorsURLLoaderFactoryTest,
       NavigationWithCrossSiteForCookies) {
  ResourceRequest request;
  GURL url = test_server()->GetURL("/echoall");
  request.mode = mojom::RequestMode::kNavigate;
  request.redirect_mode = mojom::RedirectMode::kManual;
  request.destination = mojom::RequestDestination::kEmpty;
  request.method = net::HttpRequestHeaders::kPostMethod;
  request.url = url;
  request.navigation_redirect_chain.push_back(request.url);
  request.request_initiator = url::Origin::Create(url);
  mojo::test::BadMessageObserver bad_message_observer;
  CreateLoaderAndStart(request);
  auto* client = test_cors_loader_clients().back().get();
  client->RunUntilComplete();
  EXPECT_TRUE(client->has_received_completion());
  EXPECT_EQ(net::OK, client->completion_status().error_code);
}

TEST_F(CorsURLLoaderFactoryTest, UntrustedCorsPreflightRequestAreFailed) {
  EXPECT_FALSE(GetCorsURLLoaderFactory()->IsCorsPreflighLoadOptionAllowed());
  ResourceRequest request;
  GURL url = test_server()->GetURL("/echoall");
  request.mode = mojom::RequestMode::kNavigate;
  request.redirect_mode = mojom::RedirectMode::kManual;
  request.destination = mojom::RequestDestination::kEmpty;
  request.navigation_redirect_chain.push_back(request.url);
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = url;
  request.request_initiator = url::Origin::Create(url);
  mojo::test::BadMessageObserver bad_message_observer;
  CreateLoaderAndStart(request, mojom::kURLLoadOptionAsCorsPreflight);
  EXPECT_EQ("CorsURLLoaderFactory: kURLLoadOptionAsCorsPreflight is set",
            bad_message_observer.WaitForBadMessage());
}

class NetworkBoundCorsURLLoaderFactoryTest : public CorsURLLoaderFactoryTest {
  void SetUp() override {
    // Setting URLLoaderFactoryParams::bound_network requires
    // net::base::NetworkChangeNotifier::AreNetworkHandlesSupported() == true.
    // To make that the case, force its support.
    mock_network_change_notifier()->ForceNetworkHandlesSupported();
    auto factory_params = network::mojom::URLLoaderFactoryParams::New();
    factory_params->disable_web_security = true;
    auto context_params = mojom::NetworkContextParams::New();
    context_params->bound_network = 1234;
    BaseSetup(std::move(factory_params), std::move(context_params));
  }
};

// Regression test for crbug.com/366242716.
TEST_F(NetworkBoundCorsURLLoaderFactoryTest, CorsPreflightRequestAreAllowed) {
  if constexpr (BUILDFLAG(IS_ANDROID)) {
    EXPECT_TRUE(GetCorsURLLoaderFactory()->IsCorsPreflighLoadOptionAllowed());
  } else {
    GTEST_SKIP() << "Network bound NetworkContext/URLRequestContext is "
                    "supported only on "
                    "Android, see URLRequestContextBuilder::BindToNetwork";
  }
}

TEST_F(CorsURLLoaderFactoryTest, ForbiddenSecHeader) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = test_server()->GetURL("/echoall");
  request.request_initiator = url::Origin::Create(request.url);
  request.headers.SetHeader("Sec-Invalid", "value");

  mojo::test::BadMessageObserver bad_message_observer;
  CreateLoaderAndStart(request);

  EXPECT_EQ("CorsURLLoaderFactory: Forbidden Sec- header from renderer",
            bad_message_observer.WaitForBadMessage());
}

class TrustedURLLoaderFactoryTest : public CorsURLLoaderFactoryTest {
  void SetUp() override {
    auto factory_params = network::mojom::URLLoaderFactoryParams::New();
    factory_params->is_trusted = true;
    auto context_params = mojom::NetworkContextParams::New();
    BaseSetup(std::move(factory_params), std::move(context_params));
  }
};

TEST_F(TrustedURLLoaderFactoryTest, DisallowedLoadFlagToTrustedLoader) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kNoCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = test_server()->GetURL("/echoall");
  request.load_flags = net::LOAD_CAN_USE_SHARED_DICTIONARY;
  request.request_initiator = url::Origin::Create(request.url);
  mojo::test::BadMessageObserver bad_message_observer;
  CreateLoaderAndStart(request);
  EXPECT_EQ("CorsURLLoaderFactory: Internal load flag received",
            bad_message_observer.WaitForBadMessage());
}

TEST_F(CorsURLLoaderFactoryTest, OutermostMainFrameFromRendererClamped) {
  base::HistogramTester histogram_tester;
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kInclude;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = test_server()->GetURL("/echoall");
  request.is_outermost_main_frame = true;
  request.request_initiator = url::Origin::Create(request.url);
  mojo::test::BadMessageObserver bad_message_observer;
  EXPECT_FALSE(CreateLoaderAndStartAndReturnIsOutermostMainFrame(request));
  test_cors_loader_clients().back()->RunUntilComplete();
  EXPECT_EQ(net::OK,
            test_cors_loader_clients().back()->completion_status().error_code);
  EXPECT_FALSE(bad_message_observer.got_bad_message());
  histogram_tester.ExpectUniqueSample(
      "NetworkService.CorsURLLoaderFactory.IsOutermostMainFrameClamped", true,
      1);
}

TEST_F(CorsURLLoaderFactoryTest,
       OutermostMainFrameNavigationFromRendererNotClamped) {
  base::HistogramTester histogram_tester;
  ResourceRequest request;
  request.mode = mojom::RequestMode::kNavigate;
  request.redirect_mode = mojom::RedirectMode::kManual;
  request.credentials_mode = mojom::CredentialsMode::kInclude;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = test_server()->GetURL("/echoall");
  request.navigation_redirect_chain.push_back(request.url);
  request.destination = mojom::RequestDestination::kEmpty;
  request.original_destination = mojom::RequestDestination::kDocument;
  request.is_outermost_main_frame = true;
  request.request_initiator = url::Origin::Create(request.url);
  mojo::test::BadMessageObserver bad_message_observer;
  EXPECT_TRUE(CreateLoaderAndStartAndReturnIsOutermostMainFrame(request));
  test_cors_loader_clients().back()->RunUntilComplete();
  EXPECT_EQ(net::OK,
            test_cors_loader_clients().back()->completion_status().error_code);
  EXPECT_FALSE(bad_message_observer.got_bad_message());
  histogram_tester.ExpectTotalCount(
      "NetworkService.CorsURLLoaderFactory.IsOutermostMainFrameClamped", 0);
}

class BrowserProcessCorsURLLoaderFactoryTest : public CorsURLLoaderFactoryTest {
 protected:
  void SetUp() override {
    BaseSetup(network::mojom::URLLoaderFactoryParams::New(),
              mojom::NetworkContextParams::New(),
              OriginatingProcessId::browser());
  }
};

TEST_F(BrowserProcessCorsURLLoaderFactoryTest, OutermostMainFrameNotClamped) {
  base::HistogramTester histogram_tester;
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kInclude;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = test_server()->GetURL("/echoall");
  request.is_outermost_main_frame = true;
  request.request_initiator = url::Origin::Create(request.url);
  mojo::test::BadMessageObserver bad_message_observer;
  EXPECT_TRUE(CreateLoaderAndStartAndReturnIsOutermostMainFrame(request));
  test_cors_loader_clients().back()->RunUntilComplete();
  EXPECT_EQ(net::OK,
            test_cors_loader_clients().back()->completion_status().error_code);
  EXPECT_FALSE(bad_message_observer.got_bad_message());
  histogram_tester.ExpectTotalCount(
      "NetworkService.CorsURLLoaderFactory.IsOutermostMainFrameClamped", 0);
}

#if BUILDFLAG(ENABLE_DISK_CACHE_SQL_BACKEND)
class SharedHttpCacheCorsURLLoaderFactoryTest
    : public CorsURLLoaderFactoryTest {
 public:
  SharedHttpCacheCorsURLLoaderFactoryTest() {
    AddScopedFeatureList().InitWithFeaturesAndParameters(
        {{net::features::kDiskCacheBackendExperiment,
          {{net::features::kDiskCacheBackendParam.name, "sql"}}},
         {net::features::kRendererAccessibleHttpCache, {}}},
        {});
  }

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    auto context_params = CreateNetworkContextParamsForTesting();
    context_params->http_cache_enabled = true;
    context_params->file_paths->http_cache_directory = temp_dir_.GetPath();

    const url::Origin origin = url::Origin::Create(GURL("https://example.com"));
    auto factory_params = network::mojom::URLLoaderFactoryParams::New();
    factory_params->renderer_accessible_http_cache_write_enabled = true;
    factory_params->isolation_info = net::IsolationInfo::Create(
        net::IsolationInfo::RequestType::kOther, origin, origin,
        net::SiteForCookies::FromOrigin(origin));

    BaseSetup(std::move(factory_params), std::move(context_params));
  }

 private:
  base::ScopedTempDir temp_dir_;
};

TEST_F(SharedHttpCacheCorsURLLoaderFactoryTest, RedirectNotCached) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.RegisterRequestHandler(base::BindRepeating(
      [](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url == "/script.js" ||
            request.relative_url == "/script2.js") {
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_code(net::HTTP_OK);
          response->set_content_type("application/javascript");
          response->AddCustomHeader("Cache-Control", "max-age=3600");
          response->AddCustomHeader("Access-Control-Allow-Origin", "*");
          response->set_content("console.log('ok');");
          return response;
        }
        if (request.relative_url == "/redirect.js") {
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_code(net::HTTP_FOUND);
          response->AddCustomHeader("Location", "/script2.js");
          response->AddCustomHeader("Access-Control-Allow-Origin", "*");
          return response;
        }
        return nullptr;
      }));
  ASSERT_TRUE(https_server.Start());

  const url::Origin initiator_origin =
      url::Origin::Create(test_server()->base_url());

  // 1. Direct request without redirect should be marked eligible for shared
  // cache.
  {
    ResourceRequest request;
    request.mode = mojom::RequestMode::kCors;
    request.credentials_mode = mojom::CredentialsMode::kOmit;
    request.method = net::HttpRequestHeaders::kGetMethod;
    request.url = https_server.GetURL("/script.js");
    request.destination = mojom::RequestDestination::kScript;
    request.request_initiator = initiator_origin;

    CreateLoaderAndStart(request);
    test_cors_loader_clients().back()->RunUntilComplete();
    EXPECT_EQ(
        net::OK,
        test_cors_loader_clients().back()->completion_status().error_code);

    disk_cache::Backend* backend =
        network_context()->GetHttpCache()->GetCurrentBackend();
    ASSERT_TRUE(backend);
    EXPECT_EQ(1u, backend->GetSharedCacheEligibleEntriesCountForTest());
  }

  // 2. Redirected request (302 -> 200 OK) must NOT be marked eligible for
  // shared cache.
  {
    ResourceRequest request;
    request.mode = mojom::RequestMode::kCors;
    request.credentials_mode = mojom::CredentialsMode::kOmit;
    request.method = net::HttpRequestHeaders::kGetMethod;
    request.url = https_server.GetURL("/redirect.js");
    request.destination = mojom::RequestDestination::kScript;
    request.request_initiator = initiator_origin;

    CreateLoaderAndStart(request);
    test_cors_loader_clients().back()->RunUntilRedirectReceived();
    url_loaders().back()->FollowRedirect({}, std::nullopt);
    test_cors_loader_clients().back()->RunUntilComplete();
    EXPECT_EQ(
        net::OK,
        test_cors_loader_clients().back()->completion_status().error_code);

    disk_cache::Backend* backend =
        network_context()->GetHttpCache()->GetCurrentBackend();
    ASSERT_TRUE(backend);
    EXPECT_EQ(1u, backend->GetSharedCacheEligibleEntriesCountForTest());
  }
}
#endif  // BUILDFLAG(ENABLE_DISK_CACHE_SQL_BACKEND)

class IsolatedWorldOriginLockCorsURLLoaderFactoryTest
    : public CorsURLLoaderFactoryTest {
 protected:
  const url::Origin isolated_origin_a_ =
      url::Origin::Create(GURL("https://isolated-a.example.com"));
  const url::Origin isolated_origin_b_ =
      url::Origin::Create(GURL("https://isolated-b.example.com"));

  void SetUp() override {}

  void SetUpFactory(bool ignore_isolated_world_origin,
                    const std::optional<url::Origin>& lock,
                    OriginatingProcessId process_id = kProcessId) {
    auto factory_params = network::mojom::URLLoaderFactoryParams::New();
    factory_params->ignore_isolated_world_origin = ignore_isolated_world_origin;
    factory_params->isolated_world_origin_lock = lock;
    BaseSetup(std::move(factory_params), mojom::NetworkContextParams::New(),
              process_id);
  }
};

TEST_F(IsolatedWorldOriginLockCorsURLLoaderFactoryTest, MatchingLockAllowed) {
  SetUpFactory(/*ignore_isolated_world_origin=*/false, isolated_origin_a_);

  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kInclude;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = test_server()->GetURL("/echoall");
  request.request_initiator = url::Origin::Create(test_server()->base_url());
  request.isolated_world_origin = isolated_origin_a_;

  mojo::test::BadMessageObserver bad_message_observer;
  CreateLoaderAndStart(request);
  EXPECT_FALSE(bad_message_observer.got_bad_message());
}

TEST_F(IsolatedWorldOriginLockCorsURLLoaderFactoryTest,
       MismatchedLockRejected) {
  SetUpFactory(/*ignore_isolated_world_origin=*/false, isolated_origin_a_);

  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kInclude;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = test_server()->GetURL("/echoall");
  request.request_initiator = url::Origin::Create(test_server()->base_url());
  request.isolated_world_origin = isolated_origin_b_;

  mojo::test::BadMessageObserver bad_message_observer;
  CreateLoaderAndStart(request);
  EXPECT_TRUE(bad_message_observer.got_bad_message());
  EXPECT_EQ("CorsURLLoaderFactory: isolated_world_origin lock mismatch",
            bad_message_observer.WaitForBadMessage());
}

TEST_F(IsolatedWorldOriginLockCorsURLLoaderFactoryTest,
       MissingLockOnPrivilegedFactoryRejected) {
  SetUpFactory(/*ignore_isolated_world_origin=*/false, std::nullopt);

  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kInclude;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = test_server()->GetURL("/echoall");
  request.request_initiator = url::Origin::Create(test_server()->base_url());
  request.isolated_world_origin = isolated_origin_a_;

  mojo::test::BadMessageObserver bad_message_observer;
  CreateLoaderAndStart(request);
  EXPECT_TRUE(bad_message_observer.got_bad_message());
  EXPECT_EQ("CorsURLLoaderFactory: isolated_world_origin lock mismatch",
            bad_message_observer.WaitForBadMessage());
}

TEST_F(IsolatedWorldOriginLockCorsURLLoaderFactoryTest,
       NoIsolatedWorldOriginAllowed) {
  SetUpFactory(/*ignore_isolated_world_origin=*/false, isolated_origin_a_);

  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kInclude;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = test_server()->GetURL("/echoall");
  request.request_initiator = url::Origin::Create(test_server()->base_url());
  request.isolated_world_origin = std::nullopt;

  mojo::test::BadMessageObserver bad_message_observer;
  CreateLoaderAndStart(request);
  EXPECT_FALSE(bad_message_observer.got_bad_message());
}

TEST_F(IsolatedWorldOriginLockCorsURLLoaderFactoryTest,
       IgnoredOriginNotRejected) {
  SetUpFactory(/*ignore_isolated_world_origin=*/true, std::nullopt);

  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kInclude;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = test_server()->GetURL("/echoall");
  request.request_initiator = url::Origin::Create(test_server()->base_url());
  request.isolated_world_origin = isolated_origin_b_;

  mojo::test::BadMessageObserver bad_message_observer;
  CreateLoaderAndStart(request);
  EXPECT_FALSE(bad_message_observer.got_bad_message());
}

TEST_F(IsolatedWorldOriginLockCorsURLLoaderFactoryTest,
       IgnoredOriginClearedAndUnsafeHeadersBlocked) {
  origin_access_list()->AddAllowListEntryForOrigin(
      isolated_origin_a_, "http", std::string(test_server()->base_url().host()),
      /*port=*/0, mojom::CorsDomainMatchMode::kAllowSubdomains,
      mojom::CorsPortMatchMode::kAllowAnyPort,
      mojom::CorsOriginAccessMatchPriority::kDefaultPriority);

  // Normal factory ignores isolated world origin and clears it.
  SetUpFactory(/*ignore_isolated_world_origin=*/true, std::nullopt);

  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kInclude;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = test_server()->GetURL("/echoall");
  request.request_initiator = url::Origin::Create(test_server()->base_url());
  request.isolated_world_origin = isolated_origin_a_;
  request.headers.SetHeader("Sec-Invalid", "value");

  // Since isolated_world_origin is cleared, ShouldAllowUnsafeHeaders falls back
  // to request_initiator, which does not have allow list privileges, so the
  // forbidden header causes a bad message.
  mojo::test::BadMessageObserver bad_message_observer;
  CreateLoaderAndStart(request);
  EXPECT_TRUE(bad_message_observer.got_bad_message());
  EXPECT_EQ("CorsURLLoaderFactory: Forbidden Sec- header from renderer",
            bad_message_observer.WaitForBadMessage());
}

TEST_F(IsolatedWorldOriginLockCorsURLLoaderFactoryTest, BrowserProcessAllowed) {
  SetUpFactory(/*ignore_isolated_world_origin=*/false, isolated_origin_a_,
               OriginatingProcessId::browser());

  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kInclude;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = test_server()->GetURL("/echoall");
  request.request_initiator = url::Origin::Create(test_server()->base_url());
  request.isolated_world_origin = isolated_origin_b_;

  mojo::test::BadMessageObserver bad_message_observer;
  CreateLoaderAndStart(request);
  EXPECT_FALSE(bad_message_observer.got_bad_message());
}

TEST_F(IsolatedWorldOriginLockCorsURLLoaderFactoryTest,
       FeatureDisabledBypassesCheck) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kEnforceIsolatedWorldOriginLock);

  SetUpFactory(/*ignore_isolated_world_origin=*/false, isolated_origin_a_);

  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kInclude;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = test_server()->GetURL("/echoall");
  request.request_initiator = url::Origin::Create(test_server()->base_url());
  request.isolated_world_origin = isolated_origin_b_;

  mojo::test::BadMessageObserver bad_message_observer;
  CreateLoaderAndStart(request);
  EXPECT_FALSE(bad_message_observer.got_bad_message());
}

TEST_F(IsolatedWorldOriginLockCorsURLLoaderFactoryTest,
       OpaqueInitiatorWithMatchingIsolatedWorldOriginAllowed) {
  SetUpFactory(/*ignore_isolated_world_origin=*/false, isolated_origin_a_);

  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kInclude;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = test_server()->GetURL("/echoall");
  // An opaque initiator (e.g. a content script running in a sandboxed iframe
  // or data: URL frame) is allowed by VerifyRequestInitiatorLock, and the
  // isolated_world_origin matches the factory lock.
  request.request_initiator = url::Origin();
  request.isolated_world_origin = isolated_origin_a_;

  mojo::test::BadMessageObserver bad_message_observer;
  CreateLoaderAndStart(request);
  EXPECT_FALSE(bad_message_observer.got_bad_message());
}

TEST_F(IsolatedWorldOriginLockCorsURLLoaderFactoryTest,
       OpaqueIsolatedWorldOriginOnDefaultFactoryAllowed) {
  SetUpFactory(/*ignore_isolated_world_origin=*/true, std::nullopt);

  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kInclude;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = test_server()->GetURL("/echoall");
  request.request_initiator = url::Origin::Create(test_server()->base_url());
  // If an isolated world has an opaque origin, Blink falls back to the default
  // factory (ignore_isolated_world_origin = true), which clears
  // isolated_world_origin without reporting a bad message.
  request.isolated_world_origin = url::Origin();

  mojo::test::BadMessageObserver bad_message_observer;
  CreateLoaderAndStart(request);
  EXPECT_FALSE(bad_message_observer.got_bad_message());
}

TEST_F(IsolatedWorldOriginLockCorsURLLoaderFactoryTest,
       OpaqueIsolatedWorldOriginOnPrivilegedFactoryRejected) {
  SetUpFactory(/*ignore_isolated_world_origin=*/false, isolated_origin_a_);

  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kInclude;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = test_server()->GetURL("/echoall");
  request.request_initiator = url::Origin::Create(test_server()->base_url());
  request.isolated_world_origin = url::Origin();

  mojo::test::BadMessageObserver bad_message_observer;
  CreateLoaderAndStart(request);
  EXPECT_TRUE(bad_message_observer.got_bad_message());
  EXPECT_EQ("CorsURLLoaderFactory: isolated_world_origin lock mismatch",
            bad_message_observer.WaitForBadMessage());
}

TEST_F(IsolatedWorldOriginLockCorsURLLoaderFactoryTest,
       LockPresentWhenIgnoredOriginIsTrueRejected) {
  mojo::test::BadMessageObserver bad_message_observer;
  SetUpFactory(/*ignore_isolated_world_origin=*/true, isolated_origin_a_);
  EXPECT_TRUE(bad_message_observer.got_bad_message());
  EXPECT_EQ(
      "CorsURLLoaderFactory: isolated_world_origin_lock set when "
      "ignore_isolated_world_origin is true",
      bad_message_observer.WaitForBadMessage());
}

}  // namespace network::cors
