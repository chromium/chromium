// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "cc/base/features.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"

namespace content {
class LargestContentfulPaintTestBrowserTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<bool> {
 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
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
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableBlinkFeatures, "ExposeRenderTimeNonTaoDelayedImage");
  }

  EvalJsResult GetLCPStartTime() const {
    std::string script = R"(
      getLCPStartTime();
    )";
    return EvalJs(shell(), script);
  }

  EvalJsResult GetFCPStartTime() const {
    std::string script = R"(
      getFCPStartTime();
    )";
    return EvalJs(shell(), script);
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(LargestContentfulPaintTestBrowserTest,
                       NonTAOImageLCPRenderTime) {
  const GURL url1(embedded_test_server()->GetURL(
      "a.com", "/performance_timeline/cross-origin-non-tao-image.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));

  double lcpStartTime = GetLCPStartTime().ExtractDouble();
  double fcpStartTime = GetFCPStartTime().ExtractDouble();

  EXPECT_NEAR(lcpStartTime, fcpStartTime, 0.01);
}

}  // namespace content
