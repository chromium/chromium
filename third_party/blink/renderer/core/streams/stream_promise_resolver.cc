// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/stream_promise_resolver.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

StreamPromiseResolver* StreamPromiseResolver::CreateResolved(
    ScriptState* script_state,
    v8::Local<v8::Value> value) {
  auto* promise = MakeGarbageCollected<StreamPromiseResolver>(script_state);
  promise->Resolve(script_state, value);
  return promise;
}

StreamPromiseResolver* StreamPromiseResolver::CreateResolvedWithUndefined(
    ScriptState* script_state) {
  return CreateResolved(script_state,
                        v8::Undefined(script_state->GetIsolate()));
}

StreamPromiseResolver* StreamPromiseResolver::CreateRejected(
    ScriptState* script_state,
    v8::Local<v8::Value> reason) {
  auto* promise = MakeGarbageCollected<StreamPromiseResolver>(script_state);
  promise->Reject(script_state, reason);
  return promise;
}

StreamPromiseResolver::StreamPromiseResolver(ScriptState* script_state) {
  v8::Local<v8::Promise::Resolver> resolver;
  if (v8::Promise::Resolver::New(script_state->GetContext())
          .ToLocal(&resolver)) {
    resolver_.Set(script_state->GetIsolate(), resolver);
  }
}

void StreamPromiseResolver::Resolve(ScriptState* script_state,
                                    v8::Local<v8::Value> value) {
  if (resolver_.IsEmpty()) {
    return;
  }
  if (is_settled_) {
    return;
  }
  is_settled_ = true;
  auto result = resolver_.NewLocal(script_state->GetIsolate())
                    ->Resolve(script_state->GetContext(), value);
  if (result.IsNothing()) {
    DVLOG(3) << "Assuming JS shutdown and ignoring failed Resolve";
  }
}

void StreamPromiseResolver::ResolveWithUndefined(ScriptState* script_state) {
  Resolve(script_state, v8::Undefined(script_state->GetIsolate()));
}

void StreamPromiseResolver::Reject(ScriptState* script_state,
                                   v8::Local<v8::Value> reason) {
  if (resolver_.IsEmpty()) {
    return;
  }
  if (is_settled_) {
    return;
  }
  is_settled_ = true;
  auto result = resolver_.NewLocal(script_state->GetIsolate())
                    ->Reject(script_state->GetContext(), reason);
  if (result.IsNothing()) {
    DVLOG(3) << "Assuming JS shutdown and ignoring failed Reject";
  }
}

ScriptPromise StreamPromiseResolver::GetScriptPromise(
    ScriptState* script_state) const {
  return ScriptPromise(script_state, V8Promise(script_state->GetIsolate()));
}

v8::Local<v8::Promise> StreamPromiseResolver::V8Promise(
    v8::Isolate* isolate) const {
  if (resolver_.IsEmpty()) {
    return v8::Local<v8::Promise>();
  }
  return resolver_.NewLocal(isolate)->GetPromise();
}

void StreamPromiseResolver::MarkAsHandled(v8::Isolate* isolate) {
  v8::Local<v8::Promise> promise = V8Promise(isolate);
  if (promise.IsEmpty()) {
    return;
  }
  promise->MarkAsHandled();
}

v8::Promise::PromiseState StreamPromiseResolver::State(
    v8::Isolate* isolate) const {
  v8::Local<v8::Promise> promise = V8Promise(isolate);
  if (promise.IsEmpty()) {
    return v8::Promise::PromiseState::kPending;
  }
  return promise->State();
}

void StreamPromiseResolver::Trace(Visitor* visitor) const {
  visitor->Trace(resolver_);
}

}  // namespace blink
