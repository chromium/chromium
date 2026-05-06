// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid_lanes/grid_lanes_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/base_layout_algorithm_test.h"
#include "third_party/blink/renderer/core/layout/grid/grid_item.h"
#include "third_party/blink/renderer/core/layout/grid/grid_layout_utils.h"
#include "third_party/blink/renderer/core/layout/grid/grid_sizing_tree.h"
#include "third_party/blink/renderer/core/layout/grid/grid_track_collection.h"
#include "third_party/blink/renderer/core/layout/grid/grid_track_sizing_algorithm.h"
#include "third_party/blink/renderer/core/layout/grid_lanes/grid_lanes_running_positions.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"

namespace blink {

class GridLanesLayoutAlgorithmTest : public BaseLayoutAlgorithmTest {
 protected:
  void SetUp() override { BaseLayoutAlgorithmTest::SetUp(); }

  void ComputeGeometry(GridLanesLayoutAlgorithm& algorithm) {
    const auto& style = algorithm.Style();
    grid_axis_direction_ = style.GridLanesTrackSizingDirection();

    GridItems* grid_items = nullptr;
    const GridLayoutSubtree* layout_subtree =
        algorithm.ComputeGridLanesGeometry(
            SizingConstraint::kLayout,
            /*should_apply_inline_size_containment=*/false, &grid_items);

    layout_data_ = layout_subtree->LayoutData();

    ASSERT_EQ(grid_axis_direction_, TrackCollection().Direction());

    // To access virtual items for testing, run a separate sizing pass.
    GridSizingTree sizing_tree;
    bool needs_intrinsic_track_size;
    algorithm.ComputeSizingTreeInGridAxis(
        SizingConstraint::kLayout,
        /*should_apply_inline_size_containment=*/false, &sizing_tree,
        needs_intrinsic_track_size);

    // We have a repeat() track definition with an intrinsic sized track(s). The
    // previous track sizing pass was used to find the track size to apply
    // to the intrinsic sized track(s). Retrieve that value, and re-run track
    // sizing to get the correct number of automatic repetitions for the
    // repeat() definition.
    //
    // https://www.w3.org/TR/css-grid-3/#masonry-intrinsic-repeat
    if (needs_intrinsic_track_size) {
      algorithm.CalculateIntrinsicTrackSizes(sizing_tree);
      algorithm.ComputeSizingTreeInGridAxis(
          SizingConstraint::kLayout,
          /*should_apply_inline_size_containment=*/false, &sizing_tree,
          needs_intrinsic_track_size);
    }

    layout_data_ = &sizing_tree.LayoutData();

    ASSERT_EQ(grid_axis_direction_, TrackCollection().Direction());

    for (const auto& grid_lanes_item : sizing_tree.GetVirtualItems()) {
      GridLanesItemCachedData item_data;

      item_data.resolved_span =
          grid_lanes_item.resolved_position.Span(grid_axis_direction_);
      item_data.contribution_sizes = grid_lanes_item.contribution_sizes;
      virtual_items_data_.emplace_back(std::move(item_data));
    }
  }

  wtf_size_t VirtualItemCount() { return virtual_items_data_.size(); }
  const GridRangeVector& Ranges() { return TrackCollection().ranges_; }

  Vector<LayoutUnit> TrackSizes() {
    const auto& tracks = TrackCollection();
    Vector<LayoutUnit> track_sizes;
    for (wtf_size_t i = 0; i < tracks.GetSetCount(); ++i) {
      track_sizes.push_back(tracks.GetSetOffset(i + 1) -
                            tracks.GetSetOffset(i));
    }
    return track_sizes;
  }

  LayoutUnit MaxContentContribution(wtf_size_t index) {
    return VirtualItemData(index)
        .contribution_sizes->min_max_contribution.max_size;
  }

  LayoutUnit MinContentContribution(wtf_size_t index) {
    return VirtualItemData(index)
        .contribution_sizes->min_max_contribution.min_size;
  }

  const GridSpan& VirtualItemSpan(wtf_size_t index) {
    return VirtualItemData(index).resolved_span;
  }

  Vector<LayoutUnit> GetMaxPositionsForAllTracks(
      const GridLanesRunningPositions& running_positions,
      wtf_size_t span_size) {
    return running_positions.GetMaxPositionsForAllTracks(span_size);
  }

  GridLanesRunningPositions InitializeGridLanesRunningPositions(
      const Vector<LayoutUnit>& running_positions,
      LayoutUnit tie_threshold) {
    const Vector<wtf_size_t> empty_collapsed_tracks;
    return GridLanesRunningPositions(running_positions, tie_threshold,
                                     empty_collapsed_tracks);
  }

  void SetAutoPlacementCursor(wtf_size_t cursor,
                              GridLanesRunningPositions& running_positions) {
    running_positions.SetAutoPlacementCursorForTesting(cursor);
  }

  const GridLayoutTrackCollection& TrackCollection() {
    const auto grid_axis_direction =
        GridLanesLayoutAlgorithmTest::grid_axis_direction_;
    return (grid_axis_direction == kForColumns) ? layout_data_->Columns()
                                                : layout_data_->Rows();
  }

 private:
  struct GridLanesItemCachedData {
    Persistent<const GridItemData::VirtualItemContributions> contribution_sizes;
    GridSpan resolved_span{GridSpan::IndefiniteGridSpan()};
  };

  const GridLanesItemCachedData& VirtualItemData(wtf_size_t index) {
    DCHECK_LT(index, virtual_items_data_.size());
    return virtual_items_data_[index];
  }

  Persistent<const GridLayoutData> layout_data_;
  GridTrackSizingDirection grid_axis_direction_ = kForColumns;

  // Virtual items represent the contributions of item groups in track sizing
  // and are not directly related to any children of the container.
  Vector<GridLanesItemCachedData> virtual_items_data_;
};

TEST_F(GridLanesLayoutAlgorithmTest, ConstructGridLanesItems) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-template-columns: auto auto [header-start] auto auto [header-end];
    }
    </style>
    <div id="grid-lanes">
      <div>1</div>
      <div style="grid-column: 3 / span 2">2</div>
      <div style="grid-column: span 2">3</div>
      <div style="grid-column: span 3">4</div>
      <div style="grid-column: 2 / 5">5</div>
      <div style="grid-column: header-start / header-end">1</div>
      <div style="grid-column: 1 / header-start">2</div>
      <div style="grid-column: 3 / header-end">2</div>
    </div>
  )HTML");

  GridLanesNode node(GetLayoutBoxByElementId("grid-lanes"));

  const GridLineResolver line_resolver(node.Style(), /*auto_repetitions=*/0);
  auto* grid_lanes_items = node.ConstructGridItems(
      line_resolver, /*must_invalidate_placement_cache=*/nullptr,
      /*opt_oof_children=*/nullptr);

  const Vector<GridSpan> expected_spans = {
      GridSpan::IndefiniteGridSpan(1),
      GridSpan::TranslatedDefiniteGridSpan(2, 4),
      GridSpan::IndefiniteGridSpan(2),
      GridSpan::IndefiniteGridSpan(3),
      GridSpan::TranslatedDefiniteGridSpan(1, 4),
      GridSpan::TranslatedDefiniteGridSpan(2, 4),
      GridSpan::TranslatedDefiniteGridSpan(0, 2),
      GridSpan::TranslatedDefiniteGridSpan(2, 4)};

  EXPECT_EQ(grid_lanes_items->Size(), expected_spans.size());

  const auto grid_axis_direction = node.Style().GridLanesTrackSizingDirection();
  for (wtf_size_t i = 0; auto& grid_lanes_item : *grid_lanes_items) {
    grid_lanes_item.MaybeTranslateSpan(/*start_offset=*/0,
                                       GridTrackSizingDirection::kForColumns);
    EXPECT_EQ(grid_lanes_item.resolved_position.Span(grid_axis_direction),
              expected_spans[i++]);
  }
}

