// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class NavigationObserver: public WebContentsObserver {
 public:
  explicit NavigationObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}
  ~NavigationObserver() override {}

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    if (navigation_handle->HasCommitted())
      navigation_url_ = navigation_handle->GetURL();
  }

  void DidRedirectNavigation(NavigationHandle* handle) override {
    redirect_url_ = handle->GetURL();
  }

  const GURL& navigation_url() const {
    return navigation_url_;
  }

  const GURL& redirect_url() const {
    return redirect_url_;
  }

 private:
  GURL redirect_url_;
  GURL navigation_url_;

  DISALLOW_COPY_AND_ASSIGN(NavigationObserver);
};

class CrossSiteRedirectorBrowserTest : public ContentBrowserTest {
 public:
  CrossSiteRedirectorBrowserTest() {}

  void SetUpOnMainThread() override {
    // Map all hosts to localhost and setup the EmbeddedTestServer for
    // redirects.
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

IN_PROC_BROWSER_TEST_F(CrossSiteRedirectorBrowserTest,
                       VerifyCrossSiteRedirectURL) {
  SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to http://localhost:<port>/cross-site/foo.com/title2.html and
  // ensure that the redirector forwards the navigation to
  // http://foo.com:<port>/title2.html.  The expectation is that the cross-site
  // redirector will take the hostname supplied in the URL and rewrite the URL.
  GURL expected_url(embedded_test_server()->GetURL("foo.com", "/title2.html"));
  NavigationObserver observer(shell()->web_contents());

  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/cross-site/foo.com/title2.html"),
      expected_url /* expected_commit_url */));

  EXPECT_EQ(expected_url, observer.navigation_url());
  EXPECT_EQ(observer.redirect_url(), observer.navigation_url());
}

using EvalJsBrowserTest = ContentBrowserTest;

// TODO(mslekova): Re-enable once test expectations are updated,
// see chromium:916975
IN_PROC_BROWSER_TEST_F(EvalJsBrowserTest, DISABLED_EvalJsErrors) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));

  {
    // Test syntax errors.
    auto result = EvalJs(shell(), "}}");
    EXPECT_FALSE(true == result);
    EXPECT_FALSE(false == result);  // EXPECT_FALSE(EvalJs()) shouldn't compile.
    EXPECT_FALSE(0 == result);
    EXPECT_FALSE(1 == result);
    EXPECT_FALSE("}}" == result);  // EXPECT_EQ should fail
    EXPECT_FALSE("}}" != result);  // As should EXPECT_NE
    EXPECT_FALSE(nullptr == result);

    std::string expected_error = R"(a JavaScript error:
SyntaxError: Unexpected token }
    at eval (<anonymous>)
    at Promise.resolve.then.script (EvalJs-runner.js:2:34)
)";
    EXPECT_FALSE(expected_error == result);
    EXPECT_EQ(expected_error, result.error);
  }

  {
    // Test throwing exceptions.
    auto result = EvalJs(shell(), "55; throw new Error('whoops');");
    EXPECT_FALSE(55 == result);
    EXPECT_FALSE(1 == result);
    EXPECT_FALSE("whoops" == result);

    std::string expected_error = R"(a JavaScript error:
Error: whoops
    at eval (__const_std::string&_script__:1:11):
        55; throw new Error('whoops');
                  ^^^^^
    at eval (<anonymous>)
    at Promise.resolve.then.script (EvalJs-runner.js:2:34)
)";
    EXPECT_FALSE(expected_error == result);
    EXPECT_EQ(expected_error, result.error);
  }

  {
    // Test reference errors in a multi-line script.
    auto result = EvalJs(shell(), R"(
    22;
    var x = 200 + 300;
    var y = z + x;
    'sweet';)");
    EXPECT_FALSE(22 == result);
    EXPECT_FALSE("sweet" == result);

    std::string expected_error = R"(a JavaScript error:
ReferenceError: z is not defined
    at eval (__const_std::string&_script__:4:13):
            var y = z + x;
                    ^^^^^
    at eval (<anonymous>)
    at Promise.resolve.then.script (EvalJs-runner.js:2:34)
)";
    EXPECT_FALSE(expected_error == result);
    EXPECT_EQ(expected_error, result.error);
  }
}

IN_PROC_BROWSER_TEST_F(EvalJsBrowserTest, EvalJsWithManualReply) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));

  std::string script = "window.domAutomationController.send(20); 'hi';";

  // Calling domAutomationController is required for EvalJsWithManualReply.
  EXPECT_EQ(20, EvalJsWithManualReply(shell(), script));

  // Calling domAutomationController is an error with EvalJs.
  auto result = EvalJs(shell(), script);
  EXPECT_FALSE(20 == result);
  EXPECT_FALSE("hi" == result);
  EXPECT_THAT(result.error,
              ::testing::StartsWith(
                  "Internal Error: expected a 2-element list of the form "));
  EXPECT_THAT(
      result.error,
      ::testing::EndsWith("This is potentially because a script tried to call "
                          "domAutomationController.send itself -- that is only "
                          "allowed when using EvalJsWithManualReply().  When "
                          "using EvalJs(), result values are just the result "
                          "of calling eval() on the script -- the completion "
                          "value is the value of the last executed statement.  "
                          "When using ExecJs(), there is no result value."));
}

}  // namespace content
