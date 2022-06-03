// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/verify_signed_data.h"

#include "base/compiler_specific.h"
#include "base/numerics/safe_math.h"
#include "crypto/openssl_util.h"
#include "net/cert/internal/cert_errors.h"
#include "net/cert/internal/signature_algorithm.h"
#include "net/der/input.h"
#include "net/der/parse_values.h"
#include "net/der/parser.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

namespace net {

namespace {

// Converts a DigestAlgorithm to an equivalent EVP_MD*.
WARN_UNUSED_RESULT bool GetDigest(DigestAlgorithm digest, const EVP_MD** out) {
  *out = nullptr;

  switch (digest) {
    case DigestAlgorithm::Md2:
    case DigestAlgorithm::Md4:
    case DigestAlgorithm::Md5:
      // Unsupported.
      break;
    case DigestAlgorithm::Sha1:
      *out = EVP_sha1();
      break;
    case DigestAlgorithm::Sha256:
      *out = EVP_sha256();
      break;
    case DigestAlgorithm::Sha384:
      *out = EVP_sha384();
      break;
    case DigestAlgorithm::Sha512:
      *out = EVP_sha512();
      break;
  }

  return *out != nullptr;
}

// Sets the RSASSA-PSS parameters on |pctx|. Returns true on success.
WARN_UNUSED_RESULT bool ApplyRsaPssOptions(const RsaPssParameters* params,
                                           EVP_PKEY_CTX* pctx) {
  // BoringSSL takes a signed int for the salt length, and interprets
  // negative values in a special manner. Make sure not to silently underflow.
  base::CheckedNumeric<int> salt_length_bytes_int(params->salt_length());
  if (!salt_length_bytes_int.IsValid())
    return false;

  const EVP_MD* mgf1_hash;
  if (!GetDigest(params->mgf1_hash(), &mgf1_hash))
    return false;

  return EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_PSS_PADDING) &&
         EVP_PKEY_CTX_set_rsa_mgf1_md(pctx, mgf1_hash) &&
         EVP_PKEY_CTX_set_rsa_pss_saltlen(pctx,
                                          salt_length_bytes_int.ValueOrDie());
}

}  // namespace

// Parses an RSA public key or EC public key from SPKI to an EVP_PKEY. Returns
// true on success.
//
// There are two flavors of RSA public key that this function should recognize
// from RFC 5912 (however note that pk-rsaSSA-PSS is not supported in the
// current implementation).
// TODO(eroman): Support id-RSASSA-PSS and its associated parameters. See
// https://crbug.com/522232
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
//  ...
//
//     pk-rsaSSA-PSS PUBLIC-KEY ::= {
//         IDENTIFIER id-RSASSA-PSS
//         KEY RSAPublicKey
//         PARAMS TYPE RSASSA-PSS-params ARE optional
//          -- Private key format not in this module --
//         CERT-KEY-USAGE { nonRepudiation, digitalSignature,
//                              keyCertSign, cRLSign }
//     }
//
// Any RSA signature algorithm can accept a "pk-rsa" (rsaEncryption). However a
// "pk-rsaSSA-PSS" key is only accepted if the signature algorithm was for PSS
// mode:
//
//     sa-rsaSSA-PSS SIGNATURE-ALGORITHM ::= {
//         IDENTIFIER id-RSASSA-PSS
//         PARAMS TYPE RSASSA-PSS-params ARE required
//         HASHES { mda-sha1 | mda-sha224 | mda-sha256 | mda-sha384
//                      | mda-sha512 }
//         PUBLIC-KEYS { pk-rsa | pk-rsaSSA-PSS }
//         SMIME-CAPS { IDENTIFIED BY id-RSASSA-PSS }
//     }
//
// Moreover, if a "pk-rsaSSA-PSS" key was used and it optionally provided
// parameters for the algorithm, they must match those of the signature
// algorithm.
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

  // TODO(eroman): This is not strict enough. It accepts BER, other RSA
  // OIDs, and does not check id-rsaEncryption parameters.
  // See https://crbug.com/522228 and https://crbug.com/522232
  CBS cbs;
  CBS_init(&cbs, public_key_spki.UnsafeData(), public_key_spki.Length());
  public_key->reset(EVP_parse_public_key(&cbs));
  if (!*public_key || CBS_len(&cbs) != 0) {
    public_key->reset();
    return false;
  }
  return true;
}

bool VerifySignedData(const SignatureAlgorithm& algorithm,
                      const der::Input& signed_data,
                      const der::BitString& signature_value,
                      EVP_PKEY* public_key) {
  // Check that the key type matches the signature algorithm.
  int expected_pkey_id = -1;
  switch (algorithm.algorithm()) {
    case SignatureAlgorithmId::Dsa:
      // DSA is not supported.
      return false;
    case SignatureAlgorithmId::RsaPkcs1:
    case SignatureAlgorithmId::RsaPss:
      expected_pkey_id = EVP_PKEY_RSA;
      break;
    case SignatureAlgorithmId::Ecdsa:
      expected_pkey_id = EVP_PKEY_EC;
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

  const EVP_MD* digest;
  if (!GetDigest(algorithm.digest(), &digest))
    return false;

  if (!EVP_DigestVerifyInit(ctx.get(), &pctx, digest, nullptr, public_key))
    return false;

  // Set the RSASSA-PSS specific options.
  if (algorithm.algorithm() == SignatureAlgorithmId::RsaPss &&
      !ApplyRsaPssOptions(algorithm.ParamsForRsaPss(), pctx)) {
    return false;
  }

  if (!EVP_DigestVerifyUpdate(ctx.get(), signed_data.UnsafeData(),
                              signed_data.Length())) {
    return false;
  }

  return 1 == EVP_DigestVerifyFinal(ctx.get(),
                                    signature_value_bytes.UnsafeData(),
                                    signature_value_bytes.Length());
}

bool VerifySignedData(const SignatureAlgorithm& algorithm,
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
