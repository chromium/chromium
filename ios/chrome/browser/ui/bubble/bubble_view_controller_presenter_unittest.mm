// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/bubble/bubble_unittest_util.h"
#import "ios/chrome/browser/ui/bubble/bubble_view.h"
#import "ios/chrome/browser/ui/bubble/bubble_view_controller.h"
#import "ios/chrome/browser/ui/bubble/bubble_view_controller_presenter+private.h"
#import "ios/chrome/browser/ui/bubble/bubble_view_controller_presenter.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/abseil-cpp/absl/types/optional.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture to test the BubbleViewControllerPresenter.
class BubbleViewControllerPresenterTest : public PlatformTest {
 public:
  BubbleViewControllerPresenterTest()
      : bubbleViewControllerPresenter_([[BubbleViewControllerPresenter alloc]
                 initWithText:@"Text"
                        title:@"Title"
                        image:[[UIImage alloc] init]
               arrowDirection:BubbleArrowDirectionUp
                    alignment:BubbleAlignmentCenter
                   bubbleType:BubbleViewTypeRichWithSnooze
            dismissalCallback:^(
                feature_engagement::Tracker::SnoozeAction action) {
              dismissalCallbackCount_++;
              dismissalCallbackAction_ = action;
            }]),
        window_([[UIWindow alloc]
            initWithFrame:CGRectMake(0.0, 0.0, 500.0, 500.0)]),
        parentViewController_([[UIViewController alloc] init]),
        anchorPoint_(CGPointMake(250.0, 250.0)),
        dismissalCallbackCount_(0),
        dismissalCallbackAction_() {
    parentViewController_.view.frame = CGRectMake(0.0, 0.0, 500.0, 500.0);
    [window_ addSubview:parentViewController_.view];
  }

  ~BubbleViewControllerPresenterTest() override {
    // Dismiss the bubble, to ensure that its dismissalCallback runs
    // before the test fixture is destroyed.
    [bubbleViewControllerPresenter_ dismissAnimated:NO];
  }

 protected:
  // The presenter object under test.
  BubbleViewControllerPresenter* bubbleViewControllerPresenter_;
  // The window the `parentViewController_`'s view is in.
  // -presentInViewController: expects the `anchorPoint` parameter to be in
  // window coordinates, which requires the `view` property to be in a window.
  UIWindow* window_;
  // The view controller the BubbleViewController is added as a child of.
  UIViewController* parentViewController_;
  // The point at which the bubble is anchored.
  CGPoint anchorPoint_;
  // How many times `bubbleViewControllerPresenter_`'s internal
  // `dismissalCallback` has been invoked. Defaults to 0. Every time the
  // callback is invoked, `dismissalCallbackCount_` increments.
  int dismissalCallbackCount_;
  absl::optional<feature_engagement::Tracker::SnoozeAction>
      dismissalCallbackAction_;
};

// Tests that, after initialization, the internal BubbleViewController and
// BubbleView have not been added to the parent.
TEST_F(BubbleViewControllerPresenterTest, InitializedNotAdded) {
  EXPECT_FALSE([parentViewController_.childViewControllers
      containsObject:bubbleViewControllerPresenter_.bubbleViewController]);
  EXPECT_FALSE([parentViewController_.view.subviews
      containsObject:bubbleViewControllerPresenter_.bubbleViewController.view]);
}

// Tests that -presentInViewController: adds the BubbleViewController and
// BubbleView to the parent.
TEST_F(BubbleViewControllerPresenterTest, PresentAddsToViewController) {
  [bubbleViewControllerPresenter_
      presentInViewController:parentViewController_
                         view:parentViewController_.view
                  anchorPoint:anchorPoint_];
  EXPECT_TRUE([parentViewController_.childViewControllers
      containsObject:bubbleViewControllerPresenter_.bubbleViewController]);
  EXPECT_TRUE([parentViewController_.view.subviews
      containsObject:bubbleViewControllerPresenter_.bubbleViewController.view]);
}

// Tests that initially the dismissal callback has not been invoked.
TEST_F(BubbleViewControllerPresenterTest, DismissalCallbackCountInitialized) {
  EXPECT_EQ(0, dismissalCallbackCount_);
}

// Tests that presenting the bubble but not dismissing it does not invoke the
// dismissal callback.
TEST_F(BubbleViewControllerPresenterTest, DismissalCallbackNotCalled) {
  [bubbleViewControllerPresenter_
      presentInViewController:parentViewController_
                         view:parentViewController_.view
                  anchorPoint:anchorPoint_];
  EXPECT_EQ(0, dismissalCallbackCount_);
}

// Tests that presenting then dismissing the bubble invokes the dismissal
// callback.
TEST_F(BubbleViewControllerPresenterTest, DismissalCallbackCalledOnce) {
  [bubbleViewControllerPresenter_
      presentInViewController:parentViewController_
                         view:parentViewController_.view
                  anchorPoint:anchorPoint_];
  [bubbleViewControllerPresenter_ dismissAnimated:NO];
  EXPECT_EQ(1, dismissalCallbackCount_);
}

