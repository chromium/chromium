// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_status_flags.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "net/base/net_errors.h"

namespace net {

CertStatus MapNetErrorToCertStatus(int error) {
  switch (error) {
    case ERR_CERT_COMMON_NAME_INVALID:
      return CERT_STATUS_COMMON_NAME_INVALID;
    case ERR_CERT_DATE_INVALID:
      return CERT_STATUS_DATE_INVALID;
    case ERR_CERT_AUTHORITY_INVALID:
      return CERT_STATUS_AUTHORITY_INVALID;
    case ERR_CERT_NO_REVOCATION_MECHANISM:
      return CERT_STATUS_NO_REVOCATION_MECHANISM;
    case ERR_CERT_UNABLE_TO_CHECK_REVOCATION:
      return CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;
    case ERR_CERTIFICATE_TRANSPARENCY_REQUIRED:
      return CERT_STATUS_CERTIFICATE_TRANSPARENCY_REQUIRED;
    case ERR_CERT_REVOKED:
      return CERT_STATUS_REVOKED;
    // We added the ERR_CERT_CONTAINS_ERRORS error code when we were using
    // WinInet, but we never figured out how it differs from ERR_CERT_INVALID.
    // We should not use ERR_CERT_CONTAINS_ERRORS in new code.
    case ERR_CERT_CONTAINS_ERRORS:
      NOTREACHED();
      FALLTHROUGH;
    case ERR_CERT_INVALID:
      return CERT_STATUS_INVALID;
    case ERR_CERT_WEAK_SIGNATURE_ALGORITHM:
      return CERT_STATUS_WEAK_SIGNATURE_ALGORITHM;
    case ERR_CERT_NON_UNIQUE_NAME:
      return CERT_STATUS_NON_UNIQUE_NAME;
    case ERR_CERT_WEAK_KEY:
      return CERT_STATUS_WEAK_KEY;
    case ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN:
      return CERT_STATUS_PINNED_KEY_MISSING;
    case ERR_CERT_NAME_CONSTRAINT_VIOLATION:
      return CERT_STATUS_NAME_CONSTRAINT_VIOLATION;
    case ERR_CERT_VALIDITY_TOO_LONG:
      return CERT_STATUS_VALIDITY_TOO_LONG;
    case ERR_CERT_SYMANTEC_LEGACY:
      return CERT_STATUS_SYMANTEC_LEGACY;
    case ERR_CERT_KNOWN_INTERCEPTION_BLOCKED:
      return (CERT_STATUS_KNOWN_INTERCEPTION_BLOCKED | CERT_STATUS_REVOKED);
    case ERR_SSL_OBSOLETE_VERSION:
      return CERT_STATUS_LEGACY_TLS;
    default:
      return 0;
  }
}

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
  // CERT_STATUS_NON_UNIQUE_NAME is intentionally not mapped to an error.
  // It is treated as just a warning and used to degrade the SSL UI.
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
  if (cert_status & CERT_STATUS_UNABLE_TO_CHECK_REVOCATION)
    return ERR_CERT_UNABLE_TO_CHECK_REVOCATION;
  if (cert_status & CERT_STATUS_NO_REVOCATION_MECHANISM)
    return ERR_CERT_NO_REVOCATION_MECHANISM;
  if (cert_status & CERT_STATUS_LEGACY_TLS)
    return ERR_SSL_OBSOLETE_VERSION;

  // Unknown status. The assumption is 0 (an OK status) won't be used here.
  NOTREACHED();
  return ERR_UNEXPECTED;
}

}  // namespace net
