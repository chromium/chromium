/*
 * Copyright (C) 2006, 2007 Apple Computer, Inc.
 * Copyright (c) 2006, 2007, 2008, 2009, 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <windows.h>  // For GetACP()

#include <unicode/uscript.h>

#include <memory>
#include <string>
#include <utility>

#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/platform/web_font_prewarmer.h"
#include "third_party/blink/renderer/platform/fonts/bitmap_glyphs_block_list.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_face_creation_params.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_priority.h"
#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/fonts/win/font_fallback_win.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/layout_locale.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/ports/SkTypeface_win.h"

namespace blink {

WebFontPrewarmer* FontCache::prewarmer_ = nullptr;

// Cached system font metrics.
AtomicString* FontCache::menu_font_family_name_ = nullptr;
int32_t FontCache::menu_font_height_ = 0;
AtomicString* FontCache::small_caption_font_family_name_ = nullptr;
int32_t FontCache::small_caption_font_height_ = 0;
AtomicString* FontCache::status_font_family_name_ = nullptr;
int32_t FontCache::status_font_height_ = 0;

namespace {

int32_t EnsureMinimumFontHeightIfNeeded(int32_t font_height) {
  // Adjustment for codepage 936 to make the fonts more legible in Simplified
  // Chinese.  Please refer to LayoutThemeFontProviderWin.cpp for more
  // information.
  return ((font_height < 12.0f) && (GetACP() == 936)) ? 12.0f : font_height;
}

static const char kChineseSimplified[] = "zh-Hant";

// For Windows out-of-process fallback calls, there is a limiation: only one
// passed locale is taken into account when requesting a fallback font from the
// DWrite API via Skia API. If we request fallback for a Han ideograph without a
// disambiguating locale, results from DWrite are unpredictable and caching such
// a font under the ambiguous locale leads to returning wrong fonts for
// subsequent requests in font_fallback_win, hence prioritize a
// Han-disambiguating locale for CJK characters.
const LayoutLocale* FallbackLocaleForCharacter(
    const FontDescription& font_description,
    const FontFallbackPriority& fallback_priority,
    const UChar32 codepoint) {
  if (IsEmojiPresentationEmoji(fallback_priority)) {
    return LayoutLocale::Get(AtomicString(kColorEmojiLocale));
  } else if (RuntimeEnabledFeatures::SystemFallbackEmojiVSSupportEnabled() &&
             IsTextPresentationEmoji(fallback_priority)) {
    return LayoutLocale::Get(AtomicString(kMonoEmojiLocale));
  }

  UErrorCode error_code = U_ZERO_ERROR;
  const UScriptCode char_script = uscript_getScript(codepoint, &error_code);
  if (U_SUCCESS(error_code) && char_script == USCRIPT_HAN) {
    // If we were unable to disambiguate the requested Han ideograph from the
    // content locale, the Accept-Language headers or system locale, assume it's
    // simplified Chinese. It's important to pass a CJK locale to the fallback
    // call in order to avoid priming the browser side cache incorrectly with an
    // ambiguous locale for Han fallback requests.
    const LayoutLocale* han_locale =
        LayoutLocale::LocaleForHan(font_description.Locale());
    return han_locale ? han_locale
                      : LayoutLocale::Get(AtomicString(kChineseSimplified));
  }

  return font_description.Locale() ? font_description.Locale()
                                   : &LayoutLocale::GetDefault();
}

}  // namespace

// static
void FontCache::PrewarmFamily(const AtomicString& family_name) {
  DCHECK(IsMainThread());

  if (!prewarmer_)
    return;

  DEFINE_STATIC_LOCAL(HashSet<AtomicString>, prewarmed_families, ());
  const auto result = prewarmed_families.insert(family_name);
  if (!result.is_new_entry)
    return;

  prewarmer_->PrewarmFamily(family_name);
}

//static
void FontCache::SetSystemFontFamily(const AtomicString&) {
  // TODO(https://crbug.com/808221) Use this instead of
  // SetMenuFontMetrics for the system font family.
  NOTREACHED_IN_MIGRATION();
}

// static
const AtomicString& FontCache::SystemFontFamily() {
  return MenuFontFamily();
}

// static
void FontCache::SetMenuFontMetrics(const AtomicString& family_name,
                                   int32_t font_height) {
  menu_font_family_name_ = new AtomicString(family_name);
  menu_font_height_ = EnsureMinimumFontHeightIfNeeded(font_height);
}

// static
void FontCache::SetSmallCaptionFontMetrics(const AtomicString& family_name,
                                           int32_t font_height) {
  small_caption_font_family_name_ = new AtomicString(family_name);
  small_caption_font_height_ = EnsureMinimumFontHeightIfNeeded(font_height);
}

// static
void FontCache::SetStatusFontMetrics(const AtomicString& family_name,
                                     int32_t font_height) {
  status_font_family_name_ = new AtomicString(family_name);
  status_font_height_ = EnsureMinimumFontHeightIfNeeded(font_height);
}

// TODO(https://crbug.com/976737): This function is deprecated and only intended
// to run in parallel with the API based OOP font fallback calls to compare the
// results and track them in UMA for a while until we decide to remove this
// completely.
const SimpleFontData* FontCache::GetFallbackFamilyNameFromHardcodedChoices(
    const FontDescription& font_description,
    UChar32 codepoint,
    FontFallbackPriority fallback_priority) {
  UScriptCode script;
  DCHECK(font_manager_);
  if (const AtomicString fallback_family =
          GetFallbackFamily(codepoint, font_description.GenericFamily(),
                            font_description.Locale(), fallback_priority,
                            *font_manager_, script)) {
    FontFaceCreationParams create_by_family =
        FontFaceCreationParams(fallback_family);
    const FontPlatformData* data =
        GetFontPlatformData(font_description, create_by_family);
    if (data && data->FontContainsCharacter(codepoint)) {
      return FontDataFromFontPlatformData(data);
    }
  }

  // If instantiating the returned fallback family was not successful, probe for
  // a set of potential fonts with wide coverage.

  // Last resort font list : PanUnicode. CJK fonts have a pretty
  // large repertoire. Eventually, we need to scan all the fonts
  // on the system to have a Firefox-like coverage.
  // Make sure that all of them are lowercased.
  const static UChar* const kCjkFonts[] = {
      u"arial unicode ms", u"ms pgothic", u"simsun", u"gulim", u"pmingliu",
      u"wenquanyi zen hei",  // Partial CJK Ext. A coverage but more widely
                             // known to Chinese users.
      u"ar pl shanheisun uni", u"ar pl zenkai uni",
      u"han nom a",  // Complete CJK Ext. A coverage.
      u"code2000"    // Complete CJK Ext. A coverage.
      // CJK Ext. B fonts are not listed here because it's of no use
      // with our current non-BMP character handling because we use
      // Uniscribe for it and that code path does not go through here.
  };

  const static UChar* const kCommonFonts[] = {
      u"tahoma", u"arial unicode ms", u"lucida sans unicode",
      u"microsoft sans serif", u"palatino linotype",
      // Six fonts below (and code2000 at the end) are not from MS, but
      // once installed, cover a very wide range of characters.
      u"dejavu serif", u"dejavu sasns", u"freeserif", u"freesans", u"gentium",
      u"gentiumalt", u"ms pgothic", u"simsun", u"gulim", u"pmingliu",
      u"code2000"};

  const UChar* const* pan_uni_fonts = nullptr;
  int num_fonts = 0;
  if (script == USCRIPT_HAN) {
    pan_uni_fonts = kCjkFonts;
    num_fonts = std::size(kCjkFonts);
  } else {
    pan_uni_fonts = kCommonFonts;
    num_fonts = std::size(kCommonFonts);
  }
  // Font returned from getFallbackFamily may not cover |character|
  // because it's based on script to font mapping. This problem is
  // critical enough for non-Latin scripts (especially Han) to
  // warrant an additional (real coverage) check with fontCotainsCharacter.
  for (int i = 0; i < num_fonts; ++i) {
    FontFaceCreationParams create_by_family =
        FontFaceCreationParams(AtomicString(pan_uni_fonts[i]));
    const FontPlatformData* data =
        GetFontPlatformData(font_description, create_by_family);
    if (data && data->FontContainsCharacter(codepoint))
      return FontDataFromFontPlatformData(data);
  }
  return nullptr;
}

const SimpleFontData* FontCache::GetDWriteFallbackFamily(
    const FontDescription& font_description,
    UChar32 codepoint,
    FontFallbackPriority fallback_priority) {
  const LayoutLocale* fallback_locale = FallbackLocaleForCharacter(
      font_description, fallback_priority, codepoint);
  DCHECK(fallback_locale);

  const std::string family_name = font_description.Family().FamilyName().Utf8();

  Bcp47Vector locales;
  locales.push_back(fallback_locale->LocaleForSkFontMgr());
  sk_sp<SkTypeface> typeface(font_manager_->matchFamilyStyleCharacter(
      family_name.c_str(), font_description.SkiaFontStyle(), locales.data(),
      locales.size(), codepoint));

  if (!typeface) {
    return nullptr;
  }

  SkString skia_family;
  typeface->getFamilyName(&skia_family);
  FontDescription fallback_updated_font_description(font_description);
  fallback_updated_font_description.UpdateFromSkiaFontStyle(
      typeface->fontStyle());
  const FontFaceCreationParams create_by_family(ToAtomicString(skia_family));
  const FontPlatformData* data =
      GetFontPlatformData(fallback_updated_font_description, create_by_family);
  if (!data || !data->FontContainsCharacter(codepoint)) {
    return nullptr;
  }
  return FontDataFromFontPlatformData(data);
}

// Given the desired base font, this will create a SimpleFontData for a specific
// font that can be used to render the given range of characters.
const SimpleFontData* FontCache::PlatformFallbackFontForCharacter(
    const FontDescription& font_description,
    UChar32 character,
    const SimpleFontData* original_font_data,
    FontFallbackPriority fallback_priority) {
  TRACE_EVENT0("ui", "FontCache::PlatformFallbackFontForCharacter");

  // First try the specified font with standard style & weight.
  if (!IsEmojiPresentationEmoji(fallback_priority) &&
      (font_description.Style() == kItalicSlopeValue ||
       font_description.Weight() >= kBoldWeightValue)) {
    const SimpleFontData* font_data =
        FallbackOnStandardFontStyle(font_description, character);
    if (font_data)
      return font_data;
  }

  FontFallbackPriority fallback_priority_with_emoji_text = fallback_priority;
  if (RuntimeEnabledFeatures::SystemFallbackEmojiVSSupportEnabled() &&
      fallback_priority == FontFallbackPriority::kText &&
      Character::IsEmoji(character)) {
    fallback_priority_with_emoji_text = FontFallbackPriority::kEmojiText;
  }

  const SimpleFontData* hardcoded_list_fallback_font =
      GetFallbackFamilyNameFromHardcodedChoices(
          font_description, character, fallback_priority_with_emoji_text);

  // Fall through to running the API-based fallback.
  if (RuntimeEnabledFeatures::LegacyWindowsDWriteFontFallbackEnabled() ||
      !hardcoded_list_fallback_font) {
    return GetDWriteFallbackFamily(font_description, character,
                                   fallback_priority_with_emoji_text);
  }

  return hardcoded_list_fallback_font;
}

static inline bool DeprecatedEqualIgnoringCase(const AtomicString& a,
                                               const SkString& b) {
  return DeprecatedEqualIgnoringCase(a, ToAtomicString(b));
}

static bool TypefacesMatchesFamily(const SkTypeface* tf,
                                   const AtomicString& family) {
  SkTypeface::LocalizedStrings* actual_families =
      tf->createFamilyNameIterator();
  bool matches_requested_family = false;
  SkTypeface::LocalizedString actual_family;

  while (actual_families->next(&actual_family)) {
    if (DeprecatedEqualIgnoringCase(family, actual_family.fString)) {
      matches_requested_family = true;
      break;
    }
  }
  actual_families->unref();

  // getFamilyName may return a name not returned by the
  // createFamilyNameIterator.
  // Specifically in cases where Windows substitutes the font based on the
  // HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\FontSubstitutes registry
  // entries.
  if (!matches_requested_family) {
    SkString family_name;
    tf->getFamilyName(&family_name);
    if (DeprecatedEqualIgnoringCase(family, family_name))
      matches_requested_family = true;
  }

  return matches_requested_family;
}

static bool TypefacesHasWeightSuffix(const AtomicString& family,
                                     AtomicString& adjusted_name,
                                     FontSelectionValue& variant_weight) {
  struct FamilyWeightSuffix {
    const UChar* suffix;
    wtf_size_t length;
    FontSelectionValue weight;
  };
  // Mapping from suffix to weight from the DirectWrite documentation.
  // http://msdn.microsoft.com/en-us/library/windows/desktop/dd368082.aspx
  const static FamilyWeightSuffix kVariantForSuffix[] = {
      {u" thin", 5, FontSelectionValue(100)},
      {u" extralight", 11, FontSelectionValue(200)},
      {u" ultralight", 11, FontSelectionValue(200)},
      {u" light", 6, FontSelectionValue(300)},
      {u" regular", 8, FontSelectionValue(400)},
      {u" medium", 7, FontSelectionValue(500)},
      {u" demibold", 9, FontSelectionValue(600)},
      {u" semibold", 9, FontSelectionValue(600)},
      {u" extrabold", 10, FontSelectionValue(800)},
      {u" ultrabold", 10, FontSelectionValue(800)},
      {u" black", 6, FontSelectionValue(900)},
      {u" heavy", 6, FontSelectionValue(900)}};
  size_t num_variants = std::size(kVariantForSuffix);
  for (size_t i = 0; i < num_variants; i++) {
    const FamilyWeightSuffix& entry = kVariantForSuffix[i];
    if (family.EndsWith(entry.suffix, kTextCaseUnicodeInsensitive)) {
      String family_name = family.GetString();
      family_name.Truncate(family.length() - entry.length);
      adjusted_name = AtomicString(family_name);
      variant_weight = entry.weight;
      return true;
    }
  }

  return false;
}

static bool TypefacesHasStretchSuffix(const AtomicString& family,
                                      AtomicString& adjusted_name,
                                      FontSelectionValue& variant_stretch) {
  struct FamilyStretchSuffix {
    const UChar* suffix;
    wtf_size_t length;
    FontSelectionValue stretch;
  };
  // Mapping from suffix to stretch value from the DirectWrite documentation.
  // http://msdn.microsoft.com/en-us/library/windows/desktop/dd368078.aspx
  // Also includes Narrow as a synonym for Condensed to to support Arial
  // Narrow and other fonts following the same naming scheme.
  const static FamilyStretchSuffix kVariantForSuffix[] = {
      {u" ultracondensed", 15, kUltraCondensedWidthValue},
      {u" extracondensed", 15, kExtraCondensedWidthValue},
      {u" condensed", 10, kCondensedWidthValue},
      {u" narrow", 7, kCondensedWidthValue},
      {u" semicondensed", 14, kSemiCondensedWidthValue},
      {u" semiexpanded", 13, kSemiExpandedWidthValue},
      {u" expanded", 9, kExpandedWidthValue},
      {u" extraexpanded", 14, kExtraExpandedWidthValue},
      {u" ultraexpanded", 14, kUltraExpandedWidthValue}};
  size_t num_variants = std::size(kVariantForSuffix);
  for (size_t i = 0; i < num_variants; i++) {
    const FamilyStretchSuffix& entry = kVariantForSuffix[i];
    if (family.EndsWith(entry.suffix, kTextCaseUnicodeInsensitive)) {
      String family_name = family.GetString();
      family_name.Truncate(family.length() - entry.length);
      adjusted_name = AtomicString(family_name);
      variant_stretch = entry.stretch;
      return true;
    }
  }

  return false;
}

const FontPlatformData* FontCache::CreateFontPlatformData(
    const FontDescription& font_description,
    const FontFaceCreationParams& creation_params,
    float font_size,
    AlternateFontName alternate_font_name) {
  TRACE_EVENT0("ui", "FontCache::CreateFontPlatformData");

  DCHECK_EQ(creation_params.CreationType(), kCreateFontByFamily);
  sk_sp<SkTypeface> typeface;

  std::string name;

  if (alternate_font_name == AlternateFontName::kLocalUniqueFace &&
      RuntimeEnabledFeatures::FontSrcLocalMatchingEnabled()) {
    typeface = CreateTypefaceFromUniqueName(creation_params);

    // We do not need to try any heuristic around the font name, as below, for
    // family matching.
    if (!typeface)
      return nullptr;

  } else {
    typeface = CreateTypeface(font_description, creation_params, name);

    // For a family match, Windows will always give us a valid pointer here,
    // even if the face name is non-existent. We have to double-check and see if
    // the family name was really used.
    if (!typeface ||
        !TypefacesMatchesFamily(typeface.get(), creation_params.Family())) {
      AtomicString adjusted_name;
      FontSelectionValue variant_weight;
      FontSelectionValue variant_stretch;

      // TODO: crbug.com/627143 LocalFontFaceSource.cpp, which implements
      // retrieving src: local() font data uses getFontData, which in turn comes
      // here, to retrieve fonts from the cache and specifies the argument to
      // local() as family name. So we do not match by full font name or
      // postscript name as the spec says:
      // https://drafts.csswg.org/css-fonts-3/#src-desc

      // Prevent one side effect of the suffix translation below where when
      // matching local("Roboto Regular") it tries to find the closest match
      // even though that can be a bold font in case of Roboto Bold.
      if (alternate_font_name == AlternateFontName::kLocalUniqueFace) {
        return nullptr;
      }

      if (alternate_font_name == AlternateFontName::kLastResort) {
        if (!typeface)
          return nullptr;
      } else if (TypefacesHasWeightSuffix(creation_params.Family(),
                                          adjusted_name, variant_weight)) {
        FontFaceCreationParams adjusted_params(adjusted_name);
        FontDescription adjusted_font_description = font_description;
        adjusted_font_description.SetWeight(variant_weight);
        typeface =
            CreateTypeface(adjusted_font_description, adjusted_params, name);
        if (!typeface ||
            !TypefacesMatchesFamily(typeface.get(), adjusted_name)) {
          return nullptr;
        }

      } else if (TypefacesHasStretchSuffix(creation_params.Family(),
                                           adjusted_name, variant_stretch)) {
        FontFaceCreationParams adjusted_params(adjusted_name);
        FontDescription adjusted_font_description = font_description;
        adjusted_font_description.SetStretch(variant_stretch);
        typeface =
            CreateTypeface(adjusted_font_description, adjusted_params, name);
        if (!typeface ||
            !TypefacesMatchesFamily(typeface.get(), adjusted_name)) {
          return nullptr;
        }
      } else {
        return nullptr;
      }
    }
  }

  bool synthetic_bold_requested =
      (font_description.Weight() >= kBoldThreshold && !typeface->isBold()) ||
      font_description.IsSyntheticBold();

  bool synthetic_italic_requested =
      ((font_description.Style() == kItalicSlopeValue) &&
       !typeface->isItalic()) ||
      font_description.IsSyntheticItalic();

  FontPlatformData* result = MakeGarbageCollected<FontPlatformData>(
      typeface, name.data(), font_size,
      synthetic_bold_requested && font_description.SyntheticBoldAllowed(),
      synthetic_italic_requested && font_description.SyntheticItalicAllowed(),
      font_description.TextRendering(), ResolvedFontFeatures(),
      font_description.Orientation());

  result->SetAvoidEmbeddedBitmaps(
      BitmapGlyphsBlockList::ShouldAvoidEmbeddedBitmapsForTypeface(*typeface));

  return result;
}

}  // namespace blink
