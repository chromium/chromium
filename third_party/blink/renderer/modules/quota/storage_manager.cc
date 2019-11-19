// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/quota/storage_manager.h"

#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_storage_estimate.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/modules/quota/quota_utils.h"
#include "third_party/blink/renderer/modules/quota/storage_estimate.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

using mojom::blink::PermissionName;
using mojom::blink::PermissionService;
using mojom::blink::PermissionStatus;
using mojom::blink::UsageBreakdownPtr;

namespace {

const char kUniqueOriginErrorMessage[] =
    "The operation is not supported in this context.";

void QueryStorageUsageAndQuotaCallback(ScriptPromiseResolver* resolver,
                                       mojom::QuotaStatusCode status_code,
                                       int64_t usage_in_bytes,
                                       int64_t quota_in_bytes,
                                       UsageBreakdownPtr usage_breakdown) {
  // Avoid crash on shutdown. crbug.com/971594
  if (!resolver)
    return;
  if (status_code != mojom::QuotaStatusCode::kOk) {
    // TODO(sashab): Replace this with a switch statement, and remove the enum
    // values from QuotaStatusCode.
    resolver->Reject(MakeGarbageCollected<DOMException>(
        static_cast<DOMExceptionCode>(status_code)));
    return;
  }

  StorageEstimate* estimate = StorageEstimate::Create();
  estimate->setUsage(usage_in_bytes);
  estimate->setQuota(quota_in_bytes);

  // We only want to show usage details for systems that are used by the app,
  // this way we do not create any web compatibility issues by unecessarily
  // exposing obsoleted/proprietary storage systems, but also report when
  // those systems are in use.
  StorageUsageDetails* details = StorageUsageDetails::Create();
  if (usage_breakdown->appcache) {
    details->setApplicationCache(usage_breakdown->appcache);
  }
  if (usage_breakdown->indexedDatabase) {
    details->setIndexedDB(usage_breakdown->indexedDatabase);
  }
  if (usage_breakdown->serviceWorkerCache) {
    details->setCaches(usage_breakdown->serviceWorkerCache);
  }
  if (usage_breakdown->serviceWorker) {
    details->setServiceWorkerRegistrations(usage_breakdown->serviceWorker);
  }
  if (usage_breakdown->fileSystem) {
    details->setFileSystem(usage_breakdown->fileSystem);
  }

  estimate->setUsageDetails(details);

  resolver->Resolve(estimate);
}

}  // namespace

ScriptPromise StorageManager::persist(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  DCHECK(execution_context->IsSecureContext());  // [SecureContext] in IDL
  const SecurityOrigin* security_origin =
      execution_context->GetSecurityOrigin();
  if (security_origin->IsOpaque()) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(), kUniqueOriginErrorMessage));
    return promise;
  }

  Document* doc = To<Document>(execution_context);
  GetPermissionService(ExecutionContext::From(script_state))
      ->RequestPermission(
          CreatePermissionDescriptor(PermissionName::DURABLE_STORAGE),
          LocalFrame::HasTransientUserActivation(doc->GetFrame()),
          WTF::Bind(&StorageManager::PermissionRequestComplete,
                    WrapPersistent(this), WrapPersistent(resolver)));

  return promise;
}

ScriptPromise StorageManager::persisted(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  DCHECK(execution_context->IsSecureContext());  // [SecureContext] in IDL
  const SecurityOrigin* security_origin =
      execution_context->GetSecurityOrigin();
  if (security_origin->IsOpaque()) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(), kUniqueOriginErrorMessage));
    return promise;
  }

  GetPermissionService(ExecutionContext::From(script_state))
      ->HasPermission(
          CreatePermissionDescriptor(PermissionName::DURABLE_STORAGE),
          WTF::Bind(&StorageManager::PermissionRequestComplete,
                    WrapPersistent(this), WrapPersistent(resolver)));
  return promise;
}

ScriptPromise StorageManager::estimate(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  DCHECK(execution_context->IsSecureContext());  // [SecureContext] in IDL

  // The BlinkIDL definition for estimate() already has a [MeasureAs] attribute,
  // so the kQuotaRead use counter must be explicitly updated.
  UseCounter::Count(execution_context, WebFeature::kQuotaRead);

  const SecurityOrigin* security_origin =
      execution_context->GetSecurityOrigin();
  if (security_origin->IsOpaque()) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(), kUniqueOriginErrorMessage));
    return promise;
  }

  auto callback =
      WTF::Bind(&QueryStorageUsageAndQuotaCallback, WrapPersistent(resolver));
  GetQuotaHost(execution_context)
      ->QueryStorageUsageAndQuota(
          WrapRefCounted(security_origin), mojom::StorageType::kTemporary,
          mojo::WrapCallbackWithDefaultInvokeIfNotRun(
              std::move(callback), mojom::QuotaStatusCode::kErrorAbort, 0, 0,
              nullptr));
  return promise;
}

PermissionService* StorageManager::GetPermissionService(
    ExecutionContext* execution_context) {
  if (!permission_service_) {
    ConnectToPermissionService(
        execution_context,
        permission_service_.BindNewPipeAndPassReceiver(
            execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI)));
    permission_service_.set_disconnect_handler(
        WTF::Bind(&StorageManager::PermissionServiceConnectionError,
                  WrapWeakPersistent(this)));
  }
  return permission_service_.get();
}

void StorageManager::PermissionServiceConnectionError() {
  permission_service_.reset();
}

void StorageManager::PermissionRequestComplete(ScriptPromiseResolver* resolver,
                                               PermissionStatus status) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;
  resolver->Resolve(status == PermissionStatus::GRANTED);
}

mojom::blink::QuotaDispatcherHost* StorageManager::GetQuotaHost(
    ExecutionContext* execution_context) {
  if (!quota_host_) {
    ConnectToQuotaDispatcherHost(
        execution_context,
        quota_host_.BindNewPipeAndPassReceiver(
            execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }
  return quota_host_.get();
}

STATIC_ASSERT_ENUM(mojom::QuotaStatusCode::kErrorNotSupported,
                   DOMExceptionCode::kNotSupportedError);
STATIC_ASSERT_ENUM(mojom::QuotaStatusCode::kErrorInvalidModification,
                   DOMExceptionCode::kInvalidModificationError);
STATIC_ASSERT_ENUM(mojom::QuotaStatusCode::kErrorInvalidAccess,
                   DOMExceptionCode::kInvalidAccessError);
STATIC_ASSERT_ENUM(mojom::QuotaStatusCode::kErrorAbort,
                   DOMExceptionCode::kAbortError);

}  // namespace blink
