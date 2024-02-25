// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_counter_style_rule.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class LayoutInlineListItemTest : public RenderingTest {};

// crbug.com/1446554
TEST_F(LayoutInlineListItemTest, GetOffsetMappingNoCrash) {
  SetBodyInnerHTML(R"HTML(
<ul>
  <li></li>
  <li style="display:none" id=li2>foo</li>
  <li style="display:inline list-item" id="li3">bar</li>
</ul>
<style>
li {
  list-style: upper-alpha;
}
</style>)HTML");
  GetElementById("li2")->removeAttribute(html_names::kStyleAttr);
  UpdateAllLifecyclePhasesForTest();
  auto* block_flow = OffsetMapping::GetInlineFormattingContextOf(
      *GetLayoutObjectByElementId("li3"));
  ASSERT_TRUE(block_flow);
  EXPECT_FALSE(block_flow->NeedsLayout());
  EXPECT_TRUE(InlineNode::GetOffsetMapping(block_flow));
  // We had a bug that the above GetOffsetMapping() unexpectedly set
  // NeedsLayout due to a lack of SetNeedsCollectInlines.
  EXPECT_FALSE(block_flow->NeedsLayout());
  EXPECT_TRUE(InlineNode::GetOffsetMapping(block_flow));
}

// crbug.com/1512284
TEST_F(LayoutInlineListItemTest, OffsetMappingBuilderNoCrash) {
  SetBodyInnerHTML(R"HTML(<style id="s">
@counter-style foo { symbols: A; }
li { display: inline list-item; }
</style>
<ol style="list-style-type: foo;"><li id="target"></li>)HTML");

  CSSStyleSheet* sheet = To<HTMLStyleElement>(GetElementById("s"))->sheet();
  auto* rule =
      To<CSSCounterStyleRule>(sheet->cssRules(ASSERT_NO_EXCEPTION)->item(0));
  rule->setPrefix(GetDocument().GetExecutionContext(), "p");
  UpdateAllLifecyclePhasesForTest();

  auto* block_flow = OffsetMapping::GetInlineFormattingContextOf(
      *GetLayoutObjectByElementId("target"));
  ASSERT_TRUE(block_flow);
  EXPECT_TRUE(InlineNode::GetOffsetMapping(block_flow));
  // We had a bug that updating a counter-style didn't trigger CollectInlines.
  // This test passes if the above GetOffsetMapping() doesn't crash by CHECK
  // failures.
}

}  // namespace blink
