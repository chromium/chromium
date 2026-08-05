// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/functional/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "cc/base/features.h"
#import "content/app_shim_remote_cocoa/render_widget_host_view_cocoa.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/browser/renderer_host/unbounded_surface_window.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "third_party/blink/public/common/features.h"

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

namespace content {

class RenderWidgetHostViewMacUnboundedZOrderTest : public ContentBrowserTest {
 public:
  RenderWidgetHostViewMacUnboundedZOrderTest() {
    feature_list_.InitWithFeatures(
        {blink::features::kUnboundedElement,
         blink::features::kUnboundedElementOnTheOpenWeb},
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
      <div id="target" style="width:50px; height:50px;" unbounded></div>
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

}  // namespace content
