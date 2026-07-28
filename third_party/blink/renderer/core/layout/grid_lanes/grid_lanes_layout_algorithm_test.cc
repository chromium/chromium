// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid_lanes/grid_lanes_layout_algorithm.h"

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/layout/base_layout_algorithm_test.h"
#include "third_party/blink/renderer/core/layout/gap/gap_geometry.h"
#include "third_party/blink/renderer/core/layout/grid/grid_item.h"
#include "third_party/blink/renderer/core/layout/grid/grid_layout_utils.h"
#include "third_party/blink/renderer/core/layout/grid/grid_sizing_tree.h"
#include "third_party/blink/renderer/core/layout/grid/grid_track_collection.h"
#include "third_party/blink/renderer/core/layout/grid/grid_track_sizing_algorithm.h"
#include "third_party/blink/renderer/core/layout/grid_lanes/grid_lanes_break_token_data.h"
#include "third_party/blink/renderer/core/layout/grid_lanes/grid_lanes_running_positions.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

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

  GridLanesDataVector GetFragmentedGridLanesData() {
    // Run the grid-lanes algorithm directly in a fragmentation context. The
    // fragmentation pass currently only collects initial item offsets into the
    // break token and does not add child layout results, so advancing to
    // pre-paint would fail its layout-state checks.
    //
    // TODO(almaher): Once grid-lanes item fragmentation is supported, test this
    // with a multicolumn container through a normal full lifecycle.
    AdvanceToLayoutPhase();
    BlockNode node(GetLayoutBoxByElementId("grid-lanes"));
    const auto space = ConstructBlockLayoutTestConstraintSpace(
        {WritingMode::kHorizontalTb, TextDirection::kLtr},
        LogicalSize(LayoutUnit(300), kIndefiniteSize),
        /*stretch_inline_size_if_auto=*/true,
        node.CreatesNewFormattingContext(),
        /*fragmentainer_space_available=*/LayoutUnit(30));
    const auto fragment_geometry =
        CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

    GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
    const LayoutResult* result = algorithm.Layout();
    const auto* fragment =
        To<PhysicalBoxFragment>(&result->GetPhysicalFragment());
    const BlockBreakToken* break_token = fragment->GetBreakToken();
    CHECK(break_token);
    return To<GridLanesBreakTokenData>(break_token->TokenData())->grid_lanes;
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
  bool must_invalidate_placement_cache = false;
  auto* grid_lanes_items =
      node.ConstructGridItems(line_resolver, &must_invalidate_placement_cache);
  EXPECT_FALSE(must_invalidate_placement_cache);

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

// Non-subgrid grid-lanes items should only be marked as auto-placed if they
// have an indefinite span in the grid axis.
TEST_F(GridLanesLayoutAlgorithmTest, GridLanesAutoPlacedItems) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-template-columns: 100px 100px 100px 100px;
    }
    </style>
    <div id="grid-lanes">
      <div style="grid-column: 1 / 3"></div>
      <div style="grid-column: span 2"></div>
      <div style="grid-column: 2 / 4"></div>
      <div></div>
    </div>
  )HTML");

  GridLanesNode node(GetLayoutBoxByElementId("grid-lanes"));

  const GridLineResolver line_resolver(node.Style(), /*auto_repetitions=*/0);
  bool must_invalidate_placement_cache = false;
  auto* grid_lanes_items =
      node.ConstructGridItems(line_resolver, &must_invalidate_placement_cache,
                              /*parent_is_auto_placed=*/false);
  EXPECT_FALSE(must_invalidate_placement_cache);

  ASSERT_EQ(grid_lanes_items->Size(), 4u);
  EXPECT_FALSE(grid_lanes_items->At(0).is_auto_placed);
  EXPECT_TRUE(grid_lanes_items->At(1).is_auto_placed);
  EXPECT_FALSE(grid_lanes_items->At(2).is_auto_placed);
  EXPECT_TRUE(grid_lanes_items->At(3).is_auto_placed);
}

// When the grid-lanes container is itself an auto-placed subgrid (e.g.
// nested in a larger grid-lanes ancestor whose tracks aren't resolved until
// placement runs after track sizing), every child must be marked
// auto-placed regardless of its own placement.
TEST_F(GridLanesLayoutAlgorithmTest,
       ConstructGridLanesItemsParentAutoPlacedMarksAll) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-template-columns: 100px 100px 100px 100px;
    }
    </style>
    <div id="grid-lanes">
      <div style="grid-column: 1 / 3"></div>
      <div style="grid-column: span 2"></div>
      <div style="grid-column: 2 / 4"></div>
      <div></div>
    </div>
  )HTML");

  GridLanesNode node(GetLayoutBoxByElementId("grid-lanes"));

  const GridLineResolver line_resolver(node.Style(), /*auto_repetitions=*/0);
  bool must_invalidate_placement_cache = false;
  auto* grid_lanes_items =
      node.ConstructGridItems(line_resolver, &must_invalidate_placement_cache,
                              /*parent_is_auto_placed=*/true);
  EXPECT_FALSE(must_invalidate_placement_cache);

  ASSERT_EQ(grid_lanes_items->Size(), 4u);
  for (const auto& grid_lanes_item : *grid_lanes_items) {
    EXPECT_TRUE(grid_lanes_item.is_auto_placed);
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
  bool must_invalidate_placement_cache = false;
  const auto* grid_lanes_items =
      node.ConstructGridItems(line_resolver, &must_invalidate_placement_cache);
  EXPECT_FALSE(must_invalidate_placement_cache);

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
  bool must_invalidate_placement_cache = false;
  const auto* grid_lanes_items =
      node.ConstructGridItems(line_resolver, &must_invalidate_placement_cache);
  EXPECT_FALSE(must_invalidate_placement_cache);

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

// A subgrid nested inside an auto-placed subgrid of a grid-lanes container,
// with no explicit placement of its own, should be marked auto-placed.
TEST_F(GridLanesLayoutAlgorithmTest,
       NestedSubgridInAutoPlacedSubgridIsAutoPlaced) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-template-columns: 100px 100px 100px;
    }
    #outer-subgrid {
      display: grid;
      grid-template-columns: subgrid;
      grid-column: span 2;
    }
    #inner-subgrid {
      display: grid;
      grid-template-columns: subgrid;
      grid-column: span 1;
    }
    </style>
    <div id="grid-lanes">
      <div id="outer-subgrid">
        <div id="inner-subgrid"></div>
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

  // Outer subgrid is auto-placed because it has no explicit grid-column in
  // the grid-lanes axis.
  const auto& outer_subgrid_item = sizing_tree.GetGridItems().At(0);
  ASSERT_TRUE(outer_subgrid_item.IsSubgrid());
  EXPECT_TRUE(outer_subgrid_item.is_auto_placed);

  // Inner subgrid should be marked auto-placed because the outer subgrid is
  // auto-placed and the inner subgrid's own placement is indefinite.
  BlockNode outer_node(GetLayoutBoxByElementId("outer-subgrid"));
  const auto outer_index = sizing_tree.LookupSubgridIndex(outer_node);
  const auto& outer_items = sizing_tree.GetGridItems(outer_index);
  ASSERT_EQ(outer_items.Size(), 1u);
  const auto& inner_subgrid_item = outer_items.At(0);
  EXPECT_TRUE(inner_subgrid_item.IsSubgrid());
  EXPECT_TRUE(inner_subgrid_item.is_auto_placed);
}

// A nested subgrid with an explicit placement inside an auto-placed subgrid
// should still be marked auto-placed: even though its position within the
// outer subgrid is explicit, the outer subgrid's own position in the
// grid-lanes ancestor's tracks is unresolved, so this item's final position
// is unknown.
TEST_F(GridLanesLayoutAlgorithmTest,
       ExplicitlyPlacedNestedSubgridInAutoPlacedSubgridIsAutoPlaced) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-template-columns: 100px 100px 100px;
    }
    #outer-subgrid {
      display: grid;
      grid-template-columns: subgrid;
      grid-column: span 2;
    }
    #inner-subgrid {
      display: grid;
      grid-template-columns: subgrid;
      grid-column: 1 / 2;
    }
    </style>
    <div id="grid-lanes">
      <div id="outer-subgrid">
        <div id="inner-subgrid"></div>
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

  const auto& outer_subgrid_item = sizing_tree.GetGridItems().At(0);
  EXPECT_TRUE(outer_subgrid_item.is_auto_placed);

  BlockNode outer_node(GetLayoutBoxByElementId("outer-subgrid"));
  const auto outer_index = sizing_tree.LookupSubgridIndex(outer_node);
  const auto& outer_items = sizing_tree.GetGridItems(outer_index);
  ASSERT_EQ(outer_items.Size(), 1u);
  const auto& inner_subgrid_item = outer_items.At(0);
  EXPECT_TRUE(inner_subgrid_item.IsSubgrid());
  EXPECT_TRUE(inner_subgrid_item.is_auto_placed);
}

