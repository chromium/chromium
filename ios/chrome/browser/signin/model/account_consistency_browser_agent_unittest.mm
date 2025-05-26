// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/account_consistency_browser_agent.h"

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/authentication/ui_bundled/account_menu/account_menu_constants.h"
#import "ios/chrome/browser/lens/model/lens_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/web/model/web_navigation_browser_agent.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class AccountConsistencyBrowserAgentTestBase : public PlatformTest {
 public:
  explicit AccountConsistencyBrowserAgentTestBase(
      bool separate_profiles_for_managed_accounts_enabled) {
    features_.InitWithFeatureState(
        kSeparateProfilesForManagedAccounts,
        separate_profiles_for_managed_accounts_enabled);
  }

  void SetUp() override {
    profile_ =
        profile_manager_.AddProfileWithBuilder(TestProfileIOS::Builder());
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
  base::test::ScopedFeatureList features_;

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  raw_ptr<AccountConsistencyBrowserAgent> agent_;
  id<ApplicationCommands> application_commands_mock_;
  id<SettingsCommands> settings_commands_mock_;
  UIViewController* base_view_controller_mock_;
};

class AccountConsistencyBrowserAgentTest
    : public AccountConsistencyBrowserAgentTestBase,
      public testing::WithParamInterface<bool> {
 public:
  AccountConsistencyBrowserAgentTest()
      : AccountConsistencyBrowserAgentTestBase(
            /*separate_profiles_for_managed_accounts_enabled=*/GetParam()) {}
};

class AccountConsistencyBrowserAgentWithSeparateProfilesTest
    : public AccountConsistencyBrowserAgentTestBase {
 public:
  AccountConsistencyBrowserAgentWithSeparateProfilesTest()
      : AccountConsistencyBrowserAgentTestBase(
            /*separate_profiles_for_managed_accounts_enabled=*/true) {}
};

// Tests the command sent by OnGoIncognito() when there is no URL.
TEST_P(AccountConsistencyBrowserAgentTest, OnGoIncognitoWithNoURL) {
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
TEST_P(AccountConsistencyBrowserAgentTest, OnGoIncognitoWithURL) {
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
TEST_P(AccountConsistencyBrowserAgentTest, OnAddAccountWithPresentedView) {
  OCMStub([base_view_controller_mock_ presentedViewController])
      .andReturn([[UIViewController alloc] init]);
  agent_->OnAddAccount(GURL());
  // Expect [application_commands_mock_ showSignin:baseViewController:] to not
  // be called. This is ensured by TearDown because application_commands_mock_
  // is a strict mock.
}

// Tests OnAddAccount() to show present a view controller if there is no view
// presented on top of the base view controller. See http://crbug.com/1399464.
TEST_P(AccountConsistencyBrowserAgentTest, OnAddAccountWithoutPresentedView) {
  OCMStub([base_view_controller_mock_ presentedViewController])
      .andReturn((id)nil);
  __block void (^completion)() = nil;
  OCMExpect([base_view_controller_mock_
      presentViewController:[OCMArg any]
                   animated:YES
                 completion:[OCMArg checkWithBlock:^BOOL(id value) {
                   completion = value;
                   return YES;
                 }]]);
  agent_->OnAddAccount(GURL());
  CHECK(completion);
  completion();
}

TEST_F(AccountConsistencyBrowserAgentWithSeparateProfilesTest,
       OnAddAccountShowsAccountMenu) {
  if (!AreSeparateProfilesForManagedAccountsEnabled()) {
    // This can happen on iOS < 17, where separate profiles are not supported.
    return;
  }
  const GURL url("https://www.example.com");
  // Register a second profile.
  TestProfileIOS::Builder builder;
  builder.SetName("work_profile");
  profile_manager_.AddProfileWithBuilder(std::move(builder));

  OCMStub([base_view_controller_mock_ presentedViewController])
      .andReturn((id)nil);

  // Since there is another profile, the agent should trigger the account menu
  // instead of the add-account flow.
  OCMExpect([application_commands_mock_
      showAccountMenuFromAccessPoint:AccountMenuAccessPoint::kWeb
                                 URL:url]);
  agent_->OnAddAccount(url);
  // Expect [application_commands_mock_ showSignin:baseViewController:] to not
  // be called. This is ensured by TearDown because application_commands_mock_
  // is a strict mock.
}

// Tests that calling the `OnRestoreGaiaCookies()` callback invokes the account
// notification command.
TEST_P(AccountConsistencyBrowserAgentTest, OnRestorGaiaCookiesCallsCommand) {
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
TEST_P(AccountConsistencyBrowserAgentTest, OnManageAccountsCallsCommand) {
  OCMExpect([settings_commands_mock_
      showAccountsSettingsFromViewController:base_view_controller_mock_
                        skipIfUINotAvailable:YES]);
  agent_->OnManageAccounts(GURL());
  // Expect -showAccountsSettingsFromViewController:skipIfUINotAvailable: to
  // have been called. This is ensured by TearDown because
  // settings_commands_mock_ is a strict mock.
}

TEST_F(AccountConsistencyBrowserAgentWithSeparateProfilesTest,
       OnManageAccountsShowsAccountMenu) {
  if (!AreSeparateProfilesForManagedAccountsEnabled()) {
    // This can happen on iOS < 17, where separate profiles are not supported.
    return;
  }
  const GURL url("https://www.example.com");
  // Register a second profile.
  TestProfileIOS::Builder builder;
  builder.SetName("work_profile");
  profile_manager_.AddProfileWithBuilder(std::move(builder));

  // Since there is another profile, the agent should trigger the account menu
  // instead of the manage accounts screen.
  OCMExpect([application_commands_mock_
      showAccountMenuFromAccessPoint:AccountMenuAccessPoint::kWeb
                                 URL:url]);
  agent_->OnManageAccounts(url);
  // Expect showAccountsSettingsFromViewController:skipIfUINotAvailable: to not
  // be called. This is ensured by TearDown because application_commands_mock_
  // is a strict mock.
}

// Tests that calling the `OnShowConsistencyPromo()` callback with the active
// web state invokes the command to show the signing promo.
TEST_P(AccountConsistencyBrowserAgentTest,
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
TEST_P(AccountConsistencyBrowserAgentTest,
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

INSTANTIATE_TEST_SUITE_P(,
                         AccountConsistencyBrowserAgentTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "WithSeparateProfiles"
                                             : "WithoutSeparateProfiles";
                         });
