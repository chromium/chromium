// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_ROOT_INLINE_BOX_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_ROOT_INLINE_BOX_PAINTER_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

struct PaintInfo;
class LayoutPoint;
class SVGRootInlineBox;

class SVGRootInlineBoxPainter {
  STACK_ALLOCATED();

 public:
  SVGRootInlineBoxPainter(const SVGRootInlineBox& svg_root_inline_box)
      : svg_root_inline_box_(svg_root_inline_box) {}

  void Paint(const PaintInfo&, const LayoutPoint& paint_offset);

 private:
  const SVGRootInlineBox& svg_root_inline_box_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_ROOT_INLINE_BOX_PAINTER_H_
