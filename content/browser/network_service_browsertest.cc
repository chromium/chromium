// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/json/values_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/os_crypt/async/browser/key_provider.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/os_crypt/async/common/encryptor_features.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "net/base/features.h"
#include "net/cookies/cookie_util.h"
#include "net/disk_cache/disk_cache.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_response_headers.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/gtest_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "sandbox/policy/features.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/cookie_encryption_provider.mojom.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "services/network/test/udp_socket_test_util.h"
#include "sql/database.h"
#include "sql/sql_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/application_status_listener.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <dbghelp.h>

#include <algorithm>

#include "base/files/memory_mapped_file.h"
#include "base/files/scoped_temp_file.h"
#include "base/rand_util.h"
#include "content/browser/network/network_service_process_tracker_win.h"
#include "sandbox/policy/features.h"
#endif

namespace content {

namespace {

class WebUITestWebUIControllerFactory : public WebUIControllerFactory {
 public:
  std::unique_ptr<WebUIController> CreateWebUIControllerForURL(
      WebUI* web_ui,
      const GURL& url) override {
    std::string foo(url.path());
    if (url.path() == "/nobinding/") {
      web_ui->SetBindings(BindingsPolicySet());
    }
    return HasWebUIScheme(url) ? std::make_unique<WebUIController>(web_ui)
                               : nullptr;
  }
  WebUI::TypeID GetWebUIType(BrowserContext* browser_context,
                             const GURL& url) override {
    return HasWebUIScheme(url) ? reinterpret_cast<WebUI::TypeID>(1) : nullptr;
  }
  bool UseWebUIForURL(BrowserContext* browser_context,
                      const GURL& url) override {
    return HasWebUIScheme(url);
  }
};

class TestWebUIDataSource : public URLDataSource {
 public:
  TestWebUIDataSource() {}

  TestWebUIDataSource(const TestWebUIDataSource&) = delete;
  TestWebUIDataSource& operator=(const TestWebUIDataSource&) = delete;

  ~TestWebUIDataSource() override {}

  std::string GetSource() override { return "webui"; }

  void StartDataRequest(const GURL& url,
                        const WebContents::Getter& wc_getter,
                        URLDataSource::GotDataCallback callback) override {
    std::move(callback).Run(base::MakeRefCounted<base::RefCountedString>(
        std::string("<html><body>Foo</body></html>")));
  }

  std::string GetMimeType(const GURL& url) override { return "text/html"; }
};

class NetworkServiceBrowserTest : public ContentBrowserTest {
 public:
  NetworkServiceBrowserTest() {
    EXPECT_TRUE(embedded_test_server()->Start());
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());

    WebUIControllerFactory::RegisterFactory(&factory_);
  }

  NetworkServiceBrowserTest(const NetworkServiceBrowserTest&) = delete;
  NetworkServiceBrowserTest& operator=(const NetworkServiceBrowserTest&) =
      delete;

  bool FetchResource(const GURL& url, bool synchronous = false) {
    if (!url.is_valid())
      return false;
    std::string script = JsReplace(
        "var xhr = new XMLHttpRequest();"
        "xhr.open('GET', $1, $2);"
        "new Promise(resolve => {"
        "  xhr.onload = function (e) {"
        "    if (xhr.readyState === 4) {"
        "      resolve(xhr.status === 200);"
        "    }"
        "  };"
        "  xhr.onerror = function () {"
        "    resolve(false);"
        "  };"
        "  try {"
        "    xhr.send(null);"
        "  } catch (error) {"
        "    resolve(false);"
        "  }"
        "});",
        url, !synchronous);

    EvalJsResult result = EvalJs(shell(), script);
    if (!result.error.empty()) {
      return false;
    }
    return result.ExtractBool();
  }

  bool CheckCanLoadHttp() {
    return FetchResource(embedded_test_server()->GetURL("/echo"),
                         /*synchronous=*/false);
  }

  void SetUpOnMainThread() override {
    URLDataSource::Add(shell()->web_contents()->GetBrowserContext(),
                       std::make_unique<TestWebUIDataSource>());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Since we assume exploited renderer process, it can bypass the same origin
    // policy at will. Simulate that by passing the disable-web-security flag.
    command_line->AppendSwitch(switches::kDisableWebSecurity);
    IsolateAllSitesForTesting(command_line);
  }

  base::FilePath GetCacheDirectory() { return temp_dir_.GetPath(); }

  base::FilePath GetCacheIndexDirectory() {
    return GetCacheDirectory()
        .AppendASCII("Cache_Data")
        .AppendASCII("index-dir");
  }

  void LoadURL(const GURL& url,
               network::mojom::URLLoaderFactory* loader_factory) {
    std::unique_ptr<network::ResourceRequest> request =
        std::make_unique<network::ResourceRequest>();
    request->url = url;
    url::Origin origin = url::Origin::Create(url);
    request->trusted_params = network::ResourceRequest::TrustedParams();
    request->trusted_params->isolation_info =
        net::IsolationInfo::CreateForInternalRequest(origin);
    request->site_for_cookies =
        request->trusted_params->isolation_info.site_for_cookies();

    SimpleURLLoaderTestHelper simple_loader_helper;
    std::unique_ptr<network::SimpleURLLoader> simple_loader =
        network::SimpleURLLoader::Create(std::move(request),
                                         TRAFFIC_ANNOTATION_FOR_TESTS);

    simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        loader_factory, simple_loader_helper.GetCallbackDeprecated());
    simple_loader_helper.WaitForCallback();
    ASSERT_TRUE(simple_loader_helper.response_body());
  }

 private:
  WebUITestWebUIControllerFactory factory_;
  base::ScopedTempDir temp_dir_;
};

#if BUILDFLAG(IS_ANDROID)
#define MAYBE_WebUIBindingsNoHttp DISABLED_WebUIBindingsNoHttp
#else
#define MAYBE_WebUIBindingsNoHttp WebUIBindingsNoHttp
#endif

// Verifies that WebUI pages with WebUI bindings can't make network requests.
IN_PROC_BROWSER_TEST_F(NetworkServiceBrowserTest, MAYBE_WebUIBindingsNoHttp) {
  GURL test_url(GetWebUIURL("webui/"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  RenderProcessHostBadIpcMessageWaiter kill_waiter(
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess());
  ASSERT_FALSE(CheckCanLoadHttp());
  EXPECT_EQ(bad_message::RPH_MOJO_PROCESS_ERROR, kill_waiter.Wait());
}

// Verifies that WebUI pages without WebUI bindings can make network requests.
IN_PROC_BROWSER_TEST_F(NetworkServiceBrowserTest, NoWebUIBindingsHttp) {
  GURL test_url(GetWebUIURL("webui/nobinding/"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  ASSERT_TRUE(CheckCanLoadHttp());
}

// Verifies the filesystem URLLoaderFactory's check, using
// ChildProcessSecurityPolicyImpl::CanRequestURL is properly rejected.
IN_PROC_BROWSER_TEST_F(NetworkServiceBrowserTest,
                       FileSystemBindingsCorrectOrigin) {
  GURL test_url(GetWebUIURL("webui/nobinding/"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));

  // Note: must be filesystem scheme (obviously).
  //       file: is not a safe web scheme (see IsWebSafeScheme),
  //       and /etc/passwd fails the CanCommitURL check.
  GURL file_url("filesystem:file:///etc/passwd");
  EXPECT_FALSE(FetchResource(file_url));
}

IN_PROC_BROWSER_TEST_F(NetworkServiceBrowserTest,
                       SimpleUrlLoader_NoAuthWhenNoWebContents) {
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = embedded_test_server()->GetURL("/auth-basic?password=");
  auto loader = network::SimpleURLLoader::Create(std::move(request),
                                                 TRAFFIC_ANNOTATION_FOR_TESTS);
  auto loader_factory = shell()
                            ->web_contents()
                            ->GetBrowserContext()
                            ->GetDefaultStoragePartition()
                            ->GetURLLoaderFactoryForBrowserProcess();
  scoped_refptr<net::HttpResponseHeaders> headers;
  base::RunLoop loop;
  loader->DownloadHeadersOnly(
      loader_factory.get(),
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             scoped_refptr<net::HttpResponseHeaders>* rh_out,
             scoped_refptr<net::HttpResponseHeaders> rh_in) {
            *rh_out = rh_in;
            std::move(quit_closure).Run();
          },
          loop.QuitClosure(), &headers));
  loop.Run();
  ASSERT_TRUE(headers.get());
  ASSERT_EQ(headers->response_code(), 401);
}

#if BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(NetworkServiceBrowserTest,
                       HttpCacheWrittenToDiskOnApplicationStateChange) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Create network context with cache pointing to the temp cache dir.
  mojo::Remote<network::mojom::NetworkContext> network_context;
  network::mojom::NetworkContextParamsPtr context_params =
      network::mojom::NetworkContextParams::New();
  context_params->cert_verifier_params = GetCertVerifierParams(
      cert_verifier::mojom::CertVerifierCreationParams::New());
  context_params->file_paths = network::mojom::NetworkContextFilePaths::New();
  context_params->file_paths->http_cache_directory = GetCacheDirectory();
  CreateNetworkContextInNetworkService(
      network_context.BindNewPipeAndPassReceiver(), std::move(context_params));

  network::mojom::URLLoaderFactoryParamsPtr params =
      network::mojom::URLLoaderFactoryParams::New();
  params->process_id = network::mojom::kBrowserProcessId;
  params->automatically_assign_isolation_info = true;
  params->is_orb_enabled = false;
  params->is_trusted = true;
  mojo::Remote<network::mojom::URLLoaderFactory> loader_factory;
  network_context->CreateURLLoaderFactory(
      loader_factory.BindNewPipeAndPassReceiver(), std::move(params));

  // Load a URL and check the cache index size.
  LoadURL(embedded_test_server()->GetURL("/cachetime"), loader_factory.get());
  int64_t directory_size = base::ComputeDirectorySize(GetCacheIndexDirectory());

  // Load another URL, cache index should not be written to disk yet.
  LoadURL(embedded_test_server()->GetURL("/cachetime?foo"),
          loader_factory.get());
  EXPECT_EQ(directory_size,
            base::ComputeDirectorySize(GetCacheIndexDirectory()));

  // After application state changes, cache index should be written to disk.
  base::android::ApplicationStatusListener::NotifyApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES);
  base::RunLoop().RunUntilIdle();
  FlushNetworkServiceInstanceForTesting();
  disk_cache::FlushCacheThreadForTesting();

  EXPECT_GT(base::ComputeDirectorySize(GetCacheIndexDirectory()),
            directory_size);
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
class NetworkConnectionObserver
    : public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  NetworkConnectionObserver() {
    content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);
    content::GetNetworkConnectionTracker()->GetConnectionType(
        &last_connection_type_,
        base::BindOnce(&NetworkConnectionObserver::OnConnectionChanged,
                       base::Unretained(this)));
  }

  ~NetworkConnectionObserver() override {
    content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(
        this);
  }

  void WaitForConnectionType(network::mojom::ConnectionType type) {
    type_to_wait_for_ = type;
    if (last_connection_type_ == type_to_wait_for_)
      return;

    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(network::mojom::ConnectionType type) override {
    last_connection_type_ = type;
    if (run_loop_ && type_to_wait_for_ == type)
      run_loop_->Quit();
  }

 private:
  network::mojom::ConnectionType type_to_wait_for_ =
      network::mojom::ConnectionType::CONNECTION_UNKNOWN;
  network::mojom::ConnectionType last_connection_type_ =
      network::mojom::ConnectionType::CONNECTION_UNKNOWN;
  std::unique_ptr<base::RunLoop> run_loop_;
};

