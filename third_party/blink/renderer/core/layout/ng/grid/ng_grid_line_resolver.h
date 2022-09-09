// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_LINE_RESOLVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_LINE_RESOLVER_H_

#include "third_party/blink/renderer/core/style/computed_grid_track_list.h"
#include "third_party/blink/renderer/core/style/grid_enums.h"
#include "third_party/blink/renderer/core/style/named_grid_lines_map.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

struct GridSpan;
class ComputedStyle;
class GridPosition;
class NGGridNamedLineCollection;

// This is a utility class with all the code related to grid items positions
// resolution.
class NGGridLineResolver {
  DISALLOW_NEW();

 public:
  explicit NGGridLineResolver(const ComputedStyle& grid_style)
      : style_(&grid_style) {}

  wtf_size_t ExplicitGridColumnCount(
      wtf_size_t auto_repeat_columns_count,
      wtf_size_t subgrid_span_size = kNotFound) const;

  wtf_size_t ExplicitGridRowCount(
      wtf_size_t auto_repeat_rows_count,
      wtf_size_t subgrid_span_size = kNotFound) const;

  wtf_size_t SpanSizeForAutoPlacedItem(const ComputedStyle&,
                                       GridTrackSizingDirection) const;

  GridSpan ResolveGridPositionsFromStyle(
      const ComputedStyle&,
      GridTrackSizingDirection,
      wtf_size_t auto_repeat_tracks_count,
      bool is_parent_grid_container = false,
      wtf_size_t subgrid_span_size = kNotFound) const;

 private:
  const NamedGridLinesMap& NamedLinesMapForDirection(
      GridTrackSizingDirection track_direction) const;

  const ComputedGridTrackList& ComputedGridTrackListForDirection(
      GridTrackSizingDirection track_direction) const;

  GridSpan ResolveGridPositionAgainstOppositePosition(
      int opposite_line,
      const GridPosition& position,
      GridPositionSide side,
      wtf_size_t auto_repeat_tracks_count,
      wtf_size_t subgrid_span_size) const;

  GridSpan ResolveNamedGridLinePositionAgainstOppositePosition(
      int opposite_line,
      const GridPosition& position,
      wtf_size_t auto_repeat_tracks_count,
      GridPositionSide side,
      wtf_size_t subgrid_span_size) const;

  int ResolveGridPositionFromStyle(const GridPosition& position,
                                   GridPositionSide side,
                                   wtf_size_t auto_repeat_tracks_count,
                                   bool is_parent_grid_container,
                                   wtf_size_t subgrid_span_size) const;

  wtf_size_t ExplicitGridSizeForSide(GridPositionSide side,
                                     wtf_size_t auto_repeat_tracks_count,
                                     wtf_size_t subgrid_span_size) const;

  wtf_size_t LookAheadForNamedGridLine(
      int start,
      wtf_size_t number_of_lines,
      wtf_size_t grid_last_line,
      NGGridNamedLineCollection& lines_collection) const;

  int LookBackForNamedGridLine(
      int end,
      wtf_size_t number_of_lines,
      int grid_last_line,
      NGGridNamedLineCollection& lines_collection) const;

  wtf_size_t SpanSizeFromPositions(const GridPosition& initial_position,
                                   const GridPosition& final_position) const;

  int ResolveNamedGridLinePositionFromStyle(const GridPosition& position,
                                            GridPositionSide side,
                                            wtf_size_t auto_repeat_tracks_count,
                                            wtf_size_t subgrid_span_size) const;

  void InitialAndFinalPositionsFromStyle(
      const ComputedStyle& grid_item_style,
      GridTrackSizingDirection track_direction,
      GridPosition& initial_position,
      GridPosition& final_position) const;

  GridSpan DefiniteGridSpanWithNamedSpanAgainstOpposite(
      int opposite_line,
      const GridPosition& position,
      GridPositionSide side,
      int last_line,
      NGGridNamedLineCollection& lines_collection) const;

  scoped_refptr<const ComputedStyle> style_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_LINE_RESOLVER_H_
