// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains utility functions and types for working with BoringSSL's
// EVP_* types.

#ifndef CRYPTO_EVP_H_
#define CRYPTO_EVP_H_

#include <vector>

#include "base/containers/span.h"
#include "crypto/crypto_export.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace crypto::evp {

// These functions consume an entire span of bytes and return a parsed EVP_PKEY,
// if the span encodes a valid object of the named kind. If the span contains
// any trailing data that is not parsed, or no valid EVP_PKEY can be parsed from
// the span, these return null.
CRYPTO_EXPORT bssl::UniquePtr<EVP_PKEY> PublicKeyFromBytes(
    base::span<const uint8_t> bytes);
CRYPTO_EXPORT bssl::UniquePtr<EVP_PKEY> PrivateKeyFromBytes(
    base::span<const uint8_t> bytes);

// These functions marshal a key of the named type into a buffer. Unlike the
// parsing functions they cannot fail.
CRYPTO_EXPORT std::vector<uint8_t> PublicKeyToBytes(const EVP_PKEY* key);
CRYPTO_EXPORT std::vector<uint8_t> PrivateKeyToBytes(const EVP_PKEY* key);

}  // namespace crypto::evp

#endif  // CRYPTO_EVP_H_
