// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_base_layout_algorithm_test.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_breaker.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_positioned_float.h"
#include "third_party/blink/renderer/core/layout/ng/ng_unpositioned_float.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

class NGLineBreakerTest : public NGLayoutTest {
 protected:
  NGInlineNode CreateInlineNode(const String& html_content) {
    SetBodyInnerHTML(html_content);

    LayoutBlockFlow* block_flow =
        To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
    return NGInlineNode(block_flow);
  }

  // Break lines using the specified available width.
  Vector<NGLineInfo> BreakToLineInfo(NGInlineNode node,
                                     LayoutUnit available_width) {
    DCHECK(node);

    node.PrepareLayoutIfNeeded();

    NGConstraintSpaceBuilder builder(WritingMode::kHorizontalTb,
                                     WritingMode::kHorizontalTb,
                                     /* is_new_fc */ false);
    builder.SetAvailableSize({available_width, kIndefiniteSize});
    NGConstraintSpace space = builder.ToConstraintSpace();

    scoped_refptr<NGInlineBreakToken> break_token;

    Vector<NGLineInfo> line_infos;
    trailing_whitespaces_.resize(0);
    NGExclusionSpace exclusion_space;
    NGPositionedFloatVector leading_floats;
    NGLineLayoutOpportunity line_opportunity(available_width);
    while (!break_token || !break_token->IsFinished()) {
      NGLineInfo& line_info = line_infos.emplace_back();
      NGLineBreaker line_breaker(node, NGLineBreakerMode::kContent, space,
                                 line_opportunity, leading_floats, 0u,
                                 break_token.get(), &exclusion_space);
      line_breaker.NextLine(&line_info);
      trailing_whitespaces_.push_back(
          line_breaker.TrailingWhitespaceForTesting());

      if (line_info.Results().IsEmpty())
        break;

      break_token = line_breaker.CreateBreakToken(line_info);
    }

    return line_infos;
  }

  Vector<NGInlineItemResults> BreakLines(NGInlineNode node,
                                         LayoutUnit available_width) {
    Vector<NGLineInfo> line_infos = BreakToLineInfo(node, available_width);
    Vector<NGInlineItemResults> lines;
    for (NGLineInfo& line_info : line_infos)
      lines.push_back(std::move(line_info.Results()));
    return lines;
  }

  Vector<NGLineBreaker::WhitespaceState> trailing_whitespaces_;
};

namespace {

String ToString(NGInlineItemResults line, NGInlineNode node) {
  StringBuilder builder;
  const String& text = node.ItemsData(false).text_content;
  for (const auto& item_result : line) {
    builder.Append(
        StringView(text, item_result.start_offset,
                   item_result.end_offset - item_result.start_offset));
  }
  return builder.ToString();
}

TEST_F(NGLineBreakerTest, SingleNode) {
  LoadAhem();
  NGInlineNode node = CreateInlineNode(R"HTML(
    <!DOCTYPE html>
    <style>
    #container {
      font: 10px/1 Ahem;
    }
    </style>
    <div id=container>123 456 789</div>
  )HTML");

  Vector<NGInlineItemResults> lines;
  lines = BreakLines(node, LayoutUnit(80));
  EXPECT_EQ(2u, lines.size());
  EXPECT_EQ("123 456", ToString(lines[0], node));
  EXPECT_EQ("789", ToString(lines[1], node));

  lines = BreakLines(node, LayoutUnit(60));
  EXPECT_EQ(3u, lines.size());
  EXPECT_EQ("123", ToString(lines[0], node));
  EXPECT_EQ("456", ToString(lines[1], node));
  EXPECT_EQ("789", ToString(lines[2], node));
}

TEST_F(NGLineBreakerTest, OverflowWord) {
  LoadAhem();
  NGInlineNode node = CreateInlineNode(R"HTML(
    <!DOCTYPE html>
    <style>
    #container {
      font: 10px/1 Ahem;
    }
    </style>
    <div id=container>12345 678</div>
  )HTML");

  // The first line overflows, but the last line does not.
  Vector<NGInlineItemResults> lines;
  lines = BreakLines(node, LayoutUnit(40));
  EXPECT_EQ(2u, lines.size());
  EXPECT_EQ("12345", ToString(lines[0], node));
  EXPECT_EQ("678", ToString(lines[1], node));

  // Both lines overflow.
  lines = BreakLines(node, LayoutUnit(20));
  EXPECT_EQ(2u, lines.size());
  EXPECT_EQ("12345", ToString(lines[0], node));
  EXPECT_EQ("678", ToString(lines[1], node));
}

