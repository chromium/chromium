/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/paint/custom_scrollbar_theme.h"

#include "third_party/blink/renderer/core/layout/custom_scrollbar.h"
#include "third_party/blink/renderer/core/layout/layout_custom_scrollbar_part.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/scroll/scrollbar.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

CustomScrollbarTheme* CustomScrollbarTheme::GetCustomScrollbarTheme() {
  DEFINE_STATIC_LOCAL(CustomScrollbarTheme, theme, ());
  return &theme;
}

void CustomScrollbarTheme::ButtonSizesAlongTrackAxis(const Scrollbar& scrollbar,
                                                     int& before_size,
                                                     int& after_size) {
  IntRect first_button = BackButtonRect(scrollbar, kBackButtonStartPart);
  IntRect second_button = ForwardButtonRect(scrollbar, kForwardButtonStartPart);
  IntRect third_button = BackButtonRect(scrollbar, kBackButtonEndPart);
  IntRect fourth_button = ForwardButtonRect(scrollbar, kForwardButtonEndPart);
  if (scrollbar.Orientation() == kHorizontalScrollbar) {
    before_size = first_button.Width() + second_button.Width();
    after_size = third_button.Width() + fourth_button.Width();
  } else {
    before_size = first_button.Height() + second_button.Height();
    after_size = third_button.Height() + fourth_button.Height();
  }
}

bool CustomScrollbarTheme::HasButtons(const Scrollbar& scrollbar) {
  int start_size;
  int end_size;
  ButtonSizesAlongTrackAxis(scrollbar, start_size, end_size);
  return (start_size + end_size) <=
         (scrollbar.Orientation() == kHorizontalScrollbar ? scrollbar.Width()
                                                          : scrollbar.Height());
}

bool CustomScrollbarTheme::HasThumb(const Scrollbar& scrollbar) {
  return TrackLength(scrollbar) - ThumbLength(scrollbar) >= 0;
}

int CustomScrollbarTheme::MinimumThumbLength(const Scrollbar& scrollbar) {
  return To<CustomScrollbar>(scrollbar).MinimumThumbLength();
}

IntRect CustomScrollbarTheme::BackButtonRect(const Scrollbar& scrollbar,
                                             ScrollbarPart part_type) {
  return To<CustomScrollbar>(scrollbar).ButtonRect(part_type);
}

IntRect CustomScrollbarTheme::ForwardButtonRect(const Scrollbar& scrollbar,
                                                ScrollbarPart part_type) {
  return To<CustomScrollbar>(scrollbar).ButtonRect(part_type);
}

IntRect CustomScrollbarTheme::TrackRect(const Scrollbar& scrollbar) {
  if (!HasButtons(scrollbar))
    return scrollbar.FrameRect();

  int start_length;
  int end_length;
  ButtonSizesAlongTrackAxis(scrollbar, start_length, end_length);

  return To<CustomScrollbar>(scrollbar).TrackRect(start_length, end_length);
}

IntRect CustomScrollbarTheme::ConstrainTrackRectToTrackPieces(
    const Scrollbar& scrollbar,
    const IntRect& rect) {
  IntRect back_rect = To<CustomScrollbar>(scrollbar).TrackPieceRectWithMargins(
      kBackTrackPart, rect);
  IntRect forward_rect =
      To<CustomScrollbar>(scrollbar).TrackPieceRectWithMargins(
          kForwardTrackPart, rect);
  IntRect result = rect;
  if (scrollbar.Orientation() == kHorizontalScrollbar) {
    result.SetX(back_rect.X());
    result.SetWidth(forward_rect.MaxX() - back_rect.X());
  } else {
    result.SetY(back_rect.Y());
    result.SetHeight(forward_rect.MaxY() - back_rect.Y());
  }
  return result;
}

void CustomScrollbarTheme::PaintScrollCorner(
    GraphicsContext& context,
    const Scrollbar* vertical_scrollbar,
    const DisplayItemClient& display_item_client,
    const IntRect& corner_rect,
    WebColorScheme color_scheme) {
  if (DrawingRecorder::UseCachedDrawingIfPossible(context, display_item_client,
                                                  DisplayItem::kScrollCorner))
    return;

  DrawingRecorder recorder(context, display_item_client,
                           DisplayItem::kScrollCorner);
  // FIXME: Implement.
  context.FillRect(corner_rect, Color::kWhite);
}

