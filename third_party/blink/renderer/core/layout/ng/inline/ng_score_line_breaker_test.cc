// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_score_line_breaker.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_leading_floats.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_break_point.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_info_list.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_widths.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

namespace {

LayoutUnit FragmentWidth(const NGInlineNode& node) {
  const NGPhysicalBoxFragment* fragment =
      node.GetLayoutBox()->GetPhysicalFragment(0);
  return fragment->Size().width;
}

void TestLinesAreContiguous(const NGLineInfoList& line_info_list) {
  for (wtf_size_t i = 1; i < line_info_list.Size(); ++i) {
    EXPECT_EQ(line_info_list[i].Start(),
              line_info_list[i - 1].BreakToken()->Start());
  }
}

}  // namespace

class NGScoreLineBreakerTest : public RenderingTest {
 public:
  void RunUntilSuspended(NGScoreLineBreaker& breaker,
                         NGScoreLineBreakContext& context) {
    NGLineInfoList& line_info_list = context.LineInfoList();
    line_info_list.Clear();
    NGLineBreakPoints& break_points = context.LineBreakPoints();
    break_points.clear();
    context.DidCreateLine(/*is_end_paragraph*/ true);
    NGLeadingFloats empty_leading_floats;
    for (;;) {
      breaker.OptimalBreakPoints(empty_leading_floats, context);
      if (!context.IsActive() || !breaker.BreakToken() ||
          line_info_list.IsEmpty()) {
        break;
      }

      // Consume the first line in `line_info_list`.
      const NGLineInfo& line_info = line_info_list.Front();
      const bool is_end_paragraph = line_info.IsEndParagraph();
      line_info_list.RemoveFront();
      context.DidCreateLine(is_end_paragraph);
    }
  }

  Vector<float> ComputeScores(const NGInlineNode& node) {
    const LayoutUnit width = FragmentWidth(node);
    NGConstraintSpace space = ConstraintSpaceForAvailableSize(width);
    NGLineWidths line_widths(width);
    const NGInlineBreakToken* break_token = nullptr;
    NGExclusionSpace exclusion_space;
    NGScoreLineBreaker optimizer(node, space, line_widths, break_token,
                                 &exclusion_space);
    Vector<float> scores;
    optimizer.SetScoresOutForTesting(&scores);
    NGLeadingFloats empty_leading_floats;
    NGScoreLineBreakContextOf<kMaxLinesForOptimal> context;
    optimizer.OptimalBreakPoints(empty_leading_floats, context);
    return scores;
  }
};

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
  const LayoutUnit width = FragmentWidth(node);
  NGConstraintSpace space = ConstraintSpaceForAvailableSize(width);
  NGLineWidths line_widths(width);
  NGScoreLineBreakContextOf<kMaxLinesForOptimal> context;
  NGLineInfoList& line_info_list = context.LineInfoList();
  const NGInlineBreakToken* break_token = nullptr;
  NGExclusionSpace exclusion_space;
  NGScoreLineBreaker optimizer(node, space, line_widths, break_token,
                               &exclusion_space);

  // Run the optimizer from the beginning of the `target`. This should cache
  // `optimizer.MaxLines()` lines.
  NGLeadingFloats empty_leading_floats;
  optimizer.OptimalBreakPoints(empty_leading_floats, context);
  EXPECT_EQ(line_info_list.Size(), optimizer.MaxLines());
  TestLinesAreContiguous(line_info_list);

  // Then continue until `NGScoreLineBreaker` consumes all lines in the block.
  wtf_size_t count = 0;
  for (; context.IsActive(); ++count) {
    // Consume the first line in `line_info_list`.
    bool is_cached = false;
    const NGLineInfo& line_info0 = line_info_list.Get(break_token, is_cached);
    EXPECT_TRUE(is_cached);
    EXPECT_EQ(line_info_list.Size(), optimizer.MaxLines() - 1);
    break_token = line_info0.BreakToken();
    // Running again should cache one more line.
    optimizer.OptimalBreakPoints(empty_leading_floats, context);
    EXPECT_EQ(line_info_list.Size(), optimizer.MaxLines());
    TestLinesAreContiguous(line_info_list);
  }
  // All is done. The `BreakToken` should be null, and there should be 6 lines.
  EXPECT_FALSE(line_info_list.Back().BreakToken());
  constexpr wtf_size_t target_num_lines = 6;
  EXPECT_EQ(count, target_num_lines - optimizer.MaxLines());
}

TEST_F(NGScoreLineBreakerTest, BalanceMaxLinesExceeded) {
  ScopedCSSTextWrapBalanceByScoreForTest balance_by_score(true);
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    #target {
      font-family: Ahem;
      font-size: 10px;
      width: 10ch;
      text-wrap: balance;
    }
    </style>
    <div id="target">
      123 56 89 123 56 89
      123 56 89 123 56 89
      123 56 89 123 56 89
      123 56 89 123 56 89
      123 56 89 123 56 89
      X
    </div>
  )HTML");
  const LayoutBlockFlow* target = GetLayoutBlockFlowByElementId("target");
  NGInlineCursor cursor(*target);
  cursor.MoveToLastLine();
  cursor.MoveToNext();
  // Neitehr `balance` nor `pretty` should be applied.
  EXPECT_EQ(cursor.Current()->Type(), NGFragmentItem::kText);
  EXPECT_EQ(cursor.Current()->TextLength(), 1u);
}

