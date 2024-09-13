// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/frame_painter.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"
#include "third_party/blink/renderer/core/paint/timing/frame_paint_timing.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_display_item_fragment.h"

namespace blink {

namespace {

gfx::QuadF GetQuadForTraceEvent(const LocalFrameView& frame_view,
                                const CullRect& cull_rect) {
  gfx::QuadF quad(gfx::RectF(cull_rect.Rect()));
  if (auto* owner = frame_view.GetFrame().OwnerLayoutObject()) {
    quad += gfx::Vector2dF(owner->PhysicalContentBoxOffset());
    owner->LocalToAbsoluteQuad(quad, kTraverseDocumentBoundaries);
  }
  return quad;
}

}  // namespace

bool FramePainter::in_paint_contents_ = false;

void FramePainter::Paint(GraphicsContext& context, PaintFlags paint_flags) {
  Document* document = GetFrameView().GetFrame().GetDocument();

  if (GetFrameView().ShouldThrottleRendering() || !document->IsActive())
    return;

  GetFrameView().NotifyPageThatContentAreaWillPaint();
  ENTER_EMBEDDER_STATE(document->GetAgent().isolate(),
                       &GetFrameView().GetFrame(), BlinkState::PAINT);
  LayoutView* layout_view = GetFrameView().GetLayoutView();
  if (!layout_view) {
    DLOG(ERROR) << "called FramePainter::paint with nil layoutObject";
    return;
  }

  // TODO(crbug.com/590856): It's still broken when we choose not to crash when
  // the check fails.
  if (!GetFrameView().CheckDoesNotNeedLayout())
    return;

  // TODO(pdr): The following should check that the lifecycle is
  // DocumentLifecycle::kInPaint but drag images currently violate this.
  DCHECK_GE(document->Lifecycle().GetState(),
            DocumentLifecycle::kPrePaintClean);

  FramePaintTiming frame_paint_timing(context, &GetFrameView().GetFrame());

  DEVTOOLS_TIMELINE_TRACE_EVENT_WITH_CATEGORIES(
      "devtools.timeline,rail", "Paint", inspector_paint_event::Data,
      &GetFrameView().GetFrame(), layout_view,
      GetQuadForTraceEvent(GetFrameView(),
                           layout_view->FirstFragment().GetCullRect()),
      /*layer_id=*/0);

  bool is_top_level_painter = !in_paint_contents_;
  in_paint_contents_ = true;

  FontCachePurgePreventer font_cache_purge_preventer;
  ScopedDisplayItemFragment display_item_fragment(context, 0u);

  PaintLayer* root_layer = layout_view->Layer();

#if DCHECK_IS_ON()
  layout_view->AssertSubtreeIsLaidOut();
  LayoutObject::SetLayoutNeededForbiddenScope forbid_set_needs_layout(
      root_layer->GetLayoutObject());
#endif

  PaintLayerPainter layer_painter(*root_layer);

  layer_painter.Paint(context, paint_flags);

  // Regions may have changed as a result of the visibility/z-index of element
  // changing.
  if (document->DraggableRegionsDirty()) {
    GetFrameView().UpdateDocumentDraggableRegions();
  }

  if (is_top_level_painter) {
    // Everything that happens after paintContents completions is considered
    // to be part of the next frame.
    in_paint_contents_ = false;
  }
}

const LocalFrameView& FramePainter::GetFrameView() {
  DCHECK(frame_view_);
  return *frame_view_;
}

}  // namespace blink
