// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/spellcheck/idle_spell_check_controller.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_check_test_base.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

using State = IdleSpellCheckController::State;

class IdleSpellCheckControllerTest : public SpellCheckTestBase {
 protected:
  IdleSpellCheckController& IdleChecker() {
    return GetSpellChecker().GetIdleSpellCheckController();
  }

  void SetUp() override {
    SpellCheckTestBase::SetUp();

    // The initial cold mode request is on on document startup. This doesn't
    // work in unit test where SpellChecker is enabled after document startup.
    // Post another request here to ensure the activation of cold mode checker.
    IdleChecker().SetNeedsColdModeInvocation();
  }

  void TransitTo(State state) {
    switch (state) {
      case State::kInactive:
        IdleChecker().Deactivate();
        break;
      case State::kHotModeRequested:
        IdleChecker().SetNeedsInvocation();
        break;
      case State::kColdModeTimerStarted:
        DCHECK(RuntimeEnabledFeatures::IdleTimeColdModeSpellCheckingEnabled());
        break;
      case State::kColdModeRequested:
        DCHECK(RuntimeEnabledFeatures::IdleTimeColdModeSpellCheckingEnabled());
        IdleChecker().SkipColdModeTimerForTesting();
        break;
      case State::kInHotModeInvocation:
      case State::kInColdModeInvocation:
        NOTREACHED();
    }
  }
};

// Test cases for lifecycle state transitions.

TEST_F(IdleSpellCheckControllerTest, InitializationWithColdMode) {
  if (!RuntimeEnabledFeatures::IdleTimeColdModeSpellCheckingEnabled())
    return;

  EXPECT_EQ(State::kColdModeTimerStarted, IdleChecker().GetState());
}

TEST_F(IdleSpellCheckControllerTest, InitializationWithoutColdMode) {
  if (RuntimeEnabledFeatures::IdleTimeColdModeSpellCheckingEnabled())
    return;

  EXPECT_EQ(State::kInactive, IdleChecker().GetState());
}

TEST_F(IdleSpellCheckControllerTest, RequestWhenInactive) {
  TransitTo(State::kInactive);
  IdleChecker().SetNeedsInvocation();
  EXPECT_EQ(State::kHotModeRequested, IdleChecker().GetState());
  EXPECT_NE(-1, IdleChecker().IdleCallbackHandle());
}

TEST_F(IdleSpellCheckControllerTest, RequestWhenHotModeRequested) {
  TransitTo(State::kHotModeRequested);
  int handle = IdleChecker().IdleCallbackHandle();
  IdleChecker().SetNeedsInvocation();
  EXPECT_EQ(State::kHotModeRequested, IdleChecker().GetState());
  EXPECT_EQ(handle, IdleChecker().IdleCallbackHandle());
  EXPECT_NE(-1, IdleChecker().IdleCallbackHandle());
}

TEST_F(IdleSpellCheckControllerTest, RequestWhenColdModeTimerStarted) {
  if (!RuntimeEnabledFeatures::IdleTimeColdModeSpellCheckingEnabled())
    return;

  TransitTo(State::kColdModeTimerStarted);
  IdleChecker().SetNeedsInvocation();
  EXPECT_EQ(State::kHotModeRequested, IdleChecker().GetState());
  EXPECT_NE(-1, IdleChecker().IdleCallbackHandle());
}

TEST_F(IdleSpellCheckControllerTest, RequestWhenColdModeRequested) {
  if (!RuntimeEnabledFeatures::IdleTimeColdModeSpellCheckingEnabled())
    return;

  TransitTo(State::kColdModeRequested);
  int handle = IdleChecker().IdleCallbackHandle();
  IdleChecker().SetNeedsInvocation();
  EXPECT_EQ(State::kHotModeRequested, IdleChecker().GetState());
  EXPECT_NE(handle, IdleChecker().IdleCallbackHandle());
  EXPECT_NE(-1, IdleChecker().IdleCallbackHandle());
}

TEST_F(IdleSpellCheckControllerTest, HotModeTransitToInactive) {
  if (RuntimeEnabledFeatures::IdleTimeColdModeSpellCheckingEnabled())
    return;

  TransitTo(State::kHotModeRequested);
  IdleChecker().ForceInvocationForTesting();
  EXPECT_EQ(State::kInactive, IdleChecker().GetState());
}

