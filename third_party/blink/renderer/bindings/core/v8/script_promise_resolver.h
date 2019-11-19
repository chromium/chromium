// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_PROMISE_RESOLVER_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_PROMISE_RESOLVER_H_

#include "base/macros.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/scoped_persistent.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/self_keep_alive.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"
#include "v8/include/v8.h"

#if DCHECK_IS_ON()
#include "base/debug/stack_trace.h"
#endif

namespace blink {

// This class wraps v8::Promise::Resolver and provides the following
// functionalities.
//  - A ScriptPromiseResolver retains a ScriptState. A caller
//    can call resolve or reject from outside of a V8 context.
//  - This class is an ContextLifecycleObserver and keeps track of the
//    associated ExecutionContext state. When it is stopped, resolve or reject
//    will be ignored.
//
// There are cases where promises cannot work (e.g., where the thread is being
// terminated). In such cases operations will silently fail.
class CORE_EXPORT ScriptPromiseResolver
    : public GarbageCollected<ScriptPromiseResolver>,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(ScriptPromiseResolver);
  USING_PRE_FINALIZER(ScriptPromiseResolver, Dispose);

 public:
  explicit ScriptPromiseResolver(ScriptState*);
  virtual ~ScriptPromiseResolver();

  void Dispose();

  // Anything that can be passed to toV8 can be passed to this function.
  template <typename T>
  void Resolve(T value) {
    ResolveOrReject(value, kResolving);
  }

  // Anything that can be passed to toV8 can be passed to this function.
  template <typename T>
  void Reject(T value) {
    ResolveOrReject(value, kRejecting);
  }

  void Resolve() { Resolve(ToV8UndefinedGenerator()); }
  void Reject() { Reject(ToV8UndefinedGenerator()); }

  // Reject with a given exception.
  void Reject(ExceptionState&);

  ScriptState* GetScriptState() const { return script_state_; }

  // Note that an empty ScriptPromise will be returned after resolve or
  // reject is called.
  ScriptPromise Promise() {
#if DCHECK_IS_ON()
    is_promise_called_ = true;
#endif
    return resolver_.Promise();
  }

  // ContextLifecycleObserver implementation.
  void ContextDestroyed(ExecutionContext*) override { Detach(); }

  // Calling this function makes the resolver release its internal resources.
  // That means the associated promise will never be resolved or rejected
  // unless it's already been resolved or rejected.
  // Do not call this function unless you truly need the behavior.
  void Detach();

  // Once this function is called this resolver stays alive while the
  // promise is pending and the associated ExecutionContext isn't stopped.
  void KeepAliveWhilePending();

  void Trace(blink::Visitor*) override;

 private:
  typedef ScriptPromise::InternalResolver Resolver;
  enum ResolutionState {
    kPending,
    kResolving,
    kRejecting,
    kDetached,
  };

  template <typename T>
  void ResolveOrReject(T value, ResolutionState new_state) {
    if (state_ != kPending || !GetScriptState()->ContextIsValid() ||
        !GetExecutionContext() || GetExecutionContext()->IsContextDestroyed())
      return;
    DCHECK(new_state == kResolving || new_state == kRejecting);
    state_ = new_state;

    ScriptState::Scope scope(script_state_);

    // Calling ToV8 in a ScriptForbiddenScope will trigger a CHECK and
    // cause a crash. ToV8 just invokes a constructor for wrapper creation,
    // which is safe (no author script can be run). Adding AllowUserAgentScript
    // directly inside createWrapper could cause a perf impact (calling
    // isMainThread() every time a wrapper is created is expensive). Ideally,
    // resolveOrReject shouldn't be called inside a ScriptForbiddenScope.
    {
      ScriptForbiddenScope::AllowUserAgentScript allow_script;
      value_.Set(script_state_->GetIsolate(),
                 ToV8(value, script_state_->GetContext()->Global(),
                      script_state_->GetIsolate()));
    }

    if (GetExecutionContext()->IsContextPaused()) {
      ScheduleResolveOrReject();
      return;
    }
    // TODO(esprehn): This is a hack, instead we should CHECK that
    // script is allowed, and v8 should be running the entry hooks below and
    // crashing if script is forbidden. We should then audit all users of
    // ScriptPromiseResolver and the related specs and switch to an async
    // resolve.
    // See: http://crbug.com/663476
    if (ScriptForbiddenScope::IsScriptForbidden()) {
      ScheduleResolveOrReject();
      return;
    }
    ResolveOrRejectImmediately();
  }

  void ResolveOrRejectImmediately();
  void ScheduleResolveOrReject();
  void ResolveOrRejectDeferred();

  ResolutionState state_;
  const Member<ScriptState> script_state_;
  TaskHandle deferred_resolve_task_;
  Resolver resolver_;
  TraceWrapperV8Reference<v8::Value> value_;

  // To support keepAliveWhilePending(), this object needs to keep itself
  // alive while in that state.
  SelfKeepAlive<ScriptPromiseResolver> keep_alive_;

#if DCHECK_IS_ON()
  // True if promise() is called.
  bool is_promise_called_ = false;

  base::debug::StackTrace create_stack_trace_{8};
#endif

  DISALLOW_COPY_AND_ASSIGN(ScriptPromiseResolver);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_PROMISE_RESOLVER_H_
