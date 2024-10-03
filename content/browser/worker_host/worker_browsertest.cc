// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/path_service.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/system/sys_info.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/process_lock.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/worker_host/shared_worker_service_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/base/features.h"
#include "net/base/filename_util.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/dns/mock_host_resolver.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/embedded_test_server/connection_tracker.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

namespace content {

namespace {

const char kSameSiteCookie[] = "same-site-cookie=same-site-cookie-value";

// Used by both the embedded test server when a header specified by
// "/echoheader" is missing, and by the test fixture when there's no cookie
// present.
const char kNoCookie[] = "None";

bool SupportsSharedWorker() {
#if BUILDFLAG(IS_ANDROID)
  // SharedWorkers are not enabled on Android. https://crbug.com/154571
  //
  // TODO(davidben): Move other SharedWorker exclusions from
  // build/android/pylib/gtest/filter/ inline.
  return false;
#else
  return true;
#endif
}

}  // namespace

// These tests are parameterized on following options:
// 0 => Base
// 1 => kPlzDedicatedWorker enabled
// 2 => kPrivateNetworkAccessForWorkers enabled
class WorkerTest : public ContentBrowserTest,
                   public testing::WithParamInterface<int> {
 public:
  WorkerTest() : select_certificate_count_(0) {
    switch (GetParam()) {
      case 0:  // Base case.
        feature_list_.InitWithFeatures({},
                                       {
                                           blink::features::kPlzDedicatedWorker,
                                       });
        break;
      case 1:  // PlzDedicatedWorker
        feature_list_.InitWithFeatures(
            {
                blink::features::kPlzDedicatedWorker,
            },
            {
                features::kPrivateNetworkAccessForWorkers,
            });
        break;
      case 2:  // PrivateNetworkAccessForWorkers
        feature_list_.InitWithFeatures(
            {
                blink::features::kPlzDedicatedWorker,
                features::kPrivateNetworkAccessForWorkers,
            },
            {});
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ShellContentBrowserClient::Get()->set_select_client_certificate_callback(
        base::BindOnce(&WorkerTest::OnSelectClientCertificate,
                       base::Unretained(this)));
    ssl_server_.AddDefaultHandlers(GetTestDataFilePath());
    ssl_server_.RegisterRequestHandler(base::BindRepeating(
        &WorkerTest::MonitorRequestCookies, base::Unretained(this)));
    ssl_server_.SetSSLConfig(
        net::test_server::EmbeddedTestServer::CERT_TEST_NAMES);
    ASSERT_TRUE(ssl_server_.Start());
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(ssl_server_.ShutdownAndWaitUntilComplete());
  }

  int select_certificate_count() const { return select_certificate_count_; }

  GURL GetTestFileURL(const std::string& test_case) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath path;
    EXPECT_TRUE(base::PathService::Get(content::DIR_TEST_DATA, &path));
    path = path.AppendASCII("workers").AppendASCII(test_case);
    return net::FilePathToFileURL(path);
  }

  GURL GetTestURL(const std::string& test_case, const std::string& query) {
    std::string url_string = "/workers/" + test_case + "?" + query;
    return ssl_server_.GetURL("a.test", url_string);
  }

  void RunTest(Shell* window, const GURL& url, bool expect_failure = false) {
    const std::u16string ok_title = u"OK";
    const std::u16string fail_title = u"FAIL";
    TitleWatcher title_watcher(window->web_contents(), ok_title);
    title_watcher.AlsoWaitForTitle(fail_title);
    EXPECT_TRUE(NavigateToURL(window, url));
    std::u16string final_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expect_failure ? fail_title : ok_title, final_title);
  }

  void RunTest(const GURL& url, bool expect_failure = false) {
    RunTest(shell(), url, expect_failure);
  }

