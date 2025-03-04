// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/masonry/masonry_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/base_layout_algorithm_test.h"
#include "third_party/blink/renderer/core/layout/grid/grid_item.h"
#include "third_party/blink/renderer/core/layout/grid/grid_track_collection.h"
#include "third_party/blink/renderer/core/layout/grid/grid_track_sizing_algorithm.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/layout/masonry/masonry_running_positions.h"

namespace blink {

class MasonryLayoutAlgorithmTest : public BaseLayoutAlgorithmTest {
 protected:
  void SetUp() override { BaseLayoutAlgorithmTest::SetUp(); }

  void ComputeGeometry(const MasonryLayoutAlgorithm& algorithm) {
    wtf_size_t start_offset;
    const GridLineResolver line_resolver(algorithm.Style(),
                                         /*auto_repetitions=*/0);
    virtual_masonry_items_ =
        algorithm.VirtualMasonryItems(line_resolver, &start_offset);

    grid_axis_tracks_ = std::make_unique<GridSizingTrackCollection>(
        algorithm.BuildGridAxisTracks(line_resolver, SizingConstraint::kLayout,
                                      &start_offset));

    masonry_items_ =
        algorithm.Node().ConstructMasonryItems(line_resolver, start_offset);
  }

  const GridRangeVector& Ranges() { return grid_axis_tracks_->ranges_; }
  wtf_size_t SetCount() { return grid_axis_tracks_->GetSetCount(); }
  wtf_size_t VirtualItemCount() {
    return virtual_masonry_items_ ? virtual_masonry_items_->Size() : 0;
  }
  wtf_size_t MasonryItemCount() {
    return masonry_items_ ? masonry_items_->Size() : 0;
  }

  LayoutUnit TrackSize(wtf_size_t index) {
    return grid_axis_tracks_->GetSetOffset(index + 1) -
           grid_axis_tracks_->GetSetOffset(index);
  }

  LayoutUnit MaxContentContribution(wtf_size_t index) {
    return ContributionSizes(index).max_size;
  }

  LayoutUnit MinContentContribution(wtf_size_t index) {
    return ContributionSizes(index).min_size;
  }

  const GridSpan& VirtualItemSpan(wtf_size_t index) {
    return virtual_masonry_items_->At(index).resolved_position.Span(
        grid_axis_tracks_->Direction());
  }

  const GridSpan& MasonryItemSpan(wtf_size_t index) {
    return masonry_items_->At(index).resolved_position.Span(
        grid_axis_tracks_->Direction());
  }

  const Vector<LayoutUnit>& GetRunningPositions(
      const MasonryRunningPositions& running_positions) {
    return running_positions.running_positions_;
  }

 private:
  const MinMaxSizes& ContributionSizes(wtf_size_t index) {
    const auto& contribution_sizes =
        virtual_masonry_items_->At(index).contribution_sizes;

    DCHECK(contribution_sizes);
    return *contribution_sizes;
  }

  std::unique_ptr<GridSizingTrackCollection> grid_axis_tracks_;

  // Virtual items represent the contributions of item groups in track sizing
  // and are not directly related to any children of the container.
  Persistent<GridItems> virtual_masonry_items_;

