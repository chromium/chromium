// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/child_process_id_util.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/resource_load_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/base/features.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_request_headers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/basic_data_buffer_factory.h"
#include "services/network/public/cpp/data_buffer_factory.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_http_cache_client.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "services/network/public/mojom/shared_http_cache_client.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_client.h"

namespace content {
namespace {

std::string GetStringFromBuffers(
    const std::unique_ptr<network::DataBufferList>& buffers) {
  std::string result_string;
  for (auto buffer : *buffers) {
    result_string.append(reinterpret_cast<const char*>(buffer.data()),
                         buffer.size());
  }
  return result_string;
}

std::unique_ptr<net::test_server::BasicHttpResponse> CreateScriptResponse(
    net::HttpStatusCode code = net::HTTP_OK) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(code);
  response->set_content_type("application/javascript");
  response->AddCustomHeader("Cache-Control", "max-age=3600");
  response->set_content("window._resolve('Done');");
  return response;
}

std::unique_ptr<net::test_server::HttpResponse> HandleCustomResponse(
    const net::test_server::HttpRequest& request) {
  GURL url = request.GetURL();
  std::string_view path = url.path();
  if (path == "/custom_resource/acao_wildcard.js") {
    auto response = CreateScriptResponse();
    response->AddCustomHeader("Access-Control-Allow-Origin", "*");
    return response;
  }
  if (path == "/custom_resource/acao_origin.js") {
    auto response = CreateScriptResponse();
    response->AddCustomHeader("Access-Control-Allow-Origin", "https://a.test");
    return response;
  }
  if (path == "/custom_resource/corp_same_origin.js") {
    auto response = CreateScriptResponse();
    response->AddCustomHeader("Cross-Origin-Resource-Policy", "same-origin");
    return response;
  }
  if (path == "/custom_resource/corp_cross_origin.js") {
    auto response = CreateScriptResponse();
    response->AddCustomHeader("Cross-Origin-Resource-Policy", "cross-origin");
    return response;
  }
  if (path == "/custom_resource/partial.js") {
    auto response = CreateScriptResponse(net::HTTP_PARTIAL_CONTENT);
    response->AddCustomHeader("Access-Control-Allow-Origin", "*");
    response->AddCustomHeader("Content-Range", "bytes 0-21/22");
    return response;
  }
  if (path == "/custom_resource/redirect.js") {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_FOUND);
    response->AddCustomHeader("Location", "/custom_resource/acao_wildcard.js");
    response->AddCustomHeader("Access-Control-Allow-Origin", "*");
    return response;
  }
  return nullptr;
}

class TestContentBrowserClient : public ContentBrowserTestContentBrowserClient {
 public:
  TestContentBrowserClient() = default;
  ~TestContentBrowserClient() override = default;

  void ConfigureNetworkContextParams(
      BrowserContext* context,
      bool in_memory,
      const base::FilePath& relative_partition_path,
      network::mojom::NetworkContextParams* network_context_params,
      cert_verifier::mojom::CertVerifierCreationParams*
          cert_verifier_creation_params) override {
    CHECK(network_context_params->http_cache_enabled);
    if (!in_memory) {
      network_context_params->file_paths =
          network::mojom::NetworkContextFilePaths::New();
      base::FilePath path = context->GetPath();
      if (!relative_partition_path.empty()) {
        path = path.Append(relative_partition_path);
      }
      path = path.AppendASCII("Cache");
      network_context_params->file_paths->http_cache_directory = path;
    }
    ContentBrowserTestContentBrowserClient::ConfigureNetworkContextParams(
        context, in_memory, relative_partition_path, network_context_params,
        cert_verifier_creation_params);
  }
};

