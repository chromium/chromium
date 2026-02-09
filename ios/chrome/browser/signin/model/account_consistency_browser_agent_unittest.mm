// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/account_consistency_browser_agent.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/signin/ios/browser/fake_signin_enabled_datasource.h"
#import "components/test/ios/test_utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/web/model/web_navigation_browser_agent.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class AccountConsistencyBrowserAgentTest : public PlatformTest {
 public:
  explicit AccountConsistencyBrowserAgentTest() = default;

  void SetUp() override {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    mock_scene_handler_ = OCMStrictProtocolMock(@protocol(SceneCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_scene_handler_
                     forProtocol:@protocol(SceneCommands)];
    settings_commands_mock_ =
        OCMStrictProtocolMock(@protocol(SettingsCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:settings_commands_mock_
                     forProtocol:@protocol(SettingsCommands)];
    browser_coordinator_commands_mock_ =
        OCMStrictProtocolMock(@protocol(BrowserCoordinatorCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:browser_coordinator_commands_mock_
                     forProtocol:@protocol(BrowserCoordinatorCommands)];

    base_view_controller_mock_ = OCMStrictClassMock([UIViewController class]);
    WebNavigationBrowserAgent::CreateForBrowser(browser_.get());
    AccountConsistencyBrowserAgent::CreateForBrowser(
        browser_.get(), base_view_controller_mock_,
        &signin_enabled_data_source_);
    agent_ = AccountConsistencyBrowserAgent::FromBrowser(browser_.get());

    WebStateList* web_state_list = browser_.get()->GetWebStateList();
    auto test_web_state = std::make_unique<web::FakeWebState>();
    web_state_list->InsertWebState(std::move(test_web_state),
                                   WebStateList::InsertionParams::AtIndex(0));
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY((id)mock_scene_handler_);
    EXPECT_OCMOCK_VERIFY((id)settings_commands_mock_);
    EXPECT_OCMOCK_VERIFY((id)browser_coordinator_commands_mock_);
    EXPECT_OCMOCK_VERIFY((id)base_view_controller_mock_);
  }

 protected:
  // Signs the user in an account with a fake identity.
  void SignIn() {
    FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(fake_identity);
    AuthenticationService* auth_service =
        AuthenticationServiceFactory::GetForProfile(profile_.get());
    auth_service->SignIn(fake_identity,
                         signin_metrics::AccessPoint::kStartPage);
  }

  const GURL url_ = GURL("https://www.example.com");
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  raw_ptr<AccountConsistencyBrowserAgent> agent_;
  id<SceneCommands> mock_scene_handler_;
  signin::FakeSigninEnabledDataSource signin_enabled_data_source_;
  id<SettingsCommands> settings_commands_mock_;
  id<BrowserCoordinatorCommands> browser_coordinator_commands_mock_;
  UIViewController* base_view_controller_mock_;
};

// Tests the command sent by OnGoIncognito() when there is no URL.
TEST_F(AccountConsistencyBrowserAgentTest, OnGoIncognitoWithNoURL) {
  __block OpenNewTabCommand* received_command = nil;
  OCMExpect([mock_scene_handler_
      openURLInNewTab:AssignValueToVariable(received_command)]);
  agent_->OnGoIncognito(GURL());
  EXPECT_NE(received_command, nil);
  EXPECT_TRUE(received_command.inIncognito);
  EXPECT_FALSE(received_command.inBackground);
  EXPECT_EQ(received_command.URL, GURL());
}

// Tests the command sent by OnGoIncognito() when there is a valid URL.
TEST_F(AccountConsistencyBrowserAgentTest, OnGoIncognitoWithURL) {
  // This URL is not opened.
  __block OpenNewTabCommand* received_command = nil;
  OCMExpect([mock_scene_handler_
      openURLInNewTab:AssignValueToVariable(received_command)]);
  agent_->OnGoIncognito(url_);
  EXPECT_NE(received_command, nil);
  EXPECT_TRUE(received_command.inIncognito);
  EXPECT_FALSE(received_command.inBackground);
  EXPECT_EQ(received_command.URL, url_);
}

// Tests OnAddAccount() to not send ShowSigninCommand if a view controller is
// presented on top of the base view controller.
// See http://crbug.com/1399464.
TEST_F(AccountConsistencyBrowserAgentTest, OnAddAccountWithPresentedView) {
  OCMStub([base_view_controller_mock_ presentedViewController])
      .andReturn([[UIViewController alloc] init]);
  agent_->OnAddAccount(GURL(), "");
  // Expect [mock_scene_handler_ showSignin:baseViewController:] to not
  // be called. This is ensured by TearDown because mock_scene_handler_
  // is a strict mock.
}

// Tests OnAddAccount() to show present a view controller if there is no view
// presented on top of the base view controller. See http://crbug.com/1399464.
TEST_F(AccountConsistencyBrowserAgentTest, OnAddAccountWithoutPresentedView) {
  OCMStub([base_view_controller_mock_ presentedViewController])
      .andReturn((id)nil);
  signin_metrics::AccessPoint access_point =
      signin_metrics::AccessPoint::kAccountConsistencyService;
  OCMExpect([browser_coordinator_commands_mock_
      showAddAccountWithAccessPoint:access_point
                     prefilledEmail:@"test"]);
  agent_->OnAddAccount(GURL(), "test");
}

TEST_F(AccountConsistencyBrowserAgentTest, OnAddAccountShowsAccountMenu) {
  SignIn();
  FakeSystemIdentity* fake_identity2 = [FakeSystemIdentity fakeIdentity2];
  FakeSystemIdentityManager* system_identity_manager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  system_identity_manager->AddIdentity(fake_identity2);
  // Register a second profile.
  TestProfileIOS::Builder builder;
  builder.SetName("work_profile");
  profile_manager_.AddProfileWithBuilder(std::move(builder));

  OCMStub([base_view_controller_mock_ presentedViewController])
      .andReturn((id)nil);

  // Since there is another profile, the agent should trigger the account menu
  // instead of the add-account flow.
  OCMExpect([mock_scene_handler_ showAccountMenuFromWebWithURL:url_]);
  // The expected email is foo2@gmail.com. Using foo.2 instead allows to check
  // adding account with a non-canonical email.
  agent_->OnAddAccount(url_, "foo.2@gmail.com");
}

// Tests that calling the `OnRestoreGaiaCookies()` callback invokes the account
// notification command.
TEST_F(AccountConsistencyBrowserAgentTest, OnRestorGaiaCookiesCallsCommand) {
  OCMExpect(
      [mock_scene_handler_ showSigninAccountNotificationFromViewController:
                               base_view_controller_mock_]);
  agent_->OnRestoreGaiaCookies();
  // Expect -showSigninAccountNotificationFromViewController to have
  // been called. This is ensured by TearDown because mock_scene_handler_
  // is a strict mock.
}

// Tests that OnAddAccount with a email not of a secondary account opens the add
// account view.
TEST_F(AccountConsistencyBrowserAgentTest, OnAddAccountShowsAddAccount) {
  SignIn();
  // Register a second profile.
  TestProfileIOS::Builder builder;
  builder.SetName("work_profile");
  profile_manager_.AddProfileWithBuilder(std::move(builder));

  OCMStub([base_view_controller_mock_ presentedViewController])
      .andReturn((id)nil);
  NSString* email = @"foo3@gmail.com";

  // Since there is another profile, the agent should trigger the account menu
  // instead of the add-account flow.
  signin_metrics::AccessPoint access_point =
      signin_metrics::AccessPoint::kAccountConsistencyService;
  OCMExpect([browser_coordinator_commands_mock_
      showAddAccountWithAccessPoint:access_point
                     prefilledEmail:email]);
  agent_->OnAddAccount(url_, base::SysNSStringToUTF8(email));
}

// Tests that calling the `OnManageAccounts()` callback invokes the account
// settings command.
TEST_F(AccountConsistencyBrowserAgentTest, OnManageAccountsCallsCommand) {
  OCMExpect([settings_commands_mock_
      showAccountsSettingsFromViewController:base_view_controller_mock_
                        skipIfUINotAvailable:YES]);
  agent_->OnManageAccounts(GURL());
  // Expect -showAccountsSettingsFromViewController:skipIfUINotAvailable: to
  // have been called. This is ensured by TearDown because
  // settings_commands_mock_ is a strict mock.
}

// Tests that OnAddAccount with a email of a secondary account opens the account
// menu.
TEST_F(AccountConsistencyBrowserAgentTest, OnManageAccountsShowsAccountMenu) {
  SignIn();
  // Register a second profile.
  TestProfileIOS::Builder builder;
  builder.SetName("work_profile");
  profile_manager_.AddProfileWithBuilder(std::move(builder));

  // Since there is another profile, the agent should trigger the account menu
  // instead of the manage accounts screen.
  OCMExpect([mock_scene_handler_ showAccountMenuFromWebWithURL:url_]);
  agent_->OnManageAccounts(url_);
  // Expect showAccountsSettingsFromViewController:skipIfUINotAvailable: to not
  // be called. This is ensured by TearDown because mock_scene_handler_
  // is a strict mock.
}

// Tests that calling the `OnShowConsistencyPromo()` callback with the active
// web state invokes the command to show the signing promo.
TEST_F(AccountConsistencyBrowserAgentTest,
       OnShowConsistencyPromoWithCurrentWebState) {
  // Activate a web state and pass that web state into `OnShowConsistencyPromo`.
  WebStateList* web_state_list = browser_.get()->GetWebStateList();
  web_state_list->ActivateWebStateAt(0);
  web::WebState* web_state =
      browser_.get()->GetWebStateList()->GetActiveWebState();
  OCMExpect([mock_scene_handler_
      showWebSigninPromoFromViewController:base_view_controller_mock_
                                       URL:url_]);
  agent_->OnShowConsistencyPromo(url_, web_state);
  // Expect -showWebSigninPromoFromViewController:URL: to have been called.
  // This is ensured by TearDown because mock_scene_handler_ is a strict
  // mock.
}

// Tests that calling the `OnShowConsistencyPromo()` callback with a non-active
// web state does not invoke the command to show the signing promo.
TEST_F(AccountConsistencyBrowserAgentTest,
       OnShowConsistencyPromoWithOtherWebState) {
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
  agent_->OnShowConsistencyPromo(url_, web_state);
  // Expect -showWebSigninPromoFromViewController:URL: to have not been called.
  // This is ensured by TearDown because mock_scene_handler_ is a strict
  // mock.
}
