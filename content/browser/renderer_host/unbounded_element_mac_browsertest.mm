// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#import "base/apple/scoped_objc_class_swizzler.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "cc/base/features.h"
#include "components/input/cursor_manager.h"
#import "content/app_shim_remote_cocoa/render_widget_host_view_cocoa.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/browser/renderer_host/unbounded_surface_host_impl.h"
#include "content/browser/renderer_host/unbounded_surface_window.h"
#include "content/browser/renderer_host/unbounded_surface_window_mac.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/popup_test_utils.h"
#include "content/shell/browser/shell.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/test/cocoa_test_event_utils.h"
#include "ui/gfx/mac/coordinate_conversion.h"

// A mock window class that simulates the custom level-restoration behavior
// of Chromium's `NativeWidgetMacNSWindow`.
@interface MockNativeWidgetMacNSWindow : NSWindow
@end

@implementation MockNativeWidgetMacNSWindow
- (void)addChildWindow:(NSWindow*)childWin ordered:(NSWindowOrderingMode)place {
  NSInteger level = childWin.level;
  [super addChildWindow:childWin ordered:place];
  childWin.level = level;
}
@end

// A test window that counts how many mouse-down events AppKit delivers to
// `-sendEvent:`, allowing tests to verify that local `NSEvent` monitors return
// `event` rather than swallowing it with `nil`.
@interface EventRecordingTestWindow : NSWindow
@property(nonatomic, readonly) int sendEventCount;
@end

@implementation EventRecordingTestWindow {
  int _sendEventCount;
}

@synthesize sendEventCount = _sendEventCount;

- (void)sendEvent:(NSEvent*)event {
  if (event.type == NSEventTypeLeftMouseDown ||
      event.type == NSEventTypeRightMouseDown ||
      event.type == NSEventTypeOtherMouseDown) {
    ++_sendEventCount;
  }
  [super sendEvent:event];
}
@end

// A test view that accepts first responder so tests can simulate focus moving
// outside the owner view to other browser UI.
@interface FocusableTestView : NSView
@end

@implementation FocusableTestView
- (BOOL)acceptsFirstResponder {
  return YES;
}
@end

// A window whose key status tests control directly, since whether the test
// runner's application is active (and so whether any window is really key)
// varies between machines.
@interface KeyWindowOverridingTestWindow : NSWindow
@property(assign, nonatomic) BOOL keyWindowForTesting;
@end

@implementation KeyWindowOverridingTestWindow
@synthesize keyWindowForTesting = _keyWindowForTesting;

- (BOOL)isKeyWindow {
  return _keyWindowForTesting;
}
@end

// Supplies a fixed screen coordinate for +[NSEvent mouseLocation] when swizzled
// in tests that exercise stationary-pointer bounds updates.
@interface FakeMouseLocationDonor : NSObject
@end

@implementation FakeMouseLocationDonor
+ (NSPoint)mouseLocation {
  return gfx::ScreenPointToNSPoint(gfx::Point(200, 200));
}
@end

namespace content {

namespace {

// Stands in for the `UnboundedSurfaceHostImpl` of `window` while in scope, and
// counts dismissal requests. `Dismiss()` calls the delegate synchronously, so
// tests can check whether a single event dismissed the surface without
// pumping the message loop.
class ScopedDismissRecorder : public UnboundedSurfaceWindow::Delegate {
 public:
  ScopedDismissRecorder(UnboundedSurfaceWindow* window,
                        UnboundedSurfaceWindow::Delegate* original_delegate)
      : window_(window->GetWeakPtr()), original_delegate_(original_delegate) {
    window->set_delegate(this);
  }
  ScopedDismissRecorder(const ScopedDismissRecorder&) = delete;
  ScopedDismissRecorder& operator=(const ScopedDismissRecorder&) = delete;
  ~ScopedDismissRecorder() override {
    if (window_) {
      window_->set_delegate(original_delegate_);
    }
  }

  // Returns the number of dismissal requests since the last call.
  int TakeDismissCount() { return std::exchange(dismiss_count_, 0); }

  // UnboundedSurfaceWindow::Delegate:
  void OnDismissRequested() override { ++dismiss_count_; }
  void OnWindowDestroyed() override {
    if (original_delegate_) {
      original_delegate_->OnWindowDestroyed();
    }
  }

 private:
  base::WeakPtr<UnboundedSurfaceWindow> window_;
  raw_ptr<UnboundedSurfaceWindow::Delegate> original_delegate_;
  int dismiss_count_ = 0;
};

}  // namespace

class RenderWidgetHostViewMacUnboundedZOrderTest : public ContentBrowserTest {
 public:
  RenderWidgetHostViewMacUnboundedZOrderTest() {
    feature_list_.InitWithFeatures(
        {blink::features::kUnboundedElement,
         blink::features::kUnsafeUnboundedElementOnTheOpenWeb},
        {::features::kTreesInViz});
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacUnboundedZOrderTest,
                       UnboundedSurfaceZOrderLevels) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string script = R"(
    document.body.innerHTML = `
      <div id="target" unbounded></div>
    `;
    document.getElementById('target').showUnboundedElement();
  )";
  ASSERT_TRUE(ExecJs(shell()->web_contents(), script));

