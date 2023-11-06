// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/memory_pressure_listener.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/base/features.h"
#include "net/base/mime_sniffer.h"
#include "net/base/schemeful_site.h"
#include "net/base/url_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "services/network/cors/cors_url_loader_factory.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/network_service_memory_cache.h"
#include "services/network/network_service_memory_cache_writer.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/http_raw_headers.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/resource_scheduler/resource_scheduler_client.h"
#include "services/network/test/fake_test_cert_verifier_params_factory.h"
#include "services/network/test/mock_devtools_observer.h"
#include "services/network/test/test_url_loader_client.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

namespace {

constexpr int kMaxTotalSize = 8 * 1024;
constexpr int kMaxPerEntrySize = 4 * 1024;

struct LoaderPair {
  LoaderPair() : client(std::make_unique<TestURLLoaderClient>()) {}
  ~LoaderPair() = default;

  LoaderPair(LoaderPair&&) = default;
  LoaderPair& operator=(LoaderPair&&) = default;
  LoaderPair(const LoaderPair&) = delete;
  LoaderPair& operator=(const LoaderPair&) = delete;

  mojo::Remote<mojom::URLLoader> loader_remote;
  std::unique_ptr<TestURLLoaderClient> client;
};

mojom::URLResponseHeadPtr CreateCacheableURLResponseHead() {
  mojom::URLResponseHeadPtr response_head = CreateURLResponseHead(net::HTTP_OK);
  response_head->headers->AddHeader("Cache-Control", "max-age=60");
  base::Time now = base::Time::Now();
  response_head->request_time = now;
  response_head->response_time = now;
  return response_head;
}

// An EmbeddedTestServer request handler that returns a cacheable response of
// which body size and max-age are specified by the query string. The content
// body consists of 'a'.
std::unique_ptr<net::test_server::HttpResponse> CacheableResponseHandler(
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().path_piece() != "/cacheable")
    return nullptr;

  auto response = std::make_unique<net::test_server::BasicHttpResponse>();

  uint64_t body_size = 64;
  std::string query_body_size;
  if (net::GetValueForKeyInQuery(request.GetURL(), "body-size",
                                 &query_body_size)) {
    EXPECT_TRUE(base::StringToUint64(query_body_size, &body_size));
  }

  uint64_t max_age = 60;
  std::string query_max_age;
  if (net::GetValueForKeyInQuery(request.GetURL(), "max-age", &query_max_age)) {
    EXPECT_TRUE(base::StringToUint64(query_max_age, &max_age));
  }

  response->AddCustomHeader("cache-control",
                            base::StringPrintf("max-age=%" PRId64, max_age));
  response->set_content(std::string(body_size, 'a'));
  return response;
}

// Similar to above, but doesn't send Content-Length header.
std::unique_ptr<net::test_server::HttpResponse>
CacheableWithoutContentLengthHandler(
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().path_piece() != "/cacheable_without_content_length")
    return nullptr;

  uint64_t body_size = 64;
  std::string query_body_size;
  if (net::GetValueForKeyInQuery(request.GetURL(), "body-size",
                                 &query_body_size)) {
    EXPECT_TRUE(base::StringToUint64(query_body_size, &body_size));
  }

  constexpr const char kHeader[] =
      "HTTP/1.1 200 OK\n"
      "Content-Type: text/plain\n";
  auto response = std::make_unique<net::test_server::RawHttpResponse>(
      kHeader, std::string(body_size, 'a'));
  return response;
}

// Used for cross origin read blocking check.
std::unique_ptr<net::test_server::HttpResponse> CorbCheckHandler(
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().path_piece() == "/corb_nosniff") {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->AddCustomHeader("cache-control", "max-age=60");
    response->AddCustomHeader("x-content-type-options", "nosniff");
    response->AddCustomHeader("content-type", "text/javascript");
    response->set_content("{\"key\": true}");
    return response;
  }

  if (request.GetURL().path_piece() == "/corb_sniff") {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->AddCustomHeader("cache-control", "max-age=60");
    response->AddCustomHeader("content-type", "text/html");
    // Set a html content of which size is larger than net::kMaxBytestosniff.
    std::string content("<html>sniffed content");
    content.append(std::string(net::kMaxBytesToSniff, ' '));
    response->set_content(content);
    return response;
  }

  return nullptr;
}

// Used in NetworkServiceMemoryCacheWithFactoryOverrideTest.
class TestURLLoaderFactory : public mojom::URLLoaderFactory {
 public:
  explicit TestURLLoaderFactory(
      mojo::PendingReceiver<mojom::URLLoaderFactory> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  void CreateLoaderAndStart(
      mojo::PendingReceiver<mojom::URLLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const ResourceRequest& resource_request,
      mojo::PendingRemote<mojom::URLLoaderClient> pending_client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    url_loader_receivers_.Add(&noop_loader_, std::move(receiver));

    mojo::Remote<mojom::URLLoaderClient> client(std::move(pending_client));
    mojom::URLResponseHeadPtr response_head = CreateCacheableURLResponseHead();
    client->OnReceiveResponse(std::move(response_head), /*body=*/{},
                              absl::nullopt);
    client->OnComplete(URLLoaderCompletionStatus(net::OK));
  }
  void Clone(mojo::PendingReceiver<mojom::URLLoaderFactory> receiver) override {
    receivers_.Add(this, std::move(receiver));
  }

 private:
  class NoopURLLoader : public mojom::URLLoader {
   public:
    void FollowRedirect(
        const std::vector<std::string>& removed_headers,
        const net::HttpRequestHeaders& modified_headers,
        const net::HttpRequestHeaders& modified_cors_exempt_headers,
        const absl::optional<GURL>& new_url) override {}
    void SetPriority(net::RequestPriority priority,
                     int32_t intra_priority_value) override {}
    void PauseReadingBodyFromNet() override {}
    void ResumeReadingBodyFromNet() override {}
  };

  NoopURLLoader noop_loader_;
  mojo::ReceiverSet<mojom::URLLoaderFactory> receivers_;
  mojo::ReceiverSet<mojom::URLLoader> url_loader_receivers_;
};

}  // namespace

