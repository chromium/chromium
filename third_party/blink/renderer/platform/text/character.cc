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
#include "third_party/blink/renderer/platform/text/character_break_iterator.h"
#include "third_party/blink/renderer/platform/text/character_property_data.h"
#include "third_party/blink/renderer/platform/text/icu_error.h"
#include "third_party/blink/renderer/platform/text/justification_opportunity.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"
#include "third_party/blink/renderer/platform/wtf/text/utf16.h"
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

bool Character::IsPotentialCustomElementNameChar(UChar32 character) {
  return GetProperty(character).is_potential_custom_element_name_char;
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

unsigned Character::ExpansionOpportunityCount(
    TextJustify method,
    base::span<const LChar> characters,
    TextDirection direction,
    JustificationContext& context) {
  unsigned count = 0;
  if (direction == TextDirection::kLtr) {
    for (size_t i = 0; i < characters.size(); ++i) {
      count += CountJustificationOpportunity8(method, characters[i], context);
    }
  } else {
    for (size_t i = characters.size(); i > 0; --i) {
      count +=
          CountJustificationOpportunity8(method, characters[i - 1], context);
    }
  }

  return count;
}

unsigned Character::ExpansionOpportunityCount(
    TextJustify method,
    base::span<const UChar> characters,
    TextDirection direction,
    JustificationContext& context) {
  if (characters.size() == 0) {
    return 0;
  }
  unsigned count = 0;

  if (!RuntimeEnabledFeatures::EmojiJustificationEnabled()) {
    if (direction == TextDirection::kLtr) {
      for (size_t i = 0; i < characters.size();) {
        UChar32 character = CodePointAtAndNext(characters, i);
        count += CountJustificationOpportunity16(method, character, context);
      }
    } else {
      for (size_t i = characters.size(); i > 0; --i) {
        UChar32 character = characters[i - 1];
        if (U16_IS_TRAIL(character) && i > 1 &&
            U16_IS_LEAD(characters[i - 2])) {
          character = U16_GET_SUPPLEMENTARY(characters[i - 2], character);
          i--;
        }
        count += CountJustificationOpportunity16(method, character, context);
      }
    }
    return count;
  }
  CharacterBreakIterator iter(characters);
  if (direction == TextDirection::kLtr) {
    for (int i = 0; static_cast<size_t>(i) < characters.size();
         i = iter.Next()) {
      UChar32 character = CodePointAt(characters, i);
      count += CountJustificationOpportunity16(method, character, context);
    }
  } else {
    for (int i = iter.Preceding(characters.size()); i != kTextBreakDone;
         i = iter.Preceding(i)) {
      UChar32 character = CodePointAt(characters, i);
      count += CountJustificationOpportunity16(method, character, context);
    }
  }
  return count;
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

}  // namespace blink
