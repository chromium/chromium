// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_scene_agent.h"

#import "base/feature_list.h"
#import "base/test/scoped_feature_list.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/features.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_constants.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/stub_browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#pragma mark - StubReauthenticationModule

@interface StubReauthenticationModule : NSObject <ReauthenticationProtocol>

@property(nonatomic, assign) BOOL canAttemptReauthWithBiometrics;
@property(nonatomic, assign) BOOL canAttemptReauth;
@property(nonatomic, assign) ReauthenticationResult returnedResult;

@end

@implementation StubReauthenticationModule

- (void)attemptReauthWithLocalizedReason:(NSString*)localizedReason
                    canReusePreviousAuth:(BOOL)canReusePreviousAuth
                                 handler:
                                     (void (^)(ReauthenticationResult success))
                                         handler {
  handler(self.returnedResult);
}

@end

namespace {

#pragma mark - IncognitoReauthSceneAgentTest

class IncognitoReauthSceneAgentTest : public PlatformTest {
 public:
  IncognitoReauthSceneAgentTest()
      : profile_(TestProfileIOS::Builder().Build()),
        scene_state_([[SceneState alloc] initWithAppState:nil]),
        scene_state_mock_(OCMPartialMock(scene_state_)),
        stub_reauth_module_([[StubReauthenticationModule alloc] init]),
        agent_([[IncognitoReauthSceneAgent alloc]
            initWithReauthModule:stub_reauth_module_]) {
    [scene_state_ addAgent:agent_];
  }

 protected:
  void SetUpTestObjects(int tab_count,
                        bool reauth_enabled,
                        bool soft_lock_enabled) {
    // Stub all calls to be able to mock the following:
    // 1. sceneState.browserProviderInterface.incognitoBrowserProvider
    //            .browser->GetWebStateList()->count()
    // 2. sceneState.browserProviderInterface.hasIncognitoBrowserProvider
    test_browser_ = std::make_unique<TestBrowser>(profile_.get());
    for (int i = 0; i < tab_count; ++i) {
      test_browser_->GetWebStateList()->InsertWebState(
          std::make_unique<web::FakeWebState>(),
          WebStateList::InsertionParams::AtIndex(i));
    }

    stub_browser_interface_provider_ =
        [[StubBrowserProviderInterface alloc] init];
    stub_browser_interface_provider_.incognitoBrowserProvider.browser =
        test_browser_.get();

    OCMStub([scene_state_mock_ browserProviderInterface])
        .andReturn(stub_browser_interface_provider_);

    [IncognitoReauthSceneAgent registerLocalState:pref_service_.registry()];
    agent_.localState = &pref_service_;
    pref_service_.SetBoolean(prefs::kIncognitoAuthenticationSetting,
                             reauth_enabled);
    feature_list_.InitWithFeatureState(kIOSSoftLock, soft_lock_enabled);
  }

  void SetUpTestObjects(int tab_count, bool enable_pref) {
    SetUpTestObjects(tab_count, enable_pref, false);
  }

