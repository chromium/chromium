// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_controller.h"

#import <optional>

#import "ios/chrome/browser/intelligence/actor/ui/actuation_header_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_task_card_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_constants.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_consumer.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_mutator.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

@interface FakeActuationWorklogMutator : NSObject <ActuationWorklogMutator>
@property(nonatomic, assign) std::optional<ActuationInterventionAction>
    triggeredAction;
@property(nonatomic, assign) BOOL stopActuationCalled;
@end

@implementation FakeActuationWorklogMutator
- (void)stopActuation {
  _stopActuationCalled = YES;
}

- (void)didTriggerInterventionAction:(ActuationInterventionAction)action {
  _triggeredAction = action;
}
@end

namespace {

using chrome_test_util::FindViewById;
using ActuationWorklogViewControllerTest = PlatformTest;

// Test that switching display modes updates subview visibility.
TEST_F(ActuationWorklogViewControllerTest, TestSetDisplayMode) {
  ActuationWorklogViewController* view_controller =
      [[ActuationWorklogViewController alloc] init];

  UIView* compact_view = FindViewById(view_controller.view,
                                      kCompactWorklogAccessibilityIdentifier);
  UIView* full_scroll_view = FindViewById(
      view_controller.view, kFullWorklogScrollViewAccessibilityIdentifier);
  UIView* header_view = FindViewById(view_controller.view,
                                     kActuationHeaderAccessibilityIdentifier);
  ASSERT_NE(header_view, nil);

  // Expanded worklog.
  view_controller.displayMode = ActuationWorklogDisplayModeExpanded;
  EXPECT_EQ(ActuationWorklogDisplayModeExpanded, view_controller.displayMode);
  EXPECT_TRUE(compact_view.hidden);
  EXPECT_FALSE(full_scroll_view.hidden);

  // Compact worklog.
  view_controller.displayMode = ActuationWorklogDisplayModeCompact;
  EXPECT_EQ(ActuationWorklogDisplayModeCompact, view_controller.displayMode);
  EXPECT_FALSE(compact_view.hidden);
  EXPECT_TRUE(full_scroll_view.hidden);

  // Minimized worklog only shows the header.
  view_controller.displayMode = ActuationWorklogDisplayModeMinimized;
  EXPECT_EQ(ActuationWorklogDisplayModeMinimized, view_controller.displayMode);
  EXPECT_TRUE(compact_view.hidden);
  EXPECT_TRUE(full_scroll_view.hidden);
  EXPECT_FALSE(header_view.hidden);
}

// Test toggling actuation active updates container visibility.
TEST_F(ActuationWorklogViewControllerTest, SetActuationActive) {
  ActuationWorklogViewController* view_controller =
      [[ActuationWorklogViewController alloc] init];

  id<ActuationWorklogConsumer> consumer =
      static_cast<id<ActuationWorklogConsumer>>(view_controller);

  // Inactive by default.
  EXPECT_TRUE(view_controller.view.hidden);

  [consumer setActuationActive:YES];
  EXPECT_FALSE(view_controller.view.hidden);

  [consumer setActuationActive:NO];
  EXPECT_TRUE(view_controller.view.hidden);
}

// Test reset clears title and worklog content.
TEST_F(ActuationWorklogViewControllerTest, TestResetClearsContent) {
  ActuationWorklogViewController* view_controller =
      [[ActuationWorklogViewController alloc] init];
  id<ActuationWorklogConsumer> consumer =
      static_cast<id<ActuationWorklogConsumer>>(view_controller);

  // Force view load.
  EXPECT_NE(view_controller.view, nil);

  [consumer setTaskTitle:@"Task Title"];
  ActuationWorklogItem* item =
      [ActuationWorklogItem simpleItemWithTitle:@"Step 1" active:YES];
  [consumer updateWorklogWithItem:item chip:nil animated:NO];
  [consumer reset];

  ActuationHeaderView* header_view = FindViewById<ActuationHeaderView>(
      view_controller.view, kActuationHeaderAccessibilityIdentifier);
  ASSERT_NE(header_view, nil);
  EXPECT_EQ(header_view.title, nil);

  UIScrollView* scroll_view = FindViewById<UIScrollView>(
      view_controller.view, kFullWorklogScrollViewAccessibilityIdentifier);
  ASSERT_NE(scroll_view, nil);
  EXPECT_TRUE(CGPointEqualToPoint(scroll_view.contentOffset, CGPointZero));
}

// Test setting an intervention forwards it to the intervention view, routes the
// triggered action to the mutator, and reset clears it.
TEST_F(ActuationWorklogViewControllerTest, TestInterventionFlow) {
  ActuationWorklogViewController* view_controller =
      [[ActuationWorklogViewController alloc] init];
  FakeActuationWorklogMutator* fake_mutator =
      [[FakeActuationWorklogMutator alloc] init];
  view_controller.mutator = fake_mutator;
  id<ActuationWorklogConsumer> consumer =
      static_cast<id<ActuationWorklogConsumer>>(view_controller);

  // Force view load.
  EXPECT_NE(view_controller.view, nil);

  UIView* card_view = FindViewById(
      view_controller.view, kActuationInterventionCardAccessibilityIdentifier);
  ASSERT_NE(card_view, nil);
  EXPECT_TRUE(card_view.hidden);

  [consumer setIntervention:[ActuationInterventionData
                                cardItemWithTitle:@"Intervention Title"
                                         subtitle:@"Intervention Subtitle"
                                primaryButtonText:@"Continue"]];
  EXPECT_FALSE(card_view.hidden);

  // Verify mutator dispatch on button tap. A card intervention renders the
  // card's own action button, not the standalone intervention button.
  ChromeButton* action_button = FindViewById<ChromeButton>(
      view_controller.view,
      kActuationTaskCardActionButtonAccessibilityIdentifier);
  ASSERT_NE(action_button, nil);
  EXPECT_NSEQ(action_button.title, @"Continue");
  [action_button sendActionsForControlEvents:UIControlEventTouchUpInside];
  ASSERT_TRUE(fake_mutator.triggeredAction.has_value());
  EXPECT_EQ(*fake_mutator.triggeredAction,
            ActuationInterventionAction::kPrimary);

  // Reset clears intervention.
  [consumer reset];
  EXPECT_TRUE(card_view.hidden);
}

}  // namespace
