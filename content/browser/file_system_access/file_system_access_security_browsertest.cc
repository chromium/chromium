// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class FileSystemAccessSecurityBrowserTest : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  bool NavigateToURLWithPdf(WebContents* web_contents, const GURL& url) {
    NavigationController::LoadURLParams params(url);
    params.transition_type = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
    params.is_pdf = true;
    NavigateToURLBlockUntilNavigationsComplete(
        web_contents, params, 1,
        /*ignore_uncommitted_navigations=*/false);
    return IsLastCommittedEntryOfPageType(web_contents, PAGE_TYPE_NORMAL) &&
           web_contents->GetLastCommittedURL() == url;
  }
};

// Verifies that a PDF-isolated process cannot access the Origin-Private File
// System.
IN_PROC_BROWSER_TEST_F(FileSystemAccessSecurityBrowserTest, OPFSBlockedForPdf) {
  GURL url = embedded_test_server()->GetURL("localhost", "/empty.html");
  ASSERT_TRUE(NavigateToURLWithPdf(shell()->web_contents(), url));

  RenderFrameHost* rfh = shell()->web_contents()->GetPrimaryMainFrame();

  // Try to access OPFS. We expect this to fail.
  auto result = EvalJs(rfh, "navigator.storage.getDirectory()");

  // This assertion should FAIL before the fix, and PASS after.
  ASSERT_FALSE(result.is_ok());
  EXPECT_THAT(result.ExtractError(), testing::HasSubstr("SecurityError"));
}

}  // namespace content
