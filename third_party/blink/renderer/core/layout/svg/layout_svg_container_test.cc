// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

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
  auto* rect_element = GetElementById("rect");
  auto* rect = rect_element->GetLayoutObject();
  auto* text_element = GetElementById("text");
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
                             AtomicString("non-scaling-stroke"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(g->TransformAffectsVectorEffect());
  EXPECT_TRUE(rect->TransformAffectsVectorEffect());
  EXPECT_FALSE(text->TransformAffectsVectorEffect());

  text_element->setAttribute(svg_names::kXAttr, AtomicString("20"));
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
  auto* rect_element = GetElementById("rect");
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

TEST_F(LayoutSVGContainerTest, PatternWithContentVisibility) {
  SetBodyInnerHTML(R"HTML(
    <svg viewBox="0 0 230 100" xmlns="http://www.w3.org/2000/svg">
      <defs>
        <pattern id="pattern" viewBox="0,0,10,10" width="10%" height="10%">
          <polygon id="polygon" points="0,0 2,5 0,10 5,8 10,10 8,5 10,0 5,2"/>
        </pattern>
      </defs>

      <circle id="circle" cx="50"  cy="50" r="50" fill="url(#pattern)"/>
    </svg>
  )HTML");

  auto* pattern = GetElementById("pattern");
  auto* polygon = GetElementById("polygon");

  pattern->setAttribute(
      svg_names::kStyleAttr,
      AtomicString("contain: strict; content-visibility: hidden"));

  UpdateAllLifecyclePhasesForTest();

  polygon->setAttribute(svg_names::kPointsAttr, AtomicString("0,0 2,5 0,10"));

  // This shouldn't cause a DCHECK, even though the pattern needs layout because
  // it's under a content-visibility: hidden subtree.
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(pattern->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(pattern->GetLayoutObject()->SelfNeedsFullLayout());
}

TEST_F(LayoutSVGContainerTest, BoundingBoxesWith3DTransformedChild) {
  ScopedSvgCss3dTransformsForTest svg_css_3d_transforms(true);
  SetBodyInnerHTML(R"HTML(
    <svg width="200" height="200">
      <g id="g">
        <rect x="50" y="50" width="100" height="100"
              style="transform: perspective(300px) translateZ(-100px);
                     transform-origin: 100px 100px"/>
      </g>
    </svg>
  )HTML");

  // The rect renders scaled by 300 / (300 + 100) = 0.75 about the transform
  // origin (100, 100).
  auto* g = GetLayoutObjectByElementId("g");
  EXPECT_EQ(gfx::RectF(62.5, 62.5, 75, 75), g->ObjectBoundingBox());
  EXPECT_EQ(gfx::RectF(62.5, 62.5, 75, 75), g->DecoratedBoundingBox());
}

}  // namespace blink
