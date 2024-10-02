// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_table_view_controller.h"

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

class AccountsTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 public:
  AccountsTableViewControllerTest() {
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

    AccountsMediator* mediator =
        [[AccountsMediator alloc] initWithSyncService:test_sync_service()
                                accountManagerService:account_manager_service()
                                          authService:authentication_service()
                                      identityManager:identity_manager()];

    AccountsTableViewController* controller =
        [[AccountsTableViewController alloc]
            initWithOfferSignout:show_signout_button_];

    mediator.consumer = controller;
    controller.modelIdentityDataSource = mediator;
    mediator_ = mediator;

    return controller;
  }

  void TearDown() override {
    mediator_ = nil;
    show_signout_button_ = NO;
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

  void showSignoutButton() { show_signout_button_ = YES; }

 private:
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::MainThreadType::IO};
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  AccountsMediator* mediator_;
  BOOL show_signout_button_ = NO;
};

// Tests that a sign out button is added.
TEST_F(AccountsTableViewControllerTest, OfferSignOut) {
  showSignoutButton();
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity);

  // Simulates a credential reload.
  authentication_service()->SignIn(
      fake_identity, signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS);
  fake_system_identity_manager()->FireSystemIdentityReloaded();

  CreateController();
  CheckController();

  EXPECT_EQ(3, NumberOfSections());
}

// Tests that a sign out button is not added.
TEST_F(AccountsTableViewControllerTest, ShouldNotOfferSignOut) {
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity);

  // Simulates a credential reload.
  authentication_service()->SignIn(
      fake_identity, signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS);
  fake_system_identity_manager()->FireSystemIdentityReloaded();

  CreateController();
  CheckController();

  EXPECT_EQ(2, NumberOfSections());
}
