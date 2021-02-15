// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_TO_V8_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_TO_V8_TRAITS_H_

#include "third_party/blink/renderer/bindings/core/v8/idl_dictionary_base.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/platform/bindings/callback_function_base.h"
#include "third_party/blink/renderer/platform/bindings/callback_interface_base.h"
#include "third_party/blink/renderer/platform/bindings/dictionary_base.h"
#include "third_party/blink/renderer/platform/bindings/dom_data_store.h"
#include "third_party/blink/renderer/platform/bindings/enumeration_base.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "v8/include/v8.h"

namespace blink {

// ToV8Traits provides C++ -> V8 conversion.
// Currently, you can use ToV8() which is defined in to_v8.h for this
// conversion, but it cannot throw an exception when an error occurs.
// We will solve this problem and replace ToV8() in to_v8.h with
// ToV8Traits::ToV8().
// TODO(canonmukai): Replace existing ToV8() with ToV8Traits<>.

// Primary template for ToV8Traits.
template <typename T, typename SFINAEHelper = void>
struct ToV8Traits;

// Boolean
template <>
struct ToV8Traits<IDLBoolean> {
  static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                        bool value) WARN_UNUSED_RESULT {
    return v8::Boolean::New(script_state->GetIsolate(), value);
  }
};

// Integer
// int8_t
template <bindings::IDLIntegerConvMode mode>
struct ToV8Traits<IDLIntegerTypeBase<int8_t, mode>> {
  static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                        int8_t value) WARN_UNUSED_RESULT {
    return v8::Integer::New(script_state->GetIsolate(), value);
  }
};

// int16_t
template <bindings::IDLIntegerConvMode mode>
struct ToV8Traits<IDLIntegerTypeBase<int16_t, mode>> {
  static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                        int16_t value) WARN_UNUSED_RESULT {
    return v8::Integer::New(script_state->GetIsolate(), value);
  }
};

// int32_t
template <bindings::IDLIntegerConvMode mode>
struct ToV8Traits<IDLIntegerTypeBase<int32_t, mode>> {
  static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                        int32_t value) WARN_UNUSED_RESULT {
    return v8::Integer::New(script_state->GetIsolate(), value);
  }
};

// int64_t
template <bindings::IDLIntegerConvMode mode>
struct ToV8Traits<IDLIntegerTypeBase<int64_t, mode>> {
  static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                        int64_t value) WARN_UNUSED_RESULT {
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
  static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                        uint8_t value) WARN_UNUSED_RESULT {
    return v8::Integer::NewFromUnsigned(script_state->GetIsolate(), value);
  }
};

// uint16_t
template <bindings::IDLIntegerConvMode mode>
struct ToV8Traits<IDLIntegerTypeBase<uint16_t, mode>> {
  static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                        uint16_t value) WARN_UNUSED_RESULT {
    return v8::Integer::NewFromUnsigned(script_state->GetIsolate(), value);
  }
};

// uint32_t
template <bindings::IDLIntegerConvMode mode>
struct ToV8Traits<IDLIntegerTypeBase<uint32_t, mode>> {
  static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                        uint32_t value) WARN_UNUSED_RESULT {
    return v8::Integer::NewFromUnsigned(script_state->GetIsolate(), value);
  }
};

// uint64_t
template <bindings::IDLIntegerConvMode mode>
struct ToV8Traits<IDLIntegerTypeBase<uint64_t, mode>> {
  static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                        uint64_t value) WARN_UNUSED_RESULT {
    uint32_t value_in_32bit = static_cast<uint32_t>(value);
    if (value_in_32bit == value) {
      return v8::Integer::NewFromUnsigned(script_state->GetIsolate(),
                                          value_in_32bit);
    }
    // v8::Integer cannot represent 64-bit integers.
    return v8::Number::New(script_state->GetIsolate(), value);
  }
};

