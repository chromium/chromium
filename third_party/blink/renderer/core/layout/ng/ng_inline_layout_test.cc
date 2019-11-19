// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

class NGInlineLayoutTest : public SimTest {
 public:
  NGConstraintSpace ConstraintSpaceForElement(LayoutBlockFlow* block_flow) {
    NGConstraintSpaceBuilder builder(block_flow->Style()->GetWritingMode(),
                                     block_flow->Style()->GetWritingMode(),
                                     /* is_new_fc */ false);
    builder.SetAvailableSize(LogicalSize(LayoutUnit(), LayoutUnit()));
    builder.SetPercentageResolutionSize(
        LogicalSize(LayoutUnit(), LayoutUnit()));
    builder.SetTextDirection(block_flow->Style()->Direction());
    return builder.ToConstraintSpace();
  }
};

TEST_F(NGInlineLayoutTest, BlockWithSingleTextNode) {
  ScopedLayoutNGForTest layout_ng(true);

  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(
      "<div id=\"target\">Hello <strong>World</strong>!</div>");

  Compositor().BeginFrame();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());

  Element* target = GetDocument().getElementById("target");
  auto* block_flow = To<LayoutBlockFlow>(target->GetLayoutObject());
  NGConstraintSpace constraint_space = ConstraintSpaceForElement(block_flow);
  NGBlockNode node(block_flow);

  NGFragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(constraint_space, node);
  scoped_refptr<const NGLayoutResult> result =
      NGBlockLayoutAlgorithm({node, fragment_geometry, constraint_space})
          .Layout();
  EXPECT_TRUE(result);

  String expected_text("Hello World!");
  auto first_child = To<NGInlineNode>(node.FirstChild());
  EXPECT_EQ(expected_text,
            StringView(first_child.ItemsData(false).text_content, 0, 12));
}

TEST_F(NGInlineLayoutTest, BlockWithTextAndAtomicInline) {
  ScopedLayoutNGForTest layout_ng(true);

  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete("<div id=\"target\">Hello <img>.</div>");

  Compositor().BeginFrame();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());

  Element* target = GetDocument().getElementById("target");
  auto* block_flow = To<LayoutBlockFlow>(target->GetLayoutObject());
  NGConstraintSpace constraint_space = ConstraintSpaceForElement(block_flow);
  NGBlockNode node(block_flow);

  NGFragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(constraint_space, node);
  scoped_refptr<const NGLayoutResult> result =
      NGBlockLayoutAlgorithm({node, fragment_geometry, constraint_space})
          .Layout();
  EXPECT_TRUE(result);

  StringBuilder expected_text;
  expected_text.Append("Hello ");
  expected_text.Append(kObjectReplacementCharacter);
  expected_text.Append('.');
  auto first_child = To<NGInlineNode>(node.FirstChild());
  EXPECT_EQ(expected_text.ToString(),
            StringView(first_child.ItemsData(false).text_content, 0, 8));

  // Delete the line box tree to avoid leaks in the test.
  block_flow->DeleteLineBoxTree();
}

}  // namespace blink
