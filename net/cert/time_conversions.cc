// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/time_conversions.h"

#include "base/time/time.h"
#include "net/der/encode_values.h"
#include "net/der/parse_values.h"

#include "third_party/boringssl/src/include/openssl/time.h"

namespace net {

bool EncodeTimeAsGeneralizedTime(const base::Time& time,
                                 der::GeneralizedTime* generalized_time) {
  base::Time::Exploded exploded;
  time.UTCExplode(&exploded);
  if (!exploded.HasValidValues()) {
    return false;
  }

  generalized_time->year = exploded.year;
  generalized_time->month = exploded.month;
  generalized_time->day = exploded.day_of_month;
  generalized_time->hours = exploded.hour;
  generalized_time->minutes = exploded.minute;
  generalized_time->seconds = exploded.second;
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

  if (base::Time::FromUTCExploded(exploded, result)) {
    return true;
  }

  // Fail on obviously bad dates.
  if (!exploded.HasValidValues()) {
    return false;
  }

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

}  // namespace net
