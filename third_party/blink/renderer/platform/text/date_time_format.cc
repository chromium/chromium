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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/text/date_time_format.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

static const DateTimeFormat::FieldType kLowerCaseToFieldTypeMap[26] = {
    DateTimeFormat::kFieldTypePeriod,                   // a
    DateTimeFormat::kFieldTypePeriodAmPmNoonMidnight,   // b
    DateTimeFormat::kFieldTypeLocalDayOfWeekStandAlon,  // c
    DateTimeFormat::kFieldTypeDayOfMonth,               // d
    DateTimeFormat::kFieldTypeLocalDayOfWeek,           // e
    DateTimeFormat::kFieldTypeInvalid,                  // f
    DateTimeFormat::kFieldTypeModifiedJulianDay,        // g
    DateTimeFormat::kFieldTypeHour12,                   // h
    DateTimeFormat::kFieldTypeInvalid,                  // i
    DateTimeFormat::kFieldTypeInvalid,                  // j
    DateTimeFormat::kFieldTypeHour24,                   // k
    DateTimeFormat::kFieldTypeInvalid,                  // l
    DateTimeFormat::kFieldTypeMinute,                   // m
    DateTimeFormat::kFieldTypeInvalid,                  // n
    DateTimeFormat::kFieldTypeInvalid,                  // o
    DateTimeFormat::kFieldTypeInvalid,                  // p
    DateTimeFormat::kFieldTypeQuaterStandAlone,         // q
    DateTimeFormat::kFieldTypeYearRelatedGregorian,     // r
    DateTimeFormat::kFieldTypeSecond,                   // s
    DateTimeFormat::kFieldTypeInvalid,                  // t
    DateTimeFormat::kFieldTypeExtendedYear,             // u
    DateTimeFormat::kFieldTypeNonLocationZone,          // v
    DateTimeFormat::kFieldTypeWeekOfYear,               // w
    DateTimeFormat::kFieldTypeZoneIso8601,              // x
    DateTimeFormat::kFieldTypeYear,                     // y
    DateTimeFormat::kFieldTypeZone,                     // z
};

static const DateTimeFormat::FieldType kUpperCaseToFieldTypeMap[26] = {
    DateTimeFormat::kFieldTypeMillisecondsInDay,  // A
    DateTimeFormat::kFieldTypePeriodFlexible,     // B
    DateTimeFormat::kFieldTypeInvalid,            // C
    DateTimeFormat::kFieldTypeDayOfYear,          // D
    DateTimeFormat::kFieldTypeDayOfWeek,          // E
    DateTimeFormat::kFieldTypeDayOfWeekInMonth,   // F
    DateTimeFormat::kFieldTypeEra,                // G
    DateTimeFormat::kFieldTypeHour23,             // H
    DateTimeFormat::kFieldTypeInvalid,            // I
    DateTimeFormat::kFieldTypeInvalid,            // J
    DateTimeFormat::kFieldTypeHour11,             // K
    DateTimeFormat::kFieldTypeMonthStandAlone,    // L
    DateTimeFormat::kFieldTypeMonth,              // M
    DateTimeFormat::kFieldTypeInvalid,            // N
    DateTimeFormat::kFieldTypeZoneLocalized,      // O
    DateTimeFormat::kFieldTypeInvalid,            // P
    DateTimeFormat::kFieldTypeQuater,             // Q
    DateTimeFormat::kFieldTypeInvalid,            // R
    DateTimeFormat::kFieldTypeFractionalSecond,   // S
    DateTimeFormat::kFieldTypeInvalid,            // T
    DateTimeFormat::kFieldTypeYearCyclicName,     // U
    DateTimeFormat::kFieldTypeZoneId,             // V
    DateTimeFormat::kFieldTypeWeekOfMonth,        // W
    DateTimeFormat::kFieldTypeZoneIso8601Z,       // X
    DateTimeFormat::kFieldTypeYearOfWeekOfYear,   // Y
    DateTimeFormat::kFieldTypeRFC822Zone,         // Z
};

static DateTimeFormat::FieldType MapCharacterToFieldType(const UChar ch) {
  if (IsASCIIUpper(ch))
    return kUpperCaseToFieldTypeMap[ch - 'A'];

  if (IsASCIILower(ch))
    return kLowerCaseToFieldTypeMap[ch - 'a'];

  return DateTimeFormat::kFieldTypeLiteral;
}

bool DateTimeFormat::Parse(const String& source, TokenHandler& token_handler) {
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

  for (unsigned index = 0; index < source.length(); ++index) {
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

  NOTREACHED_IN_MIGRATION();
  return false;
}

static bool IsASCIIAlphabetOrQuote(UChar ch) {
  return IsASCIIAlpha(ch) || ch == '\'';
}

void DateTimeFormat::QuoteAndappend(const String& literal,
                                    StringBuilder& buffer) {
  if (literal.length() <= 0)
    return;

  if (literal.Find(IsASCIIAlphabetOrQuote) == kNotFound) {
    buffer.Append(literal);
    return;
  }

  if (literal.find('\'') == kNotFound) {
    buffer.Append('\'');
    buffer.Append(literal);
    buffer.Append('\'');
    return;
  }

  for (unsigned i = 0; i < literal.length(); ++i) {
    if (literal[i] == '\'') {
      buffer.Append("''");
    } else {
      String escaped = literal.Substring(i);
      escaped.Replace("'", "''");
      buffer.Append('\'');
      buffer.Append(escaped);
      buffer.Append('\'');
      return;
    }
  }
}

}  // namespace blink
