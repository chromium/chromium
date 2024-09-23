// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"

#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/markers/spell_check_marker.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_check_requester.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_check_test_base.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/input_type_names.h"

namespace blink {

class SpellCheckerTest : public SpellCheckTestBase {
 protected:
  unsigned LayoutCount() const {
    return Page().GetFrameView().LayoutCountForTesting();
  }
  DummyPageHolder& Page() const { return GetDummyPageHolder(); }

  void ForceLayout();
};

void SpellCheckerTest::ForceLayout() {
  LocalFrameView& frame_view = Page().GetFrameView();
  gfx::Rect frame_rect = frame_view.FrameRect();
  frame_rect.set_width(frame_rect.width() + 1);
  frame_rect.set_height(frame_rect.height() + 1);
  Page().GetFrameView().SetFrameRect(frame_rect);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
}

TEST_F(SpellCheckerTest, AdvanceToNextMisspellingWithEmptyInputNoCrash) {
  SetBodyContent("<input placeholder='placeholder'>abc");
  UpdateAllLifecyclePhasesForTest();
  Element* input = GetDocument().QuerySelector(AtomicString("input"));
  input->Focus();
  // Do not crash in advanceToNextMisspelling.
  GetSpellChecker().AdvanceToNextMisspelling(false);
}

// Regression test for crbug.com/701309
TEST_F(SpellCheckerTest, AdvanceToNextMisspellingWithImageInTableNoCrash) {
  SetBodyContent(
      "<div contenteditable>"
      "<table><tr><td>"
      "<img src=foo.jpg>"
      "</td></tr></table>"
      "zz zz zz"
      "</div>");
  GetDocument().QuerySelector(AtomicString("div"))->Focus();
  UpdateAllLifecyclePhasesForTest();

  // Do not crash in advanceToNextMisspelling.
  GetSpellChecker().AdvanceToNextMisspelling(false);
}

// Regression test for crbug.com/728801
TEST_F(SpellCheckerTest, AdvancedToNextMisspellingWrapSearchNoCrash) {
  SetBodyContent("<div contenteditable>  zz zz zz  </div>");

  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  div->Focus();
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .Collapse(Position::LastPositionInNode(*div))
                               .Build(),
                           SetSelectionOptions());
  UpdateAllLifecyclePhasesForTest();

  GetSpellChecker().AdvanceToNextMisspelling(false);
}

TEST_F(SpellCheckerTest, SpellCheckDoesNotCauseUpdateLayout) {
  SetBodyContent("<input>");
  auto* input =
      To<HTMLInputElement>(GetDocument().QuerySelector(AtomicString("input")));
  input->Focus();
  input->SetValue("Hello, input field");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  Position new_position(input->InnerEditorElement()->firstChild(), 3);
  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder().Collapse(new_position).Build(),
      SetSelectionOptions());
  ASSERT_EQ(3u, input->selectionStart());

  EXPECT_TRUE(GetSpellChecker().IsSpellCheckingEnabled());
  ForceLayout();
  unsigned start_count = LayoutCount();
  GetSpellChecker().RespondToChangedSelection();
  EXPECT_EQ(start_count, LayoutCount());
}