  static void QuitUIMessageLoop(base::OnceClosure callback,
                                bool is_primary_main_frame /* unused */,
                                bool is_navigation /* unused */) {
    GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(callback));
  }

  void NavigateAndWaitForAuth(const GURL& url) {
    ShellContentBrowserClient* browser_client =
        ShellContentBrowserClient::Get();
    scoped_refptr<MessageLoopRunner> runner = new MessageLoopRunner();
    browser_client->set_login_request_callback(
        base::BindOnce(&QuitUIMessageLoop, runner->QuitClosure()));
    shell()->LoadURL(url);
    runner->Run();
  }

  void SetSameSiteCookie(const std::string& host) {
    StoragePartition* partition = shell()
                                      ->web_contents()
                                      ->GetBrowserContext()
                                      ->GetDefaultStoragePartition();
    mojo::Remote<network::mojom::CookieManager> cookie_manager;
    partition->GetNetworkContext()->GetCookieManager(
        cookie_manager.BindNewPipeAndPassReceiver());
    net::CookieOptions options;
    options.set_same_site_cookie_context(
        net::CookieOptions::SameSiteCookieContext(
            net::CookieOptions::SameSiteCookieContext::ContextType::
                SAME_SITE_LAX));
    GURL cookie_url = ssl_server_.GetURL(host, "/");
    std::unique_ptr<net::CanonicalCookie> cookie =
        net::CanonicalCookie::CreateForTesting(
            cookie_url, std::string(kSameSiteCookie) + "; SameSite=Lax; Secure",
            base::Time::Now());
    base::RunLoop run_loop;
    cookie_manager->SetCanonicalCookie(
        *cookie, cookie_url, options,
        base::BindLambdaForTesting(
            [&](net::CookieAccessResult set_cookie_result) {
              EXPECT_TRUE(set_cookie_result.status.IsInclude());
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  // Returns the cookie received with the request for the specified path. If the
  // path was requested but no cookie was received, return kNoCookie. Waits for
  // the path to be requested if it hasn't been requested already.
  std::string GetReceivedCookie(const std::string& path) {
    {
      base::AutoLock auto_lock(path_cookie_map_lock_);
      DCHECK(path_to_wait_for_.empty());
      DCHECK(!path_wait_loop_);
      if (path_cookie_map_.find(path) != path_cookie_map_.end())
        return path_cookie_map_[path];
      path_to_wait_for_ = path;
      path_wait_loop_ = std::make_unique<base::RunLoop>();
    }

    path_wait_loop_->Run();

    base::AutoLock auto_lock(path_cookie_map_lock_);
    path_to_wait_for_.clear();
    path_wait_loop_.reset();
    return path_cookie_map_[path];
  }

  void ClearReceivedCookies() {
    base::AutoLock auto_lock(path_cookie_map_lock_);
    path_cookie_map_.clear();
  }

  SharedWorkerHost* GetSharedWorkerHost(const GURL& url) {
    StoragePartition* partition = shell()
                                      ->web_contents()
                                      ->GetBrowserContext()
                                      ->GetDefaultStoragePartition();
    DCHECK(partition);
    auto* service = static_cast<SharedWorkerServiceImpl*>(
        partition->GetSharedWorkerService());
    return service->FindMatchingSharedWorkerHost(
        url, "", blink::StorageKey::CreateFirstParty(url::Origin::Create(url)),
        blink::mojom::SharedWorkerSameSiteCookies::kAll);
  }

  net::test_server::EmbeddedTestServer* ssl_server() { return &ssl_server_; }

 private:
  base::OnceClosure OnSelectClientCertificate(
      content::WebContents* web_contents,
      net::SSLCertRequestInfo* cert_request_info,
      net::ClientCertIdentityList client_certs,
      std::unique_ptr<content::ClientCertificateDelegate> delegate) {
    select_certificate_count_++;
    return base::OnceClosure();
  }

  std::unique_ptr<net::test_server::HttpResponse> MonitorRequestCookies(
      const net::test_server::HttpRequest& request) {
    // Ignore every host but "a.test", to help catch cases of sending requests
    // to the wrong host.
    auto host_header = request.headers.find("Host");
    if (host_header == request.headers.end() ||
        !base::StartsWith(host_header->second,
                          "a.test:", base::CompareCase::SENSITIVE)) {
      return nullptr;
    }

    base::AutoLock auto_lock(path_cookie_map_lock_);

    if (path_cookie_map_.find(request.relative_url) != path_cookie_map_.end()) {
      path_cookie_map_[request.relative_url] = "path requested multiple times";
      return nullptr;
    }

    auto cookie_header = request.headers.find("Cookie");
    if (cookie_header == request.headers.end()) {
      path_cookie_map_[request.relative_url] = kNoCookie;
    } else {
      path_cookie_map_[request.relative_url] = cookie_header->second;
    }
    if (path_to_wait_for_ == request.relative_url) {
      path_wait_loop_->Quit();
    }
    return nullptr;
  }

  // Mapping of paths requested from "a.test" to cookies they were requested
  // with. Paths may only be requested once without clearing the map.
  std::map<std::string, std::string> path_cookie_map_
      GUARDED_BY(path_cookie_map_lock_);
  // If non-empty, path to wait for the test server to see a request for on the
  // "a.test" server.
  std::string path_to_wait_for_ GUARDED_BY(path_cookie_map_lock_);
  // If non-null, quit when a request for |path_to_wait_for_| is observed. May
  // only be created or dereferenced off of the UI thread while holding
  // |path_cookie_map_lock_|, its run method must be called while not holding
  // the lock.
  std::unique_ptr<base::RunLoop> path_wait_loop_;
  // Lock that must be held while modifying |path_cookie_map_|, as it's used on
  // both the test server's thread and the UI thread.
  base::Lock path_cookie_map_lock_;

  // The cookie tests require an SSL server, since SameSite None cookies can
  // only be set on secure origins. Most other tests use this, too, to keep
  // things simpler, though they could use an HTTP server instead.
  net::test_server::EmbeddedTestServer ssl_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};

  int select_certificate_count_;

  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, WorkerTest, testing::Range(0, 3));

IN_PROC_BROWSER_TEST_P(WorkerTest, SingleWorker) {
  RunTest(GetTestURL("single_worker.html", std::string()));
}

IN_PROC_BROWSER_TEST_P(WorkerTest, SingleWorkerFromFile) {
  RunTest(GetTestFileURL("single_worker.html"), true /* expect_failure */);
}

class WorkerTestWithAllowFileAccessFromFiles : public WorkerTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WorkerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kAllowFileAccessFromFiles);
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         WorkerTestWithAllowFileAccessFromFiles,
                         testing::Range(0, 3));

IN_PROC_BROWSER_TEST_P(WorkerTestWithAllowFileAccessFromFiles,
                       SingleWorkerFromFile) {
  RunTest(GetTestFileURL("single_worker.html"));
}

IN_PROC_BROWSER_TEST_P(WorkerTest, HttpPageCantCreateFileWorker) {
  GURL url = GetTestURL(
      "single_worker.html",
      "workerUrl=" + base::EscapeQueryParamValue(
                         GetTestFileURL("worker_common.js").spec(), true));
  RunTest(url, /*expect_failure=*/true);
}

IN_PROC_BROWSER_TEST_P(WorkerTest, MultipleWorkers) {
  RunTest(GetTestURL("multi_worker.html", std::string()));
}

IN_PROC_BROWSER_TEST_P(WorkerTest, SingleSharedWorker) {
  if (!SupportsSharedWorker())
    return;

  RunTest(GetTestURL("single_worker.html", "shared=true"));
}

