// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/fenced_frame/fenced_frame.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame.mojom-test-utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
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

  FencedFrame* CreateAndGetFencedFrame() {
    RenderFrameHostImplWrapper primary_rfh(primary_main_frame_host());

    size_t previous_fenced_frame_count = primary_rfh->GetFencedFrames().size();

    EXPECT_TRUE(ExecJs(
        primary_rfh.get(),
        "document.body.appendChild(document.createElement('fencedframe'));"));

    std::vector<FencedFrame*> fenced_frames = primary_rfh->GetFencedFrames();
    EXPECT_EQ(previous_fenced_frame_count + 1, fenced_frames.size());

    // Return the most-recently added FencedFrame.
    return fenced_frames.back();
  }

  WebContentsImpl* web_contents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* primary_main_frame_host() {
    return web_contents()->GetMainFrame();
  }

  // Note that this method assumes the fenced frame was the first child added to
  // the primary render frame host.
  FrameTreeNode* fenced_frame_root_node() {
    RenderFrameHostImplWrapper dummy_child_frame(
        primary_main_frame_host()->child_at(0)->current_frame_host());
    EXPECT_NE(dummy_child_frame->inner_tree_main_frame_tree_node_id(),
              FrameTreeNode::kFrameTreeNodeInvalidId);
    return FrameTreeNode::GloballyFindByID(
        dummy_child_frame->inner_tree_main_frame_tree_node_id());
  }

  RenderFrameHostImpl* fenced_frame_root_host() {
    return fenced_frame_root_node()->current_frame_host();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the renderer can create a <fencedframe> that results in a
// browser-side content::FencedFrame also being created.
IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest, CreateFromScriptAndDestroy) {
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                         "fencedframe.test", "/title1.html")));
  RenderFrameHostImplWrapper primary_rfh(primary_main_frame_host());

  FencedFrame* fenced_frame = CreateAndGetFencedFrame();
  EXPECT_NE(fenced_frame, nullptr);
  RenderFrameHostImplWrapper inner_fenced_frame_rfh(fenced_frame_root_host());

  // Test that the outer => inner delegate mechanism works correctly.
  EXPECT_THAT(
      CollectAllRenderFrameHosts(primary_rfh.get()),
      testing::ElementsAre(primary_rfh.get(), inner_fenced_frame_rfh.get()));

  // Test that the inner => outer delegate mechanism works correctly.
  EXPECT_EQ(inner_fenced_frame_rfh->ParentOrOuterDelegateFrame(),
            primary_rfh.get());

  // Test `FrameTreeNode::IsFencedFrameRoot()`.
  EXPECT_FALSE(web_contents()->GetFrameTree()->root()->IsFencedFrameRoot());
  EXPECT_FALSE(primary_rfh->child_at(0)->IsFencedFrameRoot());
  EXPECT_TRUE(fenced_frame_root_node()->IsFencedFrameRoot());

  // Test `FrameTreeNode::IsInFencedFrameTree()`.
  EXPECT_FALSE(web_contents()->GetFrameTree()->root()->IsInFencedFrameTree());
  EXPECT_FALSE(primary_rfh->child_at(0)->IsInFencedFrameTree());
  EXPECT_TRUE(fenced_frame_root_node()->IsInFencedFrameTree());

  base::RunLoop destroyed_run_loop;
  fenced_frame->SetOnDestroyedCallbackForTesting(
      destroyed_run_loop.QuitClosure());
  EXPECT_TRUE(ExecJs(primary_rfh.get(),
                     "document.querySelector('fencedframe').remove();"));
  destroyed_run_loop.Run();

  EXPECT_TRUE(primary_rfh->GetFencedFrames().empty());
  EXPECT_TRUE(inner_fenced_frame_rfh.IsDestroyed());
}

IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest, CreateFromParser) {
  const GURL top_level_url = embedded_test_server()->GetURL(
      "fencedframe.test", "/fenced_frames/basic.html");
  EXPECT_TRUE(NavigateToURL(shell(), top_level_url));

  // The fenced frame is set-up synchronously, so it should exist immediately.
  EXPECT_TRUE(fenced_frame_root_host());
}

IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest, Navigation) {
  const GURL top_level_url =
      embedded_test_server()->GetURL("fencedframe.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), top_level_url));

  RenderFrameHostImplWrapper primary_rfh(primary_main_frame_host());

  FencedFrame* fenced_frame = CreateAndGetFencedFrame();
  EXPECT_NE(fenced_frame, nullptr);
  RenderFrameHostImplWrapper inner_fenced_frame_rfh(fenced_frame_root_host());

  const GURL fenced_frame_url =
      embedded_test_server()->GetURL("fencedframe.test", "/title2.html");
  EXPECT_TRUE(
      ExecJs(primary_rfh.get(),
             JsReplace("document.querySelector('fencedframe').src = $1;",
                       fenced_frame_url.spec())));
  fenced_frame->WaitForDidStopLoadingForTesting();

  // Test that a fenced frame navigation does not impact the primary main
  // frame...
  EXPECT_EQ(top_level_url, primary_rfh->GetLastCommittedURL());
  // ... but should target the correct frame.
  EXPECT_EQ(fenced_frame_url, fenced_frame_root_host()->GetLastCommittedURL());
  EXPECT_EQ(url::Origin::Create(fenced_frame_url),
            fenced_frame_root_host()->GetLastCommittedOrigin());
}

}  // namespace content
