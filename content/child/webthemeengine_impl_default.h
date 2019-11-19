// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBTHEMEENGINE_IMPL_DEFAULT_H_
#define CONTENT_CHILD_WEBTHEMEENGINE_IMPL_DEFAULT_H_

#include <stdint.h>

#include "build/build_config.h"
#include "third_party/blink/public/platform/web_theme_engine.h"

namespace content {

class WebThemeEngineDefault : public blink::WebThemeEngine {
 public:
  // WebThemeEngine methods:
  ~WebThemeEngineDefault() override;
  blink::WebSize GetSize(blink::WebThemeEngine::Part) override;
  void Paint(cc::PaintCanvas* canvas,
             blink::WebThemeEngine::Part part,
             blink::WebThemeEngine::State state,
             const blink::WebRect& rect,
             const blink::WebThemeEngine::ExtraParams* extra_params,
             blink::WebColorScheme color_scheme) override;
  void GetOverlayScrollbarStyle(
      blink::WebThemeEngine::ScrollbarStyle*) override;
  bool SupportsNinePatch(Part part) const override;
  blink::WebSize NinePatchCanvasSize(Part part) const override;
  blink::WebRect NinePatchAperture(Part part) const override;
  base::Optional<SkColor> GetSystemColor(blink::WebThemeEngine::SystemThemeColor
                                             system_theme_color) const override;
#if defined(OS_WIN)
  // Caches the scrollbar metrics. These are retrieved in the browser and passed
  // to the renderer in blink::mojom::RendererPreferences because the required
  // Windows system calls cannot be made in sandboxed renderers.
  static void cacheScrollBarMetrics(int32_t vertical_scroll_bar_width,
                                    int32_t horizontal_scroll_bar_height,
                                    int32_t vertical_arrow_bitmap_height,
                                    int32_t horizontal_arrow_bitmap_width);
#endif
  blink::ForcedColors GetForcedColors() const override;
  void SetForcedColors(const blink::ForcedColors forced_colors) override;
  blink::PreferredColorScheme PreferredColorScheme() const override;
  void SetPreferredColorScheme(
      const blink::PreferredColorScheme preferred_color_scheme) override;
};

}  // namespace content

#endif  // CONTENT_CHILD_WEBTHEMEENGINE_IMPL_DEFAULT_H_
