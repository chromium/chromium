// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/box_paint_invalidator.h"

#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/ink_overflow.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/object_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"

namespace blink {

bool BoxPaintInvalidator::HasEffectiveBackground() {
  // The view can paint background not from the style.
  if (IsA<LayoutView>(box_))
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

  if (layer.Repeat().x != EFillRepeat::kRepeatFill &&
      layer.Repeat().x != EFillRepeat::kNoRepeatFill) {
    return true;
  }

  // TODO(alancutter): Make this work correctly for calc lengths.
  if (layer.PositionX().HasPercent() && !layer.PositionX().IsZero()) {
    return true;
  }

  if (layer.BackgroundXOrigin() != BackgroundEdgeOrigin::kLeft)
    return true;

  EFillSizeType size_type = layer.SizeType();

  if (size_type == EFillSizeType::kContain ||
      size_type == EFillSizeType::kCover)
    return true;

  DCHECK_EQ(size_type, EFillSizeType::kSizeLength);

  // TODO(alancutter): Make this work correctly for calc lengths.
  const Length& width = layer.SizeLength().Width();
  if (width.HasPercent() && !width.IsZero()) {
    return true;
  }

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

  if (layer.Repeat().y != EFillRepeat::kRepeatFill &&
      layer.Repeat().y != EFillRepeat::kNoRepeatFill) {
    return true;
  }

  // TODO(alancutter): Make this work correctly for calc lengths.
  if (layer.PositionY().HasPercent() && !layer.PositionY().IsZero()) {
    return true;
  }

  if (layer.BackgroundYOrigin() != BackgroundEdgeOrigin::kTop)
    return true;

  EFillSizeType size_type = layer.SizeType();

  if (size_type == EFillSizeType::kContain ||
      size_type == EFillSizeType::kCover)
    return true;

  DCHECK_EQ(size_type, EFillSizeType::kSizeLength);

  // TODO(alancutter): Make this work correctly for calc lengths.
  const Length& height = layer.SizeLength().Height();
  if (height.HasPercent() && !height.IsZero()) {
    return true;
  }

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

  if (reason == PaintInvalidationReason::kNone)
    return reason;

  if (IsLayoutFullPaintInvalidationReason(reason)) {
    return reason;
  }

  if (IsFullPaintInvalidationReason(reason) &&
      !box_.ShouldCheckLayoutForPaintInvalidation()) {
    return reason;
  }

  const ComputedStyle& style = box_.StyleRef();

  if (style.MaskLayers().AnyLayerUsesContentBox() &&
      box_.PreviousPhysicalContentBoxRect() != box_.PhysicalContentBoxRect())
    return PaintInvalidationReason::kLayout;

  if (const auto* layout_replaced = DynamicTo<LayoutReplaced>(box_)) {
    if (layout_replaced->ReplacedContentRect() !=
        layout_replaced->ReplacedContentRectFrom(
            box_.PreviousPhysicalContentBoxRect())) {
      return PaintInvalidationReason::kLayout;
    }
  }

#if DCHECK_IS_ON()
  // TODO(crbug.com/1205708): Audit this.
  InkOverflow::ReadUnsetAsNoneScope read_unset_as_none;
#endif
  if (box_.PreviousSize() == box_.Size() &&
      box_.PreviousSelfVisualOverflowRect() == box_.SelfVisualOverflowRect()) {
    return IsFullPaintInvalidationReason(reason)
               ? reason
               : PaintInvalidationReason::kNone;
  }

  // Incremental invalidation is not applicable if there is visual overflow.
  if (box_.PreviousSelfVisualOverflowRect().size != box_.PreviousSize() ||
      box_.SelfVisualOverflowRect().size != box_.Size()) {
    return PaintInvalidationReason::kLayout;
  }

  // Incremental invalidation is not applicable if paint offset or size has
  // fraction.
  if (context_.old_paint_offset.HasFraction() ||
      context_.fragment_data->PaintOffset().HasFraction() ||
      box_.PreviousSize().HasFraction() || box_.Size().HasFraction()) {
    return PaintInvalidationReason::kLayout;
  }

  // Incremental invalidation is not applicable if there is border in the
  // direction of border box size change because we don't know the border
  // width when issuing incremental raster invalidations.
  if (box_.BorderRight() || box_.BorderBottom())
    return PaintInvalidationReason::kLayout;

  if (style.HasVisualOverflowingEffect() || style.HasEffectiveAppearance() ||
      style.HasFilterInducingProperty() || style.HasMask() ||
      style.HasClipPath())
    return PaintInvalidationReason::kLayout;

  if (style.HasBorderRadius() || style.CanRenderBorderImage())
    return PaintInvalidationReason::kLayout;

  // Needs to repaint frame boundaries.
  if (box_.IsFrameSet()) {
    return PaintInvalidationReason::kLayout;
  }

  // Background invalidation has been done during InvalidateBackground(), so
  // we don't need to check background in this function.

  return reason;
}

bool BoxPaintInvalidator::BackgroundGeometryDependsOnScrollableOverflowRect() {
  return HasEffectiveBackground() &&
         box_.StyleRef().BackgroundLayers().AnyLayerHasLocalAttachmentImage();
}

bool BoxPaintInvalidator::BackgroundPaintsInContentsSpace() {
  if (!HasEffectiveBackground())
    return false;
  return box_.GetBackgroundPaintLocation() & kBackgroundPaintInContentsSpace;
}

bool BoxPaintInvalidator::BackgroundPaintsInBorderBoxSpace() {
  if (!HasEffectiveBackground())
    return false;
  return box_.GetBackgroundPaintLocation() & kBackgroundPaintInBorderBoxSpace;
}

bool BoxPaintInvalidator::
    ShouldFullyInvalidateBackgroundOnScrollableOverflowChange(
        const PhysicalRect& old_scrollable_overflow,
        const PhysicalRect& new_scrollable_overflow) {
  if (new_scrollable_overflow == old_scrollable_overflow) {
    return false;
  }

  if (!BackgroundGeometryDependsOnScrollableOverflowRect()) {
    return false;
  }

  // The background should invalidate on most location changes.
  if (new_scrollable_overflow.offset != old_scrollable_overflow.offset) {
    return true;
  }

  return ShouldFullyInvalidateFillLayersOnSizeChange(
      box_.StyleRef().BackgroundLayers(), old_scrollable_overflow.size,
      new_scrollable_overflow.size);
}

BoxPaintInvalidator::BackgroundInvalidationType
BoxPaintInvalidator::ComputeViewBackgroundInvalidation() {
  DCHECK(IsA<LayoutView>(box_));

  const auto& layout_view = To<LayoutView>(box_);
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
    for (const auto& object :
         layout_view.GetFrameView()->BackgroundAttachmentFixedObjects())
      object->SetBackgroundNeedsFullPaintInvalidation();
  }

