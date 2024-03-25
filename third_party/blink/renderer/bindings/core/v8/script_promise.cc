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

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"

#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

class PromiseAllHandler final : public GarbageCollected<PromiseAllHandler> {
 public:
  static ScriptPromise All(ScriptState* script_state,
                           const HeapVector<ScriptPromise>& promises) {
    if (promises.empty()) {
      return ScriptPromise::FromUntypedValueForBindings(
          script_state, v8::Array::New(script_state->GetIsolate()));
    }
    auto* resolver =
        MakeGarbageCollected<ScriptPromiseResolverTyped<IDLSequence<IDLAny>>>(
            script_state);
    MakeGarbageCollected<PromiseAllHandler>(script_state, promises, resolver);
    return resolver->Promise();
  }

  PromiseAllHandler(ScriptState* script_state,
                    HeapVector<ScriptPromise> promises,
                    ScriptPromiseResolverTyped<IDLSequence<IDLAny>>* resolver)
      : number_of_pending_promises_(promises.size()), resolver_(resolver) {
    DCHECK(!promises.empty());
    values_.resize(promises.size());
    for (wtf_size_t i = 0; i < promises.size(); ++i) {
      promises[i].Then(CreateFulfillFunction(script_state, i),
                       CreateRejectFunction(script_state));
    }
  }

  PromiseAllHandler(const PromiseAllHandler&) = delete;
  PromiseAllHandler& operator=(const PromiseAllHandler&) = delete;

  virtual void Trace(Visitor* visitor) const {
    visitor->Trace(resolver_);
    visitor->Trace(values_);
  }

 private:
  class AdapterFunction : public ScriptFunction::Callable {
   public:
    enum ResolveType {
      kFulfilled,
      kRejected,
    };

    AdapterFunction(ResolveType resolve_type,
                    wtf_size_t index,
                    PromiseAllHandler* handler)
        : resolve_type_(resolve_type), index_(index), handler_(handler) {}

    void Trace(Visitor* visitor) const override {
      visitor->Trace(handler_);
      ScriptFunction::Callable::Trace(visitor);
    }

    ScriptValue Call(ScriptState*, ScriptValue value) override {
      if (resolve_type_ == kFulfilled)
        handler_->OnFulfilled(index_, value);
      else
        handler_->OnRejected(value);
      // This return value is never used.
      return ScriptValue();
    }

   private:
    const ResolveType resolve_type_;
    const wtf_size_t index_;
    Member<PromiseAllHandler> handler_;
  };

  v8::Local<v8::Function> CreateFulfillFunction(ScriptState* script_state,
                                                wtf_size_t index) {
    return MakeGarbageCollected<ScriptFunction>(
               script_state, MakeGarbageCollected<AdapterFunction>(
                                 AdapterFunction::kFulfilled, index, this))
        ->V8Function();
  }

  v8::Local<v8::Function> CreateRejectFunction(ScriptState* script_state) {
    return MakeGarbageCollected<ScriptFunction>(
               script_state, MakeGarbageCollected<AdapterFunction>(
                                 AdapterFunction::kRejected, 0, this))
        ->V8Function();
  }

  void OnFulfilled(wtf_size_t index, const ScriptValue& value) {
    if (is_settled_)
      return;

    DCHECK_LT(index, values_.size());
    values_[index] = value;
    if (--number_of_pending_promises_ > 0)
      return;

    is_settled_ = true;
    resolver_->Resolve(values_);
    values_.clear();
  }

  void OnRejected(const ScriptValue& value) {
    if (is_settled_)
      return;
    is_settled_ = true;
    resolver_->Reject(value);
    values_.clear();
  }

  size_t number_of_pending_promises_;
  Member<ScriptPromiseResolverTyped<IDLSequence<IDLAny>>> resolver_;
  bool is_settled_ = false;

  // This is cleared when owners of this handler, that is, given promises are
  // settled.
  HeapVector<ScriptValue> values_;
};

}  // namespace

ScriptPromise::ScriptPromise(ScriptState* script_state,
                             v8::Local<v8::Value> value)
    : script_state_(script_state) {
  if (value.IsEmpty())
    return;

  if (!value->IsPromise()) {
    promise_ = ScriptValue();
    V8ThrowException::ThrowTypeError(script_state->GetIsolate(),
                                     "the given value is not a Promise");
    return;
  }
  promise_ = ScriptValue(script_state->GetIsolate(), value);
}

ScriptPromise::ScriptPromise(const ScriptPromise& other) {
  script_state_ = other.script_state_;
  promise_ = other.promise_;
}

