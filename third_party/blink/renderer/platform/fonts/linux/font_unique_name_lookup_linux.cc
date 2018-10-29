// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/linux/font_unique_name_lookup_linux.h"
#include "third_party/blink/public/platform/linux/out_of_process_font.h"
#include "third_party/blink/public/platform/linux/web_sandbox_support.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/fonts/skia/sktypeface_factory.h"

namespace blink {

FontUniqueNameLookupLinux::~FontUniqueNameLookupLinux() = default;

sk_sp<SkTypeface> FontUniqueNameLookupLinux::MatchUniqueName(
    const String& font_unique_name) {
  OutOfProcessFont uniquely_matched_font;
  if (!Platform::Current()->GetSandboxSupport()) {
    LOG(ERROR) << "@font-face src: local() instantiation only available when "
                  "connected to browser process.";
    DCHECK(Platform::Current()->GetSandboxSupport());
    return nullptr;
  }

  Platform::Current()
      ->GetSandboxSupport()
      ->MatchFontByPostscriptNameOrFullFontName(
          font_unique_name.Utf8(WTF::kStrictUTF8Conversion).data(),
          &uniquely_matched_font);
  if (!uniquely_matched_font.filename.size())
    return nullptr;

  return SkTypeface_Factory::FromFontConfigInterfaceIdAndTtcIndex(
      uniquely_matched_font.fontconfig_interface_id,
      uniquely_matched_font.ttc_index);
}

}  // namespace blink
