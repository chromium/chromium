// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_MASK_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_MASK_PAINTER_H_

#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class GraphicsContext;
class LayoutObject;
class LayoutSVGResourceMasker;

class SVGMaskPainter {
  STACK_ALLOCATED();

 public:
  SVGMaskPainter(LayoutSVGResourceMasker& mask) : mask_(mask) {}

  bool PrepareEffect(const LayoutObject&, GraphicsContext&);
  void FinishEffect(const LayoutObject&, GraphicsContext&);

 private:
  LayoutSVGResourceMasker& mask_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_MASK_PAINTER_H_
