// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CERT_STATUS_FLAGS_H_
#define NET_CERT_CERT_STATUS_FLAGS_H_

#include <stdint.h>

#include "net/base/net_export.h"

namespace net {

// Bitmask of status flags of a certificate, representing any errors, as well as
// other non-error status information such as whether the certificate is EV.
typedef uint32_t CertStatus;

// NOTE: Because these names have appeared in bug reports, we preserve them as
// MACRO_STYLE for continuity, instead of renaming them to kConstantStyle as
// befits most static consts.
#define CERT_STATUS_FLAG(label, value) \
    CertStatus static const CERT_STATUS_##label = value;
#include "net/cert/cert_status_flags_list.h"
#undef CERT_STATUS_FLAG

static const CertStatus CERT_STATUS_ALL_ERRORS = 0xFF00FFFF;

// Returns true if the specified cert status has an error set.
inline bool IsCertStatusError(CertStatus status) {
  return (CERT_STATUS_ALL_ERRORS & status) != 0;
}

// Maps the most serious certificate error in the certificate status flags
// to the equivalent network error code.
NET_EXPORT int MapCertStatusToNetError(CertStatus cert_status);

}  // namespace net

#endif  // NET_CERT_CERT_STATUS_FLAGS_H_
