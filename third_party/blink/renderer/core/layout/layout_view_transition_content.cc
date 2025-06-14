// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_view_transition_content.h"

#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/transform_util.h"

namespace blink {

LayoutViewTransitionContent::LayoutViewTransitionContent(
    ViewTransitionContentElement* element)
    : LayoutReplaced(element),
      layer_(cc::ViewTransitionContentLayer::Create(
          element->resource_id(),
          element->is_live_content_element())),
      captured_rect_(element->captured_rect()),
      reference_rect_in_enclosing_layer_space_(
          element->reference_rect_in_enclosing_layer_space()),
      propagate_max_extent_rect_(element->propagate_max_extent_rect()) {}

LayoutViewTransitionContent::~LayoutViewTransitionContent() = default;

void LayoutViewTransitionContent::OnIntrinsicSizeUpdated(
    const gfx::RectF& captured_rect,
    const gfx::RectF& reference_rect_in_enclosing_layer_space,
    bool propagate_max_extent_rect) {
  NOT_DESTROYED();
  if (captured_rect_ != captured_rect) {
    SetShouldDoFullPaintInvalidationWithoutLayoutChange(
        PaintInvalidationReason::kImage);
  }

  captured_rect_ = captured_rect;
  reference_rect_in_enclosing_layer_space_ =
      reference_rect_in_enclosing_layer_space;
  propagate_max_extent_rect_ = propagate_max_extent_rect;

  SetIntrinsicLogicalWidthsDirty();
  SetNeedsLayout(layout_invalidation_reason::kSizeChanged);
}

PaintLayerType LayoutViewTransitionContent::LayerTypeRequired() const {
  NOT_DESTROYED();
  return kNormalPaintLayer;
}

PhysicalRect
LayoutViewTransitionContent::ReplacedContentRectForCapturedContent() const {
  gfx::RectF paint_rect = gfx::RectF(ReplacedContentRect());
  gfx::RectF clipped_paint_rect = gfx::MapRect(
      captured_rect_, reference_rect_in_enclosing_layer_space_, paint_rect);
  return PhysicalRect::EnclosingRect(clipped_paint_rect);
}

PhysicalNaturalSizingInfo LayoutViewTransitionContent::GetNaturalDimensions()
    const {
  NOT_DESTROYED();
  const PhysicalSize natural_size(
      LayoutUnit(reference_rect_in_enclosing_layer_space_.width()),
      LayoutUnit(reference_rect_in_enclosing_layer_space_.height()));
  return PhysicalNaturalSizingInfo::MakeFixed(natural_size);
}

void LayoutViewTransitionContent::PaintReplaced(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) const {
  NOT_DESTROYED();
  GraphicsContext& context = paint_info.context;

  PhysicalRect paint_rect = ReplacedContentRectForCapturedContent();
  paint_rect.Move(paint_offset);
  gfx::Rect pixel_snapped_rect = ToPixelSnappedRect(paint_rect);
  layer_->SetBounds(
      gfx::Size(pixel_snapped_rect.width(), pixel_snapped_rect.height()));
  layer_->SetIsDrawable(true);

  if (propagate_max_extent_rect_) {
    layer_->SetMaxExtentsRectInOriginatingLayerSpace(
        propagate_max_extent_rect_ ? captured_rect_ : gfx::RectF());
  }

  RecordForeignLayer(
      context, *this, DisplayItem::kForeignLayerViewTransitionContent, layer_,
      gfx::Point(pixel_snapped_rect.x(), pixel_snapped_rect.y()));
}

}  // namespace blink
