// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_multitasking_test_util.h"

#import <XCTest/XCTest.h>

#import "base/test/ios/wait_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

namespace {

NSString* const kSpringboardBundleIdentifier = @"com.apple.springboard";

const NSTimeInterval kDragDuration = 0.5;

using base::test::ios::kWaitForUIElementTimeout;

}  // namespace

@implementation ChromeMultitaskingTestUtil

+ (void)moveToWindowedMode {
  XCUIApplication* app = [[XCUIApplication alloc] init];
  XCUIElement* appWindow = app.windows[@"0"];

  // Drag bottom-right corner inwards to enter Stage Manager windowed mode.
  XCUICoordinate* bottomRightStart =
      [appWindow coordinateWithNormalizedOffset:CGVectorMake(1.0, 1.0)];
  XCUICoordinate* dragDestination =
      [appWindow coordinateWithNormalizedOffset:CGVectorMake(0.85, 0.85)];

  [bottomRightStart pressForDuration:kDragDuration
                thenDragToCoordinate:dragDestination
                        withVelocity:XCUIGestureVelocityDefault
                 thenHoldForDuration:kDragDuration];

  GREYCondition* isWindowed =
      [GREYCondition conditionWithName:@"Wait for windowed mode"
                                 block:^BOOL {
                                   return [ChromeEarlGrey isWindowedMode];
                                 }];
  GREYAssertTrue(
      [isWindowed waitWithTimeout:kWaitForUIElementTimeout.InSecondsF()],
      @"App did not enter windowed mode.");
  [self waitForWindowFrameToStabilize];
}

+ (void)resizeWindowToCompact {
  XCUIApplication* app = [[XCUIApplication alloc] init];
  XCUIElement* appWindow = app.windows[@"0"];

  // Drag bottom-left corner inwards to trigger compact width.
  XCUICoordinate* bottomLeftStart =
      [appWindow coordinateWithNormalizedOffset:CGVectorMake(0.0, 1.0)];
  XCUICoordinate* dragDestination =
      [appWindow coordinateWithNormalizedOffset:CGVectorMake(0.4, 0.6)];

  [bottomLeftStart pressForDuration:kDragDuration
               thenDragToCoordinate:dragDestination
                       withVelocity:XCUIGestureVelocityDefault
                thenHoldForDuration:kDragDuration];

  GREYCondition* isCompact =
      [GREYCondition conditionWithName:@"Wait for compact width"
                                 block:^BOOL {
                                   return [ChromeEarlGrey isCompactWidth];
                                 }];
  GREYAssertTrue(
      [isCompact waitWithTimeout:kWaitForUIElementTimeout.InSecondsF()],
      @"Window did not transition to compact width.");
  [self waitForWindowFrameToStabilize];
}

+ (void)resizeWindowToRegular {
  XCUIApplication* springboard = [[XCUIApplication alloc]
      initWithBundleIdentifier:kSpringboardBundleIdentifier];
  XCUIApplication* app = [[XCUIApplication alloc] init];
  XCUIElement* appWindow = app.windows[@"0"];

  // Drag bottom-left corner back to left edge to restore regular width.
  // Note: Avoid dragging to screen bottom-left corner (0.0, 1.0) as it snaps
  // the app back to fullscreen mode.
  XCUICoordinate* newBottomLeft =
      [appWindow coordinateWithNormalizedOffset:CGVectorMake(0.0, 1.0)];
  XCUICoordinate* screenLeftEdge =
      [springboard coordinateWithNormalizedOffset:CGVectorMake(0.0, 0.8)];

  [newBottomLeft pressForDuration:kDragDuration
             thenDragToCoordinate:screenLeftEdge
                     withVelocity:XCUIGestureVelocityDefault
              thenHoldForDuration:kDragDuration];

  GREYCondition* isRegular =
      [GREYCondition conditionWithName:@"Wait for regular width"
                                 block:^BOOL {
                                   return ![ChromeEarlGrey isCompactWidth];
                                 }];
  GREYAssertTrue(
      [isRegular waitWithTimeout:kWaitForUIElementTimeout.InSecondsF()],
      @"Window did not transition back to regular width.");
  [self waitForWindowFrameToStabilize];
}

+ (void)moveToFullscreenMode {
  XCUIApplication* springboard = [[XCUIApplication alloc]
      initWithBundleIdentifier:kSpringboardBundleIdentifier];
  XCUIApplication* app = [[XCUIApplication alloc] init];
  XCUIElement* appWindow = app.windows[@"0"];

  // Drag bottom-right corner to screen corner to exit Stage Manager back to
  // fullscreen.
  XCUICoordinate* bottomRightStart =
      [appWindow coordinateWithNormalizedOffset:CGVectorMake(1.0, 1.0)];
  XCUICoordinate* screenBottomRight =
      [springboard coordinateWithNormalizedOffset:CGVectorMake(1.0, 1.0)];

  [bottomRightStart pressForDuration:kDragDuration
                thenDragToCoordinate:screenBottomRight
                        withVelocity:XCUIGestureVelocityDefault
                 thenHoldForDuration:kDragDuration];

  GREYCondition* isFullscreen =
      [GREYCondition conditionWithName:@"Wait for fullscreen mode"
                                 block:^BOOL {
                                   return ![ChromeEarlGrey isWindowedMode];
                                 }];
  GREYAssertTrue(
      [isFullscreen waitWithTimeout:kWaitForUIElementTimeout.InSecondsF()],
      @"App did not return to fullscreen mode.");
  [self waitForWindowFrameToStabilize];
}

#pragma mark - Private

// Waits for the app window's frame to stabilize by ensuring it does not change
// across consecutive checks. This ensures system-level window resizing
// animations have completed.
+ (void)waitForWindowFrameToStabilize {
  XCUIApplication* app = [[XCUIApplication alloc] init];
  XCUIElement* appWindow = app.windows[@"0"];
  __block CGRect lastFrame = CGRectNull;
  GREYCondition* frameStabilized = [GREYCondition
      conditionWithName:@"Wait for window frame to stabilize"
                  block:^BOOL {
                    CGRect currentFrame = appWindow.frame;
                    if (CGRectEqualToRect(lastFrame, CGRectNull)) {
                      lastFrame = currentFrame;
                      return NO;
                    }
                    BOOL stable = CGRectEqualToRect(lastFrame, currentFrame);
                    lastFrame = currentFrame;
                    return stable;
                  }];
  GREYAssertTrue(
      [frameStabilized waitWithTimeout:kWaitForUIElementTimeout.InSecondsF()],
      @"Window frame did not stabilize.");
}

@end