TEST_F(GridLanesLayoutAlgorithmTest, BuildRanges) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-template-columns: 5% repeat(3, 10px auto) repeat(1, auto 5px 1fr);
    }
    </style>
    <div id="grid-lanes">
      <div style="grid-column: span 2 / 1"></div>
      <div style="grid-column: 9 / span 5"></div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), LayoutUnit(100)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  // The first item spans 2 tracks before the explicit grid, creating the first
  // range of 2 tracks. Then follows the template track ranges: one range of a
  // single track for the `5%`, then a range for the `repeat(3, ...)` which
  // spans 6 tracks. The last repeat creates a range of 3 tracks, but it's split
  // by the second item, creating one range of 1 track and another of 2 tracks.
  // Finally, the second item spans a range of 3 tracks past the explicit grid.
  const Vector<wtf_size_t> expected_start_lines = {0, 2, 3, 9, 10, 12};
  const Vector<wtf_size_t> expected_track_counts = {2, 1, 6, 1, 2, 3};

  const auto& ranges = Ranges();
  EXPECT_EQ(ranges.size(), expected_start_lines.size());

  for (wtf_size_t i = 0; i < ranges.size(); ++i) {
    EXPECT_EQ(ranges[i].start_line, expected_start_lines[i]);
    EXPECT_EQ(ranges[i].track_count, expected_track_counts[i]);
    EXPECT_FALSE(ranges[i].IsCollapsed());
  }
}

TEST_F(GridLanesLayoutAlgorithmTest, BuildFixedTrackSizes) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-template-columns: 5% repeat(3, 10px 15%) repeat(1, 15px 5px 20px);
    }
    </style>
    <div id="grid-lanes"></div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), LayoutUnit(100)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  EXPECT_EQ(TrackSizes(), Vector<LayoutUnit>({LayoutUnit(5), LayoutUnit(30),
                                              LayoutUnit(45), LayoutUnit(15),
                                              LayoutUnit(5), LayoutUnit(20)}));
}

TEST_F(GridLanesLayoutAlgorithmTest, CollectGridLanesItemGroups) {
  SetBodyInnerHTML(R"HTML(
    <div id="grid-lanes" style="display: grid-lanes">
      <div></div>
      <div style="grid-column: 1"></div>
      <div style="grid-column: 1 / 4"></div>
      <div style="grid-column: span 3"></div>
      <div style="grid-column: span 3 / 4"></div>
      <div></div>
    </div>
  )HTML");

  GridLanesNode node(GetLayoutBoxByElementId("grid-lanes"));

  wtf_size_t max_end_line, start_offset;
  const GridLineResolver line_resolver(node.Style(), /*auto_repetitions=*/0);
  const auto* grid_lanes_items = node.ConstructGridItems(
      line_resolver, /*must_invalidate_placement_cache=*/nullptr,
      /*opt_oof_children=*/nullptr);
  wtf_size_t unplaced_item_span_count = 0;
  const auto item_groups =
      node.CollectItemGroups(line_resolver, *grid_lanes_items, max_end_line,
                             start_offset, unplaced_item_span_count);

  EXPECT_EQ(item_groups.size(), 4u);

  for (const auto& group : item_groups) {
    const auto& items = group->items;
    const auto& properties = group->properties;
    wtf_size_t expected_size = 0;
    const auto& span = properties.Span();
    if (span == GridSpan::IndefiniteGridSpan(3) ||
        span == GridSpan::TranslatedDefiniteGridSpan(0, 1)) {
      expected_size = 1;
    } else if (span == GridSpan::IndefiniteGridSpan(1) ||
               span == GridSpan::TranslatedDefiniteGridSpan(0, 3)) {
      expected_size = 2;
    }
    EXPECT_EQ(items.size(), expected_size);
  }
}

TEST_F(GridLanesLayoutAlgorithmTest, CollectGridLanesItemGroupsWithBaseline) {
  SetBodyInnerHTML(R"HTML(
    <div id="grid-lanes" style="display: grid-lanes">
      <div style="justify-self: first baseline"></div>
      <div style="justify-self: baseline"></div>
      <div style="justify-self: last baseline"></div>
      <div style="grid-column: span 2"></div>
      <div style="grid-column: span 2; justify-self: first baseline"></div>
      <div style="grid-column: span 2; justify-self: first baseline"></div>
      <div style="grid-column: span 2; justify-self: last baseline"></div>
      <div style="grid-column: span 2; justify-self: last baseline"></div>
      <div style="grid-column: span 2; justify-self: last baseline"></div>
    </div>
  )HTML");

  GridLanesNode node(GetLayoutBoxByElementId("grid-lanes"));

  wtf_size_t max_end_line, start_offset;
  const GridLineResolver line_resolver(node.Style(), /*auto_repetitions=*/0);
  const auto* grid_lanes_items = node.ConstructGridItems(
      line_resolver, /*must_invalidate_placement_cache=*/nullptr,
      /*opt_oof_children=*/nullptr);
  wtf_size_t unplaced_item_span_count = 0;
  const auto item_groups =
      node.CollectItemGroups(line_resolver, *grid_lanes_items, max_end_line,
                             start_offset, unplaced_item_span_count);

  EXPECT_EQ(item_groups.size(), 5u);
  const auto grid_axis_direction = node.Style().GridLanesTrackSizingDirection();

  for (const auto& group : item_groups) {
    const auto& items = group->items;
    const auto& properties = group->properties;
    const auto& span = properties.Span();
    if (span == GridSpan::IndefiniteGridSpan(1)) {
      BaselineGroup baseline_group =
          items.size() == 2u ? BaselineGroup::kMajor : BaselineGroup::kMinor;
      for (const auto& item : items) {
        EXPECT_TRUE(item->IsBaselineAligned(grid_axis_direction));
        EXPECT_EQ(item->BaselineGroup(grid_axis_direction), baseline_group);
      }
    } else if (span == GridSpan::IndefiniteGridSpan(2)) {
      bool is_baseline_aligned =
          items[0]->IsBaselineAligned(grid_axis_direction);
      if (is_baseline_aligned) {
        BaselineGroup baseline_group =
            items.size() == 2u ? BaselineGroup::kMajor : BaselineGroup::kMinor;
        for (const auto& item : items) {
          EXPECT_TRUE(item->IsBaselineAligned(grid_axis_direction));
          EXPECT_EQ(item->BaselineGroup(grid_axis_direction), baseline_group);
        }
      } else {
        EXPECT_EQ(items.size(), 1u);
      }
    }
  }
}

TEST_F(GridLanesLayoutAlgorithmTest, ExplicitlyPlacedVirtualItems) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    body { font: 10px/1 Ahem }
    #grid-lanes {
      display: grid-lanes;
      grid-template-columns: repeat(2, 100px);
    }
    </style>
    <div id="grid-lanes">
      <div style="grid-column: 1">XX XX</div>
      <div style="grid-column: -4 / 3">XXX X</div>
      <div style="grid-column: span 3 / 3">X XX X</div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), LayoutUnit(100)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  const auto item_count = VirtualItemCount();
  EXPECT_EQ(item_count, 2u);

  for (wtf_size_t i = 0; i < item_count; ++i) {
    LayoutUnit expected_max_size, expected_min_size;
    const auto& span = VirtualItemSpan(i);
    if (span == GridSpan::TranslatedDefiniteGridSpan(1, 2)) {
      expected_max_size = LayoutUnit(50);
      expected_min_size = LayoutUnit(20);
    } else if (span == GridSpan::TranslatedDefiniteGridSpan(0, 3)) {
      expected_max_size = LayoutUnit(60);
      expected_min_size = LayoutUnit(30);
    }
    EXPECT_EQ(MaxContentContribution(i), expected_max_size);
    EXPECT_EQ(MinContentContribution(i), expected_min_size);
  }
}

