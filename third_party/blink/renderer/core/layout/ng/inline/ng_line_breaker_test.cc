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
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

class NGLineBreakerTest : public NGBaseLayoutAlgorithmTest {
 protected:
  NGInlineNode CreateInlineNode(const String& html_content) {
    SetBodyInnerHTML(html_content);

    LayoutNGBlockFlow* block_flow =
        ToLayoutNGBlockFlow(GetLayoutObjectByElementId("container"));
    return NGInlineNode(block_flow);
  }

  // Break lines using the specified available width.
  Vector<NGInlineItemResults> BreakLines(NGInlineNode node,
                                         LayoutUnit available_width) {
    DCHECK(node);

    node.PrepareLayoutIfNeeded();

    NGConstraintSpace space =
        NGConstraintSpaceBuilder(
            WritingMode::kHorizontalTb,
            /* icb_size */ {NGSizeIndefinite, NGSizeIndefinite})
            .SetAvailableSize({available_width, NGSizeIndefinite})
            .ToConstraintSpace(WritingMode::kHorizontalTb);

    Vector<NGPositionedFloat> positioned_floats;
    NGUnpositionedFloatVector unpositioned_floats;

    scoped_refptr<NGInlineBreakToken> break_token;

    Vector<NGInlineItemResults> lines;
    NGExclusionSpace exclusion_space;
    NGLineLayoutOpportunity line_opportunity(available_width);
    while (!break_token || !break_token->IsFinished()) {
      NGLineInfo line_info;
      NGLineBreaker line_breaker(node, NGLineBreakerMode::kContent, space,
                                 &positioned_floats, &unpositioned_floats,
                                 /* container_builder */ nullptr,
                                 &exclusion_space, 0u, line_opportunity,
                                 break_token.get());
      line_breaker.NextLine(&line_info);

      if (line_info.Results().IsEmpty())
        break;

      break_token = line_breaker.CreateBreakToken(line_info);
      lines.push_back(std::move(line_info.Results()));
    }

    return lines;
  }
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

#undef MAYBE_OverflowAtomicInline
}  // namespace
}  // namespace blink
