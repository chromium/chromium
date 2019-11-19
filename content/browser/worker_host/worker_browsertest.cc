// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/base/escape.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_data_directory.h"
#include "url/gurl.h"

namespace content {

namespace {

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

class WorkerTest : public ContentBrowserTest {
 public:
  WorkerTest() : select_certificate_count_(0) {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ShellContentBrowserClient::Get()->set_select_client_certificate_callback(
        base::BindOnce(&WorkerTest::OnSelectClientCertificate,
                       base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());
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
    return embedded_test_server()->GetURL(url_string);
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
    base::PostTask(FROM_HERE, {BrowserThread::UI}, std::move(callback));
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

 private:
  void OnSelectClientCertificate() { select_certificate_count_++; }

  int select_certificate_count_;
};

IN_PROC_BROWSER_TEST_F(WorkerTest, SingleWorker) {
  RunTest(GetTestURL("single_worker.html", std::string()));
}

IN_PROC_BROWSER_TEST_F(WorkerTest, SingleWorkerFromFile) {
  RunTest(GetTestFileURL("single_worker.html"));
}

IN_PROC_BROWSER_TEST_F(WorkerTest, HttpPageCantCreateFileWorker) {
  GURL url = GetTestURL(
      "single_worker.html",
      "workerUrl=" + net::EscapeQueryParamValue(
                         GetTestFileURL("worker_common.js").spec(), true));
  RunTest(url, /*expect_failure=*/true);
}

IN_PROC_BROWSER_TEST_F(WorkerTest, MultipleWorkers) {
  RunTest(GetTestURL("multi_worker.html", std::string()));
}

IN_PROC_BROWSER_TEST_F(WorkerTest, SingleSharedWorker) {
  if (!SupportsSharedWorker())
    return;

  RunTest(GetTestURL("single_worker.html", "shared=true"));
}

// http://crbug.com/96435
IN_PROC_BROWSER_TEST_F(WorkerTest, MultipleSharedWorkers) {
  if (!SupportsSharedWorker())
    return;

  RunTest(GetTestURL("multi_worker.html", "shared=true"));
}

// Incognito windows should not share workers with non-incognito windows
// http://crbug.com/30021
IN_PROC_BROWSER_TEST_F(WorkerTest, IncognitoSharedWorkers) {
  if (!SupportsSharedWorker())
    return;

  // Load a non-incognito tab and have it create a shared worker
  RunTest(embedded_test_server()->GetURL("/workers/incognito_worker.html"));

  // Incognito worker should not share with non-incognito
  RunTest(CreateOffTheRecordBrowser(),
          embedded_test_server()->GetURL("/workers/incognito_worker.html"));
}

// Make sure that auth dialog is displayed from worker context.
// http://crbug.com/33344
IN_PROC_BROWSER_TEST_F(WorkerTest, WorkerHttpAuth) {
  GURL url = embedded_test_server()->GetURL("/workers/worker_auth.html");

  NavigateAndWaitForAuth(url);
}

// Tests that TLS client auth prompts for normal workers's importScripts.
IN_PROC_BROWSER_TEST_F(WorkerTest, WorkerTlsClientAuthImportScripts) {
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
IN_PROC_BROWSER_TEST_F(WorkerTest, WorkerTlsClientAuthFetch) {
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
IN_PROC_BROWSER_TEST_F(WorkerTest, SharedWorkerTlsClientAuthImportScripts) {
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

IN_PROC_BROWSER_TEST_F(WorkerTest, WebSocketSharedWorker) {
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

IN_PROC_BROWSER_TEST_F(WorkerTest, PassMessagePortToSharedWorker) {
  if (!SupportsSharedWorker())
    return;

  RunTest(GetTestURL("pass_messageport_to_sharedworker.html", ""));
}

IN_PROC_BROWSER_TEST_F(WorkerTest,
                       PassMessagePortToSharedWorkerDontWaitForConnect) {
  if (!SupportsSharedWorker())
    return;

  RunTest(GetTestURL(
      "pass_messageport_to_sharedworker_dont_wait_for_connect.html", ""));
}

// Tests the value of |request_initiator| for shared worker resources.
IN_PROC_BROWSER_TEST_F(WorkerTest, VerifyInitiatorSharedWorker) {
  if (!SupportsSharedWorker())
    return;

  const GURL start_url(embedded_test_server()->GetURL("/frame_tree/top.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  // To make things tricky about |top_frame_origin|, this test navigates to
  // a page on |embedded_test_server()| which has a cross-origin iframe that
  // registers the worker.
  std::string cross_site_domain("cross-site.com");
  const GURL test_url(embedded_test_server()->GetURL(
      cross_site_domain, "/workers/simple_shared_worker.html"));

  // There are three requests to test:
  // 1) The request for the worker itself ("worker.js")
  // 2) importScripts("empty.js") from the worker
  // 3) fetch("empty.html") from the worker
  const GURL worker_url(
      embedded_test_server()->GetURL(cross_site_domain, "/workers/worker.js"));
  const GURL script_url(
      embedded_test_server()->GetURL(cross_site_domain, "/workers/empty.js"));
  const GURL resource_url(
      embedded_test_server()->GetURL(cross_site_domain, "/workers/empty.html"));

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
}

}  // namespace content
