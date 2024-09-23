// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/incognito_blocker_scene_agent.h"

#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

class IncognitoBlockerSceneAgentTest : public PlatformTest {
 public:
  IncognitoBlockerSceneAgentTest()
      : scene_state_([[SceneState alloc] initWithAppState:nil]),
        scene_state_mock_(OCMPartialMock(scene_state_)),
        agent_([[IncognitoBlockerSceneAgent alloc] init]) {
    scene_session_mock_ = OCMClassMock([UISceneSession class]);
    OCMStub([scene_session_mock_ persistentIdentifier])
        .andReturn([[NSUUID UUID] UUIDString]);
    scene_mock_ = OCMClassMock([UIWindowScene class]);
    OCMStub([scene_mock_ session]).andReturn(scene_session_mock_);
    scene_state_.scene = scene_mock_;
    OCMStub([scene_state_mock_ scene]).andReturn(scene_mock_);
    agent_.sceneState = scene_state_;
  }

  ~IncognitoBlockerSceneAgentTest() override {
    scene_state_.incognitoContentVisible = NO;
  }

 protected:
  // The scene state that the agent works with.
  SceneState* scene_state_;
  // Mock for scene_state_'s underlying UIWindowScene.
  id scene_mock_;
  // Mock for scene_mock_'s underlying UISceneSession.
  id scene_session_mock_;
  // Partial mock for stubbing scene_state_'s methods
  id scene_state_mock_;
  // The tested agent
  IncognitoBlockerSceneAgent* agent_;
};

TEST_F(IncognitoBlockerSceneAgentTest, ShowIncognitoBlocker) {
  // Pretend there's only one window on this scene.
  UIWindow* window = [[UIWindow alloc] init];

  id applicationWindowMock = nil;
  OCMStub([scene_mock_ windows]).andReturn(@[ window ]);

  // Prepare to go to background with some incognito content.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  scene_state_.incognitoContentVisible = YES;
  EXPECT_EQ(window.subviews.count, 0u);

  // Upon background with incognito content, the blocker should be added.
  scene_state_.activationLevel = SceneActivationLevelBackground;
  EXPECT_EQ(window.subviews.count, 1u);

  // Upon foreground, the blocker should be removed.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  EXPECT_EQ(window.subviews.count, 0u);

  // No blocker should be added when no incognito content is shown.
  scene_state_.incognitoContentVisible = NO;
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
  UIWindow* bottomWindow = [[UIWindow alloc] init];
  bottomWindow.windowLevel = UIWindowLevelNormal;

  UIWindow* topWindow = [[UIWindow alloc] init];
  topWindow.windowLevel = UIWindowLevelStatusBar + 1;

  NSArray* windows = @[ topWindow, bottomWindow ];

  id applicationWindowMock = nil;
  OCMStub([scene_mock_ windows]).andReturn(windows);

  // Prepare to go to background with some incognito content.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  scene_state_.incognitoContentVisible = YES;
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
