// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/password/passwords_coordinator.h"

#import "ios/chrome/browser/authentication/trusted_vault_reauthentication/coordinator/trusted_vault_reauthentication_coordinator.h"
#import "ios/chrome/browser/settings/ui_bundled/password/passwords_coordinator+Testing.h"
#import "ios/chrome/browser/settings/ui_bundled/password/reauthentication/local_reauthentication_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/fakes/fake_ui_navigation_controller.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class PasswordsCoordinatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    browser_ =
        std::make_unique<TestBrowser>(TestProfileIOS::Builder().Build().get());

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
