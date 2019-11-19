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

#include "base/macros.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
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
    if (promises.IsEmpty())
      return ScriptPromise::Cast(script_state,
                                 v8::Array::New(script_state->GetIsolate()));
    return (MakeGarbageCollected<PromiseAllHandler>(script_state, promises))
        ->resolver_.Promise();
  }

  PromiseAllHandler(ScriptState* script_state,
                    HeapVector<ScriptPromise> promises)
      : number_of_pending_promises_(promises.size()), resolver_(script_state) {
    DCHECK(!promises.IsEmpty());
    values_.resize(promises.size());
    for (wtf_size_t i = 0; i < promises.size(); ++i) {
      promises[i].Then(CreateFulfillFunction(script_state, i),
                       CreateRejectFunction(script_state));
    }
  }

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(resolver_);
    visitor->Trace(values_);
  }

 private:
  class AdapterFunction : public ScriptFunction {
   public:
    enum ResolveType {
      kFulfilled,
      kRejected,
    };

    static v8::Local<v8::Function> Create(ScriptState* script_state,
                                          ResolveType resolve_type,
                                          wtf_size_t index,
                                          PromiseAllHandler* handler) {
      AdapterFunction* self = MakeGarbageCollected<AdapterFunction>(
          script_state, resolve_type, index, handler);
      return self->BindToV8Function();
    }

    AdapterFunction(ScriptState* script_state,
                    ResolveType resolve_type,
                    wtf_size_t index,
                    PromiseAllHandler* handler)
        : ScriptFunction(script_state),
          resolve_type_(resolve_type),
          index_(index),
          handler_(handler) {}

    void Trace(blink::Visitor* visitor) override {
      visitor->Trace(handler_);
      ScriptFunction::Trace(visitor);
    }

   private:
    ScriptValue Call(ScriptValue value) override {
      if (resolve_type_ == kFulfilled)
        handler_->OnFulfilled(index_, value);
      else
        handler_->OnRejected(value);
      // This return value is never used.
      return ScriptValue();
    }

    const ResolveType resolve_type_;
    const wtf_size_t index_;
    Member<PromiseAllHandler> handler_;
  };

  v8::Local<v8::Function> CreateFulfillFunction(ScriptState* script_state,
                                                wtf_size_t index) {
    return AdapterFunction::Create(script_state, AdapterFunction::kFulfilled,
                                   index, this);
  }

  v8::Local<v8::Function> CreateRejectFunction(ScriptState* script_state) {
    return AdapterFunction::Create(script_state, AdapterFunction::kRejected, 0,
                                   this);
  }

  void OnFulfilled(wtf_size_t index, const ScriptValue& value) {
    if (is_settled_)
      return;

    DCHECK_LT(index, values_.size());
    values_[index] = value;
    if (--number_of_pending_promises_ > 0)
      return;

    v8::Local<v8::Value> values = ToV8(values_, resolver_.GetScriptState());
    MarkPromiseSettled();
    resolver_.Resolve(values);
  }

  void OnRejected(const ScriptValue& value) {
    if (is_settled_)
      return;
    MarkPromiseSettled();
    resolver_.Reject(value.V8Value());
  }

  void MarkPromiseSettled() {
    DCHECK(!is_settled_);
    is_settled_ = true;
    values_.clear();
  }

  size_t number_of_pending_promises_;
  ScriptPromise::InternalResolver resolver_;
  bool is_settled_ = false;

  // This is cleared when owners of this handler, that is, given promises are
  // settled.
  HeapVector<ScriptValue> values_;

  DISALLOW_COPY_AND_ASSIGN(PromiseAllHandler);
};

}  // namespace

ScriptPromise::InternalResolver::InternalResolver(ScriptState* script_state)
    : script_state_(script_state),
      resolver_(script_state->GetIsolate(),
                v8::Promise::Resolver::New(script_state->GetContext())) {
  // |resolver| can be empty when the thread is being terminated. We ignore such
  // errors.
}

v8::Local<v8::Promise> ScriptPromise::InternalResolver::V8Promise() const {
  if (resolver_.IsEmpty())
    return v8::Local<v8::Promise>();
  return resolver_.V8Value().As<v8::Promise::Resolver>()->GetPromise();
}

ScriptPromise ScriptPromise::InternalResolver::Promise() const {
  if (resolver_.IsEmpty())
    return ScriptPromise();
  return ScriptPromise(script_state_, V8Promise());
}

