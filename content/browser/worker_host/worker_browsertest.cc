// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/check.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/system/sys_info.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/browser/web_contents/web_contents_impl.h"
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
#include "net/base/escape.h"
#include "net/base/filename_util.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/dns/mock_host_resolver.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace content {

namespace {

const char kSameSiteCookie[] = "same-site-cookie=same-site-cookie-value";

// Used by both the embedded test server when a header specified by
// "/echoheader" is missing, and by the test fixture when there's no cookie
// present.
const char kNoCookie[] = "None";

bool SupportsSharedWorker() {
#if defined(OS_ANDROID)
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

// These tests are parameterized on whether kPlzDedicatedWorker is enabled.
class WorkerTest : public ContentBrowserTest,
                   public testing::WithParamInterface<bool> {
 public:
  WorkerTest() : select_certificate_count_(0) {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(blink::features::kPlzDedicatedWorker);
    } else {
      feature_list_.InitAndDisableFeature(blink::features::kPlzDedicatedWorker);
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
    const base::string16 ok_title = base::ASCIIToUTF16("OK");
    const base::string16 fail_title = base::ASCIIToUTF16("FAIL");
    TitleWatcher title_watcher(window->web_contents(), ok_title);
    title_watcher.AlsoWaitForTitle(fail_title);
    EXPECT_TRUE(NavigateToURL(window, url));
    base::string16 final_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expect_failure ? fail_title : ok_title, final_title);
  }

  void RunTest(const GURL& url, bool expect_failure = false) {
    RunTest(shell(), url, expect_failure);
  }

  static void QuitUIMessageLoop(base::OnceClosure callback,
                                bool is_main_frame /* unused */) {
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
    StoragePartition* partition = BrowserContext::GetDefaultStoragePartition(
        shell()->web_contents()->GetBrowserContext());
    mojo::Remote<network::mojom::CookieManager> cookie_manager;
    partition->GetNetworkContext()->GetCookieManager(
        cookie_manager.BindNewPipeAndPassReceiver());
    net::CookieOptions options;
    options.set_same_site_cookie_context(
        net::CookieOptions::SameSiteCookieContext(
            net::CookieOptions::SameSiteCookieContext::ContextType::
                SAME_SITE_LAX));
    GURL cookie_url = ssl_server_.GetURL(host, "/");
    std::unique_ptr<net::CanonicalCookie> cookie = net::CanonicalCookie::Create(
        cookie_url, std::string(kSameSiteCookie) + "; SameSite=Lax; Secure",
        base::Time::Now(), base::nullopt /* server_time */);
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

  net::test_server::EmbeddedTestServer* ssl_server() { return &ssl_server_; }

 private:
  void OnSelectClientCertificate() { select_certificate_count_++; }

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

INSTANTIATE_TEST_SUITE_P(All, WorkerTest, testing::ValuesIn({false, true}));

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
                         testing::ValuesIn({false, true}));

IN_PROC_BROWSER_TEST_P(WorkerTestWithAllowFileAccessFromFiles,
                       SingleWorkerFromFile) {
  RunTest(GetTestFileURL("single_worker.html"));
}

IN_PROC_BROWSER_TEST_P(WorkerTest, HttpPageCantCreateFileWorker) {
  GURL url = GetTestURL(
      "single_worker.html",
      "workerUrl=" + net::EscapeQueryParamValue(
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
          net::EscapeQueryParamValue(https_server.GetURL("/").spec(), true)));
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
          net::EscapeQueryParamValue(https_server.GetURL("/").spec(), true)));
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
          net::EscapeQueryParamValue(https_server.GetURL("/").spec(), true)));
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
  const base::string16 expected_title = base::ASCIIToUTF16("OK");
  TitleWatcher title_watcher(window->web_contents(), expected_title);
  EXPECT_TRUE(NavigateToURL(window, url));
  base::string16 final_title = title_watcher.WaitAndGetTitle();
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
      url::Origin::Create(worker_url.GetOrigin());

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
                            ->GetFrameTree()
                            ->root();
  NavigateFrameToURL(root->child_at(0), test_url);
  waiter.Run();

  // Check cookies sent with each request to "a.test". Frame request should not
  // have SameSite cookies, but SharedWorker could (though eventually this will
  // need to be changed, to protect against cross-site user tracking).
  EXPECT_EQ(kNoCookie, GetReceivedCookie(test_url.path()));
  EXPECT_EQ(kSameSiteCookie, GetReceivedCookie(worker_url.path()));
  EXPECT_EQ(kSameSiteCookie, GetReceivedCookie(script_url.path()));
  EXPECT_EQ(kSameSiteCookie, GetReceivedCookie(resource_url.path()));
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
      shell()->web_contents()->GetMainFrame(),
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
      shell()->web_contents(),
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
      shell()->web_contents()->GetMainFrame(),
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
      shell()->web_contents(),
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

}  // namespace content
