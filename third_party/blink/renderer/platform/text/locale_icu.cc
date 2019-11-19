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

#include "third_party/blink/renderer/platform/text/locale_icu.h"

#include <unicode/udatpg.h>
#include <unicode/udisplaycontext.h>
#include <unicode/uloc.h>
#include <limits>
#include <memory>

#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/date_math.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

std::unique_ptr<Locale> Locale::Create(const String& locale) {
  return std::make_unique<LocaleICU>(locale.Utf8());
}

LocaleICU::LocaleICU(const std::string& locale)
    : locale_(locale),
      number_format_(nullptr),
      short_date_format_(nullptr),
      did_create_decimal_format_(false),
      did_create_short_date_format_(false),
      first_day_of_week_(0),
      medium_time_format_(nullptr),
      short_time_format_(nullptr),
      did_create_time_format_(false) {}

LocaleICU::~LocaleICU() {
  unum_close(number_format_);
  udat_close(short_date_format_);
  udat_close(medium_time_format_);
  udat_close(short_time_format_);
}

String LocaleICU::DecimalSymbol(UNumberFormatSymbol symbol) {
  UErrorCode status = U_ZERO_ERROR;
  int32_t buffer_length =
      unum_getSymbol(number_format_, symbol, nullptr, 0, &status);
  DCHECK(U_SUCCESS(status) || status == U_BUFFER_OVERFLOW_ERROR);
  if (U_FAILURE(status) && status != U_BUFFER_OVERFLOW_ERROR)
    return String();
  StringBuffer<UChar> buffer(buffer_length);
  status = U_ZERO_ERROR;
  unum_getSymbol(number_format_, symbol, buffer.Characters(), buffer_length,
                 &status);
  if (U_FAILURE(status))
    return String();
  return String::Adopt(buffer);
}

String LocaleICU::DecimalTextAttribute(UNumberFormatTextAttribute tag) {
  UErrorCode status = U_ZERO_ERROR;
  int32_t buffer_length =
      unum_getTextAttribute(number_format_, tag, nullptr, 0, &status);
  DCHECK(U_SUCCESS(status) || status == U_BUFFER_OVERFLOW_ERROR);
  if (U_FAILURE(status) && status != U_BUFFER_OVERFLOW_ERROR)
    return String();
  StringBuffer<UChar> buffer(buffer_length);
  status = U_ZERO_ERROR;
  unum_getTextAttribute(number_format_, tag, buffer.Characters(), buffer_length,
                        &status);
  DCHECK(U_SUCCESS(status));
  if (U_FAILURE(status))
    return String();
  return String::Adopt(buffer);
}

void LocaleICU::InitializeLocaleData() {
  if (did_create_decimal_format_)
    return;
  did_create_decimal_format_ = true;
  UErrorCode status = U_ZERO_ERROR;
  number_format_ =
      unum_open(UNUM_DECIMAL, nullptr, 0, locale_.c_str(), nullptr, &status);
  if (!U_SUCCESS(status))
    return;

  Vector<String, kDecimalSymbolsSize> symbols;
  symbols.push_back(DecimalSymbol(UNUM_ZERO_DIGIT_SYMBOL));
  symbols.push_back(DecimalSymbol(UNUM_ONE_DIGIT_SYMBOL));
  symbols.push_back(DecimalSymbol(UNUM_TWO_DIGIT_SYMBOL));
  symbols.push_back(DecimalSymbol(UNUM_THREE_DIGIT_SYMBOL));
  symbols.push_back(DecimalSymbol(UNUM_FOUR_DIGIT_SYMBOL));
  symbols.push_back(DecimalSymbol(UNUM_FIVE_DIGIT_SYMBOL));
  symbols.push_back(DecimalSymbol(UNUM_SIX_DIGIT_SYMBOL));
  symbols.push_back(DecimalSymbol(UNUM_SEVEN_DIGIT_SYMBOL));
  symbols.push_back(DecimalSymbol(UNUM_EIGHT_DIGIT_SYMBOL));
  symbols.push_back(DecimalSymbol(UNUM_NINE_DIGIT_SYMBOL));
  symbols.push_back(DecimalSymbol(UNUM_DECIMAL_SEPARATOR_SYMBOL));
  symbols.push_back(DecimalSymbol(UNUM_GROUPING_SEPARATOR_SYMBOL));
  DCHECK_EQ(symbols.size(), kDecimalSymbolsSize);
  SetLocaleData(symbols, DecimalTextAttribute(UNUM_POSITIVE_PREFIX),
                DecimalTextAttribute(UNUM_POSITIVE_SUFFIX),
                DecimalTextAttribute(UNUM_NEGATIVE_PREFIX),
                DecimalTextAttribute(UNUM_NEGATIVE_SUFFIX));
}

