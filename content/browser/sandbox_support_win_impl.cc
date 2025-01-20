// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <string>
#include <string_view>

#include "base/strings/string_util.h"
#include "base/strings/string_util_win.h"
#include "content/browser/sandbox_support_impl.h"

namespace content {
namespace {
LCID CallLocaleNameToLCID(std::u16string_view locale) {
  CHECK(locale.size() < LOCALE_NAME_MAX_LENGTH);
  return ::LocaleNameToLCID(base::as_wcstr(locale), 0);
}

std::u16string GetLocaleInfoString(LCID lcid,
                                   LCTYPE type,
                                   bool force_defaults) {
  if (force_defaults) {
    type = type | LOCALE_NOUSEROVERRIDE;
  }
  int size_with_nul = ::GetLocaleInfo(lcid, type, 0, 0);
  if (size_with_nul <= 0) {
    return std::u16string();
  }
  std::u16string buffer;
  // basic_string guarantees that `buffer` can be indexed from
  // [0, size], with the requirement that buffer[size] is only set
  // to NUL.
  buffer.resize(size_with_nul - 1);
  ::GetLocaleInfo(lcid, type, base::as_writable_wcstr(buffer.data()),
                  size_with_nul);
  return buffer;
}

std::vector<std::u16string> GetLocaleInfoStrings(LCID lcid,
                                                 base::span<const LCTYPE> types,
                                                 bool force_defaults,
                                                 bool allow_empty) {
  std::vector<std::u16string> strings;
  for (const auto& type : types) {
    auto str = GetLocaleInfoString(lcid, type, force_defaults);
    if (str.empty() && !allow_empty) {
      return {};
    }
    strings.push_back(std::move(str));
  }
  return strings;
}

std::optional<DWORD> GetLocaleInfoDWORD(LCID lcid,
                                        LCTYPE type,
                                        bool force_defaults) {
  DWORD result = 0;
  if (force_defaults) {
    type = type | LOCALE_NOUSEROVERRIDE;
  }
  if (::GetLocaleInfo(lcid, type | LOCALE_RETURN_NUMBER,
                      reinterpret_cast<LPWSTR>(&result),
                      sizeof(DWORD) / sizeof(TCHAR)) > 0) {
    return result;
  }
  return std::nullopt;
}

std::u16string_view LanguageCode(const std::u16string_view locale) {
  size_t dash_pos = locale.find('-');
  if (dash_pos == std::u16string::npos) {
    return locale;
  }
  return locale.substr(0, dash_pos);
}

LCID LCIDFromLocale(const std::u16string& locale, bool force_defaults) {
  auto default_language_code = GetLocaleInfoString(
      LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME, force_defaults);
  auto locale_language_code = LanguageCode(locale);
  if (base::EqualsCaseInsensitiveASCII(locale_language_code,
                                       default_language_code)) {
    return LOCALE_USER_DEFAULT;
  }
  return CallLocaleNameToLCID(locale);
}

}  // namespace

SandboxSupportImpl::SandboxSupportImpl() = default;
SandboxSupportImpl::~SandboxSupportImpl() = default;

void SandboxSupportImpl::BindReceiver(
    mojo::PendingReceiver<mojom::SandboxSupport> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void SandboxSupportImpl::LcidAndFirstDayOfWeek(
    const std::u16string& locale,
    const std::u16string& default_language,
    bool force_defaults,
    LcidAndFirstDayOfWeekCallback callback) {
  if (locale.size() > LOCALE_NAME_MAX_LENGTH ||
      default_language.size() > LOCALE_NAME_MAX_LENGTH) {
    receivers_.ReportBadMessage("Locale larger than LOCALE_NAME_MAX_LENGTH.");
    return;
  }
  LCID lcid = LCIDFromLocale(locale, force_defaults);
  if (!lcid) {
    lcid = LCIDFromLocale(default_language, force_defaults);
  }
  auto first_day =
      GetLocaleInfoDWORD(lcid, LOCALE_IFIRSTDAYOFWEEK, force_defaults);
  int first_day_of_week = ((first_day.has_value() ? *first_day : 0) + 1) % 7;
  std::move(callback).Run(lcid, first_day_of_week);
}

// https://learn.microsoft.com/en-us/windows/win32/intl/locale-idigitsubstitution
constexpr inline DWORD kDigitSubstitution0to9 = 1;
// https://learn.microsoft.com/en-us/windows/win32/intl/locale-ineg-constants
constexpr inline DWORD kNegNumberSignPrefix = 1;

void SandboxSupportImpl::DigitsAndSigns(uint32_t lcid,
                                        bool force_defaults,
                                        DigitsAndSignsCallback callback) {
  DWORD digit_substitution =
      GetLocaleInfoDWORD(lcid, LOCALE_IDIGITSUBSTITUTION, force_defaults)
          .value_or(kDigitSubstitution0to9);
  std::u16string digits;
  if (digit_substitution != kDigitSubstitution0to9) {
    digits = GetLocaleInfoString(lcid, LOCALE_SNATIVEDIGITS, force_defaults);
  }
  auto decimal = GetLocaleInfoString(lcid, LOCALE_SDECIMAL, force_defaults);
  auto thousand = GetLocaleInfoString(lcid, LOCALE_STHOUSAND, force_defaults);
  auto negative_sign =
      GetLocaleInfoString(lcid, LOCALE_SNEGATIVESIGN, force_defaults);
  DWORD negnumber = GetLocaleInfoDWORD(lcid, LOCALE_INEGNUMBER, force_defaults)
                        .value_or(kNegNumberSignPrefix);
  std::move(callback).Run(digit_substitution, digits, decimal, thousand,
                          negative_sign, negnumber);
}

void SandboxSupportImpl::LocaleStrings(uint32_t lcid,
                                       bool force_defaults,
                                       LcTypeStrings collection,
                                       LocaleStringsCallback callback) {
  std::vector<std::u16string> result;
  switch (collection) {
    case LcTypeStrings::kMonths: {
      static constexpr LCTYPE kTypes[12] = {
          LOCALE_SMONTHNAME1,  LOCALE_SMONTHNAME2,  LOCALE_SMONTHNAME3,
          LOCALE_SMONTHNAME4,  LOCALE_SMONTHNAME5,  LOCALE_SMONTHNAME6,
          LOCALE_SMONTHNAME7,  LOCALE_SMONTHNAME8,  LOCALE_SMONTHNAME9,
          LOCALE_SMONTHNAME10, LOCALE_SMONTHNAME11, LOCALE_SMONTHNAME12,
      };
      result = GetLocaleInfoStrings(lcid, kTypes, force_defaults, false);
      break;
    }
    case LcTypeStrings::kShortMonths: {
      static constexpr LCTYPE kTypes[12] = {
          LOCALE_SABBREVMONTHNAME1,  LOCALE_SABBREVMONTHNAME2,
          LOCALE_SABBREVMONTHNAME3,  LOCALE_SABBREVMONTHNAME4,
          LOCALE_SABBREVMONTHNAME5,  LOCALE_SABBREVMONTHNAME6,
          LOCALE_SABBREVMONTHNAME7,  LOCALE_SABBREVMONTHNAME8,
          LOCALE_SABBREVMONTHNAME9,  LOCALE_SABBREVMONTHNAME10,
          LOCALE_SABBREVMONTHNAME11, LOCALE_SABBREVMONTHNAME12,
      };
      result = GetLocaleInfoStrings(lcid, kTypes, force_defaults, false);
      break;
    }
    case LcTypeStrings::kShortWeekDays: {
      static constexpr LCTYPE kTypes[7] = {
          // Numbered 1 (Monday) - 7 (Sunday), so do 7, then 1-6
          LOCALE_SSHORTESTDAYNAME7, LOCALE_SSHORTESTDAYNAME1,
          LOCALE_SSHORTESTDAYNAME2, LOCALE_SSHORTESTDAYNAME3,
          LOCALE_SSHORTESTDAYNAME4, LOCALE_SSHORTESTDAYNAME5,
          LOCALE_SSHORTESTDAYNAME6};
      result = GetLocaleInfoStrings(lcid, kTypes, force_defaults, false);
      break;
    }
    case LcTypeStrings::kAmPm: {
      static constexpr LCTYPE kTypes[2] = {
          LOCALE_S1159,
          LOCALE_S2359,
      };
      result = GetLocaleInfoStrings(lcid, kTypes, force_defaults, true);
      break;
    }
  }

  std::move(callback).Run(result);
}

void SandboxSupportImpl::LocaleString(uint32_t lcid,
                                      bool force_defaults,
                                      LcTypeString type,
                                      LocaleStringCallback callback) {
  LCTYPE lctype;
  switch (type) {
    case LcTypeString::kShortDate:
      lctype = LOCALE_SSHORTDATE;
      break;
    case LcTypeString::kYearMonth:
      lctype = LOCALE_SYEARMONTH;
      break;
    case LcTypeString::kTimeFormat:
      lctype = LOCALE_STIMEFORMAT;
      break;
    case LcTypeString::kShortTime:
      lctype = LOCALE_SSHORTTIME;
      break;
  }
  auto result = GetLocaleInfoString(lcid, lctype, force_defaults);
  std::move(callback).Run(result);
}

}  // namespace content
