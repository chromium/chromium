// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_LAYOUT_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_LAYOUT_UTILS_H_

#include "base/functional/function_ref.h"
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
class GridItems;
class GridLayoutTrackCollection;
class GridLineResolver;
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
                          LayoutUnit item_stacking_position,
                          bool item_moved_to_earlier_opening) = 0;

  virtual std::optional<LayoutUnit> FirstBaseline() const = 0;
  virtual std::optional<LayoutUnit> LastBaseline() const = 0;
};

// Performs a layout of `grid_item` for measurement purposes. Disables layout
// side effects when appropriate.
const LayoutResult* LayoutGridItemForMeasure(
    const GridItemData& grid_item,
    const ConstraintSpace& constraint_space,
    SizingConstraint sizing_constraint);

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

// Computes the start/end offset for an out-of-flow item in a single direction.
LayoutUnit TrackStartOffset(const GridLayoutTrackCollection& track_collection,
                            const wtf_size_t range_index,
                            const wtf_size_t offset_in_range);
LayoutUnit TrackEndOffset(const GridLayoutTrackCollection& track_collection,
                          const wtf_size_t range_index,
                          const wtf_size_t offset_in_range);

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
// `min_content_contribution`, `max_content_contribution`, and
// `subgrid_minmax_sizes` are callbacks that lazily compute expensive layout
// values only when actually needed. `min_content_contribution` and
// `max_content_contribution` compute the content based min and maximum
// contributions for the provided `grid_item` respectively.
// `subgrid_minmax_sizes` computes the min/max size result for a subgrid item.
//
// This method will set `maybe_clamp` to true if the content based contribution
// was returned and should be considered for clamping. Otherwise, it will be set
// to false.
//
// [1] https://drafts.csswg.org/css-grid/#min-size-auto
LayoutUnit CalculateIntrinsicMinimumContribution(
    bool is_parallel_with_track_direction,
    bool special_spanning_criteria,
    base::FunctionRef<LayoutUnit()> min_content_contribution,
    base::FunctionRef<LayoutUnit()> max_content_contribution,
    base::FunctionRef<MinMaxSizesResult()> subgrid_minmax_sizes,
    const ConstraintSpace& space,
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
                            const GridLayoutData& layout_data,
                            GridTrackSizingDirection track_direction);

// Returns the baseline of an item from its fragment. Handles both first and
// last baseline based on `is_last_baseline`.
LayoutUnit GetLogicalBaseline(const LogicalBoxFragment& baseline_fragment,
                              FontBaseline font_baseline,
                              bool is_last_baseline);

// Updates `layout_data` with a baseline value on the appropriate track for the
// given item, based on its baseline-sharing group (major → start-most track,
// minor → end-most track).
void SetTrackBaseline(const GridItemData& grid_item,
                      GridTrackSizingDirection track_direction,
                      LayoutUnit baseline,
                      GridLayoutData& layout_data);

// Calculates and stores an item's baseline in the appropriate track.
// `extra_margin` should include any margins and subgrid extra margins that need
// to be added to the baseline.
void StoreItemBaseline(const LogicalBoxFragment& baseline_fragment,
                       GridTrackSizingDirection track_direction,
                       FontBaseline font_baseline,
                       LayoutUnit extra_margin,
                       GridLayoutData& layout_data,
                       GridItemData& item);

// Computes the baseline offset for aligning a grid item within its
// baseline-sharing group. Returns the offset needed to align the item's
// baseline with its track's baseline, accounting for major/minor baseline
// groups.
LayoutUnit ComputeBaselineOffset(const GridItemData& grid_item,
                                 const GridLayoutData& layout_data,
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
    const GridLineResolver& line_resolver,
    GridSizingTree* sizing_tree,
    HeapVector<Member<LayoutBox>>* opt_oof_children,
    const SubgriddedItemData& opt_subgrid_data = kNoSubgriddedItemData,
    const GridLineResolver* opt_parent_line_resolver = nullptr,
    SizingConstraint sizing_constraint = SizingConstraint::kLayout,
    bool must_invalidate_placement_cache = false,
    bool must_ignore_children = false,
    bool needs_intrinsic_track_size = false);

template <typename LayoutAlgorithmType>
GridSizingTree BuildGridSizingTree(
    const LayoutAlgorithmType& algorithm,
    const GridLineResolver& line_resolver,
    HeapVector<Member<LayoutBox>>* opt_oof_children = nullptr,
    SizingConstraint sizing_constraint = SizingConstraint::kLayout,
    bool needs_intrinsic_track_size = false);

