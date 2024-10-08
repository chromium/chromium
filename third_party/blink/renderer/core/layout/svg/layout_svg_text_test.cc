// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

using LayoutSVGTextTest = RenderingTest;

TEST_F(LayoutSVGTextTest, RectBasedHitTest) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0 }</style>
    <svg id=svg width="300" height="300">
      <a id="link">
        <text id="text" y="20">text</text>
      </a>
    </svg>
  )HTML");

  const auto& svg = *GetElementById("svg");
  const auto& text = *GetElementById("text")->firstChild();

  // Rect based hit testing
  auto results = RectBasedHitTest(PhysicalRect(0, 0, 300, 300));
  int count = 0;
  EXPECT_EQ(2u, results.size());
  for (auto result : results) {
    Node* node = result.Get();
    if (node == svg || node == text)
      count++;
  }
  EXPECT_EQ(2, count);
}

TEST_F(LayoutSVGTextTest, RectBasedHitTest_RotatedText) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0 }</style>
    <svg id="svg" width="300" height="300">
      <path id="path" d="M50,80L150,180"/>
      <text font-size="100" font-family="Ahem">
        <textPath href="#path">MM</textPath>
      </text>
    </svg>
  )HTML");

  auto* svg = GetElementById("svg");

  {
    // Non-intersecting.
    auto results = RectBasedHitTest(PhysicalRect(25, 10, 10, 100));
    EXPECT_EQ(1u, results.size());
    EXPECT_TRUE(results.Contains(svg));
  }
  {
    // Intersects the axis-aligned bounding box of the text but not the actual
    // (local) bounding box.
    auto results = RectBasedHitTest(PhysicalRect(12, 12, 50, 50));
    EXPECT_EQ(1u, results.size());
    EXPECT_TRUE(results.Contains(svg));
  }
}

TEST_F(LayoutSVGTextTest, TransformAffectsVectorEffect) {
  SetBodyInnerHTML(R"HTML(
    <svg width="300" height="300">
      <text id="text1">A<tspan id="tspan1">B</tspan>C</text>
      <text id="text2" vector-effect="non-scaling-stroke">D</text>
      <text id="text3">E
        <tspan id="tspan3" vector-effect="non-scaling-stroke">F</tspan>G
      </text>
    </svg>
  )HTML");

  auto* text1 = GetLayoutObjectByElementId("text1");
  auto* text2 = GetLayoutObjectByElementId("text2");
  auto* text3 = GetLayoutObjectByElementId("text3");
  EXPECT_FALSE(text1->TransformAffectsVectorEffect());
  EXPECT_TRUE(text2->TransformAffectsVectorEffect());
  EXPECT_TRUE(text3->TransformAffectsVectorEffect());

  GetElementById("tspan1")->setAttribute(svg_names::kVectorEffectAttr,
                                         AtomicString("non-scaling-stroke"));
  GetElementById("text2")->removeAttribute(svg_names::kVectorEffectAttr);
  GetElementById("tspan3")->removeAttribute(svg_names::kVectorEffectAttr);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(text1->TransformAffectsVectorEffect());
  EXPECT_FALSE(text2->TransformAffectsVectorEffect());
  EXPECT_FALSE(text3->TransformAffectsVectorEffect());
}

// DevTools element overlay uses AbsoluteQuads().
TEST_F(LayoutSVGTextTest, AbsoluteQuads) {
  SetBodyInnerHTML(R"HTML(
<style>
body { margin:0; padding: 0; }
</style>
<svg xmlns="http://www.w3.org/2000/svg" width="400" height="400">
  <text id="t" font-size="16" x="7" textLength="300">Foo</text>
</svg>)HTML");
  UpdateAllLifecyclePhasesForTest();

  Vector<gfx::QuadF> quads;
  auto* object = GetLayoutObjectByElementId("t");
  object->AbsoluteQuads(quads, 0);
  EXPECT_EQ(1u, quads.size());
  gfx::RectF bounding = quads.back().BoundingBox();
  EXPECT_EQ(7.0f, bounding.x());
  EXPECT_EQ(307.0f, bounding.right());
}

TEST_F(LayoutSVGTextTest, ObjectBoundingBox) {
  SetBodyInnerHTML(R"HTML(
<html>
<body>
<svg xmlns="http://www.w3.org/2000/svg" width="100%" height="100%" viewBox="0 0 480 360">
<text text-anchor="middle" x="240" y="25" font-size="16" id="t">
qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq</text>
</svg>
</body><style>
* { scale: 4294967108 33 -0.297499; }
</style>)HTML");
  UpdateAllLifecyclePhasesForTest();

  gfx::RectF box = GetLayoutObjectByElementId("t")->ObjectBoundingBox();
  EXPECT_FALSE(std::isinf(box.origin().x()));
  EXPECT_FALSE(std::isinf(box.origin().y()));
  EXPECT_FALSE(std::isinf(box.width()));
  EXPECT_FALSE(std::isinf(box.height()));
}

// crbug.com/1285666
TEST_F(LayoutSVGTextTest, SubtreeLayout) {
  SetBodyInnerHTML(R"HTML(
<body>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 480 360">
<text x="240" y="25" font-size="16" id="t">foo</text>
<text x="240" y="50" font-size="16" id="t2">bar</text>
</svg>
</body>)HTML");
  UpdateAllLifecyclePhasesForTest();
  LocalFrameView* frame_view = GetFrame().View();
  LayoutView& view = GetLayoutView();
  ASSERT_FALSE(view.NeedsLayout());

  GetElementById("t")->setAttribute(svg_names::kTransformAttr,
                                    AtomicString("scale(0.5)"));
  GetDocument().UpdateStyleAndLayoutTreeForThisDocument();
  EXPECT_TRUE(frame_view->IsSubtreeLayout());

  ;
  uint32_t pre_layout_count = frame_view->BlockLayoutCountForTesting();
  UpdateAllLifecyclePhasesForTest();
  // Only the <text> and its parent <svg> should be laid out again.
  EXPECT_EQ(2u, frame_view->BlockLayoutCountForTesting() - pre_layout_count);
}

// crbug.com/1320615
TEST_F(LayoutSVGTextTest, WillBeRemovedFromTree) {
  SetHtmlInnerHTML(R"HTML(
<body>
<div id="to_be_skipped">
<div id="d">
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 480 360" id="svg">
<text id="t">foo</text>
</svg>
</div>
</div>
</body>)HTML");
  // The <text> is registered to #d, #to_be_skipped, body, ...
  UpdateAllLifecyclePhasesForTest();

  // #d's containing block will be the LayoutView.
  GetElementById("d")->setAttribute(html_names::kStyleAttr,
                                    AtomicString("position:absolute;"));
  UpdateAllLifecyclePhasesForTest();

  // The <text> should be unregistered from all of ancestors.
  GetElementById("svg")->remove();
  GetElementById("to_be_skipped")
      ->setAttribute(html_names::kStyleAttr,
                     AtomicString("transform:rotate(20deg)"));
  UpdateAllLifecyclePhasesForTest();
}

}  // namespace blink
