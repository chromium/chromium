// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

// A minimal UIWindow subclass to use in tests that doesn't register globally
// and tracks whether it gets deallocated.
@interface TestUIWindow : UIWindow
@property(nonatomic, assign) BOOL* deallocatedPtr;
@end

@implementation TestUIWindow
- (void)dealloc {
  if (self.deallocatedPtr) {
    *self.deallocatedPtr = YES;
  }
}
@end

// Sets up a window and a view.
class UIViewWindowCoordinatesTest : public PlatformTest {
 protected:
  UIViewWindowCoordinatesTest()
      : window_([[UIWindow alloc]
            initWithWindowScene:chrome_test_util::GetAnyWindowScene()]),
        view_([[UIView alloc] init]) {}

  UIWindow* window_;
  UIView* view_;
};

// Checks that the callback is called when the view is added to a window.
TEST_F(UIViewWindowCoordinatesTest, SetWindow) {
  __block BOOL callback_called = NO;
  UIView* retained_view = view_;
  view_.cr_onWindowCoordinatesChanged = ^(UIView* view) {
    callback_called = YES;
    EXPECT_EQ(view, retained_view);
  };

  [window_ addSubview:view_];

  EXPECT_TRUE(callback_called);
}

// Checks that the initial callback is not called when the callback is reset and
// the view is added to a window.
TEST_F(UIViewWindowCoordinatesTest, Reset) {
  view_.cr_onWindowCoordinatesChanged = ^(UIView* view) {
    FAIL() << "Callback should not have been called after being unset.";
  };
  view_.cr_onWindowCoordinatesChanged = nil;

  [window_ addSubview:view_];
}

// Checks the callback is called when the view's frame changes.
TEST_F(UIViewWindowCoordinatesTest, ChangeFrameAsDirectSubview) {
  [window_ addSubview:view_];
  __block BOOL callback_called = NO;
  view_.cr_onWindowCoordinatesChanged = ^(UIView* view) {
    callback_called = YES;
  };
  // The callback is called when set if the view is in a window. Now reset it to
  // check that it's called again when changing view_'s frame.
  EXPECT_TRUE(callback_called);
  callback_called = NO;

  view_.frame = CGRectMake(10, 20, 30, 40);
  [window_ setNeedsLayout];
  [window_ layoutIfNeeded];

  // Wait until the expected handler is called.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return callback_called;
  }));
}

// Checks the callback is called when any parent view's frame changes (even
// though the view's frame itself doesn't change in its parent).
TEST_F(UIViewWindowCoordinatesTest, ChangeFrameOfParentView) {
  UIView* parent_view = [[UIView alloc] init];
  [parent_view addSubview:view_];
  [window_ addSubview:parent_view];
  __block BOOL callback_called = NO;
  view_.cr_onWindowCoordinatesChanged = ^(UIView* view) {
    callback_called = YES;
  };
  // The callback is called when set if the view is in a window. Now reset it to
  // check that it's called again when changing view_'s frame.
  EXPECT_TRUE(callback_called);
  callback_called = NO;

  parent_view.frame = CGRectMake(10, 20, 30, 40);
  [window_ setNeedsLayout];
  [window_ layoutIfNeeded];

  // Wait until the expected handler is called.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return callback_called;
  }));
}

// Verifies that destroying the window while the view continues to observe it
// doesn't cause a Use-After-Free access of the mirror view.
TEST_F(UIViewWindowCoordinatesTest, WindowDeallocationRegressionTest) {
  __block __weak UIView* weakNotifyingView = nil;
  BOOL windowDeallocated = NO;

  @autoreleasepool {
    // Instantiate a freestanding window not attached to a scene so it can be
    // deallocated directly.
    TestUIWindow* local_window =
        [[TestUIWindow alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
    local_window.deallocatedPtr = &windowDeallocated;

    // Attach the view and register the observer.
    view_.cr_onWindowCoordinatesChanged = ^(UIView* view) {
      // NOP
    };
    [local_window addSubview:view_];

    // Force layout to instantiate the mirror view.
    [local_window layoutIfNeeded];

    // Find the NotifyingView in the hierarchy to track its lifetime.
    for (UIView* subview in local_window.subviews) {
      if ([NSStringFromClass([subview class])
              containsString:@"NotifyingView"]) {
        weakNotifyingView = subview;
        break;
      }
    }
    EXPECT_NE(weakNotifyingView, nil);

    // Remove reference to allow deallocation.
    local_window = nil;
  }

  // Ensure the window is deallocated and the notifying view is detached.
  @autoreleasepool {
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
      return (BOOL)(windowDeallocated && weakNotifyingView.superview == nil);
    }));

    // Clearing the callback releases the retained mirror view.
    view_.cr_onWindowCoordinatesChanged = nil;
  }
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return (BOOL)(weakNotifyingView == nil);
  }));
}

