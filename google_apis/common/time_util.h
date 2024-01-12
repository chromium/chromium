// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_COMMON_TIME_UTIL_H_
#define GOOGLE_APIS_COMMON_TIME_UTIL_H_

#include <string>
#include <string_view>

namespace base {
class Time;
}  // namespace base

namespace google_apis {
namespace util {

// Parses an RFC 3339 date/time into a base::Time, returning true on success.
// The time string must be in the format "yyyy-mm-ddThh:mm:ss.dddTZ" (TZ is
// either '+hh:mm', '-hh:mm', 'Z' (representing UTC), or an empty string).
bool GetTimeFromString(std::string_view raw_value, base::Time* time);

// Parses a date only string of format "yyyy-mm-dd" into a `base::Time`
// returning true on success. The time part of `base::Time` is set to 00:00
// (midnight) UTC.
bool GetDateOnlyFromString(std::string_view raw_value, base::Time* time);

// Formats a base::Time as an RFC 3339 date/time (in UTC).
// If |time| is null, returns "null".
std::string FormatTimeAsString(const base::Time& time);

// Formats a base::Time as an RFC 3339 date/time (in localtime).
// If |time| is null, returns "null".
std::string FormatTimeAsStringLocaltime(const base::Time& time);

}  // namespace util
}  // namespace google_apis

#endif  // GOOGLE_APIS_COMMON_TIME_UTIL_H_
