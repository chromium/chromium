// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/common/time_util.h"

#include <string>
#include <vector>

#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"

namespace google_apis {
namespace util {

namespace {

const char kNullTimeString[] = "null";

bool ParseTimezone(base::StringPiece timezone,
                   bool ahead,
                   base::TimeDelta* out_offset_to_utc) {
  DCHECK(out_offset_to_utc);

  std::vector<base::StringPiece> parts = base::SplitStringPiece(
      timezone, ":", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  int hour = 0;
  if (parts.empty() || !base::StringToInt(parts[0], &hour))
    return false;

  int minute = 0;
  if (parts.size() > 1 && !base::StringToInt(parts[1], &minute))
    return false;

  *out_offset_to_utc =
      (base::Hours(hour) + base::Minutes(minute)) * (ahead ? +1 : -1);
  return true;
}

}  // namespace

bool GetTimeFromString(base::StringPiece raw_value, base::Time* parsed_time) {
  base::StringPiece date;
  base::StringPiece time_and_tz;
  base::StringPiece time;
  base::Time::Exploded exploded = {0};
  bool has_timezone = false;
  base::TimeDelta offset_to_utc;

  // Splits the string into "date" part and "time" part.
  {
    std::vector<base::StringPiece> parts = base::SplitStringPiece(
        raw_value, "T", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (parts.size() != 2)
      return false;
    date = parts[0];
    time_and_tz = parts[1];
  }

  // Parses timezone suffix on the time part if available.
  {
    std::vector<base::StringPiece> parts;
    if (time_and_tz.back() == 'Z') {
      // Timezone is 'Z' (UTC)
      has_timezone = true;
      time = time_and_tz;
      time.remove_suffix(1);
    } else {
      bool ahead = true;
      parts = base::SplitStringPiece(time_and_tz, "+", base::KEEP_WHITESPACE,
                                     base::SPLIT_WANT_NONEMPTY);
      if (parts.size() != 2) {
        ahead = false;
        parts = base::SplitStringPiece(time_and_tz, "-", base::KEEP_WHITESPACE,
                                       base::SPLIT_WANT_NONEMPTY);
      }
      if (parts.size() == 2) {
        // Timezone is "+/-hh:mm"
        if (!ParseTimezone(parts[1], ahead, &offset_to_utc)) {
          return false;
        }
        has_timezone = true;
        time = parts[0];
      } else {
        // No timezone (uses local timezone)
        time = time_and_tz;
      }
    }
  }

  // Parses the date part.
  {
    std::vector<base::StringPiece> parts = base::SplitStringPiece(
        date, "-", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (parts.size() != 3)
      return false;

    if (!base::StringToInt(parts[0], &exploded.year) ||
        !base::StringToInt(parts[1], &exploded.month) ||
        !base::StringToInt(parts[2], &exploded.day_of_month)) {
      return false;
    }
  }

  // Parses the time part.
  {
    std::vector<base::StringPiece> parts = base::SplitStringPiece(
        time, ":", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (parts.size() != 3)
      return false;

    if (!base::StringToInt(parts[0], &exploded.hour) ||
        !base::StringToInt(parts[1], &exploded.minute)) {
      return false;
    }

    std::vector<base::StringPiece> seconds_parts = base::SplitStringPiece(
        parts[2], ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (seconds_parts.size() >= 3)
      return false;

    if (!base::StringToInt(seconds_parts[0], &exploded.second))
      return false;

    // Only accept milli-seconds (3-digits).
    if (seconds_parts.size() > 1 && seconds_parts[1].length() == 3 &&
        !base::StringToInt(seconds_parts[1], &exploded.millisecond)) {
      return false;
    }
  }

  exploded.day_of_week = 0;
  if (!exploded.HasValidValues())
    return false;

  if (has_timezone) {
    if (!base::Time::FromUTCExploded(exploded, parsed_time))
      return false;
    *parsed_time -= offset_to_utc;
  } else {
    if (!base::Time::FromLocalExploded(exploded, parsed_time))
      return false;
  }

  return true;
}

bool GetDateOnlyFromString(base::StringPiece raw_value,
                           base::Time* parsed_time) {
  base::Time::Exploded exploded = {0};
  base::Time time;

  // Parses the date part only.
  {
    std::vector<base::StringPiece> parts = base::SplitStringPiece(
        raw_value, "-", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (parts.size() != 3)
      return false;

    if (!base::StringToInt(parts[0], &exploded.year) ||
        !base::StringToInt(parts[1], &exploded.month) ||
        !base::StringToInt(parts[2], &exploded.day_of_month)) {
      return false;
    }
  }

  if (!base::Time::FromUTCExploded(exploded, &time))
    return false;

  // Ensure that the time section (everything after the "yyyy-mm-dd" date) is
  // zeros.
  *parsed_time = time.UTCMidnight();

  return true;
}

std::string FormatTimeAsString(const base::Time& time) {
  return time.is_null() ? std::string(kNullTimeString)
                        : base::TimeFormatAsIso8601(time);
}

std::string FormatTimeAsStringLocaltime(const base::Time& time) {
  if (time.is_null())
    return kNullTimeString;

  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);
  return base::StringPrintf("%04d-%02d-%02dT%02d:%02d:%02d.%03d", exploded.year,
                            exploded.month, exploded.day_of_month,
                            exploded.hour, exploded.minute, exploded.second,
                            exploded.millisecond);
}

}  // namespace util
}  // namespace google_apis
