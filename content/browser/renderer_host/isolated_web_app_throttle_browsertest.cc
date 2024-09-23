// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/base/net_errors.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

static constexpr WebExposedIsolationLevel kNotIsolated =
    WebExposedIsolationLevel::kNotIsolated;
static constexpr WebExposedIsolationLevel kIsolatedApplication =
    WebExposedIsolationLevel::kIsolatedApplication;

const char kAppHost[] = "app.com";
const char kNonAppHost[] = "other.com";

class IsolatedWebAppContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  explicit IsolatedWebAppContentBrowserClient(
      net::EmbeddedTestServer* embedded_https_server) {
    GURL app_url = embedded_https_server->GetURL(kAppHost, "/");
    app_origin_ = url::Origin::Create(app_url);
  }

  bool ShouldUrlUseApplicationIsolationLevel(BrowserContext* browser_context,
                                             const GURL& url) override {
    return url.host() == kAppHost;
  }

  bool AreIsolatedWebAppsEnabled(BrowserContext*) override { return true; }

 private:
  url::Origin app_origin_;
};

}  // namespace

class HttpsBrowserTest : public ContentBrowserTest {
 public:
  HttpsBrowserTest() : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    ContentBrowserTest::SetUpInProcessBrowserTestFixture();
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    ContentBrowserTest::TearDownInProcessBrowserTestFixture();
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    https_server()->ServeFilesFromSourceDirectory(GetTestDataFilePath());
    ASSERT_TRUE(https_server()->Start());
  }

 protected:
  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  net::EmbeddedTestServer https_server_;
  ContentMockCertVerifier mock_cert_verifier_;
};

class IsolatedWebAppThrottleBrowserTest : public HttpsBrowserTest {
 public:
  void SetUpOnMainThread() override {
    HttpsBrowserTest::SetUpOnMainThread();

    test_client_ =
        std::make_unique<IsolatedWebAppContentBrowserClient>(https_server());
  }

  void TearDownOnMainThread() override {
    HttpsBrowserTest::TearDownOnMainThread();
    test_client_.reset();
  }

 protected:
  GURL GetAppURL(const std::string& path) {
    return https_server()->GetURL(kAppHost, path);
  }

  GURL GetNonAppURL(const std::string& path) {
    return https_server()->GetURL(kNonAppHost, path);
  }

  RenderFrameHost* CreateChildIframe(RenderFrameHost* parent_rfh,
                                     const GURL& iframe_src) {
    // For now assume this is the only child iframe.
    EXPECT_FALSE(ChildFrameAt(parent_rfh, 0));

    TestNavigationObserver navigation_observer(web_contents());
    EXPECT_TRUE(ExecJs(
        parent_rfh, JsReplace("const iframe = document.createElement('iframe');"
                              "iframe.id = 'child_iframe';"
                              "iframe.src = $1;"
                              "document.body.appendChild(iframe);",
                              iframe_src)));
    navigation_observer.Wait();
    EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
    EXPECT_EQ(net::OK, navigation_observer.last_net_error_code());
    EXPECT_EQ(iframe_src, navigation_observer.last_navigation_url());

    RenderFrameHost* iframe = ChildFrameAt(parent_rfh, 0);
    EXPECT_TRUE(iframe);
    return iframe;
  }

  // Perform a renderer-initiated navigation in |iframe| to |url| whose
  // initiator is the iframe itself.
  std::unique_ptr<TestNavigationObserver> SelfNavigateIframeToURL(
      RenderFrameHost* iframe,
      const GURL& url) {
    auto navigation_observer =
        std::make_unique<TestNavigationObserver>(web_contents());
    EXPECT_TRUE(ExecJs(iframe, JsReplace("location.href = $1", url)));
    navigation_observer->Wait();
    return navigation_observer;
  }

  // Perform a renderer-initiated navigation of |iframe| to |url|, whose
  // initiator is |iframe|'s parent frame.
  std::unique_ptr<TestNavigationObserver> NavigateIframeToUrlFromParent(
      RenderFrameHost* iframe,
      const GURL& url) {
    auto navigation_observer =
        std::make_unique<TestNavigationObserver>(web_contents());
    EXPECT_TRUE(
        ExecJs(iframe->GetParent(), JsReplace("child_iframe.src = $1", url)));
    navigation_observer->Wait();
    return navigation_observer;
  }

  RenderFrameHost* main_rfh() { return web_contents()->GetPrimaryMainFrame(); }

  WebContents* web_contents() { return shell()->web_contents(); }

 private:
  std::unique_ptr<IsolatedWebAppContentBrowserClient> test_client_;
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppThrottleBrowserTest,
                       BlockMainFrameNavigationIntoApp) {
  EXPECT_TRUE(NavigateToURL(web_contents(), GetNonAppURL("/simple_page.html")));
  EXPECT_EQ(kNotIsolated, main_rfh()->GetWebExposedIsolationLevel());

  TestNavigationObserver navigation_observer(web_contents());
  shell()->LoadURL(GetAppURL("/cross-origin-isolated.html"));
  navigation_observer.Wait();
  EXPECT_FALSE(navigation_observer.last_navigation_succeeded());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT,
            navigation_observer.last_net_error_code());
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppThrottleBrowserTest,
                       CancelCrossOriginNavigationInApp) {
  GURL app_url = GetAppURL("/cross-origin-isolated.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), app_url));
  EXPECT_EQ(kIsolatedApplication, main_rfh()->GetWebExposedIsolationLevel());

  TestNavigationObserver navigation_observer(web_contents());
  shell()->LoadURL(GetNonAppURL("/simple_page.html"));
  navigation_observer.Wait();
  EXPECT_FALSE(navigation_observer.last_navigation_succeeded());
  EXPECT_EQ(app_url, main_rfh()->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppThrottleBrowserTest,
                       IframeInitiatedIframeNavigationIntoAppBlocked) {
  GURL app_url = GetAppURL("/cross-origin-isolated.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), app_url));
  EXPECT_EQ(kIsolatedApplication, main_rfh()->GetWebExposedIsolationLevel());

  RenderFrameHost* iframe =
      CreateChildIframe(main_rfh(), GetNonAppURL("/corp-cross-origin.html"));
  const blink::LocalFrameToken iframe_token = iframe->GetFrameToken();

  std::unique_ptr<TestNavigationObserver> navigation_observer =
      SelfNavigateIframeToURL(iframe, app_url);
  EXPECT_EQ(iframe_token,
            navigation_observer->last_initiator_frame_token().value());
  EXPECT_FALSE(navigation_observer->last_navigation_succeeded());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT,
            navigation_observer->last_net_error_code());
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppThrottleBrowserTest,
                       AppInitiatedIframeNavigationIntoAppAllowed) {
  GURL app_url = GetAppURL("/cross-origin-isolated.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), app_url));
  EXPECT_EQ(kIsolatedApplication, main_rfh()->GetWebExposedIsolationLevel());

  RenderFrameHost* iframe =
      CreateChildIframe(main_rfh(), GetNonAppURL("/corp-cross-origin.html"));

  std::unique_ptr<TestNavigationObserver> navigation_observer =
      NavigateIframeToUrlFromParent(iframe, app_url);
  EXPECT_EQ(main_rfh()->GetFrameToken(),
            navigation_observer->last_initiator_frame_token().value());
  EXPECT_TRUE(navigation_observer->last_navigation_succeeded());
  EXPECT_EQ(net::OK, navigation_observer->last_net_error_code());
}

}  // namespace content
