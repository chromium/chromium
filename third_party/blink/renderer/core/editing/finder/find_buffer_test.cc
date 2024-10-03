// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/finder/find_buffer.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/finder/find_results.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"

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

  EphemeralRangeInFlatTree CreateRange(const Node& start_node,
                                       int start_offset,
                                       const Node& end_node,
                                       int end_offset) {
    return EphemeralRangeInFlatTree(
        PositionInFlatTree(start_node, start_offset),
        PositionInFlatTree(end_node, end_offset));
  }

  std::string SerializeRange(const EphemeralRangeInFlatTree& range) {
    return GetSelectionTextInFlatTreeFromBody(
        SelectionInFlatTree::Builder().SetAsForwardSelection(range).Build());
  }

  static unsigned CaseInsensitiveMatchCount(FindBuffer& buffer,
                                            const String& query) {
    return buffer.FindMatches(query, kCaseInsensitive).CountForTesting();
  }

  static constexpr FindOptions kCaseInsensitive =
      FindOptions().SetCaseInsensitive(true);
};

// A test with an HTML data containing no <ruby> should use FindBufferParamTest,
// and should create a FindBuffer with GetParam().
class FindBufferParamTest : public FindBufferTest,
                            public testing::WithParamInterface<RubySupport> {};

INSTANTIATE_TEST_SUITE_P(,
                         FindBufferParamTest,
                         ::testing::Values(RubySupport::kDisabled,
                                           RubySupport::kEnabledForcefully));

TEST_P(FindBufferParamTest, FindInline) {
  SetBodyContent(
      "<div id='container'>a<span id='span'>b</span><b id='b'>c</b><div "
      "id='none' style='display:none'>d</div><div id='inline-div' "
      "style='display: inline;'>e</div></div>");
  FindBuffer buffer(WholeDocumentRange(), GetParam());
  EXPECT_TRUE(buffer.PositionAfterBlock().IsNull());
  FindResults results = buffer.FindMatches("abce", kCaseInsensitive);
  EXPECT_EQ(1u, results.CountForTesting());
  MatchResultICU match = *results.begin();
  EXPECT_EQ(0u, match.start);
  EXPECT_EQ(4u, match.length);
  EXPECT_EQ(
      EphemeralRangeInFlatTree(PositionFromParentId("container", 0),
                               PositionFromParentId("inline-div", 1)),
      buffer.RangeFromBufferIndex(match.start, match.start + match.length));
}

TEST_P(FindBufferParamTest, RangeFromBufferIndex) {
  SetBodyContent(
      "<div id='container'>a <span id='span'> b</span><b id='b'>cc</b><div "
      "id='none' style='display:none'>d</div><div id='inline-div' "
      "style='display: inline;'>e</div></div>");
  FindBuffer buffer(WholeDocumentRange(), GetParam());
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

TEST_P(FindBufferParamTest, FindBetweenPositionsSameNode) {
  PositionInFlatTree start_position =
      ToPositionInFlatTree(SetCaretTextToBody("f|oofoo"));
  Node* node = start_position.ComputeContainerNode();
  // |end_position| = foofoo| (end of text).
  PositionInFlatTree end_position =
      PositionInFlatTree::LastPositionInNode(*node);
  {
    FindBuffer buffer(EphemeralRangeInFlatTree(start_position, end_position),
                      GetParam());
    EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, "foo"));
    EXPECT_EQ(2u, CaseInsensitiveMatchCount(buffer, "oo"));
    EXPECT_EQ(4u, CaseInsensitiveMatchCount(buffer, "o"));
    EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, "f"));
  }
  // |start_position| = fo|ofoo
  // |end_position| = foof|oo
  start_position = PositionInFlatTree(*node, 2u);
  end_position = PositionInFlatTree(*node, 4u);
  {
    FindBuffer buffer(EphemeralRangeInFlatTree(start_position, end_position),
                      GetParam());
    EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, "foo"));
    EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, "oo"));
    EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, "o"));
    EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, "f"));
  }
}

TEST_P(FindBufferParamTest, FindBetweenPositionsDifferentNodes) {
  SetBodyContent(
      "<div id='div'>foo<span id='span'>foof<b id='b'>oo</b></span></div>");
  Element* div = GetElementById("div");
  Element* span = GetElementById("span");
  Element* b = GetElementById("b");
  // <div>^foo<span>foof|<b>oo</b></span></div>
  // So buffer = "foofoof"
  {
    FindBuffer buffer(
        EphemeralRangeInFlatTree(
            PositionInFlatTree::FirstPositionInNode(*div->firstChild()),
            PositionInFlatTree::LastPositionInNode(*span->firstChild())),
        GetParam());
    EXPECT_EQ(2u, CaseInsensitiveMatchCount(buffer, "foo"));
    EXPECT_EQ(2u, CaseInsensitiveMatchCount(buffer, "fo"));
    EXPECT_EQ(2u, CaseInsensitiveMatchCount(buffer, "oof"));
    EXPECT_EQ(2u, CaseInsensitiveMatchCount(buffer, "oo"));
    EXPECT_EQ(4u, CaseInsensitiveMatchCount(buffer, "o"));
    EXPECT_EQ(3u, CaseInsensitiveMatchCount(buffer, "f"));
  }
  // <div>f^oo<span>foof<b>o|o</b></span></div>
  // So buffer = "oofoofo"
  {
    FindBuffer buffer(CreateRange(*div->firstChild(), 1, *b->firstChild(), 1),
                      GetParam());
    EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, "foo"));
    EXPECT_EQ(2u, CaseInsensitiveMatchCount(buffer, "oof"));
    EXPECT_EQ(2u, CaseInsensitiveMatchCount(buffer, "fo"));
    EXPECT_EQ(2u, CaseInsensitiveMatchCount(buffer, "oo"));
    EXPECT_EQ(5u, CaseInsensitiveMatchCount(buffer, "o"));
    EXPECT_EQ(2u, CaseInsensitiveMatchCount(buffer, "f"));
  }
  // <div>foo<span>f^oof|<b>oo</b></span></div>
  // So buffer = "oof"
  {
    FindBuffer buffer(
        CreateRange(*span->firstChild(), 1, *span->firstChild(), 4),
        GetParam());
    EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, "foo"));
    EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, "oof"));
    EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, "fo"));
    EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, "oo"));
    EXPECT_EQ(2u, CaseInsensitiveMatchCount(buffer, "o"));
    EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, "f"));
  }
  // <div>foo<span>foof^<b>oo|</b></span></div>
  // So buffer = "oo"
  FindBuffer buffer(CreateRange(*span->firstChild(), 4, *b->firstChild(), 2),
                    GetParam());
  EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, "foo"));
  EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, "oof"));
  EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, "fo"));
  EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, "oo"));
  EXPECT_EQ(2u, CaseInsensitiveMatchCount(buffer, "o"));
  EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, "f"));
}

