// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"

#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

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

}  // namespace blink
