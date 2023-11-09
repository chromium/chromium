// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator.h"

#import "base/test/ios/wait_util.h"
#import "components/sync/service/sync_service_utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/trusted_vault_client_backend_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/providers/signin/fake_trusted_vault_client_backend.h"
#import "ios/web/common/uikit_ui_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "third_party/ocmock/ocmock_extensions.h"

class TrustedVaultReauthenticationCoordinatorTest : public PlatformTest {
 public:
  TrustedVaultReauthenticationCoordinatorTest() {}

  void SetUp() override {
    PlatformTest::SetUp();

    base_view_controller_ = [[UIViewController alloc] init];
    base_view_controller_.view.backgroundColor = UIColor.blueColor;
    GetAnyKeyWindow().rootViewController = base_view_controller_;
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    browser_state_ = builder.Build();
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
    id<SystemIdentity> identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity);
    AuthenticationService* authentication_service =
        AuthenticationServiceFactory::GetForBrowserState(browser_state_.get());
    authentication_service->SignIn(
        identity, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);

    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
  }

  Browser* browser() { return browser_.get(); }

 protected:
  // Needed for test browser state created by TestChromeBrowserState().
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;

  std::unique_ptr<Browser> browser_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;

  UIViewController* base_view_controller_ = nil;
};

// Opens the trusted vault reauth dialog, and simulate a user cancel.
TEST_F(TrustedVaultReauthenticationCoordinatorTest, TestCancel) {
  // Create sign-in coordinator.
  syncer::TrustedVaultUserActionTriggerForUMA trigger =
      syncer::TrustedVaultUserActionTriggerForUMA::kSettings;
  SigninCoordinator* signinCoordinator = [SigninCoordinator
      trustedVaultReAuthenticationCoordinatorWithBaseViewController:
          base_view_controller_
                                                            browser:browser()
                                                             intent:
                                         SigninTrustedVaultDialogIntentFetchKeys
                                                            trigger:trigger];
  // Open and cancel the web sign-in dialog.
  __block bool signin_completion_called = false;
  signinCoordinator.signinCompletion =
      ^(SigninCoordinatorResult result, SigninCompletionInfo* info) {
        signin_completion_called = true;
        EXPECT_EQ(SigninCoordinatorResultCanceledByUser, result);
        EXPECT_EQ(nil, info.identity);
      };
  [signinCoordinator start];
  // Wait until the view controllre is presented.
  EXPECT_NE(nil, base_view_controller_.presentedViewController);
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool() {
        return !base_view_controller_.presentedViewController.beingPresented;
      }));

  // The TrustedVaultClientBackend instance is created by the provider API.
  // The test implementation returns a `FakeTrustedVaultClientBackend`. The
  // provider API implementation is selected at link time and since it is a
  // function, if multiple implementation are linked at the same time, the
  // linker will fail.
  //
  // The class `FakeTrustedVaultClientBackend` is defined in the same target
  // as the test implementation of the trusted_vault API. This means that if
  // the current executable succeeded at link time, it is guaranteed to use
  // the test implementation of the trusted_vault API (as the current target
  // depends on it, and a binary cannot depend on two version of the API).
  //
  // This means that it is safe to cast the `TrustedVaultClientBackend` to
  // `FakeTrustedVaultClientBackend` at runtime.
  static_cast<FakeTrustedVaultClientBackend*>(
      TrustedVaultClientBackendFactory::GetForBrowserState(
          browser_state_.get()))
      ->SimulateUserCancel();

  // Test the completion block.
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool() {
        return signin_completion_called;
      }));
  [signinCoordinator stop];
}

// Opens the trusted vault reauth dialog, and simulate a user cancel.
TEST_F(TrustedVaultReauthenticationCoordinatorTest, TestInterruptWithDismiss) {
  // Create sign-in coordinator.
  syncer::TrustedVaultUserActionTriggerForUMA trigger =
      syncer::TrustedVaultUserActionTriggerForUMA::kSettings;
  SigninCoordinator* signinCoordinator = [SigninCoordinator
      trustedVaultReAuthenticationCoordinatorWithBaseViewController:
          base_view_controller_
                                                            browser:browser()
                                                             intent:
                                         SigninTrustedVaultDialogIntentFetchKeys
                                                            trigger:trigger];
  // Open and cancel the web sign-in dialog.
  __block bool signin_completion_called = false;
  signinCoordinator.signinCompletion =
      ^(SigninCoordinatorResult result, SigninCompletionInfo* info) {
        signin_completion_called = true;
        EXPECT_EQ(SigninCoordinatorResultInterrupted, result);
        EXPECT_EQ(nil, info.identity);
      };
  [signinCoordinator start];
  // Wait until the view controllre is presented.
  EXPECT_NE(nil, base_view_controller_.presentedViewController);
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool() {
        return !base_view_controller_.presentedViewController.beingPresented;
      }));
  // Interrupt the coordinator.
  __block bool interrupt_completion_called = false;
  [signinCoordinator
      interruptWithAction:SigninCoordinatorInterrupt::DismissWithoutAnimation
               completion:^() {
                 EXPECT_TRUE(signin_completion_called);
                 interrupt_completion_called = true;
               }];
  // The sign-in and interrupt completion blocks should be called
  // asynchronously, after the UI is dismissed.
  EXPECT_FALSE(signin_completion_called);
  EXPECT_FALSE(interrupt_completion_called);
  // Test the completion block.
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool() {
        return interrupt_completion_called;
      }));
  [signinCoordinator stop];
}

// Opens the trusted vault reauth dialog, and interrupt it with
// `UIShutdownNoDismiss`.
TEST_F(TrustedVaultReauthenticationCoordinatorTest,
       TestInterruptUIShutdownNoDismiss) {
  // Create sign-in coordinator.
  syncer::TrustedVaultUserActionTriggerForUMA trigger =
      syncer::TrustedVaultUserActionTriggerForUMA::kSettings;
  SigninCoordinator* signinCoordinator = [SigninCoordinator
      trustedVaultReAuthenticationCoordinatorWithBaseViewController:
          base_view_controller_
                                                            browser:browser()
                                                             intent:
                                                                 SigninTrustedVaultDialogIntentFetchKeys
                                                            trigger:trigger];
  // Open and cancel the web sign-in dialog.
  __block bool signin_completion_called = false;
  signinCoordinator.signinCompletion =
      ^(SigninCoordinatorResult result, SigninCompletionInfo* info) {
        signin_completion_called = true;
        EXPECT_EQ(SigninCoordinatorResultInterrupted, result);
        EXPECT_EQ(nil, info.identity);
      };
  [signinCoordinator start];
  // Wait until the view controllre is presented.
  EXPECT_NE(nil, base_view_controller_.presentedViewController);
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool() {
        return !base_view_controller_.presentedViewController.beingPresented;
      }));
  // Interrupt the coordinator.
  __block bool interrupt_completion_called = false;
  [signinCoordinator
      interruptWithAction:SigninCoordinatorInterrupt::UIShutdownNoDismiss
               completion:^() {
                 EXPECT_TRUE(signin_completion_called);
                 interrupt_completion_called = true;
               }];
  // Sign-in and interrupt completion blocks should be called synchronously.
  EXPECT_TRUE(signin_completion_called);
  EXPECT_TRUE(interrupt_completion_called);
  [signinCoordinator stop];
}
