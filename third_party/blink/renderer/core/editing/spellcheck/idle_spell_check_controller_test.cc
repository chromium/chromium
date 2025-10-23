// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/spellcheck/idle_spell_check_controller.h"

#include <gtest/gtest.h>

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/spellcheck/features.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_check_test_base.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"

namespace blink {

using State = IdleSpellCheckController::State;

class IdleSpellCheckControllerTest
    : public SpellCheckTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 protected:
  IdleSpellCheckController& IdleChecker() {
    return GetSpellChecker().GetIdleSpellCheckController();
  }

  void SetUp() override {
    if (IsRestrictionActiveForContents() ||
        IsRestrictionActiveForEnablement() ||
        IsRestrictionActiveForSelection()) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          features::kRestrictSpellingAndGrammarHighlights,
          {
              {
                  "changed_contents",
                  IsRestrictionActiveForContents() ? "true" : "false",
              },
              {
                  "changed_enablement",
                  IsRestrictionActiveForEnablement() ? "true" : "false",
              },
              {
                  "changed_selection",
                  IsRestrictionActiveForSelection() ? "true" : "false",
              },
          });
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kRestrictSpellingAndGrammarHighlights);
    }
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
        IdleChecker().RespondToChangedContents();
        break;
      case State::kColdModeTimerStarted:
        break;
      case State::kColdModeRequested:
        IdleChecker().SkipColdModeTimerForTesting();
        break;
      case State::kInHotModeInvocation:
      case State::kInColdModeInvocation:
        NOTREACHED();
    }
  }

  bool IsRestrictionActiveForContents() { return std::get<0>(GetParam()); }

  bool IsRestrictionActiveForEnablement() { return std::get<1>(GetParam()); }

  bool IsRestrictionActiveForSelection() { return std::get<2>(GetParam()); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    IdleSpellCheckControllerTest,
    ::testing::Combine(/*restrict_contents=*/::testing::Bool(),
                       /*restrict_enablement=*/::testing::Bool(),
                       /*restrict_selection=*/::testing::Bool()));

// Test cases for lifecycle state transitions.

TEST_P(IdleSpellCheckControllerTest, InitializationWithColdMode) {
  EXPECT_EQ(State::kColdModeTimerStarted, IdleChecker().GetState());
}

TEST_P(IdleSpellCheckControllerTest, RequestWhenInactive) {
  TransitTo(State::kInactive);
  IdleChecker().RespondToChangedContents();
  EXPECT_EQ(State::kHotModeRequested, IdleChecker().GetState());
  EXPECT_NE(-1, IdleChecker().IdleCallbackHandle());
}

TEST_P(IdleSpellCheckControllerTest, RequestWhenHotModeRequested) {
  TransitTo(State::kHotModeRequested);
  int handle = IdleChecker().IdleCallbackHandle();
  IdleChecker().RespondToChangedContents();
  EXPECT_EQ(State::kHotModeRequested, IdleChecker().GetState());
  EXPECT_EQ(handle, IdleChecker().IdleCallbackHandle());
  EXPECT_NE(-1, IdleChecker().IdleCallbackHandle());
}

TEST_P(IdleSpellCheckControllerTest, RequestWhenColdModeTimerStarted) {
  TransitTo(State::kColdModeTimerStarted);
  IdleChecker().RespondToChangedContents();
  EXPECT_EQ(State::kHotModeRequested, IdleChecker().GetState());
  EXPECT_NE(-1, IdleChecker().IdleCallbackHandle());
}

TEST_P(IdleSpellCheckControllerTest, RequestWhenColdModeRequested) {
  TransitTo(State::kColdModeRequested);
  int handle = IdleChecker().IdleCallbackHandle();
  IdleChecker().RespondToChangedContents();
  EXPECT_EQ(State::kHotModeRequested, IdleChecker().GetState());
  EXPECT_NE(handle, IdleChecker().IdleCallbackHandle());
  EXPECT_NE(-1, IdleChecker().IdleCallbackHandle());
}

TEST_P(IdleSpellCheckControllerTest, HotModeTransitToColdMode) {
  TransitTo(State::kHotModeRequested);
  IdleChecker().ForceInvocationForTesting();
  EXPECT_EQ(State::kColdModeTimerStarted, IdleChecker().GetState());
}