class NetworkServiceMemoryCacheTest : public testing::Test {
 public:
  NetworkServiceMemoryCacheTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kNetworkServiceMemoryCache,
        {{"max_total_size", base::NumberToString(kMaxTotalSize)},
         {"max_per_entry_size", base::NumberToString(kMaxPerEntrySize)}});
  }

  ~NetworkServiceMemoryCacheTest() override = default;

  void SetUp() override {
    should_redirect_in_cacheable_handler_ = false;

    test_server_.AddDefaultHandlers();
    test_server_.RegisterRequestHandler(
        base::BindRepeating(&CacheableResponseHandler));
    test_server_.RegisterRequestHandler(
        base::BindRepeating(&CacheableWithoutContentLengthHandler));
    test_server_.RegisterRequestHandler(base::BindRepeating(&CorbCheckHandler));
    test_server_.RegisterRequestHandler(base::BindRepeating(
        &NetworkServiceMemoryCacheTest::CacheableOrRedirectHandler,
        base::Unretained(this)));
    ASSERT_TRUE(test_server_.Start());

    // The following setup similar to CorsURLLoaderFactoryTest.

    network_service_ = NetworkService::CreateForTesting();

    auto context_params = mojom::NetworkContextParams::New();
    context_params->cert_verifier_params =
        FakeTestCertVerifierParamsFactory::GetCertVerifierParams();
    context_params->initial_proxy_config =
        net::ProxyConfigWithAnnotation::CreateDirect();
    network_context_ = std::make_unique<NetworkContext>(
        network_service_.get(),
        network_context_remote_.BindNewPipeAndPassReceiver(),
        std::move(context_params));

    auto factory_params = network::mojom::URLLoaderFactoryParams::New();
    constexpr int kProcessId = 123;
    factory_params->process_id = kProcessId;
    factory_params->is_trusted = true;
    factory_params->request_initiator_origin_lock =
        url::Origin::Create(test_server_.base_url());
    if (HasFactoryOverride()) {
      factory_params->factory_override = mojom::URLLoaderFactoryOverride::New();
      mojo::PendingRemote<mojom::URLLoaderFactory> factory_remote;
      overriding_factory_ = std::make_unique<TestURLLoaderFactory>(
          factory_remote.InitWithNewPipeAndPassReceiver());
      factory_params->factory_override->overriding_factory =
          std::move(factory_remote);
    }

    url::Origin test_server_origin =
        url::Origin::Create(test_server_.base_url());
    factory_params->isolation_info =
        net::IsolationInfo::CreateForInternalRequest(test_server_origin);

    cors_url_loader_factory_ = std::make_unique<cors::CorsURLLoaderFactory>(
        network_context_.get(), std::move(factory_params),
        /*resource_scheduler_client=*/nullptr,
        cors_url_loader_factory_remote_.BindNewPipeAndPassReceiver(),
        &origin_access_list_, /*resource_block_list=*/nullptr);
  }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  net::test_server::EmbeddedTestServer& test_server() { return test_server_; }

  net::URLRequestContext& url_request_context() {
    return *network_context_->url_request_context();
  }

  NetworkServiceMemoryCache& memory_cache() {
    return *network_context_->GetMemoryCache();
  }

  void MakeCacheableHandlerSendRedirect() {
    should_redirect_in_cacheable_handler_ = true;
  }

  ResourceRequest CreateRequest(const std::string& relative_path) {
    ResourceRequest request;
    GURL url = test_server().GetURL(relative_path);
    url::Origin origin = url::Origin::Create(url);
    request.url = url;
    request.request_initiator = origin;
    request.enable_load_timing = true;
    return request;
  }

  std::unique_ptr<net::URLRequest> CreateURLRequest(const GURL& url) {
    return url_request_context().CreateRequest(url, net::DEFAULT_PRIORITY,
                                               /*delegate=*/nullptr,
                                               TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  LoaderPair CreateLoaderAndStart(const ResourceRequest& request) {
    LoaderPair pair;
    cors_url_loader_factory_->CreateLoaderAndStart(
        pair.loader_remote.BindNewPipeAndPassReceiver(), /*request_id=*/1,
        mojom::kURLLoadOptionNone, request, pair.client->CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
    return pair;
  }

  void StoreResponseToMemoryCache(const ResourceRequest& request) {
    LoaderPair pair = CreateLoaderAndStart(request);
    pair.client->RunUntilComplete();
  }

  bool WriterWillBeCreatedToStoreResponse(
      net::URLRequest* url_request,
      const mojom::URLResponseHeadPtr& response_head) {
    std::unique_ptr<NetworkServiceMemoryCacheWriter> writer =
        memory_cache().MaybeCreateWriter(url_request,
                                         mojom::RequestDestination::kDocument,
                                         net::TransportInfo(), response_head);
    return writer.get() != nullptr;
  }

  bool CanServeFromMemoryCache(const ResourceRequest& request) {
    net::SchemefulSite site(request.url);
    net::NetworkIsolationKey network_isolation_key(/*top_frame_site=*/site,
                                                   /*frame_site=*/site);
    return CanServeFromMemoryCache(request, network_isolation_key);
  }

  bool CanServeFromMemoryCache(
      const ResourceRequest& request,
      const net::NetworkIsolationKey& network_isolation_key) {
    return CanServeFromMemoryCache(request, network_isolation_key,
                                   CrossOriginEmbedderPolicy());
  }

  bool CanServeFromMemoryCache(
      const ResourceRequest& request,
      const net::NetworkIsolationKey& network_isolation_key,
      const CrossOriginEmbedderPolicy& cross_origin_embedder_policy) {
    return memory_cache()
        .CanServe(mojom::kURLLoadOptionNone, request, network_isolation_key,
                  cross_origin_embedder_policy,
                  /*client_security_state=*/nullptr)
        .has_value();
  }

  virtual bool HasFactoryOverride() const { return false; }

 private:
  std::unique_ptr<net::test_server::HttpResponse> CacheableOrRedirectHandler(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().path_piece() != "/cacheable_or_redirect")
      return nullptr;

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();

    if (should_redirect_in_cacheable_handler_) {
      response->set_code(net::HttpStatusCode::HTTP_FOUND);
      response->AddCustomHeader("location", "/cacheable");
    } else {
      constexpr size_t kBodySize = 64;
      response->AddCustomHeader("cache-control", "max-age=60");
      response->set_content(std::string(kBodySize, 'a'));
    }

    return response;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::URLRequestContext> url_request_context_;
  std::unique_ptr<NetworkService> network_service_;
  std::unique_ptr<NetworkContext> network_context_;
  mojo::Remote<mojom::NetworkContext> network_context_remote_;

  net::test_server::EmbeddedTestServer test_server_;

  bool should_redirect_in_cacheable_handler_ = false;

  std::unique_ptr<mojom::URLLoaderFactory> cors_url_loader_factory_;
  mojo::Remote<mojom::URLLoaderFactory> cors_url_loader_factory_remote_;

  cors::OriginAccessList origin_access_list_;

  std::unique_ptr<TestURLLoaderFactory> overriding_factory_;
};

class NetworkServiceMemoryCacheWithFactoryOverrideTest
    : public NetworkServiceMemoryCacheTest {
 public:
  bool HasFactoryOverride() const override { return true; }
};

TEST_F(NetworkServiceMemoryCacheTest,
       CreateWriter_SchemeIsNeitherHTTPNorHTTPS) {
  std::unique_ptr<net::URLRequest> url_request =
      CreateURLRequest(GURL("data:text/plain;foo"));

  mojom::URLResponseHeadPtr response_head = CreateCacheableURLResponseHead();

  ASSERT_FALSE(
      WriterWillBeCreatedToStoreResponse(url_request.get(), response_head));
}

TEST_F(NetworkServiceMemoryCacheTest, CreateWriter_MethodIsNotGet) {
  std::unique_ptr<net::URLRequest> url_request =
      CreateURLRequest(test_server().GetURL("/cacheable"));
  url_request->set_method("POST");

  mojom::URLResponseHeadPtr response_head = CreateCacheableURLResponseHead();

  ASSERT_FALSE(
      WriterWillBeCreatedToStoreResponse(url_request.get(), response_head));
}

TEST_F(NetworkServiceMemoryCacheTest,
       CreateWriter_NetworkIsolationKeyIsTransient) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      net::features::kSplitCacheByNetworkIsolationKey);

  std::unique_ptr<net::URLRequest> url_request =
      CreateURLRequest(test_server().GetURL("/cacheable"));
  url_request->set_isolation_info(net::IsolationInfo::CreateTransient());

  mojom::URLResponseHeadPtr response_head = CreateCacheableURLResponseHead();

  ASSERT_FALSE(
      WriterWillBeCreatedToStoreResponse(url_request.get(), response_head));
}

