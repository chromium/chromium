// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper class for a strongly-typed multiple variant token. Allows creating
// a token that can represent one of a collection of distinct token types.
// It would be great to replace this with a much simpler C++17 std::variant
// when that is available.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_TOKENS_MULTI_TOKEN_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_TOKENS_MULTI_TOKEN_H_

#include <stdint.h>

#include <limits>
#include <type_traits>

#include "base/types/variant_util.h"
#include "base/unguessable_token.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/tokens/multi_token_internal.h"

namespace blink {

// `MultiToken<Tokens...>` is a variant of 2 or more token types. Each token
// type must be an instantiation of `base::TokenType`, and each token type must
// be unique within `Tokens...`. Unlike `base::UnguessableToken`, a `MultiToken`
// is always valid: there is no null state. Default constructing a `MultiToken`
// will create a `MultiToken` containing an instance of the first token type in
// `Tokens`.
//
// Usage:
//
// using CowToken = base::TokenType<class CowTokenTag>;
// using GoatToken = base::TokenType<class GoatTokenTag>;
// using UngulateToken = blink::MultiToken<CowToken, GoatToken>;
//
// void TeleportCow(const CowToken&);
// void TeleportGoat(const GoatToken&);
//
// void TeleportUngulate(const UngulateToken& token) {
//   if (token.Is<CowToken>()) {
//     TeleportCow(token.Get<CowToken>());
//   } else if (token.Is<GoatToken>()) {
//     TeleportGoat(token.Get<GoatToken>());
//   }
//   CHECK(false);  // Not reachable.
// }
template <typename... Tokens>
class MultiToken {
  static_assert(sizeof...(Tokens) > 1);
  static_assert(sizeof...(Tokens) <= std::numeric_limits<uint32_t>::max());
  static_assert(std::conjunction_v<internal::IsBaseTokenType<Tokens>...>);
  static_assert(internal::AreAllUnique<Tokens...>);

  template <typename T>
  using EnableIfIsSupportedToken =
      internal::EnableIfIsSupportedToken<T, Tokens...>;

 public:
  using Storage = absl::variant<Tokens...>;
  // In an ideal world, this would use StrongAlias, but a StrongAlias is not
  // usable in a switch statement, even when the underlying type is integral.
  enum class Tag : uint32_t {};

  // A default constructed token will hold a default-constructed instance (i.e.
  // randomly initialised) of the first token type in `Tokens...`.
  MultiToken() = default;

  template <typename T, EnableIfIsSupportedToken<T> = 0>
  // NOLINTNEXTLINE(google-explicit-constructor)
  MultiToken(const T& token) : storage_(token) {}
  MultiToken(const MultiToken&) = default;

  template <typename T, EnableIfIsSupportedToken<T> = 0>
  MultiToken& operator=(const T& token) {
    storage_ = token;
    return *this;
  }
  MultiToken& operator=(const MultiToken&) = default;

  ~MultiToken() = default;

  // Returns true iff `this` currently holds a token of type `T`.
  template <typename T, EnableIfIsSupportedToken<T> = 0>
  bool Is() const {
    return absl::holds_alternative<T>(storage_);
  }

  // Returns `T` if `this` currently holds a token of type `T`; otherwise,
  // crashes.
  template <typename T, EnableIfIsSupportedToken<T> = 0>
  const T& GetAs() const {
    return absl::get<T>(storage_);
  }

  // Comparison operators
  friend bool operator==(const MultiToken& lhs, const MultiToken& rhs) {
    return lhs.storage_ == rhs.storage_;
  }

  friend bool operator!=(const MultiToken& lhs, const MultiToken& rhs) {
    return !(lhs == rhs);
  }

  template <typename T, EnableIfIsSupportedToken<T> = 0>
  friend bool operator==(const MultiToken& lhs, const T& rhs) {
    return absl::holds_alternative<T>(lhs.storage_) &&
           absl::get<T>(lhs.storage_) == rhs;
  }

  template <typename T, EnableIfIsSupportedToken<T> = 0>
  friend bool operator==(const T& lhs, const MultiToken& rhs) {
    return rhs == lhs;
  }

  template <typename T, EnableIfIsSupportedToken<T> = 0>
  friend bool operator!=(const MultiToken& lhs, const T& rhs) {
    return !(lhs == rhs);
  }

  template <typename T, EnableIfIsSupportedToken<T> = 0>
  friend bool operator!=(const T& lhs, const MultiToken& rhs) {
    return !(lhs == rhs);
  }

  // Unlike equality comparisons, ordering comparisons typically do not compare
  // a MultiToken and a sub type from `Tokens...`, so do not bother with the
  // extra overloads.
  friend bool operator<(const MultiToken& lhs, const MultiToken& rhs) {
    return lhs.storage_ < rhs.storage_;
  }

  friend bool operator<=(const MultiToken& lhs, const MultiToken& rhs) {
    return !(lhs > rhs);
  }

  friend bool operator>(const MultiToken& lhs, const MultiToken& rhs) {
    return rhs < lhs;
  }

  friend bool operator>=(const MultiToken& lhs, const MultiToken& rhs) {
    return !(lhs < rhs);
  }

  // Hash functor for use in unordered containers.
  struct Hasher {
    using argument_type = MultiToken;
    using result_type = size_t;
    result_type operator()(const MultiToken& token) const {
      return base::UnguessableTokenHash()(token.value());
    }
  };

  // Prefer the above helpers where possible. These methods are primarily useful
  // for serialization/deserialization.

  // Returns the underlying `base::UnguessableToken` of the currently held
  // token.
  const base::UnguessableToken& value() const;

  // 0-based index of the currently held token's type, based on its position in
  // `Tokens...`.
  Tag variant_index() const { return static_cast<Tag>(storage_.index()); }

  // Returns the 0-based index that a token of type `T` would have if it were
  // currently held.
  template <typename T, EnableIfIsSupportedToken<T> = 0>
  static constexpr Tag IndexOf() {
    return static_cast<Tag>(base::VariantIndexOfType<Storage, T>());
  }

  // Equivalent to `value().ToString()`.
  std::string ToString() const;

 private:
  Storage storage_;
};

template <typename... Tokens>
const base::UnguessableToken& MultiToken<Tokens...>::value() const {
  return absl::visit(
      [](const auto& token) -> const base::UnguessableToken& {
        return token.value();
      },
      storage_);
}

template <typename... Tokens>
std::string MultiToken<Tokens...>::ToString() const {
  return absl::visit([](const auto& token) { return token.ToString(); },
                     storage_);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_TOKENS_MULTI_TOKEN_H_
