// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/scrollbar_display_item.h"

#include "base/trace_event/traced_value.h"
#include "cc/input/scrollbar.h"
#include "cc/layers/scrollbar_layer_base.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

ScrollbarDisplayItem::ScrollbarDisplayItem(
    DisplayItemClientId client_id,
    Type type,
    scoped_refptr<cc::Scrollbar> scrollbar,
    const gfx::Rect& visual_rect,
    const TransformPaintPropertyNode* scroll_translation,
    CompositorElementId element_id,
    cc::HitTestOpaqueness hit_test_opaqueness,
    RasterEffectOutset outset,
    PaintInvalidationReason paint_invalidation_reason)
    : DisplayItem(client_id,
                  type,
                  visual_rect,
                  outset,
                  paint_invalidation_reason,
                  /*draws_content*/ true),
      data_(new Data{std::move(scrollbar), scroll_translation, element_id,
                     hit_test_opaqueness}) {
  DCHECK(IsScrollbar());
  CHECK(!data_->scroll_translation_ ||
        data_->scroll_translation_->ScrollNode());
}

PaintRecord ScrollbarDisplayItem::Paint() const {
  DCHECK(!IsTombstone());
  if (!data_->record_.empty() && !NeedsUpdateDisplay()) {
    return data_->record_;
  }

  PaintRecorder recorder;
  const gfx::Rect& rect = VisualRect();
  recorder.beginRecording();
  auto* canvas = recorder.getRecordingCanvas();
  auto* scrollbar = data_->scrollbar_.get();

  // Skip track and button painting for Minimal mode Fluent scrollbars.
  if (!scrollbar->IsFluentOverlayScrollbarMinimalMode()) {
    scrollbar->PaintTrackAndButtons(*canvas, rect);
  }

  gfx::Rect thumb_rect = scrollbar->ThumbRect();
  thumb_rect.Offset(rect.OffsetFromOrigin());
  if (scrollbar->IsFluentOverlayScrollbarMinimalMode()) {
    thumb_rect = scrollbar->ShrinkMainThreadedMinimalModeThumbRect(thumb_rect);
  }
  scrollbar->PaintThumb(*canvas, thumb_rect);

  scrollbar->ClearNeedsUpdateDisplay();
  data_->record_ = recorder.finishRecordingAsPicture();
  return data_->record_;
}

bool ScrollbarDisplayItem::NeedsUpdateDisplay() const {
  return data_->scrollbar_->NeedsUpdateDisplay();
}

scoped_refptr<cc::ScrollbarLayerBase> ScrollbarDisplayItem::CreateOrReuseLayer(
    cc::ScrollbarLayerBase* existing_layer,
    gfx::Vector2dF offset_of_decomposited_transforms) const {
  DCHECK(!IsTombstone());
  // This function is called when the scrollbar is composited. We don't need
  // record_ which is for non-composited scrollbars.
  data_->record_ = PaintRecord();

  auto* scrollbar = data_->scrollbar_.get();
  auto layer = cc::ScrollbarLayerBase::CreateOrReuse(scrollbar, existing_layer);
  layer->SetIsDrawable(true);
  layer->SetContentsOpaque(IsOpaque());
  layer->SetHitTestOpaqueness(data_->hit_test_opaqueness_);
  layer->SetElementId(data_->element_id_);
  layer->SetScrollElementId(
      data_->scroll_translation_
          ? data_->scroll_translation_->ScrollNode()->GetCompositorElementId()
          : CompositorElementId());
  layer->SetOffsetToTransformParent(
      gfx::Vector2dF(VisualRect().OffsetFromOrigin()) +
      offset_of_decomposited_transforms);
  layer->SetBounds(VisualRect().size());

  // TODO(crbug.com/1414885): This may be duplicate with
  // ScrollableArea::ScrollableArea::SetScrollbarNeedsPaintInvalidation()
  // which calls PaintArtifactCompositor::SetScrollbarNeedsDisplay().
  if (NeedsUpdateDisplay()) {
    layer->SetNeedsDisplay();
    scrollbar->ClearNeedsUpdateDisplay();
  }
  return layer;
}

bool ScrollbarDisplayItem::IsOpaque() const {
  DCHECK(!IsTombstone());

  return data_->scrollbar_->IsOpaque();
}

bool ScrollbarDisplayItem::EqualsForUnderInvalidationImpl(
    const ScrollbarDisplayItem& other) const {
  DCHECK(RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled());
  // Don't check scrollbar_ because it's always newly created when we repaint
  // a scrollbar (including forced repaint for PaintUnderInvalidationChecking).
  // Don't check record_ because it's lazily created, and the DCHECKs in Paint()
  // can catch most under-invalidation cases.
  return data_->scroll_translation_ == other.data_->scroll_translation_ &&
         data_->element_id_ == other.data_->element_id_;
}

#if DCHECK_IS_ON()
void ScrollbarDisplayItem::PropertiesAsJSONImpl(JSONObject& json) const {
  json.SetString("scrollTranslation",
                 String::Format("%p", data_->scroll_translation_.Get()));
}
#endif

void ScrollbarDisplayItem::Record(
    GraphicsContext& context,
    const DisplayItemClient& client,
    DisplayItem::Type type,
    scoped_refptr<cc::Scrollbar> scrollbar,
    const gfx::Rect& visual_rect,
    const TransformPaintPropertyNode* scroll_translation,
    CompositorElementId element_id,
    cc::HitTestOpaqueness hit_test_opaqueness) {
  PaintController& paint_controller = context.GetPaintController();
  // Must check PaintController::UseCachedItemIfPossible before this function.
  DCHECK(RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() ||
         !paint_controller.UseCachedItemIfPossible(client, type));
  CHECK(IsScrollbarElementId(element_id));

  paint_controller.CreateAndAppend<ScrollbarDisplayItem>(
      client, type, std::move(scrollbar), visual_rect, scroll_translation,
      element_id, hit_test_opaqueness,
      client.VisualRectOutsetForRasterEffects(),
      client.GetPaintInvalidationReason());
}

}  // namespace blink
