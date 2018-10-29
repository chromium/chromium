// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/pointer_lock_browsertest.h"

#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#ifdef USE_AURA
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/web_contents/web_contents_view_aura.h"
#endif  // USE_AURA

namespace content {

class MockPointerLockWebContentsDelegate : public WebContentsDelegate {
 public:
  MockPointerLockWebContentsDelegate() {}
  ~MockPointerLockWebContentsDelegate() override {}

  void RequestToLockMouse(WebContents* web_contents,
                          bool user_gesture,
                          bool last_unlocked_by_target) override {
    web_contents->GotResponseToLockMouseRequest(true);
  }

  void LostMouseLock() override {}
};

#ifdef USE_AURA
class MockPointerLockRenderWidgetHostView : public RenderWidgetHostViewAura {
 public:
  MockPointerLockRenderWidgetHostView(RenderWidgetHost* host,
                                      bool is_guest_view_hack)
      : RenderWidgetHostViewAura(host,
                                 is_guest_view_hack,
                                 false /* is_mus_browser_plugin_guest */),
        host_(RenderWidgetHostImpl::From(host)) {}
  ~MockPointerLockRenderWidgetHostView() override {
    if (IsMouseLocked())
      UnlockMouse();
  }

  bool LockMouse() override {
    event_handler()->mouse_locked_ = true;
    return true;
  }

  void UnlockMouse() override {
    host_->LostMouseLock();
    event_handler()->mouse_locked_ = false;
  }

  bool IsMouseLocked() override { return event_handler()->mouse_locked(); }

  bool HasFocus() const override { return true; }

  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override {
    // Ignore window focus events.
  }

  RenderWidgetHostImpl* host_;
};

void InstallCreateHooksForPointerLockBrowserTests() {
  WebContentsViewAura::InstallCreateHookForTests(
      [](RenderWidgetHost* host,
         bool is_guest_view_hack) -> RenderWidgetHostViewAura* {
        return new MockPointerLockRenderWidgetHostView(host,
                                                       is_guest_view_hack);
      });
}
#endif  // USE_AURA

class PointerLockBrowserTest : public ContentBrowserTest {
 public:
  PointerLockBrowserTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
  }

  void SetUp() override {
    InstallCreateHooksForPointerLockBrowserTests();
    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());

    web_contents()->SetDelegate(&web_contents_delegate_);
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

 protected:
  MockPointerLockWebContentsDelegate web_contents_delegate_;
};

IN_PROC_BROWSER_TEST_F(PointerLockBrowserTest, PointerLock) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);

  // Request a pointer lock on the root frame's body.
  EXPECT_TRUE(ExecJs(root, "document.body.requestPointerLock()"));

  // Root frame should have been granted pointer lock.
  EXPECT_EQ(true, EvalJs(root, "document.pointerLockElement == document.body"));

  // Request a pointer lock on the child frame's body.
  EXPECT_TRUE(ExecJs(child, "document.body.requestPointerLock()"));

  // Child frame should not be granted pointer lock since the root frame has it.
  EXPECT_EQ(false,
            EvalJs(child, "document.pointerLockElement == document.body"));

  // Release pointer lock on root frame.
  EXPECT_TRUE(ExecJs(root, "document.exitPointerLock()"));

  // Request a pointer lock on the child frame's body.
  EXPECT_TRUE(ExecJs(child, "document.body.requestPointerLock()"));

  // Child frame should have been granted pointer lock.
  EXPECT_EQ(true,
            EvalJs(child, "document.pointerLockElement == document.body"));
}

