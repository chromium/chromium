// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_shape.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"

namespace blink {

class LayoutSVGRootTest : public RenderingTest {
  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
  }
};

TEST_F(LayoutSVGRootTest, VisualRectMappingWithoutViewportClipWithBorder) {
  SetBodyInnerHTML(R"HTML(
    <svg id='root' style='border: 10px solid red; width: 200px; height:
    100px; overflow: visible' viewBox='0 0 200 100'>
       <rect id='rect' x='80' y='80' width='100' height='100'/>
    </svg>
  )HTML");

  const LayoutSVGRoot& root =
      *ToLayoutSVGRoot(GetLayoutObjectByElementId("root"));
  const LayoutSVGShape& svg_rect =
      *ToLayoutSVGShape(GetLayoutObjectByElementId("rect"));

  auto rect = SVGLayoutSupport::VisualRectInAncestorSpace(svg_rect, root);
  // (80, 80, 100, 100) added by root's content rect offset from border rect,
  // not clipped.
  EXPECT_EQ(PhysicalRect(90, 90, 100, 100), rect);

  auto root_visual_rect =
      static_cast<const LayoutObject&>(root).LocalVisualRect();
  // SVG root's local overflow does not include overflow from descendants.
  EXPECT_EQ(PhysicalRect(0, 0, 220, 120), root_visual_rect);

  EXPECT_TRUE(root.MapToVisualRectInAncestorSpace(&root, root_visual_rect));
  EXPECT_EQ(PhysicalRect(0, 0, 220, 120), root_visual_rect);
}

TEST_F(LayoutSVGRootTest, VisualOverflowExpandsLayer) {
  SetBodyInnerHTML(R"HTML(
    <svg id='root' style='width: 100px; will-change: transform; height:
    100px; overflow: visible; position: absolute;'>
       <rect id='rect' x='0' y='0' width='100' height='100'/>
    </svg>
  )HTML");

  const LayoutSVGRoot& root =
      *ToLayoutSVGRoot(GetLayoutObjectByElementId("root"));
  auto* paint_layer = root.Layer();
  ASSERT_TRUE(paint_layer);
  auto* graphics_layer = paint_layer->GraphicsLayerBacking(&root);
  ASSERT_TRUE(graphics_layer);
  EXPECT_EQ(graphics_layer->Size(), gfx::Size(100, 100));

  GetDocument().getElementById("rect")->setAttribute("height", "200");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(graphics_layer->Size(), gfx::Size(100, 200));
}

TEST_F(LayoutSVGRootTest, VisualRectMappingWithViewportClipAndBorder) {
  SetBodyInnerHTML(R"HTML(
    <svg id='root' style='border: 10px solid red; width: 200px; height:
    100px; overflow: hidden' viewBox='0 0 200 100'>
       <rect id='rect' x='80' y='80' width='100' height='100'/>
    </svg>
  )HTML");

  const LayoutSVGRoot& root =
      *ToLayoutSVGRoot(GetLayoutObjectByElementId("root"));
  const LayoutSVGShape& svg_rect =
      *ToLayoutSVGShape(GetLayoutObjectByElementId("rect"));

  auto rect = SVGLayoutSupport::VisualRectInAncestorSpace(svg_rect, root);
  EXPECT_EQ(PhysicalRect(90, 90, 100, 20), rect);

  auto root_visual_rect =
      static_cast<const LayoutObject&>(root).LocalVisualRect();
  // SVG root with overflow:hidden doesn't include overflow from children, just
  // border box rect.
  EXPECT_EQ(PhysicalRect(0, 0, 220, 120), root_visual_rect);

  EXPECT_TRUE(root.MapToVisualRectInAncestorSpace(&root, root_visual_rect));
  // LayoutSVGRoot should not apply overflow clip on its own rect.
  EXPECT_EQ(PhysicalRect(0, 0, 220, 120), root_visual_rect);
}

