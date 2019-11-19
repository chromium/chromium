// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_REVOCATION_UTIL_H_
#define NET_CERT_INTERNAL_REVOCATION_UTIL_H_

#include "base/optional.h"
#include "net/base/net_export.h"

namespace base {
class Time;
class TimeDelta;
}  // namespace base

namespace net {

namespace der {
struct GeneralizedTime;
}

// Returns true if a revocation status with |this_update| field and potentially
// a |next_update| field, is valid at |verify_time| and not older than
// |max_age|.  Expressed differently, returns true if |this_update <=
// verify_time < next_update|, and |this_update >= verify_time - max_age|.
NET_EXPORT_PRIVATE bool CheckRevocationDateValid(
    const der::GeneralizedTime& this_update,
    const der::GeneralizedTime* next_update,
    const base::Time& verify_time,
    const base::TimeDelta& max_age) WARN_UNUSED_RESULT;

}  // namespace net

#endif  // NET_CERT_INTERNAL_REVOCATION_UTIL_H_
