/*
 * Copyright (c) 2006, 2007, 2008, 2009, 2010, 2012 Google Inc. All rights
 * reserved.
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

#include "third_party/blink/renderer/platform/fonts/win/font_fallback_win.h"

#include <unicode/uchar.h>

#include <limits>

#include "base/check_op.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_priority.h"
#include "third_party/blink/renderer/platform/text/icu_error.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace blink {

namespace {

inline bool IsFontPresent(const char* font_name_utf8,
                          const SkFontMgr& font_manager) {
  sk_sp<SkTypeface> tf(
      font_manager.matchFamilyStyle(font_name_utf8, SkFontStyle()));
  if (!tf)
    return false;

  if (RuntimeEnabledFeatures::FontPresentWinEnabled()) {
    return true;
  }

  const String font_name = String::FromUTF8(font_name_utf8);
  SkTypeface::LocalizedStrings* actual_families =
      tf->createFamilyNameIterator();
  bool matches_requested_family = false;
  SkTypeface::LocalizedString actual_family;
  while (actual_families->next(&actual_family)) {
    if (DeprecatedEqualIgnoringCase(
            font_name, String::FromUTF8(actual_family.fString.c_str()))) {
      matches_requested_family = true;
      break;
    }
  }
  actual_families->unref();

  return matches_requested_family;
}

const char* FirstAvailableFont(
    base::span<const char* const> candidate_family_names,
    const SkFontMgr& font_manager) {
  for (const char* family : candidate_family_names) {
    if (IsFontPresent(family, font_manager)) {
      return family;
    }
  }
  return nullptr;
}

struct FontMapping {
  const char* FirstAvailableFont(const SkFontMgr& font_manager) {
    if (!candidate_family_names.empty()) {
      family_name =
          blink::FirstAvailableFont(candidate_family_names, font_manager);
      candidate_family_names = {};
    }
    return family_name;
  }

  const char* family_name;
  base::span<const char* const> candidate_family_names;
};

struct ScriptToFontFamilies {
  UScriptCode script;
  base::span<const char* const> families;
};

// A simple mapping from UScriptCode to family name. This is a sparse array,
// which works well since the range of UScriptCode values is small.
class ScriptToFontMap {
 public:
  static constexpr UScriptCode kSize = USCRIPT_CODE_LIMIT;

  FontMapping& operator[](UScriptCode script) { return mappings_[script]; }

  void Set(base::span<const ScriptToFontFamilies> families) {
    for (const auto& family : families) {
      mappings_[family.script].candidate_family_names = family.families;
    }
  }

 private:
  FontMapping mappings_[kSize];
};

const AtomicString& FindMonospaceFontForScript(UScriptCode script) {
  if (script == USCRIPT_ARABIC || script == USCRIPT_HEBREW) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(AtomicString, kCourierNew, ("courier new"));
    return kCourierNew;
  }
  return g_null_atom;
}

void InitializeScriptFontMap(ScriptToFontMap& script_font_map) {
  // For the following scripts, multiple fonts may be listed. They are tried
  // in order. The first slot is preferred but the font may not be available,
  // if so the remaining slots are tried in order.
  // In general the order is the Windows 10 font follow by the 8.1, 8.0 and
  // finally the font for Windows 7.
  // For scripts where an optional or region specific font may be available
  // that should be listed before the generic one.
  // Based on the "Script and Font Support in Windows" MSDN documentation [1]
  // with overrides and additional fallbacks as needed.
  // 1: https://msdn.microsoft.com/en-us/goglobal/bb688099.aspx
  static const char* const kArabicFonts[] = {"Tahoma", "Segoe UI"};
  static const char* const kArmenianFonts[] = {"Segoe UI", "Sylfaen"};
  static const char* const kBengaliFonts[] = {"Nirmala UI", "Vrinda"};
  static const char* const kBrahmiFonts[] = {"Segoe UI Historic"};
  static const char* const kBrailleFonts[] = {"Segoe UI Symbol"};
  static const char* const kBugineseFonts[] = {"Leelawadee UI"};
  static const char* const kCanadianAaboriginalFonts[] = {"Gadugi", "Euphemia"};
  static const char* const kCarianFonts[] = {"Segoe UI Historic"};
  static const char* const kCherokeeFonts[] = {"Gadugi", "Plantagenet"};
  static const char* const kCopticFonts[] = {"Segoe UI Symbol"};
  static const char* const kCuneiformFonts[] = {"Segoe UI Historic"};
  static const char* const kCypriotFonts[] = {"Segoe UI Historic"};
  static const char* const kCyrillicFonts[] = {"Times New Roman"};
  static const char* const kDeseretFonts[] = {"Segoe UI Symbol"};
  static const char* const kDevanagariFonts[] = {"Nirmala UI", "Mangal"};
  static const char* const kEgyptianHieroglyphsFonts[] = {"Segoe UI Historic"};
  static const char* const kEthiopicFonts[] = {"Nyala",
                                               "Abyssinica SIL",
                                               "Ethiopia Jiret",
                                               "Visual Geez Unicode",
                                               "GF Zemen Unicode",
                                               "Ebrima"};
  static const char* const kGeorgianFonts[] = {"Sylfaen", "Segoe UI"};
  static const char* const kGlagoliticFonts[] = {"Segoe UI Historic",
                                                 "Segoe UI Symbol"};
  static const char* const kGothicFonts[] = {"Segoe UI Historic",
                                             "Segoe UI Symbol"};
  static const char* const kGreekFonts[] = {"Times New Roman"};
  static const char* const kGujaratiFonts[] = {"Nirmala UI", "Shruti"};
  static const char* const kGurmukhiFonts[] = {"Nirmala UI", "Raavi"};
  static const char* const kHangulFonts[] = {"Noto Sans KR", "Noto Sans CJK KR",
                                             "Malgun Gothic", "Gulim"};
  static const char* const kHangulFontsNoNoto[] = {"Malgun Gothic", "Gulim"};
  static const char* const kHebrewFonts[] = {"David", "Segoe UI"};
  static const char* const kImperialAramaicFonts[] = {"Segoe UI Historic"};
  static const char* const kInscriptionalPahlaviFonts[] = {"Segoe UI Historic"};
  static const char* const kInscriptionalParthianFonts[] = {
      "Segoe UI Historic"};
  static const char* const kJavaneseFonts[] = {"Javanese Text"};
  static const char* const kKannadaFonts[] = {"Tunga", "Nirmala UI"};
  static const char* const kKatakanaOrHiraganaFonts[] = {
      "Noto Sans JP", "Noto Sans CJK JP", "Meiryo",
      "Yu Gothic",    "MS PGothic",       "Microsoft YaHei"};
  static const char* const kKatakanaOrHiraganaFontsNoNoto[] = {
      "Meiryo", "Yu Gothic", "MS PGothic", "Microsoft YaHei"};
  static const char* const kKharoshthiFonts[] = {"Segoe UI Historic"};
  // Try Khmer OS before Vista fonts as it goes along better with Latin
  // and looks better/larger for the same size.
  static const char* const kKhmerFonts[] = {
      "Leelawadee UI", "Khmer UI", "Khmer OS", "MoolBoran", "DaunPenh"};
  static const char* const kLaoFonts[] = {"Leelawadee UI", "Lao UI",
                                          "DokChampa",     "Saysettha OT",
                                          "Phetsarath OT", "Code2000"};
  static const char* const kLatinFonts[] = {"Times New Roman"};
  static const char* const kLisuFonts[] = {"Segoe UI"};
  static const char* const kLycianFonts[] = {"Segoe UI Historic"};
  static const char* const kLydianFonts[] = {"Segoe UI Historic"};
  static const char* const kMalayalamFonts[] = {"Nirmala UI", "Kartika"};
  static const char* const kMeroiticCursiveFonts[] = {"Segoe UI Historic",
                                                      "Segoe UI Symbol"};
  static const char* const kMongolianFonts[] = {"Mongolian Baiti"};
  static const char* const kMyanmarFonts[] = {
      "Myanmar Text", "Padauk", "Parabaik", "Myanmar3", "Code2000"};
  static const char* const kNewTaiLueFonts[] = {"Microsoft New Tai Lue"};
  static const char* const kNkoFonts[] = {"Ebrima"};
  static const char* const kOghamFonts[] = {"Segoe UI Historic",
                                            "Segoe UI Symbol"};
  static const char* const kOlChikiFonts[] = {"Nirmala UI"};
  static const char* const kOldItalicFonts[] = {"Segoe UI Historic",
                                                "Segoe UI Symbol"};
  static const char* const kOldPersianFonts[] = {"Segoe UI Historic"};
  static const char* const kOldSouthArabianFonts[] = {"Segoe UI Historic"};
  static const char* const kOriyaFonts[] = {"Kalinga", "ori1Uni", "Lohit Oriya",
                                            "Nirmala UI"};
  static const char* const kOrkhonFonts[] = {"Segoe UI Historic",
                                             "Segoe UI Symbol"};
  static const char* const kOsmanyaFonts[] = {"Ebrima"};
  static const char* const kPhagsPaFonts[] = {"Microsoft PhagsPa"};
  static const char* const kRunicFonts[] = {"Segoe UI Historic",
                                            "Segoe UI Symbol"};
  static const char* const kShavianFonts[] = {"Segoe UI Historic"};
  static const char* const kSimplifiedHanFonts[] = {
      "Noto Sans SC", "Noto Sans CJK SC", "Microsoft YaHei", "simsun"};
  static const char* const kSimplifiedHanFontsNoNoto[] = {"Microsoft YaHei",
                                                          "simsun"};
  static const char* const kSinhalaFonts[] = {"Iskoola Pota", "AksharUnicode",
                                              "Nirmala UI"};
  static const char* const kSoraSompengFonts[] = {"Nirmala UI"};
  static const char* const kSymbolsFonts[] = {"Segoe UI Symbol"};
  static const char* const kSyriacFonts[] = {"Estrangelo Edessa",
                                             "Estrangelo Nisibin", "Code2000"};
  static const char* const kTaiLeFonts[] = {"Microsoft Tai Le"};
  static const char* const kTamilFonts[] = {"Nirmala UI", "Latha"};
  static const char* const kTeluguFonts[] = {"Nirmala UI", "Gautami"};
  static const char* const kThaanaFonts[] = {"MV Boli"};
  static const char* const kThaiFonts[] = {"Tahoma", "Leelawadee UI",
                                           "Leelawadee"};
  static const char* const kTibetanFonts[] = {"Microsoft Himalaya", "Jomolhari",
                                              "Tibetan Machine Uni"};
  static const char* const kTifinaghFonts[] = {"Ebrima"};
  static const char* const kTraditionalHanFonts[] = {
      "Noto Sans TC", "Noto Sans CJK TC", "Microsoft JhengHei", "pmingli"};
  static const char* const kTraditionalHanFontsNoNoto[] = {"Microsoft JhengHei",
                                                           "pmingli"};
  static const char* const kVaiFonts[] = {"Ebrima"};
  static const char* const kYiFonts[] = {"Microsoft Yi Baiti", "Nuosu SIL",
                                         "Code2000"};

  static const ScriptToFontFamilies kScriptToFontFamilies[] = {
      {USCRIPT_ARABIC, kArabicFonts},
      {USCRIPT_ARMENIAN, kArmenianFonts},
      {USCRIPT_BENGALI, kBengaliFonts},
      {USCRIPT_BRAHMI, kBrahmiFonts},
      {USCRIPT_BRAILLE, kBrailleFonts},
      {USCRIPT_BUGINESE, kBugineseFonts},
      {USCRIPT_CANADIAN_ABORIGINAL, kCanadianAaboriginalFonts},
      {USCRIPT_CARIAN, kCarianFonts},
      {USCRIPT_CHEROKEE, kCherokeeFonts},
      {USCRIPT_COPTIC, kCopticFonts},
      {USCRIPT_CUNEIFORM, kCuneiformFonts},
      {USCRIPT_CYPRIOT, kCypriotFonts},
      {USCRIPT_CYRILLIC, kCyrillicFonts},
      {USCRIPT_DESERET, kDeseretFonts},
      {USCRIPT_DEVANAGARI, kDevanagariFonts},
      {USCRIPT_EGYPTIAN_HIEROGLYPHS, kEgyptianHieroglyphsFonts},
      {USCRIPT_ETHIOPIC, kEthiopicFonts},
      {USCRIPT_GEORGIAN, kGeorgianFonts},
      {USCRIPT_GLAGOLITIC, kGlagoliticFonts},
      {USCRIPT_GOTHIC, kGothicFonts},
      {USCRIPT_GREEK, kGreekFonts},
      {USCRIPT_GUJARATI, kGujaratiFonts},
      {USCRIPT_GURMUKHI, kGurmukhiFonts},
      {USCRIPT_HANGUL, kHangulFonts},
      {USCRIPT_HEBREW, kHebrewFonts},
      {USCRIPT_HIRAGANA, kKatakanaOrHiraganaFonts},
      {USCRIPT_IMPERIAL_ARAMAIC, kImperialAramaicFonts},
      {USCRIPT_INSCRIPTIONAL_PAHLAVI, kInscriptionalPahlaviFonts},
      {USCRIPT_INSCRIPTIONAL_PARTHIAN, kInscriptionalParthianFonts},
      {USCRIPT_JAVANESE, kJavaneseFonts},
      {USCRIPT_KANNADA, kKannadaFonts},
      {USCRIPT_KATAKANA, kKatakanaOrHiraganaFonts},
      {USCRIPT_KATAKANA_OR_HIRAGANA, kKatakanaOrHiraganaFonts},
      {USCRIPT_KHAROSHTHI, kKharoshthiFonts},
      {USCRIPT_KHMER, kKhmerFonts},
      {USCRIPT_LAO, kLaoFonts},
      {USCRIPT_LATIN, kLatinFonts},
      {USCRIPT_LISU, kLisuFonts},
      {USCRIPT_LYCIAN, kLycianFonts},
      {USCRIPT_LYDIAN, kLydianFonts},
      {USCRIPT_MALAYALAM, kMalayalamFonts},
      {USCRIPT_MEROITIC_CURSIVE, kMeroiticCursiveFonts},
      {USCRIPT_MONGOLIAN, kMongolianFonts},
      {USCRIPT_MYANMAR, kMyanmarFonts},
      {USCRIPT_NEW_TAI_LUE, kNewTaiLueFonts},
      {USCRIPT_NKO, kNkoFonts},
      {USCRIPT_OGHAM, kOghamFonts},
      {USCRIPT_OL_CHIKI, kOlChikiFonts},
      {USCRIPT_OLD_ITALIC, kOldItalicFonts},
      {USCRIPT_OLD_PERSIAN, kOldPersianFonts},
      {USCRIPT_OLD_SOUTH_ARABIAN, kOldSouthArabianFonts},
      {USCRIPT_ORIYA, kOriyaFonts},
      {USCRIPT_ORKHON, kOrkhonFonts},
      {USCRIPT_OSMANYA, kOsmanyaFonts},
      {USCRIPT_PHAGS_PA, kPhagsPaFonts},
      {USCRIPT_RUNIC, kRunicFonts},
      {USCRIPT_SHAVIAN, kShavianFonts},
      {USCRIPT_SIMPLIFIED_HAN, kSimplifiedHanFonts},
      {USCRIPT_SINHALA, kSinhalaFonts},
      {USCRIPT_SORA_SOMPENG, kSoraSompengFonts},
      {USCRIPT_SYMBOLS, kSymbolsFonts},
      {USCRIPT_SYRIAC, kSyriacFonts},
      {USCRIPT_TAI_LE, kTaiLeFonts},
      {USCRIPT_TAMIL, kTamilFonts},
      {USCRIPT_TELUGU, kTeluguFonts},
      {USCRIPT_THAANA, kThaanaFonts},
      {USCRIPT_THAI, kThaiFonts},
      {USCRIPT_TIBETAN, kTibetanFonts},
      {USCRIPT_TIFINAGH, kTifinaghFonts},
      {USCRIPT_TRADITIONAL_HAN, kTraditionalHanFonts},
      {USCRIPT_VAI, kVaiFonts},
      {USCRIPT_YI, kYiFonts}};
  script_font_map.Set(kScriptToFontFamilies);

  if (!RuntimeEnabledFeatures::FontSystemFallbackNotoCjkEnabled())
      [[unlikely]] {
    const ScriptToFontFamilies no_noto[] = {
        {USCRIPT_HANGUL, kHangulFontsNoNoto},
        {USCRIPT_HIRAGANA, kKatakanaOrHiraganaFontsNoNoto},
        {USCRIPT_KATAKANA, kKatakanaOrHiraganaFontsNoNoto},
        {USCRIPT_KATAKANA_OR_HIRAGANA, kKatakanaOrHiraganaFontsNoNoto},
        {USCRIPT_SIMPLIFIED_HAN, kSimplifiedHanFontsNoNoto},
        {USCRIPT_TRADITIONAL_HAN, kTraditionalHanFontsNoNoto},
    };
    script_font_map.Set(no_noto);
  }

  // Initialize the locale-dependent mapping from system locale.
  UScriptCode han_script = LayoutLocale::GetSystem().GetScriptForHan();
  DCHECK_NE(han_script, USCRIPT_HAN);
  const FontMapping& han_mapping = script_font_map[han_script];
  if (!han_mapping.candidate_family_names.empty()) {
    script_font_map[USCRIPT_HAN].candidate_family_names =
        han_mapping.candidate_family_names;
  }
}

// There are a lot of characters in USCRIPT_COMMON that can be covered
// by fonts for scripts closely related to them. See
// http://unicode.org/cldr/utility/list-unicodeset.jsp?a=[:Script=Common:]
// FIXME: make this more efficient with a wider coverage
UScriptCode GetScriptBasedOnUnicodeBlock(int ucs4) {
  UBlockCode block = ublock_getCode(ucs4);
  switch (block) {
    case UBLOCK_CJK_SYMBOLS_AND_PUNCTUATION:
      return USCRIPT_HAN;
    case UBLOCK_HIRAGANA:
    case UBLOCK_KATAKANA:
      return USCRIPT_KATAKANA_OR_HIRAGANA;
    case UBLOCK_ARABIC:
      return USCRIPT_ARABIC;
    case UBLOCK_THAI:
      return USCRIPT_THAI;
    case UBLOCK_GREEK:
      return USCRIPT_GREEK;
    case UBLOCK_DEVANAGARI:
      // For Danda and Double Danda (U+0964, U+0965), use a Devanagari
      // font for now although they're used by other scripts as well.
      // Without a context, we can't do any better.
      return USCRIPT_DEVANAGARI;
    case UBLOCK_ARMENIAN:
      return USCRIPT_ARMENIAN;
    case UBLOCK_GEORGIAN:
      return USCRIPT_GEORGIAN;
    case UBLOCK_KANNADA:
      return USCRIPT_KANNADA;
    case UBLOCK_GOTHIC:
      return USCRIPT_GOTHIC;
    default:
      return USCRIPT_COMMON;
  }
}

UScriptCode GetScript(int ucs4) {
  ICUError err;
  UScriptCode script = uscript_getScript(ucs4, &err);
  // If script is invalid, common or inherited or there's an error,
  // infer a script based on the unicode block of a character.
  if (script <= USCRIPT_INHERITED || U_FAILURE(err))
    script = GetScriptBasedOnUnicodeBlock(ucs4);
  return script;
}

const char* AvailableColorEmojiFont(const SkFontMgr& font_manager) {
  static const char* const kEmojiFonts[] = {"Segoe UI Emoji",
                                            "Segoe UI Symbol"};
  static const char* emoji_font = nullptr;
  // `std::once()` may cause hangs. crbug.com/349456407
  static bool initialized = false;
  if (!initialized) {
    emoji_font = FirstAvailableFont(kEmojiFonts, font_manager);
    initialized = true;
  }
  return emoji_font;
}

const char* AvailableMonoEmojiFont(const SkFontMgr& font_manager) {
  static const char* const kEmojiFonts[] = {"Segoe UI Symbol",
                                            "Segoe UI Emoji"};
  static const char* emoji_font = nullptr;
  // `std::once()` may cause hangs. crbug.com/349456407
  static bool initialized = false;
  if (!initialized) {
    emoji_font = FirstAvailableFont(kEmojiFonts, font_manager);
    initialized = true;
  }
  return emoji_font;
}

const char* FirstAvailableMathFont(const SkFontMgr& font_manager) {
  static const char* const kMathFonts[] = {"Cambria Math", "Segoe UI Symbol",
                                           "Code2000"};
  static const char* math_font = nullptr;
  // `std::once()` may cause hangs. crbug.com/349456407
  static bool initialized = false;
  if (!initialized) {
    math_font = FirstAvailableFont(kMathFonts, font_manager);
    initialized = true;
  }
  return math_font;
}

const AtomicString& GetColorEmojiFont(const SkFontMgr& font_manager) {
  // Calling `AvailableColorEmojiFont()` from `DEFINE_THREAD_SAFE_STATIC_LOCAL`
  // may cause hangs. crbug.com/349456407
  DEFINE_THREAD_SAFE_STATIC_LOCAL(AtomicString, emoji_font, (g_empty_atom));
  if (emoji_font.empty() && !emoji_font.IsNull()) {
    emoji_font = AtomicString(AvailableColorEmojiFont(font_manager));
    CHECK(!emoji_font.empty() || emoji_font.IsNull());
  }
  return emoji_font;
}

const AtomicString& GetMonoEmojiFont(const SkFontMgr& font_manager) {
  // Calling `AvailableMonoEmojiFont()` from `DEFINE_THREAD_SAFE_STATIC_LOCAL`
  // may cause hangs. crbug.com/349456407
  DEFINE_THREAD_SAFE_STATIC_LOCAL(AtomicString, emoji_font, (g_empty_atom));
  if (emoji_font.empty() && !emoji_font.IsNull()) {
    emoji_font = AtomicString(AvailableMonoEmojiFont(font_manager));
    CHECK(!emoji_font.empty() || emoji_font.IsNull());
  }
  return emoji_font;
}

const AtomicString& GetMathFont(const SkFontMgr& font_manager) {
  // Calling `AvailableMonoEmojiFont()` from `DEFINE_THREAD_SAFE_STATIC_LOCAL`
  // may cause hangs. crbug.com/349456407
  DEFINE_THREAD_SAFE_STATIC_LOCAL(AtomicString, math_font, (g_empty_atom));
  if (math_font.empty() && !math_font.IsNull()) {
    math_font = AtomicString(FirstAvailableMathFont(font_manager));
    CHECK(!math_font.empty() || math_font.IsNull());
  }
  return math_font;
}

const AtomicString& GetFontBasedOnUnicodeBlock(UBlockCode block_code,
                                               const SkFontMgr& font_manager) {
  switch (block_code) {
    case UBLOCK_EMOTICONS:
    case UBLOCK_ENCLOSED_ALPHANUMERIC_SUPPLEMENT:
      // We call this function only when FallbackPriority is not kEmojiEmoji or
      // kEmojiEmojiWithVS, so we need a text presentation of emoji.
      return GetMonoEmojiFont(font_manager);
    case UBLOCK_PLAYING_CARDS:
    case UBLOCK_MISCELLANEOUS_SYMBOLS:
    case UBLOCK_MISCELLANEOUS_SYMBOLS_AND_ARROWS:
    case UBLOCK_MISCELLANEOUS_SYMBOLS_AND_PICTOGRAPHS:
    case UBLOCK_TRANSPORT_AND_MAP_SYMBOLS:
    case UBLOCK_ALCHEMICAL_SYMBOLS:
    case UBLOCK_DINGBATS:
    case UBLOCK_GOTHIC: {
      DEFINE_THREAD_SAFE_STATIC_LOCAL(AtomicString, kSymbolFont,
                                      ("Segoe UI Symbol"));
      return kSymbolFont;
    }
    case UBLOCK_ARROWS:
    case UBLOCK_MATHEMATICAL_OPERATORS:
    case UBLOCK_MISCELLANEOUS_TECHNICAL:
    case UBLOCK_GEOMETRIC_SHAPES:
    case UBLOCK_MISCELLANEOUS_MATHEMATICAL_SYMBOLS_A:
    case UBLOCK_SUPPLEMENTAL_ARROWS_A:
    case UBLOCK_SUPPLEMENTAL_ARROWS_B:
    case UBLOCK_MISCELLANEOUS_MATHEMATICAL_SYMBOLS_B:
    case UBLOCK_SUPPLEMENTAL_MATHEMATICAL_OPERATORS:
    case UBLOCK_MATHEMATICAL_ALPHANUMERIC_SYMBOLS:
    case UBLOCK_ARABIC_MATHEMATICAL_ALPHABETIC_SYMBOLS:
    case UBLOCK_GEOMETRIC_SHAPES_EXTENDED:
      return GetMathFont(font_manager);
    default:
      return g_null_atom;
  }
}

}  // namespace

// FIXME: this is font fallback code version 0.1
//  - Cover all the scripts
//  - Get the default font for each script/generic family from the
//    preference instead of hardcoding in the source.
//    (at least, read values from the registry for IE font settings).
//  - Support generic families (from FontDescription)
//  - If the default font for a script is not available,
//    try some more fonts known to support it. Finally, we can
//    use EnumFontFamilies or similar APIs to come up with a list of
//    fonts supporting the script and cache the result.
//  - Consider using UnicodeSet (or UnicodeMap) converted from
//    GLYPHSET (BMP) or directly read from truetype cmap tables to
//    keep track of which character is supported by which font
//  - Update script_font_cache in response to WM_FONTCHANGE

const AtomicString& GetFontFamilyForScript(
    UScriptCode script,
    FontDescription::GenericFamilyType generic,
    const SkFontMgr& font_manager) {
  if (script < 0 || script >= ScriptToFontMap::kSize) [[unlikely]] {
    return g_null_atom;
  }

  if (generic == FontDescription::kMonospaceFamily) {
    if (const AtomicString& family = FindMonospaceFontForScript(script)) {
      return family;
    }
  }

  // Try the `AtomicString` cache first. `AtomicString` must be per thread, and
  // thus it can't be added to `ScriptToFontMap`.
  struct AtomicFamilies {
    std::optional<AtomicString> families[ScriptToFontMap::kSize];
  };
  DEFINE_THREAD_SAFE_STATIC_LOCAL(AtomicFamilies, families, ());
  std::optional<AtomicString>& family = families.families[script];
  if (family) {
    return *family;
  }

  static ScriptToFontMap script_font_map;
  static std::once_flag once_flag;
  std::call_once(once_flag, [] { InitializeScriptFontMap(script_font_map); });
  family.emplace(script_font_map[script].FirstAvailableFont(font_manager));
  return *family;
}

// FIXME:
//  - Handle 'Inherited', 'Common' and 'Unknown'
//    (see http://www.unicode.org/reports/tr24/#Usage_Model )
//    For 'Inherited' and 'Common', perhaps we need to
//    accept another parameter indicating the previous family
//    and just return it.
//  - All the characters (or characters up to the point a single
//    font can cover) need to be taken into account
const AtomicString& GetFallbackFamily(
    UChar32 character,
    FontDescription::GenericFamilyType generic,
    const LayoutLocale* content_locale,
    FontFallbackPriority fallback_priority,
    const SkFontMgr& font_manager,
    UScriptCode& script_out) {
  DCHECK(character);
  if (IsEmojiPresentationEmoji(fallback_priority)) [[unlikely]] {
    if (const AtomicString& family = GetColorEmojiFont(font_manager)) {
      script_out = USCRIPT_INVALID_CODE;
      return family;
    }
  } else if (IsTextPresentationEmoji(fallback_priority)) [[unlikely]] {
    if (const AtomicString& family = GetMonoEmojiFont(font_manager)) {
      script_out = USCRIPT_INVALID_CODE;
      return family;
    }
  } else {
    const UBlockCode block = ublock_getCode(character);
    if (const AtomicString& family =
            GetFontBasedOnUnicodeBlock(block, font_manager)) {
      script_out = USCRIPT_INVALID_CODE;
      return family;
    }
  }

  UScriptCode script = GetScript(character);

  // For the full-width ASCII characters (U+FF00 - U+FF5E), use the font for
  // Han (determined in a locale-dependent way above). Full-width ASCII
  // characters are rather widely used in Japanese and Chinese documents and
  // they're fully covered by Chinese, Japanese and Korean fonts.
  if (0xFF00 < character && character < 0xFF5F)
    script = USCRIPT_HAN;

  if (script == USCRIPT_COMMON)
    script = GetScriptBasedOnUnicodeBlock(character);

  // For unified-Han scripts, try the lang attribute, system, or
  // accept-languages.
  if (script == USCRIPT_HAN) {
    if (const LayoutLocale* locale_for_han =
            LayoutLocale::LocaleForHan(content_locale))
      script = locale_for_han->GetScriptForHan();
    // If still unknown, USCRIPT_HAN uses UI locale.
    // See initializeScriptFontMap().
  }

  script_out = script;

  // TODO(kojii): Limiting `GetFontFamilyForScript()` only to BMP may need
  // review to match the modern environment. This was done in 2010 for
  // https://bugs.webkit.org/show_bug.cgi?id=35605.
  if (character <= 0xFFFF) {
    if (const AtomicString& family =
            GetFontFamilyForScript(script, generic, font_manager)) {
      return family;
    }
  }

  // Another lame work-around to cover non-BMP characters.
  // If the font family for script is not found or the character is
  // not in BMP (> U+FFFF), we resort to the hard-coded list of
  // fallback fonts for now.
  int plane = character >> 16;
  switch (plane) {
    case 1: {
      DEFINE_THREAD_SAFE_STATIC_LOCAL(AtomicString, kPlane1, ("code2001"));
      return kPlane1;
    }
    case 2:
      // Use a Traditional Chinese ExtB font if in Traditional Chinese locale.
      // Otherwise, use a Simplified Chinese ExtB font. Windows Japanese
      // fonts do support a small subset of ExtB (that are included in JIS X
      // 0213), but its coverage is rather sparse.
      // Eventually, this should be controlled by lang/xml:lang.
      if (icu::Locale::getDefault() == icu::Locale::getTraditionalChinese()) {
        DEFINE_THREAD_SAFE_STATIC_LOCAL(AtomicString, kPlane2zht,
                                        ("pmingliu-extb"));
        return kPlane2zht;
      }
      DEFINE_THREAD_SAFE_STATIC_LOCAL(AtomicString, kPlane2zhs,
                                      ("simsun-extb"));
      return kPlane2zhs;
  }

  DEFINE_THREAD_SAFE_STATIC_LOCAL(AtomicString, kLastResort,
                                  ("lucida sans unicode"));
  return kLastResort;
}

}  // namespace blink
