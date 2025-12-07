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


TEST_F(LayoutSVGViewportContainerTest, NestedSvgCssSizingPropertiesUseCounterWithStyleSheets) {
  // Test when the feature is DISABLED - this is when the use counter triggers
  ScopedWidthAndHeightAsPresentationAttributesOnNestedSvgForTest scoped_feature(
      false);

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kNestedSvgCssSizingProperties));

  SetHtmlInnerHTML(R"HTML(
    <style>
    svg{
      width:100%;
      height:auto;
    }
    </style>
    <svg width="400px" height="300px">
      <svg width="200px" height="150px">
        <rect width="50px" height="50px"/>
      </svg>
    </svg>
  )HTML");

  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kNestedSvgCssSizingProperties));
}

TEST_F(LayoutSVGViewportContainerTest, NestedSvgCssSizingPropertiesUseCounterWithInlineStyles) {
  // Test when the feature is DISABLED - this is when the use counter triggers
  ScopedWidthAndHeightAsPresentationAttributesOnNestedSvgForTest scoped_feature(
      false);

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kNestedSvgCssSizingProperties));

  SetBodyInnerHTML(R"HTML(
    <svg width="400px" height="300px">
      <svg style="width: 100px; height: 100px;">
        <rect width="50px" height="50px"/>
      </svg>
    </svg>
  )HTML");

  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kNestedSvgCssSizingProperties));
}

TEST_F(LayoutSVGViewportContainerTest, NestedSvgCssSizingPropertiesUseCounterNotTriggered) {
  // Test when the feature is DISABLED but sizes match - no use counter
  ScopedWidthAndHeightAsPresentationAttributesOnNestedSvgForTest scoped_feature(
      false);

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kNestedSvgCssSizingProperties));

  SetBodyInnerHTML(R"HTML(
    <svg width="400px" height="300px">
      <svg width="200px" height="150px">
        <rect width="50px" height="50px"/>
      </svg>
    </svg>
  )HTML");

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kNestedSvgCssSizingProperties));
}

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

TEST_F(LayoutSVGViewportContainerTest,
       UseCssSizingPropertiesUseCounterTriggered) {
  // Test when the feature is DISABLED and the use counter triggers when use
  // properties overrides
  ScopedWidthAndHeightStylePropertiesOnUseAndSymbolForTest scoped_feature(
      false);

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kUseCssSizingProperties));

  SetBodyInnerHTML(R"HTML(
    <svg>
      <defs>
        <symbol id="s" width="500px" height="500px">
          <rect width="200px" height="200px"/>
        </symbol>
      </defs>
      <use href="#s" style="width:100px;height:100px;"></use>
    </svg>
  )HTML");

  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kUseCssSizingProperties));
}

TEST_F(LayoutSVGViewportContainerTest,
       UseCssSizingPropertiesSameValuesAsSymbolUseCounterNotTriggered) {
  // Test when the feature is DISABLED and the use counter does not trigger when
  // width/height is same
  ScopedWidthAndHeightStylePropertiesOnUseAndSymbolForTest scoped_feature(
      false);

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kUseCssSizingProperties));

  SetBodyInnerHTML(R"HTML(
    <svg>
      <defs>
        <symbol id="s" width="100px" height="100px">
          <rect width="200px" height="200px"/>
        </symbol>
      </defs>
      <use href="#s" style="width:100px;height:100px;"></use>
    </svg>
  )HTML");

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kUseCssSizingProperties));
}

TEST_F(LayoutSVGViewportContainerTest,
       UseCssSizingPropertiesUseCounterNotTriggered) {
  // Test when the feature is DISABLED but the use counter does not trigger when
  // no width/height specified on use
  ScopedWidthAndHeightStylePropertiesOnUseAndSymbolForTest scoped_feature(
      false);

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kUseCssSizingProperties));

  SetBodyInnerHTML(R"HTML(
    <svg>
      <defs>
        <symbol id="s" width="500px" height="500px">
          <rect width="200px" height="200px"/>
        </symbol>
      </defs>
      <use href="#s"></use>
    </svg>
  )HTML");

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kUseCssSizingProperties));
}

}  // namespace blink
