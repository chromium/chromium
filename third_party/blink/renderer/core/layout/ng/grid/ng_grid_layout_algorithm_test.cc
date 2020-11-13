// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_base_layout_algorithm_test.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {
namespace {
#define EXPECT_RANGE(expected_start, expected_count, iterator)              \
  EXPECT_EQ(expected_count, iterator.RepeatCount());                        \
  EXPECT_EQ(expected_start, iterator.RangeTrackStart());                    \
  EXPECT_EQ(expected_start + expected_count - 1, iterator.RangeTrackEnd()); \
  EXPECT_FALSE(iterator.IsRangeCollapsed());
#define EXPECT_COLLAPSED_RANGE(expected_start, expected_count, iterator)    \
  EXPECT_EQ(expected_start, iterator.RangeTrackStart());                    \
  EXPECT_EQ(expected_count, iterator.RepeatCount());                        \
  EXPECT_EQ(expected_start + expected_count - 1, iterator.RangeTrackEnd()); \
  EXPECT_TRUE(iterator.IsRangeCollapsed());
#define EXPECT_GRID_AREA(area, expected_column_start, expected_column_end, \
                         expected_row_start, expected_row_end)             \
  EXPECT_EQ(area.columns.StartLine(), expected_column_start);              \
  EXPECT_EQ(area.columns.EndLine(), expected_column_end);                  \
  EXPECT_EQ(area.rows.StartLine(), expected_row_start);                    \
  EXPECT_EQ(area.rows.EndLine(), expected_row_end);
}  // namespace
class NGGridLayoutAlgorithmTest
    : public NGBaseLayoutAlgorithmTest,
      private ScopedLayoutNGGridForTest,
      private ScopedLayoutNGBlockFragmentationForTest {
 protected:
  NGGridLayoutAlgorithmTest()
      : ScopedLayoutNGGridForTest(true),
        ScopedLayoutNGBlockFragmentationForTest(true) {}

  void SetUp() override {
    NGBaseLayoutAlgorithmTest::SetUp();
    style_ = ComputedStyle::Create();
  }

  // Helper methods to access private data on NGGridLayoutAlgorithm. This class
  // is a friend of NGGridLayoutAlgorithm but the individual tests are not.
  wtf_size_t GridItemCount(const NGGridLayoutAlgorithm& algorithm) {
    return algorithm.grid_items_.size();
  }

  Vector<LayoutUnit> GridItemInlineSizes(
      const NGGridLayoutAlgorithm& algorithm) {
    Vector<LayoutUnit> results;
    for (const auto& item : algorithm.grid_items_) {
      results.push_back(item.inline_size);
    }
    return results;
  }

  Vector<LayoutUnit> GridItemInlineMarginSum(
      const NGGridLayoutAlgorithm& algorithm) {
    Vector<LayoutUnit> results;
    for (const auto& item : algorithm.grid_items_) {
      results.push_back(item.margins.InlineSum());
    }
    return results;
  }

  Vector<MinMaxSizes> GridItemMinMaxSizes(
      const NGGridLayoutAlgorithm& algorithm) {
    Vector<MinMaxSizes> results;
    for (const auto& item : algorithm.grid_items_) {
      results.push_back(item.min_max_sizes);
    }
    return results;
  }

  Vector<GridArea> GridItemGridAreas(const NGGridLayoutAlgorithm& algorithm) {
    Vector<GridArea> results;
    for (const auto& item : algorithm.grid_items_) {
      results.push_back(item.resolved_position);
    }
    return results;
  }

  void DetermineGridItemsSpanningIntrinsicOrFlexTracks(
      NGGridLayoutAlgorithm& algorithm,
      GridTrackSizingDirection track_direction) {
    algorithm.DetermineGridItemsSpanningIntrinsicOrFlexTracks(track_direction);
  }

  Vector<wtf_size_t> GridItemsSpanningIntrinsicTrack(
      const NGGridLayoutAlgorithm& algorithm) {
    Vector<wtf_size_t> results;
    for (wtf_size_t i = 0; i < algorithm.grid_items_.size(); ++i) {
      if (algorithm.grid_items_[i].is_spanning_intrinsic_track)
        results.push_back(i);
    }
    return results;
  }

  Vector<wtf_size_t> GridItemsSpanningFlexTrack(
      const NGGridLayoutAlgorithm& algorithm) {
    Vector<wtf_size_t> results;
    for (wtf_size_t i = 0; i < algorithm.grid_items_.size(); ++i) {
      if (algorithm.grid_items_[i].is_spanning_flex_track)
        results.push_back(i);
    }
    return results;
  }

  void SetAutoTrackRepeat(NGGridLayoutAlgorithm& algorithm,
                          wtf_size_t auto_column,
                          wtf_size_t auto_row) {
    algorithm.SetAutomaticTrackRepetitionsForTesting(auto_column, auto_row);
  }

  Vector<LayoutUnit> BaseSizes(NGGridLayoutAlgorithm& algorithm,
                               GridTrackSizingDirection track_direction) {
    NGGridLayoutAlgorithmTrackCollection& collection =
        (track_direction == kForColumns)
            ? algorithm.algorithm_column_track_collection_
            : algorithm.algorithm_row_track_collection_;

    Vector<LayoutUnit> base_sizes;
    for (auto set_iterator = collection.GetSetIterator();
         !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
      base_sizes.push_back(set_iterator.CurrentSet().BaseSize());
    }
    return base_sizes;
  }

  Vector<LayoutUnit> GrowthLimits(NGGridLayoutAlgorithm& algorithm,
                                  GridTrackSizingDirection track_direction) {
    NGGridLayoutAlgorithmTrackCollection& collection =
        (track_direction == kForColumns)
            ? algorithm.algorithm_column_track_collection_
            : algorithm.algorithm_row_track_collection_;

    Vector<LayoutUnit> growth_limits;
    for (auto set_iterator = collection.GetSetIterator();
         !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
      growth_limits.push_back(set_iterator.CurrentSet().GrowthLimit());
    }
    return growth_limits;
  }

  scoped_refptr<const NGPhysicalBoxFragment> RunBlockLayoutAlgorithm(
      Element* element) {
    NGBlockNode container(element->GetLayoutBox());
    NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
        {WritingMode::kHorizontalTb, TextDirection::kLtr},
        LogicalSize(LayoutUnit(1000), kIndefiniteSize));
    return NGBaseLayoutAlgorithmTest::RunBlockLayoutAlgorithm(container, space);
  }

  String DumpFragmentTree(Element* element) {
    auto fragment = RunBlockLayoutAlgorithm(element);
    return DumpFragmentTree(fragment.get());
  }

  String DumpFragmentTree(const blink::NGPhysicalBoxFragment* fragment) {
    NGPhysicalFragment::DumpFlags flags =
        NGPhysicalFragment::DumpHeaderText | NGPhysicalFragment::DumpSubtree |
        NGPhysicalFragment::DumpIndentation | NGPhysicalFragment::DumpOffset |
        NGPhysicalFragment::DumpSize;

    return fragment->DumpFragmentTree(flags);
  }

  scoped_refptr<ComputedStyle> style_;
};