bool LocaleICU::InitializeShortDateFormat() {
  if (did_create_short_date_format_)
    return short_date_format_;
  short_date_format_ = OpenDateFormat(UDAT_NONE, UDAT_SHORT);
  did_create_short_date_format_ = true;
  return short_date_format_;
}

UDateFormat* LocaleICU::OpenDateFormat(UDateFormatStyle time_style,
                                       UDateFormatStyle date_style) const {
  const UChar kGmtTimezone[3] = {'G', 'M', 'T'};
  UErrorCode status = U_ZERO_ERROR;
  return udat_open(time_style, date_style, locale_.c_str(), kGmtTimezone,
                   base::size(kGmtTimezone), nullptr, -1, &status);
}

// We cannot use udat_*Symbols API to get standalone month names to use in
// calendar headers for Russian and potentially other languages. Instead,
// we have to format dates with patterns "LLLL" or "LLL" and set the
// display context to 'standalone'. See
// http://bugs.icu-project.org/trac/ticket/11552
UDateFormat* LocaleICU::OpenDateFormatForStandAloneMonthLabels(
    bool is_short) const {
  const UChar kMonthPattern[4] = {'L', 'L', 'L', 'L'};
  UErrorCode status = U_ZERO_ERROR;
  UDateFormat* formatter =
      udat_open(UDAT_PATTERN, UDAT_PATTERN, locale_.c_str(), nullptr, -1,
                kMonthPattern, is_short ? 3 : 4, &status);
  udat_setContext(formatter, UDISPCTX_CAPITALIZATION_FOR_STANDALONE, &status);
  DCHECK(U_SUCCESS(status));
  return formatter;
}

static String GetDateFormatPattern(const UDateFormat* date_format) {
  if (!date_format)
    return g_empty_string;

  UErrorCode status = U_ZERO_ERROR;
  int32_t length = udat_toPattern(date_format, TRUE, nullptr, 0, &status);
  if (status != U_BUFFER_OVERFLOW_ERROR || !length)
    return g_empty_string;
  StringBuffer<UChar> buffer(length);
  status = U_ZERO_ERROR;
  udat_toPattern(date_format, TRUE, buffer.Characters(), length, &status);
  if (U_FAILURE(status))
    return g_empty_string;
  return String::Adopt(buffer);
}

std::unique_ptr<Vector<String>> LocaleICU::CreateLabelVector(
    const UDateFormat* date_format,
    UDateFormatSymbolType type,
    int32_t start_index,
    int32_t size) {
  if (!date_format)
    return std::unique_ptr<Vector<String>>();
  if (udat_countSymbols(date_format, type) != start_index + size)
    return std::unique_ptr<Vector<String>>();

  std::unique_ptr<Vector<String>> labels = std::make_unique<Vector<String>>();
  labels->ReserveCapacity(size);
  bool is_stand_alone_month = (type == UDAT_STANDALONE_MONTHS) ||
                              (type == UDAT_STANDALONE_SHORT_MONTHS);
  for (int32_t i = 0; i < size; ++i) {
    UErrorCode status = U_ZERO_ERROR;
    int32_t length;
    static const UDate kEpoch = U_MILLIS_PER_DAY * 15u;  // 1970-01-15
    static const UDate kMonth = U_MILLIS_PER_DAY * 30u;  // 30 days in ms
    if (is_stand_alone_month) {
      length = udat_format(date_format, kEpoch + i * kMonth, nullptr, 0,
                           nullptr, &status);
    } else {
      length = udat_getSymbols(date_format, type, start_index + i, nullptr, 0,
                               &status);
    }
    if (status != U_BUFFER_OVERFLOW_ERROR)
      return std::unique_ptr<Vector<String>>();
    StringBuffer<UChar> buffer(length);
    status = U_ZERO_ERROR;
    if (is_stand_alone_month) {
      udat_format(date_format, kEpoch + i * kMonth, buffer.Characters(), length,
                  nullptr, &status);
    } else {
      udat_getSymbols(date_format, type, start_index + i, buffer.Characters(),
                      length, &status);
    }
    if (U_FAILURE(status))
      return std::unique_ptr<Vector<String>>();
    labels->push_back(String::Adopt(buffer));
  }
  return labels;
}

static std::unique_ptr<Vector<String>> CreateFallbackWeekDayShortLabels() {
  std::unique_ptr<Vector<String>> labels = std::make_unique<Vector<String>>();
  labels->ReserveCapacity(7);
  labels->push_back("Sun");
  labels->push_back("Mon");
  labels->push_back("Tue");
  labels->push_back("Wed");
  labels->push_back("Thu");
  labels->push_back("Fri");
  labels->push_back("Sat");
  return labels;
}

