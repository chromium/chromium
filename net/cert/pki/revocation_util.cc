// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/pki/revocation_util.h"

#include "net/der/encode_values.h"
#include "net/der/parse_values.h"

namespace net {

namespace {

constexpr int64_t kMinValidTime = -62167219200;  // 0000-01-01 00:00:00 UTC
constexpr int64_t kMaxValidTime = 253402300799;  // 9999-12-31 23:59:59 UTC

}  // namespace

bool CheckRevocationDateValid(const der::GeneralizedTime& this_update,
                              const der::GeneralizedTime* next_update,
                              int64_t verify_time_epoch_seconds,
                              int64_t max_age_seconds) {
  if (verify_time_epoch_seconds > kMaxValidTime ||
      verify_time_epoch_seconds < kMinValidTime ||
      max_age_seconds > kMaxValidTime || max_age_seconds < 0) {
    return false;
  }
  der::GeneralizedTime verify_time;
  if (!der::EncodePosixTimeAsGeneralizedTime(verify_time_epoch_seconds,
                                             &verify_time)) {
    return false;
  }

  if (this_update > verify_time) {
    return false;  // Response is not yet valid.
  }

  if (next_update && (*next_update <= verify_time)) {
    return false;  // Response is no longer valid.
  }

  der::GeneralizedTime earliest_this_update;
  if (!der::EncodePosixTimeAsGeneralizedTime(
          verify_time_epoch_seconds - max_age_seconds, &earliest_this_update)) {
    return false;
  }
  if (this_update < earliest_this_update) {
    return false;  // Response is too old.
  }

  return true;
}

}  // namespace net
