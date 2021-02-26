// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_OBJECT_PAINTER_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_OBJECT_PAINTER_BASE_H_

#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ComputedStyle;
class GraphicsContext;
struct PaintInfo;
struct PhysicalRect;

// Base class for object painting. Has no dependencies on the layout tree and
// thus provides functionality and definitions that can be shared between both
// legacy layout and LayoutNG.
class ObjectPainterBase {
  STACK_ALLOCATED();

 public:
  static void DrawBoxSide(GraphicsContext& context,
                          const IntRect& snapped_edge_rect,
                          BoxSide side,
                          Color color,
                          EBorderStyle style) {
    DrawLineForBoxSide(context, snapped_edge_rect.X(), snapped_edge_rect.Y(),
                       snapped_edge_rect.MaxX(), snapped_edge_rect.MaxY(), side,
                       color, style, 0, 0, true);
  }

  // TODO(wangxianzhu): The float parameters are truncated to int in the
  // function, which implicitly snaps to whole pixels incorrectly. We should
  // always use the above function. For now the only outside caller is
  // BoxBorderPainter::PaintOneBorderSide().
  static void DrawLineForBoxSide(GraphicsContext&,
                                 float x1,
                                 float y1,
                                 float x2,
                                 float y2,
                                 BoxSide,
                                 Color,
                                 EBorderStyle,
                                 int adjacent_edge_width1,
                                 int adjacent_edge_width2,
                                 bool antialias = false);

 protected:
  ObjectPainterBase() = default;
  void PaintOutlineRects(const PaintInfo&,
                         const Vector<PhysicalRect>&,
                         const ComputedStyle&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_OBJECT_PAINTER_BASE_H_
