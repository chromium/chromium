// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SECURITY_CERT_VERIFICATION_ERROR_H_
#define IOS_WEB_SECURITY_CERT_VERIFICATION_ERROR_H_

#include "base/containers/mru_cache.h"
#include "ios/web/security/cert_host_pair.h"
#include "net/cert/cert_status_flags.h"

namespace web {

// Represents cert verification error, which happened inside
// |webView:didReceiveAuthenticationChallenge:completionHandler:| and should be
// checked inside |webView:didFailProvisionalNavigation:withError:|.
struct CertVerificationError {
  CertVerificationError(bool is_recoverable, net::CertStatus status)
      : is_recoverable(is_recoverable), status(status) {}

  bool is_recoverable;
  net::CertStatus status;
};

// Type of Cache object for storing cert verification errors.
typedef base::MRUCache<web::CertHostPair, CertVerificationError>
    CertVerificationErrorsCacheType;

}  // namespace web

#endif  // IOS_WEB_SECURITY_CERT_VERIFICATION_ERROR_H_
