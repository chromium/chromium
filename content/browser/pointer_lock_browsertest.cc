// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/pointer_lock_browsertest.h"

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/features.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/ui_base_features.h"

#ifdef USE_AURA
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/web_contents/web_contents_view_aura.h"
#endif  // USE_AURA

namespace content {

class MockPointerLockWebContentsDelegate : public WebContentsDelegate {
 public:
  MockPointerLockWebContentsDelegate() {}
  ~MockPointerLockWebContentsDelegate() override {}

  void RequestPointerLock(WebContents* web_contents,
                          bool user_gesture,
                          bool last_unlocked_by_target) override {
    if (user_gesture)
      web_contents->GotResponseToPointerLockRequest(
          blink::mojom::PointerLockResult::kSuccess);
    else
      web_contents->GotResponseToPointerLockRequest(
          blink::mojom::PointerLockResult::kRequiresUserGesture);
  }

  void LostPointerLock() override {}
};

#ifdef USE_AURA
class ScopedEnableUnadjustedMouseEventsForTesting
    : public aura::ScopedEnableUnadjustedMouseEvents {
 public:
  explicit ScopedEnableUnadjustedMouseEventsForTesting() {}
  ~ScopedEnableUnadjustedMouseEventsForTesting() override {}
};

class MockPointerLockRenderWidgetHostView : public RenderWidgetHostViewAura {
 public:
  MockPointerLockRenderWidgetHostView(RenderWidgetHost* host)
      : RenderWidgetHostViewAura(host),
        host_(RenderWidgetHostImpl::From(host)) {}
  ~MockPointerLockRenderWidgetHostView() override {
    if (IsPointerLocked()) {
      UnlockPointer();
    }
  }

  blink::mojom::PointerLockResult LockPointer(
      bool request_unadjusted_movement) override {
    event_handler()->mouse_locked_ = true;
    event_handler()->mouse_locked_unadjusted_movement_ =
        request_unadjusted_movement
            ? std::make_unique<ScopedEnableUnadjustedMouseEventsForTesting>()
            : nullptr;
    return blink::mojom::PointerLockResult::kSuccess;
  }

  void UnlockPointer() override {
    host_->LostPointerLock();
    event_handler()->mouse_locked_ = false;
    event_handler()->mouse_locked_unadjusted_movement_.reset();
  }

  bool GetIsPointerLockedUnadjustedMovementForTesting() override {
    return IsPointerLocked() &&
           event_handler()->mouse_locked_unadjusted_movement_;
  }

  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override {
    // Ignore window focus events.
  }

  bool IsPointerLocked() override { return event_handler()->mouse_locked(); }

  bool HasFocus() override { return has_focus_; }

