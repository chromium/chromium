// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_INLINE_FLOW_BOX_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_INLINE_FLOW_BOX_PAINTER_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

struct PaintInfo;
class LayoutPoint;
class SVGInlineFlowBox;

class SVGInlineFlowBoxPainter {
  STACK_ALLOCATED();

 public:
  SVGInlineFlowBoxPainter(const SVGInlineFlowBox& svg_inline_flow_box)
      : svg_inline_flow_box_(svg_inline_flow_box) {}

  void PaintSelectionBackground(const PaintInfo&);
  void Paint(const PaintInfo&, const LayoutPoint& paint_offset);

 private:
  const SVGInlineFlowBox& svg_inline_flow_box_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_INLINE_FLOW_BOX_PAINTER_H_
