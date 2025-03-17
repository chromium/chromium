// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/box_painter.h"

#include <optional>

#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/scrollable_area_painter.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {

void BoxPainter::RecordRegionCaptureData(
    const PaintInfo& paint_info,
    const PhysicalRect& paint_rect,
    const DisplayItemClient& background_client) {
  const Element* element = DynamicTo<Element>(layout_box_.GetNode());
  if (element) {
    const RegionCaptureCropId* crop_id = element->GetRegionCaptureCropId();
    if (crop_id) {
      paint_info.context.GetPaintController().RecordRegionCaptureData(
          background_client, *crop_id, ToPixelSnappedRect(paint_rect));
    }
  }
}

void BoxPainter::RecordScrollHitTestData(
    const PaintInfo& paint_info,
    const DisplayItemClient& background_client,
    const FragmentData* fragment) {
  if (!fragment) {
    return;
  }

  // Scroll hit test data are only needed for compositing. This flag is used for
  // printing and drag images which do not need hit testing.
  if (paint_info.ShouldOmitCompositingInfo())
    return;

  // If an object is not visible, it does not scroll.
  const ComputedStyle& style = layout_box_.StyleRef();
  if (style.Visibility() != EVisibility::kVisible) {
    return;
  }

  if (!layout_box_.GetScrollableArea())
    return;

  // If there is an associated scroll node, emit scroll hit test data.
  const auto* properties = fragment->PaintProperties();
  auto hit_test_opaqueness = ObjectPainter(layout_box_).GetHitTestOpaqueness();
  if (properties && properties->Scroll()) {
    DCHECK(properties->ScrollTranslation());
    // We record scroll hit test data in the local border box properties
    // instead of the contents properties so that the scroll hit test is not
    // clipped or scrolled.
    auto& paint_controller = paint_info.context.GetPaintController();
#if DCHECK_IS_ON()
    // TODO(crbug.com/1256990): This should be
    // DCHECK_EQ(fragment->LocalBorderBoxProperties(),
    //           paint_controller.CurrentPaintChunkProperties());
    // but we have problems about the effect node with CompositingReason::
    // kTransform3DSceneLeaf on non-stacking-context elements.
    auto border_box_properties = fragment->LocalBorderBoxProperties();
    auto current_properties = paint_controller.CurrentPaintChunkProperties();
    DCHECK_EQ(&border_box_properties.Transform(),
              &current_properties.Transform())
        << border_box_properties.Transform().ToTreeString().Utf8()
        << current_properties.Transform().ToTreeString().Utf8();
    DCHECK_EQ(&border_box_properties.Clip(), &current_properties.Clip())
        << border_box_properties.Clip().ToTreeString().Utf8()
        << current_properties.Clip().ToTreeString().Utf8();
#endif
    gfx::Rect cull_rect = fragment->GetContentsCullRect().Rect();
    if (cull_rect.Contains(properties->Scroll()->ContentsRect())) {
      cull_rect = CullRect::Infinite().Rect();
    } else {
      // Don't pass the cull rect if it doesn't cover the container rect
      // because cc can't distinguish the case from paint checkerboarding.
      gfx::Rect cull_rect_in_container_space = gfx::ToEnclosingRect(
          gfx::RectF(cull_rect) +
          properties->ScrollTranslation()->Get2dTranslation());
      if (!cull_rect_in_container_space.Contains(
              properties->Scroll()->ContainerRect())) {
        cull_rect = CullRect::Infinite().Rect();
      }
    }
    paint_controller.RecordScrollHitTestData(
        background_client, DisplayItem::kScrollHitTest,
        properties->ScrollTranslation(), VisualRect(fragment->PaintOffset()),
        hit_test_opaqueness, cull_rect);
  }

  if (hit_test_opaqueness != cc::HitTestOpaqueness::kTransparent) {
    ScrollableAreaPainter(*layout_box_.GetScrollableArea())
        .RecordResizerScrollHitTestData(paint_info.context,
                                        fragment->PaintOffset());
  }
}

gfx::Rect BoxPainter::VisualRect(const PhysicalOffset& paint_offset) {
  DCHECK(!layout_box_.VisualRectRespectsVisibility() ||
         layout_box_.StyleRef().Visibility() == EVisibility::kVisible);
  PhysicalRect rect = layout_box_.SelfVisualOverflowRect();
  rect.Move(paint_offset);
  return ToEnclosingRect(rect);
}

}  // namespace blink
