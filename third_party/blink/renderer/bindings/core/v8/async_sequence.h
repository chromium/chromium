// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_ASYNC_SEQUENCE_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_ASYNC_SEQUENCE_H_

#include "base/check.h"
#include "base/types/pass_key.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/script_iterator.h"
#include "third_party/blink/renderer/bindings/core/v8/world_safe_v8_reference.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-primitive.h"

namespace blink {

// Declared in to_v8_traits.h, which cannot be included here: it includes
// native_value_traits_impl.h, which includes this header.
template <typename T>
struct ToV8Traits;

namespace bindings {

// This is the state recorded when converting a JavaScript value to an IDL
// async_sequence<T>: the object, and the iteration method looked up on it. The
// method is looked up at conversion time but only called later, when the
// sequence is opened, which is what distinguishes an async sequence from a
// sequence. https://webidl.spec.whatwg.org/#js-async-iterable
//
// The type has no runtime flag of its own. Nothing is web-exposed until an
// operation declares an argument of this type, and that operation's
// [RuntimeEnabled] is the gate.
//
// This header is on native_value_traits_impl.h's include path, so it cannot
// include script_promise.h, which includes native_value_traits_impl.h. The
// part that reacts to promises lives in async_sequence_iterator.h.
class CORE_EXPORT AsyncSequenceBase
    : public GarbageCollected<AsyncSequenceBase> {
 public:
  // The object the value was converted from, so that handing an async sequence
  // back to script yields that object rather than a wrapper.
  v8::Local<v8::Object> V8Object(ScriptState* script_state) const;

  ScriptIterator::Kind Kind() const { return kind_; }

  void Trace(Visitor* visitor) const;

 protected:
  AsyncSequenceBase(v8::Isolate* isolate,
                    v8::Local<v8::Object> object,
                    v8::Local<v8::Function> method,
                    ScriptIterator::Kind kind);

  // https://webidl.spec.whatwg.org/#async-sequence-open, where step 2's
  // CreateAsyncFromSyncIterator() is ScriptIterator's kAsyncFromSync kind.
  ScriptIterator OpenInternal(ScriptState* script_state,
                              ExceptionState& exception_state) const;

 private:
  const WorldSafeV8Reference<v8::Object> object_;
  const WorldSafeV8Reference<v8::Function> method_;
  const ScriptIterator::Kind kind_;
};

}  // namespace bindings

template <typename T>
class AsyncSequenceIterator;

// An IDL async_sequence<T> value. Open it with AsyncSequenceIterator<T>::Open()
// to read the values it yields.
template <typename T>
class AsyncSequence final : public bindings::AsyncSequenceBase {
 public:
  using PassKey = base::PassKey<AsyncSequence<T>>;

  // Converting a JavaScript value to an IDL async sequence value, steps 2-4:
  // https://webidl.spec.whatwg.org/#js-async-iterable
  //
  // Like ScriptIterator::FromIterable(), returns nullptr without an exception
  // when `object` has neither iteration method, leaving step 3.2's TypeError to
  // the caller so that a union can try another member before giving up.
  static AsyncSequence* FromAsyncIterable(v8::Isolate* isolate,
                                          v8::Local<v8::Object> object,
                                          ExceptionState& exception_state) {
    v8::Local<v8::Function> method;
    ScriptIterator::Kind kind = ScriptIterator::Kind::kNull;
    if (!ScriptIterator::LookUpAsyncIterableMethod(isolate, object, &method,
                                                   &kind, exception_state)) {
      CHECK(exception_state.HadException());
      return nullptr;
    }
    if (method.IsEmpty()) {
      CHECK(!exception_state.HadException());
      return nullptr;
    }
    return MakeGarbageCollected<AsyncSequence>(PassKey(), isolate, object,
                                               method, kind);
  }

  AsyncSequence(PassKey,
                v8::Isolate* isolate,
                v8::Local<v8::Object> object,
                v8::Local<v8::Function> method,
                ScriptIterator::Kind kind)
      : AsyncSequenceBase(isolate, object, method, kind) {}

 private:
  friend class AsyncSequenceIterator<T>;

  ScriptIterator Open(ScriptState* script_state,
                      ExceptionState& exception_state) const {
    return OpenInternal(script_state, exception_state);
  }
};

// Defined here rather than in native_value_traits_impl.h and to_v8_traits.h so
// that AsyncSequence<T> stays with its traits.
template <typename T>
struct NativeValueTraits<IDLAsyncSequence<T>>
    : public NativeValueTraitsBase<IDLAsyncSequence<T>> {
  static AsyncSequence<T>* NativeValue(v8::Isolate* isolate,
                                       v8::Local<v8::Value> value,
                                       ExceptionState& exception_state) {
    // 1. If V is not an Object, then throw a TypeError.
    if (!value->IsObject()) {
      exception_state.ThrowTypeError(
          "The provided value cannot be converted to an async sequence.");
      return nullptr;
    }
    AsyncSequence<T>* async_sequence = AsyncSequence<T>::FromAsyncIterable(
        isolate, value.As<v8::Object>(), exception_state);
    if (exception_state.HadException()) {
      return nullptr;
    }
    // 3.2. If syncMethod is undefined, throw a TypeError.
    if (!async_sequence) {
      exception_state.ThrowTypeError(
          "The object must have a callable @@asyncIterator or @@iterator "
          "property.");
      return nullptr;
    }
    return async_sequence;
  }
};

template <typename T>
struct NativeValueTraits<IDLNullable<IDLAsyncSequence<T>>>
    : public NativeValueTraitsBase<IDLNullable<IDLAsyncSequence<T>>> {
  static AsyncSequence<T>* NativeValue(v8::Isolate* isolate,
                                       v8::Local<v8::Value> value,
                                       ExceptionState& exception_state) {
    if (value->IsNullOrUndefined()) {
      return nullptr;
    }
    return NativeValueTraits<IDLAsyncSequence<T>>::NativeValue(isolate, value,
                                                               exception_state);
  }
};

template <typename T>
struct ToV8Traits<IDLAsyncSequence<T>> {
  [[nodiscard]] static v8::Local<v8::Value> ToV8(
      ScriptState* script_state,
      const AsyncSequence<T>* async_sequence) {
    DCHECK(async_sequence);
    return async_sequence->V8Object(script_state);
  }
};

template <typename T>
struct ToV8Traits<IDLNullable<IDLAsyncSequence<T>>> {
  [[nodiscard]] static v8::Local<v8::Value> ToV8(
      ScriptState* script_state,
      const AsyncSequence<T>* async_sequence) {
    if (!async_sequence) {
      return v8::Null(script_state->GetIsolate());
    }
    return ToV8Traits<IDLAsyncSequence<T>>::ToV8(script_state, async_sequence);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_ASYNC_SEQUENCE_H_
