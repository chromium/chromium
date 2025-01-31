// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/wake_lock/wake_lock.h"

#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
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

namespace blink {

using mojom::blink::PermissionService;

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
                                                V8WakeLockType::Enum::kScreen),
          MakeGarbageCollected<WakeLockManager>(
              navigator.GetExecutionContext(),
              V8WakeLockType::Enum::kSystem)} {}

ScriptPromise<WakeLockSentinel> WakeLock::request(
    ScriptState* script_state,
    V8WakeLockType type,
    ExceptionState& exception_state) {
  // https://w3c.github.io/screen-wake-lock/#the-request-method

  // 4. If the document's browsing context is null, reject promise with a
  //    "NotAllowedError" DOMException and return promise.
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "The document has no associated browsing context");
    return EmptyPromise();
  }

  auto* context = ExecutionContext::From(script_state);
  DCHECK(context->IsWindow() || context->IsDedicatedWorkerGlobalScope());

  if (type == V8WakeLockType::Enum::kSystem &&
      !RuntimeEnabledFeatures::SystemWakeLockEnabled()) {
    exception_state.ThrowTypeError(
        "The provided value 'system' is not a valid enum value of type "
        "WakeLockType.");
    return EmptyPromise();
  }

  // 2. If document is not allowed to use the policy-controlled feature named
  //    "screen-wake-lock", return a promise rejected with a "NotAllowedError"
  //     DOMException.
  // TODO: Check permissions policy enabling for System Wake Lock
  // [N.B. Per https://github.com/w3c/webappsec-permissions-policy/issues/207
  // there is no official support for workers in the Permissions Policy spec,
  // but we can perform FP checks in workers in Blink]
  if (type == V8WakeLockType::Enum::kScreen &&
      !context->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kScreenWakeLock,
          ReportOptions::kReportOnFailure)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
                                      "Access to Screen Wake Lock features is "
                                      "disallowed by permissions policy");
    return EmptyPromise();
  }

  if (context->IsDedicatedWorkerGlobalScope()) {
    // N.B. The following steps were removed from the spec when System Wake Lock
    // was spun off into a separate specification.
    // 3. If the current global object is the DedicatedWorkerGlobalScope object:
    // 3.1. If the current global object's owner set is empty, reject promise
    //      with a "NotAllowedError" DOMException and return promise.
    // 3.2. If type is "screen", reject promise with a "NotAllowedError"
    //      DOMException, and return promise.
    if (type == V8WakeLockType::Enum::kScreen) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotAllowedError,
          "Screen locks cannot be requested from workers");
      return EmptyPromise();
    }
  } else if (auto* window = DynamicTo<LocalDOMWindow>(context)) {
    // 1. Let document be this's relevant settings object's associated
    //    Document.
    // 5. If document is not fully active, return a promise rejected with with a
    //    "NotAllowedError" DOMException.
    if (!window->document()->IsActive()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
                                        "The document is not active");
      return EmptyPromise();
    }
    // 6. If the steps to determine the visibility state return hidden, return a
    //    promise rejected with "NotAllowedError" DOMException.
    if (type == V8WakeLockType::Enum::kScreen &&
        !window->GetFrame()->GetPage()->IsPageVisible()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
                                        "The requesting page is not visible");
      return EmptyPromise();
    }

    // Measure calls without sticky activation as proposed in
    // https://github.com/w3c/screen-wake-lock/pull/351.
    if (type == V8WakeLockType::Enum::kScreen &&
        !window->GetFrame()->HasStickyUserActivation()) {
      UseCounter::Count(
          context,
          WebFeature::kWakeLockAcquireScreenLockWithoutStickyActivation);
    }
  }

  // 7. Let promise be a new promise.
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<WakeLockSentinel>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  switch (type.AsEnum()) {
    case V8WakeLockType::Enum::kScreen:
      UseCounter::Count(context, WebFeature::kWakeLockAcquireScreenLock);
      break;
    case V8WakeLockType::Enum::kSystem:
      UseCounter::Count(context, WebFeature::kWakeLockAcquireSystemLock);
      break;
  }

  // 8. Run the following steps in parallel:
  DoRequest(type.AsEnum(), resolver);

  // 9. Return promise.
  return promise;
}

void WakeLock::DoRequest(V8WakeLockType::Enum type,
                         ScriptPromiseResolver<WakeLockSentinel>* resolver) {
  // https://w3c.github.io/screen-wake-lock/#the-request-method
  // 8.1. Let state be the result of requesting permission to use
  //      "screen-wake-lock".
  mojom::blink::PermissionName permission_name;
  switch (type) {
    case V8WakeLockType::Enum::kScreen:
      permission_name = mojom::blink::PermissionName::SCREEN_WAKE_LOCK;
      break;
    case V8WakeLockType::Enum::kSystem:
      permission_name = mojom::blink::PermissionName::SYSTEM_WAKE_LOCK;
      break;
  }

  auto* window = DynamicTo<LocalDOMWindow>(GetExecutionContext());
  auto* local_frame = window ? window->GetFrame() : nullptr;
  GetPermissionService()->RequestPermission(
      CreatePermissionDescriptor(permission_name),
      LocalFrame::HasTransientUserActivation(local_frame),
      resolver->WrapCallbackInScriptScope(
          WTF::BindOnce(&WakeLock::DidReceivePermissionResponse,
                        WrapPersistent(this), type)));
}

void WakeLock::DidReceivePermissionResponse(
    V8WakeLockType::Enum type,
    ScriptPromiseResolver<WakeLockSentinel>* resolver,
    mojom::blink::PermissionStatus status) {
  // https://w3c.github.io/screen-wake-lock/#the-request-method
  // 8.2. If state is "denied", then:
  // 8.2.1. Queue a global task on the screen wake lock task source given
  //        document's relevant global object to reject promise with a
  //        "NotAllowedError" DOMException.
  // 8.2.2. Abort these steps.
  // Note: Treat ASK permission (default in headless_shell) as DENIED.
  if (status != mojom::blink::PermissionStatus::GRANTED) {
    resolver->Reject(V8ThrowDOMException::CreateOrDie(
        resolver->GetScriptState()->GetIsolate(),
        DOMExceptionCode::kNotAllowedError,
        "Wake Lock permission request denied"));
    return;
  }
  // 8.3. Queue a global task on the screen wake lock task source given
  //      document's relevant global object to run these steps:
  if (type == V8WakeLockType::Enum::kScreen &&
      !(GetPage() && GetPage()->IsPageVisible())) {
    // 8.3.1. If the steps to determine the visibility state return hidden,
    //        then:
    // 8.3.1.1. Reject promise with a "NotAllowedError" DOMException.
    // 8.3.1.2. Abort these steps.
    resolver->Reject(V8ThrowDOMException::CreateOrDie(
        resolver->GetScriptState()->GetIsolate(),
        DOMExceptionCode::kNotAllowedError,
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
      managers_[static_cast<size_t>(V8WakeLockType::Enum::kScreen)];
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
  for (const Member<WakeLockManager>& manager : managers_)
    visitor->Trace(manager);
  visitor->Trace(permission_service_);
  Supplement<NavigatorBase>::Trace(visitor);
  PageVisibilityObserver::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
