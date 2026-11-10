// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "cc/base/features.h"
#import "content/app_shim_remote_cocoa/render_widget_host_view_cocoa.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/browser/renderer_host/unbounded_surface_host_impl.h"
#include "content/browser/renderer_host/unbounded_surface_window.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/shell/browser/shell.h"
#include "third_party/blink/public/common/features.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/cocoa_test_event_utils.h"

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

}  // namespace content