TEST_P(FindBufferParamTest, FindBetweenPositionsSkippedNodes) {
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
  {
    FindBuffer buffer(
        EphemeralRangeInFlatTree(
            PositionInFlatTree::FirstPositionInNode(*div->firstChild()),
            PositionInFlatTree(*span->firstChild(), 3)),
        GetParam());
    EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, "foo"));
    EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, "oof"));
    EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, "fo"));
    EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, "oo"));
    EXPECT_EQ(2u, CaseInsensitiveMatchCount(buffer, "o"));
    EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, "f"));
  }
  // <div>foo<span style='display:none;'>f^oof</span><b>oo|</b>
  // <script>fo</script><a>o</a></div>
  // So buffer = "oo"
  {
    FindBuffer buffer(CreateRange(*span->firstChild(), 1, *b->firstChild(), 2),
                      GetParam());
    EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, "foo"));
    EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, "oo"));
    EXPECT_EQ(2u, CaseInsensitiveMatchCount(buffer, "o"));
    EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, "f"));
  }
  // <div>foo<span style='display:none;'>f^oof</span><b>oo|</b>
  // <script>f|o</script><a>o</a></div>
  // So buffer = "oo"
  {
    FindBuffer buffer(
        CreateRange(*span->firstChild(), 1, *script->firstChild(), 2),
        GetParam());
    EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, "foo"));
    EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, "oo"));
    EXPECT_EQ(2u, CaseInsensitiveMatchCount(buffer, "o"));
    EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, "f"));
  }
  // <div>foo<span style='display:none;'>foof</span><b>oo|</b>
  // <script>f^o</script><a>o|</a></div>
  // So buffer = "o"
  {
    FindBuffer buffer(
        CreateRange(*script->firstChild(), 1, *a->firstChild(), 1), GetParam());
    EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, "foo"));
    EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, "oo"));
    EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, "o"));
    EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, "f"));
  }
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

  constexpr FindOptions kCaseInsensitiveBackwards =
      FindOptions().SetCaseInsensitive(true).SetBackwards(true);

  // <div>^foo<a>foof</a><b>oo|</b></div>, backwards
  match = FindBuffer::FindMatchInRange(WholeDocumentRange(), "foo",
                                       kCaseInsensitiveBackwards);
  EXPECT_EQ(foo3, match);
  // <div>^foo<a>foof</a><b>o|o</b></div>, backwards
  match = FindBuffer::FindMatchInRange(
      EphemeralRangeInFlatTree(
          PositionInFlatTree::FirstPositionInNode(*div->firstChild()),
          PositionInFlatTree(*b->firstChild(), 1)),
      "foo", kCaseInsensitiveBackwards);
  EXPECT_EQ(foo2, match);
  // <div>foo<a>^foof</a><b>o|o</b></div>, backwards
  match = FindBuffer::FindMatchInRange(
      EphemeralRangeInFlatTree(
          PositionInFlatTree::FirstPositionInNode(*a->firstChild()),
          PositionInFlatTree(*b->firstChild(), 1)),
      "foo", kCaseInsensitiveBackwards);
  EXPECT_EQ(foo2, match);
  // <div>foo<a>foo^f</a><b>o|o</b></div>, backwards
  match = FindBuffer::FindMatchInRange(
      EphemeralRangeInFlatTree(PositionInFlatTree(*a->firstChild(), 3),
                               PositionInFlatTree(*b->firstChild(), 1)),
      "foo", kCaseInsensitiveBackwards);
  EXPECT_TRUE(match.IsNull());
  // <div>^foo<a>fo|of</a><b>oo</b></div>, backwards
  match = FindBuffer::FindMatchInRange(
      EphemeralRangeInFlatTree(
          PositionInFlatTree::FirstPositionInNode(*div->firstChild()),
          PositionInFlatTree(*a->firstChild(), 2)),
      "foo", kCaseInsensitiveBackwards);
  EXPECT_EQ(foo1, match);
}

// https://issues.chromium.org/issues/327017912
TEST_F(FindBufferTest, FindMatchInRangeIgnoreNonSearchable) {
  SetBodyContent(R"(
    <div inert>Do not find me!</div>
    <div style="display: none">Do not find me!</div>)");
  EphemeralRangeInFlatTree match = FindBuffer::FindMatchInRange(
      WholeDocumentRange(), "me", kCaseInsensitive);
  EXPECT_TRUE(match.IsNull());
}