TEST_F(NGLineBreakerTest, OverflowTab) {
  LoadAhem();
  NGInlineNode node = CreateInlineNode(R"HTML(
    <!DOCTYPE html>
    <style>
    #container {
      font: 10px/1 Ahem;
      tab-size: 8;
      white-space: pre-wrap;
      width: 10ch;
    }
    </style>
    <div id=container>12345&#9;&#9;678</div>
  )HTML");

  Vector<NGInlineItemResults> lines;
  lines = BreakLines(node, LayoutUnit(100));
  EXPECT_EQ(2u, lines.size());
  EXPECT_EQ("12345\t\t", ToString(lines[0], node));
  EXPECT_EQ("678", ToString(lines[1], node));
}

TEST_F(NGLineBreakerTest, OverflowTabBreakWord) {
  LoadAhem();
  NGInlineNode node = CreateInlineNode(R"HTML(
    <!DOCTYPE html>
    <style>
    #container {
      font: 10px/1 Ahem;
      tab-size: 8;
      white-space: pre-wrap;
      width: 10ch;
      word-wrap: break-word;
    }
    </style>
    <div id=container>12345&#9;&#9;678</div>
  )HTML");

  Vector<NGInlineItemResults> lines;
  lines = BreakLines(node, LayoutUnit(100));
  EXPECT_EQ(2u, lines.size());
  EXPECT_EQ("12345\t\t", ToString(lines[0], node));
  EXPECT_EQ("678", ToString(lines[1], node));
}

TEST_F(NGLineBreakerTest, OverflowAtomicInline) {
  LoadAhem();
  NGInlineNode node = CreateInlineNode(R"HTML(
    <!DOCTYPE html>
    <style>
    #container {
      font: 10px/1 Ahem;
    }
    span {
      display: inline-block;
      width: 30px;
      height: 10px;
    }
    </style>
    <div id=container>12345<span></span>678</div>
  )HTML");

  Vector<NGInlineItemResults> lines;
  lines = BreakLines(node, LayoutUnit(80));
  EXPECT_EQ(2u, lines.size());
  EXPECT_EQ(String(u"12345\uFFFC"), ToString(lines[0], node));
  EXPECT_EQ("678", ToString(lines[1], node));

  lines = BreakLines(node, LayoutUnit(70));
  EXPECT_EQ(2u, lines.size());
  EXPECT_EQ("12345", ToString(lines[0], node));
  EXPECT_EQ(String(u"\uFFFC678"), ToString(lines[1], node));

  lines = BreakLines(node, LayoutUnit(40));
  EXPECT_EQ(3u, lines.size());
  EXPECT_EQ("12345", ToString(lines[0], node));
  EXPECT_EQ(String(u"\uFFFC"), ToString(lines[1], node));
  EXPECT_EQ("678", ToString(lines[2], node));

  lines = BreakLines(node, LayoutUnit(20));
  EXPECT_EQ(3u, lines.size());
  EXPECT_EQ("12345", ToString(lines[0], node));
  EXPECT_EQ(String(u"\uFFFC"), ToString(lines[1], node));
  EXPECT_EQ("678", ToString(lines[2], node));
}

TEST_F(NGLineBreakerTest, OverflowMargin) {
  LoadAhem();
  NGInlineNode node = CreateInlineNode(R"HTML(
    <!DOCTYPE html>
    <style>
    #container {
      font: 10px/1 Ahem;
    }
    span {
      margin-right: 4em;
    }
    </style>
    <div id=container><span>123 456</span> 789</div>
  )HTML");
  const Vector<NGInlineItem>& items = node.ItemsData(false).items;

  // While "123 456" can fit in a line, "456" has a right margin that cannot
  // fit. Since "456" and its right margin is not breakable, "456" should be on
  // the next line.
  Vector<NGInlineItemResults> lines;
  lines = BreakLines(node, LayoutUnit(80));
  EXPECT_EQ(3u, lines.size());
  EXPECT_EQ("123", ToString(lines[0], node));
  EXPECT_EQ("456", ToString(lines[1], node));
  DCHECK_EQ(NGInlineItem::kCloseTag, items[lines[1].back().item_index].Type());
  EXPECT_EQ("789", ToString(lines[2], node));

  // Same as above, but this time "456" overflows the line because it is 70px.
  lines = BreakLines(node, LayoutUnit(60));
  EXPECT_EQ(3u, lines.size());
  EXPECT_EQ("123", ToString(lines[0], node));
  EXPECT_EQ("456", ToString(lines[1], node));
  DCHECK_EQ(NGInlineItem::kCloseTag, items[lines[1].back().item_index].Type());
  EXPECT_EQ("789", ToString(lines[2], node));
}

