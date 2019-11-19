// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/finder/find_buffer.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"

namespace blink {

class FindBufferTest : public EditingTestBase {
 protected:
  PositionInFlatTree LastPositionInDocument() {
    return GetDocument().documentElement()->lastChild()
               ? PositionInFlatTree::AfterNode(
                     *GetDocument().documentElement()->lastChild())
               : PositionInFlatTree::LastPositionInNode(
                     *GetDocument().documentElement());
  }

  EphemeralRangeInFlatTree WholeDocumentRange() {
    return EphemeralRangeInFlatTree(PositionInFlatTree::FirstPositionInNode(
                                        *GetDocument().documentElement()),
                                    LastPositionInDocument());
  }

  PositionInFlatTree PositionFromParentId(const char* id, unsigned offset) {
    return PositionInFlatTree(GetElementById(id)->firstChild(), offset);
  }

  std::string SerializeRange(const EphemeralRangeInFlatTree& range) {
    return GetSelectionTextInFlatTreeFromBody(
        SelectionInFlatTree::Builder().SetAsForwardSelection(range).Build());
  }
};

TEST_F(FindBufferTest, FindInline) {
  SetBodyContent(
      "<div id='container'>a<span id='span'>b</span><b id='b'>c</b><div "
      "id='none' style='display:none'>d</div><div id='inline-div' "
      "style='display: inline;'>e</div></div>");
  FindBuffer buffer(WholeDocumentRange());
  EXPECT_TRUE(buffer.PositionAfterBlock().IsNull());
  std::unique_ptr<FindBuffer::Results> results =
      buffer.FindMatches("abce", kCaseInsensitive);
  EXPECT_EQ(1u, results->CountForTesting());
  FindBuffer::BufferMatchResult match = *results->begin();
  EXPECT_EQ(0u, match.start);
  EXPECT_EQ(4u, match.length);
  EXPECT_EQ(
      EphemeralRangeInFlatTree(PositionFromParentId("container", 0),
                               PositionFromParentId("inline-div", 1)),
      buffer.RangeFromBufferIndex(match.start, match.start + match.length));
}

TEST_F(FindBufferTest, RangeFromBufferIndex) {
  SetBodyContent(
      "<div id='container'>a <span id='span'> b</span><b id='b'>cc</b><div "
      "id='none' style='display:none'>d</div><div id='inline-div' "
      "style='display: inline;'>e</div></div>");
  FindBuffer buffer(WholeDocumentRange());
  // Range for "a"
  EXPECT_EQ(EphemeralRangeInFlatTree(PositionFromParentId("container", 0),
                                     PositionFromParentId("container", 1)),
            buffer.RangeFromBufferIndex(0, 1));
  EXPECT_EQ(
      "<div id=\"container\">^a| <span id=\"span\"> b</span><b "
      "id=\"b\">cc</b><div id=\"none\" style=\"display:none\">d</div><div "
      "id=\"inline-div\" style=\"display: inline;\">e</div></div>",
      SerializeRange(buffer.RangeFromBufferIndex(0, 1)));
  // Range for "a "
  EXPECT_EQ(EphemeralRangeInFlatTree(PositionFromParentId("container", 0),
                                     PositionFromParentId("container", 2)),
            buffer.RangeFromBufferIndex(0, 2));
  EXPECT_EQ(
      "<div id=\"container\">^a |<span id=\"span\"> b</span><b "
      "id=\"b\">cc</b><div id=\"none\" style=\"display:none\">d</div><div "
      "id=\"inline-div\" style=\"display: inline;\">e</div></div>",
      SerializeRange(buffer.RangeFromBufferIndex(0, 2)));
  // Range for "a b"
  EXPECT_EQ(EphemeralRangeInFlatTree(PositionFromParentId("container", 0),
                                     PositionFromParentId("span", 2)),
            buffer.RangeFromBufferIndex(0, 3));
  EXPECT_EQ(
      "<div id=\"container\">^a <span id=\"span\"> b|</span><b "
      "id=\"b\">cc</b><div id=\"none\" style=\"display:none\">d</div><div "
      "id=\"inline-div\" style=\"display: inline;\">e</div></div>",
      SerializeRange(buffer.RangeFromBufferIndex(0, 3)));
  // Range for "a bc"
  EXPECT_EQ(EphemeralRangeInFlatTree(PositionFromParentId("container", 0),
                                     PositionFromParentId("b", 1)),
            buffer.RangeFromBufferIndex(0, 4));
  EXPECT_EQ(
      "<div id=\"container\">^a <span id=\"span\"> b</span><b "
      "id=\"b\">c|c</b><div id=\"none\" style=\"display:none\">d</div><div "
      "id=\"inline-div\" style=\"display: inline;\">e</div></div>",
      SerializeRange(buffer.RangeFromBufferIndex(0, 4)));
  // Range for "a bcc"
  EXPECT_EQ(EphemeralRangeInFlatTree(PositionFromParentId("container", 0),
                                     PositionFromParentId("b", 2)),
            buffer.RangeFromBufferIndex(0, 5));
  EXPECT_EQ(
      "<div id=\"container\">^a <span id=\"span\"> b</span><b "
      "id=\"b\">cc|</b><div id=\"none\" style=\"display:none\">d</div><div "
      "id=\"inline-div\" style=\"display: inline;\">e</div></div>",
      SerializeRange(buffer.RangeFromBufferIndex(0, 5)));
  // Range for "a bcce"
  EXPECT_EQ(EphemeralRangeInFlatTree(PositionFromParentId("container", 0),
                                     PositionFromParentId("inline-div", 1)),
            buffer.RangeFromBufferIndex(0, 6));
  EXPECT_EQ(
      "<div id=\"container\">^a <span id=\"span\"> b</span><b "
      "id=\"b\">cc</b><div id=\"none\" style=\"display:none\">d</div><div "
      "id=\"inline-div\" style=\"display: inline;\">e|</div></div>",
      SerializeRange(buffer.RangeFromBufferIndex(0, 6)));
  // Range for " b"
  EXPECT_EQ(EphemeralRangeInFlatTree(PositionFromParentId("container", 1),
                                     PositionFromParentId("span", 2)),
            buffer.RangeFromBufferIndex(1, 3));
  EXPECT_EQ(
      "<div id=\"container\">a^ <span id=\"span\"> b|</span><b "
      "id=\"b\">cc</b><div id=\"none\" style=\"display:none\">d</div><div "
      "id=\"inline-div\" style=\"display: inline;\">e</div></div>",
      SerializeRange(buffer.RangeFromBufferIndex(1, 3)));
  // Range for " bc"
  EXPECT_EQ(EphemeralRangeInFlatTree(PositionFromParentId("container", 1),
                                     PositionFromParentId("b", 1)),
            buffer.RangeFromBufferIndex(1, 4));
  EXPECT_EQ(
      "<div id=\"container\">a^ <span id=\"span\"> b</span><b "
      "id=\"b\">c|c</b><div id=\"none\" style=\"display:none\">d</div><div "
      "id=\"inline-div\" style=\"display: inline;\">e</div></div>",
      SerializeRange(buffer.RangeFromBufferIndex(1, 4)));
}

TEST_F(FindBufferTest, FindBetweenPositionsSameNode) {
  PositionInFlatTree start_position =
      ToPositionInFlatTree(SetCaretTextToBody("f|oofoo"));
  Node* node = start_position.ComputeContainerNode();
  // |end_position| = foofoo| (end of text).
  PositionInFlatTree end_position =
      PositionInFlatTree::LastPositionInNode(*node);
  FindBuffer buffer(EphemeralRangeInFlatTree(start_position, end_position));
  EXPECT_EQ(1u, buffer.FindMatches("foo", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(2u, buffer.FindMatches("oo", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(4u, buffer.FindMatches("o", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("f", kCaseInsensitive)->CountForTesting());
  // |start_position| = fo|ofoo
  // |end_position| = foof|oo
  start_position = PositionInFlatTree(*node, 2u);
  end_position = PositionInFlatTree(*node, 4u);
  buffer = FindBuffer(EphemeralRangeInFlatTree(start_position, end_position));
  EXPECT_EQ(0u, buffer.FindMatches("foo", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("oo", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("o", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("f", kCaseInsensitive)->CountForTesting());
}

TEST_F(FindBufferTest, FindBetweenPositionsDifferentNodes) {
  SetBodyContent(
      "<div id='div'>foo<span id='span'>foof<b id='b'>oo</b></span></div>");
  Element* div = GetElementById("div");
  Element* span = GetElementById("span");
  Element* b = GetElementById("b");
  // <div>^foo<span>foof|<b>oo</b></span></div>
  // So buffer = "foofoof"
  FindBuffer buffer(EphemeralRangeInFlatTree(
      PositionInFlatTree::FirstPositionInNode(*div->firstChild()),
      PositionInFlatTree::LastPositionInNode(*span->firstChild())));
  EXPECT_EQ(2u, buffer.FindMatches("foo", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(2u, buffer.FindMatches("fo", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(2u, buffer.FindMatches("oof", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(2u, buffer.FindMatches("oo", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(4u, buffer.FindMatches("o", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(3u, buffer.FindMatches("f", kCaseInsensitive)->CountForTesting());
  // <div>f^oo<span>foof<b>o|o</b></span></div>
  // So buffer = "oofoofo"
  buffer = FindBuffer(
      EphemeralRangeInFlatTree(PositionInFlatTree(*div->firstChild(), 1),
                               PositionInFlatTree(*b->firstChild(), 1)));
  EXPECT_EQ(1u, buffer.FindMatches("foo", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(2u, buffer.FindMatches("oof", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(2u, buffer.FindMatches("fo", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(2u, buffer.FindMatches("oo", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(5u, buffer.FindMatches("o", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(2u, buffer.FindMatches("f", kCaseInsensitive)->CountForTesting());
  // <div>foo<span>f^oof|<b>oo</b></span></div>
  // So buffer = "oof"
  buffer = FindBuffer(
      EphemeralRangeInFlatTree(PositionInFlatTree(*span->firstChild(), 1),
                               PositionInFlatTree(*span->firstChild(), 4)));
  EXPECT_EQ(0u, buffer.FindMatches("foo", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("oof", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("fo", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("oo", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(2u, buffer.FindMatches("o", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("f", kCaseInsensitive)->CountForTesting());
  // <div>foo<span>foof^<b>oo|</b></span></div>
  // So buffer = "oo"
  buffer = FindBuffer(
      EphemeralRangeInFlatTree(PositionInFlatTree(*span->firstChild(), 4),
                               PositionInFlatTree(*b->firstChild(), 2)));
  EXPECT_EQ(0u, buffer.FindMatches("foo", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("oof", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("fo", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("oo", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(2u, buffer.FindMatches("o", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("f", kCaseInsensitive)->CountForTesting());
}

TEST_F(FindBufferTest, FindBetweenPositionsSkippedNodes) {
  SetBodyContent(
      "<div id='div'>foo<span id='span' style='display:none'>foof</span><b "
      "id='b'>oo</b><script id='script'>fo</script><a id='a'>o</o></div>");
  Element* div = GetElementById("div");
  Element* span = GetElementById("span");
  Element* b = GetElementById("b");
  Element* script = GetElementById("script");
  Element* a = GetElementById("a");

  // <div>^foo<span style='display:none;'>foo|f</span><b>oo</b>
  // <script>fo</script><a>o</a></div>
  // So buffer = "foo"
  FindBuffer buffer(EphemeralRangeInFlatTree(
      PositionInFlatTree::FirstPositionInNode(*div->firstChild()),
      PositionInFlatTree(*span->firstChild(), 3)));
  EXPECT_EQ(1u, buffer.FindMatches("foo", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("oof", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("fo", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("oo", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(2u, buffer.FindMatches("o", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("f", kCaseInsensitive)->CountForTesting());
  // <div>foo<span style='display:none;'>f^oof</span><b>oo|</b>
  // <script>fo</script><a>o</a></div>
  // So buffer = "oo"
  buffer = FindBuffer(
      EphemeralRangeInFlatTree(PositionInFlatTree(*span->firstChild(), 1),
                               PositionInFlatTree(*b->firstChild(), 2)));
  EXPECT_EQ(0u, buffer.FindMatches("foo", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("oo", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(2u, buffer.FindMatches("o", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("f", kCaseInsensitive)->CountForTesting());
  // <div>foo<span style='display:none;'>f^oof</span><b>oo|</b>
  // <script>f|o</script><a>o</a></div>
  // So buffer = "oo"
  buffer = FindBuffer(
      EphemeralRangeInFlatTree(PositionInFlatTree(*span->firstChild(), 1),
                               PositionInFlatTree(*script->firstChild(), 2)));
  EXPECT_EQ(0u, buffer.FindMatches("foo", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("oo", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(2u, buffer.FindMatches("o", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("f", kCaseInsensitive)->CountForTesting());
  // <div>foo<span style='display:none;'>foof</span><b>oo|</b>
  // <script>f^o</script><a>o|</a></div>
  // So buffer = "o"
  buffer = FindBuffer(
      EphemeralRangeInFlatTree(PositionInFlatTree(*script->firstChild(), 1),
                               PositionInFlatTree(*a->firstChild(), 1)));
  EXPECT_EQ(0u, buffer.FindMatches("foo", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("oo", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("o", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("f", kCaseInsensitive)->CountForTesting());
}

TEST_F(FindBufferTest, FindMatchInRange) {
  SetBodyContent("<div id='div'>foo<a id='a'>foof</a><b id='b'>oo</b></div>");
  Element* div = GetElementById("div");
  Element* a = GetElementById("a");
  Element* b = GetElementById("b");
  EphemeralRangeInFlatTree foo1 = EphemeralRangeInFlatTree(
      PositionInFlatTree::FirstPositionInNode(*div->firstChild()),
      PositionInFlatTree::LastPositionInNode(*div->firstChild()));
  EphemeralRangeInFlatTree foo2 = EphemeralRangeInFlatTree(
      PositionInFlatTree::FirstPositionInNode(*a->firstChild()),
      PositionInFlatTree(*a->firstChild(), 3));
  EphemeralRangeInFlatTree foo3 = EphemeralRangeInFlatTree(
      PositionInFlatTree(*a->firstChild(), 3),
      PositionInFlatTree::LastPositionInNode(*b->firstChild()));

  // <div>^foo<a>foof</a><b>oo|</b></div>, forwards
  EphemeralRangeInFlatTree match = FindBuffer::FindMatchInRange(
      WholeDocumentRange(), "foo", kCaseInsensitive);
  EXPECT_EQ(foo1, match);
  // <div>f^oo<a>foof</a><b>oo|</b></div>, forwards
  match = FindBuffer::FindMatchInRange(
      EphemeralRangeInFlatTree(PositionInFlatTree(*div->firstChild(), 1),
                               LastPositionInDocument()),
      "foo", kCaseInsensitive);
  EXPECT_EQ(foo2, match);
  // <div>foo<a>^foo|f</a><b>oo</b></div>, forwards
  match = FindBuffer::FindMatchInRange(foo2, "foo", kCaseInsensitive);
  EXPECT_EQ(foo2, match);
  // <div>foo<a>f^oof|</a><b>oo</b></div>, forwards
  match = FindBuffer::FindMatchInRange(
      EphemeralRangeInFlatTree(
          PositionInFlatTree(*a->firstChild(), 1),
          PositionInFlatTree::LastPositionInNode(*a->firstChild())),
      "foo", kCaseInsensitive);
  EXPECT_TRUE(match.IsNull());
  // <div>foo<a>f^oof</a><b>oo|</b></div>, forwards
  match = FindBuffer::FindMatchInRange(
      EphemeralRangeInFlatTree(PositionInFlatTree(*a->firstChild(), 1),
                               LastPositionInDocument()),
      "foo", kCaseInsensitive);
  EXPECT_EQ(foo3, match);

  // <div>^foo<a>foof</a><b>oo|</b></div>, backwards
  match = FindBuffer::FindMatchInRange(WholeDocumentRange(), "foo",
                                       kCaseInsensitive | kBackwards);
  EXPECT_EQ(foo3, match);
  // <div>^foo<a>foof</a><b>o|o</b></div>, backwards
  match = FindBuffer::FindMatchInRange(
      EphemeralRangeInFlatTree(
          PositionInFlatTree::FirstPositionInNode(*div->firstChild()),
          PositionInFlatTree(*b->firstChild(), 1)),
      "foo", kCaseInsensitive | kBackwards);
  EXPECT_EQ(foo2, match);
  // <div>foo<a>^foof</a><b>o|o</b></div>, backwards
  match = FindBuffer::FindMatchInRange(
      EphemeralRangeInFlatTree(
          PositionInFlatTree::FirstPositionInNode(*a->firstChild()),
          PositionInFlatTree(*b->firstChild(), 1)),
      "foo", kCaseInsensitive | kBackwards);
  EXPECT_EQ(foo2, match);
  // <div>foo<a>foo^f</a><b>o|o</b></div>, backwards
  match = FindBuffer::FindMatchInRange(
      EphemeralRangeInFlatTree(PositionInFlatTree(*a->firstChild(), 3),
                               PositionInFlatTree(*b->firstChild(), 1)),
      "foo", kCaseInsensitive | kBackwards);
  EXPECT_TRUE(match.IsNull());
  // <div>^foo<a>fo|of</a><b>oo</b></div>, backwards
  match = FindBuffer::FindMatchInRange(
      EphemeralRangeInFlatTree(
          PositionInFlatTree::FirstPositionInNode(*div->firstChild()),
          PositionInFlatTree(*a->firstChild(), 2)),
      "foo", kCaseInsensitive | kBackwards);
  EXPECT_EQ(foo1, match);
}

class FindBufferBlockTest : public FindBufferTest,
                            public testing::WithParamInterface<std::string> {};

INSTANTIATE_TEST_SUITE_P(Blocks,
                         FindBufferBlockTest,
                         testing::Values("block",
                                         "table",
                                         "flow-root",
                                         "grid",
                                         "flex",
                                         "list-item"));

TEST_P(FindBufferBlockTest, FindBlock) {
  SetBodyContent("text<div id='block' style='display: " + GetParam() +
                 ";'>block</div><span id='span'>span</span>");
  FindBuffer text_buffer(WholeDocumentRange());
  EXPECT_EQ(GetElementById("block"),
            *text_buffer.PositionAfterBlock().ComputeContainerNode());
  EXPECT_EQ(
      1u, text_buffer.FindMatches("text", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(0u, text_buffer.FindMatches("textblock", kCaseInsensitive)
                    ->CountForTesting());
  EXPECT_EQ(0u, text_buffer.FindMatches("text block", kCaseInsensitive)
                    ->CountForTesting());

  FindBuffer block_buffer(EphemeralRangeInFlatTree(
      text_buffer.PositionAfterBlock(), LastPositionInDocument()));
  EXPECT_EQ(GetElementById("span"),
            *block_buffer.PositionAfterBlock().ComputeContainerNode());
  EXPECT_EQ(
      1u,
      block_buffer.FindMatches("block", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(0u, block_buffer.FindMatches("textblock", kCaseInsensitive)
                    ->CountForTesting());
  EXPECT_EQ(0u, block_buffer.FindMatches("text block", kCaseInsensitive)
                    ->CountForTesting());
  EXPECT_EQ(0u, block_buffer.FindMatches("blockspan", kCaseInsensitive)
                    ->CountForTesting());
  EXPECT_EQ(0u, block_buffer.FindMatches("block span", kCaseInsensitive)
                    ->CountForTesting());

  FindBuffer span_buffer(EphemeralRangeInFlatTree(
      block_buffer.PositionAfterBlock(), LastPositionInDocument()));
  EXPECT_TRUE(span_buffer.PositionAfterBlock().IsNull());
  EXPECT_EQ(
      1u, span_buffer.FindMatches("span", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(0u, span_buffer.FindMatches("blockspan", kCaseInsensitive)
                    ->CountForTesting());
  EXPECT_EQ(0u, span_buffer.FindMatches("block span", kCaseInsensitive)
                    ->CountForTesting());
}

class FindBufferSeparatorTest
    : public FindBufferTest,
      public testing::WithParamInterface<std::string> {};

INSTANTIATE_TEST_SUITE_P(Separators,
                         FindBufferSeparatorTest,
                         testing::Values("br",
                                         "hr",
                                         "legend",
                                         "meter",
                                         "object",
                                         "progress",
                                         "select",
                                         "video"));

TEST_P(FindBufferSeparatorTest, FindSeparatedElements) {
  SetBodyContent("a<" + GetParam() + ">a</" + GetParam() + ">a");
  FindBuffer buffer(WholeDocumentRange());
  EXPECT_EQ(0u, buffer.FindMatches("aa", kCaseInsensitive)->CountForTesting());
}

TEST_F(FindBufferTest, WhiteSpaceCollapsingPreWrap) {
  SetBodyContent(
      " a  \n   b  <b> c </b> d  <span style='white-space: pre-wrap'> e  "
      "</span>");
  FindBuffer buffer(WholeDocumentRange());
  EXPECT_EQ(
      1u,
      buffer.FindMatches("a b c d  e  ", kCaseInsensitive)->CountForTesting());
}

TEST_F(FindBufferTest, WhiteSpaceCollapsingPre) {
  SetBodyContent("<div style='white-space: pre;'>a \n b</div>");
  FindBuffer buffer(WholeDocumentRange());
  EXPECT_EQ(1u, buffer.FindMatches("a", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("b", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("ab", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("a b", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(0u,
            buffer.FindMatches("a  b", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(0u,
            buffer.FindMatches("a   b", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(0u,
            buffer.FindMatches("a\n b", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(0u,
            buffer.FindMatches("a \nb", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(1u,
            buffer.FindMatches("a \n b", kCaseInsensitive)->CountForTesting());
}

TEST_F(FindBufferTest, WhiteSpaceCollapsingPreLine) {
  SetBodyContent("<div style='white-space: pre-line;'>a \n b</div>");
  FindBuffer buffer(WholeDocumentRange());
  EXPECT_EQ(1u, buffer.FindMatches("a", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("b", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("ab", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("a b", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(0u,
            buffer.FindMatches("a  b", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(0u,
            buffer.FindMatches("a   b", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(0u,
            buffer.FindMatches("a \n b", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(0u,
            buffer.FindMatches("a\n b", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(0u,
            buffer.FindMatches("a \nb", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(1u,
            buffer.FindMatches("a\nb", kCaseInsensitive)->CountForTesting());
}

TEST_F(FindBufferTest, BidiTest) {
  SetBodyContent("<bdo dir=rtl id=bdo>foo<span>bar</span></bdo>");
  FindBuffer buffer(WholeDocumentRange());
  EXPECT_EQ(1u,
            buffer.FindMatches("foobar", kCaseInsensitive)->CountForTesting());
}

TEST_F(FindBufferTest, KanaSmallVsNormal) {
  SetBodyContent("や");  // Normal-sized や
  FindBuffer buffer(WholeDocumentRange());
  // Should find normal-sized や
  EXPECT_EQ(1u, buffer.FindMatches("や", kCaseInsensitive)->CountForTesting());
  // Should not find smalll-sized ゃ
  EXPECT_EQ(0u, buffer.FindMatches("ゃ", kCaseInsensitive)->CountForTesting());
}

TEST_F(FindBufferTest, KanaDakuten) {
  SetBodyContent("びゃ");  // Hiragana bya
  FindBuffer buffer(WholeDocumentRange());
  // Should find bi
  EXPECT_EQ(1u, buffer.FindMatches("び", kCaseInsensitive)->CountForTesting());
  // Should find smalll-sized ゃ
  EXPECT_EQ(1u, buffer.FindMatches("ゃ", kCaseInsensitive)->CountForTesting());
  // Should find bya
  EXPECT_EQ(1u,
            buffer.FindMatches("びゃ", kCaseInsensitive)->CountForTesting());
  // Should not find hi
  EXPECT_EQ(0u, buffer.FindMatches("ひ", kCaseInsensitive)->CountForTesting());
  // Should not find pi
  EXPECT_EQ(0u, buffer.FindMatches("ぴ", kCaseInsensitive)->CountForTesting());
}

TEST_F(FindBufferTest, KanaHalfFull) {
  // Should treat hiragana, katakana, half width katakana as the same.
  // hiragana ra, half width katakana ki, full width katakana na
  SetBodyContent("らｷナ");
  FindBuffer buffer(WholeDocumentRange());
  // Should find katakana ra
  EXPECT_EQ(1u, buffer.FindMatches("ラ", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("ﾗ", kCaseInsensitive)->CountForTesting());
  // Should find hiragana & katakana ki
  EXPECT_EQ(1u, buffer.FindMatches("き", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("キ", kCaseInsensitive)->CountForTesting());
  // Should find hiragana & katakana na
  EXPECT_EQ(1u, buffer.FindMatches("な", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("ﾅ", kCaseInsensitive)->CountForTesting());
  // Should find whole word
  EXPECT_EQ(1u,
            buffer.FindMatches("らきな", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("ﾗｷﾅ", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(1u,
            buffer.FindMatches("ラキナ", kCaseInsensitive)->CountForTesting());
}

TEST_F(FindBufferTest, WholeWordTest) {
  SetBodyContent("foo bar foobar 六本木");
  FindBuffer buffer(WholeDocumentRange());
  EXPECT_EQ(2u, buffer.FindMatches("foo", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("foo", kCaseInsensitive | kWholeWord)
                    ->CountForTesting());
  EXPECT_EQ(2u, buffer.FindMatches("bar", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("bar", kCaseInsensitive | kWholeWord)
                    ->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("六", kCaseInsensitive | kWholeWord)
                    ->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("本木", kCaseInsensitive | kWholeWord)
                    ->CountForTesting());
}

TEST_F(FindBufferTest, KanaDecomposed) {
  SetBodyContent("は　゛");
  FindBuffer buffer(WholeDocumentRange());
  EXPECT_EQ(0u, buffer.FindMatches("ば", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(1u,
            buffer.FindMatches("は　゛", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(0u, buffer.FindMatches("バ ", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(1u,
            buffer.FindMatches("ハ ゛", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(1u, buffer.FindMatches("ﾊ ﾞ", kCaseInsensitive)->CountForTesting());
  EXPECT_EQ(1u,
            buffer.FindMatches("ﾊ ゛", kCaseInsensitive)->CountForTesting());
}

TEST_F(FindBufferTest, FindPlainTextInvalidTarget1) {
  static const char* body_content = "<div>foo bar test</div>";
  SetBodyContent(body_content);

  // A lone lead surrogate (0xDA0A) example taken from fuzz-58.
  static const UChar kInvalid1[] = {0x1461u, 0x2130u, 0x129bu, 0xd711u, 0xd6feu,
                                    0xccadu, 0x7064u, 0xd6a0u, 0x4e3bu, 0x03abu,
                                    0x17dcu, 0xb8b7u, 0xbf55u, 0xfca0u, 0x07fau,
                                    0x0427u, 0xda0au, 0};

  FindBuffer buffer(WholeDocumentRange());
  const auto results = buffer.FindMatches(String(kInvalid1), 0);
  EXPECT_TRUE(results->IsEmpty());
}

TEST_F(FindBufferTest, FindPlainTextInvalidTarget2) {
  static const char* body_content = "<div>foo bar test</div>";
  SetBodyContent(body_content);

  // A lone trailing surrogate (U+DC01).
  static const UChar kInvalid2[] = {0x1461u, 0x2130u, 0x129bu, 0xdc01u,
                                    0xd6feu, 0xccadu, 0};

  FindBuffer buffer(WholeDocumentRange());
  const auto results = buffer.FindMatches(String(kInvalid2), 0);
  EXPECT_TRUE(results->IsEmpty());
}

TEST_F(FindBufferTest, FindPlainTextInvalidTarget3) {
  static const char* body_content = "<div>foo bar test</div>";
  SetBodyContent(body_content);

  // A trailing surrogate followed by a lead surrogate (U+DC03 U+D901).
  static const UChar kInvalid3[] = {0xd800u, 0xdc00u, 0x0061u, 0xdc03u,
                                    0xd901u, 0xccadu, 0};
  FindBuffer buffer(WholeDocumentRange());
  const auto results = buffer.FindMatches(String(kInvalid3), 0);
  EXPECT_TRUE(results->IsEmpty());
}

TEST_F(FindBufferTest, DisplayInline) {
  SetBodyContent("<span>fi</span>nd");
  FindBuffer buffer(WholeDocumentRange());
  const auto results = buffer.FindMatches("find", 0);
  ASSERT_EQ(1u, results->CountForTesting());
  EXPECT_EQ(FindBuffer::BufferMatchResult({0, 4}), results->front());
}

TEST_F(FindBufferTest, DisplayBlock) {
  SetBodyContent("<div>fi</div>nd");
  FindBuffer buffer(WholeDocumentRange());
  const auto results = buffer.FindMatches("find", 0);
  ASSERT_EQ(0u, results->CountForTesting())
      << "We should not match across block.";
}

TEST_F(FindBufferTest, DisplayContents) {
  SetBodyContent("<div style='display: contents'>fi</div>nd");
  FindBuffer buffer(WholeDocumentRange());
  const auto results = buffer.FindMatches("find", 0);
  ASSERT_EQ(1u, results->CountForTesting());
  EXPECT_EQ(FindBuffer::BufferMatchResult({0, 4}), results->front());
}

TEST_F(FindBufferTest, WBRTest) {
  SetBodyContent("fi<wbr>nd and fin<wbr>d");
  FindBuffer buffer(WholeDocumentRange());
  const auto results = buffer.FindMatches("find", 0);
  ASSERT_EQ(2u, results->CountForTesting());
}

TEST_F(FindBufferTest, InputTest) {
  SetBodyContent("fi<input type='text'>nd and fin<input type='text'>d");
  FindBuffer buffer(WholeDocumentRange());
  const auto results = buffer.FindMatches("find", 0);
  ASSERT_EQ(0u, results->CountForTesting());
}

TEST_F(FindBufferTest, SelectMultipleTest) {
  SetBodyContent("<select multiple><option>find me</option></select>");
  FindBuffer buffer(WholeDocumentRange());
#if defined(OS_ANDROID)
  EXPECT_EQ(0u, buffer.FindMatches("find", 0)->CountForTesting());
#else
  EXPECT_EQ(1u, buffer.FindMatches("find", 0)->CountForTesting());
#endif  // defined(OS_ANDROID)
  SetBodyContent("<select size=2><option>find me</option></select>");
  buffer = FindBuffer(WholeDocumentRange());
#if defined(OS_ANDROID)
  EXPECT_EQ(0u, buffer.FindMatches("find", 0)->CountForTesting());
#else
  EXPECT_EQ(1u, buffer.FindMatches("find", 0)->CountForTesting());
#endif  // defined(OS_ANDROID)
  SetBodyContent("<select size=1><option>find me</option></select>");
  buffer = FindBuffer(WholeDocumentRange());
  EXPECT_EQ(0u, buffer.FindMatches("find", 0)->CountForTesting());
}

TEST_F(FindBufferTest, NullRange) {
  SetBodyContent("x<div></div>");
  FindBuffer buffer(WholeDocumentRange());
  EXPECT_EQ(0u, buffer.FindMatches("find", 0)->CountForTesting());
}

}  // namespace blink
