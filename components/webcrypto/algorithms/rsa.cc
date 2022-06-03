// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/algorithms/rsa.h"

#include <utility>

#include "base/check_op.h"
#include "components/webcrypto/algorithms/asymmetric_key_util.h"
#include "components/webcrypto/algorithms/util.h"
#include "components/webcrypto/blink_key_handle.h"
#include "components/webcrypto/crypto_data.h"
#include "components/webcrypto/generate_key_result.h"
#include "components/webcrypto/jwk.h"
#include "components/webcrypto/status.h"
#include "crypto/openssl_util.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

namespace webcrypto {

namespace {

// Describes the RSA components for a parsed key. The names of the properties
// correspond with those from the JWK spec. Note that Chromium's WebCrypto
// implementation does not support multi-primes, so there is no parsed field
// for "oth".
struct JwkRsaInfo {
  bool is_private_key = false;
  std::string n;
  std::string e;
  std::string d;
  std::string p;
  std::string q;
  std::string dp;
  std::string dq;
  std::string qi;
};

// Parses a UTF-8 encoded JWK (key_data), and extracts the RSA components to
// |*result|. Returns Status::Success() on success, otherwise an error.
// In order for this to succeed:
//   * expected_alg must match the JWK's "alg", if present.
//   * expected_extractable must be consistent with the JWK's "ext", if
//     present.
//   * expected_usages must be a subset of the JWK's "key_ops" if present.
Status ReadRsaKeyJwk(const CryptoData& key_data,
                     const std::string& expected_alg,
                     bool expected_extractable,
                     blink::WebCryptoKeyUsageMask expected_usages,
                     JwkRsaInfo* result) {
  JwkReader jwk;
  Status status = jwk.Init(key_data, expected_extractable, expected_usages,
                           "RSA", expected_alg);
  if (status.IsError())
    return status;

  // An RSA public key must have an "n" (modulus) and an "e" (exponent) entry
  // in the JWK, while an RSA private key must have those, plus at least a "d"
  // (private exponent) entry.
  // See http://tools.ietf.org/html/draft-ietf-jose-json-web-algorithms-18,
  // section 6.3.
  status = jwk.GetBigInteger("n", &result->n);
  if (status.IsError())
    return status;
  status = jwk.GetBigInteger("e", &result->e);
  if (status.IsError())
    return status;

  result->is_private_key = jwk.HasMember("d");
  if (!result->is_private_key)
    return Status::Success();

  status = jwk.GetBigInteger("d", &result->d);
  if (status.IsError())
    return status;

  // The "p", "q", "dp", "dq", and "qi" properties are optional in the JWA
  // spec. However they are required by Chromium's WebCrypto implementation.

  status = jwk.GetBigInteger("p", &result->p);
  if (status.IsError())
    return status;

  status = jwk.GetBigInteger("q", &result->q);
  if (status.IsError())
    return status;

  status = jwk.GetBigInteger("dp", &result->dp);
  if (status.IsError())
    return status;

  status = jwk.GetBigInteger("dq", &result->dq);
  if (status.IsError())
    return status;

  status = jwk.GetBigInteger("qi", &result->qi);
  if (status.IsError())
    return status;

  return Status::Success();
}

// Creates a blink::WebCryptoAlgorithm having the modulus length and public
// exponent  of |key|.
Status CreateRsaHashedKeyAlgorithm(
    blink::WebCryptoAlgorithmId rsa_algorithm,
    blink::WebCryptoAlgorithmId hash_algorithm,
    EVP_PKEY* key,
    blink::WebCryptoKeyAlgorithm* key_algorithm) {
  DCHECK_EQ(EVP_PKEY_RSA, EVP_PKEY_id(key));

  RSA* rsa = EVP_PKEY_get0_RSA(key);
  if (!rsa)
    return Status::ErrorUnexpected();

  unsigned int modulus_length_bits = BN_num_bits(rsa->n);

  // Convert the public exponent to big-endian representation.
  std::vector<uint8_t> e(BN_num_bytes(rsa->e));
  if (e.size() == 0)
    return Status::ErrorUnexpected();
  if (e.size() != BN_bn2bin(rsa->e, &e[0]))
    return Status::ErrorUnexpected();

  *key_algorithm = blink::WebCryptoKeyAlgorithm::CreateRsaHashed(
      rsa_algorithm, modulus_length_bits, &e[0],
      static_cast<unsigned int>(e.size()), hash_algorithm);

  return Status::Success();
}

// Creates a WebCryptoKey that wraps |private_key|.
Status CreateWebCryptoRsaPrivateKey(
    bssl::UniquePtr<EVP_PKEY> private_key,
    const blink::WebCryptoAlgorithmId rsa_algorithm_id,
    const blink::WebCryptoAlgorithm& hash,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) {
  blink::WebCryptoKeyAlgorithm key_algorithm;
  Status status = CreateRsaHashedKeyAlgorithm(
      rsa_algorithm_id, hash.Id(), private_key.get(), &key_algorithm);
  if (status.IsError())
    return status;

  return CreateWebCryptoPrivateKey(std::move(private_key), key_algorithm,
                                   extractable, usages, key);
}

// Creates a WebCryptoKey that wraps |public_key|.
Status CreateWebCryptoRsaPublicKey(
    bssl::UniquePtr<EVP_PKEY> public_key,
    const blink::WebCryptoAlgorithmId rsa_algorithm_id,
    const blink::WebCryptoAlgorithm& hash,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) {
  blink::WebCryptoKeyAlgorithm key_algorithm;
  Status status = CreateRsaHashedKeyAlgorithm(rsa_algorithm_id, hash.Id(),
                                              public_key.get(), &key_algorithm);
  if (status.IsError())
    return status;

  return CreateWebCryptoPublicKey(std::move(public_key), key_algorithm,
                                  extractable, usages, key);
}

Status ImportRsaPrivateKey(const blink::WebCryptoAlgorithm& algorithm,
                           bool extractable,
                           blink::WebCryptoKeyUsageMask usages,
                           const JwkRsaInfo& params,
                           blink::WebCryptoKey* key) {
  bssl::UniquePtr<RSA> rsa(RSA_new());

  rsa->n = CreateBIGNUM(params.n);
  rsa->e = CreateBIGNUM(params.e);
  rsa->d = CreateBIGNUM(params.d);
  rsa->p = CreateBIGNUM(params.p);
  rsa->q = CreateBIGNUM(params.q);
  rsa->dmp1 = CreateBIGNUM(params.dp);
  rsa->dmq1 = CreateBIGNUM(params.dq);
  rsa->iqmp = CreateBIGNUM(params.qi);

  if (!rsa->n || !rsa->e || !rsa->d || !rsa->p || !rsa->q || !rsa->dmp1 ||
      !rsa->dmq1 || !rsa->iqmp) {
    return Status::OperationError();
  }

  // TODO(eroman): This should be a DataError.
  if (!RSA_check_key(rsa.get()))
    return Status::OperationError();

  // Create a corresponding EVP_PKEY.
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new());
  if (!pkey || !EVP_PKEY_set1_RSA(pkey.get(), rsa.get()))
    return Status::OperationError();

