// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/public/cwv_x509_certificate.h"

#import "ios/web_view/internal/cwv_x509_certificate_internal.h"

#include "base/memory/scoped_refptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "net/cert/x509_certificate.h"

@implementation CWVX509Certificate {
  scoped_refptr<net::X509Certificate> _internalCertificate;
}

- (instancetype)initWithInternalCertificate:
    (scoped_refptr<net::X509Certificate>)internalCertificate {
  DCHECK(internalCertificate);
  self = [super init];
  if (self) {
    _internalCertificate = internalCertificate;
  }
  return self;
}

- (NSString*)issuerDisplayName {
  const net::CertPrincipal& issuer = _internalCertificate->issuer();
  return base::SysUTF8ToNSString(issuer.GetDisplayName());
}

- (NSDate*)validExpiry {
  const base::Time& valid_expiry = _internalCertificate->valid_expiry();
  return valid_expiry.ToNSDate();
}

@end