class BlockInInlineTest : public NGScoreLineBreakerTest,
                          public testing::WithParamInterface<int> {};
INSTANTIATE_TEST_SUITE_P(NGScoreLineBreakerTest,
                         BlockInInlineTest,
                         testing::Range(0, 4));

TEST_P(BlockInInlineTest, BeforeAfter) {
  LoadAhem();
  const int test_index = GetParam();
  const bool has_before = test_index & 1;
  const bool has_after = test_index & 2;
  SetBodyInnerHTML(String::Format(
      R"HTML(
    <!DOCTYPE html>
    <style>
    #target {
      font-family: Ahem;
      font-size: 10px;
      width: 10ch;
    }
    </style>
    <div id="target">
      <span>%s<div>
        Inside 89 1234 6789 1234 6789 1234 6789 12
      </div>%s</span>
    </div>
  )HTML",
      has_before ? "Before 89 1234 6789 1234 6789 1234 6789 12" : "",
      has_after ? "After 789 1234 6789 1234 6789 1234 6789 12" : ""));
  const NGInlineNode node = GetInlineNodeByElementId("target");
  const LayoutUnit width = FragmentWidth(node);
  NGConstraintSpace space = ConstraintSpaceForAvailableSize(width);
  NGLineWidths line_widths(width);
  NGScoreLineBreakContextOf<kMaxLinesForOptimal> context;
  NGLineInfoList& line_info_list = context.LineInfoList();
  NGLineBreakPoints& break_points = context.LineBreakPoints();
  NGExclusionSpace exclusion_space;
  NGScoreLineBreaker optimizer(node, space, line_widths,
                               /*break_token*/ nullptr, &exclusion_space);
  // The `NGScoreLineBreaker` should suspend at before the block-in-inline.
  RunUntilSuspended(optimizer, context);
  if (has_before) {
    // The content before the block-in-inline should be optimized.
    EXPECT_NE(break_points.size(), 0u);
  } else {
    // The content before the block-in-inline is just a `<span>`.
    EXPECT_EQ(break_points.size(), 0u);
    EXPECT_EQ(line_info_list.Size(), 1u);
    EXPECT_TRUE(line_info_list[0].HasForcedBreak());
  }

  // Then the block-in-inline comes. Since it's like an atomic inline, it's not
  // optimized.
  RunUntilSuspended(optimizer, context);
  EXPECT_EQ(break_points.size(), 0u);
  EXPECT_EQ(line_info_list.Size(), 1u);
  EXPECT_TRUE(line_info_list[0].IsBlockInInline());
  EXPECT_TRUE(line_info_list[0].HasForcedBreak());

  // Then the content after the block-in-inline.
  RunUntilSuspended(optimizer, context);
  if (has_after) {
    EXPECT_NE(break_points.size(), 0u);
  } else {
    EXPECT_EQ(break_points.size(), 0u);
    EXPECT_EQ(line_info_list.Size(), 1u);
  }
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
  const LayoutUnit width = FragmentWidth(node);
  NGConstraintSpace space = ConstraintSpaceForAvailableSize(width);
  NGLineWidths line_widths(width);
  NGScoreLineBreakContextOf<kMaxLinesForOptimal> context;
  NGLineInfoList& line_info_list = context.LineInfoList();
  NGLineBreakPoints& break_points = context.LineBreakPoints();
  const NGInlineBreakToken* break_token = nullptr;
  NGExclusionSpace exclusion_space;
  NGScoreLineBreaker optimizer(node, space, line_widths, break_token,
                               &exclusion_space);

  // Run the optimizer from the beginning of the `target`. This should stop at
  // `<br>` so that paragraphs separated by forced breaks are optimized
  // separately.
  //
  // Since the paragraphs has only 2 break candidates, it should return two
  // `NGLineInfo` without the optimization.
  NGLeadingFloats empty_leading_floats;
  optimizer.OptimalBreakPoints(empty_leading_floats, context);
  EXPECT_EQ(break_points.size(), 0u);
  EXPECT_EQ(line_info_list.Size(), 2u);

  // Pretend all the lines are consumed.
  EXPECT_TRUE(optimizer.BreakToken());
  line_info_list.Clear();
  context.DidCreateLine(/*is_end_paragraph*/ true);

  // Run the optimizer again to continue. This should run up to the end of
  // `target`. It has 4 break candidates so the optimization should apply.
  optimizer.OptimalBreakPoints(empty_leading_floats, context);
  EXPECT_EQ(break_points.size(), 3u);
  // `line_info_list` should be partially cleared, only after break points were
  // different.
  EXPECT_NE(line_info_list.Size(), 3u);
}

