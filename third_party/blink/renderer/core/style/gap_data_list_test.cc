// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/gap_data_list.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(GapDataListTest, GapDataListEquivalence) {
  // Gap data list with the same value(s) should be equal.
  GapDataList<StyleColor> gap_colors =
      GapDataList<StyleColor>(StyleColor(Color(0, 0, 1)));
  GapDataList<StyleColor> gap_colors1 =
      GapDataList<StyleColor>(StyleColor(Color(0, 0, 1)));
  EXPECT_EQ(gap_colors, gap_colors1);

  // Gap data list with same GapDataVector should equal.
  typename GapDataList<StyleColor>::GapDataVector gap_data_vector;
  gap_data_vector.push_back(GapData<StyleColor>(StyleColor(Color(0, 0, 1))));
  gap_data_vector.push_back(GapData<StyleColor>(StyleColor(Color(1, 0, 0))));
  GapDataList<StyleColor> gap_colors2 =
      GapDataList<StyleColor>(std::move(gap_data_vector));

  typename GapDataList<StyleColor>::GapDataVector gap_data_vector2;
  gap_data_vector2.push_back(GapData<StyleColor>(StyleColor(Color(0, 0, 1))));
  gap_data_vector2.push_back(GapData<StyleColor>(StyleColor(Color(1, 0, 0))));
  GapDataList<StyleColor> gap_colors3 =
      GapDataList<StyleColor>(std::move(gap_data_vector2));
  EXPECT_EQ(gap_colors2, gap_colors3);

  // Gap data list with different values should not be equal.
  GapDataList<StyleColor> default_gap_colors =
      GapDataList<StyleColor>::DefaultGapColorDataList();
  EXPECT_NE(gap_colors3, default_gap_colors);
}

}  // namespace blink
