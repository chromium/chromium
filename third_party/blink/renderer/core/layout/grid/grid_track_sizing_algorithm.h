// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_TRACK_SIZING_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_TRACK_SIZING_ALGORITHM_H_

#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/style/grid_enums.h"

namespace blink {

class ComputedStyle;
class GridItems;
class GridSizingTrackCollection;
struct BoxStrut;

class GridTrackSizingAlgorithm {
  STACK_ALLOCATED();

 public:
  struct FirstSetGeometry {
    LayoutUnit gutter_size;
    LayoutUnit start_offset;
  };

  // Caches the track span properties necessary for the track sizing algorithm
  // to work based on the grid items' placement within the track collection.
  static void CacheGridItemsProperties(
      const GridSizingTrackCollection& track_collection,
      GridItems* grid_items);

  // Calculates the specified `[column|row]-gap` of the container.
  static LayoutUnit CalculateGutterSize(
      const ComputedStyle& container_style,
      const LogicalSize& container_available_size,
      GridTrackSizingDirection track_direction,
      LayoutUnit parent_gutter_size = LayoutUnit());

  // For the first track, computes the start offset and gutter size based on the
  // alignment properties and available size of the container.
  static FirstSetGeometry ComputeFirstSetGeometry(
      const GridSizingTrackCollection& track_collection,
      const ComputedStyle& container_style,
      const LogicalSize& container_available_size,
      const BoxStrut& container_border_scrollbar_padding);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_TRACK_SIZING_ALGORITHM_H_
