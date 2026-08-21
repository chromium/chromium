// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_BASE_HASH_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_BASE_HASH_TRAITS_H_

#include <limits>

#include "base/unguessable_token.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"

namespace blink {

template <>
struct HashTraits<base::UnguessableToken>
    : GenericHashTraits<base::UnguessableToken> {
  static unsigned GetHash(const base::UnguessableToken& token) {
    return token.is_empty()
               ? 0
               : static_cast<unsigned>(base::UnguessableTokenHash()(token));
  }

  static constexpr bool kEmptyValueIsZero = true;

  static base::UnguessableToken EmptyValue() {
    return base::UnguessableToken();
  }

  static base::UnguessableToken DeletedValue() {
    return base::UnguessableToken::Deserialize(
               std::numeric_limits<uint64_t>::max(),
               std::numeric_limits<uint64_t>::max())
        .value();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_BASE_HASH_TRAITS_H_
