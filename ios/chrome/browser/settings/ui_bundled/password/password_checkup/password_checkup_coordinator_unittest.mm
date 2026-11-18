// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/password/password_checkup/password_checkup_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "base/test/bind.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/affiliations/core/browser/fake_affiliation_service.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/password_store/test_password_store.h"
#import "components/password_manager/core/browser/ui/password_check_referrer.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/browser/affiliations/model/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/device_reauth/model/fake_reauthentication_service_util.h"
#import "ios/chrome/browser/device_reauth/model/reauthentication_service.h"
#import "ios/chrome/browser/device_reauth/model/reauthentication_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/metrics/ios_password_manager_metrics.h"
#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_checkup/password_checkup_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_issues/password_issues_table_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/password/reauthentication/local_reauthentication_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/common/ui/reauthentication/fake_reauthentication_module.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/public/provider/chrome/browser/push_notification/push_notification_api.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using password_manager::PasswordCheckReferrer;
using ::testing::Values;

namespace password_manager {

// Test fixture for PasswordCheckupCoordinator.
class PasswordCheckupCoordinatorTest
    : public PlatformTest,
      public testing::WithParamInterface<PasswordCheckReferrer> {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    SetUpCoordinatorWithReferrer(GetParam());
    AttachWindowAndStartCoordinator();
  }

  // Creates the test environment and `coordinator_` for `referrer` without
  // starting it.
  void SetUpCoordinatorWithReferrer(PasswordCheckReferrer referrer) {
    TestProfileIOS::Builder builder;
    // Add test password store and affiliation service. Used by the mediator.
    builder.AddTestingFactory(
        IOSChromeProfilePasswordStoreFactory::GetInstance(),
        base::BindOnce(&password_manager::BuildPasswordStore<
                       password_manager::TestPasswordStore>));
    builder.AddTestingFactory(
        IOSChromeAffiliationServiceFactory::GetInstance(),
        base::BindOnce(
            &std::make_unique<affiliations::FakeAffiliationService>));
    // Add authentication service.
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindOnce(&CreateTestSyncService));
    // Add reauthentication service.
    builder.AddTestingFactory(ReauthenticationServiceFactory::GetInstance(),
                              base::BindOnce(&CreateFakeReauthService));

    // Create scene state for reauthentication coordinator.
    scene_state_ = [[SceneState alloc] init];
    scene_state_.activationLevel = SceneActivationLevelForegroundActive;

    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);

    // Mock SceneCommands. Since SceneCommands conforms to
    // SettingsCommands, it must be mocked as well.
    mocked_scene_handler_ = OCMStrictProtocolMock(@protocol(SceneCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mocked_scene_handler_
                     forProtocol:@protocol(SceneCommands)];
    id mocked_application_settings_command_handler =
        OCMProtocolMock(@protocol(SettingsCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mocked_application_settings_command_handler
                     forProtocol:@protocol(SettingsCommands)];

    // Sign in.
    fake_system_identity_ = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(fake_system_identity_);
    AuthenticationService* authentication_service =
        AuthenticationServiceFactory::GetForProfile(profile_.get());
    authentication_service->SignIn(fake_system_identity_,
                                   signin_metrics::AccessPoint::kStartPage);

    // Init navigation controller with a root vc.
    base_navigation_controller_ = [[UINavigationController alloc]
        initWithRootViewController:[[UIViewController alloc] init]];
    fake_reauth_module_ =
        base::apple::ObjCCastStrict<FakeReauthenticationModule>(
            ReauthenticationServiceFactory::GetForProfile(profile_.get())
                ->GetReauthModule());
    // Delay auth result so auth doesn't pass right after starting coordinator.
    // Needed for verifying behavior when auth is required.
    fake_reauth_module_.shouldSkipReAuth = NO;
    fake_reauth_module_.expectedResult = ReauthenticationResult::kSuccess;

    push_notification_service_ = ios::provider::CreatePushNotificationService();

    coordinator_ = [[PasswordCheckupCoordinator alloc]
        initWithBaseNavigationController:base_navigation_controller_
                                 browser:browser_.get()
                                referrer:referrer];
  }

  // Attaches `base_navigation_controller_` to the key window, starts
  // `coordinator_`, and waits for presentation.
  void AttachWindowAndStartCoordinator() {
    scoped_window_.Get().rootViewController = base_navigation_controller_;

    [coordinator_ start];

    // Wait for presentation animation of the coordinator's view controller.
    base::test::ios::SpinRunLoopWithMaxDelay(
        base::test::ios::kWaitForUIElementTimeout);
  }

  void TearDown() override {
    [coordinator_ stop];
    PlatformTest::TearDown();
  }

  // Verifies that the PasswordCheckupViewController was pushed in the
  // navigation controller.
  void CheckPasswordCheckupIsPresented() {
    ASSERT_TRUE([base_navigation_controller_.topViewController
        isKindOfClass:[PasswordCheckupViewController class]]);
  }

  // Verifies that the PasswordCheckupViewController is not on top of the
  // navigation controller.
  void CheckPasswordCheckupIsNotPresented() {
    ASSERT_FALSE([base_navigation_controller_.topViewController
        isKindOfClass:[PasswordCheckupViewController class]]);
  }

  // Verifies that a given number of password checkup visits have been recorded.
  void CheckPasswordCheckupVisitMetricsCount(int count) {
    histogram_tester_.ExpectUniqueSample(
        /*name=*/password_manager::kPasswordManagerSurfaceVisitHistogramName,
        /*sample=*/password_manager::PasswordManagerSurface::kPasswordCheckup,
        /*count=*/count);
  }

  web::WebTaskEnvironment task_environment_;
  SceneState* scene_state_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<PushNotificationService> push_notification_service_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<ProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  ScopedKeyWindow scoped_window_;
  UINavigationController* base_navigation_controller_ = nil;
  FakeReauthenticationModule* fake_reauth_module_ = nil;
  id mocked_scene_handler_;
  base::HistogramTester histogram_tester_;
  PasswordCheckupCoordinator* coordinator_ = nil;
  FakeSystemIdentity* fake_system_identity_;
};

