// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/box_model_object_painter.h"

#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/line/root_inline_box.h"
#include "third_party/blink/renderer/core/paint/background_image_geometry.h"
#include "third_party/blink/renderer/core/paint/box_decoration_data.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/geometry/layout_point.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect_outsets.h"
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

LayoutSize LogicalOffsetOnLine(const InlineFlowBox& flow_box) {
  // Compute the offset of the passed flow box when seen as part of an
  // unbroken continuous strip (c.f box-decoration-break: slice.)
  LayoutUnit logical_offset_on_line;
  if (flow_box.IsLeftToRightDirection()) {
    for (const InlineFlowBox* curr = flow_box.PrevForSameLayoutObject(); curr;
         curr = curr->PrevForSameLayoutObject())
      logical_offset_on_line += curr->LogicalWidth();
  } else {
    for (const InlineFlowBox* curr = flow_box.NextForSameLayoutObject(); curr;
         curr = curr->NextForSameLayoutObject())
      logical_offset_on_line += curr->LogicalWidth();
  }
  LayoutSize logical_offset(logical_offset_on_line, LayoutUnit());
  return flow_box.IsHorizontal() ? logical_offset
                                 : logical_offset.TransposedSize();
}

}  // anonymous namespace

BoxModelObjectPainter::BoxModelObjectPainter(const LayoutBoxModelObject& box,
                                             const InlineFlowBox* flow_box)
    : BoxPainterBase(&box.GetDocument(), box.StyleRef(), GetNode(box)),
      box_model_(box),
      flow_box_(flow_box) {}

void BoxModelObjectPainter::PaintTextClipMask(
    const PaintInfo& paint_info,
    const gfx::Rect& mask_rect,
    const PhysicalOffset& paint_offset,
    bool object_has_multiple_boxes) {
  PaintInfo mask_paint_info(paint_info.context, CullRect(mask_rect),
                            PaintPhase::kTextClip);
  mask_paint_info.SetFragmentID(paint_info.FragmentID());
  if (flow_box_) {
    LayoutSize local_offset = ToLayoutSize(flow_box_->Location());
    if (object_has_multiple_boxes &&
        box_model_.StyleRef().BoxDecorationBreak() ==
            EBoxDecorationBreak::kSlice) {
      local_offset -= LogicalOffsetOnLine(*flow_box_);
    }
    // TODO(layout-ng): This looks incorrect in flipped writing mode.
    PhysicalOffset physical_local_offset(local_offset.Width(),
                                         local_offset.Height());
    const RootInlineBox& root = flow_box_->Root();
    flow_box_->Paint(mask_paint_info, paint_offset - physical_local_offset,
                     root.LineTop(), root.LineBottom());
  } else if (auto* layout_block = DynamicTo<LayoutBlock>(box_model_)) {
    layout_block->PaintObject(mask_paint_info, paint_offset);
  } else {
    // We should go through the above path for LayoutInlines.
    DCHECK(!box_model_.IsLayoutInline());
    // Other types of objects don't have anything meaningful to paint for text
    // clip mask.
  }
}

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
  LayoutRectOutsets border = AdjustedBorderOutsets(info);
  scrolled_paint_rect.SetWidth(border.Left() + this_box.ScrollWidth() +
                               border.Right());
  scrolled_paint_rect.SetHeight(this_box.BorderTop() + this_box.ScrollHeight() +
                                this_box.BorderBottom());
  return scrolled_paint_rect;
}

LayoutRectOutsets BoxModelObjectPainter::ComputeBorders() const {
  return box_model_.BorderBoxOutsets();
}

LayoutRectOutsets BoxModelObjectPainter::ComputePadding() const {
  return box_model_.PaddingOutsets();
}

BoxPainterBase::FillLayerInfo BoxModelObjectPainter::GetFillLayerInfo(
    const Color& color,
    const FillLayer& bg_layer,
    BackgroundBleedAvoidance bleed_avoidance,
    bool is_painting_background_in_contents_space) const {
  PhysicalBoxSides sides_to_include;
  if (flow_box_)
    sides_to_include = flow_box_->SidesToInclude();
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