class RendererAccessibleHttpCacheBrowserTestBase : public ContentBrowserTest {
 public:
  explicit RendererAccessibleHttpCacheBrowserTestBase(bool wal_mode) {
    base::FieldTrialParams shared_cache_params;
    shared_cache_params[net::features::kRendererAccessibleHttpCacheWalMode
                            .name] = wal_mode ? "true" : "false";

    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        // The renderer accessible HTTP Cache depends on the SQL disk cache
        // backend.
        {net::features::kDiskCacheBackendExperiment,
         {{net::features::kDiskCacheBackendParam.name, "sql"}}},
        {net::features::kRendererAccessibleHttpCache, shared_cache_params}};
    std::vector<base::test::FeatureRef> disabled_features = {
        // Need to disable kNetworkServiceInProcess feature to run the tests on
        // Android, as we crash the network service in SetUpOnMainThread() to
        // enable the on-disk HTTP Cache.
        features::kNetworkServiceInProcess,
        // Origin-keyed agent clusters are not eligible for the renderer
        // accessible HTTP cache. Disable kOriginKeyedProcessesByDefault so
        // HTTPS test origins remain site-keyed on builders like linux-oi-rel.
        features::kOriginKeyedProcessesByDefault};
    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);
  }
  ~RendererAccessibleHttpCacheBrowserTestBase() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_https_test_server().SetSSLConfig(
        net::EmbeddedTestServer::CERT_TEST_NAMES);
    embedded_https_test_server().RegisterRequestHandler(
        base::BindRepeating(&HandleCustomResponse));
    embedded_https_test_server().ServeFilesFromSourceDirectory(
        "content/test/data");
    ASSERT_TRUE(embedded_https_test_server().Start());
    ContentBrowserTest::SetUpOnMainThread();
    // When creating a network context,
    // ContentBrowserClient::ConfigureNetworkContextParams() is called, where
    // setting file_paths and http_cache_directory enables the on-disk HTTP
    // Cache. However, since ShellContentBrowserClient and the network context
    // are created within ContentBrowserTest::SetUp, it is not possible to
    // substitute it with TestContentBrowserClient during that timing.
    // Therefore, after creating TestContentBrowserClient, we trigger a Network
    // Service crash and recreate the network context to enable the on-disk HTTP
    // Cache.
    test_content_browser_client_ = std::make_unique<TestContentBrowserClient>();
    SimulateNetworkServiceCrash();
  }

  void TearDownOnMainThread() override { test_content_browser_client_.reset(); }

 protected:
  GURL GetURL(std::string_view path) {
    return embedded_https_test_server().GetURL("a.test", path);
  }

  network::mojom::NetworkContext* GetNetworkContext() {
    return shell()
        ->web_contents()
        ->GetBrowserContext()
        ->GetDefaultStoragePartition()
        ->GetNetworkContext();
  }

  void FlushSharedCacheEligibleEntries() {
    mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
    GetNetworkService()->BindTestInterfaceForTesting(
        network_service_test.BindNewPipeAndPassReceiver());
    base::RunLoop run_loop;
    network_service_test->ProcessSharedCacheEligibleEntriesForTesting(
        run_loop.QuitClosure());
    run_loop.Run();
  }

  void ReloadInNewProcess(const GURL& page_url) {
    DisableBFCacheForRFHForTesting(
        shell()->web_contents()->GetPrimaryMainFrame());
    EXPECT_TRUE(
        NavigateToURL(shell()->web_contents(), GetWebUIURL("blob-internals")));
    EXPECT_TRUE(NavigateToURL(shell()->web_contents(), page_url));
  }

  std::string LoadTestFile(const std::string_view path) {
    base::FilePath test_data_dir;
    CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir));
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string test_file_string;
    CHECK(base::ReadFileToString(
        test_data_dir.AppendASCII("content/test/data").AppendASCII(path),
        &test_file_string));
    return test_file_string;
  }

  void FetchScript(const GURL& resource_url) {
    std::string fetch_script = JsReplace(
        R"(
        new Promise(resolve => {
          window._resolve = resolve;
          const script = document.createElement('script');
          script.src = $1;
          document.body.appendChild(script);
        });
        )",
        resource_url);
    EXPECT_EQ("Done", EvalJs(shell()->web_contents(), fetch_script));
  }

  scoped_refptr<network::SharedHttpCacheClient> InitSharedHttpCacheClient(
      const GURL& page_url) {
    base::RunLoop run_loop_for_db_init;
    mojo::PendingRemote<network::mojom::SharedHttpCacheClientFactory>
        http_cache_client_remote;
    auto shared_http_cache_client =
        network::SharedHttpCacheClient::CreateAndInit(
            http_cache_client_remote.InitWithNewPipeAndPassReceiver(),
            base::ThreadPool::CreateSequencedTaskRunner(
                {base::TaskPriority::USER_BLOCKING}),
            base::ThreadPool::CreateSequencedTaskRunner(
                {base::TaskPriority::USER_BLOCKING, base::MayBlock()}),
            run_loop_for_db_init.QuitClosure());

    net::NetworkIsolationKey key{net::SchemefulSite(page_url),
                                 net::SchemefulSite(page_url)};
    GetNetworkContext()->RegisterHttpCacheClient(
        key, std::move(http_cache_client_remote));

    run_loop_for_db_init.Run();
    return shared_http_cache_client;
  }

  // Ensures the Shared Cache SQLite database is created for `page_url`'s
  // NetworkIsolationKey before initializing and returning a
  // SharedHttpCacheClient.
  //
  // `SqlSharedCache` delays database file creation and `CreateClient`
  // notification until at least one eligible entry is written and flushed. In
  // negative test cases where the tested resource is expected to be excluded
  // from the cache, `InitSharedHttpCacheClient` alone would block indefinitely
  // waiting for `CreateClient`. This helper fetches a known cacheable resource
  // first, flushes eligible entries, initializes the client, and verifies that
  // the control resource is present in the cache.
  scoped_refptr<network::SharedHttpCacheClient>
  EnsureSharedCacheCreatedAndInitClient(const GURL& page_url) {
    GURL valid_url = GetURL("/loader/cacheable_jsonp.js");
    FetchScript(valid_url);
    FlushSharedCacheEligibleEntries();
    auto client = InitSharedHttpCacheClient(page_url);
    EXPECT_TRUE(CheckResourceInSharedCache(client, valid_url));
    return client;
  }

  bool CheckResourceInSharedCache(
      scoped_refptr<network::SharedHttpCacheClient> shared_http_cache_client,
      const GURL& resource_url) {
    network::ResourceRequest request = CreateSubresourceRequest(resource_url);

    base::RunLoop run_loop_for_find;
    std::optional<network::SharedHttpCacheClient::Response> found_response;
    shared_http_cache_client->Find(
        request, base::MakeRefCounted<network::BasicDataBufferFactory>(),
        base::BindLambdaForTesting(
            [&](std::optional<network::SharedHttpCacheClient::Response>
                    response) {
              found_response = std::move(response);
              run_loop_for_find.Quit();
            }),
        base::SequencedTaskRunner::GetCurrentDefault());
    run_loop_for_find.Run();
    return found_response.has_value();
  }

  mojo::Remote<network::mojom::URLLoaderFactory> CreateDirectURLLoaderFactory(
      bool renderer_accessible_http_cache_write_enabled) {
    RenderFrameHost* frame = shell()->web_contents()->GetPrimaryMainFrame();
    const url::Origin& origin = frame->GetLastCommittedOrigin();
    net::IsolationInfo isolation_info = net::IsolationInfo::Create(
        net::IsolationInfo::RequestType::kOther, origin, origin,
        net::SiteForCookies::FromOrigin(origin));

    network::mojom::URLLoaderFactoryParamsPtr params =
        network::mojom::URLLoaderFactoryParams::New();
    params->process_id = ToOriginatingProcessId(frame->GetProcess()->GetID());
    params->request_initiator_origin_lock = origin;
    params->isolation_info = isolation_info;
    params->renderer_accessible_http_cache_write_enabled =
        renderer_accessible_http_cache_write_enabled;

    mojo::Remote<network::mojom::URLLoaderFactory> factory;
    GetNetworkContext()->CreateURLLoaderFactory(
        factory.BindNewPipeAndPassReceiver(), std::move(params));
    return factory;
  }

  network::ResourceRequest CreateSubresourceRequest(
      const GURL& url,
      std::string_view method = "GET") {
    network::ResourceRequest request;
    request.url = url;
    request.method = std::string(method);
    request.destination = network::mojom::RequestDestination::kScript;
    request.mode = network::mojom::RequestMode::kCors;
    request.credentials_mode = network::mojom::CredentialsMode::kInclude;
    request.request_initiator = shell()
                                    ->web_contents()
                                    ->GetPrimaryMainFrame()
                                    ->GetLastCommittedOrigin();
    return request;
  }

  int LoadRequest(network::mojom::URLLoaderFactory* factory,
                  const network::ResourceRequest& request) {
    mojo::Remote<network::mojom::URLLoader> loader;
    network::TestURLLoaderClient client;
    factory->CreateLoaderAndStart(
        loader.BindNewPipeAndPassReceiver(), 0,
        network::mojom::kURLLoadOptionNone, request, client.CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
    client.RunUntilComplete();
    return client.completion_status().error_code;
  }

 private:
  std::unique_ptr<TestContentBrowserClient> test_content_browser_client_;
  base::test::ScopedFeatureList feature_list_;
};

