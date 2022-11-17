// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/account_consistency_browser_agent.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/lens/lens_browser_agent.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/web/web_navigation_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class AccountConsistencyBrowserAgentTest : public PlatformTest {
 public:
  void SetUp() override {
    TestChromeBrowserState::Builder builder;
    chrome_browser_state_ = builder.Build();
    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get());

    application_commands_mock_ =
        OCMStrictProtocolMock(@protocol(ApplicationCommands));
    LensBrowserAgent::CreateForBrowser(browser_.get());
    WebNavigationBrowserAgent::CreateForBrowser(browser_.get());
    AccountConsistencyBrowserAgent::CreateForBrowser(
        browser_.get(), nil, application_commands_mock_);
    agent_ = AccountConsistencyBrowserAgent::FromBrowser(browser_.get());

    WebStateList* web_state_list = browser_.get()->GetWebStateList();
    auto test_web_state = std::make_unique<web::FakeWebState>();
    WebStateOpener opener;
    web_state_list->InsertWebState(0, std::move(test_web_state),
                                   WebStateList::INSERT_FORCE_INDEX, opener);
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY((id)application_commands_mock_);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<Browser> browser_;
  AccountConsistencyBrowserAgent* agent_;
  id<ApplicationCommands> application_commands_mock_;
};

// Tests the command sent by OnGoIncognito() when there is no URL.
TEST_F(AccountConsistencyBrowserAgentTest, OnGoIncognitoWithNoURL) {
  __block OpenNewTabCommand* received_command = nil;
  OCMExpect([application_commands_mock_
      openURLInNewTab:[OCMArg checkWithBlock:^BOOL(OpenNewTabCommand* command) {
        received_command = command;
        return YES;
      }]]);
  agent_->OnGoIncognito(GURL());
  EXPECT_NE(received_command, nil);
  EXPECT_TRUE(received_command.inIncognito);
  EXPECT_FALSE(received_command.inBackground);
  EXPECT_EQ(received_command.URL, GURL());
}

// Tests the command sent by OnGoIncognito() when there is a valid URL.
TEST_F(AccountConsistencyBrowserAgentTest, OnGoIncognitoWithURL) {
  // This URL is not opened.
  GURL url("http://www.example.com");
  __block OpenNewTabCommand* received_command = nil;
  OCMExpect([application_commands_mock_
      openURLInNewTab:[OCMArg checkWithBlock:^BOOL(OpenNewTabCommand* command) {
        received_command = command;
        return YES;
      }]]);
  agent_->OnGoIncognito(url);
  EXPECT_NE(received_command, nil);
  EXPECT_TRUE(received_command.inIncognito);
  EXPECT_FALSE(received_command.inBackground);
  EXPECT_EQ(received_command.URL, url);
}