TEST_F(NetworkServiceMemoryCacheTest, CreateWriter_StatusCodeIsNotOK) {
  std::unique_ptr<net::URLRequest> url_request =
      CreateURLRequest(test_server().GetURL("/cacheable"));

  mojom::URLResponseHeadPtr response_head =
      CreateURLResponseHead(net::HTTP_BAD_REQUEST);

  ASSERT_FALSE(
      WriterWillBeCreatedToStoreResponse(url_request.get(), response_head));
}

TEST_F(NetworkServiceMemoryCacheTest, CreateWriter_EmptyResponse) {
  std::unique_ptr<net::URLRequest> url_request =
      CreateURLRequest(test_server().GetURL("/cacheable"));

  auto response_head = mojom::URLResponseHead::New();

  ASSERT_FALSE(
      WriterWillBeCreatedToStoreResponse(url_request.get(), response_head));
}

TEST_F(NetworkServiceMemoryCacheTest, CreateWriter_BypassCache) {
  std::unique_ptr<net::URLRequest> url_request =
      CreateURLRequest(test_server().GetURL("/cacheable"));
  url_request->SetLoadFlags(net::LOAD_BYPASS_CACHE);

  mojom::URLResponseHeadPtr response_head = CreateCacheableURLResponseHead();

  ASSERT_FALSE(
      WriterWillBeCreatedToStoreResponse(url_request.get(), response_head));
}

TEST_F(NetworkServiceMemoryCacheTest, CreateWriter_DisableCache) {
  std::unique_ptr<net::URLRequest> url_request =
      CreateURLRequest(test_server().GetURL("/cacheable"));
  url_request->SetLoadFlags(net::LOAD_DISABLE_CACHE);

  mojom::URLResponseHeadPtr response_head = CreateCacheableURLResponseHead();

  ASSERT_FALSE(
      WriterWillBeCreatedToStoreResponse(url_request.get(), response_head));
}

TEST_F(NetworkServiceMemoryCacheTest, CreateWriter_ResponseNotCacheable) {
  std::unique_ptr<net::URLRequest> url_request =
      CreateURLRequest(test_server().GetURL("/cacheable"));

  mojom::URLResponseHeadPtr response_head = CreateCacheableURLResponseHead();
  response_head->headers->RemoveHeader("cache-control");
  response_head->headers->AddHeader("cache-control", "no-store");

  ASSERT_FALSE(
      WriterWillBeCreatedToStoreResponse(url_request.get(), response_head));
}

TEST_F(NetworkServiceMemoryCacheTest, CreateWriter_IfUnmodifiedSince) {
  std::unique_ptr<net::URLRequest> url_request =
      CreateURLRequest(test_server().GetURL("/cacheable"));
  url_request->SetExtraRequestHeaderByName("iF-unMOdified-since", "hello",
                                           /*overwrite=*/true);

  mojom::URLResponseHeadPtr response_head = CreateCacheableURLResponseHead();
  ASSERT_FALSE(
      WriterWillBeCreatedToStoreResponse(url_request.get(), response_head));
}

