// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/password_details_coordinator.h"

#import "base/test/bind.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/password_store/test_password_store.h"
#import "components/password_manager/core/browser/ui/affiliated_group.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/metrics/ios_password_manager_metrics.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/sync/model/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_handler.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_metrics.h"
#import "ios/chrome/test/app/mock_reauthentication_module.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using base::HistogramTester;

namespace {

// Creates an affiliated group with one credential.
password_manager::AffiliatedGroup GetTestAffiliatedGroup() {
  password_manager::PasswordForm form;
  form.url = GURL("https://example.com");
  form.username_value = u"user";
  form.password_value = u"password";
  password_manager::CredentialUIEntry credential(form);
  return password_manager::AffiliatedGroup(
      /*credentials=*/{credential},
      /*branding=*/affiliations::FacetBrandingInfo());
}

// Registers a mock command handler in the dispatcher.
void HandleCommand(Protocol* command_protocol, CommandDispatcher* dispatcher) {
  id mocked_handler = OCMStrictProtocolMock(command_protocol);
  [dispatcher startDispatchingToTarget:mocked_handler
                           forProtocol:command_protocol];
}

// Verifies that a given number of password details visits have been recorded.
void CheckPasswordDetailsVisitMetricsCount(
    int count,
    const HistogramTester& histogram_tester) {
  histogram_tester.ExpectUniqueSample(
      /*name=*/password_manager::kPasswordManagerSurfaceVisitHistogramName,
      /*sample=*/password_manager::PasswordManagerSurface::kPasswordDetails,
      /*count=*/count);
}

}  // namespace

// Test fixture for testing the PasswordDetailsCoordinatorTest class.
class PasswordDetailsCoordinatorTest : public PlatformTest {
 protected:
  PasswordDetailsCoordinatorTest() {
    TestProfileIOS::Builder builder;

    builder.AddTestingFactory(
        IOSChromeProfilePasswordStoreFactory::GetInstance(),
        base::BindRepeating(
            &password_manager::BuildPasswordStore<
                web::BrowserState, password_manager::TestPasswordStore>));

    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));

    // Create scene state for reauthentication coordinator.
    scene_state_ = [[SceneState alloc] initWithAppState:nil];
    scene_state_.activationLevel = SceneActivationLevelForegroundActive;

    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);

    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    // Mock ApplicationCommands and SettingsCommands
    HandleCommand(@protocol(ApplicationCommands), dispatcher);
    HandleCommand(@protocol(SettingsCommands), dispatcher);

    // Mock SnackbarCommands.
    HandleCommand(@protocol(SnackbarCommands), dispatcher);

    mock_reauth_module_ = [[MockReauthenticationModule alloc] init];
    // Delay auth result so auth doesn't pass right after requested by the
    // coordinator. Needed for verifying behavior when auth is required.
    mock_reauth_module_.shouldSkipReAuth = NO;
    mock_reauth_module_.expectedResult = ReauthenticationResult::kSuccess;

    UINavigationController* navigation_controller =
        [[UINavigationController alloc] init];
    coordinator_ = [[PasswordDetailsCoordinator alloc]
        initWithBaseNavigationController:navigation_controller
                                 browser:browser_.get()
                         affiliatedGroup:GetTestAffiliatedGroup()
                            reauthModule:mock_reauth_module_
                                 context:DetailsContext::kPasswordSettings];
  }

  ~PasswordDetailsCoordinatorTest() override { [coordinator_ stop]; }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  MockReauthenticationModule* mock_reauth_module_;
  SceneState* scene_state_;
  PasswordDetailsCoordinator* coordinator_;
};

#pragma mark - Tests

// Tests Password Visit metrics are logged only once after opening the surface.
TEST_F(PasswordDetailsCoordinatorTest, VisitMetricsAreLoggedOnlyOnce) {
  HistogramTester histogram_tester;
  CheckPasswordDetailsVisitMetricsCount(0, histogram_tester);

  // Starting the coordinator should record a visit.
  [coordinator_ start];
  CheckPasswordDetailsVisitMetricsCount(1, histogram_tester);

  // Simulate scene transitioning to the background and back to foreground. This
  // should trigger an auth request.
  scene_state_.activationLevel = SceneActivationLevelForegroundInactive;
  scene_state_.activationLevel = SceneActivationLevelBackground;
  scene_state_.activationLevel = SceneActivationLevelForegroundInactive;
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  // Simulate successful auth.
  [mock_reauth_module_ returnMockedReauthenticationResult];

  // Validate no new visits were logged.
  CheckPasswordDetailsVisitMetricsCount(1, histogram_tester);
}

// Tests that onShareButtonPressed will result in metrics logged.
TEST_F(PasswordDetailsCoordinatorTest, OnShareButtonPressedMetricsLogged) {
  base::HistogramTester histogram_tester;

  // Call the tested function.
  ASSERT_TRUE(
      [coordinator_ conformsToProtocol:@protocol(PasswordDetailsHandler)]);
  [(id<PasswordDetailsHandler>)coordinator_ onShareButtonPressed];

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordSharingIOS.UserAction",
      PasswordSharingInteraction::kPasswordDetailsShareButtonClicked, 1);
}