class FindBufferBlockTest
    : public FindBufferTest,
      public testing::WithParamInterface<std::tuple<std::string, RubySupport>> {
};

std::string GenerateSuffix(
    const testing::TestParamInfo<FindBufferBlockTest::ParamType>& info) {
  auto [display, ruby_support] = info.param;
  auto it = display.find("-");
  if (it != std::string::npos) {
    display.replace(it, 1, "");
  }
  return display + "_" +
         (ruby_support == RubySupport::kDisabled ? "RubyDisabled"
                                                 : "RubyEnabled");
}

INSTANTIATE_TEST_SUITE_P(
    Blocks,
    FindBufferBlockTest,
    testing::Combine(testing::Values("block",
                                     "table",
                                     "flow-root",
                                     "grid",
                                     "flex",
                                     "list-item"),
                     testing::Values(RubySupport::kDisabled,
                                     RubySupport::kEnabledForcefully)),
    GenerateSuffix);

TEST_P(FindBufferBlockTest, FindBlock) {
  auto [display, ruby_support] = GetParam();
  SetBodyContent("text<div id='block' style='display: " + display +
                 ";'>block</div><span id='span'>span</span>");
  PositionInFlatTree position_after_block;
  {
    FindBuffer text_buffer(WholeDocumentRange(), ruby_support);
    EXPECT_EQ(GetElementById("block"),
              *text_buffer.PositionAfterBlock().ComputeContainerNode());
    EXPECT_EQ(1u, CaseInsensitiveMatchCount(text_buffer, "text"));
    EXPECT_EQ(0u, CaseInsensitiveMatchCount(text_buffer, "textblock"));
    EXPECT_EQ(0u, CaseInsensitiveMatchCount(text_buffer, "text block"));
    position_after_block = text_buffer.PositionAfterBlock();
  }
  {
    FindBuffer block_buffer(EphemeralRangeInFlatTree(position_after_block,
                                                     LastPositionInDocument()),
                            ruby_support);
    EXPECT_EQ(GetElementById("span"),
              *block_buffer.PositionAfterBlock().ComputeContainerNode());
    EXPECT_EQ(1u, CaseInsensitiveMatchCount(block_buffer, "block"));
    EXPECT_EQ(0u, CaseInsensitiveMatchCount(block_buffer, "textblock"));
    EXPECT_EQ(0u, CaseInsensitiveMatchCount(block_buffer, "text block"));
    EXPECT_EQ(0u, CaseInsensitiveMatchCount(block_buffer, "blockspan"));
    EXPECT_EQ(0u, CaseInsensitiveMatchCount(block_buffer, "block span"));
    position_after_block = block_buffer.PositionAfterBlock();
  }
  {
    FindBuffer span_buffer(EphemeralRangeInFlatTree(position_after_block,
                                                    LastPositionInDocument()),
                           ruby_support);
    EXPECT_TRUE(span_buffer.PositionAfterBlock().IsNull());
    EXPECT_EQ(1u, CaseInsensitiveMatchCount(span_buffer, "span"));
    EXPECT_EQ(0u, CaseInsensitiveMatchCount(span_buffer, "blockspan"));
    EXPECT_EQ(0u, CaseInsensitiveMatchCount(span_buffer, "block span"));
  }
}

class FindBufferSeparatorTest
    : public FindBufferTest,
      public testing::WithParamInterface<std::string> {};

INSTANTIATE_TEST_SUITE_P(Separators,
                         FindBufferSeparatorTest,
                         testing::Values("br",
                                         "hr",
                                         "meter",
                                         "object",
                                         "progress",
                                         "select",
                                         "video"));

TEST_P(FindBufferSeparatorTest, FindSeparatedElements) {
  SetBodyContent("a<" + GetParam() + ">a</" + GetParam() + ">a");
  {
    FindBuffer buffer(WholeDocumentRange());
    EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, "aa"));
  }

  {
    FindBuffer buffer(WholeDocumentRange(), RubySupport::kEnabledForcefully);
    EXPECT_EQ(0u, buffer.FindMatches("aa", kCaseInsensitive).CountForTesting());
  }
}

TEST_P(FindBufferSeparatorTest, FindBRSeparatedElements) {
  SetBodyContent("a<br>a");
  {
    FindBuffer buffer(WholeDocumentRange());
    EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, "a\na"));
  }

  {
    FindBuffer buffer(WholeDocumentRange(), RubySupport::kEnabledForcefully);
    EXPECT_EQ(1u,
              buffer.FindMatches("a\na", kCaseInsensitive).CountForTesting());
  }
}

TEST_P(FindBufferParamTest, WhiteSpaceCollapsingPreWrap) {
  SetBodyContent(
      " a  \n   b  <b> c </b> d  <span style='white-space: pre-wrap'> e  "
      "</span>");
  FindBuffer buffer(WholeDocumentRange(), GetParam());
  EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, "a b c d  e  "));
}

TEST_P(FindBufferParamTest, WhiteSpaceCollapsingPre) {
  SetBodyContent("<div style='white-space: pre;'>a \n b</div>");
  FindBuffer buffer(WholeDocumentRange(), GetParam());
  EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, "a"));
  EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, "b"));
  EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, "ab"));
  EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, "a b"));
  EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, "a  b"));
  EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, "a   b"));
  EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, "a\n b"));
  EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, "a \nb"));
  EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, "a \n b"));
}