TEST_F(NGGridLayoutAlgorithmTest, NGGridLayoutAlgorithmMeasuring) {
  if (!RuntimeEnabledFeatures::LayoutNGGridEnabled())
    return;

  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    body {
      font: 10px/1 Ahem;
    }
    #grid1 {
      display: grid;
      width: 200px;
      height: 200px;
      grid-template-columns: min-content min-content min-content;
      grid-template-rows: 100px 100px 100px;
    }
    /*  Basic fixed width specified, evaluates to 150px (50px width + 50px
        margin-left + 50px margin-right). */
    #cell1 {
      grid-column: 1;
      grid-row: 1;
      width: 50px;
      height: 50px;
      margin: 50px;
    }
    /*  100px content, with margin/border/padding. Evaluates to 146px
        (100px width + 15px margin-left + 15px margin-righ + 5px border-left +
        5px border-right + 3px padding-left + 3px padding-right). */
    #cell2 {
      grid-column: 2;
      grid-row: 1;
      min-width: 50px;
      height: 100px;
      border: 5px solid black;
      margin: 15px;
      padding: 3px;
    }
    /*  % resolution, needs another pass for the real computed value. For now,
        this is evaluated based on the 200px grid content, so it evaluates
        to the (currently incorrect) value of 50% of 200px = 100px. */
    #cell3 {
      grid-column: 3;
      grid-row: 1;
      width: 50%;
      height: 50%;
    }
    /*  'auto' sizing, with fixed 100px child, evaluates to 100px. */
    #cell4 {
      grid-column: 1;
      grid-row: 2;
      width: auto;
      height: auto;
    }
    /*  'auto' sizing replaced content, evaluates to default replaced width of
        300px. */
    #cell5 {
      grid-column: 2;
      grid-row: 2;
      width: auto;
      height: auto;
    }
    /*  'auto' sizing replaced content, max-width restricts 300px size to
          evaluate to 100px. */
    #cell6 {
      grid-column: 3;
      grid-row: 2;
      width: auto;
      height: auto;
      max-width: 100px;
    }
    /*  'auto' sizing replaced content, min-width expands to 400px, which
        in a total offset size of 410 (400px + 5px margin-left + 5px
        margin-right). */
    #cell7 {
      grid-column: 1;
      grid-row: 3;
      width: auto;
      height: auto;
      margin: 5px;
      min-width: 400px;
    }
    /*  'auto' sizing with 100px content, min-width and margin evaluates to
        100px + 50px margin-left + 50px margin-right = 200px. */
    #cell8 {
      grid-column: 2;
      grid-row: 3;
      width: auto;
      height: auto;
      margin: 50px;
      min-width: 100px;
    }
    /* 'auto' sizing with text content and vertical writing mode. In horizontal
       writing-modes, this would be an expected inline size of 40px (at 10px
       per character), but since it's set to a vertical writing mode, the
       expected width is 10px (at 10px per character). */
    #cell9 {
      grid-column: 3;
      grid-row: 3;
      width: auto;
      height: auto;
      writing-mode: vertical-lr;
    }
    #block {
      width: 100px;
      height: 100px;
    }
    </style>
    <div id="grid1">
      <div id="cell1">Cell 1</div>
      <div id="cell2"><div id="block"></div></div>
      <div id="cell3">Cell 3</div>
      <div id="cell4"><div id="block"></div></div>
      <svg id="cell5">
        <rect width="100%" height="100%" fill="blue" />
      </svg>
      <svg id="cell6">
        <rect width="100%" height="100%" fill="blue" />
      </svg>
      <svg id="cell7">
        <rect width="100%" height="100%" fill="blue" />
      </svg>
      <div id="cell8"><div id="block"></div></div>
      <div id="cell9">Text</div>
    </div>
  )HTML");

  NGBlockNode node(GetLayoutBoxByElementId("grid1"));

  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(200), LayoutUnit(200)),
      /* stretch_inline_size_if_auto */ true,
      /* is_new_formatting_context */ true);

  NGFragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, node);

  NGGridLayoutAlgorithm algorithm({node, fragment_geometry, space});
  EXPECT_EQ(GridItemCount(algorithm), 0U);
  algorithm.Layout();
  EXPECT_EQ(GridItemCount(algorithm), 9U);

  Vector<LayoutUnit> actual_inline_sizes = GridItemInlineSizes(algorithm);
  EXPECT_EQ(GridItemCount(algorithm), actual_inline_sizes.size());

  LayoutUnit expected_inline_sizes[] = {
      LayoutUnit(50),  LayoutUnit(116), LayoutUnit(100),
      LayoutUnit(100), LayoutUnit(300), LayoutUnit(100),
      LayoutUnit(400), LayoutUnit(100), LayoutUnit(10)};

  Vector<LayoutUnit> actual_inline_margin_sums =
      GridItemInlineMarginSum(algorithm);
  EXPECT_EQ(GridItemCount(algorithm), actual_inline_margin_sums.size());

  LayoutUnit expected_inline_margin_sums[] = {
      LayoutUnit(100), LayoutUnit(30),  LayoutUnit(0),
      LayoutUnit(0),   LayoutUnit(0),   LayoutUnit(0),
      LayoutUnit(10),  LayoutUnit(100), LayoutUnit(0)};

  Vector<MinMaxSizes> actual_min_max_sizes = GridItemMinMaxSizes(algorithm);
  EXPECT_EQ(GridItemCount(algorithm), actual_min_max_sizes.size());

  MinMaxSizes expected_min_max_sizes[] = {
      {LayoutUnit(40), LayoutUnit(60)},   {LayoutUnit(116), LayoutUnit(116)},
      {LayoutUnit(40), LayoutUnit(60)},   {LayoutUnit(100), LayoutUnit(100)},
      {LayoutUnit(300), LayoutUnit(300)}, {LayoutUnit(300), LayoutUnit(300)},
      {LayoutUnit(300), LayoutUnit(300)}, {LayoutUnit(100), LayoutUnit(100)},
      {LayoutUnit(40), LayoutUnit(40)}};

  for (size_t i = 0; i < GridItemCount(algorithm); ++i) {
    EXPECT_EQ(actual_inline_sizes[i], expected_inline_sizes[i])
        << " index: " << i;
    EXPECT_EQ(actual_inline_margin_sums[i], expected_inline_margin_sums[i])
        << " index: " << i;
    EXPECT_EQ(actual_min_max_sizes[i].min_size,
              expected_min_max_sizes[i].min_size)
        << " index: " << i;
    EXPECT_EQ(actual_min_max_sizes[i].max_size,
              expected_min_max_sizes[i].max_size)
        << " index: " << i;
  }
}

