// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/locks/lock_manager.h"

#include <algorithm>
#include <utility>

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_lock_granted_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_lock_info.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_lock_manager_snapshot.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/modules/locks/lock.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_receiver.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/scheduling_policy.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

constexpr char kRequestAbortedMessage[] = "The request was aborted.";

LockInfo* ToLockInfo(const mojom::blink::LockInfoPtr& record) {
  LockInfo* info = LockInfo::Create();
  info->setMode(Lock::ModeToString(record->mode));
  info->setName(record->name);
  info->setClientId(record->client_id);
  return info;
}

HeapVector<Member<LockInfo>> ToLockInfos(
    const Vector<mojom::blink::LockInfoPtr>& records) {
  HeapVector<Member<LockInfo>> out;
  out.ReserveInitialCapacity(records.size());
  for (const auto& record : records)
    out.push_back(ToLockInfo(record));
  return out;
}

}  // namespace

class LockManager::LockRequestImpl final
    : public GarbageCollected<LockRequestImpl>,
      public NameClient,
      public mojom::blink::LockRequest {
 public:
  LockRequestImpl(
      V8LockGrantedCallback* callback,
      ScriptPromiseResolver* resolver,
      const String& name,
      mojom::blink::LockMode mode,
      mojo::PendingAssociatedReceiver<mojom::blink::LockRequest> receiver,
      mojo::PendingRemote<mojom::blink::ObservedFeature> lock_lifetime,
      LockManager* manager)
      : callback_(callback),
        resolver_(resolver),
        name_(name),
        mode_(mode),
        receiver_(this, manager->GetExecutionContext()),
        lock_lifetime_(std::move(lock_lifetime)),
        manager_(manager) {
    receiver_.Bind(
        std::move(receiver),
        manager->GetExecutionContext()->GetTaskRunner(TaskType::kWebLocks));
  }

  ~LockRequestImpl() override = default;

  void Trace(Visitor* visitor) const {
    visitor->Trace(resolver_);
    visitor->Trace(manager_);
    visitor->Trace(callback_);
    visitor->Trace(receiver_);
  }

  const char* NameInHeapSnapshot() const override {
    return "LockManager::LockRequestImpl";
  }

  // Called to immediately close the pipe which signals the back-end,
  // unblocking further requests, without waiting for GC finalize the object.
  void Cancel() { receiver_.reset(); }

  void Abort(const String& reason) override {
    // Abort signal after acquisition should be ignored.
    if (!manager_->IsPendingRequest(this))
      return;

    manager_->RemovePendingRequest(this);
    receiver_.reset();

    if (!resolver_->GetScriptState()->ContextIsValid())
      return;

    resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kAbortError, reason));
  }

  void Failed() override {
    auto* callback = callback_.Release();

    manager_->RemovePendingRequest(this);
    receiver_.reset();

    ScriptState* script_state = resolver_->GetScriptState();
    if (!script_state->ContextIsValid())
      return;

    // Lock was not granted e.g. because ifAvailable was specified but
    // the lock was not available.
    ScriptState::Scope scope(script_state);
    v8::TryCatch try_catch(script_state->GetIsolate());
    v8::Maybe<ScriptValue> result = callback->Invoke(nullptr, nullptr);
    if (try_catch.HasCaught()) {
      resolver_->Reject(try_catch.Exception());
    } else if (!result.IsNothing()) {
      resolver_->Resolve(result.FromJust());
    }
  }

  void Granted(mojo::PendingAssociatedRemote<mojom::blink::LockHandle>
                   handle_remote) override {
    DCHECK(receiver_.is_bound());

    auto* callback = callback_.Release();

    manager_->RemovePendingRequest(this);
    receiver_.reset();

    ScriptState* script_state = resolver_->GetScriptState();
    if (!script_state->ContextIsValid()) {
      // If a handle was returned, it will be automatically be released.
      return;
    }

    Lock* lock = MakeGarbageCollected<Lock>(
        script_state, name_, mode_, std::move(handle_remote),
        std::move(lock_lifetime_), manager_);
    manager_->held_locks_.insert(lock);

    ScriptState::Scope scope(script_state);
    v8::TryCatch try_catch(script_state->GetIsolate());
    v8::Maybe<ScriptValue> result = callback->Invoke(nullptr, lock);
    if (try_catch.HasCaught()) {
      lock->HoldUntil(
          ScriptPromise::Reject(script_state, try_catch.Exception()),
          resolver_);
    } else if (!result.IsNothing()) {
      lock->HoldUntil(ScriptPromise::Cast(script_state, result.FromJust()),
                      resolver_);
    }
  }

 private:
  // Callback passed by script; invoked when the lock is granted.
  Member<V8LockGrantedCallback> callback_;

  // Rejects if the request was aborted, otherwise resolves/rejects with
  // |callback_|'s result.
  Member<ScriptPromiseResolver> resolver_;

  // Held to stamp the Lock object's |name| property.
  String name_;

  // Held to stamp the Lock object's |mode| property.
  mojom::blink::LockMode mode_;

  HeapMojoAssociatedReceiver<mojom::blink::LockRequest, LockRequestImpl>
      receiver_;

  // Held to pass into the Lock if granted, to inform the browser that
  // WebLocks are being used by this frame.
  mojo::PendingRemote<mojom::blink::ObservedFeature> lock_lifetime_;

  // The |manager_| keeps |this| alive until a response comes in and this is
  // registered. If the context is destroyed then |manager_| will dispose of
  // |this| which terminates the request on the service side.
  Member<LockManager> manager_;

  DISALLOW_COPY_AND_ASSIGN(LockRequestImpl);
};

