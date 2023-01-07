// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/pki/verify_signed_data.h"

#include "crypto/openssl_util.h"
#include "net/cert/pki/cert_errors.h"
#include "net/cert/pki/signature_algorithm.h"
#include "net/der/input.h"
#include "net/der/parse_values.h"
#include "net/der/parser.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

namespace net {

// Parses an RSA public key or EC public key from SPKI to an EVP_PKEY. Returns
// true on success.
//
// This function only recognizes the "pk-rsa" (rsaEncryption) flavor of RSA
// public key from RFC 5912.
//
//     pk-rsa PUBLIC-KEY ::= {
//      IDENTIFIER rsaEncryption
//      KEY RSAPublicKey
//      PARAMS TYPE NULL ARE absent
//      -- Private key format not in this module --
//      CERT-KEY-USAGE {digitalSignature, nonRepudiation,
//      keyEncipherment, dataEncipherment, keyCertSign, cRLSign}
//     }
//
// COMPATIBILITY NOTE: RFC 5912 and RFC 3279 are in disagreement on the value
// of parameters for rsaEncryption. Whereas RFC 5912 says they must be absent,
// RFC 3279 says they must be NULL:
//
//     The rsaEncryption OID is intended to be used in the algorithm field
//     of a value of type AlgorithmIdentifier.  The parameters field MUST
//     have ASN.1 type NULL for this algorithm identifier.
//
// Following RFC 3279 in this case.
//
// In the case of parsing EC keys, RFC 5912 describes all the ECDSA
// signature algorithms as requiring a public key of type "pk-ec":
//
//     pk-ec PUBLIC-KEY ::= {
//      IDENTIFIER id-ecPublicKey
//      KEY ECPoint
//      PARAMS TYPE ECParameters ARE required
//      -- Private key format not in this module --
//      CERT-KEY-USAGE { digitalSignature, nonRepudiation, keyAgreement,
//                           keyCertSign, cRLSign }
//     }
//
// Moreover RFC 5912 stipulates what curves are allowed. The ECParameters
// MUST NOT use an implicitCurve or specificCurve for PKIX:
//
//     ECParameters ::= CHOICE {
//      namedCurve      CURVE.&id({NamedCurve})
//      -- implicitCurve   NULL
//        -- implicitCurve MUST NOT be used in PKIX
//      -- specifiedCurve  SpecifiedCurve
//        -- specifiedCurve MUST NOT be used in PKIX
//        -- Details for specifiedCurve can be found in [X9.62]
//        -- Any future additions to this CHOICE should be coordinated
//        -- with ANSI X.9.
//     }
//     -- If you need to be able to decode ANSI X.9 parameter structures,
//     -- uncomment the implicitCurve and specifiedCurve above, and also
//     -- uncomment the following:
//     --(WITH COMPONENTS {namedCurve PRESENT})
//
// The namedCurves are extensible. The ones described by RFC 5912 are:
//
//     NamedCurve CURVE ::= {
//     { ID secp192r1 } | { ID sect163k1 } | { ID sect163r2 } |
//     { ID secp224r1 } | { ID sect233k1 } | { ID sect233r1 } |
//     { ID secp256r1 } | { ID sect283k1 } | { ID sect283r1 } |
//     { ID secp384r1 } | { ID sect409k1 } | { ID sect409r1 } |
//     { ID secp521r1 } | { ID sect571k1 } | { ID sect571r1 },
//     ... -- Extensible
//     }
bool ParsePublicKey(const der::Input& public_key_spki,
                    bssl::UniquePtr<EVP_PKEY>* public_key) {
  // Parse the SPKI to an EVP_PKEY.
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  CBS cbs;
  CBS_init(&cbs, public_key_spki.UnsafeData(), public_key_spki.Length());
  public_key->reset(EVP_parse_public_key(&cbs));
  if (!*public_key || CBS_len(&cbs) != 0) {
    public_key->reset();
    return false;
  }
  return true;
}

bool VerifySignedData(SignatureAlgorithm algorithm,
                      const der::Input& signed_data,
                      const der::BitString& signature_value,
                      EVP_PKEY* public_key) {
  int expected_pkey_id = 1;
  const EVP_MD* digest = nullptr;
  bool is_rsa_pss = false;
  switch (algorithm) {
    case SignatureAlgorithm::kRsaPkcs1Sha1:
      expected_pkey_id = EVP_PKEY_RSA;
      digest = EVP_sha1();
      break;
    case SignatureAlgorithm::kRsaPkcs1Sha256:
      expected_pkey_id = EVP_PKEY_RSA;
      digest = EVP_sha256();
      break;
    case SignatureAlgorithm::kRsaPkcs1Sha384:
      expected_pkey_id = EVP_PKEY_RSA;
      digest = EVP_sha384();
      break;
    case SignatureAlgorithm::kRsaPkcs1Sha512:
      expected_pkey_id = EVP_PKEY_RSA;
      digest = EVP_sha512();
      break;

    case SignatureAlgorithm::kEcdsaSha1:
      expected_pkey_id = EVP_PKEY_EC;
      digest = EVP_sha1();
      break;
    case SignatureAlgorithm::kEcdsaSha256:
      expected_pkey_id = EVP_PKEY_EC;
      digest = EVP_sha256();
      break;
    case SignatureAlgorithm::kEcdsaSha384:
      expected_pkey_id = EVP_PKEY_EC;
      digest = EVP_sha384();
      break;
    case SignatureAlgorithm::kEcdsaSha512:
      expected_pkey_id = EVP_PKEY_EC;
      digest = EVP_sha512();
      break;

    case SignatureAlgorithm::kRsaPssSha256:
      expected_pkey_id = EVP_PKEY_RSA;
      digest = EVP_sha256();
      is_rsa_pss = true;
      break;
    case SignatureAlgorithm::kRsaPssSha384:
      expected_pkey_id = EVP_PKEY_RSA;
      digest = EVP_sha384();
      is_rsa_pss = true;
      break;
    case SignatureAlgorithm::kRsaPssSha512:
      expected_pkey_id = EVP_PKEY_RSA;
      digest = EVP_sha512();
      is_rsa_pss = true;
      break;
  }

  if (expected_pkey_id != EVP_PKEY_id(public_key))
    return false;

  // For the supported algorithms the signature value must be a whole
  // number of bytes.
  if (signature_value.unused_bits() != 0)
    return false;
  const der::Input& signature_value_bytes = signature_value.bytes();

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  bssl::ScopedEVP_MD_CTX ctx;
  EVP_PKEY_CTX* pctx = nullptr;  // Owned by |ctx|.

  if (!EVP_DigestVerifyInit(ctx.get(), &pctx, digest, nullptr, public_key))
    return false;

  if (is_rsa_pss) {
    // All supported RSASSA-PSS algorithms match signing and MGF-1 digest. They
    // also use the digest length as the salt length, which is specified with -1
    // in OpenSSL's API.
    if (!EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_PSS_PADDING) ||
        !EVP_PKEY_CTX_set_rsa_pss_saltlen(pctx, -1)) {
      return false;
    }
  }

  if (!EVP_DigestVerifyUpdate(ctx.get(), signed_data.UnsafeData(),
                              signed_data.Length())) {
    return false;
  }

  return 1 == EVP_DigestVerifyFinal(ctx.get(),
                                    signature_value_bytes.UnsafeData(),
                                    signature_value_bytes.Length());
}

bool VerifySignedData(SignatureAlgorithm algorithm,
                      const der::Input& signed_data,
                      const der::BitString& signature_value,
                      const der::Input& public_key_spki) {
  bssl::UniquePtr<EVP_PKEY> public_key;
  if (!ParsePublicKey(public_key_spki, &public_key))
    return false;
  return VerifySignedData(algorithm, signed_data, signature_value,
                          public_key.get());
}

}  // namespace net