TEST_F(NGGridLayoutAlgorithmTest, NGGridLayoutAlgorithmRanges) {
  if (!RuntimeEnabledFeatures::LayoutNGGridEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
    #grid1 {
      display: grid;
      grid-template-columns: repeat(2, 100px 100px 200px 200px);
      grid-template-rows: repeat(1000, 100px);
    }
    </style>
    <div id="grid1">
      <div id="cell1">Cell 1</div>
      <div id="cell2">Cell 2</div>
      <div id="cell3">Cell 3</div>
      <div id="cell4">Cell 4</div>
    </div>
  )HTML");

  NGBlockNode node(GetLayoutBoxByElementId("grid1"));

  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), LayoutUnit(100)),
      /* stretch_inline_size_if_auto */ true,
      /* is_new_formatting_context */ true);

  NGFragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, node);

  NGGridLayoutAlgorithm algorithm({node, fragment_geometry, space});
  EXPECT_EQ(GridItemCount(algorithm), 0U);
  algorithm.Layout();
  EXPECT_EQ(GridItemCount(algorithm), 4U);

  NGGridTrackCollectionBase::RangeRepeatIterator row_iterator(
      &algorithm.RowTrackCollection(), 0u);
  EXPECT_RANGE(0u, 1u, row_iterator);
  EXPECT_TRUE(row_iterator.MoveToNextRange());
  EXPECT_RANGE(1u, 999u, row_iterator);
  EXPECT_FALSE(row_iterator.MoveToNextRange());

  NGGridTrackCollectionBase::RangeRepeatIterator column_iterator(
      &algorithm.ColumnTrackCollection(), 0u);
  EXPECT_RANGE(0u, 1u, column_iterator);
  EXPECT_TRUE(column_iterator.MoveToNextRange());
  EXPECT_RANGE(1u, 1u, column_iterator);
  EXPECT_TRUE(column_iterator.MoveToNextRange());
  EXPECT_RANGE(2u, 1u, column_iterator);
  EXPECT_TRUE(column_iterator.MoveToNextRange());
  EXPECT_RANGE(3u, 1u, column_iterator);
  EXPECT_TRUE(column_iterator.MoveToNextRange());
  EXPECT_RANGE(4u, 4u, column_iterator);
  EXPECT_FALSE(column_iterator.MoveToNextRange());
}

TEST_F(NGGridLayoutAlgorithmTest, NGGridLayoutAlgorithmRangesWithAutoRepeater) {
  if (!RuntimeEnabledFeatures::LayoutNGGridEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
    #grid1 {
      display: grid;
      grid-template-columns: 5px repeat(auto-fit, 150px) repeat(3, 10px) 10px 10px;
      grid-template-rows: repeat(20, 100px) 10px 10px;
    }
    </style>
    <div id="grid1">
      <div id="cell1">Cell 1</div>
      <div id="cell2">Cell 2</div>
      <div id="cell3">Cell 3</div>
      <div id="cell4">Cell 4</div>
    </div>
  )HTML");

  NGBlockNode node(GetLayoutBoxByElementId("grid1"));

  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), LayoutUnit(100)),
      /* stretch_inline_size_if_auto */ true,
      /* is_new_formatting_context */ true);

  NGFragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, node);

  NGGridLayoutAlgorithm algorithm({node, fragment_geometry, space});
  EXPECT_EQ(GridItemCount(algorithm), 0U);
  SetAutoTrackRepeat(algorithm, 3, 3);
  algorithm.Layout();
  EXPECT_EQ(GridItemCount(algorithm), 4U);

  NGGridTrackCollectionBase::RangeRepeatIterator row_iterator(
      &algorithm.RowTrackCollection(), 0u);
  EXPECT_RANGE(0u, 1u, row_iterator);
  EXPECT_TRUE(row_iterator.MoveToNextRange());
  EXPECT_RANGE(1u, 19u, row_iterator);
  EXPECT_TRUE(row_iterator.MoveToNextRange());

  EXPECT_RANGE(20u, 1u, row_iterator);
  EXPECT_TRUE(row_iterator.MoveToNextRange());
  EXPECT_RANGE(21u, 1u, row_iterator);
  EXPECT_FALSE(row_iterator.MoveToNextRange());

  NGGridTrackCollectionBase::RangeRepeatIterator column_iterator(
      &algorithm.ColumnTrackCollection(), 0u);

  EXPECT_RANGE(0u, 1u, column_iterator);
  EXPECT_TRUE(column_iterator.MoveToNextRange());

  EXPECT_RANGE(1u, 1u, column_iterator);
  EXPECT_TRUE(column_iterator.MoveToNextRange());

  EXPECT_RANGE(2u, 1u, column_iterator);
  EXPECT_TRUE(column_iterator.MoveToNextRange());

  EXPECT_RANGE(3u, 1u, column_iterator);
  EXPECT_TRUE(column_iterator.MoveToNextRange());
  EXPECT_RANGE(4u, 1u, column_iterator);
  EXPECT_TRUE(column_iterator.MoveToNextRange());
  EXPECT_RANGE(5u, 1u, column_iterator);
  EXPECT_FALSE(column_iterator.MoveToNextRange());
}

TEST_F(NGGridLayoutAlgorithmTest, NGGridLayoutAlgorithmRangesImplicit) {
  if (!RuntimeEnabledFeatures::LayoutNGGridEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
    #grid1 {
      display: grid;
    }
    #cell1 {
      grid-column: 1 / 2;
      grid-row: 1 / 2;
      width: 50px;
    }
    #cell2 {
      grid-column: 2 / 3;
      grid-row: 1 / 2;
      width: 50px;
    }
    #cell3 {
      grid-column: 1 / 2;
      grid-row: 2 / 3;
      width: 50px;
    }
    #cell4 {
      grid-column: 2 / 5;
      grid-row: 2 / 3;
      width: 50px;
    }
    </style>
    <div id="grid1">
      <div id="cell1">Cell 1</div>
      <div id="cell2">Cell 2</div>
      <div id="cell3">Cell 3</div>
      <div id="cell4">Cell 4</div>
    </div>
  )HTML");

  NGBlockNode node(GetLayoutBoxByElementId("grid1"));

  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), LayoutUnit(100)),
      /* stretch_inline_size_if_auto */ true,
      /* is_new_formatting_context */ true);

  NGFragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, node);

  NGGridLayoutAlgorithm algorithm({node, fragment_geometry, space});
  EXPECT_EQ(GridItemCount(algorithm), 0U);
  algorithm.Layout();
  EXPECT_EQ(GridItemCount(algorithm), 4U);

  NGGridTrackCollectionBase::RangeRepeatIterator column_iterator(
      &algorithm.ColumnTrackCollection(), 0u);
  EXPECT_RANGE(0u, 1u, column_iterator);
  EXPECT_TRUE(column_iterator.MoveToNextRange());

  EXPECT_RANGE(1u, 1u, column_iterator);
  EXPECT_TRUE(column_iterator.MoveToNextRange());

  EXPECT_RANGE(2u, 2u, column_iterator);
  EXPECT_FALSE(column_iterator.MoveToNextRange());

  NGGridTrackCollectionBase::RangeRepeatIterator row_iterator(
      &algorithm.RowTrackCollection(), 0u);
  EXPECT_RANGE(0u, 1u, row_iterator);
  EXPECT_TRUE(row_iterator.MoveToNextRange());

  EXPECT_RANGE(1u, 1u, row_iterator);
  EXPECT_FALSE(row_iterator.MoveToNextRange());
}

