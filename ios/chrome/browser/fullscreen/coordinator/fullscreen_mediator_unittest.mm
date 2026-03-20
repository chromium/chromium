// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/fullscreen/coordinator/fullscreen_mediator.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web/model/web_view_proxy/web_view_proxy_tab_helper.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

// Test fixture for testing FullscreenMediator class.
class FullscreenMediatorTest : public PlatformTest {
 protected:
  FullscreenMediatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    FullscreenBrowserAgent::CreateForBrowser(browser_.get());
    mediator_ = [[FullscreenMediator alloc]
        initWithBrowserAgent:FullscreenBrowserAgent::FromBrowser(browser_.get())
                webStateList:browser_->GetWebStateList()];
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
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