ScriptPromiseTyped<IDLAny> ScriptPromise::Then(
    v8::Local<v8::Function> on_fulfilled,
    v8::Local<v8::Function> on_rejected) {
  if (promise_.IsEmpty())
    return ScriptPromiseTyped<IDLAny>();

  v8::Local<v8::Promise> promise = promise_.V8Value().As<v8::Promise>();

  if (on_fulfilled.IsEmpty() && on_rejected.IsEmpty())
    return ScriptPromiseTyped<IDLAny>::FromV8Promise(script_state_, promise);

  v8::Local<v8::Promise> result_promise;
  if (on_rejected.IsEmpty()) {
    if (!promise->Then(script_state_->GetContext(), on_fulfilled)
             .ToLocal(&result_promise)) {
      return ScriptPromiseTyped<IDLAny>();
    }
    return ScriptPromiseTyped<IDLAny>::FromV8Promise(script_state_,
                                                     result_promise);
  }

  if (on_fulfilled.IsEmpty()) {
    if (!promise->Catch(script_state_->GetContext(), on_rejected)
             .ToLocal(&result_promise)) {
      return ScriptPromiseTyped<IDLAny>();
    }
    return ScriptPromiseTyped<IDLAny>::FromV8Promise(script_state_,
                                                     result_promise);
  }

  if (!promise->Then(script_state_->GetContext(), on_fulfilled, on_rejected)
           .ToLocal(&result_promise)) {
    return ScriptPromiseTyped<IDLAny>();
  }
  return ScriptPromiseTyped<IDLAny>::FromV8Promise(script_state_,
                                                   result_promise);
}

ScriptPromiseTyped<IDLAny> ScriptPromise::Then(ScriptFunction* on_fulfilled,
                                               ScriptFunction* on_rejected) {
  const v8::Local<v8::Function> empty;
  return Then(on_fulfilled ? on_fulfilled->V8Function() : empty,
              on_rejected ? on_rejected->V8Function() : empty);
}

ScriptPromise ScriptPromise::CastUndefined(ScriptState* script_state) {
  return FromUntypedValueForBindings(script_state,
                                     v8::Undefined(script_state->GetIsolate()));
}

ScriptPromise ScriptPromise::FromUntypedValueForBindings(
    ScriptState* script_state,
    v8::Local<v8::Value> value) {
  if (value.IsEmpty())
    return ScriptPromise();
  if (value->IsPromise()) {
    return ScriptPromise(script_state, value);
  }
  return ScriptPromise(script_state, ResolveRaw(script_state, value));
}

ScriptPromise ScriptPromise::Reject(ScriptState* script_state,
                                    const ScriptValue& value) {
  return ScriptPromise::Reject(script_state, value.V8Value());
}

ScriptPromise ScriptPromise::Reject(ScriptState* script_state,
                                    v8::Local<v8::Value> value) {
  return ScriptPromise(script_state, RejectRaw(script_state, value));
}

ScriptPromise ScriptPromise::Reject(ScriptState* script_state,
                                    ExceptionState& exception_state) {
  DCHECK(exception_state.HadException());
  ScriptPromise promise = Reject(script_state, exception_state.GetException());
  exception_state.ClearException();
  return promise;
}

ScriptPromise ScriptPromise::RejectWithDOMException(ScriptState* script_state,
                                                    DOMException* exception) {
  DCHECK(script_state->GetIsolate()->InContext());
  return Reject(script_state,
                ToV8Traits<DOMException>::ToV8(script_state, exception));
}

v8::Local<v8::Promise> ScriptPromise::ResolveRaw(ScriptState* script_state,
                                                 v8::Local<v8::Value> value) {
  v8::MicrotasksScope microtasks_scope(
      script_state->GetIsolate(), ToMicrotaskQueue(script_state),
      v8::MicrotasksScope::kDoNotRunMicrotasks);
  auto resolver =
      v8::Promise::Resolver::New(script_state->GetContext()).ToLocalChecked();
  std::ignore = resolver->Resolve(script_state->GetContext(), value);
  return resolver->GetPromise();
}

v8::Local<v8::Promise> ScriptPromise::RejectRaw(ScriptState* script_state,
                                                v8::Local<v8::Value> value) {
  v8::MicrotasksScope microtasks_scope(
      script_state->GetIsolate(), ToMicrotaskQueue(script_state),
      v8::MicrotasksScope::kDoNotRunMicrotasks);
  auto resolver =
      v8::Promise::Resolver::New(script_state->GetContext()).ToLocalChecked();
  std::ignore = resolver->Reject(script_state->GetContext(), value);
  return resolver->GetPromise();
}

void ScriptPromise::MarkAsHandled() {
  if (promise_.IsEmpty())
    return;
  promise_.V8Value().As<v8::Promise>()->MarkAsHandled();
}

ScriptPromise ScriptPromise::All(ScriptState* script_state,
                                 const HeapVector<ScriptPromise>& promises) {
  return PromiseAllHandler::All(script_state, promises);
}

ScriptPromiseTyped<IDLUndefined> ToResolvedUndefinedPromise(
    ScriptState* script_state) {
  return ToResolvedPromise<IDLUndefined>(script_state,
                                         ToV8UndefinedGenerator());
}

}  // namespace blink
