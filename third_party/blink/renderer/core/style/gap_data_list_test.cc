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

namespace {

using GapDataVector = GapDataList<int>::GapDataVector;

GapData<int> IntegerRepeater(Vector<int> values, wtf_size_t repeat_count) {
  auto* repeater =
      MakeGarbageCollected<ValueRepeater<int>>(std::move(values), repeat_count);
  return GapData<int>(repeater);
}

GapData<int> AutoRepeater(Vector<int> values) {
  auto* repeater = MakeGarbageCollected<ValueRepeater<int>>(
      std::move(values), /*repeat_count=*/std::nullopt);
  return GapData<int>(repeater);
}

// Asserts that `GapDataListValueAccessor::ValueAt` (random access) agrees
// with `GapDataListIterator::Next()` (sequential access) at every index, for
// `gap_count` gaps.
void ExpectAccessorMatchesIterator(const GapDataVector& gap_data_list,
                                   wtf_size_t gap_count) {
  GapDataListValueAccessor<int> accessor(gap_data_list, gap_count);
  GapDataListIterator<int> iterator(gap_data_list, gap_count);
  for (wtf_size_t i = 0; i < gap_count; ++i) {
    EXPECT_EQ(accessor.ValueAt(i), iterator.Next()) << "at index " << i;
  }
}

}  // namespace

TEST(GapDataListTest, IteratorSkipsZeroSlotAutoRepeater) {
  GapDataVector gap_data_list;
  gap_data_list.push_back(AutoRepeater({5}));
  gap_data_list.push_back(GapData<int>(9));
  gap_data_list.push_back(GapData<int>(10));

  GapDataListIterator<int> iterator(gap_data_list, /*gap_count=*/2);
  EXPECT_EQ(iterator.Next(), 9);
  EXPECT_EQ(iterator.Next(), 10);
}

TEST(GapDataListTest, ValueAccessorLeadingOnlyList) {
  // A plain (non-repeater) list with no auto-repeater cycles back to the
  // start when there are more gaps than list entries.
  GapDataVector gap_data_list;
  gap_data_list.push_back(GapData<int>(1));
  gap_data_list.push_back(GapData<int>(2));
  gap_data_list.push_back(GapData<int>(3));
  ExpectAccessorMatchesIterator(gap_data_list, /*gap_count=*/7);
}

TEST(GapDataListTest, ValueAccessorIntegerRepeaterInLeadingRegion) {
  GapDataVector gap_data_list;
  gap_data_list.push_back(GapData<int>(1));
  gap_data_list.push_back(IntegerRepeater({7, 8}, /*repeat_count=*/2));
  gap_data_list.push_back(GapData<int>(9));
  ExpectAccessorMatchesIterator(gap_data_list, /*gap_count=*/8);
}

TEST(GapDataListTest, ValueAccessorAutoRepeaterOnly) {
  GapDataVector gap_data_list;
  gap_data_list.push_back(AutoRepeater({10, 20}));
  ExpectAccessorMatchesIterator(gap_data_list, /*gap_count=*/5);
}

TEST(GapDataListTest, ValueAccessorLeadingAutoAndTrailingRegions) {
  GapDataVector gap_data_list;
  gap_data_list.push_back(GapData<int>(1));
  gap_data_list.push_back(AutoRepeater({5, 6}));
  gap_data_list.push_back(GapData<int>(9));
  ExpectAccessorMatchesIterator(gap_data_list, /*gap_count=*/8);
}

TEST(GapDataListTest, ValueAccessorAutoRegionSquashedByFixedRegions) {
  // When leading + trailing slot counts already meet or exceed `gap_count`,
  // the auto region contributes zero slots.
  GapDataVector gap_data_list;
  gap_data_list.push_back(GapData<int>(1));
  gap_data_list.push_back(GapData<int>(2));
  gap_data_list.push_back(AutoRepeater({5, 6}));
  gap_data_list.push_back(GapData<int>(9));
  ExpectAccessorMatchesIterator(gap_data_list, /*gap_count=*/3);
}

}  // namespace blink
