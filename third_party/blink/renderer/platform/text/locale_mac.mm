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

#include "third_party/blink/renderer/platform/text/locale_mac.h"

#import <Foundation/Foundation.h>

#include <iterator>
#include <memory>

#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/blink/renderer/platform/wtf/date_math.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "ui/base/ui_base_features.h"

namespace blink {

static inline String LanguageFromLocale(const String& locale) {
  String normalized_locale = locale;
  normalized_locale.Replace('-', '_');
  wtf_size_t separator_position = normalized_locale.find('_');
  if (separator_position == kNotFound)
    return normalized_locale;
  return normalized_locale.Left(separator_position);
}

static NSLocale* DetermineLocale(const String& locale) {
  if (!WebTestSupport::IsRunningWebTest()) {
    NSLocale* current_locale = NSLocale.currentLocale;
    String current_locale_language =
        LanguageFromLocale(String(current_locale.localeIdentifier));
    String locale_language = LanguageFromLocale(locale);
    if (DeprecatedEqualIgnoringCase(current_locale_language, locale_language))
      return current_locale;
  }
  // It seems localeWithLocaleIdentifier accepts dash-separated locale
  // identifier.
  return [NSLocale localeWithLocaleIdentifier:locale];
}

std::unique_ptr<Locale> Locale::Create(const String& locale) {
  return LocaleMac::Create(DetermineLocale(locale));
}

static NSDateFormatter* CreateDateTimeFormatter(
    NSLocale* locale,
    NSCalendar* calendar,
    NSDateFormatterStyle date_style,
    NSDateFormatterStyle time_style) {
  NSDateFormatter* formatter = [[NSDateFormatter alloc] init];
  formatter.locale = locale;
  formatter.dateStyle = date_style;
  formatter.timeStyle = time_style;
  formatter.timeZone = [NSTimeZone timeZoneWithAbbreviation:@"UTC"];
  formatter.calendar = calendar;
  return formatter;
}

static inline String NormalizeWhitespace(const String& date_time_format) {
  String normalized_date_time_format = date_time_format;
  // Revert ICU 72 change that introduced U+202F instead of U+0020
  // to separate time from AM/PM.
  //
  // TODO(https://crbug.com/1453047): Move this normalization to
  // `//third_party/icu/` or `//third_party/icu/patches/`.
  return normalized_date_time_format.Replace(0x202f, 0x20);
}

LocaleMac::LocaleMac(NSLocale* locale)
    : locale_(locale),
      gregorian_calendar_([[NSCalendar alloc]
          initWithCalendarIdentifier:NSCalendarIdentifierGregorian]),
      did_initialize_number_data_(false) {
  NSArray* available_languages = NSLocale.ISOLanguageCodes;
  // NSLocale returns a lower case NSLocaleLanguageCode so we don't have care
  // about case.
  NSString* language = [locale_ objectForKey:NSLocaleLanguageCode];
  if ([available_languages indexOfObject:language] == NSNotFound) {
    locale_ = [[NSLocale alloc] initWithLocaleIdentifier:DefaultLanguage()];
  }
  [gregorian_calendar_ setLocale:locale_];
}

LocaleMac::~LocaleMac() = default;

std::unique_ptr<LocaleMac> LocaleMac::Create(const String& locale_identifier) {
  NSLocale* locale = [NSLocale localeWithLocaleIdentifier:locale_identifier];
  return LocaleMac::Create(locale);
}

std::unique_ptr<LocaleMac> LocaleMac::Create(NSLocale* locale) {
  return base::WrapUnique(new LocaleMac(locale));
}

NSDateFormatter* LocaleMac::ShortDateFormatter() {
  return CreateDateTimeFormatter(locale_, gregorian_calendar_,
                                 NSDateFormatterShortStyle,
                                 NSDateFormatterNoStyle);
}

const Vector<String>& LocaleMac::MonthLabels() {
  if (month_labels_.empty()) {
    month_labels_.reserve(12);
    NSArray* array = ShortDateFormatter().monthSymbols;
    if (array.count == 12) {
      for (unsigned i = 0; i < 12; ++i) {
        month_labels_.push_back(String(array[i]));
      }
    } else {
      base::ranges::copy(kFallbackMonthNames,
                         std::back_inserter(month_labels_));
    }
  }
  return month_labels_;
}

const Vector<String>& LocaleMac::WeekDayShortLabels() {
  if (week_day_short_labels_.empty()) {
    week_day_short_labels_.reserve(7);
    NSArray* array = ShortDateFormatter().veryShortWeekdaySymbols;
    if (array.count == 7) {
      for (unsigned i = 0; i < 7; ++i) {
        week_day_short_labels_.push_back(String(array[i]));
      }
    } else {
      base::ranges::copy(kFallbackWeekdayShortNames,
                         std::back_inserter(week_day_short_labels_));
    }
  }
  return week_day_short_labels_;
}

unsigned LocaleMac::FirstDayOfWeek() {
  // The document for NSCalendar - firstWeekday doesn't have an explanation of
  // firstWeekday value. We can guess it by the document of NSDateComponents -
  // weekDay, so it can be 1 through 7 and 1 is Sunday.
  return static_cast<unsigned>(gregorian_calendar_.firstWeekday - 1);
}

bool LocaleMac::IsRTL() {
  return NSLocaleLanguageDirectionRightToLeft ==
         [NSLocale characterDirectionForLanguage:
                       [NSLocale canonicalLanguageIdentifierFromString:
                                     locale_.localeIdentifier]];
}

NSDateFormatter* LocaleMac::TimeFormatter() {
  return CreateDateTimeFormatter(locale_, gregorian_calendar_,
                                 NSDateFormatterNoStyle,
                                 NSDateFormatterMediumStyle);
}

NSDateFormatter* LocaleMac::ShortTimeFormatter() {
  return CreateDateTimeFormatter(locale_, gregorian_calendar_,
                                 NSDateFormatterNoStyle,
                                 NSDateFormatterShortStyle);
}

NSDateFormatter* LocaleMac::DateTimeFormatterWithSeconds() {
  return CreateDateTimeFormatter(locale_, gregorian_calendar_,
                                 NSDateFormatterShortStyle,
                                 NSDateFormatterMediumStyle);
}

NSDateFormatter* LocaleMac::DateTimeFormatterWithoutSeconds() {
  return CreateDateTimeFormatter(locale_, gregorian_calendar_,
                                 NSDateFormatterShortStyle,
                                 NSDateFormatterShortStyle);
}

String LocaleMac::DateFormat() {
  if (!date_format_.IsNull())
    return date_format_;
  date_format_ = ShortDateFormatter().dateFormat;
  return date_format_;
}

String LocaleMac::MonthFormat() {
  if (!month_format_.IsNull())
    return month_format_;
  // Gets a format for "MMMM" because Windows API always provides formats for
  // "MMMM" in some locales.
  month_format_ = [NSDateFormatter dateFormatFromTemplate:@"yyyyMMMM"
                                                  options:0
                                                   locale:locale_];
  return month_format_;
}

String LocaleMac::ShortMonthFormat() {
  if (!short_month_format_.IsNull())
    return short_month_format_;
  short_month_format_ = [NSDateFormatter dateFormatFromTemplate:@"yyyyMMM"
                                                        options:0
                                                         locale:locale_];
  return short_month_format_;
}

String LocaleMac::TimeFormat() {
  if (!time_format_with_seconds_.IsNull())
    return time_format_with_seconds_;
  time_format_with_seconds_ = NormalizeWhitespace(TimeFormatter().dateFormat);
  return time_format_with_seconds_;
}

String LocaleMac::ShortTimeFormat() {
  if (!time_format_without_seconds_.IsNull())
    return time_format_without_seconds_;
  time_format_without_seconds_ =
      NormalizeWhitespace(ShortTimeFormatter().dateFormat);
  return time_format_without_seconds_;
}

String LocaleMac::DateTimeFormatWithSeconds() {
  if (!date_time_format_with_seconds_.IsNull())
    return date_time_format_with_seconds_;
  date_time_format_with_seconds_ =
      NormalizeWhitespace(DateTimeFormatterWithSeconds().dateFormat);
  return date_time_format_with_seconds_;
}

String LocaleMac::DateTimeFormatWithoutSeconds() {
  if (!date_time_format_without_seconds_.IsNull())
    return date_time_format_without_seconds_;
  date_time_format_without_seconds_ =
      NormalizeWhitespace(DateTimeFormatterWithoutSeconds().dateFormat);
  return date_time_format_without_seconds_;
}

const Vector<String>& LocaleMac::ShortMonthLabels() {
  if (short_month_labels_.empty()) {
    short_month_labels_.reserve(12);
    NSArray* array = ShortDateFormatter().shortMonthSymbols;
    if (array.count == 12) {
      for (unsigned i = 0; i < 12; ++i) {
        short_month_labels_.push_back(array[i]);
      }
    } else {
      base::ranges::copy(kFallbackMonthShortNames,
                         std::back_inserter(short_month_labels_));
    }
  }
  return short_month_labels_;
}

const Vector<String>& LocaleMac::StandAloneMonthLabels() {
  if (!stand_alone_month_labels_.empty())
    return stand_alone_month_labels_;
  NSArray* array = ShortDateFormatter().standaloneMonthSymbols;
  if (array.count == 12) {
    stand_alone_month_labels_.reserve(12);
    for (unsigned i = 0; i < 12; ++i)
      stand_alone_month_labels_.push_back(array[i]);
    return stand_alone_month_labels_;
  }
  stand_alone_month_labels_ = ShortMonthLabels();
  return stand_alone_month_labels_;
}

const Vector<String>& LocaleMac::ShortStandAloneMonthLabels() {
  if (!short_stand_alone_month_labels_.empty())
    return short_stand_alone_month_labels_;
  NSArray* array = ShortDateFormatter().shortStandaloneMonthSymbols;
  if (array.count == 12) {
    short_stand_alone_month_labels_.reserve(12);
    for (unsigned i = 0; i < 12; ++i)
      short_stand_alone_month_labels_.push_back(array[i]);
    return short_stand_alone_month_labels_;
  }
  short_stand_alone_month_labels_ = ShortMonthLabels();
  return short_stand_alone_month_labels_;
}

const Vector<String>& LocaleMac::TimeAMPMLabels() {
  if (!time_ampm_labels_.empty())
    return time_ampm_labels_;
  time_ampm_labels_.reserve(2);
  NSDateFormatter* formatter = ShortTimeFormatter();
  time_ampm_labels_.push_back(formatter.AMSymbol);
  time_ampm_labels_.push_back(formatter.PMSymbol);
  return time_ampm_labels_;
}

void LocaleMac::InitializeLocaleData() {
  if (did_initialize_number_data_)
    return;
  did_initialize_number_data_ = true;

  NSNumberFormatter* formatter = [[NSNumberFormatter alloc] init];
  formatter.locale = locale_;
  formatter.numberStyle = NSNumberFormatterDecimalStyle;
  formatter.usesGroupingSeparator = NO;

  NSNumber* sample_number = @9876543210;
  String nine_to_zero([formatter stringFromNumber:sample_number]);
  if (nine_to_zero.length() != 10)
    return;
  Vector<String, kDecimalSymbolsSize> symbols;
  for (unsigned i = 0; i < 10; ++i)
    symbols.push_back(nine_to_zero.Substring(9 - i, 1));
  DCHECK(symbols.size() == kDecimalSeparatorIndex);
  symbols.push_back([formatter decimalSeparator]);
  DCHECK(symbols.size() == kGroupSeparatorIndex);
  symbols.push_back([formatter groupingSeparator]);
  DCHECK(symbols.size() == kDecimalSymbolsSize);

  String positive_prefix(formatter.positivePrefix);
  String positive_suffix(formatter.positiveSuffix);
  String negative_prefix(formatter.negativePrefix);
  String negative_suffix(formatter.negativeSuffix);
  SetLocaleData(symbols, positive_prefix, positive_suffix, negative_prefix,
                negative_suffix);
}
}
