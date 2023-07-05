// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_base_layout_algorithm_test.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_breaker.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_info.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_positioned_float.h"
#include "third_party/blink/renderer/core/layout/ng/ng_unpositioned_float.h"
#include "third_party/blink/renderer/core/testing/mock_hyphenation.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

String ToString(NGInlineItemResults line, NGInlineNode node) {
  StringBuilder builder;
  const String& text = node.ItemsData(false).text_content;
  for (const auto& item_result : line) {
    builder.Append(
        StringView(text, item_result.StartOffset(), item_result.Length()));
  }
  return builder.ToString();
}

class NGLineBreakerTest : public RenderingTest {
 protected:
  NGInlineNode CreateInlineNode(const String& html_content) {
    SetBodyInnerHTML(html_content);

    LayoutBlockFlow* block_flow =
        To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
    return NGInlineNode(block_flow);
  }

  // Break lines using the specified available width.
  Vector<std::pair<String, unsigned>> BreakLines(
      NGInlineNode node,
      LayoutUnit available_width,
      void (*callback)(const NGLineBreaker&, const NGLineInfo&) = nullptr,
      bool fill_first_space_ = false) {
    DCHECK(node);
    node.PrepareLayoutIfNeeded();
    NGConstraintSpace space = ConstraintSpaceForAvailableSize(available_width);
    const NGInlineBreakToken* break_token = nullptr;
    Vector<std::pair<String, unsigned>> lines;
    trailing_whitespaces_.resize(0);
    NGExclusionSpace exclusion_space;
    NGLeadingFloats leading_floats;
    NGLineLayoutOpportunity line_opportunity(available_width);
    NGLineInfo line_info;
    do {
      NGLineBreaker line_breaker(node, NGLineBreakerMode::kContent, space,
                                 line_opportunity, leading_floats, break_token,
                                 /* column_spanner_path */ nullptr,
                                 &exclusion_space);
      line_breaker.NextLine(&line_info);
      if (callback)
        callback(line_breaker, line_info);
      trailing_whitespaces_.push_back(
          line_breaker.TrailingWhitespaceForTesting());

      if (line_info.Results().empty())
        break;

      break_token = line_info.BreakToken();
      if (fill_first_space_ && lines.empty()) {
        first_should_hang_trailing_space_ =
            line_info.ShouldHangTrailingSpaces();
        first_hang_width_ = line_info.HangWidthForAlignment();
      }
      lines.push_back(std::make_pair(ToString(line_info.Results(), node),
                                     line_info.Results().back().item_index));
    } while (break_token);

    return lines;
  }

  wtf_size_t BreakLinesAt(NGInlineNode node,
                          LayoutUnit available_width,
                          base::span<NGLineBreakPoint> break_points,
                          base::span<NGLineInfo> line_info_list) {
    DCHECK(node);
    node.PrepareLayoutIfNeeded();
    NGConstraintSpace space = ConstraintSpaceForAvailableSize(available_width);
    const NGInlineBreakToken* break_token = nullptr;
    NGExclusionSpace exclusion_space;
    NGLeadingFloats leading_floats;
    NGLineLayoutOpportunity line_opportunity(available_width);
    wtf_size_t line_index = 0;
    do {
      NGLineBreaker line_breaker(node, NGLineBreakerMode::kContent, space,
                                 line_opportunity, leading_floats, break_token,
                                 /* column_spanner_path */ nullptr,
                                 &exclusion_space);
      if (line_index < break_points.size()) {
        line_breaker.SetBreakAt(break_points[line_index]);
      }
      CHECK_LT(line_index, line_info_list.size());
      NGLineInfo& line_info = line_info_list[line_index];
      line_breaker.NextLine(&line_info);
      break_token = line_info.BreakToken();
      ++line_index;
    } while (break_token);
    return line_index;
  }

  wtf_size_t BreakLines(NGInlineNode node,
                        LayoutUnit available_width,
                        base::span<NGLineInfo> line_info_list) {
    Vector<NGLineBreakPoint> break_points;
    return BreakLinesAt(node, available_width, break_points, line_info_list);
  }

  MinMaxSizes ComputeMinMaxSizes(NGInlineNode node) {
    const auto space =
        NGConstraintSpaceBuilder(node.Style().GetWritingMode(),
                                 node.Style().GetWritingDirection(),
                                 /* is_new_fc */ false)
            .ToConstraintSpace();

    return node
        .ComputeMinMaxSizes(node.Style().GetWritingMode(), space,
                            MinMaxSizesFloatInput())
        .sizes;
  }

