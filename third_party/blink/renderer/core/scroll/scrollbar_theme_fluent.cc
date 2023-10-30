// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scroll/scrollbar_theme_fluent.h"

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/public/platform/web_theme_engine.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollbar.h"
#include "third_party/blink/renderer/platform/theme/web_theme_engine_helper.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

ScrollbarThemeFluent& ScrollbarThemeFluent::GetInstance() {
  DEFINE_STATIC_LOCAL(ScrollbarThemeFluent, theme, ());
  return theme;
}

ScrollbarThemeFluent::ScrollbarThemeFluent() {
  WebThemeEngine* theme_engine = WebThemeEngineHelper::GetNativeThemeEngine();
  scrollbar_thumb_thickness_ =
      theme_engine->GetSize(WebThemeEngine::kPartScrollbarVerticalThumb)
          .width();
  scrollbar_track_thickness_ =
      theme_engine->GetSize(WebThemeEngine::kPartScrollbarVerticalTrack)
          .width();
  // Web tests expect buttons to be squares with the length of the track.
  scrollbar_button_length_ =
      WebTestSupport::IsRunningWebTest()
          ? scrollbar_track_thickness_
          : theme_engine->GetSize(WebThemeEngine::kPartScrollbarUpArrow)
                .height();

  is_fluent_overlay_scrollbar_enabled_ =
      theme_engine->IsFluentOverlayScrollbarEnabled();
  if (!is_fluent_overlay_scrollbar_enabled_) {
    return;
  }
  // Hit testable invisible border around the scrollbar's track.
  scrollbar_track_inset_ = theme_engine->GetPaintedScrollbarTrackInset();
  scrollbar_track_thickness_ -= 2 * scrollbar_track_inset_;

  WebThemeEngineHelper::GetNativeThemeEngine()->GetOverlayScrollbarStyle(
      &style_);
  if (WebTestSupport::IsRunningWebTest()) {
    style_.fade_out_delay = base::TimeDelta();
    style_.fade_out_duration = base::TimeDelta();
  }
}

int ScrollbarThemeFluent::ScrollbarThickness(float scale_from_dip,
                                             EScrollbarWidth scrollbar_width) {
  // The difference between track's and thumb's thicknesses should always be
  // even to have equal thumb offsets from both sides so the thumb can remain
  // in the middle of the track. Add one pixel if the difference is odd.
  // TODO(https://crbug.com/1479169): Use ClampRound instead of ClampFloor.
  int scrollbar_thickness =
      base::ClampFloor(scrollbar_track_thickness_ * scale_from_dip);
  if (UsesOverlayScrollbars()) {
    scrollbar_thickness += 2 * ScrollbarTrackInsetPx(scale_from_dip);
  }
  return (scrollbar_thickness - ThumbThickness(scale_from_dip)) % 2 != 0
             ? scrollbar_thickness + 1
             : scrollbar_thickness;
}

gfx::Rect ScrollbarThemeFluent::ThumbRect(const Scrollbar& scrollbar) {
  gfx::Rect thumb_rect = ScrollbarTheme::ThumbRect(scrollbar);
  const int thumb_thickness = ThumbThickness(scrollbar.ScaleFromDIP());
  if (scrollbar.Orientation() == kHorizontalScrollbar) {
    thumb_rect.set_height(thumb_thickness);
  } else {
    thumb_rect.set_width(thumb_thickness);
  }

  const gfx::Rect track_rect = TrackRect(scrollbar);
  const float offset_from_viewport =
      scrollbar.Orientation() == kHorizontalScrollbar
          ? (track_rect.height() - thumb_thickness) / 2.0f
          : (track_rect.width() - thumb_thickness) / 2.0f;

  // Thumb rect position is relative to the inner edge of the scrollbar
  // track. Therefore the thumb is translated to the opposite end (towards
  // viewport border) of the track with the offset deducted.
  if (scrollbar.Orientation() == kHorizontalScrollbar) {
    thumb_rect.Offset(
        0, track_rect.height() - thumb_rect.height() - offset_from_viewport);
  } else {
    thumb_rect.Offset(
        track_rect.width() - thumb_rect.width() - offset_from_viewport, 0);
  }

  return thumb_rect;
}

int ScrollbarThemeFluent::ThumbThickness(const float scale_from_dip) const {
  return static_cast<int>(scrollbar_thumb_thickness_ * scale_from_dip);
}

