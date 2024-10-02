// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/ui_bundled/idle/idle_timeout_policy_scene_agent.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/test/task_environment.h"
#import "components/enterprise/idle/idle_pref_names.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/fake_startup_information.h"
#import "ios/chrome/browser/enterprise/model/idle/action_runner.h"
#import "ios/chrome/browser/enterprise/model/idle/idle_service.h"
#import "ios/chrome/browser/policy/ui_bundled/idle/idle_timeout_confirmation_coordinator_delegate.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_ui_provider.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/scoped_ui_blocker/scoped_ui_blocker.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using ::testing::_;

// A fake that allows setting initStage.
@interface FakeSceneUIProvider<SceneUIProvider> : NSObject
@end

@implementation FakeSceneUIProvider {
  UIViewController* _viewControllerForTesting;
}

- (void)initViewController:(UIViewController*)viewController {
  _viewControllerForTesting = viewController;
}

- (UIViewController*)activeViewController {
  return _viewControllerForTesting;
}

@end

// A fake that allows setting initStage.
@interface FakeAppStateForAgent : AppState

// Init stage that will be returned by the initStage getter when testing.
@property(nonatomic, assign) AppInitStage initStageForTesting;

@end

@implementation FakeAppStateForAgent

- (AppInitStage)initStage {
  return self.initStageForTesting;
}

@end

// Mocks the `Run()` method which is used to check that actions run the
// right time(s) in the tests.
class MockActionRunner : public enterprise_idle::ActionRunner {
 public:
  MockActionRunner() {}
  MOCK_METHOD(void,
              Run,
              (enterprise_idle::ActionRunner::ActionsCompletedCallback),
              (override));
  ~MockActionRunner() override {}
};

class IdleTimeoutPolicySceneAgentTest : public PlatformTest {
 public:
  IdleTimeoutPolicySceneAgentTest() = default;

  void SetUp() override {
    SetUpAppState();
    SetIdleTimeoutPolicies();
    InitIdleService();
    InitSceneWithAgent();
  }

  void TearDown() override { [agent_ sceneStateDidDisableUI:scene_state_]; }

  void SetUpAppState() {
    // Set up scene state.
    profile_ = TestProfileIOS::Builder().Build();
    startup_information_ = [[FakeStartupInformation alloc] init];
    app_state_ = [[FakeAppStateForAgent alloc]
        initWithStartupInformation:startup_information_];
  }

  void InitIdleService() {
    idle_service_ = std::make_unique<enterprise_idle::IdleService>(
        profile_->GetOriginalProfile());
    idle_service_->SetActionRunnerForTesting(
        base::WrapUnique(new MockActionRunner()));
  }

  void SetIdleTimeoutPolicies() {
    PrefService* prefs = profile_->GetPrefs();
    prefs->SetTimeDelta(enterprise_idle::prefs::kIdleTimeout, base::Minutes(1));
    base::Value::List actions;
    actions.Append(
        static_cast<int>(enterprise_idle::ActionType::kClearBrowsingHistory));
    // Set the `IdleTimeoutActions` policy. This is needed for the snackbar
    // message.
    prefs->SetList(
        enterprise_idle::prefs::kIdleTimeoutActions, std::move(actions));
  }

