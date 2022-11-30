// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/privacy_budget/identifiability_metrics.h"

#include <cstdint>

#include "base/containers/span.h"
#include "base/hash/legacy_hash.h"

namespace blink {

uint64_t IdentifiabilityDigestOfBytes(base::span<const uint8_t> in) {
  // The chosen hash function satisfies the following requirements:
  //
  //   * Fast. These hashes will need to be calculated during performance
  //     critical code.
  //   * Suitable for fingerprinting. I.e. broad domain, good diffusion, low
  //     collision rate.
  //   * Resistant to hash flooding.
  //   * Able to use the entire 64-bit space we have at our disposal.
  //   * Either support iterative operation or be usable as a primitive for
  //     constructing one.
  //   * Remains stable for the duration of the identifiability study O(months).
  //     This one is trivial. It just means that the hash is not in danger of
  //     imminent change.
  //   * Implemented, well tested, and usable by //content, //chrome, as well
  //     as //blink/common.
  //
  // It is not a requirement for the digest to be a cryptographic hash. I.e. not
  // necessary to deter second-preimage construction.
  //
  // base::PersistentHash(): (Rejected)
  //   - Based on SuperFastHash() which doesn't meet the fingerprinting
  //     requirement due to a high collision rate.
  //   - Digest is 32-bits.
  //   - No stateful implementation in //base. Blink's StringHasher is
  //     interestingly a stateful implementation of SuperFastHash but is not
  //     available in //blink/public/common.
  //
  // base::legacy::CityHash64{WithSeed}(): (Selected)
  //   - Based on Google's CityHash 1.0.3. Some known weaknesses, but still
  //     good enough.
  //   - No ready-to-use chaining implementation.
  //   + Digest is 64-bits.
  //   + Seeded variant is a useful primitive for a chained hash function.
  //     Would be better if it took two seeds, but one is also usable.
  //
  // Other hash functions were considered, but were rejected due to one or more
  // of the following reasons:
  //   - An implementation was not available.
  //   - The version available has significant known weaknesses.
  //
  // One in particular that would have been nice to have is FarmHash.
  //
  // CityHash is quite efficient for small buffers. Operation counts are
  // roughly as follows. For small buffers, fetches dominate.:
  //
  //     Length │  Fetches │   Muls  │ Shifts  │
  //     ───────┼──────────┼─────────┼─────────┤
  //     1..16  │     3    │    3    │    4    │
  //     ───────┼──────────┼─────────┼─────────┤
  //     17..32 │     4    │    3    │    8    │
  //     ───────┼──────────┼─────────┼─────────┤
  //     33..64 │    10    │    4    │   18    │
  //     ───────┴──────────┴─────────┴─────────┘
  return base::legacy::CityHash64(in);
}

}  // namespace blink