  RenderFrameHostImpl* rfhi = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return rfhi->GetUnboundedSurfaceWindow() != nullptr; }));

  UnboundedSurfaceWindow* unbounded_window = rfhi->GetUnboundedSurfaceWindow();
  ASSERT_TRUE(unbounded_window);

  NSWindow* child_nswindow =
      unbounded_window->GetNativeWindow().GetNativeNSWindow();
  ASSERT_TRUE(child_nswindow);

  RenderWidgetHostView* rwhv = rfhi->GetView();
  RenderWidgetHostViewMac* rwhv_mac =
      static_cast<RenderWidgetHostViewMac*>(rwhv);
  NSWindow* parent_nswindow = [rwhv_mac->GetInProcessNSView() window];
  ASSERT_TRUE(parent_nswindow);

  // Verify parent-child window linkage
  EXPECT_TRUE([[parent_nswindow childWindows] containsObject:child_nswindow]);

  // Verify child window level stacks strictly above the parent window level
  EXPECT_GT([child_nswindow level], [parent_nswindow level]);
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacUnboundedZOrderTest,
                       UnboundedSurfaceZOrderLevelsWithMockWrapperParent) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHostImpl* rfhi = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  RenderWidgetHostViewMac* rwhv_mac =
      static_cast<RenderWidgetHostViewMac*>(rfhi->GetView());
  NSView* main_view = rwhv_mac->GetInProcessNSView();
  NSWindow* original_window = [main_view window];
  ASSERT_TRUE(original_window);

  // Create a mock parent window that implements the addChildWindow
  // level-restoration wrapper
  MockNativeWidgetMacNSWindow* mock_parent =
      [[MockNativeWidgetMacNSWindow alloc]
          initWithContentRect:[original_window frame]
                    styleMask:NSWindowStyleMaskBorderless
                      backing:NSBackingStoreBuffered
                        defer:NO];
  mock_parent.releasedWhenClosed = NO;
  [mock_parent orderFront:nil];

  // Move the view to the mock parent window
  [mock_parent.contentView addSubview:main_view];
  EXPECT_EQ([main_view window], mock_parent);

  // Trigger unbounded element show
  std::string script = R"(
    document.body.innerHTML = `
      <div id="target" unbounded></div>
    `;
    document.getElementById('target').showUnboundedElement();
  )";
  ASSERT_TRUE(ExecJs(shell()->web_contents(), script));

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return rfhi->GetUnboundedSurfaceWindow() != nullptr; }));

  UnboundedSurfaceWindow* unbounded_window = rfhi->GetUnboundedSurfaceWindow();
  ASSERT_TRUE(unbounded_window);

  NSWindow* child_nswindow =
      unbounded_window->GetNativeWindow().GetNativeNSWindow();
  ASSERT_TRUE(child_nswindow);

  // Verify parent-child window linkage on mock parent
  EXPECT_TRUE([[mock_parent childWindows] containsObject:child_nswindow]);

  // Verify level stacks strictly above the mock parent window level
  EXPECT_GT([child_nswindow level], [mock_parent level]);

  // Clean up: sever parent-child linkage, move the view back, and close the
  // mock window
  [mock_parent removeChildWindow:child_nswindow];
  [original_window.contentView addSubview:main_view];
  [mock_parent close];
  mock_parent = nil;
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacUnboundedZOrderTest,
                       LocalClickEventMonitorDismissesOnOutsideWindowClick) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string script = R"(
    document.body.innerHTML = `
      <div id="target" style="width:50px; height:50px;" unbounded></div>
    `;
    document.getElementById('target').showUnboundedElement();
  )";
  ASSERT_TRUE(ExecJs(shell()->web_contents(), script));

  RenderFrameHostImpl* rfhi = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return rfhi->GetUnboundedSurfaceWindow() != nullptr; }));

  UnboundedSurfaceWindow* unbounded_window = rfhi->GetUnboundedSurfaceWindow();
  ASSERT_TRUE(unbounded_window);
  base::WeakPtr<UnboundedSurfaceWindow> weak_window =
      unbounded_window->GetWeakPtr();

  NSWindow* child_nswindow =
      unbounded_window->GetNativeWindow().GetNativeNSWindow();
  ASSERT_TRUE(child_nswindow);

  // Dispatching a mouse-down event targeting the unbounded window itself must
  // not trigger light-dismiss via the local event monitor.
  [NSApp sendEvent:cocoa_test_event_utils::LeftMouseDownAtPointInWindow(
                       NSMakePoint(10, 10), child_nswindow)];

  std::string get_style =
      "getComputedStyle(document.getElementById('target')).visibility";
  EXPECT_EQ("visible", EvalJs(shell()->web_contents(), get_style));
  EXPECT_TRUE(weak_window);

  // Create a separate application window (simulating non-web browser UI) so
  // the click does not route through RenderWidgetHostViewCocoa and is handled
  // solely by UnboundedSurfaceWindowMac's local NSEvent monitor.
  EventRecordingTestWindow* chrome_ui_window = [[EventRecordingTestWindow alloc]
      initWithContentRect:NSMakeRect(0, 0, 100, 100)
                styleMask:NSWindowStyleMaskBorderless
                  backing:NSBackingStoreBuffered
                    defer:NO];
  chrome_ui_window.releasedWhenClosed = NO;
  [chrome_ui_window orderFront:nil];

  [NSApp sendEvent:cocoa_test_event_utils::LeftMouseDownAtPointInWindow(
                       NSMakePoint(10, 10), chrome_ui_window)];
  EXPECT_EQ(1, [chrome_ui_window sendEventCount]);

  EXPECT_TRUE(base::test::RunUntil([&]() { return !weak_window; }));
  EXPECT_EQ("hidden", EvalJs(shell()->web_contents(), get_style));

  // Verify that a right mouse-down event outside the unbounded window also
  // passes through the local monitor and triggers dismissal.
  ASSERT_TRUE(
      ExecJs(shell()->web_contents(),
             "document.getElementById('target').showUnboundedElement();"));
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return rfhi->GetUnboundedSurfaceWindow() != nullptr; }));
  weak_window = rfhi->GetUnboundedSurfaceWindow()->GetWeakPtr();

  [NSApp sendEvent:cocoa_test_event_utils::RightMouseDownAtPointInWindow(
                       NSMakePoint(10, 10), chrome_ui_window)];
  EXPECT_EQ(2, [chrome_ui_window sendEventCount]);

  EXPECT_TRUE(base::test::RunUntil([&]() { return !weak_window; }));
  EXPECT_EQ("hidden", EvalJs(shell()->web_contents(), get_style));

  // Verify that an other mouse-down event outside the unbounded window also
  // passes through the local monitor and triggers dismissal.
  ASSERT_TRUE(
      ExecJs(shell()->web_contents(),
             "document.getElementById('target').showUnboundedElement();"));
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return rfhi->GetUnboundedSurfaceWindow() != nullptr; }));
  weak_window = rfhi->GetUnboundedSurfaceWindow()->GetWeakPtr();

  [NSApp sendEvent:cocoa_test_event_utils::MouseEventAtPointInWindow(
                       NSMakePoint(10, 10), NSEventTypeOtherMouseDown,
                       chrome_ui_window, 1)];
  EXPECT_EQ(3, [chrome_ui_window sendEventCount]);

  EXPECT_TRUE(base::test::RunUntil([&]() { return !weak_window; }));
  EXPECT_EQ("hidden", EvalJs(shell()->web_contents(), get_style));

  [chrome_ui_window close];
  chrome_ui_window = nil;
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacUnboundedZOrderTest,
                       ClickInsideChildPopupWindowDoesNotDismiss) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string script = R"(
    document.body.innerHTML = `
      <div id="target" style="width:100px; height:100px;" unbounded>
        <input id="d" type="date">
      </div>
    `;
    document.getElementById('target').showUnboundedElement();
  )";
  ASSERT_TRUE(ExecJs(shell()->web_contents(), script));

  RenderFrameHostImpl* rfhi = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return rfhi->GetUnboundedSurfaceWindow() != nullptr; }));

  UnboundedSurfaceWindow* unbounded_window = rfhi->GetUnboundedSurfaceWindow();
  ASSERT_TRUE(unbounded_window);
  base::WeakPtr<UnboundedSurfaceWindow> weak_window =
      unbounded_window->GetWeakPtr();
  ScopedDismissRecorder recorder(
      unbounded_window, UnboundedSurfaceHostImpl::GetForCurrentDocument(rfhi));

  WaitForHitTestData(rfhi);
  ShowPopupWidgetWaiter waiter(shell()->web_contents(), rfhi);
  ASSERT_TRUE(ExecJs(rfhi, "document.getElementById('d').showPicker();"));
  waiter.Wait();

  auto* parent_view_mac =
      static_cast<RenderWidgetHostViewMac*>(rfhi->GetView());
  ASSERT_TRUE(parent_view_mac);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return parent_view_mac->PopupChildHostView() != nullptr &&
           parent_view_mac->PopupChildHostView()->GetInProcessNSView() != nil;
  }));

  NSWindow* picker_window =
      [parent_view_mac->PopupChildHostView()->GetInProcessNSView() window];
  ASSERT_TRUE(picker_window);

  // Dispatch mouse down targeting the date picker popup window.
  [NSApp sendEvent:cocoa_test_event_utils::LeftMouseDownAtPointInWindow(
                       NSMakePoint(10, 10), picker_window)];

  // Wait for any asynchronous event handling and input to be processed.
  RunUntilInputProcessed(rfhi->GetRenderWidgetHost());

  // The unbounded surface must not be dismissed.
  EXPECT_EQ(0, recorder.TakeDismissCount());
  EXPECT_TRUE(weak_window);
  std::string get_style =
      "getComputedStyle(document.getElementById('target')).visibility";
  EXPECT_EQ("visible", EvalJs(shell()->web_contents(), get_style));
}