  void InitSceneWithAgent() {
    scene_state_ = [[FakeSceneState alloc] initWithAppState:app_state_
                                                    profile:profile_.get()];
    scene_state_.scene = static_cast<UIWindowScene*>(
        [[[UIApplication sharedApplication] connectedScenes] anyObject]);

    // Set up the idle timeout scene agent.
    Browser* browser =
        scene_state_.browserProviderInterface.mainBrowserProvider.browser;
    id<SceneUIProvider> fake_provider =
        OCMStrictProtocolMock(@protocol(SceneUIProvider));

    // Create mock command handlers. These are just for initializing the view
    // controller; because the handlers are local to this methdd, they will not
    // exist during tests, so if the tests call any commands they will fail.
    mock_application_handler_ = OCMProtocolMock(@protocol(ApplicationCommands));
    mockSettingsHandler_ = OCMProtocolMock(@protocol(SettingsCommands));
    mock_snackbar_handler_ = OCMProtocolMock(@protocol(SnackbarCommands));

    CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
    [dispatcher startDispatchingToTarget:mock_snackbar_handler_
                             forProtocol:@protocol(SnackbarCommands)];
    [dispatcher startDispatchingToTarget:mock_application_handler_
                             forProtocol:@protocol(ApplicationCommands)];
    [dispatcher startDispatchingToTarget:mockSettingsHandler_
                             forProtocol:@protocol(SettingsCommands)];

    agent_ = [[IdleTimeoutPolicySceneAgent alloc]
           initWithSceneUIProvider:fake_provider
        applicationCommandsHandler:HandlerForProtocol(dispatcher,
                                                      ApplicationCommands)
           snackbarCommandsHandler:HandlerForProtocol(dispatcher,
                                                      SnackbarCommands)
                       idleService:idle_service_.get()
                       mainBrowser:browser];

    agent_.sceneState = scene_state_;
    [agent_ sceneStateDidEnableUI:scene_state_];
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<enterprise_idle::IdleService> idle_service_;
  FakeStartupInformation* startup_information_;
  FakeAppStateForAgent* app_state_;
  // The scene state that the agent works with.
  FakeSceneState* scene_state_;
  // The agent under test.
  IdleTimeoutPolicySceneAgent* agent_;
  id mock_snackbar_handler_;
  id mock_application_handler_;
  id mockSettingsHandler_;
};

// The UI should not be showing the dialog or blocking other scenes if the app
// state has not reached its final init stage.
TEST_F(IdleTimeoutPolicySceneAgentTest, DialogDoesNotShowWhenAppStateNotReady) {
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  app_state_.initStageForTesting = AppInitStage::kEnterprise;
  OCMReject([mock_application_handler_
      dismissModalDialogsWithCompletion:[OCMArg any]]);
  idle_service_->RunActionsForStateForTesting(
      enterprise_idle::IdleService::LastState::kIdleOnForeground);
  EXPECT_NE(app_state_.currentUIBlocker, scene_state_);
  EXPECT_OCMOCK_VERIFY(mock_application_handler_);
}

// The UI should show the dialog and block other scenes if the app state has
// reached its final init stage.
TEST_F(IdleTimeoutPolicySceneAgentTest, DialogShowsWhenAppStateReady) {
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  app_state_.initStageForTesting = AppInitStage::kFinal;
  OCMExpect([mock_application_handler_
      dismissModalDialogsWithCompletion:[OCMArg any]]);
  idle_service_->RunActionsForStateForTesting(
      enterprise_idle::IdleService::LastState::kIdleOnForeground);
  EXPECT_EQ(app_state_.currentUIBlocker, scene_state_);
  EXPECT_OCMOCK_VERIFY(mock_application_handler_);
}

// The UI should not show the dialog when the scene is not foregrounded.
// This case will likely never happen.
TEST_F(IdleTimeoutPolicySceneAgentTest,
       DialogDoesNotShowWhenSceneStateNotInForeground) {
  app_state_.initStageForTesting = AppInitStage::kFinal;
  scene_state_.activationLevel = SceneActivationLevelForegroundInactive;
  OCMReject([mock_application_handler_
      dismissModalDialogsWithCompletion:[OCMArg any]]);
  idle_service_->RunActionsForStateForTesting(
      enterprise_idle::IdleService::LastState::kIdleOnForeground);
  EXPECT_NE(app_state_.currentUIBlocker, scene_state_);
  EXPECT_OCMOCK_VERIFY(mock_application_handler_);
}

// The UI should show the dialog when the scene is foregrounded.
TEST_F(IdleTimeoutPolicySceneAgentTest, DialogShowsWhenSceneStateInForeground) {
  app_state_.initStageForTesting = AppInitStage::kFinal;
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  OCMExpect([mock_application_handler_
      dismissModalDialogsWithCompletion:[OCMArg any]]);
  idle_service_->RunActionsForStateForTesting(
      enterprise_idle::IdleService::LastState::kIdleOnForeground);
  EXPECT_EQ(app_state_.currentUIBlocker, scene_state_);
  EXPECT_OCMOCK_VERIFY(mock_application_handler_);
}

// The UI should not show on a scene that is blocked by an overlay modal.
TEST_F(IdleTimeoutPolicySceneAgentTest,
       DialogDoesNotShowWhenSceneStateBlockedByOtherScene) {
  app_state_.initStageForTesting = AppInitStage::kFinal;
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  scene_state_.presentingModalOverlay = true;
  OCMReject([mock_application_handler_
      dismissModalDialogsWithCompletion:[OCMArg any]]);
  idle_service_->RunActionsForStateForTesting(
      enterprise_idle::IdleService::LastState::kIdleOnForeground);
  EXPECT_NE(app_state_.currentUIBlocker, scene_state_);
  EXPECT_OCMOCK_VERIFY(mock_application_handler_);
}

// The snackbar should show when the actions are completed.
TEST_F(IdleTimeoutPolicySceneAgentTest,
       SnackbarShowsOnActionsCompletedWhenUIAvailable) {
  app_state_.initStageForTesting = AppInitStage::kFinal;
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  // Simulate that app ran actions on reforeground, and action bar showed
  // after actions ran since the app is foregrounded and active.
  idle_service_->RunActionsForStateForTesting(
      enterprise_idle::IdleService::LastState::kIdleOnBackground);
  OCMExpect([mock_snackbar_handler_ showSnackbarMessage:[OCMArg isNotNil]]);
  idle_service_->OnActionsCompleted();
  EXPECT_FALSE(idle_service_->ShouldIdleTimeoutSnackbarBePresented());
  EXPECT_OCMOCK_VERIFY(mock_snackbar_handler_);
}

// The snackbar should not show when the actions are completed but the scene is
// not foregrounded yet.
TEST_F(IdleTimeoutPolicySceneAgentTest,
       NoSnackbarShowsOnActionsCompletedWhenUINotAvailable) {
  app_state_.initStageForTesting = AppInitStage::kFinal;
  scene_state_.activationLevel = SceneActivationLevelForegroundInactive;
  // Simulate that app ran actions on reforeground. The snack bar does not show
  // after actions run since the app is not foregrounded. The snackbar will be
  // pending display.
  OCMReject([mock_snackbar_handler_ showSnackbarMessage:[OCMArg any]]);
  idle_service_->RunActionsForStateForTesting(
      enterprise_idle::IdleService::LastState::kIdleOnBackground);
  idle_service_->OnActionsCompleted();
  EXPECT_TRUE(idle_service_->ShouldIdleTimeoutSnackbarBePresented());
  EXPECT_OCMOCK_VERIFY(mock_snackbar_handler_);
}

// A  snackbar should show when the actions are completed and the scene
// is foregrounded.
TEST_F(IdleTimeoutPolicySceneAgentTest,
       PendingSnackbarShowsOnTransitionToActiveForegroundedScene) {
  app_state_.initStageForTesting = AppInitStage::kFinal;
  scene_state_.activationLevel = SceneActivationLevelForegroundInactive;
  // Simulate that app ran actions on reforeground. If a snackbar is pending
  // after actions run to completion, it should show when the scebe state
  // transitions to `SceneActivationLevelForegroundActive`.
  idle_service_->RunActionsForStateForTesting(
      enterprise_idle::IdleService::LastState::kIdleOnBackground);
  OCMExpect([mock_snackbar_handler_ showSnackbarMessage:[OCMArg isNotNil]]);
  idle_service_->OnActionsCompleted();
  EXPECT_TRUE(idle_service_->ShouldIdleTimeoutSnackbarBePresented());
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  [agent_ sceneState:scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];
  EXPECT_FALSE(idle_service_->ShouldIdleTimeoutSnackbarBePresented());
  EXPECT_OCMOCK_VERIFY(mock_snackbar_handler_);
}
