// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/range.h"

#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_array_buffer_or_array_buffer_view.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/css/font_face_descriptors.h"
#include "third_party/blink/renderer/core/css/font_face_set_document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_list.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/geometry/float_quad.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class RangeTest : public EditingTestBase {};

TEST_F(RangeTest, extractContentsWithDOMMutationEvent) {
  GetDocument().body()->SetInnerHTMLFromString("<span><b>abc</b>def</span>");
  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* const script_element =
      GetDocument().CreateRawElement(html_names::kScriptTag);
  script_element->setTextContent(
      "let count = 0;"
      "const span = document.querySelector('span');"
      "span.addEventListener('DOMSubtreeModified', () => {"
      "  if (++count > 1) return;"
      "  span.firstChild.textContent = 'ABC';"
      "  span.lastChild.textContent = 'DEF';"
      "});");
  GetDocument().body()->AppendChild(script_element);

  Element* const span_element = GetDocument().QuerySelector("span");
  auto* const range = MakeGarbageCollected<Range>(GetDocument(), span_element,
                                                  0, span_element, 1);
  Element* const result = GetDocument().CreateRawElement(html_names::kDivTag);
  result->AppendChild(range->extractContents(ASSERT_NO_EXCEPTION));

  EXPECT_EQ("<b>abc</b>", result->InnerHTMLAsString())
      << "DOM mutation event handler should not affect result.";
  EXPECT_EQ("<span>DEF</span>", span_element->OuterHTMLAsString())
      << "DOM mutation event handler should be executed.";
}

// http://crbug.com/822510
TEST_F(RangeTest, IntersectsNode) {
  SetBodyContent(
      "<div>"
      "<span id='s0'>s0</span>"
      "<span id='s1'>s1</span>"
      "<span id='s2'>s2</span>"
      "</div>");
  Element* const div = GetDocument().QuerySelector("div");
  Element* const s0 = GetDocument().getElementById("s0");
  Element* const s1 = GetDocument().getElementById("s1");
  Element* const s2 = GetDocument().getElementById("s2");
  Range& range = *Range::Create(GetDocument());

  // Range encloses s0
  range.setStart(div, 0);
  range.setEnd(div, 1);
  EXPECT_TRUE(range.intersectsNode(s0, ASSERT_NO_EXCEPTION));
  EXPECT_FALSE(range.intersectsNode(s1, ASSERT_NO_EXCEPTION));
  EXPECT_FALSE(range.intersectsNode(s2, ASSERT_NO_EXCEPTION));

  // Range encloses s1
  range.setStart(div, 1);
  range.setEnd(div, 2);
  EXPECT_FALSE(range.intersectsNode(s0, ASSERT_NO_EXCEPTION));
  EXPECT_TRUE(range.intersectsNode(s1, ASSERT_NO_EXCEPTION));
  EXPECT_FALSE(range.intersectsNode(s2, ASSERT_NO_EXCEPTION));

  // Range encloses s2
  range.setStart(div, 2);
  range.setEnd(div, 3);
  EXPECT_FALSE(range.intersectsNode(s0, ASSERT_NO_EXCEPTION));
  EXPECT_FALSE(range.intersectsNode(s1, ASSERT_NO_EXCEPTION));
  EXPECT_TRUE(range.intersectsNode(s2, ASSERT_NO_EXCEPTION));
}

TEST_F(RangeTest, SplitTextNodeRangeWithinText) {
  V8TestingScope scope;

  GetDocument().body()->SetInnerHTMLFromString("1234");
  auto* old_text = To<Text>(GetDocument().body()->firstChild());

  auto* range04 =
      MakeGarbageCollected<Range>(GetDocument(), old_text, 0, old_text, 4);
  auto* range02 =
      MakeGarbageCollected<Range>(GetDocument(), old_text, 0, old_text, 2);
  auto* range22 =
      MakeGarbageCollected<Range>(GetDocument(), old_text, 2, old_text, 2);
  auto* range24 =
      MakeGarbageCollected<Range>(GetDocument(), old_text, 2, old_text, 4);

  old_text->splitText(2, ASSERT_NO_EXCEPTION);
  auto* new_text = To<Text>(old_text->nextSibling());

  EXPECT_TRUE(range04->BoundaryPointsValid());
  EXPECT_EQ(old_text, range04->startContainer());
  EXPECT_EQ(0u, range04->startOffset());
  EXPECT_EQ(new_text, range04->endContainer());
  EXPECT_EQ(2u, range04->endOffset());

  EXPECT_TRUE(range02->BoundaryPointsValid());
  EXPECT_EQ(old_text, range02->startContainer());
  EXPECT_EQ(0u, range02->startOffset());
  EXPECT_EQ(old_text, range02->endContainer());
  EXPECT_EQ(2u, range02->endOffset());

  // Our implementation always moves the boundary point at the separation point
  // to the end of the original text node.
  EXPECT_TRUE(range22->BoundaryPointsValid());
  EXPECT_EQ(old_text, range22->startContainer());
  EXPECT_EQ(2u, range22->startOffset());
  EXPECT_EQ(old_text, range22->endContainer());
  EXPECT_EQ(2u, range22->endOffset());

  EXPECT_TRUE(range24->BoundaryPointsValid());
  EXPECT_EQ(old_text, range24->startContainer());
  EXPECT_EQ(2u, range24->startOffset());
  EXPECT_EQ(new_text, range24->endContainer());
  EXPECT_EQ(2u, range24->endOffset());
}