  void SetUp() override {
    // Set up default stub reauth module behavior.
    stub_reauth_module_.canAttemptReauthWithBiometrics = YES;
    stub_reauth_module_.canAttemptReauth = YES;
    stub_reauth_module_.returnedResult = ReauthenticationResult::kSuccess;
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;

  // The scene state that the agent works with.
  SceneState* scene_state_;
  // Partial mock for stubbing scene_state_'s methods
  id scene_state_mock_;
  StubReauthenticationModule* stub_reauth_module_;
  // The tested agent
  IncognitoReauthSceneAgent* agent_;
  StubBrowserProviderInterface* stub_browser_interface_provider_;
  std::unique_ptr<TestBrowser> test_browser_;
  TestingPrefServiceSimple pref_service_;
  base::test::ScopedFeatureList feature_list_;
};

// Test that when the feature pref is disabled, auth isn't required.
TEST_F(IncognitoReauthSceneAgentTest, PrefDisabled) {
  SetUpTestObjects(/*tab_count=*/1,
                   /*enable_pref=*/false);

  // Go foreground.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  EXPECT_FALSE(agent_.authenticationRequired);
}

// Test that when the feature is enabled, we're foregrounded with some incognito
// content already present, auth is required
TEST_F(IncognitoReauthSceneAgentTest, NeedsAuth) {
  SetUpTestObjects(/*tab_count=*/1, /*enable_pref=*/true);

  // Go foreground.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  EXPECT_TRUE(agent_.authenticationRequired);
}

// Test that when auth is required and is successfully performed, it's not
// required anymore.
TEST_F(IncognitoReauthSceneAgentTest, SuccessfulAuth) {
  SetUpTestObjects(/*tab_count=*/1, /*enable_pref=*/true);

  // Go foreground.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  EXPECT_TRUE(agent_.authenticationRequired);

  [agent_ authenticateIncognitoContent];

  // Auth not required
  EXPECT_FALSE(agent_.authenticationRequired);

  // Auth required after backgrounding.
  scene_state_.activationLevel = SceneActivationLevelBackground;
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  EXPECT_TRUE(agent_.authenticationRequired);
}

// Tests that authentication is still required if authentication fails.
TEST_F(IncognitoReauthSceneAgentTest, FailedSkippedAuth) {
  SetUpTestObjects(/*tab_count=*/1, /*enable_pref=*/true);

  // Go foreground.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  EXPECT_TRUE(agent_.authenticationRequired);

  stub_reauth_module_.returnedResult = ReauthenticationResult::kFailure;

  [agent_ authenticateIncognitoContent];
  // Auth still required
  EXPECT_TRUE(agent_.authenticationRequired);

  stub_reauth_module_.returnedResult = ReauthenticationResult::kSkipped;
  [agent_ authenticateIncognitoContent];
  // Auth still required
  EXPECT_TRUE(agent_.authenticationRequired);
}

// Test that when the feature is enabled, auth isn't required if we foreground
// without any incognito tabs.
TEST_F(IncognitoReauthSceneAgentTest, AuthNotRequiredWhenNoIncognitoTabs) {
  SetUpTestObjects(/*tab_count=*/0, /*enable_pref=*/true);

  // Go foreground.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  EXPECT_FALSE(agent_.authenticationRequired);
}

// Test that when the feature is enabled, we're foregrounded with some incognito
// content already present, auth is required
TEST_F(IncognitoReauthSceneAgentTest,
       AuthNotRequiredWhenNoIncognitoTabsOnForeground) {
  SetUpTestObjects(/*tab_count=*/0, /*enable_pref=*/true);

  // Go foreground.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  EXPECT_FALSE(agent_.authenticationRequired);

  // Open another tab.
  test_browser_->GetWebStateList()->InsertWebState(
      std::make_unique<web::FakeWebState>(),
      WebStateList::InsertionParams::AtIndex(0));

  EXPECT_FALSE(agent_.authenticationRequired);
}

#pragma mark - Soft Lock tests

// Test that when both reauth and soft lock are disabled, no overlay is
// displayed.
TEST_F(IncognitoReauthSceneAgentTest, AllFeaturesDisabled) {
  SetUpTestObjects(/*tab_count=*/1,
                   /*reauth_enabled=*/false,
                   /*soft_lock_enabled=*/false);

  // Go foreground.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  EXPECT_EQ(agent_.incognitoLockState, IncognitoLockState::kNone);
}

// Test that the correct overlay is displayed when soft lock is enabled.
TEST_F(IncognitoReauthSceneAgentTest, SoftLockEnabled) {
  SetUpTestObjects(/*tab_count=*/1,
                   /*reauth_enabled=*/false,
                   /*soft_lock_enabled=*/true);

  // Go foreground.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  EXPECT_EQ(agent_.incognitoLockState, IncognitoLockState::kSoftLock);
}

// Test that the correct overlay is displayed when both reauth and soft lock are
// enabled.
TEST_F(IncognitoReauthSceneAgentTest, AllFeaturesEnabled) {
  SetUpTestObjects(/*tab_count=*/1,
                   /*reauth_enabled=*/true,
                   /*soft_lock_enabled=*/true);

  // Go foreground.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  EXPECT_EQ(agent_.incognitoLockState, IncognitoLockState::kReauth);
}

// Test that when unlock is required and is successfully performed, it's
// not required anymore.
TEST_F(IncognitoReauthSceneAgentTest, SuccessfulSoftUnlock) {
  SetUpTestObjects(/*tab_count=*/1, /*reauth_enabled=*/false,
                   /*soft_lock_enabled=*/true);

  // Go foreground.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  EXPECT_TRUE(agent_.authenticationRequired);

  [agent_ authenticateIncognitoContent];

  // Auth not required
  EXPECT_FALSE(agent_.authenticationRequired);

  // Auth required after backgrounding.
  scene_state_.activationLevel = SceneActivationLevelBackground;
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  EXPECT_TRUE(agent_.authenticationRequired);
}

// Test that when soft lock is enabled, unlock isn't required if we foreground
// without any incognito tabs.
TEST_F(IncognitoReauthSceneAgentTest,
       SoftUnlockNotRequiredWhenNoIncognitoTabs) {
  SetUpTestObjects(/*tab_count=*/0, /*reauth_enabled=*/false,
                   /*soft_lock_enabled=*/true);

  // Go foreground.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  EXPECT_FALSE(agent_.authenticationRequired);
}

// Test that when soft lock is enabled, we're foregrounded with some incognito
// content already present, unlock is not required.
TEST_F(IncognitoReauthSceneAgentTest,
       SoftUnlockNotRequiredWhenNoIncognitoTabsOnForeground) {
  SetUpTestObjects(/*tab_count=*/0, /*reauth_enabled=*/false,
                   /*soft_lock_enabled*/ true);

  // Go foreground.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  EXPECT_FALSE(agent_.authenticationRequired);

  // Open another tab.
  test_browser_->GetWebStateList()->InsertWebState(
      std::make_unique<web::FakeWebState>(),
      WebStateList::InsertionParams::AtIndex(0));

  EXPECT_FALSE(agent_.authenticationRequired);
}

}  // namespace
