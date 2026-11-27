// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actuation_intervention_view_controller.h"

#import "ios/chrome/browser/intelligence/actor/ui/actuation_intervention_mutator.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_task_card_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_constants.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

@interface FakeActuationInterventionMutator
    : NSObject <ActuationInterventionMutator>
@property(nonatomic, strong) ActuationInterventionAction* triggeredAction;
@end

@implementation FakeActuationInterventionMutator
- (void)didTriggerInterventionAction:(ActuationInterventionAction*)action {
  _triggeredAction = action;
}
@end

@interface FakeActuationInterventionViewControllerDelegate
    : NSObject <ActuationInterventionViewControllerDelegate>
@property(nonatomic, assign) NSUInteger heightChangeCount;
@end

@implementation FakeActuationInterventionViewControllerDelegate
- (void)interventionViewControllerDidChangeHeight:
    (ActuationInterventionViewController*)viewController {
  _heightChangeCount++;
}
@end

namespace {

using chrome_test_util::FindViewById;

// Arbitrary identifier for the test action.
constexpr InterventionActionIdentifier kActionIdentifier =
    InterventionActionIdentifier::kContinue;

// Creates an intervention displaying a row with a single action.
ActuationInterventionData* CreateRowIntervention() {
  ActuationInterventionAction* action =
      [[ActuationInterventionAction alloc] initWithIdentifier:kActionIdentifier
                                                        title:@"Continue"];
  return [ActuationInterventionData rowItemWithActions:@[ action ]];
}

class ActuationInterventionViewControllerTest : public PlatformTest {
 protected:
  ActuationInterventionViewControllerTest() {
    view_controller_ = [[ActuationInterventionViewController alloc] init];
    fake_mutator_ = [[FakeActuationInterventionMutator alloc] init];
    fake_delegate_ =
        [[FakeActuationInterventionViewControllerDelegate alloc] init];
    view_controller_.mutator = fake_mutator_;
    view_controller_.delegate = fake_delegate_;
    [view_controller_ loadViewIfNeeded];
  }

  // Returns the view controller through the interface used by the mediator.
  id<ActuationInterventionConsumer> consumer() { return view_controller_; }

  ActuationInterventionViewController* view_controller_;
  // Held strongly since `mutator` and `delegate` are weak properties.
  FakeActuationInterventionMutator* fake_mutator_;
  FakeActuationInterventionViewControllerDelegate* fake_delegate_;
};

// Tests that setting an intervention displays it, routes the triggered action
// to the mutator, and that setting nil hides it.
TEST_F(ActuationInterventionViewControllerTest, TestInterventionFlow) {
  UIView* card_view = FindViewById(
      view_controller_.view, kActuationInterventionCardAccessibilityIdentifier);
  ASSERT_NE(card_view, nil);
  EXPECT_TRUE(card_view.hidden);

  ActuationInterventionAction* action =
      [[ActuationInterventionAction alloc] initWithIdentifier:kActionIdentifier
                                                        title:@"Continue"];
  [consumer() setIntervention:[ActuationInterventionData
                                  cardItemWithTitle:@"Intervention Title"
                                           subtitle:@"Intervention Subtitle"
                                             action:action]];
  EXPECT_FALSE(card_view.hidden);

  // A card intervention renders the card's own action button, not the
  // standalone intervention button.
  ChromeButton* action_button = FindViewById<ChromeButton>(
      view_controller_.view,
      kActuationTaskCardActionButtonAccessibilityIdentifier);
  ASSERT_NE(action_button, nil);
  EXPECT_NSEQ(action_button.title, @"Continue");
  [action_button sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_EQ(fake_mutator_.triggeredAction, action);

  [consumer() setIntervention:nil];
  EXPECT_TRUE(card_view.hidden);
}

// Tests that setting the displayed intervention again doesn't redraw it, and
// that it can be displayed again after being dismissed.
TEST_F(ActuationInterventionViewControllerTest,
       TestSameInterventionIsNotRedrawn) {
  ActuationInterventionData* intervention = CreateRowIntervention();
  NSString* button_id =
      ActuationInterventionActionButtonAccessibilityIdentifier(0);

  [consumer() setIntervention:intervention];
  UIView* button = FindViewById(view_controller_.view, button_id);
  ASSERT_NE(button, nil);

  [consumer() setIntervention:intervention];
  EXPECT_EQ(FindViewById(view_controller_.view, button_id), button);

  [consumer() setIntervention:nil];
  EXPECT_EQ(FindViewById(view_controller_.view, button_id), nil);
  [consumer() setIntervention:intervention];
  EXPECT_NE(FindViewById(view_controller_.view, button_id), nil);
}

// Tests that the fitting height is zero without an intervention, and that the
// delegate is notified only when the displayed intervention changes.
TEST_F(ActuationInterventionViewControllerTest, TestFittingHeight) {
  view_controller_.view.frame = CGRectMake(0, 0, 300, 0);
  EXPECT_EQ([view_controller_ fittingHeight], 0.0);

  ActuationInterventionData* intervention = CreateRowIntervention();
  [consumer() setIntervention:intervention];
  EXPECT_GT([view_controller_ fittingHeight], 0.0);
  EXPECT_EQ(fake_delegate_.heightChangeCount, 1u);

  [consumer() setIntervention:intervention];
  EXPECT_EQ(fake_delegate_.heightChangeCount, 1u);

  [consumer() setIntervention:nil];
  EXPECT_EQ([view_controller_ fittingHeight], 0.0);
  EXPECT_EQ(fake_delegate_.heightChangeCount, 2u);
}

}  // namespace
