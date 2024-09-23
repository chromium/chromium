// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"

namespace blink {

DrawingRecorder::DrawingRecorder(GraphicsContext& context,
                                 const DisplayItemClient& display_item_client,
                                 DisplayItem::Type display_item_type,
                                 const gfx::Rect& visual_rect)
    : context_(context),
      client_(display_item_client),
      type_(display_item_type),
      visual_rect_(visual_rect) {
  // Must check DrawingRecorder::UseCachedDrawingIfPossible before creating the
  // DrawingRecorder.
#if DCHECK_IS_ON()
  context_.GetPaintController().AssertLastCheckedCachedItem(client_, type_);
  DCHECK(DisplayItem::IsDrawingType(display_item_type));
#endif

  context.SetInDrawingRecorder(true);
  context.BeginRecording();

  if (context.NeedsDOMNodeId()) {
    DOMNodeId dom_node_id = display_item_client.OwnerNodeId();
    if (dom_node_id != kInvalidDOMNodeId) {
      dom_node_id_to_restore_ = context.GetDOMNodeId();
      context.SetDOMNodeId(dom_node_id);
    }
  }
}

DrawingRecorder::~DrawingRecorder() {
  if (dom_node_id_to_restore_)
    context_.SetDOMNodeId(dom_node_id_to_restore_.value());

  context_.SetInDrawingRecorder(false);

  context_.GetPaintController().CreateAndAppend<DrawingDisplayItem>(
      client_, type_, visual_rect_, context_.EndRecording(),
      client_.VisualRectOutsetForRasterEffects(),
      client_.GetPaintInvalidationReason());
}

}  // namespace blink
