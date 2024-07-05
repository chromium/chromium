// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/line_widths.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/inline/inline_child_layout_context.h"
#include "third_party/blink/renderer/core/layout/inline/inline_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node.h"
#include "third_party/blink/renderer/core/layout/inline/leading_floats.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

namespace {

LayoutUnit FragmentWidth(const InlineNode& node) {
  const PhysicalBoxFragment* fragment =
      node.GetLayoutBox()->GetPhysicalFragment(0);
  return fragment->Size().width;
}

}  // namespace

class LineWidthsTest : public RenderingTest {
 public:
  std::optional<LineWidths> ComputeLineWidths(InlineNode node) {
    const LayoutUnit width = FragmentWidth(node);
    ConstraintSpace space = ConstraintSpaceForAvailableSize(width);
    const ComputedStyle& style = node.Style();
    BoxFragmentBuilder container_builder(node, &style, space,
                                         style.GetWritingDirection(),
                                         /*previous_break_token=*/nullptr);
    SimpleInlineChildLayoutContext context(node, &container_builder);
    InlineLayoutAlgorithm algorithm(node, space, /*break_token*/ nullptr,
                                    /*column_spanner_path*/ nullptr, &context);
    ExclusionSpace exclusion_space(space.GetExclusionSpace());
    LeadingFloats leading_floats;
    algorithm.PositionLeadingFloats(exclusion_space, leading_floats);
    const LayoutOpportunityVector& opportunities =
        exclusion_space.AllLayoutOpportunities(
            {space.GetBfcOffset().line_offset,
             /*bfc_block_offset*/ LayoutUnit()},
            space.AvailableSize().inline_size);
    LineWidths line_width;
    if (line_width.Set(node, opportunities)) {
      return line_width;
    }
    return std::nullopt;
  }

 protected:
};

struct LineWidthsData {
  std::vector<int> widths;
  const char* html;
} line_widths_data[] = {
    // It should be computable if no floats.
    {{100, 100}, R"HTML(
      <div id="target">0123 5678</div>
    )HTML"},
    {{100, 100}, R"HTML(
      <div id="target">0123 <b>5</b>678</div>
    )HTML"},
    // Single left/right leading float should be computable.
    {{70, 100}, R"HTML(
      <div id="target">
        <div class="left"></div>
        0123 5678
      </div>
    )HTML"},
    {{70, 100}, R"HTML(
      <div id="target">
        <div class="right"></div>
        0123 5678
      </div
    )HTML"},
    // Non-leading floats are not computable.
    {{}, R"HTML(
      <div id="target">
        0123 5678
        <div class="left"></div>
      </div>
    )HTML"},
    // Even when the float is taller than the font, it's computable as long as
    // it fits in the leading.
    {{70, 100}, R"HTML(
      <div id="target" style="line-height: 15px">
        <div class="left" style="height: 11px"></div>
        0123 5678
      </div>
    )HTML"},
    // The 2nd line is also narrow if the float is taller than one line.
    {{70, 70, 100}, R"HTML(
      <div id="target">
        <div class="left" style="height: 11px"></div>
        0123 5678
      </div>
    )HTML"},
    {{70, 70, 100}, R"HTML(
      <div id="target" style="line-height: 15px">
        <div class="left" style="height: 16px"></div>
        0123 5678
      </div>
    )HTML"},
    // "46.25 / 23" needs more precision than `LayoutUnit`.
    {{70, 70, 70, 100}, R"HTML(
      <div id="target" style="line-height: 23px">
        <div class="left" style="height: 46.25px"></div>
        0123 5678
      </div>
    )HTML"},
    // Multiple floats are computable if they produce single exclusion.
    {{40, 100}, R"HTML(
      <div id="target">
        <div class="left"></div>
        <div class="left"></div>
        0123 5678
      </div>
    )HTML"},
    // ...but not computable if they produce multiple exclusions.
    {{}, R"HTML(
      <div id="target">
        <div class="left" style="height: 20px"></div>
        <div class="left"></div>
        0123 5678
      </div>
    )HTML"},
    // Different `vertical-align` is not computable.
    {{}, R"HTML(
      <div id="target">
        <div class="left"></div>
        0123 5678 <span style="vertical-align: top">0</span>123 5678
      </div>
    )HTML"},
    // When it uses multiple fonts, it's computable if all its ascent/descent
    // fit to the strut (and therefore the line height is the same as the single
    // font case,) but not so otherwise.
    {{70, 100}, R"HTML(
      <div id="target">
        <div class="left"></div>
        0123 5678 <small>0123</small> 5678
      </div>
    )HTML"},
    {{}, R"HTML(
      <div id="target">
        <div class="left"></div>
        0123 5678 <big>0123</big> 5678
      </div>
    )HTML"},
    // Atomic inlines are not computable if there are leading floats.
    {{100, 100}, R"HTML(
      <div id="target">
        0123 <span style="display: inline-block"></span> 5678
      </div>
    )HTML"},
    {{}, R"HTML(
      <div id="target">
        <div class="left"></div>
        0123 <span style="display: inline-block"></span> 5678
      </div>
    )HTML"},
};
class LineWidthsDataTest : public LineWidthsTest,
                           public testing::WithParamInterface<LineWidthsData> {
};
INSTANTIATE_TEST_SUITE_P(LineWidthsTest,
                         LineWidthsDataTest,
                         testing::ValuesIn(line_widths_data));

TEST_P(LineWidthsDataTest, Data) {
  const auto& data = GetParam();
  LoadAhem();
  SetBodyInnerHTML(String::Format(R"HTML(
    <!DOCTYPE html>
    <style>
    #target {
      font-family: Ahem;
      font-size: 10px;
      width: 100px;
    }
    .left {
      float: left;
      width: 30px;
      height: 10px;
    }
    .right {
      float: right;
      width: 30px;
      height: 10px;
    }
    </style>
    %s
  )HTML",
                                  data.html));
  const InlineNode target = GetInlineNodeByElementId("target");
  const std::optional<LineWidths> line_widths = ComputeLineWidths(target);
  std::vector<int> actual_widths;
  if (line_widths) {
    const size_t size = data.widths.size() ? data.widths.size() : 3;
    for (wtf_size_t i = 0; i < size; ++i) {
      actual_widths.push_back((*line_widths)[i].ToInt());
    }
  }
  EXPECT_THAT(actual_widths, data.widths);
}

}  // namespace blink
