// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_STORAGE_PARTITION_IMPL_H_
#define CONTENT_BROWSER_STORAGE_PARTITION_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"
#include "components/services/storage/public/mojom/indexed_db_control.mojom.h"
#include "components/services/storage/public/mojom/partition.mojom.h"
#include "components/services/storage/public/mojom/storage_service.mojom.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/background_sync/background_sync_context_impl.h"
#include "content/browser/bluetooth/bluetooth_allowed_devices_map.h"
#include "content/browser/broadcast_channel/broadcast_channel_provider.h"
#include "content/browser/cache_storage/cache_storage_context_impl.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/content_index/content_index_context_impl.h"
#include "content/browser/devtools/devtools_background_services_context_impl.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/font_access/font_access_manager_impl.h"
#include "content/browser/indexed_db/indexed_db_control_wrapper.h"
#include "content/browser/locks/lock_manager.h"
#include "content/browser/notifications/platform_notification_context_impl.h"
#include "content/browser/payments/payment_app_context_impl.h"
#include "content/browser/push_messaging/push_messaging_context.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/browser/worker_host/dedicated_worker_service_impl.h"
#include "content/browser/worker_host/shared_worker_service_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/blink/public/mojom/dom_storage/dom_storage.mojom.h"

#if !defined(OS_ANDROID)
#include "content/browser/host_zoom_level_context.h"
#endif

namespace leveldb_proto {
class ProtoDatabaseProvider;
}