TEST_F(NGGridLayoutAlgorithmTest,
       NGGridLayoutAlgorithmRangesImplicitAutoColumns) {
  if (!RuntimeEnabledFeatures::LayoutNGGridEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
    #grid1 {
      display: grid;
    }
    #cell1 {
      grid-row: 1 / 2;
      width: 50px;
    }
    #cell2 {
      grid-row: 1 / 2;
      width: 50px;
    }
    #cell3 {
      grid-row: 2 / 3;
      width: 50px;
    }
    #cell4 {
      grid-row: 2 / 3;
      width: 50px;
    }
    </style>
    <div id="grid1">
      <div id="cell1">Cell 1</div>
      <div id="cell2">Cell 2</div>
      <div id="cell3">Cell 3</div>
      <div id="cell4">Cell 4</div>
    </div>
  )HTML");

  NGBlockNode node(GetLayoutBoxByElementId("grid1"));

  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), LayoutUnit(100)),
      /* stretch_inline_size_if_auto */ true,
      /* is_new_formatting_context */ true);

  NGFragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, node);

  NGGridLayoutAlgorithm algorithm({node, fragment_geometry, space});
  EXPECT_EQ(GridItemCount(algorithm), 0U);
  SetAutoTrackRepeat(algorithm, 0, 0);
  algorithm.Layout();
  EXPECT_EQ(GridItemCount(algorithm), 4U);

  NGGridTrackCollectionBase::RangeRepeatIterator column_iterator(
      &algorithm.ColumnTrackCollection(), 0u);
  EXPECT_RANGE(0u, 1u, column_iterator);
  EXPECT_TRUE(column_iterator.MoveToNextRange());

  EXPECT_RANGE(1u, 1u, column_iterator);
  EXPECT_FALSE(column_iterator.MoveToNextRange());

  NGGridTrackCollectionBase::RangeRepeatIterator row_iterator(
      &algorithm.RowTrackCollection(), 0u);
  EXPECT_RANGE(0u, 1u, row_iterator);
  EXPECT_TRUE(row_iterator.MoveToNextRange());

  EXPECT_RANGE(1u, 1u, row_iterator);
  EXPECT_FALSE(row_iterator.MoveToNextRange());
}

TEST_F(NGGridLayoutAlgorithmTest, NGGridLayoutAlgorithmRangesImplicitAutoRows) {
  if (!RuntimeEnabledFeatures::LayoutNGGridEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
    #grid1 {
      display: grid;
    }
    #cell1 {
      grid-column: 1 / 2;
      width: 50px;
    }
    #cell2 {
      grid-column: 2 / 3;
      width: 50px;
    }
    #cell3 {
      grid-column: 1 / 2;
      width: 50px;
    }
    #cell4 {
      grid-column: 2 / 5;
      width: 50px;
    }
    </style>
    <div id="grid1">
      <div id="cell1">Cell 1</div>
      <div id="cell2">Cell 2</div>
      <div id="cell3">Cell 3</div>
      <div id="cell4">Cell 4</div>
    </div>
  )HTML");

  NGBlockNode node(GetLayoutBoxByElementId("grid1"));

  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), LayoutUnit(100)),
      /* stretch_inline_size_if_auto */ true,
      /* is_new_formatting_context */ true);

  NGFragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, node);

  NGGridLayoutAlgorithm algorithm({node, fragment_geometry, space});
  EXPECT_EQ(GridItemCount(algorithm), 0U);
  SetAutoTrackRepeat(algorithm, 0, 0);
  algorithm.Layout();
  EXPECT_EQ(GridItemCount(algorithm), 4U);

  NGGridTrackCollectionBase::RangeRepeatIterator column_iterator(
      &algorithm.ColumnTrackCollection(), 0u);
  EXPECT_RANGE(0u, 1u, column_iterator);
  EXPECT_TRUE(column_iterator.MoveToNextRange());

  EXPECT_RANGE(1u, 1u, column_iterator);
  EXPECT_TRUE(column_iterator.MoveToNextRange());

  EXPECT_RANGE(2u, 2u, column_iterator);
  EXPECT_FALSE(column_iterator.MoveToNextRange());

  NGGridTrackCollectionBase::RangeRepeatIterator row_iterator(
      &algorithm.RowTrackCollection(), 0u);
  EXPECT_RANGE(0u, 1u, row_iterator);
  EXPECT_TRUE(row_iterator.MoveToNextRange());

  EXPECT_RANGE(1u, 1u, row_iterator);
  EXPECT_FALSE(row_iterator.MoveToNextRange());
}

TEST_F(NGGridLayoutAlgorithmTest, NGGridLayoutAlgorithmRangesImplicitMixed) {
  if (!RuntimeEnabledFeatures::LayoutNGGridEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
    #grid1 {
      display: grid;
    }
    #cell1 {
      grid-column: 2;
      grid-row: 1;
    }
    </style>
    <div id="grid1">
      <div id="cell1">Cell 1</div>
      <div id="cell2">Cell 2</div>
      <div id="cell3">Cell 3</div>
      <div id="cell4">Cell 4</div>
      <div id="cell4">Cell 5</div>
    </div>
  )HTML");

  NGBlockNode node(GetLayoutBoxByElementId("grid1"));

  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), LayoutUnit(100)),
      /* stretch_inline_size_if_auto */ true,
      /* is_new_formatting_context */ true);

  NGFragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, node);

  NGGridLayoutAlgorithm algorithm({node, fragment_geometry, space});
  EXPECT_EQ(GridItemCount(algorithm), 0U);
  SetAutoTrackRepeat(algorithm, 0, 0);
  algorithm.Layout();
  EXPECT_EQ(GridItemCount(algorithm), 5U);

  NGGridTrackCollectionBase::RangeRepeatIterator column_iterator(
      &algorithm.ColumnTrackCollection(), 0u);
  EXPECT_RANGE(0u, 1u, column_iterator);
  EXPECT_TRUE(column_iterator.MoveToNextRange());

  EXPECT_RANGE(1u, 1u, column_iterator);
  EXPECT_FALSE(column_iterator.MoveToNextRange());

  NGGridTrackCollectionBase::RangeRepeatIterator row_iterator(
      &algorithm.RowTrackCollection(), 0u);
  EXPECT_RANGE(0u, 1u, row_iterator);
  EXPECT_TRUE(row_iterator.MoveToNextRange());

  EXPECT_RANGE(1u, 1u, row_iterator);
  EXPECT_TRUE(row_iterator.MoveToNextRange());

  EXPECT_RANGE(2u, 1u, row_iterator);
  EXPECT_FALSE(row_iterator.MoveToNextRange());
}

