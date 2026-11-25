// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_MULTI_TOKEN_HASH_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_MULTI_TOKEN_HASH_TRAITS_H_

#include <cstdint>
#include <limits>
#include <tuple>

#include "base/unguessable_token.h"
#include "third_party/blink/public/common/tokens/multi_token.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"

namespace blink {

// Hash traits for `blink::MultiToken<Tokens...>`, e.g., `blink::FrameToken`.
//
// Lives in `platform/mojo/` rather than `platform/wtf/` because it is pulled in
// by `blink_cpp_typemaps` in `//third_party/blink/public/mojom/tokens` and
// `wtf` should not depend on `//third_party/blink/public/common/tokens` (which
// depends on Mojo bindings).
//
// Intentionally does NOT include `platform/wtf/base_hash_traits.h`: because
// this header is included from `tokens.mojom-blink.h`, pulling in
// `HashTraits<base::TokenType<T>>::EmptyValue() = delete` breaks Blink mojom
// targets that use a `base::TokenType` as a `HashMap` mapped value (e.g.,
// `services/webnn/public/mojom/webnn_graph.mojom`).
//
// Unlike `HashTraits<base::TokenType<T>>`, `MultiToken` cannot reuse zeroed
// empty slots (`kEmptyValueIsZero = true`) because it wraps a `std::variant`
// whose active-alternative discriminant and non-trivial `TokenType` members
// cannot be safely zero-initialized. Defining a concrete `EmptyValue()` also
// allows `MultiToken` to be used as a `HashMap` mapped value without invoking
// random token generation.
//
// Distinct non-empty sentinel values of the first token type — `(max, max - 1)`
// for empty and `(max, max)` for deleted — are assumed never to collide with
// real randomly minted tokens. The Mojo deserialization path
// (`mojo::MapTraits<blink::HashMap<K, V>>::Insert`) rejects both sentinels via
// `IsValidKey()`.
template <typename... Tokens>
struct HashTraits<MultiToken<Tokens...>>
    : GenericHashTraits<MultiToken<Tokens...>> {
  using MultiTokenType = MultiToken<Tokens...>;
  using FirstToken = std::tuple_element_t<0, std::tuple<Tokens...>>;

  static uint32_t GetHash(const MultiTokenType& token) {
    return HashInts(
        static_cast<uint32_t>(token.variant_index()),
        static_cast<uint32_t>(base::UnguessableTokenHash()(token.value())));
  }

  static MultiTokenType EmptyValue() {
    return FirstToken(base::UnguessableToken::Deserialize(
                          std::numeric_limits<uint64_t>::max(),
                          std::numeric_limits<uint64_t>::max() - 1)
                          .value());
  }

  static MultiTokenType DeletedValue() {
    return FirstToken(base::UnguessableToken::Deserialize(
                          std::numeric_limits<uint64_t>::max(),
                          std::numeric_limits<uint64_t>::max())
                          .value());
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_MULTI_TOKEN_HASH_TRAITS_H_
