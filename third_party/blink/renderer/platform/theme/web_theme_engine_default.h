// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_THEME_WEB_THEME_ENGINE_DEFAULT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_THEME_WEB_THEME_ENGINE_DEFAULT_H_

#include <stdint.h>

#include "build/build_config.h"
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
             mojom::ColorScheme color_scheme,
             const absl::optional<SkColor>& accent_color) override;
  void GetOverlayScrollbarStyle(WebThemeEngine::ScrollbarStyle*) override;
  bool SupportsNinePatch(Part part) const override;
  gfx::Size NinePatchCanvasSize(Part part) const override;
  gfx::Rect NinePatchAperture(Part part) const override;
  absl::optional<SkColor> GetSystemColor(
      WebThemeEngine::SystemThemeColor system_theme_color) const override;
  absl::optional<SkColor> GetAccentColor() const override;
#if BUILDFLAG(IS_WIN)
  // Caches the scrollbar metrics. These are retrieved in the browser and passed
  // to the renderer in RendererPreferences because the required Windows
  // system calls cannot be made in sandboxed renderers.
  static void cacheScrollBarMetrics(int32_t vertical_scroll_bar_width,
                                    int32_t horizontal_scroll_bar_height,
                                    int32_t vertical_arrow_bitmap_height,
                                    int32_t horizontal_arrow_bitmap_width);
#endif
  ForcedColors GetForcedColors() const override;
  void OverrideForcedColorsTheme(bool is_dark_theme) override;
  void SetForcedColors(const ForcedColors forced_colors) override;
  void ResetToSystemColors(
      WebThemeEngine::SystemColorInfoState system_color_info_state) override;
  WebThemeEngine::SystemColorInfoState GetSystemColorInfo() override;
  bool UpdateColorProviders(
      const ui::RendererColorMap& light_colors,
      const ui::RendererColorMap& dark_colors,
      const ui::RendererColorMap& forced_colors_map) override;
  void EmulateForcedColors(bool is_dark_theme, bool is_web_test) override;
  bool IsFluentOverlayScrollbarEnabled() const override;
  int GetPaintedScrollbarTrackInset() const override;

 protected:
  const ui::ColorProvider* GetColorProviderForPainting(
      mojom::ColorScheme color_scheme) const;

 private:
  void SetEmulateForcedColors(bool emulate_forced_colors) {
    emulate_forced_colors_ = emulate_forced_colors;
  }
  // Returns whether `part` should be affected by the accent color depending on
  // the type of part and its state.
  bool ShouldPartBeAffectedByAccentColor(
      WebThemeEngine::Part part,
      WebThemeEngine::State state,
      const WebThemeEngine::ExtraParams* extra_params) const;
  SkColor GetContrastingColorFor(mojom::ColorScheme color_scheme,
                                 WebThemeEngine::Part part,
                                 WebThemeEngine::State state) const;
  // This returns a color scheme which provides enough contrast with the custom
  // `accent_color` to make it easy to see. `light_contrasting_color` is the
  // color which is used to paint adjacent to `accent_color` from the
  // `light_color_provider_`, and `dark_contrasting_color` is the one used from
  // `dark_color_provider_`.
  mojom::ColorScheme CalculateColorSchemeForAccentColor(
      absl::optional<SkColor> accent_color,
      mojom::ColorScheme color_scheme,
      SkColor light_contrasting_color,
      SkColor dark_contrasting_color) const;
  bool emulate_forced_colors_ = false;
  // These providers are kept in sync with ColorProviders in the browser and
  // will be updated when the theme changes.
  // TODO(crbug.com/1251637): Currently these reflect the ColorProviders
  // corresponding to the global NativeTheme for web instance in the browser. We
  // should instead update blink to use ColorProviders that correspond to their
  // hosting Page.
  ui::ColorProvider light_color_provider_;
  ui::ColorProvider dark_color_provider_;
  ui::ColorProvider forced_colors_provider_;

  // This provider is used when forced color emulation is enabled, overriding
  // the light, dark or forced colors color providers.
  // TODO(samomekarajr): Remove this provider when we figure out how to change
  // the ForcedColors key from the renderer.
  ui::ColorProvider emulated_forced_colors_provider_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_THEME_WEB_THEME_ENGINE_DEFAULT_H_
