// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_break_candidate.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_breaker.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_info.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"

namespace blink {

class NGLineBreakCandidateTest : public RenderingTest {
 public:
  bool ComputeCandidates(const NGInlineNode& node,
                         LayoutUnit available_width,
                         NGLineBreakCandidates& candidates) {
    NGConstraintSpace space = ConstraintSpaceForAvailableSize(available_width);
    NGExclusionSpace exclusion_space;
    NGLeadingFloats leading_floats;
    NGLineLayoutOpportunity line_opportunity(available_width);
    const NGInlineBreakToken* break_token = nullptr;
    NGLineInfo line_info;
    NGLineBreakCandidateContext context(candidates);
    bool is_first = true;
    do {
      NGLineBreaker line_breaker(node, NGLineBreakerMode::kContent, space,
                                 line_opportunity, leading_floats, break_token,
                                 /* column_spanner_path */ nullptr,
                                 &exclusion_space);
      line_breaker.NextLine(&line_info);
      if (is_first) {
        context.EnsureFirstSentinel(line_info);
        is_first = false;
      }
      if (!context.AppendLine(line_info, line_breaker)) {
        return false;
      }
      break_token = line_info.BreakToken();
    } while (break_token);
    context.EnsureLastSentinel(line_info);
    return true;
  }
};

TEST_F(NGLineBreakCandidateTest, Text) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    #target {
      font-family: Ahem;
      font-size: 10px;
    }
    </style>
    <div id="target">
      01 345
    </div>
  )HTML");
  const NGInlineNode target = GetInlineNodeByElementId("target");
  for (int width : {800, 50, 10}) {
    NGLineBreakCandidates candidates;
    EXPECT_TRUE(ComputeCandidates(target, LayoutUnit(width), candidates));
    EXPECT_THAT(candidates, testing::ElementsAre(
                                NGLineBreakCandidate({0, 0}, 0),
                                NGLineBreakCandidate({0, 3}, {0, 2}, 30, 20),
                                NGLineBreakCandidate({0, 6}, 60)))
        << String::Format("Width=%d", width);
  }
}

TEST_F(NGLineBreakCandidateTest, SoftHyphen) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    #target {
      font-family: Ahem;
      font-size: 10px;
    }
    </style>
    <div id="target">
      01&shy;345&shy;7890&shy;
    </div>
  )HTML");
  const NGInlineNode target = GetInlineNodeByElementId("target");
  for (int width : {800, 70, 60, 50, 10}) {
    NGLineBreakCandidates candidates;
    EXPECT_TRUE(ComputeCandidates(target, LayoutUnit(width), candidates));
    EXPECT_THAT(candidates,
                testing::ElementsAre(
                    NGLineBreakCandidate({0, 0}, 0),
                    NGLineBreakCandidate({0, 3}, {0, 3}, 20, 30, 0, true),
                    NGLineBreakCandidate({0, 7}, {0, 7}, 50, 60, 0, true),
                    NGLineBreakCandidate({0, 12}, 90)))
        << String::Format("Width=%d", width);
  }
}

TEST_F(NGLineBreakCandidateTest, SoftHyphenDisabled) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    #target {
      font-family: Ahem;
      font-size: 10px;
      hyphens: none;
    }
    </style>
    <div id="target">
      01&shy;345&shy;7890
    </div>
  )HTML");
  const NGInlineNode target = GetInlineNodeByElementId("target");
  for (int width : {800, 60, 10}) {
    NGLineBreakCandidates candidates;
    EXPECT_TRUE(ComputeCandidates(target, LayoutUnit(width), candidates));
    EXPECT_THAT(candidates,
                testing::ElementsAre(NGLineBreakCandidate({0, 0}, 0),
                                     NGLineBreakCandidate({0, 11}, 90)))
        << String::Format("Width=%d", width);
  }
}

TEST_F(NGLineBreakCandidateTest, Span) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    #target {
      font-family: Ahem;
      font-size: 10px;
    }
    </style>
    <div id="target">
      01 <span>345</span> 7890
    </div>
  )HTML");
  const NGInlineNode target = GetInlineNodeByElementId("target");
  for (const int width : {800, 60, 50, 10}) {
    NGLineBreakCandidates candidates;
    EXPECT_TRUE(ComputeCandidates(target, LayoutUnit(width), candidates));
    EXPECT_THAT(candidates, testing::ElementsAre(
                                NGLineBreakCandidate({0, 0}, 0),
                                NGLineBreakCandidate({0, 3}, {0, 2}, 30, 20),
                                NGLineBreakCandidate({4, 7}, {2, 6}, 70, 60),
                                NGLineBreakCandidate({4, 11}, 110)))
        << String::Format("Width=%d", width);
  }
}

TEST_F(NGLineBreakCandidateTest, SpanMidWord) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    #target {
      font-family: Ahem;
      font-size: 10px;
    }
    </style>
    <div id="target">
      0<span>12</span>345 7890
    </div>
  )HTML");
  const NGInlineNode target = GetInlineNodeByElementId("target");
  for (const int width : {800, 80, 10}) {
    NGLineBreakCandidates candidates;
    EXPECT_TRUE(ComputeCandidates(target, LayoutUnit(width), candidates));
    EXPECT_THAT(candidates, testing::ElementsAre(
                                NGLineBreakCandidate({0, 0}, 0),
                                NGLineBreakCandidate({4, 7}, {4, 6}, 70, 60),
                                NGLineBreakCandidate({4, 11}, 110)))
        << String::Format("Width=%d", width);
  }
}

