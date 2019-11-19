// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SCROLLABLE_AREA_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SCROLLABLE_AREA_PAINTER_H_

#include "base/macros.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class CullRect;
class DisplayItemClient;
class GraphicsContext;
class IntPoint;
class IntRect;
class Scrollbar;
struct PaintInfo;
class PaintLayerScrollableArea;

class ScrollableAreaPainter {
  STACK_ALLOCATED();

 public:
  explicit ScrollableAreaPainter(
      PaintLayerScrollableArea& paint_layer_scrollable_area)
      : scrollable_area_(&paint_layer_scrollable_area) {}

  void PaintOverflowControls(const PaintInfo&, const IntPoint& paint_offset);
  void PaintResizer(GraphicsContext&,
                    const IntPoint& paint_offset,
                    const CullRect&);
  void PaintScrollCorner(GraphicsContext&,
                         const IntPoint& paint_offset,
                         const CullRect&);

 private:
  void DrawPlatformResizerImage(GraphicsContext&,
                                const IntRect& resizer_corner_rect);
  void PaintScrollbar(GraphicsContext&,
                      Scrollbar&,
                      const CullRect&,
                      const IntPoint& paint_offset);

  PaintLayerScrollableArea& GetScrollableArea() const;
  const DisplayItemClient& DisplayItemClientForCorner() const;

  Member<PaintLayerScrollableArea> scrollable_area_;

  DISALLOW_COPY_AND_ASSIGN(ScrollableAreaPainter);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SCROLLABLE_AREA_PAINTER_H_
