// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/inline_caret_position.h"

#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/inline/offset_mapping.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class InlineCaretPositionTest : public RenderingTest {
 public:
  void SetUp() override {
    RenderingTest::SetUp();
    LoadAhem();
  }

 protected:
  void SetInlineFormattingContext(const char* id,
                                  const char* html,
                                  unsigned width,
                                  TextDirection dir = TextDirection::kLtr,
                                  const char* style = nullptr) {
    InsertStyleElement(
        "body { font: 10px/10px Ahem;  }"
        "bdo { display:block; }");
    const char* pattern =
        dir == TextDirection::kLtr
            ? "<div id='%s' style='width: %u0px; %s'>%s</div>"
            : "<bdo dir=rtl id='%s' style='width: %u0px; %s'>%s</bdo>";
    SetBodyInnerHTML(String::Format(
        pattern, id, width, style ? style : "word-break: break-all", html));
    container_ = GetElementById(id);
    DCHECK(container_);
    context_ = To<LayoutBlockFlow>(container_->GetLayoutObject());
    DCHECK(context_);
    DCHECK(context_->IsLayoutNGObject());
  }

  InlineCaretPosition ComputeInlineCaretPosition(unsigned offset,
                                                 TextAffinity affinity) const {
    return blink::ComputeInlineCaretPosition(*context_, offset, affinity);
  }

  InlineCursor FragmentOf(const Node* node) const {
    InlineCursor cursor;
    cursor.MoveTo(*node->GetLayoutObject());
    return cursor;
  }

  Persistent<Element> container_;
  Persistent<const LayoutBlockFlow> context_;
};

#define TEST_CARET(caret, fragment_, type_, offset_)                         \
  {                                                                          \
    EXPECT_EQ(caret.cursor, fragment_);                                      \
    EXPECT_EQ(caret.position_type, InlineCaretPositionType::type_);          \
    EXPECT_EQ(caret.text_offset, offset_) << caret.text_offset.value_or(-1); \
  }

TEST_F(InlineCaretPositionTest, AfterSpan) {
  InsertStyleElement("b { background-color: yellow; }");
  SetBodyInnerHTML("<div><b id=target>ABC</b></div>");
  const auto& target = *GetElementById("target");

  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(Position::AfterNode(target))),
             FragmentOf(&target), kAfterBox, std::nullopt);
}

TEST_F(InlineCaretPositionTest, AfterSpanCulled) {
  SetBodyInnerHTML("<div><b id=target>ABC</b></div>");
  const auto& target = *GetElementById("target");

  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(Position::AfterNode(target))),
             FragmentOf(target.firstChild()), kAtTextOffset,
             std::optional<unsigned>(3));
}

TEST_F(InlineCaretPositionTest, InlineCaretPositionInOneLineOfText) {
  SetInlineFormattingContext("t", "foo", 3);
  const Node* text = container_->firstChild();
  const InlineCursor& text_fragment = FragmentOf(text);

  // Beginning of line
  TEST_CARET(ComputeInlineCaretPosition(0, TextAffinity::kDownstream),
             text_fragment, kAtTextOffset, std::optional<unsigned>(0));
  TEST_CARET(ComputeInlineCaretPosition(0, TextAffinity::kUpstream),
             text_fragment, kAtTextOffset, std::optional<unsigned>(0));

  // Middle in the line
  TEST_CARET(ComputeInlineCaretPosition(1, TextAffinity::kDownstream),
             text_fragment, kAtTextOffset, std::optional<unsigned>(1));
  TEST_CARET(ComputeInlineCaretPosition(1, TextAffinity::kUpstream),
             text_fragment, kAtTextOffset, std::optional<unsigned>(1));

  // End of line
  TEST_CARET(ComputeInlineCaretPosition(3, TextAffinity::kDownstream),
             text_fragment, kAtTextOffset, std::optional<unsigned>(3));
  TEST_CARET(ComputeInlineCaretPosition(3, TextAffinity::kUpstream),
             text_fragment, kAtTextOffset, std::optional<unsigned>(3));
}

