/*
 * Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
 * Copyright (C) 2010 &yet, LLC. (nate@andyet.net)
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.

 * Copyright 2006-2008 the V8 project authors. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Google Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
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

#include "third_party/blink/renderer/platform/wtf/date_math.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <unicode/basictz.h>
#include <unicode/timezone.h>

#include <algorithm>
#include <limits>
#include <memory>

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#else
#include <sys/time.h>
#endif

namespace WTF {

/* Constants */

static const double kHoursPerDay = 24.0;

static const double kMinimumECMADateInMs = -8640000000000000.0;
static const double kMaximumECMADateInMs = 8640000000000000.0;

// Day of year for the first day of each month, where index 0 is January, and
// day 0 is January 1.  First for non-leap years, then for leap years.
static const int kFirstDayOfMonth[2][12] = {
    {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334},
    {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335}};

bool IsLeapYear(int year) {
  if (year % 4 != 0)
    return false;
  if (year % 400 == 0)
    return true;
  if (year % 100 == 0)
    return false;
  return true;
}

static inline int DaysInYear(int year) {
  return 365 + IsLeapYear(year);
}

static inline double DaysFrom1970ToYear(int year) {
  // The Gregorian Calendar rules for leap years:
  // Every fourth year is a leap year.  2004, 2008, and 2012 are leap years.
  // However, every hundredth year is not a leap year.  1900 and 2100 are not
  // leap years.
  // Every four hundred years, there's a leap year after all.  2000 and 2400 are
  // leap years.

  static const int kLeapDaysBefore1971By4Rule = 1970 / 4;
  static const int kExcludedLeapDaysBefore1971By100Rule = 1970 / 100;
  static const int kLeapDaysBefore1971By400Rule = 1970 / 400;

  const double year_minus_one = year - 1;
  const double years_to_add_by4_rule =
      floor(year_minus_one / 4.0) - kLeapDaysBefore1971By4Rule;
  const double years_to_exclude_by100_rule =
      floor(year_minus_one / 100.0) - kExcludedLeapDaysBefore1971By100Rule;
  const double years_to_add_by400_rule =
      floor(year_minus_one / 400.0) - kLeapDaysBefore1971By400Rule;

  return 365.0 * (year - 1970) + years_to_add_by4_rule -
         years_to_exclude_by100_rule + years_to_add_by400_rule;
}

static double MsToDays(double ms) {
  return floor(ms / kMsPerDay);
}

int MsToYear(double ms) {
  DCHECK(std::isfinite(ms));
  DCHECK_GE(ms, kMinimumECMADateInMs);
  DCHECK_LE(ms, kMaximumECMADateInMs);
  int approx_year = static_cast<int>(floor(ms / (kMsPerDay * 365.2425)) + 1970);
  double ms_from_approx_year_to1970 =
      kMsPerDay * DaysFrom1970ToYear(approx_year);
  if (ms_from_approx_year_to1970 > ms)
    return approx_year - 1;
  if (ms_from_approx_year_to1970 + kMsPerDay * DaysInYear(approx_year) <= ms)
    return approx_year + 1;
  return approx_year;
}

int DayInYear(double ms, int year) {
  return static_cast<int>(MsToDays(ms) - DaysFrom1970ToYear(year));
}

int MonthFromDayInYear(int day_in_year, bool leap_year) {
  const int d = day_in_year;
  int step;

  if (d < (step = 31))
    return 0;
  step += (leap_year ? 29 : 28);
  if (d < step)
    return 1;
  if (d < (step += 31))
    return 2;
  if (d < (step += 30))
    return 3;
  if (d < (step += 31))
    return 4;
  if (d < (step += 30))
    return 5;
  if (d < (step += 31))
    return 6;
  if (d < (step += 31))
    return 7;
  if (d < (step += 30))
    return 8;
  if (d < (step += 31))
    return 9;
  if (d < (step += 30))
    return 10;
  return 11;
}

static inline bool CheckMonth(int day_in_year,
                              int& start_day_of_this_month,
                              int& start_day_of_next_month,
                              int days_in_this_month) {
  start_day_of_this_month = start_day_of_next_month;
  start_day_of_next_month += days_in_this_month;
  return (day_in_year <= start_day_of_next_month);
}

