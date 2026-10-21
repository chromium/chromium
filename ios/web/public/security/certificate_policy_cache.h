// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_SECURITY_CERTIFICATE_POLICY_CACHE_H_
#define IOS_WEB_PUBLIC_SECURITY_CERTIFICATE_POLICY_CACHE_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"

namespace web {

// A manager for certificate policy decisions for hosts, used to remember
// decisions about how to handle problematic certs.
// This class is thread-safe only in that in can be created and passed around
// on any thread; the policy-related methods can only be called from the IO
// thread.
class CertificatePolicyCache
    : public base::RefCountedThreadSafe<CertificatePolicyCache> {
 public:
  // The judgments the certificate policy cache can reach.
  enum Judgment {
    // There is no policy information for this certificate.
    UNKNOWN,
    // This certificate is allowed.
    ALLOWED,
    // This certificate is denied.
    DENIED,
  };

  CertificatePolicyCache(const CertificatePolicyCache&) = delete;
  CertificatePolicyCache& operator=(const CertificatePolicyCache&) = delete;

  // Everything from here on can only be called from the IO thread.

  // Records that `cert` is permitted to be used for `host` in the future.
  virtual void AllowCertForHost(const net::X509Certificate* cert,
                                const std::string& host,
                                net::CertStatus error) = 0;

  // Queries whether `cert` is allowed or denied for `host`.
  virtual Judgment QueryPolicy(const net::X509Certificate* cert,
                               const std::string& host,
                               net::CertStatus error) = 0;

  // Removes all policies stored in this instance.
  virtual void ClearCertificatePolicies() = 0;

 protected:
  CertificatePolicyCache() = default;
  virtual ~CertificatePolicyCache() = default;

 private:
  friend class base::RefCountedThreadSafe<CertificatePolicyCache>;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_SECURITY_CERTIFICATE_POLICY_CACHE_H_
