// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_score_line_breaker.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_break_point.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_info_list.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

namespace {

void TestLinesAreContiguous(const NGLineInfoList& line_info_list) {
  for (wtf_size_t i = 1; i < line_info_list.Size(); ++i) {
    EXPECT_EQ(line_info_list[i].Start(),
              line_info_list[i - 1].BreakToken()->Start());
  }
}

}  // namespace

class NGScoreLineBreakerTest : public RenderingTest {};

TEST_F(NGScoreLineBreakerTest, LastLines) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    #target {
      font-family: Ahem;
      font-size: 10px;
      width: 10ch;
    }
    </style>
    <div id="target">
      1234 67 90
      234 67 901
      34 678 012
      456 89 123
      567 901 34
      678 012 45
    </div>
  )HTML");
  const NGInlineNode node = GetInlineNodeByElementId("target");
  const NGPhysicalBoxFragment* fragment =
      node.GetLayoutBox()->GetPhysicalFragment(0);
  const LayoutUnit width = fragment->Size().width;
  NGConstraintSpace space = ConstraintSpaceForAvailableSize(width);
  NGLineLayoutOpportunity line_opportunity(width);
  NGScoreLineBreakContext context;
  NGLineInfoList& line_info_list = context.LineInfoList();
  NGScoreLineBreaker optimizer(node, space, line_opportunity);

  // Run the optimizer from the beginning of the `target`. This should cache
  // `NGLineInfoList::kCapacity` lines.
  const NGInlineBreakToken* break_token = nullptr;
  optimizer.OptimalBreakPoints(break_token, context);
  EXPECT_EQ(line_info_list.Size(), NGLineInfoList::kCapacity);
  TestLinesAreContiguous(line_info_list);

  // Then continue until `NGScoreLineBreaker` consumes all lines in the block.
  wtf_size_t count = 0;
  for (; context.IsActive(); ++count) {
    // Consume the first line in `line_info_list`.
    bool is_cached = false;
    const NGLineInfo& line_info0 = line_info_list.Get(break_token, is_cached);
    EXPECT_TRUE(is_cached);
    EXPECT_EQ(line_info_list.Size(), NGLineInfoList::kCapacity - 1);
    break_token = line_info0.BreakToken();
    // Running again should cache one more line.
    optimizer.OptimalBreakPoints(break_token, context);
    EXPECT_EQ(line_info_list.Size(), NGLineInfoList::kCapacity);
    TestLinesAreContiguous(line_info_list);
  }
  // All is done. The `BreakToken` should be null, and there should be 6 lines.
  EXPECT_FALSE(line_info_list.Back().BreakToken());
  constexpr wtf_size_t target_num_lines = 6;
  EXPECT_EQ(count, target_num_lines - NGLineInfoList::kCapacity);
}

TEST_F(NGScoreLineBreakerTest, ForcedBreak) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    #target {
      font-family: Ahem;
      font-size: 10px;
      width: 10em;
    }
    </style>
    <div id="target">
      1234 6789 12<br>
      1234 6789
      1234 6789
      12
    </div>
  )HTML");
  const NGInlineNode node = GetInlineNodeByElementId("target");
  const NGPhysicalBoxFragment* fragment =
      node.GetLayoutBox()->GetPhysicalFragment(0);
  const LayoutUnit width = fragment->Size().width;
  NGConstraintSpace space = ConstraintSpaceForAvailableSize(width);
  NGLineLayoutOpportunity line_opportunity(width);
  NGScoreLineBreakContext context;
  NGLineInfoList& line_info_list = context.LineInfoList();
  NGLineBreakPoints& break_points = context.LineBreakPoints();
  NGScoreLineBreaker optimizer(node, space, line_opportunity);

  // Run the optimizer from the beginning of the `target`. This should stop at
  // `<br>` so that paragraphs separated by forced breaks are optimized
  // separately.
  //
  // Since the paragraphs has only 2 break candidates, it should return two
  // `NGLineInfo` without the optimization.
  const NGInlineBreakToken* break_token = nullptr;
  optimizer.OptimalBreakPoints(break_token, context);
  EXPECT_EQ(break_points.size(), 0u);
  EXPECT_EQ(line_info_list.Size(), 2u);

  // Pretend all the lines are consumed.
  break_token = line_info_list.Back().BreakToken();
  EXPECT_TRUE(break_token);
  line_info_list.Clear();
  context.DidCreateLine();

  // Run the optimizer again to continue. This should run up to the end of
  // `target`. It has 4 break candidates so the optimization should apply.
  optimizer.OptimalBreakPoints(break_token, context);
  EXPECT_EQ(break_points.size(), 3u);
  // `line_info_list` should be partially cleared, only after break points were
  // different.
  EXPECT_NE(line_info_list.Size(), 3u);
}

}  // namespace blink
