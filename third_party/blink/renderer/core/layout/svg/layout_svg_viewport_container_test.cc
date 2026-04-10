// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/svg/layout_svg_viewport_container.h"

#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/svg/svg_graphics_element.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class LayoutSVGViewportContainerTest : public RenderingTest {};

TEST_F(LayoutSVGViewportContainerTest, GetBBoxUseCounterForZeroHeight) {
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kGetBBoxForNestedSVGElementWithZeroWidthOrHeight));

  SetBodyInnerHTML(R"HTML(
    <svg>
      <svg id="svg" width="100" height="0">
        <rect width="100" height="100"/>
      </svg>
    </svg>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  To<SVGGraphicsElement>(GetElementById("svg"))->GetBBox();

  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kGetBBoxForNestedSVGElementWithZeroWidthOrHeight));
}

TEST_F(LayoutSVGViewportContainerTest, GetBBoxUseCounterForZeroWidth) {
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kGetBBoxForNestedSVGElementWithZeroWidthOrHeight));

  SetBodyInnerHTML(R"HTML(
    <svg>
      <svg id="svg" width="0" height="100">
        <rect width="100" height="100"/>
      </svg>
    </svg>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  To<SVGGraphicsElement>(GetElementById("svg"))->GetBBox();

  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kGetBBoxForNestedSVGElementWithZeroWidthOrHeight));
}

TEST_F(LayoutSVGViewportContainerTest, GetBBoxUseCounterForZeroWidthAndHeight) {
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kGetBBoxForNestedSVGElementWithZeroWidthOrHeight));

  SetBodyInnerHTML(R"HTML(
    <svg>
      <svg id="svg" width="0" height="0">
        <rect width="100" height="100"/>
      </svg>
    </svg>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  To<SVGGraphicsElement>(GetElementById("svg"))->GetBBox();

  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kGetBBoxForNestedSVGElementWithZeroWidthOrHeight));
}

TEST_F(LayoutSVGViewportContainerTest,
       GetBBoxUseCounterForNonZeroWidthAndHeight) {
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kGetBBoxForNestedSVGElementWithZeroWidthOrHeight));

  SetBodyInnerHTML(R"HTML(
    <svg>
      <svg id="svg" width="100" height="100">
        <rect width="100" height="100"/>
      </svg>
    </svg>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  To<SVGGraphicsElement>(GetElementById("svg"))->GetBBox();

  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kGetBBoxForNestedSVGElementWithZeroWidthOrHeight));
}

}  // namespace blink
