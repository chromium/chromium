// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_LAYOUT_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_LAYOUT_UTILS_H_

#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

class BlockNode;
class BoxFragmentBuilder;
class ConstraintSpace;
class GridLayoutTrackCollection;
class GridTrackList;
class LogicalBoxFragment;

enum class AxisEdge;
struct BoxStrut;
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
    const Vector<LayoutUnit>* intrinsic_repeat_track_sizes = nullptr);

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

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_LAYOUT_UTILS_H_