// A subgrid nested inside an explicitly-placed subgrid of a grid-lanes
// container should NOT be marked auto-placed, because the outer subgrid is
// not auto-placed.
TEST_F(GridLanesLayoutAlgorithmTest,
       NestedSubgridUnderExplicitOuterSubgridIsNotAutoPlaced) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-template-columns: 100px 100px 100px;
    }
    #outer-subgrid {
      display: grid;
      grid-template-columns: subgrid;
      grid-column: 1 / 3;
    }
    #inner-subgrid {
      display: grid;
      grid-template-columns: subgrid;
      grid-column: span 1;
    }
    </style>
    <div id="grid-lanes">
      <div id="outer-subgrid">
        <div id="inner-subgrid"></div>
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

  // Outer subgrid has explicit grid-column so it is NOT auto-placed.
  const auto& outer_subgrid_item = sizing_tree.GetGridItems().At(0);
  ASSERT_TRUE(outer_subgrid_item.IsSubgrid());
  EXPECT_FALSE(outer_subgrid_item.is_auto_placed);

  BlockNode outer_node(GetLayoutBoxByElementId("outer-subgrid"));
  const auto outer_index = sizing_tree.LookupSubgridIndex(outer_node);
  const auto& outer_items = sizing_tree.GetGridItems(outer_index);
  ASSERT_EQ(outer_items.Size(), 1u);
  const auto& inner_subgrid_item = outer_items.At(0);
  EXPECT_FALSE(inner_subgrid_item.is_auto_placed);
}

// Three levels of nested auto-placed subgrids: the auto-placed property
// should chain all the way down.
TEST_F(GridLanesLayoutAlgorithmTest,
       DeepNestedAutoPlacedSubgridsAreAllAutoPlaced) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-template-columns: 100px 100px 100px;
    }
    .subgrid {
      display: grid;
      grid-template-columns: subgrid;
    }
    #outer { grid-column: span 3; }
    #middle { grid-column: span 2; }
    #inner { grid-column: span 1; }
    </style>
    <div id="grid-lanes">
      <div id="outer" class="subgrid">
        <div id="middle" class="subgrid">
          <div id="inner" class="subgrid"></div>
        </div>
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

  // Level 1: outer (direct child of grid-lanes).
  const auto& outer_item = sizing_tree.GetGridItems().At(0);
  EXPECT_TRUE(outer_item.is_auto_placed);

  // Level 2: middle (inside outer).
  BlockNode outer_node(GetLayoutBoxByElementId("outer"));
  const auto& middle_items =
      sizing_tree.GetGridItems(sizing_tree.LookupSubgridIndex(outer_node));
  ASSERT_EQ(middle_items.Size(), 1u);
  const auto& middle_item = middle_items.At(0);
  EXPECT_TRUE(middle_item.is_auto_placed);

  // Level 3: inner (inside middle). Chain should have propagated through.
  BlockNode middle_node(GetLayoutBoxByElementId("middle"));
  const auto& inner_items =
      sizing_tree.GetGridItems(sizing_tree.LookupSubgridIndex(middle_node));
  ASSERT_EQ(inner_items.Size(), 1u);
  const auto& inner_item = inner_items.At(0);
  EXPECT_TRUE(inner_item.is_auto_placed);
}

// All children of an auto-placed subgrid — both implicitly and explicitly
// placed — should be marked auto-placed, since the outer subgrid's position
// in the grid-lanes ancestor is unresolved.
TEST_F(GridLanesLayoutAlgorithmTest,
       AllChildrenOfAutoPlacedSubgridAreAutoPlaced) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-template-columns: 100px 100px 100px;
    }
    #outer-subgrid {
      display: grid;
      grid-template-columns: subgrid;
      grid-column: span 2;
    }
    #placed-leaf { grid-column: 1 / 2; }
    </style>
    <div id="grid-lanes">
      <div id="outer-subgrid">
        <div id="auto-leaf"></div>
        <div id="placed-leaf"></div>
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

  BlockNode outer_node(GetLayoutBoxByElementId("outer-subgrid"));
  const auto& outer_items =
      sizing_tree.GetGridItems(sizing_tree.LookupSubgridIndex(outer_node));
  ASSERT_EQ(outer_items.Size(), 2u);

  for (const auto& item : outer_items) {
    EXPECT_TRUE(item.is_auto_placed);
  }
}

// Changing `grid-lanes-direction` changes the grid axis and therefore where
// items may be placed, so it must invalidate the cached placement.
TEST_F(GridLanesLayoutAlgorithmTest,
       GridLanesDirectionChangeMarksPlacementDirty) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-lanes-direction: column;
      grid-template-columns: 100px 100px;
    }
    </style>
    <div id="grid-lanes">
      <div></div>
      <div></div>
    </div>
  )HTML");

  EXPECT_FALSE(GetLayoutBoxByElementId("grid-lanes")->IsGridPlacementDirty());

  GetElementById("grid-lanes")
      ->SetInlineStyleProperty(CSSPropertyID::kGridLanesDirection, "row");
  GetDocument().UpdateStyleAndLayoutTree();

  EXPECT_TRUE(GetLayoutBoxByElementId("grid-lanes")->IsGridPlacementDirty());
}

// Changing `grid-lanes-pack` changes how auto-placed items are packed into the
// lanes and therefore where items may be placed, so it must invalidate the
// cached placement.
TEST_F(GridLanesLayoutAlgorithmTest, GridLanesPackChangeMarksPlacementDirty) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-lanes-pack: normal;
      grid-template-columns: 100px 100px;
    }
    </style>
    <div id="grid-lanes">
      <div></div>
      <div></div>
    </div>
  )HTML");

  EXPECT_FALSE(GetLayoutBoxByElementId("grid-lanes")->IsGridPlacementDirty());

  GetElementById("grid-lanes")
      ->SetInlineStyleProperty(CSSPropertyID::kGridLanesPack, "dense");
  GetDocument().UpdateStyleAndLayoutTree();

  EXPECT_TRUE(GetLayoutBoxByElementId("grid-lanes")->IsGridPlacementDirty());
}

// With `grid-lanes-direction: normal` the grid axis resolves based on which of
// `grid-template-columns`/`grid-template-rows` is specified. Changing
// which template is specified flips the resolved grid axis even though the
// `grid-lanes-direction` property itself is unchanged, so it must invalidate
// the cached placement.
TEST_F(GridLanesLayoutAlgorithmTest, ResolvedGridAxisFlipMarksPlacementDirty) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-lanes-direction: normal;
      grid-template-rows: 100px 100px;
    }
    </style>
    <div id="grid-lanes">
      <div></div>
      <div></div>
    </div>
  )HTML");

  EXPECT_FALSE(GetLayoutBoxByElementId("grid-lanes")->IsGridPlacementDirty());

  // Removing the rows template makes the grid axis resolve to columns while
  // `grid-lanes-direction` stays `normal`.
  GetElementById("grid-lanes")
      ->SetInlineStyleProperty(CSSPropertyID::kGridTemplateRows, "none");
  GetDocument().UpdateStyleAndLayoutTree();

  EXPECT_TRUE(GetLayoutBoxByElementId("grid-lanes")->IsGridPlacementDirty());
}

TEST_F(GridLanesLayoutAlgorithmTest, GapGeometryColumn) {
  ScopedCSSGapDecorationForTest scoped_gap_decoration(true);
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-template-columns: 80px 120px 90px;
      column-gap: 14px;
      column-rule: 2px solid black;
      border: 5px solid black;
      box-sizing: border-box;
      height: 400px;
    }
    #grid-lanes > div {
      background: lightblue;
      height: 40px;
      outline: 1px solid blue;
    }
    </style>
    <div id="grid-lanes">
      <div>Item 1</div>
      <div>Item 2</div>
      <div>Item 3</div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));
  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), LayoutUnit(400)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);
  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);
  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
  algorithm.Layout();

  const GapGeometry* gap_geometry = algorithm.GetGapGeometry();
  ASSERT_NE(gap_geometry, nullptr);
  EXPECT_EQ(gap_geometry->GetContainerType(),
            GapGeometry::ContainerType::kGridLanes);
  EXPECT_EQ(gap_geometry->GetMainDirection(), kForColumns);
  EXPECT_EQ(gap_geometry->GetInlineGapSize(), LayoutUnit(14));
  EXPECT_EQ(gap_geometry->CrossGapCount(), 0u);

  const auto& main_gaps = gap_geometry->GetMainGaps();
  ASSERT_EQ(main_gaps.size(), 2u);
  EXPECT_EQ(main_gaps[0].GetGapOffset(), LayoutUnit(92));
  EXPECT_EQ(main_gaps[1].GetGapOffset(), LayoutUnit(226));
  EXPECT_EQ(gap_geometry->GetContentInlineStart(), LayoutUnit(5));
  EXPECT_EQ(gap_geometry->GetContentInlineEnd(), LayoutUnit(323));
  EXPECT_EQ(gap_geometry->GetContentBlockStart(), LayoutUnit(5));
  EXPECT_EQ(gap_geometry->GetContentBlockEnd(), LayoutUnit(395));
}

