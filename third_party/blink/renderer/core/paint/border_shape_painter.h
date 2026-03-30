// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BORDER_SHAPE_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BORDER_SHAPE_PAINTER_H_

#include "base/memory/stack_allocated.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"

namespace blink {

class ComputedStyle;
class GraphicsContext;
struct PhysicalRect;
class Path;

struct BorderShapeReferenceRects {
  STACK_ALLOCATED();

 public:
  PhysicalRect outer;
  PhysicalRect inner;
};

class BorderShapePainter {
  STACK_ALLOCATED();

 public:
  static bool Paint(GraphicsContext&,
                    const ComputedStyle&,
                    const PhysicalRect& outer_reference_rect,
                    const PhysicalRect& inner_reference_rect);

  // Paints an outline that follows the border-shape path.
  // Returns true if an outline was painted.
  static bool PaintOutline(GraphicsContext&,
                           const ComputedStyle&,
                           const PhysicalRect& outer_reference_rect,
                           int outline_width,
                           int outline_offset);

  static Path InnerPath(const ComputedStyle&,
                        const PhysicalRect& inner_reference_rect);
  static Path OuterPath(const ComputedStyle&,
                        const PhysicalRect& outer_reference_rect);


  // Returns an outer path offset by the given amount (positive = outward).
  static Path OuterPathWithOffset(const ComputedStyle&,
                                  const PhysicalRect& outer_reference_rect,
                                  float offset);

  // Returns the union of |path| and its stroke at |stroke_thickness|,
  // matching the shape used when painting border-shape shadows. Shared
  // between painting (BoxPainterBase) and ink overflow (VisualOutsets).
  static Path ExpandPathWithStroke(const Path& path, float stroke_thickness);

  // Returns the complete ink overflow outsets for a border-shape element:
  // both the visual extent of the border path (where the border stroke/fill
  // draws outside the border box) and the visual extent of any normal
  // (non-inset) box-shadows.
  //
  // Shadow outsets are computed by replicating the exact path the painter
  // builds — ExpandPathWithStroke(OuterPath, spread*2) — then outset
  // by sigma_3 = ceil(3*sigma), which is how Skia bounds a box blur.
  // This is more precise than BoxDecorationOutsets(), which omits the blur
  // term from the path expansion. Callers should Unite (not Expand/add) with
  // existing outsets so that these path-based shadow bounds supersede the
  // approximate bounds from BoxDecorationOutsets().
  static PhysicalBoxStrut VisualOutsets(
      const ComputedStyle&,
      const PhysicalRect& border_rect,
      const PhysicalRect& outer_reference_rect,
      const PhysicalRect& inner_reference_rect);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BORDER_SHAPE_PAINTER_H_