TEST_F(NetworkServiceMemoryCacheTest, CreateWriter_IfMatch) {
  std::unique_ptr<net::URLRequest> url_request =
      CreateURLRequest(test_server().GetURL("/cacheable"));
  url_request->SetExtraRequestHeaderByName("IF-match", "foo",
                                           /*overwrite=*/true);

  mojom::URLResponseHeadPtr response_head = CreateCacheableURLResponseHead();
  ASSERT_FALSE(
      WriterWillBeCreatedToStoreResponse(url_request.get(), response_head));
}

TEST_F(NetworkServiceMemoryCacheTest, CreateWriter_IfRange) {
  std::unique_ptr<net::URLRequest> url_request =
      CreateURLRequest(test_server().GetURL("/cacheable"));
  url_request->SetExtraRequestHeaderByName("if-rangE", "bar",
                                           /*overwrite=*/true);

  mojom::URLResponseHeadPtr response_head = CreateCacheableURLResponseHead();
  ASSERT_FALSE(
      WriterWillBeCreatedToStoreResponse(url_request.get(), response_head));
}

TEST_F(NetworkServiceMemoryCacheTest, CreateWriter_IfModifiedSince) {
  std::unique_ptr<net::URLRequest> url_request =
      CreateURLRequest(test_server().GetURL("/cacheable"));
  url_request->SetExtraRequestHeaderByName("if-modified-since", "bar",
                                           /*overwrite=*/true);

  mojom::URLResponseHeadPtr response_head = CreateCacheableURLResponseHead();
  ASSERT_FALSE(
      WriterWillBeCreatedToStoreResponse(url_request.get(), response_head));
}

TEST_F(NetworkServiceMemoryCacheTest, CreateWriter_IfNoneMatch) {
  std::unique_ptr<net::URLRequest> url_request =
      CreateURLRequest(test_server().GetURL("/cacheable"));
  url_request->SetExtraRequestHeaderByName("if-none-match", "bar",
                                           /*overwrite=*/true);

  mojom::URLResponseHeadPtr response_head = CreateCacheableURLResponseHead();
  ASSERT_FALSE(
      WriterWillBeCreatedToStoreResponse(url_request.get(), response_head));
}

TEST_F(NetworkServiceMemoryCacheTest, CacheControlBogus) {
  std::unique_ptr<net::URLRequest> url_request =
      CreateURLRequest(test_server().GetURL("/cacheable"));
  url_request->SetExtraRequestHeaderByName("cache-control", "bogus",
                                           /*overwrite=*/true);

  mojom::URLResponseHeadPtr response_head = CreateCacheableURLResponseHead();
  ASSERT_TRUE(
      WriterWillBeCreatedToStoreResponse(url_request.get(), response_head));
}

TEST_F(NetworkServiceMemoryCacheTest, CacheControlNoCache) {
  std::unique_ptr<net::URLRequest> url_request =
      CreateURLRequest(test_server().GetURL("/cacheable"));
  url_request->SetExtraRequestHeaderByName("cache-control", "no-cache",
                                           /*overwrite=*/true);

  mojom::URLResponseHeadPtr response_head = CreateCacheableURLResponseHead();
  ASSERT_FALSE(
      WriterWillBeCreatedToStoreResponse(url_request.get(), response_head));
}

TEST_F(NetworkServiceMemoryCacheTest, PragmaNoCatche) {
  std::unique_ptr<net::URLRequest> url_request =
      CreateURLRequest(test_server().GetURL("/cacheable"));
  url_request->SetExtraRequestHeaderByName("pragma", "no-cache",
                                           /*overwrite=*/true);

  mojom::URLResponseHeadPtr response_head = CreateCacheableURLResponseHead();
  ASSERT_FALSE(
      WriterWillBeCreatedToStoreResponse(url_request.get(), response_head));
}

TEST_F(NetworkServiceMemoryCacheTest, CanServe_Basic) {
  ResourceRequest request = CreateRequest("/cacheable");
  StoreResponseToMemoryCache(request);

  ASSERT_TRUE(CanServeFromMemoryCache(request));
}

TEST_F(NetworkServiceMemoryCacheTest, CanServe_NetworkIsolationKeyIsTransient) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      net::features::kSplitCacheByNetworkIsolationKey);

  ResourceRequest request = CreateRequest("/cacheable");
  StoreResponseToMemoryCache(request);

  ASSERT_TRUE(CanServeFromMemoryCache(request));
  ASSERT_FALSE(CanServeFromMemoryCache(
      request, net::NetworkIsolationKey::CreateTransientForTesting()));
}

TEST_F(NetworkServiceMemoryCacheTest, CanServe_InvalidURL) {
  ResourceRequest request;
  request.url = GURL();

  ASSERT_FALSE(CanServeFromMemoryCache(request));
}

TEST_F(NetworkServiceMemoryCacheTest, CanServe_SchemeIsNeitherHTTPNorHTTPS) {
  ResourceRequest request;
  request.url = GURL("data:text/plain;foo");

  ASSERT_FALSE(CanServeFromMemoryCache(request));
}

TEST_F(NetworkServiceMemoryCacheTest, CanServe_MethodIsNotGet) {
  ResourceRequest request = CreateRequest("/cacheable");
  request.method = net::HttpRequestHeaders::kPostMethod;

  ASSERT_FALSE(CanServeFromMemoryCache(request));
}

TEST_F(NetworkServiceMemoryCacheTest, CanServe_BypassCache) {
  ResourceRequest request = CreateRequest("/cacheable");
  request.load_flags |= net::LOAD_BYPASS_CACHE;

  ASSERT_FALSE(CanServeFromMemoryCache(request));
}

TEST_F(NetworkServiceMemoryCacheTest, CanServe_DisableCache) {
  ResourceRequest request = CreateRequest("/cacheable");
  request.load_flags |= net::LOAD_DISABLE_CACHE;

  ASSERT_FALSE(CanServeFromMemoryCache(request));
}

TEST_F(NetworkServiceMemoryCacheTest, CanServe_ValidateCache) {
  ResourceRequest request = CreateRequest("/cacheable");
  request.load_flags |= net::LOAD_VALIDATE_CACHE;

  ASSERT_FALSE(CanServeFromMemoryCache(request));
}