// Test fixture for tests opening Password Checkup without authentication
// required.
class PasswordCheckupCoordinatorWithoutReauthenticationTest
    : public PasswordCheckupCoordinatorTest {};

// Test fixture for tests opening Password Checkup with authentication required.
class PasswordCheckupCoordinatorWithReauthenticationTest
    : public PasswordCheckupCoordinatorTest {};

// Tests that Password Checkup is presented without authentication required.
TEST_P(PasswordCheckupCoordinatorWithoutReauthenticationTest,
       PasswordCheckupPresentedWithoutAuth) {
  CheckPasswordCheckupIsPresented();

  // One visit should have been recorded.
  CheckPasswordCheckupVisitMetricsCount(1);
}

// Tests that opening Password Issues when `coordinator_` does not require
// authentication on start presents `PasswordIssuesTableViewController` directly
// without prompting.
TEST_P(PasswordCheckupCoordinatorWithoutReauthenticationTest,
       PasswordIssuesPresentedWithoutAuth) {
  CheckPasswordCheckupIsPresented();

  [coordinator_ showPasswordIssuesWithWarningType:
                    password_manager::WarningType::kWeakPasswordsWarning];

  UINavigationController* navigation_controller = base_navigation_controller_;
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return [navigation_controller.topViewController
                   isKindOfClass:[PasswordIssuesTableViewController class]] &&
               navigation_controller.transitionCoordinator == nil;
      }));
  EXPECT_EQ(nil, fake_reauth_module_.localizedReasonForAuthentication);
  histogram_tester_.ExpectBucketCount(
      password_manager::kPasswordManagerSurfaceVisitHistogramName,
      password_manager::PasswordManagerSurface::kPasswordIssues, 1);
}

// Tests that Password Check is presented only after passing authentication
TEST_P(PasswordCheckupCoordinatorWithReauthenticationTest,
       PasswordCheckupPresentedWithAuth) {
  // Checkup should be covered until auth is passed.
  CheckPasswordCheckupIsNotPresented();

  // No visits recorded until successful auth.
  CheckPasswordCheckupVisitMetricsCount(0);

  [fake_reauth_module_ returnMockedReauthenticationResult];

  // Successful auth should leave Checkup visible.
  CheckPasswordCheckupIsPresented();

  // One visit should have been recorded.
  CheckPasswordCheckupVisitMetricsCount(1);
}

