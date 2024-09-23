// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/forms/layout_text_control.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

namespace {

class LayoutTextControlTest : public RenderingTest {
 public:
  LayoutTextControlTest() = default;

 protected:
  TextControlElement* GetTextControlElementById(const char* id) {
    return To<TextControlElement>(GetElementById(id));
  }
  // Return the LayoutText from inside a text control's user agent shadow tree.
  LayoutText* GetInnerLayoutText(TextControlElement* control) {
    return To<LayoutText>(
        control->InnerEditorElement()->GetLayoutObject()->SlowFirstChild());
  }

  // Focus on |control|, select 1-3 characters, get the first LayoutText, and
  // check if selection invalidation state is clean.
  LayoutText* SetupLayoutTextWithCleanSelection(TextControlElement* control) {
    control->Focus();
    control->SetSelectionRange(1, 3);
    UpdateAllLifecyclePhasesForTest();
    auto* selected_text = GetInnerLayoutText(control);
    EXPECT_FALSE(selected_text->ShouldInvalidateSelection());
    return selected_text;
  }

  void CheckSelectionInvalidationChanges(const LayoutText& selected_text) {
    GetDocument().View()->UpdateLifecycleToLayoutClean(
        DocumentUpdateReason::kTest);
    EXPECT_TRUE(selected_text.ShouldInvalidateSelection());

    UpdateAllLifecyclePhasesForTest();
    EXPECT_FALSE(selected_text.ShouldInvalidateSelection());
  }
};

TEST_F(LayoutTextControlTest,
       ChangingPseudoSelectionStyleShouldInvalidateSelectionSingle) {
  SetBodyInnerHTML(R"HTML(
    <style>
      input::selection { background-color: blue; }
      .pseudoSelection::selection { background-color: green; }
    </style>
    <input id="input" type="text" value="AAAAAAAAAAAA">
  )HTML");

  auto* text_control = GetTextControlElementById("input");
  auto* selected_text = SetupLayoutTextWithCleanSelection(text_control);

  text_control->setAttribute(html_names::kClassAttr,
                             AtomicString("pseudoSelection"));
  CheckSelectionInvalidationChanges(*selected_text);
}

TEST_F(LayoutTextControlTest,
       ChangingPseudoSelectionStyleShouldInvalidateSelectionMulti) {
  SetBodyInnerHTML(R"HTML(
    <style>
      textarea::selection { background-color: blue; }
      .pseudoSelection::selection { background-color: green; }
    </style>
    <textarea id="textarea">AAAAAAAAAAAA</textarea>
  )HTML");

  auto* text_control = GetTextControlElementById("textarea");
  auto* selected_text = SetupLayoutTextWithCleanSelection(text_control);

  text_control->setAttribute(html_names::kClassAttr,
                             AtomicString("pseudoSelection"));
  CheckSelectionInvalidationChanges(*selected_text);
}

TEST_F(LayoutTextControlTest,
       AddingPseudoSelectionStyleShouldInvalidateSelectionSingle) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .pseudoSelection::selection { background-color: green; }
    </style>
    <input id="input" type="text" value="AAAAAAAAAAAA">
  )HTML");

  auto* text_control = GetTextControlElementById("input");
  auto* selected_text = SetupLayoutTextWithCleanSelection(text_control);

  text_control->setAttribute(html_names::kClassAttr,
                             AtomicString("pseudoSelection"));
  CheckSelectionInvalidationChanges(*selected_text);
}

TEST_F(LayoutTextControlTest,
       AddingPseudoSelectionStyleShouldInvalidateSelectionMulti) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .pseudoSelection::selection { background-color: green; }
    </style>
    <textarea id="textarea" >AAAAAAAAAAAA</textarea>
  )HTML");

  auto* text_control = GetTextControlElementById("textarea");
  auto* selected_text = SetupLayoutTextWithCleanSelection(text_control);

  text_control->setAttribute(html_names::kClassAttr,
                             AtomicString("pseudoSelection"));
  CheckSelectionInvalidationChanges(*selected_text);
}

TEST_F(LayoutTextControlTest,
       RemovingPseudoSelectionStyleShouldInvalidateSelectionSingle) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .pseudoSelection::selection { background-color: green; }
    </style>
    <input id="input" type="text" class="pseudoSelection" value="AAAAAAAAAAAA">
  )HTML");

  auto* text_control = GetTextControlElementById("input");
  auto* selected_text = SetupLayoutTextWithCleanSelection(text_control);

  text_control->removeAttribute(html_names::kClassAttr);
  CheckSelectionInvalidationChanges(*selected_text);
}

TEST_F(LayoutTextControlTest,
       RemovingPseudoSelectionStyleShouldInvalidateSelectionMulti) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .pseudoSelection::selection { background-color: green; }
    </style>
    <textarea id="textarea" class="pseudoSelection">AAAAAAAAAAAA</textarea>
  )HTML");

  auto* text_control = GetTextControlElementById("textarea");
  auto* selected_text = SetupLayoutTextWithCleanSelection(text_control);

  text_control->removeAttribute(html_names::kClassAttr);
  CheckSelectionInvalidationChanges(*selected_text);
}

TEST_F(LayoutTextControlTest, HitTestSearchInput) {
  SetBodyInnerHTML(R"HTML(
    <input id="input" type="search"
           style="border-width: 20px; font-size: 30px; padding: 0">
  )HTML");

  auto* input = GetTextControlElementById("input");
  HitTestResult result;
  HitTestLocation location(PhysicalOffset(40, 30));
  EXPECT_TRUE(input->GetLayoutObject()->HitTestAllPhases(result, location,
                                                         PhysicalOffset()));
  EXPECT_EQ(PhysicalOffset(20, 10), result.LocalPoint());
  EXPECT_EQ(input->InnerEditorElement(), result.InnerElement());
}

}  // anonymous namespace

}  // namespace blink
