/*
 * Copyright (C) 2011 Apple Inc. All Rights Reserved.
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

#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"

#include "base/optional.h"
#include "build/build_config.h"
#include "cc/input/scrollbar.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_mouse_event.h"
#include "third_party/blink/public/platform/web_point.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/renderer/core/scroll/scrollbar.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme_overlay_mock.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

#if !defined(OS_MACOSX)
#include "third_party/blink/public/platform/web_theme_engine.h"
#endif

namespace blink {

void ScrollbarTheme::Paint(const Scrollbar& scrollbar,
                           GraphicsContext& graphics_context) {
  PaintTrackButtonsTickmarks(graphics_context, scrollbar, IntPoint());

  IntRect thumb_rect = ThumbRect(scrollbar);
  if (HasThumb(scrollbar))
    PaintThumbWithOpacity(graphics_context, scrollbar, thumb_rect);
}

ScrollbarPart ScrollbarTheme::HitTest(const Scrollbar& scrollbar,
                                      const IntPoint& position_in_root_frame) {
  ScrollbarPart result = kNoPart;
  if (!scrollbar.Enabled())
    return result;

  IntPoint test_position =
      scrollbar.ConvertFromRootFrame(position_in_root_frame);
  test_position.Move(scrollbar.X(), scrollbar.Y());

  if (!scrollbar.FrameRect().Contains(test_position))
    return kNoPart;

  result = kScrollbarBGPart;

  IntRect track = TrackRect(scrollbar);
  if (track.Contains(test_position)) {
    IntRect before_thumb_rect;
    IntRect thumb_rect;
    IntRect after_thumb_rect;
    SplitTrack(scrollbar, track, before_thumb_rect, thumb_rect,
               after_thumb_rect);
    if (thumb_rect.Contains(test_position))
      result = kThumbPart;
    else if (before_thumb_rect.Contains(test_position))
      result = kBackTrackPart;
    else if (after_thumb_rect.Contains(test_position))
      result = kForwardTrackPart;
    else
      result = kTrackBGPart;
  } else if (BackButtonRect(scrollbar, kBackButtonStartPart)
                 .Contains(test_position)) {
    result = kBackButtonStartPart;
  } else if (BackButtonRect(scrollbar, kBackButtonEndPart)
                 .Contains(test_position)) {
    result = kBackButtonEndPart;
  } else if (ForwardButtonRect(scrollbar, kForwardButtonStartPart)
                 .Contains(test_position)) {
    result = kForwardButtonStartPart;
  } else if (ForwardButtonRect(scrollbar, kForwardButtonEndPart)
                 .Contains(test_position)) {
    result = kForwardButtonEndPart;
  }
  return result;
}

void ScrollbarTheme::PaintScrollCorner(
    GraphicsContext& context,
    const Scrollbar* vertical_scrollbar,
    const DisplayItemClient& display_item_client,
    const IntRect& corner_rect,
    WebColorScheme color_scheme) {
  if (corner_rect.IsEmpty())
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(context, display_item_client,
                                                  DisplayItem::kScrollCorner))
    return;

  DrawingRecorder recorder(context, display_item_client,
                           DisplayItem::kScrollCorner);
#if defined(OS_MACOSX)
  context.FillRect(corner_rect, Color::kWhite);
#else
  Platform::Current()->ThemeEngine()->Paint(
      context.Canvas(), WebThemeEngine::kPartScrollbarCorner,
      WebThemeEngine::kStateNormal, WebRect(corner_rect), nullptr,
      color_scheme);
#endif
}

void ScrollbarTheme::PaintTickmarks(GraphicsContext& context,
                                    const Scrollbar& scrollbar,
                                    const IntRect& rect) {
// Android paints tickmarks in the browser at FindResultBar.java.
#if !defined(OS_ANDROID)
  if (scrollbar.Orientation() != kVerticalScrollbar)
    return;

  if (rect.Height() <= 0 || rect.Width() <= 0)
    return;

  Vector<IntRect> tickmarks = scrollbar.GetTickmarks();
  if (!tickmarks.size())
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          context, scrollbar, DisplayItem::kScrollbarTickmarks))
    return;

  DrawingRecorder recorder(context, scrollbar,
                           DisplayItem::kScrollbarTickmarks);
  GraphicsContextStateSaver state_saver(context);
  context.SetShouldAntialias(false);

  for (const IntRect& tickmark : tickmarks) {
    // Calculate how far down (in %) the tick-mark should appear.
    const float percent =
        static_cast<float>(tickmark.Y()) / scrollbar.TotalSize();

    // Calculate how far down (in pixels) the tick-mark should appear.
    const int y_pos = rect.Y() + (rect.Height() * percent);

    FloatRect tick_rect(rect.X(), y_pos, rect.Width(), 3);
    context.FillRect(tick_rect, Color(0xCC, 0xAA, 0x00, 0xFF));

    FloatRect tick_stroke(rect.X() + TickmarkBorderWidth(), y_pos + 1,
                          rect.Width() - 2 * TickmarkBorderWidth(), 1);
    context.FillRect(tick_stroke, Color(0xFF, 0xDD, 0x00, 0xFF));
  }
#endif
}

base::TimeDelta ScrollbarTheme::OverlayScrollbarFadeOutDelay() const {
  // On Mac, fading is controlled by the painting code in ScrollAnimatorMac.
  return base::TimeDelta();
}

base::TimeDelta ScrollbarTheme::OverlayScrollbarFadeOutDuration() const {
  // On Mac, fading is controlled by the painting code in ScrollAnimatorMac.
  return base::TimeDelta();
}

int ScrollbarTheme::ThumbPosition(const Scrollbar& scrollbar,
                                  float scroll_position) {
  if (scrollbar.Enabled()) {
    float size = scrollbar.TotalSize() - scrollbar.VisibleSize();
    // Avoid doing a floating point divide by zero and return 1 when
    // usedTotalSize == visibleSize.
    if (!size)
      return 0;
    float pos = std::max(0.0f, scroll_position) *
                (TrackLength(scrollbar) - ThumbLength(scrollbar)) / size;
    return (pos < 1 && pos > 0) ? 1 : pos;
  }
  return 0;
}

int ScrollbarTheme::ThumbLength(const Scrollbar& scrollbar) {
  if (!scrollbar.Enabled())
    return 0;

  float overhang = fabsf(scrollbar.ElasticOverscroll());
  float proportion = 0.0f;
  float total_size = scrollbar.TotalSize();
  if (total_size > 0.0f) {
    proportion = (scrollbar.VisibleSize() - overhang) / total_size;
  }
  int track_len = TrackLength(scrollbar);
  int length = round(proportion * track_len);
  length = std::max(length, MinimumThumbLength(scrollbar));
  if (length > track_len)
    length = track_len;  // Once the thumb is below the track length,
                         // it fills the track.
  return length;
}

int ScrollbarTheme::TrackPosition(const Scrollbar& scrollbar) {
  IntRect constrained_track_rect =
      ConstrainTrackRectToTrackPieces(scrollbar, TrackRect(scrollbar));
  return (scrollbar.Orientation() == kHorizontalScrollbar)
             ? constrained_track_rect.X() - scrollbar.X()
             : constrained_track_rect.Y() - scrollbar.Y();
}

int ScrollbarTheme::TrackLength(const Scrollbar& scrollbar) {
  IntRect constrained_track_rect =
      ConstrainTrackRectToTrackPieces(scrollbar, TrackRect(scrollbar));
  return (scrollbar.Orientation() == kHorizontalScrollbar)
             ? constrained_track_rect.Width()
             : constrained_track_rect.Height();
}

IntRect ScrollbarTheme::ThumbRect(const Scrollbar& scrollbar) {
  if (!HasThumb(scrollbar))
    return IntRect();

  IntRect track = TrackRect(scrollbar);
  IntRect start_track_rect;
  IntRect thumb_rect;
  IntRect end_track_rect;
  SplitTrack(scrollbar, track, start_track_rect, thumb_rect, end_track_rect);

  return thumb_rect;
}

int ScrollbarTheme::ThumbThickness(const Scrollbar& scrollbar) {
  IntRect track = TrackRect(scrollbar);
  return scrollbar.Orientation() == kHorizontalScrollbar ? track.Height()
                                                         : track.Width();
}

void ScrollbarTheme::SplitTrack(const Scrollbar& scrollbar,
                                const IntRect& unconstrained_track_rect,
                                IntRect& before_thumb_rect,
                                IntRect& thumb_rect,
                                IntRect& after_thumb_rect) {
  // This function won't even get called unless we're big enough to have some
  // combination of these three rects where at least one of them is non-empty.
  IntRect track_rect =
      ConstrainTrackRectToTrackPieces(scrollbar, unconstrained_track_rect);
  int thumb_pos = ThumbPosition(scrollbar);
  if (scrollbar.Orientation() == kHorizontalScrollbar) {
    thumb_rect = IntRect(track_rect.X() + thumb_pos, track_rect.Y(),
                         ThumbLength(scrollbar), scrollbar.Height());
    before_thumb_rect =
        IntRect(track_rect.X(), track_rect.Y(),
                thumb_pos + thumb_rect.Width() / 2, track_rect.Height());
    after_thumb_rect = IntRect(
        track_rect.X() + before_thumb_rect.Width(), track_rect.Y(),
        track_rect.MaxX() - before_thumb_rect.MaxX(), track_rect.Height());
  } else {
    thumb_rect = IntRect(track_rect.X(), track_rect.Y() + thumb_pos,
                         scrollbar.Width(), ThumbLength(scrollbar));
    before_thumb_rect =
        IntRect(track_rect.X(), track_rect.Y(), track_rect.Width(),
                thumb_pos + thumb_rect.Height() / 2);
    after_thumb_rect = IntRect(
        track_rect.X(), track_rect.Y() + before_thumb_rect.Height(),
        track_rect.Width(), track_rect.MaxY() - before_thumb_rect.MaxY());
  }
}

base::TimeDelta ScrollbarTheme::InitialAutoscrollTimerDelay() {
  return kInitialAutoscrollTimerDelay;
}

base::TimeDelta ScrollbarTheme::AutoscrollTimerDelay() {
  return base::TimeDelta::FromSecondsD(1.f / kAutoscrollMultiplier);
}

ScrollbarTheme& ScrollbarTheme::GetTheme() {
  if (MockScrollbarsEnabled()) {
    // We only support mock overlay scrollbars.
    DCHECK(OverlayScrollbarsEnabled());
    DEFINE_STATIC_LOCAL(ScrollbarThemeOverlayMock, overlay_mock_theme, ());
    return overlay_mock_theme;
  }
  return NativeTheme();
}

void ScrollbarTheme::PaintTrackAndButtons(GraphicsContext& context,
                                          const Scrollbar& scrollbar,
                                          const IntPoint& offset) {
  // CustomScrollbarTheme must override this method.
  DCHECK(!scrollbar.IsCustomScrollbar());

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          context, scrollbar, DisplayItem::kScrollbarTrackAndButtons))
    return;
  DrawingRecorder recorder(context, scrollbar,
                           DisplayItem::kScrollbarTrackAndButtons);

  if (HasButtons(scrollbar)) {
    IntRect back_button_rect = BackButtonRect(scrollbar, kBackButtonStartPart);
    back_button_rect.MoveBy(offset);
    PaintButton(context, scrollbar, back_button_rect, kBackButtonStartPart);

    IntRect forward_button_rect =
        ForwardButtonRect(scrollbar, kForwardButtonEndPart);
    forward_button_rect.MoveBy(offset);
    PaintButton(context, scrollbar, forward_button_rect, kForwardButtonEndPart);

    // Non-custom scrollbars don't have kBackButtonEndPart and
    // kForwardButtonStartPart.
    DCHECK(BackButtonRect(scrollbar, kBackButtonEndPart).IsEmpty());
    DCHECK(ForwardButtonRect(scrollbar, kForwardButtonStartPart).IsEmpty());
  }

  IntRect track_rect = TrackRect(scrollbar);
  track_rect.MoveBy(offset);
  PaintTrack(context, scrollbar, track_rect);
}

void ScrollbarTheme::PaintTrackButtonsTickmarks(GraphicsContext& context,
                                                const Scrollbar& scrollbar,
                                                const IntPoint& offset) {
  PaintTrackAndButtons(context, scrollbar, offset);
  if (scrollbar.HasTickmarks()) {
    IntRect track_rect = TrackRect(scrollbar);
    track_rect.MoveBy(offset);
    PaintTickmarks(context, scrollbar, track_rect);
  }
}

}  // namespace blink
