// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/suggestion/text_suggestion_controller.h"

#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/markers/suggestion_marker_properties.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"

using ui::mojom::ImeTextSpanThickness;

namespace blink {

class TextSuggestionControllerTest : public EditingTestBase {
 public:
  bool IsTextSuggestionHostAvailable() {
    return bool(GetDocument()
                    .GetFrame()
                    ->GetTextSuggestionController()
                    .text_suggestion_host_);
  }

  void ShowSuggestionMenu(
      const HeapVector<std::pair<Member<const Text>, Member<DocumentMarker>>>&
          node_suggestion_marker_pairs,
      size_t max_number_of_suggestions) {
    GetDocument().GetFrame()->GetTextSuggestionController().ShowSuggestionMenu(
        node_suggestion_marker_pairs, max_number_of_suggestions);
  }

  EphemeralRangeInFlatTree ComputeRangeSurroundingCaret(
      const PositionInFlatTree& caret_position) {
    const Node* const position_node = caret_position.ComputeContainerNode();
    const unsigned position_offset_in_node =
        caret_position.ComputeOffsetInContainerNode();
    // See ComputeRangeSurroundingCaret() in TextSuggestionController.
    return EphemeralRangeInFlatTree(
        PositionInFlatTree(position_node, position_offset_in_node - 1),
        PositionInFlatTree(position_node, position_offset_in_node + 1));
  }
};

TEST_F(TextSuggestionControllerTest, ApplySpellCheckSuggestion) {
  SetBodyContent(
      "<div contenteditable>"
      "spllchck"
      "</div>");
  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();

  GetDocument().Markers().AddActiveSuggestionMarker(
      EphemeralRange(Position(text, 0), Position(text, 8)), Color::kBlack,
      ImeTextSpanThickness::kThin, Color::kBlack);
  // Select immediately before misspelling
  GetDocument().GetFrame()->Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 0), Position(text, 0))
          .Build());
  GetDocument()
      .GetFrame()
      ->GetTextSuggestionController()
      .ApplySpellCheckSuggestion("spellcheck");

  EXPECT_EQ("spellcheck", text->textContent());

  // Cursor should be at end of replaced text
  const VisibleSelectionInFlatTree& selection =
      GetFrame().Selection().ComputeVisibleSelectionInFlatTree();
  EXPECT_EQ(text, selection.Start().ComputeContainerNode());
  EXPECT_EQ(10, selection.Start().ComputeOffsetInContainerNode());
  EXPECT_EQ(text, selection.End().ComputeContainerNode());
  EXPECT_EQ(10, selection.End().ComputeOffsetInContainerNode());
}

