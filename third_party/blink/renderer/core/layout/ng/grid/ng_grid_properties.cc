// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_properties.h"

#include "third_party/blink/renderer/core/style/grid_positions_resolver.h"

namespace blink {

NGGridProperties::NGGridProperties()
    : has_auto_max_column(false),
      has_auto_max_row(false),
      has_auto_min_column(false),
      has_auto_min_row(false),
      has_baseline_column(false),
      has_baseline_row(false),
      has_flexible_column(false),
      has_flexible_row(false),
      has_intrinsic_column(false),
      has_intrinsic_row(false),
      has_orthogonal_item(false) {}

bool NGGridProperties::HasBaseline(GridTrackSizingDirection direction) const {
  return (direction == kForColumns)
             ? (has_baseline_column ||
                (has_orthogonal_item && has_baseline_row))
             : (has_baseline_row ||
                (has_orthogonal_item && has_baseline_column));
}

bool NGGridProperties::HasFlexibleTrack(
    GridTrackSizingDirection direction) const {
  return (direction == kForColumns) ? has_flexible_column : has_flexible_row;
}

bool NGGridProperties::HasIntrinsicTrack(
    GridTrackSizingDirection direction) const {
  return (direction == kForColumns) ? has_intrinsic_column : has_intrinsic_row;
}

bool NGGridProperties::HasAutoMaxTrack(
    GridTrackSizingDirection direction) const {
  return (direction == kForColumns)
             ? (has_auto_min_column || has_auto_max_column)
             : (has_auto_min_row || has_auto_max_row);
}

bool NGGridPlacementProperties::operator==(
    const NGGridPlacementProperties& other) const {
  return column_start_offset == other.column_start_offset &&
         row_start_offset == other.row_start_offset &&
         minor_max_end_line == other.minor_max_end_line &&
         positions == other.positions;
}

}  // namespace blink
