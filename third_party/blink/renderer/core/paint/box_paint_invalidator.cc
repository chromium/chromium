// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/box_paint_invalidator.h"

#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/object_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"

namespace blink {

bool BoxPaintInvalidator::HasEffectiveBackground() {
  // The view can paint background not from the style.
  if (box_.IsLayoutView())
    return true;
  return box_.StyleRef().HasBackground() && !box_.BackgroundTransfersToView();
}

// |width| is of the positioning area.
static bool ShouldFullyInvalidateFillLayersOnWidthChange(
    const FillLayer& layer) {
  // Nobody will use multiple layers without wanting fancy positioning.
  if (layer.Next())
    return true;

  // The layer properties checked below apply only when there is a valid image.
  const StyleImage* image = layer.GetImage();
  if (!image || !image->CanRender())
    return false;

  if (layer.RepeatX() != EFillRepeat::kRepeatFill &&
      layer.RepeatX() != EFillRepeat::kNoRepeatFill)
    return true;

  // TODO(alancutter): Make this work correctly for calc lengths.
  if (layer.PositionX().IsPercentOrCalc() && !layer.PositionX().IsZero())
    return true;

  if (layer.BackgroundXOrigin() != BackgroundEdgeOrigin::kLeft)
    return true;

  EFillSizeType size_type = layer.SizeType();

  if (size_type == EFillSizeType::kContain ||
      size_type == EFillSizeType::kCover)
    return true;

  DCHECK_EQ(size_type, EFillSizeType::kSizeLength);

  // TODO(alancutter): Make this work correctly for calc lengths.
  const Length& width = layer.SizeLength().Width();
  if (width.IsPercentOrCalc() && !width.IsZero())
    return true;

  if (width.IsAuto() && !image->HasIntrinsicSize())
    return true;

  return false;
}

// |height| is of the positioning area.
static bool ShouldFullyInvalidateFillLayersOnHeightChange(
    const FillLayer& layer) {
  // Nobody will use multiple layers without wanting fancy positioning.
  if (layer.Next())
    return true;

  // The layer properties checked below apply only when there is a valid image.
  const StyleImage* image = layer.GetImage();
  if (!image || !image->CanRender())
    return false;

  if (layer.RepeatY() != EFillRepeat::kRepeatFill &&
      layer.RepeatY() != EFillRepeat::kNoRepeatFill)
    return true;

  // TODO(alancutter): Make this work correctly for calc lengths.
  if (layer.PositionY().IsPercentOrCalc() && !layer.PositionY().IsZero())
    return true;

  if (layer.BackgroundYOrigin() != BackgroundEdgeOrigin::kTop)
    return true;

  EFillSizeType size_type = layer.SizeType();

  if (size_type == EFillSizeType::kContain ||
      size_type == EFillSizeType::kCover)
    return true;

  DCHECK_EQ(size_type, EFillSizeType::kSizeLength);

  // TODO(alancutter): Make this work correctly for calc lengths.
  const Length& height = layer.SizeLength().Height();
  if (height.IsPercentOrCalc() && !height.IsZero())
    return true;

  if (height.IsAuto() && !image->HasIntrinsicSize())
    return true;

  return false;
}

// old_size and new_size are the old and new sizes of the positioning area.
bool ShouldFullyInvalidateFillLayersOnSizeChange(const FillLayer& layer,
                                                 const PhysicalSize& old_size,
                                                 const PhysicalSize& new_size) {
  return (old_size.width != new_size.width &&
          ShouldFullyInvalidateFillLayersOnWidthChange(layer)) ||
         (old_size.height != new_size.height &&
          ShouldFullyInvalidateFillLayersOnHeightChange(layer));
}

PaintInvalidationReason BoxPaintInvalidator::ComputePaintInvalidationReason() {
  PaintInvalidationReason reason =
      ObjectPaintInvalidatorWithContext(box_, context_)
          .ComputePaintInvalidationReason();

  if (reason != PaintInvalidationReason::kIncremental)
    return reason;

  const ComputedStyle& style = box_.StyleRef();

  if (style.MaskLayers().AnyLayerUsesContentBox() &&
      box_.PreviousPhysicalContentBoxRect() != box_.PhysicalContentBoxRect())
    return PaintInvalidationReason::kGeometry;

  if (box_.PreviousSize() == box_.Size() &&
      context_.old_visual_rect == context_.fragment_data->VisualRect())
    return PaintInvalidationReason::kNone;

  // If either border box changed or bounds changed, and old or new border box
  // doesn't equal old or new bounds, incremental invalidation is not
  // applicable. This captures the following cases:
  // - pixel snapping, or not snapping e.g. for some visual overflowing effects,
  // - scale, rotate, skew etc. transforms,
  // - visual (ink) overflows.
  if (PhysicalRect(context_.old_visual_rect) !=
          PhysicalRect(context_.old_paint_offset, box_.PreviousSize()) ||
      PhysicalRect(context_.fragment_data->VisualRect()) !=
          PhysicalRect(context_.fragment_data->PaintOffset(), box_.Size())) {
    return PaintInvalidationReason::kGeometry;
  }

  DCHECK_NE(box_.PreviousSize(), box_.Size());

  // Incremental invalidation is not applicable if there is border in the
  // direction of border box size change because we don't know the border
  // width when issuing incremental raster invalidations.
  if (box_.BorderRight() || box_.BorderBottom())
    return PaintInvalidationReason::kGeometry;

  if (style.HasVisualOverflowingEffect() || style.HasEffectiveAppearance() ||
      style.HasFilterInducingProperty() || style.HasMask() || style.ClipPath())
    return PaintInvalidationReason::kGeometry;

  if (style.HasBorderRadius() || style.CanRenderBorderImage())
    return PaintInvalidationReason::kGeometry;

  // Needs to repaint frame boundaries.
  if (box_.IsFrameSet())
    return PaintInvalidationReason::kGeometry;

  // Needs to repaint column rules.
  if (box_.IsLayoutMultiColumnSet())
    return PaintInvalidationReason::kGeometry;

  // Background invalidation has been done during InvalidateBackground(), so
  // we don't need to check background in this function.

  return PaintInvalidationReason::kIncremental;
}

bool BoxPaintInvalidator::BackgroundGeometryDependsOnLayoutOverflowRect() {
  return HasEffectiveBackground() &&
         box_.StyleRef().BackgroundLayers().AnyLayerHasLocalAttachmentImage();
}

bool BoxPaintInvalidator::BackgroundPaintsOntoScrollingContentsLayer() {
  if (!HasEffectiveBackground())
    return false;
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    return box_.GetBackgroundPaintLocation() &
           kBackgroundPaintInScrollingContents;
  }
  if (!box_.HasLayer())
    return false;
  if (auto* mapping = box_.Layer()->GetCompositedLayerMapping())
    return mapping->BackgroundPaintsOntoScrollingContentsLayer();
  return false;
}

