// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/fragment_item.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/core/layout/inline/fragment_items_builder.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node.h"
#include "third_party/blink/renderer/core/layout/inline/logical_line_item.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

using testing::ElementsAre;

namespace blink {

class FragmentItemsBuilderTest : public RenderingTest {};

TEST_F(FragmentItemsBuilderTest, MultipleLogicalLineItems) {
  SetBodyInnerHTML(R"HTML(
    <div id="container">
      1<br>
      2
    </div>
  )HTML");
  LayoutBlockFlow* container =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));

  // Get |PhysicalLineBoxFragment|s to use for testing.
  InlineCursor cursor(*container);
  cursor.MoveToFirstLine();
  const PhysicalLineBoxFragment* line_fragment1 =
      cursor.Current()->LineBoxFragment();
  cursor.MoveToNextLine();
  const PhysicalLineBoxFragment* line_fragment2 =
      cursor.Current()->LineBoxFragment();

  InlineNode inline_node(container);
  {
    // First test emulates what |BlockLayoutAlgorithm| does, which loops
    // following calls for each line:
    // 1. |AcquireLogicalLineItems|
    // 2. |AssociateLogicalLineItems|
    // 3. |AddLine|.
    FragmentItemsBuilder items_builder(
        inline_node, {WritingMode::kHorizontalTb, TextDirection::kLtr}, false);
    auto* line_container1 = items_builder.AcquireLogicalLineContainer();
    items_builder.AssociateLogicalLineContainer(line_container1,
                                                *line_fragment1);
    items_builder.AddLine(*line_fragment1, LogicalOffset());
    auto* line_container2 = items_builder.AcquireLogicalLineContainer();
    items_builder.AssociateLogicalLineContainer(line_container2,
                                                *line_fragment2);
    items_builder.AddLine(*line_fragment2, LogicalOffset());

    // In this case, we should reuse one |LogicalLineContainer| instance.
    EXPECT_EQ(line_container1, line_container2);

    const auto& items = items_builder.Items(PhysicalSize());
    EXPECT_EQ(items.size(), 2u);
    EXPECT_EQ(items[0].item->LineBoxFragment(), line_fragment1);
    EXPECT_EQ(items[1].item->LineBoxFragment(), line_fragment2);
  }
  {
    // Custom layout produces all line boxes first without adding them to the
    // container box. Then runs worklet, and add line boxes to the container
    // box.
    FragmentItemsBuilder items_builder(
        inline_node, {WritingMode::kHorizontalTb, TextDirection::kLtr}, false);
    auto* line_container1 = items_builder.AcquireLogicalLineContainer();
    items_builder.AssociateLogicalLineContainer(line_container1,
                                                *line_fragment1);
    auto* line_container2 = items_builder.AcquireLogicalLineContainer();
    items_builder.AssociateLogicalLineContainer(line_container2,
                                                *line_fragment2);

    // Because |AcquireLogicalLineItems| without |AddLine|, new instances should
    // be allocated for line 2.
    EXPECT_NE(line_container1, line_container2);

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
    FragmentItemsBuilder items_builder(
        inline_node, {WritingMode::kHorizontalTb, TextDirection::kLtr}, false);
    auto* line_container1 = items_builder.AcquireLogicalLineContainer();
    items_builder.AssociateLogicalLineContainer(line_container1,
                                                *line_fragment1);
    auto* line_container2 = items_builder.AcquireLogicalLineContainer();
    items_builder.AssociateLogicalLineContainer(line_container2,
                                                *line_fragment2);

    // Because |AcquireLogicalLineItems| without |AddLine|, new instances should
    // be allocated for line 2.
    EXPECT_NE(line_container1, line_container2);

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
    FragmentItemsBuilder items_builder(
        inline_node, {WritingMode::kHorizontalTb, TextDirection::kLtr}, false);
    auto* line_container1 = items_builder.AcquireLogicalLineContainer();
    items_builder.AssociateLogicalLineContainer(line_container1,
                                                *line_fragment1);
    auto* line_container2 = items_builder.AcquireLogicalLineContainer();
    items_builder.AssociateLogicalLineContainer(line_container2,
                                                *line_fragment2);

    // Because |AcquireLogicalLineItems| without |AddLine|, new instances should
    // be allocated for line 2.
    EXPECT_NE(line_container1, line_container2);

    // Add line2, but not line1.
    items_builder.AddLine(*line_fragment2, LogicalOffset());
    const auto& items = items_builder.Items(PhysicalSize());
    EXPECT_EQ(items.size(), 1u);
    EXPECT_EQ(items[0].item->LineBoxFragment(), line_fragment2);
  }
}

}  // namespace blink