template <typename LayoutAlgorithmType>
GridSizingTree BuildGridSizingTreeIgnoringChildren(
    const LayoutAlgorithmType& algorithm,
    const GridLineResolver& line_resolver,
    SizingConstraint sizing_constraint = SizingConstraint::kLayout,
    bool needs_intrinsic_track_size = false);

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

    const SubgriddedItemData subgridded_item(
        grid_item, &layout_data,
        algorithm.GetConstraintSpace().GetWritingMode());
    const auto space =
        algorithm.CreateConstraintSpaceForLayout(subgridded_item);
    const auto fragment_geometry = CalculateInitialFragmentGeometryForSubgrid(
        grid_item, space,
        should_compute_min_max_sizes ? next_subgrid_subtree
                                     : kNoGridSizingSubtree);

    // TODO(almaher): This should use GridLanesLayoutAlgorithm when the subgrid
    // is a grid-lanes container.
    const GridLayoutAlgorithm subgrid_algorithm(
        {grid_item.node, fragment_geometry, space});

    DCHECK(next_subgrid_subtree);
    callback_func(subgrid_algorithm, next_subgrid_subtree, subgridded_item);

    next_subgrid_subtree = next_subgrid_subtree.NextSibling();
  }
}

GridLayoutTrackCollection* CreateSubgridTrackCollection(
    const SubgriddedItemData& subgrid_data,
    const ComputedStyle& style,
    const ConstraintSpace& space,
    const BoxStrut& border_scrollbar_padding,
    const LogicalSize grid_available_size,
    GridTrackSizingDirection track_direction);

GridTrackBaselines* CreateSubgridBaselines(
    const SubgriddedItemData& subgrid_data,
    const ComputedStyle& style,
    const ConstraintSpace& space,
    const BoxStrut& border_scrollbar_padding,
    const LogicalSize grid_available_size,
    GridTrackSizingDirection track_direction,
    const GridTrackBaselines& parent_baselines);

// Initialize the track collections of a given grid sizing data.
void InitializeTrackCollection(const SubgriddedItemData& opt_subgrid_data,
                               const ComputedStyle& style,
                               const ConstraintSpace& space,
                               const BoxStrut& border_scrollbar_padding,
                               const LogicalSize grid_available_size,
                               GridTrackSizingDirection track_direction,
                               GridLayoutData* layout_data);

// Checks if any of the items within `grid_items` have block-size dependent
// sizing.
bool HasBlockSizeDependentGridItem(const GridItems& grid_items);

// Appends items from any subgridded children to `grid_items`.
template <typename NodeType>
void AppendSubgriddedItems(const NodeType& node, GridItems* grid_items) {
  DCHECK(grid_items);

  const auto& root_grid_style = node.Style();
  for (wtf_size_t i = 0; i < grid_items->Size(); ++i) {
    auto& current_item = grid_items->At(i);

    if (!current_item.must_consider_grid_items_for_column_sizing &&
        !current_item.must_consider_grid_items_for_row_sizing) {
      continue;
    }

    // TODO(almaher): This should eventually support grid lanes, as well.
    bool must_invalidate_placement_cache = false;
    const auto subgrid = To<GridNode>(current_item.node);

    auto* subgridded_items = subgrid.ConstructGridItems(
        subgrid.CachedLineResolver(), root_grid_style, subgrid.Style(),
        current_item.must_consider_grid_items_for_column_sizing,
        current_item.must_consider_grid_items_for_row_sizing,
        &must_invalidate_placement_cache,
        /*parent_is_auto_placed=*/current_item.is_auto_placed);

    DCHECK(!must_invalidate_placement_cache)
        << "We shouldn't need to invalidate the placement cache if we relied "
           "on the cached line resolver; it must produce the same placement.";

    for (auto& subgridded_item : *subgridded_items) {
      subgridded_item.is_subgridded_to_parent_grid = true;

      // TODO(almaher): We will eventually need to update this for grid lanes
      // subgrids.
      //
      // If the subgrid has a different writing mode, columns and rows are
      // swapped in its coordinate system relative to the root grid.
      if (!current_item.is_parallel_with_root_grid) {
        std::swap(subgridded_item.resolved_position.columns,
                  subgridded_item.resolved_position.rows);
      }

      node.AdjustSubgriddedItemSpan(current_item, subgridded_item);
    }
    grid_items->Append(subgridded_items);
  }
}

