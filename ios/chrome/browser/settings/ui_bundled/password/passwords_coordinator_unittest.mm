// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/password/passwords_coordinator.h"

#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/authentication/trusted_vault_reauthentication/coordinator/trusted_vault_reauthentication_coordinator.h"
#import "ios/chrome/browser/settings/ui_bundled/password/passwords_coordinator+Testing.h"
#import "ios/chrome/browser/settings/ui_bundled/password/reauthentication/local_reauthentication_coordinator.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/fakes/fake_ui_navigation_controller.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class PasswordsCoordinatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegateForTesting(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    coordinator_ = [[PasswordsCoordinator alloc]
        initWithBaseNavigationController:[[FakeUINavigationController alloc]
                                             init]
                                 browser:browser_.get()];

    ASSERT_TRUE([coordinator_
        conformsToProtocol:@protocol(
                               LocalReauthenticationCoordinatorDelegate)]);

    trusted_vault_reauthentication_coordinator_mock_ =
        OCMStrictClassMock([TrustedVaultReauthenticationCoordinator class]);
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  PasswordsCoordinator* coordinator_;
  id trusted_vault_reauthentication_coordinator_mock_;
};

// Tests that coordinator is being correctly dismissed during the preparation
// to Local Authentication.
// TODO(crbug.com/417667093): Remove this after adding EarlGrey tests of the
// Trusted Vault GPM management UI widget.
TEST_F(
    PasswordsCoordinatorTest,
    DismissesTrustedVaultCoordinatorOnWillPushReauthenticationViewController) {
  [coordinator_ setTrustedVaultReauthenticationCoordinator:
                    trusted_vault_reauthentication_coordinator_mock_];

  OCMExpect([trusted_vault_reauthentication_coordinator_mock_ stop]);
  OCMExpect([trusted_vault_reauthentication_coordinator_mock_ setDelegate:nil]);

  [(id<LocalReauthenticationCoordinatorDelegate>)
          coordinator_ willPushReauthenticationViewController];

  EXPECT_OCMOCK_VERIFY(trusted_vault_reauthentication_coordinator_mock_);
  EXPECT_EQ([coordinator_ trustedVaultReauthenticationCoordinator], nil);
}

// Tests that credential import flow is canceled when sign-in is disabled.
TEST_F(PasswordsCoordinatorTest, CancelsCredentialImportWhenSigninDisabled) {
  if (@available(iOS 26, *)) {
    GetApplicationContext()->GetLocalState()->SetBoolean(
        prefs::kSigninAllowedOnDevice, false);
    signin::IdentityManager* identityManager =
        IdentityManagerFactory::GetForProfile(profile_.get());
    ASSERT_FALSE(
        identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin));

    // Simulate start of the credential import flow, which should trigger after
    // successful reauth with non-empty `credentialImportUUID`.
    coordinator_.credentialImportUUID = [NSUUID UUID];
    [(id<LocalReauthenticationCoordinatorDelegate>)coordinator_
        successfulReauthenticationWithCoordinator:nil];

    // Verify that the coordinator does not crash and the UUID is cleared.
    EXPECT_EQ(coordinator_.credentialImportUUID, nil);
  } else {
    GTEST_SKIP() << "Credential import flow is only available on iOS 26+";
  }
}