TEST_F(RangeTest, SplitTextNodeRangeOutsideText) {
  V8TestingScope scope;

  GetDocument().body()->SetInnerHTMLFromString(
      "<span id=\"outer\">0<span id=\"inner-left\">1</span>SPLITME<span "
      "id=\"inner-right\">2</span>3</span>");

  Element* outer =
      GetDocument().getElementById(AtomicString::FromUTF8("outer"));
  Element* inner_left =
      GetDocument().getElementById(AtomicString::FromUTF8("inner-left"));
  Element* inner_right =
      GetDocument().getElementById(AtomicString::FromUTF8("inner-right"));
  auto* old_text = To<Text>(outer->childNodes()->item(2));

  auto* range_outer_outside =
      MakeGarbageCollected<Range>(GetDocument(), outer, 0, outer, 5);
  auto* range_outer_inside =
      MakeGarbageCollected<Range>(GetDocument(), outer, 1, outer, 4);
  auto* range_outer_surrounding_text =
      MakeGarbageCollected<Range>(GetDocument(), outer, 2, outer, 3);
  auto* range_inner_left =
      MakeGarbageCollected<Range>(GetDocument(), inner_left, 0, inner_left, 1);
  auto* range_inner_right = MakeGarbageCollected<Range>(
      GetDocument(), inner_right, 0, inner_right, 1);
  auto* range_from_text_to_middle_of_element =
      MakeGarbageCollected<Range>(GetDocument(), old_text, 6, outer, 3);

  old_text->splitText(3, ASSERT_NO_EXCEPTION);
  auto* new_text = To<Text>(old_text->nextSibling());

  EXPECT_TRUE(range_outer_outside->BoundaryPointsValid());
  EXPECT_EQ(outer, range_outer_outside->startContainer());
  EXPECT_EQ(0u, range_outer_outside->startOffset());
  EXPECT_EQ(outer, range_outer_outside->endContainer());
  EXPECT_EQ(6u,
            range_outer_outside
                ->endOffset());  // Increased by 1 since a new node is inserted.

  EXPECT_TRUE(range_outer_inside->BoundaryPointsValid());
  EXPECT_EQ(outer, range_outer_inside->startContainer());
  EXPECT_EQ(1u, range_outer_inside->startOffset());
  EXPECT_EQ(outer, range_outer_inside->endContainer());
  EXPECT_EQ(5u, range_outer_inside->endOffset());

  EXPECT_TRUE(range_outer_surrounding_text->BoundaryPointsValid());
  EXPECT_EQ(outer, range_outer_surrounding_text->startContainer());
  EXPECT_EQ(2u, range_outer_surrounding_text->startOffset());
  EXPECT_EQ(outer, range_outer_surrounding_text->endContainer());
  EXPECT_EQ(4u, range_outer_surrounding_text->endOffset());

  EXPECT_TRUE(range_inner_left->BoundaryPointsValid());
  EXPECT_EQ(inner_left, range_inner_left->startContainer());
  EXPECT_EQ(0u, range_inner_left->startOffset());
  EXPECT_EQ(inner_left, range_inner_left->endContainer());
  EXPECT_EQ(1u, range_inner_left->endOffset());

  EXPECT_TRUE(range_inner_right->BoundaryPointsValid());
  EXPECT_EQ(inner_right, range_inner_right->startContainer());
  EXPECT_EQ(0u, range_inner_right->startOffset());
  EXPECT_EQ(inner_right, range_inner_right->endContainer());
  EXPECT_EQ(1u, range_inner_right->endOffset());

  EXPECT_TRUE(range_from_text_to_middle_of_element->BoundaryPointsValid());
  EXPECT_EQ(new_text, range_from_text_to_middle_of_element->startContainer());
  EXPECT_EQ(3u, range_from_text_to_middle_of_element->startOffset());
  EXPECT_EQ(outer, range_from_text_to_middle_of_element->endContainer());
  EXPECT_EQ(4u, range_from_text_to_middle_of_element->endOffset());
}

