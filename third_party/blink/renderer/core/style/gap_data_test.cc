// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/gap_data.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(GapData, GapDataEquivalence) {
  // Gap color data with the same color should be equal.
  GapData color = GapData(StyleColor(Color(1, 0, 0)));
  GapData color2 = GapData(StyleColor(Color(1, 0, 0)));

  EXPECT_EQ(color, color2);

  // Gap color data with different colors should not be equal.
  GapData color3 = GapData(StyleColor(Color(0, 0, 1)));
  EXPECT_NE(color, color3);

  // Gap color data with a repeater should not be equal to a gap color data with
  // a color.
  StyleColorVector colors;
  colors.push_back(StyleColor(Color(1, 0, 0)));
  StyleColorRepeater* repeater =
      MakeGarbageCollected<StyleColorRepeater>(std::move(colors));
  GapData color_repeater = GapData(repeater);
  EXPECT_NE(color, color_repeater);

  // Gap color data with the same repeater value should be equal.
  StyleColorVector colors2;
  colors2.push_back(StyleColor(Color(1, 0, 0)));
  StyleColorRepeater* repeater2 =
      MakeGarbageCollected<StyleColorRepeater>(std::move(colors2));
  GapData color_repeater2 = GapData(repeater2);
  EXPECT_EQ(color_repeater, color_repeater2);
}

}  // namespace blink
