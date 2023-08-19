// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/util_swift.h"

#import <UIKit/UIKit.h>

#import "base/test/ios/wait_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

// Sets up a window and a view.
class UIViewWindowCoordinatesTest : public PlatformTest {
 protected:
  UIViewWindowCoordinatesTest()
      : window_([[UIWindow alloc] init]), view_([[UIView alloc] init]) {}

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
