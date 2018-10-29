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

#include "third_party/blink/renderer/platform/fonts/font_cache.h"

#include <memory>
#include <utility>

#include "base/debug/alias.h"
#include "third_party/blink/renderer/platform/fonts/bitmap_glyphs_blacklist.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_face_creation_params.h"
#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/fonts/win/font_fallback_win.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/ports/SkTypeface_win.h"

namespace blink {

HashMap<String, sk_sp<SkTypeface>, CaseFoldingHash>*
    FontCache::sideloaded_fonts_ = nullptr;

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
  return (font_height < 12.0f) && (GetACP() == 936) ? 12.0f : font_height;
}

}  // namespace

// static
void FontCache::AddSideloadedFontForTesting(sk_sp<SkTypeface> typeface) {
  if (!sideloaded_fonts_)
    sideloaded_fonts_ = new HashMap<String, sk_sp<SkTypeface>, CaseFoldingHash>;
  SkString name;
  typeface->getFamilyName(&name);
  String name_wtf(name.c_str());
  sideloaded_fonts_->Set(name_wtf, std::move(typeface));
}

// static
const AtomicString& FontCache::SystemFontFamily() {
  return MenuFontFamily();
}

// static
void FontCache::SetMenuFontMetrics(const wchar_t* family_name,
                                   int32_t font_height) {
  menu_font_family_name_ = new AtomicString(family_name);
  menu_font_height_ = EnsureMinimumFontHeightIfNeeded(font_height);
}

// static
void FontCache::SetSmallCaptionFontMetrics(const wchar_t* family_name,
                                           int32_t font_height) {
  small_caption_font_family_name_ = new AtomicString(family_name);
  small_caption_font_height_ = EnsureMinimumFontHeightIfNeeded(font_height);
}

// static
void FontCache::SetStatusFontMetrics(const wchar_t* family_name,
                                     int32_t font_height) {
  status_font_family_name_ = new AtomicString(family_name);
  status_font_height_ = EnsureMinimumFontHeightIfNeeded(font_height);
}

FontCache::FontCache() : purge_prevent_count_(0) {
  font_manager_ = sk_ref_sp(static_font_manager_);
  if (!font_manager_) {
    // This code path is only for unit tests. This SkFontMgr does not work in
    // sandboxed environments, but injecting this initialization code to all
    // unit tests isn't easy.
    font_manager_ = SkFontMgr_New_DirectWrite();
    // Set |is_test_font_mgr_| to capture if this is not happening in the
    // production code. crbug.com/561873
    is_test_font_mgr_ = true;
  }
  DCHECK(font_manager_.get());
}

