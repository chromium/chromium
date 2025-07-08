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
class GridTrackList;

struct BoxStrut;
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
// auto_repeat_track_size` is not nullopt, this will indicate what to size an
// auto track definition within an auto repeater.
wtf_size_t CalculateAutomaticRepetitions(
    const GridTrackList& track_list,
    const LayoutUnit gutter_size,
    LayoutUnit available_size,
    LayoutUnit min_available_size,
    LayoutUnit max_available_size,
    std::optional<LayoutUnit> auto_repeat_track_size = std::nullopt);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_LAYOUT_UTILS_H_