  Vector<NGLineBreaker::WhitespaceState> trailing_whitespaces_;
  bool first_should_hang_trailing_space_;
  LayoutUnit first_hang_width_;
};

namespace {

TEST_F(NGLineBreakerTest, FitWithEpsilon) {
  LoadAhem();
  NGInlineNode node = CreateInlineNode(R"HTML(
    <!DOCTYPE html>
    <style>
    #container {
      font: 10px/1 Ahem;
      width: 49.99px;
      overflow: hidden;
      text-overflow: ellipsis;
    }
    </style>
    <div id=container>00000</div>
  )HTML");
  auto lines = BreakLines(
      node, LayoutUnit::FromFloatRound(50 - LayoutUnit::Epsilon()),
      [](const NGLineBreaker& line_breaker, const NGLineInfo& line_info) {
        EXPECT_FALSE(line_info.HasOverflow());
      });
  EXPECT_EQ(1u, lines.size());

  // Make sure ellipsizing code use the same |HasOverflow|.
  NGInlineCursor cursor(*node.GetLayoutBlockFlow());
  for (; cursor; cursor.MoveToNext())
    EXPECT_FALSE(cursor.Current().IsEllipsis());
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

  Vector<std::pair<String, unsigned>> lines;
  lines = BreakLines(node, LayoutUnit(80));
  EXPECT_EQ(2u, lines.size());
  EXPECT_EQ("123 456", lines[0].first);
  EXPECT_EQ("789", lines[1].first);

  lines = BreakLines(node, LayoutUnit(60));
  EXPECT_EQ(3u, lines.size());
  EXPECT_EQ("123", lines[0].first);
  EXPECT_EQ("456", lines[1].first);
  EXPECT_EQ("789", lines[2].first);
}

// For "text-combine-upright-break-inside-001a.html"
TEST_F(NGLineBreakerTest, TextCombineCloseTag) {
  LoadAhem();
  InsertStyleElement(
      "#container {"
      "  font: 10px/2 Ahem;"
      "  writing-mode: vertical-lr;"
      "}"
      "tcy { text-combine-upright: all }");
  NGInlineNode node = CreateInlineNode(
      "<div id=container>"
      "abc<tcy style='white-space:pre'>XYZ</tcy>def");

  Vector<std::pair<String, unsigned>> lines;
  lines = BreakLines(node, LayoutUnit(30));
  EXPECT_EQ(1u, lines.size());
  // |NGLineBreaker::auto_wrap_| doesn't care about CSS "white-space" property
  // in the element with "text-combine-upright:all".
  //  NGInlineItemResult
  //    [0] kText 0-3 can_break_after_=false
  //    [1] kOpenTag 3-3 can_break_after_=false
  //    [2] kStartTag 3-3 can_break_after _= fasle
  //    [3] kAtomicInline 3-4 can_break_after _= false
  //    [4] kCloseTag 4-4 can_break_after _= false
  EXPECT_EQ(String(u"abc\uFFFCdef"), lines[0].first);
}

TEST_F(NGLineBreakerTest, TextCombineBreak) {
  LoadAhem();
  InsertStyleElement(
      "#container {"
      "  font: 10px/2 Ahem;"
      "  writing-mode: vertical-lr;"
      "}"
      "tcy { text-combine-upright: all }");
  NGInlineNode node = CreateInlineNode("<div id=container>abc<tcy>-</tcy>def");

  Vector<std::pair<String, unsigned>> lines;
  lines = BreakLines(node, LayoutUnit(30));
  EXPECT_EQ(2u, lines.size());
  // NGLineBreaker attempts to break line for "abc-def".
  EXPECT_EQ(String(u"abc\uFFFC"), lines[0].first);
  EXPECT_EQ(String(u"def"), lines[1].first);
}

TEST_F(NGLineBreakerTest, TextCombineNoBreak) {
  LoadAhem();
  InsertStyleElement(
      "#container {"
      "  font: 10px/2 Ahem;"
      "  writing-mode: vertical-lr;"
      "}"
      "tcy { text-combine-upright: all }");
  NGInlineNode node =
      CreateInlineNode("<div id=container>abc<tcy>XYZ</tcy>def");

  Vector<std::pair<String, unsigned>> lines;
  lines = BreakLines(node, LayoutUnit(30));
  EXPECT_EQ(1u, lines.size());
  // NGLineBreaker attempts to break line for "abcXYZdef".
  EXPECT_EQ(String(u"abc\uFFFCdef"), lines[0].first);
}

