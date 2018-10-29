// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_storage_partition.h"

namespace content {

TestStoragePartition::TestStoragePartition() {}
TestStoragePartition::~TestStoragePartition() {}

base::FilePath TestStoragePartition::GetPath() {
  return file_path_;
}

net::URLRequestContextGetter* TestStoragePartition::GetURLRequestContext() {
  return url_request_context_getter_;
}

net::URLRequestContextGetter*
TestStoragePartition::GetMediaURLRequestContext() {
  return media_url_request_context_getter_;
}

network::mojom::NetworkContext* TestStoragePartition::GetNetworkContext() {
  return network_context_;
}

scoped_refptr<network::SharedURLLoaderFactory>
TestStoragePartition::GetURLLoaderFactoryForBrowserProcess() {
  return nullptr;
}

std::unique_ptr<network::SharedURLLoaderFactoryInfo>
TestStoragePartition::GetURLLoaderFactoryForBrowserProcessIOThread() {
  return nullptr;
}

network::mojom::CookieManager*
TestStoragePartition::GetCookieManagerForBrowserProcess() {
  return cookie_manager_for_browser_process_;
}

storage::QuotaManager* TestStoragePartition::GetQuotaManager() {
  return quota_manager_;
}

AppCacheService* TestStoragePartition::GetAppCacheService() {
  return app_cache_service_;
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

IndexedDBContext* TestStoragePartition::GetIndexedDBContext() {
  return indexed_db_context_;
}

ServiceWorkerContext* TestStoragePartition::GetServiceWorkerContext() {
  return service_worker_context_;
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
    const OriginMatcherFunction& origin_matcher,
    network::mojom::CookieDeletionFilterPtr cookie_deletion_filter,
    bool perform_cleanup,
    const base::Time begin,
    const base::Time end,
    base::OnceClosure callback) {}

void TestStoragePartition::ClearHttpAndMediaCaches(
    const base::Time begin,
    const base::Time end,
    const base::Callback<bool(const GURL&)>& url_matcher,
    base::OnceClosure callback) {}

void TestStoragePartition::ClearCodeCaches(base::OnceClosure callback) {}

void TestStoragePartition::Flush() {}

void TestStoragePartition::ResetURLLoaderFactories() {}

void TestStoragePartition::ClearBluetoothAllowedDevicesMapForTesting() {}

void TestStoragePartition::FlushNetworkInterfaceForTesting() {}

void TestStoragePartition::WaitForDeletionTasksForTesting() {}

}  // namespace content