bool BoxPaintInvalidator::BackgroundPaintsOntoMainGraphicsLayer() {
  if (!HasEffectiveBackground())
    return false;
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return box_.GetBackgroundPaintLocation() & kBackgroundPaintInGraphicsLayer;
  if (!box_.HasLayer())
    return true;
  if (auto* mapping = box_.Layer()->GetCompositedLayerMapping())
    return mapping->BackgroundPaintsOntoGraphicsLayer();
  return true;
}

bool BoxPaintInvalidator::ShouldFullyInvalidateBackgroundOnLayoutOverflowChange(
    const PhysicalRect& old_layout_overflow,
    const PhysicalRect& new_layout_overflow) {
  if (new_layout_overflow == old_layout_overflow)
    return false;

  if (!BackgroundGeometryDependsOnLayoutOverflowRect())
    return false;

  // The background should invalidate on most location changes.
  if (new_layout_overflow.offset != old_layout_overflow.offset)
    return true;

  return ShouldFullyInvalidateFillLayersOnSizeChange(
      box_.StyleRef().BackgroundLayers(), old_layout_overflow.size,
      new_layout_overflow.size);
}

BoxPaintInvalidator::BackgroundInvalidationType
BoxPaintInvalidator::ComputeViewBackgroundInvalidation() {
  DCHECK(box_.IsLayoutView());

  const auto& layout_view = ToLayoutView(box_);
  auto new_background_rect = layout_view.BackgroundRect();
  auto old_background_rect = layout_view.PreviousBackgroundRect();
  layout_view.SetPreviousBackgroundRect(new_background_rect);

  // BackgroundRect is the positioning area of all fixed attachment backgrounds,
  // including the LayoutView's and descendants'.
  bool background_location_changed =
      new_background_rect.offset != old_background_rect.offset;
  bool background_size_changed =
      new_background_rect.size != old_background_rect.size;
  if (background_location_changed || background_size_changed) {
    for (auto* object :
         layout_view.GetFrameView()->BackgroundAttachmentFixedObjects())
      object->SetBackgroundNeedsFullPaintInvalidation();
  }

  if (background_location_changed ||
      layout_view.BackgroundNeedsFullPaintInvalidation())
    return BackgroundInvalidationType::kFull;

  // LayoutView's non-fixed-attachment background is positioned in the
  // document element and needs to invalidate if the size changes.
  // See: https://drafts.csswg.org/css-backgrounds-3/#root-background.
  if (BackgroundGeometryDependsOnLayoutOverflowRect()) {
    Element* document_element = box_.GetDocument().documentElement();
    if (document_element) {
      const auto* document_background = document_element->GetLayoutObject();
      if (document_background && document_background->IsBox()) {
        const auto* document_background_box = ToLayoutBox(document_background);
        if (ShouldFullyInvalidateBackgroundOnLayoutOverflowChange(
                document_background_box->PreviousPhysicalLayoutOverflowRect(),
                document_background_box->PhysicalLayoutOverflowRect())) {
          return BackgroundInvalidationType::kFull;
        }
      }
    }
  }

  return background_size_changed ? BackgroundInvalidationType::kIncremental
                                 : BackgroundInvalidationType::kNone;
}

