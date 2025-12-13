// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_SIGN_H_
#define CRYPTO_SIGN_H_

#include "base/containers/span.h"
#include "crypto/crypto_export.h"
#include "crypto/keypair.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace crypto::sign {

enum SignatureKind {
  RSA_PKCS1_SHA1,
  RSA_PKCS1_SHA256,
  RSA_PKCS1_SHA384,
  RSA_PKCS1_SHA512,

  // These RSA-PSS SignatureKinds use the named hash function as both the hash
  // function and the MGF, and use the default salt length.
  RSA_PSS_SHA256,
  RSA_PSS_SHA384,
  RSA_PSS_SHA512,

  ECDSA_SHA256,

  // Note: Ed25519 cannot be used in streaming modes and has to be passed the
  // entire message to sign; it does not take a separate hash function.
  ED25519,
};

// One-shot signature function: produce a signature of `data` using `key`.
CRYPTO_EXPORT std::vector<uint8_t> Sign(SignatureKind kind,
                                        const crypto::keypair::PrivateKey& key,
                                        base::span<const uint8_t> data);

// One-shot verification function: check a signature and return whether it is
// valid.
[[nodiscard]] CRYPTO_EXPORT bool Verify(SignatureKind kind,
                                        const crypto::keypair::PublicKey& key,
                                        base::span<const uint8_t> data,
                                        base::span<const uint8_t> signature);

// A streaming signer interface. Calling Finish() produces the final signature.
class CRYPTO_EXPORT Signer {
 public:
  Signer(SignatureKind kind, crypto::keypair::PrivateKey key);
  ~Signer();

  // Put more data into the signing function.
  void Update(base::span<const uint8_t> data);

  // Finish the signature and return the signature value. After this is called,
  // the Signer cannot be used any more.
  std::vector<uint8_t> Finish();

 private:
  crypto::keypair::PrivateKey key_;
  bssl::UniquePtr<EVP_MD_CTX> sign_context_;
};

// A streaming verifier interface. Calling Finish() checks the signature.
class CRYPTO_EXPORT Verifier {
 public:
  Verifier(SignatureKind kind,
           crypto::keypair::PublicKey key,
           base::span<const uint8_t> signature);
  ~Verifier();

  // Put more data into the verification function.
  void Update(base::span<const uint8_t> data);

  // Finish the verification and return whether the signature matched the
  // expected value provided at construction time. After this is called, the
  // Verifier cannot be used any more.
  [[nodiscard]] bool Finish();

 private:
  crypto::keypair::PublicKey key_;
  std::vector<uint8_t> signature_;
  bssl::UniquePtr<EVP_MD_CTX> verify_context_;
};

}  // namespace crypto::sign

#endif  // CRYPTO_SIGN_H_
