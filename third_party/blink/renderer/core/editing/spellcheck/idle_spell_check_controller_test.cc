// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/spellcheck/idle_spell_check_controller.h"

#include <gtest/gtest.h>

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_check_test_base.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"
#include "third_party/blink/renderer/core/keywords.h"

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
                  "RestrictSpellingAndGrammarHighlightsChangedContents",
                  IsRestrictionActiveForContents() ? "true" : "false",
              },
              {
                  "RestrictSpellingAndGrammarHighlightsChangedEnablement",
                  IsRestrictionActiveForEnablement() ? "true" : "false",
              },
              {
                  "RestrictSpellingAndGrammarHighlightsChangedSelection",
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
  if (IsRestrictionActiveForContents()) {
    EXPECT_EQ(State::kInactive, IdleChecker().GetState());
    EXPECT_EQ(-1, IdleChecker().IdleCallbackHandle());
  } else {
    EXPECT_EQ(State::kHotModeRequested, IdleChecker().GetState());
    EXPECT_NE(-1, IdleChecker().IdleCallbackHandle());
  }
}

TEST_P(IdleSpellCheckControllerTest, RequestWhenHotModeRequested) {
  TransitTo(State::kHotModeRequested);
  if (IsRestrictionActiveForContents()) {
    EXPECT_EQ(-1, IdleChecker().IdleCallbackHandle());
  } else {
    EXPECT_NE(-1, IdleChecker().IdleCallbackHandle());
  }
  IdleChecker().RespondToChangedContents();
  if (IsRestrictionActiveForContents()) {
    EXPECT_EQ(State::kInactive, IdleChecker().GetState());
    EXPECT_EQ(-1, IdleChecker().IdleCallbackHandle());
  } else {
    EXPECT_EQ(State::kHotModeRequested, IdleChecker().GetState());
    EXPECT_NE(-1, IdleChecker().IdleCallbackHandle());
  }
}

TEST_P(IdleSpellCheckControllerTest, RequestWhenColdModeTimerStarted) {
  TransitTo(State::kColdModeTimerStarted);
  IdleChecker().RespondToChangedContents();
  if (IsRestrictionActiveForContents()) {
    EXPECT_EQ(State::kInactive, IdleChecker().GetState());
    EXPECT_EQ(-1, IdleChecker().IdleCallbackHandle());
  } else {
    EXPECT_EQ(State::kHotModeRequested, IdleChecker().GetState());
    EXPECT_NE(-1, IdleChecker().IdleCallbackHandle());
  }
}

TEST_P(IdleSpellCheckControllerTest, RequestWhenColdModeRequested) {
  TransitTo(State::kColdModeRequested);
  EXPECT_NE(-1, IdleChecker().IdleCallbackHandle());
  IdleChecker().RespondToChangedContents();
  if (IsRestrictionActiveForContents()) {
    EXPECT_EQ(State::kInactive, IdleChecker().GetState());
    EXPECT_EQ(-1, IdleChecker().IdleCallbackHandle());
  } else {
    EXPECT_EQ(State::kHotModeRequested, IdleChecker().GetState());
    EXPECT_NE(-1, IdleChecker().IdleCallbackHandle());
  }
}

