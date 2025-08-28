// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_LAYOUT_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_LAYOUT_UTILS_H_

#include "third_party/blink/renderer/core/layout/geometry/static_position.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

class BlockNode;
class BoxFragmentBuilder;
class ConstraintSpace;
class GridLayoutTrackCollection;
class GridTrackList;

enum class AxisEdge;
struct BoxStrut;
struct GridItemData;
struct LogicalOffset;
struct LogicalSize;

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

// Common out-of-flow positioning utilities shared between grid and masonry.

// Computes the start offset and size for an out-of-flow item in a single
// direction (either inline or block).
void ComputeOutOfFlowOffsetAndSize(
    const GridItemData& out_of_flow_item,
    const GridLayoutTrackCollection& track_collection,
    const BoxStrut& borders,
    const LogicalSize& border_box_size,
    LayoutUnit* start_offset,
    LayoutUnit* size);

// Computes alignment offset for out-of-flow items.
void AlignmentOffsetForOutOfFlow(AxisEdge inline_axis_edge,
                                 AxisEdge block_axis_edge,
                                 LogicalSize container_size,
                                 LogicalStaticPosition::InlineEdge* inline_edge,
                                 LogicalStaticPosition::BlockEdge* block_edge,
                                 LogicalOffset* offset);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_LAYOUT_UTILS_H_