TEST_P(FindBufferParamTest, WhiteSpaceCollapsingPreLine) {
  SetBodyContent("<div style='white-space: pre-line;'>a \n b</div>");
  FindBuffer buffer(WholeDocumentRange(), GetParam());
  EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, "a"));
  EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, "b"));
  EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, "ab"));
  EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, "a b"));
  EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, "a  b"));
  EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, "a   b"));
  EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, "a \n b"));
  EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, "a\n b"));
  EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, "a \nb"));
  EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, "a\nb"));
}

TEST_P(FindBufferParamTest, BidiTest) {
  SetBodyContent("<bdo dir=rtl id=bdo>foo<span>bar</span></bdo>");
  FindBuffer buffer(WholeDocumentRange(), GetParam());
  EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, "foobar"));
}

TEST_P(FindBufferParamTest, KanaSmallVsNormal) {
  SetBodyContent("や");  // Normal-sized や
  FindBuffer buffer(WholeDocumentRange(), GetParam());
  // Should find normal-sized や
  EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, u"や"));
  // Should not find smalll-sized ゃ
  EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, u"ゃ"));
}

TEST_P(FindBufferParamTest, KanaDakuten) {
  SetBodyContent("びゃ");  // Hiragana bya
  FindBuffer buffer(WholeDocumentRange(), GetParam());
  // Should find bi
  EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, u"び"));
  // Should find smalll-sized ゃ
  EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, u"ゃ"));
  // Should find bya
  EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, u"びゃ"));
  // Should not find hi
  EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, u"ひ"));
  // Should not find pi
  EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, u"ぴ"));
}

TEST_P(FindBufferParamTest, KanaHalfFull) {
  // Should treat hiragana, katakana, half width katakana as the same.
  // hiragana ra, half width katakana ki, full width katakana na
  SetBodyContent("らｷナ");
  FindBuffer buffer(WholeDocumentRange(), GetParam());
  // Should find katakana ra
  EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, u"ラ"));
  EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, u"ﾗ"));
  // Should find hiragana & katakana ki
  EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, u"き"));
  EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, u"キ"));
  // Should find hiragana & katakana na
  EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, u"な"));
  EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, u"ﾅ"));
  // Should find whole word
  EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, u"らきな"));
  EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, u"ﾗｷﾅ"));
  EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, u"ラキナ"));
}

TEST_P(FindBufferParamTest, WholeWordTest) {
  SetBodyContent("foo bar foobar 六本木");
  FindBuffer buffer(WholeDocumentRange(), GetParam());
  EXPECT_EQ(2u, CaseInsensitiveMatchCount(buffer, "foo"));
  constexpr FindOptions kCaseInsensitiveWholeWord =
      FindOptions().SetCaseInsensitive(true).SetWholeWord(true);
  EXPECT_EQ(
      1u,
      buffer.FindMatches("foo", kCaseInsensitiveWholeWord).CountForTesting());
  EXPECT_EQ(2u, CaseInsensitiveMatchCount(buffer, "bar"));
  EXPECT_EQ(
      1u,
      buffer.FindMatches("bar", kCaseInsensitiveWholeWord).CountForTesting());
  EXPECT_EQ(
      1u,
      buffer.FindMatches(u"六", kCaseInsensitiveWholeWord).CountForTesting());
  EXPECT_EQ(
      1u,
      buffer.FindMatches(u"本木", kCaseInsensitiveWholeWord).CountForTesting());
}

TEST_P(FindBufferParamTest, KanaDecomposed) {
  SetBodyContent("は　゛");
  FindBuffer buffer(WholeDocumentRange(), GetParam());
  EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, u"ば"));
  EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, u"は　゛"));
  EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, u"バ "));
  EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, u"ハ ゛"));
  EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, u"ﾊ ﾞ"));
  EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, u"ﾊ ゛"));
}

TEST_P(FindBufferParamTest, FindDecomposedKanaInComposed) {
  // Hiragana Ba, composed
  SetBodyInnerHTML(u"\u3070");
  FindBuffer buffer(WholeDocumentRange(), GetParam());
  // Hiragana Ba, decomposed
  EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, String(u"\u306F\u3099")));
}

TEST_P(FindBufferParamTest, FindPlainTextInvalidTarget1) {
  static const char* body_content = "<div>foo bar test</div>";
  SetBodyContent(body_content);

  // A lone lead surrogate (0xDA0A) example taken from fuzz-58.
  static const UChar kInvalid1[] = {0x1461u, 0x2130u, 0x129bu, 0xd711u, 0xd6feu,
                                    0xccadu, 0x7064u, 0xd6a0u, 0x4e3bu, 0x03abu,
                                    0x17dcu, 0xb8b7u, 0xbf55u, 0xfca0u, 0x07fau,
                                    0x0427u, 0xda0au, 0};

  FindBuffer buffer(WholeDocumentRange(), GetParam());
  const auto results = buffer.FindMatches(String(kInvalid1), FindOptions());
  EXPECT_TRUE(results.IsEmpty());
}

TEST_P(FindBufferParamTest, FindPlainTextInvalidTarget2) {
  static const char* body_content = "<div>foo bar test</div>";
  SetBodyContent(body_content);

  // A lone trailing surrogate (U+DC01).
  static const UChar kInvalid2[] = {0x1461u, 0x2130u, 0x129bu, 0xdc01u,
                                    0xd6feu, 0xccadu, 0};

  FindBuffer buffer(WholeDocumentRange(), GetParam());
  const auto results = buffer.FindMatches(String(kInvalid2), FindOptions());
  EXPECT_TRUE(results.IsEmpty());
}

