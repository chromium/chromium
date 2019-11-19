// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/box_painter.h"

#include "base/optional.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/core/layout/background_bleed_avoidance.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
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
#include "third_party/blink/renderer/platform/graphics/paint/hit_test_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_hit_test_display_item.h"

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
    if (child->IsSVGForeignObject()) {
      SVGForeignObjectPainter(ToLayoutSVGForeignObject(*child))
          .PaintLayer(paint_info);
    } else {
      child->Paint(child_info);
    }
  }
}

void BoxPainter::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  PhysicalRect paint_rect;
  const DisplayItemClient* background_client = nullptr;
  base::Optional<ScopedBoxContentsPaintState> contents_paint_state;
  bool painting_scrolling_background =
      BoxDecorationData::IsPaintingScrollingBackground(paint_info, layout_box_);
  if (painting_scrolling_background) {
    // For the case where we are painting the background into the scrolling
    // contents layer of a composited scroller we need to include the entire
    // overflow rect.
    paint_rect = layout_box_.PhysicalLayoutOverflowRect();
    contents_paint_state.emplace(paint_info, paint_offset, layout_box_);
    paint_rect.Move(contents_paint_state->PaintOffset());

    // The background painting code assumes that the borders are part of the
    // paint_rect so we expand the paint_rect by the border size when painting
    // the background into the scrolling contents layer.
    paint_rect.Expand(layout_box_.BorderBoxOutsets());

    background_client = &layout_box_.GetScrollableArea()
                             ->GetScrollingBackgroundDisplayItemClient();
  } else {
    paint_rect = layout_box_.PhysicalBorderBoxRect();
    paint_rect.Move(paint_offset);
    background_client = &layout_box_;
  }

  // Paint the background if we're visible and this block has a box decoration
  // (background, border, appearance, or box shadow).
  const ComputedStyle& style = layout_box_.StyleRef();
  if (style.Visibility() == EVisibility::kVisible &&
      layout_box_.HasBoxDecorationBackground()) {
    PaintBoxDecorationBackgroundWithRect(
        contents_paint_state ? contents_paint_state->GetPaintInfo()
                             : paint_info,
        paint_rect, *background_client);
  }

  RecordHitTestData(paint_info, paint_rect, *background_client);

  bool needs_scroll_hit_test = true;
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    // Pre-CompositeAfterPaint, there is no need to emit scroll hit test
    // display items for composited scrollers because these display items are
    // only used to create non-fast scrollable regions for non-composited
    // scrollers. With CompositeAfterPaint, we always paint the scroll hit
    // test display items but ignore the non-fast region if the scroll was
    // composited in PaintArtifactCompositor::UpdateNonFastScrollableRegions.
    if (layout_box_.HasLayer() &&
        layout_box_.Layer()->GetCompositedLayerMapping() &&
        layout_box_.Layer()->GetCompositedLayerMapping()->HasScrollingLayer()) {
      needs_scroll_hit_test = false;
    }
  }

  // Record the scroll hit test after the non-scrolling background so
  // background squashing is not affected. Hit test order would be equivalent
  // if this were immediately before the non-scrolling background.
  if (!painting_scrolling_background && needs_scroll_hit_test)
    RecordScrollHitTestData(paint_info, *background_client);
}

bool BoxPainter::BackgroundIsKnownToBeOpaque(const PaintInfo& paint_info) {
  // If the box has multiple fragments, its VisualRect is the bounding box of
  // all fragments' visual rects, which is likely to cover areas that are not
  // covered by painted background.
  if (layout_box_.FirstFragment().NextFragment())
    return false;

  PhysicalRect bounds =
      BoxDecorationData::IsPaintingScrollingBackground(paint_info, layout_box_)
          ? layout_box_.PhysicalLayoutOverflowRect()
          : layout_box_.PhysicalSelfVisualOverflowRect();
  return layout_box_.BackgroundIsKnownToBeOpaqueInRect(bounds);
}

