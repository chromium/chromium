// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/cascade_filter.h"
#include <gtest/gtest.h>
#include "third_party/blink/renderer/core/css/properties/longhands.h"

namespace blink {

TEST(CascadeFilterTest, FilterNothing) {
  CascadeFilter filter;
  EXPECT_FALSE(filter.Rejects(GetCSSPropertyBackgroundColor()));
  EXPECT_FALSE(filter.Rejects(GetCSSPropertyColor()));
  EXPECT_FALSE(filter.Rejects(GetCSSPropertyDisplay()));
  EXPECT_FALSE(filter.Rejects(GetCSSPropertyFloat()));
  EXPECT_FALSE(filter.Rejects(GetCSSPropertyInternalVisitedColor()));
}

TEST(CascadeFilterTest, ConstructorBehavesLikeSingleAdd) {
  EXPECT_EQ(CascadeFilter().Add(CSSProperty::kInherited, true),
            CascadeFilter(CSSProperty::kInherited, true));
  EXPECT_EQ(CascadeFilter().Add(CSSProperty::kInherited, false),
            CascadeFilter(CSSProperty::kInherited, false));
}

TEST(CascadeFilterTest, Equals) {
  EXPECT_EQ(CascadeFilter(CSSProperty::kInherited, true),
            CascadeFilter(CSSProperty::kInherited, true));
  EXPECT_EQ(CascadeFilter(CSSProperty::kInherited, false),
            CascadeFilter(CSSProperty::kInherited, false));
}

TEST(CascadeFilterTest, NotEqualsMask) {
  EXPECT_NE(CascadeFilter(CSSProperty::kInherited, true),
            CascadeFilter(CSSProperty::kInherited, false));
  EXPECT_NE(CascadeFilter(CSSProperty::kInherited, false),
            CascadeFilter(CSSProperty::kVisited, false));
  EXPECT_NE(CascadeFilter(CSSProperty::kInherited, false),
            CascadeFilter(CSSProperty::kInherited, false)
                .Add(CSSProperty::kVisited, false));
  EXPECT_NE(CascadeFilter(CSSProperty::kInherited, false), CascadeFilter());
}

TEST(CascadeFilterTest, FilterInherited) {
  CascadeFilter filter(CSSProperty::kInherited, true);
  EXPECT_FALSE(filter.Rejects(GetCSSPropertyBackgroundColor()));
  EXPECT_TRUE(filter.Rejects(GetCSSPropertyColor()));
  EXPECT_TRUE(filter.Rejects(GetCSSPropertyFontSize()));
  EXPECT_FALSE(filter.Rejects(GetCSSPropertyDisplay()));
  EXPECT_FALSE(filter.Rejects(GetCSSPropertyFloat()));
  EXPECT_TRUE(filter.Rejects(GetCSSPropertyInternalVisitedColor()));
}

TEST(CascadeFilterTest, FilterNonInherited) {
  CascadeFilter filter(CSSProperty::kInherited, false);
  EXPECT_TRUE(filter.Rejects(GetCSSPropertyBackgroundColor()));
  EXPECT_FALSE(filter.Rejects(GetCSSPropertyColor()));
  EXPECT_TRUE(filter.Rejects(GetCSSPropertyDisplay()));
  EXPECT_TRUE(filter.Rejects(GetCSSPropertyFloat()));
  EXPECT_FALSE(filter.Rejects(GetCSSPropertyInternalVisitedColor()));
}

TEST(CascadeFilterTest, FilterVisitedAndInherited) {
  auto filter = CascadeFilter()
                    .Add(CSSProperty::kVisited, true)
                    .Add(CSSProperty::kInherited, true);
  EXPECT_FALSE(filter.Rejects(GetCSSPropertyBackgroundColor()));
  EXPECT_TRUE(filter.Rejects(GetCSSPropertyColor()));
  EXPECT_FALSE(filter.Rejects(GetCSSPropertyDisplay()));
  EXPECT_FALSE(filter.Rejects(GetCSSPropertyFloat()));
  EXPECT_TRUE(filter.Rejects(GetCSSPropertyInternalVisitedBackgroundColor()));
}

TEST(CascadeFilterTest, FilterVisitedAndNonInherited) {
  auto filter = CascadeFilter()
                    .Add(CSSProperty::kVisited, true)
                    .Add(CSSProperty::kInherited, false);
  EXPECT_TRUE(filter.Rejects(GetCSSPropertyBackgroundColor()));
  EXPECT_FALSE(filter.Rejects(GetCSSPropertyColor()));
  EXPECT_TRUE(filter.Rejects(GetCSSPropertyDisplay()));
  EXPECT_TRUE(filter.Rejects(GetCSSPropertyFloat()));
  EXPECT_TRUE(filter.Rejects(GetCSSPropertyInternalVisitedColor()));
}

TEST(CascadeFilterTest, RejectFlag) {
  auto filter = CascadeFilter()
                    .Add(CSSProperty::kVisited, true)
                    .Add(CSSProperty::kInherited, false);
  EXPECT_TRUE(filter.Rejects(CSSProperty::kVisited, true));
  EXPECT_TRUE(filter.Rejects(CSSProperty::kInherited, false));
  EXPECT_FALSE(filter.Rejects(CSSProperty::kVisited, false));
  EXPECT_FALSE(filter.Rejects(CSSProperty::kInherited, true));
}

TEST(CascadeFilterTest, AddDoesNotOverwrite) {
  auto filter = CascadeFilter()
                    .Add(CSSProperty::kVisited, true)
                    .Add(CSSProperty::kInherited, false);
  EXPECT_TRUE(filter.Rejects(CSSProperty::kVisited, true));
  EXPECT_TRUE(filter.Rejects(CSSProperty::kInherited, false));
  filter = filter.Add(CSSProperty::kVisited, false);
  filter = filter.Add(CSSProperty::kInherited, true);
  // Add has no effect if flags are already set:
  EXPECT_TRUE(filter.Rejects(CSSProperty::kVisited, true));
  EXPECT_TRUE(filter.Rejects(CSSProperty::kInherited, false));
}

TEST(CascadeFilterTest, SetDoesOverwrite) {
  auto filter = CascadeFilter()
                    .Add(CSSProperty::kVisited, true)
                    .Add(CSSProperty::kInherited, false);
  EXPECT_TRUE(filter.Rejects(CSSProperty::kVisited, true));
  EXPECT_TRUE(filter.Rejects(CSSProperty::kInherited, false));
  filter = filter.Set(CSSProperty::kVisited, false);
  filter = filter.Set(CSSProperty::kInherited, true);
  // Add has no effect if flags are already set:
  EXPECT_TRUE(filter.Rejects(CSSProperty::kVisited, false));
  EXPECT_TRUE(filter.Rejects(CSSProperty::kInherited, true));
}

TEST(CascadeFilterTest, FilterLegacyOverlapping) {
  auto filter = CascadeFilter().Add(CSSProperty::kLegacyOverlapping, true);
  EXPECT_TRUE(filter.Rejects(GetCSSPropertyWebkitTransformOriginX()));
  EXPECT_TRUE(filter.Rejects(GetCSSPropertyWebkitTransformOriginY()));
  EXPECT_TRUE(filter.Rejects(GetCSSPropertyWebkitTransformOriginZ()));
  EXPECT_TRUE(filter.Rejects(GetCSSPropertyWebkitPerspectiveOriginX()));
  EXPECT_TRUE(filter.Rejects(GetCSSPropertyWebkitPerspectiveOriginY()));
  EXPECT_TRUE(filter.Rejects(GetCSSPropertyWebkitBorderImage()));
  EXPECT_FALSE(filter.Rejects(GetCSSPropertyTransformOrigin()));
  EXPECT_FALSE(filter.Rejects(GetCSSPropertyPerspectiveOrigin()));
  EXPECT_FALSE(filter.Rejects(GetCSSPropertyBorderImageSource()));
  EXPECT_FALSE(filter.Rejects(GetCSSPropertyBorderImageSlice()));
  EXPECT_FALSE(filter.Rejects(GetCSSPropertyBorderImageRepeat()));
  EXPECT_FALSE(filter.Rejects(GetCSSPropertyBorderImageWidth()));
  EXPECT_FALSE(filter.Rejects(GetCSSPropertyBorderImageOutset()));
  EXPECT_FALSE(filter.Rejects(GetCSSPropertyColor()));
  EXPECT_FALSE(filter.Rejects(GetCSSPropertyFloat()));
}

TEST(CascadeFilterTest, FilterOverlapping) {
  auto filter = CascadeFilter().Add(CSSProperty::kOverlapping, true);
  EXPECT_TRUE(filter.Rejects(GetCSSPropertyWebkitTransformOriginX()));
  EXPECT_TRUE(filter.Rejects(GetCSSPropertyWebkitTransformOriginY()));
  EXPECT_TRUE(filter.Rejects(GetCSSPropertyWebkitTransformOriginZ()));
  EXPECT_TRUE(filter.Rejects(GetCSSPropertyWebkitPerspectiveOriginX()));
  EXPECT_TRUE(filter.Rejects(GetCSSPropertyWebkitPerspectiveOriginY()));
  EXPECT_TRUE(filter.Rejects(GetCSSPropertyWebkitBorderImage()));
  EXPECT_TRUE(filter.Rejects(GetCSSPropertyTransformOrigin()));
  EXPECT_TRUE(filter.Rejects(GetCSSPropertyPerspectiveOrigin()));
  EXPECT_TRUE(filter.Rejects(GetCSSPropertyBorderImageSource()));
  EXPECT_TRUE(filter.Rejects(GetCSSPropertyBorderImageSlice()));
  EXPECT_TRUE(filter.Rejects(GetCSSPropertyBorderImageRepeat()));
  EXPECT_TRUE(filter.Rejects(GetCSSPropertyBorderImageWidth()));
  EXPECT_TRUE(filter.Rejects(GetCSSPropertyBorderImageOutset()));
  EXPECT_FALSE(filter.Rejects(GetCSSPropertyColor()));
  EXPECT_FALSE(filter.Rejects(GetCSSPropertyFloat()));
}

}  // namespace blink
