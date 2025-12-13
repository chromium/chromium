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

#include "third_party/blink/renderer/platform/wtf/date_math.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <unicode/basictz.h>
#include <unicode/timezone.h>

#include <algorithm>
#include <array>
#include <limits>
#include <memory>

#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

/* Constants */

static const double kMinimumECMADateInMs = -8640000000000000.0;
static const double kMaximumECMADateInMs = 8640000000000000.0;

// Day of year for the first day of each month, where index 0 is January, and
// day 0 is January 1.  First for non-leap years, then for leap years.
static const std::array<std::array<int, 12>, 2> kFirstDayOfMonth = {
    {{0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334},
     {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335}}};

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
  return base::Milliseconds(ms).InDaysFloored();
}

int MsToYear(double ms) {
  DCHECK(std::isfinite(ms));
  DCHECK_GE(ms, kMinimumECMADateInMs);
  DCHECK_LE(ms, kMaximumECMADateInMs);
  static constexpr double kMsPerDay = base::Time::kMillisecondsPerDay;
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

base::TimeDelta ConvertToLocalTime(base::Time time) {
  double ms = time.InMillisecondsFSinceUnixEpoch();
  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createDefault());
  int32_t raw_offset, dst_offset;
  UErrorCode status = U_ZERO_ERROR;
  timezone->getOffset(ms, false, raw_offset, dst_offset, status);
  DCHECK(U_SUCCESS(status));
  return base::Milliseconds(ms + static_cast<double>(raw_offset + dst_offset));
}

}  // namespace blink
