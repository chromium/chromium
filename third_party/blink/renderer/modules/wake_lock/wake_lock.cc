// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/wake_lock/wake_lock.h"

#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/navigator_base.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_manager.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_type.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using mojom::blink::PermissionService;
using mojom::blink::PermissionStatus;

// static
const char WakeLock::kSupplementName[] = "WakeLock";

// static
WakeLock* WakeLock::wakeLock(NavigatorBase& navigator) {
  WakeLock* supplement = Supplement<NavigatorBase>::From<WakeLock>(navigator);
  if (!supplement && navigator.GetExecutionContext()) {
    supplement = MakeGarbageCollected<WakeLock>(navigator);
    ProvideTo(navigator, supplement);
  }
  return supplement;
}

WakeLock::WakeLock(NavigatorBase& navigator)
    : Supplement<NavigatorBase>(navigator),
      ExecutionContextLifecycleObserver(navigator.GetExecutionContext()),
      PageVisibilityObserver(navigator.DomWindow()
                                 ? navigator.DomWindow()->GetFrame()->GetPage()
                                 : nullptr),
      permission_service_(navigator.GetExecutionContext()),
      managers_{
          MakeGarbageCollected<WakeLockManager>(navigator.GetExecutionContext(),
                                                WakeLockType::kScreen),
          MakeGarbageCollected<WakeLockManager>(navigator.GetExecutionContext(),
                                                WakeLockType::kSystem)} {}

ScriptPromise WakeLock::request(ScriptState* script_state,
                                const String& type,
                                ExceptionState& exception_state) {
  // https://w3c.github.io/screen-wake-lock/#the-request-method

  // 4. If the document's browsing context is null, reject promise with a
  //    "NotAllowedError" DOMException and return promise.
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "The document has no associated browsing context");
    return ScriptPromise();
  }

  auto* context = ExecutionContext::From(script_state);
  DCHECK(context->IsWindow() || context->IsDedicatedWorkerGlobalScope());

  if (type == "system" && !RuntimeEnabledFeatures::SystemWakeLockEnabled()) {
    exception_state.ThrowTypeError(
        "The provided value 'system' is not a valid enum value of type "
        "WakeLockType.");
    return ScriptPromise();
  }

  // 2. If document is not allowed to use the policy-controlled feature named
  //    "screen-wake-lock", return a promise rejected with a "NotAllowedError"
  //     DOMException.
  // TODO: Check permissions policy enabling for System Wake Lock
  // [N.B. Per https://github.com/w3c/webappsec-permissions-policy/issues/207
  // there is no official support for workers in the Permissions Policy spec,
  // but we can perform FP checks in workers in Blink]
  if (type == "screen" &&
      !context->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kScreenWakeLock,
          ReportOptions::kReportOnFailure)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
                                      "Access to Screen Wake Lock features is "
                                      "disallowed by permissions policy");
    return ScriptPromise();
  }

  if (context->IsDedicatedWorkerGlobalScope()) {
    // N.B. The following steps were removed from the spec when System Wake Lock
    // was spun off into a separate specification.
    // 3. If the current global object is the DedicatedWorkerGlobalScope object:
    // 3.1. If the current global object's owner set is empty, reject promise
    //      with a "NotAllowedError" DOMException and return promise.
    // 3.2. If type is "screen", reject promise with a "NotAllowedError"
    //      DOMException, and return promise.
    if (type == "screen") {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotAllowedError,
          "Screen locks cannot be requested from workers");
      return ScriptPromise();
    }
  } else if (auto* window = DynamicTo<LocalDOMWindow>(context)) {
    // 1. Let document be this's relevant settings object's associated
    //    Document.
    // 5. If document is not fully active, return a promise rejected with with a
    //    "NotAllowedError" DOMException.
    if (!window->document()->IsActive()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
                                        "The document is not active");
      return ScriptPromise();
    }
    // 6. If the steps to determine the visibility state return hidden, return a
    //    promise rejected with "NotAllowedError" DOMException.
    if (type == "screen" && !window->GetFrame()->GetPage()->IsPageVisible()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
                                        "The requesting page is not visible");
      return ScriptPromise();
    }
  }

  // 7. Let promise be a new promise.
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  WakeLockType wake_lock_type = ToWakeLockType(type);

  switch (wake_lock_type) {
    case WakeLockType::kScreen:
      UseCounter::Count(context, WebFeature::kWakeLockAcquireScreenLock);
      break;
    case WakeLockType::kSystem:
      UseCounter::Count(context, WebFeature::kWakeLockAcquireSystemLock);
      break;
    default:
      NOTREACHED();
      break;
  }

  // 8. Run the following steps in parallel:
  DoRequest(wake_lock_type, resolver);

  // 9. Return promise.
  return promise;
}

