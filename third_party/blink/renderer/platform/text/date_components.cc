/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/platform/text/date_components.h"

#include <limits.h>
#include "base/notreached.h"
#include "third_party/blink/renderer/platform/wtf/date_math.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// HTML5 specification defines minimum week of year is one.
const int DateComponents::kMinimumWeekNumber = 1;

// HTML5 specification defines maximum week of year is 53.
const int DateComponents::kMaximumWeekNumber = 53;

// This is September, since months are 0 based.
static const int kMaximumMonthInMaximumYear = 8;
static const int kMaximumDayInMaximumMonth = 13;
static const int kMaximumWeekInMaximumYear = 37;  // The week of 275760-09-13

static const int kDaysInMonth[12] = {31, 28, 31, 30, 31, 30,
                                     31, 31, 30, 31, 30, 31};

// 'month' is 0-based.
static int MaxDayOfMonth(int year, int month) {
  if (month != 1)  // February?
    return kDaysInMonth[month];
  return IsLeapYear(year) ? 29 : 28;
}

// 'month' is 0-based.
static int DayOfWeek(int year, int month, int day) {
  int shifted_month = month + 2;
  // 2:January, 3:Feburuary, 4:March, ...

  // Zeller's congruence
  if (shifted_month <= 3) {
    shifted_month += 12;
    year--;
  }
  // 4:March, ..., 14:January, 15:February

  int high_year = year / 100;
  int low_year = year % 100;
  // We add 6 to make the result Sunday-origin.
  int result = (day + 13 * shifted_month / 5 + low_year + low_year / 4 +
                high_year / 4 + 5 * high_year + 6) %
               7;
  return result;
}

int DateComponents::WeekDay() const {
  return DayOfWeek(year_, month_, month_day_);
}

int DateComponents::MaxWeekNumberInYear() const {
  int day = DayOfWeek(year_, 0, 1);  // January 1.
  return day == kThursday || (day == kWednesday && IsLeapYear(year_))
             ? kMaximumWeekNumber
             : kMaximumWeekNumber - 1;
}

static unsigned CountDigits(const String& src, unsigned start) {
  unsigned index = start;
  for (; index < src.length(); ++index) {
    if (!IsASCIIDigit(src[index]))
      break;
  }
  return index - start;
}

// Very strict integer parser. Do not allow leading or trailing whitespace
// unlike charactersToIntStrict().
static bool ToInt(const String& src,
                  unsigned parse_start,
                  unsigned parse_length,
                  int& out) {
  if (parse_start + parse_length > src.length() || !parse_length)
    return false;
  int value = 0;
  unsigned current = parse_start;
  unsigned end = current + parse_length;

  // We don't need to handle negative numbers for ISO 8601.
  for (; current < end; ++current) {
    if (!IsASCIIDigit(src[current]))
      return false;
    int digit = src[current] - '0';
    if (value > (INT_MAX - digit) / 10)  // Check for overflow.
      return false;
    value = value * 10 + digit;
  }
  out = value;
  return true;
}

bool DateComponents::ParseYear(const String& src,
                               unsigned start,
                               unsigned& end) {
  unsigned digits_length = CountDigits(src, start);
  // Needs at least 4 digits according to the standard.
  if (digits_length < 4)
    return false;
  int year;
  if (!ToInt(src, start, digits_length, year))
    return false;
  if (year < MinimumYear() || year > MaximumYear())
    return false;
  year_ = year;
  end = start + digits_length;
  return true;
}

static bool WithinHTMLDateLimits(int year, int month) {
  if (year < DateComponents::MinimumYear())
    return false;
  if (year < DateComponents::MaximumYear())
    return true;
  return month <= kMaximumMonthInMaximumYear;
}

static bool WithinHTMLDateLimits(int year, int month, int month_day) {
  if (year < DateComponents::MinimumYear())
    return false;
  if (year < DateComponents::MaximumYear())
    return true;
  if (month < kMaximumMonthInMaximumYear)
    return true;
  return month_day <= kMaximumDayInMaximumMonth;
}

