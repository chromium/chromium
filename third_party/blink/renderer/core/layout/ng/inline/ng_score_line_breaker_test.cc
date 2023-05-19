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

class NGScoreLineBreakerTest : public RenderingTest {};

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
