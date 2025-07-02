// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BORDER_SHAPE_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BORDER_SHAPE_PAINTER_H_

#include <optional>

#include "base/memory/stack_allocated.h"

namespace blink {

class ComputedStyle;
class GraphicsContext;
struct PhysicalRect;
class Path;

class BorderShapePainter {
  STACK_ALLOCATED();

 public:
  static bool Paint(GraphicsContext&,
                    const PhysicalRect&,
                    const ComputedStyle&);

  static std::optional<Path> InnerPath(const PhysicalRect&,
                                       const ComputedStyle&);
  static std::optional<Path> OuterPath(const PhysicalRect&,
                                       const ComputedStyle&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BORDER_SHAPE_PAINTER_H_
