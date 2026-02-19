/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/platform/text/character.h"

#include <unicode/uchar.h>
#include <unicode/ucptrie.h>
#include <unicode/uobject.h>
#include <unicode/uscript.h>

#include <algorithm>

#include "base/synchronization/lock.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/character_property_data.h"
#include "third_party/blink/renderer/platform/text/icu_error.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"

namespace blink {

namespace {

UCPTrie* CreateTrie() {
  // Create a Trie from the value array.
  ICUError error;
  UCPTrie* trie = ucptrie_openFromBinary(
      UCPTrieType::UCPTRIE_TYPE_FAST, UCPTrieValueWidth::UCPTRIE_VALUE_BITS_16,
      kSerializedCharacterData, kSerializedCharacterDataSize, nullptr, &error);
  DCHECK_EQ(error, U_ZERO_ERROR);
  return trie;
}

inline CharacterProperty GetProperty(UChar32 c) {
  static const UCPTrie* trie = CreateTrie();
  static_assert(sizeof(CharacterProperty) == 2);
  const auto value = UNSAFE_TODO(UCPTRIE_FAST_GET(trie, UCPTRIE_16, c));
  return CharacterProperty(value);
}

base::Lock& GetFreezePatternLock() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(base::Lock, lock, ());
  return lock;
}

}  // namespace

void Character::ApplyPatternAndFreezeIfEmpty(icu::UnicodeSet* unicodeSet,
                                             const char* pattern) {
  base::AutoLock locker(GetFreezePatternLock());
  if (!unicodeSet->isEmpty()) {
    return;
  }
  blink::ICUError err;
  // Use ICU's invariant-character initialization method.
  unicodeSet->applyPattern(icu::UnicodeString(pattern, -1, US_INV), err);
  unicodeSet->freeze();
  DCHECK_EQ(err, U_ZERO_ERROR);
}

bool Character::IsUprightInMixedVertical(UChar32 character) {
  return u_getIntPropertyValue(character,
                               UProperty::UCHAR_VERTICAL_ORIENTATION) !=
         UVerticalOrientation::U_VO_ROTATED;
}

bool Character::IsCJKIdeographOrSymbolSlow(UChar32 c) {
  return GetProperty(c).is_cjk_ideograph_or_symbol;
}

bool Character::IsBidiControl(UChar32 character) {
  return GetProperty(character).is_bidi_control;
}

bool Character::IsHangulSlow(UChar32 character) {
  return GetProperty(character).is_hangul;
}

// static
HanKerningCharType Character::GetHanKerningCharType(UChar32 character) {
  return GetProperty(character).han_kerning;
}

// static
EastAsianSpacingType Character::GetEastAsianSpacingType(UChar32 character) {
  return GetProperty(character).east_asian_spacing;
}

bool Character::MaybeHanKerningOpenSlow(UChar32 ch) {
  // See `HanKerning::GetCharType`.
  const HanKerningCharType type = Character::GetHanKerningCharType(ch);
  return type == HanKerningCharType::kOpen ||
         type == HanKerningCharType::kOpenQuote;
}

bool Character::MaybeHanKerningCloseSlow(UChar32 ch) {
  // See `HanKerning::GetCharType`.
  const HanKerningCharType type = Character::GetHanKerningCharType(ch);
  return type == HanKerningCharType::kClose ||
         type == HanKerningCharType::kCloseQuote;
}

bool Character::CanTextDecorationSkipInk(UChar32 codepoint) {
  if (codepoint == uchar::kSolidus || codepoint == uchar::kReverseSolidus ||
      codepoint == uchar::kLowLine) {
    return false;
  }

  if (Character::IsCJKIdeographOrSymbol(codepoint))
    return false;

  UBlockCode block = ublock_getCode(codepoint);
  switch (block) {
    // These blocks contain CJK characters we don't want to skip ink, but are
    // not ideograph that IsCJKIdeographOrSymbol() does not cover.
    case UBLOCK_HANGUL_JAMO:
    case UBLOCK_HANGUL_COMPATIBILITY_JAMO:
    case UBLOCK_HANGUL_SYLLABLES:
    case UBLOCK_HANGUL_JAMO_EXTENDED_A:
    case UBLOCK_HANGUL_JAMO_EXTENDED_B:
    case UBLOCK_LINEAR_B_IDEOGRAMS:
      return false;
    default:
      return true;
  }
}