TEST_F(RangeTest, updateOwnerDocumentIfNeeded) {
  Element* foo = GetDocument().CreateElementForBinding("foo");
  Element* bar = GetDocument().CreateElementForBinding("bar");
  foo->AppendChild(bar);

  auto* range = MakeGarbageCollected<Range>(GetDocument(), Position(bar, 0),
                                            Position(foo, 1));

  auto* another_document = MakeGarbageCollected<Document>();
  another_document->AppendChild(foo);

  EXPECT_EQ(bar, range->startContainer());
  EXPECT_EQ(0u, range->startOffset());
  EXPECT_EQ(foo, range->endContainer());
  EXPECT_EQ(1u, range->endOffset());
}

// Regression test for crbug.com/639184
TEST_F(RangeTest, NotMarkedValidByIrrelevantTextInsert) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<div><span id=span1>foo</span>bar<span id=span2>baz</span></div>");

  Element* div = GetDocument().QuerySelector("div");
  Element* span1 = GetDocument().getElementById("span1");
  Element* span2 = GetDocument().getElementById("span2");
  auto* text = To<Text>(div->childNodes()->item(1));

  auto* range = MakeGarbageCollected<Range>(GetDocument(), span2, 0, div, 3);

  div->RemoveChild(span1);
  text->insertData(0, "bar", ASSERT_NO_EXCEPTION);

  EXPECT_TRUE(range->BoundaryPointsValid());
  EXPECT_EQ(span2, range->startContainer());
  EXPECT_EQ(0u, range->startOffset());
  EXPECT_EQ(div, range->endContainer());
  EXPECT_EQ(2u, range->endOffset());
}

// Regression test for crbug.com/639184
TEST_F(RangeTest, NotMarkedValidByIrrelevantTextRemove) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<div><span id=span1>foofoo</span>bar<span id=span2>baz</span></div>");

  Element* div = GetDocument().QuerySelector("div");
  Element* span1 = GetDocument().getElementById("span1");
  Element* span2 = GetDocument().getElementById("span2");
  auto* text = To<Text>(div->childNodes()->item(1));

  auto* range = MakeGarbageCollected<Range>(GetDocument(), span2, 0, div, 3);

  div->RemoveChild(span1);
  text->deleteData(0, 3, ASSERT_NO_EXCEPTION);

  EXPECT_TRUE(range->BoundaryPointsValid());
  EXPECT_EQ(span2, range->startContainer());
  EXPECT_EQ(0u, range->startOffset());
  EXPECT_EQ(div, range->endContainer());
  EXPECT_EQ(2u, range->endOffset());
}

// Regression test for crbug.com/698123
TEST_F(RangeTest, ExpandNotCrash) {
  Range* range = Range::Create(GetDocument());
  auto* div = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  range->setStart(div, 0, ASSERT_NO_EXCEPTION);
  range->expand("", ASSERT_NO_EXCEPTION);
}

TEST_F(RangeTest, ToPosition) {
  auto& textarea = *MakeGarbageCollected<HTMLTextAreaElement>(GetDocument());
  Range& range = *Range::Create(GetDocument());
  const Position position = Position(&textarea, 0);
  range.setStart(position, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(position, range.StartPosition());
  EXPECT_EQ(position, range.EndPosition());
}

TEST_F(RangeTest, BoundingRectMustIndependentFromSelection) {
  LoadAhem();
  GetDocument().body()->SetInnerHTMLFromString(
      "<div style='font: Ahem; width: 2em;letter-spacing: 5px;'>xx xx </div>");
  Node* const div = GetDocument().QuerySelector("div");
  // "x^x
  //  x|x "
  auto* const range = MakeGarbageCollected<Range>(
      GetDocument(), div->firstChild(), 1, div->firstChild(), 4);
  const FloatRect rect_before = range->BoundingRect();
  EXPECT_GT(rect_before.Width(), 0);
  EXPECT_GT(rect_before.Height(), 0);
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(EphemeralRange(range))
          .Build());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(Selection().SelectedText(), "x x");
  const FloatRect rect_after = range->BoundingRect();
  EXPECT_EQ(rect_before, rect_after);
}