struct DisabledByLineBreakerData {
  bool disabled;
  const char* html;
} disabled_by_line_breaker_data[] = {
    // Normal, should not be disabled.
    {false, R"HTML(
      <div id="target">
        0123 5678
        1234 6789
        234 67890
        45
      </div>
    )HTML"},
    // Overflowing lines should disable.
    {true, R"HTML(
      <div id="target">
        0123 5678
        123456789012
        23 567 90
        45
      </div>
    )HTML"},
    // `overflow-wrap` should be ok, except...
    {false, R"HTML(
      <div id="target" style="overflow-wrap: anywhere">
        0123 5678
        1234 6789
        23 567 90
        45
      </div>
    )HTML"},
    {false, R"HTML(
      <div id="target" style="overflow-wrap: break-word">
        0123 5678
        1234 6789
        23 567 90
        45
      </div>
    )HTML"},
    // ...when there're overflows.
    {true, R"HTML(
      <div id="target" style="overflow-wrap: anywhere">
        0123 5678
        123456789012
        23 567 90
        45
      </div>
    )HTML"},
    {true, R"HTML(
      <div id="target" style="overflow-wrap: break-word">
        0123 5678
        123456789012
        23 567 90
        45
      </div>
    )HTML"},
    // `break-sapces` is not supported.
    {true, R"HTML(
      <div id="target" style="white-space: break-spaces">0123 5678 1234 6789 23 567 90 45</div>
    )HTML"},
    // `box-decoration-break: clone` is not supported.
    {true, R"HTML(
      <div id="target">
        0123 5678
        1234 6789
        23 <span style="-webkit-box-decoration-break: clone">567</span> 90
        45
      </div>
    )HTML"}};

class DisabledByLineBreakerTest
    : public NGScoreLineBreakerTest,
      public testing::WithParamInterface<DisabledByLineBreakerData> {};

INSTANTIATE_TEST_SUITE_P(NGScoreLineBreakerTest,
                         DisabledByLineBreakerTest,
                         testing::ValuesIn(disabled_by_line_breaker_data));

TEST_P(DisabledByLineBreakerTest, Data) {
  const auto& data = GetParam();
  LoadAhem();
  SetBodyInnerHTML(String(R"HTML(
    <!DOCTYPE html>
    <style>
    #target {
      font-family: Ahem;
      font-size: 10px;
      width: 10ch;
    }
    </style>
  )HTML") + data.html);
  const NGInlineNode node = GetInlineNodeByElementId("target");
  const LayoutUnit width = FragmentWidth(node);
  NGConstraintSpace space = ConstraintSpaceForAvailableSize(width);
  NGLineWidths line_widths(width);
  NGScoreLineBreakContextOf<kMaxLinesForOptimal> context;
  const NGInlineBreakToken* break_token = nullptr;
  NGExclusionSpace exclusion_space;
  NGScoreLineBreaker optimizer(node, space, line_widths, break_token,
                               &exclusion_space);
  NGLeadingFloats empty_leading_floats;
  optimizer.OptimalBreakPoints(empty_leading_floats, context);
  EXPECT_FALSE(context.IsActive());
  if (data.disabled) {
    EXPECT_EQ(context.LineBreakPoints().size(), 0u);
  } else {
    EXPECT_NE(context.LineBreakPoints().size(), 0u);
  }
}

// Test when `NGInlineLayoutAlgorithm::Layout` runs `NGLineBreaker` twice for
// the same line, to retry line breaking due to float placements.
TEST_F(NGScoreLineBreakerTest, FloatRetry) {
  ScopedCSSTextWrapPrettyForTest enable(true);
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    .container {
      font-size: 16px;
      text-wrap: pretty;
      width: 110px;
    }
    .float {
      float: right;
      width: 50px;
      height: 50px;
    }
    </style>
    <div class="container">
      <div class="float"></div>
      Blah.
      <div class="float"></div>
      Blah blah blah.
    </div>
  )HTML");
  // Test pass if it doesn't crash.
}

TEST_F(NGScoreLineBreakerTest, Zoom) {
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
      012 45 789
      012 45 789
      012 45 789
      012
    </div>
  )HTML");
  const NGInlineNode target = GetInlineNodeByElementId("target");
  Vector<float> scores = ComputeScores(target);

  constexpr float zoom = 2;
  GetFrame().SetPageZoomFactor(zoom);
  UpdateAllLifecyclePhasesForTest();
  const Vector<float> scores2 = ComputeScores(target);

  // The scores should be the same even when `EffectiveZoom()` are different.
  EXPECT_EQ(scores.size(), scores2.size());
  for (wtf_size_t i = 0; i < scores.size(); ++i) {
    const float zoomed_score = scores[i] * zoom;
    if (fabs(zoomed_score - scores2[i]) < 3) {
      continue;  // Ignore floating point errors.
    }
    EXPECT_EQ(zoomed_score, scores2[i]) << i;
  }
}

}  // namespace blink
