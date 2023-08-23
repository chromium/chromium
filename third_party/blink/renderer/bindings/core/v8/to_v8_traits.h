// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_TO_V8_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_TO_V8_TRAITS_H_

#include "base/numerics/safe_conversions.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/platform/bindings/dom_data_store.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "v8/include/v8.h"

namespace blink {

class CallbackFunctionBase;
class CallbackInterfaceBase;
class ScriptWrappable;

namespace bindings {

class DictionaryBase;
class EnumerationBase;
class UnionBase;

}  // namespace bindings

// ToV8Traits provides C++ -> V8 conversion.
// Currently, you can use ToV8() which is defined in to_v8.h for this
// conversion, but it cannot throw an exception when an error occurs.
// We will solve this problem and replace ToV8() in to_v8.h with
// ToV8Traits::ToV8().
// TODO(canonmukai): Replace existing ToV8() with ToV8Traits<>.

// Primary template for ToV8Traits.
template <typename T, typename SFINAEHelper = void>
struct ToV8Traits;

// Any
template <>
struct ToV8Traits<IDLAny> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      ScriptState* script_state,
      const ScriptValue& script_value) {
    // It is not correct to take empty |script_value|.
    // However, some call sites expect to get v8::Undefined
    // when ToV8 takes empty |script_value|.
    // TODO(crbug.com/1183637): Enable the following DCHECK.
    // DCHECK(!script_value.IsEmpty());
    return script_value.V8ValueFor(script_state);
  }

  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      ScriptState* script_state,
      const v8::Local<v8::Value>& value) {
    // TODO(crbug.com/1183637): Remove this if-branch.
    if (value.IsEmpty())
      return v8::Undefined(script_state->GetIsolate());
    return value;
  }
};

// Boolean
template <>
struct ToV8Traits<IDLBoolean> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      bool value) {
    return v8::Boolean::New(script_state->GetIsolate(), value);
  }
};

// Bigint
template <>
struct ToV8Traits<IDLBigint> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      const BigInt& bigint) {
    return bigint.ToV8(script_state->GetContext());
  }
};

// Integer
// int8_t
template <bindings::IDLIntegerConvMode mode>
struct ToV8Traits<IDLIntegerTypeBase<int8_t, mode>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      int8_t value) {
    return v8::Integer::New(script_state->GetIsolate(), value);
  }
};

// int16_t
template <bindings::IDLIntegerConvMode mode>
struct ToV8Traits<IDLIntegerTypeBase<int16_t, mode>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      int16_t value) {
    return v8::Integer::New(script_state->GetIsolate(), value);
  }
};

// int32_t
template <bindings::IDLIntegerConvMode mode>
struct ToV8Traits<IDLIntegerTypeBase<int32_t, mode>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      int32_t value) {
    return v8::Integer::New(script_state->GetIsolate(), value);
  }
};

// int64_t
template <bindings::IDLIntegerConvMode mode>
struct ToV8Traits<IDLIntegerTypeBase<int64_t, mode>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      int64_t value) {
    int32_t value_in_32bit = static_cast<int32_t>(value);
    if (value_in_32bit == value)
      return v8::Integer::New(script_state->GetIsolate(), value_in_32bit);
    // v8::Integer cannot represent 64-bit integers.
    return v8::Number::New(script_state->GetIsolate(), value);
  }
};

// uint8_t
template <bindings::IDLIntegerConvMode mode>
struct ToV8Traits<IDLIntegerTypeBase<uint8_t, mode>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      uint8_t value) {
    return v8::Integer::NewFromUnsigned(script_state->GetIsolate(), value);
  }
};

// uint16_t
template <bindings::IDLIntegerConvMode mode>
struct ToV8Traits<IDLIntegerTypeBase<uint16_t, mode>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      uint16_t value) {
    return v8::Integer::NewFromUnsigned(script_state->GetIsolate(), value);
  }
};

// uint32_t
template <bindings::IDLIntegerConvMode mode>
struct ToV8Traits<IDLIntegerTypeBase<uint32_t, mode>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      uint32_t value) {
    return v8::Integer::NewFromUnsigned(script_state->GetIsolate(), value);
  }
};