  raw_ptr<RenderWidgetHostImpl> host_;
  bool has_focus_ = true;
};

void InstallCreateHooksForPointerLockBrowserTests() {
  WebContentsViewAura::InstallCreateHookForTests(
      [](RenderWidgetHost* host) -> RenderWidgetHostViewAura* {
        return new MockPointerLockRenderWidgetHostView(host);
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

namespace {
class PointerLockHelper {
 public:
  // requestPointerLock is an asynchronous operation. This method returns when
  // document.body.requestPointerLock() either succeeds or fails.
  // Returns true if Pointer Lock on body was successful.
  static EvalJsResult RequestPointerLockOnBody(
      const ToRenderFrameHost& execution_target,
      const int options = EXECUTE_SCRIPT_DEFAULT_OPTIONS) {
    return EvalJs(execution_target,
                  set_pointer_lock_promise_ +
                      "document.body.requestPointerLock();" +
                      wait_for_pointer_lock_promise_,
                  options);
  }
  static EvalJsResult RequestPointerLockWithUnadjustedMovementOnBody(
      const ToRenderFrameHost& execution_target,
      const int options = EXECUTE_SCRIPT_DEFAULT_OPTIONS) {
    return EvalJs(
        execution_target,
        set_pointer_lock_promise_ +
            "document.body.requestPointerLock({unadjustedMovement:true});" +
            wait_for_pointer_lock_promise_,
        options);
  }
  // exitPointerLock is an asynchronous operation. This method returns when
  // document.exitPointerLock() either succeeds or fails.
  // Returns true if Exit Pointer Lock was successful
  static EvalJsResult ExitPointerLock(
      const ToRenderFrameHost& execution_target,
      const int options = EXECUTE_SCRIPT_DEFAULT_OPTIONS) {
    return EvalJs(execution_target,
                  set_pointer_lock_promise_ + "document.exitPointerLock();" +
                      wait_for_pointer_lock_promise_,
                  options);
  }
  static EvalJsResult IsPointerLockOnBody(
      const ToRenderFrameHost& execution_target,
      const int options = EXECUTE_SCRIPT_DEFAULT_OPTIONS) {
    return EvalJs(execution_target,
                  "document.pointerLockElement === document.body", options);
  }

 private:
  static const std::string set_pointer_lock_promise_;
  static const std::string wait_for_pointer_lock_promise_;
};

// static
const std::string PointerLockHelper::set_pointer_lock_promise_ =
    R"code(pointerLockPromise=new Promise(function (resolve, reject){
        document.addEventListener('pointerlockchange', resolve);
        document.addEventListener('pointerlockerror', reject);
     });)code";
// static
const std::string PointerLockHelper::wait_for_pointer_lock_promise_ =
    "(async()=> {return await pointerLockPromise.then(()=>true, "
    "()=>false);})()";
}  // namespace

IN_PROC_BROWSER_TEST_F(PointerLockBrowserTest, PointerLockBasic) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);

  // Request a pointer lock on the root frame's body.
  EXPECT_EQ(true, PointerLockHelper::RequestPointerLockOnBody(root));
  EXPECT_EQ(true, PointerLockHelper::IsPointerLockOnBody(root));

  // Request a pointer lock on the child frame's body.
  EXPECT_EQ(false, PointerLockHelper::RequestPointerLockOnBody(child));
  // Child frame should not be granted pointer lock since the root frame has it.
  EXPECT_EQ(false, PointerLockHelper::IsPointerLockOnBody(child));

  // Release pointer lock on root frame.
  EXPECT_EQ(true, PointerLockHelper::ExitPointerLock(root));

  // Request a pointer lock on the child frame's body.
  EXPECT_EQ(true, PointerLockHelper::RequestPointerLockOnBody(child));
  // ensure request finishes before moving on.

  // Child frame should have been granted pointer lock.
  EXPECT_EQ(true, PointerLockHelper::IsPointerLockOnBody(child));
}

