// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/static_range.h"

#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_list.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class StaticRangeTest : public testing::Test {
 protected:
  void SetUp() override;

  HTMLDocument& GetDocument() const;

 private:
  Persistent<HTMLDocument> document_;
};

void StaticRangeTest::SetUp() {
  document_ = MakeGarbageCollected<HTMLDocument>();
  auto* html = MakeGarbageCollected<HTMLHtmlElement>(*document_);
  html->AppendChild(MakeGarbageCollected<HTMLBodyElement>(*document_));
  document_->AppendChild(html);
}

HTMLDocument& StaticRangeTest::GetDocument() const {
  return *document_;
}

TEST_F(StaticRangeTest, SplitTextNodeRangeWithinText) {
  V8TestingScope scope;
  GetDocument().body()->SetInnerHTMLFromString("1234");
  auto* old_text = To<Text>(GetDocument().body()->firstChild());

  auto* static_range04 = MakeGarbageCollected<StaticRange>(
      GetDocument(), old_text, 0u, old_text, 4u);
  auto* static_range02 = MakeGarbageCollected<StaticRange>(
      GetDocument(), old_text, 0u, old_text, 2u);
  auto* static_range22 = MakeGarbageCollected<StaticRange>(
      GetDocument(), old_text, 2u, old_text, 2u);
  auto* static_range24 = MakeGarbageCollected<StaticRange>(
      GetDocument(), old_text, 2u, old_text, 4u);

  Range* range04 = static_range04->toRange(ASSERT_NO_EXCEPTION);
  Range* range02 = static_range02->toRange(ASSERT_NO_EXCEPTION);
  Range* range22 = static_range22->toRange(ASSERT_NO_EXCEPTION);
  Range* range24 = static_range24->toRange(ASSERT_NO_EXCEPTION);

  old_text->splitText(2, ASSERT_NO_EXCEPTION);
  auto* new_text = To<Text>(old_text->nextSibling());

  // Range should mutate.
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

  // StaticRange shouldn't mutate.
  EXPECT_EQ(old_text, static_range04->startContainer());
  EXPECT_EQ(0u, static_range04->startOffset());
  EXPECT_EQ(old_text, static_range04->endContainer());
  EXPECT_EQ(4u, static_range04->endOffset());

  EXPECT_EQ(old_text, static_range02->startContainer());
  EXPECT_EQ(0u, static_range02->startOffset());
  EXPECT_EQ(old_text, static_range02->endContainer());
  EXPECT_EQ(2u, static_range02->endOffset());

  EXPECT_EQ(old_text, static_range22->startContainer());
  EXPECT_EQ(2u, static_range22->startOffset());
  EXPECT_EQ(old_text, static_range22->endContainer());
  EXPECT_EQ(2u, static_range22->endOffset());

  EXPECT_EQ(old_text, static_range24->startContainer());
  EXPECT_EQ(2u, static_range24->startOffset());
  EXPECT_EQ(old_text, static_range24->endContainer());
  EXPECT_EQ(4u, static_range24->endOffset());
}