TEST_F(GridLanesLayoutAlgorithmTest, GapGeometryRow) {
  ScopedCSSGapDecorationForTest scoped_gap_decoration(true);
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-template-rows: 90px 130px 110px;
      row-gap: 12px;
      row-rule: 2px solid black;
      border: 5px solid black;
      box-sizing: border-box;
      width: 500px;
    }
    #grid-lanes > div {
      background: lightblue;
      outline: 1px solid blue;
      width: 40px;
    }
    </style>
    <div id="grid-lanes">
      <div>Item 1</div>
      <div>Item 2</div>
      <div>Item 3</div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));
  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(500), LayoutUnit(1000)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);
  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);
  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
  algorithm.Layout();

  const GapGeometry* gap_geometry = algorithm.GetGapGeometry();
  ASSERT_NE(gap_geometry, nullptr);
  EXPECT_EQ(gap_geometry->GetMainDirection(), kForRows);
  EXPECT_EQ(gap_geometry->GetBlockGapSize(), LayoutUnit(12));
  EXPECT_EQ(gap_geometry->CrossGapCount(), 0u);

  const auto& main_gaps = gap_geometry->GetMainGaps();
  ASSERT_EQ(main_gaps.size(), 2u);
  EXPECT_EQ(main_gaps[0].GetGapOffset(), LayoutUnit(101));
  EXPECT_EQ(main_gaps[1].GetGapOffset(), LayoutUnit(243));
  EXPECT_EQ(gap_geometry->GetContentInlineStart(), LayoutUnit(5));
  EXPECT_EQ(gap_geometry->GetContentInlineEnd(), LayoutUnit(495));
  EXPECT_EQ(gap_geometry->GetContentBlockStart(), LayoutUnit(5));
  EXPECT_EQ(gap_geometry->GetContentBlockEnd(), LayoutUnit(359));
}

TEST_F(GridLanesLayoutAlgorithmTest, GapGeometryEmptyExplicitTracks) {
  ScopedCSSGapDecorationForTest scoped_gap_decoration(true);
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-template-columns: repeat(3, 50px);
      column-gap: 10px;
      column-rule: 2px solid black;
      border: 5px solid black;
      box-sizing: border-box;
      height: 100px;
    }
    </style>
    <div id="grid-lanes"></div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));
  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), LayoutUnit(100)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);
  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);
  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
  algorithm.Layout();

  const GapGeometry* gap_geometry = algorithm.GetGapGeometry();
  ASSERT_NE(gap_geometry, nullptr);
  EXPECT_EQ(gap_geometry->MainGapCount(), 2u);
  EXPECT_EQ(gap_geometry->CrossGapCount(), 0u);
  EXPECT_EQ(gap_geometry->GetMainGaps()[0].GetGapOffset(), LayoutUnit(60));
  EXPECT_EQ(gap_geometry->GetMainGaps()[1].GetGapOffset(), LayoutUnit(120));
}

TEST_F(GridLanesLayoutAlgorithmTest, GapGeometrySingleTrack) {
  ScopedCSSGapDecorationForTest scoped_gap_decoration(true);
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-template-columns: 100px;
      column-gap: 10px;
      column-rule: 2px solid black;
      border: 5px solid black;
      box-sizing: border-box;
      height: 100px;
    }
    </style>
    <div id="grid-lanes"></div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));
  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), LayoutUnit(100)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);
  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);
  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
  algorithm.Layout();

  EXPECT_EQ(algorithm.GetGapGeometry(), nullptr);
}

TEST_F(GridLanesLayoutAlgorithmTest, GapGeometryRequiresGapRule) {
  ScopedCSSGapDecorationForTest scoped_gap_decoration(true);
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-template-columns: 100px 100px;
      column-gap: 20px;
      border: 5px solid black;
      box-sizing: border-box;
      height: 100px;
    }
    </style>
    <div id="grid-lanes"></div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));
  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), LayoutUnit(100)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);
  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);
  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
  algorithm.Layout();

  EXPECT_EQ(algorithm.GetGapGeometry(), nullptr);
}

TEST_F(GridLanesLayoutAlgorithmTest, GapGeometryCollapsedAutoFitTracks) {
  ScopedCSSGapDecorationForTest scoped_gap_decoration(true);
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-template-columns: repeat(auto-fit, 100px);
      column-gap: 10px;
      column-rule: 2px solid black;
      border: 5px solid black;
      box-sizing: border-box;
    }
    #grid-lanes > div {
      background: lightblue;
      height: 100px;
      outline: 1px solid blue;
    }
    </style>
    <div id="grid-lanes">
      <div>Item 1</div>
      <div>Item 2</div>
      <div>Item 3</div>
      <div>Item 4</div>
      <div>Item 5</div>
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
  algorithm.Layout();

  const GapGeometry* gap_geometry = algorithm.GetGapGeometry();
  ASSERT_NE(gap_geometry, nullptr);
  ASSERT_EQ(gap_geometry->MainGapCount(), 4u);
  EXPECT_EQ(gap_geometry->CrossGapCount(), 0u);
  for (const auto& main_gap : gap_geometry->GetMainGaps()) {
    EXPECT_NE(main_gap.GetGapOffset(), LayoutUnit::Max());
  }
}

TEST_F(GridLanesLayoutAlgorithmTest, GapGeometryGridAxisAlignment) {
  ScopedCSSGapDecorationForTest scoped_gap_decoration(true);
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-template-columns: 100px 100px;
      column-gap: 20px;
      column-rule: 2px solid black;
      justify-content: center;
      border: 5px solid black;
      box-sizing: border-box;
      width: 300px;
      height: 100px;
    }
    #grid-lanes > div {
      background: lightblue;
      height: 40px;
      outline: 1px solid blue;
    }
    </style>
    <div id="grid-lanes">
      <div>Item 1</div>
      <div>Item 2</div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));
  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(300), LayoutUnit(100)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);
  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);
  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
  algorithm.Layout();

  const GapGeometry* gap_geometry = algorithm.GetGapGeometry();
  ASSERT_NE(gap_geometry, nullptr);
  ASSERT_EQ(gap_geometry->MainGapCount(), 1u);
  EXPECT_EQ(gap_geometry->GetMainGaps()[0].GetGapOffset(), LayoutUnit(150));
  EXPECT_EQ(gap_geometry->GetContentInlineStart(), LayoutUnit(40));
  EXPECT_EQ(gap_geometry->GetContentInlineEnd(), LayoutUnit(260));
  EXPECT_EQ(gap_geometry->GetContentBlockStart(), LayoutUnit(5));
  EXPECT_EQ(gap_geometry->GetContentBlockEnd(), LayoutUnit(95));
}

// Main-gap geometry includes block-end overflow from placed items.
TEST_F(GridLanesLayoutAlgorithmTest, GapGeometryColumnStackingAxisOverflow) {
  ScopedCSSGapDecorationForTest scoped_gap_decoration(true);
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-template-columns: 80px 80px;
      column-gap: 10px;
      row-gap: 6px;
      column-rule: 2px solid black;
      border: 5px solid black;
      box-sizing: border-box;
      height: 60px;
    }
    #grid-lanes > div {
      background: lightblue;
      height: 40px;
    }
    </style>
    <div id="grid-lanes">
      <div style="grid-column: 1"></div>
      <div style="grid-column: 1"></div>
      <div style="grid-column: 2"></div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));
  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(1000), LayoutUnit(60)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);
  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);
  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
  algorithm.Layout();

  const GapGeometry* gap_geometry = algorithm.GetGapGeometry();
  ASSERT_NE(gap_geometry, nullptr);
  ASSERT_EQ(gap_geometry->MainGapCount(), 1u);

  // The second item extends past the 55px content-box end.
  EXPECT_EQ(gap_geometry->GetContentBlockStart(), LayoutUnit(5));
  EXPECT_EQ(gap_geometry->GetContentBlockEnd(), LayoutUnit(91));
}

// Main-gap geometry includes inline-end overflow from placed items.
TEST_F(GridLanesLayoutAlgorithmTest, GapGeometryRowStackingAxisOverflow) {
  ScopedCSSGapDecorationForTest scoped_gap_decoration(true);
  SetBodyInnerHTML(R"HTML(
    <style>
    #grid-lanes {
      display: grid-lanes;
      grid-template-rows: 80px 80px;
      row-gap: 10px;
      column-gap: 6px;
      row-rule: 2px solid black;
      border: 5px solid black;
      box-sizing: border-box;
      width: 60px;
    }
    #grid-lanes > div {
      background: lightblue;
      width: 40px;
    }
    </style>
    <div id="grid-lanes">
      <div style="grid-row: 1"></div>
      <div style="grid-row: 1"></div>
      <div style="grid-row: 2"></div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("grid-lanes"));
  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(60), LayoutUnit(1000)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);
  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);
  GridLanesLayoutAlgorithm algorithm({node, fragment_geometry, space});
  algorithm.Layout();

  const GapGeometry* gap_geometry = algorithm.GetGapGeometry();
  ASSERT_NE(gap_geometry, nullptr);
  ASSERT_EQ(gap_geometry->MainGapCount(), 1u);

  // The second item extends past the 55px content-box end.
  EXPECT_EQ(gap_geometry->GetContentInlineStart(), LayoutUnit(5));
  EXPECT_EQ(gap_geometry->GetContentInlineEnd(), LayoutUnit(91));
}

