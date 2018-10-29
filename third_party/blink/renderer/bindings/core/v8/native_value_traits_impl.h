// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_NATIVE_VALUE_TRAITS_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_NATIVE_VALUE_TRAITS_IMPL_H_

#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class CallbackFunctionBase;

// Boolean
template <>
struct CORE_EXPORT NativeValueTraits<IDLBoolean>
    : public NativeValueTraitsBase<IDLBoolean> {
  static bool NativeValue(v8::Isolate* isolate,
                          v8::Local<v8::Value> value,
                          ExceptionState& exception_state) {
    return ToBoolean(isolate, value, exception_state);
  }
};

// Integers
//
// All integer specializations offer a second nativeValue() besides the default
// one: it takes an IntegerConversionConfiguration argument to let callers
// specify how the integers should be converted. The default nativeValue()
// overload will always use NormalConversion.
template <>
struct CORE_EXPORT NativeValueTraits<IDLByte>
    : public NativeValueTraitsBase<IDLByte> {
  static int8_t NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state) {
    return ToInt8(isolate, value, kNormalConversion, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLOctet>
    : public NativeValueTraitsBase<IDLOctet> {
  static uint8_t NativeValue(v8::Isolate* isolate,
                             v8::Local<v8::Value> value,
                             ExceptionState& exception_state) {
    return ToUInt8(isolate, value, kNormalConversion, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLShort>
    : public NativeValueTraitsBase<IDLShort> {
  static int16_t NativeValue(v8::Isolate* isolate,
                             v8::Local<v8::Value> value,
                             ExceptionState& exception_state) {
    return ToInt16(isolate, value, kNormalConversion, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLUnsignedShort>
    : public NativeValueTraitsBase<IDLUnsignedShort> {
  static uint16_t NativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> value,
                              ExceptionState& exception_state) {
    return ToUInt16(isolate, value, kNormalConversion, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLLong>
    : public NativeValueTraitsBase<IDLLong> {
  static int32_t NativeValue(v8::Isolate* isolate,
                             v8::Local<v8::Value> value,
                             ExceptionState& exception_state) {
    return ToInt32(isolate, value, kNormalConversion, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLUnsignedLong>
    : public NativeValueTraitsBase<IDLUnsignedLong> {
  static uint32_t NativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> value,
                              ExceptionState& exception_state) {
    return ToUInt32(isolate, value, kNormalConversion, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLLongLong>
    : public NativeValueTraitsBase<IDLLongLong> {
  static int64_t NativeValue(v8::Isolate* isolate,
                             v8::Local<v8::Value> value,
                             ExceptionState& exception_state) {
    return ToInt64(isolate, value, kNormalConversion, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLUnsignedLongLong>
    : public NativeValueTraitsBase<IDLUnsignedLongLong> {
  static uint64_t NativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> value,
                              ExceptionState& exception_state) {
    return ToUInt64(isolate, value, kNormalConversion, exception_state);
  }
};

// [Clamp] Integers
template <>
struct CORE_EXPORT NativeValueTraits<IDLByteClamp>
    : public NativeValueTraitsBase<IDLByte> {
  static int8_t NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state) {
    return ToInt8(isolate, value, kClamp, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLOctetClamp>
    : public NativeValueTraitsBase<IDLOctet> {
  static uint8_t NativeValue(v8::Isolate* isolate,
                             v8::Local<v8::Value> value,
                             ExceptionState& exception_state) {
    return ToUInt8(isolate, value, kClamp, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLShortClamp>
    : public NativeValueTraitsBase<IDLShort> {
  static int16_t NativeValue(v8::Isolate* isolate,
                             v8::Local<v8::Value> value,
                             ExceptionState& exception_state) {
    return ToInt16(isolate, value, kClamp, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLUnsignedShortClamp>
    : public NativeValueTraitsBase<IDLUnsignedShort> {
  static uint16_t NativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> value,
                              ExceptionState& exception_state) {
    return ToUInt16(isolate, value, kClamp, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLLongClamp>
    : public NativeValueTraitsBase<IDLLong> {
  static int32_t NativeValue(v8::Isolate* isolate,
                             v8::Local<v8::Value> value,
                             ExceptionState& exception_state) {
    return ToInt32(isolate, value, kClamp, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLUnsignedLongClamp>
    : public NativeValueTraitsBase<IDLUnsignedLong> {
  static uint32_t NativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> value,
                              ExceptionState& exception_state) {
    return ToUInt32(isolate, value, kClamp, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLLongLongClamp>
    : public NativeValueTraitsBase<IDLLongLong> {
  static int64_t NativeValue(v8::Isolate* isolate,
                             v8::Local<v8::Value> value,
                             ExceptionState& exception_state) {
    return ToInt64(isolate, value, kClamp, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLUnsignedLongLongClamp>
    : public NativeValueTraitsBase<IDLUnsignedLongLong> {
  static uint64_t NativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> value,
                              ExceptionState& exception_state) {
    return ToUInt64(isolate, value, kClamp, exception_state);
  }
};

// [EnforceRange] Integers
template <>
struct CORE_EXPORT NativeValueTraits<IDLByteEnforceRange>
    : public NativeValueTraitsBase<IDLByte> {
  static int8_t NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state) {
    return ToInt8(isolate, value, kEnforceRange, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLOctetEnforceRange>
    : public NativeValueTraitsBase<IDLOctet> {
  static uint8_t NativeValue(v8::Isolate* isolate,
                             v8::Local<v8::Value> value,
                             ExceptionState& exception_state) {
    return ToUInt8(isolate, value, kEnforceRange, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLShortEnforceRange>
    : public NativeValueTraitsBase<IDLShort> {
  static int16_t NativeValue(v8::Isolate* isolate,
                             v8::Local<v8::Value> value,
                             ExceptionState& exception_state) {
    return ToInt16(isolate, value, kEnforceRange, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLUnsignedShortEnforceRange>
    : public NativeValueTraitsBase<IDLUnsignedShort> {
  static uint16_t NativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> value,
                              ExceptionState& exception_state) {
    return ToUInt16(isolate, value, kEnforceRange, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLLongEnforceRange>
    : public NativeValueTraitsBase<IDLLong> {
  static int32_t NativeValue(v8::Isolate* isolate,
                             v8::Local<v8::Value> value,
                             ExceptionState& exception_state) {
    return ToInt32(isolate, value, kEnforceRange, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLUnsignedLongEnforceRange>
    : public NativeValueTraitsBase<IDLUnsignedLong> {
  static uint32_t NativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> value,
                              ExceptionState& exception_state) {
    return ToUInt32(isolate, value, kEnforceRange, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLLongLongEnforceRange>
    : public NativeValueTraitsBase<IDLLongLong> {
  static int64_t NativeValue(v8::Isolate* isolate,
                             v8::Local<v8::Value> value,
                             ExceptionState& exception_state) {
    return ToInt64(isolate, value, kEnforceRange, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLUnsignedLongLongEnforceRange>
    : public NativeValueTraitsBase<IDLUnsignedLongLong> {
  static uint64_t NativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> value,
                              ExceptionState& exception_state) {
    return ToUInt64(isolate, value, kEnforceRange, exception_state);
  }
};

// Strings
template <V8StringResourceMode Mode>
struct NativeValueTraits<IDLByteStringBase<Mode>>
    : public NativeValueTraitsBase<IDLByteStringBase<Mode>> {
  // http://heycam.github.io/webidl/#es-ByteString
  static String NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state) {
    V8StringResource<Mode> string_resource(value);
    // 1. Let x be ToString(v)
    if (!string_resource.Prepare(isolate, exception_state))
      return String();
    String x = string_resource;
    // 2. If the value of any element of x is greater than 255, then throw a
    //    TypeError.
    if (!x.ContainsOnlyLatin1OrEmpty()) {
      exception_state.ThrowTypeError("Value is not a valid ByteString.");
      return String();
    }

    // 3. Return an IDL ByteString value whose length is the length of x, and
    //    where the value of each element is the value of the corresponding
    //    element of x.
    //    Blink: A ByteString is simply a String with a range constrained per
    //    the above, so this is the identity operation.
    return x;
  }

  static String NullValue() { return String(); }
};

template <V8StringResourceMode Mode>
struct NativeValueTraits<IDLStringBase<Mode>>
    : public NativeValueTraitsBase<IDLStringBase<Mode>> {
  // https://heycam.github.io/webidl/#es-DOMString
  static String NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state) {
    V8StringResource<Mode> string(value);
    if (!string.Prepare(isolate, exception_state))
      return String();
    return string;
  }

  static String NullValue() { return String(); }
};

template <V8StringResourceMode Mode>
struct NativeValueTraits<IDLUSVStringBase<Mode>>
    : public NativeValueTraitsBase<IDLUSVStringBase<Mode>> {
  // http://heycam.github.io/webidl/#es-USVString
  static String NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state) {
    // 1. Let string be the result of converting V to a DOMString.
    V8StringResource<Mode> string(value);
    if (!string.Prepare(isolate, exception_state))
      return String();
    // 2. Return an IDL USVString value that is the result of converting string
    //    to a sequence of Unicode scalar values.
    return ReplaceUnmatchedSurrogates(string);
  }

  static String NullValue() { return String(); }
};

// Floats and doubles
template <>
struct CORE_EXPORT NativeValueTraits<IDLDouble>
    : public NativeValueTraitsBase<IDLDouble> {
  static double NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state) {
    return ToRestrictedDouble(isolate, value, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLUnrestrictedDouble>
    : public NativeValueTraitsBase<IDLUnrestrictedDouble> {
  static double NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state) {
    return ToDouble(isolate, value, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLFloat>
    : public NativeValueTraitsBase<IDLFloat> {
  static float NativeValue(v8::Isolate* isolate,
                           v8::Local<v8::Value> value,
                           ExceptionState& exception_state) {
    return ToRestrictedFloat(isolate, value, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLUnrestrictedFloat>
    : public NativeValueTraitsBase<IDLUnrestrictedFloat> {
  static float NativeValue(v8::Isolate* isolate,
                           v8::Local<v8::Value> value,
                           ExceptionState& exception_state) {
    return ToFloat(isolate, value, exception_state);
  }
};

// Promises
template <>
struct CORE_EXPORT NativeValueTraits<IDLPromise>
    : public NativeValueTraitsBase<IDLPromise> {
  static ScriptPromise NativeValue(v8::Isolate* isolate,
                                   v8::Local<v8::Value> value,
                                   ExceptionState& exception_state) {
    return NativeValue(isolate, value);
  }

  static ScriptPromise NativeValue(v8::Isolate* isolate,
                                   v8::Local<v8::Value> value) {
    return ScriptPromise::Cast(ScriptState::Current(isolate), value);
  }
};

// Type-specific overloads
template <>
struct CORE_EXPORT NativeValueTraits<IDLDate>
    : public NativeValueTraitsBase<IDLDate> {
  static double NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state) {
    return ToCoreDate(isolate, value, exception_state);
  }
};

// Sequences
template <typename T>
struct NativeValueTraits<IDLSequence<T>>
    : public NativeValueTraitsBase<IDLSequence<T>> {
  // Nondependent types need to be explicitly qualified to be accessible.
  using typename NativeValueTraitsBase<IDLSequence<T>>::ImplType;

  // https://heycam.github.io/webidl/#es-sequence
  static ImplType NativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> value,
                              ExceptionState& exception_state) {
    if (!value->IsObject()) {
      exception_state.ThrowTypeError(
          "The provided value cannot be converted to a sequence.");
      return ImplType();
    }

    ImplType result;
    // TODO(rakuco): Checking for IsArray() may not be enough. Other engines
    // also prefer regular array iteration over a custom @@iterator when the
    // latter is defined, but it is not clear if this is a valid optimization.
    if (value->IsArray()) {
      ConvertSequenceFast(isolate, value.As<v8::Array>(), exception_state,
                          result);
    } else {
      ConvertSequenceSlow(isolate, value.As<v8::Object>(), exception_state,
                          result);
    }

    if (exception_state.HadException())
      return ImplType();
    return result;
  }

 private:
  // Fast case: we're interating over an Array that adheres to
  // %ArrayIteratorPrototype%'s protocol.
  static void ConvertSequenceFast(v8::Isolate* isolate,
                                  v8::Local<v8::Array> v8_array,
                                  ExceptionState& exception_state,
                                  ImplType& result) {
    const uint32_t length = v8_array->Length();
    if (length > ImplType::MaxCapacity()) {
      exception_state.ThrowRangeError("Array length exceeds supported limit.");
      return;
    }
    result.ReserveInitialCapacity(length);
    v8::TryCatch block(isolate);
    for (uint32_t i = 0; i < length; ++i) {
      v8::Local<v8::Value> element;
      if (!v8_array->Get(isolate->GetCurrentContext(), i).ToLocal(&element)) {
        exception_state.RethrowV8Exception(block.Exception());
        return;
      }
      result.UncheckedAppend(
          NativeValueTraits<T>::NativeValue(isolate, element, exception_state));
      if (exception_state.HadException())
        return;
    }
  }

  // Slow case: follow WebIDL's "Creating a sequence from an iterable" steps to
  // iterate through each element.
  // https://heycam.github.io/webidl/#create-sequence-from-iterable
  static void ConvertSequenceSlow(v8::Isolate* isolate,
                                  v8::Local<v8::Object> v8_object,
                                  ExceptionState& exception_state,
                                  ImplType& result) {
    v8::TryCatch block(isolate);

    v8::Local<v8::Object> iterator =
        GetEsIterator(isolate, v8_object, exception_state);
    if (exception_state.HadException())
      return;

    v8::Local<v8::String> next_key = V8AtomicString(isolate, "next");
    v8::Local<v8::String> value_key = V8AtomicString(isolate, "value");
    v8::Local<v8::String> done_key = V8AtomicString(isolate, "done");
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    while (true) {
      v8::Local<v8::Value> next;
      if (!iterator->Get(context, next_key).ToLocal(&next)) {
        exception_state.RethrowV8Exception(block.Exception());
        return;
      }
      if (!next->IsFunction()) {
        exception_state.ThrowTypeError("Iterator.next should be callable.");
        return;
      }
      v8::Local<v8::Value> next_result;
      if (!V8ScriptRunner::CallFunction(next.As<v8::Function>(),
                                        ToExecutionContext(context), iterator,
                                        0, nullptr, isolate)
               .ToLocal(&next_result)) {
        exception_state.RethrowV8Exception(block.Exception());
        return;
      }
      if (!next_result->IsObject()) {
        exception_state.ThrowTypeError(
            "Iterator.next() did not return an object.");
        return;
      }
      v8::Local<v8::Object> result_object = next_result.As<v8::Object>();
      v8::Local<v8::Value> element;
      v8::Local<v8::Value> done;
      if (!result_object->Get(context, value_key).ToLocal(&element) ||
          !result_object->Get(context, done_key).ToLocal(&done)) {
        exception_state.RethrowV8Exception(block.Exception());
        return;
      }
      bool done_boolean;
      if (!done->BooleanValue(context).To(&done_boolean)) {
        exception_state.RethrowV8Exception(block.Exception());
        return;
      }
      if (done_boolean)
        break;
      result.emplace_back(
          NativeValueTraits<T>::NativeValue(isolate, element, exception_state));
      if (exception_state.HadException())
        return;
    }
  }
};

// Records
template <typename K, typename V>
struct NativeValueTraits<IDLRecord<K, V>>
    : public NativeValueTraitsBase<IDLRecord<K, V>> {
  // Nondependent types need to be explicitly qualified to be accessible.
  using typename NativeValueTraitsBase<IDLRecord<K, V>>::ImplType;

  // Converts a JavaScript value |O| to an IDL record<K, V> value. In C++, a
  // record is represented as a Vector<std::pair<k, v>> (or a HeapVector if
  // necessary). See https://heycam.github.io/webidl/#es-record.
  static ImplType NativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> v8_value,
                              ExceptionState& exception_state) {
    v8::Local<v8::Context> context = isolate->GetCurrentContext();

    // "1. If Type(O) is not Object, throw a TypeError."
    if (!v8_value->IsObject()) {
      exception_state.ThrowTypeError(
          "Only objects can be converted to record<K,V> types");
      return ImplType();
    }
    v8::Local<v8::Object> v8_object = v8::Local<v8::Object>::Cast(v8_value);
    v8::TryCatch block(isolate);

    // "3. Let keys be ? O.[[OwnPropertyKeys]]()."
    v8::Local<v8::Array> keys;
    // While we could pass v8::ONLY_ENUMERABLE below, doing so breaks
    // web-platform-tests' headers-record.html and deviates from the spec
    // algorithm.
    if (!v8_object
             ->GetOwnPropertyNames(context,
                                   static_cast<v8::PropertyFilter>(
                                       v8::PropertyFilter::ALL_PROPERTIES))
             .ToLocal(&keys)) {
      exception_state.RethrowV8Exception(block.Exception());
      return ImplType();
    }
    if (keys->Length() > ImplType::MaxCapacity()) {
      exception_state.ThrowRangeError("Array length exceeds supported limit.");
      return ImplType();
    }

    // "2. Let result be a new empty instance of record<K, V>."
    ImplType result;
    result.ReserveInitialCapacity(keys->Length());

    // The conversion algorithm needs a data structure with fast insertion at
    // the end while at the same time requiring fast checks for previous insert
    // of a given key. |seenKeys| is a key/position in |result| map that aids in
    // the latter part.
    HashMap<String, uint32_t> seen_keys;

    for (uint32_t i = 0; i < keys->Length(); ++i) {
      // "4. Repeat, for each element key of keys in List order:"
      v8::Local<v8::Value> key;
      if (!keys->Get(context, i).ToLocal(&key)) {
        exception_state.RethrowV8Exception(block.Exception());
        return ImplType();
      }

      // V8's GetOwnPropertyNames() does not convert numeric property indices
      // to strings, so we have to do it ourselves.
      if (!key->IsName())
        key = key->ToString(context).ToLocalChecked();

      // "4.1. Let desc be ? O.[[GetOwnProperty]](key)."
      v8::Local<v8::Value> desc;
      if (!v8_object->GetOwnPropertyDescriptor(context, key.As<v8::Name>())
               .ToLocal(&desc)) {
        exception_state.RethrowV8Exception(block.Exception());
        return ImplType();
      }

      // "4.2. If desc is not undefined and desc.[[Enumerable]] is true:"
      // We can call ToLocalChecked() and ToChecked() here because
      // GetOwnPropertyDescriptor is responsible for catching any exceptions
      // and failures, and if we got to this point of the code we have a proper
      // object that was not created by a user.
      if (desc->IsUndefined())
        continue;
      DCHECK(desc->IsObject());
      v8::Local<v8::Value> enumerable =
          v8::Local<v8::Object>::Cast(desc)
              ->Get(context, V8String(isolate, "enumerable"))
              .ToLocalChecked();
      if (!enumerable->BooleanValue(context).ToChecked())
        continue;

      // "4.2.1. Let typedKey be key converted to an IDL value of type K."
      String typed_key =
          NativeValueTraits<K>::NativeValue(isolate, key, exception_state);
      if (exception_state.HadException())
        return ImplType();

      // "4.2.2. Let value be ? Get(O, key)."
      v8::Local<v8::Value> value;
      if (!v8_object->Get(context, key).ToLocal(&value)) {
        exception_state.RethrowV8Exception(block.Exception());
        return ImplType();
      }

      // "4.2.3. Let typedValue be value converted to an IDL value of type V."
      typename ImplType::ValueType::second_type typed_value =
          NativeValueTraits<V>::NativeValue(isolate, value, exception_state);
      if (exception_state.HadException())
        return ImplType();

      if (seen_keys.Contains(typed_key)) {
        // "4.2.4. If typedKey is already a key in result, set its value to
        //         typedValue.
        //         Note: This can happen when O is a proxy object."
        const uint32_t pos = seen_keys.at(typed_key);
        result[pos] = std::make_pair(typed_key, typed_value);
      } else {
        // "4.2.5. Otherwise, append to result a mapping (typedKey,
        // typedValue)."
        // Note we can take this shortcut because we are always appending.
        const uint32_t pos = result.size();
        seen_keys.Set(typed_key, pos);
        result.UncheckedAppend(std::make_pair(typed_key, typed_value));
      }
    }
    // "5. Return result."
    return result;
  }
};

// Callback functions
template <typename T>
struct NativeValueTraits<
    T,
    typename std::enable_if<
        std::is_base_of<CallbackFunctionBase, T>::value>::type>
    : public NativeValueTraitsBase<T> {
  static T* NativeValue(v8::Isolate* isolate,
                        v8::Local<v8::Value> value,
                        ExceptionState& exception_state) {
    // Not implemented because of no use case so far.
    CHECK(false)
        // Emit a message so that NativeValueTraitsImplTest.IDLCallbackFunction
        // test can confirm that it's hitting this specific failure. i.e.
        // the template resolution is working as expected.
        << "NativeValueTraits<CallbackFunctionBase>::NativeValue "
        << "is not yet implemented.";
    return nullptr;
  }
};

// Nullable
template <typename InnerType>
struct NativeValueTraits<IDLNullable<InnerType>>
    : public NativeValueTraitsBase<IDLNullable<InnerType>> {
  // https://heycam.github.io/webidl/#es-nullable-type
  static typename IDLNullable<InnerType>::ResultType NativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> v8_value,
      ExceptionState& exception_state) {
    if (v8_value->IsNullOrUndefined())
      return IDLNullable<InnerType>::NullValue();
    return NativeValueTraits<InnerType>::NativeValue(isolate, v8_value,
                                                     exception_state);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_NATIVE_VALUE_TRAITS_IMPL_H_