class RendererAccessibleHttpCacheBrowserTest
    : public RendererAccessibleHttpCacheBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  RendererAccessibleHttpCacheBrowserTest()
      : RendererAccessibleHttpCacheBrowserTestBase(GetParam()) {}
  ~RendererAccessibleHttpCacheBrowserTest() override = default;

  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    return info.param ? "WalEnabled" : "WalDisabled";
  }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    RendererAccessibleHttpCacheBrowserTest,
    testing::Bool(),
    &RendererAccessibleHttpCacheBrowserTest::DescribeParams);

IN_PROC_BROWSER_TEST_P(RendererAccessibleHttpCacheBrowserTest, Basic) {
  GURL page_url = GetURL("/loader/blank.html");
  EXPECT_TRUE(NavigateToURL(shell()->web_contents(), page_url));

  GURL resource_url = GetURL("/loader/cacheable_jsonp.js");
  FetchScript(resource_url);

  FlushSharedCacheEligibleEntries();
  auto shared_http_cache_client = InitSharedHttpCacheClient(page_url);

  network::ResourceRequest request = CreateSubresourceRequest(resource_url);

  base::RunLoop run_loop_for_find;
  std::optional<network::SharedHttpCacheClient::Response> found_response;
  shared_http_cache_client->Find(
      request, base::MakeRefCounted<network::BasicDataBufferFactory>(),
      base::BindLambdaForTesting(
          [&](std::optional<network::SharedHttpCacheClient::Response>
                  response) {
            found_response = std::move(response);
            run_loop_for_find.Quit();
          }),
      base::SequencedTaskRunner::GetCurrentDefault());
  run_loop_for_find.Run();
  ASSERT_TRUE(found_response.has_value());
  EXPECT_TRUE(found_response->head);
  EXPECT_TRUE(found_response->head->was_fetched_via_cache);
  EXPECT_FALSE(found_response->head->request_time.is_null());
  EXPECT_FALSE(found_response->head->response_time.is_null());
  EXPECT_LE(found_response->head->request_time,
            found_response->head->response_time);
  EXPECT_FALSE(found_response->head->load_timing.request_start.is_null());
  EXPECT_FALSE(found_response->head->load_timing.receive_headers_end.is_null());
  EXPECT_GE(found_response->head->load_timing.receive_headers_end,
            found_response->head->load_timing.request_start);
  ASSERT_TRUE(found_response->body);

  std::string expected_script_content =
      LoadTestFile("loader/cacheable_jsonp.js");
  EXPECT_EQ(GetStringFromBuffers(found_response->body),
            expected_script_content);

  ReloadInNewProcess(page_url);

  ResourceLoadObserver observer(shell()->web_contents());
  base::TimeTicks before = base::TimeTicks::Now();
  FetchScript(resource_url);
  base::TimeTicks after = base::TimeTicks::Now();

  ASSERT_EQ(1U, observer.resource_load_entries().size());
  const auto& entry = observer.resource_load_entries()[0];
  EXPECT_EQ(resource_url, entry.original_url);
  EXPECT_TRUE(entry.resource_load_info->was_cached);
  const net::LoadTimingInfo& timing =
      entry.resource_load_info->load_timing_info;
  EXPECT_FALSE(timing.request_start.is_null());
  EXPECT_FALSE(timing.receive_headers_end.is_null());
  EXPECT_LE(before, timing.request_start);
  EXPECT_LE(timing.request_start, timing.receive_headers_end);
  EXPECT_LE(timing.receive_headers_end, after);
  EXPECT_EQ(base::ByteSize(expected_script_content.size()),
            entry.resource_load_info->raw_body_bytes);
}

