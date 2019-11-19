// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_TO_V8_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_TO_V8_H_

// ToV8() provides C++ -> V8 conversion. Note that ToV8() can return an empty
// handle. Call sites must check IsEmpty() before using return value.

#include <utility>

#include "base/containers/span.h"
#include "base/optional.h"
#include "third_party/blink/renderer/platform/bindings/callback_function_base.h"
#include "third_party/blink/renderer/platform/bindings/callback_interface_base.h"
#include "third_party/blink/renderer/platform/bindings/dom_data_store.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "v8/include/v8.h"

namespace blink {

// ScriptWrappable

inline v8::Local<v8::Value> ToV8(ScriptWrappable* impl,
                                 v8::Local<v8::Object> creation_context,
                                 v8::Isolate* isolate) {
  if (UNLIKELY(!impl))
    return v8::Null(isolate);
  v8::Local<v8::Value> wrapper = DOMDataStore::GetWrapper(impl, isolate);
  if (!wrapper.IsEmpty())
    return wrapper;

  wrapper = impl->Wrap(isolate, creation_context);
  DCHECK(!wrapper.IsEmpty());
  return wrapper;
}

// Callback function

inline v8::Local<v8::Value> ToV8(CallbackFunctionBase* callback,
                                 v8::Local<v8::Object> creation_context,
                                 v8::Isolate* isolate) {
  // |creation_context| is intentionally ignored. Callback functions are not
  // wrappers nor clonable. ToV8 on a callback function must be used only when
  // it's in the same world.
  DCHECK(!callback ||
         (&callback->GetWorld() ==
          &ScriptState::From(creation_context->CreationContext())->World()));
  return callback ? callback->CallbackObject().As<v8::Value>()
                  : v8::Null(isolate).As<v8::Value>();
}

// Callback interface

inline v8::Local<v8::Value> ToV8(CallbackInterfaceBase* callback,
                                 v8::Local<v8::Object> creation_context,
                                 v8::Isolate* isolate) {
  // |creation_context| is intentionally ignored. Callback interfaces are not
  // wrappers nor clonable. ToV8 on a callback interface must be used only when
  // it's in the same world.
  DCHECK(!callback ||
         (&callback->GetWorld() ==
          &ScriptState::From(creation_context->CreationContext())->World()));
  return callback ? callback->CallbackObject().As<v8::Value>()
                  : v8::Null(isolate).As<v8::Value>();
}

// Primitives

inline v8::Local<v8::Value> ToV8(const String& value,
                                 v8::Local<v8::Object> creation_context,
                                 v8::Isolate* isolate) {
  return V8String(isolate, value);
}

inline v8::Local<v8::Value> ToV8(const char* value,
                                 v8::Local<v8::Object> creation_context,
                                 v8::Isolate* isolate) {
  return V8String(isolate, value);
}

template <size_t sizeOfValue>
inline v8::Local<v8::Value> ToV8SignedIntegerInternal(int64_t value,
                                                      v8::Isolate*);

template <>
inline v8::Local<v8::Value> ToV8SignedIntegerInternal<4>(int64_t value,
                                                         v8::Isolate* isolate) {
  return v8::Integer::New(isolate, static_cast<int32_t>(value));
}

template <>
inline v8::Local<v8::Value> ToV8SignedIntegerInternal<8>(int64_t value,
                                                         v8::Isolate* isolate) {
  int32_t value_in32_bit = static_cast<int32_t>(value);
  if (value_in32_bit == value)
    return v8::Integer::New(isolate, value_in32_bit);
  // V8 doesn't have a 64-bit integer implementation.
  return v8::Number::New(isolate, value);
}

template <size_t sizeOfValue>
inline v8::Local<v8::Value> ToV8UnsignedIntegerInternal(uint64_t value,
                                                        v8::Isolate*);

template <>
inline v8::Local<v8::Value> ToV8UnsignedIntegerInternal<4>(
    uint64_t value,
    v8::Isolate* isolate) {
  return v8::Integer::NewFromUnsigned(isolate, static_cast<uint32_t>(value));
}

template <>
inline v8::Local<v8::Value> ToV8UnsignedIntegerInternal<8>(
    uint64_t value,
    v8::Isolate* isolate) {
  uint32_t value_in32_bit = static_cast<uint32_t>(value);
  if (value_in32_bit == value)
    return v8::Integer::NewFromUnsigned(isolate, value_in32_bit);
  // V8 doesn't have a 64-bit integer implementation.
  return v8::Number::New(isolate, value);
}

inline v8::Local<v8::Value> ToV8(int32_t value,
                                 v8::Local<v8::Object> creation_context,
                                 v8::Isolate* isolate) {
  return ToV8SignedIntegerInternal<sizeof value>(value, isolate);
}

inline v8::Local<v8::Value> ToV8(int64_t value,
                                 v8::Local<v8::Object> creation_context,
                                 v8::Isolate* isolate) {
  return ToV8SignedIntegerInternal<sizeof value>(value, isolate);
}

inline v8::Local<v8::Value> ToV8(uint32_t value,
                                 v8::Local<v8::Object> creation_context,
                                 v8::Isolate* isolate) {
  return ToV8UnsignedIntegerInternal<sizeof value>(value, isolate);
}

inline v8::Local<v8::Value> ToV8(uint64_t value,
                                 v8::Local<v8::Object> creation_context,
                                 v8::Isolate* isolate) {
  return ToV8UnsignedIntegerInternal<sizeof value>(value, isolate);
}

inline v8::Local<v8::Value> ToV8(double value,
                                 v8::Local<v8::Object> creation_context,
                                 v8::Isolate* isolate) {
  return v8::Number::New(isolate, value);
}

inline v8::Local<v8::Value> ToV8(bool value,
                                 v8::Local<v8::Object> creation_context,
                                 v8::Isolate* isolate) {
  return v8::Boolean::New(isolate, value);
}

// Identity operator

inline v8::Local<v8::Value> ToV8(v8::Local<v8::Value> value,
                                 v8::Local<v8::Object> creation_context,
                                 v8::Isolate*) {
  return value;
}

// Undefined

struct ToV8UndefinedGenerator {
  DISALLOW_NEW();
};  // Used only for having toV8 return v8::Undefined.

inline v8::Local<v8::Value> ToV8(const ToV8UndefinedGenerator& value,
                                 v8::Local<v8::Object> creation_context,
                                 v8::Isolate* isolate) {
  return v8::Undefined(isolate);
}

// Forward declaration to allow interleaving with sequences.
template <typename InnerType>
inline v8::Local<v8::Value> ToV8(const base::Optional<InnerType>& value,
                                 v8::Local<v8::Object> creation_context,
                                 v8::Isolate*);

// Array

// Declare the function here but define it later so it can call the ToV8()
// overloads below.
template <typename Sequence>
inline v8::Local<v8::Value> ToV8SequenceInternal(
    const Sequence&,
    v8::Local<v8::Object> creation_context,
    v8::Isolate*);

template <typename T, size_t Extent>
inline v8::Local<v8::Value> ToV8(base::span<T, Extent> value,
                                 v8::Local<v8::Object> creation_context,
                                 v8::Isolate* isolate) {
  return ToV8SequenceInternal(value, creation_context, isolate);
}

template <typename T, wtf_size_t inlineCapacity>
inline v8::Local<v8::Value> ToV8(const Vector<T, inlineCapacity>& value,
                                 v8::Local<v8::Object> creation_context,
                                 v8::Isolate* isolate) {
  return ToV8SequenceInternal(value, creation_context, isolate);
}

template <typename T, wtf_size_t inlineCapacity>
inline v8::Local<v8::Value> ToV8(const HeapVector<T, inlineCapacity>& value,
                                 v8::Local<v8::Object> creation_context,
                                 v8::Isolate* isolate) {
  return ToV8SequenceInternal(value, creation_context, isolate);
}

// The following two overloads are also used to convert record<K,V> IDL types
// back into ECMAScript Objects.
template <typename T>
inline v8::Local<v8::Value> ToV8(const Vector<std::pair<String, T>>& value,
                                 v8::Local<v8::Object> creation_context,
                                 v8::Isolate* isolate) {
  v8::Local<v8::Object> object;
  {
    v8::Context::Scope context_scope(creation_context->CreationContext());
    object = v8::Object::New(isolate);
  }
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  for (unsigned i = 0; i < value.size(); ++i) {
    v8::Local<v8::Value> v8_value = ToV8(value[i].second, object, isolate);
    if (v8_value.IsEmpty())
      v8_value = v8::Undefined(isolate);
    bool created_property;
    if (!object
             ->CreateDataProperty(
                 context, V8AtomicString(isolate, value[i].first), v8_value)
             .To(&created_property) ||
        !created_property) {
      return v8::Local<v8::Value>();
    }
  }
  return object;
}

template <typename T>
inline v8::Local<v8::Value> ToV8(const HeapVector<std::pair<String, T>>& value,
                                 v8::Local<v8::Object> creation_context,
                                 v8::Isolate* isolate) {
  v8::Local<v8::Object> object;
  {
    v8::Context::Scope context_scope(creation_context->CreationContext());
    object = v8::Object::New(isolate);
  }
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  for (unsigned i = 0; i < value.size(); ++i) {
    v8::Local<v8::Value> v8_value = ToV8(value[i].second, object, isolate);
    if (v8_value.IsEmpty())
      v8_value = v8::Undefined(isolate);
    bool created_property;
    if (!object
             ->CreateDataProperty(
                 context, V8AtomicString(isolate, value[i].first), v8_value)
             .To(&created_property) ||
        !created_property) {
      return v8::Local<v8::Value>();
    }
  }
  return object;
}

template <typename Sequence>
inline v8::Local<v8::Value> ToV8SequenceInternal(
    const Sequence& sequence,
    v8::Local<v8::Object> creation_context,
    v8::Isolate* isolate) {
  RUNTIME_CALL_TIMER_SCOPE(isolate,
                           RuntimeCallStats::CounterId::kToV8SequenceInternal);
  v8::Local<v8::Array> array;
  {
    v8::Context::Scope context_scope(creation_context->CreationContext());
    array = v8::Array::New(isolate, SafeCast<int>(sequence.size()));
  }
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  uint32_t index = 0;
  typename Sequence::const_iterator end = sequence.end();
  for (typename Sequence::const_iterator iter = sequence.begin(); iter != end;
       ++iter) {
    v8::Local<v8::Value> value = ToV8(*iter, array, isolate);
    if (value.IsEmpty())
      value = v8::Undefined(isolate);
    bool created_property;
    if (!array->CreateDataProperty(context, index++, value)
             .To(&created_property) ||
        !created_property) {
      return v8::Local<v8::Value>();
    }
  }
  return array;
}

// Nullable

template <typename InnerType>
inline v8::Local<v8::Value> ToV8(const base::Optional<InnerType>& value,
                                 v8::Local<v8::Object> creation_context,
                                 v8::Isolate* isolate) {
  if (!value)
    return v8::Null(isolate);
  return ToV8(*value, creation_context, isolate);
}

// In all cases allow script state instead of creation context + isolate.
// Use this function only if the call site does not otherwise need the global,
// since v8::Context::Global is heavy.
template <typename T>
inline v8::Local<v8::Value> ToV8(T&& value, ScriptState* script_state) {
  return ToV8(std::forward<T>(value), script_state->GetContext()->Global(),
              script_state->GetIsolate());
}

// Only declare ToV8(void*,...) for checking function overload mismatch.
// This ToV8(void*,...) should be never used. So we will find mismatch
// because of "unresolved external symbol".
// Without ToV8(void*, ...), call to toV8 with T* will match with
// ToV8(bool, ...) if T is not a subclass of ScriptWrappable or if T is
// declared but not defined (so it's not clear that T is a subclass of
// ScriptWrappable).
// This hack helps detect such unwanted implicit conversions from T* to bool.
v8::Local<v8::Value> ToV8(void* value,
                          v8::Local<v8::Object> creation_context,
                          v8::Isolate*) = delete;

}  // namespace blink

#endif  // ToV8ForPlatform_h
