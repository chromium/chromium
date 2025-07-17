// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_base.h"
#include "net/dns/mock_host_resolver.h"

namespace content {

class ChromeInternalUrlsBrowserTest
    : public ContentBrowserTestBase,
      public testing::WithParamInterface<std::string> {};

// Monitors navigations for the `WebContents` and asserts that they include a
// Content-Security-Policy header.
class AssertNavigationHasCspHeader : WebContentsObserver {
 public:
  explicit AssertNavigationHasCspHeader(WebContents* wc)
      : WebContentsObserver(wc) {}
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override {
    ASSERT_TRUE(navigation_handle->GetResponseHeaders()->HasHeader(
        "Content-Security-Policy"));
  }
};

// Tests that the chrome:// URL has a Content-Security-Policy header and that
// no messages are logged about violations. This tests that there are no CSP
// violations by looking at the logged console messages.
IN_PROC_BROWSER_TEST_P(ChromeInternalUrlsBrowserTest, NoCspMessages) {
  GURL url = GetWebUIURL(GetParam());
  WebContentsConsoleObserver console_observer(web_contents());

  // This will monitor all navigations that occur.
  AssertNavigationHasCspHeader asserter(web_contents());

  ASSERT_TRUE(NavigateToURL(shell(), url));

  // Log to the console. We look at all messages logged *before* this. That will
  // include any messages logged during page load.
  const std::string kSentinel = "hello from NoCspMessages";
  ASSERT_TRUE(ExecJs(shell(), JsReplace("console.log($1)", kSentinel)));
  for (int i = 0;; i++) {
    ASSERT_TRUE(console_observer.Wait());
    std::string message = console_observer.GetMessageAt(i);
    if (message == kSentinel) {
      break;
    }
    // Ensure that the message doesn't look like  CSP violation.
    EXPECT_THAT(message, Not(testing::HasSubstr("Content Security Policy")));
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         ChromeInternalUrlsBrowserTest,
                         testing::Values(kChromeUIBlobInternalsHost));

}  // namespace content