// uint64_t
template <bindings::IDLIntegerConvMode mode>
struct ToV8Traits<IDLIntegerTypeBase<uint64_t, mode>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      uint64_t value) {
    uint32_t value_in_32bit = static_cast<uint32_t>(value);
    if (value_in_32bit == value) {
      return v8::Integer::NewFromUnsigned(script_state->GetIsolate(),
                                          value_in_32bit);
    }
    // v8::Integer cannot represent 64-bit integers.
    return v8::Number::New(script_state->GetIsolate(), value);
  }
};

// Floating Point Number
template <typename T, bindings::IDLFloatingPointNumberConvMode mode>
struct ToV8Traits<IDLFloatingPointNumberTypeBase<T, mode>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      T value) {
    return v8::Number::New(script_state->GetIsolate(), value);
  }
};

// String
template <typename T>
struct ToV8Traits<
    T,
    std::enable_if_t<std::is_base_of<IDLStringTypeBase, T>::value>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      const String& value) {
    // if |value| is a null string, V8String() returns an empty string.
    return V8String(script_state->GetIsolate(), value);
  }
};

// Object
template <>
struct ToV8Traits<IDLObject> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      ScriptState* script_state,
      const v8::Local<v8::Object>& value) {
    DCHECK(!value.IsEmpty());
    return value;
  }

  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      ScriptState* script_state,
      const ScriptValue& script_value) {
    DCHECK(!script_value.IsEmpty());
    v8::Local<v8::Value> v8_value = script_value.V8ValueFor(script_state);
    // TODO(crbug.com/1185033): Change this if-branch to DCHECK.
    if (!v8_value->IsObject())
      return v8::Undefined(script_state->GetIsolate());
    return v8_value;
  }
};

// Promise
template <>
struct ToV8Traits<IDLPromise> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      ScriptState* script_state,
      const ScriptPromise& script_promise) {
    DCHECK(!script_promise.IsEmpty());
    return script_promise.V8Value();
  }
};

// ScriptWrappable

namespace bindings {

// Helper function for ScriptWrappable
inline v8::MaybeLocal<v8::Value> ToV8HelperScriptWrappable(
    ScriptState* script_state,
    ScriptWrappable* script_wrappable) {
  CHECK(script_wrappable);
  v8::Local<v8::Value> wrapper =
      DOMDataStore::GetWrapper(script_wrappable, script_state->GetIsolate());
  if (!wrapper.IsEmpty()) {
    return wrapper;
  }

  return script_wrappable->Wrap(script_state);
}

// For optimization
inline v8::MaybeLocal<v8::Value> ToV8HelperScriptWrappable(
    v8::Isolate* isolate,
    ScriptWrappable* script_wrappable,
    v8::Local<v8::Object> creation_context_object) {
  CHECK(script_wrappable);
  v8::Local<v8::Value> wrapper =
      DOMDataStore::GetWrapper(script_wrappable, isolate);
  if (!wrapper.IsEmpty()) {
    return wrapper;
  }

  CHECK(!creation_context_object.IsEmpty());
  ScriptState* script_state =
      ScriptState::From(creation_context_object->GetCreationContextChecked());
  return script_wrappable->Wrap(script_state);
}

}  // namespace bindings

template <typename T>
struct ToV8Traits<
    T,
    std::enable_if_t<std::is_base_of<ScriptWrappable, T>::value>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      T* script_wrappable) {
    return bindings::ToV8HelperScriptWrappable(script_state, script_wrappable);
  }

  // This overload is used for the case when a ToV8 caller does not have
  // |script_state| but has a receiver object (a creation context object)
  // which is needed to create a wrapper. If a wrapper object corresponding to
  // the receiver object exists, ToV8 can return it without a call to
  // CreationContext() which is slow.
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      v8::Isolate* isolate,
      T* script_wrappable,
      v8::Local<v8::Object> creation_context_object) {
    return bindings::ToV8HelperScriptWrappable(isolate, script_wrappable,
                                               creation_context_object);
  }
};

// Dictionary
template <typename T>
struct ToV8Traits<
    T,
    std::enable_if_t<std::is_base_of<bindings::DictionaryBase, T>::value>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      const T* dictionary) {
    DCHECK(dictionary);
    return dictionary->ToV8Value(script_state);
  }
};

// Callback function
template <typename T>
struct ToV8Traits<
    T,
    std::enable_if_t<std::is_base_of<CallbackFunctionBase, T>::value>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      T* callback) {
    // creation_context (|script_state->GetContext()|) is intentionally ignored.
    // Callback functions are not wrappers nor clonable. ToV8 on a callback
    // function must be used only when it's in the same world.
    DCHECK(callback);
    DCHECK(&callback->GetWorld() == &script_state->World());
    return callback->CallbackObject().template As<v8::Value>();
  }
};

