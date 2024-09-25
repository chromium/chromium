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
  static ScriptPromiseUntyped All(
      ScriptState* script_state,
      const HeapVector<ScriptPromiseUntyped>& promises) {
    if (promises.empty()) {
      return ToResolvedPromise<IDLSequence<IDLAny>>(script_state,
                                                    HeapVector<ScriptValue>());
    }
    auto* resolver =
        MakeGarbageCollected<ScriptPromiseResolver<IDLSequence<IDLAny>>>(
            script_state);
    MakeGarbageCollected<PromiseAllHandler>(script_state, promises, resolver);
    return resolver->Promise();
  }

  PromiseAllHandler(ScriptState* script_state,
                    HeapVector<ScriptPromiseUntyped> promises,
                    ScriptPromiseResolver<IDLSequence<IDLAny>>* resolver)
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

  ScriptFunction* CreateFulfillFunction(ScriptState* script_state,
                                        wtf_size_t index) {
    return MakeGarbageCollected<ScriptFunction>(
        script_state, MakeGarbageCollected<AdapterFunction>(
                          AdapterFunction::kFulfilled, index, this));
  }

  ScriptFunction* CreateRejectFunction(ScriptState* script_state) {
    return MakeGarbageCollected<ScriptFunction>(
        script_state, MakeGarbageCollected<AdapterFunction>(
                          AdapterFunction::kRejected, 0, this));
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
  Member<ScriptPromiseResolver<IDLSequence<IDLAny>>> resolver_;
  bool is_settled_ = false;

  // This is cleared when owners of this handler, that is, given promises are
  // settled.
  HeapVector<ScriptValue> values_;
};

}  // namespace

ScriptPromiseUntyped::ScriptPromiseUntyped(v8::Isolate* isolate,
                                           v8::Local<v8::Promise> promise)
    : promise_(isolate, promise) {}

ScriptPromiseUntyped::ScriptPromiseUntyped(const ScriptPromiseUntyped& other) {
  promise_ = other.promise_;
}

ScriptPromise<IDLAny> ScriptPromiseUntyped::Then(ScriptFunction* on_fulfilled,
                                                 ScriptFunction* on_rejected) {
  CHECK(on_fulfilled || on_rejected);
  CHECK(!on_fulfilled || !on_rejected ||
        on_fulfilled->GetScriptState() == on_rejected->GetScriptState());
  if (promise_.IsEmpty())
    return EmptyPromise();

  v8::Local<v8::Promise> promise = V8Promise();
  v8::Local<v8::Promise> result_promise;
  ScriptState* script_state = on_fulfilled ? on_fulfilled->GetScriptState()
                                           : on_rejected->GetScriptState();
  if (!on_rejected) {
    if (!promise->Then(script_state->GetContext(), on_fulfilled->V8Function())
             .ToLocal(&result_promise)) {
      return EmptyPromise();
    }
  } else if (!on_fulfilled) {
    if (!promise->Catch(script_state->GetContext(), on_rejected->V8Function())
             .ToLocal(&result_promise)) {
      return EmptyPromise();
    }
  } else {
    if (!promise
             ->Then(script_state->GetContext(), on_fulfilled->V8Function(),
                    on_rejected->V8Function())
             .ToLocal(&result_promise)) {
      return EmptyPromise();
    }
  }
  return ScriptPromise<IDLAny>::FromV8Promise(script_state->GetIsolate(),
                                              result_promise);
}

ScriptPromiseUntyped ScriptPromiseUntyped::Reject(ScriptState* script_state,
                                                  const ScriptValue& value) {
  return ScriptPromiseUntyped::Reject(script_state, value.V8Value());
}

ScriptPromiseUntyped ScriptPromiseUntyped::Reject(ScriptState* script_state,
                                                  v8::Local<v8::Value> value) {
  return ScriptPromiseUntyped(script_state->GetIsolate(),
                              RejectRaw(script_state, value));
}

v8::Local<v8::Promise> ScriptPromiseUntyped::ResolveRaw(
    ScriptState* script_state,
    v8::Local<v8::Value> value) {
  v8::MicrotasksScope microtasks_scope(
      script_state->GetIsolate(), ToMicrotaskQueue(script_state),
      v8::MicrotasksScope::kDoNotRunMicrotasks);
  auto resolver =
      v8::Promise::Resolver::New(script_state->GetContext()).ToLocalChecked();
  std::ignore = resolver->Resolve(script_state->GetContext(), value);
  return resolver->GetPromise();
}

v8::Local<v8::Promise> ScriptPromiseUntyped::RejectRaw(
    ScriptState* script_state,
    v8::Local<v8::Value> value) {
  v8::MicrotasksScope microtasks_scope(
      script_state->GetIsolate(), ToMicrotaskQueue(script_state),
      v8::MicrotasksScope::kDoNotRunMicrotasks);
  auto resolver =
      v8::Promise::Resolver::New(script_state->GetContext()).ToLocalChecked();
  std::ignore = resolver->Reject(script_state->GetContext(), value);
  return resolver->GetPromise();
}

void ScriptPromiseUntyped::MarkAsHandled() {
  if (promise_.IsEmpty())
    return;
  promise_.V8Value().As<v8::Promise>()->MarkAsHandled();
}

ScriptPromiseUntyped ScriptPromiseUntyped::All(
    ScriptState* script_state,
    const HeapVector<ScriptPromiseUntyped>& promises) {
  return PromiseAllHandler::All(script_state, promises);
}

ScriptPromise<IDLUndefined> ToResolvedUndefinedPromise(
    ScriptState* script_state) {
  return ToResolvedPromise<IDLUndefined>(script_state,
                                         ToV8UndefinedGenerator());
}

}  // namespace blink
