// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/embedded_content_painter.h"

#include <optional>

#include "cc/layers/view_transition_content_layer.h"
#include "third_party/blink/renderer/core/frame/embedded_content_view.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/replaced_painter.h"
#include "third_party/blink/renderer/core/paint/scrollable_area_painter.h"
#include "third_party/blink/renderer/core/view_transition/view_transition.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_cache_skipper.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"

namespace blink {

namespace {
scoped_refptr<cc::ViewTransitionContentLayer> GetSubframeSnapshotLayer(
    const EmbeddedContentView& embedded_content_view,
    PaintPhase phase) {
  if (phase != PaintPhase::kForeground) {
    return nullptr;
  }

  auto* local_frame_view = DynamicTo<LocalFrameView>(embedded_content_view);
  if (!local_frame_view) {
    return nullptr;
  }

  auto* transition = ViewTransitionUtils::GetTransition(
      *local_frame_view->GetFrame().GetDocument());
  if (!transition) {
    return nullptr;
  }

  return transition->GetSubframeSnapshotLayer();
}

}  // namespace

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

  // During a ViewTransition in a LocalFrame sub-frame, we need to keep painting
  // the old Document's last frame until the new Document is ready to start
  // rendering.
  //
  // Note: The iframe is throttled for the duration the new state is not ready
  // to display. This is true for both same-document transitions (the update
  // callback is running) and cross-document transitions (the new Document is
  // render-blocked).
  //
  // When the iframe is throttled, the embedded content view will not paint
  // anything but we still paint this foreign layer to keep displaying the old
  // content.
  if (auto layer =
          GetSubframeSnapshotLayer(*embedded_content_view, paint_info.phase)) {
    GraphicsContext& context = paint_info.context;
    layer->SetBounds(embedded_content_view->FrameRect().size());
    layer->SetIsDrawable(true);
    RecordForeignLayer(context, layout_embedded_content_,
                       DisplayItem::kForeignLayerViewTransitionContent,
                       std::move(layer), paint_location);
  }
}

}  // namespace blink
