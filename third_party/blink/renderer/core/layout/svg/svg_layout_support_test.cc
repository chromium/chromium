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

}  // namespace blink