TEST_F(NGLineBreakerTest, TextCombineNoBreakWithSpace) {
  LoadAhem();
  InsertStyleElement(
      "#container {"
      "  font: 10px/2 Ahem;"
      "  writing-mode: vertical-lr;"
      "}"
      "tcy { text-combine-upright: all }");
  NGInlineNode node =
      CreateInlineNode("<div id=container>abc<tcy>X Z</tcy>def");

  Vector<std::pair<String, unsigned>> lines;
  lines = BreakLines(node, LayoutUnit(30));
  EXPECT_EQ(1u, lines.size());
  // NGLineBreaker checks whether can break after "Z" in "abcX Zdef".
  EXPECT_EQ(String(u"abc\uFFFCdef"), lines[0].first);
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
  Vector<std::pair<String, unsigned>> lines;
  lines = BreakLines(node, LayoutUnit(40));
  EXPECT_EQ(2u, lines.size());
  EXPECT_EQ("12345", lines[0].first);
  EXPECT_EQ("678", lines[1].first);

  // Both lines overflow.
  lines = BreakLines(node, LayoutUnit(20));
  EXPECT_EQ(2u, lines.size());
  EXPECT_EQ("12345", lines[0].first);
  EXPECT_EQ("678", lines[1].first);
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

  Vector<std::pair<String, unsigned>> lines;
  lines = BreakLines(node, LayoutUnit(100));
  EXPECT_EQ(2u, lines.size());
  EXPECT_EQ("12345\t\t", lines[0].first);
  EXPECT_EQ("678", lines[1].first);
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

  Vector<std::pair<String, unsigned>> lines;
  lines = BreakLines(node, LayoutUnit(100));
  EXPECT_EQ(2u, lines.size());
  EXPECT_EQ("12345\t\t", lines[0].first);
  EXPECT_EQ("678", lines[1].first);
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

  Vector<std::pair<String, unsigned>> lines;
  lines = BreakLines(node, LayoutUnit(80));
  EXPECT_EQ(2u, lines.size());
  EXPECT_EQ(String(u"12345\uFFFC"), lines[0].first);
  EXPECT_EQ("678", lines[1].first);

  lines = BreakLines(node, LayoutUnit(70));
  EXPECT_EQ(2u, lines.size());
  EXPECT_EQ("12345", lines[0].first);
  EXPECT_EQ(String(u"\uFFFC678"), lines[1].first);

  lines = BreakLines(node, LayoutUnit(40));
  EXPECT_EQ(3u, lines.size());
  EXPECT_EQ("12345", lines[0].first);
  EXPECT_EQ(String(u"\uFFFC"), lines[1].first);
  EXPECT_EQ("678", lines[2].first);

  lines = BreakLines(node, LayoutUnit(20));
  EXPECT_EQ(3u, lines.size());
  EXPECT_EQ("12345", lines[0].first);
  EXPECT_EQ(String(u"\uFFFC"), lines[1].first);
  EXPECT_EQ("678", lines[2].first);
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
  const HeapVector<NGInlineItem>& items = node.ItemsData(false).items;

  // While "123 456" can fit in a line, "456" has a right margin that cannot
  // fit. Since "456" and its right margin is not breakable, "456" should be on
  // the next line.
  Vector<std::pair<String, unsigned>> lines;
  lines = BreakLines(node, LayoutUnit(80));
  EXPECT_EQ(3u, lines.size());
  EXPECT_EQ("123", lines[0].first);
  EXPECT_EQ("456", lines[1].first);
  DCHECK_EQ(NGInlineItem::kCloseTag, items[lines[1].second - 1].Type());
  EXPECT_EQ("789", lines[2].first);

  // Same as above, but this time "456" overflows the line because it is 70px.
  lines = BreakLines(node, LayoutUnit(60));
  EXPECT_EQ(3u, lines.size());
  EXPECT_EQ("123", lines[0].first);
  EXPECT_EQ("456", lines[1].first);
  DCHECK_EQ(NGInlineItem::kCloseTag, items[lines[1].second].Type());
  EXPECT_EQ("789", lines[2].first);
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

  Vector<std::pair<String, unsigned>> lines;
  lines = BreakLines(node, LayoutUnit(100));
  EXPECT_EQ(3u, lines.size());
  EXPECT_EQ("12345  ", lines[0].first);
  EXPECT_EQ("1234567890", lines[1].first);
  EXPECT_EQ("123", lines[2].first);
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

  Vector<std::pair<String, unsigned>> lines;
  lines = BreakLines(node, LayoutUnit(100));
  EXPECT_EQ(2u, lines.size());
  EXPECT_EQ("AAA AAA", lines[0].first);
  EXPECT_EQ("AAA BB CC", lines[1].first);
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

  Vector<std::pair<String, unsigned>> lines;
  lines = BreakLines(node, LayoutUnit(100));
  EXPECT_EQ(2u, lines.size());
  EXPECT_EQ("Star", lines[0].first);
  EXPECT_EQ("Wars", lines[1].first);
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
  Vector<std::pair<String, unsigned>> lines;
  lines = BreakLines(node, LayoutUnit(80));
  EXPECT_EQ(3u, lines.size());
  EXPECT_EQ("123", lines[0].first);
  EXPECT_EQ("456789", lines[1].first);
  EXPECT_EQ("abc", lines[2].first);

  // Same as above, but this time "456789" overflows the line because it is
  // 60px.
  lines = BreakLines(node, LayoutUnit(50));
  EXPECT_EQ(3u, lines.size());
  EXPECT_EQ("123", lines[0].first);
  EXPECT_EQ("456789", lines[1].first);
  EXPECT_EQ("abc", lines[2].first);
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

  Vector<std::pair<String, unsigned>> lines;
  lines = BreakLines(node, LayoutUnit(80));
  EXPECT_EQ(2u, lines.size());
  EXPECT_EQ("123456", lines[0].first);
  EXPECT_EQ("789", lines[1].first);

  lines = BreakLines(node, LayoutUnit(50));
  EXPECT_EQ(2u, lines.size());
  EXPECT_EQ("123456", lines[0].first);
  EXPECT_EQ("789", lines[1].first);

  lines = BreakLines(node, LayoutUnit(20));
  EXPECT_EQ(2u, lines.size());
  EXPECT_EQ("123456", lines[0].first);
  EXPECT_EQ("789", lines[1].first);
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

  BreakLines(node, LayoutUnit(50));
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

  BreakLines(node, LayoutUnit(50), nullptr, true);
  if (first_should_hang_trailing_space_) {
    EXPECT_EQ(first_hang_width_, LayoutUnit(10) * data.trailing_space_width);
  } else {
    EXPECT_EQ(first_hang_width_, LayoutUnit());
  }
}

