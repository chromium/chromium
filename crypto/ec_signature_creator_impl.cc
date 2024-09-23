// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/ec_signature_creator_impl.h"

#include <stddef.h>
#include <stdint.h>

#include "crypto/ec_private_key.h"
#include "crypto/openssl_util.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ecdsa.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace crypto {

ECSignatureCreatorImpl::ECSignatureCreatorImpl(ECPrivateKey* key) : key_(key) {}

ECSignatureCreatorImpl::~ECSignatureCreatorImpl() = default;

bool ECSignatureCreatorImpl::Sign(base::span<const uint8_t> data,
                                  std::vector<uint8_t>* signature) {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);
  bssl::ScopedEVP_MD_CTX ctx;
  size_t sig_len = 0;
  if (!ctx.get() ||
      !EVP_DigestSignInit(ctx.get(), nullptr, EVP_sha256(), nullptr,
                          key_->key()) ||
      !EVP_DigestSignUpdate(ctx.get(), data.data(), data.size()) ||
      !EVP_DigestSignFinal(ctx.get(), nullptr, &sig_len)) {
    return false;
  }

  signature->resize(sig_len);
  if (!EVP_DigestSignFinal(ctx.get(), &signature->front(), &sig_len))
    return false;

  // NOTE: A call to EVP_DigestSignFinal() with a nullptr second parameter
  // returns a maximum allocation size, while the call without a nullptr
  // returns the real one, which may be smaller.
  signature->resize(sig_len);
  return true;
}

bool ECSignatureCreatorImpl::DecodeSignature(
    const std::vector<uint8_t>& der_sig,
    std::vector<uint8_t>* out_raw_sig) {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);
  // Create ECDSA_SIG object from DER-encoded data.
  bssl::UniquePtr<ECDSA_SIG> ecdsa_sig(
      ECDSA_SIG_from_bytes(der_sig.data(), der_sig.size()));
  if (!ecdsa_sig.get())
    return false;

  // The result is made of two 32-byte vectors.
  const size_t kMaxBytesPerBN = 32;
  std::vector<uint8_t> result(2 * kMaxBytesPerBN);

  if (!BN_bn2bin_padded(&result[0], kMaxBytesPerBN, ecdsa_sig->r) ||
      !BN_bn2bin_padded(&result[kMaxBytesPerBN], kMaxBytesPerBN,
                        ecdsa_sig->s)) {
    return false;
  }
  out_raw_sig->swap(result);
  return true;
}

}  // namespace crypto