static bool WithinHTMLDateLimits(int year,
                                 int month,
                                 int month_day,
                                 int hour,
                                 int minute,
                                 int second,
                                 int millisecond) {
  if (year < DateComponents::MinimumYear())
    return false;
  if (year < DateComponents::MaximumYear())
    return true;
  if (month < kMaximumMonthInMaximumYear)
    return true;
  if (month_day < kMaximumDayInMaximumMonth)
    return true;
  if (month_day > kMaximumDayInMaximumMonth)
    return false;
  // (year, month, monthDay) =
  // (MaximumYear, kMaximumMonthInMaximumYear, kMaximumDayInMaximumMonth)
  return !hour && !minute && !second && !millisecond;
}

bool DateComponents::ParseMonth(const String& src,
                                unsigned start,
                                unsigned& end) {
  unsigned index;
  if (!ParseYear(src, start, index))
    return false;
  if (index >= src.length() || src[index] != '-')
    return false;
  ++index;

  int month;
  if (!ToInt(src, index, 2, month) || month < 1 || month > 12)
    return false;
  --month;
  if (!WithinHTMLDateLimits(year_, month))
    return false;
  month_ = month;
  end = index + 2;
  type_ = kMonth;
  return true;
}

bool DateComponents::ParseDate(const String& src,
                               unsigned start,
                               unsigned& end) {
  unsigned index;
  if (!ParseMonth(src, start, index))
    return false;
  // '-' and 2-digits are needed.
  if (index + 2 >= src.length())
    return false;
  if (src[index] != '-')
    return false;
  ++index;

  int day;
  if (!ToInt(src, index, 2, day) || day < 1 ||
      day > MaxDayOfMonth(year_, month_))
    return false;
  if (!WithinHTMLDateLimits(year_, month_, day))
    return false;
  month_day_ = day;
  end = index + 2;
  type_ = kDate;
  return true;
}

bool DateComponents::ParseWeek(const String& src,
                               unsigned start,
                               unsigned& end) {
  unsigned index;
  if (!ParseYear(src, start, index))
    return false;

  // 4 characters ('-' 'W' digit digit) are needed.
  if (index + 3 >= src.length())
    return false;
  if (src[index] != '-')
    return false;
  ++index;
  if (src[index] != 'W')
    return false;
  ++index;

  int week;
  if (!ToInt(src, index, 2, week) || week < kMinimumWeekNumber ||
      week > MaxWeekNumberInYear())
    return false;
  if (year_ == MaximumYear() && week > kMaximumWeekInMaximumYear)
    return false;
  week_ = week;
  end = index + 2;
  type_ = kWeek;
  return true;
}

bool DateComponents::ParseTime(const String& src,
                               unsigned start,
                               unsigned& end) {
  int hour;
  if (!ToInt(src, start, 2, hour) || hour < 0 || hour > 23)
    return false;
  unsigned index = start + 2;
  if (index >= src.length())
    return false;
  if (src[index] != ':')
    return false;
  ++index;

  int minute;
  if (!ToInt(src, index, 2, minute) || minute < 0 || minute > 59)
    return false;
  index += 2;

  int second = 0;
  int millisecond = 0;
  // Optional second part.
  // Do not return with false because the part is optional.
  if (index + 2 < src.length() && src[index] == ':') {
    if (ToInt(src, index + 1, 2, second) && second >= 0 && second <= 59) {
      index += 3;

      // Optional fractional second part.
      if (index < src.length() && src[index] == '.') {
        unsigned digits_length = CountDigits(src, index + 1);
        if (digits_length > 0) {
          ++index;
          bool ok;
          if (digits_length == 1) {
            ok = ToInt(src, index, 1, millisecond);
            millisecond *= 100;
          } else if (digits_length == 2) {
            ok = ToInt(src, index, 2, millisecond);
            millisecond *= 10;
          } else if (digits_length == 3) {
            ok = ToInt(src, index, 3, millisecond);
          } else {  // digits_length >= 4
            return false;
          }
          DCHECK(ok);
          index += digits_length;
        }
      }
    }
  }
  hour_ = hour;
  minute_ = minute;
  second_ = second;
  millisecond_ = millisecond;
  end = index;
  type_ = kTime;
  return true;
}

