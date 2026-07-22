// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/ui_bundled/incognito_blocker_scene_agent.h"

#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/incognito_state.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

class IncognitoBlockerSceneAgentTest : public PlatformTest {
 public:
  IncognitoBlockerSceneAgentTest()
      : scene_state_([[SceneState alloc] init]),
        agent_([[IncognitoBlockerSceneAgent alloc] init]) {
    scene_mock_ = OCMClassMock([UIWindowScene class]);
    scene_state_.scene = scene_mock_;
    agent_.sceneState = scene_state_;
  }

  ~IncognitoBlockerSceneAgentTest() override {
    scene_state_.incognitoState.incognitoContentVisible = NO;
  }

 protected:
  // The scene state that the agent works with.
  SceneState* scene_state_;
  // Mock for scene_state_'s underlying UIWindowScene.
  id scene_mock_;
  // The tested agent
  IncognitoBlockerSceneAgent* agent_;
};

TEST_F(IncognitoBlockerSceneAgentTest, ShowIncognitoBlocker) {
  // Pretend there's only one window on this scene.
  UIWindow* window = [[UIWindow alloc]
      initWithWindowScene:chrome_test_util::GetAnyWindowScene()];

  id applicationWindowMock = nil;
  OCMStub([scene_mock_ windows]).andReturn(@[ window ]);

  // Prepare to go to background with some incognito content.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  scene_state_.incognitoState.incognitoContentVisible = YES;
  EXPECT_EQ(window.subviews.count, 0u);

  // Upon background with incognito content, the blocker should be added.
  scene_state_.activationLevel = SceneActivationLevelBackground;
  EXPECT_EQ(window.subviews.count, 1u);

  // Upon foreground, the blocker should be removed.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  EXPECT_EQ(window.subviews.count, 0u);

  // Upon background with incognito content, the blocker should be added.
  scene_state_.activationLevel = SceneActivationLevelBackground;
  EXPECT_EQ(window.subviews.count, 1u);

  // Upon destruction, the blocker should be removed.
  scene_state_.activationLevel = SceneActivationLevelDisconnected;
  EXPECT_EQ(window.subviews.count, 0u);

  // No blocker should be added when no incognito content is shown.
  scene_state_.incognitoState.incognitoContentVisible = NO;
  scene_state_.activationLevel = SceneActivationLevelBackground;
  EXPECT_EQ(window.subviews.count, 0u);

  // Prepare to go to background with the QR scanner visible.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  scene_state_.QRScannerVisible = YES;
  EXPECT_EQ(window.subviews.count, 0u);

  // Upon background with the QR scanner visible, the blocker should be added.
  scene_state_.activationLevel = SceneActivationLevelBackground;
  EXPECT_EQ(window.subviews.count, 1u);

  [applicationWindowMock stopMocking];
}

// Test that when there are multiple windows, for example when there's a
// fullscreen video playing in incognito in a scene, the overlay is added to it.
TEST_F(IncognitoBlockerSceneAgentTest, ShowBlockerOnTopWindow) {
  // Pretend there's two windows on this scene.
  UIWindow* bottomWindow = [[UIWindow alloc]
      initWithWindowScene:chrome_test_util::GetAnyWindowScene()];
  bottomWindow.windowLevel = UIWindowLevelNormal;

  UIWindow* topWindow = [[UIWindow alloc]
      initWithWindowScene:chrome_test_util::GetAnyWindowScene()];
  topWindow.windowLevel = UIWindowLevelStatusBar + 1;

  NSArray* windows = @[ topWindow, bottomWindow ];

  id applicationWindowMock = nil;
  OCMStub([scene_mock_ windows]).andReturn(windows);

  // Prepare to go to background with some incognito content.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  scene_state_.incognitoState.incognitoContentVisible = YES;
  EXPECT_EQ(topWindow.subviews.count, 0u);
  EXPECT_EQ(bottomWindow.subviews.count, 0u);

  // Upon background, the blocker should be added only to the topmost window.
  scene_state_.activationLevel = SceneActivationLevelBackground;
  EXPECT_EQ(topWindow.subviews.count, 1u);
  EXPECT_EQ(bottomWindow.subviews.count, 0u);

  // Upon foreground, the blocker should be removed.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  EXPECT_EQ(topWindow.subviews.count, 0u);
  EXPECT_EQ(bottomWindow.subviews.count, 0u);

  [applicationWindowMock stopMocking];
}

}  // anonymous namespace
