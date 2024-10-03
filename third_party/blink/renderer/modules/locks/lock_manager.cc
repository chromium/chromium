// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/locks/lock_manager.h"

#include <algorithm>
#include <utility>

#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
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
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
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

constexpr char kSecurityErrorMessage[] = "The request was denied.";
constexpr char kInvalidStateErrorMessage[] = "The document is not active.";

LockInfo* ToLockInfo(const mojom::blink::LockInfoPtr& record) {
  LockInfo* info = LockInfo::Create();
  info->setMode(Lock::ModeToEnum(record->mode));
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
      ScriptPromiseResolver<IDLAny>* resolver,
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

  LockRequestImpl(const LockRequestImpl&) = delete;
  LockRequestImpl& operator=(const LockRequestImpl&) = delete;

  ~LockRequestImpl() override = default;

  void Trace(Visitor* visitor) const {
    visitor->Trace(resolver_);
    visitor->Trace(manager_);
    visitor->Trace(callback_);
    visitor->Trace(receiver_);
    visitor->Trace(abort_handle_);
  }

  const char* NameInHeapSnapshot() const override {
    return "LockManager::LockRequestImpl";
  }

  // Called to immediately close the pipe which signals the back-end,
  // unblocking further requests, without waiting for GC finalize the object.
  void Cancel() { receiver_.reset(); }

  void InitializeAbortAlgorithm(AbortSignal::AlgorithmHandle& handle) {
    DCHECK(!abort_handle_);
    abort_handle_ = &handle;
  }

  void Abort(AbortSignal* signal) {
    // Abort signal after acquisition should be ignored.
    if (!manager_->IsPendingRequest(this)) {
      return;
    }

    manager_->RemovePendingRequest(this);
    receiver_.reset();
    abort_handle_.Clear();

    DCHECK(resolver_);

    ScriptState* const script_state = resolver_->GetScriptState();

    if (!IsInParallelAlgorithmRunnable(resolver_->GetExecutionContext(),
                                       script_state)) {
      return;
    }

    ScriptState::Scope script_state_scope(script_state);

    resolver_->Reject(signal->reason(script_state));
  }

  void Failed() override {
    auto* callback = callback_.Release();

    manager_->RemovePendingRequest(this);
    receiver_.reset();
    abort_handle_.Clear();

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
    abort_handle_.Clear();

    ScriptState* script_state = resolver_->GetScriptState();
    if (!script_state->ContextIsValid()) {
      // If a handle was returned, it will be automatically be released.
      return;
    }

    Lock* lock = MakeGarbageCollected<Lock>(
        script_state, name_, mode_, std::move(handle_remote),
        std::move(lock_lifetime_), manager_);
    manager_->held_locks_.insert(lock);

    // Note that either invoking `callback` or calling
    // ScriptPromiseUntyped::Cast to convert the resulting value to a Promise
    // can or will execute javascript. This means that the ExecutionContext
    // could be synchronously destroyed, and the `lock` might be released before
    // HoldUntil is called. This is safe, as releasing a lock twice is harmless.
    ScriptState::Scope scope(script_state);
    v8::TryCatch try_catch(script_state->GetIsolate());
    v8::Maybe<ScriptValue> result = callback->Invoke(nullptr, lock);
    if (try_catch.HasCaught()) {
      lock->HoldUntil(
          ScriptPromise<IDLAny>::Reject(script_state, try_catch.Exception()),
          resolver_);
    } else if (!result.IsNothing()) {
      lock->HoldUntil(
          ToResolvedPromise<IDLAny>(script_state, result.FromJust()),
          resolver_);
    }
  }

 private:
  // Callback passed by script; invoked when the lock is granted.
  Member<V8LockGrantedCallback> callback_;

  // Rejects if the request was aborted, otherwise resolves/rejects with
  // |callback_|'s result.
  Member<ScriptPromiseResolver<IDLAny>> resolver_;

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

  // Handle that keeps the associated abort algorithm alive for the duration of
  // the request.
  Member<AbortSignal::AlgorithmHandle> abort_handle_;
};

const char LockManager::kSupplementName[] = "LockManager";

// static
LockManager* LockManager::locks(NavigatorBase& navigator) {
  auto* supplement = Supplement<NavigatorBase>::From<LockManager>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<LockManager>(navigator);
    Supplement<NavigatorBase>::ProvideTo(navigator, supplement);
  }
  return supplement;
}

LockManager::LockManager(NavigatorBase& navigator)
    : Supplement<NavigatorBase>(navigator),
      ExecutionContextLifecycleObserver(navigator.GetExecutionContext()),
      service_(navigator.GetExecutionContext()),
      observer_(navigator.GetExecutionContext()) {}

void LockManager::SetManager(
    mojo::PendingRemote<mojom::blink::LockManager> manager,
    ExecutionContext* execution_context) {
  service_.Bind(std::move(manager),
                execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI));
}