void LocaleICU::InitializeCalendar() {
  if (week_day_short_labels_)
    return;

  if (!InitializeShortDateFormat()) {
    first_day_of_week_ = 0;
    week_day_short_labels_ = CreateFallbackWeekDayShortLabels();
    return;
  }
  first_day_of_week_ = ucal_getAttribute(udat_getCalendar(short_date_format_),
                                         UCAL_FIRST_DAY_OF_WEEK) -
                       UCAL_SUNDAY;

  if (RuntimeEnabledFeatures::FormControlsRefreshEnabled()) {
    week_day_short_labels_ = CreateLabelVector(
        short_date_format_, UDAT_NARROW_WEEKDAYS, UCAL_SUNDAY, 7);
  } else {
    week_day_short_labels_ = CreateLabelVector(
        short_date_format_, UDAT_SHORT_WEEKDAYS, UCAL_SUNDAY, 7);
  }
  if (!week_day_short_labels_)
    week_day_short_labels_ = CreateFallbackWeekDayShortLabels();
}

static std::unique_ptr<Vector<String>> CreateFallbackMonthLabels() {
  std::unique_ptr<Vector<String>> labels = std::make_unique<Vector<String>>();
  labels->ReserveCapacity(base::size(WTF::kMonthFullName));
  for (unsigned i = 0; i < base::size(WTF::kMonthFullName); ++i)
    labels->push_back(WTF::kMonthFullName[i]);
  return labels;
}

const Vector<String>& LocaleICU::MonthLabels() {
  if (month_labels_)
    return *month_labels_;
  if (InitializeShortDateFormat()) {
    month_labels_ =
        CreateLabelVector(short_date_format_, UDAT_MONTHS, UCAL_JANUARY, 12);
    if (month_labels_)
      return *month_labels_;
  }
  month_labels_ = CreateFallbackMonthLabels();
  return *month_labels_;
}

const Vector<String>& LocaleICU::WeekDayShortLabels() {
  InitializeCalendar();
  return *week_day_short_labels_;
}

unsigned LocaleICU::FirstDayOfWeek() {
  InitializeCalendar();
  return first_day_of_week_;
}

bool LocaleICU::IsRTL() {
  UErrorCode status = U_ZERO_ERROR;
  return uloc_getCharacterOrientation(locale_.c_str(), &status) ==
         ULOC_LAYOUT_RTL;
}

static std::unique_ptr<Vector<String>> CreateFallbackAMPMLabels() {
  std::unique_ptr<Vector<String>> labels = std::make_unique<Vector<String>>();
  labels->ReserveCapacity(2);
  labels->push_back("AM");
  labels->push_back("PM");
  return labels;
}

void LocaleICU::InitializeDateTimeFormat() {
  if (did_create_time_format_)
    return;

  // We assume ICU medium time pattern and short time pattern are compatible
  // with LDML, because ICU specific pattern character "V" doesn't appear
  // in both medium and short time pattern.
  medium_time_format_ = OpenDateFormat(UDAT_MEDIUM, UDAT_NONE);
  time_format_with_seconds_ = GetDateFormatPattern(medium_time_format_);

  short_time_format_ = OpenDateFormat(UDAT_SHORT, UDAT_NONE);
  time_format_without_seconds_ = GetDateFormatPattern(short_time_format_);

  UDateFormat* date_time_format_with_seconds =
      OpenDateFormat(UDAT_MEDIUM, UDAT_SHORT);
  date_time_format_with_seconds_ =
      GetDateFormatPattern(date_time_format_with_seconds);
  udat_close(date_time_format_with_seconds);

  UDateFormat* date_time_format_without_seconds =
      OpenDateFormat(UDAT_SHORT, UDAT_SHORT);
  date_time_format_without_seconds_ =
      GetDateFormatPattern(date_time_format_without_seconds);
  udat_close(date_time_format_without_seconds);

  std::unique_ptr<Vector<String>> time_ampm_labels =
      CreateLabelVector(medium_time_format_, UDAT_AM_PMS, UCAL_AM, 2);
  if (!time_ampm_labels)
    time_ampm_labels = CreateFallbackAMPMLabels();
  time_ampm_labels_ = *time_ampm_labels;

  did_create_time_format_ = true;
}

String LocaleICU::DateFormat() {
  if (!date_format_.IsNull())
    return date_format_;
  if (!InitializeShortDateFormat())
    return "yyyy-MM-dd";
  date_format_ = GetDateFormatPattern(short_date_format_);
  return date_format_;
}

