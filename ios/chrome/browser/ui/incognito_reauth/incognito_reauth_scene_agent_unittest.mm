// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_scene_agent.h"

#import "base/feature_list.h"
#import "base/test/scoped_feature_list.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/main/browser_interface_provider.h"
#import "ios/chrome/browser/ui/main/test/stub_browser_interface_provider.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - StubReauthenticationModule

@interface StubReauthenticationModule : NSObject <ReauthenticationProtocol>

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
      : browser_state_(TestChromeBrowserState::Builder().Build()),
        scene_state_([[SceneState alloc] initWithAppState:nil]),
        scene_state_mock_(OCMPartialMock(scene_state_)),
        stub_reauth_module_([[StubReauthenticationModule alloc] init]),
        agent_([[IncognitoReauthSceneAgent alloc]
            initWithReauthModule:stub_reauth_module_]) {
    [scene_state_ addAgent:agent_];
  }

 protected:
  void SetUpTestObjects(int tab_count, bool enable_pref) {
    // Stub all calls to be able to mock the following:
    // 1. sceneState.interfaceProvider.incognitoInterface
    //            .browser->GetWebStateList()->count()
    // 2. sceneState.interfaceProvider.hasIncognitoInterface
    test_browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    for (int i = 0; i < tab_count; ++i) {
      test_browser_->GetWebStateList()->InsertWebState(
          i, std::make_unique<web::FakeWebState>(),
          WebStateList::INSERT_FORCE_INDEX, WebStateOpener());
    }

    stub_browser_interface_provider_ =
        [[StubBrowserInterfaceProvider alloc] init];
    stub_browser_interface_provider_.incognitoInterface.browser =
        test_browser_.get();

    OCMStub([scene_state_mock_ interfaceProvider])
        .andReturn(stub_browser_interface_provider_);

    [IncognitoReauthSceneAgent registerLocalState:pref_service_.registry()];
    agent_.localState = &pref_service_;
    pref_service_.SetBoolean(prefs::kIncognitoAuthenticationSetting,
                             enable_pref);
  }

  void SetUp() override {
    // Set up default stub reauth module behavior.
    stub_reauth_module_.canAttemptReauth = YES;
    stub_reauth_module_.returnedResult = ReauthenticationResult::kSuccess;
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;

  // The scene state that the agent works with.
  SceneState* scene_state_;
  // Partial mock for stubbing scene_state_'s methods
  id scene_state_mock_;
  StubReauthenticationModule* stub_reauth_module_;
  // The tested agent
  IncognitoReauthSceneAgent* agent_;
  StubBrowserInterfaceProvider* stub_browser_interface_provider_;
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
      0, std::make_unique<web::FakeWebState>(),
      WebStateList::INSERT_FORCE_INDEX, WebStateOpener());

  EXPECT_FALSE(agent_.authenticationRequired);
}

}  // namespace
