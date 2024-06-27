// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/scoped_paint_state.h"

#include "third_party/blink/renderer/core/layout/layout_replaced.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/physical_fragment.h"
#include "third_party/blink/renderer/core/paint/box_model_object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"

namespace blink {

ScopedPaintState::ScopedPaintState(const LayoutObject& object,
                                   const PaintInfo& paint_info,
                                   const FragmentData* fragment_data)
    : fragment_to_paint_(fragment_data), input_paint_info_(paint_info) {
  if (!fragment_to_paint_) {
    // The object has nothing to paint in the current fragment.
    // TODO(wangxianzhu): Use DCHECK(fragment_to_paint_) in PaintOffset()
    // when all painters check FragmentToPaint() before painting.
    paint_offset_ =
        PhysicalOffset(LayoutUnit::NearlyMax(), LayoutUnit::NearlyMax());
    return;
  }

  paint_offset_ = fragment_to_paint_->PaintOffset();
  if (paint_info.phase == PaintPhase::kOverlayOverflowControls ||
      (object.HasLayer() &&
       To<LayoutBoxModelObject>(object).HasSelfPaintingLayer())) {
    // PaintLayerPainter already adjusted for PaintOffsetTranslation for
    // PaintContainer.
    return;
  }

  AdjustForPaintProperties(object);
}

void ScopedPaintState::AdjustForPaintProperties(const LayoutObject& object) {
  // Paint properties of SVG children are handled in SVG code paths.
  if (object.IsSVGChild())
    return;

  const auto* properties = fragment_to_paint_->PaintProperties();
  if (!properties)
    return;

  if (!object.Parent() && !object.HasLayer()) {
#if DCHECK_IS_ON()
    DCHECK(object.IsInDetachedNonDomTree());
    DCHECK(object.IsBox());
    DCHECK_EQ(To<LayoutBox>(object).GetPhysicalFragment(0)->GetBoxType(),
              PhysicalFragment::kPageBorderBox);
#endif

    // The page border box fragment paints @page borders and other decorations,
    // in addition to the document background (the one typically defined on the
    // BODY or HTML element). Therefore, this is in the coordinate system of the
    // document, which may have a different scale factor than the page
    // container, which is fitted to the paper size, if any.
    chunk_properties_.emplace(
        input_paint_info_.context.GetPaintController(),
        fragment_to_paint_->LocalBorderBoxProperties(), object,
        DisplayItem::PaintPhaseToDrawingType(input_paint_info_.phase));
    return;
  }

  auto new_chunk_properties = input_paint_info_.context.GetPaintController()
                                  .CurrentPaintChunkProperties();
  bool needs_new_chunk_properties = false;

  if (const auto* paint_offset_translation =
          properties->PaintOffsetTranslation()) {
    adjusted_paint_info_.emplace(input_paint_info_);
    adjusted_paint_info_->TransformCullRect(*paint_offset_translation);
    new_chunk_properties.SetTransform(*paint_offset_translation);
    needs_new_chunk_properties = true;

    if (input_paint_info_.context.InDrawingRecorder()) {
      // If we are recording drawings, we should issue the translation as a raw
      // paint operation instead of paint chunk properties. One case is that we
      // are painting table row background behind a cell having paint offset
      // translation.
      input_paint_info_.context.Save();
      gfx::Vector2dF translation = paint_offset_translation->Get2dTranslation();
      input_paint_info_.context.Translate(translation.x(), translation.y());
      paint_offset_translation_as_drawing_ = true;
    }
  }

  if (input_paint_info_.context.InDrawingRecorder())
    return;

  if (const auto* transform = properties->Transform()) {
    // This transform node stores some transform-related information for a
    // non-stacked object without real transform (otherwise PaintLayerPainter
    // should have handled the transform node for painting).
    DCHECK(transform->IsIdentity());
    new_chunk_properties.SetTransform(*transform);
    needs_new_chunk_properties = true;
  }
  DCHECK(!properties->Translate());
  DCHECK(!properties->Rotate());
  DCHECK(!properties->Scale());
  DCHECK(!properties->Offset());
  if (const auto* effect = properties->Effect()) {
    // Similar to the above.
    DCHECK(!effect->HasRealEffects());
    new_chunk_properties.SetEffect(*effect);
    needs_new_chunk_properties = true;
  }

  if (needs_new_chunk_properties) {
    chunk_properties_.emplace(
        input_paint_info_.context.GetPaintController(), new_chunk_properties,
        object, DisplayItem::PaintPhaseToDrawingType(input_paint_info_.phase));
  }
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

  if (const auto* properties = fragment_to_paint_->PaintProperties()) {
    // See comments for ScrollTranslation in object_paint_properties.h
    // for the reason of adding ScrollOrigin(). The paint offset will
    // be used only for the scrolling contents that are not painted through
    // descendant objects' Paint() method, e.g. inline boxes.
    if (properties->ScrollTranslation())
      paint_offset_ += PhysicalOffset(box.ScrollOrigin());
  }

  // We calculated cull rects for PaintLayers only.
  if (!box.HasLayer())
    return;
  adjusted_paint_info_.emplace(input_paint_info_);
  adjusted_paint_info_->SetCullRect(fragment_to_paint_->GetContentsCullRect());
  if (box.Layer()->PreviousPaintResult() == kFullyPainted) {
    PhysicalRect contents_visual_rect =
        PaintLayerPainter::ContentsVisualRect(*fragment_to_paint_, box);
    if (!PhysicalRect(fragment_to_paint_->GetContentsCullRect().Rect())
             .Contains(contents_visual_rect)) {
      box.Layer()->SetPreviousPaintResult(kMayBeClippedByCullRect);
    }
  }

  if (input_paint_info_.phase == PaintPhase::kForeground) {
    // We treat horizontal-scrollable scrollers like replaced objects.
    if (auto* mf_checker = MobileFriendlinessChecker::From(box.GetDocument())) {
      if (!box.IsLayoutView()) {
        if (auto* scrollable_area = box.GetScrollableArea()) {
          if (scrollable_area->MaximumScrollOffset().x() != 0) {
            PhysicalRect content_rect = box.OverflowClipRect(paint_offset_);
            content_rect.Intersect(
                PhysicalRect(input_paint_info_.GetCullRect().Rect()));
            mf_checker->NotifyPaintReplaced(
                content_rect, input_paint_info_.context.GetPaintController()
                                  .CurrentPaintChunkProperties()
                                  .Transform());
            mf_ignore_scope_.emplace(*mf_checker);
          }
        }
        // Don't check mobile friendliness for beyond viewport in position:fixed
        // boxes because they don't scroll in the viewport.
        if (const auto* properties = fragment_to_paint_->PaintProperties()) {
          if (const auto* translation = properties->PaintOffsetTranslation()) {
            if (translation->ScrollTranslationForFixed())
              mf_ignore_scope_.emplace(*mf_checker);
          }
        }
      }
    }
  }
}

}  // namespace blink
