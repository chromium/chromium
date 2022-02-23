// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_SIGNATURE_ALGORITHM_H_
#define NET_CERT_INTERNAL_SIGNATURE_ALGORITHM_H_

#include <stdint.h>

#include <memory>

#include "net/base/net_export.h"

namespace net {

class CertErrors;

namespace der {
class Input;
}  // namespace der

// The digest algorithm used within a signature.
enum class DigestAlgorithm {
  Md2,
  Md4,
  Md5,
  Sha1,
  Sha256,
  Sha384,
  Sha512,
};

// The signature scheme used within a signature. Parameters are specified
// separately.
enum class SignatureAlgorithmId {
  RsaPkcs1,  // RSA PKCS#1 v1.5
  RsaPss,    // RSASSA-PSS
  Ecdsa,     // ECDSA
  Dsa,       // DSA
};

// A classification of the RSA-PSS parameters. This is only used in histograms
// and is gathered with the hope to reduce RSA-PSS to a small set of enums in
// the future. See https://crbug.com/1279975.
//
// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "RsaPssClassification" in src/tools/metrics/histograms/enums.xml.
enum class RsaPssClassification {
  // The MGF-1 digest and signing digest did not match.
  kDigestMismatch = 0,
  // The digest algorithm was MD5 or older.
  kLegacyDigest = 1,
  // SHA-1 with salt length of 20.
  kSha1 = 2,
  // SHA-1 with a non-standard salt length.
  kSha1NonstandardSalt = 3,
  // SHA-256 with a salt length of 32.
  kSha256 = 4,
  // SHA-256 with a non-standard salt length.
  kSha256NonstandardSalt = 5,
  // SHA-384 with a salt length of 48.
  kSha384 = 6,
  // SHA-384 with a non-standard salt length.
  kSha384NonstandardSalt = 7,
  // SHA-512 with a salt length of 64.
  kSha512 = 8,
  // SHA-512 with a non-standard salt length.
  kSha512NonstandardSalt = 9,
  kMaxValue = kSha512NonstandardSalt,
};

// Parses a HashAlgorithm as defined by RFC 5912:
//
//     HashAlgorithm  ::=  AlgorithmIdentifier{DIGEST-ALGORITHM,
//                             {HashAlgorithms}}
//
//     HashAlgorithms DIGEST-ALGORITHM ::=  {
//         { IDENTIFIER id-sha1 PARAMS TYPE NULL ARE preferredPresent } |
//         { IDENTIFIER id-sha224 PARAMS TYPE NULL ARE preferredPresent } |
//         { IDENTIFIER id-sha256 PARAMS TYPE NULL ARE preferredPresent } |
//         { IDENTIFIER id-sha384 PARAMS TYPE NULL ARE preferredPresent } |
//         { IDENTIFIER id-sha512 PARAMS TYPE NULL ARE preferredPresent }
//     }
[[nodiscard]] bool ParseHashAlgorithm(const der::Input& input,
                                      DigestAlgorithm* out);

// Base class for describing algorithm parameters.
class NET_EXPORT SignatureAlgorithmParameters {
 public:
  SignatureAlgorithmParameters() {}

  SignatureAlgorithmParameters(const SignatureAlgorithmParameters&) = delete;
  SignatureAlgorithmParameters& operator=(const SignatureAlgorithmParameters&) =
      delete;

  virtual ~SignatureAlgorithmParameters() {}
};

// Parameters for an RSASSA-PSS signature algorithm.
//
// The trailer is assumed to be 1 and the mask generation algorithm to be MGF1,
// as that is all that is implemented, and any other values while parsing the
// AlgorithmIdentifier will thus be rejected.
class NET_EXPORT RsaPssParameters : public SignatureAlgorithmParameters {
 public:
  RsaPssParameters(DigestAlgorithm mgf1_hash, uint32_t salt_length);

  DigestAlgorithm mgf1_hash() const { return mgf1_hash_; }
  uint32_t salt_length() const { return salt_length_; }

 private:
  const DigestAlgorithm mgf1_hash_;
  const uint32_t salt_length_;
};

// SignatureAlgorithm describes a signature algorithm and its parameters. This
// corresponds to "AlgorithmIdentifier" from RFC 5280.
class NET_EXPORT SignatureAlgorithm {
 public:
  SignatureAlgorithm(const SignatureAlgorithm&) = delete;
  SignatureAlgorithm& operator=(const SignatureAlgorithm&) = delete;

  ~SignatureAlgorithm();

  SignatureAlgorithmId algorithm() const { return algorithm_; }
  DigestAlgorithm digest() const { return digest_; }

  // Creates a SignatureAlgorithm by parsing a DER-encoded "AlgorithmIdentifier"
  // (RFC 5280). Returns nullptr on failure. If |errors| was non-null then
  // error/warning information is output to it.
  static std::unique_ptr<SignatureAlgorithm> Create(
      const der::Input& algorithm_identifier,
      CertErrors* errors);

  // Creates a new SignatureAlgorithm with the given type and parameters.
  // Guaranteed to return non-null result.
  static std::unique_ptr<SignatureAlgorithm> CreateRsaPkcs1(
      DigestAlgorithm digest);
  static std::unique_ptr<SignatureAlgorithm> CreateDsa(DigestAlgorithm digest);
  static std::unique_ptr<SignatureAlgorithm> CreateEcdsa(
      DigestAlgorithm digest);
  static std::unique_ptr<SignatureAlgorithm> CreateRsaPss(
      DigestAlgorithm digest,
      DigestAlgorithm mgf1_hash,
      uint32_t salt_length);

  // The following methods retrieve the parameters for the signature algorithm.
  //
  // The correct parameters should be chosen based on the algorithm ID. For
  // instance a SignatureAlgorithm with |algorithm() == RsaPss| should retrieve
  // parameters via ParametersForRsaPss().
  //
  // The returned pointer is non-owned, and has the same lifetime as |this|.
  const RsaPssParameters* ParamsForRsaPss() const;

  bool has_params() const { return !!params_; }

  // Returns true if |alg1_tlv| and |alg2_tlv| represent an equivalent
  // AlgorithmIdentifier once parsed.
  static bool IsEquivalent(const der::Input& alg1_tlv,
                           const der::Input& alg2_tlv);

 private:
  SignatureAlgorithm(SignatureAlgorithmId algorithm,
                     DigestAlgorithm digest,
                     std::unique_ptr<SignatureAlgorithmParameters> params);

  const SignatureAlgorithmId algorithm_;
  const DigestAlgorithm digest_;
  const std::unique_ptr<SignatureAlgorithmParameters> params_;
};

}  // namespace net

#endif  // NET_CERT_INTERNAL_SIGNATURE_ALGORITHM_H_