// A key-down while the owning `RenderWidgetHostViewCocoa` is not the first
// responder (e.g. a browser shortcut while focus is in the omnibox) dismisses
// the surface and still reaches its window. A key-down while the owning view
// is the first responder is left to `RenderWidgetHostImpl`.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacUnboundedZOrderTest,
                       LocalEventMonitorDismissesOnKeyDownOutsideOwnerView) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  ASSERT_TRUE(ExecJs(shell()->web_contents(), R"(
    document.body.innerHTML = `<div id="target" unbounded></div>`;
    document.getElementById('target').showUnboundedElement();
  )"));

  RenderFrameHostImpl* rfhi = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return rfhi->GetUnboundedSurfaceWindow() != nullptr; }));
  UnboundedSurfaceWindow* unbounded_window = rfhi->GetUnboundedSurfaceWindow();
  ASSERT_TRUE(unbounded_window);
  ScopedDismissRecorder recorder(
      unbounded_window, UnboundedSurfaceHostImpl::GetForCurrentDocument(rfhi));

  RenderWidgetHostViewMac* rwhv_mac =
      static_cast<RenderWidgetHostViewMac*>(rfhi->GetView());
  NSView* cocoa_view = rwhv_mac->GetInProcessNSView();
  NSWindow* parent_nswindow = [cocoa_view window];
  ASSERT_TRUE(parent_nswindow);

  // A key-down while the owning view is the first responder does not dismiss.
  ASSERT_TRUE([parent_nswindow makeFirstResponder:cocoa_view]);
  [NSApp sendEvent:cocoa_test_event_utils::SynthesizeKeyEvent(
                       parent_nswindow, /*keyDown=*/true, ui::VKEY_A, 0)];
  EXPECT_EQ(0, recorder.TakeDismissCount());

  // Editing shortcuts (such as Cmd+C) while the owning view is the first
  // responder do not dismiss the surface.
  [NSApp sendEvent:cocoa_test_event_utils::SynthesizeKeyEvent(
                       parent_nswindow, /*keyDown=*/true, ui::VKEY_C,
                       NSEventModifierFlagCommand)];
  EXPECT_EQ(0, recorder.TakeDismissCount());

  // A Command key browser shortcut while the owning view is the first responder
  // dismisses the surface before ChromeCommandDispatcherDelegate intercepts
  // it. Add a non-destructive menu item so the shortcut matches a main-menu
  // browser command without closing or unfocusing the window under test.
  NSMenuItem* test_menu_item =
      [[NSMenuItem alloc] initWithTitle:@"Test Command"
                                 action:@selector(description)
                          keyEquivalent:@"d"];
  test_menu_item.keyEquivalentModifierMask = NSEventModifierFlagCommand;
  NSMenu* file_menu = [[NSApp mainMenu] itemAtIndex:1].submenu
                          ? [[NSApp mainMenu] itemAtIndex:1].submenu
                          : [NSApp mainMenu];
  [file_menu addItem:test_menu_item];

  [NSApp sendEvent:cocoa_test_event_utils::SynthesizeKeyEvent(
                       parent_nswindow, /*keyDown=*/true, ui::VKEY_D,
                       NSEventModifierFlagCommand)];
  EXPECT_EQ(1, recorder.TakeDismissCount());
  [file_menu removeItem:test_menu_item];

  // A key-down while focus is in other browser UI in the same window (e.g. the
  // omnibox) dismisses the surface.
  FocusableTestView* other_view =
      [[FocusableTestView alloc] initWithFrame:NSMakeRect(0, 0, 50, 50)];
  [parent_nswindow.contentView addSubview:other_view];
  ASSERT_TRUE([parent_nswindow makeFirstResponder:other_view]);
  ASSERT_EQ(other_view, [parent_nswindow firstResponder]);
  // Blurring the owner view requests a dismissal of its own; discard it
  // so we can isolate the key event behavior.
  recorder.TakeDismissCount();

  [NSApp sendEvent:cocoa_test_event_utils::SynthesizeKeyEvent(
                       parent_nswindow, /*keyDown=*/true, ui::VKEY_A, 0)];
  EXPECT_EQ(1, recorder.TakeDismissCount());

  [other_view removeFromSuperview];
  other_view = nil;
}