// Test that resetting `cr_onWindowCoordinatesChanged` while the view is in a
// window removes the internal `NotifyingView` mirror from the window, and that
// reassigning the callback multiple times, reparenting within the same window,
// or moving across windows reuses the same mirror view instance without
// accumulating duplicate mirror views or constraints.
TEST_F(UIViewWindowCoordinatesTest, ResetAndReassignRemovesMirrorView) {
  UIView* parent_view_1 =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 50, 50)];
  UIView* parent_view_2 =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 50, 50)];
  [window_ addSubview:parent_view_1];
  [window_ addSubview:parent_view_2];
  [parent_view_1 addSubview:view_];

  auto find_notifying_views = ^NSArray<UIView*>*(UIWindow* window) {
    return chrome_test_util::FindViewsWithPredicate(
        window, ^BOOL(UIView* subview) {
          return [NSStringFromClass([subview class])
              containsString:@"NotifyingView"];
        });
  };

  auto count_mirror_constraints = ^NSUInteger(UIWindow* window,
                                              UIView* mirror) {
    NSUInteger count = 0;
    for (NSLayoutConstraint* constraint in window.constraints) {
      if (constraint.firstItem == mirror || constraint.secondItem == mirror) {
        count++;
      }
    }
    return count;
  };

  __block BOOL callback_called = NO;
  view_.cr_onWindowCoordinatesChanged = ^(UIView* view) {
    callback_called = YES;
  };
  NSArray<UIView*>* initial_views = find_notifying_views(window_);
  ASSERT_EQ(1u, initial_views.count);
  UIView* initial_mirror = initial_views.firstObject;
  EXPECT_EQ(4u, count_mirror_constraints(window_, initial_mirror));

  // Reassigning the closure while the view is already in the window must
  // reuse the existing mirror view without adding duplicate constraints.
  view_.cr_onWindowCoordinatesChanged = ^(UIView* view) {
    callback_called = YES;
  };
  NSArray<UIView*>* reassigned_views = find_notifying_views(window_);
  ASSERT_EQ(1u, reassigned_views.count);
  EXPECT_EQ(initial_mirror, reassigned_views.firstObject);
  EXPECT_EQ(4u, count_mirror_constraints(window_, initial_mirror));

  // Reparenting the view within the same window without removing from superview
  // first must keep the existing mirror view and constraints intact.
  [parent_view_2 addSubview:view_];
  NSArray<UIView*>* reparented_views = find_notifying_views(window_);
  ASSERT_EQ(1u, reparented_views.count);
  EXPECT_EQ(initial_mirror, reparented_views.firstObject);
  EXPECT_EQ(4u, count_mirror_constraints(window_, initial_mirror));

  [window_ layoutIfNeeded];
  callback_called = NO;
  parent_view_2.frame = CGRectMake(10, 20, 50, 50);
  [window_ setNeedsLayout];
  [window_ layoutIfNeeded];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return callback_called;
  }));

  // Moving the view to a temporary window, removing it from all windows, and
  // moving it back to the original window (as UIKit does during background
  // snapshotting) must preserve and re-insert the same mirror view instance.
  UIWindow* temp_window =
      [[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  [temp_window addSubview:view_];
  EXPECT_EQ(0u, find_notifying_views(window_).count);
  EXPECT_EQ(0u, count_mirror_constraints(window_, initial_mirror));
  NSArray<UIView*>* temp_views = find_notifying_views(temp_window);
  ASSERT_EQ(1u, temp_views.count);
  EXPECT_EQ(initial_mirror, temp_views.firstObject);
  EXPECT_EQ(4u, count_mirror_constraints(temp_window, initial_mirror));

  [view_ removeFromSuperview];
  EXPECT_EQ(nil, initial_mirror.superview);

  [window_ addSubview:view_];
  NSArray<UIView*>* restored_views = find_notifying_views(window_);
  ASSERT_EQ(1u, restored_views.count);
  EXPECT_EQ(initial_mirror, restored_views.firstObject);
  EXPECT_EQ(4u, count_mirror_constraints(window_, initial_mirror));

  // Clearing the closure must remove the mirror view from the window.
  view_.cr_onWindowCoordinatesChanged = nil;
  EXPECT_EQ(0u, find_notifying_views(window_).count);
  EXPECT_EQ(0u, count_mirror_constraints(window_, initial_mirror));
}
