// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_controller.h"

#import "ios/chrome/browser/intelligence/actor/ui/actuation_task_card_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_constants.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_consumer.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_item_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_mutator.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/intelligence/ui/intelligence_header_item.h"
#import "ios/chrome/browser/intelligence/ui/intelligence_header_view.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

@interface FakeActuationWorklogMutator : NSObject <ActuationWorklogMutator>
@property(nonatomic, strong) ActuationInterventionAction* triggeredAction;
@property(nonatomic, assign) BOOL stopActuationCalled;
@property(nonatomic, assign) BOOL pauseActuationCalled;
@end

@implementation FakeActuationWorklogMutator
- (void)stopActuation {
  _stopActuationCalled = YES;
}

- (void)pauseActuation {
  _pauseActuationCalled = YES;
}

- (void)didTriggerInterventionAction:(ActuationInterventionAction*)action {
  _triggeredAction = action;
}
@end

namespace {

using chrome_test_util::FindViewById;
using chrome_test_util::FindViewsByClass;

// Arbitrary identifier for the test action.
constexpr InterventionActionIdentifier kActionIdentifier =
    InterventionActionIdentifier::kContinue;

// Returns a header item with `identifier` and accessibility identifier
// `accessibility_id`.
IntelligenceHeaderItem* CreateHeaderItem(NSString* identifier,
                                         NSString* accessibility_id) {
  return [IntelligenceHeaderItem itemWithIdentifier:identifier
                                               icon:[[UIImage alloc] init]
                                              title:@"Header item"
                            accessibilityIdentifier:accessibility_id];
}

// Returns header variants displaying `primary_item` in every mode.
ActuationHeaderContentVariants* CreateHeaderContentVariants(
    IntelligenceHeaderItem* primary_item) {
  ActuationHeaderContent* content =
      [[ActuationHeaderContent alloc] initWithTitle:@"Task Title"
                                           subtitle:nil
                                        primaryItem:primary_item];
  return
      [[ActuationHeaderContentVariants alloc] initWithMinimizedContent:content
                                                        compactContent:content
                                                       expandedContent:content];
}

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
  [consumer() setHeaderContentVariants:CreateHeaderContentVariants(nil)];
  ActuationWorklogItem* item =
      [ActuationWorklogItem simpleItemWithTitle:@"Step 1"];
  [consumer() updateWorklogWithItem:item chip:nil animated:NO];
  [consumer() reset];

