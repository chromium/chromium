// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/box_painter.h"

#include "base/unguessable_token.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/core/layout/background_bleed_avoidance.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/paint/background_image_geometry.h"
#include "third_party/blink/renderer/core/paint/box_decoration_data.h"
#include "third_party/blink/renderer/core/paint/box_model_object_painter.h"
#include "third_party/blink/renderer/core/paint/box_painter_base.h"
#include "third_party/blink/renderer/core/paint/nine_piece_image_painter.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/rounded_border_geometry.h"
#include "third_party/blink/renderer/core/paint/scoped_paint_state.h"
#include "third_party/blink/renderer/core/paint/scrollable_area_painter.h"
#include "third_party/blink/renderer/core/paint/svg_foreign_object_painter.h"
#include "third_party/blink/renderer/core/paint/theme_painter.h"
#include "third_party/blink/renderer/platform/geometry/layout_point.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_cache_skipper.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"

namespace blink {

void BoxPainter::Paint(const PaintInfo& paint_info) {
  // Default implementation. Just pass paint through to the children.
  ScopedPaintState paint_state(layout_box_, paint_info);
  PaintChildren(paint_state.GetPaintInfo());
}

void BoxPainter::PaintChildren(const PaintInfo& paint_info) {
  if (paint_info.DescendantPaintingBlocked())
    return;

  PaintInfo child_info(paint_info);
  for (LayoutObject* child = layout_box_.SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (child->IsSVGForeignObjectIncludingNG()) {
      SVGForeignObjectPainter(To<LayoutBlockFlow>(*child))
          .PaintLayer(paint_info);
    } else {
      child->Paint(child_info);
    }
  }
}

void BoxPainter::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  if (layout_box_.StyleRef().Visibility() != EVisibility::kVisible)
    return;

  PhysicalRect paint_rect;
  const DisplayItemClient* background_client = nullptr;
  absl::optional<ScopedBoxContentsPaintState> contents_paint_state;
  bool painting_background_in_contents_space =
      paint_info.IsPaintingBackgroundInContentsSpace();
  gfx::Rect visual_rect;
  if (painting_background_in_contents_space) {
    // For the case where we are painting the background in the contents space,
    // we need to include the entire overflow rect.
    paint_rect = layout_box_.PhysicalLayoutOverflowRect();
    contents_paint_state.emplace(paint_info, paint_offset, layout_box_);
    paint_rect.Move(contents_paint_state->PaintOffset());

    // The background painting code assumes that the borders are part of the
    // paint_rect so we expand the paint_rect by the border size when painting
    // the background into the scrolling contents layer.
    paint_rect.Expand(layout_box_.BorderBoxOutsets());

    background_client = &layout_box_.GetScrollableArea()
                             ->GetScrollingBackgroundDisplayItemClient();
    visual_rect =
        layout_box_.GetScrollableArea()->ScrollingBackgroundVisualRect(
            paint_offset);
  } else {
    paint_rect = layout_box_.PhysicalBorderBoxRect();
    paint_rect.Move(paint_offset);
    background_client = &layout_box_;
    visual_rect = VisualRect(paint_offset);
  }

  // Paint the background if we're visible and this block has a box decoration
  // (background, border, appearance, or box shadow).
  const ComputedStyle& style = layout_box_.StyleRef();
  if (style.Visibility() == EVisibility::kVisible &&
      layout_box_.HasBoxDecorationBackground()) {
    PaintBoxDecorationBackgroundWithRect(
        contents_paint_state ? contents_paint_state->GetPaintInfo()
                             : paint_info,
        visual_rect, paint_rect, *background_client);
  }

  RecordHitTestData(paint_info, paint_rect, *background_client);
  RecordRegionCaptureData(paint_info, paint_rect, *background_client);

  // Record the scroll hit test after the non-scrolling background so
  // background squashing is not affected. Hit test order would be equivalent
  // if this were immediately before the non-scrolling background.
  if (!painting_background_in_contents_space)
    RecordScrollHitTestData(paint_info, *background_client);
}