void ScriptPromise::InternalResolver::Resolve(v8::Local<v8::Value> value) {
  if (resolver_.IsEmpty())
    return;
  v8::Maybe<bool> result =
      resolver_.V8Value().As<v8::Promise::Resolver>()->Resolve(
          script_state_->GetContext(), value);
  // |result| can be empty when the thread is being terminated. We ignore such
  // errors.
  ALLOW_UNUSED_LOCAL(result);

  Clear();
}

void ScriptPromise::InternalResolver::Reject(v8::Local<v8::Value> value) {
  if (resolver_.IsEmpty())
    return;
  v8::Maybe<bool> result =
      resolver_.V8Value().As<v8::Promise::Resolver>()->Reject(
          script_state_->GetContext(), value);
  // |result| can be empty when the thread is being terminated. We ignore such
  // errors.
  ALLOW_UNUSED_LOCAL(result);

  Clear();
}

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
  this->script_state_ = other.script_state_;
  this->promise_ = other.promise_;
}

ScriptPromise ScriptPromise::Then(v8::Local<v8::Function> on_fulfilled,
                                  v8::Local<v8::Function> on_rejected) {
  if (promise_.IsEmpty())
    return ScriptPromise();

  v8::Local<v8::Promise> promise = promise_.V8Value().As<v8::Promise>();

  if (on_fulfilled.IsEmpty() && on_rejected.IsEmpty())
    return *this;

  v8::Local<v8::Promise> result_promise;
  if (on_rejected.IsEmpty()) {
    if (!promise->Then(script_state_->GetContext(), on_fulfilled)
             .ToLocal(&result_promise)) {
      return ScriptPromise();
    }
    return ScriptPromise(script_state_, result_promise);
  }

  if (on_fulfilled.IsEmpty()) {
    if (!promise->Catch(script_state_->GetContext(), on_rejected)
             .ToLocal(&result_promise)) {
      return ScriptPromise();
    }
    return ScriptPromise(script_state_, result_promise);
  }

  if (!promise->Then(script_state_->GetContext(), on_fulfilled, on_rejected)
           .ToLocal(&result_promise)) {
    return ScriptPromise();
  }
  return ScriptPromise(script_state_, result_promise);
}

ScriptPromise ScriptPromise::CastUndefined(ScriptState* script_state) {
  return ScriptPromise::Cast(script_state,
                             v8::Undefined(script_state->GetIsolate()));
}

ScriptPromise ScriptPromise::Cast(ScriptState* script_state,
                                  const ScriptValue& value) {
  return ScriptPromise::Cast(script_state, value.V8Value());
}

ScriptPromise ScriptPromise::Cast(ScriptState* script_state,
                                  v8::Local<v8::Value> value) {
  if (value.IsEmpty())
    return ScriptPromise();
  if (value->IsPromise()) {
    return ScriptPromise(script_state, value);
  }
  InternalResolver resolver(script_state);
  ScriptPromise promise = resolver.Promise();
  resolver.Resolve(value);
  return promise;
}

ScriptPromise ScriptPromise::Reject(ScriptState* script_state,
                                    const ScriptValue& value) {
  return ScriptPromise::Reject(script_state, value.V8Value());
}

ScriptPromise ScriptPromise::Reject(ScriptState* script_state,
                                    v8::Local<v8::Value> value) {
  if (value.IsEmpty())
    return ScriptPromise();
  InternalResolver resolver(script_state);
  ScriptPromise promise = resolver.Promise();
  resolver.Reject(value);
  return promise;
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
                ToV8(exception, script_state->GetContext()->Global(),
                     script_state->GetIsolate()));
}

v8::Local<v8::Promise> ScriptPromise::RejectRaw(ScriptState* script_state,
                                                v8::Local<v8::Value> value) {
  if (value.IsEmpty())
    return v8::Local<v8::Promise>();
  v8::Local<v8::Promise::Resolver> resolver;
  if (!v8::Promise::Resolver::New(script_state->GetContext())
           .ToLocal(&resolver))
    return v8::Local<v8::Promise>();
  v8::Local<v8::Promise> promise = resolver->GetPromise();
  resolver->Reject(script_state->GetContext(), value).ToChecked();
  return promise;
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

}  // namespace blink