void WakeLock::DoRequest(WakeLockType type, ScriptPromiseResolver* resolver) {
  // https://w3c.github.io/screen-wake-lock/#the-request-method
  // 8.1. Let state be the result of requesting permission to use
  //      "screen-wake-lock".
  mojom::blink::PermissionName permission_name;
  switch (type) {
    case WakeLockType::kScreen:
      permission_name = mojom::blink::PermissionName::SCREEN_WAKE_LOCK;
      break;
    case WakeLockType::kSystem:
      permission_name = mojom::blink::PermissionName::SYSTEM_WAKE_LOCK;
      break;
  }

  auto* window = DynamicTo<LocalDOMWindow>(GetExecutionContext());
  auto* local_frame = window ? window->GetFrame() : nullptr;
  GetPermissionService()->RequestPermission(
      CreatePermissionDescriptor(permission_name),
      LocalFrame::HasTransientUserActivation(local_frame),
      WTF::Bind(&WakeLock::DidReceivePermissionResponse, WrapPersistent(this),
                type, WrapPersistent(resolver)));
}

void WakeLock::DidReceivePermissionResponse(WakeLockType type,
                                            ScriptPromiseResolver* resolver,
                                            PermissionStatus status) {
  // https://w3c.github.io/screen-wake-lock/#the-request-method
  DCHECK(status == PermissionStatus::GRANTED ||
         status == PermissionStatus::DENIED);
  DCHECK(resolver);
  // Support creating DOMException with JS stack.
  ScriptState* resolver_script_state = resolver->GetScriptState();
  if (!IsInParallelAlgorithmRunnable(resolver->GetExecutionContext(),
                                     resolver_script_state)) {
    return;
  }
  // switch to the resolver's context to let DOMException pick up the resolver's
  // JS stack
  ScriptState::Scope script_state_scope(resolver_script_state);

  // 8.2. If state is "denied", then:
  // 8.2.1. Queue a global task on the screen wake lock task source given
  //        document's relevant global object to reject promise with a
  //        "NotAllowedError" DOMException.
  // 8.2.2. Abort these steps.
  if (status != PermissionStatus::GRANTED) {
    resolver->Reject(V8ThrowDOMException::CreateOrDie(
        resolver_script_state->GetIsolate(), DOMExceptionCode::kNotAllowedError,
        "Wake Lock permission request denied"));
    return;
  }
  // 8.3. Queue a global task on the screen wake lock task source given
  //      document's relevant global object to run these steps:
  if (type == WakeLockType::kScreen &&
      !(GetPage() && GetPage()->IsPageVisible())) {
    // 8.3.1. If the steps to determine the visibility state return hidden,
    //        then:
    // 8.3.1.1. Reject promise with a "NotAllowedError" DOMException.
    // 8.3.1.2. Abort these steps.
    resolver->Reject(V8ThrowDOMException::CreateOrDie(
        resolver_script_state->GetIsolate(), DOMExceptionCode::kNotAllowedError,
        "The requesting page is not visible"));
    return;
  }
  // Steps 8.3.2 to 8.3.5 are described in AcquireWakeLock() and related
  // functions.
  WakeLockManager* manager = managers_[static_cast<size_t>(type)];
  DCHECK(manager);
  manager->AcquireWakeLock(resolver);
}

void WakeLock::ContextDestroyed() {
  // https://w3c.github.io/screen-wake-lock/#handling-document-loss-of-full-activity
  // 1. For each lock in document.[[ActiveLocks]]["screen"]:
  // 1.1. Run release a wake lock with document, lock, and "screen".
  // N.B. The following steps were removed from the spec when System Wake Lock
  // was spun off into a separate specification.
  // 2. For each lock in document.[[ActiveLocks]]["system"]:
  // 2.1. Run release a wake lock with document, lock, and "system".
  for (WakeLockManager* manager : managers_) {
    if (manager)
      manager->ClearWakeLocks();
  }
}

void WakeLock::PageVisibilityChanged() {
  // https://w3c.github.io/screen-wake-lock/#handling-document-loss-of-visibility
  if (GetPage() && GetPage()->IsPageVisible())
    return;
  // 1. For each lock in document.[[ActiveLocks]]["screen"]:
  // 1.1. Run release a wake lock with document, lock, and "screen".
  WakeLockManager* manager =
      managers_[static_cast<size_t>(WakeLockType::kScreen)];
  if (manager)
    manager->ClearWakeLocks();
}

PermissionService* WakeLock::GetPermissionService() {
  if (!permission_service_.is_bound()) {
    ConnectToPermissionService(
        GetExecutionContext(),
        permission_service_.BindNewPipeAndPassReceiver(
            GetExecutionContext()->GetTaskRunner(TaskType::kWakeLock)));
  }
  return permission_service_.get();
}

void WakeLock::Trace(Visitor* visitor) const {
  for (const WakeLockManager* manager : managers_)
    visitor->Trace(manager);
  visitor->Trace(permission_service_);
  Supplement<NavigatorBase>::Trace(visitor);
  PageVisibilityObserver::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