// For http://crbug.com/1021993
// We should not call |InlineCursor::CurrentBidiLevel()| for soft hyphen
TEST_F(InlineCaretPositionTest, InlineCaretPositionAtSoftHyphen) {
  // We have three fragment "foo\u00AD", "\u2010", "bar"
  SetInlineFormattingContext("t", "foo&shy;bar", 3, TextDirection::kLtr, "");
  const LayoutText& text =
      *To<Text>(container_->firstChild())->GetLayoutObject();
  InlineCursor cursor;
  cursor.MoveTo(text);
  const InlineCursor foo_fragment = cursor;

  TEST_CARET(ComputeInlineCaretPosition(4, TextAffinity::kDownstream),
             foo_fragment, kAtTextOffset, std::optional<unsigned>(4));
  TEST_CARET(ComputeInlineCaretPosition(4, TextAffinity::kUpstream),
             foo_fragment, kAtTextOffset, std::optional<unsigned>(4));
}

TEST_F(InlineCaretPositionTest, InlineCaretPositionAtSoftLineWrap) {
  SetInlineFormattingContext("t", "foobar", 3);
  const LayoutText& text =
      *To<Text>(container_->firstChild())->GetLayoutObject();
  InlineCursor cursor;
  cursor.MoveTo(text);
  const InlineCursor foo_fragment = cursor;
  cursor.MoveToNextForSameLayoutObject();
  const InlineCursor bar_fragment = cursor;

  TEST_CARET(ComputeInlineCaretPosition(3, TextAffinity::kDownstream),
             bar_fragment, kAtTextOffset, std::optional<unsigned>(3));
  TEST_CARET(ComputeInlineCaretPosition(3, TextAffinity::kUpstream),
             foo_fragment, kAtTextOffset, std::optional<unsigned>(3));
}

TEST_F(InlineCaretPositionTest, InlineCaretPositionAtSoftLineWrapWithSpace) {
  SetInlineFormattingContext("t", "foo bar", 3);
  const LayoutText& text =
      *To<Text>(container_->firstChild())->GetLayoutObject();
  InlineCursor cursor;
  cursor.MoveTo(text);
  const InlineCursor foo_fragment = cursor;
  cursor.MoveToNextForSameLayoutObject();
  const InlineCursor bar_fragment = cursor;

  // Before the space
  TEST_CARET(ComputeInlineCaretPosition(3, TextAffinity::kDownstream),
             foo_fragment, kAtTextOffset, std::optional<unsigned>(3));
  TEST_CARET(ComputeInlineCaretPosition(3, TextAffinity::kUpstream),
             foo_fragment, kAtTextOffset, std::optional<unsigned>(3));

  // After the space
  TEST_CARET(ComputeInlineCaretPosition(4, TextAffinity::kDownstream),
             bar_fragment, kAtTextOffset, std::optional<unsigned>(4));
  TEST_CARET(ComputeInlineCaretPosition(4, TextAffinity::kUpstream),
             bar_fragment, kAtTextOffset, std::optional<unsigned>(4));
}

TEST_F(InlineCaretPositionTest, InlineCaretPositionAtForcedLineBreak) {
  SetInlineFormattingContext("t", "foo<br>bar", 3);
  const Node* foo = container_->firstChild();
  const Node* br = foo->nextSibling();
  const Node* bar = br->nextSibling();
  const InlineCursor& foo_fragment = FragmentOf(foo);
  const InlineCursor& bar_fragment = FragmentOf(bar);

  // Before the BR
  TEST_CARET(ComputeInlineCaretPosition(3, TextAffinity::kDownstream),
             foo_fragment, kAtTextOffset, std::optional<unsigned>(3));
  TEST_CARET(ComputeInlineCaretPosition(3, TextAffinity::kUpstream),
             foo_fragment, kAtTextOffset, std::optional<unsigned>(3));

  // After the BR
  TEST_CARET(ComputeInlineCaretPosition(4, TextAffinity::kDownstream),
             bar_fragment, kAtTextOffset, std::optional<unsigned>(4));
  TEST_CARET(ComputeInlineCaretPosition(4, TextAffinity::kUpstream),
             bar_fragment, kAtTextOffset, std::optional<unsigned>(4));
}

TEST_F(InlineCaretPositionTest, InlineCaretPositionAtEmptyLine) {
  SetInlineFormattingContext("f", "foo<br><br>bar", 3);
  const Node* foo = container_->firstChild();
  const Node* br1 = foo->nextSibling();
  const Node* br2 = br1->nextSibling();
  const InlineCursor& br2_fragment = FragmentOf(br2);

  TEST_CARET(ComputeInlineCaretPosition(4, TextAffinity::kDownstream),
             br2_fragment, kAtTextOffset, std::optional<unsigned>(4));
  TEST_CARET(ComputeInlineCaretPosition(4, TextAffinity::kUpstream),
             br2_fragment, kAtTextOffset, std::optional<unsigned>(4));
}