IN_PROC_BROWSER_TEST_F(PointerLockBrowserTest, PointerLockEventRouting) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);
  RenderWidgetHostInputEventRouter* router =
      web_contents()->GetInputEventRouter();
  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetView());
  RenderWidgetHostViewBase* child_view = static_cast<RenderWidgetHostViewBase*>(
      child->current_frame_host()->GetView());

  WaitForHitTestDataOrChildSurfaceReady(child->current_frame_host());

  // Request a pointer lock on the root frame's body.
  EXPECT_TRUE(ExecJs(root, "document.body.requestPointerLock()"));

  // Root frame should have been granted pointer lock.
  EXPECT_EQ(true, EvalJs(root, "document.pointerLockElement == document.body"));

  // Add a mouse move event listener to the root frame.
  EXPECT_TRUE(ExecJs(
      root,
      "var x; var y; var mX; var mY; document.addEventListener('mousemove', "
      "function(e) {x = e.x; y = e.y; mX = e.movementX; mY = e.movementY;});"));

  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::kMouseMove, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.SetPositionInWidget(10, 11);
  mouse_event.movement_x = 12;
  mouse_event.movement_y = 13;
  router->RouteMouseEvent(root_view, &mouse_event, ui::LatencyInfo());

  // Make sure that the renderer handled the input event.
  MainThreadFrameObserver root_observer(root_view->GetRenderWidgetHost());
  root_observer.Wait();

  EXPECT_EQ("[10,11,12,13]", EvalJs(root, "JSON.stringify([x,y,mX,mY])"));

  // Release pointer lock on root frame.
  EXPECT_TRUE(ExecJs(root, "document.exitPointerLock()"));

  // Request a pointer lock on the child frame's body.
  EXPECT_TRUE(ExecJs(child, "document.body.requestPointerLock()"));

  // Child frame should have been granted pointer lock.
  EXPECT_EQ(true,
            EvalJs(child, "document.pointerLockElement == document.body"));

  // Add a mouse move event listener to the child frame.
  EXPECT_TRUE(ExecJs(
      child,
      "var x; var y; var mX; var mY; document.addEventListener('mousemove', "
      "function(e) {x = e.x; y = e.y; mX = e.movementX; mY = e.movementY;});"));

  gfx::PointF transformed_point;
  root_view->TransformPointToCoordSpaceForView(gfx::PointF(0, 0), child_view,
                                               &transformed_point,
                                               viz::EventSource::MOUSE);

  mouse_event.SetPositionInWidget(-transformed_point.x() + 14,
                                  -transformed_point.y() + 15);
  mouse_event.movement_x = 16;
  mouse_event.movement_y = 17;
  // We use root_view intentionally as the RenderWidgetHostInputEventRouter is
  // responsible for correctly routing the event to the child frame.
  router->RouteMouseEvent(root_view, &mouse_event, ui::LatencyInfo());

  // Make sure that the renderer handled the input event.
  MainThreadFrameObserver child_observer(child_view->GetRenderWidgetHost());
  child_observer.Wait();

  EXPECT_EQ("[14,15,16,17]", EvalJs(child, "JSON.stringify([x,y,mX,mY])"));
}

// Tests that the browser will not unlock the pointer if a RenderWidgetHostView
// that doesn't hold the pointer lock is destroyed.
IN_PROC_BROWSER_TEST_F(PointerLockBrowserTest, PointerLockChildFrameDetached) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Request a pointer lock on the root frame's body.
  EXPECT_TRUE(ExecJs(root, "document.body.requestPointerLock()"));

  // Root frame should have been granted pointer lock.
  EXPECT_EQ(true, EvalJs(root, "document.pointerLockElement == document.body"));

  // Root (platform) RenderWidgetHostView should have the pointer locked.
  EXPECT_TRUE(root->current_frame_host()->GetView()->IsMouseLocked());
  EXPECT_EQ(root->current_frame_host()->GetRenderWidgetHost(),
            web_contents()->GetMouseLockWidget());

  // Detach the child frame.
  EXPECT_TRUE(ExecJs(root, "document.querySelector('iframe').remove()"));

  // Root (platform) RenderWidgetHostView should still have the pointer locked.
  EXPECT_TRUE(root->current_frame_host()->GetView()->IsMouseLocked());
  EXPECT_EQ(root->current_frame_host()->GetRenderWidgetHost(),
            web_contents()->GetMouseLockWidget());
}

