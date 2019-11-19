// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/test_content_browser_client.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/gfx/geometry/size.h"

namespace content {

class RenderWidgetHostViewChildFrameTest : public ContentBrowserTest {
 public:
  RenderWidgetHostViewChildFrameTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void CheckScreenWidth(RenderFrameHost* render_frame_host) {
    int width =
        ExecuteScriptAndGetValue(render_frame_host, "window.screen.width")
            .GetInt();
    EXPECT_EQ(expected_screen_width_, width);
  }

  // Tests that the FrameSinkId of each child frame has been updated by the
  // RenderFrameProxy.
  void CheckFrameSinkId(RenderFrameHost* render_frame_host) {
    RenderWidgetHostViewBase* child_view =
        static_cast<RenderFrameHostImpl*>(render_frame_host)
            ->GetRenderWidgetHost()
            ->GetView();
    // Only interested in updated FrameSinkIds on child frames.
    if (!child_view || !child_view->IsRenderWidgetHostViewChildFrame())
      return;

    // Ensure that the received viz::FrameSinkId was correctly set on the child
    // frame.
    viz::FrameSinkId actual_frame_sink_id_ = child_view->GetFrameSinkId();
    EXPECT_EQ(expected_frame_sink_id_, actual_frame_sink_id_);

    // The viz::FrameSinkID will be replaced while the test blocks for
    // navigation. It should differ from the information stored in the child's
    // RenderWidgetHost.
    EXPECT_NE(base::checked_cast<uint32_t>(
                  child_view->GetRenderWidgetHost()->GetProcess()->GetID()),
              actual_frame_sink_id_.client_id());
    EXPECT_NE(base::checked_cast<uint32_t>(
                  child_view->GetRenderWidgetHost()->GetRoutingID()),
              actual_frame_sink_id_.sink_id());
  }

  void GiveItSomeTime() {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }

  void set_expected_frame_sink_id(viz::FrameSinkId frame_sink_id) {
    expected_frame_sink_id_ = frame_sink_id;
  }

  void set_expected_screen_width(int width) { expected_screen_width_ = width; }

 private:
  viz::FrameSinkId expected_frame_sink_id_;
  int expected_screen_width_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewChildFrameTest);
};

// Tests that the screen is properly reflected for RWHVChildFrame.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewChildFrameTest, Screen) {
  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // Load cross-site page into iframe.
  GURL cross_site_url(
      embedded_test_server()->GetURL("foo.com", "/title2.html"));
  NavigateFrameToURL(root->child_at(0), cross_site_url);

  int main_frame_screen_width =
      ExecuteScriptAndGetValue(shell()->web_contents()->GetMainFrame(),
                               "window.screen.width")
          .GetInt();
  set_expected_screen_width(main_frame_screen_width);
  EXPECT_NE(main_frame_screen_width, 0);

  shell()->web_contents()->ForEachFrame(
      base::BindRepeating(&RenderWidgetHostViewChildFrameTest::CheckScreenWidth,
                          base::Unretained(this)));
}

// Test that auto-resize sizes in the top frame are propagated to OOPIF
// RenderWidgetHostViews. See https://crbug.com/726743.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewChildFrameTest,
                       ChildFrameAutoResizeUpdate) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "a.com", "/cross_site_iframe_factory.html?a(b)")));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  root->current_frame_host()
      ->GetRenderWidgetHost()
      ->GetView()
      ->EnableAutoResize(gfx::Size(0, 0), gfx::Size(100, 100));

  RenderWidgetHostView* rwhv =
      root->child_at(0)->current_frame_host()->GetRenderWidgetHost()->GetView();

  // Fake an auto-resize update from the parent renderer.
  viz::LocalSurfaceId local_surface_id(10, 10,
                                       base::UnguessableToken::Create());
  cc::RenderFrameMetadata metadata;
  metadata.viewport_size_in_pixels = gfx::Size(75, 75);
  metadata.local_surface_id_allocation =
      viz::LocalSurfaceIdAllocation(local_surface_id, base::TimeTicks::Now());
  root->current_frame_host()->GetRenderWidgetHost()->DidUpdateVisualProperties(
      metadata);

  // The child frame's RenderWidgetHostView should now use the auto-resize value
  // for its visible viewport.
  EXPECT_EQ(gfx::Size(75, 75), rwhv->GetVisibleViewportSize());
}

// Validate that OOPIFs receive presentation feedbacks.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewChildFrameTest,
                       PresentationFeedback) {
  base::HistogramTester histogram_tester;
  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  // Load cross-site page into iframe.
  GURL cross_site_url(
      embedded_test_server()->GetURL("foo.com", "/title2.html"));
  NavigateFrameToURL(root->child_at(0), cross_site_url);

  auto* child_rwh_impl =
      root->child_at(0)->current_frame_host()->GetRenderWidgetHost();
  // Hide the frame and make it visible again, to force it to record the
  // tab-switch time, which is generated from presentation-feedback.
  child_rwh_impl->WasHidden();
  child_rwh_impl->WasShown(RecordTabSwitchTimeRequest{
      base::TimeTicks::Now(), /* destination_is_loaded */ true,
      /* destination_is_frozen */ false});
  // Force the child to submit a new frame.
  ASSERT_TRUE(ExecuteScript(root->child_at(0)->current_frame_host(),
                            "document.write('Force a new frame.');"));
  do {
    FetchHistogramsFromChildProcesses();
    GiveItSomeTime();
  } while (histogram_tester.GetAllSamples("MPArch.RWH_TabSwitchPaintDuration")
               .size() != 1);
}

}  // namespace content