TEST_F(SpellCheckerTest, MarkAndReplaceForHandlesMultipleReplacements) {
  SetBodyContent(
      "<div contenteditable>"
      "spllchck"
      "</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  Node* text = div->firstChild();
  EphemeralRange range_to_check =
      EphemeralRange(Position(text, 0), Position(text, 8));

  SpellCheckRequest* request = SpellCheckRequest::Create(range_to_check, 0);

  TextCheckingResult result;
  result.decoration = TextDecorationType::kTextDecorationTypeSpelling;
  result.location = 0;
  result.length = 8;
  result.replacements = Vector<String>({"spellcheck", "spillchuck"});

  GetDocument().GetFrame()->GetSpellChecker().MarkAndReplaceFor(
      request, Vector<TextCheckingResult>({result}));

  ASSERT_EQ(1u, GetDocument().Markers().Markers().size());

  // The Spelling marker's description should be a newline-separated list of the
  // suggested replacements
  EXPECT_EQ("spellcheck\nspillchuck",
            To<SpellCheckMarker>(GetDocument().Markers().Markers()[0].Get())
                ->Description());
}

TEST_F(SpellCheckerTest, GetSpellCheckMarkerUnderSelection_FirstCharSelected) {
  SetBodyContent(
      "<div contenteditable>"
      "spllchck"
      "</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  Node* text = div->firstChild();

  GetDocument().Markers().AddSpellingMarker(
      EphemeralRange(Position(text, 0), Position(text, 8)));

  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 0), Position(text, 1))
          .Build(),
      SetSelectionOptions());

  DocumentMarkerGroup* result = GetDocument()
                                    .GetFrame()
                                    ->GetSpellChecker()
                                    .GetSpellCheckMarkerGroupUnderSelection();
  ASSERT_NE(nullptr, result);
  const DocumentMarker* marker = result->GetMarkerForText(To<Text>(text));
  ASSERT_NE(nullptr, marker);
  EXPECT_EQ(0u, marker->StartOffset());
  EXPECT_EQ(8u, marker->EndOffset());
}

TEST_F(SpellCheckerTest, GetSpellCheckMarkerUnderSelection_LastCharSelected) {
  SetBodyContent(
      "<div contenteditable>"
      "spllchck"
      "</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  Node* text = div->firstChild();

  GetDocument().Markers().AddSpellingMarker(
      EphemeralRange(Position(text, 0), Position(text, 8)));

  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 7), Position(text, 8))
          .Build(),
      SetSelectionOptions());

  DocumentMarkerGroup* result = GetDocument()
                                    .GetFrame()
                                    ->GetSpellChecker()
                                    .GetSpellCheckMarkerGroupUnderSelection();
  ASSERT_NE(nullptr, result);
  const DocumentMarker* marker = result->GetMarkerForText(To<Text>(text));
  ASSERT_NE(nullptr, marker);
  EXPECT_EQ(0u, marker->StartOffset());
  EXPECT_EQ(8u, marker->EndOffset());
}

TEST_F(SpellCheckerTest,
       GetSpellCheckMarkerUnderSelection_SingleCharWordSelected) {
  SetBodyContent(
      "<div contenteditable>"
      "s"
      "</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  Node* text = div->firstChild();

  GetDocument().Markers().AddSpellingMarker(
      EphemeralRange(Position(text, 0), Position(text, 1)));

  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 0), Position(text, 1))
          .Build(),
      SetSelectionOptions());

  DocumentMarkerGroup* result = GetDocument()
                                    .GetFrame()
                                    ->GetSpellChecker()
                                    .GetSpellCheckMarkerGroupUnderSelection();
  ASSERT_NE(nullptr, result);
  const DocumentMarker* marker = result->GetMarkerForText(To<Text>(text));
  ASSERT_NE(nullptr, marker);
  EXPECT_EQ(0u, marker->StartOffset());
  EXPECT_EQ(1u, marker->EndOffset());
}

TEST_F(SpellCheckerTest,
       GetSpellCheckMarkerUnderSelection_CaretLeftOfSingleCharWord) {
  SetBodyContent(
      "<div contenteditable>"
      "s"
      "</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  Node* text = div->firstChild();

  GetDocument().Markers().AddSpellingMarker(
      EphemeralRange(Position(text, 0), Position(text, 1)));

  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 0), Position(text, 0))
          .Build(),
      SetSelectionOptions());

  DocumentMarkerGroup* result = GetDocument()
                                    .GetFrame()
                                    ->GetSpellChecker()
                                    .GetSpellCheckMarkerGroupUnderSelection();
  ASSERT_NE(nullptr, result);
  const DocumentMarker* marker = result->GetMarkerForText(To<Text>(text));
  ASSERT_NE(nullptr, marker);
  EXPECT_EQ(0u, marker->StartOffset());
  EXPECT_EQ(1u, marker->EndOffset());
}

