// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/scoped_paint_state.h"

#include "third_party/blink/renderer/core/layout/layout_replaced.h"
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

  // Then adjust paint offset and cull rect for scroll translation.
  const auto* properties = fragment_to_paint_->PaintProperties();
  if (!properties)
    return;
  const auto* scroll_translation = properties->ScrollTranslation();
  if (!scroll_translation)
    return;

  // See comments for ScrollTranslation in object_paint_properties.h
  // for the reason of adding ScrollOrigin(). contents_paint_offset will
  // be used only for the scrolling contents that are not painted through
  // descendant objects' Paint() method, e.g. inline boxes.
  paint_offset_ += PhysicalOffset(box.ScrollOrigin());

  // If a LayoutView is using infinite cull rect, we are painting with viewport
  // clip disabled, so don't cull the scrolling contents. This is just for
  // completeness because we always paint the whole scrolling background even
  // with a smaller cull rect, and the scrolling document contents are under the
  // layer of document element which will use infinite cull rect calculated in
  // PaintLayerPainter::AdjustForPaintProperties().
  if (box.IsLayoutView() && input_paint_info_.GetCullRect().IsInfinite())
    return;

  adjusted_paint_info_.emplace(input_paint_info_);
  adjusted_paint_info_->TransformCullRect(*scroll_translation);
}

}  // namespace blink