bool DateComponents::ParseDateTimeLocal(const String& src,
                                        unsigned start,
                                        unsigned& end) {
  unsigned index;
  if (!ParseDate(src, start, index))
    return false;
  if (index >= src.length())
    return false;
  if (src[index] != 'T' && src[index] != ' ')
    return false;
  ++index;
  if (!ParseTime(src, index, end))
    return false;
  if (!WithinHTMLDateLimits(year_, month_, month_day_, hour_, minute_, second_,
                            millisecond_))
    return false;
  type_ = kDateTimeLocal;
  return true;
}

static inline double PositiveFmod(double value, double divider) {
  double remainder = fmod(value, divider);
  return remainder < 0 ? remainder + divider : remainder;
}

void DateComponents::SetMillisecondsSinceMidnightInternal(double ms_in_day) {
  DCHECK_GE(ms_in_day, 0);
  DCHECK_LT(ms_in_day, kMsPerDay);
  millisecond_ = static_cast<int>(fmod(ms_in_day, kMsPerSecond));
  double value = std::floor(ms_in_day / kMsPerSecond);
  second_ = static_cast<int>(fmod(value, kSecondsPerMinute));
  value = std::floor(value / kSecondsPerMinute);
  minute_ = static_cast<int>(fmod(value, kMinutesPerHour));
  hour_ = static_cast<int>(value / kMinutesPerHour);
}

bool DateComponents::SetMillisecondsSinceEpochForDateInternal(double ms) {
  year_ = MsToYear(ms);
  int year_day = DayInYear(ms, year_);
  month_ = MonthFromDayInYear(year_day, IsLeapYear(year_));
  month_day_ = DayInMonthFromDayInYear(year_day, IsLeapYear(year_));
  return true;
}

bool DateComponents::SetMillisecondsSinceEpochForDate(double ms) {
  type_ = kInvalid;
  if (!std::isfinite(ms))
    return false;
  if (!SetMillisecondsSinceEpochForDateInternal(round(ms)))
    return false;
  if (!WithinHTMLDateLimits(year_, month_, month_day_))
    return false;
  type_ = kDate;
  return true;
}

bool DateComponents::SetMillisecondsSinceEpochForDateTimeLocal(double ms) {
  type_ = kInvalid;
  if (!std::isfinite(ms))
    return false;
  ms = round(ms);
  SetMillisecondsSinceMidnightInternal(PositiveFmod(ms, kMsPerDay));
  if (!SetMillisecondsSinceEpochForDateInternal(ms))
    return false;
  if (!WithinHTMLDateLimits(year_, month_, month_day_, hour_, minute_, second_,
                            millisecond_))
    return false;
  type_ = kDateTimeLocal;
  return true;
}

bool DateComponents::SetMillisecondsSinceEpochForMonth(double ms) {
  type_ = kInvalid;
  if (!std::isfinite(ms))
    return false;
  if (!SetMillisecondsSinceEpochForDateInternal(round(ms)))
    return false;
  if (!WithinHTMLDateLimits(year_, month_))
    return false;
  type_ = kMonth;
  return true;
}

bool DateComponents::SetMillisecondsSinceMidnight(double ms) {
  type_ = kInvalid;
  if (!std::isfinite(ms))
    return false;
  SetMillisecondsSinceMidnightInternal(PositiveFmod(round(ms), kMsPerDay));
  type_ = kTime;
  return true;
}

bool DateComponents::SetMonthsSinceEpoch(double months) {
  if (!std::isfinite(months))
    return false;
  months = round(months);
  double double_month = PositiveFmod(months, 12);
  double double_year = 1970 + (months - double_month) / 12;
  if (double_year < MinimumYear() || MaximumYear() < double_year)
    return false;
  int year = static_cast<int>(double_year);
  int month = static_cast<int>(double_month);
  if (!WithinHTMLDateLimits(year, month))
    return false;
  year_ = year;
  month_ = month;
  type_ = kMonth;
  return true;
}

// Offset from January 1st to Monday of the ISO 8601's first week.
//   ex. If January 1st is Friday, such Monday is 3 days later. Returns 3.
static int OffsetTo1stWeekStart(int year) {
  int offset_to1st_week_start = 1 - DayOfWeek(year, 0, 1);
  if (offset_to1st_week_start <= -4)
    offset_to1st_week_start += 7;
  return offset_to1st_week_start;
}

