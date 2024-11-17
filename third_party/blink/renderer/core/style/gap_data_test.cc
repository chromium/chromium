// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/gap_data.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(GapDataTest, GapDataEquivalence) {
  // Gap data with the same value should be equal.
  GapData<StyleColor> color = GapData<StyleColor>(StyleColor(Color(1, 0, 0)));
  GapData<StyleColor> color2 = GapData<StyleColor>(StyleColor(Color(1, 0, 0)));

  EXPECT_EQ(color, color2);

  // Gap data with different values should not be equal.
  GapData<StyleColor> color3 = GapData<StyleColor>(StyleColor(Color(0, 0, 1)));
  EXPECT_NE(color, color3);

  // Gap data with a repeater should not be equal to a gap data with
  // a single value.
  typename ValueRepeater<StyleColor>::VectorType colors;
  colors.push_back(StyleColor(Color(1, 0, 0)));
  ValueRepeater<StyleColor>* repeater =
      MakeGarbageCollected<ValueRepeater<StyleColor>>(
          std::move(colors), /*repeat_count=*/std::nullopt);
  GapData<StyleColor> color_repeater = GapData<StyleColor>(repeater);
  EXPECT_NE(color, color_repeater);

  // Gap data with the same repeater value should be equal.
  typename ValueRepeater<StyleColor>::VectorType colors2;
  colors2.push_back(StyleColor(Color(1, 0, 0)));
  ValueRepeater<StyleColor>* repeater2 =
      MakeGarbageCollected<ValueRepeater<StyleColor>>(
          std::move(colors2), /*repeat_count=*/std::nullopt);
  GapData<StyleColor> color_repeater2 = GapData<StyleColor>(repeater2);
  EXPECT_EQ(color_repeater, color_repeater2);
}

}  // namespace blink
