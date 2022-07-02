// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/numerics/safe_math.h"
#include "components/webcrypto/algorithms/rsa_sign.h"
#include "components/webcrypto/algorithms/util.h"
#include "components/webcrypto/blink_key_handle.h"
#include "components/webcrypto/status.h"
#include "crypto/openssl_util.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

namespace webcrypto {

namespace {

// Extracts the OpenSSL key and digest from a WebCrypto key. The returned
// pointers will remain valid as long as |key| is alive.
Status GetPKeyAndDigest(const blink::WebCryptoKey& key,
                        EVP_PKEY** pkey,
                        const EVP_MD** digest) {
  *pkey = GetEVP_PKEY(key);

  *digest = GetDigest(key.Algorithm().RsaHashedParams()->GetHash());
  if (!*digest)
    return Status::ErrorUnsupported();

  return Status::Success();
}

// Sets the PSS parameters on |pctx| if the key is for RSA-PSS.
//
// Otherwise returns Success without doing anything.
Status ApplyRsaPssOptions(const blink::WebCryptoKey& key,
                          const EVP_MD* const mgf_digest,
                          unsigned int salt_length_bytes,
                          EVP_PKEY_CTX* pctx) {
  // Only apply RSA-PSS options if the key is for RSA-PSS.
  if (key.Algorithm().Id() != blink::kWebCryptoAlgorithmIdRsaPss) {
    DCHECK_EQ(0u, salt_length_bytes);
    DCHECK_EQ(blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
              key.Algorithm().Id());
    return Status::Success();
  }

  // BoringSSL takes a signed int for the salt length, and interprets
  // negative values in a special manner. Make sure not to silently underflow.
  base::CheckedNumeric<int> salt_length_bytes_int(salt_length_bytes);
  if (!salt_length_bytes_int.IsValid()) {
    // TODO(eroman): Give a better error message.
    return Status::OperationError();
  }

  if (1 != EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_PSS_PADDING) ||
      1 != EVP_PKEY_CTX_set_rsa_mgf1_md(pctx, mgf_digest) ||
      1 != EVP_PKEY_CTX_set_rsa_pss_saltlen(
               pctx, salt_length_bytes_int.ValueOrDie())) {
    return Status::OperationError();
  }

  return Status::Success();
}

}  // namespace

Status RsaSign(const blink::WebCryptoKey& key,
               unsigned int pss_salt_length_bytes,
               base::span<const uint8_t> data,
               std::vector<uint8_t>* buffer) {
  if (key.GetType() != blink::kWebCryptoKeyTypePrivate)
    return Status::ErrorUnexpectedKeyType();

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  bssl::ScopedEVP_MD_CTX ctx;
  EVP_PKEY_CTX* pctx = nullptr;  // Owned by |ctx|.

  EVP_PKEY* private_key = nullptr;
  const EVP_MD* digest = nullptr;
  Status status = GetPKeyAndDigest(key, &private_key, &digest);
  if (status.IsError())
    return status;

  // NOTE: A call to EVP_DigestSignFinal() with a NULL second parameter
  // returns a maximum allocation size, while the call without a NULL returns
  // the real one, which may be smaller.
  size_t sig_len = 0;
  if (!EVP_DigestSignInit(ctx.get(), &pctx, digest, nullptr, private_key)) {
    return Status::OperationError();
  }

  // Set PSS-specific options (if applicable).
  status = ApplyRsaPssOptions(key, digest, pss_salt_length_bytes, pctx);
  if (status.IsError())
    return status;

  if (!EVP_DigestSignUpdate(ctx.get(), data.data(), data.size()) ||
      !EVP_DigestSignFinal(ctx.get(), nullptr, &sig_len)) {
    return Status::OperationError();
  }

  buffer->resize(sig_len);
  if (!EVP_DigestSignFinal(ctx.get(), buffer->data(), &sig_len))
    return Status::OperationError();

  buffer->resize(sig_len);
  return Status::Success();
}

Status RsaVerify(const blink::WebCryptoKey& key,
                 unsigned int pss_salt_length_bytes,
                 base::span<const uint8_t> signature,
                 base::span<const uint8_t> data,
                 bool* signature_match) {
  if (key.GetType() != blink::kWebCryptoKeyTypePublic)
    return Status::ErrorUnexpectedKeyType();

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  bssl::ScopedEVP_MD_CTX ctx;
  EVP_PKEY_CTX* pctx = nullptr;  // Owned by |ctx|.

  EVP_PKEY* public_key = nullptr;
  const EVP_MD* digest = nullptr;
  Status status = GetPKeyAndDigest(key, &public_key, &digest);
  if (status.IsError())
    return status;

  if (!EVP_DigestVerifyInit(ctx.get(), &pctx, digest, nullptr, public_key))
    return Status::OperationError();

  // Set PSS-specific options (if applicable).
  status = ApplyRsaPssOptions(key, digest, pss_salt_length_bytes, pctx);
  if (status.IsError())
    return status;

  if (!EVP_DigestVerifyUpdate(ctx.get(), data.data(), data.size()))
    return Status::OperationError();

  *signature_match =
      1 == EVP_DigestVerifyFinal(ctx.get(), signature.data(), signature.size());
  return Status::Success();
}

}  // namespace webcrypto
