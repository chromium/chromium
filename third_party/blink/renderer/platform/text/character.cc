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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/text/character.h"

#include <unicode/uchar.h>
#include <unicode/ucptrie.h>
#include <unicode/uobject.h>
#include <unicode/uscript.h>

#include <algorithm>

#include "base/synchronization/lock.h"
#include "third_party/blink/renderer/platform/text/character_property_data.h"
#include "third_party/blink/renderer/platform/text/icu_error.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

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

unsigned GetProperty(UChar32 c, CharacterProperty property) {
  static const UCPTrie* trie = CreateTrie();
  return UCPTRIE_FAST_GET(trie, UCPTRIE_16, c) &
         static_cast<CharacterPropertyType>(property);
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
  return GetProperty(c, CharacterProperty::kIsCJKIdeographOrSymbol);
}

bool Character::IsPotentialCustomElementNameChar(UChar32 character) {
  return GetProperty(character,
                     CharacterProperty::kIsPotentialCustomElementNameChar);
}

bool Character::IsBidiControl(UChar32 character) {
  return GetProperty(character, CharacterProperty::kIsBidiControl);
}

bool Character::IsHangulSlow(UChar32 character) {
  return GetProperty(character, CharacterProperty::kIsHangul);
}

HanKerningCharType Character::GetHanKerningCharType(UChar32 character) {
  return static_cast<HanKerningCharType>(
      GetProperty(character, CharacterProperty::kHanKerningShiftedMask) >>
      static_cast<unsigned>(CharacterProperty::kHanKerningShift));
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
    base::span<const LChar> characters,
    TextDirection direction,
    bool& is_after_expansion) {
  unsigned count = 0;
  if (direction == TextDirection::kLtr) {
    for (size_t i = 0; i < characters.size(); ++i) {
      if (TreatAsSpace(characters[i])) {
        count++;
        is_after_expansion = true;
      } else {
        is_after_expansion = false;
      }
    }
  } else {
    for (size_t i = characters.size(); i > 0; --i) {
      if (TreatAsSpace(characters[i - 1])) {
        count++;
        is_after_expansion = true;
      } else {
        is_after_expansion = false;
      }
    }
  }

  return count;
}

unsigned Character::ExpansionOpportunityCount(
    base::span<const UChar> characters,
    TextDirection direction,
    bool& is_after_expansion) {
  unsigned count = 0;
  if (direction == TextDirection::kLtr) {
    for (size_t i = 0; i < characters.size(); ++i) {
      UChar32 character = characters[i];
      if (TreatAsSpace(character)) {
        count++;
        is_after_expansion = true;
        continue;
      }
      if (U16_IS_LEAD(character) && i + 1 < characters.size() &&
          U16_IS_TRAIL(characters[i + 1])) {
        character = U16_GET_SUPPLEMENTARY(character, characters[i + 1]);
        i++;
      }
      if (IsCJKIdeographOrSymbol(character)) {
        if (!is_after_expansion)
          count++;
        count++;
        is_after_expansion = true;
        continue;
      } else if (!IsDefaultIgnorable(character)) {
        is_after_expansion = false;
      }
    }
  } else {
    for (size_t i = characters.size(); i > 0; --i) {
      UChar32 character = characters[i - 1];
      if (TreatAsSpace(character)) {
        count++;
        is_after_expansion = true;
        continue;
      }
      if (U16_IS_TRAIL(character) && i > 1 && U16_IS_LEAD(characters[i - 2])) {
        character = U16_GET_SUPPLEMENTARY(characters[i - 2], character);
        i--;
      }
      if (IsCJKIdeographOrSymbol(character)) {
        if (!is_after_expansion)
          count++;
        count++;
        is_after_expansion = true;
        continue;
      } else if (!IsDefaultIgnorable(character)) {
        is_after_expansion = false;
      }
    }
  }
  return count;
}

bool Character::CanTextDecorationSkipInk(UChar32 codepoint) {
  if (codepoint == kSolidusCharacter || codepoint == kReverseSolidusCharacter ||
      codepoint == kLowLineCharacter)
    return false;

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
  WTF::unicode::CharCategory category = WTF::unicode::Category(c);
  if (category &
      (WTF::unicode::kSeparator_Space | WTF::unicode::kSeparator_Line |
       WTF::unicode::kSeparator_Paragraph | WTF::unicode::kOther_NotAssigned |
       WTF::unicode::kOther_Control | WTF::unicode::kOther_Format))
    return false;

  // Additional word-separator characters listed in CSS Text Level 3 Editor's
  // Draft 3 November 2010.
  if (c == kEthiopicWordspaceCharacter ||
      c == kAegeanWordSeparatorLineCharacter ||
      c == kAegeanWordSeparatorDotCharacter ||
      c == kUgariticWordDividerCharacter ||
      c == kTibetanMarkIntersyllabicTshegCharacter ||
      c == kTibetanMarkDelimiterTshegBstarCharacter)
    return false;

  return true;
}

bool Character::IsEmojiTagSequence(UChar32 c) {
  // http://www.unicode.org/reports/tr51/proposed.html#valid-emoji-tag-sequences
  return (c >= kTagDigitZero && c <= kTagDigitNine) ||
         (c >= kTagLatinSmallLetterA && c <= kTagLatinSmallLetterZ);
}

bool Character::IsExtendedPictographic(UChar32 c) {
  return u_hasBinaryProperty(c, UCHAR_EXTENDED_PICTOGRAPHIC);
}

bool Character::IsEmojiComponent(UChar32 c) {
  return u_hasBinaryProperty(c, UCHAR_EMOJI_COMPONENT);
}

bool Character::MaybeEmojiPresentation(UChar32 c) {
  return c == kZeroWidthJoinerCharacter || c == 0x00A9 /* copyright sign */ ||
         c == 0x00AE /* registered sign */ || IsEmojiKeycapBase(c) ||
         IsInRange(c, 0x203C, 0x2B55) || c == kVariationSelector15Character ||
         c == 0x3030 || c == 0x303D || c == 0x3297 || c == 0x3299 ||
         c == kVariationSelector16Character || c >= 65536;
}

bool Character::IsCommonOrInheritedScript(UChar32 character) {
  ICUError status;
  UScriptCode script = uscript_getScript(character, &status);
  return U_SUCCESS(status) &&
         (script == USCRIPT_COMMON || script == USCRIPT_INHERITED);
}

bool Character::IsPrivateUse(UChar32 character) {
  return WTF::unicode::Category(character) & WTF::unicode::kOther_PrivateUse;
}

bool Character::IsNonCharacter(UChar32 character) {
  return U_IS_UNICODE_NONCHAR(character);
}

bool Character::HasDefiniteScript(UChar32 character) {
  ICUError err;
  UScriptCode hint_char_script = uscript_getScript(character, &err);
  if (!U_SUCCESS(err))
    return false;
  return hint_char_script != USCRIPT_INHERITED &&
         hint_char_script != USCRIPT_COMMON;
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
  return text_content != kArabicMathematicalOperatorMeemWithHahWithTatweel &&
         text_content != kArabicMathematicalOperatorHahWithDal &&
         !std::binary_search(stretchy_operator_with_inline_axis,
                             stretchy_operator_with_inline_axis +
                                 std::size(stretchy_operator_with_inline_axis),
                             text_content);
}

}  // namespace blink
