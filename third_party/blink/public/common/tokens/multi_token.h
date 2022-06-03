// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper class for a strongly-typed multiple variant token. Allows creating
// a token that can represent one of a collection of distinct token types.
// It would be great to replace this with a much simpler C++17 std::variant
// when that is available.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_TOKENS_MULTI_TOKEN_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_TOKENS_MULTI_TOKEN_H_

#include <type_traits>

#include "base/unguessable_token.h"
#include "third_party/blink/public/common/tokens/multi_token_internal.h"

namespace blink {

// Defines MultiToken, which is effectively a variant over 2 or more
// instances of base::TokenType.
//
// A MultiToken<..> emulates a token like interface. When default constructed
// it will construct itself as an instance of |TokenVariant0|. Additionally it
// offers the following functions allowing casting and querying token types at
// runtime:
//
//   // Determines whether this token stores an instance of a TokenType.
//   bool Is<TokenType>() const;
//
//   // Extracts the stored token in its original type. The stored token must
//   // be of the provided type otherwise this will explode at runtime.
//   const TokenType& GetAs<TokenType>() const;
//
// A variant must have at least 2 valid input types, but can have arbitrarily
// many. They must all be distinct, and they must all be instances of
// base::TokenType.
template <typename TokenVariant0,
          typename TokenVariant1,
          typename... TokenVariants>
class MultiToken : public internal::MultiTokenBase<TokenVariant0,
                                                   TokenVariant1,
                                                   TokenVariants...> {
 public:
  using Base =
      internal::MultiTokenBase<TokenVariant0, TokenVariant1, TokenVariants...>;

  // The total number of types.
  static const uint32_t kVariantCount = Base::VariantCount::kValue;

  // Default constructor. The resulting token will be a valid token of type
  // TokenVariant0.
  MultiToken() = default;

  // Copy constructors.
  MultiToken(const MultiToken& other) = default;
  template <typename InputTokenType,
            typename = typename std::enable_if<
                Base::template ValidType<InputTokenType>::kValue>::type>
  // NOLINTNEXTLINE(google-explicit-constructor)
  MultiToken(const InputTokenType& input_token)
      : value_(input_token.value()),
        variant_index_(Base::template TypeIndex<InputTokenType>::kValue) {}

  ~MultiToken() = default;

  // Assignment operators.
  MultiToken& operator=(const MultiToken& other) = default;
  template <typename InputTokenType,
            typename = typename std::enable_if<
                Base::template ValidType<InputTokenType>::kValue>::type>
  MultiToken& operator=(const InputTokenType& input_token) {
    value_ = input_token.value();
    variant_index_ = Base::template TypeIndex<InputTokenType>::kValue;
    return *this;
  }

  const base::UnguessableToken& value() const { return value_; }
  uint32_t variant_index() const { return variant_index_; }
  std::string ToString() const { return value().ToString(); }

  // Type checking.
  template <typename InputTokenType,
            typename = typename std::enable_if<
                Base::template ValidType<InputTokenType>::kValue>::type>
  bool Is() const {
    return variant_index_ == Base::template TypeIndex<InputTokenType>::kValue;
  }

  // Type conversion. Allows extracting the underlying token type. This should
  // only be called for the actual type that is stored in this token. This can
  // be checked by calling "Is<>" first.
  template <typename InputTokenType,
            typename = typename std::enable_if<
                Base::template ValidType<InputTokenType>::kValue>::type>
  InputTokenType GetAs() const {
    CHECK(Is<InputTokenType>()) << "invalid token type cast";
    // Type-punning via casting is undefined behaviour, so we return by value.
    return InputTokenType(value_);
  }

  // Comparison with untyped tokens. Only compares the token value, ignoring the
  // type.
  int Compare(const base::UnguessableToken& other) const {
    return Base::CompareImpl(value_, other);
  }
  bool operator<(const base::UnguessableToken& other) const {
    return Compare(other) == -1;
  }
  bool operator==(const base::UnguessableToken& other) const {
    return Compare(other) == 0;
  }
  bool operator!=(const base::UnguessableToken& other) const {
    return Compare(other) != 0;
  }

  // Comparison with other MultiTokens. Compares by token, then type.
  int Compare(const MultiToken& other) const {
    return Base::CompareImpl(std::tie(value_, variant_index_),
                             std::tie(other.value_, other.variant_index_));
  }
  bool operator<(const MultiToken& other) const { return Compare(other) == -1; }
  bool operator==(const MultiToken& other) const { return Compare(other) == 0; }
  bool operator!=(const MultiToken& other) const { return Compare(other) != 0; }

  // Comparison with individual typed tokens. Compares by token, then type.
  template <typename InputTokenType,
            typename = typename std::enable_if<
                Base::template ValidType<InputTokenType>::kValue>::type>
  int Compare(const InputTokenType& other) const {
    static constexpr uint32_t kInputTokenTypeIndex =
        Base::template TypeIndex<InputTokenType>::kValue;
    return Base::CompareImpl(std::tie(value_, variant_index_),
                             std::tie(other.value(), kInputTokenTypeIndex));
  }
  template <typename InputTokenType,
            typename = typename std::enable_if<
                Base::template ValidType<InputTokenType>::kValue>::type>
  bool operator<(const InputTokenType& other) const {
    return Compare(other) == -1;
  }
  template <typename InputTokenType,
            typename = typename std::enable_if<
                Base::template ValidType<InputTokenType>::kValue>::type>
  bool operator==(const InputTokenType& other) const {
    return Compare(other) == 0;
  }
  template <typename InputTokenType,
            typename = typename std::enable_if<
                Base::template ValidType<InputTokenType>::kValue>::type>
  bool operator!=(const InputTokenType& other) const {
    return Compare(other) != 0;
  }

  // Hash functor for use in unordered containers.
  struct Hasher {
    using argument_type = MultiToken;
    using result_type = size_t;
    result_type operator()(const MultiToken& token) const {
      return base::UnguessableTokenHash()(token.value_);
    }
  };

 private:
  // The underlying untyped token value. This will *never* be null initialized.
  base::UnguessableToken value_ = base::UnguessableToken::Create();

  // The index of the variant type that is currently stored in this token.
  uint32_t variant_index_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_TOKENS_MULTI_TOKEN_H_