  return CreateWebCryptoRsaPrivateKey(
      std::move(pkey), algorithm.Id(),
      algorithm.RsaHashedImportParams()->GetHash(), extractable, usages, key);
}

Status ImportRsaPublicKey(const blink::WebCryptoAlgorithm& algorithm,
                          bool extractable,
                          blink::WebCryptoKeyUsageMask usages,
                          const CryptoData& n,
                          const CryptoData& e,
                          blink::WebCryptoKey* key) {
  bssl::UniquePtr<RSA> rsa(RSA_new());

  rsa->n = BN_bin2bn(n.bytes(), n.byte_length(), nullptr);
  rsa->e = BN_bin2bn(e.bytes(), e.byte_length(), nullptr);

  if (!rsa->n || !rsa->e)
    return Status::OperationError();

  // Create a corresponding EVP_PKEY.
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new());
  if (!pkey || !EVP_PKEY_set1_RSA(pkey.get(), rsa.get()))
    return Status::OperationError();

  return CreateWebCryptoRsaPublicKey(
      std::move(pkey), algorithm.Id(),
      algorithm.RsaHashedImportParams()->GetHash(), extractable, usages, key);
}

// Converts a BIGNUM to a big endian byte array.
std::vector<uint8_t> BIGNUMToVector(const BIGNUM* n) {
  std::vector<uint8_t> v(BN_num_bytes(n));
  BN_bn2bin(n, v.data());
  return v;
}

// Synthesizes an import algorithm given a key algorithm, so that
// deserialization can re-use the ImportKey*() methods.
blink::WebCryptoAlgorithm SynthesizeImportAlgorithmForClone(
    const blink::WebCryptoKeyAlgorithm& algorithm) {
  return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(
      algorithm.Id(), new blink::WebCryptoRsaHashedImportParams(
                          algorithm.RsaHashedParams()->GetHash()));
}

}  // namespace