TEST_P(FindBufferParamTest, FindPlainTextInvalidTarget3) {
  static const char* body_content = "<div>foo bar test</div>";
  SetBodyContent(body_content);

  // A trailing surrogate followed by a lead surrogate (U+DC03 U+D901).
  static const UChar kInvalid3[] = {0xd800u, 0xdc00u, 0x0061u, 0xdc03u,
                                    0xd901u, 0xccadu, 0};
  FindBuffer buffer(WholeDocumentRange(), GetParam());
  const auto results = buffer.FindMatches(String(kInvalid3), FindOptions());
  EXPECT_TRUE(results.IsEmpty());
}

TEST_P(FindBufferParamTest, DisplayInline) {
  SetBodyContent("<span>fi</span>nd");
  FindBuffer buffer(WholeDocumentRange(), GetParam());
  const auto results = buffer.FindMatches("find", FindOptions());
  ASSERT_EQ(1u, results.CountForTesting());
  EXPECT_EQ(MatchResultICU({0, 4}), results.front());
}

TEST_P(FindBufferParamTest, DisplayBlock) {
  SetBodyContent("<div>fi</div>nd");
  FindBuffer buffer(WholeDocumentRange(), GetParam());
  const auto results = buffer.FindMatches("find", FindOptions());
  ASSERT_EQ(0u, results.CountForTesting())
      << "We should not match across block.";
}

TEST_P(FindBufferParamTest, DisplayContents) {
  SetBodyContent("<div style='display: contents'>fi</div>nd");
  FindBuffer buffer(WholeDocumentRange(), GetParam());
  const auto results = buffer.FindMatches("find", FindOptions());
  ASSERT_EQ(1u, results.CountForTesting());
  EXPECT_EQ(MatchResultICU({0, 4}), results.front());
}

TEST_P(FindBufferParamTest, WBRTest) {
  SetBodyContent("fi<wbr>nd and fin<wbr>d");
  FindBuffer buffer(WholeDocumentRange(), GetParam());
  const auto results = buffer.FindMatches("find", FindOptions());
  ASSERT_EQ(2u, results.CountForTesting());
}

TEST_P(FindBufferParamTest, InputTest) {
  SetBodyContent("fi<input type='text' id=i1>nd and fin<input type='text'>d");
  FindBuffer buffer(WholeDocumentRange(), GetParam());
  const auto results = buffer.FindMatches("find", FindOptions());
  ASSERT_EQ(0u, results.CountForTesting());
  EXPECT_EQ(buffer.PositionAfterBlock(),
            PositionInFlatTree::FirstPositionInNode(*GetElementById("i1")));
}

TEST_P(FindBufferParamTest, SelectMultipleTest) {
  SetBodyContent("<select multiple><option>find me</option></select>");
  {
    FindBuffer buffer(WholeDocumentRange(), GetParam());
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    EXPECT_EQ(0u, buffer.FindMatches("find", FindOptions()).CountForTesting());
#else
    EXPECT_EQ(1u, buffer.FindMatches("find", FindOptions()).CountForTesting());
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  }
  SetBodyContent("<select size=2><option>find me</option></select>");
  {
    FindBuffer buffer(WholeDocumentRange(), GetParam());
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    EXPECT_EQ(0u, buffer.FindMatches("find", FindOptions()).CountForTesting());
#else
    EXPECT_EQ(1u, buffer.FindMatches("find", FindOptions()).CountForTesting());
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  }
  SetBodyContent("<select size=1><option>find me</option></select>");
  {
    FindBuffer buffer(WholeDocumentRange(), GetParam());
    EXPECT_EQ(0u, buffer.FindMatches("find", FindOptions()).CountForTesting());
  }
}

TEST_P(FindBufferParamTest, NullRange) {
  SetBodyContent("x<div></div>");
  FindBuffer buffer(WholeDocumentRange(), GetParam());
  EXPECT_EQ(0u, buffer.FindMatches("find", FindOptions()).CountForTesting());
}

TEST_P(FindBufferParamTest, FindObjectReplacementCharacter) {
  SetBodyContent(
      "some text with <script></script> and \uFFFC (object replacement "
      "character)");
  FindBuffer buffer(WholeDocumentRange(), GetParam());
  const auto results = buffer.FindMatches(u"\uFFFC", FindOptions());
  ASSERT_EQ(1u, results.CountForTesting());
}

TEST_P(FindBufferParamTest,
       FindMaxCodepointWithReplacedElementAndMaxCodepointUTF32) {
  SetBodyContent(
      "some text with <img/> <script></script> and \U0010FFFF (max codepoint)");
  FindBuffer buffer(WholeDocumentRange(), GetParam());
  const auto results = buffer.FindMatches(u"\U0010FFFF", FindOptions());
  ASSERT_EQ(1u, results.CountForTesting());
}

TEST_P(FindBufferParamTest, FindMaxCodepointNormalTextUTF32) {
  SetBodyContent("some text");
  FindBuffer buffer(WholeDocumentRange(), GetParam());
  const auto results = buffer.FindMatches(u"\U0010FFFF", FindOptions());
  ASSERT_EQ(0u, results.CountForTesting());
}

TEST_P(FindBufferParamTest, FindMaxCodepointWithReplacedElementUTF32) {
  SetBodyContent("some text with <img/> <script></script>");
  FindBuffer buffer(WholeDocumentRange(), GetParam());
  const auto results = buffer.FindMatches(u"\U0010FFFF", FindOptions());
  ASSERT_EQ(0u, results.CountForTesting());
}