TEST_F(GridLanesLayoutAlgorithmTest, AutoPlacedVirtualItems) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    body { font: 10px/1 Ahem }
    #grid-lanes {
      display: grid-lanes;
      grid-template-columns: repeat(3, auto);
    }
    </style>
    <div id="grid-lanes">
      <div>X X X X X</div>
      <div style="grid-column: span 2">XXX X</div>
      <div>XX XX XX XX XX</div>
      <div style="grid-column: span 2">X XX X</div>
      <div>X XX XXX XX X</div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), LayoutUnit(100)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  const auto item_count = VirtualItemCount();
  EXPECT_EQ(item_count, 5u);

  for (wtf_size_t i = 0; i < item_count; ++i) {
    LayoutUnit expected_max_size, expected_min_size;
    const auto& span = VirtualItemSpan(i);
    if (span == GridSpan::TranslatedDefiniteGridSpan(0, 2) ||
        span == GridSpan::TranslatedDefiniteGridSpan(1, 3)) {
      expected_max_size = LayoutUnit(60);
      expected_min_size = LayoutUnit(30);
    } else if (span == GridSpan::TranslatedDefiniteGridSpan(0, 1) ||
               span == GridSpan::TranslatedDefiniteGridSpan(1, 2) ||
               span == GridSpan::TranslatedDefiniteGridSpan(2, 3)) {
      expected_max_size = LayoutUnit(140);
      expected_min_size = LayoutUnit(30);
    }
    EXPECT_EQ(MaxContentContribution(i), expected_max_size);
    EXPECT_EQ(MinContentContribution(i), expected_min_size);
  }
}

TEST_F(GridLanesLayoutAlgorithmTest, BuildIntrinsicTrackSizes) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    body { font: 10px/1 Ahem }
    #grid-lanes {
      display: grid-lanes;
      grid-template-columns: min-content max-content;
    }
    </style>
    <div id="grid-lanes">
      <div style="grid-column: 1">XX XX</div>
      <div style="grid-column: 2">XX XX</div>
      <div style="grid-column: 1 / 3">XXX XXXXXX XXXXXXXXX</div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), LayoutUnit(100)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  EXPECT_EQ(TrackSizes(),
            Vector<LayoutUnit>({LayoutUnit(30), LayoutUnit(170)}));
}

TEST_F(GridLanesLayoutAlgorithmTest, MaximizeAndStretchAutoTracks) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    body { font: 10px/1 Ahem }
    #grid-lanes {
      display: grid-lanes;
      grid-template-columns: minmax(15px, min-content) max-content auto;
    }
    </style>
    <div id="grid-lanes">
      <div style="grid-column: 1">XXX XXX</div>
      <div style="grid-column: 1 / 3">X XX X</div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), LayoutUnit(100)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  // First track starts at 15px, but should be resolved to 30px (which is the
  // min-content size of the first item) later in the maximize tracks step.
  // To acomodate the max-content size of the second item, which is 60px minus
  // 15px that the first track already has, the second track expands to 45px.
  // Finally, the last track takes the remaining space after the first two
  // tracks are maximized, which is 100px - 30px - 45px = 25px.
  EXPECT_EQ(TrackSizes(), Vector<LayoutUnit>({LayoutUnit(30), LayoutUnit(45),
                                              LayoutUnit(25)}));
}

TEST_F(GridLanesLayoutAlgorithmTest, ExpandFlexibleTracks) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-template-columns: 1fr 5fr 3fr 1fr;
    }
    </style>
    <div id="grid-lanes"></div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), LayoutUnit(100)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  EXPECT_EQ(TrackSizes(), Vector<LayoutUnit>({LayoutUnit(10), LayoutUnit(50),
                                              LayoutUnit(30), LayoutUnit(10)}));
}

TEST_F(GridLanesLayoutAlgorithmTest, BuildRowSizes) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      height: 100px;
      display: grid-lanes;
      grid-lanes-direction: row;
      grid-template-rows: 20px 1fr 30%;
    }
    </style>
    <div id="grid-lanes"></div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), LayoutUnit(100)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  EXPECT_EQ(TrackSizes(), Vector<LayoutUnit>({LayoutUnit(20), LayoutUnit(50),
                                              LayoutUnit(30)}));
}

TEST_F(GridLanesLayoutAlgorithmTest, ColumnAutoFitAutoPlacement) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-template-columns: repeat(auto-fit, 100px);
    }
    #grid-lanes > div {
      width: 100%;
      height: 100px;
    }
    </style>
    <div id="grid-lanes">
      <div></div>
      <div></div>
      <div></div>
      <div></div>
      <div></div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), LayoutUnit(200)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  EXPECT_EQ(TrackSizes(), Vector<LayoutUnit>({LayoutUnit(100), LayoutUnit(100),
                                              LayoutUnit(100), LayoutUnit(100),
                                              LayoutUnit(100)}));
}

TEST_F(GridLanesLayoutAlgorithmTest, ColumnAutoFitAutoAndExplicitPlacement) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-template-columns: repeat(auto-fit, 100px);
    }
    #grid-lanes > div {
      width: 100%;
      height: 100px;
    }
    </style>
    <div id="grid-lanes">
      <div></div>
      <div></div>
      <div style="grid-column: 4"></div>
      <div style="grid-column: 6"></div>
      <div></div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), LayoutUnit(200)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  EXPECT_EQ(TrackSizes(), Vector<LayoutUnit>({LayoutUnit(100), LayoutUnit(100),
                                              LayoutUnit(100), LayoutUnit(100),
                                              LayoutUnit(100)}));
}

TEST_F(GridLanesLayoutAlgorithmTest, ColumnAutoFillAutoFitAutoPlacement) {
  SetBodyInnerHTML(R"HTML(
  <style>
  #grid-lanes {
      display: grid-lanes;
      grid-template-columns: repeat(5, 100px) repeat(auto-fit, 100px);
  }
  #grid-lanes > div {
      width: 100%;
      height: 100px;
  }
  </style>
  <div id="grid-lanes">
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
  </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), LayoutUnit(200)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  EXPECT_EQ(
      TrackSizes(),
      Vector<LayoutUnit>({LayoutUnit(100), LayoutUnit(100), LayoutUnit(100),
                          LayoutUnit(100), LayoutUnit(100), LayoutUnit(100)}));
}

TEST_F(GridLanesLayoutAlgorithmTest, ColumnAutoFillAutoFitNoCollapse) {
  SetBodyInnerHTML(R"HTML(
  <style>
  #grid-lanes {
      display: grid-lanes;
      grid-template-columns: repeat(auto-fit, 100px) repeat(5, 100px);
  }
  #grid-lanes > div {
      width: 100%;
      height: 100px;
  }
  </style>
  <div id="grid-lanes">
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
  </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), LayoutUnit(200)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  EXPECT_EQ(
      TrackSizes(),
      Vector<LayoutUnit>({LayoutUnit(100), LayoutUnit(100), LayoutUnit(100),
                          LayoutUnit(100), LayoutUnit(100), LayoutUnit(100),
                          LayoutUnit(100), LayoutUnit(100), LayoutUnit(100),
                          LayoutUnit(100)}));
}

