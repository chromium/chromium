// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/scoped_paint_state.h"

#include "third_party/blink/renderer/core/layout/layout_replaced.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/box_model_object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"

namespace blink {

void ScopedPaintState::AdjustForPaintOffsetTranslation(
    const LayoutObject& object,
    const TransformPaintPropertyNode& paint_offset_translation) {
  if (input_paint_info_.context.InDrawingRecorder()) {
    // If we are recording drawings, we should issue the translation as a raw
    // paint operation instead of paint chunk properties. One case is that we
    // are painting table row background behind a cell having paint offset
    // translation.
    input_paint_info_.context.Save();
    FloatSize translation = paint_offset_translation.Translation2D();
    input_paint_info_.context.Translate(translation.Width(),
                                        translation.Height());
    paint_offset_translation_as_drawing_ = true;
  } else {
    chunk_properties_.emplace(
        input_paint_info_.context.GetPaintController(),
        paint_offset_translation, object,
        DisplayItem::PaintPhaseToDrawingType(input_paint_info_.phase));
  }

  adjusted_paint_info_.emplace(input_paint_info_);
  adjusted_paint_info_->TransformCullRect(paint_offset_translation);
}

void ScopedPaintState::FinishPaintOffsetTranslationAsDrawing() {
  // This scope should not interlace with scopes of DrawingRecorders.
  DCHECK(paint_offset_translation_as_drawing_);
  DCHECK(input_paint_info_.context.InDrawingRecorder());
  input_paint_info_.context.Restore();
}

void ScopedBoxContentsPaintState::AdjustForBoxContents(const LayoutBox& box) {
  DCHECK(input_paint_info_.phase != PaintPhase::kSelfOutlineOnly &&
         input_paint_info_.phase != PaintPhase::kMask);

  if (!fragment_to_paint_ || !fragment_to_paint_->HasLocalBorderBoxProperties())
    return;

  DCHECK_EQ(paint_offset_, fragment_to_paint_->PaintOffset());

  chunk_properties_.emplace(input_paint_info_.context.GetPaintController(),
                            fragment_to_paint_->ContentsProperties(), box,
                            input_paint_info_.DisplayItemTypeForClipping());

  const auto* properties = fragment_to_paint_->PaintProperties();
  const auto* scroll_translation =
      properties ? properties->ScrollTranslation() : nullptr;

  // See comments for ScrollTranslation in object_paint_properties.h
  // for the reason of adding ScrollOrigin(). The paint offset will
  // be used only for the scrolling contents that are not painted through
  // descendant objects' Paint() method, e.g. inline boxes.
  if (scroll_translation)
    paint_offset_ += PhysicalOffset(box.ScrollOrigin());

  if (RuntimeEnabledFeatures::CullRectUpdateEnabled()) {
    // We calculated cull rects for PaintLayers only.
    if (!box.HasLayer())
      return;
    adjusted_paint_info_.emplace(input_paint_info_);
    adjusted_paint_info_->SetCullRect(
        fragment_to_paint_->GetContentsCullRect());
    if (box.Layer()->PreviousPaintResult() == kFullyPainted) {
      PhysicalRect contents_visual_rect =
          box.PhysicalContentsVisualOverflowRect();
      contents_visual_rect.Move(fragment_to_paint_->PaintOffset());
      if (!PhysicalRect(fragment_to_paint_->GetContentsCullRect().Rect())
               .Contains(contents_visual_rect)) {
        box.Layer()->SetPreviousPaintResult(kMayBeClippedByCullRect);
      }
    }
    return;
  }

  // If a LayoutView is using infinite cull rect, we are painting with viewport
  // clip disabled, so don't cull the scrolling contents. This is just for
  // completeness because we always paint the whole scrolling background even
  // with a smaller cull rect, and the scrolling document contents are under the
  // layer of document element which will use infinite cull rect calculated in
  // PaintLayerPainter::AdjustForPaintProperties().
  if (IsA<LayoutView>(box) && input_paint_info_.GetCullRect().IsInfinite())
    return;

  if (scroll_translation) {
    adjusted_paint_info_.emplace(input_paint_info_);
    adjusted_paint_info_->TransformCullRect(*scroll_translation);
  }
}

}  // namespace blink
