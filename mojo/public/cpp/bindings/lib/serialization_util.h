// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_SERIALIZATION_UTIL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_SERIALIZATION_UTIL_H_

#include <stddef.h>

#include <type_traits>
#include <utility>

#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/lib/serialization_forward.h"
#include "mojo/public/cpp/bindings/lib/template_util.h"

namespace mojo {
namespace internal {

template <typename T, typename U, typename SFINAE = void>
struct HasIsNullMethod : std::false_type {
  static_assert(sizeof(T), "T must be a complete type.");
};

template <typename T, typename U>
struct HasIsNullMethod<
    T,
    U,
    std::void_t<decltype(T::IsNull(std::declval<const U&>()))>>
    : std::true_type {};

template <typename Traits, typename UserType>
bool CallIsNullIfExists(const UserType& input) {
  if constexpr (HasIsNullMethod<Traits, UserType>::value) {
    return Traits::IsNull(input);
  } else {
    return false;
  }
}

template <typename T, typename U, typename SFINAE = void>
struct HasSetToNullMethod : std::false_type {
  static_assert(sizeof(T), "T must be a complete type.");
};

template <typename T, typename U>
struct HasSetToNullMethod<
    T,
    U,
    std::void_t<decltype(T::SetToNull(std::declval<U*>()))>> : std::true_type {
};

template <typename Traits, typename UserType>
bool CallSetToNullIfExists(UserType* output) {
  if constexpr (HasSetToNullMethod<Traits, UserType>::value) {
    Traits::SetToNull(output);
  }

  // Note that it is not considered an error to attempt to read a null value
  // into a non-nullable `output` object. In such cases, the caller must have
  // used a DataView's corresponding MaybeRead[FieldName] method, thus
  // explicitly choosing to ignore null values without affecting `output`.
  //
  // If instead a caller unwittingly attempts to use a corresponding
  // Read[FieldName] method to read an optional value into the same non-nullable
  // UserType, it will result in a compile-time error. As such, we don't need to
  // account for that case here.
  return true;
}

template <typename MojomType, typename UserType, typename = void>
struct TraitsFinder {
  using Traits = StructTraits<MojomType, UserType>;
};

template <typename MojomType, typename UserType>
struct TraitsFinder<
    MojomType,
    UserType,
    std::enable_if_t<BelongsTo<MojomType, MojomTypeCategory::kUnion>::value>> {
  using Traits = UnionTraits<MojomType, UserType>;
};

template <typename UserType>
struct TraitsFinder<StringDataView, UserType> {
  using Traits = StringTraits<UserType>;
};

template <typename UserType, typename ElementType>
struct TraitsFinder<ArrayDataView<ElementType>, UserType> {
  using Traits = ArrayTraits<UserType>;
};

template <typename UserType, typename KeyType, typename ValueType>
struct TraitsFinder<MapDataView<KeyType, ValueType>, UserType> {
  using Traits = MapTraits<UserType>;
};

template <typename MojomType, typename UserType>
constexpr bool IsValidUserTypeForOptionalValue() {
  if constexpr (IsAbslOptional<UserType>::value) {
    return true;
  } else {
    using Traits = typename TraitsFinder<MojomType, UserType>::Traits;
    return HasSetToNullMethod<Traits, UserType>::value;
  }
}

template <typename T, typename U, typename SFINAE = void>
struct HasGetBeginMethod : std::false_type {
  static_assert(sizeof(T), "T must be a complete type.");
};

template <typename T, typename U>
struct HasGetBeginMethod<T,
                         U,
                         std::void_t<decltype(T::GetBegin(std::declval<U&>()))>>
    : std::true_type {};

template <typename T, typename U, typename SFINAE = void>
struct HasGetDataMethod : std::false_type {
  static_assert(sizeof(T), "T must be a complete type.");
};

// TODO(dcheng): Figure out why the `&` below is load-bearing and document it or
// improve this and remove the hack.
template <typename T, typename U>
struct HasGetDataMethod<T,
                        U,
                        std::void_t<decltype(&T::GetData(std::declval<U&>()))>>
    : std::true_type {};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_SERIALIZATION_UTIL_H_
