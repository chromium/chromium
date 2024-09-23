/*
 * Copyright (c) 2011 Google Inc. All rights reserved.
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

#include "base/feature_list.h"
#include "skia/ext/font_utils.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/font_family_names.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_face_creation_params.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_priority.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace blink {

namespace {
const char kNotoColorEmoji[] = "NotoColorEmoji";
}

static AtomicString DefaultFontFamily(sk_sp<SkFontMgr> font_manager) {
  // Pass nullptr to get the default typeface. The default typeface in Android
  // is "sans-serif" if exists, or the first entry in fonts.xml.
  sk_sp<SkTypeface> typeface(
      font_manager->legacyMakeTypeface(nullptr, SkFontStyle()));
  if (typeface) {
    SkString family_name;
    typeface->getFamilyName(&family_name);
    if (family_name.size())
      return ToAtomicString(family_name);
  }

  // Some devices do not return the default typeface. There's not much we can
  // do here, use "Arial", the value LayoutTheme uses for CSS system font
  // keywords such as "menu".
  NOTREACHED_IN_MIGRATION();
  return font_family_names::kArial;
}

static AtomicString DefaultFontFamily() {
  if (sk_sp<SkFontMgr> font_manager = FontCache::Get().FontManager())
    return DefaultFontFamily(font_manager);
  return DefaultFontFamily(skia::DefaultFontMgr());
}

// static
const AtomicString& FontCache::SystemFontFamily() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(AtomicString, system_font_family,
                                  (DefaultFontFamily()));
  return system_font_family;
}

// static
void FontCache::SetSystemFontFamily(const AtomicString&) {}

sk_sp<SkTypeface> FontCache::CreateLocaleSpecificTypeface(
    const FontDescription& font_description,
    const char* locale_family_name) {
  // TODO(crbug.com/1252383, crbug.com/1237860, crbug.com/1233315): Skia handles
  // "und-" by simple string matches, and falls back to the first
  // `fallbackFor="serif"` in the `fonts.xml`. Because all non-CJK languages use
  // "und-" in the AOSP `fonts.xml`, apply locale-specific typeface only to CJK
  // to work around this problem.
  const LayoutLocale& locale = font_description.LocaleOrDefault();
  if (!locale.HasScriptForHan())
    return nullptr;

  const char* bcp47 = locale.LocaleForSkFontMgr();
  DCHECK(bcp47);
  SkFontMgr* font_manager =
      font_manager_ ? font_manager_.get() : skia::DefaultFontMgr().get();
  sk_sp<SkTypeface> typeface(font_manager->matchFamilyStyleCharacter(
      locale_family_name, font_description.SkiaFontStyle(), &bcp47,
      /* bcp47Count */ 1,
      // |matchFamilyStyleCharacter| is the only API that accepts |bcp47|, but
      // it also checks if a character has a glyph. To look up the first
      // match, use the space character, because all fonts are likely to have
      // a glyph for it.
      kSpaceCharacter));
  if (!typeface)
    return nullptr;

  // When the specified family of the specified language does not exist, we want
  // to fall back to the specified family of the default language, but
  // |matchFamilyStyleCharacter| falls back to the default family of the
  // specified language. Get the default family of the language and compare
  // with what we get.
  SkString skia_family_name;
  typeface->getFamilyName(&skia_family_name);
  sk_sp<SkTypeface> fallback(font_manager->matchFamilyStyleCharacter(
      nullptr, font_description.SkiaFontStyle(), &bcp47,
      /* bcp47Count */ 1, kSpaceCharacter));
  SkString skia_fallback_name;
  fallback->getFamilyName(&skia_fallback_name);
  if (typeface != fallback)
    return typeface;
  return nullptr;
}

