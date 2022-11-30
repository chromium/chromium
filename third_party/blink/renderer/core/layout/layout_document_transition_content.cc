// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_document_transition_content.h"

#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"

namespace blink {

LayoutDocumentTransitionContent::LayoutDocumentTransitionContent(
    DocumentTransitionContentElement* element)
    : LayoutReplaced(element),
      layer_(cc::DocumentTransitionContentLayer::Create(
          element->resource_id(),
          element->is_live_content_element())) {
  SetIntrinsicSize(element->intrinsic_size());
}

LayoutDocumentTransitionContent::~LayoutDocumentTransitionContent() = default;

void LayoutDocumentTransitionContent::OnIntrinsicSizeUpdated(
    const LayoutSize& intrinsic_size) {
  NOT_DESTROYED();
  SetIntrinsicSize(intrinsic_size);
  SetIntrinsicLogicalWidthsDirty();
  SetNeedsLayout(layout_invalidation_reason::kSizeChanged);
}

PaintLayerType LayoutDocumentTransitionContent::LayerTypeRequired() const {
  NOT_DESTROYED();
  return kNormalPaintLayer;
}

void LayoutDocumentTransitionContent::PaintReplaced(
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
      context, *this, DisplayItem::kForeignLayerDocumentTransitionContent,
      layer_, gfx::Point(pixel_snapped_rect.x(), pixel_snapped_rect.y()));
}

}  // namespace blink
