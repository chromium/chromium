// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_OBJECT_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_OBJECT_PAINTER_H_

#include "cc/paint/paint_flags.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class AffineTransform;
class ComputedStyle;
class GraphicsContext;

enum LayoutSVGResourceMode {
  kApplyToFillMode,
  kApplyToStrokeMode,
};

class SVGObjectPainter {
  STACK_ALLOCATED();

 public:
  SVGObjectPainter(const LayoutObject& layout_object)
      : layout_object_(layout_object) {
    DCHECK(layout_object.IsSVG());
  }

  // Initializes |paint_flags| for painting an SVG object or a part of the
  // object. Returns true if successful, and the caller can continue to paint
  // using |paint_flags|.
  bool PreparePaint(
      const GraphicsContext& context,
      bool is_rendering_clip_path_as_mask_image,
      const ComputedStyle&,
      LayoutSVGResourceMode,
      cc::PaintFlags& paint_flags,
      const AffineTransform* additional_paint_server_transform = nullptr);

  void PaintResourceSubtree(GraphicsContext&);

 private:
  bool ApplyPaintResource(
      const SVGPaint& paint,
      const AffineTransform* additional_paint_server_transform,
      cc::PaintFlags& flags);

  const LayoutObject& layout_object_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_OBJECT_PAINTER_H_