TEST_F(NGLineBreakCandidateTest, SpanCloseAfterSpace) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    #target {
      font-family: Ahem;
      font-size: 10px;
    }
    </style>
    <div id="target">
      01 <span>345 </span>7890
    </div>
  )HTML");
  const NGInlineNode target = GetInlineNodeByElementId("target");
  for (const int width : {800, 50, 10}) {
    NGLineBreakCandidates candidates;
    EXPECT_TRUE(ComputeCandidates(target, LayoutUnit(width), candidates));
    EXPECT_THAT(candidates, testing::ElementsAre(
                                NGLineBreakCandidate({0, 0}, 0),
                                NGLineBreakCandidate({0, 3}, {0, 2}, 30, 20),
                                NGLineBreakCandidate({4, 7}, {2, 6}, 70, 60),
                                NGLineBreakCandidate({4, 11}, 110)))
        << String::Format("Width=%d", width);
  }
}

TEST_F(NGLineBreakCandidateTest, TrailingSpacesCollapsed) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    #target {
      font-family: Ahem;
      font-size: 10px;
    }
    </style>
    <div id="target">
      012 <span style="font-size: 20px"> </span>456
    </div>
  )HTML");
  const NGInlineNode target = GetInlineNodeByElementId("target");
  for (const int width : {800, 50, 10}) {
    NGLineBreakCandidates candidates;
    EXPECT_TRUE(ComputeCandidates(target, LayoutUnit(width), candidates));
    // TODO(kojii): There shouldn't be a break opportunity before `<span>`, but
    // `item_results[0].can_break_after` is set.
    if (width < 70) {
      EXPECT_THAT(candidates, testing::ElementsAre(
                                  NGLineBreakCandidate({0, 0}, 0),
                                  NGLineBreakCandidate({0, 4}, {0, 3}, 40, 30),
                                  NGLineBreakCandidate({4, 4}, {0, 3}, 40, 30),
                                  NGLineBreakCandidate({4, 7}, 70)))
          << String::Format("Width=%d", width);
      continue;
    }
    EXPECT_THAT(candidates, testing::ElementsAre(
                                NGLineBreakCandidate({0, 0}, 0),
                                NGLineBreakCandidate({0, 4}, {0, 3}, 40, 30),
                                NGLineBreakCandidate({4, 7}, 70)))
        << String::Format("Width=%d", width);
  }
}

TEST_F(NGLineBreakCandidateTest, AtomicInline) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    #target {
      font-family: Ahem;
      font-size: 10px;
    }
    span {
      display: inline-block;
      width: 1em;
    }
    </style>
    <div id="target"><span></span><span></span></div>
  )HTML");
  const NGInlineNode target = GetInlineNodeByElementId("target");
  for (const int width : {800, 10}) {
    NGLineBreakCandidates candidates;
    EXPECT_TRUE(ComputeCandidates(target, LayoutUnit(width), candidates));
    EXPECT_THAT(candidates,
                testing::ElementsAre(NGLineBreakCandidate({0, 0}, 0),
                                     NGLineBreakCandidate({1, 1}, 10),
                                     NGLineBreakCandidate({2, 2}, 20)))
        << String::Format("Width=%d", width);
  }
}

// fast/borders/border-image-border-radius.html
TEST_F(NGLineBreakCandidateTest, AtomicInlineBr) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    #target {
      font-family: Ahem;
      font-size: 10px;
    }
    span {
      display: inline-block;
      width: 1em;
    }
    </style>
    <div id="target">
      <span></span>
      <br>
    </div>
  )HTML");
  const NGInlineNode target = GetInlineNodeByElementId("target");
  for (const int width : {800, 10}) {
    NGLineBreakCandidates candidates;
    EXPECT_TRUE(ComputeCandidates(target, LayoutUnit(width), candidates));
    EXPECT_THAT(candidates, testing::ElementsAre(
                                NGLineBreakCandidate({0, 0}, 0),
                                NGLineBreakCandidate({2, 2}, {1, 1}, 10, 10)))
        << String::Format("Width=%d", width);
  }
}

// All/VisualRectMappingTest.LayoutTextContainerFlippedWritingMode/6
TEST_F(NGLineBreakCandidateTest, AtomicInlineTrailingSpaces) {
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
  const NGInlineNode target = GetInlineNodeByElementId("target");
  for (const int width : {800, 10}) {
    NGLineBreakCandidates candidates;
    EXPECT_TRUE(ComputeCandidates(target, LayoutUnit(width), candidates));
    EXPECT_THAT(candidates, testing::ElementsAre(
                                NGLineBreakCandidate({0, 0}, 0),
                                // TODO(kojii): {3,2} should be {4,2}.
                                NGLineBreakCandidate({3, 2}, {2, 1}, 20, 10),
                                NGLineBreakCandidate({7, 4}, {5, 4}, 40, 40)))
        << String::Format("Width=%d", width);
  }
}

TEST_F(NGLineBreakCandidateTest, ForcedBreak) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    #target {
      font-family: Ahem;
      font-size: 10px;
    }
    </style>
    <div id="target">
      01 345<br>
      01 3456 <br>
    </div>
  )HTML");
  const NGInlineNode target = GetInlineNodeByElementId("target");
  for (const int width : {800, 40, 10}) {
    NGLineBreakCandidates candidates;
    EXPECT_TRUE(ComputeCandidates(target, LayoutUnit(width), candidates));
    EXPECT_THAT(
        candidates,
        testing::ElementsAre(NGLineBreakCandidate({0, 0}, 0),
                             NGLineBreakCandidate({0, 3}, {0, 2}, 30, 20),
                             NGLineBreakCandidate({1, 7}, {0, 6}, 60, 60),
                             NGLineBreakCandidate({2, 10}, {2, 9}, 90, 80),
                             NGLineBreakCandidate({3, 15}, {2, 14}, 130, 130)))
        << String::Format("Width=%d", width);
  }
}

}  // namespace blink
