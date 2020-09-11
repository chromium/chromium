// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PRIVACY_BUDGET_IDENTIFIABILITY_METRICS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PRIVACY_BUDGET_IDENTIFIABILITY_METRICS_H_

#include <cstdint>
#include <cstring>
#include <type_traits>

#include "base/containers/span.h"
#include "third_party/blink/public/common/common_export.h"

namespace blink {

// IdentifiabilityDigestOfBytes, which is NOT a cryptographic hash function,
// takes a span of bytes as input and calculates a digest that can be used with
// identifiability metric reporting functions.
//
// The returned digest ...:
//
// * Is Stable: The returned digest will be consistent across different versions
//   of Chromium. Thus it can be persisted and meaningfully aggregated across
//   browser versions.
//
// * Is approximately uniformly distributed when the input is uniformly
//   distributed.
//
// * Is NOT optimized for any other distribution of input including narrow
//   integral ranges.
//
// * Is NOT collision resistant: Callers should assume that it is easy to come
//   up with collisions, and to come up with a pre-image given a digest.
//
// Note: This is NOT a cryptographic hash function.
BLINK_COMMON_EXPORT uint64_t
IdentifiabilityDigestOfBytes(base::span<const uint8_t> in);

// A family of helper function overloads that produce digests for basic types.
// If sizeof(in) <= 64, the underlying bits are used directly; no hash is
// invoked.
//
// The set of supported types can be extended by declaring overloads of
// IdentifiabilityDigestHelper(); such declarations should be made in a header
// included before this header so that they can be used by the span and
// parameter pack overloads of IdentifiabilityDigestHelper.
//
// TODO(asanka): Remove once callers have been migrated to
// IdentifiabilityToken().

// Integer version.
template <typename T,
          typename std::enable_if_t<std::is_integral<T>::value>* = nullptr>
uint64_t IdentifiabilityDigestHelper(T in) {
  static_assert(sizeof(in) <= sizeof(uint64_t),
                "The input type must be smaller than 64 bits.");
  return in;
}

// Floating-point version. Note that this doesn't account for endianness, and
// that integer and floating point endianness need not match for a given
// architecture.
template <
    typename T,
    typename std::enable_if_t<std::is_floating_point<T>::value>* = nullptr>
uint64_t IdentifiabilityDigestHelper(T in) {
  static_assert(sizeof(in) <= sizeof(uint64_t),
                "The input type must be smaller than 64 bits.");
  uint64_t result = 0;
  std::memcpy(&result, &in, sizeof(in));
  return result;
}

// Enum version. This just casts to the underlying type of the enum, which is
// then converted to uint64_t.
template <typename T,
          typename std::enable_if_t<std::is_enum<T>::value>* = nullptr>
uint64_t IdentifiabilityDigestHelper(T in) {
  static_assert(sizeof(in) <= sizeof(uint64_t),
                "The input type must be smaller than 64 bits.");
  return static_cast<typename std::underlying_type<T>::type>(in);
}

// Computes a combined digest for a span of elements. T can be any type
// supported by a IdentifiabilityDigestHelper overload declared before this
// function.
template <typename T, size_t Extent>
uint64_t IdentifiabilityDigestHelper(base::span<T, Extent> span) {
  uint64_t cur_digest = 0;
  for (const auto& element : span) {
    uint64_t digests[2];
    digests[0] = cur_digest;
    digests[1] = IdentifiabilityDigestHelper(element);
    cur_digest = IdentifiabilityDigestOfBytes(
        base::make_span(reinterpret_cast<uint8_t*>(digests), sizeof(digests)));
  }
  return cur_digest;
}

// Computes a combined digest value for a series of elements passed in as
// arguments. This declaration must appear after any other
// IdentifiabilityDigestHelper()
// overloads.
template <typename T, typename... Targs>
uint64_t IdentifiabilityDigestHelper(T in, Targs... extra_in) {
  uint64_t digests[2];
  digests[0] = IdentifiabilityDigestHelper(in);
  digests[1] = IdentifiabilityDigestHelper(extra_in...);
  return IdentifiabilityDigestOfBytes(
      base::make_span(reinterpret_cast<uint8_t*>(digests), sizeof(digests)));
}

// The zero-length digest, i.e. the digest computed for no bytes.
static constexpr uint64_t kIdentifiabilityDigestOfNoBytes =
    0x9ae16a3b2f90404fULL;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PRIVACY_BUDGET_IDENTIFIABILITY_METRICS_H_
