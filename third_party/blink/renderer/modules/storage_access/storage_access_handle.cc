// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/storage_access/storage_access_handle.h"

#include "base/types/pass_key.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_storage_estimate.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_storage_usage_details.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/workers/shared_worker.h"
#include "third_party/blink/renderer/modules/broadcastchannel/broadcast_channel.h"
#include "third_party/blink/renderer/modules/file_system_access/storage_manager_file_system_access.h"
#include "third_party/blink/renderer/modules/storage_access/global_storage_access_handle.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

using PassKey = base::PassKey<StorageAccessHandle>;

// static
const char StorageAccessHandle::kSupplementName[] = "StorageAccessHandle";

// static
const char StorageAccessHandle::kSessionStorageNotRequested[] =
    "Session storage not requested when storage access handle was initialized.";

// static
const char StorageAccessHandle::kLocalStorageNotRequested[] =
    "Local storage not requested when storage access handle was initialized.";

// static
const char StorageAccessHandle::kIndexedDBNotRequested[] =
    "IndexedDB not requested when storage access handle was initialized.";

// static
const char StorageAccessHandle::kLocksNotRequested[] =
    "Web Locks not requested when storage access handle was initialized.";

// static
const char StorageAccessHandle::kCachesNotRequested[] =
    "Cache Storage not requested when storage access handle was initialized.";

// static
const char StorageAccessHandle::kGetDirectoryNotRequested[] =
    "Origin Private File System not requested when storage access handle was "
    "initialized.";

// static
const char StorageAccessHandle::kEstimateNotRequested[] =
    "The estimate function for Quota was not requested when storage access "
    "handle was initialized.";

// static
const char StorageAccessHandle::kCreateObjectURLNotRequested[] =
    "The createObjectURL function for Blob Stoage was not requested when "
    "storage access handle was initialized.";

// static
const char StorageAccessHandle::kRevokeObjectURLNotRequested[] =
    "The revokeObjectURL function for Blob Stoage was not requested when "
    "storage access handle was initialized.";

// static
const char StorageAccessHandle::kBroadcastChannelNotRequested[] =
    "Broadcast Channel was not requested when storage access handle was "
    "initialized.";

// static
const char StorageAccessHandle::kSharedWorkerNotRequested[] =
    "Shared Worker was not requested when storage access handle was "
    "initialized.";

namespace {

void EstimateImplAfterRemoteEstimate(
    ScriptPromiseResolver<StorageEstimate>* resolver,
    int64_t current_usage,
    int64_t current_quota,
    bool success) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid()) {
    return;
  }
  ScriptState::Scope scope(script_state);

  if (!success) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError,
        "Unknown error occurred while getting estimate."));
    return;
  }

  StorageEstimate* estimate = StorageEstimate::Create();
  estimate->setUsage(current_usage);
  estimate->setQuota(current_quota);
  estimate->setUsageDetails(StorageUsageDetails::Create());
  resolver->Resolve(estimate);
}

}  // namespace