IN_PROC_BROWSER_TEST_F(PointerLockBrowserTest, PointerLockAndUserActivation) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(b))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);
  FrameTreeNode* grand_child = child->child_at(0);

  // Without user activation, pointer lock request from any (child or
  // grand_child) frame fails.
  EXPECT_EQ(false, PointerLockHelper::RequestPointerLockOnBody(
                       child, EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_EQ(false, PointerLockHelper::IsPointerLockOnBody(
                       child, EXECUTE_SCRIPT_NO_USER_GESTURE));

  EXPECT_EQ(false, PointerLockHelper::RequestPointerLockOnBody(
                       grand_child, EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_EQ(false, PointerLockHelper::IsPointerLockOnBody(
                       grand_child, EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Execute a empty (dummy) JS to activate the child frame.
  EXPECT_TRUE(ExecJs(child, ""));

  // With user activation in the child frame, pointer lock from the same frame
  // succeeds.
  EXPECT_EQ(true, PointerLockHelper::RequestPointerLockOnBody(
                      child, EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_EQ(true, PointerLockHelper::IsPointerLockOnBody(
                      child, EXECUTE_SCRIPT_NO_USER_GESTURE));

  // But with user activation in the child frame, pointer lock from the
  // grand_child frame fails.
  EXPECT_EQ(false, PointerLockHelper::RequestPointerLockOnBody(
                       grand_child, EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_EQ(false, PointerLockHelper::IsPointerLockOnBody(
                       grand_child, EXECUTE_SCRIPT_NO_USER_GESTURE));
}

// crbug.com/1210940: flaky on Linux
#if BUILDFLAG(IS_LINUX)
#define MAYBE_PointerLockEventRouting DISABLED_PointerLockEventRouting
#else
#define MAYBE_PointerLockEventRouting PointerLockEventRouting
#endif
IN_PROC_BROWSER_TEST_F(PointerLockBrowserTest, MAYBE_PointerLockEventRouting) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);
  input::RenderWidgetHostInputEventRouter* router =
      web_contents()->GetInputEventRouter();
  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetView());
  RenderWidgetHostViewBase* child_view = static_cast<RenderWidgetHostViewBase*>(
      child->current_frame_host()->GetView());

  WaitForHitTestData(child->current_frame_host());

  std::string set_mouse_move_event_listener = R"(
    mouseMoveExecuted = new Promise(function (resolve, reject) {
      mousemoveHandler = function(e) {
        x = e.x;
        y = e.y;
        mX = e.movementX;
        mY = e.movementY;
        resolve();
      };
      document.addEventListener('mousemove', mousemoveHandler, {once: true});
    });
    true; // A promise is defined above, but do not wait.
  )";
  std::string define_variables = R"(
    var x;
    var y;
    var mX;
    var mY;
    var mouseMoveExecuted;
    var mousemoveHandler;
  )";
  // Add a mouse move event listener to the root frame.
  EXPECT_TRUE(ExecJs(root, define_variables));
  EXPECT_TRUE(ExecJs(root, set_mouse_move_event_listener));

  // Send a mouse move to root frame before lock to set last mouse position.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseMove,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.pointer_type = blink::WebPointerProperties::PointerType::kMouse;
  mouse_event.SetPositionInWidget(6, 7);
  mouse_event.SetPositionInScreen(6, 7);
  mouse_event.movement_x = 8;
  mouse_event.movement_y = 9;
  router->RouteMouseEvent(root_view, &mouse_event, ui::LatencyInfo());

  // wait for mouse move to fire mouse move event
  EXPECT_EQ(true, EvalJs(root,
                         "(async ()=> {return await "
                         "mouseMoveExecuted.then(()=>true);})();"));
  EXPECT_EQ("[6,7,0,0]", EvalJs(root, "JSON.stringify([x,y,mX,mY])"));
  EXPECT_EQ(true, PointerLockHelper::RequestPointerLockOnBody(root));
  // Root frame should have been granted pointer lock.
  EXPECT_EQ(true, PointerLockHelper::IsPointerLockOnBody(root));
  EXPECT_TRUE(ExecJs(root, set_mouse_move_event_listener));

  mouse_event.SetPositionInWidget(10, 12);
  mouse_event.SetPositionInScreen(10, 12);
  mouse_event.movement_x = 12;
  mouse_event.movement_y = 13;
  router->RouteMouseEvent(root_view, &mouse_event, ui::LatencyInfo());

  EXPECT_EQ(true, EvalJs(root,
                         "(async ()=> {return await "
                         "mouseMoveExecuted.then(()=>true);})();"));
  // Locked event has same coordinates as before locked.
  EXPECT_EQ("[6,7,4,5]", EvalJs(root, "JSON.stringify([x,y,mX,mY])"));

  EXPECT_EQ(true, PointerLockHelper::ExitPointerLock(root));

  EXPECT_EQ(true, PointerLockHelper::RequestPointerLockOnBody(child));

  // define all all global variables on the child
  EXPECT_TRUE(ExecJs(child, define_variables));
  // Child frame should have been granted pointer lock.
  EXPECT_EQ(true, PointerLockHelper::IsPointerLockOnBody(child));

  // Add a mouse move event listener to the child frame.
  EXPECT_TRUE(ExecJs(child, set_mouse_move_event_listener));

  gfx::PointF transformed_point;
  root_view->TransformPointToCoordSpaceForView(gfx::PointF(0, 0), child_view,
                                               &transformed_point);
  mouse_event.SetPositionInWidget(-transformed_point.x() + 14,
                                  -transformed_point.y() + 15);
  mouse_event.SetPositionInScreen(-transformed_point.x() + 14,
                                  -transformed_point.y() + 15);
  mouse_event.movement_x = 16;
  mouse_event.movement_y = 17;
  // We use root_view intentionally as the RenderWidgetHostInputEventRouter is
  // responsible for correctly routing the event to the child frame.
  router->RouteMouseEvent(root_view, &mouse_event, ui::LatencyInfo());

  EXPECT_EQ(true, EvalJs(child,
                         "(async ()=> {return await "
                         "mouseMoveExecuted.then(()=>true);})()"));
  // This is the first event to child render, so the coordinates is (0, 0)
  EXPECT_EQ("[0,0,0,0]", EvalJs(child, "JSON.stringify([x,y,mX,mY])"));
}

// Tests that the browser will not unlock the pointer if a RenderWidgetHostView
// that doesn't hold the pointer lock is destroyed
IN_PROC_BROWSER_TEST_F(PointerLockBrowserTest, PointerLockChildFrameDetached) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Request a pointer lock on the root frame's body.
  EXPECT_EQ(true, PointerLockHelper::RequestPointerLockOnBody(root));
  // Root frame should have been granted pointer lock.
  EXPECT_EQ(true, PointerLockHelper::IsPointerLockOnBody(root));

  // Root (platform) RenderWidgetHostView should have the pointer locked.
  EXPECT_TRUE(root->current_frame_host()->GetView()->IsPointerLocked());
  EXPECT_EQ(root->current_frame_host()->GetRenderWidgetHost(),
            web_contents()->GetPointerLockWidget());

  // Detach the child frame.
  EXPECT_TRUE(ExecJs(root, "document.querySelector('iframe').remove()"));

  // Root (platform) RenderWidgetHostView should still have the pointer locked.
  EXPECT_TRUE(root->current_frame_host()->GetView()->IsPointerLocked());
  EXPECT_EQ(root->current_frame_host()->GetRenderWidgetHost(),
            web_contents()->GetPointerLockWidget());
}

// Tests that the browser will unlock the pointer if a RenderWidgetHostView that
// holds the pointer lock crashes.
IN_PROC_BROWSER_TEST_F(PointerLockBrowserTest,
                       PointerLockInnerContentsCrashes) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(b))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

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
  EXPECT_EQ(true, PointerLockHelper::RequestPointerLockOnBody(
                      inner_contents->GetPrimaryMainFrame()));
  EXPECT_EQ(true, PointerLockHelper::IsPointerLockOnBody(
                      inner_contents->GetPrimaryMainFrame()));

  // Root (platform) RenderWidgetHostView should have the pointer locked.
  EXPECT_TRUE(root->current_frame_host()->GetView()->IsPointerLocked());

  // The widget doing the lock is the one from the inner WebContents. A link
  // to that RWH is saved into the outer webcontents.
  RenderWidgetHost* expected_lock_widget =
      inner_contents->GetPrimaryMainFrame()->GetView()->GetRenderWidgetHost();
  EXPECT_EQ(expected_lock_widget, web_contents()->GetPointerLockWidget());
  EXPECT_EQ(expected_lock_widget, web_contents()->pointer_lock_widget_);
  EXPECT_EQ(
      expected_lock_widget,
      static_cast<WebContentsImpl*>(inner_contents)->pointer_lock_widget_);

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
  EXPECT_EQ(nullptr, web_contents()->GetPointerLockWidget());
  EXPECT_EQ(nullptr, web_contents()->pointer_lock_widget_.get());
  EXPECT_FALSE(web_contents()->HasPointerLock(
      root->current_frame_host()->GetRenderWidgetHost()));
}

IN_PROC_BROWSER_TEST_F(PointerLockBrowserTest, PointerLockOopifCrashes) {
  // This test runs three times, testing a crash at each level of the frametree.
  for (int crash_depth = 0; crash_depth < 3; crash_depth++) {
    GURL main_url(embedded_test_server()->GetURL(
        "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
    EXPECT_TRUE(NavigateToURL(shell(), main_url));

    FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
    FrameTreeNode* lock_node = root->child_at(0)->child_at(0);

    // Pick which node to crash.
    FrameTreeNode* crash_node = root;
    for (int i = 0; i < crash_depth; i++)
      crash_node = crash_node->child_at(0);

    // Request a pointer lock to |lock_node|'s document.body.
    EXPECT_EQ(true, PointerLockHelper::RequestPointerLockOnBody(lock_node));
    EXPECT_EQ(true, PointerLockHelper::IsPointerLockOnBody(lock_node));

    // Root (platform) RenderWidgetHostView should have the pointer locked.
    EXPECT_TRUE(root->current_frame_host()->GetView()->IsPointerLocked());
    EXPECT_EQ(lock_node->current_frame_host()->GetRenderWidgetHost(),
              web_contents()->GetPointerLockWidget());

    // Crash the process of |crash_node|.
    RenderProcessHost* crash_process =
        crash_node->current_frame_host()->GetProcess();
    RenderProcessHostWatcher crash_observer(
        crash_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    crash_process->Shutdown(0);
    crash_observer.Wait();

    // This should cancel the pointer lock.
    EXPECT_EQ(nullptr, web_contents()->GetPointerLockWidget());
    EXPECT_EQ(nullptr, web_contents()->pointer_lock_widget_.get());
    EXPECT_FALSE(web_contents()->HasPointerLock(
        root->current_frame_host()->GetRenderWidgetHost()));
    if (crash_depth != 0)
      EXPECT_FALSE(root->current_frame_host()->GetView()->IsPointerLocked());
    else
      EXPECT_EQ(nullptr, root->current_frame_host()->GetView());
  }
}

#if BUILDFLAG(IS_LINUX)
#define MAYBE_PointerLockWheelEventRouting DISABLED_PointerLockWheelEventRouting
#else
#define MAYBE_PointerLockWheelEventRouting PointerLockWheelEventRouting
#endif
IN_PROC_BROWSER_TEST_F(PointerLockBrowserTest,
                       MAYBE_PointerLockWheelEventRouting) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);
  input::RenderWidgetHostInputEventRouter* router =
      web_contents()->GetInputEventRouter();
  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetView());
  RenderWidgetHostViewBase* child_view = static_cast<RenderWidgetHostViewBase*>(
      child->current_frame_host()->GetView());

  WaitForHitTestData(child->current_frame_host());

  // Add a mouse move event listener to the root frame.
  EXPECT_TRUE(ExecJs(
      root,
      "var x; var y; var dX; var dY; document.addEventListener('mousemove', "
      "function(e) {x = e.x; y = e.y; mX = e.movementX; mY = e.movementY;});"));

  // Send a mouse move to root frame before lock to set last mouse position.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseMove,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.pointer_type = blink::WebPointerProperties::PointerType::kMouse;
  mouse_event.SetPositionInWidget(6, 7);
  mouse_event.SetPositionInScreen(6, 7);
  mouse_event.movement_x = 8;
  mouse_event.movement_y = 9;
  router->RouteMouseEvent(root_view, &mouse_event, ui::LatencyInfo());

  // Make sure that the renderer handled the input event.
  MainThreadFrameObserver root_observer(root_view->GetRenderWidgetHost());
  root_observer.Wait();

  EXPECT_EQ("[6,7,0,0]", EvalJs(root, "JSON.stringify([x,y,mX,mY])"));

  EXPECT_EQ(true, PointerLockHelper::RequestPointerLockOnBody(root));

  // Root frame should have been granted pointer lock.
  EXPECT_EQ(true, PointerLockHelper::IsPointerLockOnBody(root));

  // Add a mouse move wheel event listener to the root frame.
  EXPECT_TRUE(ExecJs(
      root,
      "var x; var y; var dX; var dY; document.addEventListener('mousewheel', "
      "function(e) {x = e.x; y = e.y; dX = e.deltaX; dY = e.deltaY;});"));
  root_observer.Wait();

  blink::WebMouseWheelEvent wheel_event(
      blink::WebInputEvent::Type::kMouseWheel,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  wheel_event.SetPositionInScreen(10, 11);
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

  // Locked event has same coordinates as before locked.
  EXPECT_EQ("[6,7,12,13]", EvalJs(root, "JSON.stringify([x, y, dX, dY])"));

  EXPECT_EQ(true, PointerLockHelper::ExitPointerLock(root));

  EXPECT_EQ(true, PointerLockHelper::RequestPointerLockOnBody(child));

  // Child frame should have been granted pointer lock.
  EXPECT_EQ(true, PointerLockHelper::IsPointerLockOnBody(child));

  // Add a mouse move event listener to the child frame.
  EXPECT_TRUE(ExecJs(
      child,
      "var x; var y; var dX; var dY; document.addEventListener('mousewheel', "
      "function(e) {x = e.x; y = e.y; dX = e.deltaX; dY = e.deltaY;});"));
  MainThreadFrameObserver child_observer(child_view->GetRenderWidgetHost());
  child_observer.Wait();

  gfx::PointF transformed_point;
  root_view->TransformPointToCoordSpaceForView(gfx::PointF(0, 0), child_view,
                                               &transformed_point);

  wheel_event.SetPositionInWidget(-transformed_point.x() + 14,
                                  -transformed_point.y() + 15);
  wheel_event.SetPositionInScreen(-transformed_point.x() + 14,
                                  -transformed_point.y() + 15);
  wheel_event.delta_x = -16;
  wheel_event.delta_y = -17;
  wheel_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  // We use root_view intentionally as the RenderWidgetHostInputEventRouter is
  // responsible for correctly routing the event to the child frame.
  router->RouteMouseWheelEvent(root_view, &wheel_event, ui::LatencyInfo());

  // Make sure that the renderer handled the input event.
  child_observer.Wait();

  // This is the first event to child render, so the coordinates is (0, 0)
  EXPECT_EQ("[0,0,16,17]", EvalJs(child, "JSON.stringify([x, y, dX, dY])"));
}

IN_PROC_BROWSER_TEST_F(PointerLockBrowserTest, PointerLockWidgetHidden) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);
  RenderWidgetHostViewBase* child_view = static_cast<RenderWidgetHostViewBase*>(
      child->current_frame_host()->GetView());

  WaitForHitTestData(child->current_frame_host());

  // Request a pointer lock on the child frame's body.
  EXPECT_EQ(true, PointerLockHelper::RequestPointerLockOnBody(child));
  // Child frame should have been granted pointer lock.
  EXPECT_EQ(true, PointerLockHelper::IsPointerLockOnBody(child));

  EXPECT_TRUE(child_view->IsPointerLocked());
  EXPECT_EQ(child_view->host(), web_contents()->GetPointerLockWidget());

  child_view->Hide();

  // Child frame should've released the mouse lock when hidden.
  EXPECT_FALSE(child_view->IsPointerLocked());
  EXPECT_EQ(nullptr, web_contents()->GetPointerLockWidget());
}

