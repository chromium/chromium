// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/wake_lock/wake_lock.h"

#include "third_party/blink/public/mojom/feature_policy/feature_policy_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_manager.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_type.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using mojom::blink::PermissionService;
using mojom::blink::PermissionStatus;

WakeLock::WakeLock(Document& document)
    : ContextLifecycleObserver(&document),
      PageVisibilityObserver(document.GetPage()),
      managers_{MakeGarbageCollected<WakeLockManager>(&document,
                                                      WakeLockType::kScreen),
                MakeGarbageCollected<WakeLockManager>(&document,
                                                      WakeLockType::kSystem)} {}

WakeLock::WakeLock(DedicatedWorkerGlobalScope& worker_scope)
    : ContextLifecycleObserver(&worker_scope),
      PageVisibilityObserver(nullptr),
      managers_{MakeGarbageCollected<WakeLockManager>(&worker_scope,
                                                      WakeLockType::kScreen),
                MakeGarbageCollected<WakeLockManager>(&worker_scope,
                                                      WakeLockType::kSystem)} {}

ScriptPromise WakeLock::request(ScriptState* script_state, const String& type) {
  // https://w3c.github.io/wake-lock/#the-request-method
  auto* context = ExecutionContext::From(script_state);
  DCHECK(context->IsDocument() || context->IsDedicatedWorkerGlobalScope());

  // 2.1. If document is not allowed to use the policy-controlled feature named
  //      "wake-lock", reject promise with a "NotAllowedError" DOMException and
  //      return promise.
  // [N.B. Per https://github.com/w3c/webappsec-feature-policy/issues/207 there
  // is no official support for workers in the Feature Policy spec, but we can
  // perform FP checks in workers in Blink]
  // 2.2. If the user agent denies the wake lock of this type for document,
  //      reject promise with a "NotAllowedError" DOMException and return
  //      promise.
  if (!context->GetSecurityContext().IsFeatureEnabled(
          mojom::FeaturePolicyFeature::kWakeLock,
          ReportOptions::kReportOnFailure)) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kNotAllowedError,
            "Access to WakeLock features is disallowed by feature policy"));
  }

  if (context->IsDedicatedWorkerGlobalScope()) {
    // 3. If the current global object is the DedicatedWorkerGlobalScope object:
    // 3.1. If the current global object's owner set is empty, reject promise
    //      with a "NotAllowedError" DOMException and return promise.
    // 3.2. If type is "screen", reject promise with a "NotAllowedError"
    //      DOMException, and return promise.
    if (type == "screen") {
      return ScriptPromise::RejectWithDOMException(
          script_state, MakeGarbageCollected<DOMException>(
                            DOMExceptionCode::kNotAllowedError,
                            "Screen locks cannot be requested from workers"));
    }
  } else if (context->IsDocument()) {
    // 2. Let document be the responsible document of the current settings
    // object.
    auto* document = To<Document>(context);

    // 4. Otherwise, if the current global object is the Window object:
    // 4.1. If the document's browsing context is null, reject promise with a
    //      "NotAllowedError" DOMException and return promise.
    if (!document) {
      return ScriptPromise::RejectWithDOMException(
          script_state, MakeGarbageCollected<DOMException>(
                            DOMExceptionCode::kNotAllowedError,
                            "The document has no associated browsing context"));
    }

    // 4.2. If document is not fully active, reject promise with a
    //      "NotAllowedError" DOMException, and return promise.
    if (!document->IsActive()) {
      return ScriptPromise::RejectWithDOMException(
          script_state,
          MakeGarbageCollected<DOMException>(DOMExceptionCode::kNotAllowedError,
                                             "The document is not active"));
    }
    // 4.3. If type is "screen" and the Document of the top-level browsing
    //      context is hidden, reject promise with a "NotAllowedError"
    //      DOMException, and return promise.
    if (type == "screen" &&
        !(document->GetPage() && document->GetPage()->IsPageVisible())) {
      return ScriptPromise::RejectWithDOMException(
          script_state, MakeGarbageCollected<DOMException>(
                            DOMExceptionCode::kNotAllowedError,
                            "The requesting page is not visible"));
    }
  }

  // 1. Let promise be a new promise.
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

  // 5. Run the following steps in parallel, but abort when type is "screen" and
  // document is hidden:
  DoRequest(wake_lock_type, resolver);

  // 7. Return promise.
  return promise;
}

void WakeLock::DoRequest(WakeLockType type, ScriptPromiseResolver* resolver) {
  // https://w3c.github.io/wake-lock/#the-request-method
  // 5.1. Let state be the result of awaiting obtain permission steps with type:
  ObtainPermission(
      type, WTF::Bind(&WakeLock::DidReceivePermissionResponse,
                      WrapPersistent(this), type, WrapPersistent(resolver)));
}

