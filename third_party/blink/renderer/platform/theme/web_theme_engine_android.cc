// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/theme/web_theme_engine_android.h"

#include "third_party/blink/public/platform/web_theme_engine.h"
#include "third_party/blink/renderer/platform/theme/web_theme_engine_helper.h"

namespace blink {

WebThemeEngineAndroid::~WebThemeEngineAndroid() = default;

gfx::Size WebThemeEngineAndroid::GetSize(WebThemeEngine::Part part) {
  switch (part) {
    case WebThemeEngine::kPartScrollbarHorizontalThumb:
    case WebThemeEngine::kPartScrollbarVerticalThumb: {
      // Minimum length for scrollbar thumb is the scrollbar thickness.
      ScrollbarStyle style;
      GetOverlayScrollbarStyle(&style);
      int scrollbarThickness = style.thumb_thickness + style.scrollbar_margin;
      return gfx::Size(scrollbarThickness, scrollbarThickness);
    }
    default:
      return WebThemeEngineDefault::GetSize(part);
  }
}

void WebThemeEngineAndroid::GetOverlayScrollbarStyle(ScrollbarStyle* style) {
  *style = WebThemeEngineHelper::AndroidScrollbarStyle();
}


}  // namespace blink
