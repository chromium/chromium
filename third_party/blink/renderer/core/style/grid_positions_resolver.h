// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GRID_POSITIONS_RESOLVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GRID_POSITIONS_RESOLVER_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/style/grid_position.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

struct GridSpan;
class LayoutBox;
class ComputedStyle;

enum GridPositionSide {
  kColumnStartSide,
  kColumnEndSide,
  kRowStartSide,
  kRowEndSide
};

enum GridTrackSizingDirection { kForColumns, kForRows };

class NamedLineCollection {
 public:
  NamedLineCollection(const ComputedStyle&,
                      const String& named_line,
                      GridTrackSizingDirection,
                      size_t last_line,
                      size_t auto_repeat_tracks_count);

  bool HasNamedLines();
  size_t FirstPosition();

  bool Contains(size_t line);

 private:
  size_t Find(size_t line);
  const Vector<size_t>* named_lines_indexes_ = nullptr;
  const Vector<size_t>* auto_repeat_named_lines_indexes_ = nullptr;

  size_t insertion_point_;
  size_t last_line_;
  size_t auto_repeat_total_tracks_;
  size_t auto_repeat_track_list_length_;

  DISALLOW_COPY_AND_ASSIGN(NamedLineCollection);
};

// This is a utility class with all the code related to grid items positions
// resolution.
class GridPositionsResolver {
  DISALLOW_NEW();

 public:
  static size_t ExplicitGridColumnCount(const ComputedStyle&,
                                        size_t auto_repeat_columns_count);
  static size_t ExplicitGridRowCount(const ComputedStyle&,
                                     size_t auto_repeat_rows_count);

  static GridPositionSide InitialPositionSide(GridTrackSizingDirection);
  static GridPositionSide FinalPositionSide(GridTrackSizingDirection);

  static size_t SpanSizeForAutoPlacedItem(const LayoutBox&,
                                          GridTrackSizingDirection);
  static GridSpan ResolveGridPositionsFromStyle(
      const ComputedStyle&,
      const LayoutBox&,
      GridTrackSizingDirection,
      size_t auto_repeat_tracks_count);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GRID_POSITIONS_RESOLVER_H_
