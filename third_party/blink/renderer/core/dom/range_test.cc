// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/range.h"

#include "base/memory/scoped_refptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_font_face_descriptors.h"
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
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {

using ::testing::ElementsAre;

class RangeTest : public EditingTestBase {};

TEST_F(RangeTest, extractContentsWithDOMMutationEvent) {
  if (!RuntimeEnabledFeatures::MutationEventsEnabledByRuntimeFlag()) {
    // TODO(crbug.com/1446498) Remove this test when MutationEvents are disabled
    // for good. This is just a test of `DOMSubtreeModified` and ranges.
    return;
  }
  GetDocument().body()->setInnerHTML("<span><b>abc</b>def</span>");
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

  Element* const span_element =
      GetDocument().QuerySelector(AtomicString("span"));
  auto* const range = MakeGarbageCollected<Range>(GetDocument(), span_element,
                                                  0, span_element, 1);
  Element* const result = GetDocument().CreateRawElement(html_names::kDivTag);
  result->AppendChild(range->extractContents(ASSERT_NO_EXCEPTION));

  EXPECT_EQ("<b>abc</b>", result->innerHTML())
      << "DOM mutation event handler should not affect result.";
  EXPECT_EQ("<span>DEF</span>", span_element->outerHTML())
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
  Element* const div = GetDocument().QuerySelector(AtomicString("div"));
  Element* const s0 = GetDocument().getElementById(AtomicString("s0"));
  Element* const s1 = GetDocument().getElementById(AtomicString("s1"));
  Element* const s2 = GetDocument().getElementById(AtomicString("s2"));
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

  GetDocument().body()->setInnerHTML("1234");
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

  GetDocument().body()->setInnerHTML(
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
  Element* foo = GetDocument().CreateElementForBinding(AtomicString("foo"));
  Element* bar = GetDocument().CreateElementForBinding(AtomicString("bar"));
  foo->AppendChild(bar);

  auto* range = MakeGarbageCollected<Range>(GetDocument(), Position(bar, 0),
                                            Position(foo, 1));

  ScopedNullExecutionContext execution_context;
  auto* another_document =
      Document::CreateForTest(execution_context.GetExecutionContext());
  another_document->AppendChild(foo);

  EXPECT_EQ(bar, range->startContainer());
  EXPECT_EQ(0u, range->startOffset());
  EXPECT_EQ(foo, range->endContainer());
  EXPECT_EQ(1u, range->endOffset());
}

// Regression test for crbug.com/639184
TEST_F(RangeTest, NotMarkedValidByIrrelevantTextInsert) {
  GetDocument().body()->setInnerHTML(
      "<div><span id=span1>foo</span>bar<span id=span2>baz</span></div>");

  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  Element* span1 = GetDocument().getElementById(AtomicString("span1"));
  Element* span2 = GetDocument().getElementById(AtomicString("span2"));
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
  GetDocument().body()->setInnerHTML(
      "<div><span id=span1>foofoo</span>bar<span id=span2>baz</span></div>");

  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  Element* span1 = GetDocument().getElementById(AtomicString("span1"));
  Element* span2 = GetDocument().getElementById(AtomicString("span2"));
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
  GetDocument().body()->setInnerHTML(
      "<div style='font: Ahem; width: 2em;letter-spacing: 5px;'>xx xx </div>");
  Node* const div = GetDocument().QuerySelector(AtomicString("div"));
  // "x^x
  //  x|x "
  auto* const range = MakeGarbageCollected<Range>(
      GetDocument(), div->firstChild(), 1, div->firstChild(), 4);
  const gfx::RectF rect_before = range->BoundingRect();
  EXPECT_GT(rect_before.width(), 0);
  EXPECT_GT(rect_before.height(), 0);
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .SetBaseAndExtent(EphemeralRange(range))
                               .Build(),
                           SetSelectionOptions());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(Selection().SelectedText(), "x x");
  const gfx::RectF rect_after = range->BoundingRect();
  EXPECT_EQ(rect_before, rect_after);
}

// Regression test for crbug.com/681536
TEST_F(RangeTest, BorderAndTextQuadsWithInputInBetween) {
  GetDocument().body()->setInnerHTML("<div>foo <u><input> bar</u></div>");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  Node* foo = GetDocument().QuerySelector(AtomicString("div"))->firstChild();
  Node* bar = GetDocument().QuerySelector(AtomicString("u"))->lastChild();
  auto* range = MakeGarbageCollected<Range>(GetDocument(), foo, 2, bar, 2);

  Vector<gfx::QuadF> quads;
  range->GetBorderAndTextQuads(quads);

  // Should get one quad for "o ", <input> and " b", respectively.
  ASSERT_EQ(3u, quads.size());
}

static Vector<gfx::QuadF> GetBorderAndTextQuads(const Position& start,
                                                const Position& end) {
  DCHECK_LE(start, end);
  auto* const range =
      MakeGarbageCollected<Range>(*start.GetDocument(), start, end);
  Vector<gfx::QuadF> quads;
  range->GetBorderAndTextQuads(quads);
  return quads;
}

static Vector<gfx::Size> ComputeSizesOfQuads(const Vector<gfx::QuadF>& quads) {
  Vector<gfx::Size> sizes;
  for (const auto& quad : quads)
    sizes.push_back(gfx::ToEnclosingRect(quad.BoundingBox()).size());
  return sizes;
}

// http://crbug.com/1240510
TEST_F(RangeTest, GetBorderAndTextQuadsWithCombinedText) {
  LoadAhem();
  InsertStyleElement(
      "body { font: 20px/25px Ahem; margin: 0px; }"
      "#sample { writing-mode: vertical-rl; }"
      "c { text-combine-upright: all; }");
  SetBodyInnerHTML(
      "<div id=sample>"
      "<c id=c1>M</c><c id=c2>MM</c><c id=c3>MMM</c><c id=c4>MMMM</c>"
      "</div>");
  const Text& text1 = *To<Text>(GetElementById("c1")->firstChild());
  const Text& text2 = *To<Text>(GetElementById("c2")->firstChild());
  const Text& text3 = *To<Text>(GetElementById("c3")->firstChild());
  const Text& text4 = *To<Text>(GetElementById("c4")->firstChild());

  EXPECT_THAT(GetBorderAndTextQuads(Position(text1, 0), Position(text1, 1)),
              ElementsAre(gfx::QuadF(gfx::RectF(3, 0, 20, 20))));
  EXPECT_THAT(GetBorderAndTextQuads(Position(text2, 0), Position(text2, 2)),
              ElementsAre(gfx::QuadF(gfx::RectF(2, 20, 22, 20))));
  EXPECT_THAT(GetBorderAndTextQuads(Position(text3, 0), Position(text3, 3)),
              ElementsAre(gfx::QuadF(gfx::RectF(2, 40, 22, 20))));
  EXPECT_THAT(GetBorderAndTextQuads(Position(text4, 0), Position(text4, 4)),
              ElementsAre(gfx::QuadF(gfx::RectF(2, 60, 22, 20))));
}

TEST_F(RangeTest, GetBorderAndTextQuadsWithFirstLetterOne) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body { font-size: 20px; }
      #sample::first-letter { font-size: 500%; }
    </style>
    <p id=sample>abc</p>
    <p id=expected><span style='font-size: 500%'>a</span>bc</p>
  )HTML");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  Element* const expected =
      GetDocument().getElementById(AtomicString("expected"));
  Element* const sample = GetDocument().getElementById(AtomicString("sample"));

  const Vector<gfx::QuadF> expected_quads =
      GetBorderAndTextQuads(Position(expected, 0), Position(expected, 2));
  const Vector<gfx::QuadF> sample_quads =
      GetBorderAndTextQuads(Position(sample, 0), Position(sample, 1));
  ASSERT_EQ(2u, sample_quads.size());
  ASSERT_EQ(3u, expected_quads.size())
      << "expected_quads has SPAN, SPAN.firstChild and P.lastChild";
  EXPECT_EQ(gfx::ToEnclosingRect(expected_quads[0].BoundingBox()).size(),
            gfx::ToEnclosingRect(sample_quads[0].BoundingBox()).size())
      << "Check size of first-letter part";
  EXPECT_EQ(gfx::ToEnclosingRect(expected_quads[2].BoundingBox()).size(),
            gfx::ToEnclosingRect(sample_quads[1].BoundingBox()).size())
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
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body { font-size: 20px; }
      #sample::first-letter { font-size: 500%; }
    </style>
    <p id=sample>(a)bc</p>
    <p id=expected><span style='font-size: 500%'>(a)</span>bc</p>
  )HTML");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  Element* const expected =
      GetDocument().getElementById(AtomicString("expected"));
  Element* const sample = GetDocument().getElementById(AtomicString("sample"));

  const Vector<gfx::QuadF> expected_quads =
      GetBorderAndTextQuads(Position(expected, 0), Position(expected, 2));
  const Vector<gfx::QuadF> sample_quads =
      GetBorderAndTextQuads(Position(sample, 0), Position(sample, 1));
  ASSERT_EQ(2u, sample_quads.size());
  ASSERT_EQ(3u, expected_quads.size())
      << "expected_quads has SPAN, SPAN.firstChild and P.lastChild";
  EXPECT_EQ(gfx::ToEnclosingRect(expected_quads[0].BoundingBox()).size(),
            gfx::ToEnclosingRect(sample_quads[0].BoundingBox()).size())
      << "Check size of first-letter part";
  EXPECT_EQ(gfx::ToEnclosingRect(expected_quads[2].BoundingBox()).size(),
            gfx::ToEnclosingRect(sample_quads[1].BoundingBox()).size())
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
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body { font-size: 20px; }
      #sample::first-letter { font-size: 500%; }
    </style>
    <p id=sample>abc</p>
    <p id=expected><span style='font-size: 500%'>a</span>bc</p>
  )HTML");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  Element* const expected =
      GetDocument().getElementById(AtomicString("expected"));
  Element* const sample = GetDocument().getElementById(AtomicString("sample"));

  const Vector<gfx::QuadF> expected_quads =
      GetBorderAndTextQuads(Position(expected, 0), Position(expected, 2));
  const Vector<gfx::QuadF> sample_quads =
      GetBorderAndTextQuads(Position(sample, 0), Position(sample, 1));
  ASSERT_EQ(2u, sample_quads.size());
  ASSERT_EQ(3u, expected_quads.size())
      << "expected_quads has SPAN, SPAN.firstChild and P.lastChild";
  EXPECT_EQ(gfx::ToEnclosingRect(expected_quads[0].BoundingBox()).size(),
            gfx::ToEnclosingRect(sample_quads[0].BoundingBox()).size())
      << "Check size of first-letter part";
  EXPECT_EQ(gfx::ToEnclosingRect(expected_quads[2].BoundingBox()).size(),
            gfx::ToEnclosingRect(sample_quads[1].BoundingBox()).size())
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