TEST_F(StaticRangeTest, SplitTextNodeRangeOutsideText) {
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

  auto* static_range_outer_outside =
      MakeGarbageCollected<StaticRange>(GetDocument(), outer, 0u, outer, 5u);
  auto* static_range_outer_inside =
      MakeGarbageCollected<StaticRange>(GetDocument(), outer, 1u, outer, 4u);
  auto* static_range_outer_surrounding_text =
      MakeGarbageCollected<StaticRange>(GetDocument(), outer, 2u, outer, 3u);
  auto* static_range_inner_left = MakeGarbageCollected<StaticRange>(
      GetDocument(), inner_left, 0u, inner_left, 1u);
  auto* static_range_inner_right = MakeGarbageCollected<StaticRange>(
      GetDocument(), inner_right, 0u, inner_right, 1u);
  auto* static_range_from_text_to_middle_of_element =
      MakeGarbageCollected<StaticRange>(GetDocument(), old_text, 6u, outer, 3u);

  Range* range_outer_outside =
      static_range_outer_outside->toRange(ASSERT_NO_EXCEPTION);
  Range* range_outer_inside =
      static_range_outer_inside->toRange(ASSERT_NO_EXCEPTION);
  Range* range_outer_surrounding_text =
      static_range_outer_surrounding_text->toRange(ASSERT_NO_EXCEPTION);
  Range* range_inner_left =
      static_range_inner_left->toRange(ASSERT_NO_EXCEPTION);
  Range* range_inner_right =
      static_range_inner_right->toRange(ASSERT_NO_EXCEPTION);
  Range* range_from_text_to_middle_of_element =
      static_range_from_text_to_middle_of_element->toRange(ASSERT_NO_EXCEPTION);

  old_text->splitText(3, ASSERT_NO_EXCEPTION);
  auto* new_text = To<Text>(old_text->nextSibling());

  // Range should mutate.
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

  // StaticRange shouldn't mutate.
  EXPECT_EQ(outer, static_range_outer_outside->startContainer());
  EXPECT_EQ(0u, static_range_outer_outside->startOffset());
  EXPECT_EQ(outer, static_range_outer_outside->endContainer());
  EXPECT_EQ(5u, static_range_outer_outside->endOffset());

  EXPECT_EQ(outer, static_range_outer_inside->startContainer());
  EXPECT_EQ(1u, static_range_outer_inside->startOffset());
  EXPECT_EQ(outer, static_range_outer_inside->endContainer());
  EXPECT_EQ(4u, static_range_outer_inside->endOffset());

  EXPECT_EQ(outer, static_range_outer_surrounding_text->startContainer());
  EXPECT_EQ(2u, static_range_outer_surrounding_text->startOffset());
  EXPECT_EQ(outer, static_range_outer_surrounding_text->endContainer());
  EXPECT_EQ(3u, static_range_outer_surrounding_text->endOffset());

  EXPECT_EQ(inner_left, static_range_inner_left->startContainer());
  EXPECT_EQ(0u, static_range_inner_left->startOffset());
  EXPECT_EQ(inner_left, static_range_inner_left->endContainer());
  EXPECT_EQ(1u, static_range_inner_left->endOffset());

  EXPECT_EQ(inner_right, static_range_inner_right->startContainer());
  EXPECT_EQ(0u, static_range_inner_right->startOffset());
  EXPECT_EQ(inner_right, static_range_inner_right->endContainer());
  EXPECT_EQ(1u, static_range_inner_right->endOffset());

  EXPECT_EQ(old_text,
            static_range_from_text_to_middle_of_element->startContainer());
  EXPECT_EQ(6u, static_range_from_text_to_middle_of_element->startOffset());
  EXPECT_EQ(outer, static_range_from_text_to_middle_of_element->endContainer());
  EXPECT_EQ(3u, static_range_from_text_to_middle_of_element->endOffset());
}

TEST_F(StaticRangeTest, InvalidToRange) {
  V8TestingScope scope;
  GetDocument().body()->SetInnerHTMLFromString("1234");
  auto* old_text = To<Text>(GetDocument().body()->firstChild());

  auto* static_range04 = MakeGarbageCollected<StaticRange>(
      GetDocument(), old_text, 0u, old_text, 4u);

  // Valid StaticRange.
  static_range04->toRange(ASSERT_NO_EXCEPTION);

  old_text->splitText(2, ASSERT_NO_EXCEPTION);
  // StaticRange shouldn't mutate, endOffset() become invalid after splitText().
  EXPECT_EQ(old_text, static_range04->startContainer());
  EXPECT_EQ(0u, static_range04->startOffset());
  EXPECT_EQ(old_text, static_range04->endContainer());
  EXPECT_EQ(4u, static_range04->endOffset());

  // Invalid StaticRange.
  DummyExceptionStateForTesting exception_state;
  static_range04->toRange(exception_state);
  EXPECT_TRUE(exception_state.HadException());
}

}  // namespace blink
