// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/storage_access/storage_access_handle.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/modules/storage/storage_controller.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

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

StorageAccessHandle::StorageAccessHandle(
    LocalDOMWindow& window,
    const StorageAccessTypes* storage_access_types)
    : Supplement<LocalDOMWindow>(window),
      storage_access_types_(storage_access_types),
      remote_(window.GetExecutionContext()) {
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
}

void StorageAccessHandle::Trace(Visitor* visitor) const {
  visitor->Trace(storage_access_types_);
  visitor->Trace(session_storage_);
  visitor->Trace(local_storage_);
  visitor->Trace(remote_);
  visitor->Trace(indexed_db_);
  visitor->Trace(locks_);
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

void StorageAccessHandle::InitSessionStorage() {
  LocalDOMWindow* window = GetSupplementable();
  if (!window->GetFrame()) {
    return;
  }
  if (!window->GetSecurityOrigin()->CanAccessSessionStorage()) {
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
  if (!window->GetFrame()) {
    return;
  }
  if (!window->GetSecurityOrigin()->CanAccessLocalStorage()) {
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
StorageAccessHandle::GetRemote() {
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
  HeapMojoRemote<mojom::blink::StorageAccessHandle>& remote = GetRemote();
  if (!remote) {
    return;
  }
  indexed_db_ = MakeGarbageCollected<IDBFactory>(GetSupplementable());
  mojo::PendingRemote<mojom::blink::IDBFactory> indexed_db_remote;
  remote->BindIndexedDB(indexed_db_remote.InitWithNewPipeAndPassReceiver());
  indexed_db_->SetRemote(std::move(indexed_db_remote));
}

void StorageAccessHandle::InitLocks() {
  HeapMojoRemote<mojom::blink::StorageAccessHandle>& remote = GetRemote();
  if (!remote) {
    return;
  }
  locks_ = MakeGarbageCollected<LockManager>(*GetSupplementable()->navigator());
  mojo::PendingRemote<mojom::blink::LockManager> locks_remote;
  remote->BindLocks(locks_remote.InitWithNewPipeAndPassReceiver());
  locks_->SetManager(std::move(locks_remote),
                     GetSupplementable()->GetExecutionContext());
}

}  // namespace blink
