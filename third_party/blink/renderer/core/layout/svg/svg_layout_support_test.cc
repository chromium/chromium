// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"

#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class SVGLayoutSupportTest : public RenderingTest {};

TEST_F(SVGLayoutSupportTest, FindClosestLayoutSVGText) {
  SetBodyInnerHTML(R"HTML(
<svg xmlns="http://www.w3.org/2000/svg"
    viewBox="0 0 450 500" id="svg">
  <g id="testContent" stroke-width="0.01" font-size="15">
    <text  x="50%" y="10%" text-anchor="middle" id="t1">
      Heading</text>
    <g text-anchor="start" id="innerContent">
      <text x="10%" y="20%" id="t2">Paragraph 1</text>
      <text x="10%" y="24%" id="t3">Paragraph 2</text>
    </g>
  </g>
</svg>)HTML");
  UpdateAllLifecyclePhasesForTest();

  constexpr gfx::PointF kBelowT3(220, 250);
  LayoutObject* hit_text = SVGLayoutSupport::FindClosestLayoutSVGText(
      GetLayoutBoxByElementId("svg"), kBelowT3);
  EXPECT_EQ("t3", To<Element>(hit_text->GetNode())->GetIdAttribute());
}

TEST_F(SVGLayoutSupportTest, VisualRectInAncestorSpaceContainerWithFilter) {
  SetBodyInnerHTML(R"HTML(
    <svg id="svg" width="500" height="500">
      <defs>
        <filter id="shadow">
          <feDropShadow dx="10" dy="10" stdDeviation="0"/>
        </filter>
      </defs>
      <g id="container" filter="url(#shadow)">
        <rect x="0" y="0" width="10" height="10"/>
        <rect id="target" transform="translate(200, 200)" width="10" height="10"/>
      </g>
    </svg>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  LayoutObject* target = GetLayoutObjectByElementId("target");
  ASSERT_TRUE(target);

  PhysicalRect result_rect = VisualRectInDocument(*target);

  EXPECT_EQ(result_rect, PhysicalRect(8, 8, 220, 220));
}

class SVGLayoutSupport3DTransformTest : public SVGLayoutSupportTest {
 protected:
  ScopedSvgCss3dTransformsForTest svg_css_3d_transforms_{true};
};

TEST_F(SVGLayoutSupport3DTransformTest, VisualRectInDocumentWith3DTransform) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0; }</style>
    <svg width="200" height="200">
      <rect id="rect" x="50" y="50" width="100" height="100"
            style="transform: perspective(300px) translateZ(-100px);
                   transform-origin: 100px 100px"/>
    </svg>
  )HTML");

  // The rect renders scaled by 300 / (300 + 100) = 0.75 about the transform
  // origin (100, 100), covering 62.5..137.5 on both axes.
  EXPECT_EQ(PhysicalRect(62, 62, 76, 76),
            VisualRectInDocument(*GetLayoutObjectByElementId("rect")));
}

TEST_F(SVGLayoutSupport3DTransformTest,
       VisualRectInDocumentUnderSVGRootPerspective) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0; }</style>
    <svg width="200" height="200" style="perspective: 300px">
      <rect id="rect" x="50" y="50" width="100" height="100"
            style="transform: translateZ(-100px)"/>
    </svg>
  )HTML");

  // Scaled by 0.75 about the default perspective origin (100, 100).
  EXPECT_EQ(PhysicalRect(62, 62, 76, 76),
            VisualRectInDocument(*GetLayoutObjectByElementId("rect")));
}

TEST_F(SVGLayoutSupport3DTransformTest,
       AbsoluteBoundingBoxUnderSVGRootPerspective) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0; }</style>
    <svg width="200" height="200" style="perspective: 300px">
      <g style="fill: green; transform-style: preserve-3d">
        <rect id="rect" x="50" y="50" width="100" height="100"
              style="transform: translateZ(-100px)"/>
      </g>
    </svg>
  )HTML");

  // Scaled by 0.75 about the default perspective origin (100, 100). The
  // intermediate <g> preserves 3D, so the rect's transform is not flattened
  // before the root's perspective applies.
  EXPECT_EQ(gfx::Rect(62, 62, 76, 76),
            GetLayoutObjectByElementId("rect")->AbsoluteBoundingBoxRect());
}

}  // namespace blink