TEST_F(SpellCheckerTest,
       GetSpellCheckMarkerUnderSelection_CaretRightOfSingleCharWord) {
  SetBodyContent(
      "<div contenteditable>"
      "s"
      "</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  Node* text = div->firstChild();

  GetDocument().Markers().AddSpellingMarker(
      EphemeralRange(Position(text, 0), Position(text, 1)));

  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 1), Position(text, 1))
          .Build(),
      SetSelectionOptions());

  DocumentMarkerGroup* result = GetDocument()
                                    .GetFrame()
                                    ->GetSpellChecker()
                                    .GetSpellCheckMarkerGroupUnderSelection();
  ASSERT_NE(nullptr, result);
  const DocumentMarker* marker = result->GetMarkerForText(To<Text>(text));
  ASSERT_NE(nullptr, marker);
  EXPECT_EQ(0u, marker->StartOffset());
  EXPECT_EQ(1u, marker->EndOffset());
}

TEST_F(SpellCheckerTest,
       GetSpellCheckMarkerUnderSelection_CaretLeftOfMultiCharWord) {
  SetBodyContent(
      "<div contenteditable>"
      "spllchck"
      "</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  Node* text = div->firstChild();

  GetDocument().Markers().AddSpellingMarker(
      EphemeralRange(Position(text, 0), Position(text, 8)));

  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 0), Position(text, 0))
          .Build(),
      SetSelectionOptions());

  DocumentMarkerGroup* result = GetDocument()
                                    .GetFrame()
                                    ->GetSpellChecker()
                                    .GetSpellCheckMarkerGroupUnderSelection();
  ASSERT_NE(nullptr, result);
  const DocumentMarker* marker = result->GetMarkerForText(To<Text>(text));
  ASSERT_NE(nullptr, marker);
  EXPECT_EQ(0u, marker->StartOffset());
  EXPECT_EQ(8u, marker->EndOffset());
}

TEST_F(SpellCheckerTest,
       GetSpellCheckMarkerUnderSelection_CaretRightOfMultiCharWord) {
  SetBodyContent(
      "<div contenteditable>"
      "spllchck"
      "</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  Node* text = div->firstChild();

  GetDocument().Markers().AddSpellingMarker(
      EphemeralRange(Position(text, 0), Position(text, 8)));

  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 8), Position(text, 8))
          .Build(),
      SetSelectionOptions());

  DocumentMarkerGroup* result = GetDocument()
                                    .GetFrame()
                                    ->GetSpellChecker()
                                    .GetSpellCheckMarkerGroupUnderSelection();
  ASSERT_NE(nullptr, result);
  const DocumentMarker* marker = result->GetMarkerForText(To<Text>(text));
  ASSERT_NE(nullptr, marker);
  EXPECT_EQ(0u, marker->StartOffset());
  EXPECT_EQ(8u, marker->EndOffset());
}

TEST_F(SpellCheckerTest, GetSpellCheckMarkerUnderSelection_CaretMiddleOfWord) {
  SetBodyContent(
      "<div contenteditable>"
      "spllchck"
      "</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  Node* text = div->firstChild();

  GetDocument().Markers().AddSpellingMarker(
      EphemeralRange(Position(text, 0), Position(text, 8)));

  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 4), Position(text, 4))
          .Build(),
      SetSelectionOptions());

  DocumentMarkerGroup* result = GetDocument()
                                    .GetFrame()
                                    ->GetSpellChecker()
                                    .GetSpellCheckMarkerGroupUnderSelection();
  ASSERT_NE(nullptr, result);
  const DocumentMarker* marker = result->GetMarkerForText(To<Text>(text));
  ASSERT_NE(nullptr, marker);
  EXPECT_EQ(0u, marker->StartOffset());
  EXPECT_EQ(8u, marker->EndOffset());
}