int DayInMonthFromDayInYear(int day_in_year, bool leap_year) {
  const int d = day_in_year;
  int step;
  int next = 30;

  if (d <= next)
    return d + 1;
  const int days_in_feb = (leap_year ? 29 : 28);
  if (CheckMonth(d, step, next, days_in_feb))
    return d - step;
  if (CheckMonth(d, step, next, 31))
    return d - step;
  if (CheckMonth(d, step, next, 30))
    return d - step;
  if (CheckMonth(d, step, next, 31))
    return d - step;
  if (CheckMonth(d, step, next, 30))
    return d - step;
  if (CheckMonth(d, step, next, 31))
    return d - step;
  if (CheckMonth(d, step, next, 31))
    return d - step;
  if (CheckMonth(d, step, next, 30))
    return d - step;
  if (CheckMonth(d, step, next, 31))
    return d - step;
  if (CheckMonth(d, step, next, 30))
    return d - step;
  step = next;
  return d - step;
}

int DayInYear(int year, int month, int day) {
  return kFirstDayOfMonth[IsLeapYear(year)][month] + day - 1;
}

double DateToDaysFrom1970(int year, int month, int day) {
  year += month / 12;

  month %= 12;
  if (month < 0) {
    month += 12;
    --year;
  }

  double yearday = floor(DaysFrom1970ToYear(year));
  DCHECK((year >= 1970 && yearday >= 0) || (year < 1970 && yearday < 0));
  return yearday + DayInYear(year, month, day);
}

static inline double YmdhmsToSeconds(int year,
                                     int64_t mon,
                                     int64_t day,
                                     int64_t hour,
                                     int64_t minute,
                                     double second) {
  double days =
      (day - 32075) + floor(1461 * (year + 4800.0 + (mon - 14) / 12) / 4) +
      367 * (mon - 2 - (mon - 14) / 12 * 12) / 12 -
      floor(3 * ((year + 4900.0 + (mon - 14) / 12) / 100) / 4) - 2440588;
  return ((days * kHoursPerDay + hour) * kMinutesPerHour + minute) *
             kSecondsPerMinute +
         second;
}

// We follow the recommendation of RFC 2822 to consider all
// obsolete time zones not listed here equivalent to "-0000".
static const struct KnownZone {
#if !BUILDFLAG(IS_WIN)
  const
#endif
      char tz_name[4];
  int tz_offset;
} known_zones[] = {{"UT", 0},     {"GMT", 0},    {"EST", -300}, {"EDT", -240},
                   {"CST", -360}, {"CDT", -300}, {"MST", -420}, {"MDT", -360},
                   {"PST", -480}, {"PDT", -420}};

inline static void SkipSpacesAndComments(const char*& s) {
  int nesting = 0;
  char ch;
  while ((ch = *s)) {
    if (!IsASCIISpace(ch)) {
      if (ch == '(')
        nesting++;
      else if (ch == ')' && nesting > 0)
        nesting--;
      else if (nesting == 0)
        break;
    }
    s++;
  }
}

// returns 0-11 (Jan-Dec); -1 on failure
static int FindMonth(const char* month_str) {
  DCHECK(month_str);
  char needle[4];
  for (int i = 0; i < 3; ++i) {
    if (!*month_str)
      return -1;
    needle[i] = static_cast<char>(ToASCIILower(*month_str++));
  }
  needle[3] = '\0';
  const char* haystack = "janfebmaraprmayjunjulaugsepoctnovdec";
  const char* str = strstr(haystack, needle);
  if (str) {
    int position = static_cast<int>(str - haystack);
    if (position % 3 == 0)
      return position / 3;
  }
  return -1;
}

static bool ParseInt(const char* string,
                     char** stop_position,
                     int base,
                     int* result) {
  int64_t int64_result = strtol(string, stop_position, base);
  // Avoid the use of errno as it is not available on Windows CE
  if (string == *stop_position ||
      int64_result <= std::numeric_limits<int>::min() ||
      int64_result >= std::numeric_limits<int>::max())
    return false;
  *result = static_cast<int>(int64_result);
  return true;
}

