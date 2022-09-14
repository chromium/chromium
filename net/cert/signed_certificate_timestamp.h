// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_SIGNED_CERTIFICATE_TIMESTAMP_H_
#define NET_CERT_SIGNED_CERTIFICATE_TIMESTAMP_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "net/base/hash_value.h"
#include "net/base/net_export.h"

namespace base {
class Pickle;
class PickleIterator;
}

// Structures related to Certificate Transparency (RFC6962).
namespace net::ct {

// Contains the data necessary to reconstruct the signed_entry of a
// SignedCertificateTimestamp, from RFC 6962, Section 3.2.
//
// All the data necessary to validate a SignedCertificateTimestamp is present
// within the SignedCertificateTimestamp, except for the signature_type,
// entry_type, and the actual entry. The only supported signature_type at
// present is certificate_timestamp.  The entry_type is implicit from the
// context in which it is received (those in the X.509 extension are
// precert_entry, all others are x509_entry). The signed_entry itself is
// reconstructed from the certificate being verified, or from the corresponding
// precertificate.
//
// The SignedEntryData contains this reconstructed data, and can be used to
// either generate or verify the signature in SCTs.
struct NET_EXPORT SignedEntryData {
  // LogEntryType enum in RFC 6962, Section 3.1
  enum Type {
    LOG_ENTRY_TYPE_X509 = 0,
    LOG_ENTRY_TYPE_PRECERT = 1
  };

  SignedEntryData();
  ~SignedEntryData();
  void Reset();

  Type type = LOG_ENTRY_TYPE_X509;

  // Set if type == LOG_ENTRY_TYPE_X509
  std::string leaf_certificate;

  // Set if type == LOG_ENTRY_TYPE_PRECERT
  SHA256HashValue issuer_key_hash;
  std::string tbs_certificate;
};

// Helper structure to represent Digitally Signed data, as described in
// Sections 4.7 and 7.4.1.4.1 of RFC 5246.
struct NET_EXPORT DigitallySigned {
  enum HashAlgorithm {
    HASH_ALGO_NONE = 0,
    HASH_ALGO_MD5 = 1,
    HASH_ALGO_SHA1 = 2,
    HASH_ALGO_SHA224 = 3,
    HASH_ALGO_SHA256 = 4,
    HASH_ALGO_SHA384 = 5,
    HASH_ALGO_SHA512 = 6,
  };

  enum SignatureAlgorithm {
    SIG_ALGO_ANONYMOUS = 0,
    SIG_ALGO_RSA = 1,
    SIG_ALGO_DSA = 2,
    SIG_ALGO_ECDSA = 3
  };

  DigitallySigned();
  ~DigitallySigned();

  // Returns true if |other_hash_algorithm| and |other_signature_algorithm|
  // match this DigitallySigned hash and signature algorithms.
  bool SignatureParametersMatch(
      HashAlgorithm other_hash_algorithm,
      SignatureAlgorithm other_signature_algorithm) const;

  HashAlgorithm hash_algorithm = HASH_ALGO_NONE;
  SignatureAlgorithm signature_algorithm = SIG_ALGO_ANONYMOUS;
  // 'signature' field.
  std::string signature_data;
};

// SignedCertificateTimestamp struct in RFC 6962, Section 3.2.
struct NET_EXPORT SignedCertificateTimestamp
    : public base::RefCountedThreadSafe<SignedCertificateTimestamp> {
  // Predicate functor used in maps when SignedCertificateTimestamp is used as
  // the key.
  struct NET_EXPORT LessThan {
    bool operator()(const scoped_refptr<SignedCertificateTimestamp>& lhs,
                    const scoped_refptr<SignedCertificateTimestamp>& rhs) const;
  };

  // Version enum in RFC 6962, Section 3.2.
  enum Version {
    V1 = 0,
  };

  // Source of the SCT - supplementary, not defined in CT RFC.
  // Note: The numeric values are used within histograms and should not change
  // or be re-assigned.
  enum Origin {
    SCT_EMBEDDED = 0,
    SCT_FROM_TLS_EXTENSION = 1,
    SCT_FROM_OCSP_RESPONSE = 2,
    SCT_ORIGIN_MAX,
  };

  SignedCertificateTimestamp();

  SignedCertificateTimestamp(const SignedCertificateTimestamp&) = delete;
  SignedCertificateTimestamp& operator=(const SignedCertificateTimestamp&) =
      delete;

  void Persist(base::Pickle* pickle);
  static scoped_refptr<SignedCertificateTimestamp> CreateFromPickle(
      base::PickleIterator* iter);

  Version version = V1;
  std::string log_id;
  base::Time timestamp;
  std::string extensions;
  DigitallySigned signature;
  Origin origin = SCT_EMBEDDED;
  // The log description is not one of the SCT fields, but a user-readable
  // name defined alongside the log key. It should not participate
  // in equality checks as the log's description could change while
  // the SCT would be the same.
  std::string log_description;

 private:
  friend class base::RefCountedThreadSafe<SignedCertificateTimestamp>;

  ~SignedCertificateTimestamp();
};

using SCTList = std::vector<scoped_refptr<ct::SignedCertificateTimestamp>>;

}  // namespace net::ct

#endif  // NET_CERT_SIGNED_CERTIFICATE_TIMESTAMP_H_