class NetworkServiceConnectionTypeSyncedBrowserTest
    : public NetworkServiceBrowserTest {
 public:
  NetworkServiceConnectionTypeSyncedBrowserTest() {
#if BUILDFLAG(IS_LINUX)
    scoped_feature_list_.InitAndEnableFeature(
        net::features::kAddressTrackerLinuxIsProxied);
#else
    scoped_feature_list_.Init();
#endif
    ForceOutOfProcessNetworkService();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(NetworkServiceConnectionTypeSyncedBrowserTest,
                       ConnectionTypeChangeSyncedToNetworkProcess) {
  NetworkConnectionObserver observer;
  net::NetworkChangeNotifier::NotifyObserversOfConnectionTypeChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  observer.WaitForConnectionType(
      network::mojom::ConnectionType::CONNECTION_WIFI);

  net::NetworkChangeNotifier::NotifyObserversOfConnectionTypeChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  observer.WaitForConnectionType(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
}
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)

class NetworkServiceOutOfProcessBrowserTest : public NetworkServiceBrowserTest {
 public:
  NetworkServiceOutOfProcessBrowserTest() { ForceOutOfProcessNetworkService(); }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(IsOutOfProcessNetworkService());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(NetworkServiceOutOfProcessBrowserTest,
                       MemoryPressureSentToNetworkProcess) {
  mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
  GetNetworkService()->BindTestInterfaceForTesting(
      network_service_test.BindNewPipeAndPassReceiver());
  // TODO(crbug.com/41423903): Make sure the network process is started to avoid
  // a deadlock on Android.
  network_service_test.FlushForTesting();

  mojo::ScopedAllowSyncCallForTesting allow_sync_call;
  base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level =
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
  network_service_test->GetLatestMemoryPressureLevel(&memory_pressure_level);
  EXPECT_EQ(memory_pressure_level,
            base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);

  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  base::RunLoop().RunUntilIdle();
  FlushNetworkServiceInstanceForTesting();

  network_service_test->GetLatestMemoryPressureLevel(&memory_pressure_level);
  EXPECT_EQ(memory_pressure_level,
            base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
}

// Verifies that sync XHRs don't hang if the network service crashes.
IN_PROC_BROWSER_TEST_F(NetworkServiceOutOfProcessBrowserTest, SyncXHROnCrash) {
  net::EmbeddedTestServer http_server;
  http_server.AddDefaultHandlers(GetTestDataFilePath());
  http_server.RegisterRequestMonitor(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request) {
        if (request.relative_url == "/hung") {
          GetUIThreadTaskRunner({})->PostTask(
              FROM_HERE,
              base::BindOnce(&BrowserTestBase::SimulateNetworkServiceCrash,
                             base::Unretained(this)));
        }
      }));
  EXPECT_TRUE(http_server.Start());

  EXPECT_TRUE(NavigateToURL(shell(), http_server.GetURL("/empty.html")));

  FetchResource(http_server.GetURL("/hung"), true);
  // If the renderer is hung the test will hang.
}

// Verifies that sync cookie calls don't hang if the network service crashes.
IN_PROC_BROWSER_TEST_F(NetworkServiceOutOfProcessBrowserTest,
                       SyncCookieGetOnCrash) {
  mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
  GetNetworkService()->BindTestInterfaceForTesting(
      network_service_test.BindNewPipeAndPassReceiver());
  network_service_test->CrashOnGetCookieList();

  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/empty.html")));

  ASSERT_TRUE(content::ExecJs(shell()->web_contents(), "document.cookie"));
  // If the renderer is hung the test will hang.
}

// Tests that CORS is performed by the network service when |factory_override|
// is used.
IN_PROC_BROWSER_TEST_F(NetworkServiceBrowserTest, FactoryOverride) {
  class TestURLLoaderFactory final : public network::mojom::URLLoaderFactory {
   public:
    void CreateLoaderAndStart(
        mojo::PendingReceiver<network::mojom::URLLoader> receiver,
        int32_t request_id,
        uint32_t options,
        const network::ResourceRequest& resource_request,
        mojo::PendingRemote<network::mojom::URLLoaderClient> pending_client,
        const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
        override {
      mojo::Remote<network::mojom::URLLoaderClient> client(
          std::move(pending_client));
      EXPECT_EQ(resource_request.url,
                GURL("https://www.example.com/hello.txt"));
      if (resource_request.method == "OPTIONS") {
        has_received_preflight_ = true;
        auto response = network::mojom::URLResponseHead::New();
        response->headers =
            base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
        response->headers->SetHeader("access-control-allow-origin",
                                     "https://www2.example.com");
        response->headers->SetHeader("access-control-allow-methods", "*");
        client->OnReceiveResponse(std::move(response),
                                  mojo::ScopedDataPipeConsumerHandle(),
                                  std::nullopt);
      } else if (resource_request.method == "custom-method") {
        has_received_request_ = true;
        auto response = network::mojom::URLResponseHead::New();
        response->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
            "HTTP/1.1 202 Accepted");
        response->headers->SetHeader("access-control-allow-origin",
                                     "https://www2.example.com");
        client->OnReceiveResponse(std::move(response),
                                  mojo::ScopedDataPipeConsumerHandle(),
                                  std::nullopt);
        client->OnComplete(network::URLLoaderCompletionStatus(net::OK));
      } else {
        client->OnComplete(
            network::URLLoaderCompletionStatus(net::ERR_INVALID_ARGUMENT));
      }
    }
    void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
        override {
      NOTREACHED_IN_MIGRATION();
    }

    bool has_received_preflight() const { return has_received_preflight_; }
    bool has_received_request() const { return has_received_request_; }

   private:
    bool has_received_preflight_ = false;
    bool has_received_request_ = false;
  };

  // Create a request that will trigger a CORS preflight request.
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL("https://www.example.com/hello.txt");
  request->mode = network::mojom::RequestMode::kCors;
  request->credentials_mode = network::mojom::CredentialsMode::kSameOrigin;
  request->method = "custom-method";
  request->request_initiator =
      url::Origin::Create(GURL("https://www2.example.com/"));

  // Inject TestURLLoaderFactory as the factory override.
  auto test_loader_factory = std::make_unique<TestURLLoaderFactory>();
  mojo::Receiver<network::mojom::URLLoaderFactory> test_loader_factory_receiver(
      test_loader_factory.get());
  mojo::Remote<network::mojom::URLLoaderFactory> loader_factory_remote;
  auto loader = network::SimpleURLLoader::Create(std::move(request),
                                                 TRAFFIC_ANNOTATION_FOR_TESTS);
  auto params = network::mojom::URLLoaderFactoryParams::New();
  params->process_id = 0;
  params->factory_override = network::mojom::URLLoaderFactoryOverride::New();
  params->factory_override->overriding_factory =
      test_loader_factory_receiver.BindNewPipeAndPassRemote();
  shell()
      ->web_contents()
      ->GetBrowserContext()
      ->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->CreateURLLoaderFactory(
          loader_factory_remote.BindNewPipeAndPassReceiver(),
          std::move(params));
  scoped_refptr<net::HttpResponseHeaders> headers;

  // Perform the request.
  base::RunLoop loop;
  loader->DownloadHeadersOnly(
      loader_factory_remote.get(),
      base::BindLambdaForTesting(
          [&](scoped_refptr<net::HttpResponseHeaders> passed_headers) {
            headers = passed_headers;
            loop.Quit();
          }));
  loop.Run();
  ASSERT_TRUE(headers.get());
  EXPECT_EQ(headers->response_code(), 202);
  EXPECT_TRUE(test_loader_factory->has_received_preflight());
  EXPECT_TRUE(test_loader_factory->has_received_request());
}

// Android doesn't support PRE_ tests.
// TODO(wfh): Enable this test when https://crbug.com/1257820 is fixed.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)
class NetworkServiceBrowserCacheResetTest : public NetworkServiceBrowserTest {
 public:
  NetworkServiceBrowserCacheResetTest() = default;

 protected:
  void StoreUrl(const GURL& url) {
    base::ScopedAllowBlockingForTesting allow_blocking;

    base::FilePath data_file =
        shell()->web_contents()->GetBrowserContext()->GetPath().Append(
            FILE_PATH_LITERAL("TestData"));
    std::string data;
    base::JSONWriter::Write(base::Value(url.spec()), &data);
    EXPECT_TRUE(base::WriteFile(data_file, data));
  }

  void RetrieveUrl(GURL& url) {
    base::ScopedAllowBlockingForTesting allow_blocking;

    base::FilePath data_file =
        shell()->web_contents()->GetBrowserContext()->GetPath().Append(
            FILE_PATH_LITERAL("TestData"));
    std::string data;
    EXPECT_TRUE(base::ReadFileToString(data_file, &data));
    auto json_data = base::JSONReader::Read(data);
    ASSERT_TRUE(json_data.has_value());
    url = GURL(json_data->GetString());
    EXPECT_TRUE(url.is_valid());
  }

  base::FilePath GetNetworkContextPath() {
    return shell()->web_contents()->GetBrowserContext()->GetPath().Append(
        FILE_PATH_LITERAL("TestContext"));
  }

  base::FilePath GetNetworkContextCachePath() {
    return GetNetworkContextPath().Append(FILE_PATH_LITERAL("Cache"));
  }

  // Creates a Network context and attempts to make a request to a resource that
  // is cacheable. Returns the net error code. If `load_only_from_cache` is
  // specified then the request will fail if the resource cannot be served from
  // the cache. `url` specifies the URL to connect to on the
  // embedded_test_server host which does not need to have a server actively
  // listening on it if `load_only_from_cache` is true.
  int MakeNetworkContentAndLoadUrl(bool reset_cache,
                                   bool load_only_from_cache,
                                   const GURL& url) {
    auto file_paths = network::mojom::NetworkContextFilePaths::New();
    base::FilePath context_path = GetNetworkContextPath();
    file_paths->data_directory = context_path.Append(FILE_PATH_LITERAL("Data"));
    file_paths->unsandboxed_data_path = context_path;
    file_paths->trigger_migration = true;

    network::mojom::NetworkContextParamsPtr context_params =
        network::mojom::NetworkContextParams::New();
    context_params->file_paths = std::move(file_paths);
    context_params->cert_verifier_params = GetCertVerifierParams(
        cert_verifier::mojom::CertVerifierCreationParams::New());
    context_params->reset_http_cache_backend = reset_cache;
    context_params->http_cache_enabled = true;
    context_params->file_paths->http_cache_directory =
        GetNetworkContextCachePath();

    mojo::Remote<network::mojom::NetworkContext> network_context;
    content::CreateNetworkContextInNetworkService(
        network_context.BindNewPipeAndPassReceiver(),
        std::move(context_params));

    network::mojom::URLLoaderFactoryParamsPtr url_loader_params =
        network::mojom::URLLoaderFactoryParams::New();
    url_loader_params->process_id = network::mojom::kBrowserProcessId;
    url_loader_params->is_trusted = true;
    mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory;
    network_context->CreateURLLoaderFactory(
        url_loader_factory.BindNewPipeAndPassReceiver(),
        std::move(url_loader_params));

    std::unique_ptr<network::ResourceRequest> request =
        std::make_unique<network::ResourceRequest>();
    request->url = url;
    url::Origin origin = url::Origin::Create(url);
    request->trusted_params = network::ResourceRequest::TrustedParams();
    request->trusted_params->isolation_info =
        net::IsolationInfo::CreateForInternalRequest(origin);
    request->site_for_cookies =
        request->trusted_params->isolation_info.site_for_cookies();

    if (load_only_from_cache)
      request->load_flags |= net::LOAD_ONLY_FROM_CACHE;
    auto loader = network::SimpleURLLoader::Create(
        std::move(request), TRAFFIC_ANNOTATION_FOR_TESTS);

    scoped_refptr<net::HttpResponseHeaders> headers;
    base::RunLoop loop;
    loader->DownloadHeadersOnly(
        url_loader_factory.get(),
        base::BindLambdaForTesting(
            [&](scoped_refptr<net::HttpResponseHeaders> passed_headers) {
              headers = passed_headers;
              loop.Quit();
            }));
    loop.Run();
    return loader->NetError();
  }

  void GetCacheFileInfo(base::File::Info& info) {
    base::FilePath ceontxt_path = GetNetworkContextPath();
    base::FileEnumerator cache_files(GetNetworkContextCachePath(), true,
                                     base::FileEnumerator::FILES);
    // Cache entries created.
    auto file_path = cache_files.Next();
    ASSERT_FALSE(file_path.empty());
    ASSERT_TRUE(base::GetFileInfo(file_path, &info));
  }
};

// Create a network context and make an HTTP request which causes cache entry to
// be created.
IN_PROC_BROWSER_TEST_F(NetworkServiceBrowserCacheResetTest,
                       PRE_PRE_CacheResetTest) {
  GURL url = embedded_test_server()->GetURL("/echoheadercache");
  // Store the URL so the requests made by the subsequent parts of this test
  // are to the same origin. Otherwise, the embedded test server might be
  // operating on a different port causing incorrect cache misses.
  ASSERT_NO_FATAL_FAILURE(StoreUrl(url));

  EXPECT_THAT(MakeNetworkContentAndLoadUrl(
                  /*reset_cache=*/false, /*load_only_from_cache=*/false, url),
              net::test::IsOk());
}

// Using the same network context, make an HTTP request and verify that the
// cache entry is correctly used.
IN_PROC_BROWSER_TEST_F(NetworkServiceBrowserCacheResetTest,
                       PRE_CacheResetTest) {
  GURL url;
  ASSERT_NO_FATAL_FAILURE(RetrieveUrl(url));

  EXPECT_THAT(MakeNetworkContentAndLoadUrl(/*reset_cache=*/false,
                                           /*load_only_from_cache=*/true, url),
              net::test::IsOk());
}

// Using the same network context, reset the cache backend and verify that cache
// miss is correctly reported.
IN_PROC_BROWSER_TEST_F(NetworkServiceBrowserCacheResetTest, CacheResetTest) {
  GURL url;
  ASSERT_NO_FATAL_FAILURE(RetrieveUrl(url));

  EXPECT_THAT(MakeNetworkContentAndLoadUrl(/*reset_cache=*/true,
                                           /*load_only_from_cache=*/true, url),
              net::test::IsError(net::ERR_CACHE_MISS));
}

#if BUILDFLAG(IS_POSIX)
IN_PROC_BROWSER_TEST_F(NetworkServiceBrowserCacheResetTest, CacheResetFailure) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  const base::FilePath path = GetNetworkContextCachePath();

  GURL url = embedded_test_server()->GetURL("/echoheadercache");

  ASSERT_TRUE(base::CreateDirectory(path));
  // Make the directory inaccessible, to see what happens when resetting the
  // cache fails.
  ASSERT_TRUE(base::SetPosixFilePermissions(path, /*mode=*/0));

  EXPECT_THAT(MakeNetworkContentAndLoadUrl(/*reset_cache=*/true,
                                           /*load_only_from_cache=*/true, url),
              net::test::IsError(net::ERR_CACHE_MISS));
}
#endif  // BUILDFLAG(IS_POSIX)
#endif  // BUILDFLAG(IS_ANDROID)

// Cache data migration is not used for Fuchsia.
#if !BUILDFLAG(IS_FUCHSIA)

const base::FilePath::CharType kCheckpointFileName[] =
    FILE_PATH_LITERAL("NetworkDataMigrated");
constexpr char kCookieName[] = "Name";
constexpr char kCookieValue[] = "Value";

net::CookieList GetCookies(
    const mojo::Remote<network::mojom::CookieManager>& cookie_manager) {
  base::RunLoop run_loop;
  net::CookieList cookies_out;
  cookie_manager->GetAllCookies(
      base::BindLambdaForTesting([&](const net::CookieList& cookies) {
        cookies_out = cookies;
        run_loop.Quit();
      }));
  run_loop.Run();
  return cookies_out;
}

void SetCookie(
    const mojo::Remote<network::mojom::CookieManager>& cookie_manager) {
  base::Time t = base::Time::Now();
  auto cookie = net::CanonicalCookie::CreateUnsafeCookieForTesting(
      kCookieName, kCookieValue, "example.test", "/", t, t + base::Days(1),
      base::Time(), base::Time(), /*secure=*/true, /*http-only=*/false,
      net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_DEFAULT);
  base::RunLoop run_loop;
  cookie_manager->SetCanonicalCookie(
      *cookie, net::cookie_util::SimulatedCookieSource(*cookie, "https"),
      net::CookieOptions(),
      base::BindLambdaForTesting(
          [&](net::CookieAccessResult result) { run_loop.Quit(); }));
  run_loop.Run();
}

void FlushCookies(
    const mojo::Remote<network::mojom::CookieManager>& cookie_manager) {
  base::RunLoop run_loop;
  cookie_manager->FlushCookieStore(
      base::BindLambdaForTesting([&]() { run_loop.Quit(); }));
  run_loop.Run();
}

mojo::PendingRemote<network::mojom::NetworkContext>
CreateNetworkContextForPaths(network::mojom::NetworkContextFilePathsPtr paths,
                             const base::FilePath& cache_path) {
  network::mojom::NetworkContextParamsPtr context_params =
      network::mojom::NetworkContextParams::New();
  context_params->file_paths = std::move(paths);
  context_params->cert_verifier_params = GetCertVerifierParams(
      cert_verifier::mojom::CertVerifierCreationParams::New());
  // Not passing in a key for simplicity, so disable encryption.
  context_params->enable_encrypted_cookies = false;
  context_params->http_cache_enabled = true;
  context_params->file_paths->http_cache_directory = cache_path;
  mojo::PendingRemote<network::mojom::NetworkContext> network_context;
  content::CreateNetworkContextInNetworkService(
      network_context.InitWithNewPipeAndPassReceiver(),
      std::move(context_params));
  return network_context;
}

enum class FailureType {
  kNoFailures = 0,
  // A file exists with the same name as the target directory so it cannot be
  // created.
  kDirIsAFile = 1,
  // The target migration directory already exists.
  kDirAlreadyThere = 2,
  // A file called 'TestCookies' already exists in the migration target
  // directory.
  kCookieFileAlreadyThere = 3,
#if BUILDFLAG(IS_WIN)
  // The 'TestCookies' file in the destination directory is locked and cannot be
  // written to. This is only valid on Windows where files can actually be
  // locked.
  kDestCookieFileIsLocked = 4,
  // The 'TestCookies' file in the source directory is locked and cannot be read
  // from (during the migration). This failure is only valid on Windows where
  // files can actually be locked.
  kSourceCookieFileIsLocked = 5,
#endif  // BUILDFLAG(IS_WIN)
  // A file exists with the same name as the Cache dir. This will cause the
  // creation of the cache dir to fail, and cache to not function either
  // (although we don't test for that here).
  kCacheDirIsAFile = 6,
};

static const FailureType kFailureTypes[] = {
    FailureType::kNoFailures,
    FailureType::kDirIsAFile,
    FailureType::kDirAlreadyThere,
    FailureType::kCookieFileAlreadyThere,
#if BUILDFLAG(IS_WIN)
    FailureType::kDestCookieFileIsLocked,
    FailureType::kSourceCookieFileIsLocked,
#endif  // BUILDFLAG(IS_WIN)
    FailureType::kCacheDirIsAFile};

static const base::FilePath::CharType kCookieDatabaseName[] =
    FILE_PATH_LITERAL("TestCookies");
static const base::FilePath::CharType kNetworkSubpath[] =
    FILE_PATH_LITERAL("Network");

// Disable the following data migration tests on Android because the data
// migration logic is disabled and compiled out on this platform.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_NetworkServiceDataMigrationBrowserTest \
  DISABLED_NetworkServiceDataMigrationBrowserTest
#define MAYBE_NetworkServiceDataMigrationBrowserTestWithFailures \
  DISABLED_NetworkServiceDataMigrationBrowserTestWithFailures
#else
#define MAYBE_NetworkServiceDataMigrationBrowserTest \
  NetworkServiceDataMigrationBrowserTest
#define MAYBE_NetworkServiceDataMigrationBrowserTestWithFailures \
  NetworkServiceDataMigrationBrowserTestWithFailures
#endif  // BUILDFLAG(IS_ANDROID)

// A class to test various behavior of network context data migration.
class MAYBE_NetworkServiceDataMigrationBrowserTest : public ContentBrowserTest {
 public:
  MAYBE_NetworkServiceDataMigrationBrowserTest() {
    // Migration only supports non-WAL sqlite databases. If this feature is
    // switched on by default before migration has been completed then the code
    // in MaybeGrantSandboxAccessToNetworkContextData will need to be updated.
    EXPECT_FALSE(
        base::FeatureList::IsEnabled(sql::features::kEnableWALModeByDefault));
#if BUILDFLAG(IS_WIN)
    // On Windows, the network sandbox needs to be disabled. This is because the
    // code that performs the migration on Windows DCHECKs if network sandbox is
    // enabled and migration is not requested, but this is used in the tests to
    // verify this behavior.
    win_network_sandbox_feature_.InitAndDisableFeature(
        sandbox::policy::features::kNetworkServiceSandbox);
#endif
  }

#if BUILDFLAG(IS_WIN)
 private:
  base::test::ScopedFeatureList win_network_sandbox_feature_;
#endif
};

// A parameterized test fixture that can simulate various failures in the
// migration step, and can also be run with either in-process or out-of-process
// network service.
class MAYBE_NetworkServiceDataMigrationBrowserTestWithFailures
    : public MAYBE_NetworkServiceDataMigrationBrowserTest,
      public ::testing::WithParamInterface<std::tuple<bool, FailureType>> {
 public:
  MAYBE_NetworkServiceDataMigrationBrowserTestWithFailures() {
    if (IsNetworkServiceRunningInProcess()) {
      ForceInProcessNetworkService();
    } else {
      ForceOutOfProcessNetworkService();
    }
  }

 protected:
  bool IsNetworkServiceRunningInProcess() { return std::get<0>(GetParam()); }
  FailureType GetFailureType() { return std::get<1>(GetParam()); }
};

// A function to verify that data files move during migration to sandboxed data
// dir. This function uses three directories to verify the behavior. It uses the
// cookies file to verify the migration occurs correctly.
//
// Testing takes place under the browser context path. First, a network context
// is created in temp dir 'one' and then a cookie is written and flushed to
// disk. This results in cookie files(s) being created on disk.
//
// BrowserContext/
// |- tempdir 'one'/ (`tempdir_one` FilePath)
// |  |- Cookies
// |  |- Cookies-journal
//
// The entire 'one' dir is then copied into a new 'two' temp folder to create
// the directory structure used for migration. This is so a second network
// context can be created in the same network service.
//
// BrowserContext/
// |- tempdir 'one'/
// |  |- Cookies
// |  |- Cookies-journal
// |- tempdir 'two'/ (`tempdir_two` FilePath)
// |  |- Cookies (copied from above)
// |  |- Cookies-journal (copied from above)
//
// A new network context is then created with `unsandboxed_data_path` set to
// root of tempdir 'two' and `data_directory` set to a directory underneath
// tempdir 'two' called 'Network' to initiate the migration. After a successful
// migration, the structure should look like this:
//
// BrowserContext/
// |- tempdir 'one'/
// |  |- Cookies
// |  |- Cookies-journal
// |- tempdir 'two'/
// |  |- Network/
// |  |  |- Cookies (migrated from tempdir 'two')
// |  |  |- Cookies-journal (migrated from tempdir 'two')
//
// This test injects various failures in the migration process to ensure that
// the network context still functions correctly if the Cookies file cannot be
// migrated.
void MigrationTestInternal(const base::FilePath& tempdir_one,
                           const base::FilePath& tempdir_two_parent,
                           FailureType failure_type) {
  EXPECT_FALSE(base::PathExists(tempdir_one.Append(kCookieDatabaseName)));

  auto file_paths = network::mojom::NetworkContextFilePaths::New();
  file_paths->data_directory = tempdir_one;
  file_paths->cookie_database_name = base::FilePath(kCookieDatabaseName);

  mojo::Remote<network::mojom::NetworkContext> network_context_one(
      CreateNetworkContextForPaths(
          std::move(file_paths),
          tempdir_one.Append(FILE_PATH_LITERAL("Cache"))));
  mojo::Remote<network::mojom::CookieManager> cookie_manager_one;
  network_context_one->GetCookieManager(
      cookie_manager_one.BindNewPipeAndPassReceiver());

  SetCookie(cookie_manager_one);
  FlushCookies(cookie_manager_one);

  // Verify that the cookie file exists in tempdir 'one'.
  EXPECT_TRUE(base::PathExists(tempdir_one.Append(kCookieDatabaseName)));

  // Now, copy the entire directory to tempdir 'two' to verify the migration.
  EXPECT_TRUE(base::CopyDirectory(tempdir_one, tempdir_two_parent, true));
  // base::CopyDirectory copies the directory into a new directory if the target
  // directory already exists, so fix up the directory name here.
  base::FilePath tempdir_two =
      tempdir_two_parent.Append(tempdir_one.BaseName());

  // Verify cookie file is there, copied across from the tempdir 'one'.
  EXPECT_TRUE(base::PathExists(tempdir_two.Append(kCookieDatabaseName)));
#if BUILDFLAG(IS_WIN)
  base::File longer_lived_file;
#endif

  switch (failure_type) {
    case FailureType::kNoFailures:
      break;
    case FailureType::kDirIsAFile: {
      // Create a file called 'Network' in the path. This will cause migration
      // to fail catastrophically as the directory cannot be created.
      base::File scoped_file(
          tempdir_two.Append(kNetworkSubpath),
          base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
      EXPECT_TRUE(scoped_file.IsValid());
    } break;
    case FailureType::kDirAlreadyThere:
      EXPECT_TRUE(base::CreateDirectory(tempdir_two.Append(kNetworkSubpath)));
      break;
    case FailureType::kCookieFileAlreadyThere: {
      EXPECT_TRUE(base::CreateDirectory(tempdir_two.Append(kNetworkSubpath)));
      // Touch a file in the new dir called the same as the cookie file. This
      // should be correctly overwritten by the migration code.
      base::File scoped_file(
          tempdir_two.Append(kNetworkSubpath).Append(kCookieDatabaseName),
          base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
      EXPECT_TRUE(scoped_file.IsValid());
    } break;
#if BUILDFLAG(IS_WIN)
    case FailureType::kDestCookieFileIsLocked:
      // Create a file called 'TestCookies' in the destination path and hold a
      // write lock on it so it can't be written to.
      EXPECT_TRUE(base::CreateDirectory(tempdir_two.Append(kNetworkSubpath)));
      longer_lived_file = base::File(
          tempdir_two.Append(kNetworkSubpath).Append(kCookieDatabaseName),
          base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE |
              base::File::FLAG_WIN_EXCLUSIVE_WRITE |
              base::File::FLAG_WIN_EXCLUSIVE_READ);
      EXPECT_TRUE(longer_lived_file.IsValid());
      break;
    case FailureType::kSourceCookieFileIsLocked:
      // Lock the Cookie file so it can't be read. This causes cookies to break
      // entirely, both the migration and the normal operation. The test can
      // merely verify that the migration fails and the failure is reported
      // correctly.
      longer_lived_file =
          base::File(tempdir_two.Append(kCookieDatabaseName),
                     base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_WRITE |
                         base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                         base::File::FLAG_WIN_EXCLUSIVE_READ);
      EXPECT_TRUE(longer_lived_file.IsValid());
      break;
#endif  // BUILDFLAG(IS_WIN)
    case FailureType::kCacheDirIsAFile: {
      // Make the cache directory invalid by deleting it and making it a file,
      // so it can't be created or used.
      base::DeletePathRecursively(
          tempdir_two.Append(FILE_PATH_LITERAL("Cache")));
      base::File scoped_file(
          tempdir_two.Append(FILE_PATH_LITERAL("Cache")),
          base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
      EXPECT_TRUE(scoped_file.IsValid());
    } break;
  }
  // Create a new network context that will migrate the files from the tempdir
  // 'two' into the new 'Network' directory underneath.
  auto new_file_paths = network::mojom::NetworkContextFilePaths::New();
  // Data path is now a new 'Network' directory under the tempdir 'two'.
  new_file_paths->data_directory = tempdir_two.Append(kNetworkSubpath);
  new_file_paths->cookie_database_name = base::FilePath(kCookieDatabaseName);
  // Migrate data from the tempdir 'two' to the new path under 'Network'.
  new_file_paths->unsandboxed_data_path = tempdir_two;
  new_file_paths->trigger_migration = true;

  base::HistogramTester histogram_tester;
  mojo::Remote<network::mojom::NetworkContext> network_context_two(
      CreateNetworkContextForPaths(
          std::move(new_file_paths),
          tempdir_two.Append(FILE_PATH_LITERAL("Cache"))));
  mojo::Remote<network::mojom::CookieManager> cookie_manager_two;
  network_context_two->GetCookieManager(
      cookie_manager_two.BindNewPipeAndPassReceiver());
  net::CookieList cookies = GetCookies(cookie_manager_two);

  bool cookies_should_work = true;

  switch (failure_type) {
    case FailureType::kNoFailures:
    case FailureType::kDirAlreadyThere:
    case FailureType::kCookieFileAlreadyThere:
      // Cookie file should have moved from the original `unsandboxed_data_path`
      // to the new 'Network' path.
      EXPECT_FALSE(base::PathExists(tempdir_two.Append(kCookieDatabaseName)));
      // Into the new directory.
      EXPECT_TRUE(base::PathExists(
          tempdir_two.Append(kNetworkSubpath).Append(kCookieDatabaseName)));
      // If there was a journal file in the original `unsandboxed_data_path`,
      // check that it has also moved.
      if (base::PathExists(tempdir_one.Append(sql::Database::JournalPath(
              base::FilePath(kCookieDatabaseName))))) {
        EXPECT_FALSE(base::PathExists(tempdir_two.Append(
            sql::Database::JournalPath(base::FilePath(kCookieDatabaseName)))));
        EXPECT_TRUE(
            base::PathExists(tempdir_two.Append(kNetworkSubpath)
                                 .Append(sql::Database::JournalPath(
                                     base::FilePath(kCookieDatabaseName)))));
      }

      histogram_tester.ExpectUniqueSample("NetworkService.GrantSandboxResult",
                                          /*sample=kSuccess=*/0,
                                          /*expected_bucket_count=*/1);
      // Checkpoint file should have been placed into the migrated directory.
      EXPECT_TRUE(base::PathExists(
          tempdir_two.Append(kNetworkSubpath).Append(kCheckpointFileName)));
      break;
    case FailureType::kDirIsAFile:
      // Cookie file should still be in the original `unsandboxed_data_path` as
      // it could not be moved.
      EXPECT_TRUE(base::PathExists(tempdir_two.Append(kCookieDatabaseName)));
      EXPECT_FALSE(base::PathExists(
          tempdir_two.Append(kNetworkSubpath).Append(kCheckpointFileName)));
      histogram_tester.ExpectUniqueSample(
          "NetworkService.GrantSandboxResult",
          /*sample=kFailedToCreateDataDirectory=*/2,
          /*expected_bucket_count=*/1);
      break;
#if BUILDFLAG(IS_WIN)
    case FailureType::kDestCookieFileIsLocked:
      // Cookie file should still be in the original `unsandboxed_data_path` as
      // it could not be moved as the destination was locked or not writable.
      EXPECT_TRUE(base::PathExists(tempdir_two.Append(kCookieDatabaseName)));
      // Source file is there, but locked.
      EXPECT_TRUE(base::PathExists(tempdir_two.Append(kCookieDatabaseName)));
      // And locked destination file is there, but cookies are working so they
      // must be backed by the original file.
      EXPECT_TRUE(
          base::PathExists(tempdir_two.Append(FILE_PATH_LITERAL("Network"))
                               .Append(kCookieDatabaseName)));
      EXPECT_FALSE(base::PathExists(
          tempdir_two.Append(kNetworkSubpath).Append(kCheckpointFileName)));
      {
        base::File attempt_to_open_locked_file(
            tempdir_two.Append(kNetworkSubpath).Append(kCookieDatabaseName),
            base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ);
        // Check that the file really is locked, so the cookies must be running
        // from the unsandboxed directory.
        EXPECT_FALSE(attempt_to_open_locked_file.IsValid());
      }
      histogram_tester.ExpectUniqueSample("NetworkService.GrantSandboxResult",
                                          /*sample=kFailedToCopyData=*/3,
                                          /*expected_bucket_count=*/1);
      break;
    case FailureType::kSourceCookieFileIsLocked:
      // Cookie file should still be in the original `unsandboxed_data_path` as
      // it could not be moved as the destination was locked or not writable.
      EXPECT_TRUE(base::PathExists(tempdir_two.Append(kCookieDatabaseName)));
      // File hasn't moved, so cookies must be backed by the original file.
      EXPECT_FALSE(base::PathExists(
          tempdir_two.Append(kNetworkSubpath).Append(kCookieDatabaseName)));
      EXPECT_FALSE(base::PathExists(
          tempdir_two.Append(kNetworkSubpath).Append(kCheckpointFileName)));
      histogram_tester.ExpectUniqueSample("NetworkService.GrantSandboxResult",
                                          /*sample=kFailedToCopyData=*/3,
                                          /*expected_bucket_count=*/1);
      // In this case the source cookie file can't be read by anything including
      // the migration code and the network context, so cookies should be
      // totally broken. :(
      cookies_should_work = false;
      break;
#endif  // BUILDFLAG(IS_WIN)
    case FailureType::kCacheDirIsAFile:
      histogram_tester.ExpectUniqueSample("NetworkService.GrantSandboxResult",
                                          /*sample=kSuccess=*/0,
                                          /*expected_bucket_count=*/1);
      break;
  }
  if (!cookies_should_work)
    return;

  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ(kCookieName, cookies[0].Name());
  EXPECT_EQ(kCookieValue, cookies[0].Value());
}

IN_PROC_BROWSER_TEST_P(MAYBE_NetworkServiceDataMigrationBrowserTestWithFailures,
                       MigrateDataTest) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::FilePath tempdir_one;
  EXPECT_TRUE(base::CreateTemporaryDirInDir(
      shell()->web_contents()->GetBrowserContext()->GetPath(),
      /*prefix=*/FILE_PATH_LITERAL("one"), &tempdir_one));
  base::FilePath tempdir_two;
  EXPECT_TRUE(base::CreateTemporaryDirInDir(
      shell()->web_contents()->GetBrowserContext()->GetPath(),
      /*prefix=*/FILE_PATH_LITERAL("two"), &tempdir_two));
  MigrationTestInternal(tempdir_one, tempdir_two, GetFailureType());
}

// This test is similar to the test above that uses two directories, but it uses
// a third directory to verify that if a migration is triggered and then later
// not triggered, then the data is still read from the new directory and not the
// old one.
IN_PROC_BROWSER_TEST_F(MAYBE_NetworkServiceDataMigrationBrowserTest,
                       MigrateThenNoMigrate) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::FilePath tempdir_one;
  EXPECT_TRUE(base::CreateTemporaryDirInDir(
      shell()->web_contents()->GetBrowserContext()->GetPath(),
      /*prefix=*/FILE_PATH_LITERAL("one"), &tempdir_one));
  base::FilePath tempdir_two;
  EXPECT_TRUE(base::CreateTemporaryDirInDir(
      shell()->web_contents()->GetBrowserContext()->GetPath(),
      /*prefix=*/FILE_PATH_LITERAL("two"), &tempdir_two));
  // Migrate within tempdir_two.
  MigrationTestInternal(tempdir_one, tempdir_two, FailureType::kNoFailures);
  // base::CopyDirectory copies the directory into a new directory if the target
  // directory already exists, so fix up the directory name here.
  base::FilePath real_tempdir_two = tempdir_two.Append(tempdir_one.BaseName());
  // Double check that the migration happened.
  EXPECT_TRUE(base::PathExists(
      real_tempdir_two.Append(kNetworkSubpath).Append(kCookieDatabaseName)));
  // Create a third testing directory, and copy the migrated data from
  // tempdir_two into it.
  base::FilePath tempdir_three;
  EXPECT_TRUE(base::CreateTemporaryDirInDir(
      shell()->web_contents()->GetBrowserContext()->GetPath(),
      /*prefix=*/FILE_PATH_LITERAL("three"), &tempdir_three));
  EXPECT_TRUE(base::CopyDirectory(real_tempdir_two, tempdir_three, true));
  // base::CopyDirectory copies the directory into a new directory if the target
  // directory already exists, so fix up the directory name here.
  base::FilePath real_tempdir_three =
      tempdir_three.Append(real_tempdir_two.BaseName());
  // Double check the directory was copied right.
  EXPECT_TRUE(base::PathExists(
      real_tempdir_three.Append(kNetworkSubpath).Append(kCookieDatabaseName)));
  // Double check cookies are not in the old directory, meaning if they work
  // they must have been read from the new directory.
  EXPECT_FALSE(
      base::PathExists(real_tempdir_three.Append(kCookieDatabaseName)));

  base::HistogramTester histogram_tester;
  // Now create a new network context with migration set to false (default) but
  // pointing to the migrated directory. This verifies that even if no migration
  // is requested, the migrated data is still read correctly and that migration
  // is a one-way operation.
  auto file_paths = network::mojom::NetworkContextFilePaths::New();
  file_paths->data_directory = real_tempdir_three.Append(kNetworkSubpath);
  file_paths->unsandboxed_data_path = real_tempdir_three;
  file_paths->cookie_database_name = base::FilePath(kCookieDatabaseName);
  // If defaults are ever changed, this test will need to be updated.
  DCHECK_EQ(file_paths->trigger_migration, false);
  mojo::Remote<network::mojom::NetworkContext> network_context(
      CreateNetworkContextForPaths(
          std::move(file_paths),
          real_tempdir_three.Append(FILE_PATH_LITERAL("Cache"))));
  mojo::Remote<network::mojom::CookieManager> cookie_manager;
  network_context->GetCookieManager(
      cookie_manager.BindNewPipeAndPassReceiver());

  net::CookieList cookies = GetCookies(cookie_manager);
  histogram_tester.ExpectUniqueSample("NetworkService.GrantSandboxResult",
                                      /*sample=kMigrationAlreadySucceeded=*/10,
                                      /*expected_bucket_count=*/1);
  // Cookies work.
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ(kCookieName, cookies[0].Name());
  EXPECT_EQ(kCookieValue, cookies[0].Value());
}

// This test verifies that a new un-used data path will be initialized correctly
// if `unsandboxed_data_path` is set. The Cookie file should be placed into the
// `data_directory` and not `unsandboxed_data_path`.
IN_PROC_BROWSER_TEST_F(MAYBE_NetworkServiceDataMigrationBrowserTest,
                       NewDataDirWithMigrationTest) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::FilePath tempdir;
  EXPECT_TRUE(base::CreateTemporaryDirInDir(
      shell()->web_contents()->GetBrowserContext()->GetPath(),
      /*prefix=*/FILE_PATH_LITERAL(""), &tempdir));

  EXPECT_FALSE(base::PathExists(tempdir.Append(kCookieDatabaseName)));

  auto file_paths = network::mojom::NetworkContextFilePaths::New();
  file_paths->data_directory = tempdir.Append(FILE_PATH_LITERAL("Network"));
  file_paths->unsandboxed_data_path = tempdir;
  file_paths->cookie_database_name = base::FilePath(kCookieDatabaseName);
  file_paths->trigger_migration = true;
  base::HistogramTester histogram_tester;

  mojo::Remote<network::mojom::NetworkContext> network_context(
      CreateNetworkContextForPaths(std::move(file_paths),
                                   tempdir.Append(FILE_PATH_LITERAL("Cache"))));
  mojo::Remote<network::mojom::CookieManager> cookie_manager;
  network_context->GetCookieManager(
      cookie_manager.BindNewPipeAndPassReceiver());

  SetCookie(cookie_manager);
  FlushCookies(cookie_manager);

  // Verify that the cookie file exists in the `data_directory` and not the
  // `unsandboxed_data_path`.
  EXPECT_FALSE(base::PathExists(tempdir.Append(kCookieDatabaseName)));
  EXPECT_TRUE(base::PathExists(tempdir.Append(FILE_PATH_LITERAL("Network"))
                                   .Append(kCookieDatabaseName)));

  net::CookieList cookies = GetCookies(cookie_manager);
  histogram_tester.ExpectUniqueSample("NetworkService.GrantSandboxResult",
                                      /*sample=kSuccess=*/0,
                                      /*expected_bucket_count=*/1);
  // Cookie should be there.
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ(kCookieName, cookies[0].Name());
  EXPECT_EQ(kCookieValue, cookies[0].Value());
}

// A test where a caller specifies both `data_directory` and
// `unsandboxed_data_path` but does not wish migration to occur. The data should
// be in `unsandboxed_data_path` in this case.
IN_PROC_BROWSER_TEST_F(MAYBE_NetworkServiceDataMigrationBrowserTest,
                       NewDataDirWithNoMigrationTest) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::FilePath tempdir;
  EXPECT_TRUE(base::CreateTemporaryDirInDir(
      shell()->web_contents()->GetBrowserContext()->GetPath(),
      /*prefix=*/FILE_PATH_LITERAL(""), &tempdir));

  EXPECT_FALSE(base::PathExists(tempdir.Append(kCookieDatabaseName)));

  auto file_paths = network::mojom::NetworkContextFilePaths::New();
  file_paths->data_directory = tempdir.Append(FILE_PATH_LITERAL("Network"));
  file_paths->unsandboxed_data_path = tempdir;
  file_paths->cookie_database_name = base::FilePath(kCookieDatabaseName);
  file_paths->trigger_migration = false;
  base::HistogramTester histogram_tester;

  mojo::Remote<network::mojom::NetworkContext> network_context(
      CreateNetworkContextForPaths(std::move(file_paths),
                                   tempdir.Append(FILE_PATH_LITERAL("Cache"))));
  mojo::Remote<network::mojom::CookieManager> cookie_manager;
  network_context->GetCookieManager(
      cookie_manager.BindNewPipeAndPassReceiver());

  SetCookie(cookie_manager);
  FlushCookies(cookie_manager);

  // Verify that the cookie file still exists in the `unsandboxed_data_path`.
  EXPECT_TRUE(base::PathExists(tempdir.Append(kCookieDatabaseName)));
  // Verify that the cookie file has not been migrated to `data_directory`.
  EXPECT_FALSE(base::PathExists(tempdir.Append(FILE_PATH_LITERAL("Network"))
                                    .Append(kCookieDatabaseName)));
  // Verify no checkpoint file was written either.
  EXPECT_FALSE(base::PathExists(tempdir.Append(FILE_PATH_LITERAL("Network"))
                                    .Append(kCheckpointFileName)));

  net::CookieList cookies = GetCookies(cookie_manager);
  histogram_tester.ExpectUniqueSample("NetworkService.GrantSandboxResult",
                                      /*sample=kNoMigrationRequested=*/9,
                                      /*expected_bucket_count=*/1);

  // Cookie should be there.
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ(kCookieName, cookies[0].Name());
  EXPECT_EQ(kCookieValue, cookies[0].Value());
}

