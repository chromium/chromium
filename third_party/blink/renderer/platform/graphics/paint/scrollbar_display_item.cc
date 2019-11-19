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
    const IntRect& rect,
    const TransformPaintPropertyNode* scroll_translation,
    CompositorElementId element_id)
    : DisplayItem(client, type, sizeof(*this), /*draws_content*/ true),
      scrollbar_(std::move(scrollbar)),
      rect_(rect),
      scroll_translation_(scroll_translation),
      element_id_(element_id) {
  DCHECK(IsScrollbar());
  DCHECK(!scroll_translation || scroll_translation->ScrollNode());
}

sk_sp<const PaintRecord> ScrollbarDisplayItem::Paint() const {
  if (record_) {
    DCHECK(!scrollbar_->NeedsRepaintPart(cc::TRACK_BUTTONS_TICKMARKS));
    DCHECK(!scrollbar_->NeedsRepaintPart(cc::THUMB));
    return record_;
  }

  PaintRecorder recorder;
  recorder.beginRecording(rect_);
  auto* canvas = recorder.getRecordingCanvas();
  scrollbar_->PaintPart(canvas, cc::TRACK_BUTTONS_TICKMARKS, rect_);
  gfx::Rect thumb_rect = scrollbar_->ThumbRect();
  thumb_rect.Offset(rect_.X(), rect_.Y());
  scrollbar_->PaintPart(canvas, cc::THUMB, thumb_rect);

  record_ = recorder.finishRecordingAsPicture();
  return record_;
}

scoped_refptr<cc::Layer> ScrollbarDisplayItem::CreateLayer() const {
  scoped_refptr<cc::ScrollbarLayerBase> layer;
  if (scrollbar_->IsSolidColor()) {
    DCHECK(scrollbar_->IsOverlay());
    bool is_horizontal = scrollbar_->Orientation() == cc::HORIZONTAL;
    gfx::Rect thumb_rect = scrollbar_->ThumbRect();
    int thumb_thickness =
        is_horizontal ? thumb_rect.height() : thumb_rect.width();
    gfx::Rect track_rect = scrollbar_->TrackRect();
    int track_start = is_horizontal ? track_rect.x() : track_rect.y();
    layer = cc::SolidColorScrollbarLayer::Create(
        scrollbar_->Orientation(), thumb_thickness, track_start,
        scrollbar_->IsLeftSideVerticalScrollbar());
  } else if (scrollbar_->UsesNinePatchThumbResource()) {
    DCHECK(scrollbar_->IsOverlay());
    layer = cc::PaintedOverlayScrollbarLayer::Create(scrollbar_);
  } else {
    layer = cc::PaintedScrollbarLayer::Create(scrollbar_);
  }

  layer->SetIsDrawable(true);
  layer->SetElementId(element_id_);
  if (scroll_translation_) {
    layer->SetScrollElementId(
        scroll_translation_->ScrollNode()->GetCompositorElementId());
  }
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
  return rect_ == other_scrollbar_item.rect_ &&
         scroll_translation_ == other_scrollbar_item.scroll_translation_ &&
         element_id_ == other_scrollbar_item.element_id_;
}

#if DCHECK_IS_ON()
void ScrollbarDisplayItem::PropertiesAsJSON(JSONObject& json) const {
  DisplayItem::PropertiesAsJSON(json);
  json.SetString("rect", rect_.ToString());
  json.SetString("scrollTranslation",
                 String::Format("%p", scroll_translation_));
}
#endif

void ScrollbarDisplayItem::Record(
    GraphicsContext& context,
    const DisplayItemClient& client,
    DisplayItem::Type type,
    scoped_refptr<cc::Scrollbar> scrollbar,
    const IntRect& rect,
    const TransformPaintPropertyNode* scroll_translation,
    CompositorElementId element_id) {
  PaintController& paint_controller = context.GetPaintController();
  if (paint_controller.DisplayItemConstructionIsDisabled())
    return;

  // Must check PaintController::UseCachedItemIfPossible before this function.
  DCHECK(RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() ||
         !paint_controller.UseCachedItemIfPossible(client, type));

  paint_controller.CreateAndAppend<ScrollbarDisplayItem>(
      client, type, std::move(scrollbar), rect, scroll_translation, element_id);
}

}  // namespace blink
