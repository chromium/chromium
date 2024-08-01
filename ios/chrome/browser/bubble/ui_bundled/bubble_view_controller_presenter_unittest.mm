// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bubble/ui_bundled/bubble_view_controller_presenter.h"

#import <UIKit/UIKit.h>

#import <optional>

#import "base/apple/foundation_util.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_constants.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_unittest_util.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_view.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_view_controller.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_view_controller_presenter+Testing.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Test fixture to test the BubbleViewControllerPresenter.
class BubbleViewControllerPresenterTest : public PlatformTest {
 public:
  BubbleViewControllerPresenterTest()
      : bubble_view_controller_presenter_([[BubbleViewControllerPresenter alloc]
                 initWithText:@"Text"
                        title:@"Title"
                        image:[[UIImage alloc] init]
               arrowDirection:BubbleArrowDirectionUp
                    alignment:BubbleAlignmentCenter
                   bubbleType:BubbleViewTypeRichWithSnooze
            dismissalCallback:^(
                IPHDismissalReasonType reason,
                feature_engagement::Tracker::SnoozeAction action) {
              dismissal_callback_count_++;
              dismissal_callback_action_ = action;
              run_loop_.Quit();
            }]),
        window_([[UIWindow alloc]
            initWithFrame:CGRectMake(0.0, 0.0, 500.0, 500.0)]),
        parent_view_controller_([[UIViewController alloc] init]),
        anchor_point_(CGPointMake(250.0, 250.0)),
        dismissal_callback_count_(0),
        dismissal_callback_action_() {
    parent_view_controller_.view.frame = CGRectMake(0.0, 0.0, 500.0, 500.0);
    [window_ addSubview:parent_view_controller_.view];
  }

  ~BubbleViewControllerPresenterTest() override {
    // Dismiss the bubble, to ensure that its dismissalCallback runs
    // before the test fixture is destroyed.
    [bubble_view_controller_presenter_ dismissAnimated:NO];
  }

 protected:
  // The presenter object under test.
  BubbleViewControllerPresenter* bubble_view_controller_presenter_;
  // The window the `parent_view_controller_`'s view is in.
  // -presentInViewController: expects the `anchorPoint` parameter to be in
  // window coordinates, which requires the `view` property to be in a window.
  UIWindow* window_;
  // The view controller the BubbleViewController is added as a child of.
  UIViewController* parent_view_controller_;
  // The point at which the bubble is anchored.
  CGPoint anchor_point_;
  // How many times `bubble_view_controller_presenter_`'s internal
  // `dismissalCallback` has been invoked. Defaults to 0. Every time the
  // callback is invoked, `dismissal_callback_count_` increments.
  int dismissal_callback_count_;
  std::optional<feature_engagement::Tracker::SnoozeAction>
      dismissal_callback_action_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::RunLoop run_loop_;
};

// Tests that, after initialization, the internal BubbleViewController and
// BubbleView have not been added to the parent.
TEST_F(BubbleViewControllerPresenterTest, InitializedNotAdded) {
  EXPECT_FALSE([parent_view_controller_.childViewControllers
      containsObject:bubble_view_controller_presenter_.bubbleViewController]);
  EXPECT_FALSE([parent_view_controller_.view.subviews
      containsObject:bubble_view_controller_presenter_.bubbleViewController
                         .view]);
}

// Tests that -presentInViewController: adds the BubbleViewController and
// BubbleView to the parent.
TEST_F(BubbleViewControllerPresenterTest, PresentAddsToViewController) {
  [bubble_view_controller_presenter_
      presentInViewController:parent_view_controller_
                  anchorPoint:anchor_point_];
  EXPECT_TRUE([parent_view_controller_.childViewControllers
      containsObject:bubble_view_controller_presenter_.bubbleViewController]);
  EXPECT_TRUE([parent_view_controller_.view.subviews
      containsObject:bubble_view_controller_presenter_.bubbleViewController
                         .view]);
}

// Tests that initially the dismissal callback has not been invoked.
TEST_F(BubbleViewControllerPresenterTest, DismissalCallbackCountInitialized) {
  EXPECT_EQ(0, dismissal_callback_count_);
}

// Tests that presenting the bubble but not dismissing it does not invoke the
// dismissal callback.
TEST_F(BubbleViewControllerPresenterTest, DismissalCallbackNotCalled) {
  [bubble_view_controller_presenter_
      presentInViewController:parent_view_controller_
                  anchorPoint:anchor_point_];
  EXPECT_EQ(0, dismissal_callback_count_);
}

