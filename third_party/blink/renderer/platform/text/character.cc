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

#include "base/stl_util.h"
#include "third_party/blink/renderer/platform/text/character_property_data.h"
#include "third_party/blink/renderer/platform/text/icu_error.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"


namespace blink {

static UCPTrie* CreateTrie() {
  // Create a Trie from the value array.
  ICUError error;
  UCPTrie* trie = ucptrie_openFromBinary(
      UCPTrieType::UCPTRIE_TYPE_FAST, UCPTrieValueWidth::UCPTRIE_VALUE_BITS_16,
      kSerializedCharacterData, kSerializedCharacterDataSize, nullptr, &error);
  DCHECK_EQ(error, U_ZERO_ERROR);
  return trie;
}

static bool HasProperty(UChar32 c, CharacterProperty property) {
  static const UCPTrie* trie = CreateTrie();
  return UCPTRIE_FAST_GET(trie, UCPTRIE_16, c) &
         static_cast<CharacterPropertyType>(property);
}

bool Character::IsUprightInMixedVertical(UChar32 character) {
  return u_getIntPropertyValue(character,
                               UProperty::UCHAR_VERTICAL_ORIENTATION) !=
         UVerticalOrientation::U_VO_ROTATED;
}

bool Character::IsCJKIdeographOrSymbolSlow(UChar32 c) {
  return HasProperty(c, CharacterProperty::kIsCJKIdeographOrSymbol);
}

bool Character::IsPotentialCustomElementNameChar(UChar32 character) {
  return HasProperty(character,
                     CharacterProperty::kIsPotentialCustomElementNameChar);
}

bool Character::IsBidiControl(UChar32 character) {
  return HasProperty(character, CharacterProperty::kIsBidiControl);
}

bool Character::IsHangulSlow(UChar32 character) {
  return HasProperty(character, CharacterProperty::kIsHangul);
}

unsigned Character::ExpansionOpportunityCount(
    base::span<const LChar> characters,
    TextDirection direction,
    bool& is_after_expansion,
    const TextJustify text_justify) {
  if (text_justify == TextJustify::kDistribute) {
    is_after_expansion = true;
    return characters.size();
  }

  unsigned count = 0;
  if (direction == TextDirection::kLtr) {
    for (unsigned i = 0; i < characters.size(); ++i) {
      if (TreatAsSpace(characters[i])) {
        count++;
        is_after_expansion = true;
      } else {
        is_after_expansion = false;
      }
    }
  } else {
    for (unsigned i = characters.size(); i > 0; --i) {
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
    bool& is_after_expansion,
    const TextJustify text_justify) {
  unsigned count = 0;
  if (direction == TextDirection::kLtr) {
    for (unsigned i = 0; i < characters.size(); ++i) {
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
      if (text_justify == TextJustify::kAuto &&
          IsCJKIdeographOrSymbol(character)) {
        if (!is_after_expansion)
          count++;
        count++;
        is_after_expansion = true;
        continue;
      }
      is_after_expansion = false;
    }
  } else {
    for (unsigned i = characters.size(); i > 0; --i) {
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
      if (text_justify == TextJustify::kAuto &&
          IsCJKIdeographOrSymbol(character)) {
        if (!is_after_expansion)
          count++;
        count++;
        is_after_expansion = true;
        continue;
      }
      is_after_expansion = false;
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

template <typename CharacterType>
static inline String NormalizeSpacesInternal(const CharacterType* characters,
                                             unsigned length) {
  StringBuilder normalized;
  normalized.ReserveCapacity(length);

  for (unsigned i = 0; i < length; ++i)
    normalized.Append(Character::NormalizeSpaces(characters[i]));

  return normalized.ToString();
}

String Character::NormalizeSpaces(const LChar* characters, unsigned length) {
  return NormalizeSpacesInternal(characters, length);
}

String Character::NormalizeSpaces(const UChar* characters, unsigned length) {
  return NormalizeSpacesInternal(characters, length);
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

}  // namespace blink