TEST_P(IdleSpellCheckControllerTest, ColdModeTimerStartedToRequested) {
  TransitTo(State::kColdModeTimerStarted);
  IdleChecker().SkipColdModeTimerForTesting();
  EXPECT_EQ(State::kColdModeRequested, IdleChecker().GetState());
  EXPECT_NE(-1, IdleChecker().IdleCallbackHandle());
}

TEST_P(IdleSpellCheckControllerTest, ColdModeStayAtColdMode) {
  TransitTo(State::kColdModeRequested);
  IdleChecker().SetNeedsMoreColdModeInvocationForTesting();
  IdleChecker().ForceInvocationForTesting();
  EXPECT_EQ(State::kColdModeTimerStarted, IdleChecker().GetState());
}

TEST_P(IdleSpellCheckControllerTest, ColdModeToInactive) {
  TransitTo(State::kColdModeRequested);
  IdleChecker().ForceInvocationForTesting();
  EXPECT_EQ(State::kInactive, IdleChecker().GetState());
}

TEST_P(IdleSpellCheckControllerTest, DetachWhenInactive) {
  TransitTo(State::kInactive);
  GetFrame().DomWindow()->FrameDestroyed();
  EXPECT_EQ(State::kInactive, IdleChecker().GetState());
}

TEST_P(IdleSpellCheckControllerTest, DetachWhenHotModeRequested) {
  TransitTo(State::kHotModeRequested);
  GetFrame().DomWindow()->FrameDestroyed();
  EXPECT_EQ(State::kInactive, IdleChecker().GetState());
}

TEST_P(IdleSpellCheckControllerTest, DetachWhenColdModeTimerStarted) {
  TransitTo(State::kColdModeTimerStarted);
  GetFrame().DomWindow()->FrameDestroyed();
  EXPECT_EQ(State::kInactive, IdleChecker().GetState());
}

TEST_P(IdleSpellCheckControllerTest, DetachWhenColdModeRequested) {
  TransitTo(State::kColdModeRequested);
  GetFrame().DomWindow()->FrameDestroyed();
  EXPECT_EQ(State::kInactive, IdleChecker().GetState());
}

// https://crbug.com/863784
TEST_P(IdleSpellCheckControllerTest, ColdModeRangeCrossesShadow) {
  SetBodyContent(
      "<div contenteditable style=\"width:800px\">"
      "foo"
      "<menu style=\"all: initial\">1127</menu>"
      "<object><optgroup></optgroup></object>"
      "</div>");
  auto* html_object_element = To<HTMLObjectElement>(QuerySelector("object"));
  html_object_element->RenderFallbackContent(
      HTMLObjectElement::ErrorEventPolicy::kDispatch);
  QuerySelector("div")->Focus();
  UpdateAllLifecyclePhasesForTest();

  // Advance to cold mode invocation
  IdleChecker().ForceInvocationForTesting();
  IdleChecker().SkipColdModeTimerForTesting();
  ASSERT_EQ(State::kColdModeRequested, IdleChecker().GetState());

  // Shouldn't crash
  IdleChecker().ForceInvocationForTesting();
  EXPECT_EQ(State::kInactive, IdleChecker().GetState());
}

TEST_P(IdleSpellCheckControllerTest,
       HotModeRangeDoesNotIncludeVisiblePosition) {
  SetBodyContent(
      "<form contenteditable='true'>"
      "<h2 contenteditable='true'>"
      "<select contenteditable='true'>"
      "<optgroup contenteditable='true'><option "
      "contenteditable='true'>hello</option></optgroup>"
      "<animateColor></animateColor>"
      "</select>"
      "</h2>"
      "<pre contenteditable='true'>"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "aaaaaaaaaaaaaaaaa</pre></form>");
  auto* option_element = QuerySelector("option");
  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .Collapse(Position(option_element, 1))
          .Build(),
      SetSelectionOptions());
  UpdateAllLifecyclePhasesForTest();
  TransitTo(State::kHotModeRequested);
  IdleChecker().RespondToChangedContents();
  // Should not crash
  IdleChecker().ForceInvocationForTesting();
  EXPECT_EQ(State::kInactive, IdleChecker().GetState());
}

}  // namespace blink