bool Character::CanReceiveTextEmphasis(UChar32 c) {
  unicode::CharCategory category = unicode::Category(c);
  if (category & (unicode::kSeparator_Space | unicode::kSeparator_Line |
                  unicode::kSeparator_Paragraph | unicode::kOther_NotAssigned |
                  unicode::kOther_Control | unicode::kOther_Format)) {
    return false;
  }

  // Additional word-separator characters listed in CSS Text Level 3 Editor's
  // Draft 3 November 2010.
  // https://www.w3.org/TR/css-text-3/#word-separator
  if (c == uchar::kEthiopicWordspace || c == uchar::kAegeanWordSeparatorLine ||
      c == uchar::kAegeanWordSeparatorDot || c == uchar::kUgariticWordDivider ||
      c == uchar::kTibetanMarkIntersyllabicTsheg ||
      c == uchar::kTibetanMarkDelimiterTshegBstar) {
    return false;
  }

  if (RuntimeEnabledFeatures::TextEmphasisPunctuationExceptionsEnabled()) {
    // A set of exceptions for punctuation.
    switch (c) {
      // List from
      // https://drafts.csswg.org/css-text-decor/#text-emphasis-style-property
      case uchar::kNumberSign:
      case uchar::kPercentSign:
      case uchar::kAmpersand:
      case uchar::kCommercialAt:
      case uchar::kSectionSign:
      case uchar::kPilcrowSign:
      case uchar::kArabicIndicPerMilleSign:
      case uchar::kArabicIndicPerTenThousandSign:
      case uchar::kArabicPercentSign:
      case uchar::kPerMilleSign:
      case uchar::kPerTenThousandSign:
      case uchar::kTironianSignEt:
      case uchar::kReversedPilcrowSign:
      case uchar::kSwungDash:
      case uchar::kPartAlternationMark:
      // Characters with NFKD equivalence to the above.
      case uchar::kSmallNumberSign:
      case uchar::kSmallAmpersand:
      case uchar::kSmallPercentSign:
      case uchar::kSmallCommercialAt:
      case uchar::kFullwidthNumberSign:
      case uchar::kFullwidthPercentSign:
      case uchar::kFullwidthAmpersand:
      case uchar::kFullwidthCommercialAt:
        return true;
      default:
        break;
    }
  }

  // Punctuation
  if (category &
      (unicode::kPunctuation_Dash | unicode::kPunctuation_Open |
       unicode::kPunctuation_Close | unicode::kPunctuation_Connector |
       unicode::kPunctuation_Other | unicode::kPunctuation_InitialQuote |
       unicode::kPunctuation_FinalQuote)) {
    return false;
  }

  return true;
}

bool Character::IsEmojiTagSequence(UChar32 c) {
  // http://www.unicode.org/reports/tr51/proposed.html#valid-emoji-tag-sequences
  return (c >= uchar::kTagDigitZero && c <= uchar::kTagDigitNine) ||
         (c >= uchar::kTagLatinSmallLetterA &&
          c <= uchar::kTagLatinSmallLetterZ);
}

bool Character::IsExtendedPictographic(UChar32 c) {
  return u_hasBinaryProperty(c, UCHAR_EXTENDED_PICTOGRAPHIC);
}

bool Character::IsEmojiComponent(UChar32 c) {
  return u_hasBinaryProperty(c, UCHAR_EMOJI_COMPONENT);
}