bool DateComponents::SetMillisecondsSinceEpochForWeek(double ms) {
  type_ = kInvalid;
  if (!std::isfinite(ms))
    return false;
  ms = round(ms);

  year_ = MsToYear(ms);
  if (year_ < MinimumYear() || year_ > MaximumYear())
    return false;

  int year_day = DayInYear(ms, year_);
  int offset = OffsetTo1stWeekStart(year_);
  if (year_day < offset) {
    // The day belongs to the last week of the previous year.
    year_--;
    if (year_ <= MinimumYear())
      return false;
    week_ = MaxWeekNumberInYear();
  } else {
    week_ = ((year_day - offset) / 7) + 1;
    if (week_ > MaxWeekNumberInYear()) {
      year_++;
      week_ = 1;
    }
    if (year_ > MaximumYear() ||
        (year_ == MaximumYear() && week_ > kMaximumWeekInMaximumYear))
      return false;
  }
  type_ = kWeek;
  return true;
}

bool DateComponents::SetWeek(int year, int week_number) {
  type_ = kInvalid;
  if (year < MinimumYear() || year > MaximumYear())
    return false;
  year_ = year;
  if (week_number < 1 || week_number > MaxWeekNumberInYear())
    return false;
  week_ = week_number;
  type_ = kWeek;
  return true;
}

double DateComponents::MillisecondsSinceEpochForTime() const {
  DCHECK(type_ == kTime || type_ == kDateTimeLocal);
  return ((hour_ * kMinutesPerHour + minute_) * kSecondsPerMinute + second_) *
             kMsPerSecond +
         millisecond_;
}

double DateComponents::MillisecondsSinceEpoch() const {
  switch (type_) {
    case kDate:
      return DateToDaysFrom1970(year_, month_, month_day_) * kMsPerDay;
    case kDateTimeLocal:
      return DateToDaysFrom1970(year_, month_, month_day_) * kMsPerDay +
             MillisecondsSinceEpochForTime();
    case kMonth:
      return DateToDaysFrom1970(year_, month_, 1) * kMsPerDay;
    case kTime:
      return MillisecondsSinceEpochForTime();
    case kWeek:
      return (DateToDaysFrom1970(year_, 0, 1) + OffsetTo1stWeekStart(year_) +
              (week_ - 1) * 7) *
             kMsPerDay;
    case kInvalid:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return InvalidMilliseconds();
}

double DateComponents::MonthsSinceEpoch() const {
  DCHECK_EQ(type_, kMonth);
  return (year_ - 1970) * 12 + month_;
}

String DateComponents::ToStringForTime(SecondFormat format) const {
  DCHECK(type_ == kDateTimeLocal || type_ == kTime);
  SecondFormat effective_format = format;
  if (millisecond_)
    effective_format = SecondFormat::kMillisecond;
  else if (format == SecondFormat::kNone && second_)
    effective_format = SecondFormat::kSecond;

  switch (effective_format) {
    default:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case SecondFormat::kNone:
      return String::Format("%02d:%02d", hour_, minute_);
    case SecondFormat::kSecond:
      return String::Format("%02d:%02d:%02d", hour_, minute_, second_);
    case SecondFormat::kMillisecond:
      return String::Format("%02d:%02d:%02d.%03d", hour_, minute_, second_,
                            millisecond_);
  }
}

String DateComponents::ToString(SecondFormat format) const {
  switch (type_) {
    case kDate:
      return String::Format("%04d-%02d-%02d", year_, month_ + 1, month_day_);
    case kDateTimeLocal:
      return String::Format("%04d-%02d-%02dT", year_, month_ + 1, month_day_) +
             ToStringForTime(format);
    case kMonth:
      return String::Format("%04d-%02d", year_, month_ + 1);
    case kTime:
      return ToStringForTime(format);
    case kWeek:
      return String::Format("%04d-W%02d", year_, week_);
    case kInvalid:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return String("(Invalid DateComponents)");
}

}  // namespace blink
