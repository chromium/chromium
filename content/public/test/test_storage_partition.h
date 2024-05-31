// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_STORAGE_PARTITION_H_
#define CONTENT_PUBLIC_TEST_TEST_STORAGE_PARTITION_H_

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/services/storage/privileged/mojom/indexed_db_control.mojom.h"
#include "components/services/storage/public/cpp/constants.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
#include "components/services/storage/public/mojom/local_storage_control.mojom.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/cert_verifier_service_updater.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace network {
namespace mojom {
class NetworkContext;
}  // namespace mojom
}  // namespace network

namespace content {

class BackgroundSyncContext;
class DevToolsBackgroundServicesContext;
class DOMStorageContext;
class FileSystemAccessEntryFactory;
class HostZoomLevelContext;
class HostZoomMap;
class PlatformNotificationContext;
class ServiceWorkerContext;
class ZoomLevelDelegate;

// Fake implementation of StoragePartition.
class TestStoragePartition : public StoragePartition {
 public:
  TestStoragePartition();

  TestStoragePartition(const TestStoragePartition&) = delete;
  TestStoragePartition& operator=(const TestStoragePartition&) = delete;

  ~TestStoragePartition() override;

  void set_config(StoragePartitionConfig config) { config_ = config; }
  const StoragePartitionConfig& GetConfig() const override;

  void set_path(base::FilePath file_path) { file_path_ = file_path; }
  const base::FilePath& GetPath() const override;

  void set_network_context(network::mojom::NetworkContext* context) {
    network_context_ = context;
  }
  network::mojom::NetworkContext* GetNetworkContext() override;
  cert_verifier::mojom::CertVerifierServiceUpdater*
  GetCertVerifierServiceUpdater() override;

  storage::SharedStorageManager* GetSharedStorageManager() override;

  scoped_refptr<network::SharedURLLoaderFactory>
  GetURLLoaderFactoryForBrowserProcess() override;

  std::unique_ptr<network::PendingSharedURLLoaderFactory>
  GetURLLoaderFactoryForBrowserProcessIOThread() override;

  void set_cookie_manager_for_browser_process(
      network::mojom::CookieManager* cookie_manager_for_browser_process) {
    cookie_manager_for_browser_process_ = cookie_manager_for_browser_process;
  }
  network::mojom::CookieManager* GetCookieManagerForBrowserProcess() override;

  void CreateTrustTokenQueryAnswerer(
      mojo::PendingReceiver<network::mojom::TrustTokenQueryAnswerer> receiver,
      const url::Origin& top_frame_origin) override;

  mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
  CreateURLLoaderNetworkObserverForFrame(int process_id,
                                         int routing_id) override;

  mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
  CreateURLLoaderNetworkObserverForNavigationRequest(
      NavigationRequest& navigation_request) override;

  void set_quota_manager(storage::QuotaManager* manager) {
    quota_manager_ = manager;
  }
  storage::QuotaManager* GetQuotaManager() override;

  void set_file_system_context(storage::FileSystemContext* context) {
    file_system_context_ = context;
  }
  storage::FileSystemContext* GetFileSystemContext() override;

  void set_background_sync_context(BackgroundSyncContext* context) {
    background_sync_context_ = context;
  }
  BackgroundSyncContext* GetBackgroundSyncContext() override;

  void set_database_tracker(storage::DatabaseTracker* tracker) {
    database_tracker_ = tracker;
  }
  storage::DatabaseTracker* GetDatabaseTracker() override;

  void set_dom_storage_context(DOMStorageContext* context) {
    dom_storage_context_ = context;
  }
  DOMStorageContext* GetDOMStorageContext() override;

  storage::mojom::LocalStorageControl* GetLocalStorageControl() override;

  storage::mojom::IndexedDBControl& GetIndexedDBControl() override;

  FileSystemAccessEntryFactory* GetFileSystemAccessEntryFactory() override;

  void set_service_worker_context(ServiceWorkerContext* context) {
    service_worker_context_ = context;
  }
  ServiceWorkerContext* GetServiceWorkerContext() override;

  DedicatedWorkerService* GetDedicatedWorkerService() override;

  void set_shared_worker_service(SharedWorkerService* service) {
    shared_worker_service_ = service;
  }
  SharedWorkerService* GetSharedWorkerService() override;

  storage::mojom::CacheStorageControl* GetCacheStorageControl() override;

  void set_generated_code_cache_context(GeneratedCodeCacheContext* context) {
    generated_code_cache_context_ = context;
  }
  GeneratedCodeCacheContext* GetGeneratedCodeCacheContext() override;

  void set_platform_notification_context(PlatformNotificationContext* context) {
    platform_notification_context_ = context;
  }
  PlatformNotificationContext* GetPlatformNotificationContext() override;

  InterestGroupManager* GetInterestGroupManager() override;

  AttributionDataModel* GetAttributionDataModel() override;

  PrivateAggregationDataModel* GetPrivateAggregationDataModel() override;

  CookieDeprecationLabelManager* GetCookieDeprecationLabelManager() override;

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  CdmStorageDataModel* GetCdmStorageDataModel() override;
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

  void DeleteStaleSessionOnlyCookiesAfterDelay() override {}

  void set_browsing_topics_site_data_manager(
      BrowsingTopicsSiteDataManager* manager) {
    browsing_topics_site_data_manager_ = manager;
  }
  BrowsingTopicsSiteDataManager* GetBrowsingTopicsSiteDataManager() override;