// Tests that presenting then dismissing the bubble invokes the dismissal
// callback.
TEST_F(BubbleViewControllerPresenterTest, DismissalCallbackCalledOnce) {
  [bubble_view_controller_presenter_
      presentInViewController:parent_view_controller_
                  anchorPoint:anchor_point_];
  [bubble_view_controller_presenter_ dismissAnimated:NO];
  EXPECT_EQ(1, dismissal_callback_count_);
}

// Tests that calling -dismissAnimated: after the bubble has already been
// dismissed does not invoke the dismissal callback again.
TEST_F(BubbleViewControllerPresenterTest, DismissalCallbackNotCalledTwice) {
  [bubble_view_controller_presenter_
      presentInViewController:parent_view_controller_
                  anchorPoint:anchor_point_];
  [bubble_view_controller_presenter_ dismissAnimated:NO];
  [bubble_view_controller_presenter_ dismissAnimated:NO];
  EXPECT_EQ(1, dismissal_callback_count_);
}

// Tests that calling -dismissAnimated: before the bubble has been presented
// does not invoke the dismissal callback.
TEST_F(BubbleViewControllerPresenterTest,
       DismissalCallbackNotCalledBeforePresentation) {
  [bubble_view_controller_presenter_ dismissAnimated:NO];
  EXPECT_EQ(0, dismissal_callback_count_);
}

// Tests that the timers are `nil` before the bubble is presented on screen.
TEST_F(BubbleViewControllerPresenterTest, TimersInitiallyNil) {
  EXPECT_EQ(nil, bubble_view_controller_presenter_.bubbleDismissalTimer);
  EXPECT_EQ(nil, bubble_view_controller_presenter_.engagementTimer);
}

// Tests that the timers are not `nil` once the bubble is presented on screen.
TEST_F(BubbleViewControllerPresenterTest, TimersInstantiatedOnPresent) {
  [bubble_view_controller_presenter_
      presentInViewController:parent_view_controller_
                  anchorPoint:anchor_point_];
  EXPECT_NE(nil, bubble_view_controller_presenter_.bubbleDismissalTimer);
  EXPECT_NE(nil, bubble_view_controller_presenter_.engagementTimer);
}

// Tests that the bubble is dismissed automatically after the timer ends.
TEST_F(BubbleViewControllerPresenterTest, BubbleDismissedAfterTimeout) {
  [bubble_view_controller_presenter_
      presentInViewController:parent_view_controller_
                  anchorPoint:anchor_point_];

  task_environment_.FastForwardBy(base::Seconds(kBubbleVisibilityDuration + 1));
  run_loop_.Run();
  EXPECT_EQ(1, dismissal_callback_count_);
}

// Tests that the bubble is not dismissed after the default timeout when a
// custom visibility duration is set.
TEST_F(BubbleViewControllerPresenterTest,
       BubbleDismissedOnlyAfterCustomTimeout) {
  bubble_view_controller_presenter_.customBubbleVisibilityDuration = 8.0;
  [bubble_view_controller_presenter_
      presentInViewController:parent_view_controller_
                  anchorPoint:anchor_point_];

  task_environment_.FastForwardBy(base::Seconds(kBubbleVisibilityDuration + 1));
  run_loop_.RunUntilIdle();
  EXPECT_EQ(0, dismissal_callback_count_);

  task_environment_.FastForwardBy(base::Seconds(3));
  run_loop_.Run();
  EXPECT_EQ(1, dismissal_callback_count_);
}

// Tests that the bubble timer is `nil` but the engagement timer is not `nil`
// when the bubble is presented and dismissed.
TEST_F(BubbleViewControllerPresenterTest, BubbleTimerNilOnDismissal) {
  [bubble_view_controller_presenter_
      presentInViewController:parent_view_controller_
                  anchorPoint:anchor_point_];
  [bubble_view_controller_presenter_ dismissAnimated:NO];
  EXPECT_EQ(nil, bubble_view_controller_presenter_.bubbleDismissalTimer);
  EXPECT_NE(nil, bubble_view_controller_presenter_.engagementTimer);
}

// Tests that the `userEngaged` property is initially `NO`.
TEST_F(BubbleViewControllerPresenterTest, UserEngagedInitiallyNo) {
  EXPECT_FALSE(bubble_view_controller_presenter_.isUserEngaged);
}

// Tests that the `userEngaged` property is `YES` once the bubble is presented
// on screen.
TEST_F(BubbleViewControllerPresenterTest, UserEngagedYesOnPresent) {
  [bubble_view_controller_presenter_
      presentInViewController:parent_view_controller_
                  anchorPoint:anchor_point_];
  EXPECT_TRUE(bubble_view_controller_presenter_.isUserEngaged);
}

