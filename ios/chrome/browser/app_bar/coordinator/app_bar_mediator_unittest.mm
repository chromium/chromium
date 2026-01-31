// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/coordinator/app_bar_mediator.h"

#import <memory>

#import "ios/chrome/browser/app_bar/ui/app_bar_consumer.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/incognito_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/tab_grid_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_paging.h"
#import "ios/chrome/browser/url_loading/model/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class AppBarMediatorTest : public PlatformTest {
 protected:
  AppBarMediatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    FakeUrlLoadingBrowserAgent::InjectForBrowser(browser_.get());

    url_loader_ = FakeUrlLoadingBrowserAgent::FromUrlLoadingBrowserAgent(
        UrlLoadingBrowserAgent::FromBrowser(browser_.get()));

    tab_grid_state_ = [[TabGridState alloc] init];
    incognito_state_ = [[IncognitoState alloc] initWithSceneState:nil];
    mock_scene_handler_ = OCMProtocolMock(@protocol(SceneCommands));
    regular_web_state_list_ =
        std::make_unique<WebStateList>(&regular_web_state_list_delegate_);
    incognito_web_state_list_ =
        std::make_unique<WebStateList>(&incognito_web_state_list_delegate_);

    mediator_ = [[AppBarMediator alloc]
        initWithRegularWebStateList:regular_web_state_list_.get()
              incognitoWebStateList:incognito_web_state_list_.get()
                        prefService:profile_->GetPrefs()
                          URLLoader:url_loader_
                       tabGridState:tab_grid_state_
                     incognitoState:incognito_state_];

    consumer_ = OCMProtocolMock(@protocol(AppBarConsumer));
    mediator_.consumer = consumer_;
    mediator_.sceneHandler = mock_scene_handler_;
  }

  ~AppBarMediatorTest() override { [mediator_ disconnect]; }

  web::WebTaskEnvironment task_environment_;
  AppBarMediator* mediator_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<FakeUrlLoadingBrowserAgent> url_loader_;
  std::unique_ptr<WebStateList> regular_web_state_list_;
  std::unique_ptr<WebStateList> incognito_web_state_list_;
  FakeWebStateListDelegate regular_web_state_list_delegate_;
  FakeWebStateListDelegate incognito_web_state_list_delegate_;
  TabGridState* tab_grid_state_;
  IncognitoState* incognito_state_;
  id mock_scene_handler_;
  id consumer_;
};

// Tests that the consumer is updated when a web state is added.
TEST_F(AppBarMediatorTest, TestDidAddWebState) {
  OCMExpect([consumer_ updateTabCount:1]);
  auto web_state = std::make_unique<web::FakeWebState>();
  regular_web_state_list_->InsertWebState(std::move(web_state));
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is updated when a web state is detached.
TEST_F(AppBarMediatorTest, TestDidDetachWebState) {
  auto web_state = std::make_unique<web::FakeWebState>();
  regular_web_state_list_->InsertWebState(std::move(web_state));

  OCMExpect([consumer_ updateTabCount:0]);
  regular_web_state_list_->DetachWebStateAt(0);
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is updated when switching to incognito.
TEST_F(AppBarMediatorTest, TestSwitchToIncognitoNonTabGrid) {
  tab_grid_state_.tabGridVisible = NO;
  incognito_state_.incognitoContentVisible = NO;

  // Add a web state to incognito.
  auto web_state = std::make_unique<web::FakeWebState>();
  incognito_web_state_list_->InsertWebState(std::move(web_state));

  // Switch to incognito.
  OCMExpect([consumer_ updateTabCount:1]);
  incognito_state_.incognitoContentVisible = YES;
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is updated when switching back to regular.
TEST_F(AppBarMediatorTest, TestSwitchToRegularNonTabGrid) {
  tab_grid_state_.tabGridVisible = NO;
  incognito_state_.incognitoContentVisible = NO;

  // Add a web state to regular.
  auto web_state = std::make_unique<web::FakeWebState>();
  regular_web_state_list_->InsertWebState(std::move(web_state));

  // Switch to incognito (empty).
  OCMExpect([consumer_ updateTabCount:0]);
  incognito_state_.incognitoContentVisible = YES;
  EXPECT_OCMOCK_VERIFY(consumer_);

  // Switch back to regular.
  OCMExpect([consumer_ updateTabCount:1]);
  incognito_state_.incognitoContentVisible = NO;
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is updated when switching to incognito.
TEST_F(AppBarMediatorTest, TestSwitchToIncognitoTabGrid) {
  tab_grid_state_.tabGridVisible = YES;
  tab_grid_state_.currentPage = TabGridPageRegularTabs;

  // Add a web state to incognito.
  auto web_state = std::make_unique<web::FakeWebState>();
  incognito_web_state_list_->InsertWebState(std::move(web_state));

  // Switch to incognito.
  OCMExpect([consumer_ updateTabCount:1]);
  tab_grid_state_.currentPage = TabGridPageIncognitoTabs;
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is updated when switching back to regular.
TEST_F(AppBarMediatorTest, TestSwitchToRegularTabGrid) {
  tab_grid_state_.tabGridVisible = YES;
  tab_grid_state_.currentPage = TabGridPageRegularTabs;

  // Add a web state to regular.
  auto web_state = std::make_unique<web::FakeWebState>();
  regular_web_state_list_->InsertWebState(std::move(web_state));

  // Switch to incognito (empty).
  OCMExpect([consumer_ updateTabCount:0]);
  tab_grid_state_.currentPage = TabGridPageIncognitoTabs;
  EXPECT_OCMOCK_VERIFY(consumer_);

  // Switch back to regular.
  OCMExpect([consumer_ updateTabCount:1]);
  tab_grid_state_.currentPage = TabGridPageRegularTabs;
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests creating a new tab from outside of the tab grid.
TEST_F(AppBarMediatorTest, TestCreateNewTabNonTabGrid) {
  tab_grid_state_.tabGridVisible = NO;

  // Try to open a new tab.
  OCMExpect([mock_scene_handler_ openURLInNewTab:[OCMArg any]]);
  [mediator_ createNewTabFromView:nil];
  EXPECT_OCMOCK_VERIFY(mock_scene_handler_);
}

// Tests creating a new tab from inside of the tab grid.
TEST_F(AppBarMediatorTest, TestCreateNewTabTabGrid) {
  tab_grid_state_.tabGridVisible = YES;
  tab_grid_state_.currentPage = TabGridPageRegularTabs;

  // Try to open a new tab.
  [mediator_ createNewTabFromView:nil];

  EXPECT_FALSE(url_loader_->last_params.in_incognito);
  EXPECT_EQ(1, url_loader_->load_new_tab_call_count);
}

// Tests creating a new tab from inside of the tab grid incognito.
TEST_F(AppBarMediatorTest, TestCreateNewTabTabGridIncognito) {
  tab_grid_state_.tabGridVisible = YES;
  tab_grid_state_.currentPage = TabGridPageIncognitoTabs;

  // Try to open a new tab.
  [mediator_ createNewTabFromView:nil];

  EXPECT_TRUE(url_loader_->last_params.in_incognito);
  EXPECT_EQ(1, url_loader_->load_new_tab_call_count);
}
