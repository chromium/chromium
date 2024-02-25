/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"

#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/markers/custom_highlight_marker.h"
#include "third_party/blink/renderer/core/editing/markers/suggestion_marker.h"
#include "third_party/blink/renderer/core/editing/markers/suggestion_marker_properties.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/highlight/highlight.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"

namespace blink {

class DocumentMarkerControllerTest : public EditingTestBase {
 protected:
  DocumentMarkerController& MarkerController() const {
    return GetDocument().Markers();
  }

  Text* CreateTextNode(const char*);
  void MarkNodeContents(Node*);
  void MarkNodeContentsTextMatch(Node*);
};

Text* DocumentMarkerControllerTest::CreateTextNode(const char* text_contents) {
  return GetDocument().createTextNode(String::FromUTF8(text_contents));
}

void DocumentMarkerControllerTest::MarkNodeContents(Node* node) {
  // Force layoutObjects to be created; TextIterator, which is used in
  // DocumentMarkerControllerTest::addMarker(), needs them.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  auto range = EphemeralRange::RangeOfContents(*node);
  MarkerController().AddSpellingMarker(range);
}

void DocumentMarkerControllerTest::MarkNodeContentsTextMatch(Node* node) {
  // Force layoutObjects to be created; TextIterator, which is used in
  // DocumentMarkerControllerTest::addMarker(), needs them.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  auto range = EphemeralRange::RangeOfContents(*node);
  MarkerController().AddTextMatchMarker(range,
                                        TextMatchMarker::MatchStatus::kActive);
}

TEST_F(DocumentMarkerControllerTest, DidMoveToNewDocument) {
  SetBodyContent("<b><i>foo</i></b>");
  auto* parent = To<Element>(GetDocument().body()->firstChild()->firstChild());
  MarkNodeContents(parent);
  EXPECT_EQ(1u, MarkerController().Markers().size());
  ScopedNullExecutionContext execution_context;
  Persistent<Document> another_document =
      Document::CreateForTest(execution_context.GetExecutionContext());
  another_document->adoptNode(parent, ASSERT_NO_EXCEPTION);

  // No more reference to marked node.
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(0u, MarkerController().Markers().size());
  EXPECT_EQ(0u, another_document->Markers().Markers().size());
}

TEST_F(DocumentMarkerControllerTest, NodeWillBeRemovedMarkedByNormalize) {
  SetBodyContent("<b><i>foo</i></b>");
  {
    auto* parent =
        To<Element>(GetDocument().body()->firstChild()->firstChild());
    parent->AppendChild(CreateTextNode("bar"));
    MarkNodeContents(parent);
    EXPECT_EQ(2u, MarkerController().Markers().size());
    parent->normalize();
    UpdateAllLifecyclePhasesForTest();
  }
  // No more reference to marked node.
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(1u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest, NodeWillBeRemovedMarkedByRemoveChildren) {
  SetBodyContent("<b><i>foo</i></b>");
  auto* parent = To<Element>(GetDocument().body()->firstChild()->firstChild());
  MarkNodeContents(parent);
  EXPECT_EQ(1u, MarkerController().Markers().size());
  parent->RemoveChildren();
  UpdateAllLifecyclePhasesForTest();
  // No more reference to marked node.
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(0u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest, NodeWillBeRemovedByRemoveMarked) {
  SetBodyContent("<b><i>foo</i></b>");
  {
    auto* parent =
        To<Element>(GetDocument().body()->firstChild()->firstChild());
    MarkNodeContents(parent);
    EXPECT_EQ(1u, MarkerController().Markers().size());
    parent->RemoveChild(parent->firstChild());
    UpdateAllLifecyclePhasesForTest();
  }
  // No more reference to marked node.
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(0u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest, NodeWillBeRemovedMarkedByRemoveAncestor) {
  SetBodyContent("<b><i>foo</i></b>");
  {
    auto* parent =
        To<Element>(GetDocument().body()->firstChild()->firstChild());
    MarkNodeContents(parent);
    EXPECT_EQ(1u, MarkerController().Markers().size());
    parent->parentNode()->parentNode()->RemoveChild(parent->parentNode());
    UpdateAllLifecyclePhasesForTest();
  }
  // No more reference to marked node.
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(0u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest, NodeWillBeRemovedMarkedByRemoveParent) {
  SetBodyContent("<b><i>foo</i></b>");
  {
    auto* parent =
        To<Element>(GetDocument().body()->firstChild()->firstChild());
    MarkNodeContents(parent);
    EXPECT_EQ(1u, MarkerController().Markers().size());
    parent->parentNode()->RemoveChild(parent);
    UpdateAllLifecyclePhasesForTest();
  }
  // No more reference to marked node.
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(0u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest, NodeWillBeRemovedMarkedByReplaceChild) {
  SetBodyContent("<b><i>foo</i></b>");
  {
    auto* parent =
        To<Element>(GetDocument().body()->firstChild()->firstChild());
    MarkNodeContents(parent);
    EXPECT_EQ(1u, MarkerController().Markers().size());
    parent->ReplaceChild(CreateTextNode("bar"), parent->firstChild());
    UpdateAllLifecyclePhasesForTest();
  }
  // No more reference to marked node.
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(0u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest, NodeWillBeRemovedBySetInnerHTML) {
  SetBodyContent("<b><i>foo</i></b>");
  {
    auto* parent =
        To<Element>(GetDocument().body()->firstChild()->firstChild());
    MarkNodeContents(parent);
    EXPECT_EQ(1u, MarkerController().Markers().size());
    SetBodyContent("");
    UpdateAllLifecyclePhasesForTest();
  }
  // No more reference to marked node.
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(0u, MarkerController().Markers().size());
}

// For http://crbug.com/862900
TEST_F(DocumentMarkerControllerTest, SynchronousMutationNotificationAfterGC) {
  SetBodyContent("<b><i>foo</i></b>");
  Persistent<Text> sibling_text = CreateTextNode("bar");
  {
    auto* parent =
        To<Element>(GetDocument().body()->firstChild()->firstChild());
    parent->parentNode()->AppendChild(sibling_text);
    MarkNodeContents(parent);
    EXPECT_EQ(1u, MarkerController().Markers().size());
    parent->parentNode()->RemoveChild(parent);
    UpdateAllLifecyclePhasesForTest();
  }

  // GC the marked node, so it disappears from WeakMember collections.
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(0u, MarkerController().Markers().size());

  // Trigger SynchronousMutationNotifier::NotifyUpdateCharacterData().
  // This matches the conditions for the crashes in crbug.com/862960.
  sibling_text->setData("baz");
}

TEST_F(DocumentMarkerControllerTest, UpdateRenderedRects) {
  SetBodyContent("<div style='margin: 100px'>foo</div>");
  auto* div = To<Element>(GetDocument().body()->firstChild());
  MarkNodeContentsTextMatch(div);
  Vector<gfx::Rect> rendered_rects =
      MarkerController().LayoutRectsForTextMatchMarkers();
  EXPECT_EQ(1u, rendered_rects.size());

  div->setAttribute(html_names::kStyleAttr, AtomicString("margin: 200px"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Vector<gfx::Rect> new_rendered_rects =
      MarkerController().LayoutRectsForTextMatchMarkers();
  EXPECT_EQ(1u, new_rendered_rects.size());
  EXPECT_NE(rendered_rects[0], new_rendered_rects[0]);
}

TEST_F(DocumentMarkerControllerTest, CompositionMarkersNotMerged) {
  SetBodyContent("<div style='margin: 100px'>foo</div>");
  Node* text = GetDocument().body()->firstChild()->firstChild();
  MarkerController().AddCompositionMarker(
      EphemeralRange(Position(text, 0), Position(text, 1)), Color::kTransparent,
      ui::mojom::ImeTextSpanThickness::kThin,
      ui::mojom::ImeTextSpanUnderlineStyle::kSolid, Color::kBlack,
      Color::kBlack);
  MarkerController().AddCompositionMarker(
      EphemeralRange(Position(text, 1), Position(text, 3)), Color::kTransparent,
      ui::mojom::ImeTextSpanThickness::kThick,
      ui::mojom::ImeTextSpanUnderlineStyle::kSolid, Color::kBlack,
      Color::kBlack);

  EXPECT_EQ(2u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest, SetMarkerActiveTest) {
  SetBodyContent("<b>foo</b>");
  auto* b_element = To<Element>(GetDocument().body()->firstChild());
  EphemeralRange ephemeral_range = EphemeralRange::RangeOfContents(*b_element);
  Position start_b_element =
      ToPositionInDOMTree(ephemeral_range.StartPosition());
  Position end_b_element = ToPositionInDOMTree(ephemeral_range.EndPosition());
  const EphemeralRange range(start_b_element, end_b_element);
  // Try to make active a marker that doesn't exist.
  EXPECT_FALSE(MarkerController().SetTextMatchMarkersActive(range, true));

  // Add a marker and try it once more.
  MarkerController().AddTextMatchMarker(
      range, TextMatchMarker::MatchStatus::kInactive);
  EXPECT_EQ(1u, MarkerController().Markers().size());
  EXPECT_TRUE(MarkerController().SetTextMatchMarkersActive(range, true));
}

TEST_F(DocumentMarkerControllerTest, RemoveStartOfMarker) {
  SetBodyContent("<b>abc</b>");
  Node* b_element = GetDocument().body()->firstChild();
  Node* text = b_element->firstChild();

  // Add marker under "abc"
  EphemeralRange marker_range =
      EphemeralRange(Position(text, 0), Position(text, 3));
  GetDocument().Markers().AddTextMatchMarker(
      marker_range, TextMatchMarker::MatchStatus::kInactive);

  // Remove markers that overlap "a"
  marker_range = EphemeralRange(Position(text, 0), Position(text, 1));
  GetDocument().Markers().RemoveMarkersInRange(
      marker_range, DocumentMarker::MarkerTypes::All());

  EXPECT_EQ(0u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest, RemoveMiddleOfMarker) {
  SetBodyContent("<b>abc</b>");
  Node* b_element = GetDocument().body()->firstChild();
  Node* text = b_element->firstChild();

  // Add marker under "abc"
  EphemeralRange marker_range =
      EphemeralRange(Position(text, 0), Position(text, 3));
  GetDocument().Markers().AddTextMatchMarker(
      marker_range, TextMatchMarker::MatchStatus::kInactive);

  // Remove markers that overlap "b"
  marker_range = EphemeralRange(Position(text, 1), Position(text, 2));
  GetDocument().Markers().RemoveMarkersInRange(
      marker_range, DocumentMarker::MarkerTypes::All());

  EXPECT_EQ(0u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest, RemoveEndOfMarker) {
  SetBodyContent("<b>abc</b>");
  Node* b_element = GetDocument().body()->firstChild();
  Node* text = b_element->firstChild();

  // Add marker under "abc"
  EphemeralRange marker_range =
      EphemeralRange(Position(text, 0), Position(text, 3));
  GetDocument().Markers().AddTextMatchMarker(
      marker_range, TextMatchMarker::MatchStatus::kInactive);

  // Remove markers that overlap "c"
  marker_range = EphemeralRange(Position(text, 2), Position(text, 3));
  GetDocument().Markers().RemoveMarkersInRange(
      marker_range, DocumentMarker::MarkerTypes::All());

  EXPECT_EQ(0u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest, RemoveSpellingMarkersUnderWords) {
  SetBodyContent("<div contenteditable>foo</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  Node* text = div->firstChild();

  // Add a spelling marker and a text match marker to "foo".
  const EphemeralRange marker_range(Position(text, 0), Position(text, 3));
  MarkerController().AddSpellingMarker(marker_range);
  MarkerController().AddTextMatchMarker(
      marker_range, TextMatchMarker::MatchStatus::kInactive);

  MarkerController().RemoveSpellingMarkersUnderWords({"foo"});

  // RemoveSpellingMarkersUnderWords does not remove text match marker.
  ASSERT_EQ(1u, MarkerController().Markers().size());
  const DocumentMarker& marker = *MarkerController().Markers()[0];
  EXPECT_EQ(0u, marker.StartOffset());
  EXPECT_EQ(3u, marker.EndOffset());
  EXPECT_EQ(DocumentMarker::kTextMatch, marker.GetType());
}

TEST_F(DocumentMarkerControllerTest, RemoveSpellingMarkersUnderAllWords) {
  SetBodyContent("<div contenteditable>foo</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  Node* text = div->firstChild();
  ASSERT_NE(text->GetLayoutObject(), nullptr);

  const EphemeralRange marker_range(Position(text, 0), Position(text, 3));
  text->GetLayoutObject()->ClearPaintInvalidationFlags();
  MarkerController().AddSpellingMarker(marker_range);
  EXPECT_TRUE(text->GetLayoutObject()->ShouldCheckForPaintInvalidation());
  ASSERT_EQ(1u, MarkerController().Markers().size());

  text->GetLayoutObject()->ClearPaintInvalidationFlags();
  MarkerController().RemoveSpellingMarkersUnderWords({"foo"});
  EXPECT_TRUE(text->GetLayoutObject()->ShouldCheckForPaintInvalidation());
  ASSERT_EQ(0u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest, RemoveSuggestionMarkerByTag) {
  SetBodyContent("<div contenteditable>foo</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  Node* text = div->firstChild();

  MarkerController().AddSuggestionMarker(
      EphemeralRange(Position(text, 0), Position(text, 1)),
      SuggestionMarkerProperties());

  ASSERT_EQ(1u, MarkerController().Markers().size());
  auto* marker = To<SuggestionMarker>(MarkerController().Markers()[0].Get());
  MarkerController().RemoveSuggestionMarkerByTag(*To<Text>(text),
                                                 marker->Tag());
  EXPECT_EQ(0u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest, RemoveSuggestionMarkerByTypeWithRange) {
  SetBodyContent("<div contenteditable>foo</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  Node* text = div->firstChild();
  EphemeralRange range(Position(text, 0), Position(text, 1));
  MarkerController().AddSuggestionMarker(range, SuggestionMarkerProperties());

  ASSERT_EQ(1u, MarkerController().Markers().size());
  auto* marker = To<SuggestionMarker>(MarkerController().Markers()[0].Get());
  MarkerController().RemoveSuggestionMarkerByType(
      ToEphemeralRangeInFlatTree(range), marker->GetSuggestionType());
  EXPECT_EQ(0u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest, RemoveSuggestionMarkerByType) {
  SetBodyContent("<div contenteditable>123 456</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  Node* text = div->firstChild();

  // Add an autocorrect marker on "123"
  MarkerController().AddSuggestionMarker(
      EphemeralRange(Position(text, 0), Position(text, 3)),
      SuggestionMarkerProperties::Builder()
          .SetType(SuggestionMarker::SuggestionType::kAutocorrect)
          .Build());
  // Add a misspelling suggestion marker on "123"
  MarkerController().AddSuggestionMarker(
      EphemeralRange(Position(text, 0), Position(text, 3)),
      SuggestionMarkerProperties::Builder()
          .SetType(SuggestionMarker::SuggestionType::kMisspelling)
          .Build());

  EXPECT_EQ(2u, MarkerController().Markers().size());
  MarkerController().RemoveSuggestionMarkerByType(
      SuggestionMarker::SuggestionType::kAutocorrect);

  EXPECT_EQ(1u, MarkerController().Markers().size());
  EXPECT_EQ(SuggestionMarker::SuggestionType::kMisspelling,
            To<SuggestionMarker>(MarkerController().Markers()[0].Get())
                ->GetSuggestionType());
}

TEST_F(DocumentMarkerControllerTest, RemoveSuggestionMarkerInRangeOnFinish) {
  SetBodyContent("<div contenteditable>foo</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  Node* text = div->firstChild();

  // Add a regular suggestion marker, RemoveSuggestionMarkerInRangeOnFinish()
  // should not remove it.
  MarkerController().AddSuggestionMarker(
      EphemeralRange(Position(text, 0), Position(text, 2)),
      SuggestionMarkerProperties());

  ASSERT_EQ(1u, MarkerController().Markers().size());
  MarkerController().RemoveSuggestionMarkerInRangeOnFinish(
      EphemeralRangeInFlatTree(PositionInFlatTree(text, 0),
                               PositionInFlatTree(text, 2)));

  EXPECT_EQ(1u, MarkerController().Markers().size());

  const auto* marker =
      To<SuggestionMarker>(MarkerController().Markers()[0].Get());
  MarkerController().RemoveSuggestionMarkerByTag(*To<Text>(text),
                                                 marker->Tag());
  ASSERT_EQ(0u, MarkerController().Markers().size());

  // Add a suggestion marker which need to be removed after finish composing,
  // RemoveSuggestionMarkerInRangeOnFinish() should remove it.
  MarkerController().AddSuggestionMarker(
      EphemeralRange(Position(text, 0), Position(text, 2)),
      SuggestionMarkerProperties::Builder()
          .SetRemoveOnFinishComposing(true)
          .Build());

  ASSERT_EQ(1u, MarkerController().Markers().size());

  MarkerController().RemoveSuggestionMarkerInRangeOnFinish(
      EphemeralRangeInFlatTree(PositionInFlatTree(text, 0),
                               PositionInFlatTree(text, 2)));
  EXPECT_EQ(0u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest, FirstMarkerIntersectingOffsetRange) {
  SetBodyContent("<div contenteditable>123456789</div>");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  auto* text = To<Text>(div->firstChild());

  // Add a spelling marker on "123"
  MarkerController().AddSpellingMarker(
      EphemeralRange(Position(text, 0), Position(text, 3)));

  // Query for a spellcheck marker intersecting "3456"
  const DocumentMarker* const result =
      MarkerController().FirstMarkerIntersectingOffsetRange(
          *text, 2, 6, DocumentMarker::MarkerTypes::Misspelling());

  EXPECT_EQ(DocumentMarker::kSpelling, result->GetType());
  EXPECT_EQ(0u, result->StartOffset());
  EXPECT_EQ(3u, result->EndOffset());
}

TEST_F(DocumentMarkerControllerTest,
       FirstMarkerIntersectingOffsetRange_collapsed) {
  SetBodyContent("<div contenteditable>123456789</div>");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  auto* text = To<Text>(div->firstChild());

  // Add a spelling marker on "123"
  MarkerController().AddSpellingMarker(
      EphemeralRange(Position(text, 0), Position(text, 3)));

  // Query for a spellcheck marker containing the position between "1" and "2"
  const DocumentMarker* const result =
      MarkerController().FirstMarkerIntersectingOffsetRange(
          *text, 1, 1, DocumentMarker::MarkerTypes::Misspelling());

  EXPECT_EQ(DocumentMarker::kSpelling, result->GetType());
  EXPECT_EQ(0u, result->StartOffset());
  EXPECT_EQ(3u, result->EndOffset());
}

TEST_F(DocumentMarkerControllerTest, MarkersAroundPosition) {
  SetBodyContent("<div contenteditable>123 456</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  Node* text = div->firstChild();

  // Add a spelling marker on "123"
  MarkerController().AddSpellingMarker(
      EphemeralRange(Position(text, 0), Position(text, 3)));
  // Add a text match marker on "123"
  MarkerController().AddTextMatchMarker(
      EphemeralRange(Position(text, 0), Position(text, 3)),
      TextMatchMarker::MatchStatus::kInactive);
  // Add a grammar marker on "456"
  MarkerController().AddSpellingMarker(
      EphemeralRange(Position(text, 4), Position(text, 7)));

  // Query for spellcheck markers at the start of "123".
  const HeapVector<std::pair<Member<const Text>, Member<DocumentMarker>>>&
      result1 = MarkerController().MarkersAroundPosition(
          PositionInFlatTree(text, 0),
          DocumentMarker::MarkerTypes::Misspelling());

  EXPECT_EQ(1u, result1.size());
  EXPECT_EQ(DocumentMarker::kSpelling, result1[0].second->GetType());
  EXPECT_EQ(0u, result1[0].second->StartOffset());
  EXPECT_EQ(3u, result1[0].second->EndOffset());

  // Query for spellcheck markers in the middle of "123".
  const HeapVector<std::pair<Member<const Text>, Member<DocumentMarker>>>&
      result2 = MarkerController().MarkersAroundPosition(
          PositionInFlatTree(text, 3),
          DocumentMarker::MarkerTypes::Misspelling());

  EXPECT_EQ(1u, result2.size());
  EXPECT_EQ(DocumentMarker::kSpelling, result2[0].second->GetType());
  EXPECT_EQ(0u, result2[0].second->StartOffset());
  EXPECT_EQ(3u, result2[0].second->EndOffset());

  // Query for spellcheck markers at the end of "123".
  const HeapVector<std::pair<Member<const Text>, Member<DocumentMarker>>>&
      result3 = MarkerController().MarkersAroundPosition(
          PositionInFlatTree(text, 3),
          DocumentMarker::MarkerTypes::Misspelling());

  EXPECT_EQ(1u, result3.size());
  EXPECT_EQ(DocumentMarker::kSpelling, result3[0].second->GetType());
  EXPECT_EQ(0u, result3[0].second->StartOffset());
  EXPECT_EQ(3u, result3[0].second->EndOffset());
}

TEST_F(DocumentMarkerControllerTest, MarkersIntersectingRange) {
  SetBodyContent("<div contenteditable>123456789</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  Node* text = div->firstChild();

  // Add a spelling marker on "123"
  MarkerController().AddSpellingMarker(
      EphemeralRange(Position(text, 0), Position(text, 3)));
  // Add a text match marker on "456"
  MarkerController().AddTextMatchMarker(
      EphemeralRange(Position(text, 3), Position(text, 6)),
      TextMatchMarker::MatchStatus::kInactive);
  // Add a grammar marker on "789"
  MarkerController().AddSpellingMarker(
      EphemeralRange(Position(text, 6), Position(text, 9)));

  // Query for spellcheck markers intersecting "3456". The text match marker
  // should not be returned, nor should the spelling marker touching the range.
  const HeapVector<std::pair<Member<const Text>, Member<DocumentMarker>>>&
      results = MarkerController().MarkersIntersectingRange(
          EphemeralRangeInFlatTree(PositionInFlatTree(text, 2),
                                   PositionInFlatTree(text, 6)),
          DocumentMarker::MarkerTypes::Misspelling());

  EXPECT_EQ(1u, results.size());
  EXPECT_EQ(DocumentMarker::kSpelling, results[0].second->GetType());
  EXPECT_EQ(0u, results[0].second->StartOffset());
  EXPECT_EQ(3u, results[0].second->EndOffset());
}

TEST_F(DocumentMarkerControllerTest, MarkersIntersectingCollapsedRange) {
  SetBodyContent("<div contenteditable>123456789</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  Node* text = div->firstChild();

  // Add a spelling marker on "123"
  MarkerController().AddSpellingMarker(
      EphemeralRange(Position(text, 0), Position(text, 3)));

  // Query for spellcheck markers containing the position between "1" and "2"
  const HeapVector<std::pair<Member<const Text>, Member<DocumentMarker>>>&
      results = MarkerController().MarkersIntersectingRange(
          EphemeralRangeInFlatTree(PositionInFlatTree(text, 1),
                                   PositionInFlatTree(text, 1)),
          DocumentMarker::MarkerTypes::Misspelling());

  EXPECT_EQ(1u, results.size());
  EXPECT_EQ(DocumentMarker::kSpelling, results[0].second->GetType());
  EXPECT_EQ(0u, results[0].second->StartOffset());
  EXPECT_EQ(3u, results[0].second->EndOffset());
}

TEST_F(DocumentMarkerControllerTest, MarkersIntersectingRangeWithShadowDOM) {
  // Set up some shadow elements in a way we know doesn't work properly when
  // using EphemeralRange instead of EphemeralRangeInFlatTree:
  // <div>not shadow</div>
  // <div> (shadow DOM host)
  //   #shadow-root
  //     <div>shadow1</div>
  //     <div>shadow2</div>
  // Caling MarkersIntersectingRange with an EphemeralRange starting in the
  // "not shadow" text and ending in the "shadow1" text will crash.
  SetBodyContent(
      "<div id=\"not_shadow\">not shadow</div><div id=\"shadow_root\" />");
  ShadowRoot* shadow_root = SetShadowContent(
      "<div id=\"shadow1\">shadow1</div><div id=\"shadow2\">shadow2</div>",
      "shadow_root");

  Element* not_shadow_div =
      GetDocument().QuerySelector(AtomicString("#not_shadow"));
  Node* not_shadow_text = not_shadow_div->firstChild();

  Element* shadow1 = shadow_root->QuerySelector(AtomicString("#shadow1"));
  Node* shadow1_text = shadow1->firstChild();

  MarkerController().AddTextMatchMarker(
      EphemeralRange(Position(not_shadow_text, 0),
                     Position(not_shadow_text, 10)),
      TextMatchMarker::MatchStatus::kInactive);

  const HeapVector<std::pair<Member<const Text>, Member<DocumentMarker>>>&
      results = MarkerController().MarkersIntersectingRange(
          EphemeralRangeInFlatTree(PositionInFlatTree(not_shadow_text, 9),
                                   PositionInFlatTree(shadow1_text, 1)),
          DocumentMarker::MarkerTypes::TextMatch());
  EXPECT_EQ(1u, results.size());
}

TEST_F(DocumentMarkerControllerTest, SuggestionMarkersHaveUniqueTags) {
  SetBodyContent("<div contenteditable>foo</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  Node* text = div->firstChild();

  MarkerController().AddSuggestionMarker(
      EphemeralRange(Position(text, 0), Position(text, 1)),
      SuggestionMarkerProperties());
  MarkerController().AddSuggestionMarker(
      EphemeralRange(Position(text, 0), Position(text, 1)),
      SuggestionMarkerProperties());

  EXPECT_EQ(2u, MarkerController().Markers().size());
  EXPECT_NE(To<SuggestionMarker>(MarkerController().Markers()[0].Get())->Tag(),
            To<SuggestionMarker>(MarkerController().Markers()[1].Get())->Tag());
}

TEST_F(DocumentMarkerControllerTest, HighlightsAreNonOverlappingAndSorted) {
  SetBodyContent("<div>012345678901234567890123456789</div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  Text* text = To<Text>(div->firstChild());

  HeapVector<Member<AbstractRange>> highlight_ranges;
  Highlight* highlight1 = Highlight::Create(highlight_ranges);
  MarkerController().AddCustomHighlightMarker(
      EphemeralRange(Position(text, 0), Position(text, 5)), "highlight1",
      highlight1);
  MarkerController().AddCustomHighlightMarker(
      EphemeralRange(Position(text, 10), Position(text, 15)), "highlight1",
      highlight1);
  MarkerController().AddCustomHighlightMarker(
      EphemeralRange(Position(text, 12), Position(text, 14)), "highlight1",
      highlight1);
  MarkerController().AddCustomHighlightMarker(
      EphemeralRange(Position(text, 14), Position(text, 20)), "highlight1",
      highlight1);
  MarkerController().AddCustomHighlightMarker(
      EphemeralRange(Position(text, 25), Position(text, 30)), "highlight1",
      highlight1);

  Highlight* highlight2 = Highlight::Create(highlight_ranges);
  MarkerController().AddCustomHighlightMarker(
      EphemeralRange(Position(text, 0), Position(text, 5)), "highlight2",
      highlight2);
  MarkerController().AddCustomHighlightMarker(
      EphemeralRange(Position(text, 0), Position(text, 15)), "highlight2",
      highlight2);
  MarkerController().AddCustomHighlightMarker(
      EphemeralRange(Position(text, 15), Position(text, 30)), "highlight2",
      highlight2);
  MarkerController().AddCustomHighlightMarker(
      EphemeralRange(Position(text, 20), Position(text, 30)), "highlight2",
      highlight2);

  MarkerController().MergeOverlappingMarkers(DocumentMarker::kCustomHighlight);
  DocumentMarkerVector markers = MarkerController().MarkersFor(
      *text, DocumentMarker::MarkerTypes::CustomHighlight());
  EXPECT_EQ(5u, markers.size());
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(5u, markers[0]->EndOffset());
  EXPECT_EQ("highlight1", To<CustomHighlightMarker>(markers[0].Get())
                              ->GetHighlightName()
                              .GetString());
  EXPECT_EQ(0u, markers[1]->StartOffset());
  EXPECT_EQ(15u, markers[1]->EndOffset());
  EXPECT_EQ("highlight2", To<CustomHighlightMarker>(markers[1].Get())
                              ->GetHighlightName()
                              .GetString());
  EXPECT_EQ(10u, markers[2]->StartOffset());
  EXPECT_EQ(20u, markers[2]->EndOffset());
  EXPECT_EQ("highlight1", To<CustomHighlightMarker>(markers[2].Get())
                              ->GetHighlightName()
                              .GetString());
  EXPECT_EQ(15u, markers[3]->StartOffset());
  EXPECT_EQ(30u, markers[3]->EndOffset());
  EXPECT_EQ("highlight2", To<CustomHighlightMarker>(markers[3].Get())
                              ->GetHighlightName()
                              .GetString());
  EXPECT_EQ(25u, markers[4]->StartOffset());
  EXPECT_EQ(30u, markers[4]->EndOffset());
  EXPECT_EQ("highlight1", To<CustomHighlightMarker>(markers[4].Get())
                              ->GetHighlightName()
                              .GetString());
}

}  // namespace blink
