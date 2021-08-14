// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_PROPERTIES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_PROPERTIES_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/style/grid_positions_resolver.h"

namespace blink {
// This class stores various grid properties. Some of these properties
// depend on grid items and some depend on tracks, hence the need for a
// separate class to consolidate them. These properties can then be used
// to skip certain parts of the grid algorithm for better performance.
struct CORE_EXPORT NGGridProperties {
  NGGridProperties();

 public:
  bool HasBaseline(GridTrackSizingDirection direction) const;
  bool HasFlexibleTrack(GridTrackSizingDirection direction) const;
  bool HasIntrinsicTrack(GridTrackSizingDirection direction) const;
  bool HasAutoMaxTrack(GridTrackSizingDirection direction) const;

  // TODO(layout-dev) Initialize these with {false} and remove the constructor
  // when the codebase moves to C++20 (this syntax isn't allowed in bitfields
  // prior to C++20).
  bool has_auto_max_column : 1;
  bool has_auto_max_row : 1;
  bool has_auto_min_column : 1;
  bool has_auto_min_row : 1;
  bool has_baseline_column : 1;
  bool has_baseline_row : 1;
  bool has_flexible_column : 1;
  bool has_flexible_row : 1;
  bool has_intrinsic_column : 1;
  bool has_intrinsic_row : 1;
  bool has_orthogonal_item : 1;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_PROPERTIES_H_