// Float
template <typename T>
struct ToV8Traits<T,
                  typename std::enable_if_t<
                      std::is_base_of<IDLBaseHelper<float>, T>::value>> {
  static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                        float value) WARN_UNUSED_RESULT {
    return v8::Number::New(script_state->GetIsolate(), value);
  }
};

// Double
template <typename T>
struct ToV8Traits<T,
                  typename std::enable_if_t<
                      std::is_base_of<IDLBaseHelper<double>, T>::value>> {
  static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                        double value) WARN_UNUSED_RESULT {
    return v8::Number::New(script_state->GetIsolate(), value);
  }
};

// String
template <typename T>
struct ToV8Traits<
    T,
    typename std::enable_if_t<
        std::is_same<IDLByteStringV2, T>::value ||
        std::is_same<IDLStringV2, T>::value ||
        std::is_same<IDLStringTreatNullAsEmptyStringV2, T>::value ||
        std::is_same<IDLUSVStringV2, T>::value ||
        std::is_same<IDLStringStringContextTrustedHTMLV2, T>::value ||
        std::is_same<IDLStringStringContextTrustedHTMLTreatNullAsEmptyStringV2,
                     T>::value ||
        std::is_same<IDLStringStringContextTrustedScriptV2, T>::value ||
        std::is_same<
            IDLStringStringContextTrustedScriptTreatNullAsEmptyStringV2,
            T>::value ||
        std::is_same<IDLUSVStringStringContextTrustedScriptURLV2, T>::value>> {
  static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                        const String& value)
      WARN_UNUSED_RESULT {
    // if |value| is a null string, V8String() returns an empty string.
    return V8String(script_state->GetIsolate(), value);
  }
};

// ScriptWrappable
template <typename T>
struct ToV8Traits<
    T,
    typename std::enable_if_t<std::is_base_of<ScriptWrappable, T>::value>> {
  static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                        T* script_wrappable)
      WARN_UNUSED_RESULT {
    CHECK(script_wrappable);
    v8::Local<v8::Value> wrapper =
        DOMDataStore::GetWrapper(script_wrappable, script_state->GetIsolate());
    if (!wrapper.IsEmpty()) {
      return wrapper;
    }

    if (!script_wrappable->WrapV2(script_state).ToLocal(&wrapper)) {
      return v8::MaybeLocal<v8::Value>();
    }
    return wrapper;
  }

  // This overload is used for the case when a ToV8 caller does not have
  // |script_state| but has a receiver object (a creation context object)
  // which is needed to create a wrapper. If a wrapper object corresponding to
  // the receiver object exists, ToV8 can return it without CreationContext()
  // which is slow.
  static v8::MaybeLocal<v8::Value> ToV8(
      v8::Isolate* isolate,
      T* script_wrappable,
      v8::Local<v8::Object> creation_context_object) WARN_UNUSED_RESULT {
    CHECK(script_wrappable);
    v8::Local<v8::Value> wrapper =
        DOMDataStore::GetWrapper(script_wrappable, isolate);
    if (!wrapper.IsEmpty()) {
      return wrapper;
    }

    CHECK(!creation_context_object.IsEmpty());
    ScriptState* script_state =
        ScriptState::From(creation_context_object->CreationContext());
    if (!script_wrappable->WrapV2(script_state).ToLocal(&wrapper)) {
      return v8::MaybeLocal<v8::Value>();
    }
    return wrapper;
  }
};

// Dictionary
template <typename T>
struct ToV8Traits<T,
                  typename std::enable_if_t<
                      std::is_base_of<bindings::DictionaryBase, T>::value>> {
  static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                        const T* dictionary)
      WARN_UNUSED_RESULT {
    DCHECK(dictionary);
    v8::Local<v8::Value> v8_value = dictionary->CreateV8Object(
        script_state->GetIsolate(), script_state->GetContext()->Global());
    DCHECK(!v8_value.IsEmpty());
    return v8_value;
  }
};