#ifdef USE_AURA
IN_PROC_BROWSER_TEST_F(PointerLockBrowserTest, PointerLockOutOfFocus) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  MockPointerLockRenderWidgetHostView* root_view =
      static_cast<MockPointerLockRenderWidgetHostView*>(
          root->current_frame_host()->GetView());

  root_view->has_focus_ = false;
  // Request a pointer lock on the root frame's body.
  EXPECT_EQ(false, PointerLockHelper::RequestPointerLockOnBody(root));
  // Root frame should not have been granted pointer lock.
  EXPECT_EQ(false, PointerLockHelper::IsPointerLockOnBody(root));
}
#endif

IN_PROC_BROWSER_TEST_F(PointerLockBrowserTest, PointerLockOnDroppedElem) {
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/pointerlock_on_dropped_elem.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(ExecJs(shell(), "document.body.click();"));

  // The second ExecJS() call here delays test termination so that the first
  // call's async tasks get a chance to run.
  EXPECT_TRUE(ExecJs(shell(), "", EXECUTE_SCRIPT_NO_USER_GESTURE));
}

IN_PROC_BROWSER_TEST_F(PointerLockBrowserTest,
                       PointerLockRequestUnadjustedMovement) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  EXPECT_TRUE(ExecJs(root, "var pointerLockPromise;"));
  std::string wait_for_pointer_lock_promise =
      "(async ()=> {return await pointerLockPromise.then(()=>true, "
      "()=>false);})()";
  std::string set_pointer_lock_promise =
      R"code(pointerLockPromise = new Promise( function(resolve, reject){
                        document.addEventListener('pointerlockchange', resolve);
                        document.addEventListener('pointerlockerror', reject)
                     });)code";

  // Request a pointer lock.
  EXPECT_EQ(true, PointerLockHelper::RequestPointerLockOnBody(root));
  // Root frame should have been granted pointer lock.
  EXPECT_EQ(true, PointerLockHelper::IsPointerLockOnBody(root));
  // Mouse is locked and unadjusted_movement is not set.
  EXPECT_TRUE(root->current_frame_host()->GetView()->IsPointerLocked());

  // Release pointer lock.
  EXPECT_EQ(true, PointerLockHelper::ExitPointerLock(root));

