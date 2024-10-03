// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/locks/lock.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/locks/lock_manager.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

class Lock::ThenFunction final : public ScriptFunction::Callable {
 public:
  enum ResolveType {
    kFulfilled,
    kRejected,
  };

  ThenFunction(Lock* lock, ResolveType type)
      : lock_(lock), resolve_type_(type) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(lock_);
    ScriptFunction::Callable::Trace(visitor);
  }

  ScriptValue Call(ScriptState*, ScriptValue value) override {
    DCHECK(lock_);
    DCHECK(resolve_type_ == kFulfilled || resolve_type_ == kRejected);
    lock_->ReleaseIfHeld();
    if (resolve_type_ == kFulfilled) {
      lock_->resolver_->Resolve(value);
      lock_ = nullptr;
      return value;
    } else {
      lock_->resolver_->Reject(value);
      lock_ = nullptr;
      return ScriptValue();
    }
  }

 private:
  Member<Lock> lock_;
  ResolveType resolve_type_;
};

Lock::Lock(ScriptState* script_state,
           const String& name,
           mojom::blink::LockMode mode,
           mojo::PendingAssociatedRemote<mojom::blink::LockHandle> handle,
           mojo::PendingRemote<mojom::blink::ObservedFeature> lock_lifetime,
           LockManager* manager)
    : ExecutionContextLifecycleObserver(ExecutionContext::From(script_state)),
      name_(name),
      mode_(mode),
      handle_(ExecutionContext::From(script_state)),
      lock_lifetime_(ExecutionContext::From(script_state)),
      manager_(manager) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      ExecutionContext::From(script_state)->GetTaskRunner(TaskType::kWebLocks);
  handle_.Bind(std::move(handle), task_runner);
  lock_lifetime_.Bind(std::move(lock_lifetime), task_runner);
  handle_.set_disconnect_handler(
      WTF::BindOnce(&Lock::OnConnectionError, WrapWeakPersistent(this)));
}

Lock::~Lock() = default;

V8LockMode Lock::mode() const {
  return V8LockMode(ModeToEnum(mode_));
}

void Lock::HoldUntil(ScriptPromise<IDLAny> promise,
                     ScriptPromiseResolver<IDLAny>* resolver) {
  DCHECK(!resolver_);

  // Note that it is possible for the ExecutionContext that this Lock lives in
  // to have already been destroyed by the time this method is called. In that
  // case `handle_` will have been reset, and the lock would have already been
  // released. This is harmless, as nothing in this class uses `handle_` without
  // first making sure it is still bound.

  ScriptState* script_state = resolver->GetScriptState();
  resolver_ = resolver;
  promise.Then(MakeGarbageCollected<ScriptFunction>(
                   script_state, MakeGarbageCollected<ThenFunction>(
                                     this, ThenFunction::kFulfilled)),
               MakeGarbageCollected<ScriptFunction>(
                   script_state, MakeGarbageCollected<ThenFunction>(
                                     this, ThenFunction::kRejected)));
}

// static
mojom::blink::LockMode Lock::EnumToMode(V8LockMode::Enum mode) {
  switch (mode) {
    case V8LockMode::Enum::kShared:
      return mojom::blink::LockMode::SHARED;
    case V8LockMode::Enum::kExclusive:
      return mojom::blink::LockMode::EXCLUSIVE;
  }
  NOTREACHED();
}

// static
V8LockMode::Enum Lock::ModeToEnum(mojom::blink::LockMode mode) {
  switch (mode) {
    case mojom::blink::LockMode::SHARED:
      return V8LockMode::Enum::kShared;
    case mojom::blink::LockMode::EXCLUSIVE:
      return V8LockMode::Enum::kExclusive;
  }
  NOTREACHED();
}

void Lock::ContextDestroyed() {
  // This is kind of redundant, as `handle_` will reset itself as well when the
  // context is destroyed, thereby releasing the lock. Explicitly releasing here
  // as well doesn't hurt though.
  ReleaseIfHeld();
}

void Lock::Trace(Visitor* visitor) const {
  ExecutionContextLifecycleObserver::Trace(visitor);
  ScriptWrappable::Trace(visitor);
  visitor->Trace(resolver_);
  visitor->Trace(handle_);
  visitor->Trace(lock_lifetime_);
  visitor->Trace(manager_);
}

void Lock::ReleaseIfHeld() {
  if (handle_.is_bound()) {
    // Drop the mojo pipe; this releases the lock on the back end.
    handle_.reset();

    lock_lifetime_.reset();

    // Let the lock manager know that this instance can be collected.
    manager_->OnLockReleased(this);
  }
}

void Lock::OnConnectionError() {
  DCHECK(resolver_);

  ReleaseIfHeld();

  ScriptState* const script_state = resolver_->GetScriptState();

  if (!IsInParallelAlgorithmRunnable(resolver_->GetExecutionContext(),
                                     script_state)) {
    return;
  }

  ScriptState::Scope script_state_scope(script_state);

  resolver_->Reject(V8ThrowDOMException::CreateOrDie(
      script_state->GetIsolate(), DOMExceptionCode::kAbortError,
      "Lock broken by another request with the 'steal' option."));
}

}  // namespace blink
