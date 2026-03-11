// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/web/web_text_check_client.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/markers/grammar_marker.h"
#include "third_party/blink/renderer/core/editing/markers/spell_check_marker.h"
#include "third_party/blink/renderer/core/editing/markers/suggestion_marker_properties.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_check_requester.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_check_requester_helper.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_check_test_base.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/input_type_names.h"

namespace blink {

class SpellCheckerTest : public SpellCheckTestBase,
                         public testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    SpellCheckTestBase::SetUp();
    if (GetParam()) {
#if BUILDFLAG(IS_ANDROID)
      feature_list_
          .InitWithFeatures(/*enabled_features=*/
                            {blink::features::kAndroidSpellcheckFullApiBlink},
                            /*disabled_features=*/{});
#endif
    }
  }

  unsigned LayoutCount() const {
    return Page().GetFrameView().LayoutCountForTesting();
  }
  DummyPageHolder& Page() const { return GetDummyPageHolder(); }

  void ForceLayout();

 private:
  base::test::ScopedFeatureList feature_list_;
};

// testing::Bool() enables / disables the kAndroidSpellcheckFullApiBlink flag
// accordingly.
INSTANTIATE_TEST_SUITE_P(All, SpellCheckerTest, testing::Bool());

void SpellCheckerTest::ForceLayout() {
  LocalFrameView& frame_view = Page().GetFrameView();
  gfx::Rect frame_rect = frame_view.FrameRect();
  frame_rect.set_width(frame_rect.width() + 1);
  frame_rect.set_height(frame_rect.height() + 1);
  Page().GetFrameView().SetFrameRect(frame_rect);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
}

TEST_P(SpellCheckerTest, AdvanceToNextMisspellingWithEmptyInputNoCrash) {
  SetBodyContent("<input placeholder='placeholder'>abc");
  UpdateAllLifecyclePhasesForTest();
  QuerySelector("input")->Focus();
  // Do not crash in advanceToNextMisspelling.
  GetSpellChecker().AdvanceToNextMisspelling(false);
}

// Regression test for crbug.com/701309
TEST_P(SpellCheckerTest, AdvanceToNextMisspellingWithImageInTableNoCrash) {
  SetBodyContent(
      "<div contenteditable>"
      "<table><tr><td>"
      "<img src=foo.jpg>"
      "</td></tr></table>"
      "zz zz zz"
      "</div>");
  QuerySelector("div")->Focus();
  UpdateAllLifecyclePhasesForTest();

  // Do not crash in advanceToNextMisspelling.
  GetSpellChecker().AdvanceToNextMisspelling(false);
}

// Regression test for crbug.com/728801
TEST_P(SpellCheckerTest, AdvancedToNextMisspellingWrapSearchNoCrash) {
  SetBodyContent("<div contenteditable>  zz zz zz  </div>");

  Element* div = QuerySelector("div");
  div->Focus();
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .Collapse(Position::LastPositionInNode(*div))
                               .Build(),
                           SetSelectionOptions());
  UpdateAllLifecyclePhasesForTest();

  GetSpellChecker().AdvanceToNextMisspelling(false);
}

