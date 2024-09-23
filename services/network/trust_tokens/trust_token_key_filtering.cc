// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_key_filtering.h"

#include <algorithm>
#include <tuple>
#include <vector>

#include "base/time/time.h"

namespace network {

void RetainSoonestToExpireTrustTokenKeys(
    std::vector<mojom::TrustTokenVerificationKeyPtr>* keys,
    size_t num_keys_to_keep) {
  DCHECK(keys);

  auto now = base::Time::Now();
  std::erase_if(*keys, [now](const mojom::TrustTokenVerificationKeyPtr& key) {
    return key->expiry <= now;
  });

  // size() < num_keys_to_keep -> subsequent code segfaults
  // size() == num_keys_to_keep -> subsequent code no-ops, so we might as well
  // return.
  if (keys->size() <= num_keys_to_keep)
    return;

  std::partial_sort(keys->begin(), keys->begin() + num_keys_to_keep,
                    keys->end(),
                    [](const mojom::TrustTokenVerificationKeyPtr& lhs,
                       const mojom::TrustTokenVerificationKeyPtr& rhs) {
                      return std::tie(lhs->expiry, lhs->body) <
                             std::tie(rhs->expiry, rhs->body);
                    });

  keys->resize(num_keys_to_keep);
}

}  // namespace network
