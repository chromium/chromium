// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBCRYPTO_GENERATE_KEY_RESULT_H_
#define COMPONENTS_WEBCRYPTO_GENERATE_KEY_RESULT_H_

#include "third_party/blink/public/platform/web_crypto.h"

namespace webcrypto {

// This is the result object when generating keys. It encapsulates either a
// secret key, or a public/private key pair.
class GenerateKeyResult {
 public:
  enum Type {
    // An empty (or "null") result.
    TYPE_NULL,

    // The result is a secret key, accessible through secret_key()
    TYPE_SECRET_KEY,

    // The result is a public/private key pair, accessible through public_key()
    // and private_key()
    TYPE_PUBLIC_PRIVATE_KEY_PAIR
  };

  // Initializes a "null" instance.
  GenerateKeyResult();

  Type type() const;

  const blink::WebCryptoKey& secret_key() const;
  const blink::WebCryptoKey& public_key() const;
  const blink::WebCryptoKey& private_key() const;

  void AssignSecretKey(const blink::WebCryptoKey& key);
  void AssignKeyPair(const blink::WebCryptoKey& public_key,
                     const blink::WebCryptoKey& private_key);

  // Sends the key(s) to the Blink result. Should not be called for "null"
  // results.
  void Complete(blink::WebCryptoResult* out) const;

 private:
  Type type_;

  blink::WebCryptoKey secret_key_;
  blink::WebCryptoKey public_key_;
  blink::WebCryptoKey private_key_;
};

}  // namespace webcrypto

#endif  // COMPONENTS_WEBCRYPTO_GENERATE_KEY_RESULT_H_