// Given the desired base font, this will create a SimpleFontData for a specific
// font that can be used to render the given range of characters.
scoped_refptr<SimpleFontData> FontCache::PlatformFallbackFontForCharacter(
    const FontDescription& font_description,
    UChar32 character,
    const SimpleFontData* original_font_data,
    FontFallbackPriority fallback_priority) {
  // First try the specified font with standard style & weight.
  if (fallback_priority != FontFallbackPriority::kEmojiEmoji &&
      (font_description.Style() == ItalicSlopeValue() ||
       font_description.Weight() >= BoldWeightValue())) {
    scoped_refptr<SimpleFontData> font_data =
        FallbackOnStandardFontStyle(font_description, character);
    if (font_data)
      return font_data;
  }

  UScriptCode script;
  const UChar* family = GetFallbackFamily(
      character, font_description.GenericFamily(), font_description.Locale(),
      &script, fallback_priority, font_manager_.get());
  if (family) {
    FontFaceCreationParams create_by_family(family);
    FontPlatformData* data =
        GetFontPlatformData(font_description, create_by_family);
    if (data && data->FontContainsCharacter(character))
      return FontDataFromFontPlatformData(data, kDoNotRetain);
  }

  if (use_skia_font_fallback_) {
    const char* bcp47_locale = nullptr;
    int locale_count = 0;
    // If the font description has a locale, use that. Otherwise, Skia will
    // fall back on the user's default locale.
    // TODO(kulshin): extract locale fallback logic from
    //   FontCacheAndroid.cpp and share that code
    if (font_description.Locale()) {
      bcp47_locale = font_description.Locale()->LocaleForSkFontMgr();
      locale_count = 1;
    }

    CString family_name = font_description.Family().Family().Utf8();

    SkTypeface* typeface = font_manager_->matchFamilyStyleCharacter(
        family_name.data(), font_description.SkiaFontStyle(), &bcp47_locale,
        locale_count, character);
    if (typeface) {
      SkString skia_family;
      typeface->getFamilyName(&skia_family);
      FontFaceCreationParams create_by_family(ToAtomicString(skia_family));
      FontPlatformData* data =
          GetFontPlatformData(font_description, create_by_family);
      if (data && data->FontContainsCharacter(character))
        return FontDataFromFontPlatformData(data, kDoNotRetain);
    }
  }

  // In production, these 3 font managers must match.
  // They don't match in unit tests or in single process mode.
  // Capture them in minidump for crbug.com/409784
  SkFontMgr* font_mgr = font_manager_.get();
  SkFontMgr* static_font_mgr = static_font_manager_;
  SkFontMgr* skia_default_font_mgr = SkFontMgr::RefDefault().get();
  base::debug::Alias(&font_mgr);
  base::debug::Alias(&static_font_mgr);
  base::debug::Alias(&skia_default_font_mgr);

  // Last resort font list : PanUnicode. CJK fonts have a pretty
  // large repertoire. Eventually, we need to scan all the fonts
  // on the system to have a Firefox-like coverage.
  // Make sure that all of them are lowercased.
  const static wchar_t* const kCjkFonts[] = {
      L"arial unicode ms", L"ms pgothic", L"simsun", L"gulim", L"pmingliu",
      L"wenquanyi zen hei",  // Partial CJK Ext. A coverage but more widely
                             // known to Chinese users.
      L"ar pl shanheisun uni", L"ar pl zenkai uni",
      L"han nom a",  // Complete CJK Ext. A coverage.
      L"code2000"    // Complete CJK Ext. A coverage.
      // CJK Ext. B fonts are not listed here because it's of no use
      // with our current non-BMP character handling because we use
      // Uniscribe for it and that code path does not go through here.
  };

  const static wchar_t* const kCommonFonts[] = {
      L"tahoma", L"arial unicode ms", L"lucida sans unicode",
      L"microsoft sans serif", L"palatino linotype",
      // Six fonts below (and code2000 at the end) are not from MS, but
      // once installed, cover a very wide range of characters.
      L"dejavu serif", L"dejavu sasns", L"freeserif", L"freesans", L"gentium",
      L"gentiumalt", L"ms pgothic", L"simsun", L"gulim", L"pmingliu",
      L"code2000"};

  const wchar_t* const* pan_uni_fonts = nullptr;
  int num_fonts = 0;
  if (script == USCRIPT_HAN) {
    pan_uni_fonts = kCjkFonts;
    num_fonts = arraysize(kCjkFonts);
  } else {
    pan_uni_fonts = kCommonFonts;
    num_fonts = arraysize(kCommonFonts);
  }
  // Font returned from getFallbackFamily may not cover |character|
  // because it's based on script to font mapping. This problem is
  // critical enough for non-Latin scripts (especially Han) to
  // warrant an additional (real coverage) check with fontCotainsCharacter.
  for (int i = 0; i < num_fonts; ++i) {
    family = pan_uni_fonts[i];
    FontFaceCreationParams create_by_family(family);
    FontPlatformData* data =
        GetFontPlatformData(font_description, create_by_family);
    if (data && data->FontContainsCharacter(character))
      return FontDataFromFontPlatformData(data, kDoNotRetain);
  }

  return nullptr;
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
    const wchar_t* suffix;
    size_t length;
    FontSelectionValue weight;
  };
  // Mapping from suffix to weight from the DirectWrite documentation.
  // http://msdn.microsoft.com/en-us/library/windows/desktop/dd368082.aspx
  const static FamilyWeightSuffix kVariantForSuffix[] = {
      {L" thin", 5, FontSelectionValue(100)},
      {L" extralight", 11, FontSelectionValue(200)},
      {L" ultralight", 11, FontSelectionValue(200)},
      {L" light", 6, FontSelectionValue(300)},
      {L" regular", 8, FontSelectionValue(400)},
      {L" medium", 7, FontSelectionValue(500)},
      {L" demibold", 9, FontSelectionValue(600)},
      {L" semibold", 9, FontSelectionValue(600)},
      {L" extrabold", 10, FontSelectionValue(800)},
      {L" ultrabold", 10, FontSelectionValue(800)},
      {L" black", 6, FontSelectionValue(900)},
      {L" heavy", 6, FontSelectionValue(900)}};
  size_t num_variants = arraysize(kVariantForSuffix);
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
    const wchar_t* suffix;
    size_t length;
    FontSelectionValue stretch;
  };
  // Mapping from suffix to stretch value from the DirectWrite documentation.
  // http://msdn.microsoft.com/en-us/library/windows/desktop/dd368078.aspx
  // Also includes Narrow as a synonym for Condensed to to support Arial
  // Narrow and other fonts following the same naming scheme.
  const static FamilyStretchSuffix kVariantForSuffix[] = {
      {L" ultracondensed", 15, UltraCondensedWidthValue()},
      {L" extracondensed", 15, ExtraCondensedWidthValue()},
      {L" condensed", 10, CondensedWidthValue()},
      {L" narrow", 7, CondensedWidthValue()},
      {L" semicondensed", 14, SemiCondensedWidthValue()},
      {L" semiexpanded", 13, SemiExpandedWidthValue()},
      {L" expanded", 9, ExpandedWidthValue()},
      {L" extraexpanded", 14, ExtraExpandedWidthValue()},
      {L" ultraexpanded", 14, UltraExpandedWidthValue()}};
  size_t num_variants = arraysize(kVariantForSuffix);
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