// A test where a caller specifies `data_directory` but does not specify
// anything else, including `unsandboxed_data_path`. This verifies that existing
// behavior remains the same for call-sites that do not update anything.
IN_PROC_BROWSER_TEST_F(MAYBE_NetworkServiceDataMigrationBrowserTest,
                       LegacyDataDir) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::FilePath tempdir;
  EXPECT_TRUE(base::CreateTemporaryDirInDir(
      shell()->web_contents()->GetBrowserContext()->GetPath(),
      /*prefix=*/FILE_PATH_LITERAL(""), &tempdir));

  EXPECT_FALSE(base::PathExists(tempdir.Append(kCookieDatabaseName)));

  auto file_paths = network::mojom::NetworkContextFilePaths::New();
  file_paths->data_directory = tempdir;
  file_paths->cookie_database_name = base::FilePath(kCookieDatabaseName);

  base::HistogramTester histogram_tester;
  mojo::Remote<network::mojom::NetworkContext> network_context(
      CreateNetworkContextForPaths(std::move(file_paths),
                                   tempdir.Append(FILE_PATH_LITERAL("Cache"))));
  mojo::Remote<network::mojom::CookieManager> cookie_manager;
  network_context->GetCookieManager(
      cookie_manager.BindNewPipeAndPassReceiver());

  SetCookie(cookie_manager);
  FlushCookies(cookie_manager);

  // Verify that the cookie file exists in the `unsandboxed_data_path`.
  EXPECT_TRUE(base::PathExists(tempdir.Append(kCookieDatabaseName)));

  net::CookieList cookies = GetCookies(cookie_manager);
  histogram_tester.ExpectUniqueSample(
      "NetworkService.GrantSandboxResult",
      /*sample=kDidNotAttemptToGrantSandboxAccess=*/7,
      /*expected_bucket_count=*/1);

  // Cookie should be there.
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ(kCookieName, cookies[0].Name());
  EXPECT_EQ(kCookieValue, cookies[0].Value());
}

