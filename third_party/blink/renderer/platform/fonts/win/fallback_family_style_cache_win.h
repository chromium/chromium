// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_WIN_FALLBACK_FAMILY_STYLE_CACHE_WIN_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_WIN_FALLBACK_FAMILY_STYLE_CACHE_WIN_H_

#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_priority.h"
#include "third_party/blink/renderer/platform/fonts/win/fallback_lru_cache_win.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace blink {

class FallbackFamilyStyleCache {
  USING_FAST_MALLOC(FallbackFamilyStyleCache);

 public:
  FallbackFamilyStyleCache();

  // Places a SkTypeface object in the cache for specified language tag and
  // fallback priority, taking a reference on SkTypeface. Adds the |SkTypeface|
  // to the beginning of a list of typefaces if previous |SkTypefaces| objects
  // where added for this set of parameters. Note, the internal list of
  // typefaces for a language tag and fallback priority is not checked for
  // duplicates when adding a |typeface| object.
  void Put(FontDescription::GenericFamilyType generic_family,
           String bcp47_language_tag,
           FontFallbackPriority fallback_priority,
           SkTypeface* typeface);

  // Fetches a |fallback_family| and |fallback_style| for a given language tag,
  // fallback priority and codepoint. Checks the internal cache for whether a
  // fallback font with glyph coverage for |character| is available for the
  // given parameters, then returns its family name and style.
  void Get(FontDescription::GenericFamilyType generic_family,
           String bcp47_language_tag,
           FontFallbackPriority fallback_priority,
           UChar32 character,
           String* fallback_family,
           SkFontStyle* fallback_style);

  // Empties the internal cache, deleting keys and unrefing the typefaces that
  // were placed in the cache.
  void Clear();

 private:
  DISALLOW_COPY_AND_ASSIGN(FallbackFamilyStyleCache);

  FallbackLruCache recent_fallback_fonts_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_WIN_FALLBACK_FAMILY_STYLE_CACHE_WIN_H_
