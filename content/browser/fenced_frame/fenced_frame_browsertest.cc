// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/fenced_frame/fenced_frame.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame.mojom-test-utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom.h"
#include "url/gurl.h"

namespace content {

class FencedFrameBrowserTest : public ContentBrowserTest {
 protected:
  FencedFrameBrowserTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFencedFrames, {{"implementation_type", "mparch"}});
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ContentBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the renderer can create a <fencedframe> that results in a
// browser-side content::FencedFrame also being created.
IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest, CreateFencedFrame) {
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                         "fencedframe.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* primary_rfh = web_contents_impl->GetMainFrame();

  EXPECT_TRUE(ExecJs(
      primary_rfh,
      "document.body.appendChild(document.createElement('fencedframe'));"));
  EXPECT_EQ(1U, primary_rfh->GetFencedFrames().size());
}

}  // namespace content