// This test is similar to the tests above that use two directories, but uses a
// third directory to verify that if a migration has previously occurred using
// the previous code without the checkpoint file, and then later takes place
// using the new code, then the data is still read from the correct directory
// despite there not being a checkpoint file prior to the migration.
IN_PROC_BROWSER_TEST_F(MAYBE_NetworkServiceDataMigrationBrowserTest,
                       MigratedPreviouslyAndMigrateAgain) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::FilePath tempdir_one;
  EXPECT_TRUE(base::CreateTemporaryDirInDir(
      shell()->web_contents()->GetBrowserContext()->GetPath(),
      /*prefix=*/FILE_PATH_LITERAL("one"), &tempdir_one));
  base::FilePath tempdir_two;
  EXPECT_TRUE(base::CreateTemporaryDirInDir(
      shell()->web_contents()->GetBrowserContext()->GetPath(),
      /*prefix=*/FILE_PATH_LITERAL("two"), &tempdir_two));
  // Migrate within tempdir_two.
  MigrationTestInternal(tempdir_one, tempdir_two, FailureType::kNoFailures);
  // base::CopyDirectory copies the directory into a new directory if the target
  // directory already exists, so fix up the directory name here.
  base::FilePath real_tempdir_two = tempdir_two.Append(tempdir_one.BaseName());
  // Double check that the migration happened.
  EXPECT_TRUE(base::PathExists(
      real_tempdir_two.Append(kNetworkSubpath).Append(kCookieDatabaseName)));
  // Create a third testing directory, and copy the migrated data from
  // tempdir_two into it.
  base::FilePath tempdir_three;
  EXPECT_TRUE(base::CreateTemporaryDirInDir(
      shell()->web_contents()->GetBrowserContext()->GetPath(),
      /*prefix=*/FILE_PATH_LITERAL("three"), &tempdir_three));
  EXPECT_TRUE(base::CopyDirectory(real_tempdir_two, tempdir_three, true));
  // base::CopyDirectory copies the directory into a new directory if the target
  // directory already exists, so fix up the directory name here.
  base::FilePath real_tempdir_three =
      tempdir_three.Append(real_tempdir_two.BaseName());
  // Double check the directory was copied right.
  EXPECT_TRUE(base::PathExists(
      real_tempdir_three.Append(kNetworkSubpath).Append(kCookieDatabaseName)));
  // Double check cookies are not in the old directory, meaning if they work
  // they must have been read from the new directory.
  EXPECT_FALSE(
      base::PathExists(real_tempdir_three.Append(kCookieDatabaseName)));
  base::FilePath checkpoint_file =
      real_tempdir_three.Append(kNetworkSubpath).Append(kCheckpointFileName);
  // The directory should be fully migrated.
  EXPECT_TRUE(base::PathExists(checkpoint_file));
  // Delete the checkpoint file. This simulates that the directory was
  // previously migrated before the concept of a checkpoint file had been
  // introduced.
  EXPECT_TRUE(base::DeleteFile(checkpoint_file));
  // Test would be invalid if the delete failed.
  EXPECT_FALSE(base::PathExists(checkpoint_file));

  base::HistogramTester histogram_tester;
  auto file_paths = network::mojom::NetworkContextFilePaths::New();
  file_paths->data_directory = real_tempdir_three.Append(kNetworkSubpath);
  file_paths->unsandboxed_data_path = real_tempdir_three;
  file_paths->cookie_database_name = base::FilePath(kCookieDatabaseName);
  file_paths->trigger_migration = true;
  mojo::Remote<network::mojom::NetworkContext> network_context(
      CreateNetworkContextForPaths(
          std::move(file_paths),
          real_tempdir_three.Append(FILE_PATH_LITERAL("Cache"))));
  mojo::Remote<network::mojom::CookieManager> cookie_manager;
  network_context->GetCookieManager(
      cookie_manager.BindNewPipeAndPassReceiver());

  net::CookieList cookies = GetCookies(cookie_manager);
  // Success is reported here because although no files were copied from
  // `unsandboxed_data_path` to `data_directory`, the migration still succeeded
  // because a fresh Checkpoint file was placed down, and existing files were
  // preserved in the `data_directory`.
  histogram_tester.ExpectUniqueSample("NetworkService.GrantSandboxResult",
                                      /*sample=kSuccess=*/0,
                                      /*expected_bucket_count=*/1);

  // Cookies work.
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ(kCookieName, cookies[0].Name());
  EXPECT_EQ(kCookieValue, cookies[0].Value());

  EXPECT_TRUE(base::PathExists(checkpoint_file));
}