IN_PROC_BROWSER_TEST_P(RendererAccessibleHttpCacheBrowserTest,
                       NetworkServiceCrash) {
  GURL page_url = GetURL("/loader/blank.html");
  EXPECT_TRUE(NavigateToURL(shell()->web_contents(), page_url));

  RenderFrameHostImpl* frame = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  EXPECT_TRUE(frame->document_associated_data()
                  .renderer_accessible_http_cache_write_enabled());

  // Simulate a Network Service crash.
  SimulateNetworkServiceCrash();
  frame->FlushNetworkAndNavigationInterfacesForTesting(
      /*do_nothing_if_no_network_service_connection=*/false);

  // After crash recovery, DocumentAssociatedData preserves the flag so that
  // SubresourceLoaderFactoriesConfig::ForLastCommittedNavigation can restore it
  // on the recreated URLLoaderFactory.
  EXPECT_TRUE(frame->document_associated_data()
                  .renderer_accessible_http_cache_write_enabled());

  GURL resource_url = GetURL("/loader/cacheable_jsonp.js");
  FetchScript(resource_url);

  FlushSharedCacheEligibleEntries();
  auto shared_http_cache_client = InitSharedHttpCacheClient(page_url);

  network::ResourceRequest request = CreateSubresourceRequest(resource_url);

  base::RunLoop run_loop_for_find;
  std::optional<network::SharedHttpCacheClient::Response> found_response;
  shared_http_cache_client->Find(
      request, base::MakeRefCounted<network::BasicDataBufferFactory>(),
      base::BindLambdaForTesting(
          [&](std::optional<network::SharedHttpCacheClient::Response>
                  response) {
            found_response = std::move(response);
            run_loop_for_find.Quit();
          }),
      base::SequencedTaskRunner::GetCurrentDefault());
  run_loop_for_find.Run();
  ASSERT_TRUE(found_response.has_value());
  EXPECT_TRUE(found_response->head);
  EXPECT_TRUE(found_response->head->was_fetched_via_cache);
  EXPECT_FALSE(found_response->head->request_time.is_null());
  EXPECT_FALSE(found_response->head->response_time.is_null());
  EXPECT_LE(found_response->head->request_time,
            found_response->head->response_time);
  EXPECT_FALSE(found_response->head->load_timing.request_start.is_null());
  EXPECT_FALSE(found_response->head->load_timing.receive_headers_end.is_null());
  EXPECT_GE(found_response->head->load_timing.receive_headers_end,
            found_response->head->load_timing.request_start);
  ASSERT_TRUE(found_response->body);

  std::string expected_script_content =
      LoadTestFile("loader/cacheable_jsonp.js");
  EXPECT_EQ(GetStringFromBuffers(found_response->body),
            expected_script_content);

  ReloadInNewProcess(page_url);

  ResourceLoadObserver observer(shell()->web_contents());
  base::TimeTicks before = base::TimeTicks::Now();
  FetchScript(resource_url);
  base::TimeTicks after = base::TimeTicks::Now();

  ASSERT_EQ(1U, observer.resource_load_entries().size());
  const auto& entry = observer.resource_load_entries()[0];
  EXPECT_EQ(resource_url, entry.original_url);
  EXPECT_TRUE(entry.resource_load_info->was_cached);
  const net::LoadTimingInfo& timing =
      entry.resource_load_info->load_timing_info;
  EXPECT_FALSE(timing.request_start.is_null());
  EXPECT_FALSE(timing.receive_headers_end.is_null());
  EXPECT_LE(before, timing.request_start);
  EXPECT_LE(timing.request_start, timing.receive_headers_end);
  EXPECT_LE(timing.receive_headers_end, after);
  EXPECT_EQ(base::ByteSize(expected_script_content.size()),
            entry.resource_load_info->raw_body_bytes);
}