TEST_F(NetworkServiceMemoryCacheTest, CanServe_BlockedByRequestHeaders) {
  constexpr const char* kSpecialHeaders[][2] = {
      {"if-Unmodified-since", "foo"},
      {"if-mAtch", "foo"},
      {"if-raNge", "foo"},
      {"if-modiFied-since", "foo"},
      {"IF-NONE-MATCH", "foo"},
      {"cachE-control", "no-cache"},
      {"praGma", "no-cache"},
      {"Cache-Control", "max-age=0"},
      {"Range", "bytes=0-"},
  };

  // Store a response to the in-memory cache first.
  {
    ResourceRequest request = CreateRequest("/cacheable");
    StoreResponseToMemoryCache(request);
  }

  for (const auto& [name, value] : kSpecialHeaders) {
    SCOPED_TRACE(base::StringPrintf("header='%s', value='%s'", name, value));
    ResourceRequest request = CreateRequest("/cacheable");
    request.headers.SetHeader(name, value);
    ASSERT_FALSE(CanServeFromMemoryCache(request));
  }
}

TEST_F(NetworkServiceMemoryCacheTest, CanServe_Expired) {
  const uint64_t kMaxAge = 60;
  ResourceRequest request =
      CreateRequest(base::StringPrintf("/cacheable?max-age=%" PRId64, kMaxAge));
  StoreResponseToMemoryCache(request);

  ASSERT_TRUE(CanServeFromMemoryCache(request));

  // The response has `max-age=60`. Set the current time 61 seconds later to
  // make the cached response stale.
  memory_cache().SetCurrentTimeForTesting(base::Time::Now() +
                                          base::Seconds(kMaxAge + 1));
  ASSERT_FALSE(CanServeFromMemoryCache(request));
}

TEST_F(NetworkServiceMemoryCacheTest, CanServe_ResponseTooLarge) {
  ResourceRequest request = CreateRequest(
      base::StringPrintf("/cacheable?body-size=%d", kMaxPerEntrySize + 1));
  StoreResponseToMemoryCache(request);

  ASSERT_FALSE(CanServeFromMemoryCache(request));
}

TEST_F(NetworkServiceMemoryCacheTest,
       CanServe_ResponseTooLargeWithoutContentLength) {
  ResourceRequest request = CreateRequest(base::StringPrintf(
      "/cacheable_without_content_length?body-size=%d", kMaxPerEntrySize + 1));
  StoreResponseToMemoryCache(request);

  ASSERT_FALSE(CanServeFromMemoryCache(request));
}

TEST_F(NetworkServiceMemoryCacheTest,
       CanServe_SplitCacheByNetworkIsolationKeyEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      net::features::kSplitCacheByNetworkIsolationKey);

  ResourceRequest request = CreateRequest("/cacheable");
  StoreResponseToMemoryCache(request);

  net::SchemefulSite same_site(request.url);
  net::NetworkIsolationKey same_site_network_isolation_key(
      /*top_frame_site=*/same_site, /*frame_site=*/same_site);
  ASSERT_TRUE(
      CanServeFromMemoryCache(request, same_site_network_isolation_key));

  net::SchemefulSite other_site(GURL("https://example.test"));
  net::NetworkIsolationKey other_site_network_isolation_key(
      /*top_frame_site=*/same_site, /*frame_site=*/other_site);
  ASSERT_FALSE(
      CanServeFromMemoryCache(request, other_site_network_isolation_key));
}

TEST_F(NetworkServiceMemoryCacheTest, CanServe_CorpBlocked) {
  ResourceRequest request = CreateRequest("/cacheable");
  StoreResponseToMemoryCache(request);

  request.mode = mojom::RequestMode::kNoCors;
  request.request_initiator =
      url::Origin::Create(GURL("https://other-origin.test/"));

  net::SchemefulSite site(request.url);
  net::NetworkIsolationKey network_isolation_key(/*top_frame_site=*/site,
                                                 /*frame_site=*/site);

  CrossOriginEmbedderPolicy cross_origin_embedder_policy;
  cross_origin_embedder_policy.value =
      mojom::CrossOriginEmbedderPolicyValue::kRequireCorp;

  ASSERT_FALSE(CanServeFromMemoryCache(request, network_isolation_key,
                                       cross_origin_embedder_policy));
}

TEST_F(NetworkServiceMemoryCacheTest, CanServe_CorbBlockedNoSniff) {
  ResourceRequest request = CreateRequest("/corb_nosniff");
  StoreResponseToMemoryCache(request);
  ASSERT_TRUE(CanServeFromMemoryCache(request));

  const auto other_origin =
      url::Origin::Create(GURL("https://other-origin.test"));
  net::SchemefulSite other_site(other_origin);
  net::NetworkIsolationKey network_isolation_key(
      /*top_frame_site=*/other_site, /*frame_site=*/other_site);

  request.request_initiator = other_origin;

  ASSERT_FALSE(CanServeFromMemoryCache(request, network_isolation_key));
}

TEST_F(NetworkServiceMemoryCacheTest, CanServe_CorbBlockedSniff) {
  ResourceRequest request = CreateRequest("/corb_sniff");
  StoreResponseToMemoryCache(request);
  ASSERT_TRUE(CanServeFromMemoryCache(request));

  const auto other_origin =
      url::Origin::Create(GURL("https://other-origin.test"));
  net::SchemefulSite other_site(other_origin);
  net::NetworkIsolationKey network_isolation_key(
      /*top_frame_site=*/other_site, /*frame_site=*/other_site);

  request.request_initiator = other_origin;

  ASSERT_FALSE(CanServeFromMemoryCache(request, network_isolation_key));
}