TEST_F(NGLineBreakerTest, OverflowAfterSpacesAcrossElements) {
  LoadAhem();
  NGInlineNode node = CreateInlineNode(R"HTML(
    <!DOCTYPE html>
    <style>
    div {
      font: 10px/1 Ahem;
      white-space: pre-wrap;
      width: 10ch;
      word-wrap: break-word;
    }
    </style>
    <div id=container><span>12345 </span> 1234567890123</div>
  )HTML");

  Vector<NGInlineItemResults> lines;
  lines = BreakLines(node, LayoutUnit(100));
  EXPECT_EQ(3u, lines.size());
  EXPECT_EQ("12345  ", ToString(lines[0], node));
  EXPECT_EQ("1234567890", ToString(lines[1], node));
  EXPECT_EQ("123", ToString(lines[2], node));
}

// Tests when the last word in a node wraps, and another node continues.
TEST_F(NGLineBreakerTest, WrapLastWord) {
  LoadAhem();
  NGInlineNode node = CreateInlineNode(R"HTML(
    <!DOCTYPE html>
    <style>
    #container {
      font: 10px/1 Ahem;
    }
    </style>
    <div id=container>AAA AAA AAA <span>BB</span> CC</div>
  )HTML");

  Vector<NGInlineItemResults> lines;
  lines = BreakLines(node, LayoutUnit(100));
  EXPECT_EQ(2u, lines.size());
  EXPECT_EQ("AAA AAA", ToString(lines[0], node));
  EXPECT_EQ("AAA BB CC", ToString(lines[1], node));
}

TEST_F(NGLineBreakerTest, WrapLetterSpacing) {
  NGInlineNode node = CreateInlineNode(R"HTML(
    <!DOCTYPE html>
    <style>
    #container {
      font: 10px/1 Times;
      letter-spacing: 10px;
      width: 0px;
    }
    </style>
    <div id=container>Star Wars</div>
  )HTML");

  Vector<NGInlineItemResults> lines;
  lines = BreakLines(node, LayoutUnit(100));
  EXPECT_EQ(2u, lines.size());
  EXPECT_EQ("Star", ToString(lines[0], node));
  EXPECT_EQ("Wars", ToString(lines[1], node));
}

TEST_F(NGLineBreakerTest, BoundaryInWord) {
  LoadAhem();
  NGInlineNode node = CreateInlineNode(R"HTML(
    <!DOCTYPE html>
    <style>
    #container {
      font: 10px/1 Ahem;
    }
    </style>
    <div id=container><span>123 456</span>789 abc</div>
  )HTML");

  // The element boundary within "456789" should not cause a break.
  // Since "789" does not fit, it should go to the next line along with "456".
  Vector<NGInlineItemResults> lines;
  lines = BreakLines(node, LayoutUnit(80));
  EXPECT_EQ(3u, lines.size());
  EXPECT_EQ("123", ToString(lines[0], node));
  EXPECT_EQ("456789", ToString(lines[1], node));
  EXPECT_EQ("abc", ToString(lines[2], node));

  // Same as above, but this time "456789" overflows the line because it is
  // 60px.
  lines = BreakLines(node, LayoutUnit(50));
  EXPECT_EQ(3u, lines.size());
  EXPECT_EQ("123", ToString(lines[0], node));
  EXPECT_EQ("456789", ToString(lines[1], node));
  EXPECT_EQ("abc", ToString(lines[2], node));
}

