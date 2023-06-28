// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class LayoutNGInlineListItemTest : public RenderingTest {};

// crbug.com/1446554
TEST_F(LayoutNGInlineListItemTest, GetOffsetMappingNoCrash) {
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
  GetDocument()
      .getElementById(AtomicString("li2"))
      ->removeAttribute(html_names::kStyleAttr);
  UpdateAllLifecyclePhasesForTest();
  auto* block_flow = NGOffsetMapping::GetInlineFormattingContextOf(
      *GetLayoutObjectByElementId("li3"));
  ASSERT_TRUE(block_flow);
  EXPECT_FALSE(block_flow->NeedsLayout());
  EXPECT_TRUE(NGInlineNode::GetOffsetMapping(block_flow));
  // We had a bug that the above GetOffsetMapping() unexpectedly set
  // NeedsLayout due to a lack of SetNeedsCollectInlines.
  EXPECT_FALSE(block_flow->NeedsLayout());
  EXPECT_TRUE(NGInlineNode::GetOffsetMapping(block_flow));
}

}  // namespace blink
