// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SESSION_SESSION_CERTIFICATE_H_
#define IOS_WEB_SESSION_SESSION_CERTIFICATE_H_

#include <string>
#include <unordered_set>

#include "base/memory/scoped_refptr.h"
#include "net/base/hash_value.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"

namespace web {
namespace proto {
class CertificateStorage;
}  // namespace proto

// Represents an allowed certificate for a specific host as stored in the
// SessionCertificatePolicyCache.
class SessionCertificate {
 public:
  // Creates a SessionCertificate representing the leaf certificate
  // `certificate` delivered by `host` with `status`.
  SessionCertificate(const scoped_refptr<net::X509Certificate>& certificate,
                     const std::string& host,
                     net::CertStatus status);

  // Creates a SessionCertificate from serialized representation.
  explicit SessionCertificate(const proto::CertificateStorage& storage);

  SessionCertificate(SessionCertificate&&);
  SessionCertificate(const SessionCertificate&);

  SessionCertificate& operator=(SessionCertificate&&);
  SessionCertificate& operator=(const SessionCertificate&);

  ~SessionCertificate();

  // Serializes the SessionCertificate into `storage`.
  void SerializeToProto(proto::CertificateStorage& storage) const;

  // Returns the `host`, `status` and `certificate` respectively.
  const std::string& host() const { return host_; }
  net::CertStatus status() const { return status_; }
  const scoped_refptr<net::X509Certificate>& certificate() const {
    return certificate_;
  }

 private:
  scoped_refptr<net::X509Certificate> certificate_;
  std::string host_;
  net::CertStatus status_;
};

// Equality and inequality operator for SessionCertificate.
bool operator==(const SessionCertificate& lhs, const SessionCertificate& rhs);
bool operator!=(const SessionCertificate& lhs, const SessionCertificate& rhs);

// Hash operator.
struct SessionCertificateHasher {
  size_t operator()(const SessionCertificate& value) const;
};

// Unordered set of SessionCertificate using SessionCertificateHasher as the
// hashing functor.
using SessionCertificateSet =
    std::unordered_set<SessionCertificate, SessionCertificateHasher>;

}  // namespace web

#endif  // IOS_WEB_SESSION_SESSION_CERTIFICATE_H_
