// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBCRYPTO_BLINK_KEY_HANDLE_H_
#define COMPONENTS_WEBCRYPTO_BLINK_KEY_HANDLE_H_

#include <stdint.h>

#include <vector>

#include "base/containers/span.h"
#include "third_party/blink/public/platform/web_crypto_key.h"
#include "third_party/boringssl/src/include/openssl/base.h"

// Blink keys (blink::WebCryptoKey) have an associated key handle
// (blink::WebCryptoKeyHandle) used to store custom data. This is where the
// underlying EVP_PKEY is stored for asymmetric keys, or an std::vector
// containing the bytes for symmetric keys.
//
// This file contains helpers for creating the key handles, and extracting
// properties from it.

namespace webcrypto {

// Returns a reference to the symmetric key data wrapped by the given Blink
// key. The returned reference is owned by |key|. This function must only be
// called on secret keys (HMAC, AES, etc).
const std::vector<uint8_t>& GetSymmetricKeyData(const blink::WebCryptoKey& key);

// Returns the EVP_PKEY* wrapped by the given Blink key. The returned pointer
// is owned by |key|. This function must only be called on asymmetric keys
// (RSA, EC, etc).
EVP_PKEY* GetEVP_PKEY(const blink::WebCryptoKey& key);

// Creates a symmetric key handle that can be passed to Blink. The caller takes
// ownership of the returned pointer.
blink::WebCryptoKeyHandle* CreateSymmetricKeyHandle(
    base::span<const uint8_t> key_bytes);

// Creates an asymmetric key handle that can be passed to Blink. The caller
// takes ownership of the returned pointer.
blink::WebCryptoKeyHandle* CreateAsymmetricKeyHandle(
    bssl::UniquePtr<EVP_PKEY> pkey);

}  // namespace webcrypto

#endif  // COMPONENTS_WEBCRYPTO_BLINK_KEY_HANDLE_H_
