// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/ui/assistant_aim_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_history_item.h"
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_ui_constants.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

constexpr CGFloat kVerticalSwipeDistance = 20.0;
constexpr CGFloat kVerticalSwipeVelocity = 200.0;
constexpr CGFloat kScrolledOffsetY = 50.0;

// Returns the first subview of `view` (or `view` itself) whose accessibility
// identifier is `identifier`, or nil if there is none.
UIView* FindViewWithAccessibilityIdentifier(UIView* view,
                                            NSString* identifier) {
  if ([view.accessibilityIdentifier isEqualToString:identifier]) {
    return view;
  }
  for (UIView* subview in view.subviews) {
    UIView* match = FindViewWithAccessibilityIdentifier(subview, identifier);
    if (match) {
      return match;
    }
  }
  return nil;
}

// Returns the closest ancestor `UIScrollView` enclosing `view`, or nil if none.
UIScrollView* FindEnclosingScrollView(UIView* view) {
  UIView* current = view.superview;
  while (current) {
    UIScrollView* scroll_view = base::apple::ObjCCast<UIScrollView>(current);
    if (scroll_view) {
      return scroll_view;
    }
    current = current.superview;
  }
  return nil;
}

}  // namespace

// Test subclass of `UIPanGestureRecognizer` with configurable velocity and
// translation.
@interface FakeAssistantAIMPanGestureRecognizer : UIPanGestureRecognizer
@property(nonatomic, assign) CGPoint fakeVelocity;
@property(nonatomic, assign) CGPoint fakeTranslation;
@end

@implementation FakeAssistantAIMPanGestureRecognizer

- (CGPoint)velocityInView:(UIView*)view {
  return self.fakeVelocity;
}

- (CGPoint)translationInView:(UIView*)view {
  return self.fakeTranslation;
}

@end

class AssistantAIMViewControllerTest : public PlatformTest {
 protected:
  AssistantAIMViewControllerTest() {
    view_controller_ = [[AssistantAIMViewController alloc] init];
    scoped_key_window_.Get().rootViewController = view_controller_;
    [view_controller_ loadViewIfNeeded];
  }

  ScopedKeyWindow scoped_key_window_;
  AssistantAIMViewController* view_controller_;
};

// Tests that `shouldPauseScrollView:forGesture:isInLargestDetent:` pauses the
// signed-out History scroll view in smaller detents so swiping up expands the
// bottom sheet first, and allows internal scrolling once in the largest detent.
TEST_F(AssistantAIMViewControllerTest,
       HistoryScrollViewExpandsSheetBeforeScrollingInternally) {
  [view_controller_ displayHistoryWithItems:{} signedIn:NO];
  [view_controller_.view layoutIfNeeded];

  UIView* signed_out_view = FindViewWithAccessibilityIdentifier(
      view_controller_.view,
      kAssistantAIMHistorySignedOutViewAccessibilityIdentifier);
  ASSERT_TRUE(signed_out_view);

  UIScrollView* history_scroll_view = FindEnclosingScrollView(signed_out_view);
  ASSERT_TRUE(history_scroll_view);

  FakeAssistantAIMPanGestureRecognizer* pan_gesture =
      [[FakeAssistantAIMPanGestureRecognizer alloc] init];

  // Swiping up (negative Y velocity/translation) at the top in a smaller
  // detent should pause the scroll view so the bottom sheet expands first.
  history_scroll_view.contentOffset = CGPointZero;
  pan_gesture.fakeTranslation = CGPointMake(0.0, -kVerticalSwipeDistance);
  pan_gesture.fakeVelocity = CGPointMake(0.0, -kVerticalSwipeVelocity);
  EXPECT_TRUE([view_controller_ shouldPauseScrollView:history_scroll_view
                                           forGesture:pan_gesture
                                    isInLargestDetent:NO]);

  // Even if the scroll view was previously scrolled down (`contentOffset.y >
  // 0`), swiping up in a smaller detent should still pause the scroll view to
  // expand the bottom sheet first.
  history_scroll_view.contentOffset = CGPointMake(0.0, kScrolledOffsetY);
  EXPECT_TRUE([view_controller_ shouldPauseScrollView:history_scroll_view
                                           forGesture:pan_gesture
                                    isInLargestDetent:NO]);

  // Once in the largest detent, swiping up should NOT pause the scroll view so
  // it can scroll internally.
  history_scroll_view.contentOffset = CGPointZero;
  EXPECT_FALSE([view_controller_ shouldPauseScrollView:history_scroll_view
                                            forGesture:pan_gesture
                                     isInLargestDetent:YES]);

  // In the largest detent at the top (`contentOffset.y <= 0`), dragging down
  // (positive Y velocity/translation) should pause the scroll view to collapse
  // the bottom sheet.
  pan_gesture.fakeTranslation = CGPointMake(0.0, kVerticalSwipeDistance);
  pan_gesture.fakeVelocity = CGPointMake(0.0, kVerticalSwipeVelocity);
  EXPECT_TRUE([view_controller_ shouldPauseScrollView:history_scroll_view
                                           forGesture:pan_gesture
                                    isInLargestDetent:YES]);

  // In the largest detent when scrolled down (`contentOffset.y > 0`), dragging
  // down should NOT pause the scroll view so it scrolls back toward the top
  // first.
  history_scroll_view.contentOffset = CGPointMake(0.0, kScrolledOffsetY);
  EXPECT_FALSE([view_controller_ shouldPauseScrollView:history_scroll_view
                                            forGesture:pan_gesture
                                     isInLargestDetent:YES]);
}
