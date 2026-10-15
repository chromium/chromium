// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_controller.h"

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
@property(nonatomic, strong) ActuationInterventionAction* triggeredAction;
@property(nonatomic, assign) BOOL stopActuationCalled;
@end

@implementation FakeActuationWorklogMutator
- (void)stopActuation {
  _stopActuationCalled = YES;
}

- (void)didTriggerInterventionAction:(ActuationInterventionAction*)action {
  _triggeredAction = action;
}
@end

namespace {

using chrome_test_util::FindViewById;

// Arbitrary identifier for the test action.
constexpr NSInteger kActionIdentifier = 0;

class ActuationWorklogViewControllerTest : public PlatformTest {
 protected:
  ActuationWorklogViewControllerTest() {
    view_controller_ = [[ActuationWorklogViewController alloc] init];
    fake_mutator_ = [[FakeActuationWorklogMutator alloc] init];
    view_controller_.mutator = fake_mutator_;
    [view_controller_ loadViewIfNeeded];
  }

  // Returns the view controller through the interface used by the mediator.
  id<ActuationWorklogConsumer> consumer() { return view_controller_; }

  ActuationWorklogViewController* view_controller_;
  // Held strongly since `mutator` is a weak property.
  FakeActuationWorklogMutator* fake_mutator_;
};

// Tests that switching display modes updates subview visibility.
TEST_F(ActuationWorklogViewControllerTest, TestSetDisplayMode) {
  UIView* compact_view = FindViewById(view_controller_.view,
                                      kCompactWorklogAccessibilityIdentifier);
  UIView* full_scroll_view = FindViewById(
      view_controller_.view, kFullWorklogScrollViewAccessibilityIdentifier);
  UIView* header_view = FindViewById(view_controller_.view,
                                     kActuationHeaderAccessibilityIdentifier);
  ASSERT_NE(header_view, nil);

  // Expanded worklog.
  view_controller_.displayMode = ActuationWorklogDisplayModeExpanded;
  EXPECT_EQ(ActuationWorklogDisplayModeExpanded, view_controller_.displayMode);
  EXPECT_TRUE(compact_view.hidden);
  EXPECT_FALSE(full_scroll_view.hidden);

  // Compact worklog.
  view_controller_.displayMode = ActuationWorklogDisplayModeCompact;
  EXPECT_EQ(ActuationWorklogDisplayModeCompact, view_controller_.displayMode);
  EXPECT_FALSE(compact_view.hidden);
  EXPECT_TRUE(full_scroll_view.hidden);

  // Minimized worklog only shows the header.
  view_controller_.displayMode = ActuationWorklogDisplayModeMinimized;
  EXPECT_EQ(ActuationWorklogDisplayModeMinimized, view_controller_.displayMode);
  EXPECT_TRUE(compact_view.hidden);
  EXPECT_TRUE(full_scroll_view.hidden);
  EXPECT_FALSE(header_view.hidden);
}

// Tests that toggling actuation active updates container visibility.
TEST_F(ActuationWorklogViewControllerTest, SetActuationActive) {
  // Inactive by default.
  EXPECT_TRUE(view_controller_.view.hidden);

  [consumer() setActuationActive:YES];
  EXPECT_FALSE(view_controller_.view.hidden);

  [consumer() setActuationActive:NO];
  EXPECT_TRUE(view_controller_.view.hidden);
}

// Tests that reset clears title and worklog content.
TEST_F(ActuationWorklogViewControllerTest, TestResetClearsContent) {
  ActuationHeaderContent* header_content =
      [[ActuationHeaderContent alloc] initWithTitle:@"Task Title" subtitle:nil];
  [consumer()
      setHeaderContentVariants:[[ActuationHeaderContentVariants alloc]
                                   initWithMinimizedContent:header_content
                                             compactContent:header_content
                                            expandedContent:header_content]];
  ActuationWorklogItem* item =
      [ActuationWorklogItem simpleItemWithTitle:@"Step 1" active:YES];
  [consumer() updateWorklogWithItem:item chip:nil animated:NO];
  [consumer() reset];

  ActuationHeaderView* header_view = FindViewById<ActuationHeaderView>(
      view_controller_.view, kActuationHeaderAccessibilityIdentifier);
  ASSERT_NE(header_view, nil);
  EXPECT_EQ(header_view.title, nil);

  UIScrollView* scroll_view = FindViewById<UIScrollView>(
      view_controller_.view, kFullWorklogScrollViewAccessibilityIdentifier);
  ASSERT_NE(scroll_view, nil);
  EXPECT_TRUE(CGPointEqualToPoint(scroll_view.contentOffset, CGPointZero));
}

// Tests that the header displays the content variant matching the display
// mode, whether the variants or the display mode change.
TEST_F(ActuationWorklogViewControllerTest, TestHeaderContentVariants) {
  ActuationHeaderView* header_view = FindViewById<ActuationHeaderView>(
      view_controller_.view, kActuationHeaderAccessibilityIdentifier);
  ASSERT_NE(header_view, nil);

  ActuationHeaderContent* minimized =
      [[ActuationHeaderContent alloc] initWithTitle:@"Minimized Title"
                                           subtitle:@"Minimized Subtitle"];
  ActuationHeaderContent* compact =
      [[ActuationHeaderContent alloc] initWithTitle:@"Compact Title"
                                           subtitle:nil];
  ActuationHeaderContent* expanded =
      [[ActuationHeaderContent alloc] initWithTitle:@"Expanded Title"
                                           subtitle:nil];
  [consumer() setHeaderContentVariants:[[ActuationHeaderContentVariants alloc]
                                           initWithMinimizedContent:minimized
                                                     compactContent:compact
                                                    expandedContent:expanded]];

  // Defaults to the compact display mode.
  EXPECT_NSEQ(header_view.title, @"Compact Title");
  EXPECT_EQ(header_view.subtitle, nil);

  view_controller_.displayMode = ActuationWorklogDisplayModeMinimized;
  EXPECT_NSEQ(header_view.title, @"Minimized Title");
  EXPECT_NSEQ(header_view.subtitle, @"Minimized Subtitle");

  view_controller_.displayMode = ActuationWorklogDisplayModeExpanded;
  EXPECT_NSEQ(header_view.title, @"Expanded Title");
  EXPECT_EQ(header_view.subtitle, nil);
}

// Tests that setting an intervention forwards it to the intervention view,
// routes the triggered action to the mutator, and reset clears it.
TEST_F(ActuationWorklogViewControllerTest, TestInterventionFlow) {
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

  // Verify mutator dispatch on button tap. A card intervention renders the
  // card's own action button, not the standalone intervention button.
  ChromeButton* action_button = FindViewById<ChromeButton>(
      view_controller_.view,
      kActuationTaskCardActionButtonAccessibilityIdentifier);
  ASSERT_NE(action_button, nil);
  EXPECT_NSEQ(action_button.title, @"Continue");
  [action_button sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_EQ(fake_mutator_.triggeredAction, action);

  // Reset clears intervention.
  [consumer() reset];
  EXPECT_TRUE(card_view.hidden);
}

}  // namespace
