// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/fullscreen/coordinator/fullscreen_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent.h"
#import "ios/chrome/browser/omnibox/model/omnibox_position/omnibox_position_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web/model/web_view_proxy/web_view_proxy_tab_helper.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

// Observer to track calls to WillUpdateObscuredInsetRange.
class FullscreenMediatorTestObserver : public FullscreenBrowserAgentObserver {
 public:
  void WillUpdateObscuredInsetRange(FullscreenBrowserAgent* agent) override {
    will_update_obscured_inset_range_count_++;
  }

  int will_update_obscured_inset_range_count() const {
    return will_update_obscured_inset_range_count_;
  }

 private:
  int will_update_obscured_inset_range_count_ = 0;
};

}  // namespace

// Test fixture for testing FullscreenMediator class.
class FullscreenMediatorTest : public PlatformTest {
 protected:
  FullscreenMediatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    FullscreenBrowserAgent::CreateForBrowser(browser_.get());
    OmniboxPositionBrowserAgent::CreateForBrowser(browser_.get());
    agent_ = FullscreenBrowserAgent::FromBrowser(browser_.get());
    agent_->AddObserver(&observer_);
    mediator_ = [[FullscreenMediator alloc]
               initWithBrowserAgent:agent_
                       webStateList:browser_->GetWebStateList()
        omniboxPositionBrowserAgent:OmniboxPositionBrowserAgent::FromBrowser(
                                        browser_.get())];
  }

  void TearDown() override {
    agent_->RemoveObserver(&observer_);
    [mediator_ disconnect];
  }

  base::test::TaskEnvironment task_environment_;

  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<FullscreenBrowserAgent> agent_;
  FullscreenMediatorTestObserver observer_;
  FullscreenMediator* mediator_;
};

// Tests that the mediator can be disconnected.
TEST_F(FullscreenMediatorTest, Disconnect) {
  [mediator_ disconnect];
}

// Tests that the mediator starts observing the scroll view proxy when the
// active web state changes.
TEST_F(FullscreenMediatorTest, ObservesScrollViewProxy) {
  auto web_state = std::make_unique<web::FakeWebState>();
  WebViewProxyTabHelper::CreateForWebState(web_state.get());

  id scroll_view_proxy_mock = OCMClassMock([CRWWebViewScrollViewProxy class]);
  id web_view_proxy_mock = OCMProtocolMock(@protocol(CRWWebViewProxy));
  OCMStub([web_view_proxy_mock scrollViewProxy])
      .andReturn(scroll_view_proxy_mock);

  WebViewProxyTabHelper::FromWebState(web_state.get())
      ->SetOverridingWebViewProxy(web_view_proxy_mock);

  OCMExpect([scroll_view_proxy_mock
      addObserver:static_cast<id<CRWWebViewScrollViewProxyObserver>>(
                      mediator_)]);

  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::Automatic().Activate());

  [scroll_view_proxy_mock verify];
  [mediator_ disconnect];
}

// Tests that webStateWasShown invalidates the inset range only once on startup.
TEST_F(FullscreenMediatorTest, WebStateWasShownInvalidatesInsetsOnce) {
  auto web_state = std::make_unique<web::FakeWebState>();
  WebViewProxyTabHelper::CreateForWebState(web_state.get());
  web::FakeWebState* web_state_ptr = web_state.get();

  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::Automatic().Activate());

  int initial_count = observer_.will_update_obscured_inset_range_count();

  // First WebState shown should invalidate.
  id<CRWWebStateObserver> web_state_observer =
      static_cast<id<CRWWebStateObserver>>(mediator_);
  [web_state_observer webStateWasShown:web_state_ptr];
  EXPECT_EQ(observer_.will_update_obscured_inset_range_count(),
            initial_count + 1);

  // Subsequent calls should NOT invalidate.
  [web_state_observer webStateWasShown:web_state_ptr];
  EXPECT_EQ(observer_.will_update_obscured_inset_range_count(),
            initial_count + 1);
}
