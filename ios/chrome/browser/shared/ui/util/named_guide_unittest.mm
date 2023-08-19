// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/named_guide.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {
// Tests that `guide`'s layoutFrame is equal to `frame`.
void VerifyLayoutFrame(UILayoutGuide* guide, CGRect frame) {
  [guide.owningView setNeedsLayout];
  [guide.owningView layoutIfNeeded];
  EXPECT_TRUE(CGRectEqualToRect(guide.layoutFrame, frame));
}
}  // namespace

using NamedGuideTest = PlatformTest;

// Tests that guides are reachable after being added to a view.
TEST_F(NamedGuideTest, TestAddAndFind) {
  GuideName* test_guide = @"NamedGuideTest";

  UIView* view = [[UIView alloc] init];
  EXPECT_EQ(nil, [NamedGuide guideWithName:test_guide view:view]);

  // The test_guide should be reachable after adding it.
  NamedGuide* guide = [[NamedGuide alloc] initWithName:test_guide];
  [view addLayoutGuide:guide];
  EXPECT_EQ(guide, [NamedGuide guideWithName:test_guide view:view]);
}

// Tests that guides added to a child view are not reachable from the parent.
TEST_F(NamedGuideTest, TestGuideOnChild) {
  GuideName* test_guide = @"NamedGuideTest";

  UIView* view = [[UIView alloc] init];
  UIView* childView = [[UIView alloc] init];
  [view addSubview:childView];

  NamedGuide* guide = [[NamedGuide alloc] initWithName:test_guide];
  [childView addLayoutGuide:guide];

  // This guide should be reachable from the child, but not from the parent.
  EXPECT_EQ(guide, [NamedGuide guideWithName:test_guide view:childView]);
  EXPECT_EQ(nil, [NamedGuide guideWithName:test_guide view:view]);
}

// Tests that children can reach guides that are added to ancestors.
TEST_F(NamedGuideTest, TestGuideOnAncestor) {
  GuideName* test_guide = @"NamedGuideTest";

  UIView* view = [[UIView alloc] init];
  UIView* childView = [[UIView alloc] init];
  UIView* grandChildView = [[UIView alloc] init];
  [view addSubview:childView];
  [childView addSubview:grandChildView];

  NamedGuide* guide = [[NamedGuide alloc] initWithName:test_guide];
  [view addLayoutGuide:guide];

  // The guide added to the top-level view should be accessible from all
  // descendent views.
  EXPECT_EQ(guide, [NamedGuide guideWithName:test_guide view:grandChildView]);
  EXPECT_EQ(guide, [NamedGuide guideWithName:test_guide view:childView]);
  EXPECT_EQ(guide, [NamedGuide guideWithName:test_guide view:view]);
}

// Tests that resetting the constrained view updates the guide.
TEST_F(NamedGuideTest, TestConstrainedView) {
  GuideName* test_guide = @"NamedGuideTest";

  UIWindow* window =
      [[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  UIView* view = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  [window addSubview:view];
  [view addSubview:[[UIView alloc] initWithFrame:CGRectMake(0, 0, 50, 100)]];
  [view addSubview:[[UIView alloc] initWithFrame:CGRectMake(50, 0, 50, 100)]];

  NamedGuide* guide = [[NamedGuide alloc] initWithName:test_guide];
  [view addLayoutGuide:guide];
  ASSERT_FALSE(guide.constrained);

  // Set the constrained view to the subviews and verify that the layout frame
  // is updated.
  for (UIView* subview in view.subviews) {
    guide.constrainedView = nil;
    EXPECT_FALSE(guide.constrained);
    guide.constrainedView = subview;
    EXPECT_TRUE(guide.constrained);
    VerifyLayoutFrame(guide, subview.frame);
  }
}

// Tests that resetting the constrained frame updates the guide.
TEST_F(NamedGuideTest, TestConstrainedFrame) {
  GuideName* test_guide = @"NamedGuideTest";

  UIWindow* window =
      [[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 200, 200)];
  UIView* view = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  [window addSubview:view];

  NamedGuide* guide = [[NamedGuide alloc] initWithName:test_guide];
  [view addLayoutGuide:guide];
  ASSERT_FALSE(guide.constrained);

  // Test updating the guide's `constrainedFrame` to the lower left corner.
  const CGRect kLowerLeftCorner = CGRectMake(0, 50, 50, 50);
  guide.constrainedFrame = kLowerLeftCorner;
  VerifyLayoutFrame(guide, kLowerLeftCorner);
  EXPECT_TRUE(guide.constrained);

  // Tests that updating the view's size stretches the layout frame such that
  // it remains the lower left quadrant.
  guide.autoresizingMask =
      (UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight |
       UIViewAutoresizingFlexibleTopMargin |
       UIViewAutoresizingFlexibleRightMargin);
  view.frame = CGRectMake(0, 0, 200, 200);
  const CGRect kNewLowerLeftCorner = CGRectMake(0, 100, 100, 100);
  VerifyLayoutFrame(guide, kNewLowerLeftCorner);
}

// Tests that setting the `constrainedView` and `constrainedFrame` correctly
// nullify other properties.
TEST_F(NamedGuideTest, TestConstrainedViewFrameMutex) {
  GuideName* test_guide = @"NamedGuideTest";
  UIWindow* window =
      [[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  UIView* view = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  [window addSubview:view];
  UIView* childView = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 50, 100)];
  [view addSubview:childView];
  NamedGuide* guide = [[NamedGuide alloc] initWithName:test_guide];
  [view addLayoutGuide:guide];
  guide.constrainedView = childView;

  // Set the guide's `constrainedFrame` and verify that `constrainedView` is
  // reset to nil.
  const CGRect kConstrainedFrame = CGRectMake(0, 0, 50, 50);
  guide.constrainedFrame = kConstrainedFrame;
  EXPECT_FALSE(guide.constrainedView);
  VerifyLayoutFrame(guide, kConstrainedFrame);

  // Set the guide's `constrainedView` and verify that `constrainedFrame` is
  // reset to CGRectNull.
  guide.constrainedView = childView;
  EXPECT_TRUE(CGRectIsNull(guide.constrainedFrame));
  VerifyLayoutFrame(guide, childView.frame);
}

// Tests that a NamedGuide resets its constraints if its constrained view is
// removed from its owning view's hierarchy.
TEST_F(NamedGuideTest, TestRemoveConstrainedView) {
  GuideName* test_guide = @"NamedGuideTest";

  UIWindow* window =
      [[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  UIView* view = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  [window addSubview:view];
  UIView* subview = [[UIView alloc] initWithFrame:view.bounds];
  [view addSubview:subview];

  NamedGuide* guide = [[NamedGuide alloc] initWithName:test_guide];
  [view addLayoutGuide:guide];
  ASSERT_FALSE(guide.constrained);

  guide.constrainedView = subview;
  EXPECT_TRUE(guide.constrained);
  [subview removeFromSuperview];
  EXPECT_FALSE(guide.constrained);
  EXPECT_FALSE(guide.constrainedView);
}
