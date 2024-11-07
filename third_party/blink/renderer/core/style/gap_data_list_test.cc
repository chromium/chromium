// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/gap_data_list.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(GapDataListTest, GapDataListEquivalence) {
  // Gap color data list with the same color(s) should be equal.
  GapDataList gap_colors = GapDataList(StyleColor(Color(0, 0, 1)));
  GapDataList gap_colors1 = GapDataList(StyleColor(Color(0, 0, 1)));
  EXPECT_EQ(gap_colors, gap_colors1);

  // Gap color data list with same GapDataVector should equal.
  GapDataVector gap_data_vector;
  gap_data_vector.push_back(GapData(StyleColor(Color(0, 0, 1))));
  gap_data_vector.push_back(GapData(StyleColor(Color(1, 0, 0))));
  GapDataList gap_colors2 = GapDataList(std::move(gap_data_vector));

  GapDataVector gap_data_vector2;
  gap_data_vector2.push_back(GapData(StyleColor(Color(0, 0, 1))));
  gap_data_vector2.push_back(GapData(StyleColor(Color(1, 0, 0))));
  GapDataList gap_colors3 = GapDataList(std::move(gap_data_vector2));
  EXPECT_EQ(gap_colors2, gap_colors3);

  // Gap color data with different colors should not be equal.
  GapDataList default_gap_colors = GapDataList::DefaultGapColorDataList();
  EXPECT_NE(gap_colors3, default_gap_colors);
}

}  // namespace blink