// The owning window losing key status dismisses the surface, even when the
// owning `RenderWidgetHostViewCocoa` is not the first responder.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacUnboundedZOrderTest,
                       WindowResigningKeyDismissesSurface) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  ASSERT_TRUE(ExecJs(shell()->web_contents(), R"(
    document.body.innerHTML = `<div id="target" unbounded></div>`;
    document.getElementById('target').showUnboundedElement();
  )"));

  RenderFrameHostImpl* rfhi = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return rfhi->GetUnboundedSurfaceWindow() != nullptr; }));
  UnboundedSurfaceWindow* unbounded_window = rfhi->GetUnboundedSurfaceWindow();
  ASSERT_TRUE(unbounded_window);
  ScopedDismissRecorder recorder(
      unbounded_window, UnboundedSurfaceHostImpl::GetForCurrentDocument(rfhi));

  RenderWidgetHostViewMac* rwhv_mac =
      static_cast<RenderWidgetHostViewMac*>(rfhi->GetView());
  NSWindow* parent_nswindow = [rwhv_mac->GetInProcessNSView() window];
  ASSERT_TRUE(parent_nswindow);
  [parent_nswindow makeFirstResponder:nil];
  ASSERT_NE(rwhv_mac->GetInProcessNSView(), [parent_nswindow firstResponder]);
  // Blurring the owner view requests a dismissal of its own; discard it
  // so we can test the window key-change behavior in isolation.
  recorder.TakeDismissCount();

  // Becoming key keeps the surface open.
  rwhv_mac->OnWindowIsKeyChanged(true);
  EXPECT_EQ(0, recorder.TakeDismissCount());

  rwhv_mac->OnWindowIsKeyChanged(false);
  EXPECT_EQ(1, recorder.TakeDismissCount());
}

// A native mouseMoved: event on the surface's NSView routes through
// UnboundedSurfaceWindowMac::RouteMouseEvent(NSEvent*) to
// UnboundedSurfaceWindow::RouteMouseEvent(const blink::WebMouseEvent&) and
// updates the CursorManager's view under the cursor.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacUnboundedZOrderTest,
                       NativeMouseMovedUpdatesViewUnderCursor) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  ASSERT_TRUE(ExecJs(shell()->web_contents(), R"(
    document.body.innerHTML =
        `<div id="target" style="width:50px; height:50px;" unbounded></div>`;
    document.getElementById('target').showUnboundedElement();
  )"));

  RenderFrameHostImpl* rfhi = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return rfhi->GetUnboundedSurfaceWindow() != nullptr; }));

  UnboundedSurfaceWindow* unbounded_window = rfhi->GetUnboundedSurfaceWindow();
  ASSERT_TRUE(unbounded_window);
  NSWindow* child_nswindow =
      unbounded_window->GetNativeWindow().GetNativeNSWindow();
  ASSERT_TRUE(child_nswindow);

  RenderWidgetHostViewMac* rwhv_mac =
      static_cast<RenderWidgetHostViewMac*>(rfhi->GetView());
  input::CursorManager* cursor_manager = rwhv_mac->GetCursorManager();
  ASSERT_TRUE(cursor_manager);

  cursor_manager->UpdateViewUnderCursor(nullptr);
  ASSERT_FALSE(cursor_manager->IsViewUnderCursor(rwhv_mac));

  [child_nswindow.contentView
      mouseMoved:cocoa_test_event_utils::MouseEventAtPointInWindow(
                     NSMakePoint(10, 10), NSEventTypeMouseMoved, child_nswindow,
                     0)];
  EXPECT_TRUE(cursor_manager->IsViewUnderCursor(rwhv_mac));

  [child_nswindow.contentView
      mouseExited:cocoa_test_event_utils::ExitEvent(NSMakePoint(10, 10),
                                                    child_nswindow)];
  EXPECT_FALSE(cursor_manager->IsViewUnderCursor(rwhv_mac));
}

