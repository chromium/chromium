// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GRID_POSITIONS_RESOLVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GRID_POSITIONS_RESOLVER_H_

#include "third_party/blink/renderer/core/style/grid_enums.h"
#include "third_party/blink/renderer/core/style/grid_position.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

struct GridSpan;
class LayoutBox;
class ComputedStyle;

class NamedLineCollection {
 public:
  NamedLineCollection(const ComputedStyle&,
                      const String& named_line,
                      GridTrackSizingDirection,
                      wtf_size_t last_line,
                      wtf_size_t auto_repeat_tracks_count,
                      bool is_subgridded_to_parent = false);

  bool HasNamedLines();
  wtf_size_t FirstPosition();

  bool Contains(wtf_size_t line);

 private:
  bool HasExplicitNamedLines();
  wtf_size_t FirstExplicitPosition();
  const Vector<wtf_size_t>* named_lines_indexes_ = nullptr;
  const Vector<wtf_size_t>* auto_repeat_named_lines_indexes_ = nullptr;
  const Vector<wtf_size_t>* implicit_named_lines_indexes_ = nullptr;

  bool is_standalone_grid_;
  wtf_size_t insertion_point_;
  wtf_size_t last_line_;
  wtf_size_t auto_repeat_total_tracks_;
  wtf_size_t auto_repeat_track_list_length_;

  NamedLineCollection(const NamedLineCollection&) = delete;
  NamedLineCollection& operator=(const NamedLineCollection&) = delete;
};

// This is a utility class with all the code related to grid items positions
// resolution.
class GridPositionsResolver {
  DISALLOW_NEW();

 public:
  static wtf_size_t ExplicitGridColumnCount(
      const ComputedStyle&,
      wtf_size_t auto_repeat_columns_count,
      wtf_size_t subgrid_span_size = kNotFound);

  static wtf_size_t ExplicitGridRowCount(
      const ComputedStyle&,
      wtf_size_t auto_repeat_rows_count,
      wtf_size_t subgrid_span_size = kNotFound);

  static wtf_size_t SpanSizeForAutoPlacedItem(const ComputedStyle&,
                                              GridTrackSizingDirection);

  static GridSpan ResolveGridPositionsFromStyle(
      const ComputedStyle&,
      const ComputedStyle&,
      GridTrackSizingDirection,
      wtf_size_t auto_repeat_tracks_count,
      bool is_subgridded_to_parent = false,
      wtf_size_t subgrid_span_size = kNotFound);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GRID_POSITIONS_RESOLVER_H_