ScriptPromise<IDLAny> LockManager::request(ScriptState* script_state,
                                           const String& name,
                                           V8LockGrantedCallback* callback,
                                           ExceptionState& exception_state) {
  return request(script_state, name, LockOptions::Create(), callback,
                 exception_state);
}

ScriptPromise<IDLAny> LockManager::request(ScriptState* script_state,
                                           const String& name,
                                           const LockOptions* options,
                                           V8LockGrantedCallback* callback,
                                           ExceptionState& exception_state) {
  // Observed context may be gone if frame is detached.
  if (!GetExecutionContext()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInvalidStateErrorMessage);
    return EmptyPromise();
  }

  ExecutionContext* context = ExecutionContext::From(script_state);
  DCHECK(context->IsContextThread());

  context->GetScheduler()->RegisterStickyFeature(
      blink::SchedulingPolicy::Feature::kWebLocks,
      {blink::SchedulingPolicy::DisableBackForwardCache()});

  // 5. If origin is an opaque origin, then reject promise with a
  // "SecurityError" DOMException.
  if (!context->GetSecurityOrigin()->CanAccessLocks()) {
    exception_state.ThrowSecurityError(
        "Access to the Locks API is denied in this context.");
    return EmptyPromise();
  }
  if (context->GetSecurityOrigin()->IsLocal()) {
    UseCounter::Count(context, WebFeature::kFileAccessedLocks);
  }

  mojom::blink::LockMode mode = Lock::EnumToMode(options->mode().AsEnum());

  // 6. Otherwise, if name starts with U+002D HYPHEN-MINUS (-), then reject
  // promise with a "NotSupportedError" DOMException.
  if (name.StartsWith("-")) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Names cannot start with '-'.");
    return EmptyPromise();
  }

  // 7. Otherwise, if both options’ steal dictionary member and option’s
  // ifAvailable dictionary member are true, then reject promise with a
  // "NotSupportedError" DOMException.
  if (options->steal() && options->ifAvailable()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The 'steal' and 'ifAvailable' options cannot be used together.");
    return EmptyPromise();
  }

  // 8. Otherwise, if options’ steal dictionary member is true and option’s mode
  // dictionary member is not "exclusive", then reject promise with a
  // "NotSupportedError" DOMException.
  if (options->steal() && mode != mojom::blink::LockMode::EXCLUSIVE) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The 'steal' option may only be used with 'exclusive' locks.");
    return EmptyPromise();
  }

  // 9. Otherwise, if option’s signal dictionary member is present, and either
  // of options’ steal dictionary member or options’ ifAvailable dictionary
  // member is true, then reject promise with a "NotSupportedError"
  // DOMException.
  if (options->hasSignal() && options->ifAvailable()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The 'signal' and 'ifAvailable' options cannot be used together.");
    return EmptyPromise();
  }
  if (options->hasSignal() && options->steal()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The 'signal' and 'steal' options cannot be used together.");
    return EmptyPromise();
  }

  // If options["signal"] exists and is aborted, then return a promise rejected
  // with options["signal"]'s abort reason.
  if (options->hasSignal() && options->signal()->aborted()) {
    return ScriptPromise<IDLAny>::Reject(
        script_state, options->signal()->reason(script_state));
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  CheckStorageAccessAllowed(
      context, resolver,
      resolver->WrapCallbackInScriptScope(WTF::BindOnce(
          &LockManager::RequestImpl, WrapWeakPersistent(this),
          WrapPersistent(options), name, WrapPersistent(callback), mode)));

  // 12. Return promise.
  return promise;
}

void LockManager::RequestImpl(const LockOptions* options,
                              const String& name,
                              V8LockGrantedCallback* callback,
                              mojom::blink::LockMode mode,
                              ScriptPromiseResolver<IDLAny>* resolver) {
  ExecutionContext* context = resolver->GetExecutionContext();
  if (!service_.is_bound()) {
    context->GetBrowserInterfaceBroker().GetInterface(
        service_.BindNewPipeAndPassReceiver(
            context->GetTaskRunner(TaskType::kMiscPlatformAPI)));

    if (!service_.is_bound()) {
      resolver->RejectWithDOMException(DOMExceptionCode::kAbortError, "");
    }
  }
  if (!observer_.is_bound()) {
    context->GetBrowserInterfaceBroker().GetInterface(
        observer_.BindNewPipeAndPassReceiver(
            context->GetTaskRunner(TaskType::kMiscPlatformAPI)));

    if (!observer_.is_bound()) {
      resolver->RejectWithDOMException(DOMExceptionCode::kAbortError, "");
    }
  }

  mojom::blink::LockManager::WaitMode wait =
      options->steal()         ? mojom::blink::LockManager::WaitMode::PREEMPT
      : options->ifAvailable() ? mojom::blink::LockManager::WaitMode::NO_WAIT
                               : mojom::blink::LockManager::WaitMode::WAIT;

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
    // In "Request a lock": If signal is present, then add the algorithm signal
    // to abort the request request with signal to signal.
    AbortSignal::AlgorithmHandle* handle = options->signal()->AddAlgorithm(
        WTF::BindOnce(&LockRequestImpl::Abort, WrapWeakPersistent(request),
                      WrapPersistent(options->signal())));
    request->InitializeAbortAlgorithm(*handle);
  }
  service_->RequestLock(name, mode, wait, std::move(request_remote));
}

