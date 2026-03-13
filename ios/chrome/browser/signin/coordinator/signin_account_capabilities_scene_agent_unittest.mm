// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/coordinator/signin_account_capabilities_scene_agent.h"

#import "base/memory/raw_ptr.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_test_utils.h"
#import "ios/chrome/browser/scoped_ui_blocker/ui_bundled/ui_blocker_manager.h"
#import "ios/chrome/browser/scoped_ui_blocker/ui_bundled/ui_blocker_target.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_ui_provider.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class SigninAccountCapabilitiesSceneAgentTest : public PlatformTest {
 public:
  SigninAccountCapabilitiesSceneAgentTest() : PlatformTest() {
    fake_system_identity_manager_ =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());

    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();

    app_state_ = OCMClassMock([AppState class]);
    scene_state_ = [[SceneState alloc] initWithAppState:app_state_];
    scene_ui_provider_ = OCMProtocolMock(@protocol(SceneUIProvider));

    profile_state_ = [[ProfileState alloc] initWithAppState:app_state_];
    profile_state_.profile = profile_.get();
    SetProfileStateInitStage(profile_state_, ProfileInitStage::kFinal);
    scene_state_.profileState = profile_state_;

    agent_ = [[SigninAccountCapabilitiesSceneAgent alloc]
        initWithSceneUIProvider:scene_ui_provider_];
    agent_.sceneState = scene_state_;
  }

  ~SigninAccountCapabilitiesSceneAgentTest() override {
    [agent_ sceneStateDidDisableUI:scene_state_];
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  id<SceneUIProvider> scene_ui_provider_;
  ProfileState* profile_state_;
  AppState* app_state_;
  SceneState* scene_state_;
  SigninAccountCapabilitiesSceneAgent* agent_;
  raw_ptr<FakeSystemIdentityManager> fake_system_identity_manager_;
};

// Tests that the agent fetches capabilities for all identities on activation.
TEST_F(SigninAccountCapabilitiesSceneAgentTest, TestFetchOnActivation) {
  FakeSystemIdentity* identity1 = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* identity2 = [FakeSystemIdentity fakeIdentity2];
  fake_system_identity_manager_->AddIdentity(identity1);
  fake_system_identity_manager_->AddIdentity(identity2);

  __block int build_context_calls = 0;
  fake_system_identity_manager_->SetBuildExternalPrivacyContextCallback(
      base::BindRepeating(^(
          id<SystemIdentity> identity, UIViewController* view_controller,
          SystemIdentityManager::BuildExternalPrivacyContextCallback callback) {
        build_context_calls++;
        std::move(callback).Run(nil);
      }));

  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  EXPECT_EQ(build_context_calls, 2);
}

// Tests that the agent fetches capabilities for new identities when the list
// changes.
TEST_F(SigninAccountCapabilitiesSceneAgentTest,
       TestFetchOnIdentityListChanged) {
  FakeSystemIdentity* identity1 = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager_->AddIdentity(identity1);

  __block int build_context_calls_identity1 = 0;
  __block int build_context_calls_identity2 = 0;
  fake_system_identity_manager_->SetBuildExternalPrivacyContextCallback(
      base::BindRepeating(^(
          id<SystemIdentity> identity, UIViewController* view_controller,
          SystemIdentityManager::BuildExternalPrivacyContextCallback callback) {
        if (identity.gaiaId == identity1.gaiaId) {
          build_context_calls_identity1++;
        } else {
          build_context_calls_identity2++;
        }
        std::move(callback).Run(nil);
      }));

  // Initial fetch.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  EXPECT_EQ(build_context_calls_identity1, 1);
  EXPECT_EQ(build_context_calls_identity2, 0);

  // Add a second identity.
  FakeSystemIdentity* identity2 = [FakeSystemIdentity fakeIdentity2];

  fake_system_identity_manager_->AddIdentity(identity2);
  fake_system_identity_manager_->FireSystemIdentityReloaded();

  // Identity 1 should NOT be refetched. Identity 2 should be fetched.
  EXPECT_EQ(build_context_calls_identity1, 1);  // Remains 1
  EXPECT_EQ(build_context_calls_identity2, 1);
}

// Tests that removing an identity clears it from the handled set.
TEST_F(SigninAccountCapabilitiesSceneAgentTest, TestIdentityRemoval) {
  FakeSystemIdentity* identity1 = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager_->AddIdentity(identity1);

  __block int build_context_calls = 0;
  fake_system_identity_manager_->SetBuildExternalPrivacyContextCallback(
      base::BindRepeating(^(
          id<SystemIdentity> identity, UIViewController* view_controller,
          SystemIdentityManager::BuildExternalPrivacyContextCallback callback) {
        build_context_calls++;
        std::move(callback).Run(nil);
      }));

  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  EXPECT_EQ(build_context_calls, 1);

  // Forget identity.
  fake_system_identity_manager_->ForgetIdentityFromOtherApplication(identity1);
  fake_system_identity_manager_->FireSystemIdentityReloaded();

  // Add it back. It should be refetched.
  fake_system_identity_manager_->AddIdentity(identity1);
  fake_system_identity_manager_->FireSystemIdentityReloaded();

  EXPECT_EQ(build_context_calls, 2);
}

// Tests that the agent fetches capabilities when a UI blocker is removed.
TEST_F(SigninAccountCapabilitiesSceneAgentTest, TestFetchOnUIBlockerRemoved) {
  FakeSystemIdentity* identity1 = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager_->AddIdentity(identity1);

  __block int build_context_calls = 0;
  fake_system_identity_manager_->SetBuildExternalPrivacyContextCallback(
      base::BindRepeating(^(
          id<SystemIdentity> identity, UIViewController* view_controller,
          SystemIdentityManager::BuildExternalPrivacyContextCallback callback) {
        build_context_calls++;
        std::move(callback).Run(nil);
      }));

  // Set a UI blocker before becoming active.
  id<UIBlockerTarget> blocker_target =
      OCMProtocolMock(@protocol(UIBlockerTarget));
  [profile_state_ incrementBlockingUICounterForTarget:blocker_target];

  // Activation should not trigger fetch because UI is blocked.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  EXPECT_EQ(build_context_calls, 0);

  // Remove UI blocker.
  [profile_state_ decrementBlockingUICounterForTarget:blocker_target];

  // The fetch should now be triggered.
  EXPECT_EQ(build_context_calls, 1);
}