#if defined(USE_AURA) || BUILDFLAG(IS_MAC)
  // Request a pointer lock with unadjustedMovement.
  EXPECT_EQ(
      true,
      PointerLockHelper::RequestPointerLockWithUnadjustedMovementOnBody(root));
  // Root frame should have been granted pointer lock.
  EXPECT_EQ(true, PointerLockHelper::IsPointerLockOnBody(root));

  // Mouse is locked and unadjusted_movement is set.
  EXPECT_TRUE(root->current_frame_host()->GetView()->IsPointerLocked());
  EXPECT_TRUE(root->current_frame_host()
                  ->GetView()
                  ->GetIsPointerLockedUnadjustedMovementForTesting());

  // Release pointer lock, unadjusted_movement bit is reset.
  EXPECT_EQ(true, PointerLockHelper::ExitPointerLock(root));

  EXPECT_FALSE(root->current_frame_host()
                   ->GetView()
                   ->GetIsPointerLockedUnadjustedMovementForTesting());
#else
  // Request a pointer lock with unadjustedMovement.
  // On platform that does not support unadjusted movement yet, do not lock and
  // a pointerlockerror event is dispatched.
  EXPECT_EQ(
      false,
      PointerLockHelper::RequestPointerLockWithUnadjustedMovementOnBody(root));
  EXPECT_EQ(false, PointerLockHelper::IsPointerLockOnBody(root));
  EXPECT_FALSE(root->current_frame_host()->GetView()->IsPointerLocked());
