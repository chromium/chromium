// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_LAYOUT_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_LAYOUT_UTILS_H_

#include "third_party/blink/renderer/core/layout/grid/grid_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/grid/grid_sizing_tree.h"
#include "third_party/blink/renderer/core/style/grid_enums.h"
#include "third_party/blink/renderer/core/style/grid_track_size.h"
#include "third_party/blink/renderer/platform/fonts/font_baseline.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

class BlockNode;
class BoxFragmentBuilder;
class ConstraintSpace;
class GridLayoutData;
class GridLayoutTrackCollection;
class GridLineResolver;
class GridSizingTrackCollection;
class GridTrackList;
class LayoutBox;
class LogicalBoxFragment;

enum class AxisEdge;
struct BoxStrut;
struct FragmentGeometry;
struct GridItemData;
struct LogicalSize;
struct LogicalStaticPosition;
struct MinMaxSizesResult;

// Base class for accumulating baseline information across grid and grid-lanes
// layouts. Provides a unified interface for handling baselines in both grid
// axis and stacking axis.
class BaselineAccumulator {
 public:
  virtual ~BaselineAccumulator() = default;

  // Accumulates baseline information for a given item.
  virtual void Accumulate(const GridItemData& item,
                          const LogicalBoxFragment& fragment,
                          const LayoutUnit block_offset,
                          LayoutUnit item_stacking_position) = 0;

  virtual std::optional<LayoutUnit> FirstBaseline() const = 0;
  virtual std::optional<LayoutUnit> LastBaseline() const = 0;
};

// Update the provided `available_size`, `min_available_size`, and
// `max_available_size` to their appropriate values.
void ComputeAvailableSizes(const BoxStrut& border_scrollbar_padding,
                           const BlockNode& node,
                           const ConstraintSpace& constraint_space,
                           const BoxFragmentBuilder& container_builder,
                           LogicalSize& available_size,
                           LogicalSize& min_available_size,
                           LogicalSize& max_available_size);

// https://drafts.csswg.org/css-grid-2/#auto-repeat
//
// This method assumes that the track list provided has an auto repeater. If
// `intrinsic_repeat_track_sizes` is not nullptr, this will indicate what to
// size an intrinsic track definition(s) within an auto repeater.
wtf_size_t CalculateAutomaticRepetitions(
    const GridTrackList& track_list,
    const LayoutUnit gutter_size,
    LayoutUnit available_size,
    LayoutUnit min_available_size,
    LayoutUnit max_available_size,
    const HashMap<GridTrackSize, LayoutUnit>* intrinsic_repeat_track_sizes =
        nullptr);

// Common out-of-flow positioning utilities shared between grid and grid-lanes.

// Computes the start offset and size for an out-of-flow item in a single
// direction (either inline or block).
//
// `is_grid_lanes_axis` indicates whether this is for gid lane's stacking axis,
// which ignores grid placement and uses the full container size.
void ComputeOutOfFlowOffsetAndSize(
    const GridItemData& out_of_flow_item,
    const GridLayoutTrackCollection& track_collection,
    const BoxStrut& borders,
    const LogicalSize& border_box_size,
    LayoutUnit* start_offset,
    LayoutUnit* size,
    bool is_grid_lanes_axis = false);

// Computes alignment offset for out-of-flow items.
void AlignmentOffsetForOutOfFlow(AxisEdge inline_axis_edge,
                                 AxisEdge block_axis_edge,
                                 LogicalSize container_size,
                                 LogicalStaticPosition*);

// Per the Grid spec [1] there is special logic for the contribution size to use
// for intrinsic minimums. This method returns the contribution size of
// `grid_item` given the provided variables.
//
// When `special_spanning_criteria` is true, always use the automatic minimum
// size - this usually happens when an item spans at least one track with a min
// track size of 'auto' or if an item spans more than one non-flexible track.
// However, in grid-lanes, we don't have this information when initially
// calculating the virtual item contributions, so grid-lanes needs this var to
// force this to both true and false to get both potential contributions for use
// later when more information is known about the tracks a virtual item spans.
//
// `min_content_contribution` and `max_content_contribution` are the content
// based min and maximums for the provided `grid_item` respectively. If the item
// is a subgrid, `subgrid_minmax_sizes` will be the min/max size result for the
// subgrid.
//
// This method will set `maybe_clamp` to true if the content based contribution
// was returned and should be considered for clamping. Otherwise, it will be set
// to false.
//
// [1] https://drafts.csswg.org/css-grid/#min-size-auto
LayoutUnit CalculateIntrinsicMinimumContribution(
    bool is_parallel_with_track_direction,
    bool special_spanning_criteria,
    const LayoutUnit min_content_contribution,
    const LayoutUnit max_content_contribution,
    const ConstraintSpace& space,
    const MinMaxSizesResult& subgrid_minmax_sizes,
    const GridItemData* grid_item,
    bool& maybe_clamp);

// Return `min_content_contribution` clamped by
// `spanned_tracks_definite_max_size` up to `min_clamp_size`. `min_clamp_size`
// usually represents the sum of border/padding, margins, and the baseline shim.
LayoutUnit ClampIntrinsicMinSize(LayoutUnit min_content_contribution,
                                 LayoutUnit min_clamp_size,
                                 LayoutUnit spanned_tracks_definite_max_size);