  void set_devtools_background_services_context(
      DevToolsBackgroundServicesContext* context) {
    devtools_background_services_context_ = context;
  }
  DevToolsBackgroundServicesContext* GetDevToolsBackgroundServicesContext()
      override;

  leveldb_proto::ProtoDatabaseProvider* GetProtoDatabaseProvider() override;
  void SetProtoDatabaseProvider(
      std::unique_ptr<leveldb_proto::ProtoDatabaseProvider> proto_db_provider)
      override;
  leveldb_proto::ProtoDatabaseProvider* GetProtoDatabaseProviderForTesting()
      override;

  void set_content_index_context(ContentIndexContext* context) {
    content_index_context_ = context;
  }
  ContentIndexContext* GetContentIndexContext() override;

  void set_host_zoom_map(HostZoomMap* map) { host_zoom_map_ = map; }
  HostZoomMap* GetHostZoomMap() override;

  void set_host_zoom_level_context(HostZoomLevelContext* context) {
    host_zoom_level_context_ = context;
  }
  HostZoomLevelContext* GetHostZoomLevelContext() override;

  void set_zoom_level_delegate(ZoomLevelDelegate* delegate) {
    zoom_level_delegate_ = delegate;
  }
  ZoomLevelDelegate* GetZoomLevelDelegate() override;

  void ClearDataForOrigin(uint32_t remove_mask,
                          uint32_t quota_storage_remove_mask,
                          const GURL& storage_origin,
                          base::OnceClosure callback) override;
  void ClearDataForBuckets(const blink::StorageKey& storage_key,
                           const std::set<std::string>& buckets,
                           base::OnceClosure callback) override;
  void ClearData(uint32_t remove_mask,
                 uint32_t quota_storage_remove_mask,
                 const blink::StorageKey& storage_key,
                 const base::Time begin,
                 const base::Time end,
                 base::OnceClosure callback) override;

  void ClearData(uint32_t remove_mask,
                 uint32_t quota_storage_remove_mask,
                 BrowsingDataFilterBuilder* filter_builder,
                 StorageKeyPolicyMatcherFunction storage_key_policy_matcher,
                 network::mojom::CookieDeletionFilterPtr cookie_deletion_filter,
                 bool perform_storage_cleanup,
                 const base::Time begin,
                 const base::Time end,
                 base::OnceClosure callback) override;

  void ClearCodeCaches(
      const base::Time begin,
      const base::Time end,
      const base::RepeatingCallback<bool(const GURL&)>& url_matcher,
      base::OnceClosure callback) override;

  void Flush() override;

  void ResetURLLoaderFactories() override;

  void AddObserver(DataRemovalObserver* observer) override;
  void RemoveObserver(DataRemovalObserver* observer) override;
  int GetDataRemovalObserverCount();

  void ClearBluetoothAllowedDevicesMapForTesting() override;
  void FlushNetworkInterfaceForTesting() override;
  void FlushCertVerifierInterfaceForTesting() override;
  void WaitForDeletionTasksForTesting() override;
  void WaitForCodeCacheShutdownForTesting() override;
  void SetNetworkContextForTesting(
      mojo::PendingRemote<network::mojom::NetworkContext>
          network_context_remote) override;
  void OverrideDeleteStaleSessionOnlyCookiesDelayForTesting(
      const base::TimeDelta& delay) override {}

  base::WeakPtr<StoragePartition> GetWeakPtr();
  void InvalidateWeakPtrs();

 private:
  StoragePartitionConfig config_;
  base::FilePath file_path_;
  mojo::Remote<network::mojom::NetworkContext> network_context_remote_;
  raw_ptr<network::mojom::NetworkContext, DanglingUntriaged> network_context_ =
      nullptr;
  raw_ptr<network::mojom::CookieManager> cookie_manager_for_browser_process_ =
      nullptr;
  raw_ptr<storage::QuotaManager> quota_manager_ = nullptr;
  raw_ptr<BackgroundSyncContext> background_sync_context_ = nullptr;
  raw_ptr<storage::FileSystemContext> file_system_context_ = nullptr;
  raw_ptr<storage::DatabaseTracker> database_tracker_ = nullptr;
  raw_ptr<DOMStorageContext> dom_storage_context_ = nullptr;
  mojo::Remote<storage::mojom::LocalStorageControl> local_storage_control_;
  mojo::Remote<storage::mojom::IndexedDBControl> indexed_db_control_;
  raw_ptr<ServiceWorkerContext> service_worker_context_ = nullptr;
  raw_ptr<DedicatedWorkerService> dedicated_worker_service_ = nullptr;
  raw_ptr<SharedWorkerService> shared_worker_service_ = nullptr;
  mojo::Remote<storage::mojom::CacheStorageControl> cache_storage_control_;
  raw_ptr<GeneratedCodeCacheContext> generated_code_cache_context_ = nullptr;
  raw_ptr<BrowsingTopicsSiteDataManager> browsing_topics_site_data_manager_ =
      nullptr;
  raw_ptr<PlatformNotificationContext> platform_notification_context_ = nullptr;
  raw_ptr<DevToolsBackgroundServicesContext>
      devtools_background_services_context_ = nullptr;
  raw_ptr<ContentIndexContext> content_index_context_ = nullptr;
  raw_ptr<HostZoomMap> host_zoom_map_ = nullptr;
  raw_ptr<HostZoomLevelContext> host_zoom_level_context_ = nullptr;
  raw_ptr<ZoomLevelDelegate> zoom_level_delegate_ = nullptr;
  int data_removal_observer_count_ = 0;

  // This member must be the last member.
  base::WeakPtrFactory<TestStoragePartition> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_STORAGE_PARTITION_H_