TEST_F(GridLanesLayoutAlgorithmTest, PopulateGridLanesBreakTokenData) {
  SetBodyInnerHTML(R"HTML(
    <div id="grid-lanes" style="display: grid-lanes; height: 200px;
        grid-template-columns: repeat(3, 100px); flow-tolerance: 0;">
      <div style="grid-column: 1; height: 20px; position: relative;
          top: 10%;"></div>
      <div style="grid-column: 2; height: 30px;"></div>
      <div style="grid-column: 1; height: 20px;"></div>
    </div>
  )HTML");

  const auto grid_lanes = GetFragmentedGridLanesData();

  // The output has one entry for each track. The unused third lane remains
  // null.
  ASSERT_EQ(grid_lanes.size(), 3u);
  ASSERT_EQ(grid_lanes[0]->item_data.size(), 2u);
  ASSERT_EQ(grid_lanes[1]->item_data.size(), 1u);
  EXPECT_FALSE(grid_lanes[2]);

  // Items are stored in placement order within each lane.
  EXPECT_EQ(grid_lanes[0]->item_data[0]->PlacementData().offset,
            LogicalOffset());
  EXPECT_EQ(grid_lanes[1]->item_data[0]->PlacementData().offset,
            (LogicalOffset{LayoutUnit(100), LayoutUnit()}));
  EXPECT_EQ(grid_lanes[0]->item_data[1]->PlacementData().offset,
            (LogicalOffset{LayoutUnit(), LayoutUnit(20)}));

  // Grid-lanes leaves relative positioning to the fragment builder.
  EXPECT_FALSE(grid_lanes[0]->item_data[0]->PlacementData().relative_offset);

  // Every entry represents the start of its single-lane item.
  EXPECT_TRUE(grid_lanes[0]->item_data[0]->is_item_start);
  EXPECT_TRUE(grid_lanes[1]->item_data[0]->is_item_start);
  EXPECT_TRUE(grid_lanes[0]->item_data[1]->is_item_start);
}

TEST_F(GridLanesLayoutAlgorithmTest, PopulateRowGridLanesBreakTokenData) {
  SetBodyInnerHTML(R"HTML(
    <div id="grid-lanes" style="display: grid-lanes; height: 200px;
        grid-template-rows: repeat(2, 50px); flow-tolerance: 0;">
      <div style="grid-row: 1; width: 20px;"></div>
      <div style="grid-row: 2; width: 30px;"></div>
      <div style="grid-row: 1; width: 20px;"></div>
    </div>
  )HTML");

  const auto grid_lanes = GetFragmentedGridLanesData();

  // The output has one entry for each row lane.
  ASSERT_EQ(grid_lanes.size(), 2u);
  ASSERT_EQ(grid_lanes[0]->item_data.size(), 2u);
  ASSERT_EQ(grid_lanes[1]->item_data.size(), 1u);

  // The item in the second row advances in the block axis, while the second
  // item in the first row advances in the inline axis.
  EXPECT_EQ(grid_lanes[0]->item_data[0]->PlacementData().offset,
            LogicalOffset());
  EXPECT_EQ(grid_lanes[1]->item_data[0]->PlacementData().offset,
            (LogicalOffset{LayoutUnit(), LayoutUnit(50)}));
  EXPECT_EQ(grid_lanes[0]->item_data[1]->PlacementData().offset,
            (LogicalOffset{LayoutUnit(20), LayoutUnit()}));
}

TEST_F(GridLanesLayoutAlgorithmTest, PopulateSpannerBreakTokenData) {
  SetBodyInnerHTML(R"HTML(
    <div id="grid-lanes" style="display: grid-lanes; height: 200px;
        grid-template-columns: repeat(3, 100px); flow-tolerance: 0;">
      <div style="grid-column: 1; height: 20px;"></div>
      <div style="grid-column: 1 / span 2; height: 20px;"></div>
      <div style="grid-column: 1 / span 3; height: 20px;"></div>
    </div>
  )HTML");

  const auto grid_lanes = GetFragmentedGridLanesData();

  // The first lane contains every item, the second contains both spanners, and
  // the third contains only the three-lane spanner.
  ASSERT_EQ(grid_lanes.size(), 3u);
  ASSERT_EQ(grid_lanes[0]->item_data.size(), 3u);
  ASSERT_EQ(grid_lanes[1]->item_data.size(), 2u);
  ASSERT_EQ(grid_lanes[2]->item_data.size(), 1u);

  EXPECT_EQ(grid_lanes[0]->item_data[0]->PlacementData().offset,
            LogicalOffset());

  // Each lane spanned by the two-lane item has a distinct entry that references
  // the same grid item and shared placement data.
  EXPECT_NE(grid_lanes[0]->item_data[0]->item.Get(),
            grid_lanes[0]->item_data[1]->item.Get());
  EXPECT_EQ(grid_lanes[0]->item_data[1]->item.Get(),
            grid_lanes[1]->item_data[0]->item.Get());
  EXPECT_EQ(grid_lanes[0]->item_data[1]->grid_lanes_placement_data,
            grid_lanes[1]->item_data[0]->grid_lanes_placement_data);
  EXPECT_EQ(grid_lanes[0]->item_data[1]->PlacementData().offset,
            (LogicalOffset{LayoutUnit(), LayoutUnit(20)}));

  // The three-lane item is likewise shared by all of its lane entries.
  EXPECT_EQ(grid_lanes[0]->item_data[2]->item.Get(),
            grid_lanes[1]->item_data[1]->item.Get());
  EXPECT_EQ(grid_lanes[1]->item_data[1]->item.Get(),
            grid_lanes[2]->item_data[0]->item.Get());
  EXPECT_EQ(grid_lanes[0]->item_data[2]->grid_lanes_placement_data,
            grid_lanes[1]->item_data[1]->grid_lanes_placement_data);
  EXPECT_EQ(grid_lanes[1]->item_data[1]->grid_lanes_placement_data,
            grid_lanes[2]->item_data[0]->grid_lanes_placement_data);
  EXPECT_EQ(grid_lanes[0]->item_data[2]->PlacementData().offset,
            (LogicalOffset{LayoutUnit(), LayoutUnit(40)}));

  // The single-lane item is a start. Only the first lane entry for each spanner
  // is its start.
  EXPECT_TRUE(grid_lanes[0]->item_data[0]->is_item_start);
  EXPECT_TRUE(grid_lanes[0]->item_data[1]->is_item_start);
  EXPECT_FALSE(grid_lanes[1]->item_data[0]->is_item_start);
  EXPECT_TRUE(grid_lanes[0]->item_data[2]->is_item_start);
  EXPECT_FALSE(grid_lanes[1]->item_data[1]->is_item_start);
  EXPECT_FALSE(grid_lanes[2]->item_data[0]->is_item_start);
}

TEST_F(GridLanesLayoutAlgorithmTest, PopulateDensePackedBreakTokenData) {
  SetBodyInnerHTML(R"HTML(
    <div id="grid-lanes" style="display: grid-lanes; height: 200px;
        grid-template-columns: repeat(3, 100px); grid-auto-flow: dense;
        grid-lanes-pack: dense; flow-tolerance: 0;">
      <div style="grid-column: 2; height: 20px;"></div>
      <div style="grid-column: 1 / span 2; height: 20px;"></div>
      <div style="grid-column: 1; height: 10px; align-self: end;"></div>
    </div>
  )HTML");

  const auto grid_lanes = GetFragmentedGridLanesData();

  // The packed item is nested in lane 1, the first item remains direct in lane
  // 2, and the unused third lane remains null.
  ASSERT_EQ(grid_lanes.size(), 3u);
  ASSERT_EQ(grid_lanes[0]->item_data.size(), 1u);
  ASSERT_EQ(grid_lanes[1]->item_data.size(), 2u);
  EXPECT_FALSE(grid_lanes[2]);

  // The packed item is stored under the spanner below its selected opening.
  const auto& spanner = *grid_lanes[0]->item_data[0];
  ASSERT_EQ(spanner.items_packed_above.size(), 1u);
  const auto& packed_item = *spanner.items_packed_above[0];

  // The packed item's shared placement record is found by its collection index
  // and end-aligned within the opening above the spanner.
  EXPECT_EQ(spanner.PlacementData().offset,
            (LogicalOffset{LayoutUnit(), LayoutUnit(20)}));
  EXPECT_EQ(packed_item.PlacementData().offset,
            (LogicalOffset{LayoutUnit(), LayoutUnit(10)}));
  EXPECT_TRUE(packed_item.is_item_start);
}