LockManager::LockManager(ExecutionContext* context)
    : ExecutionContextLifecycleObserver(context),
      service_(context),
      observer_(context) {}

ScriptPromise LockManager::request(ScriptState* script_state,
                                   const String& name,
                                   V8LockGrantedCallback* callback,
                                   ExceptionState& exception_state) {
  return request(script_state, name, LockOptions::Create(), callback,
                 exception_state);
}

ScriptPromise LockManager::request(ScriptState* script_state,
                                   const String& name,
                                   const LockOptions* options,
                                   V8LockGrantedCallback* callback,
                                   ExceptionState& exception_state) {
  // Observed context may be gone if frame is detached.
  if (!GetExecutionContext())
    return ScriptPromise();

  ExecutionContext* context = ExecutionContext::From(script_state);
  DCHECK(context->IsContextThread());

  context->GetScheduler()->RegisterStickyFeature(
      blink::SchedulingPolicy::Feature::kWebLocks,
      {blink::SchedulingPolicy::RecordMetricsForBackForwardCache()});

  // 5. If origin is an opaque origin, then reject promise with a
  // "SecurityError" DOMException.
  if (!context->GetSecurityOrigin()->CanAccessLocks() ||
      !AllowLocks(script_state)) {
    exception_state.ThrowSecurityError(
        "Access to the Locks API is denied in this context.");
    return ScriptPromise();
  }
  if (context->GetSecurityOrigin()->IsLocal()) {
    UseCounter::Count(context, WebFeature::kFileAccessedLocks);
  }

  if (!service_.is_bound()) {
    context->GetBrowserInterfaceBroker().GetInterface(
        service_.BindNewPipeAndPassReceiver(
            context->GetTaskRunner(TaskType::kMiscPlatformAPI)));

    if (!service_.is_bound()) {
      exception_state.ThrowTypeError("Service not available.");
      return ScriptPromise();
    }
  }
  if (!observer_.is_bound()) {
    context->GetBrowserInterfaceBroker().GetInterface(
        observer_.BindNewPipeAndPassReceiver(
            context->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }

  mojom::blink::LockMode mode = Lock::StringToMode(options->mode());

  // 6. Otherwise, if name starts with U+002D HYPHEN-MINUS (-), then reject
  // promise with a "NotSupportedError" DOMException.
  if (name.StartsWith("-")) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Names cannot start with '-'.");
    return ScriptPromise();
  }

  // 7. Otherwise, if both options’ steal dictionary member and option’s
  // ifAvailable dictionary member are true, then reject promise with a
  // "NotSupportedError" DOMException.
  if (options->steal() && options->ifAvailable()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The 'steal' and 'ifAvailable' options cannot be used together.");
    return ScriptPromise();
  }

  // 8. Otherwise, if options’ steal dictionary member is true and option’s mode
  // dictionary member is not "exclusive", then reject promise with a
  // "NotSupportedError" DOMException.
  if (options->steal() && mode != mojom::blink::LockMode::EXCLUSIVE) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The 'steal' option may only be used with 'exclusive' locks.");
    return ScriptPromise();
  }

  // 9. Otherwise, if option’s signal dictionary member is present, and either
  // of options’ steal dictionary member or options’ ifAvailable dictionary
  // member is true, then reject promise with a "NotSupportedError"
  // DOMException.
  if (options->hasSignal() && options->ifAvailable()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The 'signal' and 'ifAvailable' options cannot be used together.");
    return ScriptPromise();
  }
  if (options->hasSignal() && options->steal()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The 'signal' and 'steal' options cannot be used together.");
    return ScriptPromise();
  }

  // 10. Otherwise, if options’ signal dictionary member is present and its
  // aborted flag is set, then reject promise with an "AbortError" DOMException.
  if (options->hasSignal() && options->signal()->aborted()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kAbortError,
                                      kRequestAbortedMessage);
    return ScriptPromise();
  }

  mojom::blink::LockManager::WaitMode wait =
      options->steal() ? mojom::blink::LockManager::WaitMode::PREEMPT
                       : options->ifAvailable()
                             ? mojom::blink::LockManager::WaitMode::NO_WAIT
                             : mojom::blink::LockManager::WaitMode::WAIT;

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  mojo::PendingRemote<mojom::blink::ObservedFeature> lock_lifetime;
  observer_->Register(lock_lifetime.InitWithNewPipeAndPassReceiver(),
                      mojom::blink::ObservedFeatureType::kWebLock);

  mojo::PendingAssociatedRemote<mojom::blink::LockRequest> request_remote;
  // 11.1. Let request be the result of running the steps to request a lock with
  // promise, the current agent, environment’s id, origin, callback, name,
  // options’ mode dictionary member, options’ ifAvailable dictionary member,
  // and options’ steal dictionary member.
  LockRequestImpl* request = MakeGarbageCollected<LockRequestImpl>(
      callback, resolver, name, mode,
      request_remote.InitWithNewEndpointAndPassReceiver(),
      std::move(lock_lifetime), this);
  AddPendingRequest(request);

  // 11.2. If options’ signal dictionary member is present, then add the
  // following abort steps to options’ signal dictionary member:
  if (options->hasSignal()) {
    // 11.2.1. Enqueue the steps to abort the request request to the lock task
    // queue.
    // 11.2.2. Reject promise with an "AbortError" DOMException.
    options->signal()->AddAlgorithm(WTF::Bind(&LockRequestImpl::Abort,
                                              WrapWeakPersistent(request),
                                              String(kRequestAbortedMessage)));
  }

  service_->RequestLock(name, mode, wait, std::move(request_remote));

  // 12. Return promise.
  return promise;
}

