// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/scoped_nsobject.h"
#include "base/test/scoped_feature_list.h"
#import "content/app_shim_remote_cocoa/web_contents_occlusion_checker_mac.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"

using remote_cocoa::mojom::DraggingInfo;
using remote_cocoa::mojom::DraggingInfoPtr;
using remote_cocoa::mojom::SelectionDirection;
using content::DropData;

namespace {
const base::Feature kMacWebContentsOcclusion{"MacWebContentsOcclusion",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
const char kEnhancedWindowOcclusionDetection[] =
    "EnhancedWindowOcclusionDetection";
const char kDisplaySleepAndAppHideDetection[] =
    "DisplaySleepAndAppHideDetection";

}  // namespace

// An NSWindow subclass that enables programmatic setting of macOS occlusion and
// miniaturize states.
@interface WebContentsHostWindowForOcclusionTesting : NSWindow {
  BOOL _miniaturizedForTesting;
}
@property(assign, nonatomic) BOOL occludedForTesting;
@end

@implementation WebContentsHostWindowForOcclusionTesting

@synthesize occludedForTesting = _occludedForTesting;

- (NSWindowOcclusionState)occlusionState {
  return _occludedForTesting ? 0 : NSWindowOcclusionStateVisible;
}

- (void)miniaturize:(id)sender {
  // Miniaturizing a window doesn't immediately take effect (isMiniaturized
  // returns false) so fake it with a flag and removal from window list.
  _miniaturizedForTesting = YES;
  [self orderOut:nil];
}

- (void)deminiaturize:(id)sender {
  _miniaturizedForTesting = NO;
  [self orderFront:nil];
}

- (BOOL)isMiniaturized {
  return _miniaturizedForTesting;
}

@end

namespace content {

// A stub class for WebContentsNSViewHost.
class WebContentsNSViewHostStub
    : public remote_cocoa::mojom::WebContentsNSViewHost {
 public:
  WebContentsNSViewHostStub() {}

  void OnMouseEvent(bool motion, bool exited) override {}

  void OnBecameFirstResponder(SelectionDirection direction) override {}

  void OnWindowVisibilityChanged(
      remote_cocoa::mojom::Visibility visibility) override {
    _visibility = visibility;
  }

  remote_cocoa::mojom::Visibility WebContentsVisibility() {
    return _visibility;
  }

  void SetDropData(const ::content::DropData& drop_data) override {}

  bool DraggingEntered(DraggingInfoPtr dragging_info,
                       uint32_t* out_result) override {
    return false;
  }

  void DraggingEntered(DraggingInfoPtr dragging_info,
                       DraggingEnteredCallback callback) override {}

  void DraggingExited() override {}

  void DraggingUpdated(DraggingInfoPtr dragging_info,
                       DraggingUpdatedCallback callback) override {}

  bool PerformDragOperation(DraggingInfoPtr dragging_info,
                            bool* out_result) override {
    return false;
  }

  void PerformDragOperation(DraggingInfoPtr dragging_info,
                            PerformDragOperationCallback callback) override {}

  bool DragPromisedFileTo(const ::base::FilePath& file_path,
                          const ::content::DropData& drop_data,
                          const ::GURL& download_url,
                          ::base::FilePath* out_file_path) override {
    return false;
  }

  void DragPromisedFileTo(const ::base::FilePath& file_path,
                          const ::content::DropData& drop_data,
                          const ::GURL& download_url,
                          DragPromisedFileToCallback callback) override {}

  void EndDrag(uint32_t drag_operation,
               const ::gfx::PointF& local_point,
               const ::gfx::PointF& screen_point) override {}

 private:
  remote_cocoa::mojom::Visibility _visibility;
};

// Sets up occlusion tests.
class WindowOcclusionBrowserTestMac : public ContentBrowserTest {
 public:
  // Creates |window_a| with a visible (i.e. unoccluded) WebContentsViewCocoa.
  void InitWindowA() {
    const NSRect kWindowAContentRect = NSMakeRect(0.0, 0.0, 80.0, 60.0);
    const NSWindowStyleMask kWindowStyleMask = NSWindowStyleMaskClosable;
    window_a.reset([[WebContentsHostWindowForOcclusionTesting alloc]
        initWithContentRect:kWindowAContentRect
                  styleMask:kWindowStyleMask
                    backing:NSBackingStoreBuffered
                      defer:YES]);
    NSRect window_frame = [NSWindow frameRectForContentRect:kWindowAContentRect
                                                  styleMask:kWindowStyleMask];
    window_frame.origin = NSMakePoint(20.0, 200.0);
    [window_a setFrame:window_frame display:NO];
    [window_a setTitle:@"window_a"];
    [window_a setReleasedWhenClosed:NO];

    const NSRect kWebContentsFrame = NSMakeRect(0.0, 0.0, 10.0, 10.0);
    window_a_web_contents_view_cocoa.reset(
        [[WebContentsViewCocoa alloc] initWithFrame:kWebContentsFrame]);
    [[window_a contentView] addSubview:window_a_web_contents_view_cocoa];

    // Set up a fake host so we can check the occlusion status.
    [window_a_web_contents_view_cocoa setHost:&_host_a];

    // Bring the browser window onscreen.
    OrderWindowFront(window_a);

    // Init visibility state.
    SetWindowAWebContentsVisibility(remote_cocoa::mojom::Visibility::kVisible);
  }