  if (background_location_changed ||
      layout_view.BackgroundNeedsFullPaintInvalidation() ||
      (context_.subtree_flags &
       PaintInvalidatorContext::kSubtreeFullInvalidation)) {
    return BackgroundInvalidationType::kFull;
  }

  if (Element* root_element = box_.GetDocument().documentElement()) {
    if (const auto* root_object = root_element->GetLayoutObject()) {
      if (root_object->IsBox()) {
        const auto* root_box = To<LayoutBox>(root_object);
        // LayoutView's non-fixed-attachment background is positioned in the
        // root element and needs to invalidate if the size changes.
        // See: https://drafts.csswg.org/css-backgrounds-3/#root-background.
        const auto& background_layers = box_.StyleRef().BackgroundLayers();
        if (ShouldFullyInvalidateFillLayersOnSizeChange(
                background_layers, root_box->PreviousSize(),
                root_box->Size())) {
          return BackgroundInvalidationType::kFull;
        }
        if (BackgroundGeometryDependsOnScrollableOverflowRect() &&
            ShouldFullyInvalidateBackgroundOnScrollableOverflowChange(
                root_box->PreviousScrollableOverflowRect(),
                root_box->ScrollableOverflowRect())) {
          return BackgroundInvalidationType::kFull;
        }
        // It also uses the root element's content box in case the background
        // comes from the root element and positioned in content box.
        if (background_layers.AnyLayerUsesContentBox() &&
            root_box->PreviousPhysicalContentBoxRect() !=
                root_box->PhysicalContentBoxRect()) {
          return BackgroundInvalidationType::kFull;
        }
      }
      // The view background paints with a transform but nevertheless extended
      // onto an infinite canvas. In cases where it has a transform we can't
      // apply incremental invalidation, because the visual rect is no longer
      // axis-aligned to the LayoutView.
      if (root_object->HasTransform())
        return BackgroundInvalidationType::kFull;
    }
  }

  return background_size_changed ? BackgroundInvalidationType::kIncremental
                                 : BackgroundInvalidationType::kNone;
}

