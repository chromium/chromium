// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SCROLLABLE_AREA_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SCROLLABLE_AREA_PAINTER_H_

#include "third_party/blink/renderer/platform/heap/handle.h"

namespace gfx {
class Rect;
class Vector2d;
}

namespace blink {

class CullRect;
class GraphicsContext;
class Scrollbar;
struct PaintInfo;
class PaintLayerScrollableArea;
struct PhysicalOffset;

class ScrollableAreaPainter {
  STACK_ALLOCATED();

 public:
  explicit ScrollableAreaPainter(
      PaintLayerScrollableArea& paint_layer_scrollable_area)
      : scrollable_area_(&paint_layer_scrollable_area) {}
  ScrollableAreaPainter(const ScrollableAreaPainter&) = delete;
  ScrollableAreaPainter& operator=(const ScrollableAreaPainter&) = delete;

  void PaintOverflowControls(const PaintInfo&,
                             const gfx::Vector2d& paint_offset);
  void PaintScrollbar(GraphicsContext&,
                      Scrollbar&,
                      const gfx::Vector2d& paint_offset,
                      const CullRect&);
  void PaintResizer(GraphicsContext&,
                    const gfx::Vector2d& paint_offset,
                    const CullRect&);
  void PaintScrollCorner(GraphicsContext&,
                         const gfx::Vector2d& paint_offset,
                         const CullRect&);

  // Records a scroll hit test data to force main thread handling of events
  // in the expanded resizer touch area.
  void RecordResizerScrollHitTestData(GraphicsContext&,
                                      const PhysicalOffset& paint_offset);

 private:
  void DrawPlatformResizerImage(GraphicsContext&,
                                const gfx::Rect& resizer_corner_rect);

  PaintLayerScrollableArea& GetScrollableArea() const;

  PaintLayerScrollableArea* scrollable_area_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SCROLLABLE_AREA_PAINTER_H_