  void InitWindowB(NSRect window_frame = NSZeroRect) {
    const NSRect kWindowBContentRect = NSMakeRect(0.0, 0.0, 40.0, 40.0);
    const NSWindowStyleMask kWindowStyleMask = NSWindowStyleMaskClosable;
    window_b.reset([[WebContentsHostWindowForOcclusionTesting alloc]
        initWithContentRect:kWindowBContentRect
                  styleMask:kWindowStyleMask
                    backing:NSBackingStoreBuffered
                      defer:YES]);
    [window_b setTitle:@"window_b"];
    [window_b setReleasedWhenClosed:NO];

    if (NSIsEmptyRect(window_frame)) {
      window_frame.size = [NSWindow frameRectForContentRect:kWindowBContentRect
                                                  styleMask:kWindowStyleMask]
                              .size;
    }
    [window_b setFrame:window_frame display:NO];

    OrderWindowFront(window_b);
  }

  void OrderWindowFront(NSWindow* window) {
    [window orderWindow:NSWindowAbove relativeTo:0];
    ASSERT_TRUE([window isVisible]);
  }

  void OrderWindowOut(NSWindow* window) {
    [window orderWindow:NSWindowOut relativeTo:0];
    ASSERT_FALSE([window isVisible]);
  }

  remote_cocoa::mojom::Visibility WindowAWebContentsVisibility() {
    return _host_a.WebContentsVisibility();
  }

  void SetWindowAWebContentsVisibility(
      remote_cocoa::mojom::Visibility visibility) {
    _host_a.OnWindowVisibilityChanged(visibility);
  }

  void TearDownInProcessBrowserTestFixture() override {
    [window_a_web_contents_view_cocoa setHost:nullptr];
  }

  base::scoped_nsobject<WebContentsHostWindowForOcclusionTesting> window_a;
  base::scoped_nsobject<WebContentsViewCocoa> window_a_web_contents_view_cocoa;
  base::scoped_nsobject<WebContentsHostWindowForOcclusionTesting> window_b;

 private:
  WebContentsNSViewHostStub _host_a;
};

class WindowOcclusionBrowserTestMacWithOcclusionDetectionFeature
    : public WindowOcclusionBrowserTestMac {
 public:
  WindowOcclusionBrowserTestMacWithOcclusionDetectionFeature() {
    _features.InitAndEnableFeatureWithParameters(
        kMacWebContentsOcclusion,
        {{kEnhancedWindowOcclusionDetection, "true"}});
  }

 private:
  base::test::ScopedFeatureList _features;
};

class WindowOcclusionBrowserTestMacWithDisplaySleepDetectionFeature
    : public WindowOcclusionBrowserTestMac {
 public:
  WindowOcclusionBrowserTestMacWithDisplaySleepDetectionFeature() {
    _features.InitAndEnableFeatureWithParameters(
        kMacWebContentsOcclusion, {{kDisplaySleepAndAppHideDetection, "true"}});
  }

 private:
  base::test::ScopedFeatureList _features;
};

// Test that enhanced occlusion detection doesn't work if the feature's not
// enabled.
IN_PROC_BROWSER_TEST_F(WindowOcclusionBrowserTestMac,
                       ManualOcclusionDetectionDisabled) {
  InitWindowA();

  // Create a second window and place it exactly over window_a. The window
  // should still be considered visible.
  InitWindowB([window_a frame]);
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);
}

// Test that display sleep and app hide detection don't work if the feature's
// not enabled.
IN_PROC_BROWSER_TEST_F(WindowOcclusionBrowserTestMac,
                       OcclusionDetectionOnDisplaySleepAndAppHideDisabled) {
  EXPECT_TRUE([[NSWorkspace sharedWorkspace] notificationCenter]);

  InitWindowA();

  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);

