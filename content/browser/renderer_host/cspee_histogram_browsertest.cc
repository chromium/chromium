// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace content {

class CSPEEHistogramBrowserTest : public ContentBrowserTest {
 public:
  CSPEEHistogramBrowserTest() = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &CSPEEHistogramBrowserTest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TestIframeNavigation(
      const GURL& iframe_url,
      NavigationRequest::NavigationCSPEmbeddedEnforcementOutcome outcome) {
    base::HistogramTester histograms;
    TestNavigationObserver observer(web_contents());
    EXPECT_TRUE(ExecJs(shell(), JsReplace(R"(
      let iframe = document.createElement('iframe');
      iframe.csp = "script-src 'none'";
      iframe.src = $1;
      document.body.appendChild(iframe);
    )",
                                          iframe_url)));
    observer.Wait();
    histograms.ExpectUniqueSample("Navigation.CSPEmbeddedEnforcement.Outcome",
                                  outcome, 1);
  }

 protected:
  WebContentsImpl* web_contents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url == "/allow-csp-from") {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->AddCustomHeader("Allow-CSP-From", "*");
      response->set_content("<html><body>allowed</body></html>");
      return response;
    }
    if (request.relative_url == "/subsumes") {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->AddCustomHeader("Content-Security-Policy", "script-src 'none'");
      response->set_content("<html><body>allowed</body></html>");
      return response;
    }
    return nullptr;
  }
};

IN_PROC_BROWSER_TEST_F(CSPEEHistogramBrowserTest, Outcome) {
  using Outcome = NavigationRequest::NavigationCSPEmbeddedEnforcementOutcome;

  // 1. kAllowLocalScheme (data: URL)
  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
  TestIframeNavigation(GURL("data:text/html,allowed"),
                       Outcome::kAllowLocalScheme);

  // 2. kAllowAllowCSPFromHeader
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.example", "/title1.html")));
  TestIframeNavigation(
      embedded_test_server()->GetURL("b.example", "/allow-csp-from"),
      Outcome::kAllowAllowCSPFromHeader);

  // 3. kAllowSubsumes
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.example", "/title1.html")));
  TestIframeNavigation(embedded_test_server()->GetURL("b.example", "/subsumes"),
                       Outcome::kAllowSubsumes);

  // 4. kBlock
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.example", "/title1.html")));
  TestIframeNavigation(
      embedded_test_server()->GetURL("b.example", "/title1.html"),
      Outcome::kBlock);
}

}  // namespace content
