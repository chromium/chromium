// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/box_painter.h"

#include "base/optional.h"
#include "third_party/blink/renderer/core/layout/background_bleed_avoidance.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_table.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_foreign_object.h"
#include "third_party/blink/renderer/core/paint/background_image_geometry.h"
#include "third_party/blink/renderer/core/paint/box_decoration_data.h"
#include "third_party/blink/renderer/core/paint/box_model_object_painter.h"
#include "third_party/blink/renderer/core/paint/box_painter_base.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/nine_piece_image_painter.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/scoped_paint_state.h"
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
  PaintInfo child_info(paint_info);
  for (LayoutObject* child = layout_box_.SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (child->IsSVGForeignObject()) {
      SVGForeignObjectPainter(ToLayoutSVGForeignObject(*child))
          .PaintLayer(paint_info);
    } else {
      child->Paint(child_info);
    }
  }
}

void BoxPainter::PaintBoxDecorationBackground(const PaintInfo& paint_info,
                                              const LayoutPoint& paint_offset) {
  LayoutRect paint_rect;
  base::Optional<ScopedBoxContentsPaintState> contents_paint_state;
  if (BoxModelObjectPainter::
          IsPaintingBackgroundOfPaintContainerIntoScrollingContentsLayer(
              &layout_box_, paint_info)) {
    // For the case where we are painting the background into the scrolling
    // contents layer of a composited scroller we need to include the entire
    // overflow rect.
    paint_rect = layout_box_.PhysicalLayoutOverflowRect();

    contents_paint_state.emplace(paint_info, paint_offset, layout_box_);
    paint_rect.MoveBy(contents_paint_state->PaintOffset());

    // The background painting code assumes that the borders are part of the
    // paint_rect so we expand the paint_rect by the border size when painting
    // the background into the scrolling contents layer.
    paint_rect.Expand(layout_box_.BorderBoxOutsets());
  } else {
    paint_rect = layout_box_.BorderBoxRect();
    paint_rect.MoveBy(paint_offset);
  }

  // Paint the background if we're visible and this block has a box decoration
  // (background, border, appearance, or box shadow).
  const ComputedStyle& style = layout_box_.StyleRef();
  if (style.Visibility() == EVisibility::kVisible &&
      layout_box_.HasBoxDecorationBackground()) {
    PaintBoxDecorationBackgroundWithRect(
        contents_paint_state ? contents_paint_state->GetPaintInfo()
                             : paint_info,
        paint_rect);
  }

  if (RuntimeEnabledFeatures::PaintTouchActionRectsEnabled())
    RecordHitTestData(paint_info, paint_offset, paint_rect);
}

bool BoxPainter::BackgroundIsKnownToBeOpaque(const PaintInfo& paint_info) {
  LayoutRect bounds =
      BoxModelObjectPainter::
              IsPaintingBackgroundOfPaintContainerIntoScrollingContentsLayer(
                  &layout_box_, paint_info)
          ? layout_box_.LayoutOverflowRect()
          : layout_box_.SelfVisualOverflowRect();
  return layout_box_.BackgroundIsKnownToBeOpaqueInRect(bounds);
}