  // Fake a display sleep notification.
  [[[NSWorkspace sharedWorkspace] notificationCenter]
      postNotificationName:NSWorkspaceScreensDidSleepNotification
                    object:nil
                  userInfo:nil];

  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);

  // Fake an application hide notification.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSApplicationDidHideNotification
                    object:nil
                  userInfo:nil];

  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);
}

// Test that we properly handle occlusion notifications from macOS.
IN_PROC_BROWSER_TEST_F(
    WindowOcclusionBrowserTestMacWithOcclusionDetectionFeature,
    MacOSOcclusionNotifications) {
  InitWindowA();

  [window_a setOccludedForTesting:YES];
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidChangeOcclusionStateNotification
                    object:window_a
                  userInfo:nil];

  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kOccluded);

  [window_a setOccludedForTesting:NO];
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidChangeOcclusionStateNotification
                    object:window_a
                  userInfo:nil];

  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);
}

IN_PROC_BROWSER_TEST_F(
    WindowOcclusionBrowserTestMacWithOcclusionDetectionFeature,
    ManualOcclusionDetection) {
  InitWindowA();

  // Create a second window and place it exactly over window_a. Unlike macOS,
  // our manual occlusion detection will determine window_a is occluded.
  InitWindowB([window_a frame]);
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kOccluded);

  // Move window_b slightly in different directions and check the occlusion
  // state of window_a's web contents.
  const NSSize window_offsets[] = {
      {1.0, 0.0}, {-1.0, 0.0}, {0.0, 1.0}, {0.0, -1.0}};
  NSRect window_b_frame = [window_b frame];
  for (size_t i = 0; i < base::size(window_offsets); i++) {
    // Move window b so that it no longer completely covers
    // window_a's webcontents.
    NSRect offset_window_frame = NSOffsetRect(
        window_b_frame, window_offsets[i].width, window_offsets[i].height);
    [window_b setFrame:offset_window_frame display:YES];

    EXPECT_EQ(WindowAWebContentsVisibility(),
              remote_cocoa::mojom::Visibility::kVisible);

    // Move it back.
    [window_b setFrame:window_b_frame display:YES];
    EXPECT_EQ(WindowAWebContentsVisibility(),
              remote_cocoa::mojom::Visibility::kOccluded);
  }
}

// Checks manual occlusion detection as windows change display order.
IN_PROC_BROWSER_TEST_F(
    WindowOcclusionBrowserTestMacWithOcclusionDetectionFeature,
    ManualOcclusionDetectionOnWindowOrderChange) {
  InitWindowA();

  // Size and position the second window so that it exactly covers the
  // first.
  InitWindowB([window_a frame]);
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kOccluded);

  OrderWindowFront(window_a);
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);

  OrderWindowFront(window_b);
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kOccluded);
}

// Checks that window_a, occluded by window_b, transitions to kVisible while the
// user resizes window_b.
IN_PROC_BROWSER_TEST_F(
    WindowOcclusionBrowserTestMacWithOcclusionDetectionFeature,
    ManualOcclusionDetectionOnWindowLiveResize) {
  InitWindowA();

  // Size and position the second window so that it exactly covers the
  // first.
  InitWindowB([window_a frame]);
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kOccluded);

  // Fake the start of a live resize. window_a's web contents should
  // become kVisible because resizing window_b may expose whatever's
  // behind it.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowWillStartLiveResizeNotification
                    object:window_b
                  userInfo:nil];
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);

  // Fake the resize end, which should return window_a to kOccluded because
  // it's still completely covered by window_b.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidEndLiveResizeNotification
                    object:window_b
                  userInfo:nil];
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kOccluded);
}

