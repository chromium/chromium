/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/text/date_time_format.h"

#include "base/compiler_specific.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink::date_time_format {

static const std::array<FieldType, 26> kLowerCaseToFieldTypeMap = {
    FieldType::kFieldTypePeriod,                   // a
    FieldType::kFieldTypePeriodAmPmNoonMidnight,   // b
    FieldType::kFieldTypeLocalDayOfWeekStandAlon,  // c
    FieldType::kFieldTypeDayOfMonth,               // d
    FieldType::kFieldTypeLocalDayOfWeek,           // e
    FieldType::kFieldTypeInvalid,                  // f
    FieldType::kFieldTypeModifiedJulianDay,        // g
    FieldType::kFieldTypeHour12,                   // h
    FieldType::kFieldTypeInvalid,                  // i
    FieldType::kFieldTypeInvalid,                  // j
    FieldType::kFieldTypeHour24,                   // k
    FieldType::kFieldTypeInvalid,                  // l
    FieldType::kFieldTypeMinute,                   // m
    FieldType::kFieldTypeInvalid,                  // n
    FieldType::kFieldTypeInvalid,                  // o
    FieldType::kFieldTypeInvalid,                  // p
    FieldType::kFieldTypeQuarterStandAlone,        // q
    FieldType::kFieldTypeYearRelatedGregorian,     // r
    FieldType::kFieldTypeSecond,                   // s
    FieldType::kFieldTypeInvalid,                  // t
    FieldType::kFieldTypeExtendedYear,             // u
    FieldType::kFieldTypeNonLocationZone,          // v
    FieldType::kFieldTypeWeekOfYear,               // w
    FieldType::kFieldTypeZoneIso8601,              // x
    FieldType::kFieldTypeYear,                     // y
    FieldType::kFieldTypeZone,                     // z
};

static const std::array<FieldType, 26> kUpperCaseToFieldTypeMap = {
    FieldType::kFieldTypeMillisecondsInDay,  // A
    FieldType::kFieldTypePeriodFlexible,     // B
    FieldType::kFieldTypeInvalid,            // C
    FieldType::kFieldTypeDayOfYear,          // D
    FieldType::kFieldTypeDayOfWeek,          // E
    FieldType::kFieldTypeDayOfWeekInMonth,   // F
    FieldType::kFieldTypeEra,                // G
    FieldType::kFieldTypeHour23,             // H
    FieldType::kFieldTypeInvalid,            // I
    FieldType::kFieldTypeInvalid,            // J
    FieldType::kFieldTypeHour11,             // K
    FieldType::kFieldTypeMonthStandAlone,    // L
    FieldType::kFieldTypeMonth,              // M
    FieldType::kFieldTypeInvalid,            // N
    FieldType::kFieldTypeZoneLocalized,      // O
    FieldType::kFieldTypeInvalid,            // P
    FieldType::kFieldTypeQuarter,            // Q
    FieldType::kFieldTypeInvalid,            // R
    FieldType::kFieldTypeFractionalSecond,   // S
    FieldType::kFieldTypeInvalid,            // T
    FieldType::kFieldTypeYearCyclicName,     // U
    FieldType::kFieldTypeZoneId,             // V
    FieldType::kFieldTypeWeekOfMonth,        // W
    FieldType::kFieldTypeZoneIso8601Z,       // X
    FieldType::kFieldTypeYearOfWeekOfYear,   // Y
    FieldType::kFieldTypeRfc822Zone,         // Z
};

static FieldType MapCharacterToFieldType(const UChar ch) {
  if (IsAsciiUpper(ch)) {
    return kUpperCaseToFieldTypeMap[ch - 'A'];
  }

  if (IsAsciiLower(ch)) {
    return kLowerCaseToFieldTypeMap[ch - 'a'];
  }

  return FieldType::kFieldTypeLiteral;
}

