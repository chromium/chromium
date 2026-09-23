// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace content {

class FencedFrameStubBrowserTest : public ContentBrowserTest {
 public:
  FencedFrameStubBrowserTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kFencedFrames},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.ServeFilesFromSourceDirectory(GetTestDataFilePath());
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    SetupCrossSiteRedirector(&https_server_);
    net::test_server::RegisterDefaultHandlers(&https_server_);
    ASSERT_TRUE(https_server_.Start());
  }

  RenderFrameHostImpl* primary_main_frame_host() {
    return static_cast<WebContentsImpl*>(shell()->web_contents())
        ->GetPrimaryMainFrame();
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(FencedFrameStubBrowserTest, ElementCreationAndLayout) {
  // Navigate to an empty page.
  const GURL main_url =
      https_server()->GetURL("a.test", "/fenced_frames/empty.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create a fenced frame via Javascript.
  constexpr char kAddFencedFrameScript[] = R"({
    const fenced_frame = document.createElement('fencedframe');
    document.body.appendChild(fenced_frame);
  })";
  EXPECT_TRUE(ExecJs(primary_main_frame_host(), kAddFencedFrameScript));

  // 1. Verify it is parsed as HTMLFencedFrameElement.
  EXPECT_EQ(true, EvalJs(primary_main_frame_host(), R"(
    (document.body.children.length === 1) &&
    (document.body.children[0] instanceof HTMLFencedFrameElement);
  )"));

  // 2. Verify the layout size of the element is the default empty frame size,
  // using iframes as a point of comparison.
  EXPECT_EQ(true, EvalJs(primary_main_frame_host(), R"(
    const ff = document.body.children[0];
    const ff_rect = ff.getBoundingClientRect();

    const reference_iframe = document.createElement('iframe');
    document.body.appendChild(reference_iframe);
    const ref_rect = reference_iframe.getBoundingClientRect();

    const is_matching = (ff_rect.width === ref_rect.width) &&
                        (ff_rect.height === ref_rect.height) &&
                        (ff.clientWidth === reference_iframe.clientWidth) &&
                        (ff.clientHeight === reference_iframe.clientHeight);

    reference_iframe.remove();

    is_matching;
  )"));

  // 3. Verify that the FencedFrameConfig class exists.
  EXPECT_EQ("function", EvalJs(primary_main_frame_host(),
                               "typeof window.FencedFrameConfig"));

  // 4. Verify that FencedFrameConfig cannot be constructed (throws TypeError
  // because the IDL constructor is removed).
  EXPECT_EQ(true, EvalJs(primary_main_frame_host(), R"(
    try {
      new FencedFrameConfig('https://example.com');
      false;
    } catch (e) {
      e instanceof TypeError;
    }
  )"));

  // 5. Verify that setting `el.config` to null is accepted.
  EXPECT_TRUE(ExecJs(primary_main_frame_host(), R"(
    const el = document.body.children[0];
    el.config = null;
  )"));

  // No child frames should be created.
  EXPECT_EQ(0U, primary_main_frame_host()->child_count());
}

}  // namespace content
