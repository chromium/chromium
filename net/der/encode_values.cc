// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/der/encode_values.h"

#include "base/time/time.h"
#include "net/der/parse_values.h"

#include "third_party/boringssl/src/include/openssl/time.h"

namespace net::der {

namespace {

bool WriteFourDigit(uint16_t value, uint8_t out[4]) {
  if (value >= 10000)
    return false;
  out[3] = '0' + (value % 10);
  value /= 10;
  out[2] = '0' + (value % 10);
  value /= 10;
  out[1] = '0' + (value % 10);
  value /= 10;
  out[0] = '0' + value;
  return true;
}

bool WriteTwoDigit(uint8_t value, uint8_t out[2]) {
  if (value >= 100)
    return false;
  out[0] = '0' + (value / 10);
  out[1] = '0' + (value % 10);
  return true;
}

}  // namespace

bool EncodeTimeAsGeneralizedTime(const base::Time& time,
                                 GeneralizedTime* generalized_time) {
  base::Time::Exploded exploded;
  time.UTCExplode(&exploded);
  if (!exploded.HasValidValues())
    return false;

  generalized_time->year = exploded.year;
  generalized_time->month = exploded.month;
  generalized_time->day = exploded.day_of_month;
  generalized_time->hours = exploded.hour;
  generalized_time->minutes = exploded.minute;
  generalized_time->seconds = exploded.second;
  return true;
}

bool EncodePosixTimeAsGeneralizedTime(int64_t posix_time,
                                      GeneralizedTime* generalized_time) {
  struct tm tmp_tm;
  if (!OPENSSL_posix_to_tm(posix_time, &tmp_tm)) {
    return false;
  }

  generalized_time->year = tmp_tm.tm_year + 1900;
  generalized_time->month = tmp_tm.tm_mon + 1;
  generalized_time->day = tmp_tm.tm_mday;
  generalized_time->hours = tmp_tm.tm_hour;
  generalized_time->minutes = tmp_tm.tm_min;
  generalized_time->seconds = tmp_tm.tm_sec;
  return true;
}

bool GeneralizedTimeToTime(const der::GeneralizedTime& generalized,
                           base::Time* result) {
  base::Time::Exploded exploded = {0};
  exploded.year = generalized.year;
  exploded.month = generalized.month;
  exploded.day_of_month = generalized.day;
  exploded.hour = generalized.hours;
  exploded.minute = generalized.minutes;
  exploded.second = generalized.seconds;

  if (base::Time::FromUTCExploded(exploded, result))
    return true;

  // Fail on obviously bad dates.
  if (!exploded.HasValidValues())
    return false;

  // TODO(mattm): consider consolidating this with
  // SaturatedTimeFromUTCExploded from cookie_util.cc
  if (static_cast<int>(generalized.year) > base::Time::kExplodedMaxYear) {
    *result = base::Time::Max();
    return true;
  }
  if (static_cast<int>(generalized.year) < base::Time::kExplodedMinYear) {
    *result = base::Time::Min();
    return true;
  }
  return false;
}

bool GeneralizedTimeToPosixTime(const der::GeneralizedTime& generalized,
                                int64_t* result) {
  struct tm tmp_tm;
  tmp_tm.tm_year = generalized.year - 1900;
  tmp_tm.tm_mon = generalized.month - 1;
  tmp_tm.tm_mday = generalized.day;
  tmp_tm.tm_hour = generalized.hours;
  tmp_tm.tm_min = generalized.minutes;
  tmp_tm.tm_sec = generalized.seconds;
  // BoringSSL POSIX time, like POSIX itself, does not support leap seconds.
  // Collapse to previous second.
  if (tmp_tm.tm_sec == 60) {
    tmp_tm.tm_sec = 59;
  }
  return OPENSSL_tm_to_posix(&tmp_tm, result);
}

bool EncodeGeneralizedTime(const GeneralizedTime& time,
                           uint8_t out[kGeneralizedTimeLength]) {
  if (!WriteFourDigit(time.year, out) || !WriteTwoDigit(time.month, out + 4) ||
      !WriteTwoDigit(time.day, out + 6) ||
      !WriteTwoDigit(time.hours, out + 8) ||
      !WriteTwoDigit(time.minutes, out + 10) ||
      !WriteTwoDigit(time.seconds, out + 12)) {
    return false;
  }
  out[14] = 'Z';
  return true;
}

bool EncodeUTCTime(const GeneralizedTime& time, uint8_t out[kUTCTimeLength]) {
  if (!time.InUTCTimeRange())
    return false;

  uint16_t year = time.year - 1900;
  if (year >= 100)
    year -= 100;

  if (!WriteTwoDigit(year, out) || !WriteTwoDigit(time.month, out + 2) ||
      !WriteTwoDigit(time.day, out + 4) ||
      !WriteTwoDigit(time.hours, out + 6) ||
      !WriteTwoDigit(time.minutes, out + 8) ||
      !WriteTwoDigit(time.seconds, out + 10)) {
    return false;
  }
  out[12] = 'Z';
  return true;
}

}  // namespace net::der
