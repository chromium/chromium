// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_view/model/browser_view_visibility_notifier_browser_agent.h"

#import "base/test/test_future.h"
#import "ios/chrome/browser/browser_view/model/browser_view_visibility_notifier_browser_agent.h"
#import "ios/chrome/browser/browser_view/public/browser_view_visibility_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

/// Test fixture for BrowserViewVisibilityNotifierBrowserAgent.
class BrowserViewVisibilityNotifierBrowserAgentTest : public PlatformTest {
 protected:
  BrowserViewVisibilityNotifierBrowserAgentTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    BrowserViewVisibilityNotifierBrowserAgent::CreateForBrowser(browser_.get());
  }

  BrowserViewVisibilityNotifierBrowserAgent* visibility_notifier() {
    return BrowserViewVisibilityNotifierBrowserAgent::FromBrowser(
        browser_.get());
  }

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
};

/// Tests that the observer responds to browser view visibility change.
TEST_F(BrowserViewVisibilityNotifierBrowserAgentTest,
       TestObserverRespondsToBrowserViewVisbilityChange) {
  using Future = base::test::TestFuture<BrowserViewVisibilityState,
                                        BrowserViewVisibilityState>;

  BrowserViewVisibilityNotifierBrowserAgent* notifier = visibility_notifier();
  BrowserViewVisibilityStateChangedCallback callback =
      notifier->GetNotificationCallback();

  // Future used to store the parameters passed to the callback when it is
  // invoked (can be used to check they have the expected value).
  Future future;

  base::CallbackListSubscription subscription =
      notifier->RegisterBrowserVisibilityStateChangedCallback(
          future.GetRepeatingCallback());
  ASSERT_FALSE(future.IsReady());

  callback.Run(BrowserViewVisibilityState::kVisible,
               BrowserViewVisibilityState::kAppearing);

  // Verify that the callback is invoked immediately with the expected values.
  EXPECT_TRUE(future.IsReady());
  auto [current_state, previous_state] = future.Take();
  EXPECT_EQ(current_state, BrowserViewVisibilityState::kVisible);
  EXPECT_EQ(previous_state, BrowserViewVisibilityState::kAppearing);

  // Verify that the callback is no longer called after the subcription
  // is destroyed.
  ASSERT_FALSE(future.IsReady());
  subscription = {};

  callback.Run(BrowserViewVisibilityState::kVisible,
               BrowserViewVisibilityState::kAppearing);

  EXPECT_FALSE(future.IsReady());
}