std::unique_ptr<FontPlatformData> FontCache::CreateFontPlatformData(
    const FontDescription& font_description,
    const FontFaceCreationParams& creation_params,
    float font_size,
    AlternateFontName alternate_font_name) {
  DCHECK_EQ(creation_params.CreationType(), kCreateFontByFamily);

  CString name;
  sk_sp<SkTypeface> typeface =
      CreateTypeface(font_description, creation_params, name);
  // Windows will always give us a valid pointer here, even if the face name
  // is non-existent. We have to double-check and see if the family name was
  // really used.
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
    // matching local("Roboto Regular") it tries to find the closest match even
    // though that can be a bold font in case of Roboto Bold.
    if (alternate_font_name == AlternateFontName::kLocalUniqueFace) {
      return nullptr;
    }

    if (alternate_font_name == AlternateFontName::kLastResort) {
      if (!typeface)
        return nullptr;
    } else if (TypefacesHasWeightSuffix(creation_params.Family(), adjusted_name,
                                        variant_weight)) {
      FontFaceCreationParams adjusted_params(adjusted_name);
      FontDescription adjusted_font_description = font_description;
      adjusted_font_description.SetWeight(variant_weight);
      typeface =
          CreateTypeface(adjusted_font_description, adjusted_params, name);
      if (!typeface || !TypefacesMatchesFamily(typeface.get(), adjusted_name)) {
        return nullptr;
      }

    } else if (TypefacesHasStretchSuffix(creation_params.Family(),
                                         adjusted_name, variant_stretch)) {
      FontFaceCreationParams adjusted_params(adjusted_name);
      FontDescription adjusted_font_description = font_description;
      adjusted_font_description.SetStretch(variant_stretch);
      typeface =
          CreateTypeface(adjusted_font_description, adjusted_params, name);
      if (!typeface || !TypefacesMatchesFamily(typeface.get(), adjusted_name)) {
        return nullptr;
      }
    } else {
      return nullptr;
    }
  }

  std::unique_ptr<FontPlatformData> result = std::make_unique<FontPlatformData>(
      typeface, name.data(), font_size,
      (font_description.Weight() >= BoldThreshold() && !typeface->isBold()) ||
          font_description.IsSyntheticBold(),
      ((font_description.Style() == ItalicSlopeValue()) &&
       !typeface->isItalic()) ||
          font_description.IsSyntheticItalic(),
      font_description.Orientation());

  result->SetAvoidEmbeddedBitmaps(
      BitmapGlyphsBlacklist::AvoidEmbeddedBitmapsForTypeface(typeface.get()));

  return result;
}

}  // namespace blink
