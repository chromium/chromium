// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BORDER_SHAPE_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BORDER_SHAPE_PAINTER_H_

#include <optional>

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

  static Path InnerPath(const ComputedStyle&,
                        const PhysicalRect& inner_reference_rect);
  static Path OuterPath(const ComputedStyle&,
                        const PhysicalRect& outer_reference_rect);

  static PhysicalBoxStrut VisualOutsets(
      const ComputedStyle&,
      const PhysicalRect& border_rect,
      const PhysicalRect& outer_reference_rect,
      const PhysicalRect& inner_reference_rect);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BORDER_SHAPE_PAINTER_H_
