// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_status_flags.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "net/base/net_errors.h"

namespace net {

int MapCertStatusToNetError(CertStatus cert_status) {
  // A certificate may have multiple errors.  We report the most
  // serious error.

  // Unrecoverable errors
  if (cert_status & CERT_STATUS_INVALID)
    return ERR_CERT_INVALID;
  if (cert_status & CERT_STATUS_PINNED_KEY_MISSING)
    return ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN;

  // Potentially recoverable errors
  if (cert_status & CERT_STATUS_KNOWN_INTERCEPTION_BLOCKED)
    return ERR_CERT_KNOWN_INTERCEPTION_BLOCKED;
  if (cert_status & CERT_STATUS_REVOKED)
    return ERR_CERT_REVOKED;
  if (cert_status & CERT_STATUS_AUTHORITY_INVALID)
    return ERR_CERT_AUTHORITY_INVALID;
  if (cert_status & CERT_STATUS_COMMON_NAME_INVALID)
    return ERR_CERT_COMMON_NAME_INVALID;
  if (cert_status & CERT_STATUS_CERTIFICATE_TRANSPARENCY_REQUIRED)
    return ERR_CERTIFICATE_TRANSPARENCY_REQUIRED;
  if (cert_status & CERT_STATUS_SYMANTEC_LEGACY)
    return ERR_CERT_SYMANTEC_LEGACY;
  if (cert_status & CERT_STATUS_NAME_CONSTRAINT_VIOLATION)
    return ERR_CERT_NAME_CONSTRAINT_VIOLATION;
  if (cert_status & CERT_STATUS_WEAK_SIGNATURE_ALGORITHM)
    return ERR_CERT_WEAK_SIGNATURE_ALGORITHM;
  if (cert_status & CERT_STATUS_WEAK_KEY)
    return ERR_CERT_WEAK_KEY;
  if (cert_status & CERT_STATUS_DATE_INVALID)
    return ERR_CERT_DATE_INVALID;
  if (cert_status & CERT_STATUS_VALIDITY_TOO_LONG)
    return ERR_CERT_VALIDITY_TOO_LONG;
  if (cert_status & CERT_STATUS_NON_UNIQUE_NAME) {
    return ERR_CERT_NON_UNIQUE_NAME;
  }
  if (cert_status & CERT_STATUS_UNABLE_TO_CHECK_REVOCATION)
    return ERR_CERT_UNABLE_TO_CHECK_REVOCATION;
  if (cert_status & CERT_STATUS_NO_REVOCATION_MECHANISM)
    return ERR_CERT_NO_REVOCATION_MECHANISM;

  // Unknown status. The assumption is 0 (an OK status) won't be used here.
  NOTREACHED_IN_MIGRATION();
  return ERR_UNEXPECTED;
}

}  // namespace net
