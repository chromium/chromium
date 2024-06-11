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

#include "third_party/blink/renderer/core/editing/iterators/character_iterator.h"

#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class CharacterIteratorTest : public EditingTestBase {};

TEST_F(CharacterIteratorTest, SubrangeWithReplacedElements) {
  static const char* body_content =
      "<div id='div' contenteditable='true'>1<img src='foo.png'>345</div>";
  SetBodyContent(body_content);
  UpdateAllLifecyclePhasesForTest();

  Node* div_node = GetDocument().getElementById(AtomicString("div"));
  auto* entire_range =
      MakeGarbageCollected<Range>(GetDocument(), div_node, 0, div_node, 3);

  EphemeralRange result =
      CalculateCharacterSubrange(EphemeralRange(entire_range), 2, 3);
  Node* text_node = div_node->lastChild();
  EXPECT_EQ(Position(text_node, 0), result.StartPosition());
  EXPECT_EQ(Position(text_node, 3), result.EndPosition());
}

TEST_F(CharacterIteratorTest, CollapsedSubrange) {
  static const char* body_content =
      "<div id='div' contenteditable='true'>hello</div>";
  SetBodyContent(body_content);
  UpdateAllLifecyclePhasesForTest();

  Node* text_node =
      GetDocument().getElementById(AtomicString("div"))->lastChild();
  auto* entire_range =
      MakeGarbageCollected<Range>(GetDocument(), text_node, 1, text_node, 4);
  EXPECT_EQ(1u, entire_range->startOffset());
  EXPECT_EQ(4u, entire_range->endOffset());

  const EphemeralRange& result =
      CalculateCharacterSubrange(EphemeralRange(entire_range), 2, 0);
  EXPECT_EQ(Position(text_node, 3), result.StartPosition());
  EXPECT_EQ(Position(text_node, 3), result.EndPosition());
}

TEST_F(CharacterIteratorTest, GetPositionWithBlock) {
  SetBodyContent("a<div>b</div>c");

  const Element& body = *GetDocument().body();
  CharacterIterator it(EphemeralRange::RangeOfContents(body));

  const Node& text_a = *body.firstChild();
  const Node& div = *text_a.nextSibling();
  const Node& text_b = *div.firstChild();
  const Node& text_c = *body.lastChild();

  EXPECT_EQ(Position(text_a, 0), it.GetPositionBefore());
  EXPECT_EQ(Position(text_a, 1), it.GetPositionAfter());
  EXPECT_EQ(Position(text_a, 0), it.StartPosition());
  EXPECT_EQ(Position(text_a, 1), it.EndPosition());

  ASSERT_FALSE(it.AtEnd());
  it.Advance(1);
  EXPECT_EQ(Position::BeforeNode(div), it.GetPositionBefore());
  EXPECT_EQ(Position::BeforeNode(div), it.GetPositionAfter());
  EXPECT_EQ(Position(body, 1), it.StartPosition());
  EXPECT_EQ(Position(body, 1), it.EndPosition());

  ASSERT_FALSE(it.AtEnd());
  it.Advance(1);
  EXPECT_EQ(Position(text_b, 0), it.GetPositionBefore());
  EXPECT_EQ(Position(text_b, 1), it.GetPositionAfter());
  EXPECT_EQ(Position(text_b, 0), it.StartPosition());
  EXPECT_EQ(Position(text_b, 1), it.EndPosition());

  ASSERT_FALSE(it.AtEnd());
  it.Advance(1);
  EXPECT_EQ(Position(text_b, 1), it.GetPositionBefore());
  EXPECT_EQ(Position(text_b, 1), it.GetPositionAfter());
  EXPECT_EQ(Position(div, 1), it.StartPosition());
  EXPECT_EQ(Position(div, 1), it.EndPosition());

  ASSERT_FALSE(it.AtEnd());
  it.Advance(1);
  EXPECT_EQ(Position(text_c, 0), it.GetPositionBefore());
  EXPECT_EQ(Position(text_c, 1), it.GetPositionAfter());
  EXPECT_EQ(Position(text_c, 0), it.StartPosition());
  EXPECT_EQ(Position(text_c, 1), it.EndPosition());

  ASSERT_FALSE(it.AtEnd());
  it.Advance(1);
  EXPECT_EQ(Position(body, 3), it.GetPositionBefore());
  EXPECT_EQ(Position(body, 3), it.GetPositionAfter());
  EXPECT_EQ(Position(body, 3), it.StartPosition());
  EXPECT_EQ(Position(body, 3), it.EndPosition());

  EXPECT_TRUE(it.AtEnd());
}

