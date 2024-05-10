// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/line_info.h"

#include "third_party/blink/renderer/core/layout/inline/line_breaker.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class LineInfoTest : public RenderingTest {
 protected:
  InlineNode CreateInlineNode(const String& html_content) {
    SetBodyInnerHTML(html_content);

    LayoutBlockFlow* block_flow =
        To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
    return InlineNode(block_flow);
  }
};

TEST_F(LineInfoTest, InflowEndOffset) {
  InlineNode node = CreateInlineNode(R"HTML(
      <div id=container>abc<ruby>def<rt>rt</ruby></div>)HTML");
  node.PrepareLayoutIfNeeded();
  ExclusionSpace exclusion_space;
  LeadingFloats leading_floats;
  ConstraintSpace space = ConstraintSpaceForAvailableSize(LayoutUnit::Max());
  LineBreaker line_breaker(node, LineBreakerMode::kContent, space,
                           LineLayoutOpportunity(LayoutUnit::Max()),
                           leading_floats, nullptr, nullptr, &exclusion_space);
  LineInfo line_info;
  line_breaker.NextLine(&line_info);
  EXPECT_EQ(InlineItem::kOpenRubyColumn, line_info.Results()[2].item->Type());
  // InflowEndOffset() should return the end offset of a text in the ruby-base.
  // 7 == "abc" + kOpenRubyColumn + "def"
  EXPECT_EQ(7u, line_info.InflowEndOffset());
}

}  // namespace blink