// Regression test for crbug.com/681536
TEST_F(RangeTest, BorderAndTextQuadsWithInputInBetween) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<div>foo <u><input> bar</u></div>");
  GetDocument().UpdateStyleAndLayout();

  Node* foo = GetDocument().QuerySelector("div")->firstChild();
  Node* bar = GetDocument().QuerySelector("u")->lastChild();
  auto* range = MakeGarbageCollected<Range>(GetDocument(), foo, 2, bar, 2);

  Vector<FloatQuad> quads;
  range->GetBorderAndTextQuads(quads);

  // Should get one quad for "o ", <input> and " b", respectively.
  ASSERT_EQ(3u, quads.size());
}

static Vector<FloatQuad> GetBorderAndTextQuads(const Position& start,
                                               const Position& end) {
  DCHECK_LE(start, end);
  auto* const range =
      MakeGarbageCollected<Range>(*start.GetDocument(), start, end);
  Vector<FloatQuad> quads;
  range->GetBorderAndTextQuads(quads);
  return quads;
}

static Vector<IntSize> ComputeSizesOfQuads(const Vector<FloatQuad>& quads) {
  Vector<IntSize> sizes;
  for (const auto& quad : quads)
    sizes.push_back(quad.EnclosingBoundingBox().Size());
  return sizes;
}

TEST_F(RangeTest, GetBorderAndTextQuadsWithFirstLetterOne) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <style>
      body { font-size: 20px; }
      #sample::first-letter { font-size: 500%; }
    </style>
    <p id=sample>abc</p>
    <p id=expected><span style='font-size: 500%'>a</span>bc</p>
  )HTML");
  GetDocument().UpdateStyleAndLayout();

  Element* const expected = GetDocument().getElementById("expected");
  Element* const sample = GetDocument().getElementById("sample");

  const Vector<FloatQuad> expected_quads =
      GetBorderAndTextQuads(Position(expected, 0), Position(expected, 2));
  const Vector<FloatQuad> sample_quads =
      GetBorderAndTextQuads(Position(sample, 0), Position(sample, 1));
  ASSERT_EQ(2u, sample_quads.size());
  ASSERT_EQ(3u, expected_quads.size())
      << "expected_quads has SPAN, SPAN.firstChild and P.lastChild";
  EXPECT_EQ(expected_quads[0].EnclosingBoundingBox().Size(),
            sample_quads[0].EnclosingBoundingBox().Size())
      << "Check size of first-letter part";
  EXPECT_EQ(expected_quads[2].EnclosingBoundingBox().Size(),
            sample_quads[1].EnclosingBoundingBox().Size())
      << "Check size of first-letter part";

  EXPECT_EQ(ComputeSizesOfQuads(
                GetBorderAndTextQuads(Position(expected->firstChild(), 0),
                                      Position(expected->firstChild(), 1))),
            ComputeSizesOfQuads(
                GetBorderAndTextQuads(Position(sample->firstChild(), 0),
                                      Position(sample->firstChild(), 1))))
      << "All first-letter part";

  EXPECT_EQ(ComputeSizesOfQuads(
                GetBorderAndTextQuads(Position(expected->lastChild(), 0),
                                      Position(expected->lastChild(), 2))),
            ComputeSizesOfQuads(
                GetBorderAndTextQuads(Position(sample->firstChild(), 1),
                                      Position(sample->firstChild(), 3))))
      << "All remaining part";
}

