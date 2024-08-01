// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/webcrypto/algorithms/ec.h"

#include <stddef.h>

#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "components/webcrypto/algorithms/asymmetric_key_util.h"
#include "components/webcrypto/algorithms/util.h"
#include "components/webcrypto/blink_key_handle.h"
#include "components/webcrypto/generate_key_result.h"
#include "components/webcrypto/jwk.h"
#include "components/webcrypto/status.h"
#include "crypto/openssl_util.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"

namespace webcrypto {

namespace {

// Maps a blink::WebCryptoNamedCurve to the corresponding NID used by
// BoringSSL.
Status WebCryptoCurveToNid(blink::WebCryptoNamedCurve named_curve, int* nid) {
  switch (named_curve) {
    case blink::kWebCryptoNamedCurveP256:
      *nid = NID_X9_62_prime256v1;
      return Status::Success();
    case blink::kWebCryptoNamedCurveP384:
      *nid = NID_secp384r1;
      return Status::Success();
    case blink::kWebCryptoNamedCurveP521:
      *nid = NID_secp521r1;
      return Status::Success();
  }
  return Status::ErrorUnsupported();
}

// Maps a BoringSSL NID to the corresponding WebCrypto named curve.
Status NidToWebCryptoCurve(int nid, blink::WebCryptoNamedCurve* named_curve) {
  switch (nid) {
    case NID_X9_62_prime256v1:
      *named_curve = blink::kWebCryptoNamedCurveP256;
      return Status::Success();
    case NID_secp384r1:
      *named_curve = blink::kWebCryptoNamedCurveP384;
      return Status::Success();
    case NID_secp521r1:
      *named_curve = blink::kWebCryptoNamedCurveP521;
      return Status::Success();
  }
  return Status::ErrorImportedEcKeyIncorrectCurve();
}

struct JwkCrvMapping {
  const char* jwk_curve;
  blink::WebCryptoNamedCurve named_curve;
};

const JwkCrvMapping kJwkCrvMappings[] = {
    {"P-256", blink::kWebCryptoNamedCurveP256},
    {"P-384", blink::kWebCryptoNamedCurveP384},
    {"P-521", blink::kWebCryptoNamedCurveP521},
};

// Gets the "crv" parameter from a JWK and converts it to a WebCryptoNamedCurve.
Status ReadJwkCrv(const JwkReader& jwk,
                  blink::WebCryptoNamedCurve* named_curve) {
  std::string jwk_curve;
  Status status = jwk.GetString("crv", &jwk_curve);
  if (status.IsError())
    return status;

  for (const auto& mapping : kJwkCrvMappings) {
    if (mapping.jwk_curve == jwk_curve) {
      *named_curve = mapping.named_curve;
      return Status::Success();
    }
  }

  return Status::ErrorJwkIncorrectCrv();
}

// Converts a WebCryptoNamedCurve to an equivalent JWK "crv".
Status WebCryptoCurveToJwkCrv(blink::WebCryptoNamedCurve named_curve,
                              std::string* jwk_crv) {
  for (const auto& mapping : kJwkCrvMappings) {
    if (mapping.named_curve == named_curve) {
      *jwk_crv = mapping.jwk_curve;
      return Status::Success();
    }
  }
  return Status::ErrorUnexpected();
}

// Verifies that an EC key imported from PKCS8 or SPKI format is correct.
// This involves verifying the key validity, and the NID for the named curve.
// Also removes the EC_PKEY_NO_PUBKEY flag if present.
Status VerifyEcKeyAfterSpkiOrPkcs8Import(
    EVP_PKEY* pkey,
    blink::WebCryptoNamedCurve expected_named_curve) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  EC_KEY* ec = EVP_PKEY_get0_EC_KEY(pkey);
  if (!ec)
    return Status::ErrorUnexpected();

