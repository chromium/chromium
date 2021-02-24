// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Internal implementation details for MultiToken. Only intended to be included
// from multi_token.h.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_TOKENS_MULTI_TOKEN_INTERNAL_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_TOKENS_MULTI_TOKEN_INTERNAL_H_

#include <algorithm>
#include <cstring>
#include <type_traits>

#include "base/unguessable_token.h"
#include "base/util/type_safety/token_type.h"

namespace blink {

namespace internal {

////////////////////////////////////////////////////////////////////////////////
// MultiTokenVariantCount
//
// Counts the number of token types.

template <typename... VariantTypes>
struct MultiTokenVariantCount;

// Recursive case.
template <typename FirstVariantType, typename... OtherVariantTypes>
struct MultiTokenVariantCount<FirstVariantType, OtherVariantTypes...> {
  // Deliberately use uint32_t here so as not to incur an extra 4 bytes of
  // overhead on 64-bit systems, as this is the same type used by the
  // |variant_index_|.
  static constexpr uint32_t kValue =
      1 + MultiTokenVariantCount<OtherVariantTypes...>::kValue;
};

// Base case.
template <>
struct MultiTokenVariantCount<> {
  static constexpr uint32_t kValue = 0;
};

////////////////////////////////////////////////////////////////////////////////
// MultiTokenVariantIsTokenType
//
// Ensures if a QueryType is a a util::TokenType<>.

// Default case.
template <typename QueryType>
struct MultiTokenVariantIsTokenType {
  static constexpr bool kValue = false;
};

// Specialization for util::TokenType<>.
template <typename TokenTypeTag>
struct MultiTokenVariantIsTokenType<::util::TokenType<TokenTypeTag>> {
  static constexpr bool kValue = true;

  // We expect an identical layout, which allows us to reinterpret_cast between
  // types. The spec does not guarantee this, but sane compilers do. Thankfully
  // we can check whether or not the compiler is sane (and if the behaviour is
  // safe) at compile-time.
  static_assert(
      sizeof(::util::TokenType<TokenTypeTag>) ==
          sizeof(::base::UnguessableToken),
      "util::TokenType must have the same sizeof as base::UnguessableToken");
  static_assert(
      alignof(::util::TokenType<TokenTypeTag>) ==
          alignof(::base::UnguessableToken),
      "util::TokenType must have the same alignof as base::UnguessableToken");
};

////////////////////////////////////////////////////////////////////////////////
// MultiTokenAllVariantsAreTokenType
//
// Ensures that all variants are of type util::TokenType.

template <typename... VariantTypes>
struct MultiTokenAllVariantsAreTokenType;

// Recursive case.
template <typename FirstVariantType, typename... OtherVariantTypes>
struct MultiTokenAllVariantsAreTokenType<FirstVariantType,
                                         OtherVariantTypes...> {
  static constexpr bool kValue =
      MultiTokenVariantIsTokenType<FirstVariantType>::kValue &&
      MultiTokenAllVariantsAreTokenType<OtherVariantTypes...>::kValue;
};

// Base case.
template <>
struct MultiTokenAllVariantsAreTokenType<> {
  static constexpr bool kValue = true;
};

////////////////////////////////////////////////////////////////////////////////
// MultiTokenTypeRepeated
//
// Determines if a QueryType is repeated in a variadic list of types.

template <typename QueryType, typename... VariantTypes>
struct MultiTokenTypeRepeated;

// Recursive case.
template <typename QueryType,
          typename FirstVariantType,
          typename... OtherVariantTypes>
struct MultiTokenTypeRepeated<QueryType,
                              FirstVariantType,
                              OtherVariantTypes...> {
  static constexpr size_t kCount =
      (std::is_same<QueryType, FirstVariantType>::value ? 1 : 0) +
      MultiTokenTypeRepeated<QueryType, OtherVariantTypes...>::kCount;
  static constexpr bool kValue = kCount > 1;
};

// Base case.
template <typename QueryType>
struct MultiTokenTypeRepeated<QueryType> {
  static constexpr size_t kCount = 0;
  static constexpr bool kValue = false;
};

////////////////////////////////////////////////////////////////////////////////
// MultiTokenAnyTypeRepeated
//
// Determines if any type is repeated in a variadic list of types.

template <typename... VariantTypes>
struct MultiTokenAnyTypeRepeated;

// Recursive case.
template <typename FirstVariantType, typename... OtherVariantTypes>
struct MultiTokenAnyTypeRepeated<FirstVariantType, OtherVariantTypes...> {
  static constexpr bool kValue =
      MultiTokenTypeRepeated<FirstVariantType,
                             FirstVariantType,
                             OtherVariantTypes...>::kValue ||
      MultiTokenAnyTypeRepeated<OtherVariantTypes...>::kValue;
};

// Base case.
template <>
struct MultiTokenAnyTypeRepeated<> {
  static constexpr bool kValue = false;
};

////////////////////////////////////////////////////////////////////////////////
// MultiTokenTypeIndex
//
// Returns the index of a QueryType from a variadic list of N types, or N if the
// QueryType is not found in the list.

template <typename QueryType, typename... VariantTypes>
struct MultiTokenTypeIndex;

// Recursive case.
template <typename QueryType,
          typename FirstVariantType,
          typename... OtherVariantTypes>
struct MultiTokenTypeIndex<QueryType, FirstVariantType, OtherVariantTypes...> {
  static constexpr size_t kValue =
      (std::is_same<QueryType, FirstVariantType>::value
           ? 0
           : (1 +
              MultiTokenTypeIndex<QueryType, OtherVariantTypes...>::kValue));
};

// Base case.
template <typename QueryType>
struct MultiTokenTypeIndex<QueryType> {
  static constexpr size_t kValue = 0;
};

////////////////////////////////////////////////////////////////////////////////
// MultiTokenBase
//
// Base class that brings helper structs into a single namespace for
// convenience.
template <typename... TokenVariants>
class MultiTokenBase {
 public:
  // Ensures that no types are repeated, as that's non-sensical.
  using AnyRepeated = internal::MultiTokenAnyTypeRepeated<TokenVariants...>;
  static_assert(!AnyRepeated::kValue, "input types must not be repeated");

  // Ensures that all variants are instances of util::TokenType.
  using AllVariantsAreTokenType =
      internal::MultiTokenAllVariantsAreTokenType<TokenVariants...>;
  static_assert(AllVariantsAreTokenType::kValue,
                "input types must be instances of util::TokenType");

  // Counts the number of variants.
  using VariantCount = internal::MultiTokenVariantCount<TokenVariants...>;

  // For determining the index of a type. Used to assign an integer ID to a
  // type, as a kind of untyped enum.
  template <typename QueryType>
  struct TypeIndex
      : public internal::MultiTokenTypeIndex<QueryType, TokenVariants...> {};

  // For determining if a type is valid for this variant. Useful in enable_if
  // statements.
  template <typename QueryType>
  struct ValidType {
    static constexpr bool kValue =
        TypeIndex<QueryType>::kValue != VariantCount::kValue;
  };

  // Helper comparator. Compares underlying types using only < and == to
  // return -1, 0, or 1 depending on their relative values.
  template <typename InputType>
  static int CompareImpl(const InputType& lhs, const InputType& rhs) {
    if (lhs < rhs)
      return -1;
    if (lhs == rhs)
      return 0;
    DCHECK(rhs < lhs);
    return 1;
  }
};

}  // namespace internal

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_TOKENS_MULTI_TOKEN_INTERNAL_H_
