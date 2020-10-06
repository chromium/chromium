// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/svg_text_painter.h"

#include "third_party/blink/renderer/core/layout/svg/layout_svg_text.h"
#include "third_party/blink/renderer/core/paint/block_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/scoped_svg_paint_state.h"
#include "third_party/blink/renderer/core/paint/svg_model_object_painter.h"

namespace blink {

void SVGTextPainter::Paint(const PaintInfo& paint_info) {
  if (paint_info.phase != PaintPhase::kForeground &&
      paint_info.phase != PaintPhase::kForcedColorsModeBackplate &&
      paint_info.phase != PaintPhase::kSelectionDragImage)
    return;

  PaintInfo block_info(paint_info);
  if (const auto* properties =
          layout_svg_text_.FirstFragment().PaintProperties()) {
    if (const auto* transform = properties->Transform())
      block_info.TransformCullRect(*transform);
  }
  ScopedSVGTransformState transform_state(block_info, layout_svg_text_);

  if (block_info.phase == PaintPhase::kForeground)
    SVGModelObjectPainter::RecordHitTestData(layout_svg_text_, block_info);

  BlockPainter(layout_svg_text_).Paint(block_info);

  // Paint the outlines, if any
  if (block_info.phase == PaintPhase::kForeground) {
    block_info.phase = PaintPhase::kOutline;
    BlockPainter(layout_svg_text_).Paint(block_info);
  }
}

}  // namespace blink
