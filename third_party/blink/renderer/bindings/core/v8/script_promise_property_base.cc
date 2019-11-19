// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_promise_property_base.h"

#include <memory>
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/scoped_persistent.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

ScriptPromisePropertyBase::ScriptPromisePropertyBase(
    ExecutionContext* execution_context,
    Name name)
    : ContextClient(execution_context),
      isolate_(execution_context->GetIsolate()),
      name_(name),
      state_(kPending) {}

ScriptPromisePropertyBase::~ScriptPromisePropertyBase() {
  // TODO(haraken): Stop calling ClearWrappers here, as the dtor is invoked
  // during oilpan GC, but ClearWrappers potentially runs user script.
  ClearWrappers();
}

ScriptPromise ScriptPromisePropertyBase::Promise(DOMWrapperWorld& world) {
  if (!GetExecutionContext())
    return ScriptPromise();

  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = ToV8Context(GetExecutionContext(), world);
  if (context.IsEmpty())
    return ScriptPromise();
  ScriptState* script_state = ScriptState::From(context);
  ScriptState::Scope scope(script_state);

  v8::Local<v8::Object> wrapper = EnsureHolderWrapper(script_state);
  DCHECK(wrapper->CreationContext() == context);

  v8::Local<v8::Value> cached_promise;
  if (PromiseSymbol().GetOrUndefined(wrapper).ToLocal(&cached_promise) &&
      cached_promise->IsPromise()) {
    return ScriptPromise(script_state, cached_promise);
  }

  // Create and cache the Promise
  v8::Local<v8::Promise::Resolver> resolver;
  if (!v8::Promise::Resolver::New(context).ToLocal(&resolver))
    return ScriptPromise();
  v8::Local<v8::Promise> promise = resolver->GetPromise();
  PromiseSymbol().Set(wrapper, promise);

  switch (state_) {
    case kPending:
      // Cache the resolver too
      ResolverSymbol().Set(wrapper, resolver);
      break;
    case kResolved:
    case kRejected:
      ResolveOrRejectInternal(resolver);
      break;
  }

  return ScriptPromise(script_state, promise);
}

void ScriptPromisePropertyBase::ResolveOrReject(State target_state) {
  DCHECK(GetExecutionContext());
  DCHECK_EQ(state_, kPending);
  DCHECK(target_state == kResolved || target_state == kRejected);

  state_ = target_state;

  v8::HandleScope handle_scope(isolate_);
  wtf_size_t i = 0;
  while (i < wrappers_.size()) {
    const std::unique_ptr<ScopedPersistent<v8::Object>>& persistent =
        wrappers_[i];
    if (persistent->IsEmpty()) {
      // wrapper has died.
      // Since v8 GC can run during the iteration and clear the reference,
      // we can't move this check out of the loop.
      wrappers_.EraseAt(i);
      continue;
    }
    v8::Local<v8::Object> wrapper = persistent->NewLocal(isolate_);
    ScriptState* script_state = ScriptState::From(wrapper->CreationContext());
    ScriptState::Scope scope(script_state);

    V8PrivateProperty::Symbol symbol = ResolverSymbol();
    DCHECK(symbol.HasValue(wrapper));
    v8::Local<v8::Value> resolver_value;
    if (!symbol.GetOrUndefined(wrapper).ToLocal(&resolver_value))
      return;
    symbol.DeleteProperty(wrapper);
    ResolveOrRejectInternal(
        v8::Local<v8::Promise::Resolver>::Cast(resolver_value));
    ++i;
  }
}

void ScriptPromisePropertyBase::ResetBase() {
  CheckThis();
  ClearWrappers();
  state_ = kPending;
}

void ScriptPromisePropertyBase::ResolveOrRejectInternal(
    v8::Local<v8::Promise::Resolver> resolver) {
  v8::Local<v8::Context> context = resolver->CreationContext();
  switch (state_) {
    case kPending:
      NOTREACHED();
      break;
    case kResolved:
      resolver->Resolve(context, ResolvedValue(isolate_, context->Global()))
          .ToChecked();
      break;
    case kRejected:
      resolver->Reject(context, RejectedValue(isolate_, context->Global()))
          .ToChecked();
      break;
  }
}