namespace {

consteval bool MaybeEmojiPresentationForAscii(unsigned char ch) {
  constexpr auto kCopyRightSign = 0xA9;
  constexpr auto kRegisteredSign = 0xAE;
  return ch == kCopyRightSign || ch == kRegisteredSign ||
         Character::IsEmojiKeycapBase(ch);
}

template <std::size_t N, typename Function>
consteval auto GenerateTable(Function&& f) {
  std::array<bool, N> arr;
  for (unsigned char i = 0; i < N; ++i) {
    arr[i] = f(i);
  }
  return arr;
}

static const auto maybe_emoji_presentation_ascii =
    GenerateTable<128>([](int i) { return MaybeEmojiPresentationForAscii(i); });

}  // namespace

bool Character::MaybeEmojiPresentation(UChar32 c) {
  if (IsASCII(c)) [[likely]] {
    return maybe_emoji_presentation_ascii[c];
  }
  // Non-ascii characters.
  return c == uchar::kZeroWidthJoiner || IsInRange(c, 0x203C, 0x2B55) ||
         c == uchar::kVariationSelector15 || c == 0x3030 || c == 0x303D ||
         c == 0x3297 || c == 0x3299 || c == uchar::kVariationSelector16 ||
         c >= 65536;
}

bool Character::IsCommonOrInheritedScript(UChar32 character) {
  ICUError status;
  UScriptCode script = uscript_getScript(character, &status);
  return U_SUCCESS(status) &&
         (script == USCRIPT_COMMON || script == USCRIPT_INHERITED);
}

bool Character::IsPrivateUse(UChar32 character) {
  return unicode::Category(character) & unicode::kOther_PrivateUse;
}

bool Character::IsNonCharacter(UChar32 character) {
  return U_IS_UNICODE_NONCHAR(character);
}

bool Character::HasLikelyScript(UChar32 character) {
  ICUError err;
  UScriptCode script = uscript_getScript(character, &err);

  if (!U_SUCCESS(err))
    return false;

  if (RuntimeEnabledFeatures::ScriptBasedOnUnicodeBlockEnabled()) {
    if (script == USCRIPT_INHERITED || script == USCRIPT_COMMON) {
      // For characters whose ICU script is USCRIPT_INHERITED or
      // USCRIPT_COMMON, infer a likely script based on their Unicode block.
      // This helps select more accurate fallback fonts for
      // inherited marks, punctuation, and similar characters.
      script = GetScriptBasedOnUnicodeBlock(character);
    }
  }
  return script != USCRIPT_COMMON && script != USCRIPT_INHERITED;
}