  IntelligenceHeaderView* header_view = FindViewById<IntelligenceHeaderView>(
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
  IntelligenceHeaderView* header_view = FindViewById<IntelligenceHeaderView>(
      view_controller_.view, kActuationHeaderAccessibilityIdentifier);
  ASSERT_NE(header_view, nil);

  ActuationHeaderContent* minimized =
      [[ActuationHeaderContent alloc] initWithTitle:@"Minimized Title"
                                           subtitle:@"Minimized Subtitle"
                                        primaryItem:nil];
  ActuationHeaderContent* compact =
      [[ActuationHeaderContent alloc] initWithTitle:@"Compact Title"
                                           subtitle:nil
                                        primaryItem:nil];
  ActuationHeaderContent* expanded =
      [[ActuationHeaderContent alloc] initWithTitle:@"Expanded Title"
                                           subtitle:nil
                                        primaryItem:nil];
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

// Tests that setting the displayed intervention again doesn't redraw it, and
// that it can be displayed again after a reset.
TEST_F(ActuationWorklogViewControllerTest, TestSameInterventionIsNotRedrawn) {
  ActuationInterventionAction* action =
      [[ActuationInterventionAction alloc] initWithIdentifier:kActionIdentifier
                                                        title:@"Continue"];
  ActuationInterventionData* intervention =
      [ActuationInterventionData rowItemWithActions:@[ action ]];
  NSString* button_id =
      ActuationInterventionActionButtonAccessibilityIdentifier(0);

  [consumer() setIntervention:intervention];
  UIView* button = FindViewById(view_controller_.view, button_id);
  ASSERT_NE(button, nil);

  [consumer() setIntervention:intervention];
  EXPECT_EQ(FindViewById(view_controller_.view, button_id), button);

  [consumer() reset];
  EXPECT_EQ(FindViewById(view_controller_.view, button_id), nil);
  [consumer() setIntervention:intervention];
  EXPECT_NE(FindViewById(view_controller_.view, button_id), nil);
}

// Tests that the header shows the close button without variants, displays the
// primary item of the current display mode (or none when nil), routes its taps
// to the mutator by identifier, keeps the button while the identifier is
// unchanged, and that reset restores the close button.
TEST_F(ActuationWorklogViewControllerTest, TestHeaderPrimaryItem) {
  UIButton* fallback_close_button = FindViewById<UIButton>(
      view_controller_.view,
      kActuationHeaderCloseButtonAccessibilityIdentifier);
  ASSERT_NE(fallback_close_button, nil);
  [fallback_close_button
      sendActionsForControlEvents:UIControlEventPrimaryActionTriggered];
  EXPECT_TRUE(fake_mutator_.stopActuationCalled);
  fake_mutator_.stopActuationCalled = NO;

  IntelligenceHeaderItem* pause_item =
      CreateHeaderItem(kActuationHeaderPauseItemIdentifier,
                       kActuationHeaderPauseButtonAccessibilityIdentifier);
  IntelligenceHeaderItem* close_item =
      CreateHeaderItem(kActuationHeaderCloseItemIdentifier,
                       kActuationHeaderCloseButtonAccessibilityIdentifier);

  [consumer() setHeaderContentVariants:CreateHeaderContentVariants(pause_item)];
  UIButton* pause_button = FindViewById<UIButton>(
      view_controller_.view,
      kActuationHeaderPauseButtonAccessibilityIdentifier);
  ASSERT_NE(pause_button, nil);
  [pause_button
      sendActionsForControlEvents:UIControlEventPrimaryActionTriggered];
  EXPECT_TRUE(fake_mutator_.pauseActuationCalled);
  EXPECT_FALSE(fake_mutator_.stopActuationCalled);

  [consumer() setHeaderContentVariants:
                  CreateHeaderContentVariants(CreateHeaderItem(
                      kActuationHeaderPauseItemIdentifier,
                      kActuationHeaderPauseButtonAccessibilityIdentifier))];
  EXPECT_EQ(FindViewById(view_controller_.view,
                         kActuationHeaderPauseButtonAccessibilityIdentifier),
            pause_button);

  [consumer() setHeaderContentVariants:CreateHeaderContentVariants(close_item)];
  EXPECT_EQ(FindViewById(view_controller_.view,
                         kActuationHeaderPauseButtonAccessibilityIdentifier),
            nil);
  UIButton* close_button = FindViewById<UIButton>(
      view_controller_.view,
      kActuationHeaderCloseButtonAccessibilityIdentifier);
  ASSERT_NE(close_button, nil);
  [close_button
      sendActionsForControlEvents:UIControlEventPrimaryActionTriggered];
  EXPECT_TRUE(fake_mutator_.stopActuationCalled);

  [consumer() setHeaderContentVariants:CreateHeaderContentVariants(nil)];
  EXPECT_EQ(FindViewById(view_controller_.view,
                         kActuationHeaderCloseButtonAccessibilityIdentifier),
            nil);
  EXPECT_EQ(FindViewById(view_controller_.view,
                         kActuationHeaderPauseButtonAccessibilityIdentifier),
            nil);

  ActuationHeaderContent* minimized =
      [[ActuationHeaderContent alloc] initWithTitle:@"Task Title"
                                           subtitle:nil
                                        primaryItem:close_item];
  ActuationHeaderContent* compact =
      [[ActuationHeaderContent alloc] initWithTitle:@"Task Title"
                                           subtitle:nil
                                        primaryItem:pause_item];
  [consumer() setHeaderContentVariants:[[ActuationHeaderContentVariants alloc]
                                           initWithMinimizedContent:minimized
                                                     compactContent:compact
                                                    expandedContent:compact]];
  EXPECT_NE(FindViewById(view_controller_.view,
                         kActuationHeaderPauseButtonAccessibilityIdentifier),
            nil);
  EXPECT_EQ(FindViewById(view_controller_.view,
                         kActuationHeaderCloseButtonAccessibilityIdentifier),
            nil);

  view_controller_.displayMode = ActuationWorklogDisplayModeMinimized;
  EXPECT_EQ(FindViewById(view_controller_.view,
                         kActuationHeaderPauseButtonAccessibilityIdentifier),
            nil);
  EXPECT_NE(FindViewById(view_controller_.view,
                         kActuationHeaderCloseButtonAccessibilityIdentifier),
            nil);

  view_controller_.displayMode = ActuationWorklogDisplayModeCompact;
  [consumer() reset];
  EXPECT_EQ(FindViewById(view_controller_.view,
                         kActuationHeaderPauseButtonAccessibilityIdentifier),
            nil);
  EXPECT_NE(FindViewById(view_controller_.view,
                         kActuationHeaderCloseButtonAccessibilityIdentifier),
            nil);
}

// Tests that only the trailing item of an ongoing actuation is current in the
// full worklog, without the items being replaced, and that the compact worklog
// displays the trailing item as current.
TEST_F(ActuationWorklogViewControllerTest, TestOnlyTrailingItemIsCurrent) {
  UIView* full_scroll_view = FindViewById(
      view_controller_.view, kFullWorklogScrollViewAccessibilityIdentifier);
  UIView* compact_view = FindViewById(view_controller_.view,
                                      kCompactWorklogAccessibilityIdentifier);
  ASSERT_NE(full_scroll_view, nil);
  ASSERT_NE(compact_view, nil);
  ActuationWorklogChip* chip =
      [[ActuationWorklogChip alloc] initWithText:@"Clicking" icon:nil];

  [consumer() setActuationActive:YES];
  [consumer()
      updateWorklogWithItem:[ActuationWorklogItem simpleItemWithTitle:@"Step 1"]
                       chip:nil
                   animated:NO];
  [consumer()
      updateWorklogWithItem:[ActuationWorklogItem simpleItemWithTitle:@"Step 2"]
                       chip:chip
                   animated:NO];

  NSArray<ActuationWorklogItemView*>* full_item_views =
      FindViewsByClass<ActuationWorklogItemView>(full_scroll_view);
  ASSERT_EQ(full_item_views.count, 2u);
  EXPECT_FALSE(full_item_views[0].current);
  EXPECT_TRUE(full_item_views[1].current);
  NSArray<ActuationWorklogItemView*>* compact_item_views =
      FindViewsByClass<ActuationWorklogItemView>(compact_view);
  ASSERT_EQ(compact_item_views.count, 1u);
  EXPECT_TRUE(compact_item_views[0].current);

  [consumer() setActuationActive:NO];

  EXPECT_EQ(
      FindViewsByClass<ActuationWorklogItemView>(full_scroll_view).lastObject,
      full_item_views[1]);
  EXPECT_FALSE(full_item_views[1].current);
}

}  // namespace