// Iterates over subgrids in `sizing_subtree` and initializes their track sizes.
template <typename LayoutAlgorithmType>
void InitializeTrackSizesForEachSubgrid(
    const GridSizingSubtree& sizing_subtree,
    const LayoutAlgorithmType& algorithm,
    const std::optional<GridTrackSizingDirection>& opt_track_direction) {
  // TODO(almaher): Support grid-lanes subgrids as well.
  ForEachSubgrid(
      sizing_subtree, algorithm,
      [&](const GridLayoutAlgorithm& subgrid_algorithm,
          const GridSizingSubtree& subgrid_subtree,
          const SubgriddedItemData& subgrid_data) {
        subgrid_algorithm.InitializeTrackSizes(
            subgrid_subtree, subgrid_data,
            subgrid_data->RelativeDirectionFilterInSubgrid(
                opt_track_direction));
      },
      /*should_compute_min_max_sizes=*/false);
}

// Iterates over subgrids in `sizing_subtree` and completes their track sizing
// algorithm.
template <typename LayoutAlgorithmType>
void CompleteTrackSizingAlgorithmForEachSubgrid(
    const GridSizingSubtree& sizing_subtree,
    const LayoutAlgorithmType& algorithm,
    GridTrackSizingDirection track_direction,
    SizingConstraint sizing_constraint,
    bool* opt_needs_additional_pass) {
  // TODO(almaher): Support grid-lanes subgrids as well.
  ForEachSubgrid(
      sizing_subtree, algorithm,
      [&](const GridLayoutAlgorithm& subgrid_algorithm,
          const GridSizingSubtree& subgrid_subtree,
          const SubgriddedItemData& subgrid_data) {
        subgrid_algorithm.CompleteTrackSizingAlgorithm(
            subgrid_subtree, subgrid_data,
            subgrid_data->RelativeDirectionInSubgrid(track_direction),
            sizing_constraint, opt_needs_additional_pass);
      });
}

// Iterates over subgrids in `sizing_subtree` and performs a baseline
// alignment pass for each.
template <typename LayoutAlgorithmType>
void ComputeBaselineAlignmentForEachSubgrid(
    const GridSizingSubtree& sizing_subtree,
    const LayoutAlgorithmType& algorithm,
    const GridLayoutTree* layout_tree,
    const std::optional<GridTrackSizingDirection>& opt_track_direction,
    SizingConstraint sizing_constraint) {
  // TODO(almaher): Support grid-lanes subgrids as well.
  ForEachSubgrid(
      sizing_subtree, algorithm,
      [&](const GridLayoutAlgorithm& subgrid_algorithm,
          const GridSizingSubtree& subgrid_subtree,
          const SubgriddedItemData& subgrid_data) {
        subgrid_algorithm.ComputeBaselineAlignment(
            layout_tree, subgrid_subtree, subgrid_data,
            subgrid_data->RelativeDirectionFilterInSubgrid(opt_track_direction),
            sizing_constraint);
      });
}

// Validates the min/max sizes cache for subgrids in the sizing tree. A
// subgrid might need to invalidate the cache if it inherited a different track
// collection in its subgridded axis. Returns true if invalidation was needed.
bool ValidateMinMaxSizesCache(const BlockNode& grid_node,
                              const GridSizingSubtree& sizing_subtree,
                              GridTrackSizingDirection track_direction);

// Returns true if an additional track sizing pass is needed after the block
// size transitions from indefinite to definite.
bool NeedsAdditionalLayoutPass(
    const ComputedStyle& style,
    const ConstraintSpace& constraint_space,
    const BlockNode& node,
    const BoxStrut& border_padding,
    const GridSizingTrackCollection& track_collection,
    LayoutUnit grid_inline_size);

// Returns the synthesized logical baseline for a grid item. This is used when
// computing min/max content contributions without a full layout result.
LayoutUnit GetSynthesizedLogicalBaseline(
    const GridItemData& grid_item,
    LayoutUnit block_size,
    GridTrackSizingDirection track_direction);

// Accommodates extra margins from subgrid items in the given track collection.
// A subgrid's border/padding/margin can extend beyond the parent's track edges
// and must be accounted for by flooring the base sizes of the edge tracks it
// spans. For auto-placed subgrids in grid-lanes, the final position isn't known
// yet, so the extra margins are accommodated at every possible start/end set
// pair for the subgrid's set span size.
void AccommodateSubgridExtraMargins(const GridSizingSubtree& sizing_subtree,
                                    GridSizingTrackCollection& track_collection,
                                    GridTrackSizingDirection track_direction);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_LAYOUT_UTILS_H_