void WakeLock::DidReceivePermissionResponse(WakeLockType type,
                                            ScriptPromiseResolver* resolver,
                                            PermissionStatus status) {
  // https://w3c.github.io/wake-lock/#the-request-method
  DCHECK(status == PermissionStatus::GRANTED ||
         status == PermissionStatus::DENIED);
  DCHECK(resolver);
  // 5.1.1. If state is "denied", then reject promise with a "NotAllowedError"
  //        DOMException, and abort these steps.
  if (status != PermissionStatus::GRANTED) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError,
        "Wake Lock permission request denied"));
    return;
  }
  // 6. If aborted, run these steps:
  // 6.1. Reject promise with a "NotAllowedError" DOMException.
  if (type == WakeLockType::kScreen &&
      !(GetPage() && GetPage()->IsPageVisible())) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError,
        "The requesting page is not visible"));
    return;
  }
  // 5.3. Let success be the result of awaiting acquire a wake lock with lock
  // and type:
  // 5.3.1. If success is false then reject promise with a "NotAllowedError"
  //        DOMException, and abort these steps.
  WakeLockManager* manager = managers_[static_cast<size_t>(type)];
  DCHECK(manager);
  manager->AcquireWakeLock(resolver);
}

void WakeLock::ContextDestroyed(ExecutionContext*) {
  // https://w3c.github.io/wake-lock/#handling-document-loss-of-full-activity
  // 1. Let document be the responsible document of the current settings object.
  // 2. Let screenRecord be the platform wake lock's state record associated
  // with document and wake lock type "screen".
  // 3. For each lock in screenRecord.[[ActiveLocks]]:
  // 3.1. Run release a wake lock with lock and "screen".
  // 4. Let systemRecord be the platform wake lock's state record associated
  // with document and wake lock type "system".
  // 5. For each lock in systemRecord.[[ActiveLocks]]:
  // 5.1. Run release a wake lock with lock and "system".
  for (WakeLockManager* manager : managers_) {
    if (manager)
      manager->ClearWakeLocks();
  }
}

void WakeLock::PageVisibilityChanged() {
  // https://w3c.github.io/wake-lock/#handling-document-loss-of-visibility
  // 1. Let document be the Document of the top-level browsing context.
  // 2. If document's visibility state is "visible", abort these steps.
  if (GetPage() && GetPage()->IsPageVisible())
    return;
  // 3. Let screenRecord be the platform wake lock's state record associated
  // with wake lock type "screen".
  // 4. For each lock in screenRecord.[[ActiveLocks]]:
  // 4.1. Run release a wake lock with lock and "screen".
  WakeLockManager* manager =
      managers_[static_cast<size_t>(WakeLockType::kScreen)];
  if (manager)
    manager->ClearWakeLocks();
}

void WakeLock::ObtainPermission(
    WakeLockType type,
    base::OnceCallback<void(PermissionStatus)> callback) {
  // https://w3c.github.io/wake-lock/#dfn-obtain-permission
  // Note we actually implement a simplified version of the "obtain permission"
  // algorithm that essentially just calls the "request permission to use"
  // algorithm from the Permissions spec (i.e. we bypass all the steps covering
  // calling the "query a permission" algorithm and handling its result).
  // * Right now, we can do that because there is no way for Chromium's
  //   permission system to get to the "prompt" state given how
  //   WakeLockPermissionContext is currently implemented.
  // * Even if WakeLockPermissionContext changes in the future, this Blink
  //   implementation is unlikely to change because
  //   WakeLockPermissionContext::RequestPermission() will take its
  //   |user_gesture| argument into account to actually implement a slightly
  //   altered version of "request permission to use", the behavior of which
  //   will match the definition of "obtain permission" in the Wake Lock spec.
  DCHECK(type == WakeLockType::kScreen || type == WakeLockType::kSystem);
  static_assert(
      static_cast<mojom::blink::WakeLockType>(WakeLockType::kScreen) ==
          mojom::blink::WakeLockType::kScreen,
      "WakeLockType and mojom::blink::WakeLockType must have identical values");
  static_assert(
      static_cast<mojom::blink::WakeLockType>(WakeLockType::kSystem) ==
          mojom::blink::WakeLockType::kSystem,
      "WakeLockType and mojom::blink::WakeLockType must have identical values");

  auto* local_frame = GetExecutionContext()->IsDocument()
                          ? To<Document>(GetExecutionContext())->GetFrame()
                          : nullptr;
  GetPermissionService()->RequestPermission(
      CreateWakeLockPermissionDescriptor(
          static_cast<mojom::blink::WakeLockType>(type)),
      LocalFrame::HasTransientUserActivation(local_frame), std::move(callback));
}

PermissionService* WakeLock::GetPermissionService() {
  if (!permission_service_) {
    ConnectToPermissionService(
        GetExecutionContext(),
        permission_service_.BindNewPipeAndPassReceiver());
  }
  return permission_service_.get();
}

void WakeLock::Trace(Visitor* visitor) {
  for (WakeLockManager* manager : managers_)
    visitor->Trace(manager);
  PageVisibilityObserver::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