TEST_F(InlineCaretPositionTest, InlineCaretPositionInOneLineOfImage) {
  SetInlineFormattingContext("t", "<img>", 3);
  const Node* img = container_->firstChild();
  const InlineCursor& img_fragment = FragmentOf(img);

  // Before the image
  TEST_CARET(ComputeInlineCaretPosition(0, TextAffinity::kDownstream),
             img_fragment, kBeforeBox, std::nullopt);
  TEST_CARET(ComputeInlineCaretPosition(0, TextAffinity::kUpstream),
             img_fragment, kBeforeBox, std::nullopt);

  // After the image
  TEST_CARET(ComputeInlineCaretPosition(1, TextAffinity::kDownstream),
             img_fragment, kAfterBox, std::nullopt);
  TEST_CARET(ComputeInlineCaretPosition(1, TextAffinity::kUpstream),
             img_fragment, kAfterBox, std::nullopt);
}

TEST_F(InlineCaretPositionTest,
       InlineCaretPositionAtSoftLineWrapBetweenImages) {
  SetInlineFormattingContext("t",
                             "<img id=img1><img id=img2>"
                             "<style>img{width: 1em; height: 1em}</style>",
                             1);
  const Node* img1 = container_->firstChild();
  const Node* img2 = img1->nextSibling();
  const InlineCursor& img1_fragment = FragmentOf(img1);
  const InlineCursor& img2_fragment = FragmentOf(img2);

  TEST_CARET(ComputeInlineCaretPosition(1, TextAffinity::kDownstream),
             img2_fragment, kBeforeBox, std::nullopt);
  TEST_CARET(ComputeInlineCaretPosition(1, TextAffinity::kUpstream),
             img1_fragment, kAfterBox, std::nullopt);
}

TEST_F(InlineCaretPositionTest,
       InlineCaretPositionAtSoftLineWrapBetweenMultipleTextNodes) {
  SetInlineFormattingContext("t",
                             "<span>A</span>"
                             "<span>B</span>"
                             "<span id=span-c>C</span>"
                             "<span id=span-d>D</span>"
                             "<span>E</span>"
                             "<span>F</span>",
                             3);
  const Node* text_c = GetElementById("span-c")->firstChild();
  const Node* text_d = GetElementById("span-d")->firstChild();
  const InlineCursor& fragment_c = FragmentOf(text_c);
  const InlineCursor& fragment_d = FragmentOf(text_d);

  const Position wrap_position(text_c, 1);
  const OffsetMapping& mapping = *OffsetMapping::GetFor(wrap_position);
  const unsigned wrap_offset = *mapping.GetTextContentOffset(wrap_position);

  TEST_CARET(ComputeInlineCaretPosition(wrap_offset, TextAffinity::kUpstream),
             fragment_c, kAtTextOffset, std::optional<unsigned>(wrap_offset));
  TEST_CARET(ComputeInlineCaretPosition(wrap_offset, TextAffinity::kDownstream),
             fragment_d, kAtTextOffset, std::optional<unsigned>(wrap_offset));
}

TEST_F(InlineCaretPositionTest,
       InlineCaretPositionAtSoftLineWrapBetweenMultipleTextNodesRtl) {
  SetInlineFormattingContext("t",
                             "<span>A</span>"
                             "<span>B</span>"
                             "<span id=span-c>C</span>"
                             "<span id=span-d>D</span>"
                             "<span>E</span>"
                             "<span>F</span>",
                             3, TextDirection::kRtl);
  const Node* text_c = GetElementById("span-c")->firstChild();
  const Node* text_d = GetElementById("span-d")->firstChild();
  const InlineCursor& fragment_c = FragmentOf(text_c);
  const InlineCursor& fragment_d = FragmentOf(text_d);

  const Position wrap_position(text_c, 1);
  const OffsetMapping& mapping = *OffsetMapping::GetFor(wrap_position);
  const unsigned wrap_offset = *mapping.GetTextContentOffset(wrap_position);

  TEST_CARET(ComputeInlineCaretPosition(wrap_offset, TextAffinity::kUpstream),
             fragment_c, kAtTextOffset, std::optional<unsigned>(wrap_offset));
  TEST_CARET(ComputeInlineCaretPosition(wrap_offset, TextAffinity::kDownstream),
             fragment_d, kAtTextOffset, std::optional<unsigned>(wrap_offset));
}