gfx::Size ScrollbarThemeFluent::ButtonSize(const Scrollbar& scrollbar) const {
  // In cases when scrollbar's frame rect is too small to contain buttons and
  // track, buttons should take all the available space.
  if (scrollbar.Orientation() == kVerticalScrollbar) {
    const int button_width = scrollbar.Width();
    const int button_height_unclamped =
        scrollbar_button_length_ * scrollbar.ScaleFromDIP();
    const int button_height = scrollbar.Height() < 2 * button_height_unclamped
                                  ? base::ClampFloor(scrollbar.Height() / 2.0f)
                                  : button_height_unclamped;
    return gfx::Size(button_width, button_height);
  } else {
    const int button_height = scrollbar.Height();
    const int button_width_unclamped =
        scrollbar_button_length_ * scrollbar.ScaleFromDIP();
    const int button_width = scrollbar.Width() < 2 * button_width_unclamped
                                 ? base::ClampFloor(scrollbar.Width() / 2.0f)
                                 : button_width_unclamped;
    return gfx::Size(button_width, button_height);
  }
}

bool ScrollbarThemeFluent::UsesOverlayScrollbars() const {
  return is_fluent_overlay_scrollbar_enabled_;
}

bool ScrollbarThemeFluent::UsesFluentOverlayScrollbars() const {
  return UsesOverlayScrollbars();
}

base::TimeDelta ScrollbarThemeFluent::OverlayScrollbarFadeOutDelay() const {
  return style_.fade_out_delay;
}

base::TimeDelta ScrollbarThemeFluent::OverlayScrollbarFadeOutDuration() const {
  return style_.fade_out_duration;
}

void ScrollbarThemeFluent::PaintTrack(GraphicsContext& context,
                                      const Scrollbar& scrollbar,
                                      const gfx::Rect& rect) {
  if (rect.IsEmpty()) {
    return;
  }
  ScrollbarThemeAura::PaintTrack(
      context, scrollbar,
      UsesOverlayScrollbars() ? InsetTrackRect(scrollbar, rect) : rect);
}

void ScrollbarThemeFluent::PaintButton(GraphicsContext& context,
                                       const Scrollbar& scrollbar,
                                       const gfx::Rect& rect,
                                       ScrollbarPart part) {
  ScrollbarThemeAura::PaintButton(
      context, scrollbar,
      UsesOverlayScrollbars() ? InsetButtonRect(scrollbar, rect, part) : rect,
      part);
}

gfx::Rect ScrollbarThemeFluent::InsetTrackRect(const Scrollbar& scrollbar,
                                               gfx::Rect rect) {
  int scaled_track_inset = ScrollbarTrackInsetPx(scrollbar.ScaleFromDIP());
  if (scrollbar.Orientation() == kHorizontalScrollbar) {
    rect.Inset(gfx::Insets::TLBR(scaled_track_inset, 0, scaled_track_inset, 0));
  } else {
    rect.Inset(gfx::Insets::TLBR(0, scaled_track_inset, 0, scaled_track_inset));
  }
  return rect;
}

gfx::Rect ScrollbarThemeFluent::InsetButtonRect(const Scrollbar& scrollbar,
                                                gfx::Rect rect,
                                                ScrollbarPart part) {
  int scaled_track_inset = ScrollbarTrackInsetPx(scrollbar.ScaleFromDIP());
  // Inset all sides of the button *except* the one that borders with the
  // scrollbar track.
  if (scrollbar.Orientation() == kHorizontalScrollbar) {
    if (part == kBackButtonStartPart) {
      rect.Inset(gfx::Insets::TLBR(scaled_track_inset, scaled_track_inset,
                                   scaled_track_inset, 0));
    } else {
      rect.Inset(gfx::Insets::TLBR(scaled_track_inset, 0, scaled_track_inset,
                                   scaled_track_inset));
    }
  } else {
    if (part == kBackButtonStartPart) {
      rect.Inset(gfx::Insets::TLBR(scaled_track_inset, scaled_track_inset, 0,
                                   scaled_track_inset));
    } else {
      rect.Inset(gfx::Insets::TLBR(0, scaled_track_inset, scaled_track_inset,
                                   scaled_track_inset));
    }
  }
  return rect;
}

int ScrollbarThemeFluent::ScrollbarTrackInsetPx(float scale) {
  return base::ClampRound(scale * scrollbar_track_inset_);
}

gfx::Rect ScrollbarThemeFluent::ShrinkMainThreadedMinimalModeThumbRect(
    Scrollbar& scrollbar,
    gfx::Rect& rect) const {
  CHECK(UsesOverlayScrollbars());
  const float idle_thickness_scale = style_.idle_thickness_scale;
  if (scrollbar.Orientation() == kHorizontalScrollbar) {
    rect.set_y(rect.y() + rect.height() * (1 - idle_thickness_scale));
    rect.set_height(rect.height() * idle_thickness_scale);
  } else {
    if (!scrollbar.IsLeftSideVerticalScrollbar()) {
      rect.set_x(rect.x() + rect.width() * (1 - idle_thickness_scale));
    }
    rect.set_width(rect.width() * idle_thickness_scale);
  }
  return rect;
}

}  // namespace blink