TEST_F(CharacterIteratorTest, GetPositionWithBlocks) {
  SetBodyContent("<p id=a>b</p><p id=c>d</p>");

  const Element& body = *GetDocument().body();
  CharacterIterator it(EphemeralRange::RangeOfContents(body));

  const Node& element_p_a = *GetDocument().getElementById(AtomicString("a"));
  const Node& text_b = *element_p_a.firstChild();
  const Node& element_p_c = *GetDocument().getElementById(AtomicString("c"));
  const Node& text_d = *element_p_c.firstChild();

  EXPECT_EQ(Position(text_b, 0), it.GetPositionBefore());
  EXPECT_EQ(Position(text_b, 1), it.GetPositionAfter());
  EXPECT_EQ(Position(text_b, 0), it.StartPosition());
  EXPECT_EQ(Position(text_b, 1), it.EndPosition());

  ASSERT_FALSE(it.AtEnd());
  it.Advance(1);
  EXPECT_EQ(Position(text_b, 1), it.GetPositionBefore());
  EXPECT_EQ(Position(text_b, 1), it.GetPositionAfter());
  EXPECT_EQ(Position(element_p_a, 1), it.StartPosition());
  EXPECT_EQ(Position(element_p_a, 1), it.EndPosition());

  ASSERT_FALSE(it.AtEnd());
  it.Advance(1);
  EXPECT_EQ(Position(text_b, 1), it.GetPositionBefore());
  EXPECT_EQ(Position(text_b, 1), it.GetPositionAfter());
  EXPECT_EQ(Position(element_p_a, 1), it.StartPosition());
  EXPECT_EQ(Position(element_p_a, 1), it.EndPosition());

  ASSERT_FALSE(it.AtEnd());
  it.Advance(1);
  EXPECT_EQ(Position(text_d, 0), it.GetPositionBefore());
  EXPECT_EQ(Position(text_d, 1), it.GetPositionAfter());
  EXPECT_EQ(Position(text_d, 0), it.StartPosition());
  EXPECT_EQ(Position(text_d, 1), it.EndPosition());

  ASSERT_FALSE(it.AtEnd());
  it.Advance(1);
  EXPECT_EQ(Position(body, 2), it.GetPositionBefore());
  EXPECT_EQ(Position(body, 2), it.GetPositionAfter());
  EXPECT_EQ(Position(body, 2), it.StartPosition());
  EXPECT_EQ(Position(body, 2), it.EndPosition());

  EXPECT_TRUE(it.AtEnd());
}

TEST_F(CharacterIteratorTest, GetPositionWithBR) {
  SetBodyContent("a<br>b");

  const Element& body = *GetDocument().body();
  CharacterIterator it(EphemeralRange::RangeOfContents(body));

  const Node& text_a = *body.firstChild();
  const Node& br = *GetDocument().QuerySelector(AtomicString("br"));
  const Node& text_b = *body.lastChild();

  EXPECT_EQ(Position(text_a, 0), it.GetPositionBefore());
  EXPECT_EQ(Position(text_a, 1), it.GetPositionAfter());
  EXPECT_EQ(Position(text_a, 0), it.StartPosition());
  EXPECT_EQ(Position(text_a, 1), it.EndPosition());

  ASSERT_FALSE(it.AtEnd());
  it.Advance(1);
  EXPECT_EQ(Position::BeforeNode(br), it.GetPositionBefore());
  EXPECT_EQ(Position::AfterNode(br), it.GetPositionAfter());
  EXPECT_EQ(Position(body, 1), it.StartPosition());
  EXPECT_EQ(Position(body, 2), it.EndPosition());

  ASSERT_FALSE(it.AtEnd());
  it.Advance(1);
  EXPECT_EQ(Position(text_b, 0), it.GetPositionBefore());
  EXPECT_EQ(Position(text_b, 1), it.GetPositionAfter());
  EXPECT_EQ(Position(text_b, 0), it.StartPosition());
  EXPECT_EQ(Position(text_b, 1), it.EndPosition());

  ASSERT_FALSE(it.AtEnd());
  it.Advance(1);
  EXPECT_EQ(Position(body, 3), it.GetPositionBefore());
  EXPECT_EQ(Position(body, 3), it.GetPositionAfter());
  EXPECT_EQ(Position(body, 3), it.StartPosition());
  EXPECT_EQ(Position(body, 3), it.EndPosition());

  EXPECT_TRUE(it.AtEnd());
}

