// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/open_in/open_in_mediator.h"

#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/ui/open_in/open_in_toolbar.h"
#include "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for testing OpenInMediator class.
class OpenInMediatorTest : public PlatformTest {
 protected:
  OpenInMediatorTest()
      : web_state_list_(&web_state_list_delegate_),
        browser_state_(TestChromeBrowserState::Builder().Build()),
        mediator_(
            [[OpenInMediator alloc] initWithWebStateList:&web_state_list_]) {}

  std::unique_ptr<web::TestWebState> CreateWebStateWithView() {
    auto web_state = std::make_unique<web::TestWebState>();
    CGRect web_view_frame = CGRectMake(0, 0, 100, 100);
    UIView* web_state_view = [[UIView alloc] initWithFrame:web_view_frame];
    web_view_proxy_mock = OCMProtocolMock(@protocol(CRWWebViewProxy));
    [[[web_view_proxy_mock stub] andReturn:scroll_view_proxy_] scrollViewProxy];
    web_state->SetWebViewProxy(web_view_proxy_mock);

    web_state->SetView(web_state_view);
    web_state->SetBrowserState(browser_state_.get());
    return web_state;
  }

  web::WebTaskEnvironment task_environment_;
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  id web_view_proxy_mock;
  CRWWebViewScrollViewProxy* scroll_view_proxy_;
  OpenInMediator* mediator_;
};

// Tests that enabling openIn adds openInToolBar to the WebState view.
TEST_F(OpenInMediatorTest, EnableForWebState) {
  auto web_state = CreateWebStateWithView();
  [mediator_ enableOpenInForWebState:web_state.get()
                     withDocumentURL:GURL::EmptyGURL()
                   suggestedFileName:@""];

  EXPECT_EQ(1U, web_state->GetView().subviews.count);
  EXPECT_TRUE([web_state->GetView().subviews.firstObject
      isKindOfClass:[OpenInToolbar class]]);
  [mediator_ disableAll];
}

// Tests that disabling openIn removes openInToolBar from the WebState view.
TEST_F(OpenInMediatorTest, DisableForWebState) {
  auto web_state = CreateWebStateWithView();

  [mediator_ enableOpenInForWebState:web_state.get()
                     withDocumentURL:GURL::EmptyGURL()
                   suggestedFileName:@""];
  ASSERT_EQ(1U, web_state->GetView().subviews.count);
  [mediator_ disableOpenInForWebState:web_state.get()];
  EXPECT_FALSE(web_state->GetView().subviews.count);
  [mediator_ disableAll];
}

// Tests that OpenInMediator can handle multiple WebStates correctly.
TEST_F(OpenInMediatorTest, MultipleWebStates) {
  auto web_state_1 = CreateWebStateWithView();
  [mediator_ enableOpenInForWebState:web_state_1.get()
                     withDocumentURL:GURL::EmptyGURL()
                   suggestedFileName:@""];
  EXPECT_EQ(1U, web_state_1->GetView().subviews.count);
  UIView* open_in_view_1 = web_state_1->GetView().subviews.firstObject;
  EXPECT_TRUE([open_in_view_1 isKindOfClass:[OpenInToolbar class]]);

  auto web_state_2 = CreateWebStateWithView();
  [mediator_ enableOpenInForWebState:web_state_2.get()
                     withDocumentURL:GURL::EmptyGURL()
                   suggestedFileName:@""];

  EXPECT_EQ(1U, web_state_2->GetView().subviews.count);
  UIView* open_in_view_2 = web_state_2->GetView().subviews.firstObject;
  EXPECT_TRUE([open_in_view_2 isKindOfClass:[OpenInToolbar class]]);
  EXPECT_NSNE(open_in_view_1, open_in_view_2);

  // Verify that destroy will detach the OpenIn view from the WebState.
  [mediator_ destroyOpenInForWebState:web_state_1.get()];
  EXPECT_FALSE(web_state_1->GetView().subviews.count);

  // Verify that destroying OpenIn for |web_state_1| doesn't affect
  // |web_state_2|.
  EXPECT_EQ(1U, web_state_2->GetView().subviews.count);

  // Verify that calling disableAll remove any remaining views.
  [mediator_ disableAll];
  EXPECT_FALSE(web_state_2->GetView().subviews.count);
}
