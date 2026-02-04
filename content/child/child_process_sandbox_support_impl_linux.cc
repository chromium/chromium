// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/child_process_sandbox_support_impl_linux.h"

#include <stddef.h>

#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "components/services/font/public/cpp/font_loader.h"
#include "components/services/font/public/mojom/font_service.mojom.h"
#include "third_party/blink/public/platform/web_font_render_style.h"
#include "third_party/blink/public/platform/web_string.h"
#include "ui/gfx/font_fallback_linux.h"

namespace content {

WebSandboxSupportLinux::WebSandboxSupportLinux(
    sk_sp<font_service::FontLoader> font_loader)
    : font_loader_(font_loader) {}

WebSandboxSupportLinux::~WebSandboxSupportLinux() = default;

bool WebSandboxSupportLinux::GetFallbackFontForCharacter(
    blink::WebUChar32 character,
    const char* preferred_locale,
    gfx::FallbackFontData* fallback_font) {
  TRACE_EVENT0("fonts", "WebSandboxSupportLinux::GetFallbackFontForCharacter");

  bool success = false;

  {
    base::AutoLock lock(lock_);
    const auto iter = unicode_font_families_.find(character);
    if (iter != unicode_font_families_.end()) {
      *fallback_font = iter->second;
      success = true;
    }
  }

  if (!success) {
    font_service::mojom::FontIdentityPtr font_identity;
    bool is_bold = false;
    bool is_italic = false;
    std::string family_name;

    if (font_loader_->FallbackFontForCharacter(character, preferred_locale,
                                               &font_identity, &family_name,
                                               &is_bold, &is_italic)) {
      fallback_font->name = family_name;
      fallback_font->fontconfig_interface_id = font_identity->id;
      fallback_font->filepath = font_identity->filepath;
      fallback_font->ttc_index = font_identity->ttc_index;
      fallback_font->is_bold = is_bold;
      fallback_font->is_italic = is_italic;

      success = true;

      base::AutoLock lock(lock_);
      unicode_font_families_.emplace(character, *fallback_font);
    }
  }

  UMA_HISTOGRAM_BOOLEAN(
      "Renderer.WebSandboxSupportLinux.GetFallbackFontForCharacter", success);

  return success;
}

bool WebSandboxSupportLinux::MatchFontByPostscriptNameOrFullFontName(
    const char* font_unique_name,
    gfx::FallbackFontData* fallback_font) {
  TRACE_EVENT0(
      "fonts",
      "WebSandboxSupportLinux::MatchFontByPostscriptNameOrFullFontName");

  font_service::mojom::FontIdentityPtr font_identity;
  if (!font_loader_->MatchFontByPostscriptNameOrFullFontName(font_unique_name,
                                                             &font_identity)) {
    base::UmaHistogramBoolean(
        "Renderer.WebSandboxSupportLinux."
        "MatchFontByPostscriptNameOrFullFontName",
        false);
    return false;
  }

  fallback_font->fontconfig_interface_id = font_identity->id;
  fallback_font->filepath = font_identity->filepath;
  fallback_font->ttc_index = font_identity->ttc_index;
  base::UmaHistogramBoolean(
      "Renderer.WebSandboxSupportLinux.MatchFontByPostscriptNameOrFullFontName",
      true);
  return true;
}

void WebSandboxSupportLinux::GetWebFontRenderStyleForStrike(
    const char* family,
    int size,
    bool is_bold,
    bool is_italic,
    float device_scale_factor,
    blink::WebFontRenderStyle* out) {
  TRACE_EVENT0("fonts",
               "WebSandboxSupportLinux::GetWebFontRenderStyleForStrike");

  *out = blink::WebFontRenderStyle();

  if (size < 0 || size > std::numeric_limits<uint16_t>::max()) {
    base::UmaHistogramBoolean(
        "Renderer.WebSandboxSupportLinux.GetWebFontRenderStyleForStrike",
        false);
    return;
  }

  font_service::mojom::FontRenderStylePtr font_render_style;
  if (!font_loader_->FontRenderStyleForStrike(family, size, is_bold, is_italic,
                                              device_scale_factor,
                                              &font_render_style) ||
      !font_render_style) {
    LOG(ERROR) << "GetRenderStyleForStrike did not receive a response for "
                  "family and size: "
               << (family ? family : "<empty>") << ", " << size;
    base::UmaHistogramBoolean(
        "Renderer.WebSandboxSupportLinux.GetWebFontRenderStyleForStrike",
        false);
    return;
  }

  out->use_bitmaps = static_cast<char>(font_render_style->use_bitmaps);
  out->use_auto_hint = static_cast<char>(font_render_style->use_autohint);
  out->use_hinting = static_cast<char>(font_render_style->use_hinting);
  out->hint_style = font_render_style->hint_style;
  out->use_anti_alias = static_cast<char>(font_render_style->use_antialias);
  out->use_subpixel_rendering =
      static_cast<char>(font_render_style->use_subpixel_rendering);
  out->use_subpixel_positioning =
      static_cast<char>(font_render_style->use_subpixel_positioning);
  base::UmaHistogramBoolean(
      "Renderer.WebSandboxSupportLinux.GetWebFontRenderStyleForStrike", true);
}

}  // namespace content