// Callback interface
template <typename T>
struct ToV8Traits<
    T,
    std::enable_if_t<std::is_base_of<CallbackInterfaceBase, T>::value>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      T* callback) {
    // creation_context (|script_state->GetContext()|) is intentionally ignored.
    // Callback Interfaces are not wrappers nor clonable. ToV8 on a callback
    // interface must be used only when it's in the same world.
    DCHECK(callback);
    DCHECK(&callback->GetWorld() == &script_state->World());
    return callback->CallbackObject().template As<v8::Value>();
  }
};

// Enumeration
template <typename T>
struct ToV8Traits<
    T,
    std::enable_if_t<std::is_base_of<bindings::EnumerationBase, T>::value>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      const T& enumeration) {
    return V8String(script_state->GetIsolate(), enumeration.AsCStr());
  }

  // TODO(crbug.com/1184543): Remove this overload.
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      const String& value) {
    DCHECK(!value.empty());
    return V8String(script_state->GetIsolate(), value);
  }

  // TODO(crbug.com/1184543): Remove this overload.
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      ScriptState* script_state,
      const AtomicString& value) {
    DCHECK(!value.empty());
    return V8String(script_state->GetIsolate(), value);
  }
};

// NotShared
template <typename T>
struct ToV8Traits<NotShared<T>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      NotShared<T> value) {
    DCHECK(!value.IsNull());
    return ToV8Traits<T>::ToV8(script_state, value.Get());
  }

  // TODO(crbug.com/1183647): Remove this overload. It is used in generated
  // code, but it might cause bugs because T* cannot tell whether it's NotShared
  // or MaybeShared.
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      T* value) {
    // TODO(canonmukai): Remove this if-branch and add DCHECK(value) instead.
    if (!value)
      return v8::Null(script_state->GetIsolate());
    return ToV8Traits<T>::ToV8(script_state, value);
  }
};

// MaybeShared
template <typename T>
struct ToV8Traits<MaybeShared<T>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      MaybeShared<T> value) {
    return ToV8Traits<T>::ToV8(script_state, value.Get());
  }
};

// Array

namespace bindings {

// Helper function for IDLSequence
template <typename ElementIDLType, typename VectorType>
inline v8::MaybeLocal<v8::Value> ToV8HelperSequence(ScriptState* script_state,
                                                    const VectorType& vector) {
  Vector<v8::Local<v8::Value>> converted_vector(vector.size());
  for (wtf_size_t i = 0; i < vector.size(); ++i) {
    v8::Local<v8::Value> v8_value;
    if (!ToV8Traits<ElementIDLType>::ToV8(script_state, vector[i])
             .ToLocal(&v8_value)) {
      return v8::Local<v8::Value>();
    }
    converted_vector[i] = v8_value;
  }
  return v8::Array::New(script_state->GetIsolate(), converted_vector.data(),
                        base::checked_cast<int>(converted_vector.size()));
}

// Helper function for IDLSequence in the case using reinterpret_cast
// In order to reduce code size, avoids template instantiation of
// ToV8HelperSequence<T> where T is a subclass of bindings::DictionaryBase or
// ScriptWrappable. Since these base classes are the leftmost base class,
// HeapVector<Member<TheBase>> has the same binary representation with
// HeapVector<Member<T>>. We leverage this fact to reduce the APK size.
//
// This hack reduces the APK size by 4 Kbytes as of 2021 March.
template <typename BaseClassOfT, typename T>
inline v8::MaybeLocal<v8::Value> ToV8HelperSequenceWithMemberUpcast(
    ScriptState* script_state,
    const HeapVector<Member<T>>* vector) {
  return ToV8HelperSequence<BaseClassOfT>(
      script_state,
      *reinterpret_cast<const HeapVector<Member<BaseClassOfT>>*>(vector));
}

template <typename BaseClassOfT, typename T>
inline v8::MaybeLocal<v8::Value> ToV8HelperSequenceWithMemberUpcast(
    ScriptState* script_state,
    const HeapVector<Member<const T>>* vector) {
  return ToV8HelperSequence<BaseClassOfT>(
      script_state,
      *reinterpret_cast<const HeapVector<Member<BaseClassOfT>>*>(vector));
}

// Helper function for IDLRecord
template <typename ValueIDLType, typename VectorType>
inline v8::MaybeLocal<v8::Value> ToV8HelperRecord(ScriptState* script_state,
                                                  const VectorType& vector) {
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::Local<v8::Object> object;
  {
    v8::Context::Scope context_scope(script_state->GetContext());
    object = v8::Object::New(isolate);
  }
  v8::Local<v8::Context> context = script_state->GetContext();
  typename VectorType::const_iterator end = vector.end();
  for (typename VectorType::const_iterator iter = vector.begin(); iter != end;
       ++iter) {
    v8::Local<v8::Value> v8_value;
    if (!ToV8Traits<ValueIDLType>::ToV8(script_state, iter->second)
             .ToLocal(&v8_value)) {
      return v8::MaybeLocal<v8::Value>();
    }
    bool is_property_created;
    if (!object
             ->CreateDataProperty(context, V8AtomicString(isolate, iter->first),
                                  v8_value)
             .To(&is_property_created) ||
        !is_property_created) {
      return v8::Local<v8::Value>();
    }
  }
  return object;
}

}  // namespace bindings

