// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_IMAGE_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_IMAGE_PAINTER_H_

#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

struct PaintInfo;
class LayoutSVGImage;

class SVGImagePainter {
  STACK_ALLOCATED();

 public:
  SVGImagePainter(const LayoutSVGImage& layout_svg_image)
      : layout_svg_image_(layout_svg_image) {}

  void Paint(const PaintInfo&);

 private:
  // Assumes the PaintInfo context has had all local transforms applied.
  void PaintForeground(const PaintInfo&);
  FloatSize ComputeImageViewportSize() const;

  const LayoutSVGImage& layout_svg_image_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_IMAGE_PAINTER_H_