TEST_F(TextSuggestionControllerTest, ApplyTextSuggestion) {
  SetBodyContent(
      "<div contenteditable>"
      "word1 word2 word3 word4"
      "</div>");
  Element* div = GetDocument().QuerySelector("div");
  auto* text = To<Text>(div->firstChild());

  // Add marker on "word1". This marker should *not* be cleared by the
  // replace operation.
  GetDocument().Markers().AddSuggestionMarker(
      EphemeralRange(Position(text, 0), Position(text, 5)),
      SuggestionMarkerProperties::Builder()
          .SetSuggestions(Vector<String>({"marker1"}))
          .Build());

  // Add marker on "word1 word2 word3 word4". This marker should *not* be
  // cleared by the replace operation.
  GetDocument().Markers().AddSuggestionMarker(
      EphemeralRange(Position(text, 0), Position(text, 23)),
      SuggestionMarkerProperties::Builder()
          .SetSuggestions(Vector<String>({"marker2"}))
          .Build());

  // Add marker on "word2 word3". This marker should *not* be cleared by the
  // replace operation.
  GetDocument().Markers().AddSuggestionMarker(
      EphemeralRange(Position(text, 6), Position(text, 17)),
      SuggestionMarkerProperties::Builder()
          .SetSuggestions(Vector<String>({"marker3"}))
          .Build());

  // Add marker on "word4". This marker should *not* be cleared by the
  // replace operation.
  GetDocument().Markers().AddSuggestionMarker(
      EphemeralRange(Position(text, 18), Position(text, 23)),
      SuggestionMarkerProperties::Builder()
          .SetSuggestions(Vector<String>({"marker4"}))
          .Build());

  // Add marker on "word1 word2". This marker should be cleared by the
  // replace operation.
  GetDocument().Markers().AddSuggestionMarker(
      EphemeralRange(Position(text, 0), Position(text, 11)),
      SuggestionMarkerProperties::Builder()
          .SetSuggestions(Vector<String>({"marker5"}))
          .Build());

  // Add marker on "word3 word4". This marker should be cleared by the
  // replace operation.
  GetDocument().Markers().AddSuggestionMarker(
      EphemeralRange(Position(text, 12), Position(text, 23)),
      SuggestionMarkerProperties::Builder()
          .SetSuggestions(Vector<String>({"marker6"}))
          .Build());

  // Select immediately before word2.
  GetDocument().GetFrame()->Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 6), Position(text, 6))
          .Build());

  // Replace "word2 word3" with "marker3" (marker should have tag 3; tags start
  // from 1, not 0).
  GetDocument().GetFrame()->GetTextSuggestionController().ApplyTextSuggestion(
      3, 0);

  // This returns the markers sorted by start offset; we need them sorted by
  // start *and* end offset, since we have multiple markers starting at 0.
  DocumentMarkerVector markers = GetDocument().Markers().MarkersFor(*text);
  std::sort(markers.begin(), markers.end(),
            [](const DocumentMarker* marker1, const DocumentMarker* marker2) {
              if (marker1->StartOffset() != marker2->StartOffset())
                return marker1->StartOffset() < marker2->StartOffset();
              return marker1->EndOffset() < marker2->EndOffset();
            });

  EXPECT_EQ(4u, markers.size());

  // marker1
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(5u, markers[0]->EndOffset());

  // marker2
  EXPECT_EQ(0u, markers[1]->StartOffset());
  EXPECT_EQ(19u, markers[1]->EndOffset());

  // marker3
  EXPECT_EQ(6u, markers[2]->StartOffset());
  EXPECT_EQ(13u, markers[2]->EndOffset());

  const auto* const suggestion_marker = To<SuggestionMarker>(markers[2].Get());
  EXPECT_EQ(1u, suggestion_marker->Suggestions().size());
  EXPECT_EQ(String("word2 word3"), suggestion_marker->Suggestions()[0]);

  // marker4
  EXPECT_EQ(14u, markers[3]->StartOffset());
  EXPECT_EQ(19u, markers[3]->EndOffset());

  // marker5 and marker6 should've been cleared

  // Cursor should be at end of replaced text
  const VisibleSelectionInFlatTree& selection =
      GetFrame().Selection().ComputeVisibleSelectionInFlatTree();
  EXPECT_EQ(text, selection.Start().ComputeContainerNode());
  EXPECT_EQ(13, selection.Start().ComputeOffsetInContainerNode());
  EXPECT_EQ(text, selection.End().ComputeContainerNode());
  EXPECT_EQ(13, selection.End().ComputeOffsetInContainerNode());
}

