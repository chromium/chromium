// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_FOREIGN_OBJECT_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_FOREIGN_OBJECT_PAINTER_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

struct PaintInfo;
class LayoutBlockFlow;

class SVGForeignObjectPainter {
  STACK_ALLOCATED();

 public:
  explicit SVGForeignObjectPainter(
      const LayoutBlockFlow& layout_svg_foreign_object);

  void Paint(const PaintInfo&);
  void PaintLayer(const PaintInfo& paint_info);

 private:
  // layout_svg_foreign_object_ must be a LayoutSVGForeignObject or a
  // LayoutNGSVGForeignObject.
  const LayoutBlockFlow& layout_svg_foreign_object_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_FOREIGN_OBJECT_PAINTER_H_