TEST_F(SpellCheckerTest,
       GetSpellCheckMarkerUnderSelection_CaretOneCharLeftOfMisspelling) {
  SetBodyContent(
      "<div contenteditable>"
      "a spllchck"
      "</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  Node* text = div->firstChild();

  GetDocument().Markers().AddSpellingMarker(
      EphemeralRange(Position(text, 2), Position(text, 10)));

  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 1), Position(text, 1))
          .Build(),
      SetSelectionOptions());

  DocumentMarkerGroup* result = GetDocument()
                                    .GetFrame()
                                    ->GetSpellChecker()
                                    .GetSpellCheckMarkerGroupUnderSelection();
  EXPECT_EQ(nullptr, result);
}

TEST_F(SpellCheckerTest,
       GetSpellCheckMarkerUnderSelection_CaretOneCharRightOfMisspelling) {
  SetBodyContent(
      "<div contenteditable>"
      "spllchck a"
      "</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  Node* text = div->firstChild();

  GetDocument().Markers().AddSpellingMarker(
      EphemeralRange(Position(text, 0), Position(text, 8)));

  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 9), Position(text, 9))
          .Build(),
      SetSelectionOptions());

  DocumentMarkerGroup* result = GetDocument()
                                    .GetFrame()
                                    ->GetSpellChecker()
                                    .GetSpellCheckMarkerGroupUnderSelection();
  EXPECT_EQ(nullptr, result);
}

TEST_F(SpellCheckerTest, GetSpellCheckMarkerUnderSelection_MultiNodeMisspell) {
  SetBodyContent(
      "<div contenteditable>"
      "spl<b>lc</b>hck"
      "</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  Node* first_text = div->firstChild();
  Node* second_text = first_text->nextSibling()->firstChild();
  Node* third_text = div->lastChild();

  GetDocument().Markers().AddSpellingMarker(
      EphemeralRange(Position(first_text, 0), Position(third_text, 3)));

  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(second_text, 1), Position(second_text, 1))
          .Build(),
      SetSelectionOptions());

  DocumentMarkerGroup* result = GetDocument()
                                    .GetFrame()
                                    ->GetSpellChecker()
                                    .GetSpellCheckMarkerGroupUnderSelection();
  ASSERT_NE(nullptr, result);
  const DocumentMarker* first_marker =
      result->GetMarkerForText(To<Text>(first_text));
  const DocumentMarker* second_marker =
      result->GetMarkerForText(To<Text>(second_text));
  const DocumentMarker* third_marker =
      result->GetMarkerForText(To<Text>(third_text));
  ASSERT_NE(nullptr, first_marker);
  EXPECT_EQ(0u, first_marker->StartOffset());
  EXPECT_EQ(3u, first_marker->EndOffset());
  ASSERT_NE(nullptr, second_marker);
  EXPECT_EQ(0u, second_marker->StartOffset());
  EXPECT_EQ(2u, second_marker->EndOffset());
  ASSERT_NE(nullptr, third_marker);
  EXPECT_EQ(0u, third_marker->StartOffset());
  EXPECT_EQ(3u, third_marker->EndOffset());
}

TEST_F(SpellCheckerTest, PasswordFieldsAreIgnored) {
  // Check that spellchecking is enabled for an input type="text".
  SetBodyContent("<input type=\"text\">");
  auto* input =
      To<HTMLInputElement>(GetDocument().QuerySelector(AtomicString("input")));
  input->Focus();
  input->SetValue("spllchck");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_TRUE(SpellChecker::IsSpellCheckingEnabledAt(
      Position(input->InnerEditorElement()->firstChild(), 0)));

  // But if this turns into a password field, this disables spellchecking.
  // input->setType(input_type_names::kPassword);
  input->setType(input_type_names::kPassword);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_FALSE(SpellChecker::IsSpellCheckingEnabledAt(
      Position(input->InnerEditorElement()->firstChild(), 0)));

  // Some websites toggle between <input type="password"> and
  // <input type="text"> via a reveal/hide button. In this case, spell
  // checking should remain disabled.
  input->setType(input_type_names::kText);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_FALSE(SpellChecker::IsSpellCheckingEnabledAt(
      Position(input->InnerEditorElement()->firstChild(), 0)));
}

}  // namespace blink
