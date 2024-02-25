// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/theme/web_theme_engine_helper.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

#if BUILDFLAG(IS_ANDROID)
#include "third_party/blink/renderer/platform/theme/web_theme_engine_android.h"
#elif BUILDFLAG(IS_MAC)
#include "third_party/blink/renderer/platform/theme/web_theme_engine_mac.h"
#else
#include "third_party/blink/renderer/platform/theme/web_theme_engine_default.h"
#endif

namespace blink {

namespace {
std::unique_ptr<WebThemeEngine> CreateWebThemeEngine() {
#if BUILDFLAG(IS_ANDROID)
  return std::make_unique<WebThemeEngineAndroid>();
#elif BUILDFLAG(IS_MAC)
  return std::make_unique<WebThemeEngineMac>();
#else
  return std::make_unique<WebThemeEngineDefault>();
#endif
}

std::unique_ptr<WebThemeEngine>& ThemeEngine() {
  DEFINE_STATIC_LOCAL(std::unique_ptr<WebThemeEngine>, theme_engine,
                      {CreateWebThemeEngine()});
  return theme_engine;
}

}  // namespace

WebThemeEngine* WebThemeEngineHelper::GetNativeThemeEngine() {
  return ThemeEngine().get();
}

std::unique_ptr<WebThemeEngine>
WebThemeEngineHelper::SwapNativeThemeEngineForTesting(
    std::unique_ptr<WebThemeEngine> new_theme) {
  ThemeEngine().swap(new_theme);
  return new_theme;
}

void WebThemeEngineHelper::DidUpdateRendererPreferences(
    const blink::RendererPreferences& renderer_prefs) {
#if BUILDFLAG(IS_WIN)
  // Update Theme preferences on Windows.
  WebThemeEngineDefault::cacheScrollBarMetrics(
      renderer_prefs.vertical_scroll_bar_width_in_dips,
      renderer_prefs.horizontal_scroll_bar_height_in_dips,
      renderer_prefs.arrow_bitmap_height_vertical_scroll_bar_in_dips,
      renderer_prefs.arrow_bitmap_width_horizontal_scroll_bar_in_dips);
#endif
}

const WebThemeEngine::ScrollbarStyle&
WebThemeEngineHelper::AndroidScrollbarStyle() {
  DEFINE_STATIC_LOCAL(WebThemeEngine::ScrollbarStyle, style,
                      ({/*thumb_thickness=*/4,
                        /*scrollbar_margin=*/0,
                        /*color=*/{0.5f, 0.5f, 0.5f, 0.5f}}));
  return style;
}

}  // namespace blink
