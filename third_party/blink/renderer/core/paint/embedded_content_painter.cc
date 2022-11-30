// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/embedded_content_painter.h"

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/frame/embedded_content_view.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/highlight_painting_utils.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/replaced_painter.h"
#include "third_party/blink/renderer/core/paint/scrollable_area_painter.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_cache_skipper.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

void EmbeddedContentPainter::PaintReplaced(const PaintInfo& paint_info,
                                           const PhysicalOffset& paint_offset) {
  EmbeddedContentView* embedded_content_view =
      layout_embedded_content_.GetEmbeddedContentView();
  if (!embedded_content_view)
    return;

  // Apply the translation to offset the content within the object's border-box
  // only if we're not using a transform node for this. If the frame size is
  // frozen then |ReplacedContentTransform| is used instead.
  gfx::Point paint_location;
  if (!layout_embedded_content_.FrozenFrameSize().has_value()) {
    paint_location = ToRoundedPoint(
        paint_offset + layout_embedded_content_.ReplacedContentRect().offset);
  }

  gfx::Vector2d view_paint_offset =
      paint_location - embedded_content_view->FrameRect().origin();
  CullRect adjusted_cull_rect = paint_info.GetCullRect();
  adjusted_cull_rect.Move(-view_paint_offset);
  embedded_content_view->Paint(paint_info.context, paint_info.GetPaintFlags(),
                               adjusted_cull_rect, view_paint_offset);
}

}  // namespace blink
