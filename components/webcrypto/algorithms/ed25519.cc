// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/algorithms/ed25519.h"

#include "components/webcrypto/algorithms/asymmetric_key_util.h"
#include "components/webcrypto/algorithms/util.h"
#include "components/webcrypto/blink_key_handle.h"
#include "components/webcrypto/generate_key_result.h"
#include "components/webcrypto/status.h"
#include "crypto/openssl_util.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace webcrypto {

namespace {
// Synthesizes an import algorithm given a key algorithm, so that
// deserialization can re-use the ImportKey*() methods.
blink::WebCryptoAlgorithm SynthesizeImportAlgorithmForClone(
    const blink::WebCryptoKeyAlgorithm& algorithm) {
  return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(algorithm.Id(),
                                                         nullptr);
}
}  // namespace

Status Ed25519Implementation::GenerateKey(
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

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  // Generate an Ed25519 key pair using the low-level API.
  uint8_t pubkey[32], privkey[64];
  ED25519_keypair(pubkey, privkey);

  // Since the RFC 8032 private key format is the 32-byte prefix of
  // |ED25519_sign|'s 64-byte private key, we can be sure we'll use the same
  // seed when regenerating the public key.
  // TODO(https://crbug.com/boringssl/521): This does a redundant base point
  // multiplication, but there aren't EVP APIs to avoid it without a lot of
  // boilerplate.
  bssl::UniquePtr<EVP_PKEY> private_pkey(EVP_PKEY_new_raw_private_key(
      EVP_PKEY_ED25519, /*engine*/ nullptr, privkey, 32));
  if (!private_pkey)
    return Status::OperationError();

  bssl::UniquePtr<EVP_PKEY> public_pkey(EVP_PKEY_new_raw_public_key(
      EVP_PKEY_ED25519, /*engine*/ nullptr, pubkey, 32));
  if (!public_pkey)
    return Status::OperationError();

  // Ed25519 algorithm doesn't need params.
  // https://wicg.github.io/webcrypto-secure-curves/#ed25519-registration
  blink::WebCryptoKeyAlgorithm key_algorithm =
      blink::WebCryptoKeyAlgorithm::CreateEd25519(algorithm.Id());

  // Note that extractable is unconditionally set to true. This is because per
  // the WebCrypto spec generated public keys are always extractable.
  blink::WebCryptoKey public_key;
  status = CreateWebCryptoPublicKey(std::move(public_pkey), key_algorithm,
                                    /*extractable*/ true, public_usages,
                                    &public_key);
  if (status.IsError())
    return status;

  blink::WebCryptoKey private_key;
  status = CreateWebCryptoPrivateKey(std::move(private_pkey), key_algorithm,
                                     extractable, private_usages, &private_key);
  if (status.IsError())
    return status;

  result->AssignKeyPair(public_key, private_key);
  return Status::Success();
}

Status Ed25519Implementation::ImportKey(
    blink::WebCryptoKeyFormat format,
    base::span<const uint8_t> key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) const {
  switch (format) {
    case blink::kWebCryptoKeyFormatPkcs8:
      return ImportKeyPkcs8(key_data, algorithm, extractable, usages, key);
    case blink::kWebCryptoKeyFormatSpki:
      return ImportKeySpki(key_data, algorithm, extractable, usages, key);
    default:
      return Status::ErrorUnsupportedImportKeyFormat();
  }
}

Status Ed25519Implementation::ExportKey(blink::WebCryptoKeyFormat format,
                                        const blink::WebCryptoKey& key,
                                        std::vector<uint8_t>* buffer) const {
  switch (format) {
    case blink::kWebCryptoKeyFormatPkcs8:
      return ExportKeyPkcs8(key, buffer);
    case blink::kWebCryptoKeyFormatSpki:
      return ExportKeySpki(key, buffer);
    default:
      return Status::ErrorUnsupportedExportKeyFormat();
  }
}