static bool ParseInt64(const char* string,
                       char** stop_position,
                       int base,
                       int64_t* result) {
  int64_t int64_result = strtoll(string, stop_position, base);
  // Avoid the use of errno as it is not available on Windows CE
  if (string == *stop_position ||
      int64_result == std::numeric_limits<int64_t>::min() ||
      int64_result == std::numeric_limits<int64_t>::max())
    return false;
  *result = int64_result;
  return true;
}

// Odd case where 'exec' is allowed to be 0, to accommodate a caller in WebCore.
static double ParseDateFromNullTerminatedCharacters(const char* date_string,
                                                    bool& have_tz,
                                                    int& offset) {
  have_tz = false;
  offset = 0;

  // This parses a date in the form:
  //     Tuesday, 09-Nov-99 23:12:40 GMT
  // or
  //     Sat, 01-Jan-2000 08:00:00 GMT
  // or
  //     Sat, 01 Jan 2000 08:00:00 GMT
  // or
  //     01 Jan 99 22:00 +0100    (exceptions in rfc822/rfc2822)
  // ### non RFC formats, added for Javascript:
  //     [Wednesday] January 09 1999 23:12:40 GMT
  //     [Wednesday] January 09 23:12:40 GMT 1999
  //
  // We ignore the weekday.

  // Skip leading space
  SkipSpacesAndComments(date_string);

  int64_t month = -1;
  const char* word_start = date_string;
  // Check contents of first words if not number
  while (*date_string && !IsASCIIDigit(*date_string)) {
    if (IsASCIISpace(*date_string) || *date_string == '(') {
      if (date_string - word_start >= 3)
        month = FindMonth(word_start);
      SkipSpacesAndComments(date_string);
      word_start = date_string;
    } else {
      date_string++;
    }
  }

  // Missing delimiter between month and day (like "January29")?
  if (month == -1 && word_start != date_string)
    month = FindMonth(word_start);

  SkipSpacesAndComments(date_string);

  if (!*date_string)
    return std::numeric_limits<double>::quiet_NaN();

  // ' 09-Nov-99 23:12:40 GMT'
  char* new_pos_str;
  int64_t day;
  if (!ParseInt64(date_string, &new_pos_str, 10, &day))
    return std::numeric_limits<double>::quiet_NaN();
  date_string = new_pos_str;

  if (!*date_string)
    return std::numeric_limits<double>::quiet_NaN();

  if (day < 0)
    return std::numeric_limits<double>::quiet_NaN();

  int year = 0;
  if (day > 31) {
    // ### where is the boundary and what happens below?
    if (*date_string != '/')
      return std::numeric_limits<double>::quiet_NaN();
    // looks like a YYYY/MM/DD date
    if (!*++date_string)
      return std::numeric_limits<double>::quiet_NaN();
    if (day <= std::numeric_limits<int>::min() ||
        day >= std::numeric_limits<int>::max())
      return std::numeric_limits<double>::quiet_NaN();
    year = static_cast<int>(day);
    if (!ParseInt64(date_string, &new_pos_str, 10, &month))
      return std::numeric_limits<double>::quiet_NaN();
    --month;
    date_string = new_pos_str;
    if (*date_string++ != '/' || !*date_string)
      return std::numeric_limits<double>::quiet_NaN();
    if (!ParseInt64(date_string, &new_pos_str, 10, &day))
      return std::numeric_limits<double>::quiet_NaN();
    date_string = new_pos_str;
  } else if (*date_string == '/' && month == -1) {
    date_string++;
    // This looks like a MM/DD/YYYY date, not an RFC date.
    month = day - 1;  // 0-based
    if (!ParseInt64(date_string, &new_pos_str, 10, &day))
      return std::numeric_limits<double>::quiet_NaN();
    if (day < 1 || day > 31)
      return std::numeric_limits<double>::quiet_NaN();
    date_string = new_pos_str;
    if (*date_string == '/')
      date_string++;
    if (!*date_string)
      return std::numeric_limits<double>::quiet_NaN();
  } else {
    if (*date_string == '-')
      date_string++;

    SkipSpacesAndComments(date_string);

    if (*date_string == ',')
      date_string++;

    if (month == -1) {  // not found yet
      month = FindMonth(date_string);
      if (month == -1)
        return std::numeric_limits<double>::quiet_NaN();

      while (*date_string && *date_string != '-' && *date_string != ',' &&
             !IsASCIISpace(*date_string))
        date_string++;

      if (!*date_string)
        return std::numeric_limits<double>::quiet_NaN();

      // '-99 23:12:40 GMT'
      if (*date_string != '-' && *date_string != '/' && *date_string != ',' &&
          !IsASCIISpace(*date_string))
        return std::numeric_limits<double>::quiet_NaN();
      date_string++;
    }
  }

  if (month < 0 || month > 11)
    return std::numeric_limits<double>::quiet_NaN();

  // '99 23:12:40 GMT'
  if (year <= 0 && *date_string) {
    if (!ParseInt(date_string, &new_pos_str, 10, &year))
      return std::numeric_limits<double>::quiet_NaN();
  }

  // Don't fail if the time is missing.
  int64_t hour = 0;
  int64_t minute = 0;
  int64_t second = 0;
  if (!*new_pos_str) {
    date_string = new_pos_str;
  } else {
    // ' 23:12:40 GMT'
    if (!(IsASCIISpace(*new_pos_str) || *new_pos_str == ',')) {
      if (*new_pos_str != ':')
        return std::numeric_limits<double>::quiet_NaN();
      // There was no year; the number was the hour.
      year = -1;
    } else {
      // in the normal case (we parsed the year), advance to the next number
      date_string = ++new_pos_str;
      SkipSpacesAndComments(date_string);
    }

    ParseInt64(date_string, &new_pos_str, 10, &hour);
    // Do not check for errno here since we want to continue
    // even if errno was set because we are still looking
    // for the timezone!

    // Read a number? If not, this might be a timezone name.
    if (new_pos_str != date_string) {
      date_string = new_pos_str;

      if (hour < 0 || hour > 23)
        return std::numeric_limits<double>::quiet_NaN();

      if (!*date_string)
        return std::numeric_limits<double>::quiet_NaN();

      // ':12:40 GMT'
      if (*date_string++ != ':')
        return std::numeric_limits<double>::quiet_NaN();

      if (!ParseInt64(date_string, &new_pos_str, 10, &minute))
        return std::numeric_limits<double>::quiet_NaN();
      date_string = new_pos_str;

      if (minute < 0 || minute > 59)
        return std::numeric_limits<double>::quiet_NaN();

      // ':40 GMT'
      if (*date_string && *date_string != ':' && !IsASCIISpace(*date_string))
        return std::numeric_limits<double>::quiet_NaN();

      // seconds are optional in rfc822 + rfc2822
      if (*date_string == ':') {
        date_string++;

        if (!ParseInt64(date_string, &new_pos_str, 10, &second))
          return std::numeric_limits<double>::quiet_NaN();
        date_string = new_pos_str;

        if (second < 0 || second > 59)
          return std::numeric_limits<double>::quiet_NaN();
      }

      SkipSpacesAndComments(date_string);

      String date_wtf_string(date_string);
      if (date_wtf_string.StartsWithIgnoringASCIICase("AM")) {
        if (hour > 12)
          return std::numeric_limits<double>::quiet_NaN();
        if (hour == 12)
          hour = 0;
        date_string += 2;
        SkipSpacesAndComments(date_string);
      } else if (date_wtf_string.StartsWithIgnoringASCIICase("PM")) {
        if (hour > 12)
          return std::numeric_limits<double>::quiet_NaN();
        if (hour != 12)
          hour += 12;
        date_string += 2;
        SkipSpacesAndComments(date_string);
      }
    }
  }

  // The year may be after the time but before the time zone.
  if (IsASCIIDigit(*date_string) && year == -1) {
    if (!ParseInt(date_string, &new_pos_str, 10, &year))
      return std::numeric_limits<double>::quiet_NaN();
    date_string = new_pos_str;
    SkipSpacesAndComments(date_string);
  }

  // Don't fail if the time zone is missing.
  // Some websites omit the time zone (4275206).
  if (*date_string) {
    String date_wtf_string(date_string);
    if (date_wtf_string.StartsWithIgnoringASCIICase("GMT") ||
        date_wtf_string.StartsWithIgnoringASCIICase("UTC")) {
      date_string += 3;
      have_tz = true;
    }

    if (*date_string == '+' || *date_string == '-') {
      int o;
      if (!ParseInt(date_string, &new_pos_str, 10, &o))
        return std::numeric_limits<double>::quiet_NaN();
      date_string = new_pos_str;

      if (o < -9959 || o > 9959)
        return std::numeric_limits<double>::quiet_NaN();

      int sgn = (o < 0) ? -1 : 1;
      o = abs(o);
      if (*date_string != ':') {
        if (o >= 24)
          offset = ((o / 100) * 60 + (o % 100)) * sgn;
        else
          offset = o * 60 * sgn;
      } else {         // GMT+05:00
        ++date_string;  // skip the ':'
        int o2;
        if (!ParseInt(date_string, &new_pos_str, 10, &o2))
          return std::numeric_limits<double>::quiet_NaN();
        date_string = new_pos_str;
        offset = (o * 60 + o2) * sgn;
      }
      have_tz = true;
    } else {
      date_wtf_string = String(date_string);
      for (size_t i = 0; i < std::size(known_zones); ++i) {
        if (date_wtf_string.StartsWithIgnoringASCIICase(
                known_zones[i].tz_name)) {
          offset = known_zones[i].tz_offset;
          date_string += strlen(known_zones[i].tz_name);
          have_tz = true;
          break;
        }
      }
    }
  }

  SkipSpacesAndComments(date_string);

  if (*date_string && year == -1) {
    if (!ParseInt(date_string, &new_pos_str, 10, &year))
      return std::numeric_limits<double>::quiet_NaN();
    date_string = new_pos_str;
    SkipSpacesAndComments(date_string);
  }

  // Trailing garbage
  if (*date_string)
    return std::numeric_limits<double>::quiet_NaN();

  // Y2K: Handle 2 digit years.
  if (year >= 0 && year < 100) {
    if (year < 50)
      year += 2000;
    else
      year += 1900;
  }

  return YmdhmsToSeconds(year, month + 1, day, hour, minute, second) *
         kMsPerSecond;
}