StorageAccessHandle::StorageAccessHandle(
    LocalDOMWindow& window,
    const StorageAccessTypes* storage_access_types)
    : Supplement<LocalDOMWindow>(window),
      storage_access_types_(storage_access_types) {
  window.CountUse(
      WebFeature::kStorageAccessAPI_requestStorageAccess_BeyondCookies);
  if (storage_access_types_->all()) {
    window.CountUse(
        WebFeature::kStorageAccessAPI_requestStorageAccess_BeyondCookies_all);
  }
  if (storage_access_types_->cookies()) {
    window.CountUse(
        WebFeature::
            kStorageAccessAPI_requestStorageAccess_BeyondCookies_cookies);
  }
  if (storage_access_types_->sessionStorage()) {
    window.CountUse(
        WebFeature::
            kStorageAccessAPI_requestStorageAccess_BeyondCookies_sessionStorage);
  }
  if (storage_access_types_->localStorage()) {
    window.CountUse(
        WebFeature::
            kStorageAccessAPI_requestStorageAccess_BeyondCookies_localStorage);
  }
  if (storage_access_types_->indexedDB()) {
    window.CountUse(
        WebFeature::
            kStorageAccessAPI_requestStorageAccess_BeyondCookies_indexedDB);
  }
  if (storage_access_types_->locks()) {
    window.CountUse(
        WebFeature::kStorageAccessAPI_requestStorageAccess_BeyondCookies_locks);
  }
  if (storage_access_types_->caches()) {
    window.CountUse(
        WebFeature::
            kStorageAccessAPI_requestStorageAccess_BeyondCookies_caches);
  }
  if (storage_access_types_->getDirectory()) {
    window.CountUse(
        WebFeature::
            kStorageAccessAPI_requestStorageAccess_BeyondCookies_getDirectory);
  }
  if (storage_access_types_->estimate()) {
    window.CountUse(
        WebFeature::
            kStorageAccessAPI_requestStorageAccess_BeyondCookies_estimate);
  }
  if (storage_access_types_->createObjectURL()) {
    window.CountUse(
        WebFeature::
            kStorageAccessAPI_requestStorageAccess_BeyondCookies_createObjectURL);
  }
  if (storage_access_types_->revokeObjectURL()) {
    window.CountUse(
        WebFeature::
            kStorageAccessAPI_requestStorageAccess_BeyondCookies_revokeObjectURL);
  }
  if (storage_access_types_->broadcastChannel()) {
    window.CountUse(
        WebFeature::
            kStorageAccessAPI_requestStorageAccess_BeyondCookies_BroadcastChannel);
  }
  if (storage_access_types_->sharedWorker()) {
    window.CountUse(
        WebFeature::
            kStorageAccessAPI_requestStorageAccess_BeyondCookies_SharedWorker);
  }
  // StorageAccessHandle is constructed in a promise, so while we are 'awaiting'
  // we should preempt the IPC we know we will need (and let local/session
  // storage have a chance to load from disk if needed) to ensure the latency of
  // synchronous methods stays low.
  if (storage_access_types_->all() || storage_access_types_->sessionStorage()) {
    GlobalStorageAccessHandle::From(window).GetSessionStorageArea();
  }
  if (storage_access_types_->all() || storage_access_types_->localStorage()) {
    GlobalStorageAccessHandle::From(window).GetLocalStorageArea();
  }
  if (storage_access_types_->all() || storage_access_types_->indexedDB()) {
    GlobalStorageAccessHandle::From(window).GetIDBFactory();
  }
  if (storage_access_types_->all() || storage_access_types_->locks()) {
    GlobalStorageAccessHandle::From(window).GetLockManager();
  }
  if (storage_access_types_->all() || storage_access_types_->caches()) {
    GlobalStorageAccessHandle::From(window).GetCacheStorage();
  }
  if (storage_access_types_->all() || storage_access_types_->getDirectory()) {
    GlobalStorageAccessHandle::From(window).GetRemote();
  }
  if (storage_access_types_->all() || storage_access_types_->estimate()) {
    GlobalStorageAccessHandle::From(window).GetRemote();
  }
  if (storage_access_types_->all() ||
      storage_access_types_->createObjectURL() ||
      storage_access_types_->revokeObjectURL() ||
      storage_access_types_->sharedWorker()) {
    GlobalStorageAccessHandle::From(window).GetPublicURLManager();
  }
  if (storage_access_types_->all() ||
      storage_access_types_->broadcastChannel()) {
    GlobalStorageAccessHandle::From(window).GetBroadcastChannelProvider();
  }
  if (storage_access_types_->all() || storage_access_types_->sharedWorker()) {
    GlobalStorageAccessHandle::From(window).GetSharedWorkerConnector();
  }
}