  // Children of the container to be laid out are represented by masonry items.
  Persistent<GridItems> masonry_items_;
};

TEST_F(MasonryLayoutAlgorithmTest, BuildMasonryItems) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    #masonry {
      display: masonry;
      masonry-template-tracks: auto auto [header-start] auto auto [header-end];
    }
    </style>
    <div id="masonry">
      <div>1</div>
      <div style="masonry-track: 3 / span 2">2</div>
      <div style="masonry-track: span 2">3</div>
      <div style="masonry-track: span 3">4</div>
      <div style="masonry-track: 2 / 5">5</div>
      <div style="masonry-track: header-start / header-end">1</div>
      <div style="masonry-track: 1 / header-start">2</div>
      <div style="masonry-track: 3 / header-end">2</div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("masonry"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), LayoutUnit(100)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);
  MasonryLayoutAlgorithm algorithm({node, fragment_geometry, space});

  EXPECT_EQ(MasonryItemCount(), 0U);
  ComputeGeometry(algorithm);
  EXPECT_EQ(MasonryItemCount(), 8U);

  const Vector<GridSpan> expected_spans = {
      GridSpan::IndefiniteGridSpan(1),
      GridSpan::TranslatedDefiniteGridSpan(2, 4),
      GridSpan::IndefiniteGridSpan(2),
      GridSpan::IndefiniteGridSpan(3),
      GridSpan::TranslatedDefiniteGridSpan(1, 4),
      GridSpan::TranslatedDefiniteGridSpan(2, 4),
      GridSpan::TranslatedDefiniteGridSpan(0, 2),
      GridSpan::TranslatedDefiniteGridSpan(2, 4)};

  for (wtf_size_t i = 0; i < expected_spans.size(); ++i) {
    EXPECT_EQ(MasonryItemSpan(i), expected_spans[i]);
  }
}

TEST_F(MasonryLayoutAlgorithmTest, BuildRanges) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #masonry {
      display: masonry;
      masonry-template-tracks: 5% repeat(3, 10px auto) repeat(1, auto 5px 1fr);
    }
    </style>
    <div id="masonry">
      <div style="masonry-track: span 2 / 1"></div>
      <div style="masonry-track: 9 / span 5"></div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("masonry"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), LayoutUnit(100)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  MasonryLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  // The first item spans 2 tracks before the explicit grid, creating the first
  // range of 2 tracks. Then follows the template track ranges: one range of a
  // single track for the `5%`, then a range for the `repeat(3, ...)` which
  // spans 6 tracks. The last repeat creates a range of 3 tracks, but it's split
  // by the second item, creating one range of 1 track and another of 2 tracks.
  // Finally, the second item spans a range of 3 track past the explicit grid.
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

TEST_F(MasonryLayoutAlgorithmTest, BuildFixedTrackSizes) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #masonry {
      display: masonry;
      masonry-template-tracks: 5% repeat(3, 10px 15%) repeat(1, 15px 5px 20px);
    }
    </style>
    <div id="masonry"></div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("masonry"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), LayoutUnit(100)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  MasonryLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  const Vector<int> expected_track_sizes = {5, 30, 45, 15, 5, 20};

  const auto set_count = SetCount();
  EXPECT_EQ(set_count, expected_track_sizes.size());
  for (wtf_size_t i = 0; i < set_count; ++i) {
    EXPECT_EQ(TrackSize(i), LayoutUnit(expected_track_sizes[i]));
  }
}

TEST_F(MasonryLayoutAlgorithmTest, CollectMasonryItemGroups) {
  SetBodyInnerHTML(R"HTML(
    <div id="masonry" style="display: masonry">
      <div></div>
      <div style="masonry-track: 1"></div>
      <div style="masonry-track: 1 / 4"></div>
      <div style="masonry-track: span 3"></div>
      <div style="masonry-track: span 3 / 4"></div>
      <div></div>
    </div>
  )HTML");

  MasonryNode node(GetLayoutBoxByElementId("masonry"));

  wtf_size_t start_offset;
  const GridLineResolver line_resolver(node.Style(), /*auto_repetitions=*/0);
  const auto item_groups = node.CollectItemGroups(line_resolver, &start_offset);

  EXPECT_EQ(item_groups.size(), 4u);

  for (const auto& [properties, items] : item_groups) {
    wtf_size_t expected_size = 0;
    const auto& span = properties.Span();
    if (span == GridSpan::IndefiniteGridSpan(3) ||
        span == GridSpan::UntranslatedDefiniteGridSpan(0, 1)) {
      expected_size = 1;
    } else if (span == GridSpan::IndefiniteGridSpan(1) ||
               span == GridSpan::UntranslatedDefiniteGridSpan(0, 3)) {
      expected_size = 2;
    }
    EXPECT_EQ(items.size(), expected_size);
  }
}

