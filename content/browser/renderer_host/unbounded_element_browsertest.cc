// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace content {

class UnboundedElementBrowserTest : public ContentBrowserTest {
 public:
  UnboundedElementBrowserTest() = default;
  ~UnboundedElementBrowserTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* primary_main_frame_host() {
    return web_contents()->GetPrimaryFrameTree().root()->current_frame_host();
  }
};

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest,
                       DISABLED_ActivationPreconditions) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Create an unbounded element via HTML snippet:
  std::string script = R"(
    document.body.innerHTML = '<div id="target" unbounded></div>';
    document.getElementById('target').showUnboundedElement().catch(e => e.name);
  )";
  // showUnboundedElement throws DOMException NotAllowedError without transient
  // user gesture.
  EXPECT_EQ("NotAllowedError", EvalJs(primary_main_frame_host(), script));
}

}  // namespace content