TEST_F(NGLineBreakerTest, FullyCollapsedSpaces) {
  // The space in `span` will be collapsed in `CollectInlines`, but it may have
  // set `NeedsLayout`. It should be cleared when a layout lifecycle is done,
  // but not by the line breaker.
  NGInlineNode node = CreateInlineNode(R"HTML(
    <style>
    #container {
      font-size: 10px;
    }
    </style>
    <div id=container>0 <span id=span> </span>2</div>
  )HTML");

  auto* span = To<LayoutInline>(GetLayoutObjectByElementId("span"));
  LayoutObject* space_text = span->FirstChild();
  space_text->SetNeedsLayout("test");

  // `NGLineBreaker` should not `ClearNeedsLayout`.
  BreakLines(node, LayoutUnit(800));
  EXPECT_TRUE(space_text->NeedsLayout());

  // But a layout pass should.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(space_text->NeedsLayout());
}

TEST_F(NGLineBreakerTest, TrailingCollapsedSpaces) {
  // The space in `span` is not collapsed but the line breaker removes it as a
  // trailing space. Similar to `FullyCollapsedSpaces` above, its `NeedsLayout`
  // should be cleared in a layout lifecycle, but not by the line breaker.
  LoadAhem();
  NGInlineNode node = CreateInlineNode(R"HTML(
    <style>
    #container {
      font-size: 10px;
      font-family: Ahem;
      width: 2em;
    }
    </style>
    <div id=container>0<span id=span> </span>2</div>
  )HTML");

  auto* span = To<LayoutInline>(GetLayoutObjectByElementId("span"));
  LayoutObject* space_text = span->FirstChild();
  space_text->SetNeedsLayout("test");

  // `NGLineBreaker` should not `ClearNeedsLayout`.
  BreakLines(node, LayoutUnit(800));
  EXPECT_TRUE(space_text->NeedsLayout());

  // But a layout pass should.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(space_text->NeedsLayout());
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

  const auto sizes = ComputeMinMaxSizes(node);
  EXPECT_EQ(sizes.min_size, LayoutUnit(50));
  EXPECT_EQ(sizes.max_size, LayoutUnit(110));
}

