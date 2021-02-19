// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace content {

class ContentSecurityPolicyBrowserTest : public ContentBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&ContentSecurityPolicyBrowserTest::HandleResponse,
                            base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  void SetCSP(const std::string& csp) { csp_ = csp; }
  void SetCSPReportOnly(const std::string& csp) { csp_report_only_ = csp; }
  void ResetHeaders() {
    csp_ = base::nullopt;
    csp_report_only_ = base::nullopt;
  }

  void SetContent(const std::string& content) { content_ = content; }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleResponse(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url == "/load_with_csp") {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html");

      if (csp_) {
        response->AddCustomHeader("Content-Security-Policy", *csp_);
      }

      if (csp_report_only_) {
        response->AddCustomHeader("Content-Security-Policy-Report-Only",
                                  *csp_report_only_);
      }

      if (content_) {
        response->set_content(content_.value());
      }
      return std::move(response);
    }

    // If we return nullptr, then the server will go ahead and actually serve
    // the file.
    return nullptr;
  }

  base::Optional<std::string> csp_;
  base::Optional<std::string> csp_report_only_;
  base::Optional<std::string> content_;
};

// Test when Content Security Policy allows web assembly to be loaded and
// compiled.
// Note: The keyword 'wasm-eval' is currently only supported for extensions, and
// should be ignored on normal schemes.
IN_PROC_BROWSER_TEST_F(ContentSecurityPolicyBrowserTest, WasmEval) {
  GURL url = embedded_test_server()->GetURL("a.com", "/load_with_csp");

  struct {
    const char* csp;
    bool expect_allowed;
  } test_cases[]{
      {"script-src http://a.com:* 'unsafe-inline'", false},
      {"script-src http://a.com:* 'unsafe-inline' 'wasm-eval'", false},
      {"script-src http://a.com:* 'unsafe-inline' 'unsafe-eval'", true},
      {"script-src http://a.com:* 'unsafe-inline' 'unsafe-eval' 'wasm-eval'",
       true},
  };

  SetContent(R"(
    <html>
      <script>
        fetch('incrementer.wasm')
            .then(response => {
              if (!response.ok) throw new Error(response.statusText);
              return response.arrayBuffer();
            })
            .then(WebAssembly.compile)
            .then(() => console.log("wasm allowed"),
                  () => console.log("wasm blocked"));
      </script>
    </html>
  )");

  for (const auto& test : test_cases) {
    WebContentsConsoleObserver console_observer(web_contents());
    console_observer.SetPattern(test.expect_allowed ? "wasm allowed"
                                                    : "wasm blocked");

    SetCSP(test.csp);
    EXPECT_TRUE(NavigateToURL(shell(), url));

    console_observer.Wait();
  }

  // Now test that the console error message for a Content Security Policy
  // violation triggered by web assembly compilation does not mention the
  // keyword 'wasm-eval' (which is currently only supported for extensions).
  // This is a regression test for https://crbug.com/1169592.
  ResetHeaders();
  SetCSPReportOnly("script-src http://a.com:* 'unsafe-inline'");

  WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern("wasm allowed");
  WebContentsConsoleObserver console_observer_2(web_contents());
  console_observer_2.SetPattern(
      "[Report Only] Refused to compile or instantiate WebAssembly module "
      "because 'unsafe-eval' is not an allowed source of script in the "
      "following Content Security Policy directive: \"script-src "
      "http://a.com:* 'unsafe-inline'\".\n");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  console_observer.Wait();
  console_observer_2.Wait();
}

}  // namespace content