  // When importing an ECPrivateKey, the public key is optional. If it was
  // omitted then the public key will be calculated by BoringSSL and added into
  // the EC_KEY. However an encoding flag is set such that when exporting to
  // PKCS8 format the public key is once again omitted. Remove this flag.
  unsigned int enc_flags = EC_KEY_get_enc_flags(ec);
  enc_flags &= ~EC_PKEY_NO_PUBKEY;
  EC_KEY_set_enc_flags(ec, enc_flags);

  if (!EC_KEY_check_key(ec))
    return Status::ErrorEcKeyInvalid();

  // Make sure the curve matches the expected curve name.
  int curve_nid = EC_GROUP_get_curve_name(EC_KEY_get0_group(ec));
  blink::WebCryptoNamedCurve named_curve = blink::kWebCryptoNamedCurveP256;
  Status status = NidToWebCryptoCurve(curve_nid, &named_curve);
  if (status.IsError())
    return status;

  if (named_curve != expected_named_curve)
    return Status::ErrorImportedEcKeyIncorrectCurve();

  return Status::Success();
}

// Creates an EC_KEY for the given WebCryptoNamedCurve.
Status CreateEC_KEY(blink::WebCryptoNamedCurve named_curve,
                    bssl::UniquePtr<EC_KEY>* ec) {
  int curve_nid = 0;
  Status status = WebCryptoCurveToNid(named_curve, &curve_nid);
  if (status.IsError())
    return status;

  ec->reset(EC_KEY_new_by_curve_name(curve_nid));
  if (!ec->get())
    return Status::OperationError();

  return Status::Success();
}

// Writes an unsigned BIGNUM into |jwk|, zero-padding it to a length of
// |padded_length|.
Status WritePaddedBIGNUM(std::string_view member_name,
                         const BIGNUM* value,
                         size_t padded_length,
                         JwkWriter* jwk) {
  std::vector<uint8_t> padded_bytes(padded_length);
  if (!BN_bn2bin_padded(padded_bytes.data(), padded_bytes.size(), value))
    return Status::OperationError();
  jwk->SetBytes(member_name, padded_bytes);
  return Status::Success();
}

// Reads a fixed length BIGNUM from a JWK.
Status ReadPaddedBIGNUM(const JwkReader& jwk,
                        std::string_view member_name,
                        size_t expected_length,
                        bssl::UniquePtr<BIGNUM>* out) {
  std::vector<uint8_t> bytes;
  Status status = jwk.GetBytes(member_name, &bytes);
  if (status.IsError())
    return status;

  if (bytes.size() != expected_length) {
    return Status::JwkOctetStringWrongLength(member_name, expected_length,
                                             bytes.size());
  }

  out->reset(BN_bin2bn(bytes.data(), bytes.size(), nullptr));
  return Status::Success();
}

int GetGroupDegreeInBytes(EC_KEY* ec) {
  const EC_GROUP* group = EC_KEY_get0_group(ec);
  return NumBitsToBytes(EC_GROUP_get_degree(group));
}

// Extracts the public key as affine coordinates (x,y).
Status GetPublicKey(EC_KEY* ec,
                    bssl::UniquePtr<BIGNUM>* x,
                    bssl::UniquePtr<BIGNUM>* y) {
  const EC_GROUP* group = EC_KEY_get0_group(ec);
  const EC_POINT* point = EC_KEY_get0_public_key(ec);

  x->reset(BN_new());
  y->reset(BN_new());

  if (!EC_POINT_get_affine_coordinates_GFp(group, point, x->get(), y->get(),
                                           nullptr)) {
    return Status::OperationError();
  }

  return Status::Success();
}

// Synthesizes an import algorithm given a key algorithm, so that
// deserialization can re-use the ImportKey*() methods.
blink::WebCryptoAlgorithm SynthesizeImportAlgorithmForClone(
    const blink::WebCryptoKeyAlgorithm& algorithm) {
  return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(
      algorithm.Id(), new blink::WebCryptoEcKeyImportParams(
                          algorithm.EcParams()->NamedCurve()));
}

}  // namespace

