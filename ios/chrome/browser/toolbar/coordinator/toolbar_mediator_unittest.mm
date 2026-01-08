// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/coordinator/toolbar_mediator.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_consumer.h"
#import "ios/chrome/browser/web/model/web_navigation_browser_agent.h"
#import "ios/chrome/browser/web/model/web_navigation_util.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

// Fixture for testing ToolbarMediator.
class ToolbarMediatorTest : public PlatformTest {
 protected:
  ToolbarMediatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    WebNavigationBrowserAgent::CreateForBrowser(browser_.get());

    mediator_ = [[ToolbarMediator alloc]
        initWithWebStateList:browser_->GetWebStateList()];
    mediator_.navigationBrowserAgent =
        WebNavigationBrowserAgent::FromBrowser(browser_.get());

    consumer_ = OCMProtocolMock(@protocol(ToolbarConsumer));
    [mediator_ setConsumer:consumer_];
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  ToolbarMediator* mediator_;
  id consumer_;
};

// Tests that selecting a web state updates the consumer.
TEST_F(ToolbarMediatorTest, TestWebStateSelectionUpdatesConsumer) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetVisibleURL(GURL("https://example.com"));
  auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
  navigation_manager->AddItem(GURL("https://example.com/1"),
                              ui::PAGE_TRANSITION_TYPED);
  navigation_manager->AddItem(GURL("https://example.com/2"),
                              ui::PAGE_TRANSITION_TYPED);
  navigation_manager->SetLastCommittedItemIndex(1);
  web_state->SetNavigationManager(std::move(navigation_manager));

  OCMExpect([consumer_ setCanGoBack:YES]);
  OCMExpect([consumer_ setCanGoForward:NO]);
  OCMExpect([consumer_ setLocationBarText:[OCMArg any]]);
  OCMExpect([consumer_ setShareEnabled:YES]);
  OCMExpect([consumer_ setIsLoading:NO]);

  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that web state updates are sent to the consumer.
TEST_F(ToolbarMediatorTest, TestWebStateUpdates) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetVisibleURL(GURL("https://example.com"));
  auto navigation_manager = std::make_unique<web::FakeNavigationManager>();

  navigation_manager->AddItem(GURL("https://example.com/1"),
                              ui::PAGE_TRANSITION_TYPED);
  navigation_manager->AddItem(GURL("https://example.com/2"),
                              ui::PAGE_TRANSITION_TYPED);
  navigation_manager->AddItem(GURL("https://example.com/3"),
                              ui::PAGE_TRANSITION_TYPED);

  web_state->SetNavigationManager(std::move(navigation_manager));
  web::FakeWebState* fake_web_state = web_state.get();

  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  // Test loading state.
  OCMExpect([consumer_ setIsLoading:YES]);
  fake_web_state->SetLoading(true);
  EXPECT_OCMOCK_VERIFY(consumer_);

  OCMExpect([consumer_ setIsLoading:NO]);
  fake_web_state->SetLoading(false);
  EXPECT_OCMOCK_VERIFY(consumer_);

  // Test back-forward state.
  web_navigation_util::GoBack(fake_web_state);
  OCMExpect([consumer_ setCanGoBack:YES]);
  OCMExpect([consumer_ setCanGoForward:YES]);
  fake_web_state->OnBackForwardStateChanged();
  EXPECT_OCMOCK_VERIFY(consumer_);

  web_navigation_util::GoBack(fake_web_state);
  OCMExpect([consumer_ setCanGoBack:NO]);
  OCMExpect([consumer_ setCanGoForward:YES]);
  fake_web_state->OnBackForwardStateChanged();
  EXPECT_OCMOCK_VERIFY(consumer_);

  web_navigation_util::GoForward(fake_web_state);
  OCMExpect([consumer_ setCanGoBack:YES]);
  OCMExpect([consumer_ setCanGoForward:YES]);
  fake_web_state->OnBackForwardStateChanged();
  EXPECT_OCMOCK_VERIFY(consumer_);

  web_navigation_util::GoForward(fake_web_state);
  OCMExpect([consumer_ setCanGoBack:YES]);
  OCMExpect([consumer_ setCanGoForward:NO]);
  fake_web_state->OnBackForwardStateChanged();
  EXPECT_OCMOCK_VERIFY(consumer_);
}
