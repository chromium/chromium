// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/win/web_font_rendering.h"

#include "third_party/blink/public/platform/web_font_rendering_client.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"

namespace blink {

// static
void WebFontRendering::SetSkiaFontManager(sk_sp<SkFontMgr> font_mgr) {
  FontCache::SetFontManager(std::move(font_mgr));
}

// static
void WebFontRendering::SetFontPrewarmer(WebFontPrewarmer* prewarmer) {
  FontCache::SetFontPrewarmer(prewarmer);
}

// static
WebFontPrewarmer* WebFontRendering::GetFontPrewarmer() {
  return FontCache::GetFontPrewarmer();
}

// static
void WebFontRendering::SetFontRenderingClient(
    WebFontRenderingClient* rendering_client) {
  FontCache::SetFontPrewarmer(rendering_client);
  // TODO(yosin): Call `FontThreadPool::SetFontRenderingClient()`.
}

// static
void WebFontRendering::SetMenuFontMetrics(const WebString& family_name,
                                          int32_t font_height) {
  FontCache::SetMenuFontMetrics(family_name, font_height);
}

// static
void WebFontRendering::SetSmallCaptionFontMetrics(const WebString& family_name,
                                                  int32_t font_height) {
  FontCache::SetSmallCaptionFontMetrics(family_name, font_height);
}

// static
void WebFontRendering::SetStatusFontMetrics(const WebString& family_name,
                                            int32_t font_height) {
  FontCache::SetStatusFontMetrics(family_name, font_height);
}

// static
void WebFontRendering::SetAntialiasedTextEnabled(bool enabled) {
  FontCache::SetAntialiasedTextEnabled(enabled);
}

// static
void WebFontRendering::SetLCDTextEnabled(bool enabled) {
  FontCache::SetLCDTextEnabled(enabled);
}

}  // namespace blink
