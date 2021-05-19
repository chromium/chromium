// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_THEME_WEB_THEME_ENGINE_DEFAULT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_THEME_WEB_THEME_ENGINE_DEFAULT_H_

#include <stdint.h>

#include "build/build_config.h"
#include "third_party/blink/public/platform/web_theme_engine.h"

namespace blink {

class WebThemeEngineDefault : public WebThemeEngine {
 public:
  // WebThemeEngine methods:
  ~WebThemeEngineDefault() override;
  gfx::Size GetSize(WebThemeEngine::Part) override;
  void Paint(cc::PaintCanvas* canvas,
             WebThemeEngine::Part part,
             WebThemeEngine::State state,
             const gfx::Rect& rect,
             const WebThemeEngine::ExtraParams* extra_params,
             mojom::ColorScheme color_scheme,
             const absl::optional<SkColor>& accent_color) override;
  void GetOverlayScrollbarStyle(WebThemeEngine::ScrollbarStyle*) override;
  bool SupportsNinePatch(Part part) const override;
  gfx::Size NinePatchCanvasSize(Part part) const override;
  gfx::Rect NinePatchAperture(Part part) const override;
  absl::optional<SkColor> GetSystemColor(
      WebThemeEngine::SystemThemeColor system_theme_color) const override;
#if defined(OS_WIN)
  // Caches the scrollbar metrics. These are retrieved in the browser and passed
  // to the renderer in RendererPreferences because the required Windows
  // system calls cannot be made in sandboxed renderers.
  static void cacheScrollBarMetrics(int32_t vertical_scroll_bar_width,
                                    int32_t horizontal_scroll_bar_height,
                                    int32_t vertical_arrow_bitmap_height,
                                    int32_t horizontal_arrow_bitmap_width);
#endif
  ForcedColors GetForcedColors() const override;
  void SetForcedColors(const ForcedColors forced_colors) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_THEME_WEB_THEME_ENGINE_DEFAULT_H_