TEST_F(GridLanesLayoutAlgorithmTest, ColumnAutoFitAutoSizeAutoPlacement) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
        display: grid-lanes;
        grid-template-columns: repeat(auto-fit, auto);
    }
    #grid-lanes > div {
        width: 100px;
        height: 100px;
    }
    </style>
    <div id="grid-lanes">
      <div style="grid-column: 1;"></div>
      <div style="grid-column: 3;"></div>
      <div></div>
      <div></div>
      <div></div>
      <div></div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), LayoutUnit(200)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  // These don't end up being 100px wide because auto tracks get stretched after
  // the other tracks were collapsed.
  EXPECT_EQ(TrackSizes(),
            Vector<LayoutUnit>({LayoutUnit(250), LayoutUnit(250),
                                LayoutUnit(250), LayoutUnit(250)}));
}

TEST_F(GridLanesLayoutAlgorithmTest,
       ColumnAutoFitAutoSizeAndAutoAndExplicitPlacement) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
        display: grid-lanes;
        grid-template-columns: repeat(auto-fit, auto);
        height: 200px;
        width: 1000px;
    }
    #grid-lanes > div {
        width: 100px;
        height: 100px;
    }
    </style>
    <div id="grid-lanes">
      <div></div>
      <div></div>
      <div style="grid-column: 4"></div>
      <div style="grid-column: 6"></div>
      <div></div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), LayoutUnit(200)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  // These don't end up being 100px wide because auto tracks get stretched after
  // the other tracks were collapsed.
  EXPECT_EQ(TrackSizes(), Vector<LayoutUnit>({LayoutUnit(200), LayoutUnit(200),
                                              LayoutUnit(200), LayoutUnit(200),
                                              LayoutUnit(200)}));
}

TEST_F(GridLanesLayoutAlgorithmTest,
       ColumnAutoFillAutoFitAutoAndAutoPlacement) {
  SetBodyInnerHTML(R"HTML(
  <style>
  #grid-lanes {
      display: grid-lanes;
      grid-template-columns: repeat(5, 100px) repeat(auto-fit, auto);
  }
  #grid-lanes > div {
      width: 100px;
      height: 100px;
  }
  </style>
  <div id="grid-lanes">
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
  </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), LayoutUnit(200)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  // The last auto-fit column is 500px because it stretches to fill the
  // remaining space.
  EXPECT_EQ(
      TrackSizes(),
      Vector<LayoutUnit>({LayoutUnit(100), LayoutUnit(100), LayoutUnit(100),
                          LayoutUnit(100), LayoutUnit(100), LayoutUnit(500)}));
}

TEST_F(GridLanesLayoutAlgorithmTest, ColumnAutoFillAutoFitAutoNoCollapse) {
  SetBodyInnerHTML(R"HTML(
  <style>
  #grid-lanes {
      display: grid-lanes;
      grid-template-columns: repeat(auto-fit, auto) repeat(5, 100px);
  }
  #grid-lanes > div {
      width: auto;
      height: 100px;
  }
  </style>
  <div id="grid-lanes">
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
  </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), LayoutUnit(200)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  EXPECT_EQ(
      TrackSizes(),
      Vector<LayoutUnit>({LayoutUnit(100), LayoutUnit(100), LayoutUnit(100),
                          LayoutUnit(100), LayoutUnit(100), LayoutUnit(100),
                          LayoutUnit(100), LayoutUnit(100), LayoutUnit(100),
                          LayoutUnit(100)}));
}

TEST_F(GridLanesLayoutAlgorithmTest, RowAutoFitAutoPlacement) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-lanes-direction: row;
      grid-template-rows: repeat(auto-fit, 100px);
      height: 1000px;
    }
    #grid-lanes > div {
      height: 100%;
      width: 100px;
    }
    </style>
    <div id="grid-lanes">
      <div></div>
      <div></div>
      <div></div>
      <div></div>
      <div></div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(200), LayoutUnit(1000)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  EXPECT_EQ(TrackSizes(), Vector<LayoutUnit>({LayoutUnit(100), LayoutUnit(100),
                                              LayoutUnit(100), LayoutUnit(100),
                                              LayoutUnit(100)}));
}

TEST_F(GridLanesLayoutAlgorithmTest, RowAutoFitAutoAndExplicitPlacement) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-lanes-direction: row;
      grid-template-rows: repeat(auto-fit, 100px);
      height: 1000px;
    }
    #grid-lanes > div {
      height: 100%;
      width: 100px;
    }
    </style>
    <div id="grid-lanes">
      <div></div>
      <div></div>
      <div style="grid-row: 4"></div>
      <div style="grid-row: 6"></div>
      <div></div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(200), LayoutUnit(1000)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  EXPECT_EQ(TrackSizes(), Vector<LayoutUnit>({LayoutUnit(100), LayoutUnit(100),
                                              LayoutUnit(100), LayoutUnit(100),
                                              LayoutUnit(100)}));
}

TEST_F(GridLanesLayoutAlgorithmTest, RowAutoFillAutoFitAutoPlacement) {
  SetBodyInnerHTML(R"HTML(
  <style>
  #grid-lanes {
      display: grid-lanes;
      grid-lanes-direction: row;
      grid-template-rows: repeat(5, 100px) repeat(auto-fit, 100px);
      height: 1000px;
  }
  #grid-lanes > div {
      height: 100%;
      width: 100px;
  }
  </style>
  <div id="grid-lanes">
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
  </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(200), LayoutUnit(1000)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  EXPECT_EQ(
      TrackSizes(),
      Vector<LayoutUnit>({LayoutUnit(100), LayoutUnit(100), LayoutUnit(100),
                          LayoutUnit(100), LayoutUnit(100), LayoutUnit(100)}));
}

TEST_F(GridLanesLayoutAlgorithmTest, RowAutoFillAutoFitNoCollapse) {
  SetBodyInnerHTML(R"HTML(
  <style>
  #grid-lanes {
      display: grid-lanes;
      grid-lanes-direction: row;
      grid-template-rows: repeat(auto-fit, 100px) repeat(5, 100px);
      height: 1000px;
  }
  #grid-lanes > div {
      height: 100%;
      width: 100px;
  }
  </style>
  <div id="grid-lanes">
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
  </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(200), LayoutUnit(1000)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  EXPECT_EQ(
      TrackSizes(),
      Vector<LayoutUnit>({LayoutUnit(100), LayoutUnit(100), LayoutUnit(100),
                          LayoutUnit(100), LayoutUnit(100), LayoutUnit(100),
                          LayoutUnit(100), LayoutUnit(100), LayoutUnit(100),
                          LayoutUnit(100)}));
}

TEST_F(GridLanesLayoutAlgorithmTest, RowAutoFitAutoSizeAutoPlacement) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
        display: grid-lanes;
        grid-lanes-direction: row;
        grid-template-rows: repeat(auto-fit, auto);
        height: 1000px;
    }
    #grid-lanes > div {
        height: 100px;
        width: 100px;
    }
    </style>
    <div id="grid-lanes">
      <div style="grid-row: 1;"></div>
      <div style="grid-row: 3;"></div>
      <div></div>
      <div></div>
      <div></div>
      <div></div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(200), LayoutUnit(1000)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  // These don't end up being 100px wide because auto tracks get stretched after
  // the other tracks were collapsed.
  EXPECT_EQ(TrackSizes(),
            Vector<LayoutUnit>({LayoutUnit(250), LayoutUnit(250),
                                LayoutUnit(250), LayoutUnit(250)}));
}

TEST_F(GridLanesLayoutAlgorithmTest,
       RowAutoFitAutoSizeAndAutoAndExplicitPlacement) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
        display: grid-lanes;
        grid-lanes-direction: row;
        grid-template-rows: repeat(auto-fit, auto);
        height: 1000px;
    }
    #grid-lanes > div {
        height: 100px;
        width: 100px;
    }
    </style>
    <div id="grid-lanes">
      <div></div>
      <div></div>
      <div style="grid-row: 4"></div>
      <div style="grid-row: 6"></div>
      <div></div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(200), LayoutUnit(1000)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  // These don't end up being 100px wide because auto tracks get stretched after
  // the other tracks were collapsed.
  EXPECT_EQ(TrackSizes(), Vector<LayoutUnit>({LayoutUnit(200), LayoutUnit(200),
                                              LayoutUnit(200), LayoutUnit(200),
                                              LayoutUnit(200)}));
}

