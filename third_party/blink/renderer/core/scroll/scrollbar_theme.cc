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

#include <optional>

#include "build/build_config.h"
#include "cc/input/scrollbar.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollbar.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme_overlay_mock.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/theme/web_theme_engine_helper.h"
#include "ui/color/color_provider.h"

#if !BUILDFLAG(IS_MAC)
#include "third_party/blink/public/platform/web_theme_engine.h"
#endif

namespace blink {

ScrollbarPart ScrollbarTheme::HitTestRootFramePosition(
    const Scrollbar& scrollbar,
    const gfx::Point& position_in_root_frame) const {
  if (!AllowsHitTest())
    return kNoPart;

  if (!scrollbar.Enabled())
    return kNoPart;

  gfx::Point test_position =
      scrollbar.ConvertFromRootFrame(position_in_root_frame);
  test_position.Offset(scrollbar.X(), scrollbar.Y());
  return HitTest(scrollbar, test_position);
}

ScrollbarPart ScrollbarTheme::HitTest(const Scrollbar& scrollbar,
                                      const gfx::Point& test_position) const {
  if (!scrollbar.FrameRect().Contains(test_position))
    return kNoPart;

  gfx::Rect track = TrackRect(scrollbar);
  if (track.Contains(test_position)) {
    gfx::Rect before_thumb_rect;
    gfx::Rect thumb_rect;
    gfx::Rect after_thumb_rect;
    SplitTrack(scrollbar, track, before_thumb_rect, thumb_rect,
               after_thumb_rect);
    if (thumb_rect.Contains(test_position))
      return kThumbPart;
    if (before_thumb_rect.Contains(test_position))
      return kBackTrackPart;
    if (after_thumb_rect.Contains(test_position))
      return kForwardTrackPart;
    return kTrackBGPart;
  }

  if (BackButtonRect(scrollbar).Contains(test_position))
    return kBackButtonStartPart;
  if (ForwardButtonRect(scrollbar).Contains(test_position))
    return kForwardButtonEndPart;

  return kScrollbarBGPart;
}

void ScrollbarTheme::PaintScrollCorner(
    GraphicsContext& context,
    const ScrollableArea& scrollable_area,
    const DisplayItemClient& display_item_client,
    const gfx::Rect& corner_rect) {
  if (corner_rect.IsEmpty())
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(context, display_item_client,
                                                  DisplayItem::kScrollCorner))
    return;

  DrawingRecorder recorder(context, display_item_client,
                           DisplayItem::kScrollCorner, corner_rect);
#if BUILDFLAG(IS_MAC)
  context.FillRect(corner_rect, Color::kWhite, AutoDarkMode::Disabled());
#else
  WebThemeEngine::ScrollbarTrackExtraParams scrollbar_track;
  const Scrollbar* scrollbar = scrollable_area.VerticalScrollbar();
  if (!scrollbar) {
    scrollbar = scrollable_area.HorizontalScrollbar();
  }
  // The scroll corner exists means at least one scrollbar exists.
  CHECK(scrollbar);
  if (scrollbar->ScrollbarTrackColor().has_value()) {
    scrollbar_track.track_color =
        scrollbar->ScrollbarTrackColor().value().toSkColor4f().toSkColor();
  }
  // TODO(crbug.com/1493088): Rounded corner of scroll corner for form controls.
  WebThemeEngine::ExtraParams extra_params(scrollbar_track);
  mojom::blink::ColorScheme color_scheme = scrollbar->UsedColorScheme();
  WebThemeEngineHelper::GetNativeThemeEngine()->Paint(
      context.Canvas(), WebThemeEngine::kPartScrollbarCorner,
      WebThemeEngine::kStateNormal, corner_rect, &extra_params, color_scheme,
      scrollbar->InForcedColorsMode(),
      scrollbar->GetColorProvider(color_scheme));
#endif
}

void ScrollbarTheme::PaintTickmarks(GraphicsContext& context,
                                    const Scrollbar& scrollbar,
                                    const gfx::Rect& rect) {
// Android paints tickmarks in the browser at FindResultBar.java.
#if !BUILDFLAG(IS_ANDROID)
  if (scrollbar.Orientation() != kVerticalScrollbar)
    return;

  if (rect.height() <= 0 || rect.width() <= 0)
    return;

  Vector<gfx::Rect> tickmarks = scrollbar.GetTickmarks();
  if (!tickmarks.size())
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          context, scrollbar, DisplayItem::kScrollbarTickmarks))
    return;

  DrawingRecorder recorder(context, scrollbar, DisplayItem::kScrollbarTickmarks,
                           rect);
  GraphicsContextStateSaver state_saver(context);
  context.SetShouldAntialias(false);

  for (const gfx::Rect& tickmark : tickmarks) {
    // Calculate how far down (in %) the tick-mark should appear.
    const float percent =
        static_cast<float>(tickmark.y()) / scrollbar.TotalSize();

    // Calculate how far down (in pixels) the tick-mark should appear.
    const int y_pos = rect.y() + (rect.height() * percent);

    gfx::RectF tick_rect(rect.x(), y_pos, rect.width(), 3);
    context.FillRect(tick_rect, Color(0xB0, 0x60, 0x00, 0xFF),
                     AutoDarkMode::Disabled());

    gfx::RectF tick_stroke(rect.x() + TickmarkBorderWidth(), y_pos + 1,
                           rect.width() - 2 * TickmarkBorderWidth(), 1);
    context.FillRect(tick_stroke, Color(0xFF, 0xDD, 0x00, 0xFF),
                     AutoDarkMode::Disabled());
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
                                  float scroll_position) const {
  if (scrollbar.Enabled()) {
    float size = scrollbar.TotalSize() - scrollbar.VisibleSize();
    // Avoid doing a floating point divide by zero and return 1 when
    // TotalSize == VisibleSize.
    if (!size)
      return 0;
    float pos = std::max(0.0f, scroll_position) *
                (TrackLength(scrollbar) - ThumbLength(scrollbar)) / size;
    return (pos < 1 && pos > 0) ? 1 : base::saturated_cast<int>(pos);
  }
  return 0;
}

