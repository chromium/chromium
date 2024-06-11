// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/block_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

class InlineLayoutTest : public SimTest {
 public:
  ConstraintSpace ConstraintSpaceForElement(LayoutBlockFlow* block_flow) {
    ConstraintSpaceBuilder builder(block_flow->Style()->GetWritingMode(),
                                   block_flow->Style()->GetWritingDirection(),
                                   /* is_new_fc */ false);
    builder.SetAvailableSize(LogicalSize(LayoutUnit(), LayoutUnit()));
    builder.SetPercentageResolutionSize(
        LogicalSize(LayoutUnit(), LayoutUnit()));
    return builder.ToConstraintSpace();
  }
};

TEST_F(InlineLayoutTest, BlockWithSingleTextNode) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(
      "<div id=\"target\">Hello <strong>World</strong>!</div>");

  Compositor().BeginFrame();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());

  Element* target = GetDocument().getElementById(AtomicString("target"));
  auto* block_flow = To<LayoutBlockFlow>(target->GetLayoutObject());
  ConstraintSpace constraint_space = ConstraintSpaceForElement(block_flow);
  BlockNode node(block_flow);

  FragmentGeometry fragment_geometry = CalculateInitialFragmentGeometry(
      constraint_space, node, /* break_token */ nullptr);
  const LayoutResult* result =
      BlockLayoutAlgorithm({node, fragment_geometry, constraint_space})
          .Layout();
  EXPECT_TRUE(result);

  String expected_text("Hello World!");
  auto first_child = To<InlineNode>(node.FirstChild());
  EXPECT_EQ(expected_text,
            StringView(first_child.ItemsData(false).text_content, 0, 12));
}

TEST_F(InlineLayoutTest, BlockWithTextAndAtomicInline) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete("<div id=\"target\">Hello <img>.</div>");

  Compositor().BeginFrame();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());

  Element* target = GetDocument().getElementById(AtomicString("target"));
  auto* block_flow = To<LayoutBlockFlow>(target->GetLayoutObject());
  ConstraintSpace constraint_space = ConstraintSpaceForElement(block_flow);
  BlockNode node(block_flow);

  FragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(constraint_space, node,
                                       /* break_token */ nullptr);
  const LayoutResult* result =
      BlockLayoutAlgorithm({node, fragment_geometry, constraint_space})
          .Layout();
  EXPECT_TRUE(result);

  StringBuilder expected_text;
  expected_text.Append("Hello ");
  expected_text.Append(kObjectReplacementCharacter);
  expected_text.Append('.');
  auto first_child = To<InlineNode>(node.FirstChild());
  EXPECT_EQ(expected_text.ToString(),
            StringView(first_child.ItemsData(false).text_content, 0, 8));
}

}  // namespace blink
