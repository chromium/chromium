// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/inline_flow_box_painter.h"

#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_api_shim.h"
#include "third_party/blink/renderer/core/layout/line/inline_flow_box.h"
#include "third_party/blink/renderer/core/layout/line/root_inline_box.h"
#include "third_party/blink/renderer/core/paint/background_image_geometry.h"
#include "third_party/blink/renderer/core/paint/box_model_object_painter.h"
#include "third_party/blink/renderer/core/paint/box_painter_base.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

namespace {

inline Node* GetNode(const LayoutObject* box_model) {
  Node* node = nullptr;
  for (const LayoutObject* obj = box_model; obj && !node; obj = obj->Parent())
    node = obj->GeneratingNode();
  return node;
}

inline const LayoutBoxModelObject* GetBoxModelObject(
    const InlineFlowBox& flow_box) {
  return To<LayoutBoxModelObject>(
      LineLayoutAPIShim::LayoutObjectFrom(flow_box.BoxModelObject()));
}

}  // anonymous namespace

InlineFlowBoxPainter::InlineFlowBoxPainter(const InlineFlowBox& flow_box)
    : InlineBoxPainterBase(
          *GetBoxModelObject(flow_box),
          &GetBoxModelObject(flow_box)->GetDocument(),
          GetNode(GetBoxModelObject(flow_box)),
          flow_box.GetLineLayoutItem().StyleRef(),
          flow_box.GetLineLayoutItem().StyleRef(flow_box.IsFirstLineStyle())),
      inline_flow_box_(flow_box) {}

void InlineFlowBoxPainter::Paint(const PaintInfo& paint_info,
                                 const PhysicalOffset& paint_offset,
                                 const LayoutUnit line_top,
                                 const LayoutUnit line_bottom) {
  DCHECK(!ShouldPaintSelfOutline(paint_info.phase) &&
         !ShouldPaintDescendantOutlines(paint_info.phase));

  if (!paint_info.IntersectsCullRect(
          inline_flow_box_.PhysicalVisualOverflowRect(line_top, line_bottom),
          paint_offset))
    return;

  if (paint_info.phase == PaintPhase::kMask) {
    PaintMask(paint_info, paint_offset);
    return;
  }

  if (paint_info.phase == PaintPhase::kForeground) {
    // Paint our background, border and box-shadow.
    PaintBackgroundBorderShadow(paint_info, paint_offset);
  }

  // Paint our children.
  PaintInfo child_info(paint_info);
  for (InlineBox* curr = inline_flow_box_.FirstChild(); curr;
       curr = curr->NextOnLine()) {
    if (curr->GetLineLayoutItem().IsText() ||
        !curr->BoxModelObject().HasSelfPaintingLayer())
      curr->Paint(child_info, paint_offset, line_top, line_bottom);
  }
}

PhysicalRect InlineFlowBoxPainter::PaintRectForImageStrip(
    const PhysicalRect& paint_rect,
    TextDirection direction) const {
  // We have a fill/border/mask image that spans multiple lines.
  // We need to adjust the offset by the width of all previous lines.
  // Think of background painting on inlines as though you had one long line, a
  // single continuous strip. Even though that strip has been broken up across
  // multiple lines, you still paint it as though you had one single line. This
  // means each line has to pick up the background where the previous line left
  // off.
  LayoutUnit logical_offset_on_line;
  LayoutUnit total_logical_width;
  if (direction == TextDirection::kLtr) {
    for (const InlineFlowBox* curr = inline_flow_box_.PrevForSameLayoutObject();
         curr; curr = curr->PrevForSameLayoutObject())
      logical_offset_on_line += curr->LogicalWidth();
    total_logical_width = logical_offset_on_line;
    for (const InlineFlowBox* curr = &inline_flow_box_; curr;
         curr = curr->NextForSameLayoutObject())
      total_logical_width += curr->LogicalWidth();
  } else {
    for (const InlineFlowBox* curr = inline_flow_box_.NextForSameLayoutObject();
         curr; curr = curr->NextForSameLayoutObject())
      logical_offset_on_line += curr->LogicalWidth();
    total_logical_width = logical_offset_on_line;
    for (const InlineFlowBox* curr = &inline_flow_box_; curr;
         curr = curr->PrevForSameLayoutObject())
      total_logical_width += curr->LogicalWidth();
  }
  LayoutUnit strip_x =
      paint_rect.X() -
      (inline_flow_box_.IsHorizontal() ? logical_offset_on_line : LayoutUnit());
  LayoutUnit strip_y =
      paint_rect.Y() -
      (inline_flow_box_.IsHorizontal() ? LayoutUnit() : logical_offset_on_line);
  LayoutUnit strip_width = inline_flow_box_.IsHorizontal() ? total_logical_width
                                                           : paint_rect.Width();
  LayoutUnit strip_height = inline_flow_box_.IsHorizontal()
                                ? paint_rect.Height()
                                : total_logical_width;
  return PhysicalRect(strip_x, strip_y, strip_width, strip_height);
}

