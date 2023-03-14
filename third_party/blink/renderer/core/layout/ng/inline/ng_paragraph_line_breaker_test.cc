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
  absl::optional<LayoutUnit> AttemptParagraphBalancing(const char* id) {
    auto* layout_object = To<LayoutBlockFlow>(GetLayoutObjectByElementId(id));
    const NGPhysicalBoxFragment* fragment =
        layout_object->GetPhysicalFragment(0);
    const LayoutUnit width = fragment->Size().width;

    NGInlineNode node(layout_object);
    NGConstraintSpaceBuilder builder(
        WritingMode::kHorizontalTb,
        {WritingMode::kHorizontalTb, TextDirection::kLtr},
        /* is_new_fc */ false);
    builder.SetAvailableSize(LogicalSize(width, LayoutUnit::Max()));
    NGConstraintSpace space = builder.ToConstraintSpace();
    NGLineLayoutOpportunity line_opportunity(width);
    return NGParagraphLineBreaker::AttemptParagraphBalancing(node, space,
                                                             line_opportunity);
  }
};

TEST_F(NGParagraphLineBreakerTest, ShouldFailByBlockInInline) {
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
  EXPECT_FALSE(AttemptParagraphBalancing("target"));
}

TEST_F(NGParagraphLineBreakerTest, ShouldFailByFloat) {
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
  EXPECT_FALSE(AttemptParagraphBalancing("target"));
}

TEST_F(NGParagraphLineBreakerTest, ShouldFailByForcedBreak) {
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
  EXPECT_FALSE(AttemptParagraphBalancing("target"));
}

TEST_F(NGParagraphLineBreakerTest, ShouldFailByInitialLetter) {
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
  EXPECT_FALSE(AttemptParagraphBalancing("target"));
}

}  // namespace blink
