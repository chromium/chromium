// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/task_updater_scene_agent.h"

#import <string>

#import "base/test/scoped_feature_list.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/fake_startup_information.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_test_utils.h"
#import "ios/chrome/app/task_orchestrator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_in_progress.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Fake SceneState to set sceneSessionID.
@interface TaskUpdaterFakeSceneState : SceneState
- (void)setFakeSceneSessionID:(const std::string&)sessionID;
@end

@implementation TaskUpdaterFakeSceneState {
  std::string _fakeSceneSessionID;
}

- (const std::string&)sceneSessionID {
  return _fakeSceneSessionID;
}

- (void)setFakeSceneSessionID:(const std::string&)sessionID {
  _fakeSceneSessionID = sessionID;
}
@end

// Fake TaskOrchestrator to record calls.
@interface FakeTaskOrchestrator : TaskOrchestrator
@property(nonatomic, strong) NSMutableArray<NSNumber*>* stages;
@end

@implementation FakeTaskOrchestrator

- (instancetype)init {
  if ((self = [super init])) {
    _stages = [NSMutableArray array];
  }
  return self;
}

- (void)updateToStage:(TaskExecutionStage)stage
             forScene:(std::string_view)sceneSessionID {
  [_stages addObject:@(static_cast<int>(stage))];
}

@end

class TaskUpdaterSceneAgentTest : public PlatformTest {
 protected:
  TaskUpdaterSceneAgentTest() {
    ResetEnableNewStartupFlowEnabledForTesting();
    scoped_feature_list_.InitAndEnableFeature(kEnableNewStartupFlow);
    SaveEnableNewStartupFlowForNextStart();
  }

  ~TaskUpdaterSceneAgentTest() override {
    ResetEnableNewStartupFlowEnabledForTesting();
  }

  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    profile_ = std::move(builder).Build();

    fake_startup_information_ = [[FakeStartupInformation alloc] init];
    app_state_ =
        [[AppState alloc] initWithStartupInformation:fake_startup_information_];

    fake_task_orchestrator_ = [[FakeTaskOrchestrator alloc] init];
    [app_state_ setValue:fake_task_orchestrator_ forKey:@"taskOrchestrator"];

    profile_state_ = [[ProfileState alloc] initWithAppState:app_state_];
    profile_state_.profile = profile_.get();

    scene_state_ =
        [[TaskUpdaterFakeSceneState alloc] initWithAppState:app_state_];
    [scene_state_ setFakeSceneSessionID:"scene-1"];
    scene_state_.profileState = profile_state_;

    agent_ = [[TaskUpdaterSceneAgent alloc] init];
    [scene_state_ addAgent:agent_];
  }

  AuthenticationService* auth_service() {
    return AuthenticationServiceFactory::GetForProfile(profile_.get());
  }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(profile_.get());
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  FakeStartupInformation* fake_startup_information_;
  AppState* app_state_;
  FakeTaskOrchestrator* fake_task_orchestrator_;
  ProfileState* profile_state_;
  TaskUpdaterFakeSceneState* scene_state_;
  TaskUpdaterSceneAgent* agent_;
};

// Tests that TaskExecutionProfileLoaded is sent when profile is loaded.
TEST_F(TaskUpdaterSceneAgentTest, TestProfileLoaded) {
  SetProfileStateInitStage(profile_state_, ProfileInitStage::kProfileLoaded);

  EXPECT_TRUE([fake_task_orchestrator_.stages
      containsObject:@(static_cast<int>(
                         TaskExecutionStage::TaskExecutionProfileLoaded))]);
}

