/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Torch Mobile, Inc. http://www.torchmobile.com/
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/parser/html_entity_parser.h"

#include <array>

#include "base/notreached.h"
#include "third_party/blink/renderer/core/html/parser/html_entity_search.h"
#include "third_party/blink/renderer/core/html/parser/html_entity_table.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"

namespace blink {

namespace {

constexpr std::array<UChar, 32> kWindowsLatin1ExtensionArray = {
    0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,  // 80-87
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,  // 88-8F
    0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,  // 90-97
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178,  // 98-9F
};

UChar AdjustEntity(UChar32 value) {
  if ((value & ~0x1F) != 0x0080)
    return value;
  return kWindowsLatin1ExtensionArray[value - 0x80];
}

void AppendMatchToDecoded(const HTMLEntityTableEntry& match,
                          DecodedHTMLEntity& decoded_entity) {
  decoded_entity.Append(match.first_value);
  if (match.second_value) {
    decoded_entity.Append(match.second_value);
  }
}

constexpr UChar32 kInvalidUnicode = -1;

typedef Vector<UChar, 64> ConsumedCharacterBuffer;

void UnconsumeCharacters(SegmentedString& source,
                         ConsumedCharacterBuffer& consumed_characters) {
  if (consumed_characters.size() == 1)
    source.Push(consumed_characters[0]);
  else if (consumed_characters.size() == 2) {
    source.Push(consumed_characters[1]);
    source.Push(consumed_characters[0]);
  } else
    source.Prepend(SegmentedString(String(consumed_characters)),
                   SegmentedString::PrependType::kUnconsume);
}

bool ConsumeNamedEntity(SegmentedString& source,
                        DecodedHTMLEntity& decoded_entity,
                        bool& not_enough_characters,
                        UChar additional_allowed_character,
                        UChar& cc) {
  ConsumedCharacterBuffer consumed_characters;
  HTMLEntitySearch entity_search;
  while (!source.IsEmpty()) {
    cc = source.CurrentChar();
    entity_search.Advance(cc);
    if (!entity_search.IsEntityPrefix())
      break;
    consumed_characters.push_back(cc);
    source.AdvanceAndASSERT(cc);
  }
  // Character reference ends in ';', so if the last character is ';' then
  // don't treat it as not enough characters (because no additional characters
  // will change the result).
  not_enough_characters = source.IsEmpty() && cc != u';';
  if (not_enough_characters) {
    // We can't decide on an entity because there might be a longer entity
    // that we could match if we had more data.
    UnconsumeCharacters(source, consumed_characters);
    return false;
  }
  if (!entity_search.MostRecentMatch()) {
    UnconsumeCharacters(source, consumed_characters);
    return false;
  }
  if (entity_search.MostRecentMatch()->length !=
      entity_search.CurrentLength()) {
    // We've consumed too many characters. We need to walk the
    // source back to the point at which we had consumed an
    // actual entity.
    UnconsumeCharacters(source, consumed_characters);
    consumed_characters.clear();
    const HTMLEntityTableEntry* most_recent = entity_search.MostRecentMatch();
    const base::span<const LChar> reference =
        HTMLEntityTable::EntityString(*most_recent);
    for (size_t i = 0; i < reference.size(); ++i) {
      cc = source.CurrentChar();
      DCHECK_EQ(cc, reference[i]);
      consumed_characters.push_back(cc);
      source.AdvanceAndASSERT(cc);
      DCHECK(!source.IsEmpty());
    }
    cc = source.CurrentChar();
  }
  if (entity_search.MostRecentMatch()->LastCharacter() == ';' ||
      !additional_allowed_character ||
      !(IsASCIIAlphanumeric(cc) || cc == '=')) {
    AppendMatchToDecoded(*entity_search.MostRecentMatch(), decoded_entity);
    return true;
  }
  UnconsumeCharacters(source, consumed_characters);
  return false;
}

}  // namespace

void AppendLegalEntityFor(UChar32 c, DecodedHTMLEntity& decoded_entity) {
  // FIXME: A number of specific entity values generate parse errors.
  if (c <= 0 || c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF)) {
    decoded_entity.Append(0xFFFD);
    return;
  }
  if (U_IS_BMP(c)) {
    decoded_entity.Append(AdjustEntity(c));
    return;
  }
  decoded_entity.Append(c);
}