// Tests that the browser will unlock the pointer if a RenderWidgetHostView that
// holds the pointer lock crashes.
IN_PROC_BROWSER_TEST_F(PointerLockBrowserTest,
                       PointerLockInnerContentsCrashes) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(b))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Attach an inner WebContents; it's owned by the FrameTree, so we obtain an
  // observer to it.
  WebContents* inner_contents = CreateAndAttachInnerContents(
      root->child_at(0)->child_at(0)->current_frame_host());
  WebContentsDestroyedWatcher inner_death_observer(inner_contents);

  // Override the delegate so that we can stub out pointer lock events.
  inner_contents->SetDelegate(&web_contents_delegate_);

  // Navigate the inner webcontents to a page.
  EXPECT_TRUE(NavigateToURLFromRenderer(
      inner_contents, embedded_test_server()->GetURL(
                          "c.com", "/cross_site_iframe_factory.html?c(d)")));

  // Request a pointer lock to the inner WebContents's document.body.
  EXPECT_EQ("success", EvalJs(inner_contents->GetMainFrame(), R"(
        new Promise((resolve, reject) => {
            document.addEventListener('pointerlockchange', resolve);
            document.addEventListener('pointerlockerror', reject);
            document.body.requestPointerLock();
        }).then(() => 'success');
        )"));

  // Root (platform) RenderWidgetHostView should have the pointer locked.
  EXPECT_TRUE(root->current_frame_host()->GetView()->IsMouseLocked());

  // The widget doing the lock is the one from the inner WebContents. A link
  // to that RWH is saved into the outer webcontents.
  RenderWidgetHost* expected_lock_widget =
      inner_contents->GetMainFrame()->GetView()->GetRenderWidgetHost();
  EXPECT_EQ(expected_lock_widget, web_contents()->GetMouseLockWidget());
  EXPECT_EQ(expected_lock_widget, web_contents()->mouse_lock_widget_);
  EXPECT_EQ(expected_lock_widget,
            static_cast<WebContentsImpl*>(inner_contents)->mouse_lock_widget_);

  // Crash the subframe process.
  RenderProcessHost* crash_process =
      root->child_at(0)->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      crash_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  crash_process->Shutdown(0);
  crash_observer.Wait();

  // Wait for destruction of |inner_contents|.
  inner_death_observer.Wait();
  inner_contents = nullptr;

  // This should cancel the pointer lock.
  EXPECT_EQ(nullptr, web_contents()->GetMouseLockWidget());
  EXPECT_EQ(nullptr, web_contents()->mouse_lock_widget_);
  EXPECT_FALSE(web_contents()->HasMouseLock(
      root->current_frame_host()->GetRenderWidgetHost()));
}

