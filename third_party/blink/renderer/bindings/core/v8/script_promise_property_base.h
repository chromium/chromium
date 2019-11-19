// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_PROMISE_PROPERTY_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_PROMISE_PROPERTY_BASE_H_

#include <memory>

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/scoped_persistent.h"
#include "third_party/blink/renderer/platform/bindings/script_promise_properties.h"
#include "third_party/blink/renderer/platform/bindings/v8_private_property.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

namespace blink {

class DOMWrapperWorld;
class ExecutionContext;
class ScriptState;

// TODO(yhirano): Remove NOINLINE once we find the cause of crashes.
class CORE_EXPORT ScriptPromisePropertyBase
    : public GarbageCollected<ScriptPromisePropertyBase>,
      public ContextClient {
  USING_GARBAGE_COLLECTED_MIXIN(ScriptPromisePropertyBase);

 public:
  virtual ~ScriptPromisePropertyBase();

  enum Name {
#define P(Unused, Name) Name,
    SCRIPT_PROMISE_PROPERTIES(P)
#undef P
  };

  enum State {
    kPending,
    kResolved,
    kRejected,
  };
  State GetState() const { return state_; }

  ScriptPromise Promise(DOMWrapperWorld&);

  void Trace(blink::Visitor*) override;

 protected:
  ScriptPromisePropertyBase(ExecutionContext*, Name);

  void ResolveOrReject(State target_state);

  // ScriptPromiseProperty overrides these to wrap the holder,
  // rejected value and resolved value. The
  // ScriptPromisePropertyBase caller will enter the V8Context for
  // the property's execution context and the world it is
  // creating/settling promises in; the implementation should use
  // this context.
  virtual v8::Local<v8::Object> Holder(
      v8::Isolate*,
      v8::Local<v8::Object> creation_context) = 0;
  virtual v8::Local<v8::Value> ResolvedValue(
      v8::Isolate*,
      v8::Local<v8::Object> creation_context) = 0;
  virtual v8::Local<v8::Value> RejectedValue(
      v8::Isolate*,
      v8::Local<v8::Object> creation_context) = 0;

  NOINLINE void ResetBase();

 private:
  typedef Vector<std::unique_ptr<ScopedPersistent<v8::Object>>>
      WeakPersistentSet;

  void ResolveOrRejectInternal(v8::Local<v8::Promise::Resolver>);
  v8::Local<v8::Object> EnsureHolderWrapper(ScriptState*);
  NOINLINE void ClearWrappers();
  // TODO(yhirano): Remove these functions once we find the cause of crashes.
  NOINLINE void CheckThis();
  NOINLINE void CheckWrappers();

  V8PrivateProperty::Symbol PromiseSymbol();
  V8PrivateProperty::Symbol ResolverSymbol();

  v8::Isolate* isolate_;
  Name name_;
  State state_;

  WeakPersistentSet wrappers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_PROMISE_PROPERTY_BASE_H_
