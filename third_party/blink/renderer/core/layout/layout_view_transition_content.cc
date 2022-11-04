// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_view_transition_content.h"

#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"

namespace blink {

LayoutViewTransitionContent::LayoutViewTransitionContent(
    ViewTransitionContentElement* element)
    : LayoutReplaced(element),
      layer_(cc::ViewTransitionContentLayer::Create(
          element->resource_id(),
          element->is_live_content_element())) {
  SetIntrinsicSize(element->intrinsic_size());
}

LayoutViewTransitionContent::~LayoutViewTransitionContent() = default;

void LayoutViewTransitionContent::OnIntrinsicSizeUpdated(
    const LayoutSize& intrinsic_size) {
  NOT_DESTROYED();
  SetIntrinsicSize(intrinsic_size);
  SetIntrinsicLogicalWidthsDirty();
  SetNeedsLayout(layout_invalidation_reason::kSizeChanged);
}

PaintLayerType LayoutViewTransitionContent::LayerTypeRequired() const {
  NOT_DESTROYED();
  return kNormalPaintLayer;
}

void LayoutViewTransitionContent::PaintReplaced(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) const {
  NOT_DESTROYED();
  GraphicsContext& context = paint_info.context;

  PhysicalRect paint_rect = ReplacedContentRect();
  paint_rect.Move(paint_offset);
  gfx::Rect pixel_snapped_rect = ToPixelSnappedRect(paint_rect);
  layer_->SetBounds(
      gfx::Size(pixel_snapped_rect.width(), pixel_snapped_rect.height()));
  layer_->SetIsDrawable(true);
  RecordForeignLayer(
      context, *this, DisplayItem::kForeignLayerViewTransitionContent, layer_,
      gfx::Point(pixel_snapped_rect.x(), pixel_snapped_rect.y()));
}

}  // namespace blink