// Checks that window_a, occluded by window_b, transitions to kVisible when
// window_b is set to close.
IN_PROC_BROWSER_TEST_F(
    WindowOcclusionBrowserTestMacWithOcclusionDetectionFeature,
    ManualOcclusionDetectionOnWindowClose) {
  InitWindowA();

  // Size and position the second window so that it exactly covers the
  // first.
  InitWindowB([window_a frame]);
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kOccluded);

  // Close window b.
  [window_b close];
  ASSERT_FALSE([window_b isVisible]);

  // window_a's web contents should be kVisible, so that it's properly
  // updated when window_b goes offscreen.
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);
}

// Checks that window_a, occluded by window_b and window_c, remains kOccluded
// when window_b is set to close.
IN_PROC_BROWSER_TEST_F(
    WindowOcclusionBrowserTestMacWithOcclusionDetectionFeature,
    ManualOcclusionDetectionOnMiddleWindowClose) {
  InitWindowA();

  // Size and position the second window so that it exactly covers the
  // first.
  InitWindowB([window_a frame]);
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kOccluded);

  // Create a window_c on top of them both.
  const NSRect kWindowCContentRect = NSMakeRect(0.0, 0.0, 80.0, 60.0);
  base::scoped_nsobject<NSWindow> window_c(
      [[WebContentsHostWindowForOcclusionTesting alloc]
          initWithContentRect:kWindowCContentRect
                    styleMask:NSWindowStyleMaskClosable
                      backing:NSBackingStoreBuffered
                        defer:YES]);
  [window_c setTitle:@"window_a"];
  [window_c setReleasedWhenClosed:NO];

  // Configure it for the test.
  [window_c setFrame:[window_a frame] display:NO];
  OrderWindowFront(window_c);

  // Close window_b.
  [window_b close];
  ASSERT_FALSE([window_b isVisible]);

  // window_a's web contents should remain kOccluded because of window_c.
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kOccluded);
}

// Checks that web contents are marked kHidden on display sleep.
IN_PROC_BROWSER_TEST_F(
    WindowOcclusionBrowserTestMacWithDisplaySleepDetectionFeature,
    OcclusionDetectionOnDisplaySleep) {
  EXPECT_TRUE([[NSWorkspace sharedWorkspace] notificationCenter]);

  InitWindowA();

  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);

  // Fake a display sleep notification.
  [[[NSWorkspace sharedWorkspace] notificationCenter]
      postNotificationName:NSWorkspaceScreensDidSleepNotification
                    object:nil
                  userInfo:nil];

  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kHidden);

  // Fake a display wake notification.
  [[[NSWorkspace sharedWorkspace] notificationCenter]
      postNotificationName:NSWorkspaceScreensDidWakeNotification
                    object:nil
                  userInfo:nil];

  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);
}

// Checks that occlusion updates are ignored in between fullscreen transition
// notifications.
IN_PROC_BROWSER_TEST_F(
    WindowOcclusionBrowserTestMacWithOcclusionDetectionFeature,
    IgnoreOcclusionUpdatesBetweenWindowFullscreenTransitionNotifications) {
  InitWindowA();

  SetWindowAWebContentsVisibility(remote_cocoa::mojom::Visibility::kHidden);
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kHidden);

  // Fake a fullscreen transition notification.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowWillEnterFullScreenNotification
                    object:window_a
                  userInfo:nil];

  // Updating visibility should have no effect while in transition.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidChangeOcclusionStateNotification
                    object:window_a
                  userInfo:nil];
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kHidden);

  // End the transition.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidEnterFullScreenNotification
                    object:window_a
                  userInfo:nil];

  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidChangeOcclusionStateNotification
                    object:window_a
                  userInfo:nil];
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);

  // Reset.
  SetWindowAWebContentsVisibility(remote_cocoa::mojom::Visibility::kHidden);
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kHidden);

  // Fake the exit transition start.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowWillExitFullScreenNotification
                    object:window_a
                  userInfo:nil];

  // Updating visibility should have no effect while in transition.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidChangeOcclusionStateNotification
                    object:window_a
                  userInfo:nil];
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kHidden);

  // End the transition.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidExitFullScreenNotification
                    object:window_a
                  userInfo:nil];

  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidChangeOcclusionStateNotification
                    object:window_a
                  userInfo:nil];
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);
}