// Old implementation of Dictionary
template <typename T>
struct ToV8Traits<
    T,
    typename std::enable_if_t<std::is_base_of<IDLDictionaryBase, T>::value>> {
  static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                        const T* dictionary)
      WARN_UNUSED_RESULT {
    DCHECK(dictionary);
    return dictionary->ToV8Impl(script_state->GetContext()->Global(),
                                script_state->GetIsolate());
  }
};

// Callback function
template <typename T>
struct ToV8Traits<T,
                  typename std::enable_if_t<
                      std::is_base_of<CallbackFunctionBase, T>::value>> {
  static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                        T* callback) WARN_UNUSED_RESULT {
    // creation_context (|script_state->GetContext()|) is intentionally ignored.
    // Callback functions are not wrappers nor clonable. ToV8 on a callback
    // function must be used only when it's in the same world.
    DCHECK(callback);
    DCHECK(&callback->GetWorld() ==
           &ScriptState::From(script_state->GetContext())->World());
    return callback->CallbackObject().template As<v8::Value>();
  }
};

// Callback interface
template <typename T>
struct ToV8Traits<T,
                  typename std::enable_if_t<
                      std::is_base_of<CallbackInterfaceBase, T>::value>> {
  static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                        T* callback) WARN_UNUSED_RESULT {
    // creation_context (|script_state->GetContext()|) is intentionally ignored.
    // Callback Interfaces are not wrappers nor clonable. ToV8 on a callback
    // interface must be used only when it's in the same world.
    DCHECK(callback);
    DCHECK(&callback->GetWorld() ==
           &ScriptState::From(script_state->GetContext())->World());
    return callback->CallbackObject().template As<v8::Value>();
  }
};

// Enumeration
template <typename T>
struct ToV8Traits<T,
                  typename std::enable_if_t<
                      std::is_base_of<bindings::EnumerationBase, T>::value>> {
  static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                        const T& enumeration)
      WARN_UNUSED_RESULT {
    return V8String(script_state->GetIsolate(), enumeration.AsCStr());
  }
};

// Nullable

// IDLNullable<IDLNullable<T>> must not be used.
template <typename T>
struct ToV8Traits<IDLNullable<IDLNullable<T>>>;

// Nullable Boolean
template <>
struct ToV8Traits<IDLNullable<IDLBoolean>> {
  static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                        const base::Optional<bool>& value)
      WARN_UNUSED_RESULT {
    if (!value)
      return v8::Null(script_state->GetIsolate());
    return ToV8Traits<IDLBoolean>::ToV8(script_state, *value);
  }
};

// Nullable Integers
template <typename T, bindings::IDLIntegerConvMode mode>
struct ToV8Traits<IDLNullable<IDLIntegerTypeBase<T, mode>>> {
  static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                        const base::Optional<T>& value)
      WARN_UNUSED_RESULT {
    if (!value)
      return v8::Null(script_state->GetIsolate());
    return ToV8Traits<IDLIntegerTypeBase<T, mode>>::ToV8(script_state, *value);
  }
};

// Nullable Float
template <typename T>
struct ToV8Traits<IDLNullable<T>,
                  typename std::enable_if_t<
                      std::is_base_of<IDLBaseHelper<float>, T>::value>> {
  static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                        const base::Optional<float>& value)
      WARN_UNUSED_RESULT {
    if (!value)
      return v8::Null(script_state->GetIsolate());
    return ToV8Traits<T>::ToV8(script_state, *value);
  }
};

// Nullable Double
template <typename T>
struct ToV8Traits<IDLNullable<T>,
                  typename std::enable_if_t<
                      std::is_base_of<IDLBaseHelper<double>, T>::value>> {
  static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                        const base::Optional<double>& value)
      WARN_UNUSED_RESULT {
    if (!value)
      return v8::Null(script_state->GetIsolate());
    return ToV8Traits<T>::ToV8(script_state, *value);
  }
};

