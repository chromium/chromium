// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"

namespace content {

class PerformanceMemoryBrowserTest : public ContentBrowserTest {
 public:
  PerformanceMemoryBrowserTest() {}

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

// Verify that performance.memory is not bucketized when sites are isolated for
// testing, and that it is bucketized when they are not.
IN_PROC_BROWSER_TEST_F(PerformanceMemoryBrowserTest, PerformanceMemory) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContents* contents = shell()->web_contents();
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(contents)->GetPrimaryFrameTree().root();
  int usedJSHeapSize =
      EvalJs(root, "performance.memory.usedJSHeapSize;").ExtractInt();

  EXPECT_GE(usedJSHeapSize, 0);
  // There is no explicit way to check if the memory values are bucketized or
  // not. As in third_party/blink/renderer/core/timing/memory_info_test.cc,
  // check that the value mod 100000 is non-zero to verify that it is
  // not bucketized. This should be the case when the renderer process is locked
  // to a site (i.e. scheme plus eTLD+1).
  if (AreAllSitesIsolatedForTesting())
    EXPECT_NE(0, usedJSHeapSize % 100000);
  else
    EXPECT_EQ(0, usedJSHeapSize % 100000);
}

}  // namespace content
