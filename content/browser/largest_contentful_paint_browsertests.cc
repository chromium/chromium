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
#include "content/public/test/browser_test_utils.h"
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
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "ExposeRenderTimeNonTaoDelayedImage");
  }

  EvalJsResult GetStartTime(std::string type) const {
    std::string script = content::JsReplace("getStartTime($1);", type);
    return EvalJs(shell(), script);
  }

 private:
  base::test::ScopedFeatureList features_;
};

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_NonTAOImageLCPRenderTime DISABLED_NonTAOImageLCPRenderTime
#else
#define MAYBE_NonTAOImageLCPRenderTime NonTAOImageLCPRenderTime
#endif
IN_PROC_BROWSER_TEST_F(LargestContentfulPaintTestBrowserTest,
                       MAYBE_NonTAOImageLCPRenderTime) {
  const GURL url1(embedded_test_server()->GetURL(
      "a.com", "/performance_timeline/cross-origin-non-tao-image.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));

  double fcpStartTime = GetStartTime("paint").ExtractDouble();

  double lcpStartTime =
      GetStartTime("largest-contentful-paint").ExtractDouble();

  EXPECT_NEAR(lcpStartTime, fcpStartTime, 0.01);
}

}  // namespace content