v8::Local<v8::Object> ScriptPromisePropertyBase::EnsureHolderWrapper(
    ScriptState* script_state) {
  v8::Local<v8::Context> context = script_state->GetContext();
  wtf_size_t i = 0;
  while (i < wrappers_.size()) {
    const std::unique_ptr<ScopedPersistent<v8::Object>>& persistent =
        wrappers_[i];
    if (persistent->IsEmpty()) {
      // wrapper has died.
      // Since v8 GC can run during the iteration and clear the reference,
      // we can't move this check out of the loop.
      wrappers_.EraseAt(i);
      continue;
    }

    v8::Local<v8::Object> wrapper = persistent->NewLocal(isolate_);
    if (wrapper->CreationContext() == context)
      return wrapper;
    ++i;
  }
  v8::Local<v8::Object> wrapper = Holder(isolate_, context->Global());
  std::unique_ptr<ScopedPersistent<v8::Object>> weak_persistent =
      std::make_unique<ScopedPersistent<v8::Object>>();
  weak_persistent->Set(isolate_, wrapper);
  weak_persistent->SetPhantom();
  wrappers_.push_back(std::move(weak_persistent));
  DCHECK(wrapper->CreationContext() == context);
  return wrapper;
}

void ScriptPromisePropertyBase::ClearWrappers() {
  CheckThis();
  CheckWrappers();
  v8::HandleScope handle_scope(isolate_);
  for (WeakPersistentSet::iterator i = wrappers_.begin(); i != wrappers_.end();
       ++i) {
    v8::Local<v8::Object> wrapper = (*i)->NewLocal(isolate_);
    if (!wrapper.IsEmpty()) {
      v8::Context::Scope scope(wrapper->CreationContext());
      // TODO(peria): Use deleteProperty() if http://crbug.com/v8/6227 is fixed.

      // Check whether the value has been set or not. Unfortunately, HasValue
      // cannot be used as it triggers regular ScriptForbiddenScope through V8
      // callbacks. GetOrUndefined avoids this because it does not enter a
      // proper scope in V8.
      v8::Local<v8::Value> cache;
      if (ResolverSymbol().GetOrUndefined(wrapper).ToLocal(&cache) &&
          !cache->IsUndefined()) {
        ResolverSymbol().Set(wrapper, v8::Undefined(isolate_));
      }
      if (PromiseSymbol().GetOrUndefined(wrapper).ToLocal(&cache) &&
          !cache->IsUndefined()) {
        PromiseSymbol().Set(wrapper, v8::Undefined(isolate_));
      }
    }
  }
  wrappers_.clear();
}

void ScriptPromisePropertyBase::CheckThis() {
  CHECK(this);
}

void ScriptPromisePropertyBase::CheckWrappers() {
  for (WeakPersistentSet::iterator i = wrappers_.begin(); i != wrappers_.end();
       ++i) {
    CHECK(*i);
  }
}

V8PrivateProperty::Symbol ScriptPromisePropertyBase::PromiseSymbol() {
  switch (name_) {
#define P(Interface, Name)                                                     \
  case Name:                                                                   \
    static const V8PrivateProperty::SymbolKey kPrivateProperty##Name##Promise; \
    return V8PrivateProperty::GetSymbol(isolate_,                              \
                                        kPrivateProperty##Name##Promise);

    SCRIPT_PROMISE_PROPERTIES(P)

#undef P
  }
  NOTREACHED();
  return V8PrivateProperty::GetEmptySymbol();
}

V8PrivateProperty::Symbol ScriptPromisePropertyBase::ResolverSymbol() {
  switch (name_) {
#define P(Interface, Name)                        \
  case Name:                                      \
    static const V8PrivateProperty::SymbolKey     \
        kPrivateProperty##Name##Resolver;         \
    return V8PrivateProperty::GetSymbol(isolate_, \
                                        kPrivateProperty##Name##Resolver);

    SCRIPT_PROMISE_PROPERTIES(P)

#undef P
  }
  NOTREACHED();
  return V8PrivateProperty::GetEmptySymbol();
}

void ScriptPromisePropertyBase::Trace(blink::Visitor* visitor) {
  ContextClient::Trace(visitor);
}

}  // namespace blink