TEST_P(FindBufferParamTest,
       FindNonCharacterWithReplacedElementAndNonCharacterUTF16) {
  SetBodyContent(
      "some text with <img/> <scrip></script> and \uFFFF (non character)");
  FindBuffer buffer(WholeDocumentRange(), GetParam());
  const auto results = buffer.FindMatches(u"\uFFFF", FindOptions());
  ASSERT_EQ(1u, results.CountForTesting());
}

TEST_P(FindBufferParamTest, FindNonCharacterNormalTextUTF16) {
  SetBodyContent("some text");
  FindBuffer buffer(WholeDocumentRange(), GetParam());
  const auto results = buffer.FindMatches(u"\uFFFF", FindOptions());
  ASSERT_EQ(0u, results.CountForTesting());
}

TEST_P(FindBufferParamTest, FindNonCharacterWithReplacedElementUTF16) {
  SetBodyContent("some text with <img/> <script></script>");
  FindBuffer buffer(WholeDocumentRange(), GetParam());
  const auto results = buffer.FindMatches(u"\uFFFF", FindOptions());
  ASSERT_EQ(0u, results.CountForTesting());
}

// Tests that a suggested value is not found by searches.
TEST_P(FindBufferParamTest, DoNotSearchInSuggestedValues) {
  SetBodyContent("<input name='field' type='text'>");

  // The first node of the document should be the input field.
  Node* input_element = GetDocument().body()->firstChild();
  ASSERT_TRUE(IsA<TextControlElement>(*input_element));
  TextControlElement& text_control_element =
      To<TextControlElement>(*input_element);
  ASSERT_EQ(text_control_element.NameForAutofill(), "field");

  // The suggested value to a string that contains the search string.
  text_control_element.SetSuggestedValue("aba");
  ASSERT_EQ(text_control_element.SuggestedValue(), "aba");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  {
    // Apply a search for 'aba'.
    FindBuffer buffer(WholeDocumentRange(), GetParam());
    const auto results = buffer.FindMatches("aba", FindOptions());

    // There should be no result because the suggested value is not supposed to
    // be considered in a search.
    EXPECT_EQ(0U, results.CountForTesting());
  }
  // Convert the suggested value to an autofill value.
  text_control_element.SetAutofillValue(text_control_element.SuggestedValue());
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  {
    // Apply a search for 'aba' again.
    FindBuffer buffer(WholeDocumentRange(), GetParam());
    const auto results = buffer.FindMatches("aba", FindOptions());

    // This time, there should be a match.
    EXPECT_EQ(1U, results.CountForTesting());
  }
}

TEST_P(FindBufferParamTest, FindInTable) {
  SetBodyContent(
      "<table id='table'><tbody><tr id='row'><td id='c1'>c1 "
      "<i>i</i></td></tr></tbody></table>");
  FindBuffer buffer(WholeDocumentRange(), GetParam());
  const auto results = buffer.FindMatches("c1", FindOptions());
  ASSERT_EQ(1u, results.CountForTesting());
}

TEST_F(FindBufferTest, IsInSameUninterruptedBlock) {
  SetBodyContent(
      "<div id=outer>a<div id=inner>b</div><i id='styled'>i</i>c</div>");
  Node* text_node_a = GetElementById("outer")->firstChild();
  Node* styled = GetElementById("styled");
  Node* text_node_i = GetElementById("styled")->firstChild();
  Node* text_node_c = GetElementById("outer")->lastChild();
  Node* text_node_b = GetElementById("inner")->firstChild();

  ASSERT_TRUE(
      FindBuffer::IsInSameUninterruptedBlock(*text_node_i, *text_node_c));
  ASSERT_TRUE(FindBuffer::IsInSameUninterruptedBlock(*styled, *text_node_c));
  ASSERT_FALSE(
      FindBuffer::IsInSameUninterruptedBlock(*text_node_a, *text_node_c));
  ASSERT_FALSE(
      FindBuffer::IsInSameUninterruptedBlock(*text_node_a, *text_node_b));
}

TEST_F(FindBufferTest, IsInSameUninterruptedBlock_input) {
  SetBodyContent("<div id='outer'>a<input value='input' id='input'>b</div>");
  Node* text_node_a = GetElementById("outer")->firstChild();
  Node* text_node_b = GetElementById("outer")->lastChild();
  Node* input = GetElementById("input");
  Node* editable_div = FlatTreeTraversal::Next(*input);

  // input elements are followed by an editable div that contains the input
  // field value.
  ASSERT_EQ("input", editable_div->textContent());

  ASSERT_FALSE(
      FindBuffer::IsInSameUninterruptedBlock(*text_node_a, *text_node_b));
  ASSERT_FALSE(FindBuffer::IsInSameUninterruptedBlock(*text_node_a, *input));
  ASSERT_FALSE(
      FindBuffer::IsInSameUninterruptedBlock(*text_node_a, *editable_div));
}

TEST_F(FindBufferTest, IsInSameUninterruptedBlock_table) {
  SetBodyContent(
      "<table id='table'>"
      "<tbody>"
      "<tr id='row'>"
      "  <td id='c1'>c1</td>"
      "  <td id='c2'>c2</td>"
      "  <td id='c3'>c3</td>"
      "</tr>"
      "</tbody>"
      "</table>");
  Node* text_node_1 = GetElementById("c1")->firstChild();
  Node* text_node_3 = GetElementById("c3")->firstChild();

  ASSERT_FALSE(
      FindBuffer::IsInSameUninterruptedBlock(*text_node_1, *text_node_3));
}