int ScrollbarTheme::ThumbLength(const Scrollbar& scrollbar) const {
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

int ScrollbarTheme::TrackPosition(const Scrollbar& scrollbar) const {
  gfx::Rect constrained_track_rect =
      ConstrainTrackRectToTrackPieces(scrollbar, TrackRect(scrollbar));
  return (scrollbar.Orientation() == kHorizontalScrollbar)
             ? constrained_track_rect.x() - scrollbar.X()
             : constrained_track_rect.y() - scrollbar.Y();
}

int ScrollbarTheme::TrackLength(const Scrollbar& scrollbar) const {
  gfx::Rect constrained_track_rect =
      ConstrainTrackRectToTrackPieces(scrollbar, TrackRect(scrollbar));
  return (scrollbar.Orientation() == kHorizontalScrollbar)
             ? constrained_track_rect.width()
             : constrained_track_rect.height();
}

gfx::Rect ScrollbarTheme::ThumbRect(const Scrollbar& scrollbar) const {
  if (!HasThumb(scrollbar))
    return gfx::Rect();

  gfx::Rect track = TrackRect(scrollbar);
  gfx::Rect start_track_rect;
  gfx::Rect thumb_rect;
  gfx::Rect end_track_rect;
  SplitTrack(scrollbar, track, start_track_rect, thumb_rect, end_track_rect);

  return thumb_rect;
}

void ScrollbarTheme::SplitTrack(const Scrollbar& scrollbar,
                                const gfx::Rect& unconstrained_track_rect,
                                gfx::Rect& before_thumb_rect,
                                gfx::Rect& thumb_rect,
                                gfx::Rect& after_thumb_rect) const {
  // This function won't even get called unless we're big enough to have some
  // combination of these three rects where at least one of them is non-empty.
  gfx::Rect track_rect =
      ConstrainTrackRectToTrackPieces(scrollbar, unconstrained_track_rect);
  int thumb_pos = ThumbPosition(scrollbar);
  if (scrollbar.Orientation() == kHorizontalScrollbar) {
    thumb_rect = gfx::Rect(track_rect.x() + thumb_pos, track_rect.y(),
                           ThumbLength(scrollbar), scrollbar.Height());
    before_thumb_rect =
        gfx::Rect(track_rect.x(), track_rect.y(),
                  thumb_pos + thumb_rect.width() / 2, track_rect.height());
    after_thumb_rect = gfx::Rect(
        track_rect.x() + before_thumb_rect.width(), track_rect.y(),
        track_rect.right() - before_thumb_rect.right(), track_rect.height());
  } else {
    thumb_rect = gfx::Rect(track_rect.x(), track_rect.y() + thumb_pos,
                           scrollbar.Width(), ThumbLength(scrollbar));
    before_thumb_rect =
        gfx::Rect(track_rect.x(), track_rect.y(), track_rect.width(),
                  thumb_pos + thumb_rect.height() / 2);
    after_thumb_rect = gfx::Rect(
        track_rect.x(), track_rect.y() + before_thumb_rect.height(),
        track_rect.width(), track_rect.bottom() - before_thumb_rect.bottom());
  }
}

base::TimeDelta ScrollbarTheme::InitialAutoscrollTimerDelay() const {
  return kInitialAutoscrollTimerDelay;
}

base::TimeDelta ScrollbarTheme::AutoscrollTimerDelay() const {
  return base::Seconds(1.f / kAutoscrollMultiplier);
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

void ScrollbarTheme::PaintTrackBackgroundAndButtons(GraphicsContext& context,
                                                    const Scrollbar& scrollbar,
                                                    const gfx::Rect& rect) {
  // CustomScrollbarTheme must override this method.
  DCHECK(!scrollbar.IsCustomScrollbar());
  CHECK_EQ(rect.size(), scrollbar.FrameRect().size());
  gfx::Vector2d offset = rect.origin() - scrollbar.Location();

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          context, scrollbar, DisplayItem::kScrollbarTrackAndButtons))
    return;
  DrawingRecorder recorder(context, scrollbar,
                           DisplayItem::kScrollbarTrackAndButtons, rect);

  if (HasButtons(scrollbar)) {
    gfx::Rect back_button_rect = BackButtonRect(scrollbar);
    back_button_rect.Offset(offset);
    PaintButton(context, scrollbar, back_button_rect, kBackButtonStartPart);

    gfx::Rect forward_button_rect = ForwardButtonRect(scrollbar);
    forward_button_rect.Offset(offset);
    PaintButton(context, scrollbar, forward_button_rect, kForwardButtonEndPart);
  }

  gfx::Rect track_rect = TrackRect(scrollbar);
  track_rect.Offset(offset);
  PaintTrackBackground(context, scrollbar, track_rect);
}

void ScrollbarTheme::PaintTrackAndButtons(GraphicsContext& context,
                                          const Scrollbar& scrollbar,
                                          const gfx::Rect& rect) {
  PaintTrackBackgroundAndButtons(context, scrollbar, rect);
  if (scrollbar.HasTickmarks()) {
    gfx::Rect track_rect = TrackRect(scrollbar);
    track_rect.Offset(rect.origin() - scrollbar.Location());
    PaintTickmarks(context, scrollbar, track_rect);
  }
}

}  // namespace blink