IN_PROC_BROWSER_TEST_F(PointerLockBrowserTest, PointerLockOopifCrashes) {
  // This test runs three times, testing a crash at each level of the frametree.
  for (int crash_depth = 0; crash_depth < 3; crash_depth++) {
    GURL main_url(embedded_test_server()->GetURL(
        "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
    EXPECT_TRUE(NavigateToURL(shell(), main_url));

    FrameTreeNode* root = web_contents()->GetFrameTree()->root();
    FrameTreeNode* lock_node = root->child_at(0)->child_at(0);

    // Pick which node to crash.
    FrameTreeNode* crash_node = root;
    for (int i = 0; i < crash_depth; i++)
      crash_node = crash_node->child_at(0);

    // Request a pointer lock to |lock_node|'s document.body.
    EXPECT_EQ("success", EvalJs(lock_node, R"(
        new Promise((resolve, reject) => {
            document.addEventListener('pointerlockchange', resolve);
            document.addEventListener('pointerlockerror', reject);
            document.body.requestPointerLock();
        }).then(() => 'success');
        )"));

    // Root (platform) RenderWidgetHostView should have the pointer locked.
    EXPECT_TRUE(root->current_frame_host()->GetView()->IsMouseLocked());
    EXPECT_EQ(lock_node->current_frame_host()->GetRenderWidgetHost(),
              web_contents()->GetMouseLockWidget());

    // Crash the process of |crash_node|.
    RenderProcessHost* crash_process =
        crash_node->current_frame_host()->GetProcess();
    RenderProcessHostWatcher crash_observer(
        crash_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    crash_process->Shutdown(0);
    crash_observer.Wait();

    // This should cancel the pointer lock.
    EXPECT_EQ(nullptr, web_contents()->GetMouseLockWidget());
    EXPECT_EQ(nullptr, web_contents()->mouse_lock_widget_);
    EXPECT_FALSE(web_contents()->HasMouseLock(
        root->current_frame_host()->GetRenderWidgetHost()));
    if (crash_depth != 0)
      EXPECT_FALSE(root->current_frame_host()->GetView()->IsMouseLocked());
    else
      EXPECT_EQ(nullptr, root->current_frame_host()->GetView());
  }
}

IN_PROC_BROWSER_TEST_F(PointerLockBrowserTest, PointerLockWheelEventRouting) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);
  RenderWidgetHostInputEventRouter* router =
      web_contents()->GetInputEventRouter();
  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetView());
  RenderWidgetHostViewBase* child_view = static_cast<RenderWidgetHostViewBase*>(
      child->current_frame_host()->GetView());

  WaitForHitTestDataOrChildSurfaceReady(child->current_frame_host());

  // Request a pointer lock on the root frame's body.
  EXPECT_TRUE(ExecJs(root, "document.body.requestPointerLock()"));

  // Root frame should have been granted pointer lock.
  EXPECT_EQ(true, EvalJs(root, "document.pointerLockElement == document.body"));

  // Add a mouse move wheel event listener to the root frame.
  EXPECT_TRUE(ExecJs(
      root,
      "var x; var y; var dX; var dY; document.addEventListener('mousewheel', "
      "function(e) {x = e.x; y = e.y; dX = e.deltaX; dY = e.deltaY;});"));
  MainThreadFrameObserver root_observer(root_view->GetRenderWidgetHost());
  root_observer.Wait();

  blink::WebMouseWheelEvent wheel_event(
      blink::WebInputEvent::kMouseWheel, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  wheel_event.SetPositionInWidget(10, 11);
  wheel_event.delta_x = -12;
  wheel_event.delta_y = -13;
  wheel_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  router->RouteMouseWheelEvent(root_view, &wheel_event, ui::LatencyInfo());

  // Make sure that the renderer handled the input event.
  root_observer.Wait();

  // All wheel events during a scroll sequence will be sent to a single target.
  // Send a wheel end event to the current target before sending wheel events to
  // a new target.
  wheel_event.delta_x = 0;
  wheel_event.delta_y = 0;
  wheel_event.phase = blink::WebMouseWheelEvent::kPhaseEnded;
  router->RouteMouseWheelEvent(root_view, &wheel_event, ui::LatencyInfo());

  // Make sure that the renderer handled the input event.
  root_observer.Wait();

  EXPECT_EQ("[10,11,12,13]", EvalJs(root, "JSON.stringify([x, y, dX, dY])"));

  // Release pointer lock on root frame.
  EXPECT_TRUE(ExecJs(root, "document.exitPointerLock()"));

  // Request a pointer lock on the child frame's body.
  EXPECT_TRUE(ExecJs(child, "document.body.requestPointerLock()"));

  // Child frame should have been granted pointer lock.
  EXPECT_EQ(true,
            EvalJs(child, "document.pointerLockElement == document.body"));

  // Add a mouse move event listener to the child frame.
  EXPECT_TRUE(ExecJs(
      child,
      "var x; var y; var dX; var dY; document.addEventListener('mousewheel', "
      "function(e) {x = e.x; y = e.y; dX = e.deltaX; dY = e.deltaY;});"));
  MainThreadFrameObserver child_observer(child_view->GetRenderWidgetHost());
  child_observer.Wait();

  gfx::PointF transformed_point;
  root_view->TransformPointToCoordSpaceForView(gfx::PointF(0, 0), child_view,
                                               &transformed_point,
                                               viz::EventSource::MOUSE);

  wheel_event.SetPositionInWidget(-transformed_point.x() + 14,
                                  -transformed_point.y() + 15);
  wheel_event.delta_x = -16;
  wheel_event.delta_y = -17;
  wheel_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  // We use root_view intentionally as the RenderWidgetHostInputEventRouter is
  // responsible for correctly routing the event to the child frame.
  router->RouteMouseWheelEvent(root_view, &wheel_event, ui::LatencyInfo());

  // Make sure that the renderer handled the input event.
  child_observer.Wait();

  EXPECT_EQ("[14,15,16,17]", EvalJs(child, "JSON.stringify([x, y, dX, dY])"));
}

IN_PROC_BROWSER_TEST_F(PointerLockBrowserTest, PointerLockWidgetHidden) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);
  RenderWidgetHostViewBase* child_view = static_cast<RenderWidgetHostViewBase*>(
      child->current_frame_host()->GetView());

  WaitForHitTestDataOrChildSurfaceReady(child->current_frame_host());

  // Request a pointer lock on the child frame's body.
  EXPECT_TRUE(ExecJs(child, "document.body.requestPointerLock()"));

  // Child frame should have been granted pointer lock.
  EXPECT_EQ(true,
            EvalJs(child, "document.pointerLockElement == document.body"));
  EXPECT_TRUE(child_view->IsMouseLocked());
  EXPECT_EQ(child_view->host(), web_contents()->GetMouseLockWidget());

  child_view->Hide();

  // Child frame should've released the mouse lock when hidden.
  EXPECT_FALSE(child_view->IsMouseLocked());
  EXPECT_EQ(nullptr, web_contents()->GetMouseLockWidget());
}

}  // namespace content