bool ConsumeHTMLEntity(SegmentedString& source,
                       DecodedHTMLEntity& decoded_entity,
                       bool& not_enough_characters,
                       UChar additional_allowed_character) {
  DCHECK(!additional_allowed_character || additional_allowed_character == '"' ||
         additional_allowed_character == '\'' ||
         additional_allowed_character == '>');
  DCHECK(!not_enough_characters);
  DCHECK(decoded_entity.IsEmpty());

  enum EntityState {
    kInitial,
    kNumber,
    kMaybeHexLowerCaseX,
    kMaybeHexUpperCaseX,
    kHex,
    kDecimal,
    kNamed
  };
  EntityState entity_state = kInitial;
  UChar32 result = 0;
  ConsumedCharacterBuffer consumed_characters;

  while (!source.IsEmpty()) {
    UChar cc = source.CurrentChar();
    switch (entity_state) {
      case kInitial: {
        if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ' ||
            cc == '<' || cc == '&')
          return false;
        if (additional_allowed_character && cc == additional_allowed_character)
          return false;
        if (cc == '#') {
          entity_state = kNumber;
          break;
        }
        if ((cc >= 'a' && cc <= 'z') || (cc >= 'A' && cc <= 'Z')) {
          entity_state = kNamed;
          continue;
        }
        return false;
      }
      case kNumber: {
        if (cc == 'x') {
          entity_state = kMaybeHexLowerCaseX;
          break;
        }
        if (cc == 'X') {
          entity_state = kMaybeHexUpperCaseX;
          break;
        }
        if (cc >= '0' && cc <= '9') {
          entity_state = kDecimal;
          continue;
        }
        source.Push('#');
        return false;
      }
      case kMaybeHexLowerCaseX: {
        if (IsASCIIHexDigit(cc)) {
          entity_state = kHex;
          continue;
        }
        source.Push('x');
        source.Push('#');
        return false;
      }
      case kMaybeHexUpperCaseX: {
        if (IsASCIIHexDigit(cc)) {
          entity_state = kHex;
          continue;
        }
        source.Push('X');
        source.Push('#');
        return false;
      }
      case kHex: {
        if (IsASCIIHexDigit(cc)) {
          if (result != kInvalidUnicode)
            result = result * 16 + ToASCIIHexValue(cc);
        } else if (cc == ';') {
          source.AdvanceAndASSERT(cc);
          AppendLegalEntityFor(result, decoded_entity);
          return true;
        } else {
          AppendLegalEntityFor(result, decoded_entity);
          return true;
        }
        break;
      }
      case kDecimal: {
        if (cc >= '0' && cc <= '9') {
          if (result != kInvalidUnicode)
            result = result * 10 + cc - '0';
        } else if (cc == ';') {
          source.AdvanceAndASSERT(cc);
          AppendLegalEntityFor(result, decoded_entity);
          return true;
        } else {
          AppendLegalEntityFor(result, decoded_entity);
          return true;
        }
        break;
      }
      case kNamed: {
        return ConsumeNamedEntity(source, decoded_entity, not_enough_characters,
                                  additional_allowed_character, cc);
      }
    }

    if (result > UCHAR_MAX_VALUE)
      result = kInvalidUnicode;

    consumed_characters.push_back(cc);
    source.AdvanceAndASSERT(cc);
  }
  DCHECK(source.IsEmpty());
  not_enough_characters = true;
  UnconsumeCharacters(source, consumed_characters);
  return false;
}

std::optional<DecodedHTMLEntity> DecodeNamedEntity(std::string_view name) {
  HTMLEntitySearch search;
  for (const auto c : name) {
    search.Advance(c);
    if (!search.IsEntityPrefix()) {
      return std::nullopt;
    }
  }
  search.Advance(';');
  if (!search.IsEntityPrefix()) {
    return std::nullopt;
  }
  DecodedHTMLEntity decoded_entity;
  AppendMatchToDecoded(*search.MostRecentMatch(), decoded_entity);
  return decoded_entity;
}

}  // namespace blink