// Cursor handling over the surface window, which sets the renderer cursor
// itself (see UnboundedSurfaceWindowMac::DisplayCursor()). -[NSCursor
// currentCursor] is the top of the application's cursor stack, which makes it
// a usable oracle without the application being active. Mouse events are
// delivered to the surface view directly rather than through AppKit tracking
// areas, which are not exercised by synthesized events. The re-apply on mouse
// move compares against the cursor shown on screen, so the assertions use
// cursors a developer's terminal is unlikely to show while running the tests
// locally (never the I-beam).
class RenderWidgetHostViewMacUnboundedCursorTest
    : public RenderWidgetHostViewMacUnboundedZOrderTest {
 protected:
  // Navigates to a test page and reparents the web contents view into a
  // KeyWindowOverridingTestWindow (which starts with `isKeyWindow == YES`) so
  // tests can deterministically control whether the owning window is key.
  void SetUpOnMainThread() override {
    RenderWidgetHostViewMacUnboundedZOrderTest::SetUpOnMainThread();
    GURL url(embedded_test_server()->GetURL("/title1.html"));
    ASSERT_TRUE(NavigateToURL(shell(), url));
    rfhi_ = static_cast<RenderFrameHostImpl*>(
        shell()->web_contents()->GetPrimaryMainFrame());
    original_window_ = [web_contents_view() window];
    ASSERT_TRUE(original_window_);
    [original_window_ disableCursorRects];
    [original_window_ setAcceptsMouseMovedEvents:NO];

    parent_window_ = [[KeyWindowOverridingTestWindow alloc]
        initWithContentRect:[original_window_ frame]
                  styleMask:NSWindowStyleMaskBorderless
                    backing:NSBackingStoreBuffered
                      defer:NO];
    parent_window_.releasedWhenClosed = NO;
    parent_window_.keyWindowForTesting = YES;
    // Prevent -[RenderWidgetHostViewCocoa updateCursor:]'s
    // invalidateCursorRectsForView: call from scheduling AppKit's cursor-rect
    // run-loop observer, which would otherwise reset [NSCursor currentCursor]
    // based on the real OS pointer location when the run loop sleeps.
    [parent_window_ disableCursorRects];
    [parent_window_ setAcceptsMouseMovedEvents:NO];
    [parent_window_ orderFront:nil];
    [parent_window_.contentView addSubview:web_contents_view()];
    ASSERT_EQ(parent_window_, [web_contents_view() window]);
    for (CrTrackingArea* area in [web_contents_view().trackingAreas copy]) {
      [area clearOwner];
      [web_contents_view() removeTrackingArea:area];
    }
  }

  void TearDownOnMainThread() override {
    if (surface_window_ && [surface_window_ parentWindow] == parent_window_) {
      [parent_window_ removeChildWindow:surface_window_];
    }
    if (original_window_ && rfhi_) {
      [original_window_.contentView addSubview:web_contents_view()];
    }
    [parent_window_ close];
    parent_window_ = nil;
    surface_window_ = nil;
    original_window_ = nil;
    rfhi_ = nullptr;
    RenderWidgetHostViewMacUnboundedZOrderTest::TearDownOnMainThread();
  }

  // Shows an unbounded element and waits for its surface window to be sized to
  // the element's 100x100 layout bounds (the initial RequestUnboundedSurface
  // IPC sends 1x1 before the element matches :unbounded and has a layout box).
  void ShowSurface() {
    ASSERT_TRUE(ExecJs(shell()->web_contents(), R"(
      document.body.innerHTML = `
        <div id="target" style="width:100px; height:100px;" unbounded></div>
      `;
      window.addEventListener('contextmenu', e => e.preventDefault());
      document.getElementById('target').showUnboundedElement();
    )"));
    ASSERT_TRUE(base::test::RunUntil([&]() {
      UnboundedSurfaceWindow* window = rfhi_->GetUnboundedSurfaceWindow();
      return window && window->GetBounds().size() == gfx::Size(100, 100);
    }));
    surface_window_ = rfhi_->GetUnboundedSurfaceWindow()
                          ->GetNativeWindow()
                          .GetNativeNSWindow();
    ASSERT_TRUE(surface_window_);
    ASSERT_EQ(parent_window_, [surface_window_ parentWindow]);
    [surface_window_ disableCursorRects];
    [surface_window_ setAcceptsMouseMovedEvents:NO];
    // Stop tracking the real OS pointer and reset containment in case AppKit
    // already delivered an enter while ordering the surface window in front,
    // so only events sent by the test itself affect cursor state. Calling
    // -clearOwner on each CrTrackingArea also blocks any enter/exit events
    // that AppKit already queued before the tracking area was removed.
    for (CrTrackingArea* area in [surface_view().trackingAreas copy]) {
      [area clearOwner];
      [surface_view() removeTrackingArea:area];
    }
    SendToSurfaceView(NSEventTypeMouseExited);
    RunUntilInputProcessed(rfhi_->GetRenderWidgetHost());
  }

  // Destroys the unbounded surface window synchronously so no message loop
  // turns run between setting up the cursor state and checking the teardown
  // handoff.
  void HideSurface() {
    rwhv_mac()->DestroyUnboundedSurface(
        rfhi_->GetUnboundedSurfaceWindow()->GetWeakPtr());
    ASSERT_EQ(nullptr, rfhi_->GetUnboundedSurfaceWindow());
    surface_window_ = nil;
  }

  RenderWidgetHostViewMac* rwhv_mac() {
    return static_cast<RenderWidgetHostViewMac*>(rfhi_->GetView());
  }

  NSView* web_contents_view() { return rwhv_mac()->GetInProcessNSView(); }

  NSView* surface_view() { return [surface_window_ contentView]; }

  // Delivers a mouse event of `type` at a point inside the surface view the
  // way AppKit would for a pointer inside its tracking area.
  void SendToSurfaceView(NSEventType type) {
    const NSPoint point = NSMakePoint(10, 10);
    switch (type) {
      case NSEventTypeMouseEntered:
        [surface_view() mouseEntered:cocoa_test_event_utils::EnterEvent(
                                         point, surface_window_)];
        break;
      case NSEventTypeMouseExited:
        // Use a point outside the 100x100 surface window (but inside the web
        // contents viewport) so Blink's last known mouse position after the
        // leave is outside #target.
        [surface_view()
            mouseExited:cocoa_test_event_utils::ExitEvent(
                            NSMakePoint(200, -200), surface_window_)];
        break;
      case NSEventTypeMouseMoved:
        [surface_view()
            mouseMoved:cocoa_test_event_utils::MouseEventAtPointInWindow(
                           point, type, surface_window_, 0)];
        break;
      case NSEventTypeLeftMouseDown:
        [surface_view()
            mouseDown:cocoa_test_event_utils::MouseEventAtPointInWindow(
                          point, type, surface_window_, 1)];
        break;
      case NSEventTypeLeftMouseUp:
        [surface_view()
            mouseUp:cocoa_test_event_utils::MouseEventAtPointInWindow(
                        point, type, surface_window_, 1)];
        break;
      case NSEventTypeRightMouseDown:
        [surface_view()
            rightMouseDown:cocoa_test_event_utils::MouseEventAtPointInWindow(
                               point, type, surface_window_, 1)];
        break;
      case NSEventTypeRightMouseUp:
        [surface_view()
            rightMouseUp:cocoa_test_event_utils::MouseEventAtPointInWindow(
                             point, type, surface_window_, 1)];
        break;
      case NSEventTypeOtherMouseDown:
        [surface_view()
            otherMouseDown:cocoa_test_event_utils::MouseEventAtPointInWindow(
                               point, type, surface_window_, 1)];
        break;
      case NSEventTypeOtherMouseUp:
        [surface_view()
            otherMouseUp:cocoa_test_event_utils::MouseEventAtPointInWindow(
                             point, type, surface_window_, 1)];
        break;
      case NSEventTypeLeftMouseDragged:
        [surface_view()
            mouseDragged:cocoa_test_event_utils::MouseEventAtPointInWindow(
                             point, type, surface_window_, 0)];
        break;
      case NSEventTypeRightMouseDragged:
        [surface_view()
            rightMouseDragged:cocoa_test_event_utils::MouseEventAtPointInWindow(
                                  point, type, surface_window_, 0)];
        break;
      default:
        NOTREACHED();
    }
  }

  raw_ptr<RenderFrameHostImpl> rfhi_ = nullptr;
  NSWindow* __strong original_window_ = nil;
  KeyWindowOverridingTestWindow* __strong parent_window_ = nil;
  NSWindow* __strong surface_window_ = nil;
};

// A surface created while a non-default cursor is displayed shows that cursor
// as soon as the pointer enters it, even though Blink does not resend an
// unchanged cursor.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacUnboundedCursorTest,
                       SeededCursorAppliedOnEnter) {
  rwhv_mac()->UpdateCursor(ui::Cursor(ui::mojom::CursorType::kHand));
  ASSERT_NO_FATAL_FAILURE(ShowSurface());

  [NSCursor.arrowCursor set];
  SendToSurfaceView(NSEventTypeMouseEntered);
  EXPECT_EQ(NSCursor.pointingHandCursor, [NSCursor currentCursor]);
}