IN_PROC_BROWSER_TEST_P(RendererAccessibleHttpCacheBrowserTest,
                       AccessControlAllowOrigin) {
  GURL page_url = GetURL("/loader/blank.html");
  EXPECT_TRUE(NavigateToURL(shell()->web_contents(), page_url));

  GURL wildcard_url = GetURL("/custom_resource/acao_wildcard.js");
  GURL origin_url = GetURL("/custom_resource/acao_origin.js");

  FetchScript(wildcard_url);
  FetchScript(origin_url);

  FlushSharedCacheEligibleEntries();
  auto shared_http_cache_client = InitSharedHttpCacheClient(page_url);

  // Responses with Access-Control-Allow-Origin: * should be stored in the
  // Shared Cache.
  EXPECT_TRUE(
      CheckResourceInSharedCache(shared_http_cache_client, wildcard_url));

  // Responses with Access-Control-Allow-Origin: <origin> (not "*") must not be
  // stored in the Shared Cache.
  EXPECT_FALSE(
      CheckResourceInSharedCache(shared_http_cache_client, origin_url));

  ReloadInNewProcess(page_url);
  FetchScript(wildcard_url);
  FetchScript(origin_url);
}

IN_PROC_BROWSER_TEST_P(RendererAccessibleHttpCacheBrowserTest,
                       CrossOriginResourcePolicy) {
  GURL page_url = GetURL("/loader/blank.html");
  EXPECT_TRUE(NavigateToURL(shell()->web_contents(), page_url));

  GURL cross_origin_url = GetURL("/custom_resource/corp_cross_origin.js");
  GURL same_origin_url = GetURL("/custom_resource/corp_same_origin.js");

  FetchScript(cross_origin_url);
  FetchScript(same_origin_url);

  FlushSharedCacheEligibleEntries();
  auto shared_http_cache_client = InitSharedHttpCacheClient(page_url);

  // Responses with Cross-Origin-Resource-Policy: cross-origin should be stored
  // in the Shared Cache.
  EXPECT_TRUE(
      CheckResourceInSharedCache(shared_http_cache_client, cross_origin_url));

  // Responses with Cross-Origin-Resource-Policy: same-origin must not be stored
  // in the Shared Cache.
  EXPECT_FALSE(
      CheckResourceInSharedCache(shared_http_cache_client, same_origin_url));

  ReloadInNewProcess(page_url);
  FetchScript(cross_origin_url);
  FetchScript(same_origin_url);
}

