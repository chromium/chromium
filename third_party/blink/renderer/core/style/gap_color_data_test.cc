// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/gap_color_data.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(GapColorData, GapColorDataEquivalence) {
  // Gap color data with the same color should be equal.
  GapColorData color = GapColorData(StyleColor(Color(1, 0, 0)));
  GapColorData color2 = GapColorData(StyleColor(Color(1, 0, 0)));

  EXPECT_EQ(color, color2);

  // Gap color data with different colors should not be equal.
  GapColorData color3 = GapColorData(StyleColor(Color(0, 0, 1)));
  EXPECT_NE(color, color3);

  // Gap color data with a repeater should not be equal to a gap color data with
  // a color.
  StyleColorVector colors;
  colors.push_back(StyleColor(Color(1, 0, 0)));
  StyleColorRepeater* repeater =
      MakeGarbageCollected<StyleColorRepeater>(std::move(colors));
  GapColorData color_repeater = GapColorData(repeater);
  EXPECT_NE(color, color_repeater);

  // Gap color data with the same repeater value should be equal.
  StyleColorVector colors2;
  colors2.push_back(StyleColor(Color(1, 0, 0)));
  StyleColorRepeater* repeater2 =
      MakeGarbageCollected<StyleColorRepeater>(std::move(colors2));
  GapColorData color_repeater2 = GapColorData(repeater2);
  EXPECT_EQ(color_repeater, color_repeater2);
}

}  // namespace blink
