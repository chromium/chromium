// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

class ContentSecurityPolicyBrowserTest : public ContentBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* current_frame_host() {
    return web_contents()->GetMainFrame();
  }
};

// Test that the console error message for a Content Security Policy violation
// triggered by web assembly compilation does not mention the keyword
// 'wasm-eval' (which is currently only supported for extensions).  This is a
// regression test for https://crbug.com/1169592.
IN_PROC_BROWSER_TEST_F(ContentSecurityPolicyBrowserTest,
                       WasmEvalBlockedConsoleMessage) {
  GURL url = embedded_test_server()->GetURL("/csp_wasm_eval.html");

  WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(
      "[Report Only] Refused to compile or instantiate WebAssembly module "
      "because 'unsafe-eval' is not an allowed source of script in the "
      "following Content Security Policy directive: \"script-src "
      "'unsafe-inline'\".\n");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  console_observer.Wait();
}

// Test that creating a duplicate Trusted Types policy will yield a console
// message containing "already exists".
//
// This & the following test together ensure that different error causes get
// appropriate messages.
//
// Note: The bulk of Trusted Types related tests are found in the WPT suite
// under trusted-types/*. These two are here, because they need to access
// console messages.
IN_PROC_BROWSER_TEST_F(ContentSecurityPolicyBrowserTest,
                       TrustedTypesCreatePolicyDupeMessage) {
  const char* page = R"(
      data:text/html,
      <meta http-equiv="Content-Security-Policy"
            content="require-trusted-types-for 'script';trusted-types a;">
      <script>
        trustedTypes.createPolicy("a", {});
        trustedTypes.createPolicy("a", {});
      </script>)";

  GURL url(page);
  WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern("*already exists*");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  console_observer.Wait();
}

// Test that creating a Trusted Types policy with a disallowed name will yield
// a console message indicating a directive has been violated.
IN_PROC_BROWSER_TEST_F(ContentSecurityPolicyBrowserTest,
                       TrustedTypesCreatePolicyForbiddenMessage) {
  const char* page = R"(
      data:text/html,
      <meta http-equiv="Content-Security-Policy"
            content="require-trusted-types-for 'script';trusted-types a;">
      <script>
        trustedTypes.createPolicy("b", {});
      </script>)";

  GURL url(page);
  WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern("*violates*the following*directive*");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  console_observer.Wait();
}

namespace {

base::FilePath TestFilePath(const char* filename) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  return GetTestFilePath("", filename);
}

}  // namespace

// We test that we correctly match the file: scheme against file: URLs.
// Unfortunately, we cannot write this as Web Platform Test since Web Platform
// Tests don't support file: urls.
IN_PROC_BROWSER_TEST_F(ContentSecurityPolicyBrowserTest, FileURLs) {
  url::Replacements<char> add_localhost;
  add_localhost.SetHost("localhost", url::Component(0, 9));
  url::Replacements<char> none;
  struct {
    const char* csp;
    std::string element_name;
    const url::Replacements<char>& document_host;
    const url::Replacements<char>& element_host;
    bool expect_allowed;
  } testCases[]{
      {"img-src 'none'", "img", none, none, false},
      {"img-src file:", "img", none, none, true},
      {"img-src 'self'", "img", none, none, true},
      {"img-src 'none'", "img", none, add_localhost, false},
      {"img-src file:", "img", none, add_localhost, true},
      {"img-src 'self'", "img", none, add_localhost, true},
      {"img-src 'none'", "img", add_localhost, none, false},
      {"img-src file:", "img", add_localhost, none, true},
      {"img-src 'self'", "img", add_localhost, none, true},
      {"img-src 'none'", "img", add_localhost, add_localhost, false},
      {"img-src file:", "img", add_localhost, add_localhost, true},
      {"img-src 'self'", "img", add_localhost, add_localhost, true},
      {"frame-src 'none'", "iframe", none, none, false},
      {"frame-src file:", "iframe", none, none, true},
      {"frame-src 'self'", "iframe", none, none, true},
      {"frame-src 'none'", "iframe", none, add_localhost, false},
      {"frame-src file:", "iframe", none, add_localhost, true},
      // TODO(antoniosartori): The following one behaves differently than
      // img-src.
      {"frame-src 'self'", "iframe", none, add_localhost, true},
      {"frame-src 'none'", "iframe", add_localhost, none, false},
      {"frame-src file:", "iframe", add_localhost, none, true},
      // TODO(antoniosartori): The following one behaves differently than
      // img-src.
      {"frame-src 'self'", "iframe", add_localhost, none, true},
      {"frame-src 'none'", "iframe", add_localhost, add_localhost, false},
      {"frame-src file:", "iframe", add_localhost, add_localhost, true},
      {"frame-src 'self'", "iframe", add_localhost, add_localhost, true},
  };

  for (const auto& testCase : testCases) {
    GURL document_url = net::FilePathToFileURL(TestFilePath("hello.html"))
                            .ReplaceComponents(testCase.document_host);

    // On windows, if `document_url` contains the host part "localhost", the
    // actual committed URL does not. So we omit EXPECT_TRUE and ignore the
    // result value here.
    ignore_result(NavigateToURL(shell(), document_url));

    GURL element_url = net::FilePathToFileURL(TestFilePath(
        testCase.element_name == "iframe" ? "empty.html" : "blank.jpg"));
    element_url = element_url.ReplaceComponents(testCase.element_host);
    TestNavigationObserver load_observer(shell()->web_contents());

    EXPECT_TRUE(
        ExecJs(current_frame_host(),
               JsReplace(R"(
          var violation = new Promise(resolve => {
            document.addEventListener("securitypolicyviolation", (e) => {
              resolve("got violation");
            });
          });

          let meta = document.createElement('meta');
          meta.httpEquiv = 'Content-Security-Policy';
          meta.content = $1;
          document.head.appendChild(meta);

          let element = document.createElement($2);
          element.src = $3;
          var promise = new Promise(resolve => {
            element.onload = () => { resolve("allowed"); };
            element.onerror = () => { resolve("blocked"); };
          });
          document.body.appendChild(element);
    )",
                         testCase.csp, testCase.element_name, element_url)));

    if (testCase.element_name == "iframe") {
      // Since iframes always trigger the onload event, we need to be more
      // careful checking whether the iframe was blocked or not.
      load_observer.Wait();
      const url::Origin child_origin = current_frame_host()
                                           ->child_at(0)
                                           ->current_frame_host()
                                           ->GetLastCommittedOrigin();
      if (testCase.expect_allowed) {
        EXPECT_TRUE(load_observer.last_navigation_succeeded())
            << element_url << " in " << document_url << " with CSPs \""
            << testCase.csp << "\" should be allowed";
        EXPECT_FALSE(child_origin.opaque());
      } else {
        EXPECT_FALSE(load_observer.last_navigation_succeeded());
        EXPECT_EQ(net::ERR_BLOCKED_BY_CSP, load_observer.last_net_error_code());
        // The blocked frame's origin should become unique.
        EXPECT_TRUE(child_origin.opaque())
            << element_url << " in " << document_url << " with CSPs \""
            << testCase.csp << "\" should be blocked";
      }
    } else {
      std::string expect_message =
          testCase.expect_allowed ? "allowed" : "blocked";
      EXPECT_EQ(expect_message, EvalJs(current_frame_host(), "promise"))
          << element_url << " in " << document_url << " with CSPs \""
          << testCase.csp << "\" should be " << expect_message;
    }

    if (!testCase.expect_allowed) {
      EXPECT_EQ("got violation", EvalJs(current_frame_host(), "violation"));
    }
  }
}

}  // namespace content
