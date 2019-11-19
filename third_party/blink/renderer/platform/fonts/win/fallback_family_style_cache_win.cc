// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/win/fallback_family_style_cache_win.h"

#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"

namespace blink {

namespace {

const wtf_size_t kMaxCacheSlots = 16;

String makeCacheKey(FontDescription::GenericFamilyType generic_family,
                    String bcp47_language_tag,
                    FontFallbackPriority fallback_priority) {
  StringBuilder cache_key;
  cache_key.Append(bcp47_language_tag);
  cache_key.AppendNumber(
      static_cast<
          std::underlying_type<FontDescription::GenericFamilyType>::type>(
          generic_family));
  cache_key.AppendNumber(
      static_cast<std::underlying_type<FontFallbackPriority>::type>(
          fallback_priority));
  return cache_key.ToString();
}

void getFallbackFamilyAndStyle(SkTypeface* typeface,
                               String* fallback_family,
                               SkFontStyle* fallback_style) {
  SkString family;
  typeface->getFamilyName(&family);
  *fallback_family = family.c_str();

  *fallback_style = typeface->fontStyle();
}
}  // namespace

FallbackFamilyStyleCache::FallbackFamilyStyleCache()
    : recent_fallback_fonts_(kMaxCacheSlots) {}

void FallbackFamilyStyleCache::Put(
    FontDescription::GenericFamilyType generic_family,
    String bcp47_language_tag,
    FontFallbackPriority fallback_priority,
    SkTypeface* typeface) {
  String cache_key =
      makeCacheKey(generic_family, bcp47_language_tag, fallback_priority);

  FallbackLruCache::TypefaceVector* existing_typefaces =
      recent_fallback_fonts_.Get(cache_key);
  if (existing_typefaces) {
    existing_typefaces->insert(0, sk_ref_sp(typeface));
  } else {
    FallbackLruCache::TypefaceVector typefaces;
    typefaces.push_back(sk_ref_sp(typeface));
    recent_fallback_fonts_.Put(std::move(cache_key), std::move(typefaces));
  }
}

void FallbackFamilyStyleCache::Get(
    FontDescription::GenericFamilyType generic_family,
    String bcp47_language_tag,
    FontFallbackPriority fallback_priority,
    UChar32 character,
    String* fallback_family,
    SkFontStyle* fallback_style) {
  FallbackLruCache::TypefaceVector* typefaces = recent_fallback_fonts_.Get(
      makeCacheKey(generic_family, bcp47_language_tag, fallback_priority));
  if (!typefaces)
    return;

  for (wtf_size_t i = 0; i < typefaces->size(); ++i) {
    sk_sp<SkTypeface>& typeface = typefaces->at(i);
    if (typeface->unicharToGlyph(character)) {
      getFallbackFamilyAndStyle(typeface.get(), fallback_family,
                                fallback_style);
      sk_sp<SkTypeface> tmp_typeface(typeface);
      // For the vector of typefaces for this specific language tag, since this
      // SkTypeface had a glyph, move it to the beginning to accelerate
      // subsequent lookups.
      typefaces->EraseAt(i);
      typefaces->insert(0, std::move(tmp_typeface));
      return;
    }
  }
}

}  // namespace blink
