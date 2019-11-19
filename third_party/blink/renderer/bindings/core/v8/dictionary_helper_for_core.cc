/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/bindings/core/v8/array_value.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer_view.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_element.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_message_port.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_text_track.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_uint8_array.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_window.h"
#include "third_party/blink/renderer/core/html/track/track_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

template <>
CORE_EXPORT bool DictionaryHelper::Get(const Dictionary& dictionary,
                                       const StringView& key,
                                       v8::Local<v8::Value>& value) {
  return dictionary.Get(key, value);
}

template <>
CORE_EXPORT bool DictionaryHelper::Get(const Dictionary& dictionary,
                                       const StringView& key,
                                       Dictionary& value) {
  return dictionary.Get(key, value);
}

template <>
CORE_EXPORT bool DictionaryHelper::Get(const Dictionary& dictionary,
                                       const StringView& key,
                                       bool& value) {
  v8::Local<v8::Value> v8_value;
  if (!dictionary.Get(key, v8_value))
    return false;

  value = v8_value->BooleanValue(dictionary.GetIsolate());
  return true;
}

template <>
CORE_EXPORT bool DictionaryHelper::Get(const Dictionary& dictionary,
                                       const StringView& key,
                                       int32_t& value) {
  v8::Local<v8::Value> v8_value;
  if (!dictionary.Get(key, v8_value))
    return false;

  return v8_value->Int32Value(dictionary.V8Context()).To(&value);
}

template <>
CORE_EXPORT bool DictionaryHelper::Get(const Dictionary& dictionary,
                                       const StringView& key,
                                       double& value) {
  v8::Local<v8::Value> v8_value;
  if (!dictionary.Get(key, v8_value))
    return false;

  return v8_value->NumberValue(dictionary.V8Context()).To(&value);
}

template <typename StringType>
bool GetStringType(const Dictionary& dictionary,
                   const StringView& key,
                   StringType& value) {
  v8::Local<v8::Value> v8_value;
  if (!dictionary.Get(key, v8_value))
    return false;

  V8StringResource<> string_value(v8_value);
  if (!string_value.Prepare())
    return false;
  value = string_value;
  return true;
}

template <>
CORE_EXPORT bool DictionaryHelper::Get(const Dictionary& dictionary,
                                       const StringView& key,
                                       String& value) {
  return GetStringType(dictionary, key, value);
}

template <>
CORE_EXPORT bool DictionaryHelper::Get(const Dictionary& dictionary,
                                       const StringView& key,
                                       AtomicString& value) {
  return GetStringType(dictionary, key, value);
}

template <typename NumericType>
bool GetNumericType(const Dictionary& dictionary,
                    const StringView& key,
                    NumericType& value) {
  int32_t int32_value;
  if (!DictionaryHelper::Get(dictionary, key, int32_value))
    return false;
  value = static_cast<NumericType>(int32_value);
  return true;
}

template <>
bool DictionaryHelper::Get(const Dictionary& dictionary,
                           const StringView& key,
                           int16_t& value) {
  return GetNumericType<int16_t>(dictionary, key, value);
}

template <>
CORE_EXPORT bool DictionaryHelper::Get(const Dictionary& dictionary,
                                       const StringView& key,
                                       uint16_t& value) {
  return GetNumericType<uint16_t>(dictionary, key, value);
}

template <>
bool DictionaryHelper::Get(const Dictionary& dictionary,
                           const StringView& key,
                           uint32_t& value) {
  return GetNumericType<uint32_t>(dictionary, key, value);
}

template <>
bool DictionaryHelper::Get(const Dictionary& dictionary,
                           const StringView& key,
                           int64_t& value) {
  v8::Local<v8::Value> v8_value;
  if (!dictionary.Get(key, v8_value))
    return false;

  if (!v8_value->IntegerValue(dictionary.V8Context()).To(&value))
    return false;
  return true;
}

template <>
bool DictionaryHelper::Get(const Dictionary& dictionary,
                           const StringView& key,
                           uint64_t& value) {
  v8::Local<v8::Value> v8_value;
  if (!dictionary.Get(key, v8_value))
    return false;

  double double_value;
  if (!v8_value->NumberValue(dictionary.V8Context()).To(&double_value))
    return false;
  value = DoubleToInteger(double_value);
  return true;
}

template <>
bool DictionaryHelper::Get(const Dictionary& dictionary,
                           const StringView& key,
                           Member<DOMWindow>& value) {
  v8::Local<v8::Value> v8_value;
  if (!dictionary.Get(key, v8_value))
    return false;

  // We need to handle a DOMWindow specially, because a DOMWindow wrapper
  // exists on a prototype chain of v8Value.
  value = ToDOMWindow(dictionary.GetIsolate(), v8_value);
  return true;
}

template <>
bool DictionaryHelper::Get(const Dictionary& dictionary,
                           const StringView& key,
                           Member<TrackBase>& value) {
  v8::Local<v8::Value> v8_value;
  if (!dictionary.Get(key, v8_value))
    return false;

  TrackBase* source = nullptr;
  if (v8_value->IsObject()) {
    v8::Local<v8::Object> wrapper = v8::Local<v8::Object>::Cast(v8_value);

    // FIXME: this will need to be changed so it can also return an AudioTrack
    // or a VideoTrack once we add them.
    v8::Local<v8::Object> track = V8TextTrack::FindInstanceInPrototypeChain(
        wrapper, dictionary.GetIsolate());
    if (!track.IsEmpty())
      source = V8TextTrack::ToImpl(track);
  }
  value = source;
  return true;
}

template <>
CORE_EXPORT bool DictionaryHelper::Get(const Dictionary& dictionary,
                                       const StringView& key,
                                       Vector<String>& value) {
  v8::Local<v8::Value> v8_value;
  if (!dictionary.Get(key, v8_value))
    return false;

  if (!v8_value->IsArray())
    return false;

  v8::Local<v8::Array> v8_array = v8::Local<v8::Array>::Cast(v8_value);
  for (uint32_t i = 0; i < v8_array->Length(); ++i) {
    v8::Local<v8::Value> indexed_value;
    if (!v8_array
             ->Get(dictionary.V8Context(),
                   v8::Uint32::New(dictionary.GetIsolate(), i))
             .ToLocal(&indexed_value))
      return false;
    TOSTRING_DEFAULT(V8StringResource<>, string_value, indexed_value, false);
    value.push_back(string_value);
  }

  return true;
}

template <>
CORE_EXPORT bool DictionaryHelper::Get(const Dictionary& dictionary,
                                       const StringView& key,
                                       ArrayValue& value) {
  v8::Local<v8::Value> v8_value;
  if (!dictionary.Get(key, v8_value))
    return false;

  if (!v8_value->IsArray())
    return false;

  DCHECK(dictionary.GetIsolate());
  DCHECK_EQ(dictionary.GetIsolate(), v8::Isolate::GetCurrent());
  value =
      ArrayValue(v8::Local<v8::Array>::Cast(v8_value), dictionary.GetIsolate());
  return true;
}

template <>
CORE_EXPORT bool DictionaryHelper::Get(const Dictionary& dictionary,
                                       const StringView& key,
                                       DOMUint8Array*& value) {
  v8::Local<v8::Value> v8_value;
  if (!dictionary.Get(key, v8_value))
    return false;

  value = V8Uint8Array::ToImplWithTypeCheck(dictionary.GetIsolate(), v8_value);
  return true;
}

}  // namespace blink