// AppKit delivers the first mouse move without a mouse enter when the surface
// appears under a stationary pointer, so a move alone counts as entry.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacUnboundedCursorTest,
                       MouseMovedWithoutEnterAppliesCursor) {
  ASSERT_NO_FATAL_FAILURE(ShowSurface());
  rwhv_mac()->UpdateCursor(ui::Cursor(ui::mojom::CursorType::kHand));
  [NSCursor.arrowCursor set];

  SendToSurfaceView(NSEventTypeMouseMoved);
  EXPECT_EQ(NSCursor.pointingHandCursor, [NSCursor currentCursor]);
}

// Mouse presses also count as entry when the surface appears under a
// stationary pointer without a prior mouse enter, while a right-mouse drag
// after exiting the surface does not flip containment back to inside.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacUnboundedCursorTest,
                       MouseDownWithoutEnterAppliesCursor) {
  ASSERT_NO_FATAL_FAILURE(ShowSurface());
  rwhv_mac()->UpdateCursor(ui::Cursor(ui::mojom::CursorType::kHand));

  const std::pair<NSEventType, NSEventType> press_and_release_types[] = {
      {NSEventTypeLeftMouseDown, NSEventTypeLeftMouseUp},
      {NSEventTypeRightMouseDown, NSEventTypeRightMouseUp},
      {NSEventTypeOtherMouseDown, NSEventTypeOtherMouseUp},
  };
  for (const auto& [press_type, release_type] : press_and_release_types) {
    SendToSurfaceView(NSEventTypeMouseExited);
    [NSCursor.arrowCursor set];
    SendToSurfaceView(press_type);
    EXPECT_EQ(NSCursor.pointingHandCursor, [NSCursor currentCursor]);
    SendToSurfaceView(release_type);
  }

  // A right-mouse drag after NSEventTypeMouseExited must not mark the pointer
  // inside again or re-apply the surface cursor while outside.
  SendToSurfaceView(NSEventTypeMouseExited);
  EXPECT_EQ(NSCursor.arrowCursor, [NSCursor currentCursor]);
  SendToSurfaceView(NSEventTypeRightMouseDragged);
  EXPECT_EQ(NSCursor.arrowCursor, [NSCursor currentCursor]);
}

// Exercises the ordering between -[UnboundedNSView trackCursorForMouseEvent:]
// and UnboundedSurfaceWindow::RouteMouseEvent when CursorManager switches
// between another view under the cursor and the surface's target view, as well
// as replacing an open surface in CreateUnboundedSurface while inside.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacUnboundedCursorTest,
                       SubframeSurfaceCursorAndReplacement) {
  ASSERT_NO_FATAL_FAILURE(ShowSurface());
  Shell* other_shell = CreateBrowser();
  auto* other_view = static_cast<RenderWidgetHostViewBase*>(
      other_shell->web_contents()->GetPrimaryMainFrame()->GetView());
  input::CursorManager* cursor_manager = rwhv_mac()->GetCursorManager();
  ASSERT_TRUE(cursor_manager);

  cursor_manager->UpdateCursor(other_view,
                               ui::Cursor(ui::mojom::CursorType::kHand));
  cursor_manager->UpdateViewUnderCursor(other_view);
  rwhv_mac()->UpdateCursor(ui::Cursor(ui::mojom::CursorType::kCross));
  [NSCursor.arrowCursor set];

  // Moving over the surface sets _pointerInside = YES before RouteMouseEvent
  // sets the CursorManager override to rwhv_mac(), so DisplayCursor(kCross)
  // applies crosshairCursor immediately.
  SendToSurfaceView(NSEventTypeMouseMoved);
  EXPECT_EQ(NSCursor.crosshairCursor, [NSCursor currentCursor]);

  // Exiting the surface sets _pointerInside = NO and resets arrowCursor before
  // ClearViewUnderCursorOverride() restores other_view's kHand cursor.
  SendToSurfaceView(NSEventTypeMouseExited);
  EXPECT_EQ(NSCursor.arrowCursor, [NSCursor currentCursor]);

  // Re-enter so the override is active and _pointerInside is YES, then replace
  // the surface via CreateUnboundedSurface: destroying the old surface first
  // resets arrowCursor and restores other_view's kHand cursor so the new
  // surface is seeded with pointingHandCursor.
  SendToSurfaceView(NSEventTypeMouseMoved);
  ASSERT_EQ(NSCursor.crosshairCursor, [NSCursor currentCursor]);
  rwhv_mac()->CreateUnboundedSurface(gfx::Rect(0, 0, 100, 100),
                                     other_view->GetWeakPtr());
  EXPECT_EQ(NSCursor.arrowCursor, [NSCursor currentCursor]);

  surface_window_ =
      rfhi_->GetUnboundedSurfaceWindow()->GetNativeWindow().GetNativeNSWindow();
  ASSERT_TRUE(surface_window_);
  for (CrTrackingArea* area in [surface_view().trackingAreas copy]) {
    [area clearOwner];
    [surface_view() removeTrackingArea:area];
  }
  SendToSurfaceView(NSEventTypeMouseEntered);
  EXPECT_EQ(NSCursor.pointingHandCursor, [NSCursor currentCursor]);

  cursor_manager->ViewBeingDestroyed(other_view);
  other_shell->Close();
}

