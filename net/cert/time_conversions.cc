// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/time_conversions.h"

#include "base/time/time.h"
#include "third_party/boringssl/src/pki/encode_values.h"
#include "third_party/boringssl/src/pki/parse_values.h"

#include "third_party/boringssl/src/include/openssl/time.h"

namespace net {

bool EncodeTimeAsGeneralizedTime(const base::Time& time,
                                 bssl::der::GeneralizedTime* generalized_time) {
  return bssl::der::EncodePosixTimeAsGeneralizedTime(
      (time - base::Time::UnixEpoch()).InSecondsFloored(), generalized_time);
}

bool GeneralizedTimeToTime(const bssl::der::GeneralizedTime& generalized,
                           base::Time* result) {
  int64_t posix_time;
  if (bssl::der::GeneralizedTimeToPosixTime(generalized, &posix_time)) {
    *result = base::Time::UnixEpoch() + base::Seconds(posix_time);
    return true;
  }
  return false;
}

}  // namespace net
