// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar_container/collapsing_toolbar_height_constraint.h"

#import "ios/chrome/browser/ui/toolbar_container/toolbar_collapsing.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// A view with a settable intrinsic height.
@interface IntrinsicHeightView : UIView
@property(nonatomic, assign) CGFloat intrinsicHeight;
@end

@implementation IntrinsicHeightView
@synthesize intrinsicHeight = _intrinsicHeight;
- (CGSize)intrinsicContentSize {
  return CGSizeMake(UIViewNoIntrinsicMetric, _intrinsicHeight);
}
@end

// A view with a settable expanded and collapsed height.
@interface CollapsingView : UIView<ToolbarCollapsing>
@property(nonatomic, assign, readwrite) CGFloat expandedToolbarHeight;
@property(nonatomic, assign, readwrite) CGFloat collapsedToolbarHeight;
@end

@implementation CollapsingView
@synthesize expandedToolbarHeight = _expandedToolbarHeight;
@synthesize collapsedToolbarHeight = _collapsedToolbarHeight;
@end

// Test fixture for CollapsingToolbarHeightConstraint.
class CollapsingToolbarHeightConstraintTest : public PlatformTest {
 public:
  CollapsingToolbarHeightConstraintTest()
      : PlatformTest(),
        container_(
            [[UIView alloc] initWithFrame:CGRectMake(0.0, 0.0, 300.0, 1000.0)]),
        constraints_([[NSMutableArray alloc] init]) {}

  ~CollapsingToolbarHeightConstraintTest() override {
    [NSLayoutConstraint deactivateConstraints:constraints_];
  }

  // Sets the progress on |constraint| and forces a layout so the changes take
  // effect.
  void SetProgress(CollapsingToolbarHeightConstraint* constraint,
                   CGFloat progress) {
    constraint.progress = progress;
    [container_ setNeedsLayout];
    [container_ layoutIfNeeded];
  }

  // Adds |view| to |container_| using constraints to hug the top, leading, and
  // trailing sides.  The return value is an activated constraint that can be
  // used to update the height.
  CollapsingToolbarHeightConstraint* AddViewToContainer(UIView* view) {
    view.translatesAutoresizingMaskIntoConstraints = NO;
    [container_ addSubview:view];
    AddSameConstraintsToSides(
        container_, view,
        LayoutSides::kLeading | LayoutSides::kTop | LayoutSides::kTrailing);
    CollapsingToolbarHeightConstraint* constraint =
        [CollapsingToolbarHeightConstraint constraintWithView:view];
    constraint.active = YES;
    [constraints_ addObject:constraint];
    return constraint;
  }

 private:
  UIView* container_ = nil;
  NSMutableArray<NSLayoutConstraint*>* constraints_ = nil;
};

// Tests interpolating the height value of a collapsing view.
TEST_F(CollapsingToolbarHeightConstraintTest, CollapsingConstraint) {
  CollapsingView* view = [[CollapsingView alloc] initWithFrame:CGRectZero];
  view.expandedToolbarHeight = 100.0;
  view.collapsedToolbarHeight = 50.0;
  CollapsingToolbarHeightConstraint* constraint = AddViewToContainer(view);
  SetProgress(constraint, 1.0);
  EXPECT_EQ(CGRectGetHeight(view.bounds), 100.0);
  SetProgress(constraint, 0.5);
  EXPECT_EQ(CGRectGetHeight(view.bounds), 75.0);
  SetProgress(constraint, 0.0);
  EXPECT_EQ(CGRectGetHeight(view.bounds), 50.0);
}

// Tests interpolating the height value of a non-collapsing view.
TEST_F(CollapsingToolbarHeightConstraintTest, NonCollapsingConstraint) {
  IntrinsicHeightView* view =
      [[IntrinsicHeightView alloc] initWithFrame:CGRectZero];
  view.intrinsicHeight = 80.0;
  CollapsingToolbarHeightConstraint* constraint = AddViewToContainer(view);
  SetProgress(constraint, 1.0);
  EXPECT_EQ(CGRectGetHeight(view.bounds), 80.0);
  SetProgress(constraint, 0.5);
  EXPECT_EQ(CGRectGetHeight(view.bounds), 80.0);
  SetProgress(constraint, 0.0);
  EXPECT_EQ(CGRectGetHeight(view.bounds), 80.0);
}

// Tests a collapsing additional height.
TEST_F(CollapsingToolbarHeightConstraintTest, AdditionalHeight) {
  CollapsingView* view = [[CollapsingView alloc] initWithFrame:CGRectZero];
  view.expandedToolbarHeight = 100.0;
  view.collapsedToolbarHeight = 50.0;
  CollapsingToolbarHeightConstraint* constraint = AddViewToContainer(view);
  constraint.additionalHeight = 100.0;
  constraint.collapsesAdditionalHeight = YES;
  SetProgress(constraint, 1.0);
  EXPECT_EQ(CGRectGetHeight(view.bounds), 200.0);
  SetProgress(constraint, 0.5);
  EXPECT_EQ(CGRectGetHeight(view.bounds), 125.0);
  SetProgress(constraint, 0.0);
  EXPECT_EQ(CGRectGetHeight(view.bounds), 50.0);
  constraint.collapsesAdditionalHeight = NO;
  SetProgress(constraint, 1.0);
  EXPECT_EQ(CGRectGetHeight(view.bounds), 200.0);
  SetProgress(constraint, 0.5);
  EXPECT_EQ(CGRectGetHeight(view.bounds), 175.0);
  SetProgress(constraint, 0.0);
  EXPECT_EQ(CGRectGetHeight(view.bounds), 150.0);
}
