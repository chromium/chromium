// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_item_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Records accessory taps forwarded by `ActuationWorklogView`.
@interface FakeActuationWorklogViewDelegate
    : NSObject <ActuationWorklogViewDelegate>
@property(nonatomic, strong) ActuationWorklogView* tappedWorklogView;
@property(nonatomic, strong) ActuationWorklogAccessoryItem* tappedAccessoryItem;
@end

@implementation FakeActuationWorklogViewDelegate

- (void)worklogView:(ActuationWorklogView*)worklogView
    didTapAccessoryItem:(ActuationWorklogAccessoryItem*)accessoryItem {
  self.tappedWorklogView = worklogView;
  self.tappedAccessoryItem = accessoryItem;
}

@end

namespace {

using chrome_test_util::FindViewsByClass;

class ActuationWorklogViewTest : public PlatformTest {
 protected:
  ActuationWorklogViewTest() {
    worklog_view_ = [[ActuationWorklogView alloc] initWithFrame:CGRectZero];
    delegate_ = [[FakeActuationWorklogViewDelegate alloc] init];
    worklog_view_.delegate = delegate_;
  }

  void TearDown() override {
    worklog_view_ = nil;
    delegate_ = nil;
    PlatformTest::TearDown();
  }

  ActuationWorklogView* worklog_view_;
  FakeActuationWorklogViewDelegate* delegate_;
};

// Tests that collapsing the worklog hides intermediate items, keeps the first
// and in-progress trailing items visible, and hides the trailing item once
// actuation ends.
TEST_F(ActuationWorklogViewTest, CollapseUpdatesItemVisibility) {
  ActuationWorklogItem* first =
      [ActuationWorklogItem simpleItemWithTitle:@"Task started"];
  ActuationWorklogItem* middle =
      [ActuationWorklogItem simpleItemWithTitle:@"Step 1"];
  ActuationWorklogItem* last =
      [ActuationWorklogItem simpleItemWithTitle:@"Step 2"];

  [worklog_view_ setItems:@[ first, middle, last ]];
  NSArray<ActuationWorklogItemView*>* views =
      FindViewsByClass<ActuationWorklogItemView>(worklog_view_);
  ASSERT_EQ(3u, views.count);
  EXPECT_TRUE(views[0].isCollapsible);
  EXPECT_FALSE(views[0].hidden);
  EXPECT_FALSE(views[1].hidden);
  EXPECT_FALSE(views[2].hidden);

  // While actuating, collapsing keeps the first and last items visible and
  // hides the middle one.
  worklog_view_.collapsed = YES;
  EXPECT_TRUE(worklog_view_.isCollapsed);
  EXPECT_TRUE(views[0].collapsed);
  EXPECT_FALSE(views[0].hidden);
  EXPECT_TRUE(views[1].hidden);
  EXPECT_FALSE(views[2].hidden);

  // Ending actuation hides the trailing item while collapsed.
  worklog_view_.actuating = NO;
  EXPECT_FALSE(views[0].hidden);
  EXPECT_TRUE(views[1].hidden);
  EXPECT_TRUE(views[2].hidden);
}

// Tests that only the trailing item is current while actuating, and that it
// stops being current once actuation ends, without the items being replaced.
TEST_F(ActuationWorklogViewTest, OnlyTrailingItemIsCurrentWhileActuating) {
  [worklog_view_ addItem:[ActuationWorklogItem simpleItemWithTitle:@"Step 1"]];
  [worklog_view_ addItem:[ActuationWorklogItem simpleItemWithTitle:@"Step 2"]];
  [worklog_view_ setChip:[[ActuationWorklogChip alloc] initWithText:@"Clicking"
                                                               icon:nil]];

  NSArray<ActuationWorklogItemView*>* views =
      FindViewsByClass<ActuationWorklogItemView>(worklog_view_);
  ASSERT_EQ(2u, views.count);
  EXPECT_FALSE(views[0].current);
  EXPECT_TRUE(views[1].current);

  worklog_view_.actuating = NO;

  EXPECT_EQ(
      views[1],
      FindViewsByClass<ActuationWorklogItemView>(worklog_view_).lastObject);
  EXPECT_FALSE(views[1].current);
}

// Tests that tapping the first item toggles the collapsed state.
TEST_F(ActuationWorklogViewTest, TappingFirstItemTogglesCollapsed) {
  [worklog_view_ setItems:@[
    [ActuationWorklogItem simpleItemWithTitle:@"Task started"],
    [ActuationWorklogItem simpleItemWithTitle:@"Step 1"],
  ]];
  NSArray<ActuationWorklogItemView*>* views =
      FindViewsByClass<ActuationWorklogItemView>(worklog_view_);
  ASSERT_EQ(2u, views.count);
  EXPECT_FALSE(worklog_view_.isCollapsed);

  [views[0].delegate worklogItemViewDidTapItem:views[0]];
  EXPECT_TRUE(worklog_view_.isCollapsed);

  [views[0].delegate worklogItemViewDidTapItem:views[0]];
  EXPECT_FALSE(worklog_view_.isCollapsed);
}

// Tests that accessory taps on item views are forwarded to `delegate`.
TEST_F(ActuationWorklogViewTest, ForwardsAccessoryTapToDelegate) {
  ActuationWorklogAccessoryItem* accessory =
      [[ActuationWorklogAccessoryItem alloc] initWithIcon:nil
                                                    title:@"user@example.com"
                                                 subtitle:nil
                                               detailText:nil
                                               hasChevron:NO];
  ActuationWorklogItem* card =
      [ActuationWorklogItem cardItemWithTitle:@"Sign in"
                                     subtitle:nil
                                         icon:nil
                                accessoryItem:accessory];
  [worklog_view_ setItems:@[ card ]];
  NSArray<ActuationWorklogItemView*>* views =
      FindViewsByClass<ActuationWorklogItemView>(worklog_view_);
  ASSERT_EQ(1u, views.count);

  [views[0].delegate worklogItemView:views[0] didTapAccessoryItem:accessory];
  EXPECT_EQ(worklog_view_, delegate_.tappedWorklogView);
  EXPECT_EQ(accessory, delegate_.tappedAccessoryItem);
}

// Tests that a taller host pinning the worklog's bottom edge below
// `UILayoutPriorityDefaultLow`, like the Gemini SDK custom content cell, does
// not stretch the worklog past its content height.
TEST_F(ActuationWorklogViewTest, DoesNotStretchInTallerHost) {
  const CGSize kHostSize = CGSizeMake(320, 1000);
  [worklog_view_ setItems:@[
    [ActuationWorklogItem simpleItemWithTitle:@"Task started"],
    [ActuationWorklogItem simpleItemWithTitle:@"Step 1"],
  ]];
  const CGFloat content_height =
      [worklog_view_
            systemLayoutSizeFittingSize:CGSizeMake(kHostSize.width,
                                                   UILayoutFittingCompressedSize
                                                       .height)
          withHorizontalFittingPriority:UILayoutPriorityRequired
                verticalFittingPriority:UILayoutPriorityFittingSizeLevel]
          .height;
  ASSERT_GT(content_height, 0);
  ASSERT_LT(content_height, kHostSize.height);

  UIView* host = [[UIView alloc]
      initWithFrame:CGRectMake(0, 0, kHostSize.width, kHostSize.height)];
  worklog_view_.translatesAutoresizingMaskIntoConstraints = NO;
  [host addSubview:worklog_view_];
  NSLayoutConstraint* bottom_constraint =
      [worklog_view_.bottomAnchor constraintEqualToAnchor:host.bottomAnchor];
  bottom_constraint.priority = UILayoutPriorityDefaultLow - 1;
  [NSLayoutConstraint activateConstraints:@[
    [worklog_view_.topAnchor constraintEqualToAnchor:host.topAnchor],
    [worklog_view_.leadingAnchor constraintEqualToAnchor:host.leadingAnchor],
    [worklog_view_.trailingAnchor constraintEqualToAnchor:host.trailingAnchor],
    bottom_constraint,
  ]];
  [host layoutIfNeeded];

  // Tolerates pixel rounding of the laid out frame.
  EXPECT_NEAR(content_height, CGRectGetHeight(worklog_view_.frame), 1.0);
}

}  // namespace
