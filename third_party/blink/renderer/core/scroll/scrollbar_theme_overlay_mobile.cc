// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scroll/scrollbar_theme_overlay_mobile.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_theme_engine.h"
#include "third_party/blink/renderer/core/scroll/scrollbar.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme_overlay_mock.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

static const WebThemeEngine::ScrollbarStyle& ScrollbarStyle() {
  static bool initialized = false;
  DEFINE_STATIC_LOCAL(WebThemeEngine::ScrollbarStyle, style,
                      (WebThemeEngine::ScrollbarStyle{3, 4, 0x80808080}));
  if (!initialized) {
    // During device emulation, the chrome WebThemeEngine implementation may not
    // be the mobile theme which can provide the overlay scrollbar styles.
    // In the case the following call will do nothing and we'll use the default
    // styles specified above.
    Platform::Current()->ThemeEngine()->GetOverlayScrollbarStyle(&style);
    DCHECK(style.thumb_thickness);
    initialized = true;
  }
  return style;
}

ScrollbarThemeOverlayMobile& ScrollbarThemeOverlayMobile::GetInstance() {
  // For unit tests which don't have Platform::Current()->ThemeEngine().
  if (MockScrollbarsEnabled()) {
    DEFINE_STATIC_LOCAL(ScrollbarThemeOverlayMock, theme, ());
    return theme;
  }

  DEFINE_STATIC_LOCAL(
      ScrollbarThemeOverlayMobile, theme,
      (ScrollbarStyle().thumb_thickness, ScrollbarStyle().scrollbar_margin,
       ScrollbarStyle().color));
  return theme;
}

ScrollbarThemeOverlayMobile::ScrollbarThemeOverlayMobile(int thumb_thickness,
                                                         int scrollbar_margin,
                                                         Color color)
    : ScrollbarThemeOverlay(thumb_thickness, scrollbar_margin), color_(color) {}

void ScrollbarThemeOverlayMobile::PaintThumb(GraphicsContext& context,
                                             const Scrollbar& scrollbar,
                                             const IntRect& rect) {
  if (!scrollbar.Enabled())
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(context, scrollbar,
                                                  DisplayItem::kScrollbarThumb))
    return;

  DrawingRecorder recorder(context, scrollbar, DisplayItem::kScrollbarThumb);
  context.FillRect(rect, color_);
}

}  // namespace blink
