// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_GAP_DECORATIONS_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_GAP_DECORATIONS_PAINTER_H_

#include "third_party/blink/renderer/core/style/grid_enums.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class GapGeometry;
class PhysicalBoxFragment;
struct PaintInfo;
struct PhysicalRect;

// Handles painting of decorations within gaps in CSS Grid, Flexbox and
// MultiColumn layouts as described by the CSS Gap Decorations spec
// (https://www.w3.org/TR/css-gaps-1/).
class GapDecorationsPainter {
  STACK_ALLOCATED();

 public:
  explicit GapDecorationsPainter(const PhysicalBoxFragment& box_fragment)
      : box_fragment_(box_fragment) {}

  void Paint(GridTrackSizingDirection track_direction,
             const PaintInfo& paint_info,
             const PhysicalRect& paint_rect,
             const GapGeometry& gap_geometry);

 private:
  const PhysicalBoxFragment& box_fragment_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_GAP_DECORATIONS_PAINTER_H_
