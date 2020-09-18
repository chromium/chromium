// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_storage_partition.h"

#include "components/leveldb_proto/public/proto_database_provider.h"
#include "content/public/browser/native_file_system_entry_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace content {

TestStoragePartition::TestStoragePartition() {}
TestStoragePartition::~TestStoragePartition() {}

base::FilePath TestStoragePartition::GetPath() {
  return file_path_;
}

network::mojom::NetworkContext* TestStoragePartition::GetNetworkContext() {
  return network_context_;
}

scoped_refptr<network::SharedURLLoaderFactory>
TestStoragePartition::GetURLLoaderFactoryForBrowserProcess() {
  return nullptr;
}

scoped_refptr<network::SharedURLLoaderFactory>
TestStoragePartition::GetURLLoaderFactoryForBrowserProcessWithCORBEnabled() {
  return nullptr;
}

std::unique_ptr<network::PendingSharedURLLoaderFactory>
TestStoragePartition::GetURLLoaderFactoryForBrowserProcessIOThread() {
  return nullptr;
}

network::mojom::CookieManager*
TestStoragePartition::GetCookieManagerForBrowserProcess() {
  return cookie_manager_for_browser_process_;
}

void TestStoragePartition::CreateHasTrustTokensAnswerer(
    mojo::PendingReceiver<network::mojom::HasTrustTokensAnswerer> receiver,
    const url::Origin& top_frame_origin) {
  NOTREACHED() << "Not implemented.";
}

storage::QuotaManager* TestStoragePartition::GetQuotaManager() {
  return quota_manager_;
}

AppCacheService* TestStoragePartition::GetAppCacheService() {
  return app_cache_service_;
}

BackgroundSyncContext* TestStoragePartition::GetBackgroundSyncContext() {
  return background_sync_context_;
}

storage::FileSystemContext* TestStoragePartition::GetFileSystemContext() {
  return file_system_context_;
}

storage::DatabaseTracker* TestStoragePartition::GetDatabaseTracker() {
  return database_tracker_;
}

DOMStorageContext* TestStoragePartition::GetDOMStorageContext() {
  return dom_storage_context_;
}

storage::mojom::IndexedDBControl& TestStoragePartition::GetIndexedDBControl() {
  // Bind and throw away the receiver. If testing is required, then add a method
  // to set the remote.
  if (!indexed_db_control_.is_bound())
    ignore_result(indexed_db_control_.BindNewPipeAndPassReceiver());
  return *indexed_db_control_;
}

NativeFileSystemEntryFactory*
TestStoragePartition::GetNativeFileSystemEntryFactory() {
  return nullptr;
}

ServiceWorkerContext* TestStoragePartition::GetServiceWorkerContext() {
  return service_worker_context_;
}

DedicatedWorkerService* TestStoragePartition::GetDedicatedWorkerService() {
  return dedicated_worker_service_;
}

SharedWorkerService* TestStoragePartition::GetSharedWorkerService() {
  return shared_worker_service_;
}

CacheStorageContext* TestStoragePartition::GetCacheStorageContext() {
  return cache_storage_context_;
}

GeneratedCodeCacheContext*
TestStoragePartition::GetGeneratedCodeCacheContext() {
  return generated_code_cache_context_;
}

PlatformNotificationContext*
TestStoragePartition::GetPlatformNotificationContext() {
  return platform_notification_context_;
}

DevToolsBackgroundServicesContext*
TestStoragePartition::GetDevToolsBackgroundServicesContext() {
  return devtools_background_services_context_;
}

ContentIndexContext* TestStoragePartition::GetContentIndexContext() {
  return content_index_context_;
}

leveldb_proto::ProtoDatabaseProvider*
TestStoragePartition::GetProtoDatabaseProvider() {
  return nullptr;
}

void TestStoragePartition::SetProtoDatabaseProvider(
    std::unique_ptr<leveldb_proto::ProtoDatabaseProvider> proto_db_provider) {}

leveldb_proto::ProtoDatabaseProvider*
TestStoragePartition::GetProtoDatabaseProviderForTesting() {
  return nullptr;
}

#if !defined(OS_ANDROID)
HostZoomMap* TestStoragePartition::GetHostZoomMap() {
  return host_zoom_map_;
}

HostZoomLevelContext* TestStoragePartition::GetHostZoomLevelContext() {
  return host_zoom_level_context_;
}

ZoomLevelDelegate* TestStoragePartition::GetZoomLevelDelegate() {
  return zoom_level_delegate_;
}
#endif  // !defined(OS_ANDROID)

void TestStoragePartition::ClearDataForOrigin(
    uint32_t remove_mask,
    uint32_t quota_storage_remove_mask,
    const GURL& storage_origin) {}

void TestStoragePartition::ClearData(
    uint32_t remove_mask,
    uint32_t quota_storage_remove_mask,
    const GURL& storage_origin,
    const base::Time begin,
    const base::Time end,
    base::OnceClosure callback) {}

void TestStoragePartition::ClearData(
    uint32_t remove_mask,
    uint32_t quota_storage_remove_mask,
    OriginMatcherFunction origin_matcher,
    network::mojom::CookieDeletionFilterPtr cookie_deletion_filter,
    bool perform_storage_cleanup,
    const base::Time begin,
    const base::Time end,
    base::OnceClosure callback) {}

void TestStoragePartition::ClearCodeCaches(
    const base::Time begin,
    const base::Time end,
    const base::RepeatingCallback<bool(const GURL&)>& url_matcher,
    base::OnceClosure callback) {}

void TestStoragePartition::Flush() {}

void TestStoragePartition::ResetURLLoaderFactories() {}

void TestStoragePartition::AddObserver(DataRemovalObserver* observer) {
  data_removal_observer_count_++;
}

void TestStoragePartition::RemoveObserver(DataRemovalObserver* observer) {
  data_removal_observer_count_--;
}

int TestStoragePartition::GetDataRemovalObserverCount() {
  return data_removal_observer_count_;
}

void TestStoragePartition::ClearBluetoothAllowedDevicesMapForTesting() {}

void TestStoragePartition::FlushNetworkInterfaceForTesting() {}

void TestStoragePartition::WaitForDeletionTasksForTesting() {}

void TestStoragePartition::WaitForCodeCacheShutdownForTesting() {}

void TestStoragePartition::SetNetworkContextForTesting(
    mojo::PendingRemote<network::mojom::NetworkContext>
        network_context_remote) {}

}  // namespace content