// `word-break: break-word` can break a space run.
TEST_F(NGLineBreakerTest, MinMaxBreakSpaces) {
  LoadAhem();
  NGInlineNode node = CreateInlineNode(R"HTML(
    <!DOCTYPE html>
    <style>
    div {
      font: 10px/1 Ahem;
      white-space: pre-wrap;
      word-break: break-word;
    }
    span {
      font-size: 200%;
    }
    </style>
    <div id=container>M):
<span>    </span>p</div>
  )HTML");

  const auto sizes = ComputeMinMaxSizes(node);
  EXPECT_EQ(sizes.min_size, LayoutUnit(10));
  EXPECT_EQ(sizes.max_size, LayoutUnit(90));
}

TEST_F(NGLineBreakerTest, MinMaxWithSoftHyphen) {
  LoadAhem();
  NGInlineNode node = CreateInlineNode(R"HTML(
    <!DOCTYPE html>
    <style>
    #container {
      font: 10px/1 Ahem;
    }
    </style>
    <div id=container>abcd&shy;ef xx</div>
  )HTML");

  const auto sizes = ComputeMinMaxSizes(node);
  EXPECT_EQ(sizes.min_size, LayoutUnit(50));
  EXPECT_EQ(sizes.max_size, LayoutUnit(90));
}

TEST_F(NGLineBreakerTest, MinMaxWithHyphensDisabled) {
  LoadAhem();
  NGInlineNode node = CreateInlineNode(R"HTML(
    <!DOCTYPE html>
    <style>
    #container {
      font: 10px/1 Ahem;
      hyphens: none;
    }
    </style>
    <div id=container>abcd&shy;ef xx</div>
  )HTML");

  const auto sizes = ComputeMinMaxSizes(node);
  EXPECT_EQ(sizes.min_size, LayoutUnit(60));
  EXPECT_EQ(sizes.max_size, LayoutUnit(90));
}

TEST_F(NGLineBreakerTest, MinMaxWithHyphensDisabledWithTrailingSpaces) {
  LoadAhem();
  NGInlineNode node = CreateInlineNode(R"HTML(
    <!DOCTYPE html>
    <style>
    #container {
      font: 10px/1 Ahem;
      hyphens: none;
    }
    </style>
    <div id=container>abcd&shy; ef xx</div>
  )HTML");

  const auto sizes = ComputeMinMaxSizes(node);
  EXPECT_EQ(sizes.min_size, LayoutUnit(50));
  EXPECT_EQ(sizes.max_size, LayoutUnit(100));
}

TEST_F(NGLineBreakerTest, MinMaxWithHyphensAuto) {
  LoadAhem();
  LayoutLocale::SetHyphenationForTesting(AtomicString("en-us"),
                                         MockHyphenation::Create());
  NGInlineNode node = CreateInlineNode(R"HTML(
    <!DOCTYPE html>
    <style>
    #container {
      font: 10px/1 Ahem;
      hyphens: auto;
    }
    </style>
    <div id=container lang="en-us">zz hyphenation xx</div>
  )HTML");

  const auto sizes = ComputeMinMaxSizes(node);
  EXPECT_EQ(sizes.min_size, LayoutUnit(50));
  EXPECT_EQ(sizes.max_size, LayoutUnit(170));
  LayoutLocale::SetHyphenationForTesting(AtomicString("en-us"), nullptr);
}

// For http://crbug.com/1104534
TEST_F(NGLineBreakerTest, SplitTextZero) {
  // Note: |V8TestingScope| is needed for |Text::splitText()|.
  V8TestingScope scope;

  LoadAhem();
  NGInlineNode node = CreateInlineNode(R"HTML(
    <!DOCTYPE html>
    <style>
    #container {
      font: 10px/1 Ahem;
      overflow-wrap: break-word;
    }
    </style>
    <div id=container>0123456789<b id=target> </b>ab</i></div>
  )HTML");

  To<Text>(GetElementById("target")->firstChild())
      ->splitText(0, ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();

  Vector<std::pair<String, unsigned>> lines;
  lines = BreakLines(node, LayoutUnit(100));
  EXPECT_EQ(2u, lines.size());
  EXPECT_EQ("0123456789", lines[0].first);
  EXPECT_EQ("ab", lines[1].first);
}

