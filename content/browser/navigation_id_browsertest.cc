// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/logging.h"
#include "content/browser/back_forward_cache_browsertest.h"

#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"

namespace content {

class NavigationIdBrowserTest : public ContentBrowserTest {
 public:
  NavigationIdBrowserTest() = default;

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ContentBrowserTest::SetUpOnMainThread();
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* current_frame_host() {
    return web_contents()->GetPrimaryFrameTree().root()->current_frame_host();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        "--enable-blink-test-features");
  }

  [[nodiscard]] EvalJsResult GetNavigationId(const std::string& name) {
    const char kGetPerformanceEntryTemplate[] = R"(
        (() => {performance.mark($1);
        return performance.getEntriesByName($1)[0].navigationId;})();
    )";
    std::string script = content::JsReplace(kGetPerformanceEntryTemplate, name);
    return EvalJs(shell(), script);
  }
};

// This test case is to verify PerformanceEntry.navigationId gets incremented
// for each back/forward cache restore.
IN_PROC_BROWSER_TEST_F(NavigationIdBrowserTest, BackForwardCacheRestore) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  const GURL url2(embedded_test_server()->GetURL("b.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));

  EXPECT_EQ(1, GetNavigationId("first_nav"));
  // Navigate away and back 3 times. The 1st time is to verify the
  // navigation id is incremented. The 2nd time is to verify that the id is
  // incremented on the same restored document. The 3rd time is to
  // verify the increment does not stop at 2.
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  for (int i = 1; i <= 3; i++) {
    // Navigate away
    ASSERT_TRUE(NavigateToURL(shell(), url2));

    // Verify `rfh_a` is stored in back/forward cache in case back/forward cache
    // feature is enabled.
    if (IsBackForwardCacheEnabled())
      ASSERT_TRUE(rfh_a->IsInBackForwardCache());
    else {
      // Verify `rfh_a` is deleted in case back/forward cache feature is
      // disabled.
      ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());
    }

    // Navigate back.
    ASSERT_TRUE(HistoryGoBack(web_contents()));

    // Verify navigation id is incremented each time in case back/forward
    // cache feature is enabled. Verify navigation id is always 0 in case
    // back/forward cache feature is not enabled.
    EXPECT_EQ(IsBackForwardCacheEnabled() ? i + 1 : 1,
              GetNavigationId("subsequent_nav" + base::NumberToString(i)));
  }
}

// This test case is to verify the navigation id of a frame does not increment
// if the page load is not a back/forward cache restore, even with the
// back/forward cache feature enabled.
IN_PROC_BROWSER_TEST_F(NavigationIdBrowserTest, NonBackForwardCacheRestore) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  const GURL url2(embedded_test_server()->GetURL("b.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));

  EXPECT_EQ(1, GetNavigationId("first_nav"));

  // Make `rfh_a`ineligible for back/forward cache so that the subsequent page
  // load is not a back/forward restore.
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  DisableBFCacheForRFHForTesting(rfh_a.get());

  // Navigate away.
  ASSERT_TRUE(NavigateToURL(shell(), url2));

  // Verify `rfh_a` is not in the back/forward cache.
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());

  // Navigate back.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  // Verify navigation id is not incremented.
  EXPECT_EQ(1, GetNavigationId("subsequent_nav"));
}

}  // namespace content