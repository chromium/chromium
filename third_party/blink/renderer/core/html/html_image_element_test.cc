// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_image_element.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class HTMLImageElementTest : public PageTestBase {
 protected:
  static constexpr int kViewportWidth = 500;
  static constexpr int kViewportHeight = 600;

  void SetUp() override {
    PageTestBase::SetUp(IntSize(kViewportWidth, kViewportHeight));
  }
};

// Instantiate class constants. Not needed after C++17.
constexpr int HTMLImageElementTest::kViewportWidth;
constexpr int HTMLImageElementTest::kViewportHeight;

TEST_F(HTMLImageElementTest, width) {
  auto* image = MakeGarbageCollected<HTMLImageElement>(GetDocument());
  image->setAttribute(html_names::kWidthAttr, "400");
  // TODO(yoav): `width` does not impact resourceWidth until we resolve
  // https://github.com/ResponsiveImagesCG/picture-element/issues/268
  EXPECT_EQ(500, image->GetResourceWidth().width);
  image->setAttribute(html_names::kSizesAttr, "100vw");
  EXPECT_EQ(500, image->GetResourceWidth().width);
}

TEST_F(HTMLImageElementTest, sourceSize) {
  auto* image = MakeGarbageCollected<HTMLImageElement>(GetDocument());
  image->setAttribute(html_names::kWidthAttr, "400");
  EXPECT_EQ(kViewportWidth, image->SourceSize(*image));
  image->setAttribute(html_names::kSizesAttr, "50vw");
  EXPECT_EQ(250, image->SourceSize(*image));
}

TEST_F(HTMLImageElementTest, attributeLazyLoadDimensionType) {
  struct TestCase {
    const char* attribute_value;
    HTMLImageElement::LazyLoadDimensionType expected_dimension_type;
  };
  const TestCase test_cases[] = {
      {"", HTMLImageElement::LazyLoadDimensionType::kNotAbsolute},
      {"invalid", HTMLImageElement::LazyLoadDimensionType::kNotAbsolute},
      {"10px", HTMLImageElement::LazyLoadDimensionType::kAbsoluteSmall},
      {"10", HTMLImageElement::LazyLoadDimensionType::kAbsoluteSmall},
      {"100px", HTMLImageElement::LazyLoadDimensionType::kAbsoluteNotSmall},
      {"100", HTMLImageElement::LazyLoadDimensionType::kAbsoluteNotSmall},
  };
  for (const auto& test : test_cases) {
    EXPECT_EQ(test.expected_dimension_type,
              HTMLImageElement::GetAttributeLazyLoadDimensionType(
                  test.attribute_value));
  }
}

TEST_F(HTMLImageElementTest, inlineStyleLazyLoadDimensionType) {
  struct TestCase {
    const char* inline_style;
    HTMLImageElement::LazyLoadDimensionType expected_dimension_type;
  };
  const TestCase test_cases[] = {
      {"", HTMLImageElement::LazyLoadDimensionType::kNotAbsolute},
      {"invalid", HTMLImageElement::LazyLoadDimensionType::kNotAbsolute},
      {"height: 1px", HTMLImageElement::LazyLoadDimensionType::kNotAbsolute},
      {"width: 1px", HTMLImageElement::LazyLoadDimensionType::kNotAbsolute},
      {"height: 1; width: 1",
       HTMLImageElement::LazyLoadDimensionType::kNotAbsolute},
      {"height: 50%; width: 50%",
       HTMLImageElement::LazyLoadDimensionType::kNotAbsolute},
      {"height: 1px; width: 1px",
       HTMLImageElement::LazyLoadDimensionType::kAbsoluteSmall},
      {"height: 10px; width: 10px",
       HTMLImageElement::LazyLoadDimensionType::kAbsoluteSmall},
      {"height: 100px; width: 10px",
       HTMLImageElement::LazyLoadDimensionType::kAbsoluteNotSmall},
      {"height: 10px; width: 100px",
       HTMLImageElement::LazyLoadDimensionType::kAbsoluteNotSmall},
      {"height: 100px; width: 100px",
       HTMLImageElement::LazyLoadDimensionType::kAbsoluteNotSmall},
      {"height: 100; width: 100",
       HTMLImageElement::LazyLoadDimensionType::kNotAbsolute},
      {"height: 100%; width: 100%",
       HTMLImageElement::LazyLoadDimensionType::kNotAbsolute},
  };
  for (const auto& test : test_cases) {
    const ImmutableCSSPropertyValueSet* property_set =
        CSSParser::ParseInlineStyleDeclaration(
            test.inline_style, kHTMLStandardMode,
            SecureContextMode::kInsecureContext);
    EXPECT_EQ(test.expected_dimension_type,
              HTMLImageElement::GetInlineStyleDimensionsType(property_set));
  }
}

}  // namespace blink
