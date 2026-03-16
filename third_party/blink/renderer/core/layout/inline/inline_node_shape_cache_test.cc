// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_spacing.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class InlineNodeForTest : public InlineNode {
 public:
  using InlineNode::InlineNode;

  const InlineItems& Items() { return Data().items; }
  bool IsNGShapeCacheAllowed() const {
    const String& text_content = Data().text_content;
    ShapeResultSpacing spacing(text_content, false);
    return InlineNode::IsNGShapeCacheAllowed(text_content, nullptr,
                                             Data().items, spacing);
  }
  void CollectInlines() { InlineNode::CollectInlines(MutableData()); }
};

class InlineNodeShapeCacheTest : public RenderingTest,
                                 private ScopedExtendedShapeCacheForTest {
 public:
  InlineNodeShapeCacheTest() : ScopedExtendedShapeCacheForTest(true) {}

 protected:
  static InlineNodeForTest CreateInlineNode(
      LayoutBlockFlow* layout_block_flow) {
    InlineNodeForTest node(layout_block_flow);
    node.InvalidatePrepareLayoutForTest();
    node.CollectInlines();
    return node;
  }

  static const ShapeResult* FirstShapeResult(
      const LayoutBlockFlow* block_flow) {
    return To<LayoutText>(block_flow->FirstChild())
        ->InlineItems()
        .front()
        .TextShapeResult();
  }
  static const ShapeResult* LastShapeResult(const LayoutBlockFlow* block_flow) {
    return To<LayoutText>(block_flow->LastChild())
        ->InlineItems()
        .back()
        .TextShapeResult();
  }
};

TEST_F(InlineNodeShapeCacheTest, ShapeCacheMultipleItems) {
  SetBodyInnerHTML("<div id=t>abc<span>def</span>ghi</div>");
  InlineNodeForTest node = CreateInlineNode(GetLayoutBlockFlowByElementId("t"));
  EXPECT_EQ(5u, node.Items().size());
  EXPECT_FALSE(node.IsNGShapeCacheAllowed());
}

TEST_F(InlineNodeShapeCacheTest, ShapeCacheSpacingRequired) {
  SetBodyInnerHTML(
      "<style>div { letter-spacing: 5px; }</style>"
      "<div id=t>abc</div>");
  InlineNodeForTest node = CreateInlineNode(GetLayoutBlockFlowByElementId("t"));
  EXPECT_FALSE(node.IsNGShapeCacheAllowed());
}

TEST_F(InlineNodeShapeCacheTest, ShapeCacheNewLine) {
  SetBodyInnerHTML("<div id=t style='white-space: pre'>abc\ndef</div>");
  InlineNodeForTest node = CreateInlineNode(GetLayoutBlockFlowByElementId("t"));
  EXPECT_TRUE(node.IsNGShapeCacheAllowed());
}

TEST_F(InlineNodeShapeCacheTest, ShapeCacheBreakLine) {
  SetBodyInnerHTML("<div id=t style='white-space: pre'>abc<br>def</div>");
  InlineNodeForTest node = CreateInlineNode(GetLayoutBlockFlowByElementId("t"));
  EXPECT_TRUE(node.IsNGShapeCacheAllowed());
}

TEST_F(InlineNodeShapeCacheTest, ShapeCacheBreakWord) {
  SetBodyInnerHTML("<div id=t style='white-space: pre'>abc<wbr>def</div>");
  InlineNodeForTest node = CreateInlineNode(GetLayoutBlockFlowByElementId("t"));
  EXPECT_TRUE(node.IsNGShapeCacheAllowed());
}

TEST_F(InlineNodeShapeCacheTest, ShapeResultPointerEqualNewLine) {
  SetBodyInnerHTML(
      "<div id=target1 style='white-space: pre'>abc\ndefg</div>"
      "<div id=target2 style='white-space: pre'>abc<br>defg</div>");

  const LayoutBlockFlow* target1 = GetLayoutBlockFlowByElementId("target1");
  const LayoutBlockFlow* target2 = GetLayoutBlockFlowByElementId("target2");

  EXPECT_EQ(FirstShapeResult(target1)->NumCharacters(), 3u);
  EXPECT_EQ(LastShapeResult(target1)->NumCharacters(), 4u);
  EXPECT_EQ(FirstShapeResult(target1), FirstShapeResult(target2));
  EXPECT_EQ(LastShapeResult(target1), LastShapeResult(target2));
}

}  // namespace blink