TEST_F(RangeTest, ContainerNodeRemoval) {
  GetDocument().body()->setInnerHTML("<p>aaaa</p><p>bbbbbb</p>");
  auto* node_a = GetDocument().body()->firstChild();
  auto* node_b = node_a->nextSibling();
  auto* text_a = To<Text>(node_a->firstChild());
  auto* text_b = To<Text>(node_b->firstChild());

  auto* rangea0a2 =
      MakeGarbageCollected<Range>(GetDocument(), text_a, 0, text_a, 2);
  auto* rangea2a4 =
      MakeGarbageCollected<Range>(GetDocument(), text_a, 2, text_a, 4);
  auto* rangea2b2 =
      MakeGarbageCollected<Range>(GetDocument(), text_a, 0, text_b, 2);
  auto* rangeb2b6 =
      MakeGarbageCollected<Range>(GetDocument(), text_b, 2, text_b, 6);

  // remove children in node_a
  node_a->setTextContent("");

  EXPECT_TRUE(rangea0a2->BoundaryPointsValid());
  EXPECT_EQ(node_a, rangea0a2->startContainer());
  EXPECT_EQ(0u, rangea0a2->startOffset());
  EXPECT_EQ(node_a, rangea0a2->endContainer());
  EXPECT_EQ(0u, rangea0a2->endOffset());

  EXPECT_TRUE(rangea2a4->BoundaryPointsValid());
  EXPECT_EQ(node_a, rangea2a4->startContainer());
  EXPECT_EQ(0u, rangea2a4->startOffset());
  EXPECT_EQ(node_a, rangea2a4->endContainer());
  EXPECT_EQ(0u, rangea2a4->endOffset());

  EXPECT_TRUE(rangea2b2->BoundaryPointsValid());
  EXPECT_EQ(node_a, rangea2b2->startContainer());
  EXPECT_EQ(0u, rangea2b2->startOffset());
  EXPECT_EQ(text_b, rangea2b2->endContainer());
  EXPECT_EQ(2u, rangea2b2->endOffset());

  EXPECT_TRUE(rangeb2b6->BoundaryPointsValid());
  EXPECT_EQ(text_b, rangeb2b6->startContainer());
  EXPECT_EQ(2u, rangeb2b6->startOffset());
  EXPECT_EQ(text_b, rangeb2b6->endContainer());
  EXPECT_EQ(6u, rangeb2b6->endOffset());

  // remove children in body.
  GetDocument().body()->setTextContent("");

  EXPECT_TRUE(rangea0a2->BoundaryPointsValid());
  EXPECT_EQ(GetDocument().body(), rangea0a2->startContainer());
  EXPECT_EQ(0u, rangea0a2->startOffset());
  EXPECT_EQ(GetDocument().body(), rangea0a2->endContainer());
  EXPECT_EQ(0u, rangea0a2->endOffset());

  EXPECT_TRUE(rangea2a4->BoundaryPointsValid());
  EXPECT_EQ(GetDocument().body(), rangea2a4->startContainer());
  EXPECT_EQ(0u, rangea2a4->startOffset());
  EXPECT_EQ(GetDocument().body(), rangea2a4->endContainer());
  EXPECT_EQ(0u, rangea2a4->endOffset());

  EXPECT_TRUE(rangea2b2->BoundaryPointsValid());
  EXPECT_EQ(GetDocument().body(), rangea2b2->startContainer());
  EXPECT_EQ(0u, rangea2b2->startOffset());
  EXPECT_EQ(GetDocument().body(), rangea2b2->endContainer());
  EXPECT_EQ(0u, rangea2b2->endOffset());

  EXPECT_TRUE(rangeb2b6->BoundaryPointsValid());
  EXPECT_EQ(GetDocument().body(), rangeb2b6->startContainer());
  EXPECT_EQ(0u, rangeb2b6->startOffset());
  EXPECT_EQ(GetDocument().body(), rangeb2b6->endContainer());
  EXPECT_EQ(0u, rangeb2b6->endOffset());
}

TEST_F(RangeTest,
       ContainerNodeRemovalWithSequentialFocusNavigationStartingPoint) {
  SetBodyContent("<input value='text inside input'>");
  const auto& input =
      ToTextControl(*GetDocument().QuerySelector(AtomicString("input")));
  Node* text_inside_input = input.InnerEditorElement()->firstChild();
  GetDocument().SetSequentialFocusNavigationStartingPoint(text_inside_input);

  // Remove children in body.
  GetDocument().body()->setTextContent("");

  Range* sequential_focus_navigation_starting_point =
      GetDocument().sequential_focus_navigation_starting_point_;

  EXPECT_TRUE(
      sequential_focus_navigation_starting_point->BoundaryPointsValid());
  EXPECT_EQ(GetDocument().body(),
            sequential_focus_navigation_starting_point->startContainer());
  EXPECT_EQ(0u, sequential_focus_navigation_starting_point->startOffset());
  EXPECT_EQ(GetDocument().body(),
            sequential_focus_navigation_starting_point->endContainer());
  EXPECT_EQ(0u, sequential_focus_navigation_starting_point->endOffset());
}

}  // namespace blink