InlineBoxPainterBase::BorderPaintingType
InlineFlowBoxPainter::GetBorderPaintType(
    const PhysicalRect& adjusted_frame_rect,
    gfx::Rect& adjusted_clip_rect,
    bool object_has_multiple_boxes) const {
  adjusted_clip_rect = ToPixelSnappedRect(adjusted_frame_rect);
  if (!inline_flow_box_.Parent() || !style_.HasBorderDecoration())
    return kDontPaintBorders;
  const NinePieceImage& border_image = style_.BorderImage();
  StyleImage* border_image_source = border_image.GetImage();
  bool has_border_image =
      border_image_source && border_image_source->CanRender();
  if (has_border_image && !border_image_source->IsLoaded())
    return kDontPaintBorders;

  // The simple case is where we either have no border image or we are the
  // only box for this object.  In those cases only a single call to draw is
  // required.
  if (!has_border_image || !object_has_multiple_boxes)
    return kPaintBordersWithoutClip;

  // We have a border image that spans multiple lines.
  adjusted_clip_rect = ToPixelSnappedRect(
      ClipRectForNinePieceImageStrip(style_, inline_flow_box_.SidesToInclude(),
                                     border_image, adjusted_frame_rect));
  return kPaintBordersWithClip;
}

void InlineFlowBoxPainter::PaintBackgroundBorderShadow(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  DCHECK(paint_info.phase == PaintPhase::kForeground);

  if (inline_flow_box_.GetLineLayoutItem().StyleRef().Visibility() !=
      EVisibility::kVisible)
    return;

  RecordHitTestData(paint_info, paint_offset);
  RecordRegionCaptureData(paint_info, paint_offset);

  // You can use p::first-line to specify a background. If so, the root line
  // boxes for a line may actually have to paint a background.
  LayoutObject* inline_flow_box_layout_object =
      LineLayoutAPIShim::LayoutObjectFrom(inline_flow_box_.GetLineLayoutItem());
  bool should_paint_box_decoration_background;
  if (inline_flow_box_.Parent())
    should_paint_box_decoration_background =
        inline_flow_box_layout_object->HasBoxDecorationBackground();
  else
    should_paint_box_decoration_background =
        inline_flow_box_.IsFirstLineStyle() && line_style_ != style_;

  if (!should_paint_box_decoration_background)
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, inline_flow_box_,
          DisplayItem::kBoxDecorationBackground))
    return;

  PhysicalRect paint_rect = AdjustedFrameRect(paint_offset);
  DrawingRecorder recorder(paint_info.context, inline_flow_box_,
                           DisplayItem::kBoxDecorationBackground,
                           VisualRect(paint_rect));

  bool object_has_multiple_boxes = inline_flow_box_.PrevForSameLayoutObject() ||
                                   inline_flow_box_.NextForSameLayoutObject();
  const auto& box_model = *To<LayoutBoxModelObject>(
      LineLayoutAPIShim::LayoutObjectFrom(inline_flow_box_.BoxModelObject()));
  BackgroundImageGeometry geometry(box_model);
  BoxModelObjectPainter box_painter(box_model, &inline_flow_box_);
  PaintBoxDecorationBackground(box_painter, paint_info, paint_offset,
                               paint_rect, geometry, object_has_multiple_boxes,
                               inline_flow_box_.SidesToInclude());
}

void InlineFlowBoxPainter::PaintMask(const PaintInfo& paint_info,
                                     const PhysicalOffset& paint_offset) {
  DCHECK_EQ(PaintPhase::kMask, paint_info.phase);
  if (!style_.HasMask() || style_.Visibility() != EVisibility::kVisible)
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, inline_flow_box_, paint_info.phase))
    return;

  PhysicalRect paint_rect = AdjustedFrameRect(paint_offset);
  DrawingRecorder recorder(paint_info.context, inline_flow_box_,
                           paint_info.phase, VisualRect(paint_rect));

  bool object_has_multiple_boxes = inline_flow_box_.PrevForSameLayoutObject() ||
                                   inline_flow_box_.NextForSameLayoutObject();
  const auto& box_model = *To<LayoutBoxModelObject>(
      LineLayoutAPIShim::LayoutObjectFrom(inline_flow_box_.BoxModelObject()));
  BackgroundImageGeometry geometry(box_model);
  BoxModelObjectPainter box_painter(box_model, &inline_flow_box_);
  InlineBoxPainterBase::PaintMask(box_painter, paint_info, paint_rect, geometry,
                                  object_has_multiple_boxes,
                                  inline_flow_box_.SidesToInclude());
}

