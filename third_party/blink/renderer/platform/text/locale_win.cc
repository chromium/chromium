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

#include "third_party/blink/renderer/platform/text/locale_win.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <limits>
#include <memory>

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/win/web_sandbox_support.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/text/date_components.h"
#include "third_party/blink/renderer/platform/text/date_time_format.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"
#include "ui/base/ui_base_features.h"

namespace blink {

namespace {
// Functions that may call Csr* functions in kernelbase.dll and need proxying.
LCID CallLocaleNameToLCID(base::span<UChar> locale) {
  CHECK_LT(locale.size(), static_cast<size_t>(LOCALE_NAME_MAX_LENGTH));
  return ::LocaleNameToLCID(base::as_writable_wcstr(locale.data()), 0);
}

String GetLocaleInfoString(LCID lcid, LCTYPE type, bool defaults_for_locale) {
  if (defaults_for_locale) {
    type = type | LOCALE_NOUSEROVERRIDE;
  }
  int buffer_size_with_nul = ::GetLocaleInfo(lcid, type, 0, 0);
  if (buffer_size_with_nul <= 0) {
    return String();
  }
  StringBuffer<UChar> buffer(buffer_size_with_nul);
  auto span = buffer.Span();
  ::GetLocaleInfo(lcid, type, base::as_writable_wcstr(span.data()),
                  span.size());
  buffer.Shrink(buffer_size_with_nul - 1);
  return String::Adopt(buffer);
}

// Get a string for every value of `types`. If any call fails, returns an
// empty vector, unless `allow_empty` is true, in which case all results
// are returned.
Vector<String> GetLocaleInfoStrings(LCID lcid,
                                    base::span<const LCTYPE> types,
                                    bool defaults_for_locale,
                                    bool allow_empty) {
  Vector<String> result;
  result.reserve(types.size());
  for (const auto& type : types) {
    result.push_back(GetLocaleInfoString(lcid, type, defaults_for_locale));
    if (result.back().empty() && !allow_empty) {
      return {};
    }
  }
  return result;
}

DWORD CallGetLocaleInfoDWORD(LCID lcid, LCTYPE type, DWORD on_failure = 0) {
  DWORD result = on_failure;
  ::GetLocaleInfo(lcid, type | LOCALE_RETURN_NUMBER,
                  reinterpret_cast<LPWSTR>(&result),
                  sizeof(DWORD) / sizeof(TCHAR));
  return result;
}

// First day of week in blink's reckoning.
unsigned GetFirstDayOfWeek(LCID lcid, bool defaults_for_locale) {
  DWORD value = CallGetLocaleInfoDWORD(
      lcid, LOCALE_IFIRSTDAYOFWEEK |
                (defaults_for_locale ? LOCALE_NOUSEROVERRIDE : 0));
  // 0:Monday, ..., 6:Sunday.
  // We need 1 for Monday, 0 for Sunday.
  return (value + 1) % 7;
}

String ExtractLanguageCode(const String& locale) {
  wtf_size_t dash_position = locale.find('-');
  if (dash_position == kNotFound)
    return locale;
  return locale.Left(dash_position);
}

LCID LCIDFromLocaleInternal(LCID user_default_lcid,
                            const String& user_default_language_code,
                            const String& locale) {
  String locale_language_code = ExtractLanguageCode(locale);
  if (DeprecatedEqualIgnoringCase(locale_language_code,
                                  user_default_language_code))
    return user_default_lcid;
  if (locale.length() >= LOCALE_NAME_MAX_LENGTH)
    return 0;
  std::array<UChar, LOCALE_NAME_MAX_LENGTH> buffer;
  auto buffer_slice = base::span(buffer).first(locale.length());
  if (locale.Is8Bit())
    StringImpl::CopyChars(buffer_slice, locale.Span8());
  else
    StringImpl::CopyChars(buffer_slice, locale.Span16());
  buffer[locale.length()] = '\0';
  return CallLocaleNameToLCID(base::span(buffer).first(locale.length() + 1));
}

LCID LCIDFromLocale(const String& locale, bool defaults_for_locale) {
  String user_default_language_code = GetLocaleInfoString(
      LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME, defaults_for_locale);

  LCID lcid = LCIDFromLocaleInternal(LOCALE_USER_DEFAULT,
                                     user_default_language_code, locale);
  if (!lcid)
    lcid = LCIDFromLocaleInternal(
        LOCALE_USER_DEFAULT, user_default_language_code, DefaultLanguage());
  return lcid;
}

std::pair<LCID, unsigned> GetLcidAndFirstDayOfWeek(const String& locale,
                                                   bool defaults_for_locale) {
  WebSandboxSupport* proxy = Platform::Current()->GetSandboxSupport();
  if (proxy && proxy->IsLocaleProxyEnabled()) {
    return proxy->LcidAndFirstDayOfWeek(locale, DefaultLanguage(),
                                        defaults_for_locale);
  }

  LCID lcid = LCIDFromLocale(locale, defaults_for_locale);
  return {lcid, GetFirstDayOfWeek(lcid, defaults_for_locale)};
}

Vector<String> GetMonthLabels(LCID lcid, bool defaults_for_locale) {
  WebSandboxSupport* proxy = Platform::Current()->GetSandboxSupport();
  if (proxy && proxy->IsLocaleProxyEnabled()) {
    return Vector<String>(proxy->MonthLabels(lcid, defaults_for_locale));
  }

  static constexpr LCTYPE kTypes[12] = {
      LOCALE_SMONTHNAME1,  LOCALE_SMONTHNAME2,  LOCALE_SMONTHNAME3,
      LOCALE_SMONTHNAME4,  LOCALE_SMONTHNAME5,  LOCALE_SMONTHNAME6,
      LOCALE_SMONTHNAME7,  LOCALE_SMONTHNAME8,  LOCALE_SMONTHNAME9,
      LOCALE_SMONTHNAME10, LOCALE_SMONTHNAME11, LOCALE_SMONTHNAME12,
  };
  return GetLocaleInfoStrings(lcid, kTypes, defaults_for_locale, false);
}

Vector<String> GetWeekDayShortLabels(LCID lcid, bool defaults_for_locale) {
  WebSandboxSupport* proxy = Platform::Current()->GetSandboxSupport();
  if (proxy && proxy->IsLocaleProxyEnabled()) {
    return Vector<String>(proxy->WeekDayShortLabels(lcid, defaults_for_locale));
  }

  static constexpr LCTYPE kTypes[7] = {
      // Numbered 1 (Monday) - 7 (Sunday), so do 7, then 1-6
      LOCALE_SSHORTESTDAYNAME7, LOCALE_SSHORTESTDAYNAME1,
      LOCALE_SSHORTESTDAYNAME2, LOCALE_SSHORTESTDAYNAME3,
      LOCALE_SSHORTESTDAYNAME4, LOCALE_SSHORTESTDAYNAME5,
      LOCALE_SSHORTESTDAYNAME6};
  return GetLocaleInfoStrings(lcid, kTypes, defaults_for_locale, false);
}

Vector<String> GetShortMonthLabels(LCID lcid, bool defaults_for_locale) {
  WebSandboxSupport* proxy = Platform::Current()->GetSandboxSupport();
  if (proxy && proxy->IsLocaleProxyEnabled()) {
    return Vector<String>(proxy->ShortMonthLabels(lcid, defaults_for_locale));
  }

  static constexpr LCTYPE kTypes[12] = {
      LOCALE_SABBREVMONTHNAME1,  LOCALE_SABBREVMONTHNAME2,
      LOCALE_SABBREVMONTHNAME3,  LOCALE_SABBREVMONTHNAME4,
      LOCALE_SABBREVMONTHNAME5,  LOCALE_SABBREVMONTHNAME6,
      LOCALE_SABBREVMONTHNAME7,  LOCALE_SABBREVMONTHNAME8,
      LOCALE_SABBREVMONTHNAME9,  LOCALE_SABBREVMONTHNAME10,
      LOCALE_SABBREVMONTHNAME11, LOCALE_SABBREVMONTHNAME12,
  };
  return GetLocaleInfoStrings(lcid, kTypes, defaults_for_locale, false);
}

Vector<String> GetTimeAMPMLabels(LCID lcid, bool defaults_for_locale) {
  WebSandboxSupport* proxy = Platform::Current()->GetSandboxSupport();
  if (proxy && proxy->IsLocaleProxyEnabled()) {
    return Vector<String>(proxy->AmPmLabels(lcid, defaults_for_locale));
  }

  static constexpr LCTYPE kTypes[2] = {
      LOCALE_S1159,
      LOCALE_S2359,
  };
  return GetLocaleInfoStrings(lcid, kTypes, defaults_for_locale, true);
}

String GetShortDate(LCID lcid, bool defaults_for_locale) {
  WebSandboxSupport* proxy = Platform::Current()->GetSandboxSupport();
  if (proxy && proxy->IsLocaleProxyEnabled()) {
    return proxy->LocaleString(lcid, LOCALE_SSHORTDATE, defaults_for_locale);
  }
  return GetLocaleInfoString(lcid, LOCALE_SSHORTDATE, defaults_for_locale);
}

String GetYearMonth(LCID lcid, bool defaults_for_locale) {
  WebSandboxSupport* proxy = Platform::Current()->GetSandboxSupport();
  if (proxy && proxy->IsLocaleProxyEnabled()) {
    return proxy->LocaleString(lcid, LOCALE_SYEARMONTH, defaults_for_locale);
  }
  return GetLocaleInfoString(lcid, LOCALE_SYEARMONTH, defaults_for_locale);
}

String GetTimeFormat(LCID lcid, bool defaults_for_locale) {
  WebSandboxSupport* proxy = Platform::Current()->GetSandboxSupport();
  if (proxy && proxy->IsLocaleProxyEnabled()) {
    return proxy->LocaleString(lcid, LOCALE_STIMEFORMAT, defaults_for_locale);
  }
  return GetLocaleInfoString(lcid, LOCALE_STIMEFORMAT, defaults_for_locale);
}

String GetShortTime(LCID lcid, bool defaults_for_locale) {
  WebSandboxSupport* proxy = Platform::Current()->GetSandboxSupport();
  if (proxy && proxy->IsLocaleProxyEnabled()) {
    return proxy->LocaleString(lcid, LOCALE_SSHORTTIME, defaults_for_locale);
  }
  return GetLocaleInfoString(lcid, LOCALE_SSHORTTIME, defaults_for_locale);
}

// Helpers
wtf_size_t CountContinuousLetters(const String& format, wtf_size_t index) {
  wtf_size_t count = 1;
  UChar reference = format[index];
  while (index + 1 < format.length()) {
    if (format[++index] != reference)
      break;
    ++count;
  }
  return count;
}

void CommitLiteralToken(StringBuilder& literal_buffer,
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
String ConvertWindowsDateTimeFormat(const String& format) {
  StringBuilder converted;
  StringBuilder literal_buffer;
  bool in_quote = false;
  bool last_quote_can_be_literal = false;
  for (wtf_size_t i = 0; i < format.length(); ++i) {
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
      wtf_size_t symbol_start = i;
      wtf_size_t count = CountContinuousLetters(format, i);
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

}  // namespace

std::unique_ptr<Locale> Locale::Create(const String& locale) {
  // Whether the default settings for the locale should be used, ignoring user
  // overrides.
  bool defaults_for_locale = WebTestSupport::IsRunningWebTest();
  auto lcid_day = GetLcidAndFirstDayOfWeek(locale, defaults_for_locale);
  return LocaleWin::Create(lcid_day.first, lcid_day.second,
                           defaults_for_locale);
}

inline LocaleWin::LocaleWin(LCID lcid,
                            unsigned first_day_of_week,
                            bool defaults_for_locale)
    : lcid_(lcid),
      first_day_of_week_(first_day_of_week),
      defaults_for_locale_(defaults_for_locale) {}

std::unique_ptr<LocaleWin> LocaleWin::Create(LCID lcid,
                                             unsigned first_day_of_week,
                                             bool defaults_for_locale) {
  return base::WrapUnique(
      new LocaleWin(lcid, first_day_of_week, defaults_for_locale));
}

// Called in unit tests so won't proxy via mojo.
std::unique_ptr<LocaleWin> LocaleWin::CreateForTesting(
    LCID lcid,
    bool defaults_for_locale) {
  return Create(lcid, GetFirstDayOfWeek(lcid, defaults_for_locale),
                defaults_for_locale);
}

LocaleWin::~LocaleWin() = default;

const Vector<String>& LocaleWin::MonthLabels() {
  if (month_labels_.empty()) {
    month_labels_ = GetMonthLabels(lcid_, defaults_for_locale_);
    if (month_labels_.empty()) {
      std::ranges::copy(kFallbackMonthNames, std::back_inserter(month_labels_));
    }
  }
  return month_labels_;
}

const Vector<String>& LocaleWin::WeekDayShortLabels() {
  if (week_day_short_labels_.empty()) {
    week_day_short_labels_ = GetWeekDayShortLabels(lcid_, defaults_for_locale_);
    if (week_day_short_labels_.empty()) {
      std::ranges::copy(kFallbackWeekdayShortNames,
                        std::back_inserter(week_day_short_labels_));
    }
  }
  return week_day_short_labels_;
}

unsigned LocaleWin::FirstDayOfWeek() {
  return first_day_of_week_;
}

bool LocaleWin::IsRTL() {
  unicode::CharDirection dir = unicode::Direction(MonthLabels()[0][0]);
  return dir == unicode::kRightToLeft || dir == unicode::kRightToLeftArabic;
}

String LocaleWin::DateFormat() {
  if (date_format_.IsNull()) {
    date_format_ =
        ConvertWindowsDateTimeFormat(GetShortDate(lcid_, defaults_for_locale_));
  }
  return date_format_;
}

String LocaleWin::DateFormatForTesting(const String& windows_format) {
  return ConvertWindowsDateTimeFormat(windows_format);
}

String LocaleWin::MonthFormat() {
  if (month_format_.IsNull()) {
    month_format_ =
        ConvertWindowsDateTimeFormat(GetYearMonth(lcid_, defaults_for_locale_));
  }
  return month_format_;
}

String LocaleWin::ShortMonthFormat() {
  if (short_month_format_.IsNull()) {
    short_month_format_ =
        ConvertWindowsDateTimeFormat(GetYearMonth(lcid_, defaults_for_locale_))
            .Replace("MMMM", "MMM");
  }
  return short_month_format_;
}

String LocaleWin::TimeFormat() {
  if (time_format_with_seconds_.IsNull()) {
    time_format_with_seconds_ = ConvertWindowsDateTimeFormat(
        GetTimeFormat(lcid_, defaults_for_locale_));
  }
  return time_format_with_seconds_;
}

String LocaleWin::ShortTimeFormat() {
  if (time_format_without_seconds_.IsNull()) {
    time_format_without_seconds_ =
        ConvertWindowsDateTimeFormat(GetShortTime(lcid_, defaults_for_locale_));
  }
  return time_format_without_seconds_;
}

// TODO(crbug.com/40408399) Proxy via mojo, or accept two calls.
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

// TODO(crbug.com/40408399) Proxy via mojo, or accept two calls.
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
    short_month_labels_ = GetShortMonthLabels(lcid_, defaults_for_locale_);
    if (short_month_labels_.empty()) {
      std::ranges::copy(kFallbackMonthShortNames,
                        std::back_inserter(short_month_labels_));
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
    time_ampm_labels_ = GetTimeAMPMLabels(lcid_, defaults_for_locale_);
  }
  return time_ampm_labels_;
}

void LocaleWin::InitializeLocaleData() {
  if (did_initialize_number_data_)
    return;
  // LOCALE_IDIGITSUBSTITUTION values from:
  // https://learn.microsoft.com/en-us/windows/win32/intl/locale-idigitsubstitution
  enum DigitSubstitution {
    kDigitSubstitutionContext = 0,
    kDigitSubstitution0to9 = 1,
    kDigitSubstitutionNative = 2,
  };
  // LOCALE_INEGNUMBER constants from:
  // https://learn.microsoft.com/en-us/windows/win32/intl/locale-ineg-constants
  enum NegativeFormat {
    kNegativeFormatParenthesis = 0,
    kNegativeFormatSignPrefix = 1,
    kNegativeFormatSignSpacePrefix = 2,
    kNegativeFormatSignSuffix = 3,
    kNegativeFormatSpaceSignSuffix = 4,
  };

  DWORD digit_substitution;
  String digits;
  String decimal;
  String thousand;
  String negative_sign;
  DWORD negnumber;

  WebSandboxSupport* proxy = Platform::Current()->GetSandboxSupport();
  if (proxy && proxy->IsLocaleProxyEnabled()) {
    auto init_data = proxy->DigitsAndSigns(lcid_, defaults_for_locale_);
    digit_substitution = init_data->digit_substitution;
    digits = std::move(init_data->digits);
    decimal = std::move(init_data->decimal);
    thousand = std::move(init_data->thousand);
    negative_sign = std::move(init_data->negative_sign);
    negnumber = init_data->negnumber;
  } else {
    digit_substitution = CallGetLocaleInfoDWORD(
        lcid_, LOCALE_IDIGITSUBSTITUTION, kDigitSubstitution0to9);
    if (digit_substitution != kDigitSubstitution0to9) {
      digits = GetLocaleInfoString(lcid_, LOCALE_SNATIVEDIGITS,
                                   defaults_for_locale_);
    }
    decimal = GetLocaleInfoString(lcid_, LOCALE_SDECIMAL, defaults_for_locale_);
    thousand =
        GetLocaleInfoString(lcid_, LOCALE_STHOUSAND, defaults_for_locale_);
    negative_sign =
        GetLocaleInfoString(lcid_, LOCALE_SNEGATIVESIGN, defaults_for_locale_);
    negnumber = CallGetLocaleInfoDWORD(lcid_, LOCALE_INEGNUMBER,
                                       kNegativeFormatSignPrefix);
  }
  // No Locale calls after this point.
  Vector<String, kDecimalSymbolsSize> symbols;
  if (digit_substitution == kDigitSubstitution0to9) {
    symbols = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
  } else {
    DCHECK_GE(digits.length(), 10u);
    for (wtf_size_t i = 0; i < 10; ++i) {
      symbols.push_back(digits.Substring(i, 1));
    }
  }
  DCHECK_EQ(symbols.size(), kDecimalSeparatorIndex);
  symbols.push_back(decimal);
  DCHECK_EQ(symbols.size(), kGroupSeparatorIndex);
  symbols.push_back(thousand);
  DCHECK_EQ(symbols.size(), kDecimalSymbolsSize);

  String negative_prefix = g_empty_string;
  String negative_suffix = g_empty_string;
  switch (negnumber) {
    case kNegativeFormatParenthesis:
      negative_prefix = "(";
      negative_suffix = ")";
      break;
    case kNegativeFormatSignSpacePrefix:
      negative_prefix = StrCat({negative_sign, " "});
      break;
    case kNegativeFormatSignSuffix:
      negative_suffix = negative_sign;
      break;
    case kNegativeFormatSpaceSignSuffix:
      negative_suffix = StrCat({" ", negative_sign});
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