TEST_F(FindBufferTest, IsInSameUninterruptedBlock_comment) {
  SetBodyContent(
      "<div id='text'><span id='span1'>abc</span><!--comment--><span "
      "id='span2'>def</span></div>");
  Node* span_1 = GetElementById("span1")->firstChild();
  Node* span_2 = GetElementById("span2")->firstChild();

  ASSERT_TRUE(FindBuffer::IsInSameUninterruptedBlock(*span_1, *span_2));
}

TEST_F(FindBufferTest, GetFirstBlockLevelAncestorInclusive) {
  SetBodyContent("<div id=outer>a<div id=inner>b</div>c</div>");
  Node* outer_div = GetElementById("outer");
  Node* text_node_a = GetElementById("outer")->firstChild();
  Node* text_node_c = GetElementById("outer")->lastChild();
  Node* inner_div = GetElementById("inner");
  Node* text_node_b = GetElementById("inner")->firstChild();

  ASSERT_EQ(outer_div,
            FindBuffer::GetFirstBlockLevelAncestorInclusive(*text_node_a));
  ASSERT_EQ(outer_div,
            FindBuffer::GetFirstBlockLevelAncestorInclusive(*text_node_c));
  ASSERT_EQ(inner_div,
            FindBuffer::GetFirstBlockLevelAncestorInclusive(*text_node_b));
}

TEST_F(FindBufferTest, ForwardVisibleTextNode) {
  SetBodyContent("\n<div>\n<p>a</p></div");
  Node* text = FindBuffer::ForwardVisibleTextNode(*GetDocument().body());
  ASSERT_TRUE(text);
  EXPECT_TRUE(IsA<Text>(*text));
  EXPECT_EQ(String("a"), To<Text>(text)->data());
}

static String ReplaceZero(const String& chars) {
  String result(chars);
  result.Replace(0, '_');
  return result;
}

TEST_F(FindBufferTest, RubyBuffersBasic) {
  SetBodyContent(
      "<p id=container>before <ruby id=r>base<rt>anno</ruby> after</p>");
  FindBuffer buffer(CreateRange(*GetElementById("r")->firstChild(), 2,
                                *GetElementById("r")->nextSibling(), 4),
                    RubySupport::kEnabledIfNecessary);
  auto buffer_list = buffer.BuffersForTesting();
  ASSERT_EQ(2u, buffer_list.size());
  EXPECT_EQ("se____ aft", ReplaceZero(buffer_list[0]));
  EXPECT_EQ("__anno aft", ReplaceZero(buffer_list[1]));
}

TEST_P(FindBufferParamTest, RubyBuffersEndAtTextInRuby) {
  SetBodyContent(
      "<p id=p>before <ruby id=r>base<rt id=rt>anno</rt>base2</ruby> "
      "after</p>");
  FindBuffer buffer(CreateRange(*GetElementById("p"), 0,
                                *GetElementById("rt")->firstChild(), 2),
                    GetParam());
  EXPECT_EQ(PositionInFlatTree::FirstPositionInNode(
                *GetElementById("rt")->nextSibling()),
            buffer.PositionAfterBlock());
}

TEST_P(FindBufferParamTest, RubyBuffersEndAtDisplayNoneTextInRuby) {
  SetBodyContent(
      "<p id=p>before <ruby id=r>base <span style='display:none' "
      "id=none>text</span> base<rt>anno</ruby> after</p>");
  FindBuffer buffer(CreateRange(*GetElementById("p"), 0,
                                *GetElementById("none")->firstChild(), 2),
                    GetParam());
  EXPECT_EQ(PositionInFlatTree::FirstPositionInNode(
                *GetElementById("none")->nextSibling()),
            buffer.PositionAfterBlock());
}

TEST_F(FindBufferTest, RubyBuffersNoRt) {
  SetBodyContent("<p>before <rub>base</ruby> after</p>");
  FindBuffer buffer(WholeDocumentRange(), RubySupport::kEnabledIfNecessary);
  auto buffer_list = buffer.BuffersForTesting();
  ASSERT_EQ(1u, buffer_list.size());
  EXPECT_EQ("before base after", ReplaceZero(buffer_list[0]));
}

TEST_F(FindBufferTest, RubyBuffersEmptyRt) {
  SetBodyContent("<p>before <ruby>base<rt></ruby> after</p>");
  FindBuffer buffer(WholeDocumentRange(), RubySupport::kEnabledIfNecessary);
  auto buffer_list = buffer.BuffersForTesting();
  ASSERT_EQ(1u, buffer_list.size());
  EXPECT_EQ("before base after", ReplaceZero(buffer_list[0]));
}

TEST_F(FindBufferTest, RubyBuffersEmptyRuby) {
  SetBodyContent("<p>before <ruby><rt>anno</ruby> after</p>");
  FindBuffer buffer(WholeDocumentRange(), RubySupport::kEnabledIfNecessary);
  auto buffer_list = buffer.BuffersForTesting();
  ASSERT_EQ(2u, buffer_list.size());
  EXPECT_EQ("before ____ after", ReplaceZero(buffer_list[0]));
  EXPECT_EQ("before anno after", ReplaceZero(buffer_list[1]));
}

TEST_F(FindBufferTest, RubyBuffersNoRuby) {
  SetBodyContent(
      "<p>before <span style='display:ruby-text'>anno</span> after</p>");
  FindBuffer buffer(WholeDocumentRange(), RubySupport::kEnabledIfNecessary);
  auto buffer_list = buffer.BuffersForTesting();
  ASSERT_EQ(2u, buffer_list.size());
  EXPECT_EQ("before ____ after", ReplaceZero(buffer_list[0]));
  EXPECT_EQ("before anno after", ReplaceZero(buffer_list[1]));
}

