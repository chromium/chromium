// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/gap_color_data_list.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(GapColorDataListTest, GapColorDataListEquivalence) {
  // Gap color data list with the same color(s) should be equal.
  GapColorDataList gap_colors = GapColorDataList(StyleColor(Color(0, 0, 1)));
  GapColorDataList gap_colors1 = GapColorDataList(StyleColor(Color(0, 0, 1)));
  EXPECT_EQ(gap_colors, gap_colors1);

  // Gap color data list with same GapDataVector should equal.
  GapDataVector gap_data_vector;
  gap_data_vector.push_back(GapColorData(StyleColor(Color(0, 0, 1))));
  gap_data_vector.push_back(GapColorData(StyleColor(Color(1, 0, 0))));
  GapColorDataList gap_colors2 = GapColorDataList(std::move(gap_data_vector));

  GapDataVector gap_data_vector2;
  gap_data_vector2.push_back(GapColorData(StyleColor(Color(0, 0, 1))));
  gap_data_vector2.push_back(GapColorData(StyleColor(Color(1, 0, 0))));
  GapColorDataList gap_colors3 = GapColorDataList(std::move(gap_data_vector2));
  EXPECT_EQ(gap_colors2, gap_colors3);

  // Gap color data with different colors should not be equal.
  GapColorDataList default_gap_colors =
      GapColorDataList::DefaultGapColorDataList();
  EXPECT_NE(gap_colors3, default_gap_colors);
}

}  // namespace blink