namespace content {

class BackgroundFetchContext;
class BlobRegistryWrapper;
class ConversionManagerImpl;
class CookieStoreContext;
class GeneratedCodeCacheContext;
class IndexedDBContextImpl;
class NativeFileSystemEntryFactory;
class NativeFileSystemManagerImpl;
class NativeIOContext;
class PrefetchURLLoaderService;
class QuotaContext;

class CONTENT_EXPORT StoragePartitionImpl
    : public StoragePartition,
      public blink::mojom::DomStorage,
      public network::mojom::NetworkContextClient {
 public:
  // It is guaranteed that storage partitions are destructed before the
  // browser context starts shutting down its corresponding IO thread residents
  // (e.g. resource context).
  ~StoragePartitionImpl() override;

  // Quota managed data uses a different representation for storage types than
  // StoragePartition uses. This method generates that representation.
  static storage::QuotaClientTypes GenerateQuotaClientTypes(
      uint32_t remove_mask);

  // Allows overriding the URLLoaderFactory creation for
  // GetURLLoaderFactoryForBrowserProcess.
  // Passing a null callback will restore the default behavior.
  // This method must be called either on the UI thread or before threads start.
  // This callback is run on the UI thread.
  using CreateNetworkFactoryCallback = base::RepeatingCallback<
      mojo::PendingRemote<network::mojom::URLLoaderFactory>(
          mojo::PendingRemote<network::mojom::URLLoaderFactory>
              original_factory)>;
  static void SetGetURLLoaderFactoryForBrowserProcessCallbackForTesting(
      CreateNetworkFactoryCallback url_loader_factory_callback);

  void OverrideQuotaManagerForTesting(
      storage::QuotaManager* quota_manager);
  void OverrideSpecialStoragePolicyForTesting(
      storage::SpecialStoragePolicy* special_storage_policy);
  void ShutdownBackgroundSyncContextForTesting();
  void OverrideBackgroundSyncContextForTesting(
      BackgroundSyncContextImpl* background_sync_context);
  void OverrideSharedWorkerServiceForTesting(
      std::unique_ptr<SharedWorkerServiceImpl> shared_worker_service);

  // StoragePartition interface.
  base::FilePath GetPath() override;
  network::mojom::NetworkContext* GetNetworkContext() override;
  scoped_refptr<network::SharedURLLoaderFactory>
  GetURLLoaderFactoryForBrowserProcess() override;
  scoped_refptr<network::SharedURLLoaderFactory>
  GetURLLoaderFactoryForBrowserProcessWithCORBEnabled() override;
  std::unique_ptr<network::PendingSharedURLLoaderFactory>
  GetURLLoaderFactoryForBrowserProcessIOThread() override;
  network::mojom::CookieManager* GetCookieManagerForBrowserProcess() override;
  void CreateHasTrustTokensAnswerer(
      mojo::PendingReceiver<network::mojom::HasTrustTokensAnswerer> receiver,
      const url::Origin& top_frame_origin) override;
  storage::QuotaManager* GetQuotaManager() override;
  ChromeAppCacheService* GetAppCacheService() override;
  BackgroundSyncContextImpl* GetBackgroundSyncContext() override;
  storage::FileSystemContext* GetFileSystemContext() override;
  storage::DatabaseTracker* GetDatabaseTracker() override;
  DOMStorageContextWrapper* GetDOMStorageContext() override;
  LockManager* GetLockManager();  // override; TODO: Add to interface
  storage::mojom::IndexedDBControl& GetIndexedDBControl() override;
  NativeFileSystemEntryFactory* GetNativeFileSystemEntryFactory() override;
  CacheStorageContextImpl* GetCacheStorageContext() override;
  ServiceWorkerContextWrapper* GetServiceWorkerContext() override;
  DedicatedWorkerServiceImpl* GetDedicatedWorkerService() override;
  SharedWorkerServiceImpl* GetSharedWorkerService() override;
  GeneratedCodeCacheContext* GetGeneratedCodeCacheContext() override;
  DevToolsBackgroundServicesContextImpl* GetDevToolsBackgroundServicesContext()
      override;
  ContentIndexContextImpl* GetContentIndexContext() override;
#if !defined(OS_ANDROID)
  HostZoomMap* GetHostZoomMap() override;
  HostZoomLevelContext* GetHostZoomLevelContext() override;
  ZoomLevelDelegate* GetZoomLevelDelegate() override;
#endif  // !defined(OS_ANDROID)
  PlatformNotificationContextImpl* GetPlatformNotificationContext() override;
  leveldb_proto::ProtoDatabaseProvider* GetProtoDatabaseProvider() override;
  void SetProtoDatabaseProvider(
      std::unique_ptr<leveldb_proto::ProtoDatabaseProvider> proto_db_provider)
      override;
  leveldb_proto::ProtoDatabaseProvider* GetProtoDatabaseProviderForTesting()
      override;
  void ClearDataForOrigin(uint32_t remove_mask,
                          uint32_t quota_storage_remove_mask,
                          const GURL& storage_origin) override;
  void ClearData(uint32_t remove_mask,
                 uint32_t quota_storage_remove_mask,
                 const GURL& storage_origin,
                 const base::Time begin,
                 const base::Time end,
                 base::OnceClosure callback) override;
  void ClearData(uint32_t remove_mask,
                 uint32_t quota_storage_remove_mask,
                 OriginMatcherFunction origin_matcher,
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
  void ClearBluetoothAllowedDevicesMapForTesting() override;
  void AddObserver(DataRemovalObserver* observer) override;
  void RemoveObserver(DataRemovalObserver* observer) override;
  void FlushNetworkInterfaceForTesting() override;
  void WaitForDeletionTasksForTesting() override;
  void WaitForCodeCacheShutdownForTesting() override;
  void SetNetworkContextForTesting(
      mojo::PendingRemote<network::mojom::NetworkContext>
          network_context_remote) override;
  BackgroundFetchContext* GetBackgroundFetchContext();
  PaymentAppContextImpl* GetPaymentAppContext();
  BroadcastChannelProvider* GetBroadcastChannelProvider();
  BluetoothAllowedDevicesMap* GetBluetoothAllowedDevicesMap();
  BlobRegistryWrapper* GetBlobRegistry();
  PrefetchURLLoaderService* GetPrefetchURLLoaderService();
  CookieStoreContext* GetCookieStoreContext();
  NativeFileSystemManagerImpl* GetNativeFileSystemManager();
  QuotaContext* GetQuotaContext();
  NativeIOContext* GetNativeIOContext();
  ConversionManagerImpl* GetConversionManager();
  FontAccessManagerImpl* GetFontAccessManager();
  std::string GetPartitionDomain();

  // blink::mojom::DomStorage interface.
  void OpenLocalStorage(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::StorageArea> receiver) override;
  void BindSessionStorageNamespace(
      const std::string& namespace_id,
      mojo::PendingReceiver<blink::mojom::SessionStorageNamespace> receiver)
      override;
  void BindSessionStorageArea(
      const url::Origin& origin,
      const std::string& namespace_id,
      mojo::PendingReceiver<blink::mojom::StorageArea> receiver) override;

  // network::mojom::NetworkContextClient interface.
  void OnAuthRequired(
      const base::Optional<base::UnguessableToken>& window_id,
      int32_t process_id,
      int32_t routing_id,
      uint32_t request_id,
      const GURL& url,
      bool first_auth_attempt,
      const net::AuthChallengeInfo& auth_info,
      network::mojom::URLResponseHeadPtr head,
      mojo::PendingRemote<network::mojom::AuthChallengeResponder>
          auth_challenge_responder) override;
  void OnCertificateRequested(
      const base::Optional<base::UnguessableToken>& window_id,
      int32_t process_id,
      int32_t routing_id,
      uint32_t request_id,
      const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
      mojo::PendingRemote<network::mojom::ClientCertificateResponder>
          cert_responder) override;
  void OnSSLCertificateError(int32_t process_id,
                             int32_t routing_id,
                             const GURL& url,
                             int net_error,
                             const net::SSLInfo& ssl_info,
                             bool fatal,
                             OnSSLCertificateErrorCallback response) override;
  void OnFileUploadRequested(int32_t process_id,
                             bool async,
                             const std::vector<base::FilePath>& file_paths,
                             OnFileUploadRequestedCallback callback) override;
  void OnCanSendReportingReports(
      const std::vector<url::Origin>& origins,
      OnCanSendReportingReportsCallback callback) override;
  void OnCanSendDomainReliabilityUpload(
      const GURL& origin,
      OnCanSendDomainReliabilityUploadCallback callback) override;
  void OnClearSiteData(int32_t process_id,
                       int32_t routing_id,
                       const GURL& url,
                       const std::string& header_value,
                       int load_flags,
                       OnClearSiteDataCallback callback) override;
#if defined(OS_ANDROID)
  void OnGenerateHttpNegotiateAuthToken(
      const std::string& server_auth_token,
      bool can_delegate,
      const std::string& auth_negotiate_android_account_type,
      const std::string& spn,
      OnGenerateHttpNegotiateAuthTokenCallback callback) override;
#endif
#if defined(OS_CHROMEOS)
  void OnTrustAnchorUsed() override;
#endif

  scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter() {
    return url_loader_factory_getter_;
  }

  // Can return nullptr while |this| is being destroyed.
  BrowserContext* browser_context() const;

  // Returns the interface used to control the corresponding remote Partition in
  // the Storage Service.
  storage::mojom::Partition* GetStorageServicePartition();

  // Exposes the shared top-level connection to the Storage Service, for tests.
  static mojo::Remote<storage::mojom::StorageService>&
  GetStorageServiceForTesting();

  // Called by each renderer process to bind its global DomStorage interface.
  // Returns the id of the created receiver.
  mojo::ReceiverId BindDomStorage(
      int process_id,
      mojo::PendingReceiver<blink::mojom::DomStorage> receiver,
      mojo::PendingRemote<blink::mojom::DomStorageClient> client);

  // Remove a receiver created by a previous BindDomStorage() call.
  void UnbindDomStorage(mojo::ReceiverId receiver_id);

  auto& dom_storage_receivers_for_testing() { return dom_storage_receivers_; }

  std::vector<std::string> cors_exempt_header_list() const {
    return cors_exempt_header_list_;
  }

  // When this StoragePartition is for guests (e.g., for a <webview> tag), this
  // is the site URL to use when creating a SiteInstance for a service worker.
  // Typically one would use the script URL of the service worker (e.g.,
  // "https://example.com/sw.js"), but if this StoragePartition is for guests,
  // one must use the <webview> guest site URL to ensure that the
  // service worker stays in this StoragePartition. This is an empty GURL if
  // this StoragePartition is not for guests.
  void set_site_for_guest_service_worker(const GURL& site_for_service_worker) {
    site_for_guest_service_worker_ = site_for_service_worker;
  }
  const GURL& site_for_guest_service_worker() const {
    return site_for_guest_service_worker_;
  }

  // Use the network context to retrieve the origin policy manager.
  network::mojom::OriginPolicyManager*
  GetOriginPolicyManagerForBrowserProcess();

  // We have to plumb |is_service_worker|, |process_id| and |routing_id| because
  // they are plumbed to WebView via WillCreateRestrictedCookieManager, which
  // makes some decision based on that.
  void CreateRestrictedCookieManager(
      network::mojom::RestrictedCookieManagerRole role,
      const url::Origin& origin,
      const net::SiteForCookies& site_for_cookies,
      const url::Origin& top_frame_origin,
      bool is_service_worker,
      int process_id,
      int routing_id,
      mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver,
      mojo::PendingRemote<network::mojom::CookieAccessObserver>
          cookie_observer);

  // Override the origin policy manager for testing use only.
  void SetOriginPolicyManagerForBrowserProcessForTesting(
      mojo::PendingRemote<network::mojom::OriginPolicyManager>
          test_origin_policy_manager);
  void ResetOriginPolicyManagerForBrowserProcessForTesting();

  mojo::PendingRemote<network::mojom::CookieAccessObserver>
  CreateCookieAccessObserverForServiceWorker();

  std::vector<std::string> GetCorsExemptHeaderList();

 private:
  class DataDeletionHelper;
  class QuotaManagedDataDeletionHelper;
  class URLLoaderFactoryForBrowserProcess;
  class ServiceWorkerCookieAccessObserver;

  friend class BackgroundSyncManagerTest;
  friend class BackgroundSyncServiceImplTestHarness;
  friend class CookieStoreManagerTest;
  friend class PaymentAppContentUnitTestBase;
  friend class ServiceWorkerRegistrationTest;
  friend class ServiceWorkerUpdateJobTest;
  friend class StoragePartitionImplMap;
  friend class URLLoaderFactoryForBrowserProcess;
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionShaderClearTest, ClearShaderCache);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedDataForeverBoth);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedDataForeverOnlyTemporary);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedDataForeverOnlyPersistent);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedDataForeverNeither);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedDataForeverSpecificOrigin);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedDataForLastHour);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedDataForLastWeek);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedUnprotectedOrigins);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedProtectedSpecificOrigin);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedProtectedOrigins);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedIgnoreDevTools);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest, RemoveCookieForever);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest, RemoveCookieLastHour);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveCookieWithDeleteInfo);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveUnprotectedLocalStorageForever);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveProtectedLocalStorageForever);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveLocalStorageForLastWeek);

  // |relative_partition_path| is the relative path under |profile_path| to the
  // StoragePartition's on-disk-storage.
  //
  // If |in_memory| is true, the |relative_partition_path| is (ab)used as a way
  // of distinguishing different in-memory partitions, but nothing is persisted
  // on to disk.
  //
  // Initialize() must be called on the StoragePartitionImpl before using it.
  static std::unique_ptr<StoragePartitionImpl> Create(
      BrowserContext* context,
      bool in_memory,
      const base::FilePath& relative_partition_path,
      const std::string& partition_domain);

  StoragePartitionImpl(BrowserContext* browser_context,
                       const base::FilePath& partition_path,
                       bool is_in_memory,
                       const base::FilePath& relative_partition_path,
                       const std::string& partition_domain,
                       storage::SpecialStoragePolicy* special_storage_policy);

  // This must be called before calling any members of the StoragePartitionImpl
  // except for GetPath and browser_context().
  // The purpose of the Create, Initialize sequence is that code that
  // initializes members of the StoragePartitionImpl and gets a pointer to it
  // can query properties of the StoragePartitionImpl (notably GetPath()).
  // If `fallback_for_blob_urls` is not null, blob urls that can't be resolved
  // in this storage partition will be attempted to be resolved in the fallback
  // storage partition instead.
  void Initialize(StoragePartitionImpl* fallback_for_blob_urls = nullptr);

  // If we're running Storage Service out-of-process and it crashes, this
  // re-establishes a connection and makes sure the service returns to a usable
  // state.
  void OnStorageServiceDisconnected();

  // We will never have both remove_origin be populated and a cookie_matcher.
  void ClearDataImpl(
      uint32_t remove_mask,
      uint32_t quota_storage_remove_mask,
      const GURL& remove_origin,
      OriginMatcherFunction origin_matcher,
      network::mojom::CookieDeletionFilterPtr cookie_deletion_filter,
      bool perform_storage_cleanup,
      const base::Time begin,
      const base::Time end,
      base::OnceClosure callback);

  void DeletionHelperDone(base::OnceClosure callback);

  // Function used by the quota system to ask the embedder for the
  // storage configuration info.
  void GetQuotaSettings(storage::OptionalQuotaSettingsCallback callback);

  // Called to initialize |network_context_| when |GetNetworkContext()| is
  // first called or there is an error.
  void InitNetworkContext();

  network::mojom::URLLoaderFactory*
  GetURLLoaderFactoryForBrowserProcessInternal(bool corb_enabled);

  IndexedDBContextImpl* GetIndexedDBContextInternal();

  // Raw pointer that should always be valid. The BrowserContext owns the
  // StoragePartitionImplMap which then owns StoragePartitionImpl. When the
  // BrowserContext is destroyed, |this| will be destroyed too.
  BrowserContext* browser_context_;

  const base::FilePath partition_path_;

  // |is_in_memory_|, |relative_partition_path_| and |partition_domain_| are
  // cached from |StoragePartitionImpl::Create()| in order to re-create
  // |NetworkContext|.
  const bool is_in_memory_;
  const base::FilePath relative_partition_path_;
  const std::string partition_domain_;

  // Until a StoragePartitionImpl is initialized using Initialize(), only
  // querying its path abd BrowserContext is allowed.
  bool initialized_ = false;

  mojo::Remote<storage::mojom::Partition> remote_partition_;
  scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter_;
  scoped_refptr<QuotaContext> quota_context_;
  scoped_refptr<storage::QuotaManager> quota_manager_;
  scoped_refptr<ChromeAppCacheService> appcache_service_;
  scoped_refptr<storage::FileSystemContext> filesystem_context_;
  scoped_refptr<storage::DatabaseTracker> database_tracker_;
  scoped_refptr<DOMStorageContextWrapper> dom_storage_context_;
  std::unique_ptr<LockManager> lock_manager_;
  std::unique_ptr<IndexedDBControlWrapper> indexed_db_control_wrapper_;
  scoped_refptr<CacheStorageContextImpl> cache_storage_context_;
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;
  std::unique_ptr<DedicatedWorkerServiceImpl> dedicated_worker_service_;
  std::unique_ptr<SharedWorkerServiceImpl> shared_worker_service_;
  std::unique_ptr<PushMessagingContext> push_messaging_context_;
  scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy_;
