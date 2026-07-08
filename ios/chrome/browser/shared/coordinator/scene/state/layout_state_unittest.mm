// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/state/layout_state.h"

#import "base/run_loop.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/layout_state_test_passkey_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/layout_transition_coordinating.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

using layout_state::LayoutStateTestPassKeyFactory;

// Helper functions to return domain-level passkeys used to mutate the layout
// state in tests.
inline LayoutStateScenePassKey ScenePassKey() {
  return LayoutStateTestPassKeyFactory::CreateSceneKey();
}
inline LayoutStateAssistantPassKey AssistantPassKey() {
  return LayoutStateTestPassKeyFactory::CreateAssistantKey();
}
inline LayoutStateToolbarPassKey ToolbarPassKey() {
  return LayoutStateTestPassKeyFactory::CreateToolbarKey();
}

// Tests for LayoutState.
class LayoutStateTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    layout_state_ = [[LayoutState alloc] init];
  }

  base::test::TaskEnvironment task_environment_;
  LayoutState* layout_state_;
};

// Tests that adding an observer works and it receives updates.
TEST_F(LayoutStateTest, AddObserver) {
  id mock_observer = OCMProtocolMock(@protocol(LayoutStateObserver));
  [layout_state_ addObserver:mock_observer];

  OCMExpect([mock_observer layoutState:layout_state_
             willChangeContainedLayout:YES
             withTransitionCoordinator:nil]);

  [layout_state_ setContainedLayoutActive:YES scenePassKey:ScenePassKey()];

  [mock_observer verify];

  OCMExpect([mock_observer layoutState:layout_state_
             willChangeContainedLayout:NO
             withTransitionCoordinator:nil]);

  [layout_state_ setContainedLayoutActive:NO
                         assistantPassKey:AssistantPassKey()];

  [mock_observer verify];
}

// Tests that willChangeContainedLayout is called with the provided coordinator.
TEST_F(LayoutStateTest, WillChangeWithCoordinator) {
  id mock_observer = OCMProtocolMock(@protocol(LayoutStateObserver));
  [layout_state_ addObserver:mock_observer];

  id mock_coordinator =
      OCMProtocolMock(@protocol(LayoutTransitionCoordinating));

  OCMExpect([mock_observer layoutState:layout_state_
             willChangeContainedLayout:YES
             withTransitionCoordinator:mock_coordinator]);

  [layout_state_ setContainedLayoutActive:YES
                withTransitionCoordinator:mock_coordinator
                             scenePassKey:ScenePassKey()];

  [mock_observer verify];

  OCMExpect([mock_observer layoutState:layout_state_
             willChangeContainedLayout:NO
             withTransitionCoordinator:mock_coordinator]);

  [layout_state_ setContainedLayoutActive:NO
                withTransitionCoordinator:mock_coordinator
                         assistantPassKey:AssistantPassKey()];

  [mock_observer verify];
}

// Tests that containedLayoutSupported updates observers.
TEST_F(LayoutStateTest, ContainedLayoutSupported) {
  id mock_observer = OCMProtocolMock(@protocol(LayoutStateObserver));
  [layout_state_ addObserver:mock_observer];

  OCMExpect([mock_observer layoutState:layout_state_
      didChangeContainedLayoutSupported:YES]);

  [layout_state_ setContainedLayoutSupported:YES passKey:ScenePassKey()];

  [mock_observer verify];
}

// Tests that windowedMode updates observers.
TEST_F(LayoutStateTest, WindowedMode) {
  id mock_observer = OCMProtocolMock(@protocol(LayoutStateObserver));
  [layout_state_ addObserver:mock_observer];

  OCMExpect([mock_observer layoutState:layout_state_
                 didChangeWindowedMode:YES]);

  [layout_state_ setWindowedMode:YES passKey:ScenePassKey()];

  [mock_observer verify];
}

// Tests that appBarPosition updates observers.
TEST_F(LayoutStateTest, AppBarPosition) {
  id mock_observer = OCMProtocolMock(@protocol(LayoutStateObserver));
  [layout_state_ addObserver:mock_observer];

  OCMExpect([mock_observer layoutState:layout_state_
               didChangeAppBarPosition:AppBarPosition::kBottom]);

  [layout_state_ setAppBarPosition:AppBarPosition::kBottom
                           passKey:ScenePassKey()];

  [mock_observer verify];
}

// Tests that toolbarPosition updates observers.
TEST_F(LayoutStateTest, ToolbarPosition) {
  id mock_observer = OCMProtocolMock(@protocol(LayoutStateObserver));
  [layout_state_ addObserver:mock_observer];

  OCMExpect([mock_observer layoutState:layout_state_
              didChangeToolbarPosition:ToolbarPosition::kBottom]);

  [layout_state_ setToolbarPosition:ToolbarPosition::kBottom
                            passKey:ToolbarPassKey()];

  EXPECT_EQ(layout_state_.toolbarPosition, ToolbarPosition::kBottom);

  [mock_observer verify];
}

// Tests that assistantContainerCutoutRadius updates observers.
TEST_F(LayoutStateTest, CutoutRadius) {
  id mock_observer = OCMProtocolMock(@protocol(LayoutStateObserver));
  [layout_state_ addObserver:mock_observer];

  OCMExpect([mock_observer layoutState:layout_state_
      didChangeAssistantContainerCutoutRadius:10.0]);

  [layout_state_ setAssistantContainerCutoutRadius:10.0
                                           passKey:AssistantPassKey()];

  EXPECT_EQ(layout_state_.assistantContainerCutoutRadius, 10.0);

  [mock_observer verify];
}

// Tests that appBarLockedInFullscreen updates observers.
TEST_F(LayoutStateTest, AppBarLockedInFullscreen) {
  id mock_observer = OCMProtocolMock(@protocol(LayoutStateObserver));
  [layout_state_ addObserver:mock_observer];

  OCMExpect([mock_observer layoutState:layout_state_
      didChangeAppBarLockedInFullscreen:YES]);

  [layout_state_ setAppBarLockedInFullscreen:YES passKey:AssistantPassKey()];

  EXPECT_TRUE(layout_state_.appBarLockedInFullscreen);

  [mock_observer verify];
}

// Tests that geminiFloatyInvoked updates observers.
TEST_F(LayoutStateTest, GeminiFloatyInvoked) {
  id mock_observer = OCMProtocolMock(@protocol(LayoutStateObserver));
  [layout_state_ addObserver:mock_observer];

  OCMExpect([mock_observer layoutState:layout_state_
          didChangeGeminiFloatyInvoked:YES]);

  [layout_state_ setGeminiFloatyInvoked:YES passKey:AssistantPassKey()];

  EXPECT_TRUE(layout_state_.geminiFloatyInvoked);

  [mock_observer verify];
}

}  // namespace