static String GetFormatForSkeleton(const char* locale, const String& skeleton) {
  String format = "yyyy-MM";
  UErrorCode status = U_ZERO_ERROR;
  UDateTimePatternGenerator* pattern_generator = udatpg_open(locale, &status);
  if (!pattern_generator)
    return format;
  status = U_ZERO_ERROR;
  Vector<UChar> skeleton_characters;
  skeleton.AppendTo(skeleton_characters);
  int32_t length =
      udatpg_getBestPattern(pattern_generator, skeleton_characters.data(),
                            skeleton_characters.size(), nullptr, 0, &status);
  if (status == U_BUFFER_OVERFLOW_ERROR && length) {
    StringBuffer<UChar> buffer(length);
    status = U_ZERO_ERROR;
    udatpg_getBestPattern(pattern_generator, skeleton_characters.data(),
                          skeleton_characters.size(), buffer.Characters(),
                          length, &status);
    if (U_SUCCESS(status))
      format = String::Adopt(buffer);
  }
  udatpg_close(pattern_generator);
  return format;
}

String LocaleICU::MonthFormat() {
  if (!month_format_.IsNull())
    return month_format_;
  // Gets a format for "MMMM" because Windows API always provides formats for
  // "MMMM" in some locales.
  month_format_ = GetFormatForSkeleton(locale_.c_str(), "yyyyMMMM");
  return month_format_;
}

String LocaleICU::ShortMonthFormat() {
  if (!short_month_format_.IsNull())
    return short_month_format_;
  short_month_format_ = GetFormatForSkeleton(locale_.c_str(), "yyyyMMM");
  return short_month_format_;
}

String LocaleICU::TimeFormat() {
  InitializeDateTimeFormat();
  return time_format_with_seconds_;
}

String LocaleICU::ShortTimeFormat() {
  InitializeDateTimeFormat();
  return time_format_without_seconds_;
}

String LocaleICU::DateTimeFormatWithSeconds() {
  InitializeDateTimeFormat();
  return date_time_format_with_seconds_;
}

String LocaleICU::DateTimeFormatWithoutSeconds() {
  InitializeDateTimeFormat();
  return date_time_format_without_seconds_;
}

const Vector<String>& LocaleICU::ShortMonthLabels() {
  if (!short_month_labels_.IsEmpty())
    return short_month_labels_;
  if (InitializeShortDateFormat()) {
    if (std::unique_ptr<Vector<String>> labels = CreateLabelVector(
            short_date_format_, UDAT_SHORT_MONTHS, UCAL_JANUARY, 12)) {
      short_month_labels_ = *labels;
      return short_month_labels_;
    }
  }
  short_month_labels_.ReserveCapacity(base::size(WTF::kMonthName));
  for (unsigned i = 0; i < base::size(WTF::kMonthName); ++i)
    short_month_labels_.push_back(WTF::kMonthName[i]);
  return short_month_labels_;
}

const Vector<String>& LocaleICU::StandAloneMonthLabels() {
  if (!stand_alone_month_labels_.IsEmpty())
    return stand_alone_month_labels_;
  UDateFormat* month_formatter = OpenDateFormatForStandAloneMonthLabels(false);
  if (month_formatter) {
    if (std::unique_ptr<Vector<String>> labels = CreateLabelVector(
            month_formatter, UDAT_STANDALONE_MONTHS, UCAL_JANUARY, 12)) {
      stand_alone_month_labels_ = *labels;
      udat_close(month_formatter);
      return stand_alone_month_labels_;
    }
    udat_close(month_formatter);
  }
  stand_alone_month_labels_ = MonthLabels();
  return stand_alone_month_labels_;
}

const Vector<String>& LocaleICU::ShortStandAloneMonthLabels() {
  if (!short_stand_alone_month_labels_.IsEmpty())
    return short_stand_alone_month_labels_;
  UDateFormat* month_formatter = OpenDateFormatForStandAloneMonthLabels(true);
  if (month_formatter) {
    if (std::unique_ptr<Vector<String>> labels = CreateLabelVector(
            month_formatter, UDAT_STANDALONE_SHORT_MONTHS, UCAL_JANUARY, 12)) {
      short_stand_alone_month_labels_ = *labels;
      udat_close(month_formatter);
      return short_stand_alone_month_labels_;
    }
    udat_close(month_formatter);
  }
  short_stand_alone_month_labels_ = ShortMonthLabels();
  return short_stand_alone_month_labels_;
}

const Vector<String>& LocaleICU::TimeAMPMLabels() {
  InitializeDateTimeFormat();
  return time_ampm_labels_;
}

}  // namespace blink