TEST_F(NGLineBreakerTest, ForcedBreakFollowedByCloseTag) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div id="container">
      <div><span>line<br></span></div>
      <div>
        <span>line<br></span>
      </div>
      <div>
        <span>
          line<br>
        </span>
      </div>
      <div>
        <span>line<br>  </span>
      </div>
      <div>
        <span>line<br>  </span>&#32;&#32;
      </div>
    </div>
  )HTML");
  const LayoutObject* container = GetLayoutObjectByElementId("container");
  for (const LayoutObject* child = container->SlowFirstChild(); child;
       child = child->NextSibling()) {
    NGInlineCursor cursor(*To<LayoutBlockFlow>(child));
    wtf_size_t line_count = 0;
    for (cursor.MoveToFirstLine(); cursor; cursor.MoveToNextLine())
      ++line_count;
    EXPECT_EQ(line_count, 1u);
  }
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

  ComputeMinMaxSizes(node);
  // Pass if |ComputeMinMaxSizes| doesn't hit DCHECK failures.
}

TEST_F(NGLineBreakerTest, BoxDecorationBreakCloneWithoutBoxDecorations) {
  SetBodyInnerHTML(R"HTML(
    <span style="-webkit-box-decoration-break: clone"></span>
  )HTML");
  // Pass if it does not hit DCHECK.
}

TEST_F(NGLineBreakerTest, RewindPositionedFloat) {
  SetBodyInnerHTML(R"HTML(
<div style="float: left">
  &#xe49d;oB&#xfb45;|&#xf237;&#xfefc;
  )&#xe2c9;&#xea7a;0{r
  6
  <span style="float: left">
    <span style="border-right: solid green 2.166621530302065e+19in"></span>
  </span>
</div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
}

// crbug.com/1091359
TEST_F(NGLineBreakerTest, RewindRubyRun) {
  NGInlineNode node = CreateInlineNode(R"HTML(
<div id="container">
<style>
* {
  -webkit-text-security:square;
  font-size:16px;
}
</style>
<big style="word-wrap: break-word">a
<ruby dir="rtl">
<rt>
B AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
<svg></svg>
<b>
</rt>
</ruby>
  )HTML");

  ComputeMinMaxSizes(node);
  // This test passes if no CHECK failures.
}

TEST_F(NGLineBreakerTest, SplitTextIntoSegements) {
  NGInlineNode node = CreateInlineNode(
      uR"HTML(
      <!DOCTYPE html>
      <svg viewBox="0 0 800 600">
      <text id="container" rotate="1" style="font-family:Times">AV)HTML"
      u"\U0001F197\u05E2\u05B4\u05D1\u05E8\u05B4\u05D9\u05EA</text></svg>)");
  BreakLines(
      node, LayoutUnit::Max(),
      [](const NGLineBreaker& line_breaker, const NGLineInfo& line_info) {
        EXPECT_EQ(8u, line_info.Results().size());
        // "A" and "V" with Times font are typically overlapped. They should
        // be split.
        EXPECT_EQ(1u, line_info.Results()[0].Length());  // A
        EXPECT_EQ(1u, line_info.Results()[1].Length());  // V
        // Non-BMP characters should not be split.
        EXPECT_EQ(2u, line_info.Results()[2].Length());  // U+1F197
        // Connected characters should not be split.
        EXPECT_EQ(2u, line_info.Results()[3].Length());  // U+05E2 U+05B4
        EXPECT_EQ(1u, line_info.Results()[4].Length());  // U+05D1
        EXPECT_EQ(2u, line_info.Results()[5].Length());  // U+05E8 U+05B4
        EXPECT_EQ(1u, line_info.Results()[6].Length());  // U+05D9
        EXPECT_EQ(1u, line_info.Results()[7].Length());  // U+05EA
      });
}

// crbug.com/1251960
TEST_F(NGLineBreakerTest, SplitTextIntoSegementsCrash) {
  NGInlineNode node = CreateInlineNode(R"HTML(<!DOCTYPE html>
      <svg viewBox="0 0 800 600">
      <text id="container" x="50 100 150">&#x0343;&#x2585;&#x0343;&#x2585;<!--
      -->&#x0343;&#x2585;</text>
      </svg>)HTML");
  BreakLines(
      node, LayoutUnit::Max(),
      [](const NGLineBreaker& line_breaker, const NGLineInfo& line_info) {
        Vector<const NGInlineItemResult*> text_results;
        for (const auto& result : line_info.Results()) {
          if (result.item->Type() == NGInlineItem::kText)
            text_results.push_back(&result);
        }
        EXPECT_EQ(4u, text_results.size());
        EXPECT_EQ(1u, text_results[0]->Length());  // U+0343
        EXPECT_EQ(1u, text_results[1]->Length());  // U+2585
        EXPECT_EQ(2u, text_results[2]->Length());  // U+0343 U+2585
        EXPECT_EQ(2u, text_results[3]->Length());  // U+0343 U+2585
      });
}