void BoxPainter::PaintBoxDecorationBackgroundWithRect(
    const PaintInfo& paint_info,
    const gfx::Rect& visual_rect,
    const PhysicalRect& paint_rect,
    const DisplayItemClient& background_client) {
  const ComputedStyle& style = layout_box_.StyleRef();

  absl::optional<DisplayItemCacheSkipper> cache_skipper;
  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() &&
      BoxPainterBase::ShouldSkipPaintUnderInvalidationChecking(layout_box_))
    cache_skipper.emplace(paint_info.context);

  BoxDecorationData box_decoration_data(paint_info, layout_box_);
  if (!box_decoration_data.ShouldPaint())
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, background_client,
          DisplayItem::kBoxDecorationBackground))
    return;

  DrawingRecorder recorder(paint_info.context, background_client,
                           DisplayItem::kBoxDecorationBackground, visual_rect);
  GraphicsContextStateSaver state_saver(paint_info.context, false);

  bool needs_end_layer = false;
  // FIXME: Should eventually give the theme control over whether the box
  // shadow should paint, since controls could have custom shadows of their
  // own.
  if (box_decoration_data.ShouldPaintShadow()) {
    BoxPainterBase::PaintNormalBoxShadow(
        paint_info, paint_rect, style, PhysicalBoxSides(),
        !box_decoration_data.ShouldPaintBackground());
  }

  if (BleedAvoidanceIsClipping(
          box_decoration_data.GetBackgroundBleedAvoidance())) {
    state_saver.Save();
    FloatRoundedRect border =
        RoundedBorderGeometry::PixelSnappedRoundedBorder(style, paint_rect);
    paint_info.context.ClipRoundedRect(border);

    if (box_decoration_data.GetBackgroundBleedAvoidance() ==
        kBackgroundBleedClipLayer) {
      paint_info.context.BeginLayer();
      needs_end_layer = true;
    }
  }

  // If we have a native theme appearance, paint that before painting our
  // background.  The theme will tell us whether or not we should also paint the
  // CSS background.
  gfx::Rect snapped_paint_rect = ToPixelSnappedRect(paint_rect);
  ThemePainter& theme_painter = LayoutTheme::GetTheme().Painter();
  bool theme_painted =
      box_decoration_data.HasAppearance() &&
      !theme_painter.Paint(layout_box_, paint_info, snapped_paint_rect);
  if (!theme_painted) {
    if (box_decoration_data.ShouldPaintBackground()) {
      PaintBackground(paint_info, paint_rect,
                      box_decoration_data.BackgroundColor(),
                      box_decoration_data.GetBackgroundBleedAvoidance());
    }
    if (box_decoration_data.HasAppearance()) {
      theme_painter.PaintDecorations(layout_box_.GetNode(),
                                     layout_box_.GetDocument(), style,
                                     paint_info, snapped_paint_rect);
    }
  }

  if (box_decoration_data.ShouldPaintShadow()) {
    BoxPainterBase::PaintInsetBoxShadowWithBorderRect(paint_info, paint_rect,
                                                      style);
  }

  // The theme will tell us whether or not we should also paint the CSS
  // border.
  if (box_decoration_data.ShouldPaintBorder()) {
    if (!theme_painted) {
      theme_painted =
          box_decoration_data.HasAppearance() &&
          !theme_painter.PaintBorderOnly(layout_box_.GetNode(), style,
                                         paint_info, snapped_paint_rect);
    }
    if (!theme_painted) {
      BoxPainterBase::PaintBorder(
          layout_box_, layout_box_.GetDocument(), layout_box_.GeneratingNode(),
          paint_info, paint_rect, style,
          box_decoration_data.GetBackgroundBleedAvoidance());
    }
  }

  if (needs_end_layer)
    paint_info.context.EndLayer();
}

