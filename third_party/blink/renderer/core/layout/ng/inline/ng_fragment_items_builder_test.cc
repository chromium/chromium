// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items_builder.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_logical_line_item.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

using testing::ElementsAre;

namespace blink {

class NGFragmentItemsBuilderTest : public RenderingTest {};

TEST_F(NGFragmentItemsBuilderTest, MultipleLogicalLineItems) {
  SetBodyInnerHTML(R"HTML(
    <div id="container">
      1<br>
      2
    </div>
  )HTML");
  LayoutBlockFlow* container =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));

  // Get |NGPhysicalLineBoxFragment|s to use for testing.
  NGInlineCursor cursor(*container);
  cursor.MoveToFirstLine();
  const NGPhysicalLineBoxFragment* line_fragment1 =
      cursor.Current()->LineBoxFragment();
  cursor.MoveToNextLine();
  const NGPhysicalLineBoxFragment* line_fragment2 =
      cursor.Current()->LineBoxFragment();

  NGInlineNode inline_node(container);
  {
    // First test emulates what |NGBlockLayoutAlgorithm| does, which loops
    // following calls for each line:
    // 1. |AcquireLogicalLineItems|
    // 2. |AssociateLogicalLineItems|
    // 3. |AddLine|.
    NGFragmentItemsBuilder items_builder(
        inline_node, {WritingMode::kHorizontalTb, TextDirection::kLtr});
    NGLogicalLineItems* line_items1 = items_builder.AcquireLogicalLineItems();
    items_builder.AssociateLogicalLineItems(line_items1, *line_fragment1);
    items_builder.AddLine(*line_fragment1, LogicalOffset());
    NGLogicalLineItems* line_items2 = items_builder.AcquireLogicalLineItems();
    items_builder.AssociateLogicalLineItems(line_items2, *line_fragment2);
    items_builder.AddLine(*line_fragment2, LogicalOffset());

    // In this case, we should reuse one |NGLogicalLineItems| instance.
    EXPECT_EQ(line_items1, line_items2);

    const auto& items = items_builder.Items(PhysicalSize());
    EXPECT_EQ(items.size(), 2u);
    EXPECT_EQ(items[0].item->LineBoxFragment(), line_fragment1);
    EXPECT_EQ(items[1].item->LineBoxFragment(), line_fragment2);
  }
  {
    // Custom layout produces all line boxes first without adding them to the
    // container box. Then runs worklet, and add line boxes to the container
    // box.
    NGFragmentItemsBuilder items_builder(
        inline_node, {WritingMode::kHorizontalTb, TextDirection::kLtr});
    NGLogicalLineItems* line_items1 = items_builder.AcquireLogicalLineItems();
    items_builder.AssociateLogicalLineItems(line_items1, *line_fragment1);
    NGLogicalLineItems* line_items2 = items_builder.AcquireLogicalLineItems();
    items_builder.AssociateLogicalLineItems(line_items2, *line_fragment2);

    // Because |AcquireLogicalLineItems| without |AddLine|, new instances should
    // be allocated for line 2.
    EXPECT_NE(line_items1, line_items2);

    items_builder.AddLine(*line_fragment1, LogicalOffset());
    items_builder.AddLine(*line_fragment2, LogicalOffset());
    const auto& items = items_builder.Items(PhysicalSize());
    EXPECT_EQ(items.size(), 2u);
    EXPECT_EQ(items[0].item->LineBoxFragment(), line_fragment1);
    EXPECT_EQ(items[1].item->LineBoxFragment(), line_fragment2);
  }
  {
    // Custom layout can reorder line boxes. In this test, line boxes are added
    // to the container box in the reverse order.
    NGFragmentItemsBuilder items_builder(
        inline_node, {WritingMode::kHorizontalTb, TextDirection::kLtr});
    NGLogicalLineItems* line_items1 = items_builder.AcquireLogicalLineItems();
    items_builder.AssociateLogicalLineItems(line_items1, *line_fragment1);
    NGLogicalLineItems* line_items2 = items_builder.AcquireLogicalLineItems();
    items_builder.AssociateLogicalLineItems(line_items2, *line_fragment2);

    // Because |AcquireLogicalLineItems| without |AddLine|, new instances should
    // be allocated for line 2.
    EXPECT_NE(line_items1, line_items2);

    // Add lines in the reverse order.
    items_builder.AddLine(*line_fragment2, LogicalOffset());
    items_builder.AddLine(*line_fragment1, LogicalOffset());
    const auto& items = items_builder.Items(PhysicalSize());
    EXPECT_EQ(items.size(), 2u);
    EXPECT_EQ(items[0].item->LineBoxFragment(), line_fragment2);
    EXPECT_EQ(items[1].item->LineBoxFragment(), line_fragment1);
  }
  {
    // Custom layout may not add all line boxes.
    NGFragmentItemsBuilder items_builder(
        inline_node, {WritingMode::kHorizontalTb, TextDirection::kLtr});
    NGLogicalLineItems* line_items1 = items_builder.AcquireLogicalLineItems();
    items_builder.AssociateLogicalLineItems(line_items1, *line_fragment1);
    NGLogicalLineItems* line_items2 = items_builder.AcquireLogicalLineItems();
    items_builder.AssociateLogicalLineItems(line_items2, *line_fragment2);

    // Because |AcquireLogicalLineItems| without |AddLine|, new instances should
    // be allocated for line 2.
    EXPECT_NE(line_items1, line_items2);

    // Add line2, but not line1.
    items_builder.AddLine(*line_fragment2, LogicalOffset());
    const auto& items = items_builder.Items(PhysicalSize());
    EXPECT_EQ(items.size(), 1u);
    EXPECT_EQ(items[0].item->LineBoxFragment(), line_fragment2);
  }
}

}  // namespace blink