TEST_F(GridLanesLayoutAlgorithmTest, RowAutoFillAutoFitAutoAndAutoPlacement) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
        display: grid-lanes;
        grid-lanes-direction: row;
        grid-template-rows: repeat(5, 100px) repeat(auto-fit, auto);
        height: 1000px;
    }
    #grid-lanes > div {
        height: 100px;
        width: 100px;
    }
    </style>
    <div id="grid-lanes">
      <div></div>
      <div></div>
      <div></div>
      <div></div>
      <div></div>
      <div></div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(200), LayoutUnit(1000)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  // The last auto-fit row is 500px because it stretches to fill the remaining
  // space.
  EXPECT_EQ(
      TrackSizes(),
      Vector<LayoutUnit>({LayoutUnit(100), LayoutUnit(100), LayoutUnit(100),
                          LayoutUnit(100), LayoutUnit(100), LayoutUnit(500)}));
}

TEST_F(GridLanesLayoutAlgorithmTest, RowAutoFillAutoFitAutoNoCollapse) {
  SetBodyInnerHTML(R"HTML(
  <style>
  #grid-lanes {
      display: grid-lanes;
      grid-lanes-direction: row;
      grid-template-rows: repeat(auto-fit, auto) repeat(5, 100px);
      height: 1000px;
  }
  #grid-lanes > div {
      height: 100px;
      width: 100px;
  }
  </style>
  <div id="grid-lanes">
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
  </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(200), LayoutUnit(1000)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  EXPECT_EQ(
      TrackSizes(),
      Vector<LayoutUnit>({LayoutUnit(100), LayoutUnit(100), LayoutUnit(100),
                          LayoutUnit(100), LayoutUnit(100), LayoutUnit(100),
                          LayoutUnit(100), LayoutUnit(100), LayoutUnit(100),
                          LayoutUnit(100)}));
}

TEST_F(GridLanesLayoutAlgorithmTest, GetFirstEligibleLine) {
  auto running_positions = InitializeGridLanesRunningPositions(
      {LayoutUnit(2.0), LayoutUnit(3.0), LayoutUnit(3.5), LayoutUnit(2.5)},
      /*tie_threshold=*/LayoutUnit(0.5));

  SetAutoPlacementCursor(1, running_positions);
  LayoutUnit max_position;
  EXPECT_EQ(
      running_positions.GetFirstEligibleLine(/*span_size=*/2, max_position),
      GridSpan::TranslatedDefiniteGridSpan(1, 3));
  EXPECT_EQ(max_position, LayoutUnit(3.5));

  EXPECT_EQ(
      running_positions.GetFirstEligibleLine(/*span_size=*/1, max_position),
      GridSpan::TranslatedDefiniteGridSpan(3, 4));
  EXPECT_EQ(max_position, LayoutUnit(2.5));

  EXPECT_EQ(
      running_positions.GetFirstEligibleLine(/*span_size=*/4, max_position),
      GridSpan::TranslatedDefiniteGridSpan(0, 4));
  EXPECT_EQ(max_position, LayoutUnit(3.5));

  SetAutoPlacementCursor(2, running_positions);
  EXPECT_EQ(
      running_positions.GetFirstEligibleLine(/*span_size=*/2, max_position),
      GridSpan::TranslatedDefiniteGridSpan(2, 4));
  EXPECT_EQ(max_position, LayoutUnit(3.5));

  SetAutoPlacementCursor(3, running_positions);
  EXPECT_EQ(
      running_positions.GetFirstEligibleLine(/*span_size=*/2, max_position),
      GridSpan::TranslatedDefiniteGridSpan(0, 2));
  EXPECT_EQ(max_position, LayoutUnit(3));

  SetAutoPlacementCursor(4, running_positions);
  EXPECT_EQ(
      running_positions.GetFirstEligibleLine(/*span_size=*/2, max_position),
      GridSpan::TranslatedDefiniteGridSpan(0, 2));
  EXPECT_EQ(max_position, LayoutUnit(3));
}

TEST_F(GridLanesLayoutAlgorithmTest, GetMaxPositionsForAllTracks) {
  auto running_positions = InitializeGridLanesRunningPositions(
      {LayoutUnit(2.0), LayoutUnit(3.0), LayoutUnit(3.5), LayoutUnit(2.5)},
      /*tie_threshold=*/LayoutUnit());

  EXPECT_EQ(GetMaxPositionsForAllTracks(running_positions, /*span_size=*/2),
            Vector<LayoutUnit>({LayoutUnit(3), LayoutUnit(3.5), LayoutUnit(3.5),
                                LayoutUnit(3.5)}));
  EXPECT_EQ(GetMaxPositionsForAllTracks(running_positions, /*span_size=*/4),
            Vector<LayoutUnit>({LayoutUnit(3.5), LayoutUnit(3.5),
                                LayoutUnit(3.5), LayoutUnit(3.5)}));
  EXPECT_EQ(GetMaxPositionsForAllTracks(running_positions, /*span_size=*/1),
            Vector<LayoutUnit>({LayoutUnit(2.0), LayoutUnit(3.0),
                                LayoutUnit(3.5), LayoutUnit(2.5)}));
}

TEST_F(GridLanesLayoutAlgorithmTest, AppendSubgriddedItemsColumns) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-template-columns: 100px 100px 100px;
    }
    #subgrid {
      display: grid;
      grid-template-columns: subgrid;
      grid-column: 2 / 4;
    }
    </style>
    <div id="grid-lanes">
      <div id="subgrid">
        <div>A</div>
        <div>B</div>
      </div>
      <div>C</div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));
  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), kIndefiniteSize),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);
  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);
  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});

  const GridLineResolver line_resolver(node.Style(), /*auto_repetitions=*/0);
  auto sizing_tree =
      BuildGridSizingTree<GridLanesLayoutAlgorithm>(algorithm, line_resolver);

  // The subgrid item should have `must_consider*` flags set for columns (the
  // grid axis of this grid-lanes container).
  const auto& subgrid_item = sizing_tree.GetGridItems().At(0);
  EXPECT_TRUE(subgrid_item.must_consider_grid_items_for_column_sizing);
  EXPECT_FALSE(subgrid_item.must_consider_grid_items_for_row_sizing);
  EXPECT_TRUE(subgrid_item.IsSubgrid());

  // After building the sizing tree, we should have 2 original items + 2
  // subgridded items.
  wtf_size_t total_count = 0;
  wtf_size_t subgridded_count = 0;
  for (const auto& item : sizing_tree.GetGridItems().IncludeSubgriddedItems()) {
    if (item.is_subgridded_to_parent_grid) {
      ++subgridded_count;
    }
    ++total_count;
  }
  EXPECT_EQ(total_count, 4u);
  EXPECT_EQ(subgridded_count, 2u);
}