TEST_F(TextSuggestionControllerTest,
       ApplyingMisspellingTextSuggestionClearsMarker) {
  SetBodyContent(
      "<div contenteditable>"
      "mispelled"
      "</div>");
  Element* div = GetDocument().QuerySelector("div");
  auto* text = To<Text>(div->firstChild());

  // Add marker on "mispelled". This marker should be cleared by the replace
  // operation.
  GetDocument().Markers().AddSuggestionMarker(
      EphemeralRange(Position(text, 0), Position(text, 9)),
      SuggestionMarkerProperties::Builder()
          .SetType(SuggestionMarker::SuggestionType::kMisspelling)
          .SetSuggestions(Vector<String>({"misspelled"}))
          .Build());

  // Check the tag for the marker that was just added (the current tag value is
  // not reset between test cases).
  int32_t marker_tag =
      To<SuggestionMarker>(GetDocument().Markers().MarkersFor(*text)[0].Get())
          ->Tag();

  // Select immediately before "mispelled".
  GetDocument().GetFrame()->Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 0), Position(text, 0))
          .Build());

  // Replace "mispelled" with "misspelled".
  GetDocument().GetFrame()->GetTextSuggestionController().ApplyTextSuggestion(
      marker_tag, 0);

  EXPECT_EQ(0u, GetDocument().Markers().MarkersFor(*text).size());
  EXPECT_EQ("misspelled", text->textContent());
}

TEST_F(TextSuggestionControllerTest, DeleteActiveSuggestionRange_DeleteAtEnd) {
  SetBodyContent(
      "<div contenteditable>"
      "word1 word2"
      "</div>");
  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();

  // Mark "word2" as the active suggestion range
  GetDocument().Markers().AddActiveSuggestionMarker(
      EphemeralRange(Position(text, 6), Position(text, 11)),
      Color::kTransparent, ImeTextSpanThickness::kThin, Color::kBlack);
  // Select immediately before word2
  GetDocument().GetFrame()->Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 6), Position(text, 6))
          .Build());
  GetDocument()
      .GetFrame()
      ->GetTextSuggestionController()
      .DeleteActiveSuggestionRange();

  EXPECT_EQ("word1\xA0", text->textContent());
}

TEST_F(TextSuggestionControllerTest,
       DeleteActiveSuggestionRange_DeleteInMiddle) {
  SetBodyContent(
      "<div contenteditable>"
      "word1 word2 word3"
      "</div>");
  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();

  // Mark "word2" as the active suggestion range
  GetDocument().Markers().AddActiveSuggestionMarker(
      EphemeralRange(Position(text, 6), Position(text, 11)),
      Color::kTransparent, ImeTextSpanThickness::kThin, Color::kBlack);
  // Select immediately before word2
  GetDocument().GetFrame()->Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 6), Position(text, 6))
          .Build());
  GetDocument()
      .GetFrame()
      ->GetTextSuggestionController()
      .DeleteActiveSuggestionRange();

  // One of the extra spaces around "word2" should have been removed
  EXPECT_EQ("word1\xA0word3", text->textContent());
}

TEST_F(TextSuggestionControllerTest,
       DeleteActiveSuggestionRange_DeleteAtBeginningWithSpaceAfter) {
  SetBodyContent(
      "<div contenteditable>"
      "word1 word2"
      "</div>");
  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();

  // Mark "word1" as the active suggestion range
  GetDocument().Markers().AddActiveSuggestionMarker(
      EphemeralRange(Position(text, 0), Position(text, 5)), Color::kTransparent,
      ImeTextSpanThickness::kThin, Color::kBlack);
  // Select immediately before word1
  GetDocument().GetFrame()->Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 0), Position(text, 0))
          .Build());
  GetDocument()
      .GetFrame()
      ->GetTextSuggestionController()
      .DeleteActiveSuggestionRange();

  // The space after "word1" should have been removed (to avoid leaving an
  // empty space at the beginning of the composition)
  EXPECT_EQ("word2", text->textContent());
}

TEST_F(TextSuggestionControllerTest,
       DeleteActiveSuggestionRange_DeleteEntireRange) {
  SetBodyContent(
      "<div contenteditable>"
      "word1"
      "</div>");
  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();

  // Mark "word1" as the active suggestion range
  GetDocument().Markers().AddActiveSuggestionMarker(
      EphemeralRange(Position(text, 0), Position(text, 5)), Color::kTransparent,
      ImeTextSpanThickness::kThin, Color::kBlack);
  // Select immediately before word1
  GetDocument().GetFrame()->Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 0), Position(text, 0))
          .Build());
  GetDocument()
      .GetFrame()
      ->GetTextSuggestionController()
      .DeleteActiveSuggestionRange();

  EXPECT_EQ("", text->textContent());
}

