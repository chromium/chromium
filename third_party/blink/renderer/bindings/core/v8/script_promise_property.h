// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_PROMISE_PROPERTY_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_PROMISE_PROPERTY_H_

#include "base/check_op.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"

namespace blink {

class ExecutionContext;

// ScriptPromiseProperty is a helper for implementing a DOM method or
// attribute whose value is a Promise, and the same Promise must be
// returned each time.
//
// Use ScriptPromise if the property is associated with only one world
// (e.g., FetchEvent.preloadResponse). Use ScriptPromiseProperty if the property
// can be accessed from multiple worlds (e.g., ServiceWorkerContainer.ready).
template <typename IDLResolvedType, typename IDLRejectedType>
class ScriptPromiseProperty final
    : public GarbageCollected<
          ScriptPromiseProperty<IDLResolvedType, IDLRejectedType>>,
      public ExecutionContextClient {
 public:
  enum State {
    kPending,
    kResolved,
    kRejected,
  };

  // Creates a ScriptPromiseProperty that will create Promises in
  // the specified ExecutionContext for a property of 'holder'
  // (typically ScriptPromiseProperty should be a member of the
  // property holder).
  ScriptPromiseProperty(ExecutionContext* execution_context)
      : ExecutionContextClient(execution_context) {}

  ScriptPromiseProperty(const ScriptPromiseProperty&) = delete;
  ScriptPromiseProperty& operator=(const ScriptPromiseProperty&) = delete;

  ScriptPromiseTyped<IDLResolvedType> Promise(DOMWrapperWorld& world) {
    if (!GetExecutionContext()) {
      return ScriptPromiseTyped<IDLResolvedType>();
    }

    v8::HandleScope handle_scope(GetExecutionContext()->GetIsolate());
    v8::Local<v8::Context> context = ToV8Context(GetExecutionContext(), world);
    if (context.IsEmpty()) {
      return ScriptPromiseTyped<IDLResolvedType>();
    }
    ScriptState* script_state = ScriptState::From(context);

    for (auto& promise : promises_) {
      if (promise.IsAssociatedWith(script_state)) {
        return static_cast<ScriptPromiseTyped<IDLResolvedType>&>(promise);
      }
    }

    ScriptState::Scope scope(script_state);

    auto* resolver =
        MakeGarbageCollected<ScriptPromiseResolverTyped<IDLResolvedType>>(
            script_state);
    // ScriptPromiseResolver usually requires a caller to reject it before
    // releasing, but ScriptPromiseProperty doesn't have such a requirement, so
    // suppress the check forcibly.
    resolver->SuppressDetachCheck();
    ScriptPromiseTyped<IDLResolvedType> promise = resolver->Promise();
    if (mark_as_handled_)
      promise.MarkAsHandled();
    switch (state_) {
      case kPending:
        resolvers_.push_back(resolver);
        break;
      case kResolved:
        resolver->Resolve(resolved_);
        break;
      case kRejected:
        resolver->template Reject<IDLRejectedType>(rejected_);
        break;
    }
    promises_.push_back(promise);
    return promise;
  }

  template <typename PassResolvedType>
  void Resolve(PassResolvedType value) {
    CHECK(!ScriptForbiddenScope::IsScriptForbidden());
    DCHECK_EQ(GetState(), kPending);
    if (!GetExecutionContext()) {
      return;
    }
    state_ = kResolved;
    resolved_ = value;
    HeapVector<Member<ScriptPromiseResolver>> resolvers;
    resolvers.swap(resolvers_);
    for (const Member<ScriptPromiseResolver>& resolver : resolvers) {
      resolver->DowncastTo<IDLResolvedType>()->Resolve(value);
    }
  }

  void ResolveWithUndefined() { Resolve(ToV8UndefinedGenerator()); }

  template <typename PassRejectedType>
  void Reject(PassRejectedType value) {
    CHECK(!ScriptForbiddenScope::IsScriptForbidden());
    if (RuntimeEnabledFeatures::BlinkLifecycleScriptForbiddenEnabled()) {
      CHECK(!ScriptForbiddenScope::WillBeScriptForbidden());
    } else {
      DCHECK(!ScriptForbiddenScope::WillBeScriptForbidden());
    }
    DCHECK_EQ(GetState(), kPending);
    if (!GetExecutionContext()) {
      return;
    }
    state_ = kRejected;
    rejected_ = value;
    HeapVector<Member<ScriptPromiseResolver>> resolvers;
    resolvers.swap(resolvers_);
    for (const Member<ScriptPromiseResolver>& resolver : resolvers) {
      resolver->Reject<IDLRejectedType>(rejected_);
    }
  }

  // Resets this property by unregistering the Promise property from the
  // holder wrapper. Resets the internal state to Pending and clears the
  // resolved and the rejected values.
  void Reset() {
    state_ = kPending;
    resolved_ = DefaultPromiseResultValue<MemberResolvedType>();
    rejected_ = DefaultPromiseResultValue<MemberRejectedType>();
    resolvers_.clear();
    promises_.clear();
  }

  // Mark generated promises as handled to avoid reporting unhandled rejections.
  void MarkAsHandled() {
    mark_as_handled_ = true;
    for (auto& promise : promises_) {
      promise.MarkAsHandled();
    }
  }

  void Trace(Visitor* visitor) const override {
    TraceIfNeeded<MemberResolvedType>::Trace(visitor, resolved_);
    TraceIfNeeded<MemberRejectedType>::Trace(visitor, rejected_);
    visitor->Trace(resolvers_);
    visitor->Trace(promises_);
    ExecutionContextClient::Trace(visitor);
  }

  State GetState() const { return state_; }

 private:
  using MemberResolvedType =
      AddMemberIfNeeded<typename IDLTypeToBlinkImplType<IDLResolvedType>::type>;
  using MemberRejectedType =
      AddMemberIfNeeded<typename IDLTypeToBlinkImplType<IDLRejectedType>::type>;

  template <typename T>
  static T DefaultPromiseResultValue() {
    return {};
  }

  template <typename T>
    requires std::derived_from<T, bindings::EnumerationBase>
  static T DefaultPromiseResultValue() {
    return T(static_cast<T::Enum>(0));
  }

  State state_ = kPending;
  MemberResolvedType resolved_{DefaultPromiseResultValue<MemberResolvedType>()};
  MemberRejectedType rejected_{DefaultPromiseResultValue<MemberRejectedType>()};

  // These vectors contain ScriptPromiseResolverTyped<IDLResolvedType> and
  // ScriptPromiseTyped<IDLResolvedType>, respectively. We save ~10KB of binary
  // size by storing them as the untemplated base class and downcasting where
  // needed.
  HeapVector<Member<ScriptPromiseResolver>> resolvers_;
  HeapVector<ScriptPromise> promises_;

  bool mark_as_handled_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_PROMISE_PROPERTY_H_