// Tests that TaskExecutionUIReady is sent when UI is ready.
TEST_F(TaskUpdaterSceneAgentTest, TestUIReady) {
  // Set Profile stage to Final.
  SetProfileStateInitStage(profile_state_, ProfileInitStage::kFinal);

  // UI not enabled yet.
  EXPECT_FALSE([fake_task_orchestrator_.stages
      containsObject:@(static_cast<int>(
                         TaskExecutionStage::TaskExecutionUIReady))]);

  // Enable UI and ForegroundActive.
  scene_state_.UIEnabled = YES;
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  EXPECT_TRUE([fake_task_orchestrator_.stages
      containsObject:@(static_cast<int>(
                         TaskExecutionStage::TaskExecutionUIReady))]);
}

// Tests that TaskExecutionUIReady is NOT sent if there is a UI blocker.
TEST_F(TaskUpdaterSceneAgentTest, TestUIBlocker) {
  SetProfileStateInitStage(profile_state_, ProfileInitStage::kFinal);
  scene_state_.UIEnabled = YES;

  // Set a UI blocker before becoming active.
  TaskUpdaterFakeSceneState* blocker_target =
      [[TaskUpdaterFakeSceneState alloc] initWithAppState:app_state_];
  [profile_state_ incrementBlockingUICounterForTarget:blocker_target];

  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  // Should NOT be ready.
  EXPECT_FALSE([fake_task_orchestrator_.stages
      containsObject:@(static_cast<int>(
                         TaskExecutionStage::TaskExecutionUIReady))]);

  // Remove UI blocker.
  [profile_state_ decrementBlockingUICounterForTarget:blocker_target];

  EXPECT_TRUE([fake_task_orchestrator_.stages
      containsObject:@(static_cast<int>(
                         TaskExecutionStage::TaskExecutionUIReady))]);
}

// Tests that TaskExecutionUIReady is NOT sent if presenting modal overlay.
TEST_F(TaskUpdaterSceneAgentTest, TestModalOverlay) {
  SetProfileStateInitStage(profile_state_, ProfileInitStage::kFinal);
  scene_state_.UIEnabled = YES;

  // Set presenting modal overlay before becoming active.
  scene_state_.presentingModalOverlay = YES;

  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  // Should NOT be ready.
  EXPECT_FALSE([fake_task_orchestrator_.stages
      containsObject:@(static_cast<int>(
                         TaskExecutionStage::TaskExecutionUIReady))]);

  // Hide modal overlay.
  scene_state_.presentingModalOverlay = NO;

  EXPECT_TRUE([fake_task_orchestrator_.stages
      containsObject:@(static_cast<int>(
                         TaskExecutionStage::TaskExecutionUIReady))]);
}

// Tests that TaskExecutionUIReady is NOT sent if signin is forced by policy
// and signin is in progress.
TEST_F(TaskUpdaterSceneAgentTest, TestSigninForcedByPolicy_InProgress) {
  SetProfileStateInitStage(profile_state_, ProfileInitStage::kFinal);
  scene_state_.UIEnabled = YES;

  // Sign in an account first.
  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentityManager* system_identity_manager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  system_identity_manager->AddIdentity(identity);
  auth_service()->SignIn(identity, signin_metrics::AccessPoint::kSettings);

  // Force signin by policy.
  GetApplicationContext()->GetLocalState()->SetInteger(
      prefs::kBrowserSigninPolicy,
      static_cast<int>(BrowserSigninMode::kForced));

  // Signin in progress.
  {
    std::unique_ptr<SigninInProgress> signin_in_progress =
        [scene_state_ createSigninInProgress];

    scene_state_.activationLevel = SceneActivationLevelForegroundActive;

    // Should NOT be ready because signinInProgress is YES.
    EXPECT_FALSE([fake_task_orchestrator_.stages
        containsObject:@(static_cast<int>(
                           TaskExecutionStage::TaskExecutionUIReady))]);
  }

  // signin_in_progress destroyed, signinDidEnd: called.
  // Should be ready now.
  EXPECT_TRUE([fake_task_orchestrator_.stages
      containsObject:@(static_cast<int>(
                         TaskExecutionStage::TaskExecutionUIReady))]);
}