// Returns the track baseline for a grid item based on its baseline-sharing
// group.
LayoutUnit GetTrackBaseline(const GridItemData& grid_item,
                            const GridLayoutTrackCollection& track_collection);

// Returns the baseline of an item from its fragment. Handles both first and
// last baseline based on `is_last_baseline`.
LayoutUnit GetLogicalBaseline(const LogicalBoxFragment& baseline_fragment,
                              FontBaseline font_baseline,
                              bool is_last_baseline);

// Calculates and stores an item's baseline in the appropriate track.
// `extra_margin` should include any margins and subgrid extra margins that need
// to be added to the baseline.
void StoreItemBaseline(const LogicalBoxFragment& baseline_fragment,
                       GridTrackSizingDirection track_direction,
                       FontBaseline font_baseline,
                       LayoutUnit extra_margin,
                       GridSizingTrackCollection& track_collection,
                       GridItemData& item);

// Computes the baseline offset for aligning a grid item within its
// baseline-sharing group. Returns the offset needed to align the item's
// baseline with its track's baseline, accounting for major/minor baseline
// groups.
LayoutUnit ComputeBaselineOffset(
    const GridItemData& grid_item,
    const GridLayoutTrackCollection& track_collection,
    const LogicalBoxFragment& baseline_fragment,
    const LogicalBoxFragment& fragment,
    FontBaseline font_baseline,
    GridTrackSizingDirection track_direction,
    LayoutUnit available_size);

// Aggregate all direct out of flow children from the grid container associated
// with `algorithm` to `opt_oof_children`, unless it's not provided.
template <typename LayoutAlgorithmType>
void BuildGridSizingSubtree(
    const LayoutAlgorithmType& algorithm,
    GridSizingTree* sizing_tree,
    HeapVector<Member<LayoutBox>>* opt_oof_children,
    const SubgriddedItemData& opt_subgrid_data = kNoSubgriddedItemData,
    const GridLineResolver* opt_parent_line_resolver = nullptr,
    bool must_invalidate_placement_cache = false,
    bool must_ignore_children = false);

template <typename LayoutAlgorithmType>
GridSizingTree BuildGridSizingTree(
    const LayoutAlgorithmType& algorithm,
    HeapVector<Member<LayoutBox>>* opt_oof_children = nullptr);

template <typename LayoutAlgorithmType>
GridSizingTree BuildGridSizingTreeIgnoringChildren(
    const LayoutAlgorithmType& algorithm);

// Calculate the initial fragment geometry for a subgrid item.
FragmentGeometry CalculateInitialFragmentGeometryForSubgrid(
    const GridItemData& subgrid_data,
    const ConstraintSpace& space,
    const GridSizingSubtree& sizing_subtree = kNoGridSizingSubtree);

// Helper which iterates over the sizing tree, and instantiates a subgrid
// algorithm to invoke the callback with.
template <typename LayoutAlgorithmType, typename CallbackFunc>
void ForEachSubgrid(const GridSizingSubtree& sizing_subtree,
                    const LayoutAlgorithmType& algorithm,
                    const CallbackFunc& callback_func,
                    bool should_compute_min_max_sizes = true) {
  // Exit early if this subtree doesn't have nested subgrids.
  auto next_subgrid_subtree = sizing_subtree.FirstChild();
  if (!next_subgrid_subtree) {
    return;
  }

  const auto& layout_data = sizing_subtree.LayoutData();

  for (const auto& grid_item : sizing_subtree.GetGridItems()) {
    if (!grid_item.IsSubgrid()) {
      continue;
    }

    const auto space =
        algorithm.CreateConstraintSpaceForLayout(grid_item, layout_data);
    const auto fragment_geometry = CalculateInitialFragmentGeometryForSubgrid(
        grid_item, space,
        should_compute_min_max_sizes ? next_subgrid_subtree
                                     : kNoGridSizingSubtree);

    // TODO(almaher): This should use GridLanesLayoutAlgorithm when the subgrid
    // is a grid-lanes container.
    const GridLayoutAlgorithm subgrid_algorithm(
        {grid_item.node, fragment_geometry, space});

    DCHECK(next_subgrid_subtree);
    callback_func(
        subgrid_algorithm, next_subgrid_subtree,
        SubgriddedItemData(grid_item, layout_data,
                           algorithm.GetConstraintSpace().GetWritingMode()));

    next_subgrid_subtree = next_subgrid_subtree.NextSibling();
  }
}

std::unique_ptr<GridLayoutTrackCollection> CreateSubgridTrackCollection(
    const SubgriddedItemData& subgrid_data,
    const ComputedStyle& style,
    const ConstraintSpace& space,
    const BoxStrut& border_scrollbar_padding,
    const LogicalSize grid_available_size,
    GridTrackSizingDirection track_direction);

// Initialize the track collections of a given grid sizing data.
void InitializeTrackCollection(const SubgriddedItemData& opt_subgrid_data,
                               const ComputedStyle& style,
                               const ConstraintSpace& space,
                               const BoxStrut& border_scrollbar_padding,
                               const LogicalSize grid_available_size,
                               GridTrackSizingDirection track_direction,
                               GridLayoutData* layout_data);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_LAYOUT_UTILS_H_