// Disable instantiation of parametrized tests for disk access sandboxing on
// Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_InProcess DISABLED_InProcess
#define MAYBE_OutOfProcess DISABLED_OutOfProcess
#else
#define MAYBE_InProcess InProcess
#define MAYBE_OutOfProcess OutOfProcess
#endif  // BUILDFLAG(IS_ANDROID)

INSTANTIATE_TEST_SUITE_P(
    MAYBE_InProcess,
    MAYBE_NetworkServiceDataMigrationBrowserTestWithFailures,
    ::testing::Combine(::testing::Values(true),
                       ::testing::ValuesIn(kFailureTypes)));
INSTANTIATE_TEST_SUITE_P(
    MAYBE_OutOfProcess,
    MAYBE_NetworkServiceDataMigrationBrowserTestWithFailures,
    ::testing::Combine(::testing::Values(false),
                       ::testing::ValuesIn(kFailureTypes)));

#endif  // !BUILDFLAG(IS_FUCHSIA)

class NetworkServiceInProcessBrowserTest : public ContentBrowserTest {
 public:
  NetworkServiceInProcessBrowserTest() { ForceInProcessNetworkService(); }

  NetworkServiceInProcessBrowserTest(
      const NetworkServiceInProcessBrowserTest&) = delete;
  NetworkServiceInProcessBrowserTest& operator=(
      const NetworkServiceInProcessBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    EXPECT_TRUE(embedded_test_server()->Start());
  }
};

