// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_PROPERTIES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_PROPERTIES_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_track_collection.h"
#include "third_party/blink/renderer/core/style/grid_positions_resolver.h"

namespace blink {
// This class stores various grid properties. Some of these properties
// depend on grid items and some depend on tracks, hence the need for a
// separate class to consolidate them. These properties can then be used
// to skip certain parts of the grid algorithm for better performance.
struct CORE_EXPORT NGGridProperties {
  NGGridProperties()
      : has_baseline_column(false),
        has_baseline_row(false),
        has_orthogonal_item(false) {}

  bool HasBaseline(const GridTrackSizingDirection track_direction) const;
  bool HasFlexibleTrack(const GridTrackSizingDirection track_direction) const;
  bool HasIntrinsicTrack(const GridTrackSizingDirection track_direction) const;
  bool IsDependentOnAvailableSize(
      const GridTrackSizingDirection track_direction) const;
  bool IsSpanningOnlyDefiniteTracks(
      const GridTrackSizingDirection track_direction) const;

  // TODO(layout-dev) Initialize these with {false} and remove the constructor
  // when the codebase moves to C++20 (this syntax isn't allowed in bitfields
  // prior to C++20).
  bool has_baseline_column : 1;
  bool has_baseline_row : 1;
  bool has_orthogonal_item : 1;

  TrackSpanProperties column_properties;
  TrackSpanProperties row_properties;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_PROPERTIES_H_
