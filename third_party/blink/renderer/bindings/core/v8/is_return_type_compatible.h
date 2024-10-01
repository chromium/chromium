// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_IS_RETURN_TYPE_COMPATIBLE_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_IS_RETURN_TYPE_COMPATIBLE_H_

#include <optional>
#include <type_traits>

#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_data_view.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"

namespace blink {

class KURL;

namespace bindings {

class EnumerationBase;

// A fall-through case.
template <typename IDLType, typename ReturnType>
inline constexpr bool IsReturnTypeCompatible = false;

// Any type is compatible to IDLAny (duh!)
template <typename IDLType>
inline constexpr bool IsReturnTypeCompatible<IDLAny, IDLType> = true;

// Any type is compatible to itself.
template <typename SameType>
inline constexpr bool IsReturnTypeCompatible<SameType, SameType> = true;

// Returning a wider type is fine.
template <typename IDLType, typename ReturnType>
  requires std::derived_from<std::remove_pointer_t<ReturnType>, IDLType>
inline constexpr bool IsReturnTypeCompatible<IDLType, ReturnType> = true;

// Pointers can be used to represent nullables, as long as types are compatible.
template <typename IDLType, typename ReturnType>
  requires IsReturnTypeCompatible<
               IDLType,
               std::remove_pointer_t<std::remove_const_t<ReturnType>>>
inline constexpr bool IsReturnTypeCompatible<IDLNullable<IDLType>, ReturnType> =
    true;

// std::optional<> is the way to implement nullable as long as inner types are
// compatible.
template <typename IDLType, typename ReturnType>
inline constexpr bool
    IsReturnTypeCompatible<IDLNullable<IDLType>, std::optional<ReturnType>> =
        IsReturnTypeCompatible<IDLType, ReturnType>;

namespace internal {

// An helper similar in spirit to `std::remove_pointer<>`.
template <typename T>
struct RemoveMember {
  using type = T;
};

template <typename T>
struct RemoveMember<Member<T>> {
  using type = T;
};

}  // namespace internal

// Anything that looks like a range of compatible types is compatible to an
// IDLSequence.
template <typename IDLType, typename ReturnType>
  requires std::ranges::range<ReturnType> &&
               IsReturnTypeCompatible<
                   IDLType,
                   typename internal::RemoveMember<
                       std::ranges::range_value_t<ReturnType>>::type>
inline constexpr bool
    IsReturnTypeCompatible<blink::IDLSequence<IDLType>, ReturnType> = true;

// ... as well as to an IDLArray.
template <typename IDLType, typename ReturnType>
  requires std::ranges::range<ReturnType> &&
               IsReturnTypeCompatible<
                   IDLType,
                   typename internal::RemoveMember<
                       std::ranges::range_value_t<ReturnType>>::type>
inline constexpr bool
    IsReturnTypeCompatible<blink::IDLArray<IDLType>, ReturnType> = true;

// IDL types derived from IDLBaseHelper already have associated impl type,
// so just use that.
template <typename IDLType, typename ReturnType>
  requires std::derived_from<IDLType, IDLBaseHelper<ReturnType>>
inline constexpr bool IsReturnTypeCompatible<IDLType, ReturnType> = true;

// TODO(caseq): this is an exception as it loses type checks, remove and take
// care of broken call sites!
template <typename IDLType>
inline constexpr bool IsReturnTypeCompatible<blink::IDLArray<IDLType>,
                                             v8::LocalVector<v8::Value>> = true;

// Just ignore NotShared when checking types.
template <typename ReturnType>
  requires std::derived_from<std::remove_pointer_t<ReturnType>,
                             DOMArrayBufferView>
inline constexpr bool
    IsReturnTypeCompatible<blink::NotShared<std::remove_pointer_t<ReturnType>>,
                           ReturnType> = true;

// IDLRecords are compatible to arrays of pairs, as long as key/value types
// are compatible between records and pairs.
template <typename IDLKey,
          typename IDLValue,
          typename ReturnKey,
          typename ReturnValue>
inline constexpr bool
    IsReturnTypeCompatible<blink::IDLRecord<IDLKey, IDLValue>,
                           WTF::Vector<std::pair<ReturnKey, ReturnValue>>> =
        IsReturnTypeCompatible<IDLKey, ReturnKey> &&
        IsReturnTypeCompatible<IDLValue, ReturnValue>;

template <typename IDLKey,
          typename IDLValue,
          typename ReturnKey,
          typename ReturnValue>
inline constexpr bool IsReturnTypeCompatible<
    blink::IDLRecord<IDLKey, IDLValue>,
    HeapVector<std::pair<ReturnKey, Member<ReturnValue>>>> =
    IsReturnTypeCompatible<IDLKey, ReturnKey> &&
    IsReturnTypeCompatible<IDLValue, ReturnValue>;

template <>
inline constexpr bool IsReturnTypeCompatible<IDLObject, v8::Local<v8::Object>> =
    true;

// TODO(caseq): this shouldn't really be allowed, as ScriptValue may carry
// values that are not objects, but keep it for now.
template <>
inline constexpr bool IsReturnTypeCompatible<IDLObject, ScriptValue> = true;

// TODO(caseq): this shouldn't really be allowed, as v8::Value may carry
// values that are not objects, but keep it for now.
template <>
inline constexpr bool IsReturnTypeCompatible<IDLObject, v8::Local<v8::Value>> =
    true;

// TODO(caseq): take care of this case, all promises should be returned as
// ScriptPromise<PromiseType> and IDLTypeImplementedAsV8Promise extended
// attribute should be removed.
template <typename PromiseType>
inline constexpr bool IsReturnTypeCompatible<blink::IDLPromise<PromiseType>,
                                             v8::Local<v8::Promise>> = true;

// Any IDL strings are compatible to any blink strings.
template <typename IDLStringType>
  requires std::derived_from<IDLStringType, IDLStringTypeBase>
inline constexpr bool IsReturnTypeCompatible<IDLStringType, WTF::String> = true;

template <typename IDLStringType>
  requires std::derived_from<IDLStringType, IDLStringTypeBase>
inline constexpr bool IsReturnTypeCompatible<IDLStringType, WTF::AtomicString> =
    true;

// TODO(caseq): allow enums to be returned as strings for now, but start
// enforcing enum-specific types latter.
template <typename IDLEnumeration>
  requires std::derived_from<IDLEnumeration, bindings::EnumerationBase>
inline constexpr bool IsReturnTypeCompatible<IDLEnumeration, WTF::String> =
    true;

template <typename IDLEnumeration>
  requires std::derived_from<IDLEnumeration, bindings::EnumerationBase>
inline constexpr bool
    IsReturnTypeCompatible<IDLEnumeration, WTF::AtomicString> = true;

// TODO(caseq): note we do not differentiate between double and float here. Not
// sure if there's a point considering representation would be the same on the
// JS side.
template <typename FloatType,
          blink::bindings::IDLFloatingPointNumberConvMode conv_mode>
inline constexpr bool IsReturnTypeCompatible<
    blink::IDLFloatingPointNumberTypeBase<FloatType, conv_mode>,
    float> = true;

template <typename FloatType,
          blink::bindings::IDLFloatingPointNumberConvMode conv_mode>
inline constexpr bool IsReturnTypeCompatible<
    blink::IDLFloatingPointNumberTypeBase<FloatType, conv_mode>,
    double> = true;

// Be relaxed about int types for now.
template <typename IDLIntType,
          typename NativeIntType,
          blink::bindings::IDLIntegerConvMode conv_mode>
inline constexpr bool
    IsReturnTypeCompatible<blink::IDLIntegerTypeBase<IDLIntType, conv_mode>,
                           NativeIntType> =
        std::is_convertible<NativeIntType, IDLIntType>::value ||
        std::is_enum<NativeIntType>::value;

// TODO(caseq): should we restrict KURLs to strings of particular type? Forbid
// implicit conversion altogether?
template <typename IDLStringType>
  requires std::derived_from<IDLStringType, IDLStringTypeBase>
inline constexpr bool IsReturnTypeCompatible<IDLStringType, blink::KURL> = true;

}  // namespace bindings
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_IS_RETURN_TYPE_COMPATIBLE_H_