TEST_F(NetworkServiceMemoryCacheTest, CanServe_VaryHeaderAcceptEncoding) {
  // The `/echoheadercache` handler sends `Vary: foo` header.
  ResourceRequest request = CreateRequest("/echoheadercache?Accept-Encoding");
  request.headers.SetHeader("accept-encoding", "gzip");

  StoreResponseToMemoryCache(request);
  ASSERT_TRUE(CanServeFromMemoryCache(request));

  // Stored response should not be served when the header that is specified in
  // `Vary` has different value.
  request.headers.SetHeader("accept-encoding", "br");
  ASSERT_FALSE(CanServeFromMemoryCache(request));

  // Specifying the net::LOAD_SKIP_VARY_CHECK flag skips Vary header checks.
  request.load_flags |= net::LOAD_SKIP_VARY_CHECK;
  ASSERT_TRUE(CanServeFromMemoryCache(request));
}

TEST_F(NetworkServiceMemoryCacheTest, CanServe_MultipleVaryHeader) {
  ResourceRequest request =
      CreateRequest("/echoheadercache?Accept-Encoding,Origin");
  request.headers.SetHeader("accept-encoding", "gzip");
  request.headers.SetHeader("origin", "https://a.test");

  StoreResponseToMemoryCache(request);
  ASSERT_TRUE(CanServeFromMemoryCache(request));

  request.headers.SetHeader("origin", "https://b.test");
  ASSERT_FALSE(CanServeFromMemoryCache(request));
}

// TODO(https://crbug.com/1339708): Change the test name and the expectation
// once we implement appropriate Vary checks.
TEST_F(NetworkServiceMemoryCacheTest, CanServe_UnsupportedVaryHeaderCookie) {
  ResourceRequest request = CreateRequest("/echoheadercache?Cookie");
  request.headers.SetHeader("cookie", "foo");

  StoreResponseToMemoryCache(request);
  ASSERT_FALSE(CanServeFromMemoryCache(request));
}

TEST_F(NetworkServiceMemoryCacheTest, CanServe_UnsupportedMultipleVaryHeader) {
  ResourceRequest request =
      CreateRequest("/echoheadercache?Accept-Encoding,X-Foo");
  request.headers.SetHeader("accept-encoding", "gzip");
  request.headers.SetHeader("x-foo", "bar");

  StoreResponseToMemoryCache(request);
  ASSERT_FALSE(CanServeFromMemoryCache(request));
}

TEST_F(NetworkServiceMemoryCacheTest, CanServe_DevToolsAttached) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kPartitionedCookies);

  ResourceRequest request = CreateRequest("/cacheable?max-age=120");
  request.devtools_request_id = "fake-id";
  StoreResponseToMemoryCache(request);

  MockDevToolsObserver devtools_observer;
  request.trusted_params = ResourceRequest::TrustedParams();
  request.trusted_params->devtools_observer = devtools_observer.Bind();

  LoaderPair loader_pair = CreateLoaderAndStart(request);
  loader_pair.client->RunUntilComplete();
  const URLLoaderCompletionStatus& status =
      loader_pair.client->completion_status();
  ASSERT_EQ(status.error_code, net::OK);
  ASSERT_TRUE(status.exists_in_memory_cache);

  devtools_observer.WaitUntilRawResponse(0u);
  ASSERT_EQ(200, devtools_observer.raw_response_http_status_code());

  // Check whether the cached response has `Cache-Control: max-age=120` as the
  // original response had.
  bool has_expected_header = false;
  for (const auto& header_pair : devtools_observer.response_headers()) {
    if (base::EqualsCaseInsensitiveASCII(header_pair->key, "cache-control") &&
        header_pair->value == "max-age=120") {
      has_expected_header = true;
      break;
    }
  }
  ASSERT_TRUE(has_expected_header);

  EXPECT_EQ(net::CookiePartitionKey::FromURLForTesting(request.url),
            devtools_observer.response_cookie_partition_key());
}

TEST_F(NetworkServiceMemoryCacheTest, CanServe_ClientSecurityStateProvided) {
  ResourceRequest request = CreateRequest("/cacheable");
  StoreResponseToMemoryCache(request);

  request.trusted_params = ResourceRequest::TrustedParams();
  request.trusted_params->client_security_state =
      mojom::ClientSecurityState::New();

  // This should not hit any (D)CHECKs.
  LoaderPair loader_pair = CreateLoaderAndStart(request);
  loader_pair.client->RunUntilComplete();
  const URLLoaderCompletionStatus& status =
      loader_pair.client->completion_status();
  ASSERT_EQ(status.error_code, net::OK);
  ASSERT_TRUE(status.exists_in_memory_cache);
}

TEST_F(NetworkServiceMemoryCacheTest, UpdateStoredCache) {
  ResourceRequest request = CreateRequest("/cacheable");

  StoreResponseToMemoryCache(request);

  net::SchemefulSite site(request.url);
  net::NetworkIsolationKey network_isolation_key(/*top_frame_site=*/site,
                                                 /*frame_site=*/site);

  absl::optional<std::string> cache_key = memory_cache().CanServe(
      mojom::kURLLoadOptionNone, request, network_isolation_key,
      CrossOriginEmbedderPolicy(),
      /*client_security_state=*/nullptr);
  ASSERT_TRUE(cache_key.has_value());
  mojom::URLResponseHeadPtr response =
      memory_cache().GetResponseHeadForTesting(*cache_key);
  base::Time first_response_time = response->response_time;

  // Store the same response again. Force validation to update the response.
  request.load_flags |= net::LOAD_VALIDATE_CACHE;
  StoreResponseToMemoryCache(request);
  response = memory_cache().GetResponseHeadForTesting(*cache_key);
  base::Time second_response_time = response->response_time;

  // Compare response time to make sure the stored response is updated.
  ASSERT_LT(first_response_time, second_response_time);
}

