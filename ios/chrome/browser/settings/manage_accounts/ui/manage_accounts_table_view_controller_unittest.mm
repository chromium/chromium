// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/manage_accounts/ui/manage_accounts_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/functional/callback_helpers.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "components/sync/test/test_sync_service.h"
#import "components/variations/scoped_variations_ids_provider.h"
#import "google_apis/gaia/core_account_id.h"
#import "ios/chrome/browser/settings/manage_accounts/coordinator/manage_accounts_mediator.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
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
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class ManageAccountsTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 public:
  ManageAccountsTableViewControllerTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegateForTesting(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));
    browser_ = std::make_unique<TestBrowser>(profile_);
  }

  LegacyChromeTableViewController* InstantiateController() override {
    ManageAccountsMediator* mediator = [[ManageAccountsMediator alloc]
        initWithAccountManagerService:account_manager_service()
                          authService:authentication_service()
                      identityManager:identity_manager()];

    ManageAccountsTableViewController* controller =
        [[ManageAccountsTableViewController alloc]
            initWithOfferSignout:show_signout_button_];

    mediator.consumer = controller;
    controller.modelIdentityDataSource = mediator;
    mediator_ = mediator;

    return controller;
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    show_signout_button_ = NO;
    browser_.reset();
    profile_ = nullptr;
    LegacyChromeTableViewControllerTest::TearDown();
  }

  // Identity Services
  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(profile_);
  }

  AuthenticationService* authentication_service() {
    return AuthenticationServiceFactory::GetForProfile(profile_);
  }

  FakeSystemIdentityManager* fake_system_identity_manager() {
    return FakeSystemIdentityManager::FromSystemIdentityManager(
        GetApplicationContext()->GetSystemIdentityManager());
  }

  ChromeAccountManagerService* account_manager_service() {
    return ChromeAccountManagerServiceFactory::GetForProfile(profile_);
  }

  void showSignoutButton() { show_signout_button_ = YES; }

 private:
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::MainThreadType::IO};
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_ = nullptr;
  std::unique_ptr<Browser> browser_;
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  ManageAccountsMediator* mediator_;
  BOOL show_signout_button_ = NO;
};

// Tests that a sign out button is added.
TEST_F(ManageAccountsTableViewControllerTest, OfferSignOut) {
  showSignoutButton();
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity);

  // Simulates a credential reload.
  authentication_service()->SignIn(fake_identity,
                                   signin_metrics::AccessPoint::kSettings);
  fake_system_identity_manager()->FireSystemIdentityReloaded();

  CreateController();
  CheckController();

  EXPECT_EQ(3, NumberOfSections());
}

// Tests that a sign out button is not added.
TEST_F(ManageAccountsTableViewControllerTest, ShouldNotOfferSignOut) {
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity);

  // Simulates a credential reload.
  authentication_service()->SignIn(fake_identity,
                                   signin_metrics::AccessPoint::kSettings);
  fake_system_identity_manager()->FireSystemIdentityReloaded();

  CreateController();
  CheckController();

  EXPECT_EQ(2, NumberOfSections());
}