TEST_P(SpellCheckerTest, SpellCheckDoesNotCauseUpdateLayout) {
  SetBodyContent("<input>");
  auto* input = To<HTMLInputElement>(QuerySelector("input"));
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

TEST_P(SpellCheckerTest,
       MarkerContainsHideSuggestionWindowAttributeFalseByDefault) {
  SetBodyContent(
      "<div contenteditable>"
      "spllchck"
      "</div>");
  Element* div = QuerySelector("div");
  Node* text = div->firstChild();
  EphemeralRange range_to_check =
      EphemeralRange(Position(text, 0), Position(text, 8));

  SpellCheckRequest* request = SpellCheckRequest::Create(
      range_to_check,
      /*request_number=*/0, /*should_force_refresh=*/false);

  TextCheckingResult result;
  result.decoration = TextDecorationType::kTextDecorationTypeSpelling;
  result.location = 0;
  result.length = 8;
  result.replacements = Vector<String>({"spellcheck", "spillchuck"});

  GetDocument().GetFrame()->GetSpellChecker().MarkAndReplaceFor(
      request, Vector<TextCheckingResult>({result}));

  ASSERT_EQ(1u, GetDocument().Markers().Markers().size());
  EXPECT_FALSE(To<SpellCheckMarker>(GetDocument().Markers().Markers()[0].Get())
                   ->ShouldHideSuggestionMenu());
}

TEST_P(SpellCheckerTest, MarkerContainsHideSuggestionWindowAttribute) {
  SetBodyContent(
      "<div contenteditable>"
      "spllchck"
      "</div>");
  Element* div = QuerySelector("div");
  Node* text = div->firstChild();
  EphemeralRange range_to_check =
      EphemeralRange(Position(text, 0), Position(text, 8));

  SpellCheckRequest* request = SpellCheckRequest::Create(
      range_to_check,
      /*request_number=*/0, /*should_force_refresh=*/false);

  TextCheckingResult result;
  result.decoration = TextDecorationType::kTextDecorationTypeSpelling;
  result.location = 0;
  result.length = 8;
  result.replacements = Vector<String>({"spellcheck", "spillchuck"});
  result.should_hide_suggestion_menu = true;

  GetDocument().GetFrame()->GetSpellChecker().MarkAndReplaceFor(
      request, Vector<TextCheckingResult>({result}));

  ASSERT_EQ(1u, GetDocument().Markers().Markers().size());
  EXPECT_TRUE(To<SpellCheckMarker>(GetDocument().Markers().Markers()[0].Get())
                  ->ShouldHideSuggestionMenu());
}

TEST_P(SpellCheckerTest, MarkAndReplaceForHandlesMultipleReplacements) {
  SetBodyContent(
      "<div contenteditable>"
      "spllchck"
      "</div>");
  Element* div = QuerySelector("div");
  Node* text = div->firstChild();
  EphemeralRange range_to_check =
      EphemeralRange(Position(text, 0), Position(text, 8));

  SpellCheckRequest* request = SpellCheckRequest::Create(
      range_to_check,
      /*request_number=*/0, /*should_force_refresh=*/false);

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

TEST_P(SpellCheckerTest, GetSpellCheckMarkerUnderSelection_FirstCharSelected) {
  SetBodyContent(
      "<div contenteditable>"
      "spllchck"
      "</div>");
  Element* div = QuerySelector("div");
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

TEST_P(SpellCheckerTest, GetSpellCheckMarkerUnderSelection_LastCharSelected) {
  SetBodyContent(
      "<div contenteditable>"
      "spllchck"
      "</div>");
  Element* div = QuerySelector("div");
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

TEST_P(SpellCheckerTest,
       GetSpellCheckMarkerUnderSelection_SingleCharWordSelected) {
  SetBodyContent(
      "<div contenteditable>"
      "s"
      "</div>");
  Element* div = QuerySelector("div");
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

TEST_P(SpellCheckerTest,
       GetSpellCheckMarkerUnderSelection_CaretLeftOfSingleCharWord) {
  SetBodyContent(
      "<div contenteditable>"
      "s"
      "</div>");
  Element* div = QuerySelector("div");
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

TEST_P(SpellCheckerTest,
       GetSpellCheckMarkerUnderSelection_CaretRightOfSingleCharWord) {
  SetBodyContent(
      "<div contenteditable>"
      "s"
      "</div>");
  Element* div = QuerySelector("div");
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

TEST_P(SpellCheckerTest,
       GetSpellCheckMarkerUnderSelection_CaretLeftOfMultiCharWord) {
  SetBodyContent(
      "<div contenteditable>"
      "spllchck"
      "</div>");
  Element* div = QuerySelector("div");
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

TEST_P(SpellCheckerTest,
       GetSpellCheckMarkerUnderSelection_CaretRightOfMultiCharWord) {
  SetBodyContent(
      "<div contenteditable>"
      "spllchck"
      "</div>");
  Element* div = QuerySelector("div");
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

TEST_P(SpellCheckerTest, GetSpellCheckMarkerUnderSelection_CaretMiddleOfWord) {
  SetBodyContent(
      "<div contenteditable>"
      "spllchck"
      "</div>");
  Element* div = QuerySelector("div");
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

TEST_P(SpellCheckerTest,
       GetSpellCheckMarkerUnderSelection_CaretOneCharLeftOfMisspelling) {
  SetBodyContent(
      "<div contenteditable>"
      "a spllchck"
      "</div>");
  Element* div = QuerySelector("div");
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

TEST_P(SpellCheckerTest,
       GetSpellCheckMarkerUnderSelection_CaretOneCharRightOfMisspelling) {
  SetBodyContent(
      "<div contenteditable>"
      "spllchck a"
      "</div>");
  Element* div = QuerySelector("div");
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

TEST_P(SpellCheckerTest, GetSpellCheckMarkerUnderSelection_MultiNodeMisspell) {
  SetBodyContent(
      "<div contenteditable>"
      "spl<b>lc</b>hck"
      "</div>");
  Element* div = QuerySelector("div");
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

TEST_P(SpellCheckerTest,
       MarkAndReplaceForAlsoRemovesSuggestionMarkersOfMisspellingType) {
#if BUILDFLAG(IS_ANDROID)
  if (!GetParam()) {
    GTEST_SKIP() << "Test only runs with Spell Check Full API flag enabled";
  }

  SetBodyContent(
      "<div contenteditable>"
      "Good moring, thisis a good day."
      "</div>");
  Element* div = QuerySelector("div");
  Node* text = div->firstChild();

  // Spelling Marker for 'moring'.
  GetDocument().Markers().AddSpellingMarker(
      EphemeralRange(Position(text, 5), Position(text, 11)));
  // Suggestion Marker of Misspelling type for 'thisis'.
  GetDocument().Markers().AddSuggestionMarker(
      EphemeralRange(Position(text, 13), Position(text, 19)),
      SuggestionMarkerProperties::Builder()
          .SetType(SuggestionMarker::SuggestionType::kMisspelling)
          .Build());

  SpellCheckRequest* request = SpellCheckRequest::Create(
      EphemeralRange(Position(text, 0), Position(text, 31)),
      /*request_number=*/0, /*should_force_refresh=*/false);

  TextCheckingResult result;
  result.decoration = TextDecorationType::kTextDecorationTypeSpelling;
  result.location = 5;
  result.length = 6;
  result.replacements = Vector<String>({"morning"});

  GetDocument().GetFrame()->GetSpellChecker().MarkAndReplaceFor(
      request, Vector<TextCheckingResult>({result}));

  ASSERT_TRUE(ShouldRemoveSuggestionMarkerOfMisspellingAndGrammarType());
  // After applying text check results, pre-existing markers should be cleared
  // and the only marker should be created from the suggested replacement.
  ASSERT_EQ(1u, GetDocument().Markers().Markers().size());
  EXPECT_EQ(GetDocument().Markers().Markers()[0]->GetType(),
            DocumentMarker::MarkerType::kSpelling);
  // The new spelling marker should have the right suggestion list (stored in
  // description).
  EXPECT_EQ("morning",
            To<SpellCheckMarker>(GetDocument().Markers().Markers()[0].Get())
                ->Description());
  // There should be no other markers, e.g. suggestion markers should be
  // cleared too.
  EXPECT_EQ(nullptr,
            GetDocument().Markers().FirstMarkerIntersectingEphemeralRange(
                EphemeralRange(Position(text, 0), Position(text, 31)),
                DocumentMarker::MarkerTypes::Suggestion()));
#else
  GTEST_SKIP() << "Test only runs on Android";
#endif
}

TEST_P(SpellCheckerTest,
       MarkAndReplaceForAlsoRemovesSuggestionMarkersOfGrammarType) {
#if BUILDFLAG(IS_ANDROID)
  if (!GetParam()) {
    GTEST_SKIP() << "Test only runs with Spell Check Full API flag enabled";
  }

  SetBodyContent(
      "<div contenteditable>"
      "Here is trees. There are a flower."
      "</div>");
  Element* div = QuerySelector("div");
  Node* text = div->firstChild();

  // Grammar Marker for 'is'.
  GetDocument().Markers().AddGrammarMarker(
      EphemeralRange(Position(text, 5), Position(text, 7)));
  // Suggestion Marker of Grammar type for 'are'.
  GetDocument().Markers().AddSuggestionMarker(
      EphemeralRange(Position(text, 21), Position(text, 24)),
      SuggestionMarkerProperties::Builder()
          .SetType(SuggestionMarker::SuggestionType::kGrammar)
          .Build());

  SpellCheckRequest* request = SpellCheckRequest::Create(
      EphemeralRange(Position(text, 0), Position(text, 34)),
      /*request_number=*/0, /*should_force_refresh=*/false);

  TextCheckingResult result;
  result.decoration = TextDecorationType::kTextDecorationTypeGrammar;
  result.location = 5;
  result.length = 2;
  result.replacements = Vector<String>({"are"});
  GrammarDetail detail;
  detail.location = 5;
  detail.length = 2;
  result.details.push_back(detail);

  GetDocument().GetFrame()->GetSpellChecker().MarkAndReplaceFor(
      request, Vector<TextCheckingResult>({result}));

  ASSERT_TRUE(ShouldRemoveSuggestionMarkerOfMisspellingAndGrammarType());
  // After applying text check results, pre-existing markers should be cleared
  // and the only marker should be created from the suggested replacement.
  ASSERT_EQ(1u, GetDocument().Markers().Markers().size());
  EXPECT_EQ(GetDocument().Markers().Markers()[0]->GetType(),
            DocumentMarker::MarkerType::kGrammar);
  // The new grammar marker should have the right suggestion list (stored in
  // description).
  EXPECT_EQ("are",
            To<SpellCheckMarker>(GetDocument().Markers().Markers()[0].Get())
                ->Description());
  // There should be no other markers, e.g. suggestion markers should be
  // cleared too.
  EXPECT_EQ(nullptr,
            GetDocument().Markers().FirstMarkerIntersectingEphemeralRange(
                EphemeralRange(Position(text, 0), Position(text, 34)),
                DocumentMarker::MarkerTypes::Suggestion()));
#else
  GTEST_SKIP() << "Test only runs on Android";
#endif
}

TEST_P(SpellCheckerTest, PasswordFieldsAreIgnored) {
  // Check that spellchecking is enabled for an input type="text".
  SetBodyContent("<input type=\"text\">");
  auto* input = To<HTMLInputElement>(QuerySelector("input"));
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

TEST_P(SpellCheckerTest, GetSpellingMarkers) {
  SetBodyContent("<div contenteditable>wellcome home</div>");
  Element* div = QuerySelector("div");
  Node* text = div->firstChild();

  // Add a spelling marker for "wellcome"
  GetDocument().Markers().AddSpellingMarker(
      EphemeralRange(Position(text, 0), Position(text, 8)));

  SpellCheckRequest* request = SpellCheckRequest::Create(
      EphemeralRange(Position(text, 0), Position(text, 13)), 0, false);

#if BUILDFLAG(IS_ANDROID)
  auto markers = request->GetSpellingMarkers();
  if (GetParam()) {
    ASSERT_TRUE(ShouldSendSpellingMarkersInfo());
    ASSERT_EQ(1u, markers.size());

    EXPECT_EQ(0u, markers[0]->StartOffset());
    EXPECT_EQ(8u, markers[0]->EndOffset());
    EXPECT_EQ(DocumentMarker::MarkerType::kSpelling, markers[0]->GetType());
  } else {
    ASSERT_FALSE(ShouldSendSpellingMarkersInfo());

    EXPECT_EQ(0u, markers.size());
  }
#else
  ASSERT_FALSE(ShouldSendSpellingMarkersInfo());

  EXPECT_EQ(0u, request->GetSpellingMarkers().size());
#endif
}

}  // namespace blink