#if !defined(OS_ANDROID)
  std::unique_ptr<HostZoomLevelContext, BrowserThread::DeleteOnUIThread>
      host_zoom_level_context_;
#endif  // !defined(OS_ANDROID)
  scoped_refptr<PlatformNotificationContextImpl> platform_notification_context_;
  scoped_refptr<BackgroundFetchContext> background_fetch_context_;
  scoped_refptr<BackgroundSyncContextImpl> background_sync_context_;
  scoped_refptr<PaymentAppContextImpl> payment_app_context_;
  std::unique_ptr<BroadcastChannelProvider> broadcast_channel_provider_;
  std::unique_ptr<BluetoothAllowedDevicesMap> bluetooth_allowed_devices_map_;
  scoped_refptr<BlobRegistryWrapper> blob_registry_;
  scoped_refptr<PrefetchURLLoaderService> prefetch_url_loader_service_;
  scoped_refptr<CookieStoreContext> cookie_store_context_;
  scoped_refptr<GeneratedCodeCacheContext> generated_code_cache_context_;
  scoped_refptr<DevToolsBackgroundServicesContextImpl>
      devtools_background_services_context_;
  scoped_refptr<NativeFileSystemManagerImpl> native_file_system_manager_;
  std::unique_ptr<leveldb_proto::ProtoDatabaseProvider>
      proto_database_provider_;
  scoped_refptr<ContentIndexContextImpl> content_index_context_;
  std::unique_ptr<NativeIOContext> native_io_context_;
  std::unique_ptr<ConversionManagerImpl> conversion_manager_;
  std::unique_ptr<FontAccessManagerImpl> font_access_manager_;

  // ReceiverSet for DomStorage, using the
  // ChildProcessSecurityPolicyImpl::Handle as the binding context type. The
  // handle can subsequently be used during interface method calls to
  // enforce security checks.
  using SecurityPolicyHandle = ChildProcessSecurityPolicyImpl::Handle;
  mojo::ReceiverSet<blink::mojom::DomStorage,
                    std::unique_ptr<SecurityPolicyHandle>>
      dom_storage_receivers_;

  // A client interface for each receiver above.
  std::map<mojo::ReceiverId, mojo::Remote<blink::mojom::DomStorageClient>>
      dom_storage_clients_;

  // This is the NetworkContext used to
  // make requests for the StoragePartition. When the network service is
  // enabled, the underlying NetworkContext will be owned by the network
  // service. When it's disabled, the underlying NetworkContext may either be
  // provided by the embedder, or is created by the StoragePartition and owned
  // by |network_context_owner_|.
  mojo::Remote<network::mojom::NetworkContext> network_context_;

  mojo::Receiver<network::mojom::NetworkContextClient>
      network_context_client_receiver_{this};

  scoped_refptr<URLLoaderFactoryForBrowserProcess>
      shared_url_loader_factory_for_browser_process_;
  scoped_refptr<URLLoaderFactoryForBrowserProcess>
      shared_url_loader_factory_for_browser_process_with_corb_;

  // URLLoaderFactory/CookieManager for use in the browser process only.
  // See the method comment for
  // StoragePartition::GetURLLoaderFactoryForBrowserProcess() for
  // more details
  mojo::Remote<network::mojom::URLLoaderFactory>
      url_loader_factory_for_browser_process_;
  bool is_test_url_loader_factory_for_browser_process_ = false;
  mojo::Remote<network::mojom::URLLoaderFactory>
      url_loader_factory_for_browser_process_with_corb_;
  bool is_test_url_loader_factory_for_browser_process_with_corb_ = false;
  mojo::Remote<network::mojom::CookieManager>
      cookie_manager_for_browser_process_;
  mojo::Remote<network::mojom::OriginPolicyManager>
      origin_policy_manager_for_browser_process_;

  // The list of cors exempt headers that are set on |network_context_|.
  // Initialized in InitNetworkContext() and never updated after then.
  std::vector<std::string> cors_exempt_header_list_;

  // See comments for site_for_guest_service_worker().
  GURL site_for_guest_service_worker_;

  // Track number of running deletion. For test use only.
  int deletion_helpers_running_;

  base::ObserverList<DataRemovalObserver> data_removal_observers_;

  // Called when all deletions are done. For test use only.
  base::OnceClosure on_deletion_helpers_done_callback_;

  // A set of connections to the network service used to notify browser process
  // about cookie reads and writes made by a service worker in this process.
  mojo::UniqueReceiverSet<network::mojom::CookieAccessObserver>
      service_worker_cookie_observers_;

  base::WeakPtrFactory<StoragePartitionImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(StoragePartitionImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_STORAGE_PARTITION_IMPL_H_