// Create a SharedWorker from a COEP:required-corp document.
IN_PROC_BROWSER_TEST_P(WorkerTest, SharedWorkerInCOEPRequireCorpDocument) {
  if (!SupportsSharedWorker())
    return;

  // Navigate to a page living in an isolated process.
  EXPECT_TRUE(NavigateToURL(
      shell(), ssl_server()->GetURL("a.test", "/cross-origin-isolated.html")));
  RenderFrameHostImpl* page_rfh = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  auto page_lock =
      ProcessLock::FromSiteInfo(page_rfh->GetSiteInstance()->GetSiteInfo());
  EXPECT_TRUE(page_lock.GetWebExposedIsolationInfo().is_isolated());
  EXPECT_GT(page_rfh->GetWebExposedIsolationLevel(),
            WebExposedIsolationLevel::kNotIsolated);

  // Create a shared worker from the cross-origin-isolated page:

  // COEP:unsafe-none
  //
  // With CoepForSharedWorker: the worker's COEP policy is laxer than its
  // creator, it is blocked as a result. It can't communicate with the document,
  // outside of the worker.onerror message.
  // Without CoepForSharedWorker: the worker isn't blocked, but it should at
  // least not be loaded in the cross-origin isolated process.
  EXPECT_EQ("Worker connected.", EvalJs(shell(), R"(
    new Promise(resolve => {
      const worker =
        new SharedWorker("/workers/messageport_worker.js");
      worker.onerror = (e) => resolve("Worker blocked.");
      worker.port.onmessage = (e) => resolve(e.data);
    })
  )"));
  auto* host = GetSharedWorkerHost(
      ssl_server()->GetURL("a.test", "/workers/messageport_worker.js"));
  EXPECT_TRUE(host);
  RenderProcessHost* worker_rph = host->GetProcessHost();
  EXPECT_NE(worker_rph, page_rfh->GetProcess());
  auto worker_lock =
      ProcessLock::FromSiteInfo(host->site_instance()->GetSiteInfo());
  EXPECT_FALSE(worker_lock.GetWebExposedIsolationInfo().is_isolated());

  // COEP:credentialless
  EXPECT_EQ("Worker connected.", EvalJs(shell(), R"(
    new Promise(resolve => {
      const worker =
        new SharedWorker("/workers/messageport_worker_coep_credentialless.js");
      worker.onerror = (e) => resolve("Worker blocked.");
      worker.port.onmessage = (e) => resolve(e.data);
    })
  )"));
  auto* host_credentialless = GetSharedWorkerHost(ssl_server()->GetURL(
      "a.test", "/workers/messageport_worker_coep_credentialless.js"));
  EXPECT_TRUE(host_credentialless);
  RenderProcessHost* worker_rph_credentialless =
      host_credentialless->GetProcessHost();
  EXPECT_NE(worker_rph_credentialless, page_rfh->GetProcess());
  auto worker_lock_credentialless = ProcessLock::FromSiteInfo(
      host_credentialless->site_instance()->GetSiteInfo());
  // Cross-origin isolation is not yet supported in COEP:credentialless
  // SharedWorker.
  EXPECT_FALSE(
      worker_lock_credentialless.GetWebExposedIsolationInfo().is_isolated());

  // COEP:require-corp
  EXPECT_EQ("Worker connected.", EvalJs(shell(), R"(
    new Promise(resolve => {
      const worker =
        new SharedWorker("/workers/messageport_worker_coep_require_corp.js");
      worker.onerror = (e) => resolve("Worker blocked.");
      worker.port.onmessage = (e) => resolve(e.data);
    })
  )"));
  auto* host_require_corp = GetSharedWorkerHost(ssl_server()->GetURL(
      "a.test", "/workers/messageport_worker_coep_require_corp.js"));
  RenderProcessHost* worker_rph_require_corp =
      host_require_corp->GetProcessHost();
  EXPECT_NE(worker_rph_require_corp, page_rfh->GetProcess());
  auto worker_lock_require_corp = ProcessLock::FromSiteInfo(
      host_require_corp->site_instance()->GetSiteInfo());
  // Cross-origin isolation is not yet supported in COEP:require-corp
  // SharedWorker.
  EXPECT_FALSE(
      worker_lock_require_corp.GetWebExposedIsolationInfo().is_isolated());
}

// Create a SharedWorker from a COEP:credentialless document.
IN_PROC_BROWSER_TEST_P(WorkerTest, SharedWorkerInCOEPCredentiallessDocument) {
  if (!SupportsSharedWorker())
    return;

  // Navigate to a page living in an isolated process.
  EXPECT_TRUE(NavigateToURL(
      shell(), ssl_server()->GetURL(
                   "a.test", "/cross-origin-isolated-credentialless.html")));
  RenderFrameHostImpl* page_rfh = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  auto page_lock =
      ProcessLock::FromSiteInfo(page_rfh->GetSiteInstance()->GetSiteInfo());
  EXPECT_TRUE(page_lock.GetWebExposedIsolationInfo().is_isolated());

  // Create a SharedWorker from the cross-origin-isolated page.

  // COEP:unsafe-none
  //
  // With CoepForSharedWorker: the worker's COEP policy is laxer than its
  // creator, it is blocked as a result. It can't communicate with the document,
  // outside of the worker.onerror message.
  // Without CoepForSharedWorker: the worker isn't blocked, but it should at
  // least not be loaded in the cross-origin isolated process.
  EXPECT_EQ("Worker connected.", EvalJs(shell(), R"(
    new Promise(resolve => {
      const worker =
        new SharedWorker("/workers/messageport_worker.js");
      worker.onerror = (e) => resolve("Worker blocked.");
      worker.port.onmessage = (e) => resolve(e.data);
    })
  )"));
  auto* host = GetSharedWorkerHost(
      ssl_server()->GetURL("a.test", "/workers/messageport_worker.js"));
  EXPECT_TRUE(host);
  RenderProcessHost* worker_rph = host->GetProcessHost();
  EXPECT_NE(worker_rph, page_rfh->GetProcess());
  auto worker_lock =
      ProcessLock::FromSiteInfo(host->site_instance()->GetSiteInfo());
  EXPECT_FALSE(worker_lock.GetWebExposedIsolationInfo().is_isolated());

  // COEP:credentialless
  EXPECT_EQ("Worker connected.", EvalJs(shell(), R"(
    new Promise(resolve => {
      const worker =
        new SharedWorker("/workers/messageport_worker_coep_credentialless.js");
      worker.onerror = (e) => resolve("Worker blocked.");
      worker.port.onmessage = (e) => resolve(e.data);
    })
  )"));
  auto* host_credentialless = GetSharedWorkerHost(ssl_server()->GetURL(
      "a.test", "/workers/messageport_worker_coep_credentialless.js"));
  EXPECT_TRUE(host_credentialless);
  RenderProcessHost* worker_rph_credentialless =
      host_credentialless->GetProcessHost();
  EXPECT_NE(worker_rph_credentialless, page_rfh->GetProcess());
  auto worker_lock_credentialless = ProcessLock::FromSiteInfo(
      host_credentialless->site_instance()->GetSiteInfo());
  // Cross-origin isolation is not yet supported in COEP:credentialless
  // SharedWorker.
  EXPECT_FALSE(
      worker_lock_credentialless.GetWebExposedIsolationInfo().is_isolated());

  // COEP:require-corp
  EXPECT_EQ("Worker connected.", EvalJs(shell(), R"(
    new Promise(resolve => {
      const worker =
        new SharedWorker("/workers/messageport_worker_coep_require_corp.js");
      worker.onerror = (e) => resolve("Worker blocked.");
      worker.port.onmessage = (e) => resolve(e.data);
    })
  )"));
  auto* host_require_corp = GetSharedWorkerHost(ssl_server()->GetURL(
      "a.test", "/workers/messageport_worker_coep_require_corp.js"));
  RenderProcessHost* worker_rph_require_corp =
      host_require_corp->GetProcessHost();
  EXPECT_NE(worker_rph_require_corp, page_rfh->GetProcess());
  auto worker_lock_require_corp = ProcessLock::FromSiteInfo(
      host_require_corp->site_instance()->GetSiteInfo());
  // Cross-origin isolation is not yet supported in COEP:require-corp
  // SharedWorker.
  EXPECT_FALSE(
      worker_lock_require_corp.GetWebExposedIsolationInfo().is_isolated());
}

// http://crbug.com/96435
IN_PROC_BROWSER_TEST_P(WorkerTest, MultipleSharedWorkers) {
  if (!SupportsSharedWorker())
    return;

  RunTest(GetTestURL("multi_worker.html", "shared=true"));
}

// Incognito windows should not share workers with non-incognito windows
// http://crbug.com/30021
IN_PROC_BROWSER_TEST_P(WorkerTest, IncognitoSharedWorkers) {
  if (!SupportsSharedWorker())
    return;

  // Load a non-incognito tab and have it create a shared worker
  RunTest(ssl_server()->GetURL("a.test", "/workers/incognito_worker.html"));

  // Incognito worker should not share with non-incognito
  RunTest(CreateOffTheRecordBrowser(),
          ssl_server()->GetURL("a.test", "/workers/incognito_worker.html"));
}

// Make sure that auth dialog is displayed from worker context.
// http://crbug.com/33344
IN_PROC_BROWSER_TEST_P(WorkerTest, WorkerHttpAuth) {
  GURL url = ssl_server()->GetURL("a.test", "/workers/worker_auth.html");

  NavigateAndWaitForAuth(url);
}

// Tests that TLS client auth prompts for normal workers's importScripts.
IN_PROC_BROWSER_TEST_P(WorkerTest, WorkerTlsClientAuthImportScripts) {
  // Launch HTTPS server.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  ASSERT_TRUE(https_server.Start());

  RunTest(GetTestURL(
      "worker_tls_client_auth.html",
      "test=import&url=" +
          base::EscapeQueryParamValue(https_server.GetURL("/").spec(), true)));
  EXPECT_EQ(1, select_certificate_count());
}

// Tests that TLS client auth prompts for normal workers's fetch() call.
IN_PROC_BROWSER_TEST_P(WorkerTest, WorkerTlsClientAuthFetch) {
  // Launch HTTPS server.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  ASSERT_TRUE(https_server.Start());

  RunTest(GetTestURL(
      "worker_tls_client_auth.html",
      "test=fetch&url=" +
          base::EscapeQueryParamValue(https_server.GetURL("/").spec(), true)));
  EXPECT_EQ(1, select_certificate_count());
}

// Tests that TLS client auth does not prompt for a shared worker; shared
// workers are not associated with a WebContents.
IN_PROC_BROWSER_TEST_P(WorkerTest, SharedWorkerTlsClientAuthImportScripts) {
  if (!SupportsSharedWorker())
    return;

  // Launch HTTPS server.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  ASSERT_TRUE(https_server.Start());

  RunTest(GetTestURL(
      "worker_tls_client_auth.html",
      "test=import&shared=true&url=" +
          base::EscapeQueryParamValue(https_server.GetURL("/").spec(), true)));
  EXPECT_EQ(0, select_certificate_count());
}

IN_PROC_BROWSER_TEST_P(WorkerTest, WebSocketSharedWorker) {
  if (!SupportsSharedWorker())
    return;

  // Launch WebSocket server.
  net::SpawnedTestServer ws_server(net::SpawnedTestServer::TYPE_WS,
                                   net::GetWebSocketTestDataDirectory());
  ASSERT_TRUE(ws_server.Start());

  // Generate test URL.
  GURL::Replacements replacements;
  replacements.SetSchemeStr("http");
  GURL url = ws_server.GetURL("websocket_shared_worker.html")
                 .ReplaceComponents(replacements);

  // Run test.
  Shell* window = shell();
  const std::u16string expected_title = u"OK";
  TitleWatcher title_watcher(window->web_contents(), expected_title);
  EXPECT_TRUE(NavigateToURL(window, url));
  std::u16string final_title = title_watcher.WaitAndGetTitle();
  EXPECT_EQ(expected_title, final_title);
}

IN_PROC_BROWSER_TEST_P(WorkerTest, PassMessagePortToSharedWorker) {
  if (!SupportsSharedWorker())
    return;

  RunTest(GetTestURL("pass_messageport_to_sharedworker.html", ""));
}

IN_PROC_BROWSER_TEST_P(WorkerTest,
                       PassMessagePortToSharedWorkerDontWaitForConnect) {
  if (!SupportsSharedWorker())
    return;

  RunTest(GetTestURL(
      "pass_messageport_to_sharedworker_dont_wait_for_connect.html", ""));
}

// Tests the value of |request_initiator| for shared worker resources.
IN_PROC_BROWSER_TEST_P(WorkerTest,
                       VerifyInitiatorAndSameSiteCookiesSharedWorker) {
  if (!SupportsSharedWorker())
    return;

  const GURL start_url(ssl_server()->GetURL("b.test", "/frame_tree/top.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  // To make things tricky about |top_frame_origin|, this test navigates to
  // a page on |ssl_server()| which has a cross-origin iframe that registers the
  // worker.
  std::string cross_site_domain("a.test");
  const GURL test_url(ssl_server()->GetURL(
      cross_site_domain, "/workers/simple_shared_worker.html"));

  // There are three requests to test:
  // 1) The request for the worker itself ("worker.js")
  // 2) importScripts("empty.js") from the worker
  // 3) fetch("empty.html") from the worker
  const GURL worker_url(
      ssl_server()->GetURL(cross_site_domain, "/workers/worker.js"));
  const GURL script_url(
      ssl_server()->GetURL(cross_site_domain, "/workers/empty.js"));
  const GURL resource_url(
      ssl_server()->GetURL(cross_site_domain, "/workers/empty.html"));

  // Set a cookie for verfifying which requests send SameSite cookies.
  SetSameSiteCookie(cross_site_domain);

  std::set<GURL> expected_request_urls = {worker_url, script_url, resource_url};
  const url::Origin expected_origin =
      url::Origin::Create(worker_url.DeprecatedGetOriginAsURL());

  base::RunLoop waiter;
  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        auto it = expected_request_urls.find(params->url_request.url);
        if (it != expected_request_urls.end()) {
          EXPECT_TRUE(params->url_request.request_initiator.has_value());
          EXPECT_EQ(expected_origin,
                    params->url_request.request_initiator.value());
          expected_request_urls.erase(it);
        }
        if (expected_request_urls.empty())
          waiter.Quit();
        return false;
      }));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), test_url));
  waiter.Run();

  // Check cookies sent with each request to "a.test".
  // Neither the frame nor the SharedWorker should get SameSite cookies.
  EXPECT_EQ(kNoCookie, GetReceivedCookie(test_url.path()));
  EXPECT_EQ(kNoCookie, GetReceivedCookie(worker_url.path()));
  EXPECT_EQ(kNoCookie, GetReceivedCookie(script_url.path()));
  EXPECT_EQ(kNoCookie, GetReceivedCookie(resource_url.path()));
}

// Test that an "a.test" worker sends "a.test" SameSite cookies, both when
// requesting the worker script and when fetching other resources.
IN_PROC_BROWSER_TEST_P(WorkerTest, WorkerSameSiteCookies1) {
  SetSameSiteCookie("a.test");
  ASSERT_TRUE(NavigateToURL(
      shell(),
      ssl_server()->GetURL(
          "a.test",
          "/workers/create_worker.html?worker_url=fetch_from_worker.js")));
  EXPECT_EQ(kSameSiteCookie,
            EvalJs(shell()->web_contents(),
                   "worker.postMessage({url: '/echoheader?Cookie'}); "
                   "waitForMessage();"));
  EXPECT_EQ(kSameSiteCookie,
            GetReceivedCookie(
                "/workers/create_worker.html?worker_url=fetch_from_worker.js"));
  EXPECT_EQ(kSameSiteCookie,
            GetReceivedCookie("/workers/fetch_from_worker.js"));
  EXPECT_EQ(kSameSiteCookie, GetReceivedCookie("/echoheader?Cookie"));
}

// Test that a "b.test" worker does not send "a.test" SameSite cookies when
// fetching resources.
IN_PROC_BROWSER_TEST_P(WorkerTest, WorkerSameSiteCookies2) {
  SetSameSiteCookie("a.test");
  ASSERT_TRUE(NavigateToURL(
      shell(),
      ssl_server()->GetURL(
          "b.test",
          "/workers/create_worker.html?worker_url=fetch_from_worker.js")));
  EXPECT_EQ(kNoCookie,
            EvalJs(shell()->web_contents(),
                   JsReplace("worker.postMessage({url: $1}); waitForMessage();",
                             ssl_server()
                                 ->GetURL("a.test", "/echoheader?Cookie")
                                 .spec()
                                 .c_str())));
  EXPECT_EQ(kNoCookie, GetReceivedCookie("/echoheader?Cookie"));
}

// Test that an "a.test" nested worker sends "a.test" SameSite cookies, both
// when requesting the worker script and when fetching other resources.
IN_PROC_BROWSER_TEST_P(WorkerTest, NestedWorkerSameSiteCookies) {
  SetSameSiteCookie("a.test");
  ASSERT_TRUE(NavigateToURL(
      shell(),
      ssl_server()->GetURL(
          "a.test",
          "/workers/"
          "create_worker.html?worker_url=fetch_from_nested_worker.js")));
  EXPECT_EQ(kSameSiteCookie,
            EvalJs(shell()->web_contents(),
                   "worker.postMessage({url: '/echoheader?Cookie'}); "
                   "waitForMessage();"));
  EXPECT_EQ(kSameSiteCookie,
            GetReceivedCookie(
                "/workers/"
                "create_worker.html?worker_url=fetch_from_nested_worker.js"));
  EXPECT_EQ(kSameSiteCookie,
            GetReceivedCookie("/workers/fetch_from_nested_worker.js"));
  EXPECT_EQ(kSameSiteCookie,
            GetReceivedCookie("/workers/fetch_from_worker.js"));
  EXPECT_EQ(kSameSiteCookie, GetReceivedCookie("/echoheader?Cookie"));
}

// Test that an "a.test" iframe in a "b.test" frame does not send same-site
// cookies when requesting an "a.test" worker or when that worker requests
// "a.test" resources.
IN_PROC_BROWSER_TEST_P(WorkerTest,
                       CrossOriginIframeWorkerDoesNotSendSameSiteCookies1) {
  SetSameSiteCookie("a.test");

  ASSERT_TRUE(NavigateToURL(
      shell(), ssl_server()->GetURL("b.test", "/workers/frame_factory.html")));

  content::TestNavigationObserver navigation_observer(
      shell()->web_contents(), /*number_of_navigations*/ 1);

  const char kSubframeName[] = "foo";
  EvalJsResult result = EvalJs(
      shell()->web_contents()->GetPrimaryMainFrame(),
      JsReplace(
          "createFrame($1, $2)",
          ssl_server()
              ->GetURL(
                  "a.test",
                  "/workers/create_worker.html?worker_url=fetch_from_worker.js")
              .spec()
              .c_str(),
          kSubframeName));
  ASSERT_TRUE(result.error.empty());
  navigation_observer.Wait();

  RenderFrameHost* subframe_rfh = FrameMatchingPredicate(
      shell()->web_contents()->GetPrimaryPage(),
      base::BindRepeating(&FrameMatchesName, kSubframeName));
  ASSERT_TRUE(subframe_rfh);
  EXPECT_EQ(kNoCookie,
            EvalJs(subframe_rfh,
                   "worker.postMessage({url: '/echoheader?Cookie'}); "
                   "waitForMessage();"));
  EXPECT_EQ(kNoCookie,
            GetReceivedCookie(
                "/workers/create_worker.html?worker_url=fetch_from_worker.js"));
  EXPECT_EQ(kNoCookie, GetReceivedCookie("/workers/fetch_from_worker.js"));
  EXPECT_EQ(kNoCookie, GetReceivedCookie("/echoheader?Cookie"));
}

// Test that an "b.test" iframe in a "a.test" frame does not send same-site
// cookies when its "b.test" worker requests "a.test" resources.
IN_PROC_BROWSER_TEST_P(WorkerTest,
                       CrossOriginIframeWorkerDoesNotSendSameSiteCookies2) {
  SetSameSiteCookie("a.test");

  ASSERT_TRUE(NavigateToURL(
      shell(), ssl_server()->GetURL("a.test", "/workers/frame_factory.html")));

  content::TestNavigationObserver navigation_observer(
      shell()->web_contents(), /*number_of_navigations*/ 1);

  const char kSubframeName[] = "foo";
  EvalJsResult result = EvalJs(
      shell()->web_contents()->GetPrimaryMainFrame(),
      JsReplace(
          "createFrame($1, $2)",
          ssl_server()
              ->GetURL(
                  "b.test",
                  "/workers/create_worker.html?worker_url=fetch_from_worker.js")
              .spec()
              .c_str(),
          kSubframeName));
  ASSERT_TRUE(result.error.empty());
  navigation_observer.Wait();

  RenderFrameHost* subframe_rfh = FrameMatchingPredicate(
      shell()->web_contents()->GetPrimaryPage(),
      base::BindRepeating(&FrameMatchesName, kSubframeName));
  ASSERT_TRUE(subframe_rfh);
  EXPECT_EQ(kNoCookie,
            EvalJs(subframe_rfh,
                   JsReplace("worker.postMessage({url: $1}); waitForMessage();",
                             ssl_server()
                                 ->GetURL("a.test", "/echoheader?Cookie")
                                 .spec()
                                 .c_str())));
  EXPECT_EQ(kNoCookie, GetReceivedCookie("/echoheader?Cookie"));
}

class WorkerFromCredentiallessIframeNikBrowserTest : public WorkerTest {
 public:
  WorkerFromCredentiallessIframeNikBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        net::features::kPartitionConnectionsByNetworkIsolationKey);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Enable parsing the iframe 'credentialless' attribute.
    command_line->AppendSwitch(switches::kEnableBlinkTestFeatures);
    WorkerTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    connection_tracker_ = std::make_unique<net::test_server::ConnectionTracker>(
        embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
    WorkerTest::SetUpOnMainThread();
  }

  void ResetNetworkState() {
    auto* network_context = shell()
                                ->web_contents()
                                ->GetBrowserContext()
                                ->GetDefaultStoragePartition()
                                ->GetNetworkContext();
    base::RunLoop close_all_connections_loop;
    network_context->CloseAllConnections(
        close_all_connections_loop.QuitClosure());
    close_all_connections_loop.Run();

    connection_tracker_->ResetCounts();
  }

 protected:
  std::unique_ptr<net::test_server::ConnectionTracker> connection_tracker_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         WorkerFromCredentiallessIframeNikBrowserTest,
                         testing::Range(0, 3));

IN_PROC_BROWSER_TEST_P(WorkerFromCredentiallessIframeNikBrowserTest,
                       SharedWorkerRequestIsDoneWithPartitionedNetworkState) {
  if (!SupportsSharedWorker())
    return;

  GURL main_url = embedded_test_server()->GetURL("/title1.html");

  for (bool credentialless : {false, true}) {
    SCOPED_TRACE(credentialless ? "credentialless iframe" : "normal iframe");
    EXPECT_TRUE(NavigateToURL(shell(), main_url));

    RenderFrameHostImpl* main_rfh = static_cast<RenderFrameHostImpl*>(
        shell()->web_contents()->GetPrimaryMainFrame());

    // Create an iframe.
    EXPECT_TRUE(ExecJs(main_rfh,
                       JsReplace("let child = document.createElement('iframe');"
                                 "child.src = $1;"
                                 "child.credentialless = $2;"
                                 "document.body.appendChild(child);",
                                 main_url, credentialless)));
    WaitForLoadStop(shell()->web_contents());
    EXPECT_EQ(1U, main_rfh->child_count());
    RenderFrameHostImpl* iframe = main_rfh->child_at(0)->current_frame_host();
    EXPECT_EQ(credentialless, iframe->IsCredentialless());
    EXPECT_EQ(credentialless, EvalJs(iframe, "window.credentialless"));
    ResetNetworkState();

    GURL worker_url = embedded_test_server()->GetURL("/workers/worker.js");

    // Preconnect a socket with the NetworkAnonymizationKey of the main frame.
    shell()
        ->web_contents()
        ->GetBrowserContext()
        ->GetDefaultStoragePartition()
        ->GetNetworkContext()
        ->PreconnectSockets(1, worker_url.DeprecatedGetOriginAsURL(),
                            network::mojom::CredentialsMode::kInclude,
                            main_rfh->GetIsolationInfoForSubresources()
                                .network_anonymization_key());

    connection_tracker_->WaitForAcceptedConnections(1);
    EXPECT_EQ(1u, connection_tracker_->GetAcceptedSocketCount());
    EXPECT_EQ(0u, connection_tracker_->GetReadSocketCount());

    std::string start_worker = JsReplace("new SharedWorker($1);", worker_url);

    ExecuteScriptAsync(iframe, start_worker);
    connection_tracker_->WaitUntilConnectionRead();

    // The normal iframe should reuse the preconnected socket, the
    // credentialless iframe should open a new one.
    if (credentialless) {
      EXPECT_EQ(2u, connection_tracker_->GetAcceptedSocketCount());
    } else {
      EXPECT_EQ(1u, connection_tracker_->GetAcceptedSocketCount());
    }
    EXPECT_EQ(1u, connection_tracker_->GetReadSocketCount());
  }
}

// Test that an "a.test" frame starting a worker without any `sameSiteCookies`
// option sends SameSite cookies on the request.
IN_PROC_BROWSER_TEST_P(WorkerTest, SameSiteCookiesSharedWorkerSameDefault) {
  if (!SupportsSharedWorker()) {
    return;
  }
  SetSameSiteCookie("a.test");
  ASSERT_TRUE(NavigateToURL(
      shell(), ssl_server()->GetURL("a.test", "/workers/simple.html")));
  EvalJsResult result =
      EvalJs(shell(), "new SharedWorker('/workers/worker.js');");
  ASSERT_TRUE(result.error.empty());
  EXPECT_EQ(kSameSiteCookie, GetReceivedCookie("/workers/worker.js"));
  EXPECT_EQ(kSameSiteCookie, GetReceivedCookie("/workers/empty.js"));
  EXPECT_EQ(kSameSiteCookie, GetReceivedCookie("/workers/empty.html"));
}

// Test that an "a.test" frame starting a worker with `sameSiteCookies: 'none'`
// doesn't send SameSite cookies on the request.
IN_PROC_BROWSER_TEST_P(WorkerTest, SameSiteCookiesSharedWorkerSameNone) {
  if (!SupportsSharedWorker()) {
    return;
  }
  SetSameSiteCookie("a.test");
  ASSERT_TRUE(NavigateToURL(
      shell(), ssl_server()->GetURL("a.test", "/workers/simple.html")));
  EvalJsResult result = EvalJs(
      shell(),
      "new SharedWorker('/workers/worker.js', {sameSiteCookies: 'none'});");
  ASSERT_TRUE(result.error.empty());
  EXPECT_EQ(kNoCookie, GetReceivedCookie("/workers/worker.js"));
  EXPECT_EQ(kNoCookie, GetReceivedCookie("/workers/empty.js"));
  EXPECT_EQ(kNoCookie, GetReceivedCookie("/workers/empty.html"));
}

// Test that an "a.test" frame starting a worker with `sameSiteCookies: 'none'`
// sends SameSite cookies on the request.
IN_PROC_BROWSER_TEST_P(WorkerTest, SameSiteCookiesSharedWorkerSameAll) {
  if (!SupportsSharedWorker()) {
    return;
  }
  SetSameSiteCookie("a.test");
  ASSERT_TRUE(NavigateToURL(
      shell(), ssl_server()->GetURL("a.test", "/workers/simple.html")));
  EvalJsResult result = EvalJs(
      shell(),
      "new SharedWorker('/workers/worker.js', {sameSiteCookies: 'all'});");
  ASSERT_TRUE(result.error.empty());
  EXPECT_EQ(kSameSiteCookie, GetReceivedCookie("/workers/worker.js"));
  EXPECT_EQ(kSameSiteCookie, GetReceivedCookie("/workers/empty.js"));
  EXPECT_EQ(kSameSiteCookie, GetReceivedCookie("/workers/empty.html"));
}

// Test that an "a.test" iframe in a "b.test" frame starting a worker without
// any `sameSiteCookies` option doesn't send SameSite cookies on the request.
IN_PROC_BROWSER_TEST_P(WorkerTest, SameSiteCookiesSharedWorkerCrossDefault) {
  if (!SupportsSharedWorker()) {
    return;
  }
  SetSameSiteCookie("a.test");
  ASSERT_TRUE(NavigateToURL(
      shell(), ssl_server()->GetURL("b.test", "/workers/frame_factory.html")));
  content::TestNavigationObserver navigation_observer(
      shell()->web_contents(), /*number_of_navigations*/ 1);
  const char kSubframeName[] = "foo";
  EvalJsResult frame_result = EvalJs(
      shell()->web_contents()->GetPrimaryMainFrame(),
      JsReplace(
          "createFrame($1, $2)",
          ssl_server()->GetURL("a.test", "/workers/simple.html").spec().c_str(),
          kSubframeName));
  ASSERT_TRUE(frame_result.error.empty());
  navigation_observer.Wait();
  RenderFrameHost* subframe_rfh = FrameMatchingPredicate(
      shell()->web_contents()->GetPrimaryPage(),
      base::BindRepeating(&FrameMatchesName, kSubframeName));
  ASSERT_TRUE(subframe_rfh);
  EvalJsResult worker_result =
      EvalJs(subframe_rfh, "new SharedWorker('/workers/worker.js');");
  ASSERT_TRUE(worker_result.error.empty());
  EXPECT_EQ(kNoCookie, GetReceivedCookie("/workers/worker.js"));
  EXPECT_EQ(kNoCookie, GetReceivedCookie("/workers/empty.js"));
  EXPECT_EQ(kNoCookie, GetReceivedCookie("/workers/empty.html"));
}

// Test that an "a.test" iframe in a "b.test" frame starting a worker with
// `sameSiteCookies: 'none'` doesn't send SameSite cookies on the request.
IN_PROC_BROWSER_TEST_P(WorkerTest, SameSiteCookiesSharedWorkerCrossNone) {
  if (!SupportsSharedWorker()) {
    return;
  }
  SetSameSiteCookie("a.test");
  ASSERT_TRUE(NavigateToURL(
      shell(), ssl_server()->GetURL("b.test", "/workers/frame_factory.html")));
  content::TestNavigationObserver navigation_observer(
      shell()->web_contents(), /*number_of_navigations*/ 1);
  const char kSubframeName[] = "foo";
  EvalJsResult frame_result = EvalJs(
      shell()->web_contents()->GetPrimaryMainFrame(),
      JsReplace(
          "createFrame($1, $2)",
          ssl_server()->GetURL("a.test", "/workers/simple.html").spec().c_str(),
          kSubframeName));
  ASSERT_TRUE(frame_result.error.empty());
  navigation_observer.Wait();
  RenderFrameHost* subframe_rfh = FrameMatchingPredicate(
      shell()->web_contents()->GetPrimaryPage(),
      base::BindRepeating(&FrameMatchesName, kSubframeName));
  ASSERT_TRUE(subframe_rfh);
  EvalJsResult worker_result = EvalJs(
      subframe_rfh,
      "new SharedWorker('/workers/worker.js', {sameSiteCookies: 'none'});");
  ASSERT_TRUE(worker_result.error.empty());
  EXPECT_EQ(kNoCookie, GetReceivedCookie("/workers/worker.js"));
  EXPECT_EQ(kNoCookie, GetReceivedCookie("/workers/empty.js"));
  EXPECT_EQ(kNoCookie, GetReceivedCookie("/workers/empty.html"));
}

// Test that an "a.test" iframe in a "b.test" frame cannot set
// `sameSiteCookies: 'all'` option when starting a shared worker.
IN_PROC_BROWSER_TEST_P(WorkerTest, SameSiteCookiesSharedWorkerCrossAll) {
  if (!SupportsSharedWorker()) {
    return;
  }
  SetSameSiteCookie("a.test");
  ASSERT_TRUE(NavigateToURL(
      shell(), ssl_server()->GetURL("b.test", "/workers/frame_factory.html")));
  content::TestNavigationObserver navigation_observer(
      shell()->web_contents(), /*number_of_navigations*/ 1);
  const char kSubframeName[] = "foo";
  EvalJsResult result_frame = EvalJs(
      shell()->web_contents()->GetPrimaryMainFrame(),
      JsReplace(
          "createFrame($1, $2)",
          ssl_server()->GetURL("a.test", "/workers/simple.html").spec().c_str(),
          kSubframeName));
  ASSERT_TRUE(result_frame.error.empty());
  navigation_observer.Wait();
  RenderFrameHost* subframe_rfh = FrameMatchingPredicate(
      shell()->web_contents()->GetPrimaryPage(),
      base::BindRepeating(&FrameMatchesName, kSubframeName));
  ASSERT_TRUE(subframe_rfh);
  EvalJsResult worker_result = EvalJs(
      subframe_rfh,
      "new SharedWorker('/workers/worker.js', {sameSiteCookies: 'all'});");
  ASSERT_FALSE(worker_result.error.empty());
}

}  // namespace content