void BoxPainter::PaintBoxDecorationBackgroundWithRect(
    const PaintInfo& paint_info,
    const LayoutRect& paint_rect) {
  bool painting_overflow_contents = BoxModelObjectPainter::
      IsPaintingBackgroundOfPaintContainerIntoScrollingContentsLayer(
          &layout_box_, paint_info);
  const ComputedStyle& style = layout_box_.StyleRef();

  base::Optional<DisplayItemCacheSkipper> cache_skipper;
  // Disable cache in under-invalidation checking mode for MediaSliderPart
  // because we always paint using the latest data (buffered ranges, current
  // time and duration) which may be different from the cached data, and for
  // delayed-invalidation object because it may change before it's actually
  // invalidated. Note that we still report harmless under-invalidation of
  // non-delayed-invalidation animated background, which should be ignored.
  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() &&
      (style.Appearance() == kMediaSliderPart ||
       layout_box_.ShouldDelayFullPaintInvalidation())) {
    cache_skipper.emplace(paint_info.context);
  }

  const DisplayItemClient& display_item_client =
      painting_overflow_contents
          ? layout_box_.GetScrollableArea()
                ->GetScrollingBackgroundDisplayItemClient()
          : layout_box_;
  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, display_item_client,
          DisplayItem::kBoxDecorationBackground))
    return;

  DrawingRecorder recorder(paint_info.context, display_item_client,
                           DisplayItem::kBoxDecorationBackground);
  BoxDecorationData box_decoration_data(layout_box_);
  GraphicsContextStateSaver state_saver(paint_info.context, false);

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled() &&
      LayoutRect(EnclosingIntRect(paint_rect)) == paint_rect &&
      BackgroundIsKnownToBeOpaque(paint_info))
    recorder.SetKnownToBeOpaque();

  bool needs_end_layer = false;
  if (!painting_overflow_contents) {
    // FIXME: Should eventually give the theme control over whether the box
    // shadow should paint, since controls could have custom shadows of their
    // own.
    BoxPainterBase::PaintNormalBoxShadow(paint_info, paint_rect, style);

    if (BleedAvoidanceIsClipping(box_decoration_data.bleed_avoidance)) {
      state_saver.Save();
      FloatRoundedRect border = style.GetRoundedBorderFor(paint_rect);
      paint_info.context.ClipRoundedRect(border);

      if (box_decoration_data.bleed_avoidance == kBackgroundBleedClipLayer) {
        paint_info.context.BeginLayer();
        needs_end_layer = true;
      }
    }
  }

  // If we have a native theme appearance, paint that before painting our
  // background.  The theme will tell us whether or not we should also paint the
  // CSS background.
  IntRect snapped_paint_rect(PixelSnappedIntRect(paint_rect));
  ThemePainter& theme_painter = LayoutTheme::GetTheme().Painter();
  bool theme_painted =
      box_decoration_data.has_appearance &&
      !theme_painter.Paint(layout_box_, paint_info, snapped_paint_rect);
  bool should_paint_background =
      !theme_painted && (!paint_info.SkipRootBackground() ||
                         paint_info.PaintContainer() != &layout_box_);
  if (should_paint_background) {
    PaintBackground(paint_info, paint_rect,
                    box_decoration_data.background_color,
                    box_decoration_data.bleed_avoidance);

    if (box_decoration_data.has_appearance) {
      theme_painter.PaintDecorations(layout_box_.GetNode(),
                                     layout_box_.GetDocument(), style,
                                     paint_info, snapped_paint_rect);
    }
  }

  if (!painting_overflow_contents) {
    BoxPainterBase::PaintInsetBoxShadowWithBorderRect(paint_info, paint_rect,
                                                      style);

    // The theme will tell us whether or not we should also paint the CSS
    // border.
    if (box_decoration_data.has_border_decoration &&
        (!box_decoration_data.has_appearance ||
         (!theme_painted &&
          LayoutTheme::GetTheme().Painter().PaintBorderOnly(
              layout_box_.GetNode(), style, paint_info, snapped_paint_rect))) &&
        !(layout_box_.IsTable() &&
          ToLayoutTable(&layout_box_)->ShouldCollapseBorders())) {
      BoxPainterBase::PaintBorder(
          layout_box_, layout_box_.GetDocument(), layout_box_.GeneratingNode(),
          paint_info, paint_rect, style, box_decoration_data.bleed_avoidance);
    }
  }

  if (needs_end_layer)
    paint_info.context.EndLayer();
}

void BoxPainter::PaintBackground(const PaintInfo& paint_info,
                                 const LayoutRect& paint_rect,
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
                           const LayoutPoint& paint_offset) {
  DCHECK_EQ(PaintPhase::kMask, paint_info.phase);

  if (!layout_box_.HasMask() ||
      layout_box_.StyleRef().Visibility() != EVisibility::kVisible)
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_box_, paint_info.phase))
    return;

  DrawingRecorder recorder(paint_info.context, layout_box_, paint_info.phase);
  LayoutRect paint_rect = LayoutRect(paint_offset, layout_box_.Size());
  PaintMaskImages(paint_info, paint_rect);
}

void BoxPainter::PaintMaskImages(const PaintInfo& paint_info,
                                 const LayoutRect& paint_rect) {
  // For mask images legacy layout painting handles multi-line boxes by giving
  // the full width of the element, not the current line box, thereby clipping
  // the offending edges.
  bool include_logical_left_edge = true;
  bool include_logical_right_edge = true;

  BackgroundImageGeometry geometry(layout_box_);
  BoxModelObjectPainter painter(layout_box_);
  painter.PaintMaskImages(paint_info, paint_rect, layout_box_, geometry,
                          include_logical_left_edge,
                          include_logical_right_edge);
}

void BoxPainter::RecordHitTestData(const PaintInfo& paint_info,
                                   const LayoutPoint& paint_offset,
                                   const LayoutRect& paint_rect) {
  // TODO(sunxd): ReplacedPainter only record hit test data for svg root which
  // skips clip. We should move the conditions and ReplacedPainter's
  // RecordHitTestData here.
  if (layout_box_.IsLayoutReplaced())
    return;

  // Hit test display items are only needed for compositing. This flag is used
  // for for printing and drag images which do not need hit testing.
  if (paint_info.GetGlobalPaintFlags() & kGlobalPaintFlattenCompositingLayers)
    return;

  // If an object is not visible, it does not participate in hit testing.
  if (layout_box_.StyleRef().Visibility() != EVisibility::kVisible)
    return;

  auto touch_action = layout_box_.EffectiveWhitelistedTouchAction();
  if (touch_action == TouchAction::kTouchActionAuto)
    return;

  HitTestData::RecordHitTestRect(paint_info.context, layout_box_,
                                 HitTestRect(paint_rect, touch_action));
}

}  // namespace blink
