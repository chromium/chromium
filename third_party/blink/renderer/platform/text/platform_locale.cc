/*
 * Copyright (C) 2011,2012 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/platform/text/platform_locale.h"

#include <memory>

#include "base/macros.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/platform/text/date_time_format.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {
Locale* g_default_locale;
}

class DateTimeStringBuilder : private DateTimeFormat::TokenHandler {
 public:
  // The argument objects must be alive until this object dies.
  DateTimeStringBuilder(Locale&, const DateComponents&);

  bool Build(const String&);
  String ToString();

 private:
  // DateTimeFormat::TokenHandler functions.
  void VisitField(DateTimeFormat::FieldType, int) final;
  void VisitLiteral(const String&) final;

  String ZeroPadString(const String&, size_t width);
  void AppendNumber(int number, size_t width);

  StringBuilder builder_;
  Locale& localizer_;
  const DateComponents& date_;

  DISALLOW_COPY_AND_ASSIGN(DateTimeStringBuilder);
};

DateTimeStringBuilder::DateTimeStringBuilder(Locale& localizer,
                                             const DateComponents& date)
    : localizer_(localizer), date_(date) {}

bool DateTimeStringBuilder::Build(const String& format_string) {
  builder_.ReserveCapacity(format_string.length());
  return DateTimeFormat::Parse(format_string, *this);
}

String DateTimeStringBuilder::ZeroPadString(const String& string,
                                            size_t pad_width) {
  if (string.length() >= pad_width)
    return string;
  wtf_size_t width = static_cast<wtf_size_t>(pad_width);
  StringBuilder zero_padded_string_builder;
  zero_padded_string_builder.ReserveCapacity(width);
  for (wtf_size_t i = string.length(); i < width; ++i)
    zero_padded_string_builder.Append('0');
  zero_padded_string_builder.Append(string);
  return zero_padded_string_builder.ToString();
}

void DateTimeStringBuilder::AppendNumber(int number, size_t width) {
  String zero_padded_number_string =
      ZeroPadString(String::Number(number), width);
  builder_.Append(
      localizer_.ConvertToLocalizedNumber(zero_padded_number_string));
}

void DateTimeStringBuilder::VisitField(DateTimeFormat::FieldType field_type,
                                       int number_of_pattern_characters) {
  switch (field_type) {
    case DateTimeFormat::kFieldTypeYear:
      // Always use padding width of 4 so it matches DateTimeEditElement.
      AppendNumber(date_.FullYear(), 4);
      return;
    case DateTimeFormat::kFieldTypeMonth:
      if (number_of_pattern_characters == 3) {
        builder_.Append(localizer_.ShortMonthLabels()[date_.Month()]);
      } else if (number_of_pattern_characters == 4) {
        builder_.Append(localizer_.MonthLabels()[date_.Month()]);
      } else {
        // Always use padding width of 2 so it matches DateTimeEditElement.
        AppendNumber(date_.Month() + 1, 2);
      }
      return;
    case DateTimeFormat::kFieldTypeMonthStandAlone:
      if (number_of_pattern_characters == 3) {
        builder_.Append(localizer_.ShortStandAloneMonthLabels()[date_.Month()]);
      } else if (number_of_pattern_characters == 4) {
        builder_.Append(localizer_.StandAloneMonthLabels()[date_.Month()]);
      } else {
        // Always use padding width of 2 so it matches DateTimeEditElement.
        AppendNumber(date_.Month() + 1, 2);
      }
      return;
    case DateTimeFormat::kFieldTypeDayOfMonth:
      // Always use padding width of 2 so it matches DateTimeEditElement.
      AppendNumber(date_.MonthDay(), 2);
      return;
    case DateTimeFormat::kFieldTypeWeekOfYear:
      // Always use padding width of 2 so it matches DateTimeEditElement.
      AppendNumber(date_.Week(), 2);
      return;
    case DateTimeFormat::kFieldTypePeriod:
      builder_.Append(
          localizer_.TimeAMPMLabels()[(date_.Hour() >= 12 ? 1 : 0)]);
      return;
    case DateTimeFormat::kFieldTypeHour12: {
      int hour12 = date_.Hour() % 12;
      if (!hour12)
        hour12 = 12;
      AppendNumber(hour12, number_of_pattern_characters);
      return;
    }
    case DateTimeFormat::kFieldTypeHour23:
      AppendNumber(date_.Hour(), number_of_pattern_characters);
      return;
    case DateTimeFormat::kFieldTypeHour11:
      AppendNumber(date_.Hour() % 12, number_of_pattern_characters);
      return;
    case DateTimeFormat::kFieldTypeHour24: {
      int hour24 = date_.Hour();
      if (!hour24)
        hour24 = 24;
      AppendNumber(hour24, number_of_pattern_characters);
      return;
    }
    case DateTimeFormat::kFieldTypeMinute:
      AppendNumber(date_.Minute(), number_of_pattern_characters);
      return;
    case DateTimeFormat::kFieldTypeSecond:
      if (!date_.Millisecond()) {
        AppendNumber(date_.Second(), number_of_pattern_characters);
      } else {
        double second = date_.Second() + date_.Millisecond() / 1000.0;
        String zero_padded_second_string = ZeroPadString(
            String::Format("%.03f", second), number_of_pattern_characters + 4);
        builder_.Append(
            localizer_.ConvertToLocalizedNumber(zero_padded_second_string));
      }
      return;
    default:
      return;
  }
}

void DateTimeStringBuilder::VisitLiteral(const String& text) {
  DCHECK(text.length());
  builder_.Append(text);
}

String DateTimeStringBuilder::ToString() {
  return builder_.ToString();
}

Locale& Locale::DefaultLocale() {
  DCHECK(IsMainThread());
  if (!g_default_locale)
    g_default_locale = Locale::Create(DefaultLanguage()).release();
  return *g_default_locale;
}

void Locale::ResetDefaultLocale() {
  // This is safe because no one owns a Locale object returned by
  // DefaultLocale().
  delete g_default_locale;
  g_default_locale = nullptr;
}

Locale::~Locale() = default;

String Locale::QueryString(int resource_id) {
  // FIXME: Returns a string locazlied for this locale.
  return Platform::Current()->QueryLocalizedString(resource_id);
}

String Locale::QueryString(int resource_id, const String& parameter) {
  // FIXME: Returns a string locazlied for this locale.
  return Platform::Current()->QueryLocalizedString(resource_id, parameter);
}

String Locale::QueryString(int resource_id,
                           const String& parameter1,
                           const String& parameter2) {
  // FIXME: Returns a string locazlied for this locale.
  return Platform::Current()->QueryLocalizedString(resource_id, parameter1,
                                                   parameter2);
}

String Locale::ValidationMessageTooLongText(unsigned value_length,
                                            int max_length) {
  return QueryString(IDS_FORM_VALIDATION_TOO_LONG,
                     ConvertToLocalizedNumber(String::Number(value_length)),
                     ConvertToLocalizedNumber(String::Number(max_length)));
}

String Locale::ValidationMessageTooShortText(unsigned value_length,
                                             int min_length) {
  if (value_length == 1) {
    return QueryString(IDS_FORM_VALIDATION_TOO_SHORT,
                       ConvertToLocalizedNumber(String::Number(value_length)),
                       ConvertToLocalizedNumber(String::Number(min_length)));
  }

  return QueryString(IDS_FORM_VALIDATION_TOO_SHORT_PLURAL,
                     ConvertToLocalizedNumber(String::Number(value_length)),
                     ConvertToLocalizedNumber(String::Number(min_length)));
}

String Locale::WeekFormatInLDML() {
  String templ = QueryString(IDS_FORM_INPUT_WEEK_TEMPLATE);
  // Converts a string like "Week $2, $1" to an LDML date format pattern like
  // "'Week 'ww', 'yyyy".
  StringBuilder builder;
  unsigned literal_start = 0;
  unsigned length = templ.length();
  for (unsigned i = 0; i + 1 < length; ++i) {
    if (templ[i] == '$' && (templ[i + 1] == '1' || templ[i + 1] == '2')) {
      if (literal_start < i)
        DateTimeFormat::QuoteAndappend(
            templ.Substring(literal_start, i - literal_start), builder);
      builder.Append(templ[++i] == '1' ? "yyyy" : "ww");
      literal_start = i + 1;
    }
  }
  if (literal_start < length)
    DateTimeFormat::QuoteAndappend(
        templ.Substring(literal_start, length - literal_start), builder);
  return builder.ToString();
}

void Locale::SetLocaleData(const Vector<String, kDecimalSymbolsSize>& symbols,
                           const String& positive_prefix,
                           const String& positive_suffix,
                           const String& negative_prefix,
                           const String& negative_suffix) {
  for (wtf_size_t i = 0; i < symbols.size(); ++i) {
    DCHECK(!symbols[i].IsEmpty());
    decimal_symbols_[i] = symbols[i];
  }
  positive_prefix_ = positive_prefix;
  positive_suffix_ = positive_suffix;
  negative_prefix_ = negative_prefix;
  negative_suffix_ = negative_suffix;
  DCHECK(!positive_prefix_.IsEmpty() || !positive_suffix_.IsEmpty() ||
         !negative_prefix_.IsEmpty() || !negative_suffix_.IsEmpty());
  has_locale_data_ = true;

  StringBuilder builder;
  for (size_t i = 0; i < kDecimalSymbolsSize; ++i) {
    // We don't accept group separatros.
    if (i != kGroupSeparatorIndex)
      builder.Append(decimal_symbols_[i]);
  }
  builder.Append(positive_prefix_);
  builder.Append(positive_suffix_);
  builder.Append(negative_prefix_);
  builder.Append(negative_suffix_);
  acceptable_number_characters_ = builder.ToString();
}

String Locale::ConvertToLocalizedNumber(const String& input) {
  InitializeLocaleData();
  if (!has_locale_data_ || input.IsEmpty())
    return input;

  unsigned i = 0;
  bool is_negative = false;
  StringBuilder builder;
  builder.ReserveCapacity(input.length());

  if (input[0] == '-') {
    ++i;
    is_negative = true;
    builder.Append(negative_prefix_);
  } else {
    builder.Append(positive_prefix_);
  }

  for (; i < input.length(); ++i) {
    switch (input[i]) {
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        builder.Append(decimal_symbols_[input[i] - '0']);
        break;
      case '.':
        builder.Append(decimal_symbols_[kDecimalSeparatorIndex]);
        break;
      default:
        NOTREACHED();
    }
  }

  builder.Append(is_negative ? negative_suffix_ : positive_suffix_);

  return builder.ToString();
}

static bool Matches(const String& text, unsigned position, const String& part) {
  if (part.IsEmpty())
    return true;
  if (position + part.length() > text.length())
    return false;
  for (unsigned i = 0; i < part.length(); ++i) {
    if (text[position + i] != part[i])
      return false;
  }
  return true;
}

bool Locale::DetectSignAndGetDigitRange(const String& input,
                                        bool& is_negative,
                                        unsigned& start_index,
                                        unsigned& end_index) {
  start_index = 0;
  end_index = input.length();
  if (negative_prefix_.IsEmpty() && negative_suffix_.IsEmpty()) {
    if (input.StartsWith(positive_prefix_) &&
        input.EndsWith(positive_suffix_)) {
      is_negative = false;
      start_index = positive_prefix_.length();
      end_index -= positive_suffix_.length();
    } else {
      is_negative = true;
    }
  } else {
    if (input.StartsWith(negative_prefix_) &&
        input.EndsWith(negative_suffix_)) {
      is_negative = true;
      start_index = negative_prefix_.length();
      end_index -= negative_suffix_.length();
    } else {
      is_negative = false;
      if (input.StartsWith(positive_prefix_) &&
          input.EndsWith(positive_suffix_)) {
        start_index = positive_prefix_.length();
        end_index -= positive_suffix_.length();
      } else {
        return false;
      }
    }
  }
  return true;
}

unsigned Locale::MatchedDecimalSymbolIndex(const String& input,
                                           unsigned& position) {
  for (unsigned symbol_index = 0; symbol_index < kDecimalSymbolsSize;
       ++symbol_index) {
    if (decimal_symbols_[symbol_index].length() &&
        Matches(input, position, decimal_symbols_[symbol_index])) {
      position += decimal_symbols_[symbol_index].length();
      return symbol_index;
    }
  }
  return kDecimalSymbolsSize;
}

String Locale::ConvertFromLocalizedNumber(const String& localized) {
  InitializeLocaleData();
  String input = localized.RemoveCharacters(IsASCIISpace);
  if (!has_locale_data_ || input.IsEmpty())
    return input;

  bool is_negative;
  unsigned start_index;
  unsigned end_index;
  if (!DetectSignAndGetDigitRange(input, is_negative, start_index, end_index))
    return input;

  // Ignore leading '+', but will reject '+'-only string later.
  if (!is_negative && end_index - start_index >= 2 && input[start_index] == '+')
    ++start_index;

  StringBuilder builder;
  builder.ReserveCapacity(input.length());
  if (is_negative)
    builder.Append('-');
  for (unsigned i = start_index; i < end_index;) {
    unsigned symbol_index = MatchedDecimalSymbolIndex(input, i);
    if (symbol_index >= kDecimalSymbolsSize)
      return input;
    if (symbol_index == kDecimalSeparatorIndex)
      builder.Append('.');
    else if (symbol_index == kGroupSeparatorIndex)
      return input;
    else
      builder.Append(static_cast<UChar>('0' + symbol_index));
  }
  String converted = builder.ToString();
  // Ignore trailing '.', but will reject '.'-only string later.
  if (converted.length() >= 2 && converted[converted.length() - 1] == '.')
    converted = converted.Left(converted.length() - 1);
  return converted;
}

String Locale::StripInvalidNumberCharacters(const String& input,
                                            const String& standard_chars) {
  InitializeLocaleData();
  StringBuilder builder;
  builder.ReserveCapacity(input.length());
  for (unsigned i = 0; i < input.length(); ++i) {
    UChar ch = input[i];
    if (standard_chars.find(ch) != kNotFound)
      builder.Append(ch);
    else if (acceptable_number_characters_.find(ch) != kNotFound)
      builder.Append(ch);
  }
  return builder.ToString();
}

String Locale::LocalizedDecimalSeparator() {
  InitializeLocaleData();
  return decimal_symbols_[kDecimalSeparatorIndex];
}

String Locale::FormatDateTime(const DateComponents& date,
                              FormatType format_type) {
  if (date.GetType() == DateComponents::kInvalid)
    return String();

  DateTimeStringBuilder builder(*this, date);
  switch (date.GetType()) {
    case DateComponents::kTime:
      builder.Build(format_type == kFormatTypeShort ? ShortTimeFormat()
                                                    : TimeFormat());
      break;
    case DateComponents::kDate:
      builder.Build(DateFormat());
      break;
    case DateComponents::kMonth:
      builder.Build(format_type == kFormatTypeShort ? ShortMonthFormat()
                                                    : MonthFormat());
      break;
    case DateComponents::kWeek:
      builder.Build(WeekFormatInLDML());
      break;
    case DateComponents::kDateTimeLocal:
      builder.Build(format_type == kFormatTypeShort
                        ? DateTimeFormatWithoutSeconds()
                        : DateTimeFormatWithSeconds());
      break;
    case DateComponents::kInvalid:
      NOTREACHED();
      break;
  }
  return builder.ToString();
}

}  // namespace blink
