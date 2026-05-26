// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/mouse_cursor_overlay_controller.h"

#import <Cocoa/Cocoa.h>

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "content/browser/media/capture/mouse_cursor_overlay_controller_unittest.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"

@interface MockKeyWindow : NSWindow
@end

@implementation MockKeyWindow
- (BOOL)isKeyWindow {
  return YES;
}
- (NSPoint)convertPointToScreen:(NSPoint)point {
  return point;
}
@end

@interface MockEvent : NSObject
@property(nonatomic, assign) NSPoint locationInWindow;
@property(nonatomic, readonly) NSEventType type;
@property(nonatomic, readonly) NSUInteger modifierFlags;
@property(nonatomic, readonly) NSTimeInterval timestamp;
@property(nonatomic, readonly) NSInteger buttonNumber;
@property(nonatomic, readonly) NSWindow* window;
@end

@implementation MockEvent
@synthesize locationInWindow = _locationInWindow;

- (NSEventType)type {
  return NSEventTypeMouseMoved;
}
- (NSUInteger)modifierFlags {
  return 0;
}
- (NSTimeInterval)timestamp {
  return 0;
}
- (NSInteger)buttonNumber {
  return 0;
}
- (NSWindow*)window {
  return nil;
}
@end

namespace content {

using testing::_;
using testing::Mock;

class MouseCursorOverlayControllerMacTest
    : public MouseCursorOverlayControllerTestBase {
 protected:
  void SetupCaptureTarget(WebContents* target_web_contents,
                          const gfx::Rect& bounds) override {
    // bounds is in Aura coordinates. Convert to Cocoa.
    // Parent view is 100x100.
    target_web_contents->GetNativeView().GetNativeNSView().frame =
        NSMakeRect(bounds.x(), 100 - bounds.y() - bounds.height(),
                   bounds.width(), bounds.height());

    window_ =
        [[MockKeyWindow alloc] initWithContentRect:NSMakeRect(0, 0, 100, 100)
                                         styleMask:NSWindowStyleMaskTitled
                                           backing:NSBackingStoreBuffered
                                             defer:NO];
    [window_ makeKeyAndOrderFront:nil];
    NSView* main_view = web_contents()->GetNativeView().GetNativeNSView();
    main_view.frame = NSMakeRect(0, 0, 100, 100);
    [window_.contentView addSubview:main_view];
    [main_view
        addSubview:target_web_contents->GetNativeView().GetNativeNSView()];

    main_view.hidden = NO;
    target_web_contents->GetNativeView().GetNativeNSView().hidden = NO;
    web_contents()->WasShown();
    target_web_contents->WasShown();
  }

  gfx::NativeView GetTargetView() override {
    return web_contents()->GetNativeView();
  }

  void InitializeEventGenerator() override {
    NSView* main_view = web_contents()->GetNativeView().GetNativeNSView();
    NSArray* tracking_areas = [main_view trackingAreas];
    ASSERT_GT([tracking_areas count], 0u);
    tracker_ = [[tracking_areas lastObject] owner];
  }

  void SendMouseMove(const gfx::Point& position_in_parent) override {
    MockEvent* event = [[MockEvent alloc] init];
    event.locationInWindow =
        NSMakePoint(position_in_parent.x(), 100 - position_in_parent.y());
    [tracker_ mouseMoved:(NSEvent*)event];
  }

  gfx::Point GetExpectedCapturedPosition(
      const gfx::Point& position_in_parent,
      const gfx::Rect& target_bounds) override {
    return position_in_parent - target_bounds.OffsetFromOrigin();
  }

 private:
  MockKeyWindow* __strong window_;
  id __strong tracker_;
};

TEST_F(MouseCursorOverlayControllerMacTest, RestrictsToWebContents) {
  RunRestrictsToWebContentsTest();
}

}  // namespace content
