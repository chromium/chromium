// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_PKI_REVOCATION_UTIL_H_
#define NET_CERT_PKI_REVOCATION_UTIL_H_

#include "net/base/net_export.h"

#include <cstdint>

namespace net {

namespace der {
struct GeneralizedTime;
}

// Returns true if a revocation status with |this_update| field and potentially
// a |next_update| field, is valid at POSIX time |verify_time_epoch_seconds| and
// not older than |max_age_seconds| seconds. Expressed differently, returns true
// if |this_update <= verify_time < next_update|, and |this_update >=
// verify_time - max_age|.
[[nodiscard]] NET_EXPORT_PRIVATE bool CheckRevocationDateValid(
    const der::GeneralizedTime& this_update,
    const der::GeneralizedTime* next_update,
    int64_t verify_time_epoch_seconds,
    int64_t max_age_seconds);

}  // namespace net

#endif  // NET_CERT_PKI_REVOCATION_UTIL_H_
