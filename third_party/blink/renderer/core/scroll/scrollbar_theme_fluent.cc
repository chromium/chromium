// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scroll/scrollbar_theme_fluent.h"

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/public/common/css/forced_colors.h"
#include "third_party/blink/public/platform/web_theme_engine.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollbar.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/theme/web_theme_engine_helper.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"

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

  WebThemeEngineHelper::GetNativeThemeEngine()->GetOverlayScrollbarStyle(
      &style_);
  if (WebTestSupport::IsRunningWebTest()) {
    style_.fade_out_delay = base::TimeDelta();
    style_.fade_out_duration = base::TimeDelta();
  }
}

int ScrollbarThemeFluent::ScrollbarThickness(
    float scale_from_dip,
    EScrollbarWidth scrollbar_width) const {
  return base::ClampRound(scrollbar_track_thickness_ *
                          Proportion(scrollbar_width) * scale_from_dip);
}

gfx::Rect ScrollbarThemeFluent::ThumbRect(const Scrollbar& scrollbar) const {
  gfx::Rect thumb_rect = ScrollbarTheme::ThumbRect(scrollbar);
  const int thumb_thickness =
      ThumbThickness(scrollbar.ScaleFromDIP(), scrollbar.CSSScrollbarWidth());
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

gfx::Size ScrollbarThemeFluent::ButtonSize(const Scrollbar& scrollbar) const {
  // In cases when scrollbar's frame rect is too small to contain buttons and
  // track, buttons should take all the available space.
  if (scrollbar.Orientation() == kVerticalScrollbar) {
    const int button_width = scrollbar.Width();
    const int desired_button_height = base::ClampRound(
        scrollbar_button_length_ * Proportion(scrollbar.CSSScrollbarWidth()) *
        scrollbar.ScaleFromDIP());
    const int button_height = scrollbar.Height() < 2 * desired_button_height
                                  ? scrollbar.Height() / 2
                                  : desired_button_height;
    return gfx::Size(button_width, button_height);
  } else {
    const int button_height = scrollbar.Height();
    const int desired_button_width = base::ClampRound(
        scrollbar_button_length_ * Proportion(scrollbar.CSSScrollbarWidth()) *
        scrollbar.ScaleFromDIP());
    const int button_width = scrollbar.Width() < 2 * desired_button_width
                                 ? scrollbar.Width() / 2
                                 : desired_button_width;
    return gfx::Size(button_width, button_height);
  }
}

bool ScrollbarThemeFluent::UsesOverlayScrollbars() const {
  return is_fluent_overlay_scrollbar_enabled_;
}

bool ScrollbarThemeFluent::UsesFluentScrollbars() const {
  return true;
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

ScrollbarPart ScrollbarThemeFluent::PartsToInvalidateOnThumbPositionChange(
    const Scrollbar& scrollbar,
    float old_position,
    float new_position) const {
  return ScrollbarPart::kNoPart;
}

int ScrollbarThemeFluent::ThumbThickness(
    const float scale_from_dip,
    const EScrollbarWidth scrollbar_width) const {
  // The difference between track's and thumb's thicknesses should always be
  // even to have equal thumb offsets from both sides so the thumb can remain
  // in the middle of the track. Subtract one pixel if the difference is odd.
  const int thumb_thickness =
      base::ClampRound(scrollbar_thumb_thickness_ *
                       Proportion(scrollbar_width) * scale_from_dip);
  const int scrollbar_thickness =
      ScrollbarThickness(scale_from_dip, scrollbar_width);
  return thumb_thickness - ((scrollbar_thickness - thumb_thickness) % 2);
}

void ScrollbarThemeFluent::PaintTrackBackground(GraphicsContext& context,
                                                const Scrollbar& scrollbar,
                                                const gfx::Rect& rect) {
  if (rect.IsEmpty()) {
    return;
  }
  ScrollbarThemeAura::PaintTrackBackground(
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
WebThemeEngine::ScrollbarThumbExtraParams
ScrollbarThemeFluent::BuildScrollbarThumbExtraParams(
    const Scrollbar& scrollbar) const {
  WebThemeEngine::ScrollbarThumbExtraParams scrollbar_thumb;
  if (scrollbar.ScrollbarThumbColor().has_value()) {
    scrollbar_thumb.thumb_color =
        scrollbar.ScrollbarThumbColor().value().toSkColor4f().toSkColor();
  }
  scrollbar_thumb.is_thumb_minimal_mode =
      scrollbar.IsFluentOverlayScrollbarMinimalMode();
  scrollbar_thumb.is_web_test = WebTestSupport::IsRunningWebTest();
  return scrollbar_thumb;
}

gfx::Rect ScrollbarThemeFluent::InsetTrackRect(const Scrollbar& scrollbar,
                                               gfx::Rect rect) const {
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
                                                ScrollbarPart part) const {
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

int ScrollbarThemeFluent::ScrollbarTrackInsetPx(float scale) const {
  return base::ClampRound(scale * scrollbar_track_inset_);
}

gfx::Rect ScrollbarThemeFluent::ShrinkMainThreadedMinimalModeThumbRect(
    const Scrollbar& scrollbar,
    const gfx::Rect& rect) const {
  CHECK(UsesOverlayScrollbars());
  const float idle_thickness_scale = style_.idle_thickness_scale;
  gfx::RectF thumb_rect(rect);
  if (scrollbar.Orientation() == kHorizontalScrollbar) {
    thumb_rect.set_y(rect.y() + rect.height() * (1 - idle_thickness_scale));
    thumb_rect.set_height(rect.height() * idle_thickness_scale);
  } else {
    if (!scrollbar.IsLeftSideVerticalScrollbar()) {
      thumb_rect.set_x(rect.x() + rect.width() * (1 - idle_thickness_scale));
    }
    thumb_rect.set_width(rect.width() * idle_thickness_scale);
  }
  return gfx::ToEnclosingRect(thumb_rect);
}

bool ScrollbarThemeFluent::UsesNinePatchTrackAndButtonsResource() const {
  return RuntimeEnabledFeatures::FluentScrollbarUsesNinePatchTrackEnabled();
}

}  // namespace blink