BoxPaintInvalidator::BackgroundInvalidationType
BoxPaintInvalidator::ComputeBackgroundInvalidation(
    bool& should_invalidate_all_layers) {
  // Need to fully invalidate the background on all layers if background paint
  // location changed.
  auto new_background_location = box_.GetBackgroundPaintLocation();
  if (new_background_location != box_.PreviousBackgroundPaintLocation()) {
    should_invalidate_all_layers = true;
    box_.GetMutableForPainting().SetPreviousBackgroundPaintLocation(
        new_background_location);
    return BackgroundInvalidationType::kFull;
  }

  // If background changed, we may paint the background on different graphics
  // layer, so we need to fully invalidate the background on all layers.
  if (box_.BackgroundNeedsFullPaintInvalidation()) {
    should_invalidate_all_layers = true;
    return BackgroundInvalidationType::kFull;
  }

  if (!HasEffectiveBackground())
    return BackgroundInvalidationType::kNone;

  const auto& background_layers = box_.StyleRef().BackgroundLayers();
  if (background_layers.AnyLayerHasDefaultAttachmentImage() &&
      ShouldFullyInvalidateFillLayersOnSizeChange(
          background_layers, PhysicalSizeToBeNoop(box_.PreviousSize()),
          PhysicalSizeToBeNoop(box_.Size())))
    return BackgroundInvalidationType::kFull;

  if (background_layers.AnyLayerUsesContentBox() &&
      box_.PreviousPhysicalContentBoxRect() != box_.PhysicalContentBoxRect())
    return BackgroundInvalidationType::kFull;

  bool layout_overflow_change_causes_invalidation =
      (BackgroundGeometryDependsOnLayoutOverflowRect() ||
       BackgroundPaintsOntoScrollingContentsLayer());

  if (!layout_overflow_change_causes_invalidation)
    return BackgroundInvalidationType::kNone;

  const auto& old_layout_overflow = box_.PreviousPhysicalLayoutOverflowRect();
  auto new_layout_overflow = box_.PhysicalLayoutOverflowRect();
  if (ShouldFullyInvalidateBackgroundOnLayoutOverflowChange(
          old_layout_overflow, new_layout_overflow))
    return BackgroundInvalidationType::kFull;

  if (new_layout_overflow != old_layout_overflow) {
    // Do incremental invalidation if possible.
    if (old_layout_overflow.offset == new_layout_overflow.offset)
      return BackgroundInvalidationType::kIncremental;
    return BackgroundInvalidationType::kFull;
  }
  return BackgroundInvalidationType::kNone;
}

