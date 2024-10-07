// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/scrollable_area_painter.h"

#include <optional>

#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/layout/custom_scrollbar.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/custom_scrollbar_theme.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_layer_delegate.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"
#include "third_party/blink/renderer/platform/graphics/paint/scrollbar_display_item.h"

namespace blink {

namespace {

bool VisibleToHitTesting(const LayoutBox& box) {
  if (RuntimeEnabledFeatures::HitTestOpaquenessEnabled()) {
    return ObjectPainter(box).GetHitTestOpaqueness() !=
           cc::HitTestOpaqueness::kTransparent;
  }
  return box.VisibleToHitTesting();
}

}  // namespace

void ScrollableAreaPainter::PaintResizer(GraphicsContext& context,
                                         const PhysicalOffset& paint_offset,
                                         const CullRect& cull_rect) {
  const auto* box = scrollable_area_.GetLayoutBox();
  DCHECK_EQ(box->StyleRef().UsedVisibility(), EVisibility::kVisible);
  if (!box->CanResize())
    return;

  gfx::Rect visual_rect =
      scrollable_area_.ResizerCornerRect(kResizerForPointer);
  // TODO(crbug.com/40105990): We should not ignore paint_offset but should
  // consider subpixel accumulation when painting resizers.
  visual_rect.Offset(ToRoundedVector2d(paint_offset));
  if (!cull_rect.Intersects(visual_rect))
    return;

  const auto& client = scrollable_area_.GetScrollCornerDisplayItemClient();
  if (const auto* resizer = scrollable_area_.Resizer()) {
    CustomScrollbarTheme::PaintIntoRect(*resizer, context,
                                        PhysicalRect(visual_rect));
    return;
  }

  if (DrawingRecorder::UseCachedDrawingIfPossible(context, client,
                                                  DisplayItem::kResizer))
    return;

  DrawingRecorder recorder(context, client, DisplayItem::kResizer, visual_rect);

  DrawPlatformResizerImage(context, visual_rect);

  // Draw a frame around the resizer (1px grey line) if there are any scrollbars
  // present.  Clipping will exclude the right and bottom edges of this frame.
  if (scrollable_area_.NeedsScrollCorner()) {
    GraphicsContextStateSaver state_saver(context);
    context.Clip(visual_rect);
    gfx::Rect larger_corner = visual_rect;
    larger_corner.set_size(
        gfx::Size(larger_corner.width() + 1, larger_corner.height() + 1));
    context.SetStrokeColor(Color(217, 217, 217));
    context.SetStrokeThickness(1);
    gfx::RectF corner_outline(larger_corner);
    corner_outline.Inset(0.5f);
    context.StrokeRect(
        corner_outline,
        PaintAutoDarkMode(box->StyleRef(),
                          DarkModeFilter::ElementRole::kBackground));
  }
}

void ScrollableAreaPainter::RecordResizerScrollHitTestData(
    GraphicsContext& context,
    const PhysicalOffset& paint_offset) {
  const auto* box = scrollable_area_.GetLayoutBox();
  DCHECK(VisibleToHitTesting(*box));
  if (!box->CanResize())
    return;

  gfx::Rect touch_rect = scrollable_area_.ResizerCornerRect(kResizerForTouch);
  // TODO(crbug.com/40105990): We should not round paint_offset but should
  // consider subpixel accumulation when painting resizers.
  touch_rect.Offset(ToRoundedVector2d(paint_offset));
  context.GetPaintController().RecordScrollHitTestData(
      scrollable_area_.GetScrollCornerDisplayItemClient(),
      DisplayItem::kResizerScrollHitTest, nullptr, touch_rect,
      // Assume hit testing in some area may pass though.
      cc::HitTestOpaqueness::kMixed);
}

void ScrollableAreaPainter::DrawPlatformResizerImage(
    GraphicsContext& context,
    const gfx::Rect& resizer_corner_rect) {
  gfx::Point points[4];
  bool on_left = false;
  float paint_scale = scrollable_area_.ScaleFromDIP();
  int edge_offset = std::ceil(paint_scale);
  if (scrollable_area_.GetLayoutBox()
          ->ShouldPlaceBlockDirectionScrollbarOnLogicalLeft()) {
    on_left = true;
    points[0].set_x(resizer_corner_rect.x() + edge_offset);
    points[1].set_x(resizer_corner_rect.x() + resizer_corner_rect.width() -
                    resizer_corner_rect.width() / 2);
    points[2].set_x(points[0].x());
    points[3].set_x(resizer_corner_rect.x() + resizer_corner_rect.width() -
                    resizer_corner_rect.width() * 3 / 4);
  } else {
    points[0].set_x(resizer_corner_rect.x() + resizer_corner_rect.width() -
                    edge_offset);
    points[1].set_x(resizer_corner_rect.x() + resizer_corner_rect.width() / 2);
    points[2].set_x(points[0].x());
    points[3].set_x(resizer_corner_rect.x() +
                    resizer_corner_rect.width() * 3 / 4);
  }
  points[0].set_y(resizer_corner_rect.y() + resizer_corner_rect.height() / 2);
  points[1].set_y(resizer_corner_rect.y() + resizer_corner_rect.height() -
                  edge_offset);
  points[2].set_y(resizer_corner_rect.y() +
                  resizer_corner_rect.height() * 3 / 4);
  points[3].set_y(points[1].y());

  cc::PaintFlags paint_flags;
  paint_flags.setStyle(cc::PaintFlags::kStroke_Style);
  paint_flags.setStrokeWidth(std::ceil(paint_scale));

  SkPath line_path;

  AutoDarkMode auto_dark_mode(
      PaintAutoDarkMode(scrollable_area_.GetLayoutBox()->StyleRef(),
                        DarkModeFilter::ElementRole::kBackground));

  // Draw a dark line, to ensure contrast against a light background
  line_path.moveTo(points[0].x(), points[0].y());
  line_path.lineTo(points[1].x(), points[1].y());
  line_path.moveTo(points[2].x(), points[2].y());
  line_path.lineTo(points[3].x(), points[3].y());
  paint_flags.setColor(SkColorSetARGB(153, 0, 0, 0));
  context.DrawPath(line_path, paint_flags, auto_dark_mode);

  // Draw a light line one pixel below the light line,
  // to ensure contrast against a dark background
  int v_offset = std::ceil(paint_scale);
  int h_offset = on_left ? -v_offset : v_offset;
  line_path.reset();
  line_path.moveTo(points[0].x(), points[0].y() + v_offset);
  line_path.lineTo(points[1].x() + h_offset, points[1].y());
  line_path.moveTo(points[2].x(), points[2].y() + v_offset);
  line_path.lineTo(points[3].x() + h_offset, points[3].y());
  paint_flags.setColor(SkColorSetARGB(153, 255, 255, 255));
  context.DrawPath(line_path, paint_flags, auto_dark_mode);
}

bool ScrollableAreaPainter::PaintOverflowControls(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset,
    const FragmentData* fragment) {
  if (!fragment) {
    return false;
  }

  // Don't do anything if we have no overflow.
  const auto& box = *scrollable_area_.GetLayoutBox();
  CHECK(box.IsScrollContainer());
  if (box.StyleRef().UsedVisibility() != EVisibility::kVisible) {
    return false;
  }

  // Overflow controls are painted in the following paint phases:
  // - Overlay overflow controls of self-painting layers or reordered overlay
  //   overflow controls are painted in PaintPhase::kOverlayOverflowControls,
  //   called from PaintLayerPainter::PaintChildren().
  // - Non-reordered overlay overflow controls of non-self-painting-layer
  //   scrollers are painted in PaintPhase::kForeground.
  // - Non-overlay overflow controls are painted in PaintPhase::kBackground.
  if (scrollable_area_.ShouldOverflowControlsPaintAsOverlay()) {
    if (box.HasSelfPaintingLayer() ||
        box.Layer()->NeedsReorderOverlayOverflowControls()) {
      if (paint_info.phase != PaintPhase::kOverlayOverflowControls)
        return false;
    } else if (paint_info.phase != PaintPhase::kForeground) {
      return false;
    }
  } else if (!ShouldPaintSelfBlockBackground(paint_info.phase)) {
    return false;
  }

  GraphicsContext& context = paint_info.context;

  const ClipPaintPropertyNode* clip = nullptr;
  const auto* properties = fragment->PaintProperties();
  // TODO(crbug.com/849278): Remove either the DCHECK or the if condition
  // when we figure out in what cases that the box doesn't have properties.
  DCHECK(properties);
  if (properties)
    clip = properties->OverflowControlsClip();

  const TransformPaintPropertyNodeOrAlias* transform = nullptr;
  if (box.IsGlobalRootScroller()) {
    LocalFrameView* frame_view = box.GetFrameView();
    DCHECK(frame_view);
    const auto* page = frame_view->GetPage();
    const auto& viewport = page->GetVisualViewport();
    if (const auto* scrollbar_transform =
            viewport.TransformNodeForViewportScrollbars()) {
      transform = scrollbar_transform;
    }
  }

  std::optional<ScopedPaintChunkProperties> scoped_paint_chunk_properties;
  if (clip || transform) {
    PaintController& paint_controller = context.GetPaintController();
    PropertyTreeStateOrAlias modified_properties(
        paint_controller.CurrentPaintChunkProperties());
    if (clip)
      modified_properties.SetClip(*clip);
    if (transform)
      modified_properties.SetTransform(*transform);

    scoped_paint_chunk_properties.emplace(paint_controller, modified_properties,
                                          box, DisplayItem::kOverflowControls);
  }

  if (scrollable_area_.HorizontalScrollbar()) {
    PaintScrollbar(context, *scrollable_area_.HorizontalScrollbar(),
                   paint_offset, paint_info.GetCullRect());
  }
  if (scrollable_area_.VerticalScrollbar()) {
    PaintScrollbar(context, *scrollable_area_.VerticalScrollbar(), paint_offset,
                   paint_info.GetCullRect());
  }

  // We fill our scroll corner with white if we have a scrollbar that doesn't
  // run all the way up to the edge of the box.
  PaintScrollCorner(context, paint_offset, paint_info.GetCullRect());

  // Paint our resizer last, since it sits on top of the scroll corner.
  PaintResizer(context, paint_offset, paint_info.GetCullRect());

  return true;
}

void ScrollableAreaPainter::PaintScrollbar(GraphicsContext& context,
                                           Scrollbar& scrollbar,
                                           const PhysicalOffset& paint_offset,
                                           const CullRect& cull_rect) {
  // Don't paint overlay scrollbars when printing otherwise all scrollbars will
  // be visible and cover contents.
  if (scrollbar.IsOverlayScrollbar() &&
      scrollable_area_.GetLayoutBox()->GetDocument().Printing()) {
    return;
  }

  // TODO(crbug.com/40105990): We should not round paint_offset but should
  // consider subpixel accumulation when painting scrollbars.
  gfx::Rect visual_rect = scrollbar.FrameRect();
  visual_rect.Offset(ToRoundedVector2d(paint_offset));
  if (!cull_rect.Intersects(visual_rect))
    return;

  const auto* properties =
      scrollable_area_.GetLayoutBox()->FirstFragment().PaintProperties();
  CHECK(properties);
  auto type = scrollbar.Orientation() == kHorizontalScrollbar
                  ? DisplayItem::kScrollbarHorizontal
                  : DisplayItem::kScrollbarVertical;
  std::optional<ScopedPaintChunkProperties> chunk_properties;
  if (const auto* effect = scrollbar.Orientation() == kHorizontalScrollbar
                               ? properties->HorizontalScrollbarEffect()
                               : properties->VerticalScrollbarEffect()) {
    chunk_properties.emplace(context.GetPaintController(), *effect, scrollbar,
                             type);
  }

  if (scrollbar.IsCustomScrollbar()) {
    To<CustomScrollbar>(scrollbar).Paint(context, paint_offset);
    // Custom scrollbars need main thread hit testing. The hit test rect will
    // contribute to the non-fast scrollable region of the containing layer.
    if (VisibleToHitTesting(*scrollable_area_.GetLayoutBox())) {
      context.GetPaintController().RecordScrollHitTestData(
          scrollbar, DisplayItem::kScrollbarHitTest, nullptr, visual_rect,
          // Assume hit testing in some area may pass though.
          cc::HitTestOpaqueness::kMixed);
    }
  } else {
    // If the scrollbar turns out to be not composited, PaintChunksToCcLayer
    // will add its visual rect into the containing layer's non-fast scrollable
    // region.
    PaintNativeScrollbar(context, scrollbar, visual_rect);
  }
}

void ScrollableAreaPainter::PaintNativeScrollbar(GraphicsContext& context,
                                                 Scrollbar& scrollbar,
                                                 gfx::Rect visual_rect) {
  auto type = scrollbar.Orientation() == kHorizontalScrollbar
                  ? DisplayItem::kScrollbarHorizontal
                  : DisplayItem::kScrollbarVertical;

  if (context.GetPaintController().UseCachedItemIfPossible(scrollbar, type))
    return;

  const auto* properties =
      scrollable_area_.GetLayoutBox()->FirstFragment().PaintProperties();
  CHECK(properties);

  const TransformPaintPropertyNode* scroll_translation = nullptr;
  if (scrollable_area_.MayCompositeScrollbar(scrollbar)) {
    scroll_translation = properties->ScrollTranslation();
    CHECK(scroll_translation);
    CHECK(scroll_translation->ScrollNode());
  }

  cc::HitTestOpaqueness hit_test_opaqueness;
  if (scrollbar.GetTheme().AllowsHitTest()) {
    hit_test_opaqueness =
        ObjectPainter(*scrollable_area_.GetLayoutBox()).GetHitTestOpaqueness();
    if (RuntimeEnabledFeatures::HitTestOpaquenessEnabled() &&
        hit_test_opaqueness == cc::HitTestOpaqueness::kMixed) {
      // A scrollbar is always opaque to hit test if it's visible to hit test,
      // which is assumed in cc for non-solid-color scrollbar layers.
      hit_test_opaqueness = cc::HitTestOpaqueness::kOpaque;
    }
  } else {
    hit_test_opaqueness = cc::HitTestOpaqueness::kTransparent;
  }

  auto delegate = base::MakeRefCounted<ScrollbarLayerDelegate>(scrollbar);
  ScrollbarDisplayItem::Record(context, scrollbar, type, std::move(delegate),
                               visual_rect, scroll_translation,
                               scrollbar.GetElementId(), hit_test_opaqueness);
}

void ScrollableAreaPainter::PaintScrollCorner(
    GraphicsContext& context,
    const PhysicalOffset& paint_offset,
    const CullRect& cull_rect) {
  gfx::Rect visual_rect = scrollable_area_.ScrollCornerRect();
  // TODO(crbug.com/40105990): We should not round paint_offset but should
  // consider subpixel accumulation when painting scroll corners.
  visual_rect.Offset(ToRoundedVector2d(paint_offset));
  if (!cull_rect.Intersects(visual_rect))
    return;

  const auto& client = scrollable_area_.GetScrollCornerDisplayItemClient();

  // Make sure to set up the effect node before painting custom or native
  // scrollbar.
  std::optional<ScopedPaintChunkProperties> chunk_properties;
  const auto* properties =
      scrollable_area_.GetLayoutBox()->FirstFragment().PaintProperties();
  if (const auto* effect = properties->ScrollCornerEffect()) {
    chunk_properties.emplace(context.GetPaintController(), *effect, client,
                             DisplayItem::kScrollCorner);
  }

  if (const auto* scroll_corner = scrollable_area_.ScrollCorner()) {
    CustomScrollbarTheme::PaintIntoRect(*scroll_corner, context,
                                        PhysicalRect(visual_rect));
    return;
  }

  // We don't want to paint opaque if we have overlay scrollbars, since we need
  // to see what is behind it.
  if (scrollable_area_.HasOverlayScrollbars()) {
    return;
  }

  ScrollbarTheme* theme = nullptr;

  if (scrollable_area_.HorizontalScrollbar()) {
    theme = &scrollable_area_.HorizontalScrollbar()->GetTheme();
  } else if (scrollable_area_.VerticalScrollbar()) {
    theme = &scrollable_area_.VerticalScrollbar()->GetTheme();
  } else {
    NOTREACHED_IN_MIGRATION();
  }

  theme->PaintScrollCorner(context, scrollable_area_, client, visual_rect);
}

}  // namespace blink
