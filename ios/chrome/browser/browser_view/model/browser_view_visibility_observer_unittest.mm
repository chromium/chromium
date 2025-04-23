// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_view/model/browser_view_visibility_audience.h"
#import "ios/chrome/browser/browser_view/model/browser_view_visibility_notifier_browser_agent.h"
#import "ios/chrome/browser/browser_view/model/browser_view_visibility_observer_bridge.h"
#import "ios/chrome/browser/browser_view/public/browser_view_visibility_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

@interface TestBrowserViewVisibilityObserver
    : NSObject <BrowserViewVisibilityObserving>
@property(nonatomic, assign) BrowserViewVisibilityState currentStateValue;
@property(nonatomic, assign) BrowserViewVisibilityState previousStateValue;
@end

@implementation TestBrowserViewVisibilityObserver

- (void)browserViewDidChangeToVisibilityState:
            (BrowserViewVisibilityState)currentState
                                    fromState:(BrowserViewVisibilityState)
                                                  previousState {
  self.currentStateValue = currentState;
  self.previousStateValue = previousState;
}

@end

/// Test fixfure for browser view visibility observer.
class BrowserViewVisibilityObserverTest : public PlatformTest {
 protected:
  BrowserViewVisibilityObserverTest() {
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
TEST_F(BrowserViewVisibilityObserverTest,
       TestObserverRespondsToBrowserViewVisbilityChange) {
  using enum BrowserViewVisibilityState;

  TestBrowserViewVisibilityObserver* observer =
      [[TestBrowserViewVisibilityObserver alloc] init];
  auto observer_bridge =
      std::make_unique<BrowserViewVisibilityObserverBridge>(observer);
  raw_ptr<BrowserViewVisibilityNotifierBrowserAgent> notifier =
      visibility_notifier();
  notifier->AddObserver(observer_bridge.get());

  id<BrowserViewVisibilityAudience> audience =
      notifier->GetBrowserViewVisibilityAudience();
  [audience browserViewDidTransitionToVisibilityState:kVisible
                                            fromState:kAppearing];
  EXPECT_EQ(observer.currentStateValue, BrowserViewVisibilityState::kVisible);
  EXPECT_EQ(observer.previousStateValue,
            BrowserViewVisibilityState::kAppearing);

  notifier->RemoveObserver(observer_bridge.get());
}
