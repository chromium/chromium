// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"

namespace content {
class LargestContentfulPaintTestBrowserTest : public ContentBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
    ContentBrowserTest::SetUpOnMainThread();
  }

  EvalJsResult GetStartTime(std::string type) const {
    std::string script = content::JsReplace("getStartTime($1);", type);
    return EvalJs(shell(), script);
  }
};

IN_PROC_BROWSER_TEST_F(LargestContentfulPaintTestBrowserTest,
                       NonTAOImageLCPRenderTime) {
  const GURL url1(embedded_test_server()->GetURL(
      "a.com", "/performance_timeline/cross-origin-non-tao-image.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));

  double fcpStartTime = GetStartTime("paint").ExtractDouble();

  double lcpStartTime =
      GetStartTime("largest-contentful-paint").ExtractDouble();

  EXPECT_NEAR(lcpStartTime, fcpStartTime, 0.01);
}

}  // namespace content
