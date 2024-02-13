// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_cache.h"

#include "skia/ext/font_utils.h"
#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"

namespace blink {

static AtomicString& MutableSystemFontFamily() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(AtomicString, system_font_family, ());
  return system_font_family;
}

// static
const AtomicString& FontCache::SystemFontFamily() {
  return MutableSystemFontFamily();
}

// static
void FontCache::SetSystemFontFamily(const AtomicString& family_name) {
  DCHECK(!family_name.empty());
  MutableSystemFontFamily() = family_name;
}

const SimpleFontData* FontCache::PlatformFallbackFontForCharacter(
    const FontDescription& font_description,
    UChar32 character,
    const SimpleFontData* font_data_to_substitute,
    FontFallbackPriority fallback_priority) {
  sk_sp<SkFontMgr> font_mgr(skia::DefaultFontMgr());
  std::string family_name = font_description.Family().FamilyName().Utf8();
  Bcp47Vector locales =
      GetBcp47LocaleForRequest(font_description, fallback_priority);
  sk_sp<SkTypeface> typeface(font_mgr->matchFamilyStyleCharacter(
      family_name.c_str(), font_description.SkiaFontStyle(), locales.data(),
      locales.size(), character));
  if (!typeface) {
    return nullptr;
  }

  bool synthetic_bold = font_description.IsSyntheticBold() &&
                        !typeface->isBold() &&
                        font_description.SyntheticBoldAllowed();
  bool synthetic_italic = font_description.IsSyntheticItalic() &&
                          !typeface->isItalic() &&
                          font_description.SyntheticItalicAllowed();

  auto* font_data = MakeGarbageCollected<FontPlatformData>(
      std::move(typeface), std::string(), font_description.EffectiveFontSize(),
      synthetic_bold, synthetic_italic, font_description.TextRendering(),
      ResolvedFontFeatures(), font_description.Orientation());

  return FontDataFromFontPlatformData(font_data);
}

}  // namespace blink