void StorageAccessHandle::Trace(Visitor* visitor) const {
  visitor->Trace(storage_access_types_);
  ScriptWrappable::Trace(visitor);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

StorageArea* StorageAccessHandle::sessionStorage(
    ExceptionState& exception_state) const {
  if (!storage_access_types_->all() &&
      !storage_access_types_->sessionStorage()) {
    exception_state.ThrowSecurityError(kSessionStorageNotRequested);
    return nullptr;
  }
  LocalDOMWindow* window = GetSupplementable();
  window->CountUse(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_sessionStorage_Use);
  StorageArea* session_storage_area =
      GlobalStorageAccessHandle::From(*window).GetSessionStorageArea();
  if (!session_storage_area) {
    return nullptr;
  }
  if (window->GetSecurityOrigin()->IsLocal()) {
    window->CountUse(WebFeature::kFileAccessedSessionStorage);
  }
  if (!session_storage_area->CanAccessStorage()) {
    exception_state.ThrowSecurityError(StorageArea::kAccessDeniedMessage);
    return nullptr;
  }
  return session_storage_area;
}

StorageArea* StorageAccessHandle::localStorage(
    ExceptionState& exception_state) const {
  if (!storage_access_types_->all() && !storage_access_types_->localStorage()) {
    exception_state.ThrowSecurityError(kLocalStorageNotRequested);
    return nullptr;
  }
  LocalDOMWindow* window = GetSupplementable();
  window->CountUse(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_localStorage_Use);
  StorageArea* local_storage_area =
      GlobalStorageAccessHandle::From(*window).GetLocalStorageArea();
  if (!local_storage_area) {
    return nullptr;
  }
  if (window->GetSecurityOrigin()->IsLocal()) {
    window->CountUse(WebFeature::kFileAccessedLocalStorage);
  }
  if (!local_storage_area->CanAccessStorage()) {
    exception_state.ThrowSecurityError(StorageArea::kAccessDeniedMessage);
    return nullptr;
  }
  return local_storage_area;
}

IDBFactory* StorageAccessHandle::indexedDB(
    ExceptionState& exception_state) const {
  if (!storage_access_types_->all() && !storage_access_types_->indexedDB()) {
    exception_state.ThrowSecurityError(kIndexedDBNotRequested);
    return nullptr;
  }
  GetSupplementable()->CountUse(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_indexedDB_Use);
  return GlobalStorageAccessHandle::From(*GetSupplementable()).GetIDBFactory();
}

LockManager* StorageAccessHandle::locks(ExceptionState& exception_state) const {
  if (!storage_access_types_->all() && !storage_access_types_->locks()) {
    exception_state.ThrowSecurityError(kLocksNotRequested);
    return nullptr;
  }
  GetSupplementable()->CountUse(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_locks_Use);
  return GlobalStorageAccessHandle::From(*GetSupplementable()).GetLockManager();
}

CacheStorage* StorageAccessHandle::caches(
    ExceptionState& exception_state) const {
  if (!storage_access_types_->all() && !storage_access_types_->caches()) {
    exception_state.ThrowSecurityError(kCachesNotRequested);
    return nullptr;
  }
  GetSupplementable()->CountUse(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_caches_Use);
  return GlobalStorageAccessHandle::From(*GetSupplementable())
      .GetCacheStorage();
}

ScriptPromise<FileSystemDirectoryHandle> StorageAccessHandle::getDirectory(
    ScriptState* script_state,
    ExceptionState& exception_state) const {
  if (!storage_access_types_->all() && !storage_access_types_->getDirectory()) {
    auto* resolver =
        MakeGarbageCollected<ScriptPromiseResolver<FileSystemDirectoryHandle>>(
            script_state, exception_state.GetContext());
    auto promise = resolver->Promise();
    resolver->RejectWithSecurityError(kGetDirectoryNotRequested,
                                      kGetDirectoryNotRequested);
    return promise;
  }
  GetSupplementable()->CountUse(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_getDirectory_Use);
  return StorageManagerFileSystemAccess::CheckStorageAccessIsAllowed(
      script_state, exception_state,
      WTF::BindOnce(&StorageAccessHandle::GetDirectoryImpl,
                    WrapWeakPersistent(this)));
}

void StorageAccessHandle::GetDirectoryImpl(
    ScriptPromiseResolver<FileSystemDirectoryHandle>* resolver) const {
  HeapMojoRemote<mojom::blink::StorageAccessHandle>& remote =
      GlobalStorageAccessHandle::From(*GetSupplementable()).GetRemote();
  if (!remote) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError));
    return;
  }
  remote->GetDirectory(
      WTF::BindOnce(&StorageManagerFileSystemAccess::DidGetSandboxedFileSystem,
                    WrapPersistent(resolver)));
}