// IDLSequence
template <typename T>
struct ToV8Traits<
    IDLSequence<T>,
    std::enable_if_t<std::is_base_of<bindings::DictionaryBase, T>::value>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      ScriptState* script_state,
      const HeapVector<Member<T>>& value) {
    return bindings::ToV8HelperSequenceWithMemberUpcast<
        bindings::DictionaryBase>(script_state, &value);
  }

  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      ScriptState* script_state,
      const HeapVector<Member<const T>>& value) {
    return bindings::ToV8HelperSequenceWithMemberUpcast<
        bindings::DictionaryBase>(script_state, &value);
  }

  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      ScriptState* script_state,
      const HeapVector<Member<T>>* value) {
    DCHECK(value);
    return bindings::ToV8HelperSequenceWithMemberUpcast<
        bindings::DictionaryBase>(script_state, value);
  }

  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      ScriptState* script_state,
      const HeapVector<Member<const T>>* value) {
    DCHECK(value);
    return bindings::ToV8HelperSequenceWithMemberUpcast<
        bindings::DictionaryBase>(script_state, value);
  }

  // TODO(crbug.com/1185046): Remove this overload.
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      ScriptState* script_state,
      const Vector<v8::Local<v8::Value>>& value) {
    return bindings::ToV8HelperSequence<IDLAny>(script_state, value);
  }
};

template <typename T>
struct ToV8Traits<
    IDLSequence<T>,
    std::enable_if_t<std::is_base_of<ScriptWrappable, T>::value>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      ScriptState* script_state,
      const HeapVector<Member<T>>& value) {
    return bindings::ToV8HelperSequenceWithMemberUpcast<ScriptWrappable>(
        script_state, &value);
  }

  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      ScriptState* script_state,
      const HeapVector<Member<const T>>& value) {
    return bindings::ToV8HelperSequenceWithMemberUpcast<ScriptWrappable>(
        script_state, &value);
  }

  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      ScriptState* script_state,
      const HeapVector<Member<T>>* value) {
    DCHECK(value);
    return bindings::ToV8HelperSequenceWithMemberUpcast<ScriptWrappable>(
        script_state, value);
  }

  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      ScriptState* script_state,
      const HeapVector<Member<const T>>* value) {
    DCHECK(value);
    return bindings::ToV8HelperSequenceWithMemberUpcast<ScriptWrappable>(
        script_state, value);
  }
};

template <typename T>
struct ToV8Traits<
    IDLSequence<T>,
    std::enable_if_t<std::is_base_of<bindings::UnionBase, T>::value>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      ScriptState* script_state,
      const HeapVector<Member<T>>& value) {
    return bindings::ToV8HelperSequenceWithMemberUpcast<bindings::UnionBase>(
        script_state, &value);
  }

  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      ScriptState* script_state,
      const HeapVector<Member<const T>>& value) {
    return bindings::ToV8HelperSequenceWithMemberUpcast<bindings::UnionBase>(
        script_state, &value);
  }

  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      ScriptState* script_state,
      const HeapVector<Member<T>>* value) {
    DCHECK(value);
    return bindings::ToV8HelperSequenceWithMemberUpcast<bindings::UnionBase>(
        script_state, value);
  }

  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      ScriptState* script_state,
      const HeapVector<Member<const T>>* value) {
    DCHECK(value);
    return bindings::ToV8HelperSequenceWithMemberUpcast<bindings::UnionBase>(
        script_state, value);
  }
};

