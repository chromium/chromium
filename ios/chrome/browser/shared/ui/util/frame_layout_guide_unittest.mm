// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/util_swift.h"

#import <UIKit/UIKit.h>

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Sets up a view and a layout guide.
class FrameLayoutGuideTest : public PlatformTest {
 protected:
  FrameLayoutGuideTest()
      : window_([[UIWindow alloc] init]),
        view_([[UIView alloc] init]),
        frame_layout_guide_([[FrameLayoutGuide alloc] init]) {}

  UIWindow* window_;
  UIView* view_;
  FrameLayoutGuide* frame_layout_guide_;
};

// Checks that the callback is called when the owning view is added to the
// window.
TEST_F(FrameLayoutGuideTest, Set) {
  [view_ addLayoutGuide:frame_layout_guide_];
  __block BOOL callback_called = NO;
  UILayoutGuide* retained_layout_guide = frame_layout_guide_;
  frame_layout_guide_.onDidMoveToWindow = ^(UILayoutGuide* layout_guide) {
    callback_called = YES;
    EXPECT_EQ(layout_guide, retained_layout_guide);
  };

  [window_ addSubview:view_];

  EXPECT_TRUE(callback_called);
}

// Checks that the callback is not called when the owning view is added to the
// window and `onDidMoveToWindow` is reset to `nil`.
TEST_F(FrameLayoutGuideTest, Reset) {
  [view_ addLayoutGuide:frame_layout_guide_];
  frame_layout_guide_.onDidMoveToWindow = ^(UILayoutGuide* layout_guide) {
    FAIL() << "Callback should not have been called after being unset.";
  };
  frame_layout_guide_.onDidMoveToWindow = nil;

  [window_ addSubview:view_];
}

// Checks that removing the owning view from its window calls the callback.
TEST_F(FrameLayoutGuideTest, RemoveFromWindow) {
  [view_ addLayoutGuide:frame_layout_guide_];
  [window_ addSubview:view_];
  __block BOOL callback_called = NO;
  frame_layout_guide_.onDidMoveToWindow = ^(UILayoutGuide* layout_guide) {
    callback_called = YES;
  };

  [view_ removeFromSuperview];

  EXPECT_EQ(view_.window, nil);
  EXPECT_TRUE(callback_called);
}

// Checks that removing the layout guide from its owning view doesn't call the
// callback once the view's window changes.
TEST_F(FrameLayoutGuideTest, RemoveFromView) {
  [view_ addLayoutGuide:frame_layout_guide_];
  frame_layout_guide_.onDidMoveToWindow = ^(UILayoutGuide* layout_guide) {
    FAIL() << "Callback should not have been called after the layout guide was "
              "removed from the non-windowed view.";
  };
  [view_ removeLayoutGuide:frame_layout_guide_];

  [window_ addSubview:view_];
}

// Checks that the callback is called only on the current owning view, and not
// on prior owning views.
TEST_F(FrameLayoutGuideTest, ChangeView) {
  [view_ addLayoutGuide:frame_layout_guide_];
  UIView* other_view = [[UIView alloc] init];
  [other_view addLayoutGuide:frame_layout_guide_];
  __block BOOL callback_called = NO;
  frame_layout_guide_.onDidMoveToWindow = ^(UILayoutGuide* layout_guide) {
    callback_called = YES;
  };

  // Check that adding the initial view to the window doesn't call the callback.
  [window_ addSubview:view_];
  EXPECT_FALSE(callback_called);

  // Check that adding the other view calls the callback, i.e. the observation
  // was transferred from view_ to other_view.
  [window_ addSubview:other_view];
  EXPECT_TRUE(callback_called);
}

// Checks that passing the layout guide from one non-windowed view to another
// doesn't call the callback (since both have `window` nil).
TEST_F(FrameLayoutGuideTest, NotCalledOnReparentNonWindowedViews) {
  [view_ addLayoutGuide:frame_layout_guide_];
  UIView* other_view = [[UIView alloc] init];
  frame_layout_guide_.onDidMoveToWindow = ^(UILayoutGuide* layout_guide) {
    FAIL() << "Callback should not have been called while being moved between "
              "non-windowed views.";
  };

  [other_view addLayoutGuide:frame_layout_guide_];
}

// Checks that passing the layout guide from one view to another in the same
// window doesn't call the callback.
TEST_F(FrameLayoutGuideTest, NotCalledOnReparentViewsInSameWindow) {
  [view_ addLayoutGuide:frame_layout_guide_];
  UIView* other_view = [[UIView alloc] init];
  [window_ addSubview:view_];
  [window_ addSubview:other_view];
  frame_layout_guide_.onDidMoveToWindow = ^(UILayoutGuide* layout_guide) {
    FAIL() << "Callback should not have been called while being moved between "
              "views in the same window.";
  };

  [other_view addLayoutGuide:frame_layout_guide_];
}

// Checks that setting the constrained frame while the guide is not added to a
// view doesn't change the layout frame.
TEST_F(FrameLayoutGuideTest, NoOwningViewZeroLayoutFrame) {
  CGRect rect = CGRectMake(10, 20, 30, 40);

  frame_layout_guide_.constrainedFrame = rect;

  EXPECT_TRUE(CGRectEqualToRect(frame_layout_guide_.layoutFrame, CGRectZero));
  EXPECT_TRUE(CGRectEqualToRect(frame_layout_guide_.constrainedFrame, rect));
}

// Checks that adding the guide to a guide with a constrained frame updates the
// layout frame accordingly.
TEST_F(FrameLayoutGuideTest, OwningViewSetsLayoutFrame) {
  CGRect rect = CGRectMake(10, 20, 30, 40);
  frame_layout_guide_.constrainedFrame = rect;

  [view_ addLayoutGuide:frame_layout_guide_];
  [view_ setNeedsLayout];
  [view_ layoutIfNeeded];

  EXPECT_TRUE(CGRectEqualToRect(frame_layout_guide_.layoutFrame, rect));
  EXPECT_TRUE(CGRectEqualToRect(frame_layout_guide_.constrainedFrame, rect));
}

// Checks that removing the guide from its owning view resets the layout frame
// but keeps the constrained frame.
TEST_F(FrameLayoutGuideTest,
       ResetOwningViewResetsLayoutFrameKeepsConstrainedFrame) {
  CGRect rect = CGRectMake(10, 20, 30, 40);
  frame_layout_guide_.constrainedFrame = rect;
  [view_ addLayoutGuide:frame_layout_guide_];
  [view_ setNeedsLayout];
  [view_ layoutIfNeeded];

  [view_ removeLayoutGuide:frame_layout_guide_];

  EXPECT_TRUE(CGRectEqualToRect(frame_layout_guide_.layoutFrame, CGRectZero));
  EXPECT_TRUE(CGRectEqualToRect(frame_layout_guide_.constrainedFrame, rect));
}
