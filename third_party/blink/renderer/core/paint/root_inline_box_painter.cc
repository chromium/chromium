// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/root_inline_box_painter.h"

#include "third_party/blink/renderer/core/layout/api/line_layout_api_shim.h"
#include "third_party/blink/renderer/core/layout/line/ellipsis_box.h"
#include "third_party/blink/renderer/core/layout/line/root_inline_box.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"

namespace blink {

void RootInlineBoxPainter::PaintEllipsisBox(const PaintInfo& paint_info,
                                            const PhysicalOffset& paint_offset,
                                            LayoutUnit line_top,
                                            LayoutUnit line_bottom) const {
  if (root_inline_box_.HasEllipsisBox() &&
      root_inline_box_.GetLineLayoutItem().StyleRef().Visibility() ==
          EVisibility::kVisible &&
      paint_info.phase == PaintPhase::kForeground)
    root_inline_box_.GetEllipsisBox()->Paint(paint_info, paint_offset, line_top,
                                             line_bottom);
}

void RootInlineBoxPainter::Paint(const PaintInfo& paint_info,
                                 const PhysicalOffset& paint_offset,
                                 LayoutUnit line_top,
                                 LayoutUnit line_bottom) {
  root_inline_box_.InlineFlowBox::Paint(paint_info, paint_offset, line_top,
                                        line_bottom);
  PaintEllipsisBox(paint_info, paint_offset, line_top, line_bottom);
}

}  // namespace blink
