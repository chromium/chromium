// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/persist_tab_context/model/persist_tab_context_state_agent.h"

#import "base/test/mock_callback.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class PersistTabContextStateAgentTest : public PlatformTest {
 protected:
  PersistTabContextStateAgentTest()
      : scene_state_([[SceneState alloc] initWithAppState:nil]) {}

  PersistTabContextStateAgent* agent() { return agent_; }
  SceneState* scene_state() { return scene_state_; }

  void CreateAgent(
      base::RepeatingCallback<void(SceneActivationLevel)> callback) {
    agent_ = [[PersistTabContextStateAgent alloc]
        initWithTransitionCallback:std::move(callback)];
    [scene_state_ addObserver:agent_];
  }

 private:
  SceneState* scene_state_;
  PersistTabContextStateAgent* agent_;
};

// Tests that the callback is called when the scene activation level changes to
// background.
TEST_F(PersistTabContextStateAgentTest, TestCallbackIsCalledOnBackground) {
  base::MockCallback<base::RepeatingCallback<void(SceneActivationLevel)>>
      mock_callback;
  CreateAgent(mock_callback.Get());

  EXPECT_CALL(mock_callback, Run(SceneActivationLevelBackground));
  scene_state().activationLevel = SceneActivationLevelBackground;
}

// Tests that the callback is called when the scene activation level changes to
// foreground active.
TEST_F(PersistTabContextStateAgentTest, TestCallbackIsCalledOnForeground) {
  base::MockCallback<base::RepeatingCallback<void(SceneActivationLevel)>>
      mock_callback;
  CreateAgent(mock_callback.Get());

  EXPECT_CALL(mock_callback, Run(SceneActivationLevelForegroundActive));
  scene_state().activationLevel = SceneActivationLevelForegroundActive;
}

// Tests that the callback is not called when the scene activation level is set
// to the same value.
TEST_F(PersistTabContextStateAgentTest, TestCallbackIsNotCalledOnSameState) {
  base::MockCallback<base::RepeatingCallback<void(SceneActivationLevel)>>
      mock_callback;
  CreateAgent(mock_callback.Get());
  EXPECT_CALL(mock_callback, Run(SceneActivationLevelForegroundActive));
  scene_state().activationLevel = SceneActivationLevelForegroundActive;

  // From here on, we don't expect any more calls.
  testing::Mock::VerifyAndClearExpectations(&mock_callback);
  EXPECT_CALL(mock_callback, Run(testing::_)).Times(0);

  scene_state().activationLevel = SceneActivationLevelForegroundActive;
}

// Tests that the callback is called multiple times when the scene activation
// level changes multiple times.
TEST_F(PersistTabContextStateAgentTest, TestCallbackIsCalledMultipleTimes) {
  base::MockCallback<base::RepeatingCallback<void(SceneActivationLevel)>>
      mock_callback;
  CreateAgent(mock_callback.Get());

  EXPECT_CALL(mock_callback, Run(SceneActivationLevelBackground));
  scene_state().activationLevel = SceneActivationLevelBackground;

  EXPECT_CALL(mock_callback, Run(SceneActivationLevelForegroundActive));
  scene_state().activationLevel = SceneActivationLevelForegroundActive;
}