TEST_F(GridLanesLayoutAlgorithmTest, AppendSubgriddedItemsRows) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-template-rows: 50px 50px 50px;
      grid-lanes-direction: row;
    }
    #subgrid {
      display: grid;
      grid-template-rows: subgrid;
      grid-row: 1 / 3;
    }
    </style>
    <div id="grid-lanes">
      <div id="subgrid">
        <div>A</div>
        <div>B</div>
      </div>
      <div>C</div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));
  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), kIndefiniteSize),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);
  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);
  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});

  const GridLineResolver line_resolver(node.Style(), /*auto_repetitions=*/0);
  auto sizing_tree =
      BuildGridSizingTree<GridLanesLayoutAlgorithm>(algorithm, line_resolver);

  // The subgrid item should have `must_consider*` flags set for rows (the
  // grid axis of this grid-lanes container).
  const auto& subgrid_item = sizing_tree.GetGridItems().At(0);
  EXPECT_FALSE(subgrid_item.must_consider_grid_items_for_column_sizing);
  EXPECT_TRUE(subgrid_item.must_consider_grid_items_for_row_sizing);
  EXPECT_TRUE(subgrid_item.IsSubgrid());

  // After building the sizing tree, we should have 2 original items + 2
  // subgridded items.
  wtf_size_t total_count = 0;
  wtf_size_t subgridded_count = 0;
  for (const auto& item : sizing_tree.GetGridItems().IncludeSubgriddedItems()) {
    if (item.is_subgridded_to_parent_grid) {
      ++subgridded_count;
    }
    ++total_count;
  }
  EXPECT_EQ(total_count, 4u);
  EXPECT_EQ(subgridded_count, 2u);
}

TEST_F(GridLanesLayoutAlgorithmTest, SubgridRowsIgnoredInColumnGridLanes) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-template-columns: 100px 100px 100px;
    }
    #subgrid {
      display: grid;
      grid-template-rows: subgrid;
      grid-column: 1 / 3;
    }
    </style>
    <div id="grid-lanes">
      <div id="subgrid">
        <div>A</div>
      </div>
      <div>C</div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));
  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), kIndefiniteSize),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);
  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);
  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});

  const GridLineResolver line_resolver(node.Style(), /*auto_repetitions=*/0);
  auto sizing_tree =
      BuildGridSizingTree<GridLanesLayoutAlgorithm>(algorithm, line_resolver);

  // The subgrid item should not have `must_consider*` flags set since it only
  // subgrids rows but the grid-lanes axis is columns.
  const auto& subgrid_item = sizing_tree.GetGridItems().At(0);
  EXPECT_FALSE(subgrid_item.must_consider_grid_items_for_column_sizing);
  EXPECT_FALSE(subgrid_item.must_consider_grid_items_for_row_sizing);

  // A child that only subgrids rows should not produce subgridded items
  // when the grid-lanes axis is columns.
  wtf_size_t total_count = 0;
  wtf_size_t subgridded_count = 0;
  for (const auto& item : sizing_tree.GetGridItems().IncludeSubgriddedItems()) {
    if (item.is_subgridded_to_parent_grid) {
      ++subgridded_count;
    }
    ++total_count;
  }
  EXPECT_EQ(total_count, 2u);
  EXPECT_EQ(subgridded_count, 0u);
}

TEST_F(GridLanesLayoutAlgorithmTest, SubgridColumnsIgnoredInRowGridLanes) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-template-rows: 50px 50px 50px;
      grid-lanes-direction: row;
    }
    #subgrid {
      display: grid;
      grid-template-columns: subgrid;
      grid-row: 1 / 3;
    }
    </style>
    <div id="grid-lanes">
      <div id="subgrid">
        <div>A</div>
      </div>
      <div>C</div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));
  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), kIndefiniteSize),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);
  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);
  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});

  const GridLineResolver line_resolver(node.Style(), /*auto_repetitions=*/0);
  auto sizing_tree =
      BuildGridSizingTree<GridLanesLayoutAlgorithm>(algorithm, line_resolver);

  // The subgrid item should not have `must_consider*` flags set since it only
  // subgrids columns but the grid-lanes axis is rows.
  const auto& subgrid_item = sizing_tree.GetGridItems().At(0);
  EXPECT_FALSE(subgrid_item.must_consider_grid_items_for_column_sizing);
  EXPECT_FALSE(subgrid_item.must_consider_grid_items_for_row_sizing);

  // A child that only subgrids columns should not produce subgridded items
  // when the grid-lanes axis is rows.
  wtf_size_t total_count = 0;
  wtf_size_t subgridded_count = 0;
  for (const auto& item : sizing_tree.GetGridItems().IncludeSubgriddedItems()) {
    if (item.is_subgridded_to_parent_grid) {
      ++subgridded_count;
    }
    ++total_count;
  }
  EXPECT_EQ(total_count, 2u);
  EXPECT_EQ(subgridded_count, 0u);
}

TEST_F(GridLanesLayoutAlgorithmTest, OrthogonalAppendSubgriddedItemsColumns) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-template-columns: 100px 100px 100px;
    }
    #subgrid {
      display: grid;
      writing-mode: vertical-rl;
      grid-template-rows: subgrid;
      grid-column: 2 / 4;
    }
    </style>
    <div id="grid-lanes">
      <div id="subgrid">
        <div>A</div>
        <div>B</div>
      </div>
      <div>C</div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));
  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), kIndefiniteSize),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);
  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);
  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});

  const GridLineResolver line_resolver(node.Style(), /*auto_repetitions=*/0);
  auto sizing_tree =
      BuildGridSizingTree<GridLanesLayoutAlgorithm>(algorithm, line_resolver);

  // The orthogonal subgrid item should have `must_consider*` flags set for
  // columns (the grid axis of this grid-lanes container).
  const auto& subgrid_item = sizing_tree.GetGridItems().At(0);
  EXPECT_TRUE(subgrid_item.must_consider_grid_items_for_column_sizing);
  EXPECT_FALSE(subgrid_item.must_consider_grid_items_for_row_sizing);
  EXPECT_TRUE(subgrid_item.IsSubgrid());
  EXPECT_FALSE(subgrid_item.is_parallel_with_root_grid);

  // After building the sizing tree, we should have 2 original items + 2
  // subgridded items.
  wtf_size_t total_count = 0;
  wtf_size_t subgridded_count = 0;
  for (const auto& item : sizing_tree.GetGridItems().IncludeSubgriddedItems()) {
    if (item.is_subgridded_to_parent_grid) {
      ++subgridded_count;
    }
    ++total_count;
  }
  EXPECT_EQ(total_count, 4u);
  EXPECT_EQ(subgridded_count, 2u);
}

TEST_F(GridLanesLayoutAlgorithmTest, OrthogonalAppendSubgriddedItemsRows) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-template-rows: 50px 50px 50px;
      grid-lanes-direction: row;
    }
    #subgrid {
      display: grid;
      writing-mode: vertical-rl;
      grid-template-columns: subgrid;
      grid-row: 1 / 3;
    }
    </style>
    <div id="grid-lanes">
      <div id="subgrid">
        <div>A</div>
        <div>B</div>
      </div>
      <div>C</div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));
  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), kIndefiniteSize),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);
  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);
  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});

  const GridLineResolver line_resolver(node.Style(), /*auto_repetitions=*/0);
  auto sizing_tree =
      BuildGridSizingTree<GridLanesLayoutAlgorithm>(algorithm, line_resolver);

  // The orthogonal subgrid item should have `must_consider*` flags set for
  // rows (the grid axis of this grid-lanes container).
  const auto& subgrid_item = sizing_tree.GetGridItems().At(0);
  EXPECT_FALSE(subgrid_item.must_consider_grid_items_for_column_sizing);
  EXPECT_TRUE(subgrid_item.must_consider_grid_items_for_row_sizing);
  EXPECT_TRUE(subgrid_item.IsSubgrid());
  EXPECT_FALSE(subgrid_item.is_parallel_with_root_grid);

  // After building the sizing tree, we should have 2 original items + 2
  // subgridded items.
  wtf_size_t total_count = 0;
  wtf_size_t subgridded_count = 0;
  for (const auto& item : sizing_tree.GetGridItems().IncludeSubgriddedItems()) {
    if (item.is_subgridded_to_parent_grid) {
      ++subgridded_count;
    }
    ++total_count;
  }
  EXPECT_EQ(total_count, 4u);
  EXPECT_EQ(subgridded_count, 2u);
}

