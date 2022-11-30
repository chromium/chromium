// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_KEY_FILTERING_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_KEY_FILTERING_H_

#include <vector>

#include "services/network/public/mojom/trust_tokens.mojom.h"

namespace network {

// Mutates |keys| to contain the |num_keys_to_keep| many soonest-to-expire (in
// particular, not yet expired) keys. If there are fewer than |num_keys_to_keep|
// such keys, mutates |keys| to contain all such keys. Breaks ties
// determistically based on key body.
//
// (A key has "expired" means its expiry time is not in the future.)
//
// |keys|'s entries must not be null.
void RetainSoonestToExpireTrustTokenKeys(
    std::vector<mojom::TrustTokenVerificationKeyPtr>* keys,
    size_t num_keys_to_keep);

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_KEY_FILTERING_H_