// The following two cases test situations that probably shouldn't occur in
// normal use (spell check/suggestoin markers not spanning a whole word), but
// are included anyway to verify that DeleteActiveSuggestionRange() is
// well-behaved in these cases

TEST_F(TextSuggestionControllerTest,
       DeleteActiveSuggestionRange_DeleteRangeWithTextBeforeAndSpaceAfter) {
  SetBodyContent(
      "<div contenteditable>"
      "word1word2 word3"
      "</div>");
  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();

  // Mark "word2" as the active suggestion range
  GetDocument().Markers().AddActiveSuggestionMarker(
      EphemeralRange(Position(text, 5), Position(text, 10)),
      Color::kTransparent, ImeTextSpanThickness::kThin, Color::kBlack);
  // Select immediately before word2
  GetDocument().GetFrame()->Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 5), Position(text, 5))
          .Build());
  GetDocument()
      .GetFrame()
      ->GetTextSuggestionController()
      .DeleteActiveSuggestionRange();

  EXPECT_EQ("word1\xA0word3", text->textContent());
}

TEST_F(TextSuggestionControllerTest,
       DeleteActiveSuggestionRange_DeleteRangeWithSpaceBeforeAndTextAfter) {
  SetBodyContent(
      "<div contenteditable>"
      "word1 word2word3"
      "</div>");
  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();

  // Mark "word2" as the active suggestion range
  GetDocument().Markers().AddActiveSuggestionMarker(
      EphemeralRange(Position(text, 6), Position(text, 11)),
      Color::kTransparent, ImeTextSpanThickness::kThin, Color::kBlack);
  // Select immediately before word2
  GetDocument().GetFrame()->Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 6), Position(text, 6))
          .Build());
  GetDocument()
      .GetFrame()
      ->GetTextSuggestionController()
      .DeleteActiveSuggestionRange();

  EXPECT_EQ("word1\xA0word3", text->textContent());
}

TEST_F(TextSuggestionControllerTest,
       DeleteActiveSuggestionRange_DeleteAtBeginningWithTextAfter) {
  SetBodyContent(
      "<div contenteditable>"
      "word1word2"
      "</div>");
  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();

  // Mark "word1" as the active suggestion range
  GetDocument().Markers().AddActiveSuggestionMarker(
      EphemeralRange(Position(text, 0), Position(text, 5)), Color::kTransparent,
      ImeTextSpanThickness::kThin, Color::kBlack);
  // Select immediately before word1
  GetDocument().GetFrame()->Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 0), Position(text, 0))
          .Build());
  GetDocument()
      .GetFrame()
      ->GetTextSuggestionController()
      .DeleteActiveSuggestionRange();

  EXPECT_EQ("word2", text->textContent());
}

TEST_F(TextSuggestionControllerTest,
       DeleteActiveSuggestionRange_OnNewWordAddedToDictionary) {
  SetBodyContent(
      "<div contenteditable>"
      "embiggen"
      "</div>");
  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();

  // Mark "embiggen" as misspelled
  GetDocument().Markers().AddSpellingMarker(
      EphemeralRange(Position(text, 0), Position(text, 8)));
  // Select inside before "embiggen"
  GetDocument().GetFrame()->Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 1), Position(text, 1))
          .Build());

  // Add some other word to the dictionary
  GetDocument()
      .GetFrame()
      ->GetTextSuggestionController()
      .OnNewWordAddedToDictionary("cromulent");
  // Verify the spelling marker is still present
  EXPECT_NE(nullptr, GetDocument()
                         .GetFrame()
                         ->GetSpellChecker()
                         .GetSpellCheckMarkerUnderSelection()
                         .first);

  // Add "embiggen" to the dictionary
  GetDocument()
      .GetFrame()
      ->GetTextSuggestionController()
      .OnNewWordAddedToDictionary("embiggen");
  // Verify the spelling marker is gone
  EXPECT_EQ(nullptr, GetDocument()
                         .GetFrame()
                         ->GetSpellChecker()
                         .GetSpellCheckMarkerUnderSelection()
                         .first);
}