BoxPaintInvalidator::BackgroundInvalidationType
BoxPaintInvalidator::ComputeBackgroundInvalidation(
    bool& should_invalidate_all_layers) {
  // If background changed, we may paint the background on different graphics
  // layer, so we need to fully invalidate the background on all layers.
  if (box_.BackgroundNeedsFullPaintInvalidation() ||
      (context_.subtree_flags &
       PaintInvalidatorContext::kSubtreeFullInvalidation)) {
    should_invalidate_all_layers = true;
    return BackgroundInvalidationType::kFull;
  }

  if (!HasEffectiveBackground())
    return BackgroundInvalidationType::kNone;

  const auto& background_layers = box_.StyleRef().BackgroundLayers();
  if (background_layers.AnyLayerHasDefaultAttachmentImage() &&
      ShouldFullyInvalidateFillLayersOnSizeChange(
          background_layers, box_.PreviousSize(), box_.Size())) {
    return BackgroundInvalidationType::kFull;
  }

  if (background_layers.AnyLayerUsesContentBox() &&
      box_.PreviousPhysicalContentBoxRect() != box_.PhysicalContentBoxRect())
    return BackgroundInvalidationType::kFull;

  bool scrollable_overflow_change_causes_invalidation =
      (BackgroundGeometryDependsOnScrollableOverflowRect() ||
       BackgroundPaintsInContentsSpace());

  if (!scrollable_overflow_change_causes_invalidation) {
    return BackgroundInvalidationType::kNone;
  }

  const auto& old_scrollable_overflow = box_.PreviousScrollableOverflowRect();
  auto new_scrollable_overflow = box_.ScrollableOverflowRect();
  if (ShouldFullyInvalidateBackgroundOnScrollableOverflowChange(
          old_scrollable_overflow, new_scrollable_overflow)) {
    return BackgroundInvalidationType::kFull;
  }

  if (new_scrollable_overflow != old_scrollable_overflow) {
    // Do incremental invalidation if possible.
    if (old_scrollable_overflow.offset == new_scrollable_overflow.offset) {
      return BackgroundInvalidationType::kIncremental;
    }
    return BackgroundInvalidationType::kFull;
  }
  return BackgroundInvalidationType::kNone;
}

void BoxPaintInvalidator::InvalidateBackground() {
  bool should_invalidate_in_both_spaces = false;
  auto background_invalidation_type =
      ComputeBackgroundInvalidation(should_invalidate_in_both_spaces);
  if (IsA<LayoutView>(box_)) {
    background_invalidation_type = std::max(
        background_invalidation_type, ComputeViewBackgroundInvalidation());
  }

  if (box_.GetScrollableArea()) {
    if (should_invalidate_in_both_spaces ||
        (BackgroundPaintsInContentsSpace() &&
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

  if (should_invalidate_in_both_spaces ||
      (BackgroundPaintsInBorderBoxSpace() &&
       background_invalidation_type == BackgroundInvalidationType::kFull)) {
    box_.GetMutableForPainting()
        .SetShouldDoFullPaintInvalidationWithoutLayoutChange(
            PaintInvalidationReason::kBackground);
  }

  if (background_invalidation_type == BackgroundInvalidationType::kNone &&
      box_.ScrollsOverflow() &&
      box_.PreviousScrollableOverflowRect() != box_.ScrollableOverflowRect()) {
    // We need to re-record the hit test data for scrolling contents.
    context_.painting_layer->SetNeedsRepaint();
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

bool BoxPaintInvalidator::NeedsToSavePreviousContentBoxRect() {
  // Replaced elements are clipped to the content box thus we need to check
  // for its size.
  if (box_.IsLayoutReplaced())
    return true;

  const ComputedStyle& style = box_.StyleRef();

  // Background and mask layers can depend on other boxes than border box. See
  // crbug.com/490533
  if ((style.BackgroundLayers().AnyLayerUsesContentBox() ||
       style.MaskLayers().AnyLayerUsesContentBox()) &&
      box_.ContentSize() != box_.Size()) {
    return true;
  }

  return false;
}

bool BoxPaintInvalidator::NeedsToSavePreviousOverflowData() {
  if (box_.HasVisualOverflow() || box_.HasScrollableOverflow()) {
    return true;
  }

  // If we don't have scrollable overflow, the layout overflow rect is the
  // padding box rect, and we need to save it if the background depends on it.
  // We also need to save the rect for the document element because the
  // LayoutView may depend on the document element's scrollable overflow rect
  // (see: ComputeViewBackgroundInvalidation).
  if ((BackgroundGeometryDependsOnScrollableOverflowRect() ||
       BackgroundPaintsInContentsSpace() || box_.IsDocumentElement()) &&
      box_.ScrollableOverflowRect() != box_.PhysicalBorderBoxRect()) {
    return true;
  }

  return false;
}

void BoxPaintInvalidator::SavePreviousBoxGeometriesIfNeeded() {
  auto mutable_box = box_.GetMutableForPainting();
  mutable_box.SavePreviousSize();

#if DCHECK_IS_ON()
  // TODO(crbug.com/1205708): Audit this.
  InkOverflow::ReadUnsetAsNoneScope read_unset_as_none;
#endif
  if (NeedsToSavePreviousOverflowData())
    mutable_box.SavePreviousOverflowData();
  else
    mutable_box.ClearPreviousOverflowData();

  if (NeedsToSavePreviousContentBoxRect())
    mutable_box.SavePreviousContentBoxRect();
  else
    mutable_box.ClearPreviousContentBoxRect();
}

}  // namespace blink