// Auto-placed subgrid: subgridded items should be marked as auto-placed
// because the subgrid's position is not known at sizing time.
TEST_F(GridLanesLayoutAlgorithmTest, AutoPlacedSubgriddedItemsAreAutoPlaced) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-template-columns: 100px 100px 100px;
    }
    #subgrid {
      display: grid;
      grid-template-columns: subgrid;
      grid-column: span 2;
    }
      #placed { grid-column: 1 / 2; }
    </style>
    <div id="grid-lanes">
      <div id="subgrid">
        <div>A</div>
        <div id="placed">B</div>
      </div>
      <div>C</div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));
  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), kIndefiniteSize),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);
  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);
  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});

  const GridLineResolver line_resolver(node.Style(), /*auto_repetitions=*/0);
  auto sizing_tree =
      BuildGridSizingTree<GridLanesLayoutAlgorithm>(algorithm, line_resolver);

  // The subgrid item should have `must_consider*` flags set for columns (the
  // grid axis of this grid-lanes container).
  const auto& subgrid_item = sizing_tree.GetGridItems().At(0);
  EXPECT_TRUE(subgrid_item.must_consider_grid_items_for_column_sizing);
  EXPECT_FALSE(subgrid_item.must_consider_grid_items_for_row_sizing);
  EXPECT_TRUE(subgrid_item.IsSubgrid());

  const auto grid_axis_direction = node.Style().GridLanesTrackSizingDirection();

  wtf_size_t subgridded_count = 0;
  for (const auto& item : sizing_tree.GetGridItems().IncludeSubgriddedItems()) {
    if (item.is_subgridded_to_parent_grid) {
      EXPECT_TRUE(item.is_auto_placed);
      EXPECT_TRUE(
          item.resolved_position.Span(grid_axis_direction).IsIndefinite());
      ++subgridded_count;
    }
  }
  EXPECT_EQ(subgridded_count, 2u);
}

// Definite subgrid with an auto-placed child: the subgrid's placement
// algorithm resolves all children to definite positions, so both the
// explicitly placed and auto-placed children end up with translated spans.
TEST_F(GridLanesLayoutAlgorithmTest,
       DefiniteSubgridChildrenAreExplicitlyPlaced) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-template-columns: 100px 100px 100px;
    }
    #subgrid {
      display: grid;
      grid-template-columns: subgrid;
      grid-column: 2 / 4;
    }
    #placed { grid-column: 1 / 2; }
    </style>
    <div id="grid-lanes">
      <div id="subgrid">
        <div>A</div>
        <div id="placed">B</div>
      </div>
      <div>C</div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));
  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), kIndefiniteSize),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);
  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);
  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});

  const GridLineResolver line_resolver(node.Style(), /*auto_repetitions=*/0);
  auto sizing_tree =
      BuildGridSizingTree<GridLanesLayoutAlgorithm>(algorithm, line_resolver);

  // The subgrid item should have `must_consider*` flags set for columns (the
  // grid axis of this grid-lanes container).
  const auto& subgrid_item = sizing_tree.GetGridItems().At(0);
  EXPECT_TRUE(subgrid_item.must_consider_grid_items_for_column_sizing);
  EXPECT_FALSE(subgrid_item.must_consider_grid_items_for_row_sizing);
  EXPECT_TRUE(subgrid_item.IsSubgrid());

  const auto grid_axis_direction = node.Style().GridLanesTrackSizingDirection();

  // Both items end up with definite positions after the subgrid's placement
  // algorithm runs. Item B has grid-column: 1 / 2 (explicitly placed), and
  // item A is resolved by the subgrid's auto-placement.
  wtf_size_t subgridded_count = 0;
  for (const auto& item : sizing_tree.GetGridItems().IncludeSubgriddedItems()) {
    if (!item.is_subgridded_to_parent_grid) {
      continue;
    }
    const auto& span = item.resolved_position.Span(grid_axis_direction);
    EXPECT_TRUE(span.IsTranslatedDefinite());
    EXPECT_FALSE(item.is_auto_placed);
    ++subgridded_count;
  }
  EXPECT_EQ(subgridded_count, 2u);
}

// Subgrid with opposite direction (RTL): the subgridded items' spans should be
// reversed within the subgrid range when translated to the parent grid's
// coordinate space.
TEST_F(GridLanesLayoutAlgorithmTest,
       OppositeDirectionSubgridReversesChildSpans) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-template-columns: 100px 100px 100px;
    }
    #subgrid {
      display: grid;
      direction: rtl;
      grid-template-columns: subgrid;
      grid-column: 1 / 4;
    }
    #child { grid-column: 1 / 2; }
    </style>
    <div id="grid-lanes">
      <div id="subgrid">
        <div id="child">A</div>
      </div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));
  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), kIndefiniteSize),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);
  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);
  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});

  const GridLineResolver line_resolver(node.Style(), /*auto_repetitions=*/0);
  auto sizing_tree =
      BuildGridSizingTree<GridLanesLayoutAlgorithm>(algorithm, line_resolver);

  // The subgrid item should have `must_consider*` flags set for columns (the
  // grid axis of this grid-lanes container).
  const auto& subgrid_item = sizing_tree.GetGridItems().At(0);
  EXPECT_TRUE(subgrid_item.must_consider_grid_items_for_column_sizing);
  EXPECT_FALSE(subgrid_item.must_consider_grid_items_for_row_sizing);
  EXPECT_TRUE(subgrid_item.IsSubgrid());

  const auto grid_axis_direction = node.Style().GridLanesTrackSizingDirection();

  // The child is at subgrid column 1/2 (0-based: 0-1). With opposite direction,
  // this should be reversed within the 3-track subgrid: position becomes 2-3.
  for (const auto& item : sizing_tree.GetGridItems().IncludeSubgriddedItems()) {
    if (!item.is_subgridded_to_parent_grid) {
      continue;
    }
    const auto& span = item.resolved_position.Span(grid_axis_direction);
    EXPECT_TRUE(span.IsTranslatedDefinite());
    EXPECT_EQ(span.StartLine(), 2u);
    EXPECT_EQ(span.EndLine(), 3u);
  }
}

TEST_F(GridLanesLayoutAlgorithmTest,
       OrthogonalSubgridColumnsIgnoredInColumnGridLanes) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-template-columns: 100px 100px 100px;
    }
    #subgrid {
      display: grid;
      writing-mode: vertical-rl;
      grid-template-columns: subgrid;
      grid-column: 1 / 3;
    }
    </style>
    <div id="grid-lanes">
      <div id="subgrid">
        <div>A</div>
      </div>
      <div>C</div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));
  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), kIndefiniteSize),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);
  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);
  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});

  const GridLineResolver line_resolver(node.Style(), /*auto_repetitions=*/0);
  auto sizing_tree =
      BuildGridSizingTree<GridLanesLayoutAlgorithm>(algorithm, line_resolver);

  // The orthogonal subgrid item should not have `must_consider*` flags set
  // since its `grid-template-columns: subgrid` maps to the parent's row axis
  // after the writing-mode swap, not columns.
  const auto& subgrid_item = sizing_tree.GetGridItems().At(0);
  EXPECT_FALSE(subgrid_item.must_consider_grid_items_for_column_sizing);
  EXPECT_FALSE(subgrid_item.must_consider_grid_items_for_row_sizing);

  // An orthogonal child with `grid-template-columns: subgrid` maps to the
  // parent's row axis after the writing-mode swap, not columns. Since the
  // grid-lanes axis is columns, no subgridded items should be produced.
  wtf_size_t total_count = 0;
  wtf_size_t subgridded_count = 0;
  for (const auto& item : sizing_tree.GetGridItems().IncludeSubgriddedItems()) {
    if (item.is_subgridded_to_parent_grid) {
      ++subgridded_count;
    }
    ++total_count;
  }
  EXPECT_EQ(total_count, 2u);
  EXPECT_EQ(subgridded_count, 0u);
}