void BoxPainter::PaintBoxDecorationBackgroundWithRect(
    const PaintInfo& paint_info,
    const PhysicalRect& paint_rect,
    const DisplayItemClient& background_client) {
  const ComputedStyle& style = layout_box_.StyleRef();

  base::Optional<DisplayItemCacheSkipper> cache_skipper;
  // Disable cache in under-invalidation checking mode for MediaSliderPart
  // because we always paint using the latest data (buffered ranges, current
  // time and duration) which may be different from the cached data, and for
  // delayed-invalidation object because it may change before it's actually
  // invalidated. Note that we still report harmless under-invalidation of
  // non-delayed-invalidation animated background, which should be ignored.
  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() &&
      (style.EffectiveAppearance() == kMediaSliderPart ||
       layout_box_.ShouldDelayFullPaintInvalidation())) {
    cache_skipper.emplace(paint_info.context);
  }

  BoxDecorationData box_decoration_data(paint_info, layout_box_);
  if (!box_decoration_data.ShouldPaint())
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, background_client,
          DisplayItem::kBoxDecorationBackground))
    return;

  DrawingRecorder recorder(paint_info.context, background_client,
                           DisplayItem::kBoxDecorationBackground);
  GraphicsContextStateSaver state_saver(paint_info.context, false);

  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
      paint_rect.EdgesOnPixelBoundaries() &&
      BackgroundIsKnownToBeOpaque(paint_info))
    recorder.SetKnownToBeOpaque();

  bool needs_end_layer = false;
  // FIXME: Should eventually give the theme control over whether the box
  // shadow should paint, since controls could have custom shadows of their
  // own.
  if (box_decoration_data.ShouldPaintShadow()) {
    BoxPainterBase::PaintNormalBoxShadow(
        paint_info, paint_rect, style, true, true,
        !box_decoration_data.ShouldPaintBackground());
  }

  if (BleedAvoidanceIsClipping(
          box_decoration_data.GetBackgroundBleedAvoidance())) {
    state_saver.Save();
    FloatRoundedRect border =
        style.GetRoundedBorderFor(paint_rect.ToLayoutRect());
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
  IntRect snapped_paint_rect(PixelSnappedIntRect(paint_rect));
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

  DrawingRecorder recorder(paint_info.context, layout_box_, paint_info.phase);
  PhysicalRect paint_rect(paint_offset, layout_box_.Size());
  PaintMaskImages(paint_info, paint_rect);
}

void BoxPainter::PaintMaskImages(const PaintInfo& paint_info,
                                 const PhysicalRect& paint_rect) {
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
                                   const PhysicalRect& paint_rect,
                                   const DisplayItemClient& background_client) {
  // Hit test display items are only needed for compositing. This flag is used
  // for for printing and drag images which do not need hit testing.
  if (paint_info.GetGlobalPaintFlags() & kGlobalPaintFlattenCompositingLayers)
    return;

  // If an object is not visible, it does not participate in hit testing.
  if (layout_box_.StyleRef().Visibility() != EVisibility::kVisible)
    return;

  auto touch_action = layout_box_.EffectiveAllowedTouchAction();
  if (touch_action == TouchAction::kTouchActionAuto)
    return;

  HitTestDisplayItem::Record(
      paint_info.context, background_client,
      HitTestRect(paint_rect.ToLayoutRect(), touch_action));
}

void BoxPainter::RecordScrollHitTestData(
    const PaintInfo& paint_info,
    const DisplayItemClient& background_client) {
  // Hit test display items are only needed for compositing. This flag is used
  // for for printing and drag images which do not need hit testing.
  if (paint_info.GetGlobalPaintFlags() & kGlobalPaintFlattenCompositingLayers)
    return;

  // If an object is not visible, it does not scroll.
  if (layout_box_.StyleRef().Visibility() != EVisibility::kVisible)
    return;

  // Only create scroll hit test data for objects that scroll.
  if (!layout_box_.GetScrollableArea() ||
      !layout_box_.GetScrollableArea()->ScrollsOverflow()) {
    return;
  }

  const auto* fragment = paint_info.FragmentToPaint(layout_box_);
  const auto* properties = fragment ? fragment->PaintProperties() : nullptr;

  // If there is an associated scroll node, emit a scroll hit test display item.
  if (properties && properties->Scroll()) {
    DCHECK(properties->ScrollTranslation());
    // The local border box properties are used instead of the contents
    // properties so that the scroll hit test is not clipped or scrolled.
    ScopedPaintChunkProperties scroll_hit_test_properties(
        paint_info.context.GetPaintController(),
        fragment->LocalBorderBoxProperties(), background_client,
        DisplayItem::kScrollHitTest);
    ScrollHitTestDisplayItem::Record(
        paint_info.context, background_client, DisplayItem::kScrollHitTest,
        properties->ScrollTranslation(), fragment->VisualRect());
  }
}

}  // namespace blink
