// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/svg_inline_flow_box_painter.h"

#include "third_party/blink/renderer/core/layout/api/line_layout_api_shim.h"
#include "third_party/blink/renderer/core/layout/svg/line/svg_inline_flow_box.h"
#include "third_party/blink/renderer/core/layout/svg/line/svg_inline_text_box.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/scoped_svg_paint_state.h"
#include "third_party/blink/renderer/core/paint/svg_inline_text_box_painter.h"

namespace blink {

void SVGInlineFlowBoxPainter::PaintSelectionBackground(
    const PaintInfo& paint_info) {
  DCHECK(paint_info.phase == PaintPhase::kForeground ||
         paint_info.phase == PaintPhase::kSelectionDragImage);

  for (InlineBox* child = svg_inline_flow_box_.FirstChild(); child;
       child = child->NextOnLine()) {
    if (auto* svg_inline_text_box = DynamicTo<SVGInlineTextBox>(child)) {
      SVGInlineTextBoxPainter(*svg_inline_text_box)
          .PaintSelectionBackground(paint_info);
    } else if (auto* svg_inline_flow_box = DynamicTo<SVGInlineFlowBox>(child)) {
      SVGInlineFlowBoxPainter(*svg_inline_flow_box)
          .PaintSelectionBackground(paint_info);
    }
  }
}

void SVGInlineFlowBoxPainter::Paint(const PaintInfo& paint_info,
                                    const PhysicalOffset& paint_offset) {
  DCHECK(paint_info.phase == PaintPhase::kForeground ||
         paint_info.phase == PaintPhase::kSelectionDragImage);

  ScopedSVGPaintState paint_state(*LineLayoutAPIShim::ConstLayoutObjectFrom(
                                      svg_inline_flow_box_.GetLineLayoutItem()),
                                  paint_info, svg_inline_flow_box_);
  for (InlineBox* child = svg_inline_flow_box_.FirstChild(); child;
       child = child->NextOnLine())
    child->Paint(paint_info, paint_offset, LayoutUnit(), LayoutUnit());
}

}  // namespace blink
