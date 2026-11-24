// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace content {

class CrossOriginEmbedderPolicyReportingBrowserTest
    : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_->AddDefaultHandlers(GetTestDataFilePath());
    https_server_->RegisterRequestHandler(base::BindRepeating(
        &CrossOriginEmbedderPolicyReportingBrowserTest::HandleRequest,
        base::Unretained(this)));
    ASSERT_TRUE(https_server_->Start());
  }

 protected:
  WebContents* contents() { return shell()->web_contents(); }
  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  void InstallCoepObserver(RenderFrameHost* rfh) {
    ASSERT_TRUE(ExecJs(rfh, R"(
      window.coepReports = [];
      new ReportingObserver(reports => {
        for (const r of reports) {
          if (r.type === 'coep') {
            const {type, blockedURL, disposition} = r.toJSON().body;
            window.coepReports.push({type, blockedURL, disposition});
          }
        }
      }).observe();
      window.takeReports = async function(count) {
        while (window.coepReports.length < count) {
          await new Promise(r => setTimeout(r, 10));
        }
        return window.coepReports.splice(0);
      };
    )"));
  }

  void RunAndWait(RenderFrameHost* rfh, std::string_view script) {
    TestNavigationObserver nav_observer(contents());
    ASSERT_TRUE(ExecJs(rfh, script));
    nav_observer.Wait();
  }

  void CreateChildIframe(RenderFrameHost* rfh,
                         std::string_view id,
                         const GURL& url) {
    RunAndWait(rfh,
               JsReplace("const f = document.createElement('iframe'); "
                         "f.id = $1; f.src = $2; document.body.appendChild(f);",
                         id, url));
  }

  void ExpectCoepReports(RenderFrameHost* rfh,
                         const std::set<std::string>& expected_types,
                         std::string_view expected_disposition,
                         std::string_view expected_blocked_url) {
    base::Value reports =
        EvalJs(rfh, JsReplace("takeReports($1)",
                              static_cast<int>(expected_types.size())))
            .TakeValue();
    ASSERT_EQ(expected_types.size(), reports.GetList().size());
    std::set<std::string> actual_types;
    for (const base::Value& entry : reports.GetList()) {
      const base::DictValue& dict = entry.GetDict();
      const std::string* type = dict.FindString("type");
      ASSERT_TRUE(type);
      actual_types.insert(*type);
      EXPECT_THAT(dict.FindString("disposition"),
                  ::testing::Pointee(std::string(expected_disposition)));
      EXPECT_THAT(dict.FindString("blockedURL"),
                  ::testing::Pointee(std::string(expected_blocked_url)));
    }
    EXPECT_EQ(expected_types, actual_types);
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    const GURL url = request.GetURL();
    if (url.path() != "/coep-redirect" && url.path() != "/coep-child") {
      return nullptr;
    }
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->AddCustomHeader("Cross-Origin-Resource-Policy", "cross-origin");
    if (url.path() == "/coep-redirect") {
      response->set_code(net::HTTP_FOUND);
      response->AddCustomHeader("Location", url.query());
    } else {
      response->set_code(net::HTTP_OK);
      response->AddCustomHeader("Cross-Origin-Embedder-Policy", "require-corp");
      response->set_content_type("text/html");
      response->set_content(base::StrCat(
          {"<!doctype html><a id='nav' href='", url.query(), "'>go</a>"}));
    }
    return response;
  }

  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

// Regression test for b/572310924: COEP reports must not disclose cross-origin
// post-redirect URLs stored in a FrameNavigationEntry on history traversal,
// while preserving full initial URLs for direct parent-initiated navigations.
IN_PROC_BROWSER_TEST_F(CrossOriginEmbedderPolicyReportingBrowserTest,
                       CoepReportSanitizesHistoryRedirectUrl) {
  const GURL blank_url = https_server()->GetURL(
      "a.test",
      "/set-header?Cross-Origin-Embedder-Policy: require-corp&"
      "Cross-Origin-Resource-Policy: cross-origin");
  const GURL parent_ro_url = https_server()->GetURL(
      "a.test",
      "/set-header?Cross-Origin-Embedder-Policy-Report-Only: require-corp");
  const GURL parent_enforce_url = https_server()->GetURL(
      "a.test", "/set-header?Cross-Origin-Embedder-Policy: require-corp");

  struct HistoryCase {
    const char* name;
    GURL parent_url;
    GURL target_url;
    std::set<std::string> types;
    const char* disposition;
  } cases[] = {
      {
          "report-only",
          parent_ro_url,
          https_server()->GetURL(
              "c.test",
              "/set-header?Content-Security-Policy: frame-ancestors 'none'&"
              "code=s3cr3t"),
          {"corp", "navigation"},
          "reporting",
      },
      {
          "enforce-navigation",
          parent_enforce_url,
          https_server()->GetURL(
              "c.test",
              "/set-header?Cross-Origin-Resource-Policy: cross-origin&"
              "code=nav_s3cr3t"),
          {"navigation"},
          "enforce",
      },
      {
          "enforce-corp",
          parent_enforce_url,
          https_server()->GetURL(
              "c.test",
              "/set-header?Cross-Origin-Embedder-Policy: require-corp&"
              "code=corp_s3cr3t"),
          {"corp"},
          "enforce",
      },
  };

  for (const auto& tc : cases) {
    SCOPED_TRACE(tc.name);
    const GURL idp_url = https_server()->GetURL(
        "b.test", "/coep-redirect?" + tc.target_url.spec());
    ASSERT_TRUE(NavigateToURL(shell(), tc.parent_url));
    RenderFrameHost* main_rfh = contents()->GetPrimaryMainFrame();
    InstallCoepObserver(main_rfh);
    CreateChildIframe(main_rfh, "child", idp_url);
    ExpectCoepReports(main_rfh, tc.types, tc.disposition, idp_url.spec());

    RunAndWait(main_rfh, JsReplace("document.getElementById('child').src = $1;",
                                   blank_url));
    RunAndWait(main_rfh, "history.back();");
    ExpectCoepReports(main_rfh, tc.types, tc.disposition,
                      tc.target_url.DeprecatedGetOriginAsURL().spec());
  }
}

// Regression test for b/572310924: navigations initiated by a cross-origin
// child document (link click or client redirect) must only report the origin.
IN_PROC_BROWSER_TEST_F(CrossOriginEmbedderPolicyReportingBrowserTest,
                       CoepReportSanitizesChildInitiatedAndClientRedirectUrls) {
  const GURL parent_ro_url = https_server()->GetURL(
      "a.test",
      "/set-header?Cross-Origin-Embedder-Policy-Report-Only: require-corp");
  const GURL child_dest_url = https_server()->GetURL(
      "c.test",
      "/set-header?Content-Security-Policy: frame-ancestors 'none'&"
      "token=child_link");
  const GURL child_url =
      https_server()->GetURL("b.test", "/coep-child?" + child_dest_url.spec());

  ASSERT_TRUE(NavigateToURL(shell(), parent_ro_url));
  RenderFrameHost* main_rfh = contents()->GetPrimaryMainFrame();
  InstallCoepObserver(main_rfh);
  CreateChildIframe(main_rfh, "child", child_url);

  // Child link click reports only `child_dest_url`'s origin.
  RenderFrameHost* child_rfh = ChildFrameAt(main_rfh, 0);
  ASSERT_TRUE(child_rfh);
  RunAndWait(child_rfh, "document.getElementById('nav').click();");
  ExpectCoepReports(main_rfh, {"corp", "navigation"}, "reporting",
                    child_dest_url.DeprecatedGetOriginAsURL().spec());

  // Child client-side redirect (`location.replace`) reports only
  // `child_dest_url`'s origin.
  RunAndWait(main_rfh, JsReplace("document.getElementById('child').src = $1;",
                                 child_url));
  child_rfh = ChildFrameAt(main_rfh, 0);
  ASSERT_TRUE(child_rfh);
  RunAndWait(child_rfh, JsReplace("location.replace($1);", child_dest_url));
  ExpectCoepReports(main_rfh, {"corp", "navigation"}, "reporting",
                    child_dest_url.DeprecatedGetOriginAsURL().spec());
}

// Verifies that when a same-origin ancestor navigates a nested iframe to
// about:blank, the full blockedURL is preserved (matching WPT
// block-local-documents-inheriting-none).
IN_PROC_BROWSER_TEST_F(CrossOriginEmbedderPolicyReportingBrowserTest,
                       CoepReportPreservesSameOriginAncestorInitiatedLocalUrl) {
  const GURL top_url = https_server()->GetURL("a.test", "/title1.html");
  const GURL middle_url = https_server()->GetURL(
      "a.test", "/set-header?Cross-Origin-Embedder-Policy: require-corp");
  const GURL blank_url = https_server()->GetURL(
      "a.test",
      "/set-header?Cross-Origin-Embedder-Policy: require-corp&"
      "Cross-Origin-Resource-Policy: cross-origin");

  ASSERT_TRUE(NavigateToURL(shell(), top_url));
  RenderFrameHost* main_rfh = contents()->GetPrimaryMainFrame();
  CreateChildIframe(main_rfh, "middle", middle_url);
  RenderFrameHost* middle_rfh = ChildFrameAt(main_rfh, 0);
  ASSERT_TRUE(middle_rfh);
  InstallCoepObserver(middle_rfh);
  CreateChildIframe(middle_rfh, "nested", blank_url);

  RunAndWait(main_rfh,
             "document.getElementById('middle').contentDocument"
             ".getElementById('nested').contentDocument.location = "
             "'about:blank';");
  ExpectCoepReports(middle_rfh, {"navigation"}, "enforce", "about:blank");
}

}  // namespace content
