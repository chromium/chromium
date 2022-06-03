// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_SHAPE_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_SHAPE_PAINTER_H_

#include "third_party/blink/renderer/platform/graphics/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/skia/include/core/SkPath.h"

namespace blink {

class GraphicsContext;
class LayoutSVGResourceMarker;
class LayoutSVGShape;
struct MarkerPosition;
struct PaintInfo;

class SVGShapePainter {
  STACK_ALLOCATED();

 public:
  SVGShapePainter(const LayoutSVGShape& layout_svg_shape)
      : layout_svg_shape_(layout_svg_shape) {}

  void Paint(const PaintInfo&);

 private:
  void FillShape(GraphicsContext&, const PaintFlags&, SkPathFillType);
  void StrokeShape(GraphicsContext&, const PaintFlags&);

  void PaintMarkers(const PaintInfo&);
  void PaintMarker(const PaintInfo&,
                   LayoutSVGResourceMarker&,
                   const MarkerPosition&,
                   float stroke_width);

  const LayoutSVGShape& layout_svg_shape_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_SHAPE_PAINTER_H_
