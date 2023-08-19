// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_paragraph_line_breaker.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class NGParagraphLineBreakerTest : public RenderingTest {
 public:
  absl::optional<LayoutUnit> AttemptParagraphBalancing(
      const NGInlineNode& node) {
    const NGPhysicalBoxFragment* fragment =
        node.GetLayoutBox()->GetPhysicalFragment(0);
    const LayoutUnit width = fragment->Size().width;
    NGConstraintSpace space = ConstraintSpaceForAvailableSize(width);
    NGLineLayoutOpportunity line_opportunity(width);
    return NGParagraphLineBreaker::AttemptParagraphBalancing(node, space,
                                                             line_opportunity);
  }
};

TEST_F(NGParagraphLineBreakerTest, IsDisabledByBlockInInline) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    #target {
      font-size: 10px;
      width: 10ch;
    }
    </style>
    <div id="target">
      <span>
        1234 6789
        1234 6789
        <div>block-in-inline</div>
        1234 6789
        1234 6789
      </span>
    </div>
  )HTML");
  const NGInlineNode target = GetInlineNodeByElementId("target");
  EXPECT_TRUE(target.IsBisectLineBreakDisabled());
  EXPECT_FALSE(target.IsScoreLineBreakDisabled());
  EXPECT_FALSE(AttemptParagraphBalancing(target));
}

TEST_F(NGParagraphLineBreakerTest, IsDisabledByFirstLine) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    #target {
      font-size: 10px;
      width: 10ch;
    }
    #target::first-line {
      font-weight: bold;
    }
    </style>
    <div id="target">
      1234 6789
      1234 6789
    </div>
  )HTML");
  const NGInlineNode target = GetInlineNodeByElementId("target");
  EXPECT_FALSE(target.IsBisectLineBreakDisabled());
  EXPECT_TRUE(target.IsScoreLineBreakDisabled());
  EXPECT_TRUE(AttemptParagraphBalancing(target));
}

TEST_F(NGParagraphLineBreakerTest, IsDisabledByFloatLeading) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    #target {
      font-size: 10px;
      width: 10ch;
    }
    .float { float: left; }
    </style>
    <div id="target">
      <div class="float">float</div>
      1234 6789
      1234 6789
    </div>
  )HTML");
  const NGInlineNode target = GetInlineNodeByElementId("target");
  EXPECT_TRUE(target.IsBisectLineBreakDisabled());
  EXPECT_FALSE(target.IsScoreLineBreakDisabled());
  EXPECT_FALSE(AttemptParagraphBalancing(target));
}

TEST_F(NGParagraphLineBreakerTest, IsDisabledByFloat) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    #target {
      font-size: 10px;
      width: 10ch;
    }
    .float { float: left; }
    </style>
    <div id="target">
      1234 6789
      <div class="float">float</div>
      1234 6789
    </div>
  )HTML");
  const NGInlineNode target = GetInlineNodeByElementId("target");
  EXPECT_TRUE(target.IsBisectLineBreakDisabled());
  EXPECT_FALSE(target.IsScoreLineBreakDisabled());
  EXPECT_FALSE(AttemptParagraphBalancing(target));
}

TEST_F(NGParagraphLineBreakerTest, IsDisabledByForcedBreak) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    #target {
      font-size: 10px;
      width: 10ch;
    }
    </style>
    <div id="target">
      1234 6789
      <br>
      1234 6789
    </div>
  )HTML");
  const NGInlineNode target = GetInlineNodeByElementId("target");
  EXPECT_TRUE(target.IsBisectLineBreakDisabled());
  EXPECT_FALSE(target.IsScoreLineBreakDisabled());
  EXPECT_FALSE(AttemptParagraphBalancing(target));
}

TEST_F(NGParagraphLineBreakerTest, IsDisabledByForcedBreakReusing) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    #target {
      font-size: 10px;
      width: 10ch;
      white-space: pre;
    }
    </style>
    <div id="target">1234 6789
1234
    </div>
  )HTML");
  const NGInlineNode target = GetInlineNodeByElementId("target");
  Element* target_node = To<Element>(target.GetDOMNode());
  target_node->AppendChild(GetDocument().createTextNode(" 6789"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(target.IsBisectLineBreakDisabled());
  EXPECT_FALSE(target.IsScoreLineBreakDisabled());
  EXPECT_FALSE(AttemptParagraphBalancing(target));
}

TEST_F(NGParagraphLineBreakerTest, IsDisabledByInitialLetter) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    #target {
      font-size: 10px;
      width: 10ch;
    }
    #target::first-letter {
      initial-letter: 2;
    }
    </style>
    <div id="target">
      1234 6789
      1234 6789
    </div>
  )HTML");
  const NGInlineNode target = GetInlineNodeByElementId("target");
  EXPECT_TRUE(target.IsBisectLineBreakDisabled());
  EXPECT_TRUE(target.IsScoreLineBreakDisabled());
  EXPECT_FALSE(AttemptParagraphBalancing(target));
}

TEST_F(NGParagraphLineBreakerTest, IsDisabledByTabulationCharacters) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    #target {
      font-size: 10px;
      width: 10ch;
      white-space: pre-wrap;
    }
    </style>
    <div id="target">1234 6789&#0009;1234 6789</div>
  )HTML");
  const NGInlineNode target = GetInlineNodeByElementId("target");
  EXPECT_FALSE(target.IsBisectLineBreakDisabled());
  EXPECT_TRUE(target.IsScoreLineBreakDisabled());
  EXPECT_TRUE(AttemptParagraphBalancing(target));
}

}  // namespace blink
