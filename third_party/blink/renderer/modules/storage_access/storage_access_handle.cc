// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/storage_access/storage_access_handle.h"

#include "base/types/pass_key.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_storage_estimate.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_storage_usage_details.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/modules/broadcastchannel/broadcast_channel.h"
#include "third_party/blink/renderer/modules/file_system_access/storage_manager_file_system_access.h"
#include "third_party/blink/renderer/modules/storage/storage_controller.h"
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

namespace {

void EstimateImplAfterRemoteEstimate(ScriptPromiseResolver* resolver,
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
      storage_access_types_(storage_access_types),
      remote_(window.GetExecutionContext()),
      broadcast_channel_(window.GetExecutionContext()) {
  window.CountUse(
      WebFeature::kStorageAccessAPI_requestStorageAccess_BeyondCookies);
  if (storage_access_types_->all()) {
    window.CountUse(
        WebFeature::kStorageAccessAPI_requestStorageAccess_BeyondCookies_all);
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
  if (storage_access_types_->all() || storage_access_types_->sessionStorage()) {
    InitSessionStorage();
  }
  if (storage_access_types_->all() || storage_access_types_->localStorage()) {
    InitLocalStorage();
  }
  if (storage_access_types_->all() || storage_access_types_->indexedDB()) {
    InitIndexedDB();
  }
  if (storage_access_types_->all() || storage_access_types_->locks()) {
    InitLocks();
  }
  if (storage_access_types_->all() || storage_access_types_->caches()) {
    InitCaches();
  }
  if (storage_access_types_->all() || storage_access_types_->getDirectory()) {
    InitGetDirectory();
  }
  if (storage_access_types_->all() || storage_access_types_->estimate()) {
    InitQuota();
  }
  if (storage_access_types_->all() ||
      storage_access_types_->createObjectURL() ||
      storage_access_types_->revokeObjectURL()) {
    InitBlobStorage();
  }
  if (storage_access_types_->all() ||
      storage_access_types_->broadcastChannel()) {
    InitBroadcastChannel();
  }
}

void StorageAccessHandle::Trace(Visitor* visitor) const {
  visitor->Trace(storage_access_types_);
  visitor->Trace(session_storage_);
  visitor->Trace(local_storage_);
  visitor->Trace(remote_);
  visitor->Trace(indexed_db_);
  visitor->Trace(locks_);
  visitor->Trace(caches_);
  visitor->Trace(blob_storage_);
  visitor->Trace(broadcast_channel_);
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
  if (!session_storage_) {
    return nullptr;
  }
  if (window->GetSecurityOrigin()->IsLocal()) {
    window->CountUse(WebFeature::kFileAccessedSessionStorage);
  }
  if (!session_storage_->CanAccessStorage()) {
    exception_state.ThrowSecurityError(StorageArea::kAccessDeniedMessage);
    return nullptr;
  }
  return session_storage_;
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
  if (!local_storage_) {
    return nullptr;
  }
  if (window->GetSecurityOrigin()->IsLocal()) {
    window->CountUse(WebFeature::kFileAccessedLocalStorage);
  }
  if (!local_storage_->CanAccessStorage()) {
    exception_state.ThrowSecurityError(StorageArea::kAccessDeniedMessage);
    return nullptr;
  }
  return local_storage_;
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
  return indexed_db_;
}

LockManager* StorageAccessHandle::locks(ExceptionState& exception_state) const {
  if (!storage_access_types_->all() && !storage_access_types_->locks()) {
    exception_state.ThrowSecurityError(kLocksNotRequested);
    return nullptr;
  }
  GetSupplementable()->CountUse(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_locks_Use);
  return locks_;
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
  return caches_;
}

ScriptPromise StorageAccessHandle::getDirectory(
    ScriptState* script_state,
    ExceptionState& exception_state) const {
  if (!storage_access_types_->all() && !storage_access_types_->getDirectory()) {
    ScriptPromiseResolver* resolver =
        MakeGarbageCollected<ScriptPromiseResolver>(
            script_state, exception_state.GetContext());
    ScriptPromise promise = resolver->Promise();
    resolver->RejectWithSecurityError(kGetDirectoryNotRequested,
                                      kGetDirectoryNotRequested);
    return promise;
  }
  GetSupplementable()->CountUse(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_getDirectory_Use);
  return StorageManagerFileSystemAccess::CheckGetDirectoryIsAllowed(
      script_state, exception_state,
      WTF::BindOnce(&StorageAccessHandle::GetDirectoryImpl,
                    WrapWeakPersistent(this)));
}

void StorageAccessHandle::GetDirectoryImpl(
    ScriptPromiseResolver* resolver) const {
  if (!remote_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError));
    return;
  }
  remote_->GetDirectory(
      WTF::BindOnce(&StorageManagerFileSystemAccess::DidGetSandboxedFileSystem,
                    WrapPersistent(resolver)));
}

ScriptPromise StorageAccessHandle::estimate(
    ScriptState* script_state,
    ExceptionState& exception_state) const {
  ScriptPromiseResolver* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();
  if (!storage_access_types_->all() && !storage_access_types_->estimate()) {
    resolver->RejectWithSecurityError(kEstimateNotRequested,
                                      kEstimateNotRequested);
    return promise;
  }
  GetSupplementable()->CountUse(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_estimate_Use);
  if (!remote_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError));
    return promise;
  }
  remote_->Estimate(WTF::BindOnce(&EstimateImplAfterRemoteEstimate,
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
  GetSupplementable()->CountUse(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_createObjectURL_Use);
  GetSupplementable()->CountUse(WebFeature::kCreateObjectURLBlob);
  CHECK(blob);
  return blob_storage_->RegisterURL(blob);
}

void StorageAccessHandle::revokeObjectURL(
    const String& url,
    ExceptionState& exception_state) const {
  if (!storage_access_types_->all() &&
      !storage_access_types_->revokeObjectURL()) {
    exception_state.ThrowSecurityError(kRevokeObjectURLNotRequested);
    return;
  }
  GetSupplementable()->CountUse(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_revokeObjectURL_Use);
  KURL resolved_url(NullURL(), url);
  GetSupplementable()->GetExecutionContext()->RemoveURLFromMemoryCache(
      resolved_url);
  blob_storage_->Revoke(resolved_url);
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
  GetSupplementable()->CountUse(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_BroadcastChannel_Use);
  return MakeGarbageCollected<blink::BroadcastChannel>(
      PassKey(), execution_context, name, broadcast_channel_.get());
}

void StorageAccessHandle::InitSessionStorage() {
  LocalDOMWindow* window = GetSupplementable();
  if (!window->GetSecurityOrigin()->CanAccessSessionStorage()) {
    return;
  }
  if (!window->GetFrame()) {
    return;
  }
  StorageNamespace* storage_namespace =
      StorageNamespace::From(window->GetFrame()->GetPage());
  if (!storage_namespace) {
    return;
  }
  session_storage_ = StorageArea::Create(
      window,
      storage_namespace->GetCachedArea(
          window, {}, StorageNamespace::StorageContext::kStorageAccessAPI),
      StorageArea::StorageType::kSessionStorage);
}

void StorageAccessHandle::InitLocalStorage() {
  LocalDOMWindow* window = GetSupplementable();
  if (!window->GetSecurityOrigin()->CanAccessLocalStorage()) {
    return;
  }
  if (!window->GetFrame()) {
    return;
  }
  if (!window->GetFrame()->GetSettings()->GetLocalStorageEnabled()) {
    return;
  }
  auto storage_area = StorageController::GetInstance()->GetLocalStorageArea(
      window, {}, StorageNamespace::StorageContext::kStorageAccessAPI);
  local_storage_ = StorageArea::Create(window, std::move(storage_area),
                                       StorageArea::StorageType::kLocalStorage);
}

HeapMojoRemote<mojom::blink::StorageAccessHandle>&
StorageAccessHandle::InitRemote() {
  if (!remote_) {
    mojo::PendingRemote<mojom::blink::StorageAccessHandle> remote;
    GetSupplementable()
        ->GetExecutionContext()
        ->GetBrowserInterfaceBroker()
        .GetInterface(remote.InitWithNewPipeAndPassReceiver());
    remote_.Bind(std::move(remote),
                 GetSupplementable()->GetExecutionContext()->GetTaskRunner(
                     TaskType::kMiscPlatformAPI));
  }
  return remote_;
}

void StorageAccessHandle::InitIndexedDB() {
  if (!GetSupplementable()->GetSecurityOrigin()->CanAccessDatabase()) {
    return;
  }
  if (!InitRemote()) {
    return;
  }
  mojo::PendingRemote<mojom::blink::IDBFactory> indexed_db_remote;
  remote_->BindIndexedDB(indexed_db_remote.InitWithNewPipeAndPassReceiver());
  indexed_db_ = MakeGarbageCollected<IDBFactory>(GetSupplementable());
  indexed_db_->SetRemote(std::move(indexed_db_remote));
}

void StorageAccessHandle::InitLocks() {
  if (!GetSupplementable()->GetSecurityOrigin()->CanAccessLocks()) {
    return;
  }
  if (!InitRemote()) {
    return;
  }
  mojo::PendingRemote<mojom::blink::LockManager> locks_remote;
  remote_->BindLocks(locks_remote.InitWithNewPipeAndPassReceiver());
  locks_ = MakeGarbageCollected<LockManager>(*GetSupplementable()->navigator());
  locks_->SetManager(std::move(locks_remote),
                     GetSupplementable()->GetExecutionContext());
}

void StorageAccessHandle::InitCaches() {
  if (!GetSupplementable()->GetSecurityOrigin()->CanAccessCacheStorage()) {
    return;
  }
  if (!InitRemote()) {
    return;
  }
  mojo::PendingRemote<mojom::blink::CacheStorage> cache_remote;
  remote_->BindCaches(cache_remote.InitWithNewPipeAndPassReceiver());
  caches_ = MakeGarbageCollected<CacheStorage>(
      GetSupplementable()->GetExecutionContext(),
      GlobalFetch::ScopedFetcher::From(*GetSupplementable()),
      std::move(cache_remote));
}

void StorageAccessHandle::InitGetDirectory() {
  if (!GetSupplementable()->GetSecurityOrigin()->CanAccessFileSystem()) {
    return;
  }
  InitRemote();
  // Nothing else to init as getDirectory is an async function not a handle.
}

void StorageAccessHandle::InitQuota() {
  if (GetSupplementable()->GetSecurityOrigin()->IsOpaque()) {
    return;
  }
  InitRemote();
  // Nothing else to init as all Quota usage is via async functions.
}

void StorageAccessHandle::InitBlobStorage() {
  if (GetSupplementable()->GetSecurityOrigin()->IsOpaque()) {
    return;
  }
  if (!InitRemote()) {
    return;
  }
  mojo::PendingAssociatedRemote<mojom::blink::BlobURLStore> blob_storage_remote;
  remote_->BindBlobStorage(
      blob_storage_remote.InitWithNewEndpointAndPassReceiver());
  blob_storage_ = MakeGarbageCollected<PublicURLManager>(
      PassKey(), GetSupplementable()->GetExecutionContext(),
      std::move(blob_storage_remote));
}

void StorageAccessHandle::InitBroadcastChannel() {
  if (GetSupplementable()->GetSecurityOrigin()->IsOpaque()) {
    return;
  }
  if (!InitRemote()) {
    return;
  }
  remote_->BindBroadcastChannel(
      broadcast_channel_.BindNewEndpointAndPassReceiver(
          GetSupplementable()->GetExecutionContext()->GetTaskRunner(
              TaskType::kInternalDefault)));
}

}  // namespace blink
