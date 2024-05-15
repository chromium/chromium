// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_storage_partition.h"

#include <tuple>

#include "components/leveldb_proto/public/proto_database_provider.h"
#include "content/public/browser/file_system_access_entry_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace content {

TestStoragePartition::TestStoragePartition() {}
TestStoragePartition::~TestStoragePartition() {}

const StoragePartitionConfig& TestStoragePartition::GetConfig() const {
  return config_;
}

const base::FilePath& TestStoragePartition::GetPath() const {
  return file_path_;
}

network::mojom::NetworkContext* TestStoragePartition::GetNetworkContext() {
  return network_context_;
}
cert_verifier::mojom::CertVerifierServiceUpdater*
TestStoragePartition::GetCertVerifierServiceUpdater() {
  return nullptr;
}

storage::SharedStorageManager* TestStoragePartition::GetSharedStorageManager() {
  return nullptr;
}

scoped_refptr<network::SharedURLLoaderFactory>
TestStoragePartition::GetURLLoaderFactoryForBrowserProcess() {
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

void TestStoragePartition::CreateTrustTokenQueryAnswerer(
    mojo::PendingReceiver<network::mojom::TrustTokenQueryAnswerer> receiver,
    const url::Origin& top_frame_origin) {
  NOTREACHED_IN_MIGRATION() << "Not implemented.";
}

mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
TestStoragePartition::CreateURLLoaderNetworkObserverForFrame(int process_id,
                                                             int routing_id) {
  return mojo::NullRemote();
}

mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
TestStoragePartition::CreateURLLoaderNetworkObserverForNavigationRequest(
    NavigationRequest& navigation_request) {
  return mojo::NullRemote();
}

storage::QuotaManager* TestStoragePartition::GetQuotaManager() {
  return quota_manager_;
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

storage::mojom::LocalStorageControl*
TestStoragePartition::GetLocalStorageControl() {
  // Bind and throw away the receiver. If testing is required, then add a method
  // to set the remote.
  if (!local_storage_control_.is_bound())
    std::ignore = local_storage_control_.BindNewPipeAndPassReceiver();
  return local_storage_control_.get();
}

storage::mojom::IndexedDBControl& TestStoragePartition::GetIndexedDBControl() {
  // Bind and throw away the receiver. If testing is required, then add a method
  // to set the remote.
  if (!indexed_db_control_.is_bound())
    std::ignore = indexed_db_control_.BindNewPipeAndPassReceiver();
  return *indexed_db_control_;
}

FileSystemAccessEntryFactory*
TestStoragePartition::GetFileSystemAccessEntryFactory() {
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

storage::mojom::CacheStorageControl*
TestStoragePartition::GetCacheStorageControl() {
  // Bind and throw away the receiver. If testing is required, then add a method
  // to set the remote.
  if (!cache_storage_control_.is_bound())
    std::ignore = cache_storage_control_.BindNewPipeAndPassReceiver();
  return cache_storage_control_.get();
}

GeneratedCodeCacheContext*
TestStoragePartition::GetGeneratedCodeCacheContext() {
  return generated_code_cache_context_;
}

PlatformNotificationContext*
TestStoragePartition::GetPlatformNotificationContext() {
  return platform_notification_context_;
}

InterestGroupManager* TestStoragePartition::GetInterestGroupManager() {
  return nullptr;
}

AttributionDataModel* TestStoragePartition::GetAttributionDataModel() {
  return nullptr;
}

PrivateAggregationDataModel*
TestStoragePartition::GetPrivateAggregationDataModel() {
  return nullptr;
}

CookieDeprecationLabelManager*
TestStoragePartition::GetCookieDeprecationLabelManager() {
  return nullptr;
}

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
CdmStorageDataModel* TestStoragePartition::GetCdmStorageDataModel() {
  return nullptr;
}
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

BrowsingTopicsSiteDataManager*
TestStoragePartition::GetBrowsingTopicsSiteDataManager() {
  return browsing_topics_site_data_manager_;
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

HostZoomMap* TestStoragePartition::GetHostZoomMap() {
  return host_zoom_map_;
}

HostZoomLevelContext* TestStoragePartition::GetHostZoomLevelContext() {
  return host_zoom_level_context_;
}

ZoomLevelDelegate* TestStoragePartition::GetZoomLevelDelegate() {
  return zoom_level_delegate_;
}

void TestStoragePartition::ClearDataForOrigin(
    uint32_t remove_mask,
    uint32_t quota_storage_remove_mask,
    const GURL& storage_origin,
    base::OnceClosure callback) {}

void TestStoragePartition::ClearDataForBuckets(
    const blink::StorageKey& storage_key,
    const std::set<std::string>& buckets,
    base::OnceClosure callback) {}

void TestStoragePartition::ClearData(uint32_t remove_mask,
                                     uint32_t quota_storage_remove_mask,
                                     const blink::StorageKey& storage_key,
                                     const base::Time begin,
                                     const base::Time end,
                                     base::OnceClosure callback) {}

void TestStoragePartition::ClearData(
    uint32_t remove_mask,
    uint32_t quota_storage_remove_mask,
    BrowsingDataFilterBuilder* filter_builder,
    StorageKeyPolicyMatcherFunction storage_key_policy_matcher,
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

void TestStoragePartition::FlushCertVerifierInterfaceForTesting() {}

void TestStoragePartition::WaitForDeletionTasksForTesting() {}

void TestStoragePartition::WaitForCodeCacheShutdownForTesting() {}

void TestStoragePartition::SetNetworkContextForTesting(
    mojo::PendingRemote<network::mojom::NetworkContext>
        network_context_remote) {}

base::WeakPtr<StoragePartition> TestStoragePartition::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void TestStoragePartition::InvalidateWeakPtrs() {
  weak_factory_.InvalidateWeakPtrs();
}

}  // namespace content