TEST_F(NGGridLayoutAlgorithmTest, NGGridLayoutAlgorithmAutoGridPositions) {
  if (!RuntimeEnabledFeatures::LayoutNGGridEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
  <style>
      body {
        font: 10px/1 Ahem;
      }
      #grid {
        display: grid;
        width: 400px;
        height: 400px;
        grid-template-columns: 100px 100px;
        grid-template-rows: 100px 100px;
      }
      .grid_item1 {
        display: block;
        width: 100px;
        height: 100px;
        grid-row: 2;
        grid-column: 2;
      }
      .grid_item2 {
        display: block;
        width: 90px;
        height: 90px;
      }
      .grid_item3 {
        display: block;
        width: 80px;
        height: 80px;
      }
      .grid_item4 {
        display: block;
        width: 70px;
        height: 70px;
        grid-row: 1;
        grid-column: 1;
      }
    </style>
    <div id="wrapper">
      <div id="grid">
        <div class="grid_item1">1</div>
        <div class="grid_item2">2</div>
        <div class="grid_item3">3</div>
        <div class="grid_item4">4</div>
      </div>
    </div>

  )HTML");

  NGBlockNode node(GetLayoutBoxByElementId("grid"));

  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(500), LayoutUnit(500)),
      /* stretch_inline_size_if_auto */ true,
      /* is_new_formatting_context */ true);

  NGFragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, node);

  NGGridLayoutAlgorithm algorithm({node, fragment_geometry, space});
  EXPECT_EQ(GridItemCount(algorithm), 0U);
  algorithm.Layout();
  EXPECT_EQ(GridItemCount(algorithm), 4U);

  Vector<GridArea> grid_positions = GridItemGridAreas(algorithm);
  ASSERT_EQ(grid_positions.size(), 4U);

  EXPECT_GRID_AREA(grid_positions[0], 1U, 2U, 1U, 2U);
  EXPECT_GRID_AREA(grid_positions[1], 1U, 2U, 0U, 1U);
  EXPECT_GRID_AREA(grid_positions[2], 0U, 1U, 1U, 2U);
  EXPECT_GRID_AREA(grid_positions[3], 0U, 1U, 0U, 1U);
}

TEST_F(NGGridLayoutAlgorithmTest, NGGridLayoutAlgorithmAutoDense) {
  if (!RuntimeEnabledFeatures::LayoutNGGridEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
      #grid {
        display: grid;
        grid-auto-flow: dense;
        grid-template-columns: repeat(10, 40px);
        grid-template-rows: repeat(10, 40px);
      }

      .item {
        display: block;
        font: 40px/1 Ahem;
        border: 2px solid black;
        width: 100%;
        height: 100%;
      }
      .auto-one {
        border: 2px solid red;
      }
      .auto-both {
        border: 2px solid blue;
      }
      .a {
        grid-column: 1 / 3;
        grid-row: 1 / 3;
      }
      .b {
        grid-column: 4 / 8;
        grid-row: 1 / 4;
      }
      .c {
        grid-column: 9 / 11;
        grid-row: 1 / 2;
      }
      .d {
        grid-column: 1 / 3;
        grid-row: 4 / 6;
      }
      .e {
        grid-column: 4 / 6;
        grid-row: 4 / 5;
      }
      .f {
        grid-column: 7 / 10;
        grid-row: 5 / 6;
      }
      .g {
        grid-column: 6 / 7;
        grid-row: 8 / 9;
      }
      .h {
        grid-column: 6 / 8;
        grid-row-end: span 2;
      }
      .i {
        grid-row: 4 / 5;
        grid-column-end: span 2;
      }
      .j {
        grid-column: 6 / 7;
      }
      .u {
        grid-column-end: span 5;
        grid-row-end: span 5;
      }
      .v {
        grid-row-end: span 9;
      }
      .w {
        grid-column-end: span 2;
        grid-row-end: span 3;
      }
      .x {
        grid-row-end: span 5;
      }

    </style>
    <div id="wrapper">
      <div id="grid">
        <div class="item a">a</div>
        <div class="item b">b</div>
        <div class="item c">c</div>
        <div class="item d">d</div>
        <div class="item e">e</div>
        <div class="item f">f</div>
        <div class="item g">g</div>
        <div class="auto-one item h">h</div>
        <div class="auto-one item i">i</div>
        <div class="auto-one item j">j</div>
        <div class="auto-both item u">u</div>
        <div class="auto-both item v">v</div>
        <div class="auto-both item w">w</div>
        <div class="auto-both item x">x</div>
        <div class="auto-both item y">y</div>
        <div class="auto-both item z">z</div>
      </div>
    </div>

  )HTML");

  NGBlockNode node(GetLayoutBoxByElementId("grid"));

  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(500), LayoutUnit(500)),
      /* stretch_inline_size_if_auto */ true,
      /* is_new_formatting_context */ true);

  NGFragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, node);

  NGGridLayoutAlgorithm algorithm({node, fragment_geometry, space});
  EXPECT_EQ(GridItemCount(algorithm), 0U);
  algorithm.Layout();
  EXPECT_EQ(GridItemCount(algorithm), 16U);

  Vector<GridArea> grid_positions = GridItemGridAreas(algorithm);
  ASSERT_EQ(grid_positions.size(), 16U);

  // Expected placements:
  //   0 1 2 3 4 5 6 7 8 9
  // 0 a a x b b b b y c c
  // 1 a a x b b b b w w v
  // 2 z * x b b b b w w v
  // 3 d d x e e i i w w v
  // 4 d d x * * j f f f v
  // 5 u u u u u h h * * v
  // 6 u u u u u h h * * v
  // 7 u u u u u g * * * v
  // 8 u u u u u * * * * v
  // 9 u u u u u * * * * v

  // Fixed positions: a-g
  EXPECT_GRID_AREA(grid_positions[0], 0U, 2U, 0U, 2U);
  EXPECT_GRID_AREA(grid_positions[1], 3U, 7U, 0U, 3U);
  EXPECT_GRID_AREA(grid_positions[2], 8U, 10U, 0U, 1U);
  EXPECT_GRID_AREA(grid_positions[3], 0U, 2U, 3U, 5U);
  EXPECT_GRID_AREA(grid_positions[4], 3U, 5U, 3U, 4U);
  EXPECT_GRID_AREA(grid_positions[5], 6U, 9U, 4U, 5U);
  EXPECT_GRID_AREA(grid_positions[6], 5U, 6U, 7U, 8U);

  // Fixed on single axis positions: h-j
  EXPECT_GRID_AREA(grid_positions[7], 5U, 7U, 5U, 7U);
  EXPECT_GRID_AREA(grid_positions[8], 5U, 7U, 3U, 4U);
  EXPECT_GRID_AREA(grid_positions[9], 5U, 6U, 4U, 5U);

  // Auto on both axis: u-z
  EXPECT_GRID_AREA(grid_positions[10], 0U, 5U, 5U, 10U);
  EXPECT_GRID_AREA(grid_positions[11], 9U, 10U, 1U, 10U);
  EXPECT_GRID_AREA(grid_positions[12], 7U, 9U, 1U, 4U);
  EXPECT_GRID_AREA(grid_positions[13], 2U, 3U, 0U, 5U);
  EXPECT_GRID_AREA(grid_positions[14], 7U, 8U, 0U, 1U);
  EXPECT_GRID_AREA(grid_positions[15], 0U, 1U, 2U, 3U);
}

