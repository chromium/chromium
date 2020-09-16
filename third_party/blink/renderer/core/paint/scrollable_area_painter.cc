// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/scrollable_area_painter.h"

#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/custom_scrollbar_theme.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_layer_delegate.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"
#include "third_party/blink/renderer/platform/graphics/paint/scrollbar_display_item.h"

namespace blink {

void ScrollableAreaPainter::PaintResizer(GraphicsContext& context,
                                         const IntPoint& paint_offset,
                                         const CullRect& cull_rect) {
  if (!GetScrollableArea().GetLayoutBox()->StyleRef().HasResize())
    return;

  IntRect visual_rect =
      GetScrollableArea().ResizerCornerRect(kResizerForPointer);
  if (visual_rect.IsEmpty())
    return;
  visual_rect.MoveBy(paint_offset);

  const auto& client = DisplayItemClientForCorner();
  if (const auto* resizer = GetScrollableArea().Resizer()) {
    if (!cull_rect.Intersects(visual_rect))
      return;
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
  if (GetScrollableArea().NeedsScrollCorner()) {
    GraphicsContextStateSaver state_saver(context);
    context.Clip(visual_rect);
    IntRect larger_corner = visual_rect;
    larger_corner.SetSize(
        IntSize(larger_corner.Width() + 1, larger_corner.Height() + 1));
    context.SetStrokeColor(Color(217, 217, 217));
    context.SetStrokeThickness(1.0f);
    context.SetFillColor(Color::kTransparent);
    context.DrawRect(larger_corner);
  }
}

void ScrollableAreaPainter::RecordResizerScrollHitTestData(
    GraphicsContext& context,
    const PhysicalOffset& paint_offset) {
  if (!GetScrollableArea().GetLayoutBox()->CanResize())
    return;

  IntRect touch_rect = scrollable_area_->ResizerCornerRect(kResizerForTouch);
  touch_rect.MoveBy(RoundedIntPoint(paint_offset));
  context.GetPaintController().RecordScrollHitTestData(
      DisplayItemClientForCorner(), DisplayItem::kResizerScrollHitTest, nullptr,
      touch_rect);
}

void ScrollableAreaPainter::DrawPlatformResizerImage(
    GraphicsContext& context,
    const IntRect& resizer_corner_rect) {
  IntPoint points[4];
  bool on_left = false;
  if (GetScrollableArea()
          .GetLayoutBox()
          ->ShouldPlaceBlockDirectionScrollbarOnLogicalLeft()) {
    on_left = true;
    points[0].SetX(resizer_corner_rect.X() + 1);
    points[1].SetX(resizer_corner_rect.X() + resizer_corner_rect.Width() -
                   resizer_corner_rect.Width() / 2);
    points[2].SetX(points[0].X());
    points[3].SetX(resizer_corner_rect.X() + resizer_corner_rect.Width() -
                   resizer_corner_rect.Width() * 3 / 4);
  } else {
    points[0].SetX(resizer_corner_rect.X() + resizer_corner_rect.Width() - 1);
    points[1].SetX(resizer_corner_rect.X() + resizer_corner_rect.Width() / 2);
    points[2].SetX(points[0].X());
    points[3].SetX(resizer_corner_rect.X() +
                   resizer_corner_rect.Width() * 3 / 4);
  }
  points[0].SetY(resizer_corner_rect.Y() + resizer_corner_rect.Height() / 2);
  points[1].SetY(resizer_corner_rect.Y() + resizer_corner_rect.Height() - 1);
  points[2].SetY(resizer_corner_rect.Y() +
                 resizer_corner_rect.Height() * 3 / 4);
  points[3].SetY(points[1].Y());

  PaintFlags paint_flags;
  paint_flags.setStyle(PaintFlags::kStroke_Style);
  paint_flags.setStrokeWidth(1);

  SkPathBuilder line_path;

  // Draw a dark line, to ensure contrast against a light background
  line_path.moveTo(points[0].X(), points[0].Y());
  line_path.lineTo(points[1].X(), points[1].Y());
  line_path.moveTo(points[2].X(), points[2].Y());
  line_path.lineTo(points[3].X(), points[3].Y());
  paint_flags.setColor(SkColorSetARGB(153, 0, 0, 0));
  context.DrawPath(line_path.detach(), paint_flags);

  // Draw a light line one pixel below the light line,
  // to ensure contrast against a dark background
  line_path.moveTo(points[0].X(), points[0].Y() + 1);
  line_path.lineTo(points[1].X() + (on_left ? -1 : 1), points[1].Y());
  line_path.moveTo(points[2].X(), points[2].Y() + 1);
  line_path.lineTo(points[3].X() + (on_left ? -1 : 1), points[3].Y());
  paint_flags.setColor(SkColorSetARGB(153, 255, 255, 255));
  context.DrawPath(line_path.detach(), paint_flags);
}

void ScrollableAreaPainter::PaintOverflowControls(
    const PaintInfo& paint_info,
    const IntPoint& paint_offset) {
  // Don't do anything if we have no overflow.
  const auto& box = *GetScrollableArea().GetLayoutBox();
  if (!box.IsScrollContainer() ||
      box.StyleRef().Visibility() != EVisibility::kVisible)
    return;

  // Overlay overflow controls are painted in the dedicated paint phase, and
  // normal overflow controls are painted in the background paint phase.
  if (GetScrollableArea().HasOverlayOverflowControls()) {
    if (paint_info.phase != PaintPhase::kOverlayOverflowControls)
      return;
  } else if (!ShouldPaintSelfBlockBackground(paint_info.phase)) {
    return;
  }

  GraphicsContext& context = paint_info.context;
  const auto* fragment = paint_info.FragmentToPaint(box);
  if (!fragment)
    return;

  base::Optional<ScopedPaintChunkProperties> scoped_paint_chunk_properties;
  const auto* properties = fragment->PaintProperties();
  // TODO(crbug.com/849278): Remove either the DCHECK or the if condition
  // when we figure out in what cases that the box doesn't have properties.
  DCHECK(properties);
  if (properties) {
    if (const auto* clip = properties->OverflowControlsClip()) {
      scoped_paint_chunk_properties.emplace(context.GetPaintController(), *clip,
                                            box,
                                            DisplayItem::kOverflowControls);
    }
  }

  if (GetScrollableArea().HorizontalScrollbar() &&
      !GetScrollableArea().GraphicsLayerForHorizontalScrollbar()) {
    PaintScrollbar(context, *GetScrollableArea().HorizontalScrollbar(),
                   paint_offset, paint_info.GetCullRect());
  }
  if (GetScrollableArea().VerticalScrollbar() &&
      !GetScrollableArea().GraphicsLayerForVerticalScrollbar()) {
    PaintScrollbar(context, *GetScrollableArea().VerticalScrollbar(),
                   paint_offset, paint_info.GetCullRect());
  }

  if (!GetScrollableArea().GraphicsLayerForScrollCorner()) {
    // We fill our scroll corner with white if we have a scrollbar that doesn't
    // run all the way up to the edge of the box.
    PaintScrollCorner(context, paint_offset, paint_info.GetCullRect());

    // Paint our resizer last, since it sits on top of the scroll corner.
    PaintResizer(context, paint_offset, paint_info.GetCullRect());
  }
}

void ScrollableAreaPainter::PaintScrollbar(GraphicsContext& context,
                                           Scrollbar& scrollbar,
                                           const IntPoint& paint_offset,
                                           const CullRect& cull_rect) {
  // TODO(crbug.com/1020913): We should not round paint_offset but should
  // consider subpixel accumulation when painting scrollbars.
  IntRect visual_rect = scrollbar.FrameRect();
  visual_rect.MoveBy(paint_offset);
  if (!cull_rect.Intersects(visual_rect))
    return;

  if (scrollbar.IsCustomScrollbar()) {
    scrollbar.Paint(context, paint_offset);

    // Prevent composited scroll hit test on the custom scrollbar which always
    // need main thread scrolling.
    context.GetPaintController().RecordScrollHitTestData(
        scrollbar, DisplayItem::kCustomScrollbarHitTest, nullptr, visual_rect);
    return;
  }

  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    scrollbar.Paint(context, paint_offset);
    return;
  }

  auto type = scrollbar.Orientation() == kHorizontalScrollbar
                  ? DisplayItem::kScrollbarHorizontal
                  : DisplayItem::kScrollbarVertical;
  if (context.GetPaintController().UseCachedItemIfPossible(scrollbar, type))
    return;

  const TransformPaintPropertyNode* scroll_translation = nullptr;
  // Use ScrollTranslation only if the scrollbar is scrollable, to prevent
  // non-scrollable scrollbars from being unnecessarily composited.
  if (scrollbar.Maximum()) {
    auto* properties =
        GetScrollableArea().GetLayoutBox()->FirstFragment().PaintProperties();
    DCHECK(properties);
    scroll_translation = properties->ScrollTranslation();
  }
  auto delegate = base::MakeRefCounted<ScrollbarLayerDelegate>(
      scrollbar, context.DeviceScaleFactor());
  ScrollbarDisplayItem::Record(context, scrollbar, type, delegate, visual_rect,
                               scroll_translation, scrollbar.GetElementId());
}

void ScrollableAreaPainter::PaintScrollCorner(GraphicsContext& context,
                                              const IntPoint& paint_offset,
                                              const CullRect& cull_rect) {
  IntRect visual_rect = GetScrollableArea().ScrollCornerRect();
  if (visual_rect.IsEmpty())
    return;
  visual_rect.MoveBy(paint_offset);

  if (const auto* scroll_corner = GetScrollableArea().ScrollCorner()) {
    if (!cull_rect.Intersects(visual_rect))
      return;
    CustomScrollbarTheme::PaintIntoRect(*scroll_corner, context,
                                        PhysicalRect(visual_rect));
    return;
  }

  // We don't want to paint opaque if we have overlay scrollbars, since we need
  // to see what is behind it.
  if (GetScrollableArea().HasOverlayScrollbars())
    return;

  ScrollbarTheme* theme = nullptr;

  if (GetScrollableArea().HorizontalScrollbar()) {
    theme = &GetScrollableArea().HorizontalScrollbar()->GetTheme();
  } else if (GetScrollableArea().VerticalScrollbar()) {
    theme = &GetScrollableArea().VerticalScrollbar()->GetTheme();
  } else {
    NOTREACHED();
  }

  const auto& client = DisplayItemClientForCorner();
  theme->PaintScrollCorner(context, GetScrollableArea().VerticalScrollbar(),
                           client, visual_rect,
                           GetScrollableArea().UsedColorScheme());
}

PaintLayerScrollableArea& ScrollableAreaPainter::GetScrollableArea() const {
  return *scrollable_area_;
}

const DisplayItemClient& ScrollableAreaPainter::DisplayItemClientForCorner()
    const {
  if (const auto* graphics_layer =
          GetScrollableArea().GraphicsLayerForScrollCorner())
    return *graphics_layer;
  return GetScrollableArea().GetScrollCornerDisplayItemClient();
}

}  // namespace blink