// Tests that opening Password Issues after passing authentication on Password
// Checkup presents `PasswordIssuesTableViewController` directly without a
// second prompt.
TEST_P(PasswordCheckupCoordinatorWithReauthenticationTest,
       PasswordIssuesOpenedAfterAuthSkipsSecondAuth) {
  [fake_reauth_module_ returnMockedReauthenticationResult];
  CheckPasswordCheckupIsPresented();

  // Wait for `UINavigationController` to finish showing
  // `PasswordCheckupViewController` after popping
  // `LocalReauthenticationViewController` before pushing Password Issues.
  UINavigationController* navigation_controller = base_navigation_controller_;
  UIViewController* checkup_view_controller =
      navigation_controller.topViewController;
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return checkup_view_controller.viewIfLoaded.window != nil &&
               checkup_view_controller.view.userInteractionEnabled &&
               navigation_controller.transitionCoordinator == nil;
      }));

  fake_reauth_module_.localizedReasonForAuthentication = nil;
  [coordinator_ showPasswordIssuesWithWarningType:
                    password_manager::WarningType::kWeakPasswordsWarning];

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return [navigation_controller.topViewController
                   isKindOfClass:[PasswordIssuesTableViewController class]] &&
               navigation_controller.transitionCoordinator == nil;
      }));
  EXPECT_EQ(nil, fake_reauth_module_.localizedReasonForAuthentication);
  histogram_tester_.ExpectBucketCount(
      password_manager::kPasswordManagerSurfaceVisitHistogramName,
      password_manager::PasswordManagerSurface::kPasswordIssues, 1);
}

// Verifies that Password Checkup visits are logged only once after the first
// successful authentication.
TEST_P(PasswordCheckupCoordinatorWithReauthenticationTest,
       PasswordCheckupVisitRecordedOnlyOnce) {
  CheckPasswordCheckupVisitMetricsCount(0);

  [fake_reauth_module_ returnMockedReauthenticationResult];

  // Successful auth should record a visit
  CheckPasswordCheckupVisitMetricsCount(1);

  // Simulate scene transitioning to the background and back to foreground. This
  // should trigger an auth request.
  scene_state_.activationLevel = SceneActivationLevelForegroundInactive;
  scene_state_.activationLevel = SceneActivationLevelBackground;
  scene_state_.activationLevel = SceneActivationLevelForegroundInactive;
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  // Simulate successful auth.
  [fake_reauth_module_ returnMockedReauthenticationResult];

  // Validate no new visits were logged.
  CheckPasswordCheckupVisitMetricsCount(1);
}

// Test Password Checkup entry points that do not require authentication.
INSTANTIATE_TEST_SUITE_P(,  // Empty instantiation name.
                         PasswordCheckupCoordinatorWithoutReauthenticationTest,
                         Values(PasswordCheckReferrer::kPasswordSettings));

// Test Password Checkup entry points that require authentication.
INSTANTIATE_TEST_SUITE_P(,  // Empty instantiation name.
                         PasswordCheckupCoordinatorWithReauthenticationTest,
                         Values(PasswordCheckReferrer::kPhishGuardDialog,
                                PasswordCheckReferrer::kSafetyCheck,
                                PasswordCheckReferrer::kPasswordBreachDialog,
                                PasswordCheckReferrer::kMoreToFixBubble,
                                PasswordCheckReferrer::kSafetyCheckMagicStack));

// Test fixture for the entry point used by `SceneCoordinator` when Password
// Issues is opened directly from outside the Password Manager: starts
// `coordinator_` and opens Password Issues in the same call frame before
// `base_navigation_controller_` is presented.
class PasswordCheckupCoordinatorPasswordIssuesEntryTest
    : public PasswordCheckupCoordinatorTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    SetUpCoordinatorWithReferrer(GetParam());
  }

  // Replays the `SceneCoordinator` sequence: starts `coordinator_`, opens
  // Password Issues, attaches `base_navigation_controller_` to the window, and
  // waits until `LocalReauthenticationViewController` requests authentication.
  void StartCoordinatorAndOpenPasswordIssues() {
    [coordinator_ start];
    [coordinator_ showPasswordIssuesWithWarningType:
                      password_manager::WarningType::kWeakPasswordsWarning];
    scoped_window_.Get().rootViewController = base_navigation_controller_;

    EXPECT_FALSE([base_navigation_controller_.topViewController
        isKindOfClass:[PasswordIssuesTableViewController class]]);
    ASSERT_TRUE([base_navigation_controller_.topViewController
        isKindOfClass:[LocalReauthenticationViewController class]]);

    FakeReauthenticationModule* fake_reauth_module = fake_reauth_module_;
    ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForUIElementTimeout, ^bool {
          return fake_reauth_module.localizedReasonForAuthentication != nil;
        }));
  }
};

