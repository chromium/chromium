/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_PROMISE_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_PROMISE_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/stack_allocated.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

namespace blink {

class DOMException;
class ScriptFunction;

template <typename IDLResolvedType>
class ScriptPromise;

// ScriptPromise is the class for representing Promise values in C++
// world. ScriptPromise holds a Promise. Holding a `ScriptPromise`
// is rarely needed — typically you hold a `ScriptPromiseResolver` when creating
// a Promise and passing it *to* JavaScript — but is necessary when
// holding a promise received *from* JavaScript. If a promise is exposed as an
// attribute in IDL and you need to return the same promise on multiple
// invocations, use ScriptPromiseProperty.
//
// There are cases where promises cannot work (e.g., where the thread is being
// terminated). In such cases operations will silently fail, so you should not
// use promises for critical use such as releasing a resource.
class CORE_EXPORT ScriptPromiseUntyped {
  DISALLOW_NEW();

 public:
  // Constructs an empty promise.
  ScriptPromiseUntyped() = default;

  // Constructs a ScriptPromiseUntyped from |promise|.
  ScriptPromiseUntyped(v8::Isolate*, v8::Local<v8::Promise> promise);

  ScriptPromiseUntyped(const ScriptPromiseUntyped&);

  ~ScriptPromiseUntyped() = default;

  ScriptPromise<IDLAny> Then(ScriptFunction* on_fulfilled,
                             ScriptFunction* on_rejected = nullptr);

  v8::Local<v8::Value> V8Value() const { return promise_.V8Value(); }
  v8::Local<v8::Promise> V8Promise() const {
    // This is safe because `promise_` always stores a promise value as long
    // as it's non-empty.
    return V8Value().As<v8::Promise>();
  }

  bool IsEmpty() const { return promise_.IsEmpty(); }

  void Clear() { promise_.Clear(); }

  // Marks this promise as handled to avoid reporting unhandled rejections.
  void MarkAsHandled();

  bool operator==(const ScriptPromiseUntyped& value) const {
    return promise_ == value.promise_;
  }

  bool operator!=(const ScriptPromiseUntyped& value) const {
    return !operator==(value);
  }

  static ScriptPromiseUntyped Reject(ScriptState*, const ScriptValue&);
  static ScriptPromiseUntyped Reject(ScriptState*, v8::Local<v8::Value>);

  // Constructs and returns a ScriptPromiseUntyped to be resolved when all
  // |promises| are resolved. If one of |promises| is rejected, the returned
  // ScriptPromiseUntyped is rejected.
  static ScriptPromiseUntyped All(
      ScriptState*,
      const HeapVector<ScriptPromiseUntyped>& promises);

  void Trace(Visitor* visitor) const { visitor->Trace(promise_); }

 protected:
  template <typename IDLType, typename BlinkType>
  friend ScriptPromise<IDLType> ToResolvedPromise(ScriptState*, BlinkType);

  static v8::Local<v8::Promise> ResolveRaw(ScriptState*, v8::Local<v8::Value>);
  static v8::Local<v8::Promise> RejectRaw(ScriptState*, v8::Local<v8::Value>);

 private:
  ScriptValue promise_;
};

template <typename IDLResolvedType>
class ScriptPromise : public ScriptPromiseUntyped {
 public:
  ScriptPromise() = default;

  static ScriptPromise<IDLResolvedType> FromV8Promise(
      v8::Isolate* isolate,
      v8::Local<v8::Promise> promise) {
    return ScriptPromise<IDLResolvedType>(isolate, promise);
  }

  static ScriptPromise<IDLResolvedType> FromV8Value(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value) {
    if (value.IsEmpty()) {
      return ScriptPromise<IDLResolvedType>();
    }
    v8::Local<v8::Promise> promise =
        value->IsPromise()
            ? value.As<v8::Promise>()
            : ResolveRaw(ScriptState::ForCurrentRealm(isolate), value);
    return ScriptPromise<IDLResolvedType>(isolate, promise);
  }

  static ScriptPromise<IDLResolvedType> RejectWithDOMException(
      ScriptState* script_state,
      DOMException* exception) {
    return Reject(script_state, exception->ToV8(script_state));
  }

  static ScriptPromise<IDLResolvedType> Reject(ScriptState* script_state,
                                               const ScriptValue& value) {
    return Reject(script_state, value.V8Value());
  }

  static ScriptPromise<IDLResolvedType> Reject(ScriptState* script_state,
                                               v8::Local<v8::Value> value) {
    if (value.IsEmpty()) {
      return ScriptPromise<IDLResolvedType>();
    }
    return ScriptPromise<IDLResolvedType>(
        script_state->GetIsolate(),
        ScriptPromiseUntyped::RejectRaw(script_state, value));
  }

  void MarkAsSilent() {
    if (!IsEmpty()) {
      V8Promise()->MarkAsSilent();
    }
  }

 private:
  template <typename IDLType>
  friend class ScriptPromiseResolver;

  template <typename IDLType, typename BlinkType>
  friend ScriptPromise<IDLType> ToResolvedPromise(ScriptState*, BlinkType);

  ScriptPromise(v8::Isolate* isolate, v8::Local<v8::Promise> promise)
      : ScriptPromiseUntyped(isolate, promise) {}
};

// Defined in to_v8_traits.h due to circular dependency.
template <typename IDLType, typename BlinkType>
ScriptPromise<IDLType> ToResolvedPromise(ScriptState*, BlinkType value);

CORE_EXPORT ScriptPromise<IDLUndefined> ToResolvedUndefinedPromise(
    ScriptState*);

// EmptyPromise() is a value similar to std::nullopt that can be used to return
// an empty ScriptPromise of any type. It is intended to be used when throwing
// an exception using an ExceptionState object, since in that case the bindings
// ignore the contents of the returned promise.
//
// The usual patterns for usage are:
//
//   if (bad thing) {
//     exception_state.ThrowRangeError("bad thing");
//     return EmptyPromise();
//   }
//
// or
//
//   FunctionThatMightThrow(script_state, exception_state);
//   if (exception_state.HadException()) {
//     return EmptyPromise();
//   }
class EmptyPromise {
  STACK_ALLOCATED();

 public:
  template <typename IDLType>
  // Intentionally permit implicit conversion. NOLINTNEXTLINE.
  operator ScriptPromise<IDLType>() {
    return ScriptPromise<IDLType>();
  }
};

}  // namespace blink

namespace WTF {

template <>
struct VectorTraits<blink::ScriptPromiseUntyped>
    : VectorTraitsBase<blink::ScriptPromiseUntyped> {
  STATIC_ONLY(VectorTraits);
  static constexpr bool kCanClearUnusedSlotsWithMemset = true;
};

template <typename T>
struct VectorTraits<blink::ScriptPromise<T>>
    : VectorTraitsBase<blink::ScriptPromise<T>> {
  STATIC_ONLY(VectorTraits);
  static constexpr bool kCanClearUnusedSlotsWithMemset = true;
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_PROMISE_H_