IN_PROC_BROWSER_TEST_P(RendererAccessibleHttpCacheBrowserTest, PartialContent) {
  GURL page_url = GetURL("/loader/blank.html");
  EXPECT_TRUE(NavigateToURL(shell()->web_contents(), page_url));

  GURL partial_url = GetURL("/custom_resource/partial.js");
  FetchScript(partial_url);

  auto shared_http_cache_client =
      EnsureSharedCacheCreatedAndInitClient(page_url);

  // Partial responses (HTTP 206) must not be stored in the Shared Cache.
  EXPECT_FALSE(
      CheckResourceInSharedCache(shared_http_cache_client, partial_url));

  ReloadInNewProcess(page_url);
  FetchScript(partial_url);
}

IN_PROC_BROWSER_TEST_P(RendererAccessibleHttpCacheBrowserTest, NonGetRequest) {
  GURL page_url = GetURL("/loader/blank.html");
  EXPECT_TRUE(NavigateToURL(shell()->web_contents(), page_url));

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      CreateDirectURLLoaderFactory(
          /*renderer_accessible_http_cache_write_enabled=*/true);

  GURL resource_url = GetURL("/custom_resource/acao_wildcard.js");

  // A compromised renderer (or future renderer behavior changes) might issue a
  // POST request for a static script resource.
  network::ResourceRequest request =
      CreateSubresourceRequest(resource_url, "POST");
  EXPECT_EQ(net::OK, LoadRequest(factory.get(), request));

  auto shared_http_cache_client =
      EnsureSharedCacheCreatedAndInitClient(page_url);

  // Non-GET responses must not be stored in the Shared Cache.
  EXPECT_FALSE(
      CheckResourceInSharedCache(shared_http_cache_client, resource_url));
}