Status RsaHashedAlgorithm::GenerateKey(
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask combined_usages,
    GenerateKeyResult* result) const {
  blink::WebCryptoKeyUsageMask public_usages = 0;
  blink::WebCryptoKeyUsageMask private_usages = 0;

  Status status = GetUsagesForGenerateAsymmetricKey(
      combined_usages, all_public_key_usages_, all_private_key_usages_,
      &public_usages, &private_usages);
  if (status.IsError())
    return status;

  const blink::WebCryptoRsaHashedKeyGenParams* params =
      algorithm.RsaHashedKeyGenParams();

  unsigned int modulus_length_bits = params->ModulusLengthBits();

  // Limit the RSA key sizes to:
  //   * Multiple of 8 bits
  //   * 256 bits to 16K bits
  //
  // These correspond with limitations at the time there was an NSS WebCrypto
  // implementation. However in practice the upper bound is also helpful
  // because generating large RSA keys is very slow.
  if (modulus_length_bits < 256 || modulus_length_bits > 16384 ||
      (modulus_length_bits % 8) != 0) {
    return Status::ErrorGenerateRsaUnsupportedModulus();
  }

  unsigned int public_exponent = 0;
  if (!params->ConvertPublicExponentToUnsigned(public_exponent))
    return Status::ErrorGenerateKeyPublicExponent();

  // OpenSSL hangs when given bad public exponents. Use a whitelist.
  if (public_exponent != 3 && public_exponent != 65537)
    return Status::ErrorGenerateKeyPublicExponent();

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  // Generate an RSA key pair.
  bssl::UniquePtr<RSA> rsa_private_key(RSA_new());
  bssl::UniquePtr<BIGNUM> bn(BN_new());
  if (!rsa_private_key.get() || !bn.get() ||
      !BN_set_word(bn.get(), public_exponent)) {
    return Status::OperationError();
  }

  if (!RSA_generate_key_ex(rsa_private_key.get(), modulus_length_bits, bn.get(),
                           nullptr)) {
    return Status::OperationError();
  }

  // Construct an EVP_PKEY for the private key.
  bssl::UniquePtr<EVP_PKEY> private_pkey(EVP_PKEY_new());
  if (!private_pkey ||
      !EVP_PKEY_set1_RSA(private_pkey.get(), rsa_private_key.get())) {
    return Status::OperationError();
  }

  // Construct an EVP_PKEY for the public key.
  bssl::UniquePtr<RSA> rsa_public_key(RSAPublicKey_dup(rsa_private_key.get()));
  bssl::UniquePtr<EVP_PKEY> public_pkey(EVP_PKEY_new());
  if (!public_pkey ||
      !EVP_PKEY_set1_RSA(public_pkey.get(), rsa_public_key.get())) {
    return Status::OperationError();
  }

  blink::WebCryptoKey public_key;
  blink::WebCryptoKey private_key;

  // Note that extractable is unconditionally set to true. This is because per
  // the WebCrypto spec generated public keys are always extractable.
  status = CreateWebCryptoRsaPublicKey(std::move(public_pkey), algorithm.Id(),
                                       params->GetHash(), true, public_usages,
                                       &public_key);
  if (status.IsError())
    return status;

  status = CreateWebCryptoRsaPrivateKey(std::move(private_pkey), algorithm.Id(),
                                        params->GetHash(), extractable,
                                        private_usages, &private_key);
  if (status.IsError())
    return status;

  result->AssignKeyPair(public_key, private_key);
  return Status::Success();
}

Status RsaHashedAlgorithm::ImportKey(blink::WebCryptoKeyFormat format,
                                     const CryptoData& key_data,
                                     const blink::WebCryptoAlgorithm& algorithm,
                                     bool extractable,
                                     blink::WebCryptoKeyUsageMask usages,
                                     blink::WebCryptoKey* key) const {
  switch (format) {
    case blink::kWebCryptoKeyFormatPkcs8:
      return ImportKeyPkcs8(key_data, algorithm, extractable, usages, key);
    case blink::kWebCryptoKeyFormatSpki:
      return ImportKeySpki(key_data, algorithm, extractable, usages, key);
    case blink::kWebCryptoKeyFormatJwk:
      return ImportKeyJwk(key_data, algorithm, extractable, usages, key);
    default:
      return Status::ErrorUnsupportedImportKeyFormat();
  }
}