template <typename T>
struct ToV8Traits<
    IDLSequence<T>,
    std::enable_if_t<!std::is_base_of<bindings::DictionaryBase, T>::value &&
                     !std::is_base_of<ScriptWrappable, T>::value &&
                     !std::is_base_of<bindings::UnionBase, T>::value>> {
  template <typename VectorType>
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      const VectorType& value) {
    return bindings::ToV8HelperSequence<T>(script_state, value);
  }

  template <typename VectorType>
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      const VectorType* value) {
    DCHECK(value);
    return bindings::ToV8HelperSequence<T>(script_state, *value);
  }

  template <typename VectorType>
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      VectorType* value) {
    DCHECK(value);
    return bindings::ToV8HelperSequence<T>(script_state, *value);
  }
};

// IDLArray
template <typename T>
struct ToV8Traits<IDLArray<T>> {
  template <typename VectorType>
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      const VectorType& value) {
    v8::Local<v8::Value> v8_value;
    if (!ToV8Traits<IDLSequence<T>>::ToV8(script_state, value)
             .ToLocal(&v8_value)) {
      return v8::MaybeLocal<v8::Value>();
    }
    v8_value.As<v8::Object>()->SetIntegrityLevel(script_state->GetContext(),
                                                 v8::IntegrityLevel::kFrozen);
    return v8_value;
  }

  template <typename VectorType>
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      const VectorType* value) {
    DCHECK(value);
    v8::Local<v8::Value> v8_value;
    if (!ToV8Traits<IDLSequence<T>>::ToV8(script_state, *value)
             .ToLocal(&v8_value)) {
      return v8::MaybeLocal<v8::Value>();
    }
    v8_value.As<v8::Object>()->SetIntegrityLevel(script_state->GetContext(),
                                                 v8::IntegrityLevel::kFrozen);
    return v8_value;
  }

  template <typename VectorType>
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      VectorType* value) {
    DCHECK(value);
    v8::Local<v8::Value> v8_value;
    if (!ToV8Traits<IDLSequence<T>>::ToV8(script_state, *value)
             .ToLocal(&v8_value)) {
      return v8::MaybeLocal<v8::Value>();
    }
    v8_value.As<v8::Object>()->SetIntegrityLevel(script_state->GetContext(),
                                                 v8::IntegrityLevel::kFrozen);
    return v8_value;
  }

  // TODO(crbug.com/1185046): Remove this overload.
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      ScriptState* script_state,
      const Vector<v8::Local<v8::Value>>& value) {
    v8::Local<v8::Value> v8_value;
    if (!ToV8Traits<IDLSequence<IDLAny>>::ToV8(script_state, value)
             .ToLocal(&v8_value)) {
      return v8::MaybeLocal<v8::Value>();
    }
    v8_value.As<v8::Object>()->SetIntegrityLevel(script_state->GetContext(),
                                                 v8::IntegrityLevel::kFrozen);
    return v8_value;
  }
};

// IDLRecord
// K must be based of IDL String types.
template <typename K, typename V>
struct ToV8Traits<IDLRecord<K, V>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      ScriptState* script_state,
      const typename IDLRecord<K, V>::ImplType& value) {
    return bindings::ToV8HelperRecord<V>(script_state, value);
  }

  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      ScriptState* script_state,
      const typename IDLRecord<K, V>::ImplType* value) {
    DCHECK(value);
    return bindings::ToV8HelperRecord<V>(script_state, *value);
  }
};

// Nullable

// IDLNullable<IDLNullable<T>> must not be used.
template <typename T>
struct ToV8Traits<IDLNullable<IDLNullable<T>>>;

// Nullable Boolean
template <>
struct ToV8Traits<IDLNullable<IDLBoolean>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      ScriptState* script_state,
      const absl::optional<bool>& value) {
    if (!value)
      return v8::Null(script_state->GetIsolate());
    return ToV8Traits<IDLBoolean>::ToV8(script_state, *value);
  }
};

