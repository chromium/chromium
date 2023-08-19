// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_ssl_util.h"

CWVCertStatus CWVCertStatusFromNetCertStatus(net::CertStatus cert_status) {
  CWVCertStatus cwv_status = 0;
  if (cert_status & net::CERT_STATUS_COMMON_NAME_INVALID) {
    cwv_status |= CWVCertStatusCommonNameInvalid;
  }
  if (cert_status & net::CERT_STATUS_DATE_INVALID) {
    cwv_status |= CWVCertStatusDateInvalid;
  }
  if (cert_status & net::CERT_STATUS_AUTHORITY_INVALID) {
    cwv_status |= CWVCertStatusAuthorityInvalid;
  }
  if (cert_status & net::CERT_STATUS_NO_REVOCATION_MECHANISM) {
    cwv_status |= CWVCertStatusNoRevocationMechanism;
  }
  if (cert_status & net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION) {
    cwv_status |= CWVCertStatusUnableToCheckRevocation;
  }
  if (cert_status & net::CERT_STATUS_REVOKED) {
    cwv_status |= CWVCertStatusRevoked;
  }
  if (cert_status & net::CERT_STATUS_INVALID) {
    cwv_status |= CWVCertStatusInvalid;
  }
  if (cert_status & net::CERT_STATUS_WEAK_SIGNATURE_ALGORITHM) {
    cwv_status |= CWVCertStatusWeakSignatureAlgorithm;
  }
  if (cert_status & net::CERT_STATUS_NON_UNIQUE_NAME) {
    cwv_status |= CWVCertStatusNonUniqueName;
  }
  if (cert_status & net::CERT_STATUS_WEAK_KEY) {
    cwv_status |= CWVCertStatusWeakKey;
  }
  if (cert_status & net::CERT_STATUS_PINNED_KEY_MISSING) {
    cwv_status |= CWVCertStatusPinnedKeyMissing;
  }
  if (cert_status & net::CERT_STATUS_NAME_CONSTRAINT_VIOLATION) {
    cwv_status |= CWVCertStatusNameConstraintViolation;
  }
  if (cert_status & net::CERT_STATUS_VALIDITY_TOO_LONG) {
    cwv_status |= CWVCertStatusValidityTooLong;
  }
  if (cert_status & net::CERT_STATUS_CERTIFICATE_TRANSPARENCY_REQUIRED) {
    cwv_status |= CWVCertStatusCertificateTransparencyRequired;
  }
  if (cert_status & net::CERT_STATUS_SYMANTEC_LEGACY) {
    cwv_status |= CWVCertStatusSymantecLegacy;
  }
  if (cert_status & net::CERT_STATUS_KNOWN_INTERCEPTION_BLOCKED) {
    cwv_status |= CWVCertStatusKnownInterceptionBlocked;
  }
  return cwv_status;
}