// A cursor update while the pointer is over the surface is applied at once,
// and a cursor reset behind our back is undone on the next mouse move or
// drag.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacUnboundedCursorTest,
                       CursorUpdateWhileInsideAndReapplyOnMove) {
  ASSERT_NO_FATAL_FAILURE(ShowSurface());
  SendToSurfaceView(NSEventTypeMouseEntered);

  rwhv_mac()->UpdateCursor(ui::Cursor(ui::mojom::CursorType::kCross));
  EXPECT_EQ(NSCursor.crosshairCursor, [NSCursor currentCursor]);

  [NSCursor.arrowCursor set];
  EXPECT_EQ(NSCursor.arrowCursor, [NSCursor currentCursor]);
  SendToSurfaceView(NSEventTypeMouseMoved);
  EXPECT_EQ(NSCursor.crosshairCursor, [NSCursor currentCursor]);

  [NSCursor.arrowCursor set];
  EXPECT_EQ(NSCursor.arrowCursor, [NSCursor currentCursor]);
  SendToSurfaceView(NSEventTypeLeftMouseDragged);
  EXPECT_EQ(NSCursor.crosshairCursor, [NSCursor currentCursor]);

  rwhv_mac()->UpdateCursor(ui::Cursor(ui::mojom::CursorType::kPointer));
  EXPECT_EQ(NSCursor.arrowCursor, [NSCursor currentCursor]);
  [NSCursor.crosshairCursor set];
  EXPECT_EQ(NSCursor.crosshairCursor, [NSCursor currentCursor]);
  SendToSurfaceView(NSEventTypeMouseMoved);
  EXPECT_EQ(NSCursor.arrowCursor, [NSCursor currentCursor]);
}

// When the pointer crosses from the web contents into the surface, AppKit may
// deliver the exit from RenderWidgetHostViewCocoa (which resets the cursor to
// the arrow) after the entry into the surface. The next mouse move over the
// surface puts the renderer cursor back.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacUnboundedCursorTest,
                       ParentExitAfterSurfaceEnterIsAbsorbedOnMove) {
  ASSERT_NO_FATAL_FAILURE(ShowSurface());
  SendToSurfaceView(NSEventTypeMouseEntered);
  rwhv_mac()->UpdateCursor(ui::Cursor(ui::mojom::CursorType::kCross));
  ASSERT_EQ(NSCursor.crosshairCursor, [NSCursor currentCursor]);

  // ContentBrowserTest sets SetIgnoreNativePlatformEvents(true), which makes
  // RenderWidgetHostViewCocoa drop mouse events early. Re-enable native
  // platform event handling while delivering the synthesized exit to it.
  ui::PlatformEventSource::SetIgnoreNativePlatformEvents(false);
  [web_contents_view() mouseExited:cocoa_test_event_utils::ExitEvent(
                                       NSMakePoint(10, 10), parent_window_)];
  ui::PlatformEventSource::SetIgnoreNativePlatformEvents(true);
  EXPECT_EQ(NSCursor.arrowCursor, [NSCursor currentCursor]);

  SendToSurfaceView(NSEventTypeMouseMoved);
  EXPECT_EQ(NSCursor.crosshairCursor, [NSCursor currentCursor]);
}

// Leaving the surface restores the default cursor; the view the pointer moves
// to is responsible for its own cursor. Cursor updates received while outside
// are remembered and applied on the next entry.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacUnboundedCursorTest,
                       ExitRestoresArrow) {
  ASSERT_NO_FATAL_FAILURE(ShowSurface());
  SendToSurfaceView(NSEventTypeMouseEntered);
  rwhv_mac()->UpdateCursor(ui::Cursor(ui::mojom::CursorType::kCross));
  ASSERT_EQ(NSCursor.crosshairCursor, [NSCursor currentCursor]);

  SendToSurfaceView(NSEventTypeMouseExited);
  EXPECT_EQ(NSCursor.arrowCursor, [NSCursor currentCursor]);

  // Calling DisplayCursor on the surface window directly while the pointer is
  // outside (even with the parent window key) remembers the cursor without
  // changing [NSCursor currentCursor] until the pointer re-enters.
  rfhi_->GetUnboundedSurfaceWindow()->DisplayCursor(
      ui::Cursor(ui::mojom::CursorType::kHand));
  EXPECT_EQ(NSCursor.arrowCursor, [NSCursor currentCursor]);
  SendToSurfaceView(NSEventTypeMouseEntered);
  EXPECT_EQ(NSCursor.pointingHandCursor, [NSCursor currentCursor]);
}

