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
#import "ios/chrome/browser/intelligence/actor/ui/test/actor_ui_test_utils.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
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

using intelligence::actor::FindViewByAccessibilityIdentifier;
using ActuationWorklogViewControllerTest = PlatformTest;

// Test toggling compact mode updates subview visibility.
TEST_F(ActuationWorklogViewControllerTest, ToggleCompact) {
  ActuationWorklogViewController* view_controller =
      [[ActuationWorklogViewController alloc] init];

  UIView* compact_view = FindViewByAccessibilityIdentifier(
      view_controller.view, kCompactWorklogAccessibilityIdentifier);
  UIView* full_scroll_view = FindViewByAccessibilityIdentifier(
      view_controller.view, kFullWorklogScrollViewAccessibilityIdentifier);

  // Expanded worklog.
  view_controller.compact = NO;
  EXPECT_FALSE(view_controller.isCompact);
  EXPECT_TRUE(compact_view.hidden);
  EXPECT_FALSE(full_scroll_view.hidden);

  // Compact worklog.
  view_controller.compact = YES;
  EXPECT_TRUE(view_controller.isCompact);
  EXPECT_FALSE(compact_view.hidden);
  EXPECT_TRUE(full_scroll_view.hidden);
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

  ActuationHeaderView* header_view =
      static_cast<ActuationHeaderView*>(FindViewByAccessibilityIdentifier(
          view_controller.view, kActuationHeaderAccessibilityIdentifier));
  ASSERT_NE(header_view, nil);
  EXPECT_EQ(header_view.title, nil);

  UIScrollView* scroll_view =
      static_cast<UIScrollView*>(FindViewByAccessibilityIdentifier(
          view_controller.view, kFullWorklogScrollViewAccessibilityIdentifier));
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

  UIView* card_view = FindViewByAccessibilityIdentifier(
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
  ChromeButton* action_button =
      static_cast<ChromeButton*>(FindViewByAccessibilityIdentifier(
          view_controller.view,
          kActuationTaskCardActionButtonAccessibilityIdentifier));
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