TEST_F(GridLanesLayoutAlgorithmTest,
       OrthogonalSubgridRowsIgnoredInRowGridLanes) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-template-rows: 50px 50px 50px;
      grid-lanes-direction: row;
    }
    #subgrid {
      display: grid;
      writing-mode: vertical-rl;
      grid-template-rows: subgrid;
      grid-row: 1 / 3;
    }
    </style>
    <div id="grid-lanes">
      <div id="subgrid">
        <div>A</div>
      </div>
      <div>C</div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));
  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), kIndefiniteSize),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);
  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);
  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});

  const GridLineResolver line_resolver(node.Style(), /*auto_repetitions=*/0);
  auto sizing_tree =
      BuildGridSizingTree<GridLanesLayoutAlgorithm>(algorithm, line_resolver);

  // The orthogonal subgrid item should not have `must_consider*` flags set
  // since its `grid-template-rows: subgrid` maps to the parent's column axis
  // after the writing-mode swap, not rows.
  const auto& subgrid_item = sizing_tree.GetGridItems().At(0);
  EXPECT_FALSE(subgrid_item.must_consider_grid_items_for_column_sizing);
  EXPECT_FALSE(subgrid_item.must_consider_grid_items_for_row_sizing);

  // An orthogonal child with `grid-template-rows: subgrid` maps to the
  // parent's column axis after the writing-mode swap, not rows. Since the
  // grid-lanes axis is rows, no subgridded items should be produced.
  wtf_size_t total_count = 0;
  wtf_size_t subgridded_count = 0;
  for (const auto& item : sizing_tree.GetGridItems().IncludeSubgriddedItems()) {
    if (item.is_subgridded_to_parent_grid) {
      ++subgridded_count;
    }
    ++total_count;
  }
  EXPECT_EQ(total_count, 2u);
  EXPECT_EQ(subgridded_count, 0u);
}

// Two definite subgrids at different positions, each with a child that has the
// same grid-column style. The subgridded children should produce virtual items
// at their respective subgrid positions, not be merged into one group.
TEST_F(GridLanesLayoutAlgorithmTest,
       DefiniteSubgridsAtDifferentPositionsProduceSeparateVirtualItems) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    body { font: 10px/1 Ahem }
    #grid-lanes {
      display: grid-lanes;
      grid-template-columns: repeat(4, auto);
    }
    .subgrid {
      display: grid;
      grid-template-columns: subgrid;
    }
    #s1 { grid-column: 1 / 3; }
    #s2 { grid-column: 3 / 5; }
    .child { grid-column: 1 / 2; }
    </style>
    <div id="grid-lanes">
      <div id="s1" class="subgrid">
        <div class="child">XXXX</div>
      </div>
      <div id="s2" class="subgrid">
        <div class="child">XXXXXXXX</div>
      </div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));
  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), kIndefiniteSize),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);
  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);
  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  // The two subgridded children have the same style (grid-column: 1 / 2)
  // but are at different positions in the parent grid (column 1 vs column 3).
  // They should produce separate virtual items. The subgrids themselves also
  // produce virtual items with zero contributions.
  const auto item_count = VirtualItemCount();
  EXPECT_EQ(item_count, 4u);

  for (wtf_size_t i = 0; i < item_count; ++i) {
    LayoutUnit expected_max_size;
    const auto& span = VirtualItemSpan(i);
    if (span == GridSpan::TranslatedDefiniteGridSpan(0, 1)) {
      expected_max_size = LayoutUnit(40);
    } else if (span == GridSpan::TranslatedDefiniteGridSpan(2, 3)) {
      expected_max_size = LayoutUnit(80);
    }
    EXPECT_EQ(MaxContentContribution(i), expected_max_size);
  }
}

// An auto-placed subgrid's children should be treated as auto-placed for
// virtual item grouping.
TEST_F(GridLanesLayoutAlgorithmTest,
       AutoPlacedSubgridChildrenAreAutoPlacedForVirtualItems) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    body { font: 10px/1 Ahem }
    #grid-lanes {
      display: grid-lanes;
      grid-template-columns: repeat(3, auto);
    }
    #subgrid {
      display: grid;
      grid-template-columns: subgrid;
      grid-column: span 2;
    }
    #placed { grid-column: 1 / 2; }
    </style>
    <div id="grid-lanes">
      <div id="subgrid">
        <div id="placed">XXXX</div>
      </div>
      <div>XX</div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));
  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), kIndefiniteSize),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);
  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);
  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  // The auto-placed subgrid's child (span 1) should produce virtual items at
  // every track of the parent grid, not just at position 0. The parent has 3
  // tracks, so we expect span-1 virtual items at [0,1), [1,2), [2,3), plus
  // span-2 virtual items from the subgrid itself at [0,2) and [1,3).
  const auto item_count = VirtualItemCount();
  EXPECT_EQ(item_count, 5u);

  const GridSpan expected_spans[] = {
      GridSpan::TranslatedDefiniteGridSpan(0, 1),
      GridSpan::TranslatedDefiniteGridSpan(1, 2),
      GridSpan::TranslatedDefiniteGridSpan(2, 3),
      GridSpan::TranslatedDefiniteGridSpan(0, 2),
      GridSpan::TranslatedDefiniteGridSpan(1, 3),
  };
  wtf_size_t matched = 0;
  for (const auto& expected : expected_spans) {
    for (wtf_size_t i = 0; i < item_count; ++i) {
      if (VirtualItemSpan(i) == expected) {
        ++matched;
        break;
      }
    }
  }
  EXPECT_EQ(matched, std::size(expected_spans));
}

// Definite subgrid with an auto-placed child: the subgrid's auto-placement
// algorithm resolves the child to a definite position within the subgrid, so
// by the time we group items for virtual items, the child should have a
// definite translated span at the correct parent position.
TEST_F(GridLanesLayoutAlgorithmTest,
       DefiniteSubgridAutoPlacedChildProducesDefiniteVirtualItem) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    body { font: 10px/1 Ahem }
    #grid-lanes {
      display: grid-lanes;
      grid-template-columns: repeat(4, auto);
    }
    #subgrid {
      display: grid;
      grid-template-columns: subgrid;
      grid-column: 2 / 4;
    }
    #placed { grid-column: 1 / 2; }
    </style>
    <div id="grid-lanes">
      <div id="subgrid">
        <div id="placed">XXXX</div>
        <div>XXXXXXXX</div>
      </div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));
  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), kIndefiniteSize),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);
  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);
  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  // The subgrid is at columns [1, 3) in the parent (grid-column: 2 / 4).
  // #placed is at subgrid column 1/2 → parent column [1, 2).
  // The auto-placed child is resolved to subgrid column 2/3 → parent [2, 3).
  // Both should produce virtual items at their definite parent positions.
  // The subgrid itself also produces a virtual item at [1, 3) with zero
  // contribution.
  const auto item_count = VirtualItemCount();
  EXPECT_EQ(item_count, 3u);

  for (wtf_size_t i = 0; i < item_count; ++i) {
    LayoutUnit expected_max_size;
    const auto& span = VirtualItemSpan(i);
    if (span == GridSpan::TranslatedDefiniteGridSpan(1, 2)) {
      expected_max_size = LayoutUnit(40);
    } else if (span == GridSpan::TranslatedDefiniteGridSpan(2, 3)) {
      expected_max_size = LayoutUnit(80);
    }
    EXPECT_EQ(MaxContentContribution(i), expected_max_size);
  }
}

}  // namespace blink
