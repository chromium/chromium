// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/svg/layout_svg_viewport_container.h"

#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
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

}  // namespace blink
