// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_COSE_H_
#define CRYPTO_COSE_H_

#include <vector>

#include "crypto/crypto_export.h"
#include "crypto/keypair.h"

namespace crypto {

// Converts a PublicKey in |key| to a COSE_Key structure, returning the
// serialized CBOR bytes. Currently, we only support keys using the RSA and
// EC-P256 algorithms.
CRYPTO_EXPORT std::vector<uint8_t> PublicKeyToCoseKey(
    const keypair::PublicKey& key);

}  // namespace crypto

#endif  // CRYPTO_COSE_H_