TEST_F(NGGridLayoutAlgorithmTest, NGGridLayoutAlgorithmGridPositions) {
  if (!RuntimeEnabledFeatures::LayoutNGGridEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
      #grid {
        display: grid;
        height: 200px;
        grid-template-columns: 200px;
        grid-template-rows: repeat(6, 1fr);
      }

      #item2 {
        background-color: yellow;
        grid-row: -2 / 4;
      }

      #item3 {
        background-color: blue;
        grid-row: span 2 / 7;
      }
    </style>
    <div id="grid">
      <div id="item1"></div>
      <div id="item2"></div>
      <div id="item3"></div>
    </div>
  )HTML");

  NGBlockNode node(GetLayoutBoxByElementId("grid"));

  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(500), LayoutUnit(500)),
      /* stretch_inline_size_if_auto */ true,
      /* is_new_formatting_context */ true);

  NGFragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, node);

  NGGridLayoutAlgorithm algorithm({node, fragment_geometry, space});
  EXPECT_EQ(GridItemCount(algorithm), 0U);
  algorithm.Layout();
  EXPECT_EQ(GridItemCount(algorithm), 3U);

  NGGridTrackCollectionBase::RangeRepeatIterator column_iterator(
      &algorithm.ColumnTrackCollection(), 0u);
  EXPECT_RANGE(0u, 1u, column_iterator);
  EXPECT_TRUE(column_iterator.MoveToNextRange());
  EXPECT_RANGE(1u, 1u, column_iterator);
  EXPECT_FALSE(column_iterator.MoveToNextRange());

  NGGridTrackCollectionBase::RangeRepeatIterator row_iterator(
      &algorithm.RowTrackCollection(), 0u);
  EXPECT_RANGE(0u, 1u, row_iterator);
  EXPECT_TRUE(row_iterator.MoveToNextRange());
  EXPECT_RANGE(1u, 2u, row_iterator);
  EXPECT_TRUE(row_iterator.MoveToNextRange());
  EXPECT_RANGE(3u, 1u, row_iterator);
  EXPECT_TRUE(row_iterator.MoveToNextRange());
  EXPECT_RANGE(4u, 1u, row_iterator);
  EXPECT_TRUE(row_iterator.MoveToNextRange());
  EXPECT_RANGE(5u, 1u, row_iterator);
  EXPECT_FALSE(row_iterator.MoveToNextRange());
}

TEST_F(NGGridLayoutAlgorithmTest, NGGridLayoutAlgorithmResolveFixedTrackSizes) {
  if (!RuntimeEnabledFeatures::LayoutNGGridEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
    #grid {
      width: 100px;
      height: 200px;
      display: grid;
      grid-template-columns: 25px repeat(3, 20px) minmax(15px, 10%);
      grid-template-rows: minmax(0px, 100px) 25% repeat(2, minmax(10%, 35px));
    }
    </style>
    <div id="grid"></div>
  )HTML");

  NGBlockNode node(GetLayoutBoxByElementId("grid"));
  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), kIndefiniteSize),
      /* stretch_inline_size_if_auto */ true,
      /* is_new_formatting_context */ true);
  NGFragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, node);

  NGGridLayoutAlgorithm algorithm({node, fragment_geometry, space});
  algorithm.Layout();

  Vector<LayoutUnit> expected_column_base_sizes = {
      LayoutUnit(25), LayoutUnit(60), LayoutUnit(15)};
  Vector<LayoutUnit> expected_column_growth_limits = {
      LayoutUnit(25), LayoutUnit(60), LayoutUnit(15)};

  Vector<LayoutUnit> base_sizes = BaseSizes(algorithm, kForColumns);
  EXPECT_EQ(expected_column_base_sizes.size(), base_sizes.size());
  for (wtf_size_t i = 0; i < base_sizes.size(); ++i)
    EXPECT_EQ(expected_column_base_sizes[i], base_sizes[i]);

  Vector<LayoutUnit> growth_limits = GrowthLimits(algorithm, kForColumns);
  EXPECT_EQ(expected_column_growth_limits.size(), growth_limits.size());
  for (wtf_size_t i = 0; i < growth_limits.size(); ++i)
    EXPECT_EQ(expected_column_growth_limits[i], growth_limits[i]);

  Vector<LayoutUnit> expected_row_base_sizes = {LayoutUnit(0), LayoutUnit(50),
                                                LayoutUnit(40)};
  Vector<LayoutUnit> expected_row_growth_limits = {
      LayoutUnit(100), LayoutUnit(50), LayoutUnit(70)};

  base_sizes = BaseSizes(algorithm, kForRows);
  EXPECT_EQ(expected_row_base_sizes.size(), base_sizes.size());
  for (wtf_size_t i = 0; i < base_sizes.size(); ++i)
    EXPECT_EQ(expected_row_base_sizes[i], base_sizes[i]);

  growth_limits = GrowthLimits(algorithm, kForRows);
  EXPECT_EQ(expected_row_growth_limits.size(), growth_limits.size());
  for (wtf_size_t i = 0; i < growth_limits.size(); ++i)
    EXPECT_EQ(expected_row_growth_limits[i], growth_limits[i]);
}