TEST_F(NetworkServiceMemoryCacheTest, EvictLeastRecentlyUsed) {
  constexpr size_t kBodySize = kMaxPerEntrySize;

  // Stores two responses to consume the full budget of the in-memory cache.
  ResourceRequest request1 = CreateRequest(
      base::StringPrintf("/cacheable?id=1&body-size=%zu", kBodySize));
  StoreResponseToMemoryCache(request1);

  ResourceRequest request2 = CreateRequest(
      base::StringPrintf("/cacheable?id=2&body-size=%zu", kBodySize));
  StoreResponseToMemoryCache(request2);

  ASSERT_TRUE(CanServeFromMemoryCache(request1));
  ASSERT_TRUE(CanServeFromMemoryCache(request2));

  // Stores the third response. It should evict the first stored response.
  ResourceRequest request3 = CreateRequest(
      base::StringPrintf("/cacheable?id=3&body-size=%zu", kBodySize));
  StoreResponseToMemoryCache(request3);

  ASSERT_FALSE(CanServeFromMemoryCache(request1));
  ASSERT_TRUE(CanServeFromMemoryCache(request2));
  ASSERT_TRUE(CanServeFromMemoryCache(request3));
  ASSERT_EQ(memory_cache().total_bytes(), kBodySize * 2);
}

// Tests that a stored response is deleted when a subsequent request that
// bypasses the cache results in a redirect.
TEST_F(NetworkServiceMemoryCacheTest, CachedAfterRedirect) {
  ResourceRequest request = CreateRequest("/cacheable_or_redirect");

  StoreResponseToMemoryCache(request);
  ASSERT_TRUE(CanServeFromMemoryCache(request));

  MakeCacheableHandlerSendRedirect();
  request.load_flags |= net::LOAD_BYPASS_CACHE;

  LoaderPair pair = CreateLoaderAndStart(request);
  pair.client->RunUntilRedirectReceived();
  pair.loader_remote->FollowRedirect(
      /*removed_headers=*/{}, /*modified_headers=*/{},
      /*modified_cors_exempt_headers=*/{}, /*new_url=*/absl::nullopt);
  pair.client->RunUntilComplete();

  request.load_flags &= ~net::LOAD_BYPASS_CACHE;
  ASSERT_FALSE(CanServeFromMemoryCache(request));
}

TEST_F(NetworkServiceMemoryCacheTest, Clear) {
  constexpr int kBodySize = 64;

  // Stores three responses.
  ResourceRequest request1 = CreateRequest(
      base::StringPrintf("/cacheable?id=1&body-size=%d", kBodySize));
  StoreResponseToMemoryCache(request1);

  ResourceRequest request2 = CreateRequest(
      base::StringPrintf("/cacheable?id=2&body-size=%d", kBodySize));
  StoreResponseToMemoryCache(request2);

  ResourceRequest request3 = CreateRequest(
      base::StringPrintf("/cacheable?id=3&body-size=%d", kBodySize));
  StoreResponseToMemoryCache(request3);

  ASSERT_TRUE(CanServeFromMemoryCache(request1));
  ASSERT_TRUE(CanServeFromMemoryCache(request2));
  ASSERT_TRUE(CanServeFromMemoryCache(request3));

  memory_cache().Clear();

  ASSERT_EQ(memory_cache().total_bytes(), 0u);
  ASSERT_FALSE(CanServeFromMemoryCache(request1));
  ASSERT_FALSE(CanServeFromMemoryCache(request2));
  ASSERT_FALSE(CanServeFromMemoryCache(request3));
}

TEST_F(NetworkServiceMemoryCacheTest, ClearOnMemoryPressure) {
  ResourceRequest request = CreateRequest("/cacheable");
  StoreResponseToMemoryCache(request);
  ASSERT_TRUE(CanServeFromMemoryCache(request));

  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel::
          MEMORY_PRESSURE_LEVEL_CRITICAL);
  task_environment().RunUntilIdle();

  ASSERT_EQ(memory_cache().total_bytes(), 0u);
  ASSERT_FALSE(CanServeFromMemoryCache(request));
}

TEST_F(NetworkServiceMemoryCacheTest, ClientDisconnectedWhileCaching) {
  ResourceRequest request = CreateRequest("/cacheable");
  LoaderPair pair = CreateLoaderAndStart(request);

  pair.client->RunUntilResponseReceived();
  pair.client->Unbind();

  ASSERT_TRUE(pair.loader_remote.is_connected());
  base::RunLoop loop;
  pair.loader_remote.set_disconnect_handler(loop.QuitClosure());
  loop.Run();

  ASSERT_FALSE(CanServeFromMemoryCache(request));
}

TEST_F(NetworkServiceMemoryCacheTest, ServeFromCache_Basic) {
  constexpr int kBodySize = 371;
  const std::string kExpectedBody(kBodySize, 'a');

  ResourceRequest request =
      CreateRequest(base::StringPrintf("/cacheable?body-size=%d", kBodySize));
  StoreResponseToMemoryCache(request);

  const base::TimeTicks before_start = base::TimeTicks::Now();

  LoaderPair pair = CreateLoaderAndStart(request);
  pair.client->RunUntilComplete();
  const URLLoaderCompletionStatus& status = pair.client->completion_status();
  ASSERT_EQ(status.error_code, net::OK);
  ASSERT_TRUE(status.exists_in_memory_cache);

  const mojom::URLResponseHeadPtr& response = pair.client->response_head();
  ASSERT_FALSE(response->network_accessed);
  ASSERT_FALSE(response->is_validated);
  ASSERT_TRUE(response->was_fetched_via_cache);
  ASSERT_LT(before_start, response->request_start);
  ASSERT_LT(before_start, response->response_start);

  const net::LoadTimingInfo& load_timing = response->load_timing;
  ASSERT_LT(before_start, load_timing.request_start);
  ASSERT_LT(before_start, load_timing.send_start);
  ASSERT_LT(before_start, load_timing.send_end);
  ASSERT_LT(before_start, load_timing.receive_headers_start);
  ASSERT_LT(before_start, load_timing.receive_headers_end);

  std::string received_body;
  ASSERT_TRUE(mojo::BlockingCopyToString(pair.client->response_body_release(),
                                         &received_body));
  ASSERT_EQ(kExpectedBody, received_body);
}