TEST_F(InlineCaretPositionTest,
       InlineCaretPositionAtSoftLineWrapBetweenDeepTextNodes) {
  SetInlineFormattingContext(
      "t",
      "<style>span {border: 1px solid black}</style>"
      "<span>A</span>"
      "<span>B</span>"
      "<span id=span-c>C</span>"
      "<span id=span-d>D</span>"
      "<span>E</span>"
      "<span>F</span>",
      4);  // Wider space to allow border and 3 characters
  const Node* text_c = GetElementById("span-c")->firstChild();
  const Node* text_d = GetElementById("span-d")->firstChild();
  const InlineCursor& fragment_c = FragmentOf(text_c);
  const InlineCursor& fragment_d = FragmentOf(text_d);

  const Position wrap_position(text_c, 1);
  const OffsetMapping& mapping = *OffsetMapping::GetFor(wrap_position);
  const unsigned wrap_offset = *mapping.GetTextContentOffset(wrap_position);

  TEST_CARET(ComputeInlineCaretPosition(wrap_offset, TextAffinity::kUpstream),
             fragment_c, kAtTextOffset, std::optional<unsigned>(wrap_offset));
  TEST_CARET(ComputeInlineCaretPosition(wrap_offset, TextAffinity::kDownstream),
             fragment_d, kAtTextOffset, std::optional<unsigned>(wrap_offset));
}

TEST_F(InlineCaretPositionTest, GeneratedZeroWidthSpace) {
  LoadAhem();
  InsertStyleElement(
      "p { font: 10px/1 Ahem; }"
      "p { width: 4ch; white-space: pre-wrap;");
  // We have ZWS before "abc" due by "pre-wrap".
  // text content is
  //    [0..3] "   "
  //    [4] ZWS
  //    [5..8] "abcd"
  SetBodyInnerHTML("<p id=t>    abcd</p>");
  const Text& text = To<Text>(*GetElementById("t")->firstChild());
  const Position after_zws(text, 4);  // before "a".

  InlineCursor cursor;
  cursor.MoveTo(*text.GetLayoutObject());

  ASSERT_EQ(TextOffsetRange(0, 4), cursor.Current().TextOffset());
  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(after_zws, TextAffinity::kUpstream)),
             cursor, kAtTextOffset, std::optional<unsigned>(4));

  cursor.MoveToNextForSameLayoutObject();
  ASSERT_EQ(TextOffsetRange(5, 9), cursor.Current().TextOffset());
  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(after_zws, TextAffinity::kDownstream)),
             cursor, kAtTextOffset, std::optional<unsigned>(5));
}

