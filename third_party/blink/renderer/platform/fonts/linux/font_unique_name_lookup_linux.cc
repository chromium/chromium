// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/linux/font_unique_name_lookup_linux.h"

#include "base/logging.h"
#include "third_party/blink/public/platform/linux/web_sandbox_support.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/fonts/skia/sktypeface_factory.h"
#include "ui/gfx/font_fallback_linux.h"

namespace blink {

FontUniqueNameLookupLinux::~FontUniqueNameLookupLinux() = default;

sk_sp<SkTypeface> FontUniqueNameLookupLinux::MatchUniqueName(
    const String& font_unique_name) {
  gfx::FallbackFontData uniquely_matched_font;
  if (!Platform::Current()->GetSandboxSupport()) {
    LOG(ERROR) << "@font-face src: local() instantiation only available when "
                  "connected to browser process.";
    return nullptr;
  }

  if (!Platform::Current()
           ->GetSandboxSupport()
           ->MatchFontByPostscriptNameOrFullFontName(
               font_unique_name.Utf8(WTF::kStrictUTF8Conversion).c_str(),
               &uniquely_matched_font))
    return nullptr;

  return SkTypeface_Factory::FromFontConfigInterfaceIdAndTtcIndex(
      uniquely_matched_font.fontconfig_interface_id,
      uniquely_matched_font.ttc_index);
}

}  // namespace blink
