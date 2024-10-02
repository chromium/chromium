// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/legacy_accounts_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/functional/callback_helpers.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/test/test_sync_service.h"
#import "components/variations/scoped_variations_ids_provider.h"
#import "google_apis/gaia/core_account_id.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_mediator.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

std::unique_ptr<KeyedService> CreateTestSyncService(
    web::BrowserState* context) {
  return std::make_unique<syncer::TestSyncService>();
}

}  // namespace

class LegacyAccountsTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 public:
  LegacyAccountsTableViewControllerTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    AuthenticationServiceFactory::CreateAndInitializeForProfile(
        profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());
  }

  LegacyChromeTableViewController* InstantiateController() override {
    // Set up ApplicationCommands mock.
    id mock_application_handler =
        OCMProtocolMock(@protocol(ApplicationCommands));
    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    [dispatcher startDispatchingToTarget:mock_application_handler
                             forProtocol:@protocol(ApplicationCommands)];

    AccountsMediator* mediator =
        [[AccountsMediator alloc] initWithSyncService:test_sync_service()
                                accountManagerService:account_manager_service()
                                          authService:authentication_service()
                                      identityManager:identity_manager()];

    LegacyAccountsTableViewController* controller =
        [[LegacyAccountsTableViewController alloc]
                                initWithBrowser:browser_.get()
                      closeSettingsOnAddAccount:NO
                     applicationCommandsHandler:mock_application_handler
            signoutDismissalByParentCoordinator:NO];

    mediator.consumer = controller;
    controller.modelIdentityDataSource = mediator;
    mediator_ = mediator;

    return controller;
  }

  void TearDown() override {
    mediator_ = nil;
    [base::apple::ObjCCast<LegacyAccountsTableViewController>(controller())
        settingsWillBeDismissed];
    LegacyChromeTableViewControllerTest::TearDown();
  }

  // Identity Services
  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(profile_.get());
  }

  AuthenticationService* authentication_service() {
    return AuthenticationServiceFactory::GetForProfile(profile_.get());
  }

  FakeSystemIdentityManager* fake_system_identity_manager() {
    return FakeSystemIdentityManager::FromSystemIdentityManager(
        GetApplicationContext()->GetSystemIdentityManager());
  }

  syncer::TestSyncService* test_sync_service() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(profile_.get()));
  }

  ChromeAccountManagerService* account_manager_service() {
    return ChromeAccountManagerServiceFactory::GetForProfile(profile_.get());
  }

 private:
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::MainThreadType::IO};
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  AccountsMediator* mediator_;
};

// Tests that a valid identity is added to the model.
TEST_F(LegacyAccountsTableViewControllerTest, AddChromeIdentity) {
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity);

  // Simulates a credential reload.
  authentication_service()->SignIn(
      fake_identity, signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS);
  fake_system_identity_manager()->FireSystemIdentityReloaded();
  base::RunLoop().RunUntilIdle();

  CreateController();
  CheckController();

  EXPECT_EQ(2, NumberOfSections());
  EXPECT_EQ(2, NumberOfItemsInSection(0));
}

// Tests that an invalid identity is not added to the model.
TEST_F(LegacyAccountsTableViewControllerTest, IgnoreMismatchWithAccountInfo) {
  FakeSystemIdentity* fake_identity1 = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity1);
  FakeSystemIdentity* fake_identity2 = [FakeSystemIdentity fakeIdentity2];
  fake_system_identity_manager()->AddIdentity(fake_identity2);

  // Simulates a credential reload.
  authentication_service()->SignIn(
      fake_identity1, signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS);
  fake_system_identity_manager()->FireSystemIdentityReloaded();
  base::RunLoop().RunUntilIdle();

  CreateController();
  CheckController();

  EXPECT_EQ(2, NumberOfSections());
  EXPECT_EQ(3, NumberOfItemsInSection(0));

  // Removes identity2 from identity service but not account info storage. This
  // is an asynchronous call, so wait for completion.
  {
    base::RunLoop run_loop;
    fake_system_identity_manager()->ForgetIdentity(
        fake_identity2, base::IgnoreArgs<NSError*>(run_loop.QuitClosure()));
    run_loop.Run();
  }

  [controller() loadModel];

  EXPECT_EQ(2, NumberOfSections());
  EXPECT_EQ(2, NumberOfItemsInSection(0));
}

// Tests that no passphrase error is exposed in the account table view (since
// it's exposed one level up).
TEST_F(LegacyAccountsTableViewControllerTest, DontHoldPassphraseError) {
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity);

  // Simulate a credential reload.
  authentication_service()->SignIn(
      fake_identity, signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS);
  fake_system_identity_manager()->FireSystemIdentityReloaded();
  base::RunLoop().RunUntilIdle();

  CoreAccountInfo account;
  account.email = base::SysNSStringToUTF8(fake_identity.userEmail);
  account.gaia = base::SysNSStringToUTF8(fake_identity.gaiaID);
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);
  test_sync_service()->SetSignedIn(signin::ConsentLevel::kSignin, account);
  test_sync_service()->GetUserSettings()->SetPassphraseRequired();

  CreateController();
  CheckController();

  EXPECT_EQ(2, NumberOfSections());
}

// Tests that when eligible account bookmarks don't have the Account Storage
// error when there is no error.
TEST_F(LegacyAccountsTableViewControllerTest,
       DontHoldPassphraseErrorWhenEligibleNoError) {
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity);

  // Simulate a credential reload.
  authentication_service()->SignIn(
      fake_identity, signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS);
  fake_system_identity_manager()->FireSystemIdentityReloaded();
  base::RunLoop().RunUntilIdle();

  CoreAccountInfo account;
  account.email = base::SysNSStringToUTF8(fake_identity.userEmail);
  account.gaia = base::SysNSStringToUTF8(fake_identity.gaiaID);
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);
  test_sync_service()->SetSignedIn(signin::ConsentLevel::kSync, account);
  ASSERT_FALSE(test_sync_service()->GetUserSettings()->IsPassphraseRequired());

  CreateController();
  CheckController();

  // Verify that there are only 2 sections, exluding the error section.
  EXPECT_EQ(2, NumberOfSections());
}
