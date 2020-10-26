// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

using LayoutSVGContainerTest = RenderingTest;

TEST_F(LayoutSVGContainerTest, TransformAffectsVectorEffect) {
  SetBodyInnerHTML(R"HTML(
    <svg id="svg" width="300" height="300">
      <g id="g">
        <rect id="rect" vector-effect="non-scaling-stroke"/>
        <text id="text" vector-effect="non-scaling-stroke">Test</text>
      </g>
    </svg>
  )HTML");

  auto* svg = GetLayoutObjectByElementId("svg");
  auto* g = GetLayoutObjectByElementId("g");
  auto* rect_element = GetDocument().getElementById("rect");
  auto* rect = rect_element->GetLayoutObject();
  auto* text_element = GetDocument().getElementById("text");
  auto* text = text_element->GetLayoutObject();

  EXPECT_FALSE(svg->TransformAffectsVectorEffect());

  EXPECT_TRUE(g->TransformAffectsVectorEffect());
  EXPECT_TRUE(rect->TransformAffectsVectorEffect());
  EXPECT_TRUE(text->TransformAffectsVectorEffect());

  rect_element->removeAttribute(svg_names::kVectorEffectAttr);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(g->TransformAffectsVectorEffect());
  EXPECT_FALSE(rect->TransformAffectsVectorEffect());
  EXPECT_TRUE(text->TransformAffectsVectorEffect());

  text_element->removeAttribute(svg_names::kVectorEffectAttr);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(g->TransformAffectsVectorEffect());
  EXPECT_FALSE(rect->TransformAffectsVectorEffect());
  EXPECT_FALSE(text->TransformAffectsVectorEffect());

  rect_element->setAttribute(svg_names::kVectorEffectAttr,
                             "non-scaling-stroke");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(g->TransformAffectsVectorEffect());
  EXPECT_TRUE(rect->TransformAffectsVectorEffect());
  EXPECT_FALSE(text->TransformAffectsVectorEffect());

  text_element->setAttribute(svg_names::kXAttr, "20");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(g->TransformAffectsVectorEffect());
  EXPECT_TRUE(rect->TransformAffectsVectorEffect());
  EXPECT_FALSE(text->TransformAffectsVectorEffect());

  EXPECT_FALSE(svg->TransformAffectsVectorEffect());
}

TEST_F(LayoutSVGContainerTest, TransformAffectsVectorEffectNestedSVG) {
  SetBodyInnerHTML(R"HTML(
    <svg id="svg" width="300" height="300">
      <g id="g">
        <svg id="nested-svg">
          <rect id="rect" vector-effect="non-scaling-stroke"/>
        </svg>
      </g>
    </svg>
  )HTML");

  auto* svg = GetLayoutObjectByElementId("svg");
  auto* g = GetLayoutObjectByElementId("g");
  auto* nested_svg = GetLayoutObjectByElementId("nested-svg");
  auto* rect_element = GetDocument().getElementById("rect");
  auto* rect = rect_element->GetLayoutObject();

  EXPECT_FALSE(svg->TransformAffectsVectorEffect());
  EXPECT_TRUE(g->TransformAffectsVectorEffect());
  EXPECT_TRUE(nested_svg->TransformAffectsVectorEffect());
  EXPECT_TRUE(rect->TransformAffectsVectorEffect());

  rect_element->removeAttribute(svg_names::kVectorEffectAttr);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(svg->TransformAffectsVectorEffect());
  EXPECT_FALSE(g->TransformAffectsVectorEffect());
  EXPECT_FALSE(nested_svg->TransformAffectsVectorEffect());
  EXPECT_FALSE(rect->TransformAffectsVectorEffect());
}

TEST_F(LayoutSVGContainerTest,
       TransformAffectsVectorEffectHiddenContainerAndUse) {
  SetBodyInnerHTML(R"HTML(
    <svg id="svg" width="300" height="300">
      <g id="g0">
        <defs>
          <rect id="rect" vector-effect="non-scaling-stroke"/>
        </defs>
      </g>
      <g id="g1">
        <use id="use" href="#rect"/>
      </g>
    </svg>
  )HTML");

  EXPECT_FALSE(
      GetLayoutObjectByElementId("svg")->TransformAffectsVectorEffect());
  EXPECT_FALSE(
      GetLayoutObjectByElementId("g0")->TransformAffectsVectorEffect());
  EXPECT_TRUE(
      GetLayoutObjectByElementId("rect")->TransformAffectsVectorEffect());
  EXPECT_TRUE(GetLayoutObjectByElementId("g1")->TransformAffectsVectorEffect());
  auto* use = GetLayoutObjectByElementId("use");
  EXPECT_TRUE(use->TransformAffectsVectorEffect());
  EXPECT_TRUE(use->SlowFirstChild()->TransformAffectsVectorEffect());
}

}  // namespace blink