TEST_F(NGLineBreakerTest, BoundaryInFirstWord) {
  LoadAhem();
  NGInlineNode node = CreateInlineNode(R"HTML(
    <!DOCTYPE html>
    <style>
    #container {
      font: 10px/1 Ahem;
    }
    </style>
    <div id=container><span>123</span>456 789</div>
  )HTML");

  Vector<NGInlineItemResults> lines;
  lines = BreakLines(node, LayoutUnit(80));
  EXPECT_EQ(2u, lines.size());
  EXPECT_EQ("123456", ToString(lines[0], node));
  EXPECT_EQ("789", ToString(lines[1], node));

  lines = BreakLines(node, LayoutUnit(50));
  EXPECT_EQ(2u, lines.size());
  EXPECT_EQ("123456", ToString(lines[0], node));
  EXPECT_EQ("789", ToString(lines[1], node));

  lines = BreakLines(node, LayoutUnit(20));
  EXPECT_EQ(2u, lines.size());
  EXPECT_EQ("123456", ToString(lines[0], node));
  EXPECT_EQ("789", ToString(lines[1], node));
}

struct WhitespaceStateTestData {
  const char* html;
  const char* white_space;
  NGLineBreaker::WhitespaceState expected;
} whitespace_state_test_data[] = {
    // The most common cases.
    {"12", "normal", NGLineBreaker::WhitespaceState::kNone},
    {"1234 5678", "normal", NGLineBreaker::WhitespaceState::kCollapsed},
    // |NGInlineItemsBuilder| collapses trailing spaces of a block, so
    // |NGLineBreaker| computes to `none`.
    {"12 ", "normal", NGLineBreaker::WhitespaceState::kNone},
    // pre/pre-wrap should preserve trailing spaces if exists.
    {"1234 5678", "pre-wrap", NGLineBreaker::WhitespaceState::kPreserved},
    {"12 ", "pre", NGLineBreaker::WhitespaceState::kPreserved},
    {"12 ", "pre-wrap", NGLineBreaker::WhitespaceState::kPreserved},
    {"12", "pre", NGLineBreaker::WhitespaceState::kNone},
    {"12", "pre-wrap", NGLineBreaker::WhitespaceState::kNone},
    // Empty/space-only cases.
    {"", "normal", NGLineBreaker::WhitespaceState::kLeading},
    {" ", "pre", NGLineBreaker::WhitespaceState::kPreserved},
    {" ", "pre-wrap", NGLineBreaker::WhitespaceState::kPreserved},
    // Cases needing to rewind.
    {"12 34<span>56</span>", "normal",
     NGLineBreaker::WhitespaceState::kCollapsed},
    {"12 34<span>56</span>", "pre-wrap",
     NGLineBreaker::WhitespaceState::kPreserved},
    // Atomic inlines.
    {"12 <span style='display: inline-block'></span>", "normal",
     NGLineBreaker::WhitespaceState::kNone},
    // fast/text/whitespace/inline-whitespace-wrapping-4.html
    {"<span style='white-space: nowrap'>1234  </span>"
     "<span style='white-space: normal'>  5678</span>",
     "pre", NGLineBreaker::WhitespaceState::kCollapsed},
};

std::ostream& operator<<(std::ostream& os,
                         const WhitespaceStateTestData& data) {
  return os << static_cast<int>(data.expected) << " for '" << data.html
            << "' with 'white-space: " << data.white_space << "'";
}

class NGWhitespaceStateTest
    : public NGLineBreakerTest,
      public testing::WithParamInterface<WhitespaceStateTestData> {};

INSTANTIATE_TEST_SUITE_P(NGLineBreakerTest,
                         NGWhitespaceStateTest,
                         testing::ValuesIn(whitespace_state_test_data));

TEST_P(NGWhitespaceStateTest, WhitespaceState) {
  const auto& data = GetParam();
  LoadAhem();
  NGInlineNode node = CreateInlineNode(String(R"HTML(
    <!DOCTYPE html>
    <style>
    #container {
      font: 10px/1 Ahem;
      width: 50px;
      white-space: )HTML") + data.white_space +
                                       R"HTML(
    }
    </style>
    <div id=container>)HTML" + data.html +
                                       R"HTML(</div>
  )HTML");

  Vector<NGLineInfo> line_infos = BreakToLineInfo(node, LayoutUnit(50));
  EXPECT_EQ(trailing_whitespaces_[0], data.expected);
}