const SimpleFontData* FontCache::PlatformFallbackFontForCharacter(
    const FontDescription& font_description,
    UChar32 c,
    const SimpleFontData*,
    FontFallbackPriority fallback_priority) {
  sk_sp<SkFontMgr> fm(skia::DefaultFontMgr());

  // Pass "serif" to |matchFamilyStyleCharacter| if the `font-family` list
  // contains `serif`, so that it fallbacks to i18n serif fonts that has the
  // specified character. Do this only for `serif` because other generic
  // families do not have the lang-specific fallback list.
  const char* generic_family_name = nullptr;
  if (font_description.GenericFamily() == FontDescription::kSerifFamily)
    generic_family_name = "serif";

  FontFallbackPriority fallback_priority_with_emoji_text = fallback_priority;

  if (RuntimeEnabledFeatures::SystemFallbackEmojiVSSupportEnabled() &&
      fallback_priority == FontFallbackPriority::kText &&
      Character::IsEmoji(c)) {
    fallback_priority_with_emoji_text = FontFallbackPriority::kEmojiText;
  }

  AtomicString family_name = GetFamilyNameForCharacter(
      fm.get(), c, font_description, generic_family_name,
      fallback_priority_with_emoji_text);

  auto skia_fallback_is_noto_color_emoji = [&]() {
    const FontPlatformData* skia_fallback_result = GetFontPlatformData(
        font_description, FontFaceCreationParams(family_name));

    // Determining the PostScript name is required as Skia on Android gives
    // synthetic family names such as "91##fallback" to fallback fonts
    // determined (Compare Skia's SkFontMgr_Android::addFamily). In order to
    // identify if really the Emoji font was returned, compare by PostScript
    // name rather than by family.
    SkString fallback_postscript_name;
    if (skia_fallback_result && skia_fallback_result->Typeface()) {
      skia_fallback_result->Typeface()->getPostScriptName(
          &fallback_postscript_name);
    }
    return fallback_postscript_name.equals(kNotoColorEmoji);
  };

  // On Android when we request font with specific emoji locale (i.e. "Zsym" or
  // "Zsye"), Skia will first search for the font with the exact emoji locale,
  // if it didn't succeed it will look at fonts with other emoji locales and
  // only after look at the fonts without any emoji locale at all. The only font
  // with "Zsym" locale on Android is "NotoSansSymbols-Regular-Subsetted2.ttf"
  // font, but some text default emoji codepoints that are not present in this
  // font, can be present in other monochromatic fonts without "Zsym" locale
  // (for instance "NotoSansSymbols-Regular-Subsetted.ttf" is a font without
  // emoji locales). So, if text presentation was requested for emoji character,
  // but `GetFamilyNameForCharacter` returned colored font, we should try to get
  // monochromatic font by searching for the font without emoji locales "Zsym"
  // or "Zsye", see https://unicode.org/reports/tr51/#Emoji_Script.
  if (RuntimeEnabledFeatures::SystemFallbackEmojiVSSupportEnabled() &&
      IsTextPresentationEmoji(fallback_priority_with_emoji_text) &&
      skia_fallback_is_noto_color_emoji()) {
    family_name = GetFamilyNameForCharacter(fm.get(), c, font_description,
                                            generic_family_name,
                                            FontFallbackPriority::kText);
  }

  // Return the GMS Core emoji font if FontFallbackPriority is kEmojiEmoji or
  // kEmojiEmojiWithVS and a) no system fallback was found or b) the system
  // fallback font's PostScript name is "Noto Color Emoji" - then we override
  // the system one with the newer one from GMS core if we have it and if it has
  // glyph coverage. This should improves coverage for sequences such as WOMAN
  // FEEDING BABY, which would otherwise get broken down into multiple
  // individual emoji from the potentially older firmware emoji font.  Don't
  // override it if a fallback font for emoji was returned but its PS name is
  // not NotoColorEmoji as we would otherwise always override an OEMs emoji
  // font.

  if (IsEmojiPresentationEmoji(fallback_priority) &&
      base::FeatureList::IsEnabled(features::kGMSCoreEmoji)) {
    if (family_name.empty() || skia_fallback_is_noto_color_emoji()) {
      const FontPlatformData* emoji_gms_core_font = GetFontPlatformData(
          font_description,
          FontFaceCreationParams(AtomicString(kNotoColorEmojiCompat)));
      if (emoji_gms_core_font) {
        SkTypeface* probe_coverage_typeface = emoji_gms_core_font->Typeface();
        if (probe_coverage_typeface &&
            probe_coverage_typeface->unicharToGlyph(c)) {
          return FontDataFromFontPlatformData(emoji_gms_core_font);
        }
      }
    }
  }

  // Remaining case, if fallback priority is not emoij or the GMS core emoji
  // font was not found or an OEM emoji font was not to be overridden.

  if (family_name.empty())
    return GetLastResortFallbackFont(font_description);

  return FontDataFromFontPlatformData(GetFontPlatformData(
      font_description, FontFaceCreationParams(family_name)));
}

// static
AtomicString FontCache::GetGenericFamilyNameForScript(
    const AtomicString& family_name,
    const AtomicString& generic_family_name_fallback,
    const FontDescription& font_description) {
  // If this is a locale-specifc family name, |FontCache| can handle different
  // typefaces per locale. Let it handle.
  if (GetLocaleSpecificFamilyName(family_name))
    return family_name;

  // If monospace, do not apply CJK hack to find i18n fonts, because
  // i18n fonts are likely not monospace. Monospace is mostly used
  // for code, but when i18n characters appear in monospace, system
  // fallback can still render the characters.
  if (family_name == font_family_names::kMonospace)
    return family_name;

  // The CJK hack below should be removed, at latest when we have
  // serif and sans-serif versions of CJK fonts. Until then, limit it
  // to only when the content locale is available. crbug.com/652146
  const LayoutLocale* content_locale = font_description.Locale();
  if (!content_locale)
    return generic_family_name_fallback;

  // This is a hack to use the preferred font for CJK scripts.
  // TODO(kojii): This logic disregards either generic family name
  // or locale. We need an API that honors both to find appropriate
  // fonts. crbug.com/642340
  UChar32 exampler_char;
  switch (content_locale->GetScript()) {
    case USCRIPT_SIMPLIFIED_HAN:
    case USCRIPT_TRADITIONAL_HAN:
    case USCRIPT_KATAKANA_OR_HIRAGANA:
      exampler_char = 0x4E00;  // A common character in Japanese and Chinese.
      break;
    case USCRIPT_HANGUL:
      exampler_char = 0xAC00;
      break;
    default:
      // For other scripts, use the default generic family mapping logic.
      return generic_family_name_fallback;
  }

  sk_sp<SkFontMgr> font_manager(skia::DefaultFontMgr());
  return GetFamilyNameForCharacter(font_manager.get(), exampler_char,
                                   font_description, nullptr,
                                   FontFallbackPriority::kText);
}

}  // namespace blink