TEST_P(IdleSpellCheckControllerTest, HotModeTransitToColdMode) {
  TransitTo(State::kHotModeRequested);
  IdleChecker().ForceInvocationForTesting();
  if (IsRestrictionActiveForContents()) {
    EXPECT_EQ(State::kInactive, IdleChecker().GetState());
  } else {
    EXPECT_EQ(State::kColdModeTimerStarted, IdleChecker().GetState());
  }
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
  if (IsRestrictionActiveForSelection()) {
    ASSERT_EQ(State::kInactive, IdleChecker().GetState());
  } else {
    IdleChecker().SkipColdModeTimerForTesting();
    ASSERT_EQ(State::kColdModeRequested, IdleChecker().GetState());
  }

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

TEST_P(IdleSpellCheckControllerTest, SpellcheckAttribute) {
  SetBodyContent("<div contenteditable=\"true\" spellcheck=\"true\">foo</div>");

  // Focus element and track lifecycle
  QuerySelector("div")->Focus(FocusParams(SelectionBehaviorOnFocus::kRestore,
                                          mojom::blink::FocusType::kMouse,
                                          /*capabilities=*/nullptr));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(State::kHotModeRequested, IdleChecker().GetState());
  IdleChecker().ForceInvocationForTesting();
  IdleChecker().SkipColdModeTimerForTesting();
  ASSERT_EQ(State::kColdModeRequested, IdleChecker().GetState());
  IdleChecker().ForceInvocationForTesting();
  EXPECT_EQ(State::kInactive, IdleChecker().GetState());

  // Disable spellcheck attribute and track lifecycle
  QuerySelector("div")->setAttribute(html_names::kSpellcheckAttr,
                                     keywords::kFalse);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(State::kInactive, IdleChecker().GetState());

  // Re-enable spellcheck attribute and track lifecycle
  QuerySelector("div")->setAttribute(html_names::kSpellcheckAttr,
                                     keywords::kTrue);
  UpdateAllLifecyclePhasesForTest();
  if (IsRestrictionActiveForEnablement()) {
    EXPECT_EQ(State::kInactive, IdleChecker().GetState());
  } else {
    EXPECT_EQ(State::kHotModeRequested, IdleChecker().GetState());
    IdleChecker().ForceInvocationForTesting();
    IdleChecker().SkipColdModeTimerForTesting();
    ASSERT_EQ(State::kColdModeRequested, IdleChecker().GetState());
    IdleChecker().ForceInvocationForTesting();
    EXPECT_EQ(State::kInactive, IdleChecker().GetState());
  }
}

TEST_P(IdleSpellCheckControllerTest, UserActivation) {
  // Update contents when user focus is inactive
  EXPECT_FALSE(LocalFrame::HasTransientUserActivation(&GetFrame()));
  TransitTo(State::kHotModeRequested);
  UpdateAllLifecyclePhasesForTest();
  IdleChecker().ForceInvocationForTesting();
  if (IsRestrictionActiveForContents()) {
    EXPECT_EQ(State::kInactive, IdleChecker().GetState());
  } else {
    IdleChecker().SkipColdModeTimerForTesting();
    ASSERT_EQ(State::kColdModeRequested, IdleChecker().GetState());
    IdleChecker().ForceInvocationForTesting();
    EXPECT_EQ(State::kInactive, IdleChecker().GetState());
  }

  // Update contents when user focus is active
  LocalFrame::NotifyUserActivation(
      &GetFrame(), mojom::UserActivationNotificationType::kTest);
  EXPECT_TRUE(LocalFrame::HasTransientUserActivation(&GetFrame()));
  TransitTo(State::kHotModeRequested);
  UpdateAllLifecyclePhasesForTest();
  IdleChecker().ForceInvocationForTesting();
  IdleChecker().SkipColdModeTimerForTesting();
  ASSERT_EQ(State::kColdModeRequested, IdleChecker().GetState());
  IdleChecker().ForceInvocationForTesting();
  EXPECT_EQ(State::kInactive, IdleChecker().GetState());
}

TEST_P(IdleSpellCheckControllerTest, SelectionFocusType) {
  for (int focus_type = (int)mojom::blink::FocusType::kMinValue;
       focus_type <= (int)mojom::blink::FocusType::kMaxValue; focus_type++) {
    // Setup focusable content
    IdleChecker().Deactivate();
    SetBodyContent(
        "<div contenteditable=\"true\" spellcheck=\"true\">foo</div>");
    UpdateAllLifecyclePhasesForTest();
    EXPECT_EQ(State::kInactive, IdleChecker().GetState());

    // Attempt focus and check resulting state
    QuerySelector("div")->Focus(FocusParams(SelectionBehaviorOnFocus::kRestore,
                                            (mojom::blink::FocusType)focus_type,
                                            /*capabilities=*/nullptr));
    UpdateAllLifecyclePhasesForTest();
    IdleChecker().ForceInvocationForTesting();
    if (IsRestrictionActiveForSelection() &&
        ((mojom::blink::FocusType)focus_type ==
             mojom::blink::FocusType::kNone ||
         (mojom::blink::FocusType)focus_type ==
             mojom::blink::FocusType::kScript)) {
      ASSERT_EQ(State::kInactive, IdleChecker().GetState());
    } else {
      IdleChecker().SkipColdModeTimerForTesting();
      ASSERT_EQ(State::kColdModeRequested, IdleChecker().GetState());
      IdleChecker().ForceInvocationForTesting();
      EXPECT_EQ(State::kInactive, IdleChecker().GetState());
    }
  }
}

}  // namespace blink
