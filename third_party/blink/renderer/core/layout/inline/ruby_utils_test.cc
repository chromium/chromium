// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/ruby_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/inline/inline_box_state.h"

namespace blink {

using ColumnList = HeapVector<Member<LogicalRubyColumn>>;
using RubyLevel = RubyBlockPositionCalculator::RubyLevel;

TEST(RubyBlockPositionCalculatorTest, GroupLinesEmpty) {
  RubyBlockPositionCalculator calculator;
  calculator.GroupLines(ColumnList());
  ASSERT_EQ(1u, calculator.RubyLineListForTesting().size());
  EXPECT_TRUE(calculator.RubyLineListForTesting()[0]->IsBaseLevel());
}

TEST(RubyBlockPositionCalculatorTest, GroupLinesOneAnnotationLevel) {
  ColumnList column_list;
  // Two LogicalRubyColumns with no overlaps.
  column_list.push_back(MakeGarbageCollected<LogicalRubyColumn>());
  column_list.back()->start_index = 1;
  column_list.back()->size = 1;
  column_list.push_back(MakeGarbageCollected<LogicalRubyColumn>());
  column_list.back()->start_index = 10;
  column_list.back()->size = 3;

  RubyBlockPositionCalculator calculator;
  calculator.GroupLines(column_list);
  ASSERT_EQ(2u, calculator.RubyLineListForTesting().size());
  EXPECT_TRUE(calculator.RubyLineListForTesting()[0]->IsBaseLevel());
  EXPECT_EQ(RubyLevel{1}, calculator.RubyLineListForTesting()[1]->Level());
}

TEST(RubyBlockPositionCalculatorTest, GroupLinesNested) {
  ColumnList column_list;
  // Two nested LogicalRubyColumns.
  column_list.push_back(MakeGarbageCollected<LogicalRubyColumn>());
  column_list.back()->start_index = 1;
  column_list.back()->size = 10;
  column_list.push_back(MakeGarbageCollected<LogicalRubyColumn>());
  column_list.back()->start_index = 3;
  column_list.back()->size = 4;

  RubyBlockPositionCalculator calculator;
  calculator.GroupLines(column_list);
  ASSERT_EQ(3u, calculator.RubyLineListForTesting().size());
  EXPECT_TRUE(calculator.RubyLineListForTesting()[0]->IsBaseLevel());
  EXPECT_EQ(RubyLevel{1}, calculator.RubyLineListForTesting()[1]->Level());
  EXPECT_EQ(RubyLevel{2}, calculator.RubyLineListForTesting()[2]->Level());
}

TEST(RubyBlockPositionCalculatorTest, GroupLinesBothSides) {
  ColumnList column_list;
  column_list.push_back(MakeGarbageCollected<LogicalRubyColumn>());
  column_list.back()->start_index = 1;
  column_list.back()->size = 10;
  column_list.back()->ruby_position = RubyPosition::kOver;
  // Nested in the above, but on the opposite position.
  column_list.push_back(MakeGarbageCollected<LogicalRubyColumn>());
  column_list.back()->start_index = 2;
  column_list.back()->size = 3;
  column_list.back()->ruby_position = RubyPosition::kUnder;

  // Another nested pairs, but RubyPositions are reversed.
  column_list.push_back(MakeGarbageCollected<LogicalRubyColumn>());
  column_list.back()->start_index = 20;
  column_list.back()->size = 10;
  column_list.back()->ruby_position = RubyPosition::kOver;
  // Nested in the above, but on the opposite position.
  column_list.push_back(MakeGarbageCollected<LogicalRubyColumn>());
  column_list.back()->start_index = 22;
  column_list.back()->size = 3;
  column_list.back()->ruby_position = RubyPosition::kUnder;

  RubyBlockPositionCalculator calculator;
  calculator.GroupLines(column_list);
  ASSERT_EQ(3u, calculator.RubyLineListForTesting().size());
  EXPECT_TRUE(calculator.RubyLineListForTesting()[0]->IsBaseLevel());

  EXPECT_EQ(RubyLevel{-1}, calculator.RubyLineListForTesting()[1]->Level());
  const ColumnList& under_list =
      calculator.RubyLineListForTesting()[1]->ColumnListForTesting();
  EXPECT_EQ(2u, under_list.size());

  EXPECT_EQ(RubyLevel{1}, calculator.RubyLineListForTesting()[2]->Level());
  const ColumnList& over_list =
      calculator.RubyLineListForTesting()[2]->ColumnListForTesting();
  EXPECT_EQ(2u, over_list.size());
}

TEST(RubyBlockPositionCalculatorTest, GroupLinesAnnotationForAnnotation) {
  ColumnList column_list;
  column_list.push_back(MakeGarbageCollected<LogicalRubyColumn>());
  column_list.back()->start_index = 1;
  column_list.back()->size = 10;
  column_list.back()->ruby_position = RubyPosition::kOver;
  // An annotation for the above annotation line.
  auto* sub_column = MakeGarbageCollected<LogicalRubyColumn>();
  column_list.back()->RubyColumnList().push_back(sub_column);
  sub_column->start_index = 2;
  sub_column->size = 3;
  sub_column->ruby_position = RubyPosition::kUnder;

  RubyBlockPositionCalculator calculator;
  calculator.GroupLines(column_list);
  ASSERT_EQ(3u, calculator.RubyLineListForTesting().size());
  EXPECT_TRUE(calculator.RubyLineListForTesting()[0]->IsBaseLevel());

  EXPECT_EQ(RubyLevel{1}, calculator.RubyLineListForTesting()[1]->Level());
  EXPECT_EQ((RubyLevel{1, -1}),
            calculator.RubyLineListForTesting()[2]->Level());
}

}  // namespace blink
