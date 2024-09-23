// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scroll/scrollbar_theme_overlay_mobile.h"

#include "third_party/blink/public/platform/web_theme_engine.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/scroll/scrollbar.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme_overlay_mock.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/theme/web_theme_engine_helper.h"

namespace blink {

static const WebThemeEngine::ScrollbarStyle& ScrollbarStyle() {
  return WebThemeEngineHelper::AndroidScrollbarStyle();
}

ScrollbarThemeOverlayMobile& ScrollbarThemeOverlayMobile::GetInstance() {
  // For unit tests.
  if (MockScrollbarsEnabled()) {
    DEFINE_STATIC_LOCAL(ScrollbarThemeOverlayMock, theme, ());
    return theme;
  }

  DEFINE_STATIC_LOCAL(
      ScrollbarThemeOverlayMobile, theme,
      (ScrollbarStyle().thumb_thickness, ScrollbarStyle().scrollbar_margin));
  return theme;
}

ScrollbarThemeOverlayMobile::ScrollbarThemeOverlayMobile(int thumb_thickness,
                                                         int scrollbar_margin)
    : ScrollbarThemeOverlay(thumb_thickness,
                            scrollbar_margin,
                            thumb_thickness,
                            scrollbar_margin),
      default_color_(Color::FromSkColor4f(ScrollbarStyle().color)) {}

void ScrollbarThemeOverlayMobile::PaintThumb(GraphicsContext& context,
                                             const Scrollbar& scrollbar,
                                             const gfx::Rect& rect) {
  if (!scrollbar.Enabled())
    return;

  const auto* box = scrollbar.GetLayoutBox();
  if (!box) {
    return;
  }

  if (DrawingRecorder::UseCachedDrawingIfPossible(context, scrollbar,
                                                  DisplayItem::kScrollbarThumb))
    return;

  DrawingRecorder recorder(context, scrollbar, DisplayItem::kScrollbarThumb,
                           rect);

  Color color = scrollbar.ScrollbarThumbColor().value_or(default_color_);
  AutoDarkMode auto_dark_mode(PaintAutoDarkMode(
      box->StyleRef(), DarkModeFilter::ElementRole::kBackground));
  context.FillRect(rect, color, auto_dark_mode);
}

SkColor4f ScrollbarThemeOverlayMobile::ThumbColor(
    const Scrollbar& scrollbar) const {
  return scrollbar.ScrollbarThumbColor().value_or(default_color_).toSkColor4f();
}

}  // namespace blink