ScriptPromise<StorageEstimate> StorageAccessHandle::estimate(
    ScriptState* script_state,
    ExceptionState& exception_state) const {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<StorageEstimate>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  if (!storage_access_types_->all() && !storage_access_types_->estimate()) {
    resolver->RejectWithSecurityError(kEstimateNotRequested,
                                      kEstimateNotRequested);
    return promise;
  }
  GetSupplementable()->CountUse(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_estimate_Use);
  HeapMojoRemote<mojom::blink::StorageAccessHandle>& remote =
      GlobalStorageAccessHandle::From(*GetSupplementable()).GetRemote();
  if (!remote) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError));
    return promise;
  }
  remote->Estimate(WTF::BindOnce(&EstimateImplAfterRemoteEstimate,
                                 WrapPersistent(resolver)));
  return promise;
}

String StorageAccessHandle::createObjectURL(
    Blob* blob,
    ExceptionState& exception_state) const {
  if (!storage_access_types_->all() &&
      !storage_access_types_->createObjectURL()) {
    exception_state.ThrowSecurityError(kCreateObjectURLNotRequested);
    return "";
  }
  PublicURLManager* public_url_manager =
      GlobalStorageAccessHandle::From(*GetSupplementable())
          .GetPublicURLManager();
  if (!public_url_manager) {
    return "";
  }
  GetSupplementable()->CountUse(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_createObjectURL_Use);
  GetSupplementable()->CountUse(WebFeature::kCreateObjectURLBlob);
  CHECK(blob);
  return public_url_manager->RegisterURL(blob);
}

void StorageAccessHandle::revokeObjectURL(
    const String& url,
    ExceptionState& exception_state) const {
  if (!storage_access_types_->all() &&
      !storage_access_types_->revokeObjectURL()) {
    exception_state.ThrowSecurityError(kRevokeObjectURLNotRequested);
    return;
  }
  PublicURLManager* public_url_manager =
      GlobalStorageAccessHandle::From(*GetSupplementable())
          .GetPublicURLManager();
  if (!public_url_manager) {
    return;
  }
  GetSupplementable()->CountUse(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_revokeObjectURL_Use);
  KURL resolved_url(NullURL(), url);
  GetSupplementable()->GetExecutionContext()->RemoveURLFromMemoryCache(
      resolved_url);
  public_url_manager->Revoke(resolved_url);
}

BroadcastChannel* StorageAccessHandle::BroadcastChannel(
    ExecutionContext* execution_context,
    const String& name,
    ExceptionState& exception_state) const {
  if (!storage_access_types_->all() &&
      !storage_access_types_->broadcastChannel()) {
    exception_state.ThrowSecurityError(kBroadcastChannelNotRequested);
    return nullptr;
  }
  HeapMojoAssociatedRemote<mojom::blink::BroadcastChannelProvider>&
      broadcast_channel_provider =
          GlobalStorageAccessHandle::From(*GetSupplementable())
              .GetBroadcastChannelProvider();
  if (!broadcast_channel_provider) {
    return nullptr;
  }
  GetSupplementable()->CountUse(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_BroadcastChannel_Use);
  return MakeGarbageCollected<blink::BroadcastChannel>(
      PassKey(), execution_context, name, broadcast_channel_provider.get());
}

blink::SharedWorker* StorageAccessHandle::SharedWorker(
    ExecutionContext* context,
    const String& url,
    const V8UnionSharedWorkerOptionsOrString* name_or_options,
    ExceptionState& exception_state) const {
  if (!storage_access_types_->all() && !storage_access_types_->sharedWorker()) {
    exception_state.ThrowSecurityError(kSharedWorkerNotRequested);
    return nullptr;
  }
  HeapMojoRemote<mojom::blink::SharedWorkerConnector>& shared_worker_connector =
      GlobalStorageAccessHandle::From(*GetSupplementable())
          .GetSharedWorkerConnector();
  if (!shared_worker_connector) {
    return nullptr;
  }
  PublicURLManager* public_url_manager =
      GlobalStorageAccessHandle::From(*GetSupplementable())
          .GetPublicURLManager();
  if (!public_url_manager) {
    return nullptr;
  }
  GetSupplementable()->CountUse(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_SharedWorker_Use);
  return SharedWorker::Create(PassKey(), context, url, name_or_options,
                              exception_state, public_url_manager,
                              &shared_worker_connector);
}

namespace bindings {

ExecutionContext* ExecutionContextFromV8Wrappable(
    const StorageAccessHandle* storage_access_handle) {
  return storage_access_handle->GetSupplementable();
}

}  // namespace bindings

}  // namespace blink
