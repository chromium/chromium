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

#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_string_resource.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"

namespace blink {

Dictionary::Dictionary(v8::Isolate* isolate,
                       v8::Local<v8::Value> dictionary_object,
                       ExceptionState& exception_state)
    : isolate_(isolate) {
  DCHECK(isolate);

  // https://heycam.github.io/webidl/#es-dictionary
  // Type of an ECMAScript value must be Undefined, Null or Object.
  if (dictionary_object.IsEmpty() || dictionary_object->IsUndefined()) {
    value_type_ = ValueType::kUndefined;
    return;
  }
  if (dictionary_object->IsNull()) {
    value_type_ = ValueType::kNull;
    return;
  }
  if (dictionary_object->IsObject()) {
    value_type_ = ValueType::kObject;
    dictionary_object_ = dictionary_object.As<v8::Object>();
    return;
  }

  exception_state.ThrowTypeError(
      "The dictionary provided is neither undefined, null nor an Object.");
}

bool Dictionary::HasProperty(const StringView& key,
                             ExceptionState& exception_state) const {
  if (dictionary_object_.IsEmpty())
    return false;

  v8::TryCatch try_catch(isolate_);
  bool has_key = false;
  if (!dictionary_object_->Has(V8Context(), V8String(isolate_, key))
           .To(&has_key)) {
    exception_state.RethrowV8Exception(try_catch.Exception());
    return false;
  }

  return has_key;
}

bool Dictionary::Get(const StringView& key, Dictionary& value) const {
  v8::Local<v8::Value> v8_value;
  if (!Get(key, v8_value))
    return false;

  if (v8_value->IsObject()) {
    DCHECK(isolate_);
    DCHECK_EQ(isolate_, v8::Isolate::GetCurrent());
    // TODO(bashi,yukishiino): Should rethrow the exception.
    // http://crbug.com/666661
    DummyExceptionStateForTesting exception_state;
    value = Dictionary(isolate_, v8_value, exception_state);
  }

  return true;
}

bool Dictionary::Get(v8::Local<v8::Value> key,
                     v8::Local<v8::Value>& result) const {
  if (dictionary_object_.IsEmpty())
    return false;

  // Swallow possible exceptions in v8::Object::Get() and Has().
  // TODO(bashi,yukishiino): Should rethrow the exception.
  // http://crbug.com/666661
  v8::TryCatch try_catch(GetIsolate());

  bool has_property;
  if (!dictionary_object_->Has(V8Context(), key).To(&has_property) ||
      !has_property)
    return false;

  return dictionary_object_->Get(V8Context(), key).ToLocal(&result);
}

bool Dictionary::GetInternal(const v8::Local<v8::Value>& key,
                             v8::Local<v8::Value>& result,
                             ExceptionState& exception_state) const {
  if (dictionary_object_.IsEmpty())
    return false;

  v8::TryCatch try_catch(GetIsolate());
  bool has_key = false;
  if (!dictionary_object_->Has(V8Context(), key).To(&has_key)) {
    DCHECK(try_catch.HasCaught());
    exception_state.RethrowV8Exception(try_catch.Exception());
    return false;
  }
  DCHECK(!try_catch.HasCaught());
  if (!has_key)
    return false;

  if (!dictionary_object_->Get(V8Context(), key).ToLocal(&result)) {
    DCHECK(try_catch.HasCaught());
    exception_state.RethrowV8Exception(try_catch.Exception());
    return false;
  }
  DCHECK(!try_catch.HasCaught());
  return true;
}

WARN_UNUSED_RESULT static v8::MaybeLocal<v8::String> GetStringValueInArray(
    v8::Local<v8::Context> context,
    v8::Local<v8::Array> array,
    uint32_t index) {
  v8::Local<v8::Value> value;
  if (!array->Get(context, index).ToLocal(&value))
    return v8::MaybeLocal<v8::String>();
  return value->ToString(context);
}

HashMap<String, String> Dictionary::GetOwnPropertiesAsStringHashMap(
    ExceptionState& exception_state) const {
  if (dictionary_object_.IsEmpty())
    return HashMap<String, String>();

  v8::TryCatch try_catch(GetIsolate());
  v8::Local<v8::Array> property_names;
  if (!dictionary_object_->GetOwnPropertyNames(V8Context())
           .ToLocal(&property_names)) {
    exception_state.RethrowV8Exception(try_catch.Exception());
    return HashMap<String, String>();
  }

  HashMap<String, String> own_properties;
  for (uint32_t i = 0; i < property_names->Length(); ++i) {
    v8::Local<v8::String> key;
    if (!GetStringValueInArray(V8Context(), property_names, i).ToLocal(&key)) {
      exception_state.RethrowV8Exception(try_catch.Exception());
      return HashMap<String, String>();
    }
    V8StringResource<> string_key(key);
    if (!string_key.Prepare(GetIsolate(), exception_state))
      return HashMap<String, String>();

    v8::Local<v8::Value> value;
    if (!dictionary_object_->Get(V8Context(), key).ToLocal(&value)) {
      exception_state.RethrowV8Exception(try_catch.Exception());
      return HashMap<String, String>();
    }
    V8StringResource<> string_value(value);
    if (!string_value.Prepare(GetIsolate(), exception_state))
      return HashMap<String, String>();

    if (!static_cast<const String&>(string_key).IsEmpty())
      own_properties.Set(string_key, string_value);
  }

  return own_properties;
}

Vector<String> Dictionary::GetPropertyNames(
    ExceptionState& exception_state) const {
  if (dictionary_object_.IsEmpty())
    return Vector<String>();

  v8::TryCatch try_catch(GetIsolate());
  v8::Local<v8::Array> property_names;
  if (!dictionary_object_->GetPropertyNames(V8Context())
           .ToLocal(&property_names)) {
    exception_state.RethrowV8Exception(try_catch.Exception());
    return Vector<String>();
  }

  Vector<String> names;
  for (uint32_t i = 0; i < property_names->Length(); ++i) {
    v8::Local<v8::String> key;
    if (!GetStringValueInArray(V8Context(), property_names, i).ToLocal(&key)) {
      exception_state.RethrowV8Exception(try_catch.Exception());
      return Vector<String>();
    }
    V8StringResource<> string_key(key);
    if (!string_key.Prepare(GetIsolate(), exception_state))
      return Vector<String>();

    names.push_back(string_key);
  }

  return names;
}

}  // namespace blink