TEST_F(GridLanesLayoutAlgorithmTest, PopulateDensePackedSpannerBreakTokenData) {
  SetBodyInnerHTML(R"HTML(
    <div id="grid-lanes" style="display: grid-lanes; height: 200px;
        grid-template-columns: repeat(3, 100px); grid-auto-flow: dense;
        grid-lanes-pack: dense; flow-tolerance: 0;">
      <div style="grid-column: 1; height: 20px;"></div>
      <div style="grid-column: 1 / span 2; height: 20px;"></div>
      <div style="grid-column: span 2; height: 10px;"></div>
    </div>
  )HTML");

  const auto grid_lanes = GetFragmentedGridLanesData();

  // The packed spanner is nested under the lane 2 spanner, but remains a direct
  // entry in lane 3 because that opening has no spanner below it.
  ASSERT_EQ(grid_lanes.size(), 3u);
  ASSERT_EQ(grid_lanes[0]->item_data.size(), 2u);
  ASSERT_EQ(grid_lanes[1]->item_data.size(), 1u);
  ASSERT_EQ(grid_lanes[2]->item_data.size(), 1u);

  // Both entries for the packed spanner reference the same grid item and shared
  // placement data.
  const auto& spanner = *grid_lanes[1]->item_data[0];
  ASSERT_EQ(spanner.items_packed_above.size(), 1u);
  const auto& packed_spanner_start = *spanner.items_packed_above[0];
  const auto& packed_spanner_continuation = *grid_lanes[2]->item_data[0];

  EXPECT_EQ(packed_spanner_start.item.Get(),
            packed_spanner_continuation.item.Get());
  EXPECT_EQ(packed_spanner_start.grid_lanes_placement_data,
            packed_spanner_continuation.grid_lanes_placement_data);
  EXPECT_EQ(packed_spanner_start.PlacementData().offset,
            (LogicalOffset{LayoutUnit(100), LayoutUnit()}));
  EXPECT_TRUE(packed_spanner_start.is_item_start);
  EXPECT_FALSE(packed_spanner_continuation.is_item_start);
}

TEST_F(GridLanesLayoutAlgorithmTest,
       PopulateDenseSpannerWithDifferentParentsPerLane) {
  SetBodyInnerHTML(R"HTML(
    <div id="grid-lanes" style="display: grid-lanes; height: 200px;
        grid-template-columns: repeat(4, 100px); grid-auto-flow: dense;
        grid-lanes-pack: dense; flow-tolerance: 0;">
      <div style="grid-column: 1; height: 30px;"></div>
      <div style="grid-column: 4; height: 30px;"></div>
      <div style="grid-column: 1 / span 2; height: 20px;"></div>
      <div style="grid-column: 3 / span 2; height: 20px;"></div>
      <div style="grid-column: 2 / span 2; height: 10px;"></div>
      <div style="grid-column: 2 / span 2; height: 5px;"></div>
    </div>
  )HTML");

  const auto grid_lanes = GetFragmentedGridLanesData();

  // Each outer lane contains its initial item and parent spanner. Each inner
  // lane contains one direct parent spanner; the packed spanners are nested.
  ASSERT_EQ(grid_lanes.size(), 4u);
  ASSERT_EQ(grid_lanes[0]->item_data.size(), 2u);
  ASSERT_EQ(grid_lanes[1]->item_data.size(), 1u);
  ASSERT_EQ(grid_lanes[2]->item_data.size(), 1u);
  ASSERT_EQ(grid_lanes[3]->item_data.size(), 2u);

  // Each packed spanner is stored under a different parent in each occupied
  // lane.
  const auto& first_parent = *grid_lanes[1]->item_data[0];
  const auto& second_parent = *grid_lanes[2]->item_data[0];
  ASSERT_EQ(first_parent.items_packed_above.size(), 2u);
  ASSERT_EQ(second_parent.items_packed_above.size(), 2u);

  const auto& first_packed_spanner_start = *first_parent.items_packed_above[0];
  const auto& first_packed_spanner_continuation =
      *second_parent.items_packed_above[0];

  const auto& second_packed_spanner_start = *first_parent.items_packed_above[1];
  const auto& second_packed_spanner_continuation =
      *second_parent.items_packed_above[1];

  // The first packed spanner starts at the top of both openings.
  EXPECT_NE(first_parent.item.Get(), second_parent.item.Get());
  EXPECT_EQ(first_packed_spanner_start.item.Get(),
            first_packed_spanner_continuation.item.Get());
  EXPECT_EQ(first_packed_spanner_start.grid_lanes_placement_data,
            first_packed_spanner_continuation.grid_lanes_placement_data);
  EXPECT_EQ(first_packed_spanner_start.PlacementData().offset,
            (LogicalOffset{LayoutUnit(100), LayoutUnit()}));
  EXPECT_TRUE(first_packed_spanner_start.is_item_start);
  EXPECT_FALSE(first_packed_spanner_continuation.is_item_start);

  // The second packed spanner follows it and remains associated with the same
  // per-lane parents.
  EXPECT_EQ(second_packed_spanner_start.item.Get(),
            second_packed_spanner_continuation.item.Get());
  EXPECT_EQ(second_packed_spanner_start.grid_lanes_placement_data,
            second_packed_spanner_continuation.grid_lanes_placement_data);
  EXPECT_EQ(second_packed_spanner_start.PlacementData().offset,
            (LogicalOffset{LayoutUnit(100), LayoutUnit(10)}));
  EXPECT_TRUE(second_packed_spanner_start.is_item_start);
  EXPECT_FALSE(second_packed_spanner_continuation.is_item_start);
}

TEST_F(GridLanesLayoutAlgorithmTest,
       PopulateDenseItemAboveDenseSpannerUsesRootSpanner) {
  SetBodyInnerHTML(R"HTML(
    <div id="grid-lanes" style="display: grid-lanes; height: 200px;
        grid-template-columns: repeat(4, 100px); grid-auto-flow: dense;
        grid-lanes-pack: dense; flow-tolerance: 0;">
      <div style="grid-column: 1; height: 30px;"></div>
      <div style="grid-column: 3; height: 10px;"></div>
      <div style="grid-column: 4; height: 30px;"></div>
      <div style="grid-column: 1 / span 2; height: 20px;"></div>
      <div style="grid-column: 3 / span 2; height: 20px;"></div>
      <div style="grid-column: 2 / span 2; height: 10px;"></div>
      <div style="grid-column: 2; height: 5px;"></div>
    </div>
  )HTML");

  const auto grid_lanes = GetFragmentedGridLanesData();

  // Lane 2 has one direct root spanner with both packed items nested under it.
  ASSERT_EQ(grid_lanes.size(), 4u);
  ASSERT_EQ(grid_lanes[0]->item_data.size(), 2u);
  ASSERT_EQ(grid_lanes[1]->item_data.size(), 1u);
  ASSERT_EQ(grid_lanes[2]->item_data.size(), 2u);
  ASSERT_EQ(grid_lanes[3]->item_data.size(), 2u);
  const auto& root_spanner = *grid_lanes[1]->item_data[0];
  ASSERT_EQ(root_spanner.items_packed_above.size(), 2u);
  const auto& packed_spanner = *root_spanner.items_packed_above[0];
  const auto& packed_item = *root_spanner.items_packed_above[1];

  // The single-lane item uses the opening above the packed spanner but remains
  // a sibling under the original root instead of nesting under that spanner.
  EXPECT_EQ(packed_spanner.PlacementData().offset,
            (LogicalOffset{LayoutUnit(100), LayoutUnit(10)}));
  EXPECT_TRUE(packed_spanner.items_packed_above.empty());
  EXPECT_EQ(packed_item.PlacementData().offset,
            (LogicalOffset{LayoutUnit(100), LayoutUnit()}));
}