TEST_F(TextSuggestionControllerTest, CallbackHappensAfterDocumentDestroyed) {
  LocalFrame& frame = *GetDocument().GetFrame();
  GetDocument().Shutdown();

  // Shouldn't crash
  frame.GetTextSuggestionController().SuggestionMenuTimeoutCallback(0);
}

TEST_F(TextSuggestionControllerTest, SuggestionMarkerWithEmptySuggestion) {
  SetBodyContent(
      "<div contenteditable>"
      "hello"
      "</div>");
  Element* div = GetDocument().QuerySelector("div");
  auto* text = To<Text>(div->firstChild());

  // Set suggestion marker with empty suggestion list.
  GetDocument().Markers().AddSuggestionMarker(
      EphemeralRange(Position(text, 0), Position(text, 5)),
      SuggestionMarkerProperties::Builder()
          .SetSuggestions(Vector<String>())
          .Build());

  // Set the caret inside the word.
  GetDocument().GetFrame()->Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 3), Position(text, 3))
          .Build());

  // Handle potential suggestion tap on the caret position.
  GetDocument()
      .GetFrame()
      ->GetTextSuggestionController()
      .HandlePotentialSuggestionTap(PositionInFlatTree(text, 3));

  // We don't trigger menu in this case so there shouldn't be any mojom
  // connection available.
  EXPECT_FALSE(IsTextSuggestionHostAvailable());

  const VisibleSelectionInFlatTree& selection =
      GetFrame().Selection().ComputeVisibleSelectionInFlatTree();
  EXPECT_FALSE(selection.IsNone());

  const EphemeralRangeInFlatTree& range_to_check =
      ComputeRangeSurroundingCaret(selection.Start());

  const HeapVector<std::pair<Member<const Text>, Member<DocumentMarker>>>&
      node_suggestion_marker_pairs =
          GetFrame().GetDocument()->Markers().MarkersIntersectingRange(
              range_to_check, DocumentMarker::MarkerTypes::Suggestion());
  EXPECT_FALSE(node_suggestion_marker_pairs.IsEmpty());

  // Calling ShowSuggestionMenu() shouldn't crash. See crbug.com/901135.
  // ShowSuggestionMenu() may still get called because of race condition.
  ShowSuggestionMenu(node_suggestion_marker_pairs, 3);
}

TEST_F(TextSuggestionControllerTest, SuggestionMarkerWithSuggestion) {
  SetBodyContent(
      "<div contenteditable>"
      "hello"
      "</div>");
  Element* div = GetDocument().QuerySelector("div");
  auto* text = To<Text>(div->firstChild());

  // Set suggestion marker with two suggestions.
  GetDocument().Markers().AddSuggestionMarker(
      EphemeralRange(Position(text, 0), Position(text, 5)),
      SuggestionMarkerProperties::Builder()
          .SetSuggestions(Vector<String>({"marker1", "marker2"}))
          .Build());

  // Set the caret inside the word.
  GetDocument().GetFrame()->Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 3), Position(text, 3))
          .Build());

  // Handle potential suggestion tap on the caret position.
  GetDocument()
      .GetFrame()
      ->GetTextSuggestionController()
      .HandlePotentialSuggestionTap(PositionInFlatTree(text, 3));

  EXPECT_TRUE(IsTextSuggestionHostAvailable());
}

}  // namespace blink