// See also ParameterizedLocalCaretRectTest.MultiColumnSingleText
TEST_F(InlineCaretPositionTest, MultiColumnSingleText) {
  LoadAhem();
  InsertStyleElement(
      "div { font: 10px/15px Ahem; column-count: 3; width: 20ch; }");
  SetBodyInnerHTML("<div id=target>abc def ghi jkl mno pqr</div>");
  // This HTML is rendered as:
  //    abc ghi mno
  //    def jkl
  const auto& target = *GetElementById("target");
  const Text& text = *To<Text>(target.firstChild());

  InlineCursor cursor;
  cursor.MoveTo(*text.GetLayoutObject());

  // "abc " in column 1
  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(Position(text, 0))),
             cursor, kAtTextOffset, std::optional<unsigned>(0));
  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(Position(text, 1))),
             cursor, kAtTextOffset, std::optional<unsigned>(1));
  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(Position(text, 2))),
             cursor, kAtTextOffset, std::optional<unsigned>(2));
  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(Position(text, 3))),
             cursor, kAtTextOffset, std::optional<unsigned>(3));
  cursor.MoveToNextForSameLayoutObject();

  // "def " in column 1
  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(Position(text, 4))),
             cursor, kAtTextOffset, std::optional<unsigned>(4));
  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(Position(text, 5))),
             cursor, kAtTextOffset, std::optional<unsigned>(5));
  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(Position(text, 6))),
             cursor, kAtTextOffset, std::optional<unsigned>(6));
  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(Position(text, 7))),
             cursor, kAtTextOffset, std::optional<unsigned>(7));
  cursor.MoveToNextForSameLayoutObject();

  // "ghi " in column 2
  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(Position(text, 8))),
             cursor, kAtTextOffset, std::optional<unsigned>(8));
  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(Position(text, 9))),
             cursor, kAtTextOffset, std::optional<unsigned>(9));
  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(Position(text, 10))),
             cursor, kAtTextOffset, std::optional<unsigned>(10));
  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(Position(text, 11))),
             cursor, kAtTextOffset, std::optional<unsigned>(11));
  cursor.MoveToNextForSameLayoutObject();

  // "jkl " in column 2
  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(Position(text, 12))),
             cursor, kAtTextOffset, std::optional<unsigned>(12));
  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(Position(text, 13))),
             cursor, kAtTextOffset, std::optional<unsigned>(13));
  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(Position(text, 14))),
             cursor, kAtTextOffset, std::optional<unsigned>(14));
  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(Position(text, 15))),
             cursor, kAtTextOffset, std::optional<unsigned>(15));
  cursor.MoveToNextForSameLayoutObject();

  // "mno " in column 3
  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(Position(text, 16))),
             cursor, kAtTextOffset, std::optional<unsigned>(16));
  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(Position(text, 17))),
             cursor, kAtTextOffset, std::optional<unsigned>(17));
  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(Position(text, 18))),
             cursor, kAtTextOffset, std::optional<unsigned>(18));
  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(Position(text, 19))),
             cursor, kAtTextOffset, std::optional<unsigned>(19));
  cursor.MoveToNextForSameLayoutObject();

  // "pqr" in column 3
  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(Position(text, 20))),
             cursor, kAtTextOffset, std::optional<unsigned>(20));
  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(Position(text, 21))),
             cursor, kAtTextOffset, std::optional<unsigned>(21));
  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(Position(text, 22))),
             cursor, kAtTextOffset, std::optional<unsigned>(22));
  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(Position(text, 23))),
             cursor, kAtTextOffset, std::optional<unsigned>(23));
  cursor.MoveToNextForSameLayoutObject();
}

// http://crbug.com/1183269
// See also InlineCaretPositionTest.InlineCaretPositionAtSoftLineWrap
TEST_F(InlineCaretPositionTest, SoftLineWrap) {
  LoadAhem();
  InsertStyleElement(
      "p { font: 10px/1 Ahem; }"
      "p { width: 4ch;");
  // Note: "contenteditable" adds
  //    line-break: after-white-space;
  //    overflow-wrap: break-word;
  SetBodyInnerHTML("<p id=t contenteditable>abc xyz</p>");
  const Text& text = To<Text>(*GetElementById("t")->firstChild());
  const Position before_xyz(text, 4);  // before "w".

  InlineCursor cursor;
  cursor.MoveTo(*text.GetLayoutObject());

  // Note: upstream/downstream before "xyz" are in different line.

  ASSERT_EQ(TextOffsetRange(0, 3), cursor.Current().TextOffset());
  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(before_xyz, TextAffinity::kUpstream)),
             cursor, kAtTextOffset, std::optional<unsigned>(3));

  cursor.MoveToNextForSameLayoutObject();
  ASSERT_EQ(TextOffsetRange(4, 7), cursor.Current().TextOffset());
  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(before_xyz, TextAffinity::kDownstream)),
             cursor, kAtTextOffset, std::optional<unsigned>(4));
}

TEST_F(InlineCaretPositionTest, ZeroWidthSpace) {
  LoadAhem();
  InsertStyleElement(
      "p { font: 10px/1 Ahem; }"
      "p { width: 4ch;");
  // dom and text content is
  //    [0..3] "abcd"
  //    [4] ZWS
  //    [5..8] "wxyz"
  SetBodyInnerHTML("<p id=t>abcd&#x200B;wxyz</p>");
  const Text& text = To<Text>(*GetElementById("t")->firstChild());
  const Position after_zws(text, 5);  // before "w".

  InlineCursor cursor;
  cursor.MoveTo(*text.GetLayoutObject());

  ASSERT_EQ(TextOffsetRange(0, 5), cursor.Current().TextOffset());
  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(after_zws, TextAffinity::kUpstream)),
             cursor, kAtTextOffset, std::optional<unsigned>(4));

  cursor.MoveToNextForSameLayoutObject();
  ASSERT_EQ(TextOffsetRange(5, 9), cursor.Current().TextOffset());
  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(after_zws, TextAffinity::kDownstream)),
             cursor, kAtTextOffset, std::optional<unsigned>(5));
}

