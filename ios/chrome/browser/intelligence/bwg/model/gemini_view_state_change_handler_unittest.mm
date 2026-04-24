// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_view_state_change_handler.h"

#import <optional>

#import "ios/public/provider/chrome/browser/bwg/bwg_api.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// A test spy for tracking delegate callbacks from GeminiViewStateChangeHandler.
class FakeGeminiViewStateChangeHandlerTarget
    : public GeminiViewStateChangeHandlerTarget {
 public:
  void OnGeminiViewStateExpanded() override { on_expanded_called_ = true; }
  void SetLastShownViewState(
      ios::provider::GeminiViewState view_state) override {
    last_shown_view_state_ = view_state;
  }
  void CollapseFloatyIfInvoked() override { collapse_floaty_called_ = true; }

  bool on_expanded_called_ = false;
  std::optional<ios::provider::GeminiViewState> last_shown_view_state_;
  bool collapse_floaty_called_ = false;
};

class GeminiViewStateChangeHandlerTest : public PlatformTest {
 protected:
  GeminiViewStateChangeHandlerTest() {
    handler_ = [[GeminiViewStateChangeHandler alloc] initWithTarget:&target_];
  }

  FakeGeminiViewStateChangeHandlerTarget target_;
  GeminiViewStateChangeHandler* handler_;
};

// Tests that the handler correctly notifies the target when the view state
// switches to expanded.
TEST_F(GeminiViewStateChangeHandlerTest, TestDidSwitchToViewStateExpanded) {
  [handler_ didSwitchToViewState:ios::provider::GeminiViewState::kExpanded];
  EXPECT_TRUE(target_.on_expanded_called_);
  EXPECT_THAT(target_.last_shown_view_state_,
              testing::Optional(ios::provider::GeminiViewState::kExpanded));
}

// Tests that the handler correctly updates the last shown view state when
// switching to collapsed.
TEST_F(GeminiViewStateChangeHandlerTest, TestDidSwitchToViewStateCollapsed) {
  [handler_ didSwitchToViewState:ios::provider::GeminiViewState::kCollapsed];
  EXPECT_FALSE(target_.on_expanded_called_);
  EXPECT_THAT(target_.last_shown_view_state_,
              testing::Optional(ios::provider::GeminiViewState::kCollapsed));
}

// Tests that the handler requests collapsing the floaty when requested to
// switch to collapsed state.
TEST_F(GeminiViewStateChangeHandlerTest, TestSwitchToViewStateCollapsed) {
  [handler_ switchToViewState:ios::provider::GeminiViewState::kCollapsed];
  EXPECT_TRUE(target_.collapse_floaty_called_);
}

// Tests that the handler does not request collapsing the floaty when requested
// to switch to expanded state.
TEST_F(GeminiViewStateChangeHandlerTest, TestSwitchToViewStateExpanded) {
  [handler_ switchToViewState:ios::provider::GeminiViewState::kExpanded];
  EXPECT_FALSE(target_.collapse_floaty_called_);
}

// Tests that the handler handles a null target gracefully without crashing.
TEST_F(GeminiViewStateChangeHandlerTest, TestNullTarget) {
  GeminiViewStateChangeHandler* null_target_handler =
      [[GeminiViewStateChangeHandler alloc] initWithTarget:nullptr];

  // Verify that calling delegate methods does not crash when target is null.
  [null_target_handler
      didSwitchToViewState:ios::provider::GeminiViewState::kExpanded];
  [null_target_handler
      switchToViewState:ios::provider::GeminiViewState::kCollapsed];

  SUCCEED();
}

// Tests that the handler stops forwarding events after disconnect.
TEST_F(GeminiViewStateChangeHandlerTest, TestDisconnect) {
  [handler_ disconnect];

  [handler_ didSwitchToViewState:ios::provider::GeminiViewState::kExpanded];
  EXPECT_FALSE(target_.on_expanded_called_);
  EXPECT_FALSE(target_.last_shown_view_state_.has_value());

  [handler_ switchToViewState:ios::provider::GeminiViewState::kCollapsed];
  EXPECT_FALSE(target_.collapse_floaty_called_);
}

}  // namespace