TEST_F(LayoutSVGRootTest, RectBasedHitTestPartialOverlap) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0 }</style>
    <svg id='svg' style='width: 300px; height: 300px; position: relative;
        top: 200px; left: 200px;'>
    </svg>
  )HTML");

  const auto& svg = *GetDocument().getElementById("svg");
  const auto& body = *GetDocument().body();

  // This is the center of the rect-based hit test below.
  EXPECT_EQ(body, *HitTest(150, 150));

  EXPECT_EQ(svg, *HitTest(200, 200));

  // The center of this rect does not overlap the SVG element, but the
  // rect itself does.
  auto results = RectBasedHitTest(PhysicalRect(0, 0, 300, 300));
  int count = 0;
  EXPECT_EQ(2u, results.size());
  for (auto result : results) {
    Node* node = result.Get();
    if (node == svg || node == body)
      count++;
  }
  EXPECT_EQ(2, count);
}

class CompositeSVGLayoutSVGRootTest : public PaintTestConfigurations,
                                      public LayoutSVGRootTest,
                                      private ScopedCompositeSVGForTest {
 public:
  CompositeSVGLayoutSVGRootTest() : ScopedCompositeSVGForTest(true) {}
};

INSTANTIATE_PAINT_TEST_SUITE_P(CompositeSVGLayoutSVGRootTest);

// A PaintLayer is needed for the purposes of creating a GraphicsLayer to limit
// CompositeSVG to SVG subtrees. This PaintLayer will not be needed with
// CompositeAfterPaint. If compositing is needed for descendants, the paint
// layer should be self-painting. Otherwise, it should be non-self-painting.
TEST_P(CompositeSVGLayoutSVGRootTest, PaintLayerType) {
  SetBodyInnerHTML(R"HTML(
    <svg id="root" style="width: 200px; height: 200px;">
      <rect id="rect" width="100" height="100" fill="green"/>
    </svg>
  )HTML");

  const LayoutSVGRoot& root =
      *ToLayoutSVGRoot(GetLayoutObjectByElementId("root"));
  ASSERT_TRUE(root.Layer());
  EXPECT_FALSE(root.Layer()->IsSelfPaintingLayer());

  GetDocument().getElementById("rect")->setAttribute("style",
                                                     "will-change: transform");
  UpdateAllLifecyclePhasesForTest();
  ASSERT_TRUE(root.Layer());
  EXPECT_TRUE(root.Layer()->IsSelfPaintingLayer());

  GetDocument().getElementById("rect")->removeAttribute("style");
  UpdateAllLifecyclePhasesForTest();
  ASSERT_TRUE(root.Layer());
  EXPECT_FALSE(root.Layer()->IsSelfPaintingLayer());
}

TEST_P(CompositeSVGLayoutSVGRootTest, HasDescendantCompositingReasons) {
  SetBodyInnerHTML(R"HTML(
    <svg id="root" style="width: 200px; height: 200px;">
      <rect id="rect" width="100" height="100" fill="green"/>
      <text id="text" x="10" y="30">
        text
        <tspan id="tspan">tspan</tspan>
      </text>
    </svg>
  )HTML");

  const LayoutSVGRoot& root =
      *ToLayoutSVGRoot(GetLayoutObjectByElementId("root"));
  EXPECT_FALSE(root.HasDescendantCompositingReasons());

  GetDocument().getElementById("rect")->setAttribute("style",
                                                     "will-change: transform");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(root.HasDescendantCompositingReasons());
  GetDocument().getElementById("rect")->removeAttribute("style");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(root.HasDescendantCompositingReasons());

  GetDocument().getElementById("text")->setAttribute("style",
                                                     "will-change: transform");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(root.HasDescendantCompositingReasons());
  GetDocument().getElementById("text")->removeAttribute("style");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(root.HasDescendantCompositingReasons());

  GetDocument().getElementById("tspan")->setAttribute("style",
                                                      "will-change: transform");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(root.HasDescendantCompositingReasons());
  GetDocument().getElementById("tspan")->removeAttribute("style");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(root.HasDescendantCompositingReasons());
}

}  // namespace blink
