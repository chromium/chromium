// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "components/web_package/web_bundle_builder.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

// Tests for <script type=webbundle>.
class ScriptWebBundleBrowserTest : public ContentBrowserTest {
 protected:
  ScriptWebBundleBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kSubresourceWebBundles);
  }
  ~ScriptWebBundleBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    original_client_ = SetBrowserClientForTesting(&browser_client_);
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.RegisterRequestHandler(base::BindRepeating(
        &ScriptWebBundleBrowserTest::HandleTestWebBundleRequest,
        base::Unretained(this)));
    https_server_.RegisterRequestMonitor(
        base::BindRepeating(&ScriptWebBundleBrowserTest::MonitorResourceRequest,
                            base::Unretained(this)));
    https_server_.AddDefaultHandlers(GetTestDataFilePath());
    ASSERT_TRUE(https_server_.Start());
  }

  void TearDownOnMainThread() override {
    ContentBrowserTest::TearDownOnMainThread();
    SetBrowserClientForTesting(original_client_);
  }

  void SetUpInProcessBrowserTestFixture() override {
    ContentBrowserTest::SetUpInProcessBrowserTestFixture();
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    ContentBrowserTest::TearDownInProcessBrowserTestFixture();
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleTestWebBundleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url != "/web_bundle/test.wbn")
      return nullptr;
    GURL test1_url(https_server_.GetURL("/web_bundle/test1.txt"));
    GURL test2_url(https_server_.GetURL("/web_bundle/test2.txt"));
    web_package::WebBundleBuilder builder("" /* fallback_url */,
                                          "" /* manifest_url */);
    builder.AddExchange(test1_url.spec(),
                        {{":status", "200"}, {"content-type", "text/plain"}},
                        "test1");
    builder.AddExchange(test2_url.spec(),
                        {{":status", "200"}, {"content-type", "text/plain"}},
                        "test2");
    auto bundle = builder.CreateBundle();
    std::string body(reinterpret_cast<const char*>(bundle.data()),
                     bundle.size());
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->set_content(body);
    http_response->set_content_type("application/webbundle");
    http_response->AddCustomHeader("X-Content-Type-Options", "nosniff");
    return http_response;
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  void MonitorResourceRequest(const net::test_server::HttpRequest& request) {
    // This should be called on `EmbeddedTestServer::io_thread_`.
    EXPECT_FALSE(
        content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    base::AutoLock auto_lock(lock_);
    request_count_by_path_[request.GetURL().PathForRequest()]++;
  }

  int GetRequestCount(const GURL& url) {
    EXPECT_TRUE(content::BrowserThread::CurrentlyOn(BrowserThread::UI));
    base::AutoLock auto_lock(lock_);
    return request_count_by_path_[url.PathForRequest()];
  }

 private:
  content::ContentMockCertVerifier mock_cert_verifier_;
  ContentBrowserClient* original_client_ = nullptr;
  ContentBrowserClient browser_client_;
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_{
      net::EmbeddedTestServer::Type::TYPE_HTTPS};
  // Counts of requests sent to the server. Keyed by path (not by full URL)
  std::map<std::string, int> request_count_by_path_ GUARDED_BY(lock_);
  base::Lock lock_;
};

IN_PROC_BROWSER_TEST_F(ScriptWebBundleBrowserTest,
                       WebBundleResourceShouldBeReused) {
  // The tentative spec:
  // https://docs.google.com/document/d/1GEJ3wTERGEeTG_4J0QtAwaNXhPTza0tedd00A7vPVsw/edit

  // Tests that webbundle resources are surely re-used when we remove a <script
  // type=webbunble> and add a new <script type=webbundle> with the same bundle
  // URL to the removed one, in the same microtask scope.

  GURL url(https_server()->GetURL("/web_bundle/empty.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  {
    // Add a <script type=webbundle>.
    DOMMessageQueue dom_message_queue(shell()->web_contents());
    ExecuteScriptAsync(shell(),
                       R"HTML(
        const script = document.createElement("script");
        script.type = "webbundle";
        script.textContent =
              JSON.stringify({"source": "/web_bundle/test.wbn",
                              "resources": ["/web_bundle/test1.txt"]});
        document.body.appendChild(script);
        (async () => {
          const response = await fetch("/web_bundle/test1.txt");
          const text = await response.text();
          window.domAutomationController.send(`fetch: ${text}`);
        })();

      )HTML");
    std::string message;
    EXPECT_TRUE(dom_message_queue.WaitForMessage(&message));
    EXPECT_EQ(message, "\"fetch: test1\"");
    EXPECT_EQ(GetRequestCount(https_server()->GetURL("/web_bundle/test.wbn")),
              1);
  }
  {
    // Remove the <script type=webbundle> from the document, and then add a new
    // <script type=webbundle> whose bundle URL is same to the removed one in
    // the same microtask scope, The added element should re-use the webbundle
    // resource which the old <script type=webbundle> has been using. Thus, the
    // bundle shouldn't be fetched twice.
    DOMMessageQueue dom_message_queue(shell()->web_contents());
    ExecuteScriptAsync(shell(),
                       R"HTML(
        script.remove();

        const script2 = document.createElement("script");
        script2.type = "webbundle";
        script2.textContent =
              JSON.stringify({"source": "/web_bundle/test.wbn",
                              "resources": ["/web_bundle/test2.txt"]});
        document.body.appendChild(script2);

        (async () => {
          const response = await fetch("/web_bundle/test2.txt");
          const text = await response.text();
          window.domAutomationController.send(`fetch: ${text}`);
        })();
      )HTML");
    std::string message;
    EXPECT_TRUE(dom_message_queue.WaitForMessage(&message));
    EXPECT_EQ(message, "\"fetch: test2\"")
        << "A new script element's rule should be effective.";
    EXPECT_EQ(GetRequestCount(https_server()->GetURL("/web_bundle/test.wbn")),
              1)
        << "A bundle should not be fetched twice.";
  }
}

}  // namespace content
