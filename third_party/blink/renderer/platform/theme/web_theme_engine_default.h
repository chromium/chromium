// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_THEME_WEB_THEME_ENGINE_DEFAULT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_THEME_WEB_THEME_ENGINE_DEFAULT_H_

#include <stdint.h>

#include "build/build_config.h"
#include "third_party/blink/public/mojom/css/preferred_contrast.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/frame/color_scheme.mojom-blink-forward.h"
#include "third_party/blink/public/platform/web_theme_engine.h"
#include "ui/color/color_provider.h"

namespace blink {

class WebThemeEngineDefault : public WebThemeEngine {
 public:
  WebThemeEngineDefault();
  ~WebThemeEngineDefault() override;

  // WebThemeEngine:
  gfx::Size GetSize(WebThemeEngine::Part) override;
  void Paint(cc::PaintCanvas* canvas,
             WebThemeEngine::Part part,
             WebThemeEngine::State state,
             const gfx::Rect& rect,
             const WebThemeEngine::ExtraParams* extra_params,
             bool forced_colors,
             mojom::blink::ColorScheme color_scheme,
             mojom::blink::PreferredContrast contrast,
             const ui::ColorProvider* color_provider,
             const std::optional<SkColor>& accent_color) override;
  gfx::Insets GetScrollbarSolidColorThumbInsets(Part part) const override;
  SkColor4f GetScrollbarThumbColor(WebThemeEngine::State,
                                   const WebThemeEngine::ExtraParams*,
                                   const ui::ColorProvider*) const override;
  void GetOverlayScrollbarStyle(WebThemeEngine::ScrollbarStyle*) override;
  bool SupportsNinePatch(Part part) const override;
  gfx::Size NinePatchCanvasSize(Part part) const override;
  gfx::Rect NinePatchAperture(Part part) const override;
  std::optional<SkColor> GetAccentColor() const override;
#if BUILDFLAG(IS_WIN)
  // Caches the scrollbar metrics. These are retrieved in the browser and passed
  // to the renderer in RendererPreferences because the required Windows
  // system calls cannot be made in sandboxed renderers.
  static void cacheScrollBarMetrics(int32_t vertical_scroll_bar_width,
                                    int32_t horizontal_scroll_bar_height,
                                    int32_t vertical_arrow_bitmap_height,
                                    int32_t horizontal_arrow_bitmap_width);
#endif
  int GetPaintedScrollbarTrackInset() const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_THEME_WEB_THEME_ENGINE_DEFAULT_H_
