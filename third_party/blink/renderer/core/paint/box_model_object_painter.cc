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
#include "third_party/blink/renderer/platform/geometry/layout_point.h"
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
    : BoxPainterBase(&box.GetDocument(), box.StyleRef(), GetNode(box)),
      box_model_(box) {}

PhysicalRect BoxModelObjectPainter::AdjustRectForScrolledContent(
    const PaintInfo& paint_info,
    const BoxPainterBase::FillLayerInfo& info,
    const PhysicalRect& rect) {
  if (!info.is_clipped_with_local_scrolling)
    return rect;
  if (paint_info.IsPaintingBackgroundInContentsSpace())
    return rect;

  GraphicsContext& context = paint_info.context;
  // Clip to the overflow area.
  // TODO(chrishtr): this should be pixel-snapped.
  const auto& this_box = To<LayoutBox>(box_model_);
  context.Clip(gfx::RectF(this_box.OverflowClipRect(rect.offset)));

  // Adjust the paint rect to reflect a scrolled content box with borders at
  // the ends.
  PhysicalRect scrolled_paint_rect = rect;
  scrolled_paint_rect.offset -=
      PhysicalOffset(this_box.PixelSnappedScrolledContentOffset());
  NGPhysicalBoxStrut border = AdjustedBorderOutsets(info);
  scrolled_paint_rect.SetWidth(border.HorizontalSum() + this_box.ScrollWidth());
  scrolled_paint_rect.SetHeight(this_box.BorderTop() + this_box.ScrollHeight() +
                                this_box.BorderBottom());
  return scrolled_paint_rect;
}

NGPhysicalBoxStrut BoxModelObjectPainter::ComputeBorders() const {
  return box_model_.BorderBoxOutsets();
}

NGPhysicalBoxStrut BoxModelObjectPainter::ComputePadding() const {
  return box_model_.PaddingOutsets();
}

BoxPainterBase::FillLayerInfo BoxModelObjectPainter::GetFillLayerInfo(
    const Color& color,
    const FillLayer& bg_layer,
    BackgroundBleedAvoidance bleed_avoidance,
    bool is_painting_background_in_contents_space) const {
  PhysicalBoxSides sides_to_include;
  RespectImageOrientationEnum respect_orientation =
      LayoutObject::ShouldRespectImageOrientation(&box_model_);
  if (auto* style_image = bg_layer.GetImage()) {
    respect_orientation =
        style_image->ForceOrientationIfNecessary(respect_orientation);
  }
  return BoxPainterBase::FillLayerInfo(
      box_model_.GetDocument(), box_model_.StyleRef(),
      box_model_.IsScrollContainer(), color, bg_layer, bleed_avoidance,
      respect_orientation, sides_to_include, box_model_.IsLayoutInline(),
      is_painting_background_in_contents_space);
}

}  // namespace blink
