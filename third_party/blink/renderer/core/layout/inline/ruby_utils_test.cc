// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/ruby_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/inline/fragment_item.h"
#include "third_party/blink/renderer/core/layout/inline/inline_box_state.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/inline/logical_line_item.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

using ColumnList = HeapVector<Member<LogicalRubyColumn>>;
using RubyLevel = RubyBlockPositionCalculator::RubyLevel;

class RubyBlockPositionCalculatorTest : public PageTestBase {};

TEST_F(RubyBlockPositionCalculatorTest, GroupLinesEmpty) {
  RubyBlockPositionCalculator calculator;
  calculator.GroupLines(ColumnList());
  ASSERT_EQ(1u, calculator.RubyLineListForTesting().size());
  EXPECT_TRUE(calculator.RubyLineListForTesting()[0]->IsBaseLevel());
}

TEST_F(RubyBlockPositionCalculatorTest, GroupLinesOneAnnotationLevel) {
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

TEST_F(RubyBlockPositionCalculatorTest, GroupLinesNested) {
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

TEST_F(RubyBlockPositionCalculatorTest, GroupLinesBothSides) {
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

TEST_F(RubyBlockPositionCalculatorTest, GroupLinesAnnotationForAnnotation) {
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

TEST_F(RubyBlockPositionCalculatorTest,
       PlaceLinesTreePlacementDisabledAndEnabled) {
  ColumnList column_list;
  column_list.push_back(MakeGarbageCollected<LogicalRubyColumn>());
  column_list.back()->start_index = 0;
  column_list.back()->size = 0;
  column_list.back()->ruby_position = RubyPosition::kOver;
  column_list.back()->annotation_items =
      MakeGarbageCollected<LogicalLineItems>();

  auto* line_items = MakeGarbageCollected<LogicalLineItems>();

  {
    ScopedTreeRubyPlacementForTest tree_ruby_placement(false);
    RubyBlockPositionCalculator calculator;
    calculator.GroupLines(column_list);
    calculator.PlaceLines(*line_items,
                          FontHeight(LayoutUnit(10), LayoutUnit(4)));
    EXPECT_EQ(2u, calculator.RubyLineListForTesting().size());
  }

  {
    ScopedTreeRubyPlacementForTest tree_ruby_placement(true);
    RubyBlockPositionCalculator calculator;
    calculator.GroupLines(column_list);
    calculator.PlaceLines(*line_items,
                          FontHeight(LayoutUnit(10), LayoutUnit(4)));
    EXPECT_EQ(2u, calculator.RubyLineListForTesting().size());
  }
}

TEST_F(RubyBlockPositionCalculatorTest, PlaceLinesTreePlacementComparison) {
  ColumnList column_list;
  auto* col1 = MakeGarbageCollected<LogicalRubyColumn>();
  col1->start_index = 0;
  col1->size = 0;
  col1->ruby_position = RubyPosition::kOver;
  col1->annotation_items = MakeGarbageCollected<LogicalLineItems>();

  auto* col1_1 = MakeGarbageCollected<LogicalRubyColumn>();
  col1_1->start_index = 0;
  col1_1->size = 0;
  col1_1->ruby_position = RubyPosition::kOver;
  col1_1->annotation_items = MakeGarbageCollected<LogicalLineItems>();
  col1->RubyColumnList().push_back(col1_1);

  auto* col1_under = MakeGarbageCollected<LogicalRubyColumn>();
  col1_under->start_index = 0;
  col1_under->size = 0;
  col1_under->ruby_position = RubyPosition::kUnder;
  col1_under->annotation_items = MakeGarbageCollected<LogicalLineItems>();
  col1->RubyColumnList().push_back(col1_under);

  column_list.push_back(col1);

  auto* line_items = MakeGarbageCollected<LogicalLineItems>();
  FontHeight line_box_metrics(LayoutUnit(12), LayoutUnit(4));

  Vector<LayoutUnit> offsets_disabled;
  Vector<LayoutUnit> offsets_enabled;

  {
    ScopedTreeRubyPlacementForTest tree_ruby_placement(false);
    RubyBlockPositionCalculator calculator;
    calculator.GroupLines(column_list);
    calculator.PlaceLines(*line_items, line_box_metrics);
    for (const auto& line : calculator.RubyLineListForTesting()) {
      offsets_disabled.push_back(line->Offset());
    }
  }

  {
    ScopedTreeRubyPlacementForTest tree_ruby_placement(true);
    RubyBlockPositionCalculator calculator;
    calculator.GroupLines(column_list);
    calculator.PlaceLines(*line_items, line_box_metrics);
    for (const auto& line : calculator.RubyLineListForTesting()) {
      offsets_enabled.push_back(line->Offset());
    }
  }

  EXPECT_EQ(offsets_disabled, offsets_enabled);
}

TEST_F(RubyBlockPositionCalculatorTest, PlaceLinesNestedRubyWithTextEmphasis) {
  ScopedTreeRubyPlacementForTest tree_ruby_placement(true);
  LoadAhem();

  SetBodyContent(R"HTML(
    <style> .e { text-emphasis:'x'; font: 20px/1 Ahem; }</style>
    <div id="target" class="e">
    before
    <ruby>
      <ruby>base<rt class="e">first1<ruby>first2<rt class="e"
       style="text-emphasis-position:under;">nested anno</rt></ruby></rt>
      </ruby>
      <rt class="e" style="text-emphasis-position:under;">second anno</rt>
    </ruby>after</div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  LayoutBlockFlow* target = To<LayoutBlockFlow>(
      GetDocument().getElementById(AtomicString("target"))->GetLayoutObject());
  ASSERT_NE(target, nullptr);

  auto find_fragment_item = [](const LayoutBlockFlow& block,
                               StringView text) -> const FragmentItem* {
    InlineCursor cursor(block);
    for (; cursor; cursor.MoveToNext()) {
      if (cursor.Current()->IsText() && cursor.Current().Text(cursor) == text) {
        return cursor.Current().Item();
      }
    }
    return nullptr;
  };

  const FragmentItem* item_before = find_fragment_item(*target, "after");
  const FragmentItem* item_base = find_fragment_item(*target, "base");
  const FragmentItem* item_first1 = find_fragment_item(*target, "first1");
  const FragmentItem* item_first = find_fragment_item(*target, "first2");
  const FragmentItem* item_nested = find_fragment_item(*target, "nested anno");
  const FragmentItem* item_second = find_fragment_item(*target, "second anno");
  ASSERT_NE(item_before, nullptr);
  ASSERT_NE(item_base, nullptr);
  ASSERT_NE(item_first1, nullptr);
  ASSERT_NE(item_first, nullptr);
  ASSERT_NE(item_nested, nullptr);
  ASSERT_NE(item_second, nullptr);

  RubyBlockPositionCalculator calculator;

  LayoutUnit base_top = item_base->OffsetInContainerFragment().top;

  LayoutUnit first_top = item_first->OffsetInContainerFragment().top;
  LayoutUnit first_bottom = first_top + item_first->Size().height;

  LayoutUnit nested_top = item_nested->OffsetInContainerFragment().top;
  LayoutUnit nested_bottom = nested_top + item_nested->Size().height;

  LayoutUnit second_top = item_second->OffsetInContainerFragment().top;
  LayoutUnit second_bottom = second_top + item_second->Size().height;

  // Emphasis mark height is 50% of font-size (20px * 0.5 = 10px).
  constexpr LayoutUnit kEmphasisHeight = LayoutUnit(10);

  // Emphasis mark boundaries:
  EXPECT_EQ(LayoutUnit(), item_before->AnnotationMetrics().ascent);
  //  90 = 20 (the first annotation) + 10 (emphasis of the nested annotation)
  //      + 20 (the nested annotation) + 10 (emphasis of the first annotation)
  //      + 10 (emphasis of the second annotation) + 20 (the second annotation).
  EXPECT_EQ(LayoutUnit(90), item_base->AnnotationMetrics().ascent);
  LayoutUnit base_over_emphasis_bottom =
      base_top - item_base->AnnotationMetrics().ascent;

  // "first1" over emphasis mark is on the top of first1.
  EXPECT_EQ(LayoutUnit(), item_first1->AnnotationMetrics().ascent);
  // "first2" over emphasis mark is above first_top.
  //  30 = 10 (emphasis of the nested annotation) + 20 (the nested annotation)
  EXPECT_EQ(LayoutUnit(30), item_first->AnnotationMetrics().ascent);
  LayoutUnit first_over_emphasis_bottom =
      first_top - item_first->AnnotationMetrics().ascent;
  LayoutUnit first_over_emphasis_top =
      first_over_emphasis_bottom - kEmphasisHeight;

  // "nested anno" under emphasis mark is below nested_bottom.
  LayoutUnit nested_under_emphasis_top = nested_bottom;
  LayoutUnit nested_under_emphasis_bottom =
      nested_under_emphasis_top + kEmphasisHeight;

  // "second anno" under emphasis mark is below second_bottom.
  LayoutUnit second_under_emphasis_top = second_bottom;
  LayoutUnit second_under_emphasis_bottom =
      second_under_emphasis_top + kEmphasisHeight;

  // "base" and "first1first2" are touching
  EXPECT_EQ(base_top, first_bottom);

  // "first1first2" and "nested anno"'s under emphasis are touching
  EXPECT_EQ(first_top, nested_under_emphasis_bottom);

  // "first1first2"'s over emphasis is touching on top of "nested anno"
  EXPECT_EQ(first_over_emphasis_bottom, nested_top);

  // Above that over emphasis, "second anno"'s under emphasis is touching
  EXPECT_EQ(first_over_emphasis_top, second_under_emphasis_bottom);

  // "second anno" touches its under emphasis below, and "base"'s over
  // emphasis above
  EXPECT_EQ(second_top, base_over_emphasis_bottom);
}

}  // namespace blink
