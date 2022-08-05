// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_LINE_RESOLVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_LINE_RESOLVER_H_

#include "third_party/blink/renderer/core/style/grid_enums.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

struct GridSpan;
class ComputedStyle;

// This is a utility class with all the code related to grid items positions
// resolution.
class NGGridLineResolver {
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
      bool is_parent_grid_container = false,
      wtf_size_t subgrid_span_size = kNotFound);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_LINE_RESOLVER_H_