// Nullable Strings
template <typename T>
struct ToV8Traits<
    IDLNullable<T>,
    typename std::enable_if_t<
        std::is_same<IDLByteStringV2, T>::value ||
        std::is_same<IDLStringV2, T>::value ||
        std::is_same<IDLStringTreatNullAsEmptyStringV2, T>::value ||
        std::is_same<IDLUSVStringV2, T>::value ||
        std::is_same<IDLStringStringContextTrustedHTMLV2, T>::value ||
        std::is_same<IDLStringStringContextTrustedHTMLTreatNullAsEmptyStringV2,
                     T>::value ||
        std::is_same<IDLStringStringContextTrustedScriptV2, T>::value ||
        std::is_same<
            IDLStringStringContextTrustedScriptTreatNullAsEmptyStringV2,
            T>::value ||
        std::is_same<IDLUSVStringStringContextTrustedScriptURLV2, T>::value>> {
  static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                        const String& value)
      WARN_UNUSED_RESULT {
    if (!value)
      return v8::Null(script_state->GetIsolate());
    return ToV8Traits<T>::ToV8(script_state, value);
  }
};

// Nullable ScriptWrappable
template <typename T>
struct ToV8Traits<
    IDLNullable<T>,
    typename std::enable_if_t<std::is_base_of<ScriptWrappable, T>::value>> {
  static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                        T* script_wrappable)
      WARN_UNUSED_RESULT {
    if (!script_wrappable)
      return v8::Null(script_state->GetIsolate());
    return ToV8Traits<T>::ToV8(script_state, script_wrappable);
  }

  static v8::MaybeLocal<v8::Value> ToV8(v8::Isolate* isolate,
                                        ScriptWrappable* script_wrappable,
                                        v8::Local<v8::Object> creation_context)
      WARN_UNUSED_RESULT {
    if (!script_wrappable)
      return v8::Null(isolate);
    return ToV8Traits<T>::ToV8(isolate, script_wrappable, creation_context);
  }
};

// Nullable Dictionary
template <typename T>
struct ToV8Traits<IDLNullable<T>,
                  typename std::enable_if_t<
                      std::is_base_of<bindings::DictionaryBase, T>::value>> {
  static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                        const T* dictionary)
      WARN_UNUSED_RESULT {
    if (!dictionary)
      return v8::Null(script_state->GetIsolate());
    return ToV8Traits<T>::ToV8(script_state, dictionary);
  }
};

// Nullable Dictionary (Old implementation)
template <typename T>
struct ToV8Traits<
    IDLNullable<T>,
    typename std::enable_if_t<std::is_base_of<IDLDictionaryBase, T>::value>> {
  static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                        const T* dictionary)
      WARN_UNUSED_RESULT {
    if (!dictionary)
      return v8::Null(script_state->GetIsolate());
    return ToV8Traits<T>::ToV8(script_state, dictionary);
  }
};

// Nullable Callback function
template <typename T>
struct ToV8Traits<IDLNullable<T>,
                  typename std::enable_if_t<
                      std::is_base_of<CallbackFunctionBase, T>::value>> {
  static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                        T* callback) WARN_UNUSED_RESULT {
    if (!callback)
      return v8::Null(script_state->GetIsolate());
    return ToV8Traits<CallbackFunctionBase>::ToV8(script_state, callback);
  }
};

// Nullable Callback interface
template <typename T>
struct ToV8Traits<IDLNullable<T>,
                  typename std::enable_if_t<
                      std::is_base_of<CallbackInterfaceBase, T>::value>> {
  static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                        T* callback) WARN_UNUSED_RESULT {
    if (!callback)
      return v8::Null(script_state->GetIsolate());
    return ToV8Traits<CallbackInterfaceBase>::ToV8(script_state, callback);
  }
};

// Nullable Enumeration
template <typename T>
struct ToV8Traits<IDLNullable<T>,
                  typename std::enable_if_t<
                      std::is_base_of<bindings::EnumerationBase, T>::value>> {
  static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                        const base::Optional<T>& enumeration)
      WARN_UNUSED_RESULT {
    if (!enumeration)
      return v8::Null(script_state->GetIsolate());
    return ToV8Traits<T>::ToV8(script_state, *enumeration);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_TO_V8_TRAITS_H_