// Tests that opening Password Issues right after starting `coordinator_` and
// before `base_navigation_controller_` is presented requires Local
// Authentication, and navigating back after authenticating does not prompt a
// second time.
TEST_P(PasswordCheckupCoordinatorPasswordIssuesEntryTest,
       PasswordIssuesOpenedBeforePresentationRequiresAuth) {
  ASSERT_NO_FATAL_FAILURE(StartCoordinatorAndOpenPasswordIssues());

  // No surface visits recorded until successful authentication.
  histogram_tester_.ExpectBucketCount(
      password_manager::kPasswordManagerSurfaceVisitHistogramName,
      password_manager::PasswordManagerSurface::kPasswordIssues, 0);
  CheckPasswordCheckupVisitMetricsCount(0);

  [fake_reauth_module_ returnMockedReauthenticationResult];

  // Successful authentication should leave `PasswordIssuesTableViewController`
  // visible and record its visit.
  ASSERT_TRUE([base_navigation_controller_.topViewController
      isKindOfClass:[PasswordIssuesTableViewController class]]);
  histogram_tester_.ExpectBucketCount(
      password_manager::kPasswordManagerSurfaceVisitHistogramName,
      password_manager::PasswordManagerSurface::kPasswordIssues, 1);

  // Navigating back to Password Checkup should not require a second prompt.
  [base_navigation_controller_ popViewControllerAnimated:NO];
  EXPECT_TRUE([base_navigation_controller_.topViewController
      isKindOfClass:[PasswordCheckupViewController class]]);
  histogram_tester_.ExpectBucketCount(
      password_manager::kPasswordManagerSurfaceVisitHistogramName,
      password_manager::PasswordManagerSurface::kPasswordCheckup, 1);
}

// Tests that failing Local Authentication when Password Issues is opened
// directly dismisses the Password Manager UI without recording surface visits.
TEST_P(PasswordCheckupCoordinatorPasswordIssuesEntryTest,
       PasswordIssuesFailedAuthDismissesUI) {
  OCMockObject<PasswordCheckupCoordinatorDelegate>* mocked_delegate =
      OCMProtocolMock(@protocol(PasswordCheckupCoordinatorDelegate));
  OCMExpect(
      [mocked_delegate dismissPasswordManagerAfterFailedReauthentication]);
  coordinator_.delegate = mocked_delegate;
  fake_reauth_module_.expectedResult = ReauthenticationResult::kFailure;

  ASSERT_NO_FATAL_FAILURE(StartCoordinatorAndOpenPasswordIssues());

  [fake_reauth_module_ returnMockedReauthenticationResult];

  EXPECT_OCMOCK_VERIFY(mocked_delegate);
  histogram_tester_.ExpectBucketCount(
      password_manager::kPasswordManagerSurfaceVisitHistogramName,
      password_manager::PasswordManagerSurface::kPasswordIssues, 0);
  CheckPasswordCheckupVisitMetricsCount(0);
}

// Tests that if Password Issues is removed before Local Authentication
// succeeds, Password Checkup is not left unauthenticated.
TEST_P(PasswordCheckupCoordinatorPasswordIssuesEntryTest,
       PasswordIssuesRemovedBeforeAuthDoesNotLeaveCheckupUnauthenticated) {
  __block bool dismissed = false;
  id<PasswordCheckupCoordinatorDelegate> mocked_delegate =
      OCMProtocolMock(@protocol(PasswordCheckupCoordinatorDelegate));
  OCMStub([mocked_delegate dismissPasswordManagerAfterFailedReauthentication])
      .andDo(^(NSInvocation*) {
        dismissed = true;
      });
  coordinator_.delegate = mocked_delegate;

  ASSERT_NO_FATAL_FAILURE(StartCoordinatorAndOpenPasswordIssues());

  // Pop `LocalReauthenticationViewController` and
  // `PasswordIssuesTableViewController` without completing authentication.
  [base_navigation_controller_ popViewControllerAnimated:NO];
  [base_navigation_controller_ popViewControllerAnimated:NO];

  // Either `PasswordCheckupCoordinator` re-pushes
  // `LocalReauthenticationViewController` or fails closed by dismissing.
  if (!dismissed) {
    EXPECT_TRUE([base_navigation_controller_.topViewController
        isKindOfClass:[LocalReauthenticationViewController class]]);
    EXPECT_FALSE([base_navigation_controller_.topViewController
        isKindOfClass:[PasswordCheckupViewController class]]);
  }
  CheckPasswordCheckupVisitMetricsCount(0);
}

// Entry points opening Password Issues directly from outside the Password
// Manager.
INSTANTIATE_TEST_SUITE_P(
    ,  // Empty instantiation name.
    PasswordCheckupCoordinatorPasswordIssuesEntryTest,
    Values(PasswordCheckReferrer::kSafetyCheckMagicStack,
           PasswordCheckReferrer::kSafetyCheckNotification));

}  // namespace password_manager
