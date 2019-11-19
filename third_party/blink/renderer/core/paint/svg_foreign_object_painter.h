// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_FOREIGN_OBJECT_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_FOREIGN_OBJECT_PAINTER_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

struct PaintInfo;
class LayoutSVGForeignObject;

class SVGForeignObjectPainter {
  STACK_ALLOCATED();

 public:
  SVGForeignObjectPainter(
      const LayoutSVGForeignObject& layout_svg_foreign_object)
      : layout_svg_foreign_object_(layout_svg_foreign_object) {}
  void Paint(const PaintInfo&);

  void PaintLayer(const PaintInfo& paint_info);

 private:
  const LayoutSVGForeignObject& layout_svg_foreign_object_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_FOREIGN_OBJECT_PAINTER_H_
