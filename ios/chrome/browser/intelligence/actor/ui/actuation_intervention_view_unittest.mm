// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actuation_intervention_view.h"

#import <optional>

#import "ios/chrome/browser/intelligence/actor/ui/actuation_task_card_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_constants.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/intelligence/actor/ui/test/actor_ui_test_utils.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

@interface FakeActuationInterventionViewDelegate
    : NSObject <ActuationInterventionViewDelegate>
@property(nonatomic, assign) std::optional<ActuationInterventionAction>
    triggeredAction;
@end

@implementation FakeActuationInterventionViewDelegate
- (void)interventionView:(ActuationInterventionView*)view
        didTriggerAction:(ActuationInterventionAction)action {
  _triggeredAction = action;
}
@end

namespace {

using intelligence::actor::FindViewByAccessibilityIdentifier;

class ActuationInterventionViewTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    view_ = [[ActuationInterventionView alloc] init];
    delegate_ = [[FakeActuationInterventionViewDelegate alloc] init];
    view_.delegate = delegate_;
  }

  // Returns the card view inside `view_`.
  UIView* CardView() {
    return FindViewByAccessibilityIdentifier(
        view_, kActuationInterventionCardAccessibilityIdentifier);
  }

  // Returns the primary action button inside `view_`.
  ChromeButton* PrimaryButton() {
    return static_cast<ChromeButton*>(FindViewByAccessibilityIdentifier(
        view_, kActuationInterventionPrimaryButtonAccessibilityIdentifier));
  }

  // Returns the secondary action button inside `view_`.
  ChromeButton* SecondaryButton() {
    return static_cast<ChromeButton*>(FindViewByAccessibilityIdentifier(
        view_, kActuationInterventionSecondaryButtonAccessibilityIdentifier));
  }

  // Returns the action button owned by the card layout of `view_`.
  ChromeButton* CardActionButton() {
    return static_cast<ChromeButton*>(FindViewByAccessibilityIdentifier(
        view_, kActuationTaskCardActionButtonAccessibilityIdentifier));
  }

  ActuationInterventionView* view_ = nil;
  FakeActuationInterventionViewDelegate* delegate_ = nil;
};

// Test a card intervention presents the card and dispatches kPrimary.
TEST_F(ActuationInterventionViewTest, TestCardLayout) {
  [view_ configureWithData:[ActuationInterventionData
                               cardItemWithTitle:@"Title"
                                        subtitle:@"Subtitle"
                               primaryButtonText:@"Continue"]];

  UIView* card_view = CardView();
  ASSERT_NE(card_view, nil);
  EXPECT_FALSE(card_view.hidden);
  EXPECT_FALSE(view_.hidden);

  ChromeButton* action_button = CardActionButton();
  ASSERT_NE(action_button, nil);
  EXPECT_NSEQ(action_button.title, @"Continue");

  [action_button sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_EQ(delegate_.triggeredAction, ActuationInterventionAction::kPrimary);
}

// Test a single-button intervention presents only the primary button.
TEST_F(ActuationInterventionViewTest, TestSingleButtonLayout) {
  [view_ configureWithData:[ActuationInterventionData
                               singleButtonItemWithPrimaryButtonText:@"OK"]];

  UIView* card_view = CardView();
  ASSERT_NE(card_view, nil);
  EXPECT_TRUE(card_view.hidden);

  ChromeButton* primary_button = PrimaryButton();
  ASSERT_NE(primary_button, nil);
  EXPECT_FALSE(primary_button.hidden);
  EXPECT_NSEQ(primary_button.title, @"OK");

  ChromeButton* secondary_button = SecondaryButton();
  ASSERT_NE(secondary_button, nil);
  EXPECT_TRUE(secondary_button.hidden);

  [primary_button sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_EQ(delegate_.triggeredAction, ActuationInterventionAction::kPrimary);
}

// Test a dual-button intervention presents both buttons.
TEST_F(ActuationInterventionViewTest, TestDualButtonLayout) {
  [view_ configureWithData:[ActuationInterventionData
                               dualButtonItemWithPrimaryButtonText:@"Confirm"
                                               secondaryButtonText:@"Cancel"]];

  UIView* card_view = CardView();
  ASSERT_NE(card_view, nil);
  EXPECT_TRUE(card_view.hidden);

  ChromeButton* primary_button = PrimaryButton();
  ASSERT_NE(primary_button, nil);
  EXPECT_FALSE(primary_button.hidden);
  EXPECT_NSEQ(primary_button.title, @"Confirm");

  ChromeButton* secondary_button = SecondaryButton();
  ASSERT_NE(secondary_button, nil);
  EXPECT_FALSE(secondary_button.hidden);
  EXPECT_NSEQ(secondary_button.title, @"Cancel");

  [secondary_button sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_EQ(delegate_.triggeredAction, ActuationInterventionAction::kSecondary);

  [primary_button sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_EQ(delegate_.triggeredAction, ActuationInterventionAction::kPrimary);
}

// Test that passing nil hides the view and collapses it to zero height.
TEST_F(ActuationInterventionViewTest, TestNilDataCollapsesView) {
  [view_ configureWithData:[ActuationInterventionData
                               singleButtonItemWithPrimaryButtonText:@"Done"]];
  EXPECT_FALSE(view_.hidden);
  CGSize size = UILayoutFittingCompressedSize;
  EXPECT_GT([view_ systemLayoutSizeFittingSize:size].height, 0.0);

  [view_ configureWithData:nil];
  EXPECT_TRUE(view_.hidden);
  EXPECT_EQ([view_ systemLayoutSizeFittingSize:size].height, 0.0);
}

}  // namespace