// Verifies that in-process network service works.
IN_PROC_BROWSER_TEST_F(NetworkServiceInProcessBrowserTest, Basic) {
  GURL test_url = embedded_test_server()->GetURL("foo.com", "/echo");
  StoragePartitionImpl* partition =
      static_cast<StoragePartitionImpl*>(shell()
                                             ->web_contents()
                                             ->GetBrowserContext()
                                             ->GetDefaultStoragePartition());
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  ASSERT_EQ(net::OK,
            LoadBasicRequest(partition->GetNetworkContext(), test_url));
}

class NetworkServiceInvalidLogBrowserTest : public ContentBrowserTest {
 public:
  NetworkServiceInvalidLogBrowserTest() = default;

  NetworkServiceInvalidLogBrowserTest(
      const NetworkServiceInvalidLogBrowserTest&) = delete;
  NetworkServiceInvalidLogBrowserTest& operator=(
      const NetworkServiceInvalidLogBrowserTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(network::switches::kLogNetLog, "/abc/def");
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    EXPECT_TRUE(embedded_test_server()->Start());
  }
};

// Verifies that an invalid --log-net-log flag won't crash the browser.
IN_PROC_BROWSER_TEST_F(NetworkServiceInvalidLogBrowserTest, Basic) {
  GURL test_url = embedded_test_server()->GetURL("foo.com", "/echo");
  StoragePartitionImpl* partition =
      static_cast<StoragePartitionImpl*>(shell()
                                             ->web_contents()
                                             ->GetBrowserContext()
                                             ->GetDefaultStoragePartition());
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  ASSERT_EQ(net::OK,
            LoadBasicRequest(partition->GetNetworkContext(), test_url));
}