TEST_F(MasonryLayoutAlgorithmTest, ExplicitlyPlacedVirtualItems) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    body { font: 10px/1 Ahem }
    #masonry {
      display: masonry;
      masonry-template-tracks: repeat(2, 100px);
    }
    </style>
    <div id="masonry">
      <div style="masonry-track: 1">XX XX</div>
      <div style="masonry-track: -4 / 3">XXX X</div>
      <div style="masonry-track: span 3 / 3">X XX X</div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("masonry"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), LayoutUnit(100)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  MasonryLayoutAlgorithm algorithm({node, fragment_geometry, space});
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

TEST_F(MasonryLayoutAlgorithmTest, BuildIntrinsicTrackSizes) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    body { font: 10px/1 Ahem }
    #masonry {
      display: masonry;
      masonry-template-tracks: min-content max-content;
    }
    </style>
    <div id="masonry">
      <div style="masonry-track: 1">XX XX</div>
      <div style="masonry-track: 2">XX XX</div>
      <div style="masonry-track: 1 / 3">XXX XXXXXX XXXXXXXXX</div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("masonry"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), LayoutUnit(100)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  MasonryLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  const Vector<int> expected_track_sizes = {30, 170};

  const auto set_count = SetCount();
  EXPECT_EQ(set_count, expected_track_sizes.size());
  for (wtf_size_t i = 0; i < set_count; ++i) {
    EXPECT_EQ(TrackSize(i), LayoutUnit(expected_track_sizes[i]));
  }
}

TEST_F(MasonryLayoutAlgorithmTest, MaximizeAndStretchAutoTracks) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    body { font: 10px/1 Ahem }
    #masonry {
      display: masonry;
      masonry-template-tracks: minmax(15px, min-content) max-content auto;
    }
    </style>
    <div id="masonry">
      <div style="masonry-track: 1">XXX XXX</div>
      <div style="masonry-track: 1 / 3">X XX X</div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("masonry"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), LayoutUnit(100)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  MasonryLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  // First track starts at 15px, but should be resolved to 30px (which is the
  // min-content size of the first item) later in the maximize tracks step.
  // To acomodate the max-content size of the second item, which is 60px minus
  // 15px that the first track already has, the second track expands to 45px.
  // Finally, the last track takes the remaining space after the first two
  // tracks are maximized, which is 100px - 30px - 45px = 25px.
  const Vector<int> expected_track_sizes = {30, 45, 25};

  const auto set_count = SetCount();
  EXPECT_EQ(set_count, expected_track_sizes.size());
  for (wtf_size_t i = 0; i < set_count; ++i) {
    EXPECT_EQ(TrackSize(i), LayoutUnit(expected_track_sizes[i]));
  }
}

TEST_F(MasonryLayoutAlgorithmTest, UpdateRunningPositionsForSpan) {
  MasonryRunningPositions running_positions(4);

  Vector<LayoutUnit> expected_running_positions = {
      LayoutUnit(0), LayoutUnit(3), LayoutUnit(3), LayoutUnit(0)};
  running_positions.UpdateRunningPositionsForSpan(
      GridSpan::TranslatedDefiniteGridSpan(1, 3), LayoutUnit(3));
  EXPECT_EQ(expected_running_positions, GetRunningPositions(running_positions));

  expected_running_positions = {LayoutUnit(4), LayoutUnit(4), LayoutUnit(4),
                                LayoutUnit(4)};
  running_positions.UpdateRunningPositionsForSpan(
      GridSpan::TranslatedDefiniteGridSpan(0, 4), LayoutUnit(4));
  EXPECT_EQ(expected_running_positions, GetRunningPositions(running_positions));

  expected_running_positions = {LayoutUnit(4), LayoutUnit(4), LayoutUnit(5),
                                LayoutUnit(5)};
  running_positions.UpdateRunningPositionsForSpan(
      GridSpan::TranslatedDefiniteGridSpan(2, 4), LayoutUnit(5));
  EXPECT_EQ(expected_running_positions, GetRunningPositions(running_positions));
}

}  // namespace blink
