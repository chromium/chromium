// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/common/time_util.h"

#include <string>
#include <string_view>
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

constexpr char kNullTimeString[] = "null";

// Sugar for calling the base:: method with the handling we always desire.
std::vector<std::string_view> SplitStringPiece(std::string_view input,
                                               std::string_view separators) {
  return base::SplitStringPiece(input, separators, base::KEEP_WHITESPACE,
                                base::SPLIT_WANT_NONEMPTY);
}

}  // namespace

bool GetTimeFromString(std::string_view raw_value, base::Time* parsed_time) {
  // Split the input into "date" and "time" parts.
  std::string_view date;
  std::string_view time;
  {
    std::vector<std::string_view> parts = SplitStringPiece(raw_value, "T");
    if (parts.size() != 2) {
      return false;
    }
    date = parts[0];
    time = parts[1];
  }

  // Parse timezone suffix, if available.
  bool has_timezone = false;
  base::TimeDelta timezone_offset;
  {
    if (time.back() == 'Z') {
      // Timezone is 'Z' (UTC).
      has_timezone = true;
      time.remove_suffix(1);
    } else {
      bool ahead = true;
      std::vector<std::string_view> parts = SplitStringPiece(time, "+");
      if (parts.size() != 2) {
        ahead = false;
        parts = SplitStringPiece(time, "-");
      }
      if (parts.size() == 2) {
        // Timezone is "+/-hh:mm".
        std::vector<std::string_view> tz_parts =
            SplitStringPiece(parts[1], ":");
        int hour = 0, minute = 0;
        if (tz_parts.empty() || !base::StringToInt(tz_parts[0], &hour) ||
            (tz_parts.size() > 1 && !base::StringToInt(tz_parts[1], &minute))) {
          return false;
        }
        has_timezone = true;
        time = parts[0];
        timezone_offset =
            (base::Hours(hour) + base::Minutes(minute)) * (ahead ? +1 : -1);
      }
    }
  }

  // Parse the date.
  base::Time::Exploded exploded = {0};
  {
    std::vector<std::string_view> parts = SplitStringPiece(date, "-");
    if (parts.size() != 3 || !base::StringToInt(parts[0], &exploded.year) ||
        !base::StringToInt(parts[1], &exploded.month) ||
        !base::StringToInt(parts[2], &exploded.day_of_month)) {
      return false;
    }
  }

  // Parse the time.
  {
    std::vector<std::string_view> parts = SplitStringPiece(time, ":");
    if (parts.size() != 3 || !base::StringToInt(parts[0], &exploded.hour) ||
        !base::StringToInt(parts[1], &exploded.minute)) {
      return false;
    }

    std::vector<std::string_view> seconds_parts =
        SplitStringPiece(parts[2], ".");
    if (seconds_parts.size() >= 3 ||
        !base::StringToInt(seconds_parts[0], &exploded.second) ||
        // Only accept milliseconds (3 digits).
        (seconds_parts.size() > 1 && seconds_parts[1].length() == 3 &&
         !base::StringToInt(seconds_parts[1], &exploded.millisecond))) {
      return false;
    }
  }

  // Convert back to a `base::Time`.
  if (!exploded.HasValidValues() ||
      !(has_timezone ? base::Time::FromUTCExploded(exploded, parsed_time)
                     : base::Time::FromLocalExploded(exploded, parsed_time))) {
    return false;
  }
  *parsed_time -= timezone_offset;
  return true;
}

bool GetDateOnlyFromString(std::string_view raw_value,
                           base::Time* parsed_time) {
  // Parse the date part.
  base::Time::Exploded exploded = {0};
  {
    std::vector<std::string_view> parts = SplitStringPiece(raw_value, "-");
    if (parts.size() != 3 || !base::StringToInt(parts[0], &exploded.year) ||
        !base::StringToInt(parts[1], &exploded.month) ||
        !base::StringToInt(parts[2], &exploded.day_of_month)) {
      return false;
    }
  }

  // Convert back to a `base::Time`.
  base::Time time;
  if (!base::Time::FromUTCExploded(exploded, &time)) {
    return false;
  }

  // Zero the time part.
  *parsed_time = time.UTCMidnight();
  return true;
}

std::string FormatTimeAsString(const base::Time& time) {
  return time.is_null() ? std::string(kNullTimeString)
                        : base::TimeFormatAsIso8601(time);
}

std::string FormatTimeAsStringLocaltime(const base::Time& time) {
  return time.is_null() ? std::string(kNullTimeString)
                        : base::UnlocalizedTimeFormatWithPattern(
                              time, "yyyy-MM-dd'T'HH:mm:ss.SSS");
}

}  // namespace util
}  // namespace google_apis