// This method should not be needed. See crbug.com/530659.
LayoutRect InlineFlowBoxPainter::FrameRectClampedToLineTopAndBottomIfNeeded()
    const {
  LayoutRect rect(inline_flow_box_.FrameRect());

  bool no_quirks_mode =
      inline_flow_box_.GetLineLayoutItem().GetDocument().InNoQuirksMode();
  if (!no_quirks_mode && !inline_flow_box_.HasTextChildren() &&
      !(inline_flow_box_.DescendantsHaveSameLineHeightAndBaseline() &&
        inline_flow_box_.HasTextDescendants())) {
    const RootInlineBox& root_box = inline_flow_box_.Root();
    LayoutUnit logical_top =
        inline_flow_box_.IsHorizontal() ? rect.Y() : rect.X();
    LayoutUnit logical_height =
        inline_flow_box_.IsHorizontal() ? rect.Height() : rect.Width();
    LayoutUnit bottom =
        std::min(root_box.LineBottom(), logical_top + logical_height);
    logical_top = std::max(root_box.LineTop(), logical_top);
    logical_height = bottom - logical_top;
    if (inline_flow_box_.IsHorizontal()) {
      rect.SetY(logical_top);
      rect.SetHeight(logical_height);
    } else {
      rect.SetX(logical_top);
      rect.SetWidth(logical_height);
    }
    if (rect != inline_flow_box_.FrameRect()) {
      UseCounter::Count(inline_flow_box_.GetLineLayoutItem().GetDocument(),
                        WebFeature::kQuirkyLineBoxBackgroundSize);
    }
  }
  return rect;
}

PhysicalRect InlineFlowBoxPainter::AdjustedFrameRect(
    const PhysicalOffset& paint_offset) const {
  LayoutRect frame_rect = FrameRectClampedToLineTopAndBottomIfNeeded();
  LayoutRect local_rect = frame_rect;
  inline_flow_box_.FlipForWritingMode(local_rect);
  PhysicalOffset adjusted_paint_offset =
      paint_offset + PhysicalOffsetToBeNoop(local_rect.Location());
  return PhysicalRect(adjusted_paint_offset, frame_rect.Size());
}

gfx::Rect InlineFlowBoxPainter::VisualRect(
    const PhysicalRect& adjusted_frame_rect) const {
  PhysicalRect visual_rect = adjusted_frame_rect;
  const auto& style = inline_flow_box_.GetLineLayoutItem().StyleRef();
  if (style.HasVisualOverflowingEffect())
    visual_rect.Expand(style.BoxDecorationOutsets());
  return ToEnclosingRect(visual_rect);
}

void InlineFlowBoxPainter::RecordHitTestData(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  LayoutObject* layout_object =
      LineLayoutAPIShim::LayoutObjectFrom(inline_flow_box_.GetLineLayoutItem());

  DCHECK_EQ(layout_object->StyleRef().Visibility(), EVisibility::kVisible);

  paint_info.context.GetPaintController().RecordHitTestData(
      inline_flow_box_, ToPixelSnappedRect(AdjustedFrameRect(paint_offset)),
      layout_object->EffectiveAllowedTouchAction(),
      layout_object->InsideBlockingWheelEventHandler());
}

void InlineFlowBoxPainter::RecordRegionCaptureData(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  LayoutObject* layout_object =
      LineLayoutAPIShim::LayoutObjectFrom(inline_flow_box_.GetLineLayoutItem());

  const Element* element = DynamicTo<Element>(layout_object->GetNode());
  if (element) {
    const RegionCaptureCropId* crop_id = element->GetRegionCaptureCropId();
    if (crop_id) {
      paint_info.context.GetPaintController().RecordRegionCaptureData(
          inline_flow_box_, *crop_id,
          ToPixelSnappedRect(AdjustedFrameRect(paint_offset)));
    }
  }
}

void InlineFlowBoxPainter::PaintNormalBoxShadow(
    const PaintInfo& info,
    const ComputedStyle& s,
    const PhysicalRect& paint_rect) {
  BoxPainterBase::PaintNormalBoxShadow(info, paint_rect, s,
                                       inline_flow_box_.SidesToInclude());
}

void InlineFlowBoxPainter::PaintInsetBoxShadow(const PaintInfo& info,
                                               const ComputedStyle& s,
                                               const PhysicalRect& paint_rect) {
  BoxPainterBase::PaintInsetBoxShadowWithBorderRect(
      info, paint_rect, s, inline_flow_box_.SidesToInclude());
}

}  // namespace blink
