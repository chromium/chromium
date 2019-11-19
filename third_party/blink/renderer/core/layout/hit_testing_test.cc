// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class HitTestingTest : public RenderingTest {};

TEST_F(HitTestingTest, OcclusionHitTest) {
  SetBodyInnerHTML(R"HTML(
    <style>
    div {
      width: 100px;
      height: 100px;
    }
    </style>

    <div id=target></div>
    <div id=occluder></div>
  )HTML");

  Element* target = GetDocument().getElementById("target");
  Element* occluder = GetDocument().getElementById("occluder");
  HitTestResult result = target->GetLayoutObject()->HitTestForOcclusion();
  EXPECT_EQ(result.InnerNode(), target);

  occluder->SetInlineStyleProperty(CSSPropertyID::kMarginTop, "-10px");
  UpdateAllLifecyclePhasesForTest();
  result = target->GetLayoutObject()->HitTestForOcclusion();
  EXPECT_EQ(result.InnerNode(), occluder);
}

TEST_F(HitTestingTest, OcclusionHitTestWithClipPath) {
  SetBodyInnerHTML(R"HTML(
    <style>
    div {
      width: 100px;
      height: 100px;
    }
    #occluder {
      clip-path: url(#clip);
    }
    </style>

    <svg viewBox="0 0 100 100" width=0>
      <clipPath id="clip">
        <circle cx="50" cy="50" r="45" stroke="none" />
      </clipPath>
    </svg>

    <div id=target></div>
    <div id=occluder></div>
  )HTML");

  Element* target = GetDocument().getElementById("target");
  Element* occluder = GetDocument().getElementById("occluder");

  // target and occluder don't overlap, no occlusion.
  HitTestResult result = target->GetLayoutObject()->HitTestForOcclusion();
  EXPECT_EQ(result.InnerNode(), target);

  // target and occluder layout rects overlap, but the overlapping area of the
  // occluder is clipped out, so no occlusion.
  occluder->SetInlineStyleProperty(CSSPropertyID::kMarginTop, "-4px");
  UpdateAllLifecyclePhasesForTest();
  result = target->GetLayoutObject()->HitTestForOcclusion();
  EXPECT_EQ(result.InnerNode(), target);

  // target and clipped area of occluder overlap, so there is occlusion.
  occluder->SetInlineStyleProperty(CSSPropertyID::kMarginTop, "-6px");
  UpdateAllLifecyclePhasesForTest();
  result = target->GetLayoutObject()->HitTestForOcclusion();
  EXPECT_EQ(result.InnerNode(), occluder);
}

}  // namespace blink
