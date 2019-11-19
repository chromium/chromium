// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/frame_painter.h"

#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/frame_paint_timing.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"

namespace blink {

bool FramePainter::in_paint_contents_ = false;

void FramePainter::Paint(GraphicsContext& context,
                         const GlobalPaintFlags global_paint_flags,
                         const CullRect& cull_rect) {
  if (GetFrameView().ShouldThrottleRendering())
    return;

  GetFrameView().NotifyPageThatContentAreaWillPaint();

  CullRect document_cull_rect(
      Intersection(cull_rect.Rect(), GetFrameView().FrameRect()));
  document_cull_rect.MoveBy(-GetFrameView().Location());

  if (document_cull_rect.Rect().IsEmpty())
    return;

  PaintContents(context, global_paint_flags, document_cull_rect);
}

void FramePainter::PaintContents(GraphicsContext& context,
                                 const GlobalPaintFlags global_paint_flags,
                                 const CullRect& cull_rect) {
  Document* document = GetFrameView().GetFrame().GetDocument();

  if (GetFrameView().ShouldThrottleRendering() || !document->IsActive())
    return;

  LayoutView* layout_view = GetFrameView().GetLayoutView();
  if (!layout_view) {
    DLOG(ERROR) << "called FramePainter::paint with nil layoutObject";
    return;
  }

  // TODO(crbug.com/590856): It's still broken when we choose not to crash when
  // the check fails.
  if (!GetFrameView().CheckDoesNotNeedLayout())
    return;

  // TODO(wangxianzhu): The following check should be stricter, but currently
  // this is blocked by the svg root issue (crbug.com/442939).
  DCHECK(document->Lifecycle().GetState() >=
         DocumentLifecycle::kCompositingClean);

  FramePaintTiming frame_paint_timing(context, &GetFrameView().GetFrame());
  TRACE_EVENT1("devtools.timeline,rail", "Paint", "data",
               inspector_paint_event::Data(
                   layout_view, PhysicalRect(cull_rect.Rect()), nullptr));

  bool is_top_level_painter = !in_paint_contents_;
  in_paint_contents_ = true;

  FontCachePurgePreventer font_cache_purge_preventer;

  PaintLayerFlags root_layer_paint_flags = 0;
  // This will prevent clipping the root PaintLayer to its visible content
  // rect when root layer scrolling is enabled.
  if (document->IsCapturingLayout())
    root_layer_paint_flags = kPaintLayerPaintingOverflowContents;

  PaintLayer* root_layer = layout_view->Layer();

#if DCHECK_IS_ON()
  layout_view->AssertSubtreeIsLaidOut();
  LayoutObject::SetLayoutNeededForbiddenScope forbid_set_needs_layout(
      root_layer->GetLayoutObject());
#endif

  PaintLayerPainter layer_painter(*root_layer);

  float device_scale_factor = blink::DeviceScaleFactorDeprecated(
      root_layer->GetLayoutObject().GetFrame());
  context.SetDeviceScaleFactor(device_scale_factor);

  layer_painter.Paint(context, cull_rect, global_paint_flags,
                      root_layer_paint_flags);

  // Regions may have changed as a result of the visibility/z-index of element
  // changing.
  if (document->AnnotatedRegionsDirty())
    GetFrameView().UpdateDocumentAnnotatedRegions();

  if (is_top_level_painter) {
    // Everything that happens after paintContents completions is considered
    // to be part of the next frame.
    GetMemoryCache()->UpdateFramePaintTimestamp();
    in_paint_contents_ = false;
  }
}

const LocalFrameView& FramePainter::GetFrameView() {
  DCHECK(frame_view_);
  return *frame_view_;
}

}  // namespace blink