// Tests that the `userEngaged` property remains `YES` once the bubble is
// presented and dismissed.
TEST_F(BubbleViewControllerPresenterTest, UserEngagedYesOnDismissal) {
  [bubble_view_controller_presenter_
      presentInViewController:parent_view_controller_
                  anchorPoint:anchor_point_];
  EXPECT_TRUE(bubble_view_controller_presenter_.isUserEngaged);
}

// Tests that tapping the bubble view's close button invoke the dismissal
// callback with a dismiss action.
TEST_F(BubbleViewControllerPresenterTest,
       BubbleViewCloseButtonCallDismissalCallback) {
  BubbleViewControllerPresenter* bubble_view_controller_presenter =
      [[BubbleViewControllerPresenter alloc]
               initWithText:@"Text"
                      title:@"Title"
                      image:[[UIImage alloc] init]
             arrowDirection:BubbleArrowDirectionUp
                  alignment:BubbleAlignmentCenter
                 bubbleType:BubbleViewTypeWithClose
          dismissalCallback:^(
              IPHDismissalReasonType reason,
              feature_engagement::Tracker::SnoozeAction action) {
            dismissal_callback_count_++;
            dismissal_callback_action_ = action;
          }];
  [bubble_view_controller_presenter
      presentInViewController:parent_view_controller_
                  anchorPoint:anchor_point_];
  BubbleView* bubble_view = base::apple::ObjCCastStrict<BubbleView>(
      bubble_view_controller_presenter.bubbleViewController.view);
  EXPECT_TRUE(bubble_view);
  UIButton* close_button = GetCloseButtonFromBubbleView(bubble_view);
  EXPECT_TRUE(close_button);
  [close_button sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_TRUE(dismissal_callback_action_);
  EXPECT_EQ(feature_engagement::Tracker::SnoozeAction::DISMISSED,
            dismissal_callback_action_);
  EXPECT_EQ(1, dismissal_callback_count_);
}

// Tests that tapping the bubble view's snooze button invoke the dismissal
// callback with a snooze action.
TEST_F(BubbleViewControllerPresenterTest,
       BubbleViewSnoozeButtonCallDismissalCallback) {
  [bubble_view_controller_presenter_
      presentInViewController:parent_view_controller_
                  anchorPoint:anchor_point_];
  BubbleView* bubble_view = base::apple::ObjCCastStrict<BubbleView>(
      bubble_view_controller_presenter_.bubbleViewController.view);
  EXPECT_TRUE(bubble_view);
  UIButton* snooze_button = GetSnoozeButtonFromBubbleView(bubble_view);
  EXPECT_TRUE(snooze_button);
  [snooze_button sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_TRUE(dismissal_callback_action_);
  EXPECT_EQ(feature_engagement::Tracker::SnoozeAction::SNOOZED,
            dismissal_callback_action_);
  EXPECT_EQ(1, dismissal_callback_count_);
}

// Tests that all gesture recognizers are attached in the default case.
TEST_F(BubbleViewControllerPresenterTest, BubbleViewGestureRecognizersPresent) {
  [bubble_view_controller_presenter_
      presentInViewController:parent_view_controller_
                  anchorPoint:anchor_point_];
  BubbleView* bubble_view = base::apple::ObjCCastStrict<BubbleView>(
      bubble_view_controller_presenter_.bubbleViewController.view);
  EXPECT_TRUE(bubble_view);
  EXPECT_EQ([[bubble_view gestureRecognizers] count], 1U);
  EXPECT_EQ([[parent_view_controller_.view gestureRecognizers] count], 3U);
}

// Tests that the default gesture recognizers have been removed after the Bubble
// View Controller Presenter was dismissed.
TEST_F(BubbleViewControllerPresenterTest, BubbleViewGestureRecognizersRemoved) {
  [bubble_view_controller_presenter_
      presentInViewController:parent_view_controller_
                  anchorPoint:anchor_point_];
  BubbleView* bubble_view = base::apple::ObjCCastStrict<BubbleView>(
      bubble_view_controller_presenter_.bubbleViewController.view);
  EXPECT_TRUE(bubble_view);
  EXPECT_EQ([[bubble_view gestureRecognizers] count], 1U);
  EXPECT_EQ([[parent_view_controller_.view gestureRecognizers] count], 3U);

  [bubble_view_controller_presenter_ dismissAnimated:NO];
  EXPECT_TRUE(bubble_view);
  EXPECT_EQ([[bubble_view gestureRecognizers] count], 0U);
  EXPECT_EQ([[parent_view_controller_.view gestureRecognizers] count], 0U);
}
