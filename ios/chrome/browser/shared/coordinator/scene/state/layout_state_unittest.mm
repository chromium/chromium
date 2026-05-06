// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/state/layout_state.h"

#import "base/run_loop.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/layout_transition_coordinating.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

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

  [layout_state_ setContainedLayoutActive:YES];

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
                withTransitionCoordinator:mock_coordinator];

  [mock_observer verify];
}

// Tests that containedLayoutSupported updates observers.
TEST_F(LayoutStateTest, ContainedLayoutSupported) {
  id mock_observer = OCMProtocolMock(@protocol(LayoutStateObserver));
  [layout_state_ addObserver:mock_observer];

  OCMExpect([mock_observer layoutState:layout_state_
      didChangeContainedLayoutSupported:YES]);

  layout_state_.containedLayoutSupported = YES;

  [mock_observer verify];
}

// Tests that windowedMode updates observers.
TEST_F(LayoutStateTest, WindowedMode) {
  id mock_observer = OCMProtocolMock(@protocol(LayoutStateObserver));
  [layout_state_ addObserver:mock_observer];

  OCMExpect([mock_observer layoutState:layout_state_
                 didChangeWindowedMode:YES]);

  layout_state_.windowedMode = YES;

  [mock_observer verify];
}

// Tests that appBarPosition updates observers.
TEST_F(LayoutStateTest, AppBarPosition) {
  id mock_observer = OCMProtocolMock(@protocol(LayoutStateObserver));
  [layout_state_ addObserver:mock_observer];

  OCMExpect([mock_observer layoutState:layout_state_
               didChangeAppBarPosition:AppBarPosition::kBottom]);

  layout_state_.appBarPosition = AppBarPosition::kBottom;

  [mock_observer verify];
}

}  // namespace
