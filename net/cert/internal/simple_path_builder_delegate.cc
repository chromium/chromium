// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/simple_path_builder_delegate.h"

#include "base/logging.h"
#include "net/cert/internal/cert_error_params.h"
#include "net/cert/internal/cert_errors.h"
#include "net/cert/internal/signature_algorithm.h"
#include "net/cert/internal/verify_signed_data.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/nid.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

namespace net {

DEFINE_CERT_ERROR_ID(SimplePathBuilderDelegate::kRsaModulusTooSmall,
                     "RSA modulus too small");

namespace {

DEFINE_CERT_ERROR_ID(kUnacceptableCurveForEcdsa,
                     "Only P-256, P-384, P-521 are supported for ECDSA");

bool IsAcceptableCurveForEcdsa(int curve_nid) {
  switch (curve_nid) {
    case NID_X9_62_prime256v1:
    case NID_secp384r1:
    case NID_secp521r1:
      return true;
  }

  return false;
}

}  // namespace

SimplePathBuilderDelegate::SimplePathBuilderDelegate(
    size_t min_rsa_modulus_length_bits,
    DigestPolicy digest_policy)
    : min_rsa_modulus_length_bits_(min_rsa_modulus_length_bits),
      digest_policy_(digest_policy) {}

void SimplePathBuilderDelegate::CheckPathAfterVerification(
    const CertPathBuilder& path_builder,
    CertPathBuilderResultPath* path) {
  // Do nothing - consider all candidate paths valid.
}

bool SimplePathBuilderDelegate::IsSignatureAlgorithmAcceptable(
    const SignatureAlgorithm& algorithm,
    CertErrors* errors) {
  // Restrict default permitted signature algorithms to:
  //
  //    RSA PKCS#1 v1.5
  //    RSASSA-PSS
  //    ECDSA
  switch (algorithm.algorithm()) {
    case SignatureAlgorithmId::Dsa:
      return false;
    case SignatureAlgorithmId::Ecdsa:
    case SignatureAlgorithmId::RsaPkcs1:
      return IsAcceptableDigest(algorithm.digest());
    case SignatureAlgorithmId::RsaPss:
      return IsAcceptableDigest(algorithm.digest()) &&
             IsAcceptableDigest(algorithm.ParamsForRsaPss()->mgf1_hash());
  }

  return false;
}

bool SimplePathBuilderDelegate::IsPublicKeyAcceptable(EVP_PKEY* public_key,
                                                      CertErrors* errors) {
  int pkey_id = EVP_PKEY_id(public_key);
  if (pkey_id == EVP_PKEY_RSA) {
    // Extract the modulus length from the key.
    RSA* rsa = EVP_PKEY_get0_RSA(public_key);
    if (!rsa)
      return false;
    unsigned int modulus_length_bits = BN_num_bits(rsa->n);

    if (modulus_length_bits < min_rsa_modulus_length_bits_) {
      errors->AddError(
          kRsaModulusTooSmall,
          CreateCertErrorParams2SizeT("actual", modulus_length_bits, "minimum",
                                      min_rsa_modulus_length_bits_));
      return false;
    }

    return true;
  }

  if (pkey_id == EVP_PKEY_EC) {
    // Extract the curve name.
    EC_KEY* ec = EVP_PKEY_get0_EC_KEY(public_key);
    if (!ec)
      return false;  // Unexpected.
    int curve_nid = EC_GROUP_get_curve_name(EC_KEY_get0_group(ec));

    if (!IsAcceptableCurveForEcdsa(curve_nid)) {
      errors->AddError(kUnacceptableCurveForEcdsa);
      return false;
    }

    return true;
  }

  // Unexpected key type.
  return false;
}

// Restricted signature digest algorithms to:
//
//    SHA1 (if digest_policy_ == kWeakAllowSha1)
//    SHA256
//    SHA384
//    SHA512
bool SimplePathBuilderDelegate::IsAcceptableDigest(
    DigestAlgorithm digest) const {
  switch (digest) {
    case DigestAlgorithm::Md2:
    case DigestAlgorithm::Md4:
    case DigestAlgorithm::Md5:
      return false;

    case DigestAlgorithm::Sha1:
      return digest_policy_ ==
             SimplePathBuilderDelegate::DigestPolicy::kWeakAllowSha1;
    case DigestAlgorithm::Sha256:
    case DigestAlgorithm::Sha384:
    case DigestAlgorithm::Sha512:
      return true;
  }

  return false;
}

}  // namespace net