Status RsaHashedAlgorithm::ExportKey(blink::WebCryptoKeyFormat format,
                                     const blink::WebCryptoKey& key,
                                     std::vector<uint8_t>* buffer) const {
  switch (format) {
    case blink::kWebCryptoKeyFormatPkcs8:
      return ExportKeyPkcs8(key, buffer);
    case blink::kWebCryptoKeyFormatSpki:
      return ExportKeySpki(key, buffer);
    case blink::kWebCryptoKeyFormatJwk:
      return ExportKeyJwk(key, buffer);
    default:
      return Status::ErrorUnsupportedExportKeyFormat();
  }
}

Status RsaHashedAlgorithm::ImportKeyPkcs8(
    const CryptoData& key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) const {
  Status status = CheckKeyCreationUsages(all_private_key_usages_, usages);
  if (status.IsError())
    return status;

  bssl::UniquePtr<EVP_PKEY> private_key;
  status = ImportUnverifiedPkeyFromPkcs8(key_data, EVP_PKEY_RSA, &private_key);
  if (status.IsError())
    return status;

  // Verify the parameters of the key.
  RSA* rsa = EVP_PKEY_get0_RSA(private_key.get());
  if (!rsa)
    return Status::ErrorUnexpected();
  if (!RSA_check_key(rsa))
    return Status::DataError();

  // TODO(eroman): Validate the algorithm OID against the webcrypto provided
  // hash. http://crbug.com/389400

  return CreateWebCryptoRsaPrivateKey(
      std::move(private_key), algorithm.Id(),
      algorithm.RsaHashedImportParams()->GetHash(), extractable, usages, key);
}

Status RsaHashedAlgorithm::ImportKeySpki(
    const CryptoData& key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) const {
  Status status = CheckKeyCreationUsages(all_public_key_usages_, usages);
  if (status.IsError())
    return status;

  bssl::UniquePtr<EVP_PKEY> public_key;
  status = ImportUnverifiedPkeyFromSpki(key_data, EVP_PKEY_RSA, &public_key);
  if (status.IsError())
    return status;

  // TODO(eroman): Validate the algorithm OID against the webcrypto provided
  // hash. http://crbug.com/389400

  return CreateWebCryptoRsaPublicKey(
      std::move(public_key), algorithm.Id(),
      algorithm.RsaHashedImportParams()->GetHash(), extractable, usages, key);
}

Status RsaHashedAlgorithm::ImportKeyJwk(
    const CryptoData& key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) const {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  const char* jwk_algorithm =
      GetJwkAlgorithm(algorithm.RsaHashedImportParams()->GetHash().Id());

  if (!jwk_algorithm)
    return Status::ErrorUnexpected();

  JwkRsaInfo jwk;
  Status status =
      ReadRsaKeyJwk(key_data, jwk_algorithm, extractable, usages, &jwk);
  if (status.IsError())
    return status;

  // Once the key type is known, verify the usages.
  if (jwk.is_private_key) {
    status = CheckKeyCreationUsages(all_private_key_usages_, usages);
  } else {
    status = CheckKeyCreationUsages(all_public_key_usages_, usages);
  }

  if (status.IsError())
    return status;

  return jwk.is_private_key
             ? ImportRsaPrivateKey(algorithm, extractable, usages, jwk, key)
             : ImportRsaPublicKey(algorithm, extractable, usages,
                                  CryptoData(jwk.n), CryptoData(jwk.e), key);
}

Status RsaHashedAlgorithm::ExportKeyPkcs8(const blink::WebCryptoKey& key,
                                          std::vector<uint8_t>* buffer) const {
  if (key.GetType() != blink::kWebCryptoKeyTypePrivate)
    return Status::ErrorUnexpectedKeyType();
  // This relies on the fact that PKCS8 formatted data was already
  // associated with the key during its creation (used by
  // structured clone).
  *buffer = GetSerializedKeyData(key);
  return Status::Success();
}

Status RsaHashedAlgorithm::ExportKeySpki(const blink::WebCryptoKey& key,
                                         std::vector<uint8_t>* buffer) const {
  if (key.GetType() != blink::kWebCryptoKeyTypePublic)
    return Status::ErrorUnexpectedKeyType();
  // This relies on the fact that SPKI formatted data was already
  // associated with the key during its creation (used by
  // structured clone).
  *buffer = GetSerializedKeyData(key);
  return Status::Success();
}

