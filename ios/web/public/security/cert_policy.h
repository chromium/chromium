// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_SECURITY_CERT_POLICY_H_
#define IOS_WEB_PUBLIC_SECURITY_CERT_POLICY_H_

#include <map>

#include "net/base/hash_value.h"
#include "net/cert/cert_status_flags.h"

namespace net {
class X509Certificate;
}

namespace web {

// This class is useful for maintaining policies about which certificates are
// permitted or forbidden for a particular purpose.
class CertPolicy {
 public:
  // The judgments this policy can reach.
  enum Judgment {
    // We don't have policy information for this certificate.
    UNKNOWN,

    // This certificate is allowed.
    ALLOWED,

    // This certificate is denied.
    DENIED,
  };

  CertPolicy();
  ~CertPolicy();

  // Returns the judgment this policy makes about this certificate.
  // For a certificate to be allowed, it must not have any *additional* errors
  // from when it was allowed.
  // This function returns either ALLOWED or UNKNOWN, but never DENIED.
  Judgment Check(net::X509Certificate* cert, net::CertStatus error) const;

  // Causes the policy to allow this certificate for a given |error|.
  void Allow(net::X509Certificate* cert, net::CertStatus error);

 private:
  // The set of fingerprints of allowed certificates.
  std::map<net::SHA256HashValue, net::CertStatus> allowed_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_SECURITY_CERT_POLICY_H_
