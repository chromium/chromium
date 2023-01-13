// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_difference.h"

#include <sstream>
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(StyleDifferenceTest, StreamOutputDefault) {
  std::stringstream string_stream;
  StyleDifference diff;
  string_stream << diff;
  EXPECT_EQ(
      "StyleDifference{layoutType=NoLayout, "
      "reshape=0, paintInvalidationType=None, recomputeVisualOverflow=0, "
      "propertySpecificDifferences=, "
      "scrollAnchorDisablingPropertyChanged=0}",
      string_stream.str());
}

TEST(StyleDifferenceTest, StreamOutputAllFieldsMutated) {
  std::stringstream string_stream;
  StyleDifference diff;
  diff.SetNeedsNormalPaintInvalidation();
  diff.SetNeedsPositionedMovementLayout();
  diff.SetNeedsReshape();
  diff.SetNeedsRecomputeVisualOverflow();
  diff.SetTransformPropertyChanged();
  diff.SetOtherTransformPropertyChanged();
  diff.SetScrollAnchorDisablingPropertyChanged();
  string_stream << diff;
  EXPECT_EQ(
      "StyleDifference{layoutType=PositionedMovement, "
      "reshape=1, paintInvalidationType=Normal, recomputeVisualOverflow=1, "
      "propertySpecificDifferences="
      "TransformPropertyChanged|OtherTransformPropertyChanged, "
      "scrollAnchorDisablingPropertyChanged=1}",
      string_stream.str());
}

TEST(StyleDifferenceTest, StreamOutputSetAllProperties) {
  std::stringstream string_stream;
  StyleDifference diff;
  diff.SetTransformPropertyChanged();
  diff.SetOtherTransformPropertyChanged();
  diff.SetOpacityChanged();
  diff.SetZIndexChanged();
  diff.SetFilterChanged();
  diff.SetCSSClipChanged();
  diff.SetTextDecorationOrColorChanged();
  diff.SetBlendModeChanged();
  string_stream << diff;
  EXPECT_EQ(
      "StyleDifference{layoutType=NoLayout, "
      "reshape=0, paintInvalidationType=None, recomputeVisualOverflow=0, "
      "propertySpecificDifferences=TransformPropertyChanged|"
      "OtherTransformPropertyChanged|OpacityChanged|"
      "ZIndexChanged|FilterChanged|CSSClipChanged|"
      "TextDecorationOrColorChanged|BlendModeChanged, "
      "scrollAnchorDisablingPropertyChanged=0}",
      string_stream.str());
}

}  // namespace blink