TEST_F(NGGridLayoutAlgorithmTest,
       NGGridLayoutAlgorithmDetermineGridItemsSpanningIntrinsicOrFlexTracks) {
  if (!RuntimeEnabledFeatures::LayoutNGGridEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
    #grid {
      display: grid;
      grid-template-columns: repeat(2, min-content 1fr 2px 3px);
      grid-template-rows: max-content 1fr 50px fit-content(100px);
    }
    #item0 {
      grid-column: 4 / 6;
      grid-row: -3 / -2;
    }
    #item1 {
      grid-column: 6 / 8;
      grid-row: -2 / -1;
    }
    #item2 {
      grid-column: 3 / 5;
      grid-row: -4 / -3;
    }
    #item3 {
      grid-column: 8 / 11;
      grid-row: -5 / -4;
    }
    </style>
    <div id="grid">
      <div id="item0"></div>
      <div id="item1"></div>
      <div id="item2"></div>
      <div id="item3"></div>
    </div>
  )HTML");

  NGBlockNode node(GetLayoutBoxByElementId("grid"));
  NGConstraintSpace space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), kIndefiniteSize),
      /* stretch_inline_size_if_auto */ true,
      /* is_new_formatting_context */ true);
  NGFragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, node);

  NGGridLayoutAlgorithm algorithm({node, fragment_geometry, space});
  algorithm.Layout();

  DetermineGridItemsSpanningIntrinsicOrFlexTracks(algorithm, kForColumns);
  Vector<wtf_size_t> expected_grid_items_spanning_intrinsic_track = {0, 1, 3};
  Vector<wtf_size_t> expected_grid_items_spanning_flex_track = {1};

  Vector<wtf_size_t> actual_items = GridItemsSpanningIntrinsicTrack(algorithm);
  EXPECT_EQ(expected_grid_items_spanning_intrinsic_track.size(),
            actual_items.size());
  for (wtf_size_t i = 0; i < actual_items.size(); ++i)
    EXPECT_EQ(expected_grid_items_spanning_intrinsic_track[i], actual_items[i]);

  actual_items = GridItemsSpanningFlexTrack(algorithm);
  EXPECT_EQ(expected_grid_items_spanning_flex_track.size(),
            actual_items.size());
  for (wtf_size_t i = 0; i < actual_items.size(); ++i)
    EXPECT_EQ(expected_grid_items_spanning_flex_track[i], actual_items[i]);

  DetermineGridItemsSpanningIntrinsicOrFlexTracks(algorithm, kForRows);
  expected_grid_items_spanning_intrinsic_track = {1, 2, 3};
  expected_grid_items_spanning_flex_track = {2};

  actual_items = GridItemsSpanningIntrinsicTrack(algorithm);
  EXPECT_EQ(expected_grid_items_spanning_intrinsic_track.size(),
            actual_items.size());
  for (wtf_size_t i = 0; i < actual_items.size(); ++i)
    EXPECT_EQ(expected_grid_items_spanning_intrinsic_track[i], actual_items[i]);

  actual_items = GridItemsSpanningFlexTrack(algorithm);
  EXPECT_EQ(expected_grid_items_spanning_flex_track.size(),
            actual_items.size());
  for (wtf_size_t i = 0; i < actual_items.size(); ++i)
    EXPECT_EQ(expected_grid_items_spanning_flex_track[i], actual_items[i]);
}

