// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SESSION_SESSION_CERTIFICATE_POLICY_CACHE_IMPL_H_
#define IOS_WEB_SESSION_SESSION_CERTIFICATE_POLICY_CACHE_IMPL_H_

#include "ios/web/public/session/session_certificate_policy_cache.h"
#include "ios/web/session/session_certificate.h"

namespace net {
class X509Certificate;
}

namespace web {
namespace proto {
class CertificatesCacheStorage;
}  // namespace proto

// Concrete implementation of SessionCertificatePolicyCache.
class SessionCertificatePolicyCacheImpl final
    : public SessionCertificatePolicyCache {
 public:
  explicit SessionCertificatePolicyCacheImpl(BrowserState* browser_state);
  ~SessionCertificatePolicyCacheImpl() final;

  // Creates a SessionCertificatePolicyCacheImpl from serialized representation.
  SessionCertificatePolicyCacheImpl(
      BrowserState* browser_state,
      const proto::CertificatesCacheStorage& storage);

  // Serializes the SessionCertificatePolicyCacheImpl into `storage`.
  void SerializeToProto(proto::CertificatesCacheStorage& storage) const;

  // SessionCertificatePolicyCache:
  void UpdateCertificatePolicyCache() const final;
  void RegisterAllowedCertificate(
      const scoped_refptr<net::X509Certificate>& certificate,
      const std::string& host,
      net::CertStatus status) final;

 private:
  // Represents the allowed certificates.
  SessionCertificateSet allowed_certs_;
};

}  // namespace web

#endif  // IOS_WEB_SESSION_SESSION_CERTIFICATE_POLICY_CACHE_IMPL_H_
