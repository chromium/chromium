// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_run_loop_timeout.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-spi.h"

namespace content {

using ::testing::Eq;

class NavigationObserver: public WebContentsObserver {
 public:
  explicit NavigationObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  NavigationObserver(const NavigationObserver&) = delete;
  NavigationObserver& operator=(const NavigationObserver&) = delete;

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

IN_PROC_BROWSER_TEST_F(EvalJsBrowserTest, EvalJsErrors) {
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

    std::string expected_error =
        "a JavaScript error: \"SyntaxError: Unexpected token '}'\"\n";
    EXPECT_FALSE(expected_error == result);
    EXPECT_EQ(expected_error, result.error);
  }

  {
    // Test throwing exceptions.
    auto result = EvalJs(shell(), "55; throw new Error('whoops');");
    EXPECT_FALSE(55 == result);
    EXPECT_FALSE(1 == result);
    EXPECT_FALSE("whoops" == result);

    std::string expected_error = R"(a JavaScript error: "Error: whoops
    at __const_std::string&_script__:1:12):
        {55; throw new Error('whoops');
                   ^^^^^
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

    std::string expected_error =
        "a JavaScript error: \"ReferenceError: z is not defined\n"
        "    at __const_std::string&_script__:4:13):\n"
        "            var y = z + x;\n"
        "                    ^^^^^\n";
    EXPECT_FALSE(expected_error == result);
    EXPECT_EQ(expected_error, result.error);
  }
}

IN_PROC_BROWSER_TEST_F(EvalJsBrowserTest, EvalJsAfterLifecycleUpdateErrors) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));

  {
    // Test syntax errors.
    auto result = EvalJsAfterLifecycleUpdate(shell(), "}}", "'hi'");

    EXPECT_TRUE(result.value.is_none());
    EXPECT_THAT(
        result.error,
        Eq("a JavaScript error: \"SyntaxError: Unexpected token '}'\n"
           "    at eval (<anonymous>)\n"
           "    at \"__const_std::string&_EvalJsAfterLifecycleUpdate__\""
           ":3:27\"\n"));

    auto result2 = EvalJsAfterLifecycleUpdate(shell(), "'hi'", "]]");

    EXPECT_TRUE(result2.value.is_none());
    EXPECT_THAT(
        result2.error,
        Eq("a JavaScript error: \"SyntaxError: Unexpected token ']'\n"
           "    at eval (<anonymous>)\n"
           "    at \"__const_std::string&_EvalJsAfterLifecycleUpdate__\""
           ":5:37\"\n"));
  }

  {
    // Test throwing exceptions.
    auto result = EvalJsAfterLifecycleUpdate(
        shell(), "55; throw new Error('whoops');", "'hi'");

    EXPECT_TRUE(result.value.is_none());
    EXPECT_THAT(
        result.error,
        Eq("a JavaScript error: \"Error: whoops\n"
           "    at eval (__const_std::string&_script__:1:11)\n"
           "    at eval (<anonymous>)\n"
           "    at \"__const_std::string&_EvalJsAfterLifecycleUpdate__\""
           ":3:27\"\n"));

    auto result2 = EvalJsAfterLifecycleUpdate(
        shell(), "'hi'", "55; throw new Error('whoopsie');");

    EXPECT_TRUE(result2.value.is_none());
    EXPECT_THAT(
        result2.error,
        Eq("a JavaScript error: \"Error: whoopsie\n"
           "    at eval (__const_std::string&_script__:1:11)\n"
           "    at eval (<anonymous>)\n"
           "    at \"__const_std::string&_EvalJsAfterLifecycleUpdate__\""
           ":5:37\"\n"));
  }
}

IN_PROC_BROWSER_TEST_F(EvalJsBrowserTest, EvalJsWithDomAutomationController) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));

  std::string script = "window.domAutomationController.send(20); 'hi';";

  // Calling domAutomationController is allowed with EvalJs, but doesn't
  // influence the completion value.
  EvalJsResult result = EvalJs(shell(), script);
  EXPECT_NE(20, result);
  EXPECT_EQ("hi", result);
}

IN_PROC_BROWSER_TEST_F(EvalJsBrowserTest, EvalJsTimeout) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));

  base::test::ScopedRunLoopTimeout scoped_run_timeout(FROM_HERE,
                                                      base::Milliseconds(1));

  // Store the promise resolve function so it doesn't get GC'd.
  static std::string script = "new Promise(resolve => {window.r = resolve})";
  static std::string error;
  static Shell* shell_ptr = shell();
  EXPECT_NONFATAL_FAILURE(error = EvalJs(shell_ptr, script).error,
                          "RunLoop::Run() timed out.");

  EXPECT_THAT(error, Eq("Timeout waiting for Javascript to execute."));
}

IN_PROC_BROWSER_TEST_F(EvalJsBrowserTest, EvalJsNotBlockedByCSP) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "/set-header?Content-Security-Policy: script-src 'self'")));

  auto result = EvalJs(shell(), "'hi'");
  EXPECT_EQ("hi", result);
}

IN_PROC_BROWSER_TEST_F(EvalJsBrowserTest,
                       EvalJsAfterLifecycleUpdateBlockedByCSP) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "/set-header?Content-Security-Policy: script-src 'self'")));

  auto result = EvalJsAfterLifecycleUpdate(shell(), "'hi'", "");
  EXPECT_TRUE(result.value.is_none());
  EXPECT_THAT(
      result.error,
      ::testing::StartsWith(
          "EvalJsAfterLifecycleUpdate encountered an EvalError, because eval() "
          "is blocked by the document's CSP on this page. To test content that "
          "is protected by CSP, consider using EvalJsAfterLifecycleUpdate in "
          "an isolated world. Details:"));
}

IN_PROC_BROWSER_TEST_F(EvalJsBrowserTest, ExecJsWithDomAutomationController) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));

  std::string script = "window.domAutomationController.send(20); 'hi';";

  // Calling domAutomationController is allowed with ExecJs.
  EXPECT_TRUE(ExecJs(shell(), script));
}

}  // namespace content