void CustomScrollbarTheme::PaintTrackAndButtons(GraphicsContext& context,
                                                const Scrollbar& scrollbar,
                                                const IntPoint& offset) {
  // Custom scrollbars are always painted in their original coordinate space,
  // i.e. the space of Scrollbar::FrameRect() and ScrollbarTheme::XXXRect()
  // which is |context|'s current space.
  DCHECK_EQ(offset, IntPoint());

  PaintPart(context, scrollbar, scrollbar.FrameRect(), kScrollbarBGPart);

  if (HasButtons(scrollbar)) {
    PaintButton(context, scrollbar,
                BackButtonRect(scrollbar, kBackButtonStartPart),
                kBackButtonStartPart);
    PaintButton(context, scrollbar,
                BackButtonRect(scrollbar, kBackButtonEndPart),
                kBackButtonEndPart);
    PaintButton(context, scrollbar,
                ForwardButtonRect(scrollbar, kForwardButtonStartPart),
                kForwardButtonStartPart);
    PaintButton(context, scrollbar,
                ForwardButtonRect(scrollbar, kForwardButtonEndPart),
                kForwardButtonEndPart);
  }

  IntRect track_rect = TrackRect(scrollbar);
  PaintPart(context, scrollbar, track_rect, kTrackBGPart);

  if (HasThumb(scrollbar)) {
    IntRect start_track_rect;
    IntRect thumb_rect;
    IntRect end_track_rect;
    SplitTrack(scrollbar, track_rect, start_track_rect, thumb_rect,
               end_track_rect);
    PaintPart(context, scrollbar, start_track_rect, kBackTrackPart);
    PaintPart(context, scrollbar, end_track_rect, kForwardTrackPart);
  }
}

void CustomScrollbarTheme::PaintButton(GraphicsContext& context,
                                       const Scrollbar& scrollbar,
                                       const IntRect& rect,
                                       ScrollbarPart part) {
  PaintPart(context, scrollbar, rect, part);
}

void CustomScrollbarTheme::PaintThumb(GraphicsContext& context,
                                      const Scrollbar& scrollbar,
                                      const IntRect& rect) {
  PaintPart(context, scrollbar, rect, kThumbPart);
}

void CustomScrollbarTheme::PaintTickmarks(GraphicsContext& context,
                                          const Scrollbar& scrollbar,
                                          const IntRect& rect) {
  GetTheme().PaintTickmarks(context, scrollbar, rect);
}

void CustomScrollbarTheme::PaintIntoRect(
    const LayoutCustomScrollbarPart& layout_custom_scrollbar_part,
    GraphicsContext& graphics_context,
    const PhysicalOffset& paint_offset,
    const PhysicalRect& rect,
    const CustomScrollbar* scrollbar) {
  // Make sure our dimensions match the rect.
  // TODO(crbug.com/856802): Setting these is a bad layering violation!
  // Move these into layout stage.
  const_cast<LayoutCustomScrollbarPart&>(layout_custom_scrollbar_part)
      .SetLocation((rect.offset - paint_offset).ToLayoutPoint());
  const_cast<LayoutCustomScrollbarPart&>(layout_custom_scrollbar_part)
      .SetWidth(rect.size.width);
  const_cast<LayoutCustomScrollbarPart&>(layout_custom_scrollbar_part)
      .SetHeight(rect.size.height);
  // TODO(crbug.com/856802): Move this into PaintPropertyTreeBuilder.
  layout_custom_scrollbar_part.GetMutableForPainting()
      .FirstFragment()
      .SetPaintOffset((scrollbar ? PhysicalOffset(scrollbar->Location())
                                 : PhysicalOffset()) +
                      layout_custom_scrollbar_part.PhysicalLocation());

  PaintInfo paint_info(graphics_context, PixelSnappedIntRect(rect),
                       PaintPhase::kForeground, kGlobalPaintNormalPhase,
                       kPaintLayerNoFlag);
  ObjectPainter(layout_custom_scrollbar_part)
      .PaintAllPhasesAtomically(paint_info);
}

void CustomScrollbarTheme::PaintPart(GraphicsContext& context,
                                     const Scrollbar& scrollbar,
                                     const IntRect& rect,
                                     ScrollbarPart part) {
  const auto& custom_scrollbar = To<CustomScrollbar>(scrollbar);
  const auto* part_layout_object = custom_scrollbar.GetPart(part);
  if (!part_layout_object)
    return;
  PaintIntoRect(*part_layout_object, context,
                PhysicalOffset(custom_scrollbar.Location()), PhysicalRect(rect),
                &custom_scrollbar);
}

}  // namespace blink