// There are a lot of characters in USCRIPT_COMMON that can be covered
// by fonts for scripts closely related to them. See
// http://unicode.org/cldr/utility/list-unicodeset.jsp?a=[:Script=Common:]
// FIXME: make this more efficient with a wider coverage
UScriptCode Character::GetScriptBasedOnUnicodeBlock(int ucs4) {
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

bool Character::IsCursiveScript(UChar32 code_point) {
  ICUError err;
  UScriptCode script = uscript_getScript(code_point, &err);
  if (!U_SUCCESS(err)) {
    return false;
  }
  return script == USCRIPT_ARABIC || script == USCRIPT_HANIFI_ROHINGYA ||
         script == USCRIPT_MANDAIC || script == USCRIPT_MONGOLIAN ||
         script == USCRIPT_NKO || script == USCRIPT_PHAGS_PA ||
         script == USCRIPT_SYRIAC;
}

// https://w3c.github.io/mathml-core/#stretchy-operator-axis
static const UChar stretchy_operator_with_inline_axis[]{
    0x003D, 0x005E, 0x005F, 0x007E, 0x00AF, 0x02C6, 0x02C7, 0x02C9, 0x02CD,
    0x02DC, 0x02F7, 0x0302, 0x0332, 0x203E, 0x20D0, 0x20D1, 0x20D6, 0x20D7,
    0x20E1, 0x2190, 0x2192, 0x2194, 0x2198, 0x2199, 0x219C, 0x219D, 0x219E,
    0x21A0, 0x21A2, 0x21A3, 0x21A4, 0x21A6, 0x21A9, 0x21AA, 0x21AB, 0x21AC,
    0x21AD, 0x21B4, 0x21B9, 0x21BC, 0x21BD, 0x21C0, 0x21C1, 0x21C4, 0x21C6,
    0x21C7, 0x21C9, 0x21CB, 0x21CC, 0x21D0, 0x21D2, 0x21D4, 0x21DA, 0x21DB,
    0x21DC, 0x21DD, 0x21E0, 0x21E2, 0x21E4, 0x21E5, 0x21E6, 0x21E8, 0x21F0,
    0x21F6, 0x21FD, 0x21FE, 0x21FF, 0x23B4, 0x23B5, 0x23DC, 0x23DD, 0x23DE,
    0x23DF, 0x23E0, 0x23E1, 0x2500, 0x27F5, 0x27F6, 0x27F7, 0x27F8, 0x27F9,
    0x27FA, 0x27FB, 0x27FC, 0x27FD, 0x27FE, 0x27FF, 0x290C, 0x290D, 0x290E,
    0x290F, 0x2910, 0x294E, 0x2950, 0x2952, 0x2953, 0x2956, 0x2957, 0x295A,
    0x295B, 0x295E, 0x295F, 0x2B45, 0x2B46, 0xFE35, 0xFE36, 0xFE37, 0xFE38};

bool Character::IsVerticalMathCharacter(UChar32 text_content) {
  return text_content !=
             uchar::kArabicMathematicalOperatorMeemWithHahWithTatweel &&
         text_content != uchar::kArabicMathematicalOperatorHahWithDal &&
         !std::binary_search(
             stretchy_operator_with_inline_axis,
             UNSAFE_TODO(stretchy_operator_with_inline_axis +
                         std::size(stretchy_operator_with_inline_axis)),
             text_content);
}

UChar32 Character::FullwidthVariant(UChar32 code_point) {
  // ASCII printable characters (U+0021..U+007E) to full-width (U+FF01..U+FF5E).
  if (code_point >= 0x21 && code_point <= 0x7E) {
    return code_point + 0xFEE0;
  }

  // Half-width Katakana (U+FF61..U+FF9F) to full-width Katakana.
  // Katakana characters are contiguous, so we can use direct array indexing.
  if (code_point >= 0xFF61 && code_point <= 0xFF9F) {
    auto kKatakanaTable = std::to_array<UChar32>(
        {0x3002, 0x300C, 0x300D, 0x3001, 0x30FB, 0x30F2, 0x30A1, 0x30A3,
         0x30A5, 0x30A7, 0x30A9, 0x30E3, 0x30E5, 0x30E7, 0x30C3, 0x30FC,
         0x30A2, 0x30A4, 0x30A6, 0x30A8, 0x30AA, 0x30AB, 0x30AD, 0x30AF,
         0x30B1, 0x30B3, 0x30B5, 0x30B7, 0x30B9, 0x30BB, 0x30BD, 0x30BF,
         0x30C1, 0x30C4, 0x30C6, 0x30C8, 0x30CA, 0x30CB, 0x30CC, 0x30CD,
         0x30CE, 0x30CF, 0x30D2, 0x30D5, 0x30D8, 0x30DB, 0x30DE, 0x30DF,
         0x30E0, 0x30E1, 0x30E2, 0x30E4, 0x30E6, 0x30E8, 0x30E9, 0x30EA,
         0x30EB, 0x30EC, 0x30ED, 0x30EF, 0x30F3, 0x3099, 0x309A});
    size_t index = code_point - 0xFF61;
    return kKatakanaTable[index];
  }

  // Half-width Hangul to full-width Hangul mapping.
  // Hangul characters have gaps, so we need a lookup table.
  if ((code_point >= 0xFFA0 && code_point <= 0xFFBE) ||
      (code_point >= 0xFFC2 && code_point <= 0xFFC7) ||
      (code_point >= 0xFFCA && code_point <= 0xFFCF) ||
      (code_point >= 0xFFD2 && code_point <= 0xFFD7) ||
      (code_point >= 0xFFDA && code_point <= 0xFFDC)) {
    static const struct {
      UChar32 halfwidth;
      UChar32 fullwidth;
    } kHangulTable[] = {
        {0xFFA0, 0x3164}, {0xFFA1, 0x3131}, {0xFFA2, 0x3132}, {0xFFA3, 0x3133},
        {0xFFA4, 0x3134}, {0xFFA5, 0x3135}, {0xFFA6, 0x3136}, {0xFFA7, 0x3137},
        {0xFFA8, 0x3138}, {0xFFA9, 0x3139}, {0xFFAA, 0x313A}, {0xFFAB, 0x313B},
        {0xFFAC, 0x313C}, {0xFFAD, 0x313D}, {0xFFAE, 0x313E}, {0xFFAF, 0x313F},
        {0xFFB0, 0x3140}, {0xFFB1, 0x3141}, {0xFFB2, 0x3142}, {0xFFB3, 0x3143},
        {0xFFB4, 0x3144}, {0xFFB5, 0x3145}, {0xFFB6, 0x3146}, {0xFFB7, 0x3147},
        {0xFFB8, 0x3148}, {0xFFB9, 0x3149}, {0xFFBA, 0x314A}, {0xFFBB, 0x314B},
        {0xFFBC, 0x314C}, {0xFFBD, 0x314D}, {0xFFBE, 0x314E}, {0xFFC2, 0x314F},
        {0xFFC3, 0x3150}, {0xFFC4, 0x3151}, {0xFFC5, 0x3152}, {0xFFC6, 0x3153},
        {0xFFC7, 0x3154}, {0xFFCA, 0x3155}, {0xFFCB, 0x3156}, {0xFFCC, 0x3157},
        {0xFFCD, 0x3158}, {0xFFCE, 0x3159}, {0xFFCF, 0x315A}, {0xFFD2, 0x315B},
        {0xFFD3, 0x315C}, {0xFFD4, 0x315D}, {0xFFD5, 0x315E}, {0xFFD6, 0x315F},
        {0xFFD7, 0x3160}, {0xFFDA, 0x3161}, {0xFFDB, 0x3162}, {0xFFDC, 0x3163},
    };
    for (const auto& entry : kHangulTable) {
      if (entry.halfwidth == code_point) {
        return entry.fullwidth;
      }
    }
  }

  // Special character mappings.
  switch (code_point) {
    case uchar::kSpace:
      return uchar::kIdeographicSpace;
    case 0x00A2:  // Cent sign
      return 0xFFE0;
    case 0x00A3:  // Pound sign
      return 0xFFE1;
    case 0x00AC:  // Not sign
      return 0xFFE2;
    case 0x00AF:  // Macron
      return 0xFFE3;
    case 0x00A6:  // Broken bar
      return 0xFFE4;
    case uchar::kYenSign:
      return 0xFFE5;
    case 0x20A9:  // Won sign
      return 0xFFE6;
    case 0x2985:  // Left white parenthesis
      return 0xFF5F;
    case 0x2986:  // Right white parenthesis
      return 0xFF60;
    case 0xFFE8:  // Halfwidth forms light vertical
      return 0x2502;
    case 0xFFE9:  // Halfwidth leftwards arrow
      return 0x2190;
    case 0xFFEA:  // Halfwidth upwards arrow
      return 0x2191;
    case 0xFFEB:  // Halfwidth rightwards arrow
      return 0x2192;
    case 0xFFEC:  // Halfwidth downwards arrow
      return 0x2193;
    case 0xFFED:  // Halfwidth black square
      return uchar::kBlackSquare;
    case 0xFFEE:  // Halfwidth white circle
      return uchar::kWhiteCircle;
  }

  return code_point;
}

UChar32 Character::FullSizeKanaVariant(UChar32 code_point) {
  switch (code_point) {
    case 0x3041:  // Hiragana small a
      return 0x3042;
    case 0x3043:  // Hiragana small i
      return 0x3044;
    case 0x3045:  // Hiragana small u
      return 0x3046;
    case 0x3047:  // Hiragana small e
      return 0x3048;
    case 0x3049:  // Hiragana small o
      return 0x304A;
    case 0x3063:  // Hiragana small tu
      return 0x3064;
    case 0x3083:  // Hiragana small ya
      return 0x3084;
    case 0x3085:  // Hiragana small yu
      return 0x3086;
    case 0x3087:  // Hiragana small yo
      return 0x3088;
    case 0x308E:  // Hiragana small wa
      return 0x308F;
    case 0x3095:  // Hiragana small ka
      return 0x304B;
    case 0x3096:  // Hiragana small ke
      return 0x3051;
    case 0x30A1:  // Katakana small a
      return 0x30A2;
    case 0x30A3:  // Katakana small i
      return 0x30A4;
    case 0x30A5:  // Katakana small u
      return 0x30A6;
    case 0x30A7:  // Katakana small e
      return 0x30A8;
    case 0x30A9:  // Katakana small o
      return 0x30AA;
    case 0x30C3:  // Katakana small tu
      return 0x30C4;
    case 0x30E3:  // Katakana small ya
      return 0x30E4;
    case 0x30E5:  // Katakana small yu
      return 0x30E6;
    case 0x30E7:  // Katakana small yo
      return 0x30E8;
    case 0x30EE:  // Katakana small wa
      return 0x30EF;
    case 0x30F5:  // Katakana small ka
      return 0x30AB;
    case 0x30F6:  // Katakana small ke
      return 0x30B1;
    case 0x31F0:  // Katakana small ku
      return 0x30AF;
    case 0x31F1:  // Katakana small si
      return 0x30B7;
    case 0x31F2:  // Katakana small su
      return 0x30B9;
    case 0x31F3:  // Katakana small to
      return 0x30C8;
    case 0x31F4:  // Katakana small nu
      return 0x30CC;
    case 0x31F5:  // Katakana small ha
      return 0x30CF;
    case 0x31F6:  // Katakana small hi
      return 0x30D2;
    case 0x31F7:  // Katakana small hu
      return 0x30D5;
    case 0x31F8:  // Katakana small he
      return 0x30D8;
    case 0x31F9:  // Katakana small ho
      return 0x30DB;
    case 0x31FA:  // Katakana small mu
      return 0x30E0;
    case 0x31FB:  // Katakana small ra
      return 0x30E9;
    case 0x31FC:  // Katakana small ri
      return 0x30EA;
    case 0x31FD:  // Katakana small ru
      return 0x30EB;
    case 0x31FE:  // Katakana small re
      return 0x30EC;
    case 0x31FF:  // Katakana small ro
      return 0x30ED;
    case 0x1B132:  // Hiragana small ko
      return 0x3053;
    case 0x1B150:  // Hiragana small wi
      return 0x3090;
    case 0x1B151:  // Hiragana small we
      return 0x3091;
    case 0x1B152:  // Hiragana small wo
      return 0x3092;
    case 0x1B155:  // Katakana small ko
      return 0x30B3;
    case 0x1B164:  // Katakana small wi
      return 0x30F0;
    case 0x1B165:  // Katakana small we
      return 0x30F1;
    case 0x1B166:  // Katakana small wo
      return 0x30F2;
    case 0x1B167:  // Katakana small n
      return 0x30F3;
    case 0xFF67:  // Halfwidth katakana small a
      return 0xFF71;
    case 0xFF68:  // Halfwidth katakana small i
      return 0xFF72;
    case 0xFF69:  // Halfwidth katakana small u
      return 0xFF73;
    case 0xFF6A:  // Halfwidth katakana small e
      return 0xFF74;
    case 0xFF6B:  // Halfwidth katakana small o
      return 0xFF75;
    case 0xFF6C:  // Halfwidth katakana small ya
      return 0xFF94;
    case 0xFF6D:  // Halfwidth katakana small yu
      return 0xFF95;
    case 0xFF6E:  // Halfwidth katakana small yo
      return 0xFF96;
    case 0xFF6F:  // Halfwidth katakana small tu
      return 0xFF82;
  }

  return code_point;
}

}  // namespace blink
