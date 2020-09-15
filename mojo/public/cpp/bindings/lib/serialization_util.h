// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_SERIALIZATION_UTIL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_SERIALIZATION_UTIL_H_

#include <stddef.h>
#include <stdint.h>

#include <queue>
#include <type_traits>

#include "base/logging.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/lib/serialization_context.h"
#include "mojo/public/cpp/bindings/lib/serialization_forward.h"
#include "mojo/public/cpp/bindings/lib/template_util.h"

namespace mojo {
namespace internal {

template <typename T>
struct HasIsNullMethod {
  template <typename U>
  static char Test(decltype(U::IsNull) *);
  template <typename U>
  static int Test(...);
  static const bool value = sizeof(Test<T>(0)) == sizeof(char);

 private:
  EnsureTypeIsComplete<T> check_t_;
};

template <
    typename Traits,
    typename UserType,
    typename std::enable_if<HasIsNullMethod<Traits>::value>::type* = nullptr>
bool CallIsNullIfExists(const UserType& input) {
  return Traits::IsNull(input);
}

template <
    typename Traits,
    typename UserType,
    typename std::enable_if<!HasIsNullMethod<Traits>::value>::type* = nullptr>
bool CallIsNullIfExists(const UserType& input) {
  return false;
}
template <typename T>
struct HasSetToNullMethod {
  template <typename U>
  static char Test(decltype(U::SetToNull) *);
  template <typename U>
  static int Test(...);
  static const bool value = sizeof(Test<T>(0)) == sizeof(char);

 private:
  EnsureTypeIsComplete<T> check_t_;
};

template <
    typename Traits,
    typename UserType,
    typename std::enable_if<HasSetToNullMethod<Traits>::value>::type* = nullptr>
bool CallSetToNullIfExists(UserType* output) {
  Traits::SetToNull(output);
  return true;
}

template <typename Traits,
          typename UserType,
          typename std::enable_if<!HasSetToNullMethod<Traits>::value>::type* =
              nullptr>
bool CallSetToNullIfExists(UserType* output) {
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

template <typename MojomType,
          typename UserType,
          typename std::enable_if<IsOptionalWrapper<UserType>::value>::type* =
              nullptr>
constexpr bool IsValidUserTypeForOptionalValue() {
  return true;
}

template <typename MojomType,
          typename UserType,
          typename std::enable_if<!IsOptionalWrapper<UserType>::value>::type* =
              nullptr>
constexpr bool IsValidUserTypeForOptionalValue() {
  using Traits = typename TraitsFinder<MojomType, UserType>::Traits;
  return HasSetToNullMethod<Traits>::value;
}

template <typename T, typename MaybeConstUserType>
struct HasGetBeginMethod {
  template <typename U>
  static char Test(
      decltype(U::GetBegin(std::declval<MaybeConstUserType&>())) *);
  template <typename U>
  static int Test(...);
  static const bool value = sizeof(Test<T>(0)) == sizeof(char);

 private:
  EnsureTypeIsComplete<T> check_t_;
};

template <
    typename Traits,
    typename MaybeConstUserType,
    typename std::enable_if<
        HasGetBeginMethod<Traits, MaybeConstUserType>::value>::type* = nullptr>
decltype(Traits::GetBegin(std::declval<MaybeConstUserType&>()))
CallGetBeginIfExists(MaybeConstUserType& input) {
  return Traits::GetBegin(input);
}

template <
    typename Traits,
    typename MaybeConstUserType,
    typename std::enable_if<
        !HasGetBeginMethod<Traits, MaybeConstUserType>::value>::type* = nullptr>
size_t CallGetBeginIfExists(MaybeConstUserType& input) {
  return 0;
}

template <typename T, typename MaybeConstUserType>
struct HasGetDataMethod {
  template <typename U>
  static char Test(decltype(U::GetData(std::declval<MaybeConstUserType&>())) *);
  template <typename U>
  static int Test(...);
  static const bool value = sizeof(Test<T>(0)) == sizeof(char);

 private:
  EnsureTypeIsComplete<T> check_t_;
};

template <
    typename Traits,
    typename MaybeConstUserType,
    typename std::enable_if<
        HasGetDataMethod<Traits, MaybeConstUserType>::value>::type* = nullptr>
decltype(Traits::GetData(std::declval<MaybeConstUserType&>()))
CallGetDataIfExists(MaybeConstUserType& input) {
  return Traits::GetData(input);
}

template <
    typename Traits,
    typename MaybeConstUserType,
    typename std::enable_if<
        !HasGetDataMethod<Traits, MaybeConstUserType>::value>::type* = nullptr>
void* CallGetDataIfExists(MaybeConstUserType& input) {
  return nullptr;
}

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_SERIALIZATION_UTIL_H_
