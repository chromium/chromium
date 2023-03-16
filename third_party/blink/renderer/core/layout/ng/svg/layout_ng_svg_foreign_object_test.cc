// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class LayoutNGSVGForeignObjectTest : public RenderingTest {
 public:
  LayoutNGSVGForeignObjectTest() = default;
};

// crbug.com/1372886
TEST_F(LayoutNGSVGForeignObjectTest, SubtreeLayoutCrash) {
  SetBodyInnerHTML(R"HTML(
<svg style="position:absolute;">
  <svg></svg>
  <foreignObject>
    <div id="in-foreign"></div>
  </foreignObject>
</svg>
<div></div>
<span></span>
<div id="sibling-div"></div>
<svg><pattern id="pat"></pattern>
</svg>)HTML");
  UpdateAllLifecyclePhasesForTest();
  GetElementById("in-foreign")->setAttribute("style", "display: inline-block");
  UpdateAllLifecyclePhasesForTest();
  GetElementById("pat")->setAttribute("viewBox", "972 815 1088 675");
  UpdateAllLifecyclePhasesForTest();
  GetElementById("sibling-div")->setAttribute("style", "display: none");
  UpdateAllLifecyclePhasesForTest();
  // Pass if no crashes.
}

TEST_F(LayoutNGSVGForeignObjectTest, ZoomChangesInvalidatePaintProperties) {
  SetBodyInnerHTML(R"HTML(
    <style> body { margin: 0; } </style>
    <svg id="svg" xmlns="http://www.w3.org/2000/svg" width="100px"
        height="100px" viewBox="-1 -1 100 100">
      <foreignObject id="foreign" xmlns="http://www.w3.org/2000/svg"
          width="100px" height="100px" style="overflow: visible;" />
    </svg>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  // Initially, the svg replaced contents transform should have no scale, and
  // there should be no foreign object transform paint property.
  LayoutObject* svg = GetLayoutObjectByElementId("svg");
  const TransformPaintPropertyNode* svg_replaced_contents =
      svg->FirstFragment().PaintProperties()->ReplacedContentTransform();
  EXPECT_EQ(gfx::Vector2dF(1, 1), svg_replaced_contents->Get2dTranslation());
  LayoutObject* foreign = GetLayoutObjectByElementId("foreign");
  EXPECT_FALSE(foreign->FirstFragment().PaintProperties());

  // Update zoom and ensure the foreign object is marked as needing a paint
  // property update prior to updating paint properties.
  GetDocument().documentElement()->setAttribute("style", "zoom: 2");
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(foreign->NeedsPaintPropertyUpdate());

  UpdateAllLifecyclePhasesForTest();

  // The svg replaced contents transform should contain the zoom, but the
  // foreign object's transform should unapply it.
  EXPECT_EQ(gfx::Vector2dF(2, 2), svg_replaced_contents->Matrix().To2dScale());
  const TransformPaintPropertyNode* foreign_transform =
      foreign->FirstFragment().PaintProperties()->Transform();
  EXPECT_EQ(gfx::Vector2dF(0.5, 0.5), foreign_transform->Matrix().To2dScale());
}

}  // namespace blink
