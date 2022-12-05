// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/accounts_table_view_controller.h"

#import "base/mac/foundation_util.h"
#import "components/variations/scoped_variations_ids_provider.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller_test.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class AccountsTableViewControllerTest : public ChromeTableViewControllerTest {
 public:
  AccountsTableViewControllerTest()
      : task_environment_(web::WebTaskEnvironment::IO_MAINLOOP) {
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    browser_state_ = builder.Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());

    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
  }

  ChromeTableViewController* InstantiateController() override {
    // Set up ApplicationCommands mock. Because ApplicationCommands conforms
    // to ApplicationSettingsCommands, that needs to be mocked and dispatched
    // as well.
    id mockApplicationCommandHandler =
        OCMProtocolMock(@protocol(ApplicationCommands));
    id mockApplicationSettingsCommandHandler =
        OCMProtocolMock(@protocol(ApplicationSettingsCommands));

    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    [dispatcher startDispatchingToTarget:mockApplicationCommandHandler
                             forProtocol:@protocol(ApplicationCommands)];
    [dispatcher
        startDispatchingToTarget:mockApplicationSettingsCommandHandler
                     forProtocol:@protocol(ApplicationSettingsCommands)];

    AccountsTableViewController* controller =
        [[AccountsTableViewController alloc] initWithBrowser:browser_.get()
                                   closeSettingsOnAddAccount:NO];
    controller.applicationCommandsHandler = mockApplicationCommandHandler;
    return controller;
  }

  void TearDown() override {
    [base::mac::ObjCCast<AccountsTableViewController>(controller())
        settingsWillBeDismissed];
    ChromeTableViewControllerTest::TearDown();
  }

  // Identity Services
  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForBrowserState(browser_state_.get());
  }

  ios::FakeChromeIdentityService* identity_service() {
    return ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
  }

  AuthenticationService* authentication_service() {
    return AuthenticationServiceFactory::GetForBrowserState(
        browser_state_.get());
  }

 private:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<Browser> browser_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
};

// Tests that a valid identity is added to the model.
TEST_F(AccountsTableViewControllerTest, AddChromeIdentity) {
  FakeSystemIdentity* identity =
      [FakeSystemIdentity identityWithEmail:@"foo1@gmail.com"
                                     gaiaID:@"foo1ID"
                                       name:@"Fake Foo 1"];
  identity_service()->AddIdentity(identity);

  // Simulates a credential reload.
  authentication_service()->SignIn(identity);
  identity_service()->FireChromeIdentityReload();
  base::RunLoop().RunUntilIdle();

  CreateController();
  CheckController();

  EXPECT_EQ(2, NumberOfSections());
  EXPECT_EQ(2, NumberOfItemsInSection(0));
}

// Tests that an invalid identity is not added to the model.
TEST_F(AccountsTableViewControllerTest, IgnoreMismatchWithAccountInfo) {
  FakeSystemIdentity* identity1 =
      [FakeSystemIdentity identityWithEmail:@"foo1@gmail.com"
                                     gaiaID:@"foo1ID"
                                       name:@"Fake Foo 1"];
  FakeSystemIdentity* identity2 =
      [FakeSystemIdentity identityWithEmail:@"foo2@gmail.com"
                                     gaiaID:@"foo2ID"
                                       name:@"Fake Foo 2"];
  identity_service()->AddIdentity(identity1);
  identity_service()->AddIdentity(identity2);

  // Simulates a credential reload.
  authentication_service()->SignIn(identity1);
  identity_service()->FireChromeIdentityReload();
  base::RunLoop().RunUntilIdle();

  CreateController();
  CheckController();

  EXPECT_EQ(2, NumberOfSections());
  EXPECT_EQ(3, NumberOfItemsInSection(0));

  // Removes identity2 from identity service but not account info storage.
  identity_service()->ForgetIdentity(identity2, nil);

  [controller() loadModel];

  EXPECT_EQ(2, NumberOfSections());
  EXPECT_EQ(2, NumberOfItemsInSection(0));
}