TEST_F(GridLanesLayoutAlgorithmTest,
       PopulateDenseItemsUseCorrectSpannerForMultipleOpenings) {
  SetBodyInnerHTML(R"HTML(
    <div id="grid-lanes" style="display: grid-lanes; height: 200px;
        grid-template-columns: repeat(2, 100px); grid-auto-flow: dense;
        grid-lanes-pack: dense; flow-tolerance: 0;">
      <div style="grid-column: 1; height: 20px;"></div>
      <div style="grid-column: 1 / span 2; height: 10px;"></div>
      <div style="grid-column: 1; height: 30px;"></div>
      <div style="grid-column: 1 / span 2; height: 10px;"></div>
      <div style="grid-column: 2; height: 15px;"></div>
      <div style="grid-column: 2; height: 25px;"></div>
    </div>
  )HTML");

  const auto grid_lanes = GetFragmentedGridLanesData();

  // Lane 1 contains its two single-lane items and both spanners. Lane 2 has the
  // two spanners, each of which created a distinct opening.
  ASSERT_EQ(grid_lanes.size(), 2u);
  ASSERT_EQ(grid_lanes[0]->item_data.size(), 4u);
  ASSERT_EQ(grid_lanes[1]->item_data.size(), 2u);

  const auto& first_spanner = *grid_lanes[1]->item_data[0];
  const auto& second_spanner = *grid_lanes[1]->item_data[1];
  ASSERT_EQ(first_spanner.items_packed_above.size(), 1u);
  ASSERT_EQ(second_spanner.items_packed_above.size(), 1u);

  const auto& first_packed_item = *first_spanner.items_packed_above[0];
  const auto& second_packed_item = *second_spanner.items_packed_above[0];

  // Each packed item is associated with the spanner below the opening it
  // selected.
  EXPECT_EQ(first_spanner.PlacementData().offset,
            (LogicalOffset{LayoutUnit(), LayoutUnit(20)}));
  EXPECT_EQ(first_packed_item.PlacementData().offset,
            (LogicalOffset{LayoutUnit(100), LayoutUnit()}));
  EXPECT_EQ(second_spanner.PlacementData().offset,
            (LogicalOffset{LayoutUnit(), LayoutUnit(60)}));
  EXPECT_EQ(second_packed_item.PlacementData().offset,
            (LogicalOffset{LayoutUnit(100), LayoutUnit(30)}));
}

TEST_F(GridLanesLayoutAlgorithmTest, PopulateGridAxisAlignedBreakTokenData) {
  SetBodyInnerHTML(R"HTML(
    <div id="grid-lanes" style="display: grid-lanes; height: 200px;
        grid-template-columns: repeat(2, 100px); flow-tolerance: 0;">
      <div style="grid-column: 1; width: 50px; height: 20px;
          justify-self: center;"></div>
      <div style="grid-column: 2; width: 60px; height: 30px;
          justify-self: end;"></div>
      <div style="grid-column: 1 / span 2; width: 100px; height: 20px;
          justify-self: center;"></div>
    </div>
  )HTML");

  const auto grid_lanes = GetFragmentedGridLanesData();

  ASSERT_EQ(grid_lanes.size(), 2u);
  ASSERT_EQ(grid_lanes[0]->item_data.size(), 2u);
  ASSERT_EQ(grid_lanes[1]->item_data.size(), 2u);

  // The first item is centered in its 100px column, while the second is end
  // aligned in the second column.
  EXPECT_EQ(grid_lanes[0]->item_data[0]->PlacementData().offset,
            (LogicalOffset{LayoutUnit(25), LayoutUnit()}));
  EXPECT_EQ(grid_lanes[1]->item_data[0]->PlacementData().offset,
            (LogicalOffset{LayoutUnit(140), LayoutUnit()}));

  // Both lane entries for the spanner preserve its centered offset within the
  // full 200px grid area.
  EXPECT_EQ(grid_lanes[0]->item_data[1]->grid_lanes_placement_data,
            grid_lanes[1]->item_data[1]->grid_lanes_placement_data);
  EXPECT_EQ(grid_lanes[0]->item_data[1]->PlacementData().offset,
            (LogicalOffset{LayoutUnit(50), LayoutUnit(30)}));
}

TEST_F(GridLanesLayoutAlgorithmTest, PopulateRowGridAxisAlignedBreakTokenData) {
  SetBodyInnerHTML(R"HTML(
    <div id="grid-lanes" style="display: grid-lanes; height: 200px;
        grid-template-rows: repeat(2, 50px); flow-tolerance: 0;">
      <div style="grid-row: 1; width: 20px; height: 20px;
          align-self: center;"></div>
      <div style="grid-row: 2; width: 30px; height: 30px;
          align-self: end;"></div>
      <div style="grid-row: 1 / span 2; width: 20px; height: 40px;
          align-self: center;"></div>
    </div>
  )HTML");

  const auto grid_lanes = GetFragmentedGridLanesData();

  ASSERT_EQ(grid_lanes.size(), 2u);
  ASSERT_EQ(grid_lanes[0]->item_data.size(), 2u);
  ASSERT_EQ(grid_lanes[1]->item_data.size(), 2u);

  // Grid-axis alignment changes the block offset for row grid-lanes.
  EXPECT_EQ(grid_lanes[0]->item_data[0]->PlacementData().offset,
            (LogicalOffset{LayoutUnit(), LayoutUnit(15)}));
  EXPECT_EQ(grid_lanes[1]->item_data[0]->PlacementData().offset,
            (LogicalOffset{LayoutUnit(), LayoutUnit(70)}));

  // Both lane entries for the spanner preserve its centered block offset within
  // the full 100px grid area.
  EXPECT_EQ(grid_lanes[0]->item_data[1]->grid_lanes_placement_data,
            grid_lanes[1]->item_data[1]->grid_lanes_placement_data);
  EXPECT_EQ(grid_lanes[0]->item_data[1]->PlacementData().offset,
            (LogicalOffset{LayoutUnit(30), LayoutUnit(30)}));
}

TEST_F(GridLanesLayoutAlgorithmTest,
       PopulateStackingAxisAlignedBreakTokenData) {
  SetBodyInnerHTML(R"HTML(
    <div id="grid-lanes" style="display: grid-lanes; height: 200px;
        grid-template-columns: repeat(2, 100px); flow-tolerance: 0;">
      <div style="grid-column: 1; height: 20px; align-self: end;"></div>
      <div style="grid-column: 2; height: 40px;"></div>
      <div style="grid-column: 1 / span 2; height: 20px;"></div>
    </div>
  )HTML");

  const auto grid_lanes = GetFragmentedGridLanesData();

  // The spanner makes the first lane's opening 20px taller than its first item,
  // so end alignment adds the full 20px to the item's block offset.
  ASSERT_EQ(grid_lanes.size(), 2u);
  ASSERT_EQ(grid_lanes[0]->item_data.size(), 2u);
  ASSERT_EQ(grid_lanes[1]->item_data.size(), 2u);
  EXPECT_EQ(grid_lanes[0]->item_data[0]->PlacementData().offset,
            (LogicalOffset{LayoutUnit(), LayoutUnit(20)}));

  // Available alignment space is only stored for auto-sized stretch items.
  EXPECT_EQ(grid_lanes[0]->item_data[0]->AvailableStackingAxisAlignmentSpace(),
            LayoutUnit());
}

TEST_F(GridLanesLayoutAlgorithmTest,
       PopulateRowStackingAxisAlignedBreakTokenData) {
  SetBodyInnerHTML(R"HTML(
    <div id="grid-lanes" style="display: grid-lanes; height: 200px;
        grid-template-rows: repeat(2, 50px); flow-tolerance: 0;">
      <div style="grid-row: 1; width: 20px; justify-self: end;"></div>
      <div style="grid-row: 2; width: 40px;"></div>
      <div style="grid-row: 1 / span 2; width: 20px;"></div>
    </div>
  )HTML");

  const auto grid_lanes = GetFragmentedGridLanesData();

  // For row grid-lanes the stacking axis is inline, so end alignment adds the
  // 20px of available space to the item's inline offset.
  ASSERT_EQ(grid_lanes.size(), 2u);
  ASSERT_EQ(grid_lanes[0]->item_data.size(), 2u);
  ASSERT_EQ(grid_lanes[1]->item_data.size(), 2u);
  EXPECT_EQ(grid_lanes[0]->item_data[0]->PlacementData().offset,
            (LogicalOffset{LayoutUnit(20), LayoutUnit()}));

  // Non-stretch items do not persist the available alignment space.
  EXPECT_EQ(grid_lanes[0]->item_data[0]->AvailableStackingAxisAlignmentSpace(),
            LayoutUnit());
}

TEST_F(GridLanesLayoutAlgorithmTest,
       PopulateExplicitStackingAxisStretchBreakTokenData) {
  SetBodyInnerHTML(R"HTML(
    <div id="grid-lanes" style="display: grid-lanes; height: 200px;
        grid-template-columns: repeat(2, 100px); flow-tolerance: 0;">
      <div style="grid-column: 1; height: 20px; align-self: stretch;"></div>
      <div style="grid-column: 2; height: 40px;"></div>
      <div style="grid-column: 1 / span 2; height: 20px;"></div>
    </div>
  )HTML");

  const auto grid_lanes = GetFragmentedGridLanesData();

  // An explicit height prevents stretch from changing the item's size or
  // offset, even though its lane opening has extra space.
  ASSERT_EQ(grid_lanes.size(), 2u);
  ASSERT_EQ(grid_lanes[0]->item_data.size(), 2u);
  ASSERT_EQ(grid_lanes[1]->item_data.size(), 2u);

  // Stretch preserves the initial offset and persists the space for the later
  // fragmented layout.
  EXPECT_EQ(grid_lanes[0]->item_data[0]->PlacementData().offset,
            LogicalOffset());

  // Explicitly sized stretch items do not need alignment space persisted for
  // later fragmentation layout.
  EXPECT_EQ(grid_lanes[0]->item_data[0]->AvailableStackingAxisAlignmentSpace(),
            LayoutUnit());
}