// crbug.com/1214232
TEST_F(NGLineBreakerTest, GetOverhangCrash) {
  NGInlineNode node = CreateInlineNode(
      R"HTML(
<!DOCTYPE html>
<style>
* { margin-inline-end: -7%; }
rb { float: right; }
rt { margin: 17179869191em; }
</style>
<div id="container">
<ruby>
<rb>
C c
<rt>
)HTML");
  // The test passes if we have no DCHECK failures in BreakLines().
  BreakLines(node, LayoutUnit::Max());
}

// https://crbug.com/1292848
// Test that, if it's not possible to break after an ideographic space (as
// happens before an end bracket), previous break opportunities are considered.
TEST_F(NGLineBreakerTest, IdeographicSpaceBeforeEndBracket) {
  LoadAhem();
  // Atomic inline, and ideographic space before the ideographic full stop.
  NGInlineNode node1 = CreateInlineNode(
      uR"HTML(
<!DOCTYPE html>
<style>
body { margin: 0; padding: 0; font: 10px/10px Ahem; }
</style>
<div id="container">
全角空白の前では、変な行末があります。　]
</div>
)HTML");
  auto lines1 = BreakLines(node1, LayoutUnit(190));

  // Test that it doesn't overflow.
  EXPECT_EQ(lines1.size(), 2u);

  // No ideographic space.
  NGInlineNode node2 = CreateInlineNode(
      uR"HTML(
<!DOCTYPE html>
<style>
body { margin: 0; padding: 0; font: 10px/10px Ahem; }
</style>
<div id="container">
全角空白の前では、変な行末があります。]
</div>
)HTML");
  auto lines2 = BreakLines(node2, LayoutUnit(190));

  // node1 and node2 should break at the same point because there aren't break
  // opportunities after the ideographic period, and any opportunities before it
  // should be the same.
  EXPECT_EQ(lines1[0].first, lines2[0].first);
}

TEST_F(NGLineBreakerTest, BreakAt) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    #target {
      font-family: Ahem;
      font-size: 10px;
    }
    inline-block {
      display: inline-block;
      width: 1em;
    }
    </style>
    <div id="target">
      0 23 5<inline-block></inline-block><inline-block></inline-block>89
    </div>
  )HTML");
  NGInlineNode target = GetInlineNodeByElementId("target");
  NGLineBreakPoint break_points[]{NGLineBreakPoint{{0, 2}},
                                  NGLineBreakPoint{{1, 6}},
                                  NGLineBreakPoint{{2, 7}}};
  NGLineInfo line_info_list[4];
  const wtf_size_t num_lines =
      BreakLinesAt(target, LayoutUnit(800), break_points, line_info_list);
  EXPECT_EQ(num_lines, 4u);
  EXPECT_EQ(line_info_list[0].BreakToken()->Start(), break_points[0].offset);
  EXPECT_EQ(line_info_list[1].BreakToken()->Start(), break_points[1].offset);
  EXPECT_EQ(line_info_list[2].BreakToken()->Start(), break_points[2].offset);
  EXPECT_EQ(line_info_list[3].BreakToken(), nullptr);
  EXPECT_FALSE(line_info_list[0].IsLastLine());
  EXPECT_FALSE(line_info_list[1].IsLastLine());
  EXPECT_FALSE(line_info_list[2].IsLastLine());
  EXPECT_TRUE(line_info_list[3].IsLastLine());
  EXPECT_EQ(line_info_list[0].Width(), LayoutUnit(10));
  EXPECT_EQ(line_info_list[1].Width(), LayoutUnit(40));
  EXPECT_EQ(line_info_list[2].Width(), LayoutUnit(10));
  EXPECT_EQ(line_info_list[3].Width(), LayoutUnit(30));
}

TEST_F(NGLineBreakerTest, BreakAtTrailingSpaces) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    #target {
      font-family: Ahem;
      font-size: 10px;
    }
    span { font-weight: bold; }
    </style>
    <div id="target">
      <span>0</span>
      23
      <span> </span>
      56
    </div>
  )HTML");
  NGInlineNode target = GetInlineNodeByElementId("target");
  NGLineBreakPoint break_points[]{NGLineBreakPoint{{7, 5}, {3, 4}}};
  NGLineInfo line_info_list[2];
  const wtf_size_t num_lines =
      BreakLinesAt(target, LayoutUnit(800), break_points, line_info_list);
  EXPECT_EQ(num_lines, 2u);
  EXPECT_EQ(line_info_list[0].BreakToken()->Start(), break_points[0].offset);
  EXPECT_EQ(line_info_list[1].BreakToken(), nullptr);
  EXPECT_FALSE(line_info_list[0].IsLastLine());
  EXPECT_TRUE(line_info_list[1].IsLastLine());
  EXPECT_EQ(line_info_list[0].Width(), LayoutUnit(40));
  EXPECT_EQ(line_info_list[1].Width(), LayoutUnit(20));
  EXPECT_EQ(line_info_list[0].Results().size(), 7u);
  EXPECT_EQ(line_info_list[1].Results().size(), 1u);
}

