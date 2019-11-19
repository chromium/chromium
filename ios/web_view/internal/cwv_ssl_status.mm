// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/security/ssl_status.h"

#import "ios/web_view/internal/cwv_ssl_status_internal.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
CWVSecurityStyle CWVSecurityStyleFromWebSecurityStyle(
    web::SecurityStyle style) {
  switch (style) {
    case web::SECURITY_STYLE_UNKNOWN:
      return CWVSecurityStyleUnknown;
    case web::SECURITY_STYLE_UNAUTHENTICATED:
      return CWVSecurityStyleUnauthenticated;
    case web::SECURITY_STYLE_AUTHENTICATION_BROKEN:
      return CWVSecurityStyleAuthenticationBroken;
    case web::SECURITY_STYLE_AUTHENTICATED:
      return CWVSecurityStyleAuthenticated;
  }
}
}  // namespace

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
  return cwv_status;
}

@implementation CWVSSLStatus {
  web::SSLStatus _internalStatus;
}

- (instancetype)initWithInternalStatus:(const web::SSLStatus&)internalStatus {
  self = [super init];
  if (self) {
    _internalStatus = internalStatus;
  }
  return self;
}

- (CWVSecurityStyle)securityStyle {
  return CWVSecurityStyleFromWebSecurityStyle(_internalStatus.security_style);
}

- (BOOL)hasOnlySecureContent {
  return _internalStatus.security_style == web::SECURITY_STYLE_AUTHENTICATED &&
         !(_internalStatus.content_status &
           web::SSLStatus::DISPLAYED_INSECURE_CONTENT);
}

- (CWVCertStatus)certStatus {
  return CWVCertStatusFromNetCertStatus(_internalStatus.cert_status);
}

@end