TEST_F(NGGridLayoutAlgorithmTest, FixedSizePositioning) {
  if (!RuntimeEnabledFeatures::LayoutNGGridEnabled())
    return;

  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
      body {
        font: 10px/1 Ahem;
      }

      #grid {
        display: grid;
        width: 200px;
        height: 200px;
        grid-template-columns: 100px 100px;
        grid-template-rows: 100px 100px;
      }

      .grid_item {
        width: 100px;
        height: 100px;
        background-color: gray;
      }

    </style>
    <div id="wrapper">
      <div id="grid">
        <div class="grid_item">1</div>
        <div class="grid_item">2</div>
        <div class="grid_item">3</div>
        <div class="grid_item">4</div>
      </div>
    </div>
  )HTML");
  String dump = DumpFragmentTree(GetElementById("wrapper"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x200
    offset:0,0 size:200x200
      offset:0,0 size:100x100
        offset:0,0 size:10x10
      offset:100,0 size:100x100
        offset:0,0 size:10x10
      offset:0,100 size:100x100
        offset:0,0 size:10x10
      offset:100,100 size:100x100
        offset:0,0 size:10x10
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGGridLayoutAlgorithmTest, FixedSizePositioningAutoRows) {
  if (!RuntimeEnabledFeatures::LayoutNGGridEnabled())
    return;

  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    body {
      font: 10px/1 Ahem;
    }

    #grid {
      display: grid;
      width: 200px;
      height: 200px;
      grid-auto-columns: 100px;
      grid-auto-rows: 100px;
    }

    .grid_item {
      width: 100px;
      height: 100px;
      background-color: gray;
    }

    .cell2 {
      width: 100px;
      height: 100px;
      grid-column: 2;
      background-color: gray;
    }

  </style>
  <div id="wrapper">
    <div id="grid">
      <div class="grid_item">1</div>
      <div class="cell2">2</div>
      <div class="grid_item">3</div>
      <div class="grid_item">4</div>
    </div>
  </div>

  )HTML");
  String dump = DumpFragmentTree(GetElementById("wrapper"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x200
    offset:0,0 size:200x200
      offset:0,0 size:100x100
        offset:0,0 size:10x10
      offset:100,0 size:100x100
        offset:0,0 size:10x10
      offset:0,100 size:100x100
        offset:0,0 size:10x10
      offset:100,100 size:100x100
        offset:0,0 size:10x10
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGGridLayoutAlgorithmTest, SpecifiedPositionsOutOfOrder) {
  if (!RuntimeEnabledFeatures::LayoutNGGridEnabled())
    return;

  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
      body {
        font: 10px/1 Ahem;
      }

      #grid {
        display: grid;
        width: 400px;
        height: 400px;
        grid-template-columns: 100px 100px;
        grid-template-rows: 100px 100px;
      }

      .grid_item1 {
        display: block;
        width: 100px;
        height: 100px;
        grid-row: 2;
        grid-column: 2;
      }

      .grid_item2 {
        display: block;
        width: 90px;
        height: 90px;
        grid-row: 1;
        grid-column: 1;
      }

      .grid_item3 {
        display: block;
        width: 80px;
        height: 80px;
        grid-row: 1;
        grid-column: 2;
      }

      .grid_item4 {
        display: block;
        width: 70px;
        height: 70px;
        grid-row: 2;
        grid-column: 1;
      }
    </style>
    <div id="wrapper">
      <div id="grid">
        <div class="grid_item1">1</div>
        <div class="grid_item2">2</div>
        <div class="grid_item3">3</div>
        <div class="grid_item4">4</div>
      </div>
    </div>
  )HTML");
  String dump = DumpFragmentTree(GetElementById("wrapper"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x400
    offset:0,0 size:400x400
      offset:100,100 size:100x100
        offset:0,0 size:10x10
      offset:0,0 size:90x90
        offset:0,0 size:10x10
      offset:100,0 size:80x80
        offset:0,0 size:10x10
      offset:0,100 size:70x70
        offset:0,0 size:10x10
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGGridLayoutAlgorithmTest, GridWithGap) {
  if (!RuntimeEnabledFeatures::LayoutNGGridEnabled())
    return;

  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
      body {
        font: 10px/1 Ahem;
      }

      #grid {
        display: grid;
        width: 200px;
        height: 200px;
        grid-template-columns: 100px 100px;
        grid-template-rows: 100px 100px;
        grid-gap: 10px;
      }

      .grid_item {
        width: 100px;
        height: 100px;
        background-color: gray;
      }

    </style>
    <div id="wrapper">
      <div id="grid">
        <div class="grid_item">1</div>
        <div class="grid_item">2</div>
        <div class="grid_item">3</div>
        <div class="grid_item">4</div>
      </div>
    </div>
  )HTML");
  String dump = DumpFragmentTree(GetElementById("wrapper"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x200
    offset:0,0 size:200x200
      offset:0,0 size:100x100
        offset:0,0 size:10x10
      offset:110,0 size:100x100
        offset:0,0 size:10x10
      offset:0,110 size:100x100
        offset:0,0 size:10x10
      offset:110,110 size:100x100
        offset:0,0 size:10x10
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGGridLayoutAlgorithmTest, GridWithPercentGap) {
  if (!RuntimeEnabledFeatures::LayoutNGGridEnabled())
    return;

  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
      body {
        font: 10px/1 Ahem;
      }

      #grid {
        display: grid;
        width: 100px;
        height: 50px;
        grid-column-gap: 50%;
        grid-row-gap: 75%;
        grid-template-columns: 100px 200px;
        grid-template-rows: 100px 100px;
      }
      .grid-item-odd {
        width: 100px;
        height: 100px;
        background: gray;
      }
      .grid-item-even {
        width: 200px;
        height: 100px;
        background: green;
      }
    </style>
    <div id="wrapper">
      <div id="grid">
        <div class="grid-item-odd">1</div>
         <div class="grid-item-even">2</div>
         <div class="grid-item-odd">3</div>
         <div class="grid-item-even">4</div>
     </div>
    </div>
  )HTML");
  String dump = DumpFragmentTree(GetElementById("wrapper"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x50
    offset:0,0 size:100x50
      offset:0,0 size:100x100
        offset:0,0 size:10x10
      offset:150,0 size:200x100
        offset:0,0 size:10x10
      offset:0,137.5 size:100x100
        offset:0,0 size:10x10
      offset:150,137.5 size:200x100
        offset:0,0 size:10x10
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGGridLayoutAlgorithmTest, AutoSizedGridWithGap) {
  if (!RuntimeEnabledFeatures::LayoutNGGridEnabled())
    return;

  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
      body {
        font: 10px/1 Ahem;
      }

      #grid {
        display: grid;
        width: auto;
        height: auto;
        grid-column-gap: 50px;
        grid-row-gap: 75px;
        grid-template-columns: 100px 200px;
        grid-template-rows: 100px 100px;
      }
      .grid-item-odd {
        width: 100px;
        height: 100px;
        background: gray;
      }
      .grid-item-even {
        width: 200px;
        height: 100px;
        background: green;
      }
    </style>
    <div id="wrapper">
      <div id="grid">
        <div class="grid-item-odd">1</div>
         <div class="grid-item-even">2</div>
         <div class="grid-item-odd">3</div>
         <div class="grid-item-even">4</div>
     </div>
    </div>
  )HTML");
  String dump = DumpFragmentTree(GetElementById("wrapper"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x275
    offset:0,0 size:1000x275
      offset:0,0 size:100x100
        offset:0,0 size:10x10
      offset:150,0 size:200x100
        offset:0,0 size:10x10
      offset:0,175 size:100x100
        offset:0,0 size:10x10
      offset:150,175 size:200x100
        offset:0,0 size:10x10
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGGridLayoutAlgorithmTest, AutoSizedGridWithPercentageGap) {
  if (!RuntimeEnabledFeatures::LayoutNGGridEnabled())
    return;

  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
      body {
        font: 10px/1 Ahem;
      }

      #grid {
        display: grid;
        width: auto;
        height: auto;
        grid-template-columns: 100px 100px 100px;
        grid-template-rows: 100px 100px;
        gap: 5%;
      }

    </style>
    <div id="wrapper">
     <div id="grid">
        <div style="background: orange;"></div>
        <div style="background: green;"></div>
        <div style="background: blueviolet;"></div>
        <div style="background: orange;"></div>
        <div style="background: green;"></div>
        <div style="background: blueviolet;"></div>
      </div>
    </div>
  )HTML");
  String dump = DumpFragmentTree(GetElementById("wrapper"));

  // TODO(ansollan): Change this expectation string as it is currently
  // incorrect. The 'auto' inline size of the second node should be resolved to
  // 300, based on the column definitions. After that work is implemented, the
  // first two nodes in the output should look like this:
  // offset:unplaced size:1000x200
  //   offset:0,0 size:300x200
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x200
    offset:0,0 size:1000x200
      offset:0,0 size:100x100
      offset:150,0 size:100x100
      offset:300,0 size:100x100
      offset:0,110 size:100x100
      offset:150,110 size:100x100
      offset:300,110 size:100x100
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGGridLayoutAlgorithmTest, ItemsSizeWithGap) {
  if (!RuntimeEnabledFeatures::LayoutNGGridEnabled())
    return;

  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
      body {
        font: 10px/1 Ahem;
      }

      #grid {
        display: grid;
        width: 340px;
        height: 100px;
        grid-template-columns: 100px 100px 100px;
        grid-template-rows: 100px;
        column-gap: 20px;
      }

      .grid_item {
        width: 100%;
        height: 100%;
      }

      #cell1 {
        grid-row: 1 / 2;
        grid-column: 1 / 2;
      }

      #cell2 {
        grid-row: 1 / 2;
        grid-column: 2 / 3;
      }

      #cell3 {
        grid-row: 1 / 2;
        grid-column: 3 / 4;
      }

    </style>
    <div id="wrapper">
     <div id="grid">
        <div class="grid_item" id="cell1" style="background: orange;">1</div>
        <div class="grid_item" id="cell2" style="background: green;">3</div>
        <div class="grid_item" id="cell3" style="background: blueviolet;">5</div>
      </div>
    </div>
  )HTML");
  String dump = DumpFragmentTree(GetElementById("wrapper"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x100
    offset:0,0 size:340x100
      offset:0,0 size:100x100
        offset:0,0 size:10x10
      offset:120,0 size:100x100
        offset:0,0 size:10x10
      offset:240,0 size:100x100
        offset:0,0 size:10x10
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(NGGridLayoutAlgorithmTest, OutOfFlowGridItems) {
  if (!RuntimeEnabledFeatures::LayoutNGGridEnabled())
    return;

  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
      body {
        font: 10px/1 Ahem;
      }

      #grid {
        display: grid;
        width: 100px;
        height: 300px;
        grid-auto-columns: 100px;
        grid-auto-rows: 100px;
        position: relative;
      }

      .grid_item {
        width: 100px;
        height: 100px;
        background-color: gray;
      }

      #cell2 {
        position: absolute;
        left: 25%;
        top: 10%;
        width: 100px;
        height: 100px;
        background-color: blue;
      }

      #cell4 {
        position: absolute;
        top: 150px;
        left: 25px;
        width: 100%;
        height: 35%;
        background-color: yellow;
      }

    </style>
    <div id="wrapper">
      <div id="grid">
        <div class="grid_item" style="background: orange;">1</div>
        <div id="cell2">2</div>
        <div class="grid_item" style="background: green;">3</div>
        <div id="cell4">4</div>
        <div class="grid_item" style="background: blueviolet;">5</div>
      </div>
    </div>
  )HTML");
  String dump = DumpFragmentTree(GetElementById("wrapper"));

  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  offset:unplaced size:1000x300
    offset:0,0 size:100x300
      offset:0,0 size:100x100
        offset:0,0 size:10x10
      offset:0,100 size:100x100
        offset:0,0 size:10x10
      offset:0,200 size:100x100
        offset:0,0 size:10x10
      offset:25,30 size:100x100
        offset:0,0 size:10x10
      offset:25,150 size:100x105
        offset:0,0 size:10x10
)DUMP";
  EXPECT_EQ(expectation, dump);
}

}  // namespace blink
