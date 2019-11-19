// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_TEXT_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_TEXT_PAINTER_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

struct PaintInfo;
class LayoutSVGText;

class SVGTextPainter {
  STACK_ALLOCATED();

 public:
  SVGTextPainter(const LayoutSVGText& layout_svg_text)
      : layout_svg_text_(layout_svg_text) {}
  void Paint(const PaintInfo&);

 private:
  // Paint a hit test display item and record hit test data. This should be
  // called when painting the background even if there is no other painted
  // content.
  void RecordHitTestData(const PaintInfo&);

  const LayoutSVGText& layout_svg_text_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_TEXT_PAINTER_H_
