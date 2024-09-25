// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overscroll_actions/model/overscroll_actions_tab_helper.h"

#import <UIKit/UIKit.h>

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/overscroll_actions/ui_bundled/overscroll_actions_controller.h"
#import "ios/chrome/browser/overscroll_actions/ui_bundled/overscroll_actions_view.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/test/fakes/fake_overscroll_actions_controller_delegate.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Test fixture for OverscrollActionsTabHelper class.
class OverscrollActionsTabHelperTest : public PlatformTest {
 protected:
  OverscrollActionsTabHelperTest()
      : profile_(TestProfileIOS::Builder().Build()),
        overscroll_delegate_(
            [[FakeOverscrollActionsControllerDelegate alloc] init]),
        scroll_view_proxy_([[CRWWebViewScrollViewProxy alloc] init]),
        ui_scroll_view_([[UIScrollView alloc] init]) {
    OverscrollActionsTabHelper::CreateForWebState(&web_state_);
    [scroll_view_proxy_ setScrollView:ui_scroll_view_];
    id web_view_proxy_mock = OCMProtocolMock(@protocol(CRWWebViewProxy));
    [[[web_view_proxy_mock stub] andReturn:scroll_view_proxy_] scrollViewProxy];
    web_state_.SetWebViewProxy(web_view_proxy_mock);
    // Setting insets to imitate having omnibox & toolbar.
    scroll_view_proxy_.contentInset = UIEdgeInsetsMake(40, 0, 82, 0);
  }

  OverscrollActionsTabHelper* overscroll_tab_helper() {
    return OverscrollActionsTabHelper::FromWebState(&web_state_);
  }

  UIView* action_view() {
    return overscroll_delegate_.headerView.subviews.firstObject;
  }

  // Simulates scroll on the `scroll_view_proxy_` view, which should trigger
  // page refresh action.
  void SimulatePullForRefreshAction() {
    [ui_scroll_view_.delegate scrollViewWillBeginDragging:ui_scroll_view_];
    // Wait until scroll action is allowed. There is no condition to wait, just
    // a time period.
    base::test::ios::SpinRunLoopWithMinDelay(
        kMinimumPullDurationToTransitionToReady);
    [ui_scroll_view_.delegate scrollViewDidScroll:ui_scroll_view_];
    // Scroll to content offset below action threshold to cancel bounce
    // animation.
    scroll_view_proxy_.contentOffset = CGPointMake(0, -56);
    // Scroll past action threshold to trigger refresh action.
    scroll_view_proxy_.contentOffset = CGPointMake(0, -293);
    CGPoint target_offset = CGPointMake(0, -92);
    [ui_scroll_view_.delegate scrollViewWillEndDragging:ui_scroll_view_
                                           withVelocity:CGPointMake(0, -1.5)
                                    targetContentOffset:&target_offset];
    [overscroll_delegate_.headerView layoutIfNeeded];
    [ui_scroll_view_.delegate scrollViewDidEndDragging:ui_scroll_view_
                                        willDecelerate:NO];
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ProfileIOS> profile_;
  web::FakeWebState web_state_;
  FakeOverscrollActionsControllerDelegate* overscroll_delegate_;
  CRWWebViewScrollViewProxy* scroll_view_proxy_;
  UIScrollView* ui_scroll_view_;
};

// Tests that OverscrollActionsControllerDelegate is set correctly and triggered
// When there is a view pull.
TEST_F(OverscrollActionsTabHelperTest, TestDelegateTrigger) {
  web_state_.SetBrowserState(profile_.get());
  overscroll_tab_helper()->SetDelegate(overscroll_delegate_);
  // Start pull for page refresh action.
  SimulatePullForRefreshAction();

  // Wait for the layout calls and the delegate call.
  using base::test::ios::kWaitForUIElementTimeout;
  using base::test::ios::WaitUntilConditionOrTimeout;
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return overscroll_delegate_.selectedAction == OverscrollAction::REFRESH;
  }));
}

// Tests that overscrolls actions view style is set correctly, for regular
// browsing profile.
TEST_F(OverscrollActionsTabHelperTest, TestRegularProfileStyle) {
  web_state_.SetBrowserState(profile_.get());
  overscroll_tab_helper()->SetDelegate(overscroll_delegate_);
  SimulatePullForRefreshAction();
  UIColor* expected_color = [UIColor colorNamed:kBackgroundColor];
  EXPECT_TRUE(action_view());
  EXPECT_NSEQ(expected_color, action_view().backgroundColor);
}

// Tests that overscrolls actions view style is set correctly, for off the
// record profile.
TEST_F(OverscrollActionsTabHelperTest, TestOffTheRecordProfileStyle) {
  web_state_.SetBrowserState(profile_->GetOffTheRecordProfile());
  overscroll_tab_helper()->SetDelegate(overscroll_delegate_);
  SimulatePullForRefreshAction();
  // For iOS 13 and dark mode, the incognito overscroll actions view uses a
  // dynamic color.
  UIColor* expected_color = [UIColor colorNamed:kBackgroundColor];
  EXPECT_TRUE(action_view());
  EXPECT_NSEQ(expected_color, action_view().backgroundColor);
}

// Tests that overscroll state is reset when Clear() is called.
TEST_F(OverscrollActionsTabHelperTest, TestClear) {
  web_state_.SetBrowserState(profile_.get());
  overscroll_tab_helper()->SetDelegate(overscroll_delegate_);
  OverscrollActionsController* controller =
      overscroll_tab_helper()->GetOverscrollActionsController();
  EXPECT_EQ(OverscrollState::NO_PULL_STARTED, controller.overscrollState);
  SimulatePullForRefreshAction();
  EXPECT_EQ(OverscrollState::ACTION_READY, controller.overscrollState);
  overscroll_tab_helper()->Clear();
  EXPECT_EQ(OverscrollState::NO_PULL_STARTED, controller.overscrollState);
}