IN_PROC_BROWSER_TEST_P(RendererAccessibleHttpCacheBrowserTest, RangeRequest) {
  GURL page_url = GetURL("/loader/blank.html");
  EXPECT_TRUE(NavigateToURL(shell()->web_contents(), page_url));

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      CreateDirectURLLoaderFactory(
          /*renderer_accessible_http_cache_write_enabled=*/true);

  GURL resource_url = GetURL("/custom_resource/acao_wildcard.js");

  // A compromised renderer (or future renderer behavior changes) might issue a
  // request with a Range header for a static script resource.
  network::ResourceRequest request = CreateSubresourceRequest(resource_url);
  request.headers.SetHeader(net::HttpRequestHeaders::kRange, "bytes=0-10");
  EXPECT_EQ(net::OK, LoadRequest(factory.get(), request));

  auto shared_http_cache_client =
      EnsureSharedCacheCreatedAndInitClient(page_url);

  // Responses for range requests must not be stored in the Shared Cache.
  EXPECT_FALSE(
      CheckResourceInSharedCache(shared_http_cache_client, resource_url));
}

IN_PROC_BROWSER_TEST_P(RendererAccessibleHttpCacheBrowserTest,
                       RedirectedResponse) {
  GURL page_url = GetURL("/loader/blank.html");
  EXPECT_TRUE(NavigateToURL(shell()->web_contents(), page_url));

  GURL redirect_url = GetURL("/custom_resource/redirect.js");
  GURL redirect_target_url = GetURL("/custom_resource/acao_wildcard.js");
  FetchScript(redirect_url);

  auto shared_http_cache_client =
      EnsureSharedCacheCreatedAndInitClient(page_url);

  // Redirected responses must not be stored in the Shared Cache under either
  // the initial URL or the redirect destination URL (since CorsURLLoader
  // updates `request_.url` to the redirect destination before receiving the
  // final response).
  EXPECT_FALSE(
      CheckResourceInSharedCache(shared_http_cache_client, redirect_url));
  EXPECT_FALSE(CheckResourceInSharedCache(shared_http_cache_client,
                                          redirect_target_url));

  ReloadInNewProcess(page_url);
  FetchScript(redirect_url);
}

}  // namespace
}  // namespace content