ScriptPromise LockManager::query(ScriptState* script_state,
                                 ExceptionState& exception_state) {
  // Observed context may be gone if frame is detached.
  if (!GetExecutionContext())
    return ScriptPromise();

  ExecutionContext* context = ExecutionContext::From(script_state);
  DCHECK(context->IsContextThread());

  if (!context->GetSecurityOrigin()->CanAccessLocks() ||
      !AllowLocks(script_state)) {
    exception_state.ThrowSecurityError(
        "Access to the Locks API is denied in this context.");
    return ScriptPromise();
  }
  if (context->GetSecurityOrigin()->IsLocal()) {
    UseCounter::Count(context, WebFeature::kFileAccessedLocks);
  }

  if (!service_.is_bound()) {
    context->GetBrowserInterfaceBroker().GetInterface(
        service_.BindNewPipeAndPassReceiver(
            context->GetTaskRunner(TaskType::kMiscPlatformAPI)));

    if (!service_.is_bound()) {
      exception_state.ThrowTypeError("Service not available.");
      return ScriptPromise();
    }
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  service_->QueryState(WTF::Bind(
      [](ScriptPromiseResolver* resolver,
         Vector<mojom::blink::LockInfoPtr> pending,
         Vector<mojom::blink::LockInfoPtr> held) {
        LockManagerSnapshot* snapshot = LockManagerSnapshot::Create();
        snapshot->setPending(ToLockInfos(pending));
        snapshot->setHeld(ToLockInfos(held));
        resolver->Resolve(snapshot);
      },
      WrapPersistent(resolver)));

  return promise;
}

void LockManager::AddPendingRequest(LockRequestImpl* request) {
  pending_requests_.insert(request);
}

void LockManager::RemovePendingRequest(LockRequestImpl* request) {
  pending_requests_.erase(request);
}

bool LockManager::IsPendingRequest(LockRequestImpl* request) {
  return pending_requests_.Contains(request);
}

void LockManager::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  visitor->Trace(pending_requests_);
  visitor->Trace(held_locks_);
  visitor->Trace(service_);
  visitor->Trace(observer_);
}

void LockManager::ContextDestroyed() {
  for (auto request : pending_requests_)
    request->Cancel();
  pending_requests_.clear();
  held_locks_.clear();
}

void LockManager::OnLockReleased(Lock* lock) {
  // Lock may be removed by an explicit call and/or when the context is
  // destroyed, so this must be idempotent.
  held_locks_.erase(lock);
}

bool LockManager::AllowLocks(ScriptState* script_state) {
  if (!cached_allowed_.has_value()) {
    ExecutionContext* execution_context = ExecutionContext::From(script_state);
    DCHECK(execution_context->IsContextThread());
    SECURITY_DCHECK(execution_context->IsWindow() ||
                    execution_context->IsWorkerGlobalScope());
    if (auto* window = DynamicTo<LocalDOMWindow>(execution_context)) {
      LocalFrame* frame = window->GetFrame();
      if (!frame) {
        cached_allowed_ = false;
      } else if (auto* settings_client = frame->GetContentSettingsClient()) {
        // This triggers a sync IPC.
        cached_allowed_ = settings_client->AllowStorageAccessSync(
            WebContentSettingsClient::StorageType::kWebLocks);
      } else {
        cached_allowed_ = true;
      }
    } else {
      WebContentSettingsClient* content_settings_client =
          To<WorkerGlobalScope>(execution_context)->ContentSettingsClient();
      if (!content_settings_client) {
        cached_allowed_ = true;
      } else {
        // This triggers a sync IPC.
        cached_allowed_ = content_settings_client->AllowStorageAccessSync(
            WebContentSettingsClient::StorageType::kWebLocks);
      }
    }
  }
  return cached_allowed_.value();
}

}  // namespace blink