#endif
}

#if defined(USE_AURA)
// Flaky on all platforms http://crbug.com/1198612.
IN_PROC_BROWSER_TEST_F(PointerLockBrowserTest, DISABLED_UnadjustedMovement) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  input::RenderWidgetHostInputEventRouter* router =
      web_contents()->GetInputEventRouter();
  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetView());

  // Add a mouse move event listener to the root frame.
  EXPECT_TRUE(ExecJs(
      root,
      "var x; var y; var mX; var mY; document.addEventListener('mousemove', "
      "function(e) {x = e.x; y = e.y; mX = e.movementX; mY = e.movementY;});"));

  // Send a mouse move to root frame before lock.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseMove,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.pointer_type = blink::WebPointerProperties::PointerType::kMouse;
  mouse_event.SetPositionInWidget(6, 7);
  mouse_event.SetPositionInScreen(6, 7);
  mouse_event.movement_x = 8;
  mouse_event.movement_y = 9;
  router->RouteMouseEvent(root_view, &mouse_event, ui::LatencyInfo());

  // Make sure that the renderer handled the input event.
  MainThreadFrameObserver root_observer(root_view->GetRenderWidgetHost());
  root_observer.Wait();

  EXPECT_EQ("[6,7,0,0]", EvalJs(root, "JSON.stringify([x,y,mX,mY])"));

  // Request a pointer lock with unadjustedMovement.
  EXPECT_EQ(
      true,
      PointerLockHelper::RequestPointerLockWithUnadjustedMovementOnBody(root));

  // Root frame should have been granted pointer lock.
  EXPECT_EQ(true, PointerLockHelper::IsPointerLockOnBody(root));

  // Mouse is locked and unadjusted_movement is not set.
  EXPECT_TRUE(root->current_frame_host()->GetView()->IsPointerLocked());

  mouse_event.SetPositionInWidget(10, 10);
  mouse_event.SetPositionInScreen(10, 10);
  mouse_event.movement_x = 12;
  mouse_event.movement_y = 9;
  mouse_event.is_raw_movement_event = true;
  router->RouteMouseEvent(root_view, &mouse_event, ui::LatencyInfo());
  root_observer.Wait();

  // Raw movement events movement value from WebMouseEvent.movement_x/y.
  EXPECT_EQ("[6,7,12,9]", EvalJs(root, "JSON.stringify([x,y,mX,mY])"));

  mouse_event.SetPositionInWidget(20, 21);
  mouse_event.SetPositionInScreen(20, 21);
  mouse_event.movement_x = 1;
  mouse_event.movement_y = 2;
  mouse_event.is_raw_movement_event = false;
  router->RouteMouseEvent(root_view, &mouse_event, ui::LatencyInfo());
  root_observer.Wait();

  // Non-raw movement events movement value from screen pos - last screen pos.
  EXPECT_EQ("[6,7,10,11]", EvalJs(root, "JSON.stringify([x,y,mX,mY])"));
}
#endif

