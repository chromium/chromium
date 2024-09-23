// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/box_model_object_painter.h"

#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/paint/background_image_geometry.h"
#include "third_party/blink/renderer/core/paint/box_decoration_data.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"

namespace blink {

namespace {

Node* GetNode(const LayoutBoxModelObject& box_model) {
  Node* node = nullptr;
  const LayoutObject* layout_object = &box_model;
  for (; layout_object && !node; layout_object = layout_object->Parent()) {
    node = layout_object->GetNode();
  }
  return node;
}

}  // anonymous namespace

BoxModelObjectPainter::BoxModelObjectPainter(const LayoutBoxModelObject& box)
    : BoxPainterBase(box.GetDocument(), box.StyleRef(), GetNode(box)),
      box_model_(box) {}

PhysicalRect BoxModelObjectPainter::AdjustRectForScrolledContent(
    GraphicsContext& context,
    const PhysicalBoxStrut& border,
    const PhysicalRect& rect) const {
  // Clip to the overflow area.
  // TODO(chrishtr): this should be pixel-snapped.
  const auto& this_box = To<LayoutBox>(box_model_);
  context.Clip(gfx::RectF(this_box.OverflowClipRect(rect.offset)));

  // Adjust the paint rect to reflect a scrolled content box with borders at
  // the ends.
  PhysicalRect scrolled_paint_rect = rect;
  scrolled_paint_rect.offset -=
      PhysicalOffset(this_box.PixelSnappedScrolledContentOffset());
  scrolled_paint_rect.SetWidth(border.HorizontalSum() + this_box.ScrollWidth());
  scrolled_paint_rect.SetHeight(this_box.BorderTop() + this_box.ScrollHeight() +
                                this_box.BorderBottom());
  return scrolled_paint_rect;
}

BoxPainterBase::FillLayerInfo BoxModelObjectPainter::GetFillLayerInfo(
    const Color& color,
    const FillLayer& bg_layer,
    BackgroundBleedAvoidance bleed_avoidance,
    bool is_painting_background_in_contents_space) const {
  return BoxPainterBase::FillLayerInfo(
      box_model_.GetDocument(), box_model_.StyleRef(),
      box_model_.IsScrollContainer(), color, bg_layer, bleed_avoidance,
      PhysicalBoxSides(), box_model_.IsLayoutInline(),
      is_painting_background_in_contents_space);
}

}  // namespace blink