TEST_F(InlineCaretPositionTest, InlineBlockBeforeContent) {
  SetInlineFormattingContext(
      "t",
      "<style>span::before{display:inline-block; content:'foo'}</style>"
      "<span id=span>bar</span>",
      100);  // Line width doesn't matter here.
  const Node* text = GetElementById("span")->firstChild();
  const InlineCursor& text_fragment = FragmentOf(text);

  // Test caret position of "|bar", which shouldn't be affected by ::before
  const Position position(text, 0);
  const OffsetMapping& mapping = *OffsetMapping::GetFor(position);
  const unsigned text_offset = *mapping.GetTextContentOffset(position);

  TEST_CARET(ComputeInlineCaretPosition(text_offset, TextAffinity::kDownstream),
             text_fragment, kAtTextOffset,
             std::optional<unsigned>(text_offset));
}

TEST_F(InlineCaretPositionTest, InlineBoxesLTR) {
  SetBodyInnerHTML(
      "<div dir=ltr>"
      "<bdo id=box1 dir=ltr>ABCD</bdo>"
      "<bdo id=box2 dir=ltr style='font-size: 150%'>EFG</bdo></div>");

  // text_content:
  //    [0] U+2068 FIRST STRONG ISOLATE
  //    [1] U+202D LEFT-TO_RIGHT_OVERRIDE
  //    [2:5] "ABCD"
  //    [6] U+202C POP DIRECTIONAL FORMATTING
  //    [7] U+2069 POP DIRECTIONAL ISOLATE
  //    [8] U+2068 FIRST STRONG ISOLATE
  //    [9] U+202D LEFT-TO_RIGHT_OVERRIDE
  //    [10:12] "EFG"
  //    [13] U+202C POP DIRECTIONAL FORMATTING
  //    [14] U+2069 POP DIRECTIONAL ISOLATE
  // For details on injected codes, see:
  // https://drafts.csswg.org/css-writing-modes-3/#bidi-control-codes-injection-table
  const Node& box1 = *GetElementById("box1")->firstChild();
  const Node& box2 = *GetElementById("box1")->firstChild();

  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(Position(box1, 4))),
             FragmentOf(&box1), kAtTextOffset, std::optional<unsigned>(6));

  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(Position(box2, 0))),
             FragmentOf(&box2), kAtTextOffset, std::optional<unsigned>(2));
}

TEST_F(InlineCaretPositionTest, InlineBoxesRTL) {
  SetBodyInnerHTML(
      "<div dir=rtl>"
      "<bdo id=box1 dir=rtl>ABCD</bdo>"
      "<bdo id=box2 dir=rtl style='font-size: 150%'>EFG</bdo></div>");

  // text_content:
  //    [0] U+2068 FIRST STRONG ISOLATE
  //    [1] U+202E RIGHT-TO_LEFT _OVERRIDE
  //    [2:5] "ABCD"
  //    [6] U+202C POP DIRECTIONAL FORMATTING
  //    [7] U+2069 POP DIRECTIONAL ISOLATE
  //    [8] U+2068 FIRST STRONG ISOLATE
  //    [9] U+202E RIGHT-TO_LEFT _OVERRIDE
  //    [10:12] "EFG"
  //    [13] U+202C POP DIRECTIONAL FORMATTING
  //    [14] U+2069 POP DIRECTIONAL ISOLATE
  // For details on injected codes, see:
  // https://drafts.csswg.org/css-writing-modes-3/#bidi-control-codes-injection-table
  const Node& box1 = *GetElementById("box1")->firstChild();
  const Node& box2 = *GetElementById("box1")->firstChild();

  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(Position(box1, 4))),
             FragmentOf(&box1), kAtTextOffset, std::optional<unsigned>(6));

  TEST_CARET(blink::ComputeInlineCaretPosition(
                 PositionWithAffinity(Position(box2, 0))),
             FragmentOf(&box2), kAtTextOffset, std::optional<unsigned>(2));
}

// https://crbug.com/1340236
TEST_F(InlineCaretPositionTest, BeforeOrAfterInlineAreaElement) {
  SetBodyInnerHTML("<area id=area>");

  const Node& area = *GetElementById("area");
  const PositionWithAffinity position1(Position::AfterNode(area));
  // DCHECK failure or crash happens here.
  blink::ComputeInlineCaretPosition(position1);

  const PositionWithAffinity position2(Position::BeforeNode(area));
  // DCHECK failure or crash happens here.
  blink::ComputeInlineCaretPosition(position2);
}

}  // namespace blink
