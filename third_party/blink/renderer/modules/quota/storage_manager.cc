// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/quota/storage_manager.h"

#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_storage_estimate.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/modules/quota/quota_utils.h"
#include "third_party/blink/renderer/modules/quota/storage_estimate.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

using mojom::blink::PermissionName;
using mojom::blink::PermissionService;
using mojom::blink::PermissionStatus;

namespace {

const char kUniqueOriginErrorMessage[] =
    "The operation is not supported in this context.";

void QueryStorageUsageAndQuotaCallback(ScriptPromiseResolver* resolver,
                                       mojom::QuotaStatusCode status_code,
                                       int64_t usage_in_bytes,
                                       int64_t quota_in_bytes) {
  if (status_code != mojom::QuotaStatusCode::kOk) {
    // TODO(sashab): Replace this with a switch statement, and remove the enum
    // values from QuotaStatusCode.
    resolver->Reject(
        DOMException::Create(static_cast<DOMExceptionCode>(status_code)));
    return;
  }

  StorageEstimate estimate;
  estimate.setUsage(usage_in_bytes);
  estimate.setQuota(quota_in_bytes);
  resolver->Resolve(estimate);
}

}  // namespace

ScriptPromise StorageManager::persist(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
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
      .RequestPermission(
          CreatePermissionDescriptor(PermissionName::DURABLE_STORAGE),
          LocalFrame::HasTransientUserActivation(doc->GetFrame()),
          WTF::Bind(&StorageManager::PermissionRequestComplete,
                    WrapPersistent(this), WrapPersistent(resolver)));

  return promise;
}

ScriptPromise StorageManager::persisted(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
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
      .HasPermission(
          CreatePermissionDescriptor(PermissionName::DURABLE_STORAGE),
          WTF::Bind(&StorageManager::PermissionRequestComplete,
                    WrapPersistent(this), WrapPersistent(resolver)));
  return promise;
}

ScriptPromise StorageManager::estimate(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
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

  auto callback =
      WTF::Bind(&QueryStorageUsageAndQuotaCallback, WrapPersistent(resolver));
  GetQuotaHost(execution_context)
      .QueryStorageUsageAndQuota(
          WrapRefCounted(security_origin), mojom::StorageType::kTemporary,
          mojo::WrapCallbackWithDefaultInvokeIfNotRun(
              std::move(callback), mojom::QuotaStatusCode::kErrorAbort, 0, 0));
  return promise;
}

PermissionService& StorageManager::GetPermissionService(
    ExecutionContext* execution_context) {
  if (!permission_service_) {
    ConnectToPermissionService(execution_context,
                               mojo::MakeRequest(&permission_service_));
    permission_service_.set_connection_error_handler(
        WTF::Bind(&StorageManager::PermissionServiceConnectionError,
                  WrapWeakPersistent(this)));
  }
  return *permission_service_;
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

mojom::blink::QuotaDispatcherHost& StorageManager::GetQuotaHost(
    ExecutionContext* execution_context) {
  if (!quota_host_) {
    ConnectToQuotaDispatcherHost(execution_context,
                                 mojo::MakeRequest(&quota_host_));
  }
  return *quota_host_;
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
