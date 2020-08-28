// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/scrollbar_display_item.h"

#include "base/trace_event/traced_value.h"
#include "cc/input/scrollbar.h"
#include "cc/layers/painted_overlay_scrollbar_layer.h"
#include "cc/layers/painted_scrollbar_layer.h"
#include "cc/layers/solid_color_scrollbar_layer.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_recorder.h"

namespace blink {

ScrollbarDisplayItem::ScrollbarDisplayItem(
    const DisplayItemClient& client,
    Type type,
    scoped_refptr<cc::Scrollbar> scrollbar,
    const IntRect& visual_rect,
    const TransformPaintPropertyNode* scroll_translation,
    CompositorElementId element_id)
    : DisplayItem(client,
                  type,
                  sizeof(*this),
                  visual_rect,
                  /*draws_content*/ true),
      scrollbar_(std::move(scrollbar)),
      scroll_translation_(scroll_translation),
      element_id_(element_id) {
  DCHECK(IsScrollbar());
  DCHECK(!scroll_translation || scroll_translation->ScrollNode());
}

sk_sp<const PaintRecord> ScrollbarDisplayItem::Paint() const {
  if (record_) {
    DCHECK(!scrollbar_->NeedsRepaintPart(
        cc::ScrollbarPart::TRACK_BUTTONS_TICKMARKS));
    DCHECK(!scrollbar_->NeedsRepaintPart(cc::ScrollbarPart::THUMB));
    return record_;
  }

  PaintRecorder recorder;
  const IntRect& rect = VisualRect();
  recorder.beginRecording(rect);
  auto* canvas = recorder.getRecordingCanvas();
  scrollbar_->PaintPart(canvas, cc::ScrollbarPart::TRACK_BUTTONS_TICKMARKS,
                        rect);
  gfx::Rect thumb_rect = scrollbar_->ThumbRect();
  thumb_rect.Offset(rect.X(), rect.Y());
  scrollbar_->PaintPart(canvas, cc::ScrollbarPart::THUMB, thumb_rect);

  record_ = recorder.finishRecordingAsPicture();
  return record_;
}

scoped_refptr<cc::ScrollbarLayerBase> ScrollbarDisplayItem::CreateOrReuseLayer(
    cc::ScrollbarLayerBase* existing_layer) const {
  // This function is called when the scrollbar is composited. We don't need
  // record_ which is for non-composited scrollbars.
  record_ = nullptr;

  auto layer =
      cc::ScrollbarLayerBase::CreateOrReuse(scrollbar_, existing_layer);
  layer->SetIsDrawable(true);
  if (!scrollbar_->IsSolidColor())
    layer->SetHitTestable(true);
  layer->SetElementId(element_id_);
  layer->SetScrollElementId(
      scroll_translation_
          ? scroll_translation_->ScrollNode()->GetCompositorElementId()
          : CompositorElementId());
  layer->SetOffsetToTransformParent(
      gfx::Vector2dF(FloatPoint(VisualRect().Location())));
  layer->SetBounds(gfx::Size(VisualRect().Size()));

  if (scrollbar_->NeedsRepaintPart(cc::ScrollbarPart::THUMB) ||
      scrollbar_->NeedsRepaintPart(cc::ScrollbarPart::TRACK_BUTTONS_TICKMARKS))
    layer->SetNeedsDisplay();
  return layer;
}

bool ScrollbarDisplayItem::Equals(const DisplayItem& other) const {
  if (!DisplayItem::Equals(other))
    return false;

  // Don't check scrollbar_ because it's always newly created when we repaint
  // a scrollbar (including forced repaint for PaintUnderInvalidationChecking).
  // Don't check record_ because it's lazily created, and the DCHECKs in Paint()
  // can catch most under-invalidation cases.
  const auto& other_scrollbar_item =
      static_cast<const ScrollbarDisplayItem&>(other);
  return scroll_translation_ == other_scrollbar_item.scroll_translation_ &&
         element_id_ == other_scrollbar_item.element_id_;
}

#if DCHECK_IS_ON()
void ScrollbarDisplayItem::PropertiesAsJSON(JSONObject& json) const {
  DisplayItem::PropertiesAsJSON(json);
  json.SetString("scrollTranslation",
                 String::Format("%p", scroll_translation_));
}
#endif

void ScrollbarDisplayItem::Record(
    GraphicsContext& context,
    const DisplayItemClient& client,
    DisplayItem::Type type,
    scoped_refptr<cc::Scrollbar> scrollbar,
    const IntRect& visual_rect,
    const TransformPaintPropertyNode* scroll_translation,
    CompositorElementId element_id) {
  PaintController& paint_controller = context.GetPaintController();
  // Must check PaintController::UseCachedItemIfPossible before this function.
  DCHECK(RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() ||
         !paint_controller.UseCachedItemIfPossible(client, type));

  paint_controller.CreateAndAppend<ScrollbarDisplayItem>(
      client, type, std::move(scrollbar), visual_rect, scroll_translation,
      element_id);
}

}  // namespace blink
