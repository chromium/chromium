// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_image.h"

#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

using LayoutImageTest = RenderingTest;

TEST_F(LayoutImageTest, HitTestUnderTransform) {
  SetBodyInnerHTML(R"HTML(
    <div style='transform: translateX(50px)'>
      <img id=target style='width: 20px; height: 20px'/>
    </div>
  )HTML");

  const auto& target = *GetElementById("target");
  HitTestLocation location(PhysicalOffset(60, 10));
  HitTestResult result(
      HitTestRequest(HitTestRequest::kReadOnly | HitTestRequest::kActive |
                     HitTestRequest::kAllowChildFrameContent),
      location);
  GetLayoutView().HitTest(location, result);
  EXPECT_EQ(PhysicalOffset(60, 10), result.PointInInnerNodeFrame());
  EXPECT_EQ(target, result.InnerNode());
}

TEST_F(LayoutImageTest, NeedsVisualOverflowRecalc) {
  SetBodyInnerHTML(R"HTML(
    <div id="target" style="position: relative; width: 100px;">
      <img id="img" style="position: absolute; width: 100%;">
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  const auto* img_layer = GetLayoutBoxByElementId("img")->Layer();
  GetElementById("target")->SetInlineStyleProperty(CSSPropertyID::kWidth, "200px");
  EXPECT_FALSE(img_layer->NeedsVisualOverflowRecalc());

  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(img_layer->NeedsVisualOverflowRecalc());
}

TEST_F(LayoutImageTest, IsUnsizedImage) {
  SetBodyInnerHTML(R"HTML(
    <body>
      <!-- explicit sizing -->
      <img width="100" height="100" id="a-fixed">
      <!-- without explicit sizing. -->
      <img width="100" style="height: 100px;" id="b-fixed">
      <img width="100" id="c-UNSIZED">
      <img style="aspect-ratio: 1 / 1;" id="d-UNSIZED">
      <!-- aspect ratio with at least width or height specified -->
      <img width="100" style="aspect-ratio: 1 / 1;"  id="e-fixedish">
    </body>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  std::map<std::string, bool> expected = {{"a-fixed", false},
                                          {"b-fixed", false},
                                          {"c-UNSIZED", true},
                                          {"d-UNSIZED", true},
                                          {"e-fixedish", false}};

  for (const auto& [id, expectedIsUnsized] : expected) {
    LayoutObject* obj = GetLayoutObjectByElementId(id.c_str());
    ASSERT_NE(obj, nullptr);
    LayoutImage* img = DynamicTo<LayoutImage>(obj);
    ASSERT_NE(img, nullptr);
    bool isUnsized = img->IsUnsizedImage();
    EXPECT_EQ(isUnsized, expectedIsUnsized);
  }
}

}  // namespace blink