void BoxPainter::PaintBackground(const PaintInfo& paint_info,
                                 const PhysicalRect& paint_rect,
                                 const Color& background_color,
                                 BackgroundBleedAvoidance bleed_avoidance) {
  if (layout_box_.BackgroundTransfersToView())
    return;
  if (layout_box_.BackgroundIsKnownToBeObscured())
    return;
  BackgroundImageGeometry geometry(layout_box_);
  BoxModelObjectPainter box_model_painter(layout_box_);
  box_model_painter.PaintFillLayers(paint_info, background_color,
                                    layout_box_.StyleRef().BackgroundLayers(),
                                    paint_rect, geometry, bleed_avoidance);
}

void BoxPainter::PaintMask(const PaintInfo& paint_info,
                           const PhysicalOffset& paint_offset) {
  DCHECK_EQ(PaintPhase::kMask, paint_info.phase);

  if (!layout_box_.HasMask() ||
      layout_box_.StyleRef().Visibility() != EVisibility::kVisible)
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_box_, paint_info.phase))
    return;

  PhysicalRect paint_rect(paint_offset, layout_box_.Size());
  BoxDrawingRecorder recorder(paint_info.context, layout_box_, paint_info.phase,
                              paint_offset);
  PaintMaskImages(paint_info, paint_rect);
}

void BoxPainter::PaintMaskImages(const PaintInfo& paint_info,
                                 const PhysicalRect& paint_rect) {
  // For mask images legacy layout painting handles multi-line boxes by giving
  // the full width of the element, not the current line box, thereby clipping
  // the offending edges.
  BackgroundImageGeometry geometry(layout_box_);
  BoxModelObjectPainter painter(layout_box_);
  painter.PaintMaskImages(paint_info, paint_rect, layout_box_, geometry,
                          PhysicalBoxSides());
}

void BoxPainter::RecordHitTestData(const PaintInfo& paint_info,
                                   const PhysicalRect& paint_rect,
                                   const DisplayItemClient& background_client) {
  if (paint_info.IsPaintingBackgroundInContentsSpace() &&
      layout_box_.EffectiveAllowedTouchAction() == TouchAction::kAuto &&
      !layout_box_.InsideBlockingWheelEventHandler()) {
    return;
  }

  // Hit test data are only needed for compositing. This flag is used for for
  // printing and drag images which do not need hit testing.
  if (paint_info.ShouldOmitCompositingInfo())
    return;

  // If an object is not visible, it does not participate in hit testing.
  if (layout_box_.StyleRef().Visibility() != EVisibility::kVisible)
    return;

  if (!paint_info.FragmentToPaint(layout_box_))
    return;

  paint_info.context.GetPaintController().RecordHitTestData(
      background_client, ToPixelSnappedRect(paint_rect),
      layout_box_.EffectiveAllowedTouchAction(),
      layout_box_.InsideBlockingWheelEventHandler());
}

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
    const DisplayItemClient& background_client) {
  // Scroll hit test data are only needed for compositing. This flag is used for
  // printing and drag images which do not need hit testing.
  if (paint_info.ShouldOmitCompositingInfo())
    return;

  // If an object is not visible, it does not scroll.
  if (layout_box_.StyleRef().Visibility() != EVisibility::kVisible)
    return;

  if (!layout_box_.GetScrollableArea())
    return;

  const auto* fragment = paint_info.FragmentToPaint(layout_box_);
  if (!fragment)
    return;

  // If there is an associated scroll node, emit scroll hit test data.
  const auto* properties = fragment->PaintProperties();
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
    paint_controller.RecordScrollHitTestData(
        background_client, DisplayItem::kScrollHitTest,
        properties->ScrollTranslation(), VisualRect(fragment->PaintOffset()));
  }

  ScrollableAreaPainter(*layout_box_.GetScrollableArea())
      .RecordResizerScrollHitTestData(paint_info.context,
                                      fragment->PaintOffset());
}

gfx::Rect BoxPainter::VisualRect(const PhysicalOffset& paint_offset) {
  DCHECK(!layout_box_.VisualRectRespectsVisibility() ||
         layout_box_.StyleRef().Visibility() == EVisibility::kVisible);
  PhysicalRect rect = layout_box_.PhysicalSelfVisualOverflowRect();
  rect.Move(paint_offset);
  return ToEnclosingRect(rect);
}

}  // namespace blink