TEST_F(FindBufferTest, RubyBuffersNonChildRt) {
  SetBodyContent(
      "<p>before <ruby>base <b><span "
      "style='display:ruby-text'>anno</span></b></ruby> after</p>");
  FindBuffer buffer(WholeDocumentRange(), RubySupport::kEnabledIfNecessary);
  auto buffer_list = buffer.BuffersForTesting();
  ASSERT_EQ(2u, buffer_list.size());
  EXPECT_EQ("before base ____ after", ReplaceZero(buffer_list[0]));
  EXPECT_EQ("before base anno after", ReplaceZero(buffer_list[1]));
}

TEST_F(FindBufferTest, RubyBuffersDisplayContents) {
  SetBodyContent(
      "<p>before <ruby>base <b style='display:contents'><span "
      "style='display:ruby-text'>anno</span></b></ruby> after</p>");
  FindBuffer buffer(WholeDocumentRange(), RubySupport::kEnabledIfNecessary);
  auto buffer_list = buffer.BuffersForTesting();
  ASSERT_EQ(2u, buffer_list.size());
  EXPECT_EQ("before base ____ after", ReplaceZero(buffer_list[0]));
  EXPECT_EQ("before _____anno after", ReplaceZero(buffer_list[1]));
}

TEST_F(FindBufferTest, RubyBuffersVisibility) {
  SetBodyContent(
      "<p>before "
      "<ruby style='visibility:hidden'>base1<rt>anno1</ruby> "
      "<ruby>base2<rt style='visibility:hidden'>anno2</ruby> "
      "<ruby style='visibility:hidden'>base3"
      "<rt style='visibility:visible'>anno3</ruby> "
      "after</p>");
  FindBuffer buffer(WholeDocumentRange(), RubySupport::kEnabledIfNecessary);
  auto buffer_list = buffer.BuffersForTesting();
  ASSERT_EQ(2u, buffer_list.size());
  EXPECT_EQ("before  base2 _____ after", ReplaceZero(buffer_list[0]));
  EXPECT_EQ("before  _____ anno3 after", ReplaceZero(buffer_list[1]));
}

TEST_F(FindBufferTest, RubyBuffersBlockRuby) {
  SetBodyContent(
      "<p>before <ruby id=r style='display:block ruby'>base<rt>anno</ruby> "
      "after</p>");
  FindBuffer buffer(CreateRange(*GetElementById("r")->firstChild(), 2,
                                *GetElementById("r")->nextSibling(), 4),
                    RubySupport::kEnabledIfNecessary);
  auto buffer_list = buffer.BuffersForTesting();
  ASSERT_EQ(2u, buffer_list.size());
  // The range end position is in " after", but the FindBuffer should have an
  // IFC scope, which is <ruby> in this case.
  EXPECT_EQ("se____", ReplaceZero(buffer_list[0]));
  EXPECT_EQ("__anno", ReplaceZero(buffer_list[1]));
}

TEST_F(FindBufferTest, FindRuby) {
  SetBodyContent(
      "<p>残暑お<ruby>見<rt>み</ruby><ruby>舞<rt>ま</ruby>い"
      "申し上げます。</p>");
  {
    FindBuffer buffer(WholeDocumentRange(), RubySupport::kEnabledIfNecessary);
    const auto results = buffer.FindMatches(u"おみまい", FindOptions());
    EXPECT_EQ(1u, results.CountForTesting());
  }
  {
    FindBuffer buffer(WholeDocumentRange(), RubySupport::kEnabledIfNecessary);
    const auto results = buffer.FindMatches(u"お見舞い", FindOptions());
    EXPECT_EQ(1u, results.CountForTesting());
  }
  {
    FindBuffer buffer(WholeDocumentRange(), RubySupport::kEnabledIfNecessary);
    EXPECT_EQ(0u, CaseInsensitiveMatchCount(buffer, u"お見まい"));
  }
}

TEST_F(FindBufferTest, FindRubyNested) {
  SetBodyContent(
      "<p>の<ruby><ruby>超電磁砲<rt>レールガン</ruby><rt>railgun</ruby></p>");
  {
    FindBuffer buffer(WholeDocumentRange(), RubySupport::kEnabledIfNecessary);
    EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, u"の超"));
  }
  {
    FindBuffer buffer(WholeDocumentRange(), RubySupport::kEnabledIfNecessary);
    EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, u"のれーる"));
  }
  {
    FindBuffer buffer(WholeDocumentRange(), RubySupport::kEnabledIfNecessary);
    EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, u"のRAIL"));
  }
}

TEST_F(FindBufferTest, FindRubyOnAnnotation) {
  SetBodyContent(
      "<p>の<ruby>超電磁砲<rt>レール<ruby>ガン<rt>gun</ruby></ruby></p>");
  {
    FindBuffer buffer(WholeDocumentRange(), RubySupport::kEnabledIfNecessary);
    EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, u"の超電磁砲"));
  }
  {
    FindBuffer buffer(WholeDocumentRange(), RubySupport::kEnabledIfNecessary);
    EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, u"のれーるガン"));
  }
  {
    FindBuffer buffer(WholeDocumentRange(), RubySupport::kEnabledIfNecessary);
    EXPECT_EQ(1u, CaseInsensitiveMatchCount(buffer, u"のレールgun"));
  }
}

}  // namespace blink