Status EcAlgorithm::GenerateKey(const blink::WebCryptoAlgorithm& algorithm,
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

  const blink::WebCryptoEcKeyGenParams* params = algorithm.EcKeyGenParams();

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  // Generate an EC key pair.
  bssl::UniquePtr<EC_KEY> ec_private_key;
  status = CreateEC_KEY(params->NamedCurve(), &ec_private_key);
  if (status.IsError())
    return status;

  if (!EC_KEY_generate_key(ec_private_key.get()))
    return Status::OperationError();

  // Construct an EVP_PKEY for the private key.
  bssl::UniquePtr<EVP_PKEY> private_pkey(EVP_PKEY_new());
  if (!private_pkey ||
      !EVP_PKEY_set1_EC_KEY(private_pkey.get(), ec_private_key.get())) {
    return Status::OperationError();
  }

  // Construct an EVP_PKEY for just the public key.
  bssl::UniquePtr<EC_KEY> ec_public_key;
  bssl::UniquePtr<EVP_PKEY> public_pkey(EVP_PKEY_new());
  status = CreateEC_KEY(params->NamedCurve(), &ec_public_key);
  if (status.IsError())
    return status;
  if (!EC_KEY_set_public_key(ec_public_key.get(),
                             EC_KEY_get0_public_key(ec_private_key.get()))) {
    return Status::OperationError();
  }
  if (!public_pkey ||
      !EVP_PKEY_set1_EC_KEY(public_pkey.get(), ec_public_key.get())) {
    return Status::OperationError();
  }

  blink::WebCryptoKey public_key;
  blink::WebCryptoKey private_key;

  blink::WebCryptoKeyAlgorithm key_algorithm =
      blink::WebCryptoKeyAlgorithm::CreateEc(algorithm.Id(),
                                             params->NamedCurve());

  // Note that extractable is unconditionally set to true. This is because per
  // the WebCrypto spec generated public keys are always extractable.
  status = CreateWebCryptoPublicKey(std::move(public_pkey), key_algorithm, true,
                                    public_usages, &public_key);
  if (status.IsError())
    return status;

  status = CreateWebCryptoPrivateKey(std::move(private_pkey), key_algorithm,
                                     extractable, private_usages, &private_key);
  if (status.IsError())
    return status;

  result->AssignKeyPair(public_key, private_key);
  return Status::Success();
}