bool Parse(const String& source, TokenHandler& token_handler) {
  enum State {
    kStateInQuote,
    kStateInQuoteQuote,
    kStateLiteral,
    kStateQuote,
    kStateSymbol,
  } state = kStateLiteral;

  FieldType field_type = kFieldTypeLiteral;
  StringBuilder literal_buffer;
  int field_counter = 0;

  for (wtf_size_t index = 0; index < source.length(); ++index) {
    const UChar ch = source[index];
    switch (state) {
      case kStateInQuote:
        if (ch == '\'') {
          state = kStateInQuoteQuote;
          break;
        }

        literal_buffer.Append(ch);
        break;

      case kStateInQuoteQuote:
        if (ch == '\'') {
          literal_buffer.Append('\'');
          state = kStateInQuote;
          break;
        }

        field_type = MapCharacterToFieldType(ch);
        if (field_type == kFieldTypeInvalid)
          return false;

        if (field_type == kFieldTypeLiteral) {
          literal_buffer.Append(ch);
          state = kStateLiteral;
          break;
        }

        if (literal_buffer.length()) {
          token_handler.VisitLiteral(literal_buffer.ToString());
          literal_buffer.Clear();
        }

        field_counter = 1;
        state = kStateSymbol;
        break;

      case kStateLiteral:
        if (ch == '\'') {
          state = kStateQuote;
          break;
        }

        field_type = MapCharacterToFieldType(ch);
        if (field_type == kFieldTypeInvalid)
          return false;

        if (field_type == kFieldTypeLiteral) {
          literal_buffer.Append(ch);
          break;
        }

        if (literal_buffer.length()) {
          token_handler.VisitLiteral(literal_buffer.ToString());
          literal_buffer.Clear();
        }

        field_counter = 1;
        state = kStateSymbol;
        break;

      case kStateQuote:
        literal_buffer.Append(ch);
        state = ch == '\'' ? kStateLiteral : kStateInQuote;
        break;

      case kStateSymbol: {
        DCHECK_NE(field_type, kFieldTypeInvalid);
        DCHECK_NE(field_type, kFieldTypeLiteral);
        DCHECK(literal_buffer.empty());

        FieldType field_type2 = MapCharacterToFieldType(ch);
        if (field_type2 == kFieldTypeInvalid)
          return false;

        if (field_type == field_type2) {
          ++field_counter;
          break;
        }

        token_handler.VisitField(field_type, field_counter);

        if (field_type2 == kFieldTypeLiteral) {
          if (ch == '\'') {
            state = kStateQuote;
          } else {
            literal_buffer.Append(ch);
            state = kStateLiteral;
          }
          break;
        }

        field_counter = 1;
        field_type = field_type2;
        break;
      }
    }
  }

  DCHECK_NE(field_type, kFieldTypeInvalid);

  switch (state) {
    case kStateLiteral:
    case kStateInQuoteQuote:
      if (literal_buffer.length())
        token_handler.VisitLiteral(literal_buffer.ToString());
      return true;

    case kStateQuote:
    case kStateInQuote:
      if (literal_buffer.length())
        token_handler.VisitLiteral(literal_buffer.ToString());
      return false;

    case kStateSymbol:
      DCHECK_NE(field_type, kFieldTypeLiteral);
      DCHECK(!literal_buffer.length());
      token_handler.VisitField(field_type, field_counter);
      return true;
  }

  NOTREACHED();
}

static bool IsAsciiAlphabetOrQuote(UChar ch) {
  return IsAsciiAlpha(ch) || ch == '\'';
}

void QuoteAndAppend(const StringView& literal, StringBuilder& buffer) {
  if (literal.length() <= 0)
    return;

  if (literal.Find(IsAsciiAlphabetOrQuote) == kNotFound) {
    buffer.Append(literal);
    return;
  }

  if (!literal.contains('\'')) {
    buffer.Append('\'');
    buffer.Append(literal);
    buffer.Append('\'');
    return;
  }

  for (wtf_size_t i = 0; i < literal.length(); ++i) {
    // SAFETY: index `i` checked against length above.
    if (UNSAFE_BUFFERS(literal[i]) == '\'') {
      buffer.Append("''");
    } else {
      String escaped = literal.substr(i).ToString().Replace("'", "''");
      buffer.Append('\'');
      buffer.Append(escaped);
      buffer.Append('\'');
      return;
    }
  }
}

}  // namespace blink::date_time_format