#if defined(USE_AURA)
// TODO(crbug.com/40635377): Remove failure test when fully implemented
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_ChangeUnadjustedMovementFailure \
  DISABLED_ChangeUnadjustedMovementFailure
#else
#define MAYBE_ChangeUnadjustedMovementFailure ChangeUnadjustedMovementFailure
#endif
// Tests that a subsequent request to RequestPointerLock with different
// options inside a Child view gets piped to the proper places and gives
// the proper unsupported error(this option is only supported on Windows
// This was prompted by this bug: https://crbug.com/1062702
IN_PROC_BROWSER_TEST_F(PointerLockBrowserTest,
                       MAYBE_ChangeUnadjustedMovementFailure) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);
  RenderWidgetHostViewBase* child_view = static_cast<RenderWidgetHostViewBase*>(
      child->current_frame_host()->GetView());

  WaitForHitTestData(child->current_frame_host());

  // Request a pointer lock on the child frame's body and wait for the promise
  // to resolve.
  EXPECT_EQ(true, PointerLockHelper::RequestPointerLockOnBody(child));
  // Child frame should have been granted pointer lock.
  EXPECT_EQ(true, PointerLockHelper::IsPointerLockOnBody(child));

  EXPECT_TRUE(child_view->IsPointerLocked());
  EXPECT_FALSE(root->current_frame_host()
                   ->GetView()
                   ->GetIsPointerLockedUnadjustedMovementForTesting());
  EXPECT_EQ(child_view->host(), web_contents()->GetPointerLockWidget());

  // Request to change pointer lock options and wait for return.
  EXPECT_EQ(
      "a JavaScript error: \"NotSupportedError: The options asked for in this "
      "request are not supported on this platform.\"\n",
      EvalJs(child,
             "document.body.requestPointerLock({unadjustedMovement:true})")
          .error);

  // The change errored out but the original lock should still be in place.
  EXPECT_TRUE(child_view->IsPointerLocked());
  EXPECT_FALSE(root->current_frame_host()
                   ->GetView()
                   ->GetIsPointerLockedUnadjustedMovementForTesting());
  EXPECT_EQ(child_view->host(), web_contents()->GetPointerLockWidget());
}
#endif

