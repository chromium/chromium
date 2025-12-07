// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/cascade_filter.h"

#include <gtest/gtest.h>

#include "third_party/blink/renderer/core/css/properties/longhands.h"

namespace blink {

TEST(CascadeFilterTest, FilterNothing) {
  CascadeFilter filter;
  EXPECT_TRUE(filter.Accepts(GetCSSPropertyBackgroundColor()));
  EXPECT_TRUE(filter.Accepts(GetCSSPropertyColor()));
  EXPECT_TRUE(filter.Accepts(GetCSSPropertyDisplay()));
  EXPECT_TRUE(filter.Accepts(GetCSSPropertyFloat()));
  EXPECT_TRUE(filter.Accepts(GetCSSPropertyInternalVisitedColor()));
}

TEST(CascadeFilterTest, ConstructorBehavesLikeSingleAdd) {
  EXPECT_EQ(CascadeFilter().Add(CSSProperty::kInherited),
            CascadeFilter(CSSProperty::kInherited));
}

TEST(CascadeFilterTest, Equals) {
  EXPECT_EQ(CascadeFilter(CSSProperty::kInherited),
            CascadeFilter(CSSProperty::kInherited));
}

TEST(CascadeFilterTest, NotEqualsMask) {
  EXPECT_NE(CascadeFilter(CSSProperty::kInherited),
            CascadeFilter(CSSProperty::kVisited));
  EXPECT_NE(CascadeFilter(CSSProperty::kInherited),
            CascadeFilter(CSSProperty::kInherited).Add(CSSProperty::kVisited));
  EXPECT_NE(CascadeFilter(CSSProperty::kInherited), CascadeFilter());
}

TEST(CascadeFilterTest, FilterNonInherited) {
  CascadeFilter filter(CSSProperty::kInherited);
  EXPECT_FALSE(filter.Accepts(GetCSSPropertyBackgroundColor()));
  EXPECT_TRUE(filter.Accepts(GetCSSPropertyColor()));
  EXPECT_FALSE(filter.Accepts(GetCSSPropertyDisplay()));
  EXPECT_FALSE(filter.Accepts(GetCSSPropertyFloat()));
  EXPECT_TRUE(filter.Accepts(GetCSSPropertyInternalVisitedColor()));
}

TEST(CascadeFilterTest, FilterVisitedAndNonInherited) {
  auto filter = CascadeFilter()
                    .Add(CSSProperty::kNotVisited)
                    .Add(CSSProperty::kInherited);
  EXPECT_FALSE(filter.Accepts(GetCSSPropertyBackgroundColor()));
  EXPECT_TRUE(filter.Accepts(GetCSSPropertyColor()));
  EXPECT_FALSE(filter.Accepts(GetCSSPropertyDisplay()));
  EXPECT_FALSE(filter.Accepts(GetCSSPropertyFloat()));
  EXPECT_FALSE(filter.Accepts(GetCSSPropertyInternalVisitedColor()));
}

TEST(CascadeFilterTest, RejectFlag) {
  auto filter =
      CascadeFilter().Add(CSSProperty::kVisited).Add(CSSProperty::kInherited);
  EXPECT_TRUE(filter.Requires(CSSProperty::kVisited));
  EXPECT_TRUE(filter.Requires(CSSProperty::kInherited));
  EXPECT_FALSE(filter.Requires(CSSProperty::kNotVisited));
}

TEST(CascadeFilterTest, FilterLegacyOverlapping) {
  auto filter = CascadeFilter().Add(CSSProperty::kNotLegacyOverlapping);
  EXPECT_FALSE(filter.Accepts(GetCSSPropertyWebkitTransformOriginX()));
  EXPECT_FALSE(filter.Accepts(GetCSSPropertyWebkitTransformOriginY()));
  EXPECT_FALSE(filter.Accepts(GetCSSPropertyWebkitTransformOriginZ()));
  EXPECT_FALSE(filter.Accepts(GetCSSPropertyWebkitPerspectiveOriginX()));
  EXPECT_FALSE(filter.Accepts(GetCSSPropertyWebkitPerspectiveOriginY()));
  EXPECT_FALSE(filter.Accepts(GetCSSPropertyWebkitBorderImage()));
  EXPECT_TRUE(filter.Accepts(GetCSSPropertyTransformOrigin()));
  EXPECT_TRUE(filter.Accepts(GetCSSPropertyPerspectiveOrigin()));
  EXPECT_TRUE(filter.Accepts(GetCSSPropertyBorderImageSource()));
  EXPECT_TRUE(filter.Accepts(GetCSSPropertyBorderImageSlice()));
  EXPECT_TRUE(filter.Accepts(GetCSSPropertyBorderImageRepeat()));
  EXPECT_TRUE(filter.Accepts(GetCSSPropertyBorderImageWidth()));
  EXPECT_TRUE(filter.Accepts(GetCSSPropertyBorderImageOutset()));
  EXPECT_TRUE(filter.Accepts(GetCSSPropertyColor()));
  EXPECT_TRUE(filter.Accepts(GetCSSPropertyFloat()));
}

TEST(CascadeFilterTest, FilterOverlapping) {
  auto filter = CascadeFilter().Add(CSSProperty::kOverlapping);
  EXPECT_TRUE(filter.Accepts(GetCSSPropertyWebkitTransformOriginX()));
  EXPECT_TRUE(filter.Accepts(GetCSSPropertyWebkitTransformOriginY()));
  EXPECT_TRUE(filter.Accepts(GetCSSPropertyWebkitTransformOriginZ()));
  EXPECT_TRUE(filter.Accepts(GetCSSPropertyWebkitPerspectiveOriginX()));
  EXPECT_TRUE(filter.Accepts(GetCSSPropertyWebkitPerspectiveOriginY()));
  EXPECT_TRUE(filter.Accepts(GetCSSPropertyWebkitBorderImage()));
  EXPECT_TRUE(filter.Accepts(GetCSSPropertyTransformOrigin()));
  EXPECT_TRUE(filter.Accepts(GetCSSPropertyPerspectiveOrigin()));
  EXPECT_TRUE(filter.Accepts(GetCSSPropertyBorderImageSource()));
  EXPECT_TRUE(filter.Accepts(GetCSSPropertyBorderImageSlice()));
  EXPECT_TRUE(filter.Accepts(GetCSSPropertyBorderImageRepeat()));
  EXPECT_TRUE(filter.Accepts(GetCSSPropertyBorderImageWidth()));
  EXPECT_TRUE(filter.Accepts(GetCSSPropertyBorderImageOutset()));
  EXPECT_FALSE(filter.Accepts(GetCSSPropertyColor()));
  EXPECT_FALSE(filter.Accepts(GetCSSPropertyFloat()));
}

}  // namespace blink
