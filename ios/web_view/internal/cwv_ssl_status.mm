// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/security/ssl_status.h"

#import "ios/web_view/internal/cwv_ssl_status_internal.h"
#import "ios/web_view/internal/cwv_ssl_util.h"
#import "ios/web_view/internal/cwv_x509_certificate_internal.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"

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

@implementation CWVSSLStatus {
  web::SSLStatus _internalStatus;
}

- (instancetype)initWithInternalStatus:(const web::SSLStatus&)internalStatus {
  self = [super init];
  if (self) {
    _internalStatus = internalStatus;

    if (internalStatus.certificate) {
      _certificate = [[CWVX509Certificate alloc]
          initWithInternalCertificate:internalStatus.certificate];
    }
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