void BoxPaintInvalidator::InvalidateBackground() {
  bool should_invalidate_all_layers = false;
  auto background_invalidation_type =
      ComputeBackgroundInvalidation(should_invalidate_all_layers);
  if (box_.IsLayoutView()) {
    background_invalidation_type = std::max(
        background_invalidation_type, ComputeViewBackgroundInvalidation());
  }

  if (box_.GetScrollableArea()) {
    if (should_invalidate_all_layers ||
        (BackgroundPaintsOntoScrollingContentsLayer() &&
         background_invalidation_type != BackgroundInvalidationType::kNone)) {
      auto reason =
          background_invalidation_type == BackgroundInvalidationType::kFull
              ? PaintInvalidationReason::kBackground
              : PaintInvalidationReason::kIncremental;
      context_.painting_layer->SetNeedsRepaint();
      ObjectPaintInvalidator(box_).InvalidateDisplayItemClient(
          box_.GetScrollableArea()->GetScrollingBackgroundDisplayItemClient(),
          reason);
    }
  }

  if (should_invalidate_all_layers ||
      (BackgroundPaintsOntoMainGraphicsLayer() &&
       background_invalidation_type == BackgroundInvalidationType::kFull)) {
    box_.GetMutableForPainting()
        .SetShouldDoFullPaintInvalidationWithoutGeometryChange(
            PaintInvalidationReason::kBackground);
  }
}

void BoxPaintInvalidator::InvalidatePaint() {
  InvalidateBackground();

  ObjectPaintInvalidatorWithContext(box_, context_)
      .InvalidatePaintWithComputedReason(ComputePaintInvalidationReason());

  if (PaintLayerScrollableArea* area = box_.GetScrollableArea())
    area->InvalidatePaintOfScrollControlsIfNeeded(context_);

  // This is for the next invalidatePaintIfNeeded so must be at the end.
  SavePreviousBoxGeometriesIfNeeded();
}

bool BoxPaintInvalidator::
    NeedsToSavePreviousContentBoxRectOrLayoutOverflowRect() {
  // The LayoutView depends on the document element's layout overflow rect (see:
  // ComputeViewBackgroundInvalidation) and needs to invalidate before the
  // document element invalidates. There are few document elements so the
  // previous layout overflow rect is always saved, rather than duplicating the
  // logic save-if-needed logic for this special case.
  if (box_.IsDocumentElement())
    return true;

  // Replaced elements are clipped to the content box thus we need to check
  // for its size.
  if (box_.IsLayoutReplaced())
    return true;

  // Don't save old box geometries if the paint rect is empty because we'll
  // fully invalidate once the paint rect becomes non-empty.
  if (context_.fragment_data->VisualRect().IsEmpty())
    return false;

  if (box_.PaintedOutputOfObjectHasNoEffectRegardlessOfSize())
    return false;

  const ComputedStyle& style = box_.StyleRef();

  // Background and mask layers can depend on other boxes than border box. See
  // crbug.com/490533
  if ((style.BackgroundLayers().AnyLayerUsesContentBox() ||
       style.MaskLayers().AnyLayerUsesContentBox()) &&
      box_.ContentSize() != box_.Size())
    return true;
  if ((BackgroundGeometryDependsOnLayoutOverflowRect() ||
       BackgroundPaintsOntoScrollingContentsLayer()) &&
      box_.LayoutOverflowRect() != box_.BorderBoxRect())
    return true;

  return false;
}

void BoxPaintInvalidator::SavePreviousBoxGeometriesIfNeeded() {
  box_.GetMutableForPainting().SavePreviousSize();

  if (NeedsToSavePreviousContentBoxRectOrLayoutOverflowRect()) {
    box_.GetMutableForPainting()
        .SavePreviousContentBoxRectAndLayoutOverflowRect();
  } else {
    box_.GetMutableForPainting()
        .ClearPreviousContentBoxRectAndLayoutOverflowRect();
  }
}

}  // namespace blink
