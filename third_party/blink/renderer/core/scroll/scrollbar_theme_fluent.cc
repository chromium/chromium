// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scroll/scrollbar_theme_fluent.h"

#include "third_party/blink/public/platform/web_theme_engine.h"
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
}

int ScrollbarThemeFluent::ScrollbarThickness(float scale_from_dip,
                                             EScrollbarWidth scrollbar_width) {
  // The difference between track's and thumb's thicknesses should always be
  // even to have equal thumb offsets from both sides so the thumb can remain
  // in the middle of the track. Add one pixel if the difference is odd.
  const int scrollbar_thickness =
      static_cast<int>(scrollbar_track_thickness_ * scale_from_dip);
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

}  // namespace blink
