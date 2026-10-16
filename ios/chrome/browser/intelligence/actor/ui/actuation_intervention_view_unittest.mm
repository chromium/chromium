// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actuation_intervention_view.h"

#import "ios/chrome/browser/intelligence/actor/ui/actuation_task_card_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_constants.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

@interface FakeActuationInterventionViewDelegate
    : NSObject <ActuationInterventionViewDelegate>
@property(nonatomic, strong) ActuationInterventionAction* triggeredAction;
@end

@implementation FakeActuationInterventionViewDelegate
- (void)interventionView:(ActuationInterventionView*)view
        didTriggerAction:(ActuationInterventionAction*)action {
  _triggeredAction = action;
}
@end

namespace {

using chrome_test_util::FindViewById;

// Arbitrary identifier for the test actions.
constexpr NSInteger kActionIdentifier = 0;

class ActuationInterventionViewTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    view_ = [[ActuationInterventionView alloc] init];
    delegate_ = [[FakeActuationInterventionViewDelegate alloc] init];
    view_.delegate = delegate_;
  }

  // Returns an action with `title`.
  ActuationInterventionAction* Action(NSString* title) {
    return [[ActuationInterventionAction alloc]
        initWithIdentifier:kActionIdentifier
                     title:title];
  }

  // Returns the card view inside `view_`.
  UIView* CardView() {
    return FindViewById(view_,
                        kActuationInterventionCardAccessibilityIdentifier);
  }

  // Returns the row button displaying the action at `index`, if any.
  ChromeButton* ActionButton(NSUInteger index) {
    return FindViewById<ChromeButton>(
        view_, ActuationInterventionActionButtonAccessibilityIdentifier(index));
  }

  // Returns the stack view laying out the action buttons.
  UIStackView* ButtonStack() {
    return FindViewById<UIStackView>(
        view_, kActuationInterventionButtonStackAccessibilityIdentifier);
  }

  // Returns the action button owned by the card layout of `view_`.
  ChromeButton* CardActionButton() {
    return FindViewById<ChromeButton>(
        view_, kActuationTaskCardActionButtonAccessibilityIdentifier);
  }

  ActuationInterventionView* view_ = nil;
  FakeActuationInterventionViewDelegate* delegate_ = nil;
};

// Tests that a card intervention presents the card and dispatches its action.
TEST_F(ActuationInterventionViewTest, TestCardLayout) {
  ActuationInterventionAction* action = Action(@"Continue");
  [view_
      configureWithData:[ActuationInterventionData cardItemWithTitle:@"Title"
                                                            subtitle:@"Subtitle"
                                                              action:action]];

  UIView* card_view = CardView();
  ASSERT_NE(card_view, nil);
  EXPECT_FALSE(card_view.hidden);
  EXPECT_FALSE(view_.hidden);

  ChromeButton* action_button = CardActionButton();
  ASSERT_NE(action_button, nil);
  EXPECT_NSEQ(action_button.title, @"Continue");

  [action_button sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_EQ(delegate_.triggeredAction, action);
}

// Tests that a two-action row presents the actions in order with equal
// tertiary emphasis, and dispatches the action of each tapped button.
TEST_F(ActuationInterventionViewTest, TestTwoActionRowLayout) {
  ActuationInterventionAction* cancel_action = Action(@"Cancel");
  ActuationInterventionAction* confirm_action = Action(@"Confirm");
  [view_ configureWithData:[ActuationInterventionData rowItemWithActions:@[
           cancel_action, confirm_action
         ]]];

  EXPECT_TRUE(CardView().hidden);

  ChromeButton* leading_button = ActionButton(0);
  ASSERT_NE(leading_button, nil);
  EXPECT_NSEQ(leading_button.title, @"Cancel");
  EXPECT_EQ(leading_button.style, ChromeButtonStyleTertiary);

  ChromeButton* trailing_button = ActionButton(1);
  ASSERT_NE(trailing_button, nil);
  EXPECT_NSEQ(trailing_button.title, @"Confirm");
  EXPECT_EQ(trailing_button.style, ChromeButtonStyleTertiary);

  [leading_button sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_EQ(delegate_.triggeredAction, cancel_action);

  [trailing_button sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_EQ(delegate_.triggeredAction, confirm_action);
}

// Tests that reconfiguring a row replaces the buttons of the previous
// configuration, styling a single remaining button as primary.
TEST_F(ActuationInterventionViewTest, TestReconfigureReplacesRowButtons) {
  [view_ configureWithData:[ActuationInterventionData rowItemWithActions:@[
           Action(@"Cancel"), Action(@"Confirm")
         ]]];
  [view_ configureWithData:[ActuationInterventionData
                               rowItemWithActions:@[ Action(@"Done") ]]];

  ChromeButton* button = ActionButton(0);
  ASSERT_NE(button, nil);
  EXPECT_NSEQ(button.title, @"Done");
  EXPECT_EQ(button.style, ChromeButtonStylePrimary);
  EXPECT_EQ(ActionButton(1), nil);
}

// Tests that a three-action column stacks the actions vertically in order with
// equal tertiary emphasis, and dispatches the action of the tapped button.
TEST_F(ActuationInterventionViewTest, TestThreeActionColumnLayout) {
  NSArray<ActuationInterventionAction*>* actions = @[
    Action(@"Always allow"), Action(@"Allow this time"),
    Action(@"Manually enter")
  ];
  [view_ configureWithData:[ActuationInterventionData
                               columnItemWithActions:actions]];

  EXPECT_TRUE(CardView().hidden);
  UIStackView* button_stack = ButtonStack();
  ASSERT_NE(button_stack, nil);
  EXPECT_EQ(button_stack.axis, UILayoutConstraintAxisVertical);

  for (NSUInteger index = 0; index < actions.count; ++index) {
    SCOPED_TRACE(index);
    ChromeButton* button = ActionButton(index);
    ASSERT_NE(button, nil);
    EXPECT_NSEQ(button.title, actions[index].title);
    EXPECT_EQ(button.style, ChromeButtonStyleTertiary);
  }

  [ActionButton(1) sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_EQ(delegate_.triggeredAction, actions[1]);
}

// Tests that configuring a row after a column lays the buttons out
// horizontally again.
TEST_F(ActuationInterventionViewTest, TestRowAfterColumnIsHorizontal) {
  [view_ configureWithData:[ActuationInterventionData columnItemWithActions:@[
           Action(@"Allow"), Action(@"Deny")
         ]]];
  [view_ configureWithData:[ActuationInterventionData rowItemWithActions:@[
           Action(@"Allow"), Action(@"Deny")
         ]]];

  UIStackView* button_stack = ButtonStack();
  ASSERT_NE(button_stack, nil);
  EXPECT_EQ(button_stack.axis, UILayoutConstraintAxisHorizontal);
}

// Tests that passing nil hides the view and collapses it to zero height.
TEST_F(ActuationInterventionViewTest, TestNilDataCollapsesView) {
  [view_ configureWithData:[ActuationInterventionData
                               rowItemWithActions:@[ Action(@"Done") ]]];
  EXPECT_FALSE(view_.hidden);
  CGSize size = UILayoutFittingCompressedSize;
  EXPECT_GT([view_ systemLayoutSizeFittingSize:size].height, 0.0);

  [view_ configureWithData:nil];
  EXPECT_TRUE(view_.hidden);
  EXPECT_EQ([view_ systemLayoutSizeFittingSize:size].height, 0.0);
}

}  // namespace
