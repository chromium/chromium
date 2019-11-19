// Copyright 2017 The Chromium Authors. All rights reserved.
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
      "collectInlines=0, reshape=0, "
      "paintInvalidationType=NoPaintInvalidation, recomputeOverflow=0, "
      "visualRectUpdate=0, propertySpecificDifferences=, "
      "scrollAnchorDisablingPropertyChanged=0}",
      string_stream.str());
}

TEST(StyleDifferenceTest, StreamOutputAllFieldsMutated) {
  std::stringstream string_stream;
  StyleDifference diff;
  diff.SetNeedsPaintInvalidationObject();
  diff.SetNeedsPositionedMovementLayout();
  diff.SetNeedsReshape();
  diff.SetNeedsCollectInlines();
  diff.SetNeedsRecomputeOverflow();
  diff.SetNeedsVisualRectUpdate();
  diff.SetTransformChanged();
  diff.SetScrollAnchorDisablingPropertyChanged();
  string_stream << diff;
  EXPECT_EQ(
      "StyleDifference{layoutType=PositionedMovement, "
      "collectInlines=1, reshape=1, "
      "paintInvalidationType=PaintInvalidationObject, recomputeOverflow=1, "
      "visualRectUpdate=1, propertySpecificDifferences=TransformChanged, "
      "scrollAnchorDisablingPropertyChanged=1}",
      string_stream.str());
}

TEST(StyleDifferenceTest, StreamOutputSetAllProperties) {
  std::stringstream string_stream;
  StyleDifference diff;
  diff.SetTransformChanged();
  diff.SetOpacityChanged();
  diff.SetZIndexChanged();
  diff.SetFilterChanged();
  diff.SetBackdropFilterChanged();
  diff.SetCSSClipChanged();
  diff.SetTextDecorationOrColorChanged();
  diff.SetBlendModeChanged();
  string_stream << diff;
  EXPECT_EQ(
      "StyleDifference{layoutType=NoLayout, "
      "collectInlines=0, reshape=0, "
      "paintInvalidationType=NoPaintInvalidation, recomputeOverflow=0, "
      "visualRectUpdate=0, "
      "propertySpecificDifferences=TransformChanged|OpacityChanged|"
      "ZIndexChanged|FilterChanged|BackdropFilterChanged|CSSClipChanged|"
      "TextDecorationOrColorChanged|BlendModeChanged, "
      "scrollAnchorDisablingPropertyChanged=0}",
      string_stream.str());
}

}  // namespace blink