TEST_F(IdleSpellCheckControllerTest, HotModeTransitToColdMode) {
  if (!RuntimeEnabledFeatures::IdleTimeColdModeSpellCheckingEnabled())
    return;

  TransitTo(State::kHotModeRequested);
  IdleChecker().ForceInvocationForTesting();
  EXPECT_EQ(State::kColdModeTimerStarted, IdleChecker().GetState());
}

TEST_F(IdleSpellCheckControllerTest, ColdModeTimerStartedToRequested) {
  if (!RuntimeEnabledFeatures::IdleTimeColdModeSpellCheckingEnabled())
    return;

  TransitTo(State::kColdModeTimerStarted);
  IdleChecker().SkipColdModeTimerForTesting();
  EXPECT_EQ(State::kColdModeRequested, IdleChecker().GetState());
  EXPECT_NE(-1, IdleChecker().IdleCallbackHandle());
}

TEST_F(IdleSpellCheckControllerTest, ColdModeStayAtColdMode) {
  if (!RuntimeEnabledFeatures::IdleTimeColdModeSpellCheckingEnabled())
    return;

  TransitTo(State::kColdModeRequested);
  IdleChecker().SetNeedsMoreColdModeInvocationForTesting();
  IdleChecker().ForceInvocationForTesting();
  EXPECT_EQ(State::kColdModeTimerStarted, IdleChecker().GetState());
}

TEST_F(IdleSpellCheckControllerTest, ColdModeToInactive) {
  if (!RuntimeEnabledFeatures::IdleTimeColdModeSpellCheckingEnabled())
    return;

  TransitTo(State::kColdModeRequested);
  IdleChecker().ForceInvocationForTesting();
  EXPECT_EQ(State::kInactive, IdleChecker().GetState());
}

TEST_F(IdleSpellCheckControllerTest, DetachWhenInactive) {
  TransitTo(State::kInactive);
  GetDocument().Shutdown();
  EXPECT_EQ(State::kInactive, IdleChecker().GetState());
}

TEST_F(IdleSpellCheckControllerTest, DetachWhenHotModeRequested) {
  TransitTo(State::kHotModeRequested);
  GetDocument().Shutdown();
  EXPECT_EQ(State::kInactive, IdleChecker().GetState());
}

TEST_F(IdleSpellCheckControllerTest, DetachWhenColdModeTimerStarted) {
  if (!RuntimeEnabledFeatures::IdleTimeColdModeSpellCheckingEnabled())
    return;

  TransitTo(State::kColdModeTimerStarted);
  GetDocument().Shutdown();
  EXPECT_EQ(State::kInactive, IdleChecker().GetState());
}

TEST_F(IdleSpellCheckControllerTest, DetachWhenColdModeRequested) {
  if (!RuntimeEnabledFeatures::IdleTimeColdModeSpellCheckingEnabled())
    return;

  TransitTo(State::kColdModeRequested);
  GetDocument().Shutdown();
  EXPECT_EQ(State::kInactive, IdleChecker().GetState());
}

// https://crbug.com/863784
TEST_F(IdleSpellCheckControllerTest, ColdModeRangeCrossesShadow) {
  ScopedIdleTimeColdModeSpellCheckingForTest cold_mode_scope(true);
  SetBodyContent(
      "<div contenteditable style=\"width:800px\">"
      "foo"
      "<menu style=\"all: initial\">1127</menu>"
      "<object><optgroup></optgroup></object>"
      "</div>");
  auto* html_object_element =
      ToHTMLObjectElement(GetDocument().QuerySelector("object"));
  html_object_element->RenderFallbackContent(
      html_object_element->ContentFrame());
  GetDocument().QuerySelector("div")->focus();
  UpdateAllLifecyclePhases();

  // Advance to cold mode invocation
  IdleChecker().ForceInvocationForTesting();
  IdleChecker().SkipColdModeTimerForTesting();
  ASSERT_EQ(State::kColdModeRequested, IdleChecker().GetState());

  // Shouldn't crash
  IdleChecker().ForceInvocationForTesting();
  EXPECT_EQ(State::kInactive, IdleChecker().GetState());
}

}  // namespace blink