TEST_F(GridLanesLayoutAlgorithmTest,
       PopulateRowExplicitStackingAxisStretchBreakTokenData) {
  SetBodyInnerHTML(R"HTML(
    <div id="grid-lanes" style="display: grid-lanes; width: 200px; height: 200px;
        grid-template-rows: repeat(2, 50px); flow-tolerance: 0;">
      <div style="grid-row: 1; width: 20px; justify-self: stretch;"></div>
      <div style="grid-row: 2; width: 40px;"></div>
      <div style="grid-row: 1 / span 2; width: 20px;"></div>
    </div>
  )HTML");

  const auto grid_lanes = GetFragmentedGridLanesData();

  // An explicit width prevents stretch from changing the item's size or offset,
  // even though its lane opening has extra space.
  ASSERT_EQ(grid_lanes.size(), 2u);
  ASSERT_EQ(grid_lanes[0]->item_data.size(), 2u);
  ASSERT_EQ(grid_lanes[1]->item_data.size(), 2u);

  // Stretch preserves the initial offset and persists the inline space for the
  // later fragmented layout.
  EXPECT_EQ(grid_lanes[0]->item_data[0]->PlacementData().offset,
            LogicalOffset());

  // Explicitly sized stretch items do not need alignment space persisted for
  // later fragmentation layout.
  EXPECT_EQ(grid_lanes[0]->item_data[0]->AvailableStackingAxisAlignmentSpace(),
            LayoutUnit());
}

TEST_F(GridLanesLayoutAlgorithmTest,
       PopulateStackingAxisStretchSpaceBreakTokenData) {
  SetBodyInnerHTML(R"HTML(
    <div id="grid-lanes" style="display: grid-lanes; height: 200px;
        grid-template-columns: repeat(2, 100px); flow-tolerance: 0;">
      <div style="grid-column: 1; align-self: stretch;">
        <div style="height: 20px;"></div>
      </div>
      <div style="grid-column: 2; height: 40px;"></div>
      <div style="grid-column: 1 / span 2; height: 20px;"></div>
    </div>
  )HTML");

  const auto grid_lanes = GetFragmentedGridLanesData();

  // The spanner creates 20px of space below the auto-sized stretch item.
  ASSERT_EQ(grid_lanes.size(), 2u);
  ASSERT_EQ(grid_lanes[0]->item_data.size(), 2u);
  ASSERT_EQ(grid_lanes[1]->item_data.size(), 2u);
  EXPECT_EQ(grid_lanes[0]->item_data[0]->PlacementData().offset,
            LogicalOffset());
  EXPECT_EQ(grid_lanes[0]->item_data[0]->AvailableStackingAxisAlignmentSpace(),
            LayoutUnit(20));
}

TEST_F(GridLanesLayoutAlgorithmTest,
       PopulateRowStackingAxisStretchSpaceBreakTokenData) {
  SetBodyInnerHTML(R"HTML(
    <div id="grid-lanes" style="display: grid-lanes; width: 200px; height: 200px;
        grid-template-rows: repeat(2, 50px); flow-tolerance: 0;">
      <div style="grid-row: 1; justify-self: stretch;">
        <div style="width: 20px;"></div>
      </div>
      <div style="grid-row: 2; width: 40px;"></div>
      <div style="grid-row: 1 / span 2; width: 20px;"></div>
    </div>
  )HTML");

  const auto grid_lanes = GetFragmentedGridLanesData();

  // For row grid-lanes, the spanner creates 20px of inline space below the
  // auto-sized stretch item.
  ASSERT_EQ(grid_lanes.size(), 2u);
  ASSERT_EQ(grid_lanes[0]->item_data.size(), 2u);
  ASSERT_EQ(grid_lanes[1]->item_data.size(), 2u);
  EXPECT_EQ(grid_lanes[0]->item_data[0]->PlacementData().offset,
            LogicalOffset());
  EXPECT_EQ(grid_lanes[0]->item_data[0]->AvailableStackingAxisAlignmentSpace(),
            LayoutUnit(20));
}

TEST_F(GridLanesLayoutAlgorithmTest,
       PopulateStackingAxisStretchSpannerSpaceBreakTokenData) {
  SetBodyInnerHTML(R"HTML(
    <div id="grid-lanes" style="display: grid-lanes; height: 200px;
        grid-template-columns: repeat(3, 100px); flow-tolerance: 0;">
      <div style="grid-column: 1 / span 2; align-self: stretch;">
        <div style="height: 20px;"></div>
      </div>
      <div style="grid-column: 3; height: 40px;"></div>
      <div style="grid-column: 1 / span 3; height: 20px;"></div>
    </div>
  )HTML");

  const auto grid_lanes = GetFragmentedGridLanesData();

  // The lower three-lane spanner creates 20px of stretch space below both
  // entries for the auto-sized upper spanner.
  ASSERT_EQ(grid_lanes.size(), 3u);
  ASSERT_EQ(grid_lanes[0]->item_data.size(), 2u);
  ASSERT_EQ(grid_lanes[1]->item_data.size(), 2u);
  ASSERT_EQ(grid_lanes[2]->item_data.size(), 2u);

  // Both entries share the initial offset and alignment space that will be used
  // to stretch the spanner during fragmented layout.
  EXPECT_EQ(grid_lanes[0]->item_data[0]->grid_lanes_placement_data,
            grid_lanes[1]->item_data[0]->grid_lanes_placement_data);
  EXPECT_EQ(grid_lanes[0]->item_data[0]->PlacementData().offset,
            LogicalOffset());
  EXPECT_EQ(grid_lanes[1]->item_data[0]->PlacementData().offset,
            LogicalOffset());
  EXPECT_EQ(grid_lanes[0]->item_data[0]->AvailableStackingAxisAlignmentSpace(),
            LayoutUnit(20));
  EXPECT_EQ(grid_lanes[1]->item_data[0]->AvailableStackingAxisAlignmentSpace(),
            LayoutUnit(20));
}

TEST_F(GridLanesLayoutAlgorithmTest,
       PopulateRowStackingAxisStretchSpannerSpaceBreakTokenData) {
  SetBodyInnerHTML(R"HTML(
    <div id="grid-lanes" style="display: grid-lanes; width: 200px; height: 200px;
        grid-template-rows: repeat(3, 50px); flow-tolerance: 0;">
      <div style="grid-row: 1 / span 2; justify-self: stretch;">
        <div style="width: 20px;"></div>
      </div>
      <div style="grid-row: 3; width: 40px;"></div>
      <div style="grid-row: 1 / span 3; width: 20px;"></div>
    </div>
  )HTML");

  const auto grid_lanes = GetFragmentedGridLanesData();

  // The lower three-lane spanner creates 20px of inline stretch space below
  // both entries for the auto-sized upper spanner.
  ASSERT_EQ(grid_lanes.size(), 3u);
  ASSERT_EQ(grid_lanes[0]->item_data.size(), 2u);
  ASSERT_EQ(grid_lanes[1]->item_data.size(), 2u);
  ASSERT_EQ(grid_lanes[2]->item_data.size(), 2u);

  // Both entries share the initial offset and alignment space that will be used
  // to stretch the spanner during fragmented layout.
  EXPECT_EQ(grid_lanes[0]->item_data[0]->grid_lanes_placement_data,
            grid_lanes[1]->item_data[0]->grid_lanes_placement_data);
  EXPECT_EQ(grid_lanes[0]->item_data[0]->PlacementData().offset,
            LogicalOffset());
  EXPECT_EQ(grid_lanes[1]->item_data[0]->PlacementData().offset,
            LogicalOffset());
  EXPECT_EQ(grid_lanes[0]->item_data[0]->AvailableStackingAxisAlignmentSpace(),
            LayoutUnit(20));
  EXPECT_EQ(grid_lanes[1]->item_data[0]->AvailableStackingAxisAlignmentSpace(),
            LayoutUnit(20));
}