Status RsaHashedAlgorithm::ExportKeyJwk(const blink::WebCryptoKey& key,
                                        std::vector<uint8_t>* buffer) const {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  EVP_PKEY* pkey = GetEVP_PKEY(key);
  RSA* rsa = EVP_PKEY_get0_RSA(pkey);
  if (!rsa)
    return Status::ErrorUnexpected();

  const char* jwk_algorithm =
      GetJwkAlgorithm(key.Algorithm().RsaHashedParams()->GetHash().Id());
  if (!jwk_algorithm)
    return Status::ErrorUnexpected();

  switch (key.GetType()) {
    case blink::kWebCryptoKeyTypePublic: {
      JwkWriter writer(jwk_algorithm, key.Extractable(), key.Usages(), "RSA");
      writer.SetBytes("n", CryptoData(BIGNUMToVector(rsa->n)));
      writer.SetBytes("e", CryptoData(BIGNUMToVector(rsa->e)));
      writer.ToJson(buffer);
      return Status::Success();
    }
    case blink::kWebCryptoKeyTypePrivate: {
      JwkWriter writer(jwk_algorithm, key.Extractable(), key.Usages(), "RSA");
      writer.SetBytes("n", CryptoData(BIGNUMToVector(rsa->n)));
      writer.SetBytes("e", CryptoData(BIGNUMToVector(rsa->e)));
      writer.SetBytes("d", CryptoData(BIGNUMToVector(rsa->d)));
      // Although these are "optional" in the JWA, WebCrypto spec requires them
      // to be emitted.
      writer.SetBytes("p", CryptoData(BIGNUMToVector(rsa->p)));
      writer.SetBytes("q", CryptoData(BIGNUMToVector(rsa->q)));
      writer.SetBytes("dp", CryptoData(BIGNUMToVector(rsa->dmp1)));
      writer.SetBytes("dq", CryptoData(BIGNUMToVector(rsa->dmq1)));
      writer.SetBytes("qi", CryptoData(BIGNUMToVector(rsa->iqmp)));
      writer.ToJson(buffer);
      return Status::Success();
    }

    default:
      return Status::ErrorUnexpected();
  }
}

// TODO(eroman): Defer import to the crypto thread. http://crbug.com/430763
Status RsaHashedAlgorithm::DeserializeKeyForClone(
    const blink::WebCryptoKeyAlgorithm& algorithm,
    blink::WebCryptoKeyType type,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    const CryptoData& key_data,
    blink::WebCryptoKey* key) const {
  if (algorithm.ParamsType() !=
      blink::kWebCryptoKeyAlgorithmParamsTypeRsaHashed)
    return Status::ErrorUnexpected();

  blink::WebCryptoAlgorithm import_algorithm =
      SynthesizeImportAlgorithmForClone(algorithm);

  Status status;

  // The serialized data will be either SPKI or PKCS8 formatted.
  switch (type) {
    case blink::kWebCryptoKeyTypePublic:
      status =
          ImportKeySpki(key_data, import_algorithm, extractable, usages, key);
      break;
    case blink::kWebCryptoKeyTypePrivate:
      status =
          ImportKeyPkcs8(key_data, import_algorithm, extractable, usages, key);
      break;
    default:
      return Status::ErrorUnexpected();
  }

  if (!status.IsSuccess())
    return status;

  // There is some duplicated information in the serialized format used by
  // structured clone (since the KeyAlgorithm is serialized separately from the
  // key data). Use this extra information to further validate what was
  // deserialized from the key data.

  if (algorithm.Id() != key->Algorithm().Id())
    return Status::ErrorUnexpected();

  if (key->GetType() != type)
    return Status::ErrorUnexpected();

  if (algorithm.RsaHashedParams()->ModulusLengthBits() !=
      key->Algorithm().RsaHashedParams()->ModulusLengthBits()) {
    return Status::ErrorUnexpected();
  }

  if (algorithm.RsaHashedParams()->PublicExponent().size() !=
          key->Algorithm().RsaHashedParams()->PublicExponent().size() ||
      0 !=
          memcmp(algorithm.RsaHashedParams()->PublicExponent().Data(),
                 key->Algorithm().RsaHashedParams()->PublicExponent().Data(),
                 key->Algorithm().RsaHashedParams()->PublicExponent().size())) {
    return Status::ErrorUnexpected();
  }

  return Status::Success();
}

}  // namespace webcrypto
