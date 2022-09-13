// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <memory>

#include "base/containers/span.h"
#include "components/webcrypto/algorithms/rsa.h"
#include "components/webcrypto/algorithms/util.h"
#include "components/webcrypto/blink_key_handle.h"
#include "components/webcrypto/status.h"
#include "crypto/openssl_util.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

namespace webcrypto {

namespace {

typedef int (*InitFunc)(EVP_PKEY_CTX* ctx);
typedef int (*EncryptDecryptFunc)(EVP_PKEY_CTX* ctx,
                                  unsigned char* out,
                                  size_t* outlen,
                                  const unsigned char* in,
                                  size_t inlen);

// Helper for doing either RSA-OAEP encryption or decryption.
//
// To encrypt call with:
//   init_func=EVP_PKEY_encrypt_init, encrypt_decrypt_func=EVP_PKEY_encrypt
//
// To decrypt call with:
//   init_func=EVP_PKEY_decrypt_init, encrypt_decrypt_func=EVP_PKEY_decrypt
Status CommonEncryptDecrypt(InitFunc init_func,
                            EncryptDecryptFunc encrypt_decrypt_func,
                            const blink::WebCryptoAlgorithm& algorithm,
                            const blink::WebCryptoKey& key,
                            base::span<const uint8_t> data,
                            std::vector<uint8_t>* buffer) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  EVP_PKEY* pkey = GetEVP_PKEY(key);
  const EVP_MD* digest =
      GetDigest(key.Algorithm().RsaHashedParams()->GetHash());
  if (!digest)
    return Status::ErrorUnsupported();

  bssl::UniquePtr<EVP_PKEY_CTX> ctx(EVP_PKEY_CTX_new(pkey, nullptr));

  if (!init_func(ctx.get()) ||
      !EVP_PKEY_CTX_set_rsa_padding(ctx.get(), RSA_PKCS1_OAEP_PADDING) ||
      !EVP_PKEY_CTX_set_rsa_oaep_md(ctx.get(), digest) ||
      !EVP_PKEY_CTX_set_rsa_mgf1_md(ctx.get(), digest)) {
    return Status::OperationError();
  }

  const blink::WebVector<uint8_t>& label =
      algorithm.RsaOaepParams()->OptionalLabel();

  if (label.size()) {
    // Make a copy of the label, since the ctx takes ownership of it when
    // calling set0_rsa_oaep_label().
    bssl::UniquePtr<uint8_t> label_copy;
    label_copy.reset(static_cast<uint8_t*>(OPENSSL_malloc(label.size())));
    memcpy(label_copy.get(), label.data(), label.size());

    if (1 != EVP_PKEY_CTX_set0_rsa_oaep_label(ctx.get(), label_copy.release(),
                                              label.size())) {
      return Status::OperationError();
    }
  }

  // Determine the maximum length of the output.
  size_t outlen = 0;
  if (!encrypt_decrypt_func(ctx.get(), nullptr, &outlen, data.data(),
                            data.size())) {
    return Status::OperationError();
  }
  buffer->resize(outlen);

  // Do the actual encryption/decryption.
  if (!encrypt_decrypt_func(ctx.get(), buffer->data(), &outlen, data.data(),
                            data.size())) {
    return Status::OperationError();
  }
  buffer->resize(outlen);

  return Status::Success();
}

class RsaOaepImplementation : public RsaHashedAlgorithm {
 public:
  RsaOaepImplementation()
      : RsaHashedAlgorithm(
            blink::kWebCryptoKeyUsageEncrypt | blink::kWebCryptoKeyUsageWrapKey,
            blink::kWebCryptoKeyUsageDecrypt |
                blink::kWebCryptoKeyUsageUnwrapKey) {}

  const char* GetJwkAlgorithm(
      const blink::WebCryptoAlgorithmId hash) const override {
    switch (hash) {
      case blink::kWebCryptoAlgorithmIdSha1:
        return "RSA-OAEP";
      case blink::kWebCryptoAlgorithmIdSha256:
        return "RSA-OAEP-256";
      case blink::kWebCryptoAlgorithmIdSha384:
        return "RSA-OAEP-384";
      case blink::kWebCryptoAlgorithmIdSha512:
        return "RSA-OAEP-512";
      default:
        return nullptr;
    }
  }

  Status Encrypt(const blink::WebCryptoAlgorithm& algorithm,
                 const blink::WebCryptoKey& key,
                 base::span<const uint8_t> data,
                 std::vector<uint8_t>* buffer) const override {
    if (key.GetType() != blink::kWebCryptoKeyTypePublic)
      return Status::ErrorUnexpectedKeyType();

    return CommonEncryptDecrypt(EVP_PKEY_encrypt_init, EVP_PKEY_encrypt,
                                algorithm, key, data, buffer);
  }

  Status Decrypt(const blink::WebCryptoAlgorithm& algorithm,
                 const blink::WebCryptoKey& key,
                 base::span<const uint8_t> data,
                 std::vector<uint8_t>* buffer) const override {
    if (key.GetType() != blink::kWebCryptoKeyTypePrivate)
      return Status::ErrorUnexpectedKeyType();

    return CommonEncryptDecrypt(EVP_PKEY_decrypt_init, EVP_PKEY_decrypt,
                                algorithm, key, data, buffer);
  }
};

}  // namespace

std::unique_ptr<AlgorithmImplementation> CreateRsaOaepImplementation() {
  return std::make_unique<RsaOaepImplementation>();
}

}  // namespace webcrypto
