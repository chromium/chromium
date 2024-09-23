// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/quota/storage_manager.h"

#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_storage_estimate.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_storage_usage_details.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/modules/quota/quota_utils.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

using mojom::blink::PermissionName;
using mojom::blink::PermissionService;
using mojom::blink::UsageBreakdownPtr;

namespace {

const char kUniqueOriginErrorMessage[] =
    "The operation is not supported in this context.";
const char kGenericErrorMessage[] =
    "Internal error when calculating storage usage.";
const char kAbortErrorMessage[] = "The operation was aborted due to shutdown.";

void QueryStorageUsageAndQuotaCallback(
    ScriptPromiseResolver<StorageEstimate>* resolver,
    mojom::blink::QuotaStatusCode status_code,
    int64_t usage_in_bytes,
    int64_t quota_in_bytes,
    UsageBreakdownPtr usage_breakdown) {
  const char* error_message = nullptr;
  switch (status_code) {
    case mojom::blink::QuotaStatusCode::kOk:
      break;
    case mojom::blink::QuotaStatusCode::kErrorNotSupported:
    case mojom::blink::QuotaStatusCode::kErrorInvalidModification:
    case mojom::blink::QuotaStatusCode::kErrorInvalidAccess:
      NOTREACHED_IN_MIGRATION();
      error_message = kGenericErrorMessage;
      break;
    case mojom::blink::QuotaStatusCode::kUnknown:
      error_message = kGenericErrorMessage;
      break;
    case mojom::blink::QuotaStatusCode::kErrorAbort:
      error_message = kAbortErrorMessage;
      break;
  }
  if (error_message) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        resolver->GetScriptState()->GetIsolate(), error_message));
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

StorageManager::StorageManager(ExecutionContext* execution_context)
    : ExecutionContextClient(execution_context),
      permission_service_(execution_context),
      quota_host_(execution_context),
      change_listener_receiver_(this, execution_context) {}

StorageManager::~StorageManager() = default;

ScriptPromise<IDLBoolean> StorageManager::persist(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  DCHECK(window->IsSecureContext());  // [SecureContext] in IDL
  if (window->GetSecurityOrigin()->IsOpaque()) {
    exception_state.ThrowTypeError(kUniqueOriginErrorMessage);
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLBoolean>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  GetPermissionService(window)->RequestPermission(
      CreatePermissionDescriptor(PermissionName::DURABLE_STORAGE),
      LocalFrame::HasTransientUserActivation(window->GetFrame()),
      WTF::BindOnce(&StorageManager::PermissionRequestComplete,
                    WrapPersistent(this), WrapPersistent(resolver)));

  return promise;
}

ScriptPromise<IDLBoolean> StorageManager::persisted(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  DCHECK(execution_context->IsSecureContext());  // [SecureContext] in IDL
  const SecurityOrigin* security_origin =
      execution_context->GetSecurityOrigin();
  if (security_origin->IsOpaque()) {
    exception_state.ThrowTypeError(kUniqueOriginErrorMessage);
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLBoolean>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  GetPermissionService(ExecutionContext::From(script_state))
      ->HasPermission(
          CreatePermissionDescriptor(PermissionName::DURABLE_STORAGE),
          WTF::BindOnce(&StorageManager::PermissionRequestComplete,
                        WrapPersistent(this), WrapPersistent(resolver)));
  return promise;
}

ScriptPromise<StorageEstimate> StorageManager::estimate(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  DCHECK(execution_context->IsSecureContext());  // [SecureContext] in IDL

  // The BlinkIDL definition for estimate() already has a [MeasureAs] attribute,
  // so the kQuotaRead use counter must be explicitly updated.
  UseCounter::Count(execution_context, WebFeature::kQuotaRead);

  const SecurityOrigin* security_origin =
      execution_context->GetSecurityOrigin();
  if (security_origin->IsOpaque()) {
    exception_state.ThrowTypeError(kUniqueOriginErrorMessage);
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<StorageEstimate>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  auto callback = resolver->WrapCallbackInScriptScope(
      WTF::BindOnce(&QueryStorageUsageAndQuotaCallback));
  GetQuotaHost(execution_context)
      ->QueryStorageUsageAndQuota(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          std::move(callback), mojom::blink::QuotaStatusCode::kErrorAbort, 0, 0,
          nullptr));
  return promise;
}

void StorageManager::Trace(Visitor* visitor) const {
  visitor->Trace(change_listener_receiver_);
  visitor->Trace(permission_service_);
  visitor->Trace(quota_host_);
  EventTarget::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

const AtomicString& StorageManager::InterfaceName() const {
  return event_type_names::kQuotachange;
}

ExecutionContext* StorageManager::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

void StorageManager::OnQuotaChange() {
  DispatchEvent(*Event::Create(event_type_names::kQuotachange));
}

void StorageManager::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  if (!quota_host_.is_bound()) {
    ExecutionContext* execution_context = GetExecutionContext();
    if (!execution_context)
      return;

    // This method will bind quota_host_.
    GetQuotaHost(execution_context);
  }
  EventTarget::AddedEventListener(event_type, registered_listener);
  StartObserving();
}

void StorageManager::RemovedEventListener(
    const AtomicString& event_type,
    const RegisteredEventListener& registered_listener) {
  EventTarget::RemovedEventListener(event_type, registered_listener);
  if (!HasEventListeners())
    StopObserving();
}

PermissionService* StorageManager::GetPermissionService(
    ExecutionContext* execution_context) {
  if (!permission_service_.is_bound()) {
    ConnectToPermissionService(
        execution_context,
        permission_service_.BindNewPipeAndPassReceiver(
            execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI)));
    permission_service_.set_disconnect_handler(
        WTF::BindOnce(&StorageManager::PermissionServiceConnectionError,
                      WrapWeakPersistent(this)));
  }
  return permission_service_.get();
}

void StorageManager::PermissionServiceConnectionError() {
  permission_service_.reset();
}

void StorageManager::PermissionRequestComplete(
    ScriptPromiseResolver<IDLBoolean>* resolver,
    mojom::blink::PermissionStatus status) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;
  resolver->Resolve(status == mojom::blink::PermissionStatus::GRANTED);
}

void StorageManager::StartObserving() {
  if (change_listener_receiver_.is_bound() || !quota_host_.is_bound())
    return;

  ExecutionContext* execution_context = GetExecutionContext();
  if (!execution_context)
    return;

  // Using kMiscPlatformAPI because the Storage specification does not
  // specify a dedicated task queue yet.
  auto task_runner =
      execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI);
  quota_host_->AddChangeListener(
      change_listener_receiver_.BindNewPipeAndPassRemote(task_runner), {});
}

void StorageManager::StopObserving() {
  if (!change_listener_receiver_.is_bound())
    return;
  change_listener_receiver_.reset();
}

mojom::blink::QuotaManagerHost* StorageManager::GetQuotaHost(
    ExecutionContext* execution_context) {
  if (!quota_host_.is_bound()) {
    ConnectToQuotaManagerHost(
        execution_context,
        quota_host_.BindNewPipeAndPassReceiver(
            execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }
  return quota_host_.get();
}

}  // namespace blink
