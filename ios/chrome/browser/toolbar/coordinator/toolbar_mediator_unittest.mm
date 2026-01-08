// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/coordinator/toolbar_mediator.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/page_side_swipe_commands.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
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

    page_side_swipe_handler_ =
        OCMProtocolMock(@protocol(PageSideSwipeCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:page_side_swipe_handler_
                     forProtocol:@protocol(PageSideSwipeCommands)];
  }

  // Returns a new fake web state and set the fake navigation manager to the
  // navigation manager used here.
  std::unique_ptr<web::FakeWebState> CreateWebState() {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetVisibleURL(GURL("https://example.com"));
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    fake_navigation_manager_ = navigation_manager.get();
    navigation_manager->AddItem(GURL("https://example.com/1"),
                                ui::PAGE_TRANSITION_TYPED);
    navigation_manager->AddItem(GURL("https://example.com/2"),
                                ui::PAGE_TRANSITION_TYPED);
    navigation_manager->AddItem(GURL("https://example.com/3"),
                                ui::PAGE_TRANSITION_TYPED);
    navigation_manager->SetLastCommittedItemIndex(2);
    web_state->SetNavigationManager(std::move(navigation_manager));

    web_state->SetBrowserState(profile_.get());

    return web_state;
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
  id page_side_swipe_handler_;
  raw_ptr<web::FakeNavigationManager> fake_navigation_manager_;
};

// Tests that selecting a web state updates the consumer.
TEST_F(ToolbarMediatorTest, TestWebStateSelectionUpdatesConsumer) {

  OCMExpect([consumer_ setCanGoBack:YES]);
  OCMExpect([consumer_ setCanGoForward:NO]);
  OCMExpect([consumer_ setLocationBarText:[OCMArg any]]);
  OCMExpect([consumer_ setShareEnabled:YES]);
  OCMExpect([consumer_ setIsLoading:NO]);

  browser_->GetWebStateList()->InsertWebState(
      CreateWebState(), WebStateList::InsertionParams::AtIndex(0).Activate());

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests update of the consumer when the webpage triggers a navigation.
TEST_F(ToolbarMediatorTest, TestWebStateUpdates) {
  std::unique_ptr<web::FakeWebState> web_state = CreateWebState();
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

// Tests that the mutator handles back action.
TEST_F(ToolbarMediatorTest, TestMutatorGoBack) {
  std::unique_ptr<web::FakeWebState> web_state = CreateWebState();
  web::FakeWebState* fake_web_state = web_state.get();

  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  ASSERT_EQ(
      2, fake_web_state->GetNavigationManager()->GetLastCommittedItemIndex());

  [mediator_ goBack];

  EXPECT_EQ(
      1, fake_web_state->GetNavigationManager()->GetLastCommittedItemIndex());
}

// Tests that the mutator handles forward action.
TEST_F(ToolbarMediatorTest, TestMutatorGoForward) {
  std::unique_ptr<web::FakeWebState> web_state = CreateWebState();
  web::FakeWebState* fake_web_state = web_state.get();

  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  web_navigation_util::GoBack(fake_web_state);

  ASSERT_EQ(
      1, fake_web_state->GetNavigationManager()->GetLastCommittedItemIndex());

  [mediator_ goForward];

  EXPECT_EQ(
      2, fake_web_state->GetNavigationManager()->GetLastCommittedItemIndex());
}

// Tests that the mutator handles reload action.
TEST_F(ToolbarMediatorTest, TestMutatorReload) {
  browser_->GetWebStateList()->InsertWebState(
      CreateWebState(), WebStateList::InsertionParams::AtIndex(0).Activate());

  ASSERT_FALSE(fake_navigation_manager_->ReloadWasCalled());

  [mediator_ reload];

  EXPECT_TRUE(fake_navigation_manager_->ReloadWasCalled());
}

// Tests that the mutator handles stop action.
TEST_F(ToolbarMediatorTest, TestMutatorStop) {
  std::unique_ptr<web::FakeWebState> web_state = CreateWebState();
  web::FakeWebState* fake_web_state = web_state.get();

  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  ASSERT_FALSE(fake_web_state->was_stopped());

  [mediator_ stop];

  EXPECT_TRUE(fake_web_state->was_stopped());
}