// Tests that each web contents in a window receives an updated occlusion
// state updated.
IN_PROC_BROWSER_TEST_F(
    WindowOcclusionBrowserTestMacWithOcclusionDetectionFeature,
    OcclusionDetectionForMultipleWebContents) {
  InitWindowA();

  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);

  // Create a second web contents.
  const NSRect kWebContentsBFrame = NSMakeRect(0.0, 0.0, 10.0, 10.0);
  WebContentsViewCocoa* web_contents_b =
      [[WebContentsViewCocoa alloc] initWithFrame:kWebContentsBFrame];
  [[window_a contentView] addSubview:web_contents_b];
  WebContentsNSViewHostStub host_2;
  [web_contents_b setHost:&host_2];
  host_2.OnWindowVisibilityChanged(remote_cocoa::mojom::Visibility::kVisible);

  const NSRect kWebContentsCFrame = NSMakeRect(0.0, 20.0, 10.0, 10.0);
  WebContentsViewCocoa* web_contents_c =
      [[WebContentsViewCocoa alloc] initWithFrame:kWebContentsCFrame];
  [[window_a contentView] addSubview:web_contents_c];
  WebContentsNSViewHostStub host_3;
  [web_contents_c setHost:&host_3];
  host_3.OnWindowVisibilityChanged(remote_cocoa::mojom::Visibility::kVisible);

  // Add window_b to occlude window_a and its web contentses.
  InitWindowB([window_a frame]);

  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kOccluded);
  EXPECT_EQ(host_2.WebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kOccluded);
  EXPECT_EQ(host_3.WebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kOccluded);

  // Close window b, which should expose the web contentses.
  [window_b close];
  ASSERT_FALSE([window_b isVisible]);

  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);
  EXPECT_EQ(host_2.WebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);
  EXPECT_EQ(host_3.WebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);

  [web_contents_b setHost:nullptr];
  [web_contents_c setHost:nullptr];
}

// Checks that web contentses are marked kHidden on WebContentsViewCocoa hide.
IN_PROC_BROWSER_TEST_F(
    WindowOcclusionBrowserTestMacWithOcclusionDetectionFeature,
    OcclusionDetectionOnWebContentsViewCocoaHide) {
  InitWindowA();

  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);

  [window_a_web_contents_view_cocoa setHidden:YES];
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kHidden);

  [window_a_web_contents_view_cocoa setHidden:NO];
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);

  // Hiding the superview should have the same effect.
  [[window_a_web_contents_view_cocoa superview] setHidden:YES];
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kHidden);

  [[window_a_web_contents_view_cocoa superview] setHidden:NO];
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);
}

// Checks that web contentses are marked kHidden on WebContentsViewCocoa removal
// from the view hierarchy.
IN_PROC_BROWSER_TEST_F(
    WindowOcclusionBrowserTestMacWithOcclusionDetectionFeature,
    OcclusionDetectionOnWebContentsViewCocoaRemoveFromSuperview) {
  InitWindowA();

  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);

  [window_a_web_contents_view_cocoa removeFromSuperview];
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kHidden);

  // Adding it back should make it visible.
  [[window_a contentView] addSubview:window_a_web_contents_view_cocoa];
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);

  // Try the same with its superview.
  const NSRect kTmpViewFrame = NSMakeRect(0.0, 0.0, 10.0, 10.0);
  base::scoped_nsobject<NSView> tmpView(
      [[NSView alloc] initWithFrame:kTmpViewFrame]);
  [[window_a contentView] addSubview:tmpView];
  [window_a_web_contents_view_cocoa removeFromSuperview];
  [tmpView addSubview:window_a_web_contents_view_cocoa];
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);

  [tmpView removeFromSuperview];
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kHidden);

  [[window_a contentView] addSubview:tmpView];
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);
}

// Checks that web contentses are marked kHidden on window miniaturize.
IN_PROC_BROWSER_TEST_F(
    WindowOcclusionBrowserTestMacWithOcclusionDetectionFeature,
    OcclusionDetectionOnWindowMiniaturize) {
  InitWindowA();

  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);

  [window_a miniaturize:nil];
  EXPECT_TRUE([window_a isMiniaturized]);
  EXPECT_NE(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);

  [window_a deminiaturize:nil];
  EXPECT_FALSE([window_a isMiniaturized]);
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);
}

}  // namespace content
