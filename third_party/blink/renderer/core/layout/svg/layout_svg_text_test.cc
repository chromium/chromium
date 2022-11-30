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

  const auto& svg = *GetDocument().getElementById("svg");
  const auto& text = *GetDocument().getElementById("text")->firstChild();

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

  GetDocument().getElementById("tspan1")->setAttribute(
      svg_names::kVectorEffectAttr, "non-scaling-stroke");
  GetDocument().getElementById("text2")->removeAttribute(
      svg_names::kVectorEffectAttr);
  GetDocument().getElementById("tspan3")->removeAttribute(
      svg_names::kVectorEffectAttr);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(text1->TransformAffectsVectorEffect());
  EXPECT_FALSE(text2->TransformAffectsVectorEffect());
  EXPECT_FALSE(text3->TransformAffectsVectorEffect());
}

}  // namespace blink