TEST_F(NGLineBreakerTest, BreakAtTrailingSpacesAfterAtomicInline) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    #target {
      font-family: Ahem;
      font-size: 10px;
    }
    inline-block {
      display: inline-block;
      width: 1em;
    }
    </style>
    <div id="target">
      <span><inline-block></inline-block></span>
      <span>23</span>
    </div>
  )HTML");
  NGInlineNode target = GetInlineNodeByElementId("target");
  NGLineBreakPoint break_points[]{NGLineBreakPoint{{4, 2}, {2, 1}}};
  NGLineInfo line_info_list[2];
  const wtf_size_t num_lines =
      BreakLinesAt(target, LayoutUnit(800), break_points, line_info_list);
  EXPECT_EQ(num_lines, 2u);
  EXPECT_EQ(line_info_list[0].BreakToken()->Start(), break_points[0].offset);
  EXPECT_EQ(line_info_list[1].BreakToken(), nullptr);
  EXPECT_FALSE(line_info_list[0].IsLastLine());
  EXPECT_TRUE(line_info_list[1].IsLastLine());
  EXPECT_EQ(line_info_list[0].Width(), LayoutUnit(10));
  EXPECT_EQ(line_info_list[1].Width(), LayoutUnit(20));
  EXPECT_EQ(line_info_list[0].Results().back().item_index, 3u);
  EXPECT_EQ(line_info_list[1].Results().front().item_index, 4u);
}

struct CanBreakInsideTestData {
  bool can_break_insde;
  const char* html;
  const char* target_css = nullptr;
  const char* style = nullptr;
} can_break_inside_test_data[] = {
    {false, "a"},
    {true, "a b"},
    {false, "a b", "white-space: nowrap;"},
    {true, "<span>a</span>a b"},
    {true, "<span>a</span> b"},
    {true, "<span>a </span>b"},
    {true, "a<span> </span>b"},
    {false, "<ib></ib>", nullptr, "ib { display: inline-block; }"},
    {true, "<ib></ib><ib></ib>", nullptr, "ib { display: inline-block; }"},
    {true, "a<ib></ib>", nullptr, "ib { display: inline-block; }"},
    {true, "<ib></ib>a", nullptr, "ib { display: inline-block; }"},
};
class CanBreakInsideTest
    : public NGLineBreakerTest,
      public testing::WithParamInterface<CanBreakInsideTestData> {};
INSTANTIATE_TEST_SUITE_P(NGLineBreakerTest,
                         CanBreakInsideTest,
                         testing::ValuesIn(can_break_inside_test_data));

TEST_P(CanBreakInsideTest, Data) {
  const auto& data = GetParam();
  SetBodyInnerHTML(String::Format(R"HTML(
    <!DOCTYPE html>
    <style>
    #target {
      font-size: 10px;
      width: 800px;
      %s
    }
    %s
    </style>
    <div id="target">%s</div>
  )HTML",
                                  data.target_css, data.style, data.html));
  NGInlineNode target = GetInlineNodeByElementId("target");
  NGLineInfo line_info_list[1];
  const LayoutUnit available_width = LayoutUnit(800);
  const wtf_size_t num_lines =
      BreakLines(target, available_width, line_info_list);
  ASSERT_EQ(num_lines, 1u);

  NGConstraintSpace space = ConstraintSpaceForAvailableSize(available_width);
  const NGInlineBreakToken* break_token = nullptr;
  NGExclusionSpace exclusion_space;
  NGLeadingFloats leading_floats;
  NGLineLayoutOpportunity line_opportunity(available_width);
  NGLineBreaker line_breaker(target, NGLineBreakerMode::kContent, space,
                             line_opportunity, leading_floats, break_token,
                             /* column_spanner_path */ nullptr,
                             &exclusion_space);
  EXPECT_EQ(line_breaker.CanBreakInside(line_info_list[0]),
            data.can_break_insde);
}

}  // namespace
}  // namespace blink