// Test fixture for using a NetworkService that has a non-default limit on the
// number of allowed open UDP sockets.
class NetworkServiceWithUDPSocketLimit : public NetworkServiceBrowserTest {
 public:
  NetworkServiceWithUDPSocketLimit() {
    base::FieldTrialParams params;
    params[net::features::kLimitOpenUDPSocketsMax.name] =
        base::NumberToString(kMaxUDPSockets);
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        net::features::kLimitOpenUDPSockets, params);
  }

 protected:
  static constexpr int kMaxUDPSockets = 4;

  // Creates and synchronously connects a UDPSocket using |network_context|.
  // Returns the network error for Connect().
  int ConnectUDPSocketSync(
      mojo::Remote<network::mojom::NetworkContext>* network_context,
      mojo::Remote<network::mojom::UDPSocket>* socket) {
    network_context->get()->CreateUDPSocket(
        socket->BindNewPipeAndPassReceiver(), mojo::NullRemote());

    // The address of this endpoint doesn't matter, since Connect() will not
    // actually send any datagrams, and is only being called to verify the
    // socket limit enforcement.
    net::IPEndPoint remote_addr(net::IPAddress(127, 0, 0, 1), 8080);

    network::mojom::UDPSocketOptionsPtr options =
        network::mojom::UDPSocketOptions::New();

    net::IPEndPoint local_addr;
    network::test::UDPSocketTestHelper helper(socket);
    return helper.ConnectSync(remote_addr, std::move(options), &local_addr);
  }

  // Creates a NetworkContext using default parameters.
  mojo::Remote<network::mojom::NetworkContext> CreateNetworkContext() {
    mojo::Remote<network::mojom::NetworkContext> network_context;
    network::mojom::NetworkContextParamsPtr context_params =
        network::mojom::NetworkContextParams::New();
    context_params->cert_verifier_params = GetCertVerifierParams(
        cert_verifier::mojom::CertVerifierCreationParams::New());
    CreateNetworkContextInNetworkService(
        network_context.BindNewPipeAndPassReceiver(),
        std::move(context_params));
    return network_context;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests calling Connect() on |kMaxUDPSockets + 4| sockets. The first
// kMaxUDPSockets should succeed, whereas the last 4 should fail with
// ERR_INSUFFICIENT_RESOURCES due to having exceeding the global bound.
IN_PROC_BROWSER_TEST_F(NetworkServiceWithUDPSocketLimit,
                       UDPSocketBoundEnforced) {
  auto network_contexts =
      std::to_array<mojo::Remote<network::mojom::NetworkContext>>({
          CreateNetworkContext(),
          CreateNetworkContext(),
      });

  std::array<mojo::Remote<network::mojom::UDPSocket>, kMaxUDPSockets> sockets;

  // Try to connect the maximum number of UDP sockets (|kMaxUDPSockets|),
  // spread evenly between 2 NetworkContexts. These should succeed as the
  // global limit has not been reached yet. This assumes there are no
  // other consumers of UDP sockets in the browser yet.
  for (size_t i = 0; i < kMaxUDPSockets; ++i) {
    auto* network_context = &network_contexts[i % network_contexts.size()];
    EXPECT_EQ(net::OK, ConnectUDPSocketSync(network_context, &sockets[i]));
  }

  // Try to connect an additional 4 sockets, alternating between each of the
  // NetworkContexts. These should all fail with ERR_INSUFFICIENT_RESOURCES as
  // the limit has already been reached. Spreading across NetworkContext
  // is done to ensure the socket limit is global and not per
  // NetworkContext.
  for (size_t i = 0; i < 4; ++i) {
    auto* network_context = &network_contexts[i % network_contexts.size()];
    mojo::Remote<network::mojom::UDPSocket> socket;
    EXPECT_EQ(net::ERR_INSUFFICIENT_RESOURCES,
              ConnectUDPSocketSync(network_context, &socket));
  }
}

class NetworkServiceNetLogBrowserTest : public ContentBrowserTest {
 public:
  NetworkServiceNetLogBrowserTest() {
    EXPECT_TRUE(embedded_test_server()->Start());
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    log_path_ = temp_dir_.Take();
    log_path_ = log_path_.Append(FILE_PATH_LITERAL("my_net_log_file.json"));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchPath(network::switches::kLogNetLog, log_path_);
  }

  void TearDownInProcessBrowserTestFixture() override {
    // Check that the log file exists and has been written to.
    base::File log_file_read(log_path_,
                             base::File::FLAG_OPEN | base::File::FLAG_READ);
    EXPECT_TRUE(log_file_read.IsValid());
    base::File::Info file_info;
    log_file_read.GetInfo(&file_info);
    EXPECT_GT(file_info.size, 0);
  }

 protected:
  base::FilePath log_path_;
  base::ScopedTempDir temp_dir_;
};

// Tests that a log file is generated and is of non-zero size.
IN_PROC_BROWSER_TEST_F(NetworkServiceNetLogBrowserTest, LogCreated) {
  // Navigate to a page to generate some data.
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));
  // Because the file isn't closed until the network service shuts down the
  // final checks are performed in TearDownInProcessBrowserTestFixture().
}

