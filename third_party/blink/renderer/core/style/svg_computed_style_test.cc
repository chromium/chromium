// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/svg_computed_style.h"

#include "third_party/blink/renderer/core/style/style_difference.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

// Ensures RefPtr values are compared by their values, not by pointers.
#define TEST_STYLE_REFCOUNTED_VALUE_NO_DIFF(type, fieldName)               \
  {                                                                        \
    scoped_refptr<SVGComputedStyle> svg1 = SVGComputedStyle::Create();     \
    scoped_refptr<SVGComputedStyle> svg2 = SVGComputedStyle::Create();     \
    scoped_refptr<type> value1 = base::MakeRefCounted<type>();             \
    scoped_refptr<type> value2 = base::MakeRefCounted<type>(value1->data); \
    svg1->Set##fieldName(value1);                                          \
    svg2->Set##fieldName(value2);                                          \
    EXPECT_FALSE(svg1->Diff(*svg2).HasDifference());                       \
  }

// This is not very useful for fields directly stored by values, because they
// can only be compared by values. This macro mainly ensures that we update the
// comparisons and tests when we change some field to RefPtr in the future.
#define TEST_STYLE_VALUE_NO_DIFF(type, fieldName)                      \
  {                                                                    \
    scoped_refptr<SVGComputedStyle> svg1 = SVGComputedStyle::Create(); \
    scoped_refptr<SVGComputedStyle> svg2 = SVGComputedStyle::Create(); \
    svg1->Set##fieldName(SVGComputedStyle::Initial##fieldName());      \
    svg2->Set##fieldName(SVGComputedStyle::Initial##fieldName());      \
    EXPECT_FALSE(svg1->Diff(*svg2).HasDifference());                   \
  }

TEST(SVGComputedStyleTest, StrokeStyleShouldCompareValue) {
  TEST_STYLE_VALUE_NO_DIFF(float, StrokeOpacity);
  TEST_STYLE_VALUE_NO_DIFF(float, StrokeMiterLimit);
  TEST_STYLE_VALUE_NO_DIFF(UnzoomedLength, StrokeWidth);
  TEST_STYLE_VALUE_NO_DIFF(Length, StrokeDashOffset);
  TEST_STYLE_REFCOUNTED_VALUE_NO_DIFF(SVGDashArray, StrokeDashArray);

  TEST_STYLE_VALUE_NO_DIFF(SVGPaint, StrokePaint);
  {
    scoped_refptr<SVGComputedStyle> svg1 = SVGComputedStyle::Create();
    scoped_refptr<SVGComputedStyle> svg2 = SVGComputedStyle::Create();
    svg1->SetInternalVisitedStrokePaint(SVGComputedStyle::InitialStrokePaint());
    svg2->SetInternalVisitedStrokePaint(SVGComputedStyle::InitialStrokePaint());
    EXPECT_FALSE(svg1->Diff(*svg2).HasDifference());
  }
}

TEST(SVGComputedStyleTest, MiscStyleShouldCompareValue) {
  TEST_STYLE_VALUE_NO_DIFF(Color, FloodColor);
  TEST_STYLE_VALUE_NO_DIFF(float, FloodOpacity);
  TEST_STYLE_VALUE_NO_DIFF(Color, LightingColor);
  TEST_STYLE_VALUE_NO_DIFF(Length, BaselineShiftValue);
}

}  // namespace blink