TEST_F(NetworkServiceMemoryCacheTest, ServeFromCache_DisableLoadTiming) {
  ResourceRequest request = CreateRequest("/cacheable");
  StoreResponseToMemoryCache(request);

  const base::TimeTicks before_start = base::TimeTicks::Now();

  request.enable_load_timing = false;
  LoaderPair pair = CreateLoaderAndStart(request);
  pair.client->RunUntilComplete();
  const URLLoaderCompletionStatus& status = pair.client->completion_status();
  ASSERT_EQ(status.error_code, net::OK);
  ASSERT_TRUE(status.exists_in_memory_cache);

  const mojom::URLResponseHeadPtr& response = pair.client->response_head();
  ASSERT_FALSE(response->network_accessed);
  ASSERT_FALSE(response->is_validated);
  ASSERT_TRUE(response->was_fetched_via_cache);
  ASSERT_LT(before_start, response->request_start);
  ASSERT_LT(before_start, response->response_start);

  const net::LoadTimingInfo& load_timing = response->load_timing;
  ASSERT_TRUE(load_timing.request_start.is_null());
  ASSERT_TRUE(load_timing.send_start.is_null());
  ASSERT_TRUE(load_timing.send_end.is_null());
  ASSERT_TRUE(load_timing.receive_headers_start.is_null());
  ASSERT_TRUE(load_timing.receive_headers_end.is_null());
}

TEST_F(NetworkServiceMemoryCacheTest, ServeFromCache_LargeBody) {
  constexpr uint32_t kReadDataSize = 512;
  // Arbitrary response body size larger than `kReadDataSize`.
  constexpr int kBodySize = 2 * 1024 + 659;
  DCHECK_GE(kMaxPerEntrySize, kBodySize);

  ResourceRequest request =
      CreateRequest(base::StringPrintf("/cacheable?body-size=%d", kBodySize));
  StoreResponseToMemoryCache(request);

  LoaderPair pair = CreateLoaderAndStart(request);
  pair.client->RunUntilResponseReceived();

  mojo::ScopedDataPipeConsumerHandle consumer_handle =
      pair.client->response_body_release();
  std::string received_body;
  while (true) {
    char buf[kReadDataSize];
    uint32_t num_bytes = kReadDataSize;
    MojoResult result =
        consumer_handle->ReadData(buf, &num_bytes, MOJO_READ_DATA_FLAG_NONE);

    if (result == MOJO_RESULT_SHOULD_WAIT) {
      base::RunLoop run_loop;
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, run_loop.QuitClosure());
      run_loop.Run();
      continue;
    }

    if (result == MOJO_RESULT_FAILED_PRECONDITION)
      break;

    ASSERT_EQ(result, MOJO_RESULT_OK);
    received_body.append(buf, num_bytes);
  }

  pair.client->RunUntilComplete();
  const URLLoaderCompletionStatus& status = pair.client->completion_status();
  ASSERT_EQ(status.error_code, net::OK);
  ASSERT_TRUE(status.exists_in_memory_cache);

  const std::string kExpectedBody(kBodySize, 'a');
  ASSERT_EQ(kExpectedBody, received_body);
}

TEST_F(NetworkServiceMemoryCacheTest,
       ServeFromCache_DataPipeDisconnectWhileReading) {
  constexpr int kBodySize = 512;
  constexpr int kReadDataSize = kBodySize / 2;
  ResourceRequest request =
      CreateRequest(base::StringPrintf("/cacheable?body-size=%d", kBodySize));
  StoreResponseToMemoryCache(request);

  // Set a small data pipe capacity so that writing data to a data pipe doesn't
  // complete at once.
  memory_cache().SetDataPipeCapacityForTesting(kReadDataSize);

  LoaderPair pair = CreateLoaderAndStart(request);
  pair.client->RunUntilResponseReceived();

  mojo::ScopedDataPipeConsumerHandle consumer_handle =
      pair.client->response_body_release();

  // Read the half of the response body.
  int num_read = 0;
  while (num_read < kReadDataSize) {
    char buf[kReadDataSize];
    uint32_t num_bytes = kReadDataSize;
    MojoResult result =
        consumer_handle->ReadData(buf, &num_bytes, MOJO_READ_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      base::RunLoop run_loop;
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, run_loop.QuitClosure());
      run_loop.Run();
      continue;
    }
    ASSERT_EQ(result, MOJO_RESULT_OK);
    num_read += num_bytes;
  }

  consumer_handle.reset();
  pair.client->RunUntilComplete();
  ASSERT_EQ(pair.client->completion_status().error_code, net::ERR_FAILED);
}

TEST_F(NetworkServiceMemoryCacheTest, ServeFromCache_GzipResponse) {
  constexpr int64_t kBodySize = 100;
  const std::string kContent(kBodySize, 'x');
  ResourceRequest request =
      CreateRequest(base::StringPrintf("/gzip-body?%s", kContent.c_str()));
  StoreResponseToMemoryCache(request);
  ASSERT_TRUE(CanServeFromMemoryCache(request));

  LoaderPair pair = CreateLoaderAndStart(request);
  pair.client->RunUntilComplete();
  const URLLoaderCompletionStatus& status = pair.client->completion_status();
  ASSERT_EQ(status.error_code, net::OK);
  ASSERT_EQ(status.decoded_body_length, kBodySize);
  ASSERT_LT(status.encoded_body_length, kBodySize);
}

TEST_F(NetworkServiceMemoryCacheTest, InvalidateOnNonSafeMethod) {
  ResourceRequest request = CreateRequest("/cacheable");
  StoreResponseToMemoryCache(request);

  ASSERT_TRUE(CanServeFromMemoryCache(request));

  ResourceRequest request2 = CreateRequest("/cacheable");
  request2.method = "PUT";

  LoaderPair pair = CreateLoaderAndStart(request2);
  pair.client->RunUntilResponseReceived();

  ASSERT_FALSE(CanServeFromMemoryCache(request));
}

TEST_F(NetworkServiceMemoryCacheWithFactoryOverrideTest,
       MemoryCacheShouldNotBeUsed) {
  ResourceRequest request = CreateRequest(base::StringPrintf("/cacheable"));
  StoreResponseToMemoryCache(request);

  ASSERT_FALSE(CanServeFromMemoryCache(request));
}

}  // namespace network