std::optional<base::Time> ParseDateFromNullTerminatedCharacters(
    const char* date_string) {
  bool have_tz;
  int offset;
  double ms =
      ParseDateFromNullTerminatedCharacters(date_string, have_tz, offset);
  if (std::isnan(ms))
    return std::nullopt;

  // fall back to local timezone
  if (!have_tz) {
    std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createDefault());
    int32_t raw_offset, dst_offset;
    UErrorCode status = U_ZERO_ERROR;
    // Handle the conversion of localtime to UTC the same way as the
    // latest ECMA 262 spec for Javascript (v8 does that, too).
    static_cast<const icu::BasicTimeZone*>(timezone.get())
        ->getOffsetFromLocal(ms, UCAL_TZ_LOCAL_FORMER, UCAL_TZ_LOCAL_FORMER,
                             raw_offset, dst_offset, status);
    DCHECK(U_SUCCESS(status));
    offset = static_cast<int>((raw_offset + dst_offset) / kMsPerMinute);
  }
  return base::Time::FromMillisecondsSinceUnixEpoch(ms -
                                                    (offset * kMsPerMinute));
}

base::TimeDelta ConvertToLocalTime(base::Time time) {
  double ms = time.InMillisecondsFSinceUnixEpoch();
  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createDefault());
  int32_t raw_offset, dst_offset;
  UErrorCode status = U_ZERO_ERROR;
  timezone->getOffset(ms, false, raw_offset, dst_offset, status);
  DCHECK(U_SUCCESS(status));
  return base::Milliseconds(ms + static_cast<double>(raw_offset + dst_offset));
}

}  // namespace WTF