// Nullable Integers
template <typename T, bindings::IDLIntegerConvMode mode>
struct ToV8Traits<IDLNullable<IDLIntegerTypeBase<T, mode>>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      ScriptState* script_state,
      const absl::optional<T>& value) {
    if (!value)
      return v8::Null(script_state->GetIsolate());
    return ToV8Traits<IDLIntegerTypeBase<T, mode>>::ToV8(script_state, *value);
  }
};

// Nullable Floating Point Number
template <typename T, bindings::IDLFloatingPointNumberConvMode mode>
struct ToV8Traits<IDLNullable<IDLFloatingPointNumberTypeBase<T, mode>>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      ScriptState* script_state,
      const absl::optional<T>& value) {
    if (!value)
      return v8::Null(script_state->GetIsolate());
    return ToV8Traits<IDLFloatingPointNumberTypeBase<T, mode>>::ToV8(
        script_state, *value);
  }
};

// Nullable Strings
template <typename T>
struct ToV8Traits<
    IDLNullable<T>,
    std::enable_if_t<std::is_base_of<IDLStringTypeBase, T>::value>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      const String& value) {
    if (!value)
      return v8::Null(script_state->GetIsolate());
    return ToV8Traits<T>::ToV8(script_state, value);
  }
};

// Nullable Object
template <>
struct ToV8Traits<IDLNullable<IDLObject>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      ScriptState* script_state,
      const ScriptValue& script_value) {
    // TODO(crbug.com/1183637): Remove this if-branch.
    if (script_value.IsEmpty())
      return v8::Null(script_state->GetIsolate());

    v8::Local<v8::Value> v8_value = script_value.V8ValueFor(script_state);
    DCHECK(v8_value->IsNull() || v8_value->IsObject());
    return v8_value;
  }
};

// Nullable ScriptWrappable
template <typename T>
struct ToV8Traits<
    IDLNullable<T>,
    std::enable_if_t<std::is_base_of<ScriptWrappable, T>::value>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      T* script_wrappable) {
    if (!script_wrappable)
      return v8::Null(script_state->GetIsolate());
    return ToV8Traits<T>::ToV8(script_state, script_wrappable);
  }

  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      v8::Isolate* isolate,
      ScriptWrappable* script_wrappable,
      v8::Local<v8::Object> creation_context) {
    if (!script_wrappable)
      return v8::Null(isolate);
    return ToV8Traits<T>::ToV8(isolate, script_wrappable, creation_context);
  }
};

// Nullable Dictionary
template <typename T>
struct ToV8Traits<
    IDLNullable<T>,
    std::enable_if_t<std::is_base_of<bindings::DictionaryBase, T>::value>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      const T* dictionary) {
    if (!dictionary)
      return v8::Null(script_state->GetIsolate());
    return ToV8Traits<T>::ToV8(script_state, dictionary);
  }
};

// Nullable Callback function
template <typename T>
struct ToV8Traits<
    IDLNullable<T>,
    std::enable_if_t<std::is_base_of<CallbackFunctionBase, T>::value>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      T* callback) {
    if (!callback)
      return v8::Null(script_state->GetIsolate());
    return ToV8Traits<T>::ToV8(script_state, callback);
  }
};

// Nullable Callback interface
template <typename T>
struct ToV8Traits<
    IDLNullable<T>,
    std::enable_if_t<std::is_base_of<CallbackInterfaceBase, T>::value>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      T* callback) {
    if (!callback)
      return v8::Null(script_state->GetIsolate());
    return ToV8Traits<T>::ToV8(script_state, callback);
  }
};

// Nullable Enumeration
template <typename T>
struct ToV8Traits<
    IDLNullable<T>,
    std::enable_if_t<std::is_base_of<bindings::EnumerationBase, T>::value>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      ScriptState* script_state,
      const absl::optional<T>& enumeration) {
    if (!enumeration)
      return v8::Null(script_state->GetIsolate());
    return ToV8Traits<T>::ToV8(script_state, *enumeration);
  }
};

// Nullable NotShared
template <typename T>
struct ToV8Traits<IDLNullable<NotShared<T>>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      NotShared<T> value) {
    if (value.IsNull())
      return v8::Null(script_state->GetIsolate());
    return ToV8Traits<NotShared<T>>::ToV8(script_state, value);
  }

  // TODO(crbug.com/1183647): Remove this overload.
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      T* value) {
    if (!value)
      return v8::Null(script_state->GetIsolate());
    return ToV8Traits<NotShared<T>>::ToV8(script_state, value);
  }
};