TEST_F(RangeTest, GetBorderAndTextQuadsWithFirstLetterThree) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <style>
      body { font-size: 20px; }
      #sample::first-letter { font-size: 500%; }
    </style>
    <p id=sample>(a)bc</p>
    <p id=expected><span style='font-size: 500%'>(a)</span>bc</p>
  )HTML");
  GetDocument().UpdateStyleAndLayout();

  Element* const expected = GetDocument().getElementById("expected");
  Element* const sample = GetDocument().getElementById("sample");

  const Vector<FloatQuad> expected_quads =
      GetBorderAndTextQuads(Position(expected, 0), Position(expected, 2));
  const Vector<FloatQuad> sample_quads =
      GetBorderAndTextQuads(Position(sample, 0), Position(sample, 1));
  ASSERT_EQ(2u, sample_quads.size());
  ASSERT_EQ(3u, expected_quads.size())
      << "expected_quads has SPAN, SPAN.firstChild and P.lastChild";
  EXPECT_EQ(expected_quads[0].EnclosingBoundingBox().Size(),
            sample_quads[0].EnclosingBoundingBox().Size())
      << "Check size of first-letter part";
  EXPECT_EQ(expected_quads[2].EnclosingBoundingBox().Size(),
            sample_quads[1].EnclosingBoundingBox().Size())
      << "Check size of first-letter part";

  EXPECT_EQ(ComputeSizesOfQuads(
                GetBorderAndTextQuads(Position(expected->firstChild(), 0),
                                      Position(expected->firstChild(), 1))),
            ComputeSizesOfQuads(
                GetBorderAndTextQuads(Position(sample->firstChild(), 0),
                                      Position(sample->firstChild(), 3))))
      << "All first-letter part";

  EXPECT_EQ(ComputeSizesOfQuads(
                GetBorderAndTextQuads(Position(expected->lastChild(), 0),
                                      Position(expected->lastChild(), 2))),
            ComputeSizesOfQuads(
                GetBorderAndTextQuads(Position(sample->firstChild(), 3),
                                      Position(sample->firstChild(), 5))))
      << "All remaining part";

  EXPECT_EQ(ComputeSizesOfQuads(GetBorderAndTextQuads(
                Position(expected->firstChild()->firstChild(), 1),
                Position(expected->firstChild()->firstChild(), 2))),
            ComputeSizesOfQuads(
                GetBorderAndTextQuads(Position(sample->firstChild(), 1),
                                      Position(sample->firstChild(), 2))))
      << "Partial first-letter part";

  EXPECT_EQ(ComputeSizesOfQuads(GetBorderAndTextQuads(
                Position(expected->firstChild()->firstChild(), 1),
                Position(expected->lastChild(), 1))),
            ComputeSizesOfQuads(
                GetBorderAndTextQuads(Position(sample->firstChild(), 1),
                                      Position(sample->firstChild(), 4))))
      << "Partial first-letter part and remaining part";
}

TEST_F(RangeTest, CollapsedRangeGetBorderAndTextQuadsWithFirstLetter) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <style>
      body { font-size: 20px; }
      #sample::first-letter { font-size: 500%; }
    </style>
    <p id=sample>abc</p>
    <p id=expected><span style='font-size: 500%'>a</span>bc</p>
  )HTML");
  GetDocument().UpdateStyleAndLayout();

  Element* const expected = GetDocument().getElementById("expected");
  Element* const sample = GetDocument().getElementById("sample");

  const Vector<FloatQuad> expected_quads =
      GetBorderAndTextQuads(Position(expected, 0), Position(expected, 2));
  const Vector<FloatQuad> sample_quads =
      GetBorderAndTextQuads(Position(sample, 0), Position(sample, 1));
  ASSERT_EQ(2u, sample_quads.size());
  ASSERT_EQ(3u, expected_quads.size())
      << "expected_quads has SPAN, SPAN.firstChild and P.lastChild";
  EXPECT_EQ(expected_quads[0].EnclosingBoundingBox().Size(),
            sample_quads[0].EnclosingBoundingBox().Size())
      << "Check size of first-letter part";
  EXPECT_EQ(expected_quads[2].EnclosingBoundingBox().Size(),
            sample_quads[1].EnclosingBoundingBox().Size())
      << "Check size of first-letter part";

  EXPECT_EQ(ComputeSizesOfQuads(GetBorderAndTextQuads(
                Position(expected->firstChild()->firstChild(), 0),
                Position(expected->firstChild()->firstChild(), 0))),
            ComputeSizesOfQuads(
                GetBorderAndTextQuads(Position(sample->firstChild(), 0),
                                      Position(sample->firstChild(), 0))))
      << "Collapsed range before first-letter part";

  EXPECT_EQ(ComputeSizesOfQuads(GetBorderAndTextQuads(
                Position(expected->firstChild()->firstChild(), 1),
                Position(expected->firstChild()->firstChild(), 1))),
            ComputeSizesOfQuads(
                GetBorderAndTextQuads(Position(sample->firstChild(), 1),
                                      Position(sample->firstChild(), 1))))
      << "Collapsed range after first-letter part";

  EXPECT_EQ(ComputeSizesOfQuads(GetBorderAndTextQuads(
                Position(expected->firstChild()->nextSibling(), 1),
                Position(expected->firstChild()->nextSibling(), 1))),
            ComputeSizesOfQuads(
                GetBorderAndTextQuads(Position(sample->firstChild(), 2),
                                      Position(sample->firstChild(), 2))))
      << "Collapsed range in remaining text part";
}

}  // namespace blink
