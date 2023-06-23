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
  return der::EncodePosixTimeAsGeneralizedTime(
      (time - base::Time::UnixEpoch()).InSecondsFloored(), generalized_time);
}

bool GeneralizedTimeToTime(const der::GeneralizedTime& generalized,
                           base::Time* result) {
  int64_t posix_time;
  if (der::GeneralizedTimeToPosixTime(generalized, &posix_time)) {
    *result = base::Time::UnixEpoch() + base::Seconds(posix_time);
    return true;
  }
  return false;
}

}  // namespace net
