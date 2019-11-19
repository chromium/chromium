// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_STREAM_PROMISE_RESOLVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_STREAM_PROMISE_RESOLVER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "v8/include/v8.h"

namespace blink {

class ScriptPromise;
class ScriptState;
class Visitor;

// A thin wrapper around v8::Promise::Resolver that matches the semantics used
// for promises in the standard. StreamPromiseResolver is used for promises that
// need to be stored somewhere: promises on the stack should generally use
// v8::Local<v8::Promise>, or ScriptPromise if they are to be returned to the
// bindings code.
class CORE_EXPORT StreamPromiseResolver final
    : public GarbageCollected<StreamPromiseResolver> {
 public:
  // Implements "a promise rejected with" from the INFRA standard.
  // https://www.w3.org/2001/tag/doc/promises-guide/#a-promise-rejected-with
  // Prefer PromiseResolve() in miscellaneous_operations.h if the value is to be
  // returned to JavaScript without storing it. This function should not be used
  // when |value| might be a Promise already, as it will not pass it through
  // unchanged at the standard requires.
  static StreamPromiseResolver* CreateResolved(ScriptState*,
                                               v8::Local<v8::Value> value);

  // Implements "a promise resolved with *undefined*". Prefer
  // PromiseResolveWithUndefined() from miscellaneous_operations.h if the value
  // is to be returned to JavaScript without storing it.
  static StreamPromiseResolver* CreateResolvedWithUndefined(ScriptState*);

  // Implements "a promise rejected with" from the INFRA standard.
  // https://www.w3.org/2001/tag/doc/promises-guide/#a-promise-rejected-with
  // Prefer PromiseResolveWithUndefined() from miscellaneous_operations.h if the
  // value is to be returned directly to JavaScript.
  static StreamPromiseResolver* CreateRejected(ScriptState*,
                                               v8::Local<v8::Value> reason);

  // Creates an initialised promise.
  explicit StreamPromiseResolver(ScriptState*);
  ~StreamPromiseResolver();

  // Resolves the promise with |value|. Does nothing if the promise is already
  // settled.
  void Resolve(ScriptState*, v8::Local<v8::Value> value);

  // Resolves the promise with undefined.
  void ResolveWithUndefined(ScriptState*);

  // Rejects the promise with |reason|. Does nothing if the promise is already
  // resolved.
  void Reject(ScriptState*, v8::Local<v8::Value> reason);

  // Returns the promise wrapped in a ScriptPromise.
  ScriptPromise GetScriptPromise(ScriptState* script_state) const;

  // Returns the promise as a v8::Promise.
  v8::Local<v8::Promise> V8Promise(v8::Isolate* isolate) const;

  // Marks the promise is handled, so if it is rejected it won't be considered
  // an unhandled rejection.
  void MarkAsHandled(v8::Isolate*);

  // Returns the state of the promise, one of pending, fulfilled or rejected.
  v8::Promise::PromiseState State(v8::Isolate*) const;

  // Returns true if the the promise is not pending.
  bool IsSettled() const { return is_settled_; }

  void Trace(Visitor*);

 private:
  TraceWrapperV8Reference<v8::Promise::Resolver> resolver_;
  bool is_settled_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_STREAM_PROMISE_RESOLVER_H_
