// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/svg/layout_svg_image.h"

#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/svg/svg_graphics_element.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class LayoutSVGImageTest : public RenderingTest {};

TEST_F(LayoutSVGImageTest, GetBBoxUseCounterForZeroHeight) {
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kGetBBoxForElementWithZeroWidthOrHeight));

  SetBodyInnerHTML(R"HTML(
    <svg>
      <image id="image" width="100" height="0"/>
    </svg>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  To<SVGGraphicsElement>(GetElementById("image"))->GetBBox();

  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kGetBBoxForElementWithZeroWidthOrHeight));
}

TEST_F(LayoutSVGImageTest, GetBBoxUseCounterForZeroWidth) {
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kGetBBoxForElementWithZeroWidthOrHeight));

  SetBodyInnerHTML(R"HTML(
    <svg>
      <image id="image" width="0" height="100"/>
    </svg>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  To<SVGGraphicsElement>(GetElementById("image"))->GetBBox();

  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kGetBBoxForElementWithZeroWidthOrHeight));
}

TEST_F(LayoutSVGImageTest, GetBBoxUseCounterForZeroWidthAndHeight) {
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kGetBBoxForElementWithZeroWidthOrHeight));

  SetBodyInnerHTML(R"HTML(
    <svg>
      <image id="image"/>
    </svg>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  To<SVGGraphicsElement>(GetElementById("image"))->GetBBox();

  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kGetBBoxForElementWithZeroWidthOrHeight));
}

TEST_F(LayoutSVGImageTest, GetBBoxUseCounterForNonZeroWidthAndHeight) {
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kGetBBoxForElementWithZeroWidthOrHeight));

  SetBodyInnerHTML(R"HTML(
    <svg>
      <image id="image" width="100" height="100"/>
    </svg>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  To<SVGGraphicsElement>(GetElementById("image"))->GetBBox();

  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kGetBBoxForElementWithZeroWidthOrHeight));
}

}  // namespace blink