Status EcAlgorithm::ImportKey(blink::WebCryptoKeyFormat format,
                              base::span<const uint8_t> key_data,
                              const blink::WebCryptoAlgorithm& algorithm,
                              bool extractable,
                              blink::WebCryptoKeyUsageMask usages,
                              blink::WebCryptoKey* key) const {
  switch (format) {
    case blink::kWebCryptoKeyFormatRaw:
      return ImportKeyRaw(key_data, algorithm, extractable, usages, key);
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

Status EcAlgorithm::ExportKey(blink::WebCryptoKeyFormat format,
                              const blink::WebCryptoKey& key,
                              std::vector<uint8_t>* buffer) const {
  switch (format) {
    case blink::kWebCryptoKeyFormatRaw:
      return ExportKeyRaw(key, buffer);
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

Status EcAlgorithm::ImportKeyRaw(base::span<const uint8_t> key_data,
                                 const blink::WebCryptoAlgorithm& algorithm,
                                 bool extractable,
                                 blink::WebCryptoKeyUsageMask usages,
                                 blink::WebCryptoKey* key) const {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  Status status = CheckKeyCreationUsages(all_public_key_usages_, usages);
  if (status.IsError())
    return status;

  const blink::WebCryptoEcKeyImportParams* params =
      algorithm.EcKeyImportParams();

  // Create an EC_KEY.
  bssl::UniquePtr<EC_KEY> ec;
  status = CreateEC_KEY(params->NamedCurve(), &ec);
  if (status.IsError())
    return status;

  bssl::UniquePtr<EC_POINT> point(EC_POINT_new(EC_KEY_get0_group(ec.get())));
  if (!point.get())
    return Status::OperationError();

  // Convert the "raw" input from X9.62 format to an EC_POINT.
  if (!EC_POINT_oct2point(EC_KEY_get0_group(ec.get()), point.get(),
                          key_data.data(), key_data.size(), nullptr)) {
    return Status::DataError();
  }

  // Copy the point (public key) into the EC_KEY.
  if (!EC_KEY_set_public_key(ec.get(), point.get()))
    return Status::OperationError();

  // Verify the key.
  if (!EC_KEY_check_key(ec.get()))
    return Status::ErrorEcKeyInvalid();

  // Wrap the EC_KEY into an EVP_PKEY.
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new());
  if (!pkey || !EVP_PKEY_set1_EC_KEY(pkey.get(), ec.get()))
    return Status::OperationError();

  blink::WebCryptoKeyAlgorithm key_algorithm =
      blink::WebCryptoKeyAlgorithm::CreateEc(algorithm.Id(),
                                             params->NamedCurve());

  // Wrap the EVP_PKEY into a WebCryptoKey
  return CreateWebCryptoPublicKey(std::move(pkey), key_algorithm, extractable,
                                  usages, key);
}

Status EcAlgorithm::ImportKeyPkcs8(base::span<const uint8_t> key_data,
                                   const blink::WebCryptoAlgorithm& algorithm,
                                   bool extractable,
                                   blink::WebCryptoKeyUsageMask usages,
                                   blink::WebCryptoKey* key) const {
  Status status = CheckKeyCreationUsages(all_private_key_usages_, usages);
  if (status.IsError())
    return status;

  bssl::UniquePtr<EVP_PKEY> private_key;
  status = ImportUnverifiedPkeyFromPkcs8(key_data, EVP_PKEY_EC, &private_key);
  if (status.IsError())
    return status;

  const blink::WebCryptoEcKeyImportParams* params =
      algorithm.EcKeyImportParams();

  status = VerifyEcKeyAfterSpkiOrPkcs8Import(private_key.get(),
                                             params->NamedCurve());
  if (status.IsError())
    return status;

  return CreateWebCryptoPrivateKey(std::move(private_key),
                                   blink::WebCryptoKeyAlgorithm::CreateEc(
                                       algorithm.Id(), params->NamedCurve()),
                                   extractable, usages, key);
}

Status EcAlgorithm::ImportKeySpki(base::span<const uint8_t> key_data,
                                  const blink::WebCryptoAlgorithm& algorithm,
                                  bool extractable,
                                  blink::WebCryptoKeyUsageMask usages,
                                  blink::WebCryptoKey* key) const {
  Status status = CheckKeyCreationUsages(all_public_key_usages_, usages);
  if (status.IsError())
    return status;

  bssl::UniquePtr<EVP_PKEY> public_key;
  status = ImportUnverifiedPkeyFromSpki(key_data, EVP_PKEY_EC, &public_key);
  if (status.IsError())
    return status;

  const blink::WebCryptoEcKeyImportParams* params =
      algorithm.EcKeyImportParams();

  status =
      VerifyEcKeyAfterSpkiOrPkcs8Import(public_key.get(), params->NamedCurve());
  if (status.IsError())
    return status;

  return CreateWebCryptoPublicKey(std::move(public_key),
                                  blink::WebCryptoKeyAlgorithm::CreateEc(
                                      algorithm.Id(), params->NamedCurve()),
                                  extractable, usages, key);
}

// The format for JWK EC keys is given by:
// https://tools.ietf.org/html/draft-ietf-jose-json-web-algorithms-36#section-6.2
Status EcAlgorithm::ImportKeyJwk(base::span<const uint8_t> key_data,
                                 const blink::WebCryptoAlgorithm& algorithm,
                                 bool extractable,
                                 blink::WebCryptoKeyUsageMask usages,
                                 blink::WebCryptoKey* key) const {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  const blink::WebCryptoEcKeyImportParams* params =
      algorithm.EcKeyImportParams();

  // When importing EC keys from JWK there may be up to *three* separate curve
  // names:
  //
  //   (1) The one given to WebCrypto's importKey (params->namedCurve()).
  //   (2) JWK's "crv" member
  //   (3) A curve implied by JWK's "alg" member.
  //
  // (In the case of ECDSA, the "alg" member implicitly names a curve and hash)

  JwkReader jwk;
  Status status = jwk.Init(key_data, extractable, usages, "EC",
                           GetJwkAlgorithm(params->NamedCurve()));
  if (status.IsError())
    return status;

  // Verify that "crv" matches expected curve.
  blink::WebCryptoNamedCurve jwk_crv = blink::kWebCryptoNamedCurveP256;
  status = ReadJwkCrv(jwk, &jwk_crv);
  if (status.IsError())
    return status;
  if (jwk_crv != params->NamedCurve())
    return Status::ErrorJwkIncorrectCrv();

  // Only private keys have a "d" parameter. The key may still be invalid, but
  // tentatively decide if it is a public or private key.
  bool is_private_key = jwk.HasMember("d");

  // Now that the key type is known, verify the usages.
  if (is_private_key) {
    status = CheckKeyCreationUsages(all_private_key_usages_, usages);
  } else {
    status = CheckKeyCreationUsages(all_public_key_usages_, usages);
  }

  if (status.IsError())
    return status;

  // Create an EC_KEY.
  bssl::UniquePtr<EC_KEY> ec;
  status = CreateEC_KEY(params->NamedCurve(), &ec);
  if (status.IsError())
    return status;

  // JWK requires the length of x, y, d to match the group degree.
  int degree_bytes = GetGroupDegreeInBytes(ec.get());

  // Read the public key's uncompressed affine coordinates.
  bssl::UniquePtr<BIGNUM> x;
  status = ReadPaddedBIGNUM(jwk, "x", degree_bytes, &x);
  if (status.IsError())
    return status;

  bssl::UniquePtr<BIGNUM> y;
  status = ReadPaddedBIGNUM(jwk, "y", degree_bytes, &y);
  if (status.IsError())
    return status;

  // TODO(eroman): Distinguish more accurately between a DataError and
  // OperationError. In general if this fails it was due to the key being an
  // invalid EC key.
  if (!EC_KEY_set_public_key_affine_coordinates(ec.get(), x.get(), y.get()))
    return Status::DataError();

  // Extract the "d" parameters.
  if (is_private_key) {
    bssl::UniquePtr<BIGNUM> d;
    status = ReadPaddedBIGNUM(jwk, "d", degree_bytes, &d);
    if (status.IsError())
      return status;

    if (!EC_KEY_set_private_key(ec.get(), d.get()))
      return Status::OperationError();
  }

  // Verify the key.
  if (!EC_KEY_check_key(ec.get()))
    return Status::ErrorEcKeyInvalid();

  // Wrap the EC_KEY into an EVP_PKEY.
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new());
  if (!pkey || !EVP_PKEY_set1_EC_KEY(pkey.get(), ec.get()))
    return Status::OperationError();

  blink::WebCryptoKeyAlgorithm key_algorithm =
      blink::WebCryptoKeyAlgorithm::CreateEc(algorithm.Id(),
                                             params->NamedCurve());

  // Wrap the EVP_PKEY into a WebCryptoKey
  if (is_private_key) {
    return CreateWebCryptoPrivateKey(std::move(pkey), key_algorithm,
                                     extractable, usages, key);
  }
  return CreateWebCryptoPublicKey(std::move(pkey), key_algorithm, extractable,
                                  usages, key);
}

Status EcAlgorithm::ExportKeyRaw(const blink::WebCryptoKey& key,
                                 std::vector<uint8_t>* buffer) const {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  if (key.GetType() != blink::kWebCryptoKeyTypePublic)
    return Status::ErrorUnexpectedKeyType();

  EVP_PKEY* pkey = GetEVP_PKEY(key);

  EC_KEY* ec = EVP_PKEY_get0_EC_KEY(pkey);
  if (!ec)
    return Status::ErrorUnexpected();

  // Serialize the public key as an uncompressed point in X9.62 form.
  uint8_t* raw;
  size_t raw_len;
  bssl::ScopedCBB cbb;
  if (!CBB_init(cbb.get(), 0) ||
      !EC_POINT_point2cbb(cbb.get(), EC_KEY_get0_group(ec),
                          EC_KEY_get0_public_key(ec),
                          POINT_CONVERSION_UNCOMPRESSED, nullptr) ||
      !CBB_finish(cbb.get(), &raw, &raw_len)) {
    return Status::OperationError();
  }
  buffer->assign(raw, raw + raw_len);
  OPENSSL_free(raw);

  return Status::Success();
}

Status EcAlgorithm::ExportKeyPkcs8(const blink::WebCryptoKey& key,
                                   std::vector<uint8_t>* buffer) const {
  if (key.GetType() != blink::kWebCryptoKeyTypePrivate)
    return Status::ErrorUnexpectedKeyType();
  return ExportPKeyPkcs8(GetEVP_PKEY(key), buffer);
}

Status EcAlgorithm::ExportKeySpki(const blink::WebCryptoKey& key,
                                  std::vector<uint8_t>* buffer) const {
  if (key.GetType() != blink::kWebCryptoKeyTypePublic)
    return Status::ErrorUnexpectedKeyType();
  return ExportPKeySpki(GetEVP_PKEY(key), buffer);
}

// The format for JWK EC keys is given by:
// https://tools.ietf.org/html/draft-ietf-jose-json-web-algorithms-36#section-6.2
Status EcAlgorithm::ExportKeyJwk(const blink::WebCryptoKey& key,
                                 std::vector<uint8_t>* buffer) const {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  EVP_PKEY* pkey = GetEVP_PKEY(key);

  EC_KEY* ec = EVP_PKEY_get0_EC_KEY(pkey);
  if (!ec)
    return Status::ErrorUnexpected();

  // No "alg" is set for EC keys.
  JwkWriter jwk(std::string(), key.Extractable(), key.Usages(), "EC");

  // Set the crv
  std::string crv;
  Status status =
      WebCryptoCurveToJwkCrv(key.Algorithm().EcParams()->NamedCurve(), &crv);
  if (status.IsError())
    return status;

  int degree_bytes = GetGroupDegreeInBytes(ec);

  jwk.SetString("crv", crv);

  bssl::UniquePtr<BIGNUM> x;
  bssl::UniquePtr<BIGNUM> y;
  status = GetPublicKey(ec, &x, &y);
  if (status.IsError())
    return status;

  status = WritePaddedBIGNUM("x", x.get(), degree_bytes, &jwk);
  if (status.IsError())
    return status;

  status = WritePaddedBIGNUM("y", y.get(), degree_bytes, &jwk);
  if (status.IsError())
    return status;

  if (key.GetType() == blink::kWebCryptoKeyTypePrivate) {
    const BIGNUM* d = EC_KEY_get0_private_key(ec);
    status = WritePaddedBIGNUM("d", d, degree_bytes, &jwk);
    if (status.IsError())
      return status;
  }

  jwk.ToJson(buffer);
  return Status::Success();
}

// TODO(eroman): Defer import to the crypto thread. http://crbug.com/430763
Status EcAlgorithm::DeserializeKeyForClone(
    const blink::WebCryptoKeyAlgorithm& algorithm,
    blink::WebCryptoKeyType type,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    base::span<const uint8_t> key_data,
    blink::WebCryptoKey* key) const {
  if (algorithm.ParamsType() != blink::kWebCryptoKeyAlgorithmParamsTypeEc)
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

  if (type != key->GetType())
    return Status::ErrorUnexpected();

  if (algorithm.EcParams()->NamedCurve() !=
      key->Algorithm().EcParams()->NamedCurve()) {
    return Status::ErrorUnexpected();
  }

  return Status::Success();
}

}  // namespace webcrypto