// Nullable MaybeShared
template <typename T>
struct ToV8Traits<IDLNullable<MaybeShared<T>>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      MaybeShared<T> value) {
    if (value.IsNull())
      return v8::Null(script_state->GetIsolate());
    return ToV8Traits<MaybeShared<T>>::ToV8(script_state, value);
  }
};

// Nullable Sequence
template <typename T>
struct ToV8Traits<IDLNullable<IDLSequence<T>>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      ScriptState* script_state,
      const absl::optional<typename IDLSequence<T>::ImplType>& value) {
    if (!value)
      return v8::Null(script_state->GetIsolate());
    return ToV8Traits<IDLSequence<T>>::ToV8(script_state, *value);
  }

  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      ScriptState* script_state,
      const typename IDLSequence<T>::ImplType* value) {
    if (!value)
      return v8::Null(script_state->GetIsolate());
    return ToV8Traits<IDLSequence<T>>::ToV8(script_state, value);
  }
};

// Nullable Frozen Array
template <typename T>
struct ToV8Traits<IDLNullable<IDLArray<T>>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      ScriptState* script_state,
      const absl::optional<typename IDLArray<T>::ImplType>& value) {
    if (!value)
      return v8::Null(script_state->GetIsolate());
    return ToV8Traits<IDLArray<T>>::ToV8(script_state, *value);
  }

  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      ScriptState* script_state,
      const typename IDLArray<T>::ImplType* value) {
    if (!value)
      return v8::Null(script_state->GetIsolate());
    return ToV8Traits<IDLArray<T>>::ToV8(script_state, value);
  }
};

// Nullable Record
template <typename K, typename V>
struct ToV8Traits<IDLNullable<IDLRecord<K, V>>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      ScriptState* script_state,
      const absl::optional<typename IDLRecord<K, V>::ImplType>& value) {
    if (!value)
      return v8::Null(script_state->GetIsolate());
    return ToV8Traits<IDLRecord<K, V>>::ToV8(script_state, *value);
  }

  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      ScriptState* script_state,
      const typename IDLRecord<K, V>::ImplType* value) {
    if (!value)
      return v8::Null(script_state->GetIsolate());
    return ToV8Traits<IDLRecord<K, V>>::ToV8(script_state, value);
  }
};

// Nullable Date
// IDLDate must be used as IDLNullable<IDLDate>.
template <>
struct ToV8Traits<IDLNullable<IDLDate>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      ScriptState* script_state,
      const absl::optional<base::Time> date) {
    if (!date)
      return v8::Null(script_state->GetIsolate());
    return v8::Date::New(script_state->GetContext(),
                         date->ToJsTimeIgnoringNull())
        .ToLocalChecked();
  }
};

// Union types

namespace bindings {

// Helper function for Union
template <typename T>
inline v8::MaybeLocal<v8::Value> ToV8HelperUnion(ScriptState* script_state,
                                                 const T& value) {
  return ToV8(value, script_state->GetContext()->Global(),
              script_state->GetIsolate());
}

}  // namespace bindings

template <typename T>
struct ToV8Traits<
    T,
    std::enable_if_t<std::is_base_of<bindings::UnionBase, T>::value>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      const T* value) {
    // TODO(crbug.com/1185018): nullptr shouldn't be passed.  This should be
    // DCHECK(value);
    if (!value)
      return v8::Null(script_state->GetIsolate());
    return value->ToV8Value(script_state);
  }
};

template <typename T>
struct ToV8Traits<
    IDLNullable<T>,
    std::enable_if_t<std::is_base_of<bindings::UnionBase, T>::value>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      const T* value) {
    if (!value)
      return v8::Null(script_state->GetIsolate());
    return ToV8Traits<T>::ToV8(script_state, value);
  }
};

// Optional
template <typename T>
struct ToV8Traits<IDLOptional<T>> {
  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                                      const T* value) {
    if (!value)
      return v8::Undefined(script_state->GetIsolate());
    return ToV8Traits<T>::ToV8(script_state, value);
  }

  [[nodiscard]] static v8::MaybeLocal<v8::Value> ToV8(
      ScriptState* script_state,
      const ScriptValue& value) {
    if (value.IsEmpty())
      return v8::Undefined(script_state->GetIsolate());
    return ToV8Traits<T>::ToV8(script_state, value);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_TO_V8_TRAITS_H_
