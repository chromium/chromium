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
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
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
class ScriptPromiseTyped;

// ScriptPromise is the class for representing Promise values in C++ world.
// ScriptPromise holds a Promise.
// Holding a `ScriptPromise` is rarely needed — typically you hold a
// `ScriptPromiseResolver` when creating a Promise and passing it *to*
// JavaScript — but is necessary when holding a promise received *from*
// JavaScript. If a promise is exposed as an attribute in IDL and you need to
// return the same promise on multiple invocations, use ScriptPromiseProperty.
//
// There are cases where promises cannot work (e.g., where the thread is being
// terminated). In such cases operations will silently fail, so you should not
// use promises for critical use such as releasing a resource.
class CORE_EXPORT ScriptPromise {
  DISALLOW_NEW();

 public:
  // Constructs an empty promise.
  ScriptPromise() = default;

  // Constructs a ScriptPromise from |promise|.
  // If |promise| is not a Promise object, throws a v8 TypeError.
  ScriptPromise(ScriptState*, v8::Local<v8::Value> promise);

  ScriptPromise(const ScriptPromise&);

  ~ScriptPromise() = default;

  ScriptPromiseTyped<IDLAny> Then(v8::Local<v8::Function> on_fulfilled,
                                  v8::Local<v8::Function> on_rejected = {});
  ScriptPromiseTyped<IDLAny> Then(ScriptFunction* on_fulfilled,
                                  ScriptFunction* on_rejected = nullptr);

  bool IsObject() const { return promise_.IsObject(); }

  bool IsNull() const { return promise_.IsNull(); }

  bool IsUndefinedOrNull() const {
    return promise_.IsUndefined() || promise_.IsNull();
  }

  ScriptValue AsScriptValue() const { return promise_; }

  v8::Local<v8::Value> V8Value() const { return promise_.V8Value(); }
  v8::Local<v8::Promise> V8Promise() const {
    // This is safe because `promise_` always stores a promise value as long
    // as it's non-empty.
    return V8Value().As<v8::Promise>();
  }

  v8::Isolate* GetIsolate() const { return script_state_->GetIsolate(); }

  bool IsEmpty() const { return promise_.IsEmpty(); }

  void Clear() { promise_.Clear(); }

  // Marks this promise as handled to avoid reporting unhandled rejections.
  void MarkAsHandled();

  bool operator==(const ScriptPromise& value) const {
    return promise_ == value.promise_;
  }

  bool operator!=(const ScriptPromise& value) const {
    return !operator==(value);
  }

  // Constructs and returns a ScriptPromise from |value|.
  // if `value` is not a Promise object, returns a Promise object
  // resolved with `value`.
  // Returns `value` itself if it is a Promise.
  // This is intended only for cases where we are receiving an arbitrary
  // `value` of unknown type from script. If constructing a ScriptPromise of
  // known type, use ToResolvedPromise<>.
  static ScriptPromise FromUntypedValueForBindings(ScriptState*,
                                                   v8::Local<v8::Value>);

  // Constructs and returns a ScriptPromise resolved with undefined.
  static ScriptPromise CastUndefined(ScriptState*);

  static ScriptPromise Reject(ScriptState*, const ScriptValue&);
  static ScriptPromise Reject(ScriptState*, v8::Local<v8::Value>);
  // Rejects with a given exception. The ExceptionState gets cleared.
  static ScriptPromise Reject(ScriptState*, ExceptionState&);

  static ScriptPromise RejectWithDOMException(ScriptState*, DOMException*);

  static v8::Local<v8::Promise> RejectRaw(ScriptState*, v8::Local<v8::Value>);

  // Constructs and returns a ScriptPromise to be resolved when all |promises|
  // are resolved. If one of |promises| is rejected, the returned
  // ScriptPromise is rejected.
  static ScriptPromise All(ScriptState*,
                           const HeapVector<ScriptPromise>& promises);