Status Ed25519Implementation::Sign(const blink::WebCryptoAlgorithm& algorithm,
                                   const blink::WebCryptoKey& key,
                                   base::span<const uint8_t> message,
                                   std::vector<uint8_t>* signature) const {
  if (key.GetType() != blink::kWebCryptoKeyTypePrivate)
    return Status::ErrorUnexpectedKeyType();

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  size_t sig_len = 64;
  signature->resize(sig_len);
  bssl::ScopedEVP_MD_CTX ctx;
  if (!EVP_DigestSignInit(ctx.get(), /*pctx=*/nullptr, /*type=*/nullptr,
                          /*e=*/nullptr, GetEVP_PKEY(key)) ||
      !EVP_DigestSign(ctx.get(), signature->data(), &sig_len, message.data(),
                      message.size())) {
    return Status::OperationError();
  }
  DCHECK_EQ(sig_len, (size_t)64);

  return Status::Success();
}

Status Ed25519Implementation::Verify(const blink::WebCryptoAlgorithm& algorithm,
                                     const blink::WebCryptoKey& key,
                                     base::span<const uint8_t> signature,
                                     base::span<const uint8_t> message,
                                     bool* signature_match) const {
  if (key.GetType() != blink::kWebCryptoKeyTypePublic)
    return Status::ErrorUnexpectedKeyType();

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  bssl::ScopedEVP_MD_CTX ctx;
  if (!EVP_DigestVerifyInit(ctx.get(), /*pctx=*/nullptr, /*type=*/nullptr,
                            /*e=*/nullptr, GetEVP_PKEY(key))) {
    return Status::OperationError();
  }

  *signature_match =
      1 == EVP_DigestVerify(ctx.get(), signature.data(), signature.size(),
                            message.data(), message.size());

  return Status::Success();
}

Status Ed25519Implementation::ImportKeyPkcs8(
    base::span<const uint8_t> key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) const {
  Status status = CheckKeyCreationUsages(all_private_key_usages_, usages);
  if (status.IsError())
    return status;

  bssl::UniquePtr<EVP_PKEY> private_key;
  status =
      ImportUnverifiedPkeyFromPkcs8(key_data, EVP_PKEY_ED25519, &private_key);
  if (status.IsError())
    return status;

  return CreateWebCryptoPrivateKey(
      std::move(private_key),
      blink::WebCryptoKeyAlgorithm::CreateEd25519(algorithm.Id()), extractable,
      usages, key);
}

Status Ed25519Implementation::ImportKeySpki(
    base::span<const uint8_t> key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) const {
  Status status = CheckKeyCreationUsages(all_public_key_usages_, usages);
  if (status.IsError())
    return status;

  bssl::UniquePtr<EVP_PKEY> public_key;
  status =
      ImportUnverifiedPkeyFromSpki(key_data, EVP_PKEY_ED25519, &public_key);
  if (status.IsError())
    return status;

  return CreateWebCryptoPublicKey(
      std::move(public_key),
      blink::WebCryptoKeyAlgorithm::CreateEd25519(algorithm.Id()), extractable,
      usages, key);
}

Status Ed25519Implementation::ExportKeyPkcs8(
    const blink::WebCryptoKey& key,
    std::vector<uint8_t>* buffer) const {
  if (key.GetType() != blink::kWebCryptoKeyTypePrivate)
    return Status::ErrorUnexpectedKeyType();
  return ExportPKeyPkcs8(GetEVP_PKEY(key), buffer);
}

Status Ed25519Implementation::ExportKeySpki(
    const blink::WebCryptoKey& key,
    std::vector<uint8_t>* buffer) const {
  if (key.GetType() != blink::kWebCryptoKeyTypePublic)
    return Status::ErrorUnexpectedKeyType();
  return ExportPKeySpki(GetEVP_PKEY(key), buffer);
}

Status Ed25519Implementation::DeserializeKeyForClone(
    const blink::WebCryptoKeyAlgorithm& algorithm,
    blink::WebCryptoKeyType type,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    base::span<const uint8_t> key_data,
    blink::WebCryptoKey* key) const {
  // Ed25519 algorithm doesn't need params for the generateKey method.
  // https://wicg.github.io/webcrypto-secure-curves/#ed25519-registration
  if (algorithm.ParamsType() != blink::kWebCryptoKeyAlgorithmParamsTypeNone)
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

  if (key->Algorithm().ParamsType() !=
      blink::kWebCryptoKeyAlgorithmParamsTypeNone) {
    return Status::ErrorUnexpected();
  }

  return Status::Success();
}

std::unique_ptr<AlgorithmImplementation> CreateEd25519Implementation() {
  return std::make_unique<Ed25519Implementation>();
}

}  // namespace webcrypto
