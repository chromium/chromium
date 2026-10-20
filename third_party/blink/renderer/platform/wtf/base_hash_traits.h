// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_BASE_HASH_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_BASE_HASH_TRAITS_H_

#include <cstdint>
#include <limits>
#include <new>

#include "base/containers/span.h"
#include "base/i18n/language_tag.h"
#include "base/token.h"
#include "base/types/pass_key.h"
#include "base/types/token_type.h"
#include "base/unguessable_token.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hasher.h"

namespace blink {

template <>
struct HashTraits<base::Token> : GenericHashTraits<base::Token> {
  static uint32_t GetHash(const base::Token& token) {
    return token.is_zero() ? 0
                           : static_cast<uint32_t>(base::TokenHash()(token));
  }

  static constexpr bool kEmptyValueIsZero = true;

  static base::Token EmptyValue() { return base::Token(); }

  static base::Token DeletedValue() {
    return base::Token(std::numeric_limits<uint64_t>::max(),
                       std::numeric_limits<uint64_t>::max());
  }
};

template <>
struct HashTraits<base::UnguessableToken>
    : GenericHashTraits<base::UnguessableToken> {
  static uint32_t GetHash(const base::UnguessableToken& token) {
    return token.is_empty()
               ? 0
               : static_cast<uint32_t>(base::UnguessableTokenHash()(token));
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

// Hash traits for strongly-typed `base::UnguessableToken`s, e.g.,
// `blink::LocalFrameToken`. These forward to the `base::UnguessableToken`
// traits above.
//
// Unlike `base::UnguessableToken`, a `base::TokenType` deliberately can't be
// constructed empty: its default constructor mints a random token, and its
// explicit constructor CHECKs that the token isn't empty. So rather than
// defining `EmptyValue()`, these traits let the hash table fill empty slots
// with zeroed memory (an empty underlying token), and detect them with
// `IsEmptyValue()`. Such slots are never exposed outside of the hash table.
template <typename TypeMarker>
struct HashTraits<base::TokenType<TypeMarker>>
    : GenericHashTraits<base::TokenType<TypeMarker>> {
  using TokenType = base::TokenType<TypeMarker>;
  using UnderlyingTraits = HashTraits<base::UnguessableToken>;

  static uint32_t GetHash(const TokenType& token) {
    return UnderlyingTraits::GetHash(token.value());
  }

  static constexpr bool kEmptyValueIsZero = true;
  // `TokenType` can't represent an empty value; see above. This also means the
  // hash table can't compare keys against an `EmptyValue()`, so it must check
  // slots with `IsEmptyValue()` and `IsDeletedValue()` instead.
  static TokenType EmptyValue() = delete;
  static constexpr bool kSafeToCompareToEmptyOrDeleted = false;
  static bool IsEmptyValue(const TokenType& token) {
    return token.value().is_empty();
  }

  static bool IsDeletedValue(const TokenType& token) {
    return token.value() == UnderlyingTraits::DeletedValue();
  }
  static void ConstructDeletedValue(TokenType& slot) {
    // The deleted value is a non-empty token, so this is a valid `TokenType`.
    new (&slot) TokenType(UnderlyingTraits::DeletedValue());
  }
};

// Enables `base::i18n::LanguageTag` to be used as a key in `blink::HashMap`,
// e.g. for the locale-keyed maps in the Blink manifest Mojo bindings.
// Special sentinel values that do not form any valid bcp47 code are used as the
// empty/deleted value.
template <>
struct HashTraits<base::i18n::LanguageTag>
    : GenericHashTraits<base::i18n::LanguageTag> {
  static uint32_t GetHash(const base::i18n::LanguageTag& tag) {
    return HashString24(base::as_byte_span(tag.tag_string()));
  }

  static base::i18n::LanguageTag EmptyValue() {
    return base::i18n::LanguageTag(PassKey(), "__empty__");
  }

  static base::i18n::LanguageTag DeletedValue() {
    return base::i18n::LanguageTag(PassKey(), "__deleted__");
  }

 private:
  using PassKey = base::PassKey<HashTraits<base::i18n::LanguageTag>>;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_BASE_HASH_TRAITS_H_
