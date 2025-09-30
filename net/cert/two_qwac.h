// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_TWO_QWAC_H_
#define NET_CERT_TWO_QWAC_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/types/expected.h"
#include "crypto/hash.h"
#include "net/base/net_export.h"
#include "net/cert/x509_certificate.h"

namespace net {

// Signature algorithms used to sign a JWS, as specified in the "alg" JWS Header
// Parameter (RFC 7515, section 4.1.1) and in the JSON Web Signature and
// Encryption Algorithms IANA registry.
//
// The 2-QWAC spec does not list required algorithms to be supported. See the
// comment in ParseJades2QwacHeader regarding which algorithms we support and
// why.
enum class JwsSigAlg {
  // RS256 - RSA signing using PKCS1-v1.5 with SHA-256
  kRsaPkcs1Sha256,
  // PS256 - RSA PSS signing using SHA-256 and MGF1 with SHA-256
  kRsaPssSha256,
  // ES256 - ECDSA using P-256 and SHA-256
  kEcdsaP256Sha256,
};

// Contains fields from a JAdES (ETSI TS 119 182-1) signature header needed for
// verifying 2-QWAC TLS certificate bindings. While JAdES is a profile of JWS
// (RFC 7515), this is not general-purpose JWS or JWT code. It is also not
// general-purpose JAdES code, as only fields needed for 2-QWAC TLS certificate
// bindings are present here.
struct NET_EXPORT_PRIVATE Jades2QwacHeader {
  Jades2QwacHeader();
  Jades2QwacHeader(const Jades2QwacHeader& other);
  Jades2QwacHeader(Jades2QwacHeader&& other);
  ~Jades2QwacHeader();

  // The signature algorithm used to sign the JWS, as provided by the "alg" JWS
  // Header Parameter (RFC 7515, section 4.1.1). Valid values for this field can
  // be found in the JSON Web Signature and Encryption Algorithms IANA registry
  // (https://www.iana.org/assignments/jose/jose.xhtml#web-signature-encryption-algorithms).
  // The consumer of this struct must check that the algorithm provided in this
  // field matches the signature algorithm of the leaf cert in |two_qwac_cert|.
  JwsSigAlg sig_alg;

  // The certificate chain with a leaf cert that is a 2-QWAC. This certificate
  // chain is used to sign the JWS, which binds the 2-QWAC to a set of TLS
  // serverAuth certificates.
  scoped_refptr<net::X509Certificate> two_qwac_cert;

  // The hash algorithm used to hash the bound certificates.
  crypto::hash::HashKind hash_alg;

  // The hashes of the bound certificates (base64url-encoded), hashed using
  // |hash_alg|. Note: this is Digest(base64url(cert)), because that's what the
  // JAdES and 2-QWAC specs require (not that it makes any sense to do that).
  std::vector<std::vector<uint8_t>> bound_cert_hashes;
};

// A TwoQwacCertBinding represents a JAdES Signature (ETSI TS 119 182-1,
// clause 3.1) used for 2-QWACs (ETSI TS 119 411-5, clause 6.2.2). It comes from
// a TLS Certificate Binding (ETSI TS 119 411-5 annex B). Note that a JAdES
// Signature (which is also a JWS, a.k.a. JSON Web Signature) consists of a
// header and a cryptographic signature, not just a signature.
class NET_EXPORT_PRIVATE TwoQwacCertBinding {
 public:
  TwoQwacCertBinding(Jades2QwacHeader header,
                     std::string header_string,
                     std::vector<uint8_t> signature);
  TwoQwacCertBinding(const TwoQwacCertBinding& other);
  TwoQwacCertBinding(TwoQwacCertBinding&& other);
  ~TwoQwacCertBinding();

  // Parses a TLS Certificate Binding structure that contains a 2-QWAC
  // certificate chain. This function also performs steps 1-7 of RFC 7515
  // section 5.2 (signature verification).
  static base::expected<TwoQwacCertBinding, std::string> Parse(
      std::string_view jws);

  // This function verifies the signature in the TLS Certificate Binding,
  // performing steps 8-10 of RFC 7515 section 5.2 (signature verification). If
  // called on a struct created with TwoQwacCertBinding::Parse, all steps of RFC
  // 7515's signature verification have been performed and this function returns
  // whether the JWS was successfully validated.
  bool VerifySignature();

  // Returns true if the 2-QWAC TLS Certificate Binding binds the 2-QWAC cert
  // to the provided TLS cert (DER encoded). This performs step 6 of ETSI TS 119
  // 411-5 clause 6.2.2.
  //
  // E.g. Chrome connects to https://example.com, sees the Link header with
  // rel="tls-certificate-binding", fetches the TLS Certificate Binding at that
  // location, and creates a TwoQwacCertBinding from those bytes. For the 2-QWAC
  // to be valid, the TLS Certificate Binding (which contains the 2-QWAC) needs
  // to bind the TLS cert used on the connection to https://example.com. By
  // passing that TLS cert into this function, one can determine whether the TLS
  // cert used for the connection is listed in the binding.
  bool BindsTlsCert(base::span<const uint8_t> cert_der);

  // Returns the parsed JWS header.
  const Jades2QwacHeader& header() const { return header_; }

  // Returns the unparsed JWS header as a string.
  const std::string& header_string() const { return header_string_; }

 private:
  // The parsed JWS Header from the certificate binding structure.
  Jades2QwacHeader header_;

  // The unparsed JWS Header, needed for verifying the signature.
  std::string header_string_;

  // The JWS Signature (RFC 7515 section 2)/JAdES Signature Value (ETSI TS 119
  // 182-1 clause 3.1) from the certificate binding structure.
  std::vector<uint8_t> signature_;
};

}  // namespace net

#endif  // NET_CERT_TWO_QWAC_H_
