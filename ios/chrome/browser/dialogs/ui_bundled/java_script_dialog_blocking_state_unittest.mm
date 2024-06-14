// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/dialogs/ui_bundled/java_script_dialog_blocking_state.h"

#import "ios/chrome/browser/dialogs/ui_bundled/java_script_blocking_fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Test fixture for testing JavaScriptDialogBlockingState.
class JavaScriptDialogBlockingStateTest : public PlatformTest {
 protected:
  JavaScriptDialogBlockingStateTest() : PlatformTest() {
    JavaScriptDialogBlockingState::CreateForWebState(&web_state_);
  }

  JavaScriptDialogBlockingState& state() {
    return *JavaScriptDialogBlockingState::FromWebState(&web_state_);
  }
  JavaScriptBlockingFakeWebState& web_state() { return web_state_; }

 private:
  JavaScriptBlockingFakeWebState web_state_;
};

// Tests that show_blocking_option() returns true after the first call to
// JavaScriptDialogWasShown() for a given presenter.
TEST_F(JavaScriptDialogBlockingStateTest, ShouldShow) {
  EXPECT_FALSE(state().show_blocking_option());
  state().JavaScriptDialogWasShown();
  EXPECT_TRUE(state().show_blocking_option());
}

// Tests that blocked() returns true after a call
// to JavaScriptDialogBlockingOptionSelected() for a given presenter.
TEST_F(JavaScriptDialogBlockingStateTest, BlockingOptionSelected) {
  EXPECT_FALSE(state().blocked());
  state().JavaScriptDialogBlockingOptionSelected();
  EXPECT_TRUE(state().blocked());
}

// Tests that blocked() returns false after user-initiated navigations.
TEST_F(JavaScriptDialogBlockingStateTest, StopBlockingForUserInitiated) {
  // Verify that the blocked bit is unset after a document-changing, user-
  // initiated navigation.
  state().JavaScriptDialogBlockingOptionSelected();
  EXPECT_TRUE(state().blocked());
  web_state().SimulateNavigationStarted(
      false /* renderer_initiated */, false /* same_document */,
      ui::PAGE_TRANSITION_TYPED, /* transition */
      true /* change_last_committed_item */);
  EXPECT_FALSE(state().blocked());

  // Verify that the blocked bit is unset after a same-changing, user-
  // initiated navigation.
  state().JavaScriptDialogBlockingOptionSelected();
  EXPECT_TRUE(state().blocked());
  web_state().SimulateNavigationStarted(
      false /* renderer_initiated */, true /* same_document */,
      ui::PAGE_TRANSITION_LINK, /* transition */
      true /* change_last_committed_item */);
  EXPECT_FALSE(state().blocked());
}

// Tests that blocked() returns false after document-changing navigations.
TEST_F(JavaScriptDialogBlockingStateTest, StopBlockingForDocumentChange) {
  // Verify that the blocked bit is unset after a document-changing, renderer-
  // initiated navigation.
  state().JavaScriptDialogBlockingOptionSelected();
  EXPECT_TRUE(state().blocked());
  web_state().SimulateNavigationStarted(
      true /* renderer_initiated */, false /* same_document */,
      ui::PAGE_TRANSITION_LINK, /* transition */
      true /* change_last_committed_item */);
  EXPECT_FALSE(state().blocked());
}

// Tests that blocked() continues to return true after a reload.
TEST_F(JavaScriptDialogBlockingStateTest, ContinueBlockingForReload) {
  state().JavaScriptDialogBlockingOptionSelected();
  EXPECT_TRUE(state().blocked());
  web_state().SimulateNavigationStarted(
      true /* renderer_initiated */, true /* same_document */,
      ui::PAGE_TRANSITION_RELOAD, /* transition */
      true /* change_last_committed_item */);
  EXPECT_TRUE(state().blocked());
}

// Tests that blocked() returns true after a renderer-initiated, same-document
// navigation.
TEST_F(JavaScriptDialogBlockingStateTest,
       ContinueBlockingForRendererInitiatedSameDocument) {
  state().JavaScriptDialogBlockingOptionSelected();
  EXPECT_TRUE(state().blocked());
  web_state().SimulateNavigationStarted(
      true /* renderer_initiated */, true /* same_document */,
      ui::PAGE_TRANSITION_LINK, /* transition */
      false /* change_last_committed_item */);
  EXPECT_TRUE(state().blocked());
}
