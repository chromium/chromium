// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/account_consistency_browser_agent.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/lens/model/lens_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/web/model/web_navigation_browser_agent.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class AccountConsistencyBrowserAgentTest : public PlatformTest {
 public:
  void SetUp() override {
    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    application_commands_mock_ =
        OCMStrictProtocolMock(@protocol(ApplicationCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:application_commands_mock_
                     forProtocol:@protocol(ApplicationCommands)];
    settings_commands_mock_ =
        OCMStrictProtocolMock(@protocol(SettingsCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:settings_commands_mock_
                     forProtocol:@protocol(SettingsCommands)];

    base_view_controller_mock_ = OCMStrictClassMock([UIViewController class]);
    LensBrowserAgent::CreateForBrowser(browser_.get());
    WebNavigationBrowserAgent::CreateForBrowser(browser_.get());
    AccountConsistencyBrowserAgent::CreateForBrowser(
        browser_.get(), base_view_controller_mock_);
    agent_ = AccountConsistencyBrowserAgent::FromBrowser(browser_.get());

    WebStateList* web_state_list = browser_.get()->GetWebStateList();
    auto test_web_state = std::make_unique<web::FakeWebState>();
    web_state_list->InsertWebState(std::move(test_web_state),
                                   WebStateList::InsertionParams::AtIndex(0));
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY((id)application_commands_mock_);
    EXPECT_OCMOCK_VERIFY((id)base_view_controller_mock_);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  raw_ptr<AccountConsistencyBrowserAgent> agent_;
  id<ApplicationCommands> application_commands_mock_;
  id<SettingsCommands> settings_commands_mock_;
  UIViewController* base_view_controller_mock_;
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

// Tests OnAddAccount() to not send ShowSigninCommand if a view controller is
// presented on top of the base view controller.
// See http://crbug.com/1399464.
TEST_F(AccountConsistencyBrowserAgentTest, OnAddAccountWithPresentedView) {
  OCMStub([base_view_controller_mock_ presentedViewController])
      .andReturn([[UIViewController alloc] init]);
  agent_->OnAddAccount();
  // Expect [application_commands_mock_ showSignin:baseViewController:] to not
  // be called. This is ensured by TearDown because application_commands_mock_
  // is a strict mock.
}

// Tests OnAddAccount() to send ShowSigninCommand if there is no view presented
// on top of the base view controller.
// See http://crbug.com/1399464.
TEST_F(AccountConsistencyBrowserAgentTest, OnAddAccountWithoutPresentedView) {
  OCMStub([base_view_controller_mock_ presentedViewController])
      .andReturn((id)nil);
  __block ShowSigninCommand* received_command = nil;
  OCMExpect([application_commands_mock_
              showSignin:[OCMArg
                             checkWithBlock:^BOOL(ShowSigninCommand* command) {
                               received_command = command;
                               return YES;
                             }]
      baseViewController:base_view_controller_mock_]);
  agent_->OnAddAccount();
  EXPECT_NE(received_command, nil);
  EXPECT_EQ(received_command.operation, AuthenticationOperation::kAddAccount);
  EXPECT_EQ(received_command.identity, nil);
  EXPECT_EQ(
      received_command.accessPoint,
      signin_metrics::AccessPoint::ACCESS_POINT_ACCOUNT_CONSISTENCY_SERVICE);
  EXPECT_EQ(received_command.promoAction,
            signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO);
}

// Tests that calling the `OnRestoreGaiaCookies()` callback invokes the account
// notification command.
TEST_F(AccountConsistencyBrowserAgentTest, OnRestorGaiaCookiesCallsCommand) {
  OCMExpect([application_commands_mock_
      showSigninAccountNotificationFromViewController:
          base_view_controller_mock_]);
  agent_->OnRestoreGaiaCookies();
  // Expect -showSigninAccountNotificationFromViewController to have
  // been called. This is ensured by TearDown because application_commands_mock_
  // is a strict mock.
}

// Tests that calling the `OnManageAccounts()` callback invokes the account
// settings command.
TEST_F(AccountConsistencyBrowserAgentTest, OnManageAccountsCallsCommand) {
  OCMExpect([settings_commands_mock_
      showAccountsSettingsFromViewController:base_view_controller_mock_
                        skipIfUINotAvailable:YES]);
  agent_->OnManageAccounts();
  // Expect -showAccountsSettingsFromViewController:skipIfUINotAvailable: to
  // have been called. This is ensured by TearDown because
  // settings_commands_mock_ is a strict mock.
}

// Tests that calling the `OnShowConsistencyPromo()` callback with the active
// web state invokes the command to show the signing promo.
TEST_F(AccountConsistencyBrowserAgentTest,
       OnShowConsistencyPromoWithCurrentWebState) {
  const GURL url("https://www.example.com");
  // Activate a web state and pass that web state into `OnShowConsistencyPromo`.
  WebStateList* web_state_list = browser_.get()->GetWebStateList();
  web_state_list->ActivateWebStateAt(0);
  web::WebState* web_state =
      browser_.get()->GetWebStateList()->GetActiveWebState();
  OCMExpect([application_commands_mock_
      showWebSigninPromoFromViewController:base_view_controller_mock_
                                       URL:url]);
  agent_->OnShowConsistencyPromo(url, web_state);
  // Expect -showWebSigninPromoFromViewController:URL: to have been called.
  // This is ensured by TearDown because application_commands_mock_ is a strict
  // mock.
}

// Tests that calling the `OnShowConsistencyPromo()` callback with a non-active
// web state does not invoke the command to show the signing promo.
TEST_F(AccountConsistencyBrowserAgentTest,
       OnShowConsistencyPromoWithOtherWebState) {
  const GURL url("https://www.example.com");
  // Activate the first web state.
  WebStateList* web_state_list = browser_.get()->GetWebStateList();
  web_state_list->ActivateWebStateAt(0);
  // Insert another web state, but don't activate it. Pass this web state in to
  // `OnShowConsistencyPromo`.
  auto test_web_state = std::make_unique<web::FakeWebState>();
  WebStateOpener opener;
  web_state_list->InsertWebState(
      std::move(test_web_state),
      WebStateList::InsertionParams::AtIndex(1).WithOpener(opener));
  web::WebState* web_state = web_state_list->GetWebStateAt(1);
  agent_->OnShowConsistencyPromo(url, web_state);
  // Expect -showWebSigninPromoFromViewController:URL: to have not been called.
  // This is ensured by TearDown because application_commands_mock_ is a strict
  // mock.
}
