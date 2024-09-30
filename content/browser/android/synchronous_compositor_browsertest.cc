// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/android/synchronous_compositor.h"

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "content/browser/android/synchronous_compositor_host.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/android/synchronous_compositor_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

class TestSynchronousCompositorClient : public SynchronousCompositorClient {
 public:
  TestSynchronousCompositorClient() = default;

  TestSynchronousCompositorClient(const TestSynchronousCompositorClient&) =
      delete;
  TestSynchronousCompositorClient& operator=(
      const TestSynchronousCompositorClient&) = delete;

  ~TestSynchronousCompositorClient() override = default;

  // SynchronousCompositorClient overrides.
  void DidInitializeCompositor(SynchronousCompositor* compositor,
                               const viz::FrameSinkId& id) override {
    DCHECK(compositor_map_.count(id) == 0);
    compositor_map_[id] = compositor;
  }

  void DidDestroyCompositor(SynchronousCompositor* compositor,
                            const viz::FrameSinkId& id) override {
    DCHECK(compositor_map_.count(id));
    compositor_map_.erase(id);
  }
  void UpdateRootLayerState(SynchronousCompositor* compositor,
                            const gfx::PointF& total_scroll_offset,
                            const gfx::PointF& max_scroll_offset,
                            const gfx::SizeF& scrollable_size,
                            float page_scale_factor,
                            float min_page_scale_factor,
                            float max_page_scale_factor) override {}
  void DidOverscroll(SynchronousCompositor* compositor,
                     const gfx::Vector2dF& accumulated_overscroll,
                     const gfx::Vector2dF& latest_overscroll_delta,
                     const gfx::Vector2dF& current_fling_velocity) override {}
  void PostInvalidate(SynchronousCompositor* compositor) override {}
  void DidUpdateContent(SynchronousCompositor* compositor) override {}
  ui::TouchHandleDrawable* CreateDrawable() override { return nullptr; }
  void CopyOutput(
      SynchronousCompositor* compositor,
      std::unique_ptr<viz::CopyOutputRequest> copy_request) override {}
  void AddBeginFrameCompletionCallback(base::OnceClosure callback) override {}
  void SetThreadIds(const std::vector<int32_t>& thread_ids) override {}

  SynchronousCompositor* GetCompositor(const viz::FrameSinkId& id) {
    auto itr = compositor_map_.find(id);
    if (itr == compositor_map_.end())
      return nullptr;
    return itr->second;
  }

 private:
  std::map<viz::FrameSinkId, raw_ptr<SynchronousCompositor, CtnExperimental>>
      compositor_map_;
};

class SynchronousCompositorBrowserTest : public ContentBrowserTest {
 public:
  SynchronousCompositorBrowserTest() = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
  }
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  TestSynchronousCompositorClient compositor_client_;
};

IN_PROC_BROWSER_TEST_F(SynchronousCompositorBrowserTest,
                       RenderWidgetHostViewAndroidReuse) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));

  // Open a popup and navigate it to a.com.
  Shell* popup = OpenPopup(
      shell(), embedded_test_server()->GetURL("a.com", "/title2.html"), "foo");
  WebContentsImpl* popup_contents =
      static_cast<WebContentsImpl*>(popup->web_contents());
  SynchronousCompositor::SetClientForWebContents(popup_contents,
                                                 &compositor_client_);
  RenderFrameHostImpl* rfh = popup_contents->GetPrimaryMainFrame();
  RenderViewHostImpl* rvh = rfh->render_view_host();
  viz::FrameSinkId id = rvh->GetWidget()->GetFrameSinkId();
  {
    SynchronousCompositorHost* compositor =
        static_cast<SynchronousCompositorHost*>(
            compositor_client_.GetCompositor(id));
    EXPECT_TRUE(compositor);
    EXPECT_TRUE(compositor->GetSynchronousCompositor());
  }

  // Navigate popup to b.com.  Because there's an opener, the RVH for a.com
  // stays around in the inactive state.
  EXPECT_TRUE(NavigateToURLInSameBrowsingInstance(
      popup, embedded_test_server()->GetURL("b.com", "/title3.html")));
  EXPECT_FALSE(rvh->is_active());
  EXPECT_FALSE(compositor_client_.GetCompositor(id));

  // Go back to a.com. This should make the rvh active again and reinitialize
  // SynchronousCompositor.
  TestNavigationObserver back_observer(popup_contents);
  popup_contents->GetController().GoBack();
  back_observer.Wait();
  EXPECT_TRUE(rvh->is_active());
  {
    SynchronousCompositorHost* compositor =
        static_cast<SynchronousCompositorHost*>(
            compositor_client_.GetCompositor(id));
    EXPECT_TRUE(compositor);
    EXPECT_TRUE(compositor->GetSynchronousCompositor());
  }
}

}  // namespace content
