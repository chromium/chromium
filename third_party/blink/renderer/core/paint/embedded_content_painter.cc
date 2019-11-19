// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/embedded_content_painter.h"

#include "base/optional.h"
#include "third_party/blink/renderer/core/frame/embedded_content_view.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/replaced_painter.h"
#include "third_party/blink/renderer/core/paint/scrollable_area_painter.h"
#include "third_party/blink/renderer/core/paint/selection_painting_utils.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_cache_skipper.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

void EmbeddedContentPainter::PaintReplaced(const PaintInfo& paint_info,
                                           const PhysicalOffset& paint_offset) {
  EmbeddedContentView* embedded_content_view =
      layout_embedded_content_.GetEmbeddedContentView();
  if (!embedded_content_view)
    return;

  IntPoint paint_location(RoundedIntPoint(
      paint_offset + layout_embedded_content_.ReplacedContentRect().offset));

  IntSize view_paint_offset =
      paint_location - embedded_content_view->FrameRect().Location();
  CullRect adjusted_cull_rect = paint_info.GetCullRect();
  adjusted_cull_rect.Move(-view_paint_offset);
  embedded_content_view->Paint(paint_info.context,
                               paint_info.GetGlobalPaintFlags(),
                               adjusted_cull_rect, view_paint_offset);
}

}  // namespace blink
