// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <cstdint>
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"

namespace content {
class PerformanceTimelineLCPStartTimePrecisionBrowserTest
    : public ContentBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ContentBrowserTest::SetUpOnMainThread();
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
  }

  RenderFrameHostImpl* current_frame_host() {
    return web_contents()->GetPrimaryFrameTree().root()->current_frame_host();
  }

  EvalJsResult GetIsEqualToPrecision() const {
    std::string script =
        content::JsReplace("isEqualToPrecision($1);", getPrecision());
    return EvalJs(shell(), script);
  }

  int32_t getPrecision() const { return precision_; }

 private:
  int32_t precision_ = 10;
};

IN_PROC_BROWSER_TEST_F(PerformanceTimelineLCPStartTimePrecisionBrowserTest,
                       LCPStartTimePrecision) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url1(embedded_test_server()->GetURL(
      "a.com", "/performance_timeline/lcp-start-time-precision.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));

  EXPECT_TRUE(GetIsEqualToPrecision().ExtractBool());
}
}  // namespace content