// Tests that calling -dismissAnimated: after the bubble has already been
// dismissed does not invoke the dismissal callback again.
TEST_F(BubbleViewControllerPresenterTest, DismissalCallbackNotCalledTwice) {
  [bubbleViewControllerPresenter_
      presentInViewController:parentViewController_
                         view:parentViewController_.view
                  anchorPoint:anchorPoint_];
  [bubbleViewControllerPresenter_ dismissAnimated:NO];
  [bubbleViewControllerPresenter_ dismissAnimated:NO];
  EXPECT_EQ(1, dismissalCallbackCount_);
}

// Tests that calling -dismissAnimated: before the bubble has been presented
// does not invoke the dismissal callback.
TEST_F(BubbleViewControllerPresenterTest,
       DismissalCallbackNotCalledBeforePresentation) {
  [bubbleViewControllerPresenter_ dismissAnimated:NO];
  EXPECT_EQ(0, dismissalCallbackCount_);
}

// Tests that the timers are `nil` before the bubble is presented on screen.
TEST_F(BubbleViewControllerPresenterTest, TimersInitiallyNil) {
  EXPECT_EQ(nil, bubbleViewControllerPresenter_.bubbleDismissalTimer);
  EXPECT_EQ(nil, bubbleViewControllerPresenter_.engagementTimer);
}

// Tests that the timers are not `nil` once the bubble is presented on screen.
TEST_F(BubbleViewControllerPresenterTest, TimersInstantiatedOnPresent) {
  [bubbleViewControllerPresenter_
      presentInViewController:parentViewController_
                         view:parentViewController_.view
                  anchorPoint:anchorPoint_];
  EXPECT_NE(nil, bubbleViewControllerPresenter_.bubbleDismissalTimer);
  EXPECT_NE(nil, bubbleViewControllerPresenter_.engagementTimer);
}

// Tests that the bubble timer is `nil` but the engagement timer is not `nil`
// when the bubble is presented and dismissed.
TEST_F(BubbleViewControllerPresenterTest, BubbleTimerNilOnDismissal) {
  [bubbleViewControllerPresenter_
      presentInViewController:parentViewController_
                         view:parentViewController_.view
                  anchorPoint:anchorPoint_];
  [bubbleViewControllerPresenter_ dismissAnimated:NO];
  EXPECT_EQ(nil, bubbleViewControllerPresenter_.bubbleDismissalTimer);
  EXPECT_NE(nil, bubbleViewControllerPresenter_.engagementTimer);
}

// Tests that the `userEngaged` property is initially `NO`.
TEST_F(BubbleViewControllerPresenterTest, UserEngagedInitiallyNo) {
  EXPECT_FALSE(bubbleViewControllerPresenter_.isUserEngaged);
}

// Tests that the `userEngaged` property is `YES` once the bubble is presented
// on screen.
TEST_F(BubbleViewControllerPresenterTest, UserEngagedYesOnPresent) {
  [bubbleViewControllerPresenter_
      presentInViewController:parentViewController_
                         view:parentViewController_.view
                  anchorPoint:anchorPoint_];
  EXPECT_TRUE(bubbleViewControllerPresenter_.isUserEngaged);
}

// Tests that the `userEngaged` property remains `YES` once the bubble is
// presented and dismissed.
TEST_F(BubbleViewControllerPresenterTest, UserEngagedYesOnDismissal) {
  [bubbleViewControllerPresenter_
      presentInViewController:parentViewController_
                         view:parentViewController_.view
                  anchorPoint:anchorPoint_];
  EXPECT_TRUE(bubbleViewControllerPresenter_.isUserEngaged);
}

// Tests that tapping the bubble view's close button invoke the dismissal
// callback with a dismiss action.
TEST_F(BubbleViewControllerPresenterTest,
       BubbleViewCloseButtonCallDismissalCallback) {
  [bubbleViewControllerPresenter_
      presentInViewController:parentViewController_
                         view:parentViewController_.view
                  anchorPoint:anchorPoint_];
  BubbleView* bubbleView = base::mac::ObjCCastStrict<BubbleView>(
      bubbleViewControllerPresenter_.bubbleViewController.view);
  EXPECT_TRUE(bubbleView);
  UIButton* closeButton = GetCloseButtonFromBubbleView(bubbleView);
  EXPECT_TRUE(closeButton);
  [closeButton sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_TRUE(dismissalCallbackAction_);
  EXPECT_EQ(feature_engagement::Tracker::SnoozeAction::DISMISSED,
            dismissalCallbackAction_);
  EXPECT_EQ(1, dismissalCallbackCount_);
}

// Tests that tapping the bubble view's snooze button invoke the dismissal
// callback with a snooze action.
TEST_F(BubbleViewControllerPresenterTest,
       BubbleViewSnoozeButtonCallDismissalCallback) {
  [bubbleViewControllerPresenter_
      presentInViewController:parentViewController_
                         view:parentViewController_.view
                  anchorPoint:anchorPoint_];
  BubbleView* bubbleView = base::mac::ObjCCastStrict<BubbleView>(
      bubbleViewControllerPresenter_.bubbleViewController.view);
  EXPECT_TRUE(bubbleView);
  UIButton* snoozeButton = GetSnoozeButtonFromBubbleView(bubbleView);
  EXPECT_TRUE(snoozeButton);
  [snoozeButton sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_TRUE(dismissalCallbackAction_);
  EXPECT_EQ(feature_engagement::Tracker::SnoozeAction::SNOOZED,
            dismissalCallbackAction_);
  EXPECT_EQ(1, dismissalCallbackCount_);
}