struct TrailingSpaceWidthTestData {
  const char* html;
  const char* white_space;
  unsigned trailing_space_width;
} trailing_space_width_test_data[] = {
    {" ", "pre", 1},
    {"   ", "pre", 3},
    {"1 ", "pre", 1},
    {"1  ", "pre", 2},
    {"1<span> </span>", "pre", 1},
    {"<span>1 </span> ", "pre", 2},
    {"1<span> </span> ", "pre", 2},
    {"1 <span> </span> ", "pre", 3},
    {"1 \t", "pre", 3},
    {"1  \n", "pre", 2},
    {"1  <br>", "pre", 2},

    {" ", "pre-wrap", 1},
    {"   ", "pre-wrap", 3},
    {"1 ", "pre-wrap", 1},
    {"1  ", "pre-wrap", 2},
    {"1<span> </span>", "pre-wrap", 1},
    {"<span>1 </span> ", "pre-wrap", 2},
    {"1<span> </span> ", "pre-wrap", 2},
    {"1 <span> </span> ", "pre-wrap", 3},
    {"1 \t", "pre-wrap", 3},
    {"1  <br>", "pre-wrap", 2},
    {"12 1234", "pre-wrap", 1},
    {"12  1234", "pre-wrap", 2},
};

class NGTrailingSpaceWidthTest
    : public NGLineBreakerTest,
      public testing::WithParamInterface<TrailingSpaceWidthTestData> {};

INSTANTIATE_TEST_SUITE_P(NGLineBreakerTest,
                         NGTrailingSpaceWidthTest,
                         testing::ValuesIn(trailing_space_width_test_data));

TEST_P(NGTrailingSpaceWidthTest, TrailingSpaceWidth) {
  const auto& data = GetParam();
  LoadAhem();
  NGInlineNode node = CreateInlineNode(String(R"HTML(
    <!DOCTYPE html>
    <style>
    #container {
      font: 10px/1 Ahem;
      width: 50px;
      tab-size: 2;
      white-space: )HTML") + data.white_space +
                                       R"HTML(;
    }
    </style>
    <div id=container>)HTML" + data.html +
                                       R"HTML(</div>
  )HTML");

  Vector<NGLineInfo> line_infos = BreakToLineInfo(node, LayoutUnit(50));
  const NGLineInfo& line_info = line_infos[0];
  if (line_info.ShouldHangTrailingSpaces()) {
    EXPECT_EQ(line_info.HangWidth(),
              LayoutUnit(10) * data.trailing_space_width);
  } else {
    EXPECT_EQ(line_info.HangWidth(), LayoutUnit());
  }
}

TEST_F(NGLineBreakerTest, MinMaxWithTrailingSpaces) {
  LoadAhem();
  NGInlineNode node = CreateInlineNode(R"HTML(
    <!DOCTYPE html>
    <style>
    #container {
      font: 10px/1 Ahem;
      white-space: pre-wrap;
    }
    </style>
    <div id=container>12345 6789 </div>
  )HTML");

  auto size = node.ComputeMinMaxSize(
      WritingMode::kHorizontalTb,
      MinMaxSizeInput(/* percentage_resolution_block_size */ (LayoutUnit())));
  EXPECT_EQ(size.min_size, LayoutUnit(60));
  EXPECT_EQ(size.max_size, LayoutUnit(110));
}

TEST_F(NGLineBreakerTest, TableCellWidthCalculationQuirkOutOfFlow) {
  NGInlineNode node = CreateInlineNode(R"HTML(
    <style>
    table {
      font-size: 10px;
      width: 5ch;
    }
    </style>
    <table><tr><td id=container>
      1234567
      <img style="position: absolute">
    </td></tr></table>
  )HTML");
  // |SetBodyInnerHTML| doesn't set compatibility mode.
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  EXPECT_TRUE(node.GetDocument().InQuirksMode());

  node.ComputeMinMaxSize(
      WritingMode::kHorizontalTb,
      MinMaxSizeInput(/* percentage_resolution_block_size */ LayoutUnit()));
  // Pass if |ComputeMinMaxSize| doesn't hit DCHECK failures.
}

#undef MAYBE_OverflowAtomicInline
}  // namespace
}  // namespace blink