TEST_F(CharacterIteratorTest, GetPositionWithCollapsedWhitespaces) {
  SetBodyContent("a <div> b </div> c");

  const Element& body = *GetDocument().body();
  CharacterIterator it(EphemeralRange::RangeOfContents(body));

  const Node& text_a = *body.firstChild();
  const Node& div = *text_a.nextSibling();
  const Node& text_b = *div.firstChild();
  const Node& text_c = *body.lastChild();

  EXPECT_EQ(Position(text_a, 0), it.GetPositionBefore());
  EXPECT_EQ(Position(text_a, 1), it.GetPositionAfter());
  EXPECT_EQ(Position(text_a, 0), it.StartPosition());
  EXPECT_EQ(Position(text_a, 1), it.EndPosition());

  ASSERT_FALSE(it.AtEnd());
  it.Advance(1);
  EXPECT_EQ(Position::BeforeNode(div), it.GetPositionBefore());
  EXPECT_EQ(Position::BeforeNode(div), it.GetPositionAfter());
  EXPECT_EQ(Position(body, 1), it.StartPosition());
  EXPECT_EQ(Position(body, 1), it.EndPosition());

  ASSERT_FALSE(it.AtEnd());
  it.Advance(1);
  EXPECT_EQ(Position(text_b, 1), it.GetPositionBefore());
  EXPECT_EQ(Position(text_b, 2), it.GetPositionAfter());
  EXPECT_EQ(Position(text_b, 1), it.StartPosition());
  EXPECT_EQ(Position(text_b, 2), it.EndPosition());

  ASSERT_FALSE(it.AtEnd());
  it.Advance(1);
  EXPECT_EQ(Position(text_b, 3), it.GetPositionBefore());
  EXPECT_EQ(Position(text_b, 3), it.GetPositionAfter());
  EXPECT_EQ(Position(div, 1), it.StartPosition());
  EXPECT_EQ(Position(div, 1), it.EndPosition());

  ASSERT_FALSE(it.AtEnd());
  it.Advance(1);
  EXPECT_EQ(Position(text_c, 1), it.GetPositionBefore());
  EXPECT_EQ(Position(text_c, 2), it.GetPositionAfter());
  EXPECT_EQ(Position(text_c, 1), it.StartPosition());
  EXPECT_EQ(Position(text_c, 2), it.EndPosition());

  ASSERT_FALSE(it.AtEnd());
  it.Advance(1);
  EXPECT_EQ(Position(body, 3), it.GetPositionBefore());
  EXPECT_EQ(Position(body, 3), it.GetPositionAfter());
  EXPECT_EQ(Position(body, 3), it.StartPosition());
  EXPECT_EQ(Position(body, 3), it.EndPosition());

  EXPECT_TRUE(it.AtEnd());
}

TEST_F(CharacterIteratorTest, GetPositionWithEmitChar16Before) {
  InsertStyleElement("b { white-space: pre; }");
  SetBodyContent("a   <b> c</b>");

  const Element& body = *GetDocument().body();
  CharacterIterator it(EphemeralRange::RangeOfContents(body));

  const Node& text_a = *body.firstChild();
  const Node& element_b = *text_a.nextSibling();
  const Node& text_c = *element_b.firstChild();

  EXPECT_EQ(Position(text_a, 0), it.GetPositionBefore());
  EXPECT_EQ(Position(text_a, 1), it.GetPositionAfter());
  EXPECT_EQ(Position(text_a, 0), it.StartPosition());
  EXPECT_EQ(Position(text_a, 1), it.EndPosition());

  ASSERT_FALSE(it.AtEnd());
  it.Advance(1);
  EXPECT_EQ(Position(text_a, 1), it.GetPositionBefore());
  EXPECT_EQ(Position(text_a, 2), it.GetPositionAfter());
  EXPECT_EQ(Position(text_a, 1), it.StartPosition());
  EXPECT_EQ(Position(text_a, 2), it.EndPosition());

  ASSERT_FALSE(it.AtEnd());
  it.Advance(1);
  EXPECT_EQ(Position(text_c, 0), it.GetPositionBefore());
  EXPECT_EQ(Position(text_c, 1), it.GetPositionAfter());
  EXPECT_EQ(Position(text_c, 0), it.StartPosition());
  EXPECT_EQ(Position(text_c, 1), it.EndPosition());

  ASSERT_FALSE(it.AtEnd());
  it.Advance(1);
  EXPECT_EQ(Position(text_c, 1), it.GetPositionBefore());
  EXPECT_EQ(Position(text_c, 2), it.GetPositionAfter());
  EXPECT_EQ(Position(text_c, 1), it.StartPosition());
  EXPECT_EQ(Position(text_c, 2), it.EndPosition());

  ASSERT_FALSE(it.AtEnd());
  it.Advance(1);
  EXPECT_EQ(Position(body, 2), it.GetPositionBefore());
  EXPECT_EQ(Position(body, 2), it.GetPositionAfter());
  EXPECT_EQ(Position(body, 2), it.StartPosition());
  EXPECT_EQ(Position(body, 2), it.EndPosition());

  EXPECT_TRUE(it.AtEnd());
}

}  // namespace blink