class NetworkServiceBoundedNetLogBrowserTest
    : public NetworkServiceNetLogBrowserTest {
 public:
  NetworkServiceBoundedNetLogBrowserTest() {
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_FUCHSIA)
    // Network sandboxing disallows the creation of a temp directory needed by
    // bounded net-logs. Disable it for this test.
    scoped_feature_list_.InitAndDisableFeature(
        sandbox::policy::features::kNetworkServiceSandbox);
#endif
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    NetworkServiceNetLogBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(network::switches::kNetLogMaxSizeMb,
                                    base::NumberToString(kMaxSizeMegaBytes));
    command_line->AppendSwitchASCII(network::switches::kNetLogCaptureMode,
                                    "Everything");
  }

  void TearDownInProcessBrowserTestFixture() override {
    // Check that the log file exists and has been written to.
    base::File log_file_read(log_path_,
                             base::File::FLAG_OPEN | base::File::FLAG_READ);
    auto error = log_file_read.error_details();
    EXPECT_EQ(error, base::File::FILE_OK);
    EXPECT_TRUE(log_file_read.IsValid());

// Skip for Fuchsia. Fuchsia's file size operation isn't fully supported yet,
// making the file size meaningless and therefore these checks.
#if !BUILDFLAG(IS_FUCHSIA)
    base::File::Info file_info;
    log_file_read.GetInfo(&file_info);

    // The max size is only a rough bound, so let's make sure the final file is
    // within a reasonable range from our max. Let's say 10%.
    const int64_t kMaxSizeUpper = kMaxSizeBytes * 1.1;
    const int64_t kMaxSizeLower = kMaxSizeBytes * 0.9;

    // Some devices don't always finish flushing the file to disk before
    // control is returned to the test, meaning that if we were to immediately
    // get the file size it would be smaller than expected because it's not
    // fully written out. Keep trying until the file is within the expected
    // range or quit if we reach our timeout.

    base::Time timeout_time =
        base::Time::Now() + TestTimeouts::action_max_timeout();
    while (
        !(file_info.size > kMaxSizeLower && file_info.size < kMaxSizeUpper)) {
      base::PlatformThread::Sleep(base::Milliseconds(10));
      log_file_read.GetInfo(&file_info);

      if (base::Time::Now() >= timeout_time) {
        break;
      }
    }

    EXPECT_GT(file_info.size, kMaxSizeLower);
    EXPECT_LT(file_info.size, kMaxSizeUpper);
#endif
  }

  // For testing, have a max log size of 1 MB. 1024*1024 == 2^20 == left shift
  // by 20 bits
  const uint32_t kMaxSizeMegaBytes = 1;
  const uint64_t kMaxSizeBytes = kMaxSizeMegaBytes << 20;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This is disabled for Mac and iOS. Mac due to the crbug below, and iOS because
// the test is flaky and the feature it's testing isn't usable on iOS.
//
// TODO(crbug.com/40276296): Try-bots use a different temp directory that the
// Mac network sandbox doesn't allow and causes this test to fail. Disable the
// test until this is resolved.
#if BUILDFLAG(IS_APPLE)
#define MAYBE_LogCreated DISABLED_LogCreated
#else
#define MAYBE_LogCreated LogCreated
#endif

IN_PROC_BROWSER_TEST_F(NetworkServiceBoundedNetLogBrowserTest,
                       MAYBE_LogCreated) {
  // Navigate to a page to generate some data.
  // Through trial and error it was found that this looping navigation results
  // in a ~2MB unbounded net-log file. Since our bounded net-log is limited to
  // 1MB this is fine.

  // This string is roughly 8KB;
  const std::string kManyAs(8192, 'a');
  for (int i = 0; i < 30; i++) {
    EXPECT_TRUE(NavigateToURL(
        shell(), embedded_test_server()->GetURL("/echo?" + kManyAs)));
  }
  // Because the file isn't closed until the network service shuts down the
  // final checks are performed in TearDownInProcessBrowserTestFixture().
}

class TestCookieEncryptionProvider
    : public network::mojom::CookieEncryptionProvider {
 public:
  TestCookieEncryptionProvider() = default;

  mojo::PendingRemote<network::mojom::CookieEncryptionProvider> BindRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }
  MOCK_METHOD(void, GetEncryptor, (GetEncryptorCallback callback), (override));

 private:
  mojo::Receiver<network::mojom::CookieEncryptionProvider> receiver_{this};
};

class NetworkServiceCookieEncryptionBrowserTest
    : public ContentBrowserTest,
      public testing::WithParamInterface</*kProtectEncryptionKey*/ bool> {
 public:
#if BUILDFLAG(IS_WIN)
  NetworkServiceCookieEncryptionBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(
        os_crypt_async::features::kProtectEncryptionKey, GetParam());
  }
#endif  // BUILDFLAG(IS_WIN)

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // A test key provider that takes a fixed key.
  class TestKeyProvider : public os_crypt_async::KeyProvider {
   public:
    explicit TestKeyProvider(base::span<const uint8_t> key)
        : key_(key.begin(), key.end()) {}

   private:
    void GetKey(KeyCallback callback) final {
      std::move(callback).Run(
          "_", os_crypt_async::Encryptor::Key(
                   key_, os_crypt_async::mojom::Algorithm::kAES256GCM));
    }

    bool UseForEncryption() final { return true; }
    bool IsCompatibleWithOsCryptSync() final { return false; }

    const std::vector<uint8_t> key_;
  };

#if BUILDFLAG(IS_WIN)
 private:
  base::test::ScopedFeatureList scoped_feature_list_;
#endif  // BUILDFLAG(IS_WIN)
};

// This test verifies that when a cookie encryption provider is set when
// creating a network context, then it results in a call to the GetEncryptor
// method on the CookieEncryptionProvider.
IN_PROC_BROWSER_TEST_P(NetworkServiceCookieEncryptionBrowserTest,
                       CookieEncryptionProvider) {
  const auto data_path =
      shell()->web_contents()->GetBrowserContext()->GetPath();
  auto context_params = network::mojom::NetworkContextParams::New();
  context_params->file_paths = network::mojom::NetworkContextFilePaths::New();
  context_params->file_paths->unsandboxed_data_path = data_path;
  context_params->file_paths->trigger_migration = true;
  context_params->file_paths->data_directory =
      data_path.Append(FILE_PATH_LITERAL("TestContext"));
  context_params->file_paths->cookie_database_name =
      base::FilePath(FILE_PATH_LITERAL("Cookies"));
  context_params->cert_verifier_params = GetCertVerifierParams(
      cert_verifier::mojom::CertVerifierCreationParams::New());
  context_params->enable_encrypted_cookies = true;

  testing::StrictMock<TestCookieEncryptionProvider> provider;
  context_params->cookie_encryption_provider = provider.BindRemote();

  mojo::Remote<network::mojom::NetworkContext> network_context;
  content::CreateNetworkContextInNetworkService(
      network_context.BindNewPipeAndPassReceiver(), std::move(context_params));
  std::vector<uint8_t> key_data(
      os_crypt_async::Encryptor::Key::kAES256GCMKeySize);
  base::RandBytes(key_data);
  std::vector<std::pair<size_t, std::unique_ptr<os_crypt_async::KeyProvider>>>
      providers;

  providers.emplace_back(/*precedence=*/10u,
                         std::make_unique<TestKeyProvider>(key_data));

  os_crypt_async::OSCryptAsync os_crypt_async(std::move(providers));
  EXPECT_CALL(provider, GetEncryptor)
      .WillOnce([&os_crypt_async](network::mojom::CookieEncryptionProvider::
                                      GetEncryptorCallback callback) {
        std::ignore = os_crypt_async.GetInstance(base::BindOnce(
            [](network::mojom::CookieEncryptionProvider::GetEncryptorCallback
                   callback,
               os_crypt_async::Encryptor encryptor,
               bool result) { std::move(callback).Run(std::move(encryptor)); },
            std::move(callback)));
      });

  // Cookie here needs to be non-session to be written to the Cookies file.
  GURL test_url = embedded_test_server()->GetURL(
      "foo.com", "/cookies/set_persistent_cookie.html");

  // This artificial delay verifies https://crbug.com/1511730 is fixed.
  mojo::Remote<network::mojom::CookieManager> cookie_manager;
  network_context->GetCookieManager(
      cookie_manager.BindNewPipeAndPassReceiver());
  cookie_manager->SetPreCommitCallbackDelayForTesting(base::Seconds(3));

  ASSERT_EQ(net::OK, LoadBasicRequest(network_context.get(), test_url));
  // This part of the test does not work with Address Sanitizer as it takes
  // copies of the memory in shadow memory. In debug mode, the size of the
  // memory is too large and it takes too long (>45s) on bots, and times out.
#if BUILDFLAG(IS_WIN) && !defined(ADDRESS_SANITIZER) && defined(NDEBUG)
  if (IsInProcessNetworkService()) {
    return;
  }
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath temp_path;
    ASSERT_TRUE(base::CreateTemporaryFile(&temp_path));

    base::File temp_file;
    temp_file.Initialize(
        temp_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_READ |
                       base::File::FLAG_WRITE | base::File::FLAG_WIN_TEMPORARY |
                       base::File::FLAG_DELETE_ON_CLOSE);
    ASSERT_TRUE(temp_file.IsValid());
    base::Process peer_process = base::Process::OpenWithExtraPrivileges(
        GetNetworkServiceProcess().Pid());
    const auto minidump_type = static_cast<MINIDUMP_TYPE>(
        MiniDumpWithFullMemory | MiniDumpIgnoreInaccessibleMemory);
    ASSERT_TRUE(::MiniDumpWriteDump(peer_process.Handle(), peer_process.Pid(),
                                    temp_file.GetPlatformFile(), minidump_type,
                                    nullptr, nullptr, nullptr));
    base::MemoryMappedFile map;
    ASSERT_TRUE(map.Initialize(std::move(temp_file)));

    auto it = map.bytes().begin();
    size_t occurrences = 0;
    while ((it = std::search(it, map.bytes().end(), key_data.begin(),
                             key_data.end())) != map.bytes().end()) {
      ++occurrences;
      it += key_data.size();
    }

    // If kProtectEncryptionKey is enabled, no instances of the key should be
    // present in the full memory dump of the network service process.
    EXPECT_EQ(GetParam() ? 0u : 1u, occurrences);
  }
#endif  // BUILDFLAG(IS_WIN) && !defined(ADDRESS_SANITIZER) && defined(NDEBUG)
}

INSTANTIATE_TEST_SUITE_P(,
                         NetworkServiceCookieEncryptionBrowserTest,
                         ::testing::Values(false
#if BUILDFLAG(IS_WIN)
                                           ,
                                           true
#endif
                                           ),
                         [](const auto& info) {
                           return info.param ? "ProtectOn" : "ProtectOff";
                         });

#if BUILDFLAG(IS_WIN)
class NetworkServiceCodeIntegrityTest : public NetworkServiceBrowserTest {
 public:
  NetworkServiceCodeIntegrityTest() {
    scoped_feature_list_.InitWithFeatures(
        {sandbox::policy::features::kNetworkServiceCodeIntegrity,
         sandbox::policy::features::kNetworkServiceSandbox},
        {});
    ForceOutOfProcessNetworkService();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This test verifies that the NetworkServiceCodeIntegrity feature works when
// used in conjunction with the network service sandbox on Windows.
IN_PROC_BROWSER_TEST_F(NetworkServiceCodeIntegrityTest, Enabled) {
  // Verify pages load.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/empty.html")));
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

}  // namespace content
