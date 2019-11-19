// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_NATIVE_VALUE_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_NATIVE_VALUE_TRAITS_H_

#include <type_traits>
#include "third_party/blink/renderer/bindings/core/v8/idl_types_base.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "v8/include/v8.h"

namespace blink {

class ExceptionState;

// NativeValueTraitsBase is supposed to be inherited by NativeValueTraits
// classes. They serve as a way to hold the ImplType typedef without requiring
// all NativeValueTraits specializations to declare it.
//
// The primary template below is used by NativeValueTraits specializations with
// types that do not inherit from IDLBase, in which case it is assumed the type
// of the specialization is also |ImplType|. The NativeValueTraitsBase
// specialization is used for IDLBase-based types, which are supposed to have
// their own |ImplType| typedefs.
//
// If present, |NullValue()| will be used when converting from the nullable type
// T?, and should be used if the impl type has an existing "null" state. If not
// present, WTF::Optional will be used to wrap the type.
template <typename T, typename SFINAEHelper = void>
struct NativeValueTraitsBase {
  using ImplType = T;
  STATIC_ONLY(NativeValueTraitsBase);
};

template <typename T>
struct NativeValueTraitsBase<
    T,
    typename std::enable_if<std::is_base_of<IDLBase, T>::value>::type> {
  using ImplType = typename T::ImplType;
  STATIC_ONLY(NativeValueTraitsBase);
};

// Primary template for NativeValueTraits. It is not supposed to be used
// directly: there needs to be a specialization for each type which represents
// a JavaScript type that will be converted to a C++ representation.
// Its main goal is to provide a standard interface for converting JS types
// into C++ ones.
//
// Example:
// template <>
// struct NativeValueTraits<IDLLong> : public NativeValueTraitsBase<IDLLong> {
//   static inline int32_t nativeValue(v8::Isolate* isolate,
//                                     v8::Local<v8::Value> value,
//                                     ExceptionState& exceptionState) {
//     return toInt32(isolate, value, exceptionState, NormalConversion);
//   }
// }
template <typename T, typename SFINAEHelper = void>
struct NativeValueTraits;

// This declaration serves only as a blueprint for specializations: the
// return type can change, but all specializations are expected to provide a
// NativeValue() method that takes the 3 arguments below.
//
// template <>
// struct NativeValueTraits<T>: public NativeValueTraitsBase<T> {
//   static inline typename NativeValueTraitsBase<T>::ImplType
//   NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
// };

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_NATIVE_VALUE_TRAITS_H_