TEST_F(GridLanesLayoutAlgorithmTest,
       PopulateLastStackingAxisAlignedBreakTokenData) {
  SetBodyInnerHTML(R"HTML(
    <div id="grid-lanes" style="display: grid-lanes; height: 200px;
        grid-template-columns: repeat(2, 100px); flow-tolerance: 0;">
      <div style="grid-column: 1; height: 20px; align-self: end;"></div>
      <div style="grid-column: 2; align-self: stretch;">
        <div style="height: 20px;"></div>
      </div>
    </div>
  )HTML");

  const auto grid_lanes = GetFragmentedGridLanesData();

  // Each item is last in its track, leaving 180px to the container's block end.
  ASSERT_EQ(grid_lanes.size(), 2u);
  ASSERT_EQ(grid_lanes[0]->item_data.size(), 1u);
  ASSERT_EQ(grid_lanes[1]->item_data.size(), 1u);

  // End alignment moves the first item to the container's block end.
  EXPECT_EQ(grid_lanes[0]->item_data[0]->PlacementData().offset,
            (LogicalOffset{LayoutUnit(), LayoutUnit(180)}));

  // Stretch preserves the same space for relayout instead of changing offset.
  EXPECT_EQ(grid_lanes[1]->item_data[0]->PlacementData().offset,
            (LogicalOffset{LayoutUnit(100), LayoutUnit()}));
  EXPECT_EQ(grid_lanes[1]->item_data[0]->AvailableStackingAxisAlignmentSpace(),
            LayoutUnit(180));
}

TEST_F(GridLanesLayoutAlgorithmTest,
       PopulateRowLastStackingAxisAlignedBreakTokenData) {
  SetBodyInnerHTML(R"HTML(
    <div id="grid-lanes" style="display: grid-lanes; width: 200px; height: 200px;
        grid-template-rows: repeat(2, 50px); flow-tolerance: 0;">
      <div style="grid-row: 1; width: 20px; justify-self: end;"></div>
      <div style="grid-row: 2; justify-self: stretch;">
        <div style="width: 20px;"></div>
      </div>
    </div>
  )HTML");

  const auto grid_lanes = GetFragmentedGridLanesData();

  // Each item is last in its track, leaving 180px to the container's inline
  // end.
  ASSERT_EQ(grid_lanes.size(), 2u);
  ASSERT_EQ(grid_lanes[0]->item_data.size(), 1u);
  ASSERT_EQ(grid_lanes[1]->item_data.size(), 1u);

  // End alignment moves the first item to the container's inline end.
  EXPECT_EQ(grid_lanes[0]->item_data[0]->PlacementData().offset,
            (LogicalOffset{LayoutUnit(180), LayoutUnit()}));

  // Stretch preserves the same space for relayout instead of changing offset.
  EXPECT_EQ(grid_lanes[1]->item_data[0]->PlacementData().offset,
            (LogicalOffset{LayoutUnit(), LayoutUnit(50)}));
  EXPECT_EQ(grid_lanes[1]->item_data[0]->AvailableStackingAxisAlignmentSpace(),
            LayoutUnit(180));
}

TEST_F(GridLanesLayoutAlgorithmTest,
       PopulateStackingAxisAlignedSpannerBreakTokenData) {
  SetBodyInnerHTML(R"HTML(
    <div id="grid-lanes" style="display: grid-lanes; height: 200px;
        grid-template-columns: repeat(3, 100px); flow-tolerance: 0;">
      <div style="grid-column: 1 / span 2; height: 20px;
          align-self: end;"></div>
      <div style="grid-column: 3; height: 40px;"></div>
      <div style="grid-column: 1 / span 3; height: 20px;"></div>
    </div>
  )HTML");

  const auto grid_lanes = GetFragmentedGridLanesData();

  // The lower three-lane spanner creates 20px of alignment space below both
  // entries for the upper two-lane spanner.
  ASSERT_EQ(grid_lanes.size(), 3u);
  ASSERT_EQ(grid_lanes[0]->item_data.size(), 2u);
  ASSERT_EQ(grid_lanes[1]->item_data.size(), 2u);
  ASSERT_EQ(grid_lanes[2]->item_data.size(), 2u);

  // Both entries share one placement record, so the spanner receives the 20px
  // adjustment once and exposes it consistently from either lane.
  EXPECT_EQ(grid_lanes[0]->item_data[0]->grid_lanes_placement_data,
            grid_lanes[1]->item_data[0]->grid_lanes_placement_data);
  EXPECT_EQ(grid_lanes[0]->item_data[0]->PlacementData().offset,
            (LogicalOffset{LayoutUnit(), LayoutUnit(20)}));
  EXPECT_EQ(grid_lanes[1]->item_data[0]->PlacementData().offset,
            (LogicalOffset{LayoutUnit(), LayoutUnit(20)}));
}

TEST_F(GridLanesLayoutAlgorithmTest,
       PopulateRowStackingAxisAlignedSpannerBreakTokenData) {
  SetBodyInnerHTML(R"HTML(
    <div id="grid-lanes" style="display: grid-lanes; width: 200px; height: 200px;
        grid-template-rows: repeat(3, 50px); flow-tolerance: 0;">
      <div style="grid-row: 1 / span 2; width: 20px;
          justify-self: end;"></div>
      <div style="grid-row: 3; width: 40px;"></div>
      <div style="grid-row: 1 / span 3; width: 20px;"></div>
    </div>
  )HTML");

  const auto grid_lanes = GetFragmentedGridLanesData();

  // The lower three-lane spanner creates 20px of inline alignment space below
  // both entries for the upper two-lane spanner.
  ASSERT_EQ(grid_lanes.size(), 3u);
  ASSERT_EQ(grid_lanes[0]->item_data.size(), 2u);
  ASSERT_EQ(grid_lanes[1]->item_data.size(), 2u);
  ASSERT_EQ(grid_lanes[2]->item_data.size(), 2u);

  // Both entries share one placement record, so the spanner receives the 20px
  // inline adjustment once and exposes it consistently from either lane.
  EXPECT_EQ(grid_lanes[0]->item_data[0]->grid_lanes_placement_data,
            grid_lanes[1]->item_data[0]->grid_lanes_placement_data);
  EXPECT_EQ(grid_lanes[0]->item_data[0]->PlacementData().offset,
            (LogicalOffset{LayoutUnit(20), LayoutUnit()}));
  EXPECT_EQ(grid_lanes[1]->item_data[0]->PlacementData().offset,
            (LogicalOffset{LayoutUnit(20), LayoutUnit()}));
}

TEST_F(GridLanesLayoutAlgorithmTest, PopulateContentAlignedBreakTokenData) {
  SetBodyInnerHTML(R"HTML(
    <div id="grid-lanes" style="display: grid-lanes; height: 200px;
        grid-template-columns: repeat(2, 100px); align-content: center;
        flow-tolerance: 0;">
      <div style="grid-column: 1; height: 20px;"></div>
      <div style="grid-column: 1 / span 2; height: 20px;"></div>
    </div>
  )HTML");

  const auto grid_lanes = GetFragmentedGridLanesData();

  // Centering 40px of content in the 200px container offsets every item by half
  // the 160px of free space.
  ASSERT_EQ(grid_lanes.size(), 2u);
  ASSERT_EQ(grid_lanes[0]->item_data.size(), 2u);
  ASSERT_EQ(grid_lanes[1]->item_data.size(), 1u);
  EXPECT_EQ(grid_lanes[0]->item_data[0]->PlacementData().offset,
            (LogicalOffset{LayoutUnit(), LayoutUnit(80)}));

  // The spanner's two lane entries share one placement record, so content
  // alignment adjusts its offset only once.
  EXPECT_EQ(grid_lanes[0]->item_data[1]->grid_lanes_placement_data,
            grid_lanes[1]->item_data[0]->grid_lanes_placement_data);
  EXPECT_EQ(grid_lanes[0]->item_data[1]->PlacementData().offset,
            (LogicalOffset{LayoutUnit(), LayoutUnit(100)}));
}

TEST_F(GridLanesLayoutAlgorithmTest, PopulateRowContentAlignedBreakTokenData) {
  SetBodyInnerHTML(R"HTML(
    <div id="grid-lanes" style="display: grid-lanes; width: 200px; height: 200px;
        grid-template-rows: repeat(2, 50px); justify-content: center;
        flow-tolerance: 0;">
      <div style="grid-row: 1; width: 20px;"></div>
      <div style="grid-row: 1 / span 2; width: 20px;"></div>
    </div>
  )HTML");

  const auto grid_lanes = GetFragmentedGridLanesData();

  // Centering 40px of content in the 200px container offsets every item by half
  // the 160px of free space in the inline stacking axis.
  ASSERT_EQ(grid_lanes.size(), 2u);
  ASSERT_EQ(grid_lanes[0]->item_data.size(), 2u);
  ASSERT_EQ(grid_lanes[1]->item_data.size(), 1u);
  EXPECT_EQ(grid_lanes[0]->item_data[0]->PlacementData().offset,
            (LogicalOffset{LayoutUnit(80), LayoutUnit()}));

  // The spanner's two lane entries share one placement record, so content
  // alignment adjusts its inline offset only once.
  EXPECT_EQ(grid_lanes[0]->item_data[1]->grid_lanes_placement_data,
            grid_lanes[1]->item_data[0]->grid_lanes_placement_data);
  EXPECT_EQ(grid_lanes[0]->item_data[1]->PlacementData().offset,
            (LogicalOffset{LayoutUnit(100), LayoutUnit()}));
}

}  // namespace blink