#if defined(USE_AURA)
#if BUILDFLAG(IS_WIN)
// Tests that a subsequent request to RequestPointerLock with different
// options inside a Child view gets piped to the proper places and updates
// the option(this option is only supported on Windows).
// This was prompted by this bug: https://crbug.com/1062702
IN_PROC_BROWSER_TEST_F(PointerLockBrowserTest,
                       ChangeUnadjustedMovementSuccess) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);
  RenderWidgetHostViewBase* child_view = static_cast<RenderWidgetHostViewBase*>(
      child->current_frame_host()->GetView());

  WaitForHitTestData(child->current_frame_host());

  // Request a pointer lock on the child frame's body and wait for the promise
  // to resolve.
  EXPECT_EQ(true, PointerLockHelper::RequestPointerLockOnBody(child));
  // Child frame should have been granted pointer lock.
  EXPECT_EQ(true, PointerLockHelper::IsPointerLockOnBody(child));

  EXPECT_TRUE(child_view->IsPointerLocked());
  EXPECT_FALSE(root->current_frame_host()
                   ->GetView()
                   ->GetIsPointerLockedUnadjustedMovementForTesting());
  EXPECT_EQ(child_view->host(), web_contents()->GetPointerLockWidget());

  // Request to change pointer lock options and wait for return.
  EXPECT_EQ(
      nullptr,
      EvalJs(child,
             "document.body.requestPointerLock({unadjustedMovement:true})"));

  // The new changed lock should now be in place.
  EXPECT_TRUE(child_view->IsPointerLocked());
  EXPECT_TRUE(root->current_frame_host()
                  ->GetView()
                  ->GetIsPointerLockedUnadjustedMovementForTesting());
  EXPECT_EQ(child_view->host(), web_contents()->GetPointerLockWidget());
}
#endif  // WIN_OS
#endif  // USE_AURA
}  // namespace content