ScriptPromise<LockManagerSnapshot> LockManager::query(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  // Observed context may be gone if frame is detached.
  if (!GetExecutionContext()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInvalidStateErrorMessage);
    return EmptyPromise();
  }
  ExecutionContext* context = ExecutionContext::From(script_state);
  DCHECK(context->IsContextThread());

  if (!context->GetSecurityOrigin()->CanAccessLocks()) {
    exception_state.ThrowSecurityError(
        "Access to the Locks API is denied in this context.");
    return EmptyPromise();
  }
  if (context->GetSecurityOrigin()->IsLocal()) {
    UseCounter::Count(context, WebFeature::kFileAccessedLocks);
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<LockManagerSnapshot>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  CheckStorageAccessAllowed(
      context, resolver,
      resolver->WrapCallbackInScriptScope(
          WTF::BindOnce(&LockManager::QueryImpl, WrapWeakPersistent(this))));
  return promise;
}

void LockManager::QueryImpl(
    ScriptPromiseResolver<LockManagerSnapshot>* resolver) {
  ExecutionContext* context = resolver->GetExecutionContext();
  if (!service_.is_bound()) {
    context->GetBrowserInterfaceBroker().GetInterface(
        service_.BindNewPipeAndPassReceiver(
            context->GetTaskRunner(TaskType::kMiscPlatformAPI)));

    if (!service_.is_bound()) {
      resolver->RejectWithDOMException(DOMExceptionCode::kAbortError, "");
    }
  }

  service_->QueryState(WTF::BindOnce(
      [](ScriptPromiseResolver<LockManagerSnapshot>* resolver,
         Vector<mojom::blink::LockInfoPtr> pending,
         Vector<mojom::blink::LockInfoPtr> held) {
        LockManagerSnapshot* snapshot = LockManagerSnapshot::Create();
        snapshot->setPending(ToLockInfos(pending));
        snapshot->setHeld(ToLockInfos(held));
        resolver->Resolve(snapshot);
      },
      WrapPersistent(resolver)));
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
  Supplement<NavigatorBase>::Trace(visitor);
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

void LockManager::CheckStorageAccessAllowed(
    ExecutionContext* context,
    ScriptPromiseResolverBase* resolver,
    base::OnceCallback<void()> callback) {
  DCHECK(context->IsWindow() || context->IsWorkerGlobalScope());

  auto wrapped_callback = WTF::BindOnce(
      &LockManager::DidCheckStorageAccessAllowed, WrapWeakPersistent(this),
      WrapPersistent(resolver), std::move(callback));

  if (cached_allowed_.has_value()) {
    std::move(wrapped_callback).Run(cached_allowed_.value());
    return;
  }

  if (auto* window = DynamicTo<LocalDOMWindow>(context)) {
    LocalFrame* frame = window->GetFrame();
    if (!frame) {
      std::move(wrapped_callback).Run(false);
      return;
    }
    frame->AllowStorageAccessAndNotify(
        WebContentSettingsClient::StorageType::kWebLocks,
        std::move(wrapped_callback));
  } else {
    WebContentSettingsClient* content_settings_client =
        To<WorkerGlobalScope>(context)->ContentSettingsClient();
    if (!content_settings_client) {
      std::move(wrapped_callback).Run(true);
      return;
    }
    content_settings_client->AllowStorageAccess(
        WebContentSettingsClient::StorageType::kWebLocks,
        std::move(wrapped_callback));
  }
}

void LockManager::DidCheckStorageAccessAllowed(
    ScriptPromiseResolverBase* resolver,
    base::OnceCallback<void()> callback,
    bool allow_access) {
  if (cached_allowed_.has_value()) {
    DCHECK_EQ(cached_allowed_.value(), allow_access);
  } else {
    cached_allowed_ = allow_access;
  }

  ScriptState* script_state = resolver->GetScriptState();

  if (!script_state->ContextIsValid()) {
    return;
  }

  if (cached_allowed_.value()) {
    std::move(callback).Run();
    return;
  }

  ScriptState::Scope scope(script_state);

  resolver->Reject(V8ThrowDOMException::CreateOrDie(
      script_state->GetIsolate(), DOMExceptionCode::kSecurityError,
      kSecurityErrorMessage));
}

}  // namespace blink