// The cursor is only changed while the owning window is key and no context
// menu is active, like RenderWidgetHostViewCocoa does for the web contents.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacUnboundedCursorTest,
                       NoCursorChangeWhileParentNotKey) {
  ASSERT_NO_FATAL_FAILURE(ShowSurface());
  SendToSurfaceView(NSEventTypeMouseEntered);
  [NSCursor.arrowCursor set];

  parent_window_.keyWindowForTesting = NO;
  rwhv_mac()->UpdateCursor(ui::Cursor(ui::mojom::CursorType::kCross));
  EXPECT_EQ(NSCursor.arrowCursor, [NSCursor currentCursor]);
  SendToSurfaceView(NSEventTypeMouseMoved);
  EXPECT_EQ(NSCursor.arrowCursor, [NSCursor currentCursor]);
  SendToSurfaceView(NSEventTypeMouseExited);
  SendToSurfaceView(NSEventTypeMouseEntered);
  EXPECT_EQ(NSCursor.arrowCursor, [NSCursor currentCursor]);

  // The pending cursor is applied on the next mouse move once the window is
  // key again, unless a context menu is open on the owning view.
  parent_window_.keyWindowForTesting = YES;
  [rwhv_mac()->GetInProcessNSView() setShowingContextMenu:YES];
  SendToSurfaceView(NSEventTypeMouseMoved);
  EXPECT_EQ(NSCursor.arrowCursor, [NSCursor currentCursor]);
  [rwhv_mac()->GetInProcessNSView() setShowingContextMenu:NO];
  SendToSurfaceView(NSEventTypeMouseMoved);
  EXPECT_EQ(NSCursor.crosshairCursor, [NSCursor currentCursor]);

  // Exiting or dismissing while the owning window is not key does not reset
  // the cursor to the arrow over another window.
  parent_window_.keyWindowForTesting = NO;
  SendToSurfaceView(NSEventTypeMouseExited);
  EXPECT_EQ(NSCursor.crosshairCursor, [NSCursor currentCursor]);
  SendToSurfaceView(NSEventTypeMouseEntered);
  ASSERT_NO_FATAL_FAILURE(HideSurface());
  EXPECT_EQ(NSCursor.crosshairCursor, [NSCursor currentCursor]);
}

// Moving or resizing the surface via SetBounds() under a stationary pointer
// updates pointer containment from NSEvent.mouseLocation so the cursor updates
// without waiting for a mouse move.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacUnboundedCursorTest,
                       SetBoundsUpdatesCursorUnderStationaryPointer) {
  base::apple::ScopedObjCClassSwizzler mouse_location_swizzler(
      [NSEvent class], [FakeMouseLocationDonor class],
      @selector(mouseLocation));
  ASSERT_NO_FATAL_FAILURE(ShowSurface());
  rwhv_mac()->UpdateCursor(ui::Cursor(ui::mojom::CursorType::kCross));
  [NSCursor.arrowCursor set];

  const gfx::Point mouse_in_screen(200, 200);
  UnboundedSurfaceWindow* window = rfhi_->GetUnboundedSurfaceWindow();
  ASSERT_TRUE(window);

  // Move the surface so it covers the stationary OS pointer.
  window->SetBounds(
      gfx::Rect(mouse_in_screen.x() - 10, mouse_in_screen.y() - 10, 50, 50));
  EXPECT_EQ(NSCursor.crosshairCursor, [NSCursor currentCursor]);

  // Move the surface away from the stationary OS pointer.
  window->SetBounds(
      gfx::Rect(mouse_in_screen.x() + 500, mouse_in_screen.y() + 500, 50, 50));
  EXPECT_EQ(NSCursor.arrowCursor, [NSCursor currentCursor]);
}

// Dismissing the surface while the pointer is over it restores the default
// cursor, since the window disappears without a mouse exit.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacUnboundedCursorTest,
                       DismissWhileInsideRestoresArrow) {
  ASSERT_NO_FATAL_FAILURE(ShowSurface());
  SendToSurfaceView(NSEventTypeMouseEntered);
  rwhv_mac()->UpdateCursor(ui::Cursor(ui::mojom::CursorType::kCross));
  ASSERT_EQ(NSCursor.crosshairCursor, [NSCursor currentCursor]);

  ASSERT_NO_FATAL_FAILURE(HideSurface());
  EXPECT_EQ(NSCursor.arrowCursor, [NSCursor currentCursor]);
}

// Dismissing the surface after the pointer left it leaves the cursor alone,
// since whatever the pointer is over now owns it.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacUnboundedCursorTest,
                       DismissWhileOutsideLeavesCursorAlone) {
  ASSERT_NO_FATAL_FAILURE(ShowSurface());
  SendToSurfaceView(NSEventTypeMouseEntered);
  SendToSurfaceView(NSEventTypeMouseExited);
  ASSERT_EQ(NSCursor.arrowCursor, [NSCursor currentCursor]);

  parent_window_.keyWindowForTesting = NO;
  [NSCursor.crosshairCursor set];
  ASSERT_NO_FATAL_FAILURE(HideSurface());
  EXPECT_EQ(NSCursor.crosshairCursor, [NSCursor currentCursor]);
}

// Entering the surface disables cursor rects on the parent window so
// -[RenderWidgetHostViewCocoa updateCursor:]'s invalidateCursorRectsForView:
// does not reset the cursor while over parts of the surface outside the web
// contents, and exiting or dismissing the surface re-enables them.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacUnboundedCursorTest,
                       ParentWindowCursorRectsBalancedOnExitAndDismiss) {
  ASSERT_NO_FATAL_FAILURE(ShowSurface());
  [parent_window_ enableCursorRects];
  ASSERT_TRUE([parent_window_ areCursorRectsEnabled]);

  SendToSurfaceView(NSEventTypeMouseEntered);
  EXPECT_FALSE([parent_window_ areCursorRectsEnabled]);

  SendToSurfaceView(NSEventTypeMouseExited);
  EXPECT_TRUE([parent_window_ areCursorRectsEnabled]);

  SendToSurfaceView(NSEventTypeMouseEntered);
  EXPECT_FALSE([parent_window_ areCursorRectsEnabled]);

  ASSERT_NO_FATAL_FAILURE(HideSurface());
  EXPECT_TRUE([parent_window_ areCursorRectsEnabled]);
}

// End-to-end renderer cursor update: hovering an unbounded element with a CSS
// cursor through UnboundedNSView displays that cursor via NSCursor.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacUnboundedCursorTest,
                       RendererCursorFromHoveredElementAppliedOnMove) {
  ASSERT_NO_FATAL_FAILURE(ShowSurface());
  ASSERT_TRUE(ExecJs(shell()->web_contents(),
                     "document.getElementById('target').style.cursor = "
                     "'crosshair';"));
  [NSCursor.arrowCursor set];

  ASSERT_TRUE(base::test::RunUntil([&]() {
    SendToSurfaceView(NSEventTypeMouseMoved);
    return [NSCursor currentCursor] == NSCursor.crosshairCursor;
  }));
}

}  // namespace content
