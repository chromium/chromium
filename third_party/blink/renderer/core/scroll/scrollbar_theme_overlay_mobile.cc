// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scroll/scrollbar_theme_overlay_mobile.h"

#include "third_party/blink/public/platform/web_theme_engine.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollbar.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme_overlay_mock.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/theme/web_theme_engine_helper.h"

namespace blink {

static const WebThemeEngine::ScrollbarStyle& ScrollbarStyle() {
  static bool initialized = false;
  DEFINE_STATIC_LOCAL(WebThemeEngine::ScrollbarStyle, style,
                      (WebThemeEngine::ScrollbarStyle{3, 4, 3, 4, 0x80808080}));
  if (!initialized) {
    // During device emulation, the chrome WebThemeEngine implementation may not
    // be the mobile theme which can provide the overlay scrollbar styles.
    // In the case the following call will do nothing and we'll use the default
    // styles specified above.
    WebThemeEngineHelper::GetNativeThemeEngine()->GetOverlayScrollbarStyle(
        &style);
    DCHECK(style.thumb_thickness);
    initialized = true;
  }
  return style;
}

ScrollbarThemeOverlayMobile& ScrollbarThemeOverlayMobile::GetInstance() {
  // For unit tests.
  if (MockScrollbarsEnabled()) {
    DEFINE_STATIC_LOCAL(ScrollbarThemeOverlayMock, theme, ());
    return theme;
  }

  DEFINE_STATIC_LOCAL(
      ScrollbarThemeOverlayMobile, theme,
      (ScrollbarStyle().thumb_thickness, ScrollbarStyle().scrollbar_margin,
       ScrollbarStyle().thumb_thickness_thin,
       ScrollbarStyle().scrollbar_margin_thin,
       Color::FromSkColor(ScrollbarStyle().color)));
  return theme;
}

ScrollbarThemeOverlayMobile::ScrollbarThemeOverlayMobile(
    int thumb_thickness_default,
    int scrollbar_margin_default,
    int thumb_thickness_thin,
    int scrollbar_margin_thin,
    Color color)
    : ScrollbarThemeOverlay(thumb_thickness_default,
                            scrollbar_margin_default,
                            thumb_thickness_thin,
                            scrollbar_margin_thin),
      color_(color) {}

void ScrollbarThemeOverlayMobile::PaintThumb(GraphicsContext& context,
                                             const Scrollbar& scrollbar,
                                             const gfx::Rect& rect) {
  if (!scrollbar.Enabled())
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(context, scrollbar,
                                                  DisplayItem::kScrollbarThumb))
    return;

  DrawingRecorder recorder(context, scrollbar, DisplayItem::kScrollbarThumb,
                           rect);

  const auto* box = scrollbar.GetScrollableArea()->GetLayoutBox();
  AutoDarkMode auto_dark_mode(PaintAutoDarkMode(
      box->StyleRef(), DarkModeFilter::ElementRole::kBackground));
  context.FillRect(rect, color_, auto_dark_mode);
}

}  // namespace blink
