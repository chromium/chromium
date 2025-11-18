// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/storage_access/global_storage_access_handle.h"

#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/modules/storage/storage_controller.h"

namespace blink {

using PassKey = base::PassKey<GlobalStorageAccessHandle>;

// static
GlobalStorageAccessHandle& GlobalStorageAccessHandle::From(
    LocalDOMWindow& window) {
  GlobalStorageAccessHandle* supplement = window.GetGlobalStorageAccessHandle();
  if (!supplement) {
    supplement =
        MakeGarbageCollected<GlobalStorageAccessHandle>(PassKey(), window);
    window.SetGlobalStorageAccessHandle(supplement);
  }
  return *supplement;
}

HeapMojoRemote<mojom::blink::StorageAccessHandle>&
GlobalStorageAccessHandle::GetRemote() {
  if (!remote_) {
    mojo::PendingRemote<mojom::blink::StorageAccessHandle> remote;
    local_dom_window_->GetExecutionContext()
        ->GetBrowserInterfaceBroker()
        .GetInterface(remote.InitWithNewPipeAndPassReceiver());
    remote_.Bind(std::move(remote),
                 local_dom_window_->GetExecutionContext()->GetTaskRunner(
                     TaskType::kMiscPlatformAPI));
  }
  return remote_;
}

StorageArea* GlobalStorageAccessHandle::GetSessionStorageArea() {
  if (!session_storage_area_) {
    if (!local_dom_window_->GetSecurityOrigin()->CanAccessSessionStorage()) {
      return nullptr;
    }
    if (!local_dom_window_->GetFrame()) {
      return nullptr;
    }
    StorageNamespace* storage_namespace =
        StorageNamespace::From(local_dom_window_->GetFrame()->GetPage());
    if (!storage_namespace) {
      return nullptr;
    }
    session_storage_area_ = StorageArea::Create(
        local_dom_window_,
        storage_namespace->GetCachedArea(
            local_dom_window_, {},
            StorageNamespace::StorageContext::kStorageAccessAPI),
        StorageArea::StorageType::kSessionStorage);
  }
  return session_storage_area_;
}

StorageArea* GlobalStorageAccessHandle::GetLocalStorageArea() {
  if (!local_storage_area_) {
    if (!local_dom_window_->GetSecurityOrigin()->CanAccessLocalStorage()) {
      return nullptr;
    }
    if (!local_dom_window_->GetFrame()) {
      return nullptr;
    }
    if (!local_dom_window_->GetFrame()
             ->GetSettings()
             ->GetLocalStorageEnabled()) {
      return nullptr;
    }
    scoped_refptr<CachedStorageArea> storage_area =
        StorageController::GetInstance()->GetLocalStorageArea(
            local_dom_window_, {},
            StorageNamespace::StorageContext::kStorageAccessAPI);
    local_storage_area_ =
        StorageArea::Create(local_dom_window_, std::move(storage_area),
                            StorageArea::StorageType::kLocalStorage);
  }
  return local_storage_area_;
}

IDBFactory* GlobalStorageAccessHandle::GetIDBFactory() {
  if (!idb_factory_) {
    if (!local_dom_window_->GetSecurityOrigin()->CanAccessDatabase()) {
      return nullptr;
    }
    HeapMojoRemote<mojom::blink::StorageAccessHandle>& remote = GetRemote();
    if (!remote) {
      return nullptr;
    }
    mojo::PendingRemote<mojom::blink::IDBFactory> indexed_db_remote;
    remote->BindIndexedDB(indexed_db_remote.InitWithNewPipeAndPassReceiver());
    idb_factory_ = MakeGarbageCollected<IDBFactory>(local_dom_window_);
    idb_factory_->SetRemote(std::move(indexed_db_remote));
  }
  return idb_factory_;
}

LockManager* GlobalStorageAccessHandle::GetLockManager() {
  if (!lock_manager_) {
    if (!local_dom_window_->GetSecurityOrigin()->CanAccessLocks()) {
      return nullptr;
    }
    HeapMojoRemote<mojom::blink::StorageAccessHandle>& remote = GetRemote();
    if (!remote) {
      return nullptr;
    }
    mojo::PendingRemote<mojom::blink::LockManager> locks_remote;
    remote->BindLocks(locks_remote.InitWithNewPipeAndPassReceiver());
    lock_manager_ =
        MakeGarbageCollected<LockManager>(*local_dom_window_->navigator());
    lock_manager_->SetManager(std::move(locks_remote),
                              local_dom_window_->GetExecutionContext());
  }
  return lock_manager_;
}

CacheStorage* GlobalStorageAccessHandle::GetCacheStorage() {
  if (!cache_storage_) {
    if (!local_dom_window_->GetSecurityOrigin()->CanAccessCacheStorage()) {
      return nullptr;
    }
    HeapMojoRemote<mojom::blink::StorageAccessHandle>& remote = GetRemote();
    if (!remote) {
      return nullptr;
    }
    mojo::PendingRemote<mojom::blink::CacheStorage> cache_remote;
    remote->BindCaches(cache_remote.InitWithNewPipeAndPassReceiver());
    cache_storage_ = MakeGarbageCollected<CacheStorage>(
        local_dom_window_->GetExecutionContext(),
        GlobalFetch::ScopedFetcher::From(*local_dom_window_),
        std::move(cache_remote));
  }
  return cache_storage_;
}

PublicURLManager* GlobalStorageAccessHandle::GetPublicURLManager() {
  if (!public_url_manager_) {
    if (local_dom_window_->GetSecurityOrigin()->IsOpaque()) {
      return nullptr;
    }
    HeapMojoRemote<mojom::blink::StorageAccessHandle>& remote = GetRemote();
    if (!remote) {
      return nullptr;
    }
    mojo::PendingAssociatedRemote<mojom::blink::BlobURLStore>
        blob_storage_remote;
    remote->BindBlobStorage(
        blob_storage_remote.InitWithNewEndpointAndPassReceiver());
    public_url_manager_ = MakeGarbageCollected<PublicURLManager>(
        PassKey(), local_dom_window_->GetExecutionContext(),
        std::move(blob_storage_remote));
  }
  return public_url_manager_;
}

HeapMojoAssociatedRemote<mojom::blink::BroadcastChannelProvider>&
GlobalStorageAccessHandle::GetBroadcastChannelProvider() {
  if (!broadcast_channel_provider_) {
    if (local_dom_window_->GetSecurityOrigin()->IsOpaque()) {
      return broadcast_channel_provider_;
    }
    HeapMojoRemote<mojom::blink::StorageAccessHandle>& remote = GetRemote();
    if (!remote) {
      return broadcast_channel_provider_;
    }
    remote->BindBroadcastChannel(
        broadcast_channel_provider_.BindNewEndpointAndPassReceiver(
            local_dom_window_->GetExecutionContext()->GetTaskRunner(
                TaskType::kInternalDefault)));
  }
  return broadcast_channel_provider_;
}

HeapMojoRemote<mojom::blink::SharedWorkerConnector>&
GlobalStorageAccessHandle::GetSharedWorkerConnector() {
  if (!shared_worker_connector_) {
    if (!local_dom_window_->GetSecurityOrigin()->CanAccessSharedWorkers()) {
      return shared_worker_connector_;
    }
    HeapMojoRemote<mojom::blink::StorageAccessHandle>& remote = GetRemote();
    if (!remote) {
      return shared_worker_connector_;
    }
    remote->BindSharedWorker(
        shared_worker_connector_.BindNewPipeAndPassReceiver(
            local_dom_window_->GetExecutionContext()->GetTaskRunner(
                TaskType::kDOMManipulation)));
  }
  return shared_worker_connector_;
}

void GlobalStorageAccessHandle::Trace(Visitor* visitor) const {
  visitor->Trace(remote_);
  visitor->Trace(session_storage_area_);
  visitor->Trace(local_storage_area_);
  visitor->Trace(idb_factory_);
  visitor->Trace(lock_manager_);
  visitor->Trace(cache_storage_);
  visitor->Trace(public_url_manager_);
  visitor->Trace(broadcast_channel_provider_);
  visitor->Trace(shared_worker_connector_);
  visitor->Trace(local_dom_window_);
}

}  // namespace blink
