/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/platform/text/locale_win.h"

#include <iterator>
#include <limits>
#include <memory>

#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/text/date_components.h"
#include "third_party/blink/renderer/platform/text/date_time_format.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/blink/renderer/platform/wtf/date_math.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"
#include "ui/base/ui_base_features.h"

namespace blink {

static String ExtractLanguageCode(const String& locale) {
  wtf_size_t dash_position = locale.find('-');
  if (dash_position == kNotFound)
    return locale;
  return locale.Left(dash_position);
}

static LCID LCIDFromLocaleInternal(LCID user_default_lcid,
                                   const String& user_default_language_code,
                                   const String& locale) {
  String locale_language_code = ExtractLanguageCode(locale);
  if (DeprecatedEqualIgnoringCase(locale_language_code,
                                  user_default_language_code))
    return user_default_lcid;
  if (locale.length() >= LOCALE_NAME_MAX_LENGTH)
    return 0;
  UChar buffer[LOCALE_NAME_MAX_LENGTH];
  if (locale.Is8Bit())
    StringImpl::CopyChars(buffer, locale.Characters8(), locale.length());
  else
    StringImpl::CopyChars(buffer, locale.Characters16(), locale.length());
  buffer[locale.length()] = '\0';
  return ::LocaleNameToLCID(base::as_writable_wcstr(buffer), 0);
}

static LCID LCIDFromLocale(const String& locale, bool defaults_for_locale) {
  // According to MSDN, 9 is enough for LOCALE_SISO639LANGNAME.
  const size_t kLanguageCodeBufferSize = 9;
  WCHAR lowercase_language_code[kLanguageCodeBufferSize];
  ::GetLocaleInfo(LOCALE_USER_DEFAULT,
                  LOCALE_SISO639LANGNAME |
                      (defaults_for_locale ? LOCALE_NOUSEROVERRIDE : 0),
                  lowercase_language_code, kLanguageCodeBufferSize);
  String user_default_language_code =
      String(base::as_u16cstr(lowercase_language_code));

  LCID lcid = LCIDFromLocaleInternal(LOCALE_USER_DEFAULT,
                                     user_default_language_code, locale);
  if (!lcid)
    lcid = LCIDFromLocaleInternal(
        LOCALE_USER_DEFAULT, user_default_language_code, DefaultLanguage());
  return lcid;
}

std::unique_ptr<Locale> Locale::Create(const String& locale) {
  // Whether the default settings for the locale should be used, ignoring user
  // overrides.
  bool defaults_for_locale = WebTestSupport::IsRunningWebTest();
  return LocaleWin::Create(LCIDFromLocale(locale, defaults_for_locale),
                           defaults_for_locale);
}

inline LocaleWin::LocaleWin(LCID lcid, bool defaults_for_locale)
    : lcid_(lcid),
      did_initialize_number_data_(false),
      defaults_for_locale_(defaults_for_locale) {
  DWORD value = 0;
  GetLocaleInfo(LOCALE_IFIRSTDAYOFWEEK |
                    (defaults_for_locale ? LOCALE_NOUSEROVERRIDE : 0),
                value);
  // 0:Monday, ..., 6:Sunday.
  // We need 1 for Monday, 0 for Sunday.
  first_day_of_week_ = (value + 1) % 7;
}

std::unique_ptr<LocaleWin> LocaleWin::Create(LCID lcid,
                                             bool defaults_for_locale) {
  return base::WrapUnique(new LocaleWin(lcid, defaults_for_locale));
}

LocaleWin::~LocaleWin() {}

String LocaleWin::GetLocaleInfoString(LCTYPE type) {
  int buffer_size_with_nul = ::GetLocaleInfo(
      lcid_, type | (defaults_for_locale_ ? LOCALE_NOUSEROVERRIDE : 0), 0, 0);
  if (buffer_size_with_nul <= 0)
    return String();
  StringBuffer<UChar> buffer(buffer_size_with_nul);
  ::GetLocaleInfo(
      lcid_, type | (defaults_for_locale_ ? LOCALE_NOUSEROVERRIDE : 0),
      base::as_writable_wcstr(buffer.Characters()), buffer_size_with_nul);
  buffer.Shrink(buffer_size_with_nul - 1);
  return String::Adopt(buffer);
}

void LocaleWin::GetLocaleInfo(LCTYPE type, DWORD& result) {
  ::GetLocaleInfo(lcid_, type | LOCALE_RETURN_NUMBER,
                  reinterpret_cast<LPWSTR>(&result),
                  sizeof(DWORD) / sizeof(TCHAR));
}

// -------------------------------- Tokenized date format

static unsigned CountContinuousLetters(const String& format, unsigned index) {
  unsigned count = 1;
  UChar reference = format[index];
  while (index + 1 < format.length()) {
    if (format[++index] != reference)
      break;
    ++count;
  }
  return count;
}

static void CommitLiteralToken(StringBuilder& literal_buffer,
                               StringBuilder& converted) {
  if (literal_buffer.length() <= 0)
    return;
  DateTimeFormat::QuoteAndappend(literal_buffer.ToString(), converted);
  literal_buffer.Clear();
}

// This function converts Windows date/time pattern format [1][2] into LDML date
// format pattern [3].
//
// i.e.
//   We set h, H, m, s, d, dd, M, or y as is. They have same meaning in both of
//   Windows and LDML.
//   We need to convert the following patterns:
//     t -> a
//     tt -> a
//     ddd -> EEE
//     dddd -> EEEE
//     g -> G
//     gg -> ignore
//
// [1] http://msdn.microsoft.com/en-us/library/dd317787(v=vs.85).aspx
// [2] http://msdn.microsoft.com/en-us/library/dd318148(v=vs.85).aspx
// [3] LDML http://unicode.org/reports/tr35/tr35-6.html#Date_Format_Patterns
static String ConvertWindowsDateTimeFormat(const String& format) {
  StringBuilder converted;
  StringBuilder literal_buffer;
  bool in_quote = false;
  bool last_quote_can_be_literal = false;
  for (unsigned i = 0; i < format.length(); ++i) {
    UChar ch = format[i];
    if (in_quote) {
      if (ch == '\'') {
        in_quote = false;
        DCHECK(i);
        if (last_quote_can_be_literal && format[i - 1] == '\'') {
          literal_buffer.Append('\'');
          last_quote_can_be_literal = false;
        } else {
          last_quote_can_be_literal = true;
        }
      } else {
        literal_buffer.Append(ch);
      }
      continue;
    }

    if (ch == '\'') {
      in_quote = true;
      if (last_quote_can_be_literal && i > 0 && format[i - 1] == '\'') {
        literal_buffer.Append(ch);
        last_quote_can_be_literal = false;
      } else {
        last_quote_can_be_literal = true;
      }
    } else if (IsASCIIAlpha(ch)) {
      CommitLiteralToken(literal_buffer, converted);
      unsigned symbol_start = i;
      unsigned count = CountContinuousLetters(format, i);
      i += count - 1;
      if (ch == 'h' || ch == 'H' || ch == 'm' || ch == 's' || ch == 'M' ||
          ch == 'y') {
        converted.Append(format, symbol_start, count);
      } else if (ch == 'd') {
        if (count <= 2)
          converted.Append(format, symbol_start, count);
        else if (count == 3)
          converted.Append("EEE");
        else
          converted.Append("EEEE");
      } else if (ch == 'g') {
        if (count == 1) {
          converted.Append('G');
        } else {
          // gg means imperial era in Windows.
          // Just ignore it.
        }
      } else if (ch == 't') {
        converted.Append('a');
      } else {
        literal_buffer.Append(format, symbol_start, count);
      }
    } else {
      literal_buffer.Append(ch);
    }
  }
  CommitLiteralToken(literal_buffer, converted);
  return converted.ToString();
}

const Vector<String>& LocaleWin::MonthLabels() {
  if (month_labels_.empty()) {
    static constexpr LCTYPE kTypes[12] = {
        LOCALE_SMONTHNAME1,  LOCALE_SMONTHNAME2,  LOCALE_SMONTHNAME3,
        LOCALE_SMONTHNAME4,  LOCALE_SMONTHNAME5,  LOCALE_SMONTHNAME6,
        LOCALE_SMONTHNAME7,  LOCALE_SMONTHNAME8,  LOCALE_SMONTHNAME9,
        LOCALE_SMONTHNAME10, LOCALE_SMONTHNAME11, LOCALE_SMONTHNAME12,
    };
    month_labels_.reserve(std::size(kTypes));
    for (unsigned i = 0; i < std::size(kTypes); ++i) {
      month_labels_.push_back(GetLocaleInfoString(kTypes[i]));
      if (month_labels_.back().empty()) {
        month_labels_.Shrink(0);
        base::ranges::copy(kFallbackMonthNames,
                           std::back_inserter(month_labels_));
        break;
      }
    }
  }
  return month_labels_;
}

const Vector<String>& LocaleWin::WeekDayShortLabels() {
  if (week_day_short_labels_.empty()) {
    static constexpr LCTYPE kTypes[7] = {
        // Numbered 1 (Monday) - 7 (Sunday), so do 7, then 1-6
        LOCALE_SSHORTESTDAYNAME7, LOCALE_SSHORTESTDAYNAME1,
        LOCALE_SSHORTESTDAYNAME2, LOCALE_SSHORTESTDAYNAME3,
        LOCALE_SSHORTESTDAYNAME4, LOCALE_SSHORTESTDAYNAME5,
        LOCALE_SSHORTESTDAYNAME6};
    week_day_short_labels_.reserve(std::size(kTypes));
    for (unsigned i = 0; i < std::size(kTypes); ++i) {
      week_day_short_labels_.push_back(GetLocaleInfoString(kTypes[i]));
      if (week_day_short_labels_.back().empty()) {
        week_day_short_labels_.Shrink(0);
        base::ranges::copy(kFallbackWeekdayShortNames,
                           std::back_inserter(week_day_short_labels_));
        break;
      }
    }
  }
  return week_day_short_labels_;
}

unsigned LocaleWin::FirstDayOfWeek() {
  return first_day_of_week_;
}

bool LocaleWin::IsRTL() {
  WTF::unicode::CharDirection dir =
      WTF::unicode::Direction(MonthLabels()[0][0]);
  return dir == WTF::unicode::kRightToLeft ||
         dir == WTF::unicode::kRightToLeftArabic;
}

String LocaleWin::DateFormat() {
  if (date_format_.IsNull())
    date_format_ =
        ConvertWindowsDateTimeFormat(GetLocaleInfoString(LOCALE_SSHORTDATE));
  return date_format_;
}

String LocaleWin::DateFormat(const String& windows_format) {
  return ConvertWindowsDateTimeFormat(windows_format);
}

String LocaleWin::MonthFormat() {
  if (month_format_.IsNull())
    month_format_ =
        ConvertWindowsDateTimeFormat(GetLocaleInfoString(LOCALE_SYEARMONTH));
  return month_format_;
}

String LocaleWin::ShortMonthFormat() {
  if (short_month_format_.IsNull())
    short_month_format_ =
        ConvertWindowsDateTimeFormat(GetLocaleInfoString(LOCALE_SYEARMONTH))
            .Replace("MMMM", "MMM");
  return short_month_format_;
}

String LocaleWin::TimeFormat() {
  if (time_format_with_seconds_.IsNull())
    time_format_with_seconds_ =
        ConvertWindowsDateTimeFormat(GetLocaleInfoString(LOCALE_STIMEFORMAT));
  return time_format_with_seconds_;
}

String LocaleWin::ShortTimeFormat() {
  if (!time_format_without_seconds_.IsNull())
    return time_format_without_seconds_;
  String format = GetLocaleInfoString(LOCALE_SSHORTTIME);
  // Vista or older Windows doesn't support LOCALE_SSHORTTIME.
  if (format.empty()) {
    format = GetLocaleInfoString(LOCALE_STIMEFORMAT);
    StringBuilder builder;
    builder.Append(GetLocaleInfoString(LOCALE_STIME));
    builder.Append("ss");
    wtf_size_t pos = format.ReverseFind(builder.ToString());
    if (pos != kNotFound)
      format.Remove(pos, builder.length());
  }
  time_format_without_seconds_ = ConvertWindowsDateTimeFormat(format);
  return time_format_without_seconds_;
}

String LocaleWin::DateTimeFormatWithSeconds() {
  if (!date_time_format_with_seconds_.IsNull())
    return date_time_format_with_seconds_;
  StringBuilder builder;
  builder.Append(DateFormat());
  builder.Append(' ');
  builder.Append(TimeFormat());
  date_time_format_with_seconds_ = builder.ToString();
  return date_time_format_with_seconds_;
}

String LocaleWin::DateTimeFormatWithoutSeconds() {
  if (!date_time_format_without_seconds_.IsNull())
    return date_time_format_without_seconds_;
  StringBuilder builder;
  builder.Append(DateFormat());
  builder.Append(' ');
  builder.Append(ShortTimeFormat());
  date_time_format_without_seconds_ = builder.ToString();
  return date_time_format_without_seconds_;
}

const Vector<String>& LocaleWin::ShortMonthLabels() {
  if (short_month_labels_.empty()) {
    static constexpr LCTYPE kTypes[12] = {
        LOCALE_SABBREVMONTHNAME1,  LOCALE_SABBREVMONTHNAME2,
        LOCALE_SABBREVMONTHNAME3,  LOCALE_SABBREVMONTHNAME4,
        LOCALE_SABBREVMONTHNAME5,  LOCALE_SABBREVMONTHNAME6,
        LOCALE_SABBREVMONTHNAME7,  LOCALE_SABBREVMONTHNAME8,
        LOCALE_SABBREVMONTHNAME9,  LOCALE_SABBREVMONTHNAME10,
        LOCALE_SABBREVMONTHNAME11, LOCALE_SABBREVMONTHNAME12,
    };
    short_month_labels_.reserve(std::size(kTypes));
    for (unsigned i = 0; i < std::size(kTypes); ++i) {
      short_month_labels_.push_back(GetLocaleInfoString(kTypes[i]));
      if (short_month_labels_.back().empty()) {
        short_month_labels_.Shrink(0);
        base::ranges::copy(kFallbackMonthShortNames,
                           std::back_inserter(short_month_labels_));
        break;
      }
    }
  }
  return short_month_labels_;
}

const Vector<String>& LocaleWin::StandAloneMonthLabels() {
  // Windows doesn't provide a way to get stand-alone month labels.
  return MonthLabels();
}

const Vector<String>& LocaleWin::ShortStandAloneMonthLabels() {
  // Windows doesn't provide a way to get stand-alone month labels.
  return ShortMonthLabels();
}

const Vector<String>& LocaleWin::TimeAMPMLabels() {
  if (time_ampm_labels_.empty()) {
    time_ampm_labels_.push_back(GetLocaleInfoString(LOCALE_S1159));
    time_ampm_labels_.push_back(GetLocaleInfoString(LOCALE_S2359));
  }
  return time_ampm_labels_;
}

void LocaleWin::InitializeLocaleData() {
  if (did_initialize_number_data_)
    return;

  Vector<String, kDecimalSymbolsSize> symbols;
  enum DigitSubstitution {
    kDigitSubstitutionContext = 0,
    kDigitSubstitution0to9 = 1,
    kDigitSubstitutionNative = 2,
  };
  DWORD digit_substitution = kDigitSubstitution0to9;
  GetLocaleInfo(LOCALE_IDIGITSUBSTITUTION, digit_substitution);
  if (digit_substitution == kDigitSubstitution0to9) {
    symbols.push_back("0");
    symbols.push_back("1");
    symbols.push_back("2");
    symbols.push_back("3");
    symbols.push_back("4");
    symbols.push_back("5");
    symbols.push_back("6");
    symbols.push_back("7");
    symbols.push_back("8");
    symbols.push_back("9");
  } else {
    String digits = GetLocaleInfoString(LOCALE_SNATIVEDIGITS);
    DCHECK_GE(digits.length(), 10u);
    for (unsigned i = 0; i < 10; ++i)
      symbols.push_back(digits.Substring(i, 1));
  }
  DCHECK(symbols.size() == kDecimalSeparatorIndex);
  symbols.push_back(GetLocaleInfoString(LOCALE_SDECIMAL));
  DCHECK(symbols.size() == kGroupSeparatorIndex);
  symbols.push_back(GetLocaleInfoString(LOCALE_STHOUSAND));
  DCHECK(symbols.size() == kDecimalSymbolsSize);

  String negative_sign = GetLocaleInfoString(LOCALE_SNEGATIVESIGN);
  enum NegativeFormat {
    kNegativeFormatParenthesis = 0,
    kNegativeFormatSignPrefix = 1,
    kNegativeFormatSignSpacePrefix = 2,
    kNegativeFormatSignSuffix = 3,
    kNegativeFormatSpaceSignSuffix = 4,
  };
  DWORD negative_format = kNegativeFormatSignPrefix;
  GetLocaleInfo(LOCALE_INEGNUMBER, negative_format);
  String negative_prefix = g_empty_string;
  String negative_suffix = g_empty_string;
  switch (negative_format) {
    case kNegativeFormatParenthesis:
      negative_prefix = "(";
      negative_suffix = ")";
      break;
    case kNegativeFormatSignSpacePrefix:
      negative_prefix = negative_sign + " ";
      break;
    case kNegativeFormatSignSuffix:
      negative_suffix = negative_sign;
      break;
    case kNegativeFormatSpaceSignSuffix:
      negative_suffix = " " + negative_sign;
      break;
    case kNegativeFormatSignPrefix:  // Fall through.
    default:
      negative_prefix = negative_sign;
      break;
  }
  did_initialize_number_data_ = true;
  SetLocaleData(symbols, g_empty_string, g_empty_string, negative_prefix,
                negative_suffix);
}
}