  void Trace(Visitor* visitor) const {
    visitor->Trace(promise_);
    visitor->Trace(script_state_);
  }

  // This is a utility class intended to be used internally.
  // ScriptPromiseResolver is for general purpose.
  class CORE_EXPORT InternalResolver {
    DISALLOW_NEW();

   public:
    explicit InternalResolver(ScriptState*);
    v8::Local<v8::Promise> V8Promise() const;
    ScriptPromise Promise() const;
    void Resolve(v8::Local<v8::Value>);
    void Reject(v8::Local<v8::Value>);
    void Clear() { resolver_.Clear(); }
    ScriptState* GetScriptState() const { return script_state_.Get(); }
    void Trace(Visitor* visitor) const {
      visitor->Trace(script_state_);
      visitor->Trace(resolver_);
    }

   protected:
    Member<ScriptState> script_state_;
    ScriptValue resolver_;
  };

  bool IsAssociatedWith(ScriptState* script_state) const {
    return script_state == script_state_;
  }

 private:
  Member<ScriptState> script_state_;
  ScriptValue promise_;
};

template <typename IDLResolvedType>
class ScriptPromiseTyped : public ScriptPromise {
 public:
  ScriptPromiseTyped() = default;
  ScriptPromiseTyped(ScriptState* script_state, v8::Local<v8::Value> promise)
      : ScriptPromise(script_state, promise) {}

  class InternalResolverTyped : public ScriptPromise::InternalResolver {
   public:
    explicit InternalResolverTyped(ScriptState* script_state)
        : InternalResolver(script_state) {}

    ScriptPromiseTyped<IDLResolvedType> Promise() {
      if (resolver_.IsEmpty()) {
        return ScriptPromiseTyped<IDLResolvedType>();
      }
      return ScriptPromiseTyped<IDLResolvedType>(script_state_, V8Promise());
    }

    static InternalResolverTyped GetTyped(InternalResolver& resolver) {
      static_assert(sizeof(InternalResolverTyped) == sizeof(InternalResolver));
      return static_cast<InternalResolverTyped&>(resolver);
    }
  };

  static ScriptPromiseTyped<IDLResolvedType> RejectWithDOMException(
      ScriptState* script_state,
      DOMException* exception) {
    return Reject(script_state, exception->ToV8(script_state));
  }

  static ScriptPromiseTyped<IDLResolvedType> Reject(ScriptState* script_state,
                                                    const ScriptValue& value) {
    return Reject(script_state, value.V8Value());
  }

  static ScriptPromiseTyped<IDLResolvedType> Reject(
      ScriptState* script_state,
      v8::Local<v8::Value> value) {
    if (value.IsEmpty()) {
      return ScriptPromiseTyped<IDLResolvedType>();
    }
    InternalResolverTyped resolver(script_state);
    ScriptPromiseTyped<IDLResolvedType> promise = resolver.Promise();
    resolver.Reject(value);
    return promise;
  }

  static ScriptPromiseTyped<IDLResolvedType> Reject(
      ScriptState* script_state,
      ExceptionState& exception_state) {
    DCHECK(exception_state.HadException());
    auto promise = Reject(script_state, exception_state.GetException());
    exception_state.ClearException();
    return promise;
  }
};

// Defined in to_v8_traits.h due to circular dependency.
template <typename IDLType, typename BlinkType>
ScriptPromiseTyped<IDLType> ToResolvedPromise(ScriptState*, BlinkType value);

CORE_EXPORT ScriptPromiseTyped<IDLUndefined> ToResolvedUndefinedPromise(
    ScriptState*);

}  // namespace blink

namespace WTF {

template <>
struct VectorTraits<blink::ScriptPromise>
    : VectorTraitsBase<blink::ScriptPromise> {
  STATIC_ONLY(VectorTraits);
  static constexpr bool kCanClearUnusedSlotsWithMemset = true;
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_PROMISE_H_
