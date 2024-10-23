// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_STORAGE_PARTITION_IMPL_H_
#define CONTENT_BROWSER_STORAGE_PARTITION_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/containers/flat_map.h"
#include "base/dcheck_is_on.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/services/storage/privileged/mojom/indexed_db_client_state_checker.mojom.h"
#include "components/services/storage/public/mojom/partition.mojom.h"
#include "components/services/storage/public/mojom/storage_service.mojom-forward.h"
#include "content/browser/background_sync/background_sync_context_impl.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/content_index/content_index_context_impl.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/locks/lock_manager.h"
#include "content/browser/notifications/platform_notification_context_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/worker_host/dedicated_worker_service_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "net/cookies/cookie_setting_override.h"
#include "services/network/public/cpp/network_service_buildflags.h"
#include "services/network/public/mojom/cert_verifier_service_updater.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_context_client.mojom.h"
#include "storage/browser/blob/blob_url_registry.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_settings.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/dom_storage/dom_storage.mojom.h"
#include "third_party/blink/public/mojom/frame/remote_frame.mojom.h"

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace net {
class IsolationInfo;
}  // namespace net

namespace network {
namespace mojom {
class SharedDictionaryAccessObserver;
}  // namespace mojom
}  // namespace network

namespace storage {
struct BucketClientInfo;
class SharedStorageManager;
}

namespace content {

namespace indexed_db {
class IndexedDBControlWrapper;
}

class AggregationService;
class AttributionManager;
class BackgroundFetchContext;
class BlobRegistryWrapper;
class BluetoothAllowedDevicesMap;
class BroadcastChannelService;
class BrowsingDataFilterBuilder;
class KeepAliveURLLoaderService;
class BucketManager;
class CacheStorageControlWrapper;
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
class CdmStorageDataModel;
class CdmStorageManager;
class MediaLicenseManager;
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)
class CookieDeprecationLabelManagerImpl;
class CookieStoreManager;
class DevToolsBackgroundServicesContextImpl;
class FileSystemAccessEntryFactory;
class FileSystemAccessManagerImpl;
class FontAccessManager;
class GeneratedCodeCacheContext;
class HostZoomLevelContext;
class InterestGroupManagerImpl;
class NavigationStateKeepAlive;
class PaymentAppContextImpl;
class PrivateAggregationDataModel;
class PrivateAggregationManager;
class PrivateAggregationManagerImpl;
class PushMessagingContext;
class QuotaContext;
class ReconnectableURLLoaderFactoryForIOThreadWrapper;
class SharedStorageHeaderObserver;
class SharedStorageWorkletHostManager;
class SharedWorkerServiceImpl;
class SubresourceProxyingURLLoaderService;
class NavigationOrDocumentHandle;

class CONTENT_EXPORT StoragePartitionImpl
    : public StoragePartition,
      public blink::mojom::DomStorage,
      public network::mojom::NetworkContextClient,
      public network::mojom::URLLoaderNetworkServiceObserver {
 public:
  StoragePartitionImpl(const StoragePartitionImpl&) = delete;
  StoragePartitionImpl& operator=(const StoragePartitionImpl&) = delete;

  // It is guaranteed that storage partitions are destructed before the
  // browser context starts shutting down its corresponding IO thread residents
  // (e.g. resource context).
  ~StoragePartitionImpl() override;

  // Quota managed data uses a different representation for storage types than
  // StoragePartition uses. This method generates that representation.
  static storage::QuotaClientTypes GenerateQuotaClientTypes(
      uint32_t remove_mask);

  // Forces Storage Service instances to be run in-process.
  static void ForceInProcessStorageServiceForTesting();

  void OverrideQuotaManagerForTesting(storage::QuotaManager* quota_manager);
  void OverrideSpecialStoragePolicyForTesting(
      storage::SpecialStoragePolicy* special_storage_policy);
  void ShutdownBackgroundSyncContextForTesting();
  void OverrideBackgroundSyncContextForTesting(
      BackgroundSyncContextImpl* background_sync_context);
  void OverrideSharedWorkerServiceForTesting(
      std::unique_ptr<SharedWorkerServiceImpl> shared_worker_service);
  void OverrideSharedStorageWorkletHostManagerForTesting(
      std::unique_ptr<SharedStorageWorkletHostManager>
          shared_storage_worklet_host_manager);
  void OverrideSharedStorageHeaderObserverForTesting(
      std::unique_ptr<SharedStorageHeaderObserver>
          shared_storage_header_observer);
  void OverrideAggregationServiceForTesting(
      std::unique_ptr<AggregationService> aggregation_service);
  void OverrideAttributionManagerForTesting(
      std::unique_ptr<AttributionManager> attribution_manager);
  void OverridePrivateAggregationManagerForTesting(
      std::unique_ptr<PrivateAggregationManagerImpl>
          private_aggregation_manager);

  // StoragePartition interface.
  const StoragePartitionConfig& GetConfig() const override;
  const base::FilePath& GetPath() const override;
  network::mojom::NetworkContext* GetNetworkContext() override;
  cert_verifier::mojom::CertVerifierServiceUpdater*
  GetCertVerifierServiceUpdater() override;
  network::mojom::URLLoaderFactoryParamsPtr CreateURLLoaderFactoryParams();
  scoped_refptr<network::SharedURLLoaderFactory>
  GetURLLoaderFactoryForBrowserProcess() override;
  std::unique_ptr<network::PendingSharedURLLoaderFactory>
  GetURLLoaderFactoryForBrowserProcessIOThread() override;
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
  storage::QuotaManager* GetQuotaManager() override;
  BackgroundSyncContextImpl* GetBackgroundSyncContext() override;
  storage::FileSystemContext* GetFileSystemContext() override;
  storage::DatabaseTracker* GetDatabaseTracker() override;
  DOMStorageContextWrapper* GetDOMStorageContext() override;
  storage::mojom::LocalStorageControl* GetLocalStorageControl() override;
  LockManager<storage::BucketId>*
  GetLockManager();  // override; TODO: Add to interface
  // TODO(crbug.com/40185706): Add this method to the StoragePartition
  // interface, which would also require making SharedStorageWorkletHostManager
  // an interface accessible in //content/public/.
  SharedStorageWorkletHostManager*
  GetSharedStorageWorkletHostManager();  // override;
  storage::mojom::IndexedDBControl& GetIndexedDBControl() override;
  FileSystemAccessEntryFactory* GetFileSystemAccessEntryFactory() override;
  storage::mojom::CacheStorageControl* GetCacheStorageControl() override;
  ServiceWorkerContextWrapper* GetServiceWorkerContext() override;
  DedicatedWorkerServiceImpl* GetDedicatedWorkerService() override;
  SharedWorkerService* GetSharedWorkerService() override;
  GeneratedCodeCacheContext* GetGeneratedCodeCacheContext() override;
  DevToolsBackgroundServicesContext* GetDevToolsBackgroundServicesContext()
      override;
  ContentIndexContextImpl* GetContentIndexContext() override;
  HostZoomMap* GetHostZoomMap() override;
  HostZoomLevelContext* GetHostZoomLevelContext() override;
  ZoomLevelDelegate* GetZoomLevelDelegate() override;
  PlatformNotificationContextImpl* GetPlatformNotificationContext() override;
  InterestGroupManager* GetInterestGroupManager() override;
  BrowsingTopicsSiteDataManager* GetBrowsingTopicsSiteDataManager() override;
  leveldb_proto::ProtoDatabaseProvider* GetProtoDatabaseProvider() override;
  // Use outside content.
  AttributionDataModel* GetAttributionDataModel() override;
  PrivateAggregationDataModel* GetPrivateAggregationDataModel() override;
  CookieDeprecationLabelManager* GetCookieDeprecationLabelManager() override;
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  CdmStorageDataModel* GetCdmStorageDataModel() override;
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)
  void DeleteStaleSessionOnlyCookiesAfterDelay() override;

  void SetProtoDatabaseProvider(
      std::unique_ptr<leveldb_proto::ProtoDatabaseProvider> proto_db_provider)
      override;
  leveldb_proto::ProtoDatabaseProvider* GetProtoDatabaseProviderForTesting()
      override;
  void ClearDataForOrigin(uint32_t remove_mask,
                          uint32_t quota_storage_remove_mask,
                          const GURL& storage_origin,
                          base::OnceClosure callback) override;
  void ClearDataForBuckets(const blink::StorageKey& storage_key,
                           const std::set<std::string>& storage_buckets,
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
  void ClearBluetoothAllowedDevicesMapForTesting() override;
  void AddObserver(DataRemovalObserver* observer) override;
  void RemoveObserver(DataRemovalObserver* observer) override;
  void FlushNetworkInterfaceForTesting() override;
  void FlushCertVerifierInterfaceForTesting() override;
  void WaitForDeletionTasksForTesting() override;
  void WaitForCodeCacheShutdownForTesting() override;
  void SetNetworkContextForTesting(
      mojo::PendingRemote<network::mojom::NetworkContext>
          network_context_remote) override;
  void OverrideDeleteStaleSessionOnlyCookiesDelayForTesting(
      const base::TimeDelta& delay) override;

  // TODO(crbug.com/352651664): Consider merging to
  // `FlushNetworkInterfaceForTesting()` if possible.
  void FlushNetworkInterfaceOnIOThreadForTesting();

  base::WeakPtr<StoragePartitionImpl> GetWeakPtr();
  BackgroundFetchContext* GetBackgroundFetchContext();
  PaymentAppContextImpl* GetPaymentAppContext();
  BroadcastChannelService* GetBroadcastChannelService();
  BluetoothAllowedDevicesMap* GetBluetoothAllowedDevicesMap();
  BlobRegistryWrapper* GetBlobRegistry();
  storage::BlobUrlRegistry* GetBlobUrlRegistry();
  SubresourceProxyingURLLoaderService* GetSubresourceProxyingURLLoaderService();
  KeepAliveURLLoaderService* GetKeepAliveURLLoaderService();
  CookieStoreManager* GetCookieStoreManager();
  FileSystemAccessManagerImpl* GetFileSystemAccessManager();
  BucketManager* GetBucketManager();
  QuotaContext* GetQuotaContext();
  // Use inside content.
  AttributionManager* GetAttributionManager();
  void SetFontAccessManagerForTesting(
      std::unique_ptr<FontAccessManager> font_access_manager);
  const std::string& GetPartitionDomain() const;
  AggregationService* GetAggregationService();
  FontAccessManager* GetFontAccessManager();
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  MediaLicenseManager* GetMediaLicenseManager();
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

  storage::SharedStorageManager* GetSharedStorageManager() override;
  PrivateAggregationManager* GetPrivateAggregationManager();

  // blink::mojom::DomStorage interface.
  void OpenLocalStorage(
      const blink::StorageKey& storage_key,
      const blink::LocalFrameToken& local_frame_token,
      mojo::PendingReceiver<blink::mojom::StorageArea> receiver) override;
  void BindSessionStorageNamespace(
      const std::string& namespace_id,
      mojo::PendingReceiver<blink::mojom::SessionStorageNamespace> receiver)
      override;
  void BindSessionStorageArea(
      const blink::StorageKey& storage_key,
      const blink::LocalFrameToken& local_frame_token,
      const std::string& namespace_id,
      mojo::PendingReceiver<blink::mojom::StorageArea> receiver) override;

  // network::mojom::NetworkContextClient interface.
  void OnFileUploadRequested(int32_t process_id,
                             bool async,
                             const std::vector<base::FilePath>& file_paths,
                             const GURL& destination_url,
                             OnFileUploadRequestedCallback callback) override;
  void OnCanSendReportingReports(
      const std::vector<url::Origin>& origins,
      OnCanSendReportingReportsCallback callback) override;
  void OnCanSendDomainReliabilityUpload(
      const url::Origin& origin,
      OnCanSendDomainReliabilityUploadCallback callback) override;
#if BUILDFLAG(IS_ANDROID)
  void OnGenerateHttpNegotiateAuthToken(
      const std::string& server_auth_token,
      bool can_delegate,
      const std::string& auth_negotiate_android_account_type,
      const std::string& spn,
      OnGenerateHttpNegotiateAuthTokenCallback callback) override;
#endif
#if BUILDFLAG(IS_CHROMEOS)
  void OnTrustAnchorUsed() override;
#endif
#if BUILDFLAG(IS_CT_SUPPORTED)
  void OnCanSendSCTAuditingReport(
      OnCanSendSCTAuditingReportCallback callback) override;
  void OnNewSCTAuditingReportSent() override;
#endif

  // network::mojom::URLLoaderNetworkServiceObserver interface.
  void OnSSLCertificateError(const GURL& url,
                             int net_error,
                             const net::SSLInfo& ssl_info,
                             bool fatal,
                             OnSSLCertificateErrorCallback response) override;
  void OnCertificateRequested(
      const std::optional<base::UnguessableToken>& window_id,
      const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
      mojo::PendingRemote<network::mojom::ClientCertificateResponder>
          cert_responder) override;
  void Clone(
      mojo::PendingReceiver<network::mojom::URLLoaderNetworkServiceObserver>
          listener) override;
  void OnWebSocketConnectedToPrivateNetwork(
      network::mojom::IPAddressSpace ip_address_space) override;
  void OnAuthRequired(
      const std::optional<base::UnguessableToken>& window_id,
      int32_t request_id,
      const GURL& url,
      bool first_auth_attempt,
      const net::AuthChallengeInfo& auth_info,
      const scoped_refptr<net::HttpResponseHeaders>& head_headers,
      mojo::PendingRemote<network::mojom::AuthChallengeResponder>
          auth_challenge_responder) override;
  void OnPrivateNetworkAccessPermissionRequired(
      const GURL& url,
      const net::IPAddress& ip_address,
      const std::optional<std::string>& private_network_device_id,
      const std::optional<std::string>& private_network_device_name,
      OnPrivateNetworkAccessPermissionRequiredCallback callback) override;
  void OnClearSiteData(
      const GURL& url,
      const std::string& header_value,
      int load_flags,
      const std::optional<net::CookiePartitionKey>& cookie_partition_key,
      bool partitioned_state_allowed_only,
      OnClearSiteDataCallback callback) override;
  void OnLoadingStateUpdate(network::mojom::LoadInfoPtr info,
                            OnLoadingStateUpdateCallback callback) override;
  void OnDataUseUpdate(int32_t network_traffic_annotation_id_hash,
                       int64_t recv_bytes,
                       int64_t sent_bytes) override;
  void OnSharedStorageHeaderReceived(
      const url::Origin& request_origin,
      std::vector<network::mojom::SharedStorageOperationPtr> operations,
      OnSharedStorageHeaderReceivedCallback callback) override;

  SharedStorageHeaderObserver* shared_storage_header_observer() {
    return shared_storage_header_observer_.get();
  }

  // Can return nullptr while `this` is being destroyed.
  BrowserContext* browser_context() const;

  // Returns the interface used to control the corresponding remote Partition in
  // the Storage Service.
  storage::mojom::Partition* GetStorageServicePartition();

  // Exposes the shared top-level connection to the Storage Service, for tests.
  static mojo::Remote<storage::mojom::StorageService>&
  GetStorageServiceForTesting();

  // Binds the mojo endpoint for an `IDBFactory` (which implements
  // `window.indexedDB`).
  void BindIndexedDB(
      const storage::BucketLocator& bucket_locator,
      const storage::BucketClientInfo& client_info,
      mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
          client_state_checker_remote,
      mojo::PendingReceiver<blink::mojom::IDBFactory> receiver);

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

  // Tracks whether this StoragePartition is for guests (e.g., for a <webview>
  // tag).  This is needed to properly create a SiteInstance for a
  // service worker or a shared worker in a guest. Typically one would use the
  // script URL of the worker (e.g., "https://example.com/sw.js"), but if this
  // StoragePartition is for guests, one must create the SiteInstance via
  // guest-specific helpers that ensure that the worker stays in the same
  // StoragePartition.
  void set_is_guest() { is_guest_ = true; }
  bool is_guest() const { return is_guest_; }

  // We have to plumb `is_service_worker`, `process_id` and `routing_id` because
  // they are plumbed to WebView via WillCreateRestrictedCookieManager, which
  // makes some decision based on that.
  void CreateRestrictedCookieManager(
      network::mojom::RestrictedCookieManagerRole role,
      const url::Origin& origin,
      const net::IsolationInfo& isolation_info,
      bool is_service_worker,
      int process_id,
      int routing_id,
      net::CookieSettingOverrides cookie_setting_overrides,
      mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver,
      mojo::PendingRemote<network::mojom::CookieAccessObserver>
          cookie_observer);

  mojo::PendingRemote<network::mojom::CookieAccessObserver>
  CreateCookieAccessObserverForServiceWorker();

  mojo::PendingRemote<network::mojom::TrustTokenAccessObserver>
  CreateTrustTokenAccessObserverForServiceWorker();

  mojo::PendingRemote<network::mojom::SharedDictionaryAccessObserver>
  CreateSharedDictionaryAccessObserverForServiceWorker();

  mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
  CreateAuthCertObserverForServiceWorker(int process_id);

  std::vector<std::string> GetCorsExemptHeaderList();

  void OpenLocalStorageForProcess(
      int process_id,
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<blink::mojom::StorageArea> receiver);
  void BindSessionStorageAreaForProcess(
      int process_id,
      const blink::StorageKey& storage_key,
      const std::string& namespace_id,
      mojo::PendingReceiver<blink::mojom::StorageArea> receiver);

  storage::QuotaManagerProxy* GetQuotaManagerProxy();

  // Called by BrowserContextImpl prior to destruction.
  void OnBrowserContextWillBeDestroyed();

  // Store `receiver` and its corresponding `handle`. These will be kept alive
  // as long as the remote endpoint of `receiver` is still alive on the renderer
  // side. The receiver will be automatically deleted when the endpoint is
  // disconnected.
  void RegisterKeepAliveHandle(
      mojo::PendingReceiver<blink::mojom::NavigationStateKeepAliveHandle>
          receiver,
      std::unique_ptr<NavigationStateKeepAlive> handle);

  // Forward the call to `NetworkContext::RevokeNetworkForNonces` and save the
  // nonces in `StoragePartitionImpl`. Clients should revoke network access for
  // nonces using this function instead of calling
  // `NetworkContext::RevokeNetworkForNonces` directly. This is because this
  // function saves the nonces so that they can be restored in case of a
  // `NetworkService` crash.
  void RevokeNetworkForNoncesInNetworkContext(
      const std::vector<base::UnguessableToken>& nonces,
      network::mojom::NetworkContext::RevokeNetworkForNoncesCallback callback);

  // Forward the call to `NetworkContext::ClearNonces` and remove the stored
  // nonce values in `StoragePartitionImpl`. Clients should clear nonces using
  // this function instead of calling `NetworkContext::ClearNonces` directly.
  // This should only be called when the nonces saved by
  // `RevokeNetworkForNoncesInNetworkContext` are no longer relevant.
  // The nonces are cleared after a time delay, which will prevent races where
  // network requests succeed while the fenced frame corresponding to the
  // nonces is being destroyed.
  void ClearNoncesInNetworkContextAfterDelay(
      const std::vector<base::UnguessableToken>& nonces);

  // Get the NavigationStateKeepAlive associated with `frame_token`. See
  // `navigation_state_keep_alive_map_`.
  NavigationStateKeepAlive* GetNavigationStateKeepAlive(
      blink::LocalFrameToken frame_token);

  // Removes the NavigationStateKeepAlive associated with `frame_token`. This
  // should be called when the keep alive is destructed.
  void RemoveKeepAliveHandleFromMap(blink::LocalFrameToken frame_token,
                                    NavigationStateKeepAlive* keep_alive);

  void SetClearNoncesInNetworkContextParamsForTesting(
      const base::TimeDelta& delay,
      base::RepeatingClosure callback);

  enum class ContextType {
    kRenderFrameHostContext,
    kNavigationRequestContext,
    kServiceWorkerContext,
  };

 private:
  class DataDeletionHelper;
  class QuotaManagedDataDeletionHelper;
  class ServiceWorkerCookieAccessObserver;
  class ServiceWorkerTrustTokenAccessObserver;
  class ServiceWorkerSharedDictionaryAccessObserver;
  struct NetworkContextOwner;

  friend class BackgroundSyncManagerTest;
  friend class BackgroundSyncServiceImplTestHarness;
  friend class CookieStoreManagerTest;
  friend class PaymentAppContentUnitTestBase;
  friend class ServiceWorkerRegistrationTest;
  friend class ServiceWorkerUpdateJobTest;
  friend class StoragePartitionImplMap;
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

  class URLLoaderNetworkContext {
   public:
    ~URLLoaderNetworkContext();

    // Allow copy and assign.
    URLLoaderNetworkContext(const URLLoaderNetworkContext& other);
    URLLoaderNetworkContext& operator=(const URLLoaderNetworkContext& other);

    // Creates a URLLoaderNetworkContext for the RenderFrameHost.
    static StoragePartitionImpl::URLLoaderNetworkContext
    CreateForRenderFrameHost(
        GlobalRenderFrameHostId global_render_frame_host_id);

    // Creates a URLLoaderNetworkContext for the navigation request.
    static StoragePartitionImpl::URLLoaderNetworkContext CreateForNavigation(
        NavigationRequest& navigation_request);

    // Used when `type` is `kRenderFrameHostContext`.
    explicit URLLoaderNetworkContext(
        GlobalRenderFrameHostId global_render_frame_host_id);

    // Used when `type` is `kServiceWorkerContext`.
    explicit URLLoaderNetworkContext(int process_id);

    // Used when `type` is `kNavigationRequestContext`.
    explicit URLLoaderNetworkContext(NavigationRequest& navigation_request);

    // Returns true if `type` is `kNavigationRequestContext`.
    bool IsNavigationRequestContext() const;

    ContextType type() const { return type_; }

    NavigationOrDocumentHandle* navigation_or_document() const {
      return navigation_or_document_.get();
    }

    int process_id() const { return process_id_; }

    // If `type_` is kServiceWorkerContext, returns nullptr. Otherwise returns
    // the WebContents.
    WebContents* GetWebContents();

    // Returns true if the request is the primary main frame navigation.
    bool IsPrimaryMainFrameRequest();

   private:
    ContextType type_;
    scoped_refptr<NavigationOrDocumentHandle> navigation_or_document_;

    // Only valid when `type_` is kServiceWorkerContext.
    int process_id_ = content::ChildProcessHost::kInvalidUniqueID;
  };

  // `relative_partition_path` is the relative path under `profile_path` to the
  // StoragePartition's on-disk-storage.
  //
  // If `in_memory` is true, the `relative_partition_path` is (ab)used as a way
  // of distinguishing different in-memory partitions, but nothing is persisted
  // on to disk.
  //
  // Initialize() must be called on the StoragePartitionImpl before using it,
  // and OnBrowserContextWillBeDestroyed() must be called on it prior to
  // `context` being destroyed.
  static std::unique_ptr<StoragePartitionImpl> Create(
      BrowserContext* context,
      const StoragePartitionConfig& config,
      const base::FilePath& relative_partition_path);

  StoragePartitionImpl(BrowserContext* browser_context,
                       const StoragePartitionConfig& config,
                       const base::FilePath& partition_path,
                       const base::FilePath& relative_partition_path,
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

  // Clears the data specified by the `storage_key` or
  // `filter_builder`/`storage_key_policy_matcher`. `storage_key` and
  // `filter_builder`/`storage_key_policy_matcher` will never both be populated.
  void ClearDataImpl(
      uint32_t remove_mask,
      uint32_t quota_storage_remove_mask,
      const blink::StorageKey& storage_key,
      BrowsingDataFilterBuilder* filter_builder,
      StorageKeyPolicyMatcherFunction storage_key_policy_matcher,
      network::mojom::CookieDeletionFilterPtr cookie_deletion_filter,
      bool perform_storage_cleanup,
      const base::Time begin,
      const base::Time end,
      base::OnceClosure callback);

  void ClearDataForBucketsDone(
      const blink::StorageKey& storage_key,
      const std::set<std::string>& storage_buckets,
      base::OnceClosure callback,
      const std::vector<blink::mojom::QuotaStatusCode>& status_codes);

  void DeletionHelperDone(base::OnceClosure callback);

  // Function used by the quota system to ask the embedder for the
  // storage configuration info.
  void GetQuotaSettings(storage::OptionalQuotaSettingsCallback callback);

  // Called to initialize `network_context_` when `GetNetworkContext()` is
  // first called or there is an error.
  void InitNetworkContext();

  bool is_in_memory() { return config_.in_memory(); }

  void CreateURLLoaderFactoryForBrowserProcessInternal(
      mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory);

  std::optional<blink::StorageKey> CalculateStorageKey(
      const url::Origin& origin,
      const base::UnguessableToken* nonce);

  GlobalRenderFrameHostId GetRenderFrameHostIdFromNetworkContext();

  void DeleteStaleSessionOnlyCookiesAfterDelayCallback();

  void ClearNoncesInNetworkContextAfterDelayCallback(
      const std::vector<base::UnguessableToken>& nonces);

  // Raw pointer that should always be valid. The BrowserContext owns the
  // StoragePartitionImplMap which then owns StoragePartitionImpl. When the
  // BrowserContext is destroyed, `this` will be destroyed too.
  raw_ptr<BrowserContext, DanglingUntriaged> browser_context_;

  const base::FilePath partition_path_;

  // `config_` and `relative_partition_path_` are cached from
  // `StoragePartitionImpl::Create()` in order to re-create `NetworkContext`.
  const StoragePartitionConfig config_;
  const base::FilePath relative_partition_path_;

  // Until a StoragePartitionImpl is initialized using Initialize(), only
  // querying its path abd BrowserContext is allowed.
  bool initialized_ = false;

  mojo::Remote<storage::mojom::Partition> remote_partition_;
  scoped_refptr<QuotaContext> quota_context_;
  scoped_refptr<storage::QuotaManager> quota_manager_;
  scoped_refptr<storage::FileSystemContext> filesystem_context_;
  scoped_refptr<storage::DatabaseTracker> database_tracker_;
  scoped_refptr<DOMStorageContextWrapper> dom_storage_context_;
  std::unique_ptr<LockManager<storage::BucketId>> lock_manager_;
  std::unique_ptr<indexed_db::IndexedDBControlWrapper>
      indexed_db_control_wrapper_;
  std::unique_ptr<CacheStorageControlWrapper> cache_storage_control_wrapper_;
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;
  std::unique_ptr<DedicatedWorkerServiceImpl> dedicated_worker_service_;
  std::unique_ptr<SharedWorkerServiceImpl> shared_worker_service_;
  std::unique_ptr<PushMessagingContext> push_messaging_context_;
  scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy_;
  std::unique_ptr<HostZoomLevelContext, BrowserThread::DeleteOnUIThread>
      host_zoom_level_context_;
  scoped_refptr<PlatformNotificationContextImpl> platform_notification_context_;
  scoped_refptr<BackgroundFetchContext> background_fetch_context_;
  scoped_refptr<BackgroundSyncContextImpl> background_sync_context_;
  scoped_refptr<PaymentAppContextImpl> payment_app_context_;
  std::unique_ptr<BroadcastChannelService> broadcast_channel_service_;
  std::unique_ptr<BluetoothAllowedDevicesMap> bluetooth_allowed_devices_map_;
  scoped_refptr<BlobRegistryWrapper> blob_registry_;
  std::unique_ptr<storage::BlobUrlRegistry> blob_url_registry_;
  std::unique_ptr<SubresourceProxyingURLLoaderService>
      subresource_proxying_url_loader_service_;
  std::unique_ptr<KeepAliveURLLoaderService> keep_alive_url_loader_service_;
  std::unique_ptr<CookieStoreManager> cookie_store_manager_;
  std::unique_ptr<BucketManager> bucket_manager_;
  scoped_refptr<GeneratedCodeCacheContext> generated_code_cache_context_;
  std::unique_ptr<DevToolsBackgroundServicesContextImpl>
      devtools_background_services_context_;
  scoped_refptr<FileSystemAccessManagerImpl> file_system_access_manager_;
  std::unique_ptr<leveldb_proto::ProtoDatabaseProvider>
      proto_database_provider_;
  scoped_refptr<ContentIndexContextImpl> content_index_context_;
  std::unique_ptr<AttributionManager> attribution_manager_;
  std::unique_ptr<FontAccessManager> font_access_manager_;
  std::unique_ptr<InterestGroupManagerImpl> interest_group_manager_;
  std::unique_ptr<BrowsingTopicsSiteDataManager>
      browsing_topics_site_data_manager_;
  std::unique_ptr<AggregationService> aggregation_service_;
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  std::unique_ptr<CdmStorageManager> cdm_storage_manager_;

  // TODO(crbug.com/40272342): Remove MediaLicenseManager once migration has
  // been completed.
  std::unique_ptr<MediaLicenseManager> media_license_manager_;
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

  // Owning pointer to the SharedStorageManager for this partition.
  std::unique_ptr<storage::SharedStorageManager> shared_storage_manager_;

  // This needs to be declared after `shared_storage_manager_` because
  // `shared_storage_worklet_host` (managed by
  // `shared_storage_worklet_host_manager_`) ultimately stores a raw pointer on
  // it.
  std::unique_ptr<SharedStorageWorkletHostManager>
      shared_storage_worklet_host_manager_;

  // Owning pointer to the `SharedStorageHeaderObserver` for this partition.
  std::unique_ptr<SharedStorageHeaderObserver> shared_storage_header_observer_;

  std::unique_ptr<PrivateAggregationManagerImpl> private_aggregation_manager_;

  std::unique_ptr<CookieDeprecationLabelManagerImpl>
      cookie_deprecation_label_manager_;

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

  // Owns the NetworkContext used to make requests for the StoragePartition.
  // Forward declared so we don't need to include network_context.mojom.h here.
  std::unique_ptr<NetworkContextOwner> network_context_owner_;

  mojo::Receiver<network::mojom::NetworkContextClient>
      network_context_client_receiver_{this};

  // Always valid/non-null after `Initialize()`.
  std::unique_ptr<ReconnectableURLLoaderFactoryForIOThreadWrapper>
      shared_url_loader_factory_for_browser_process_;

  mojo::Remote<cert_verifier::mojom::CertVerifierServiceUpdater>
      cert_verifier_service_updater_;

  // URLLoaderFactory/CookieManager for use in the browser process only.
  // See the method comment for
  // StoragePartition::GetURLLoaderFactoryForBrowserProcess() for
  // more details
  mojo::Remote<network::mojom::CookieManager>
      cookie_manager_for_browser_process_;

  // The list of cors exempt headers that are set on `network_context_`.
  // Initialized in InitNetworkContext() and never updated after then.
  std::vector<std::string> cors_exempt_header_list_;

  // See comments for is_guest().
  bool is_guest_ = false;

  // Track number of running deletion. For test use only.
  int deletion_helpers_running_;

  base::ObserverList<DataRemovalObserver> data_removal_observers_;

  // Called when all deletions are done. For test use only.
  base::OnceClosure on_deletion_helpers_done_callback_;

  // A set of connections to the network service used to notify browser process
  // about cookie reads and writes made by a service worker in this process.
  mojo::UniqueReceiverSet<network::mojom::CookieAccessObserver>
      service_worker_cookie_observers_;

  // A set of connections to the network service used to notify browser process
  // about Trust Token accesses made by a service worker in this process.
  mojo::UniqueReceiverSet<network::mojom::TrustTokenAccessObserver>
      service_worker_trust_token_observers_;

  // A set of connections to the network service used to notify browser process
  // about shared dictionary accesses made by a service worker in this process.
  mojo::UniqueReceiverSet<network::mojom::SharedDictionaryAccessObserver>
      service_worker_shared_dictionary_observers_;

  mojo::ReceiverSet<network::mojom::URLLoaderNetworkServiceObserver,
                    URLLoaderNetworkContext>
      url_loader_network_observers_;

  int next_pending_trust_token_issuance_callback_key_ = 0;

  // Maps frame tokens to NavigationStateKeepAlives. There is one
  // NavigationStateKeepAlive per LocalFrameToken. It's possible to have
  // multiple keep alives per LocalFrameToken (e.g., multiple in-flight
  // navigations per RenderFrameHost), but this map will store the most recent
  // NavigationStateKeepAlive.
  // In the case of multiple navigations for a RenderFrameHost,
  // it is assumed that they are handled in order, with the latest navigation's
  // keep alive storing the state for that RenderFrameHost.
  // Note: This member must be above `keep_alive_handles_receiver_set_`. During
  // destruction, when NavigationStateKeepAlives get removed from the receiver
  // set, they will them remove themselves from
  // `navigation_state_keep_alive_map_`, so this map must still be alive when
  // that happens.
  using TokenNavigationStateKeepAliveMap =
      std::unordered_map<blink::LocalFrameToken,
                         NavigationStateKeepAlive*,
                         blink::LocalFrameToken::Hasher>;
  TokenNavigationStateKeepAliveMap navigation_state_keep_alive_map_;

  // Active keepalive handles for in-flight navigations. They are retained
  // on `StoragePartition` because, by design, they may need to outlive the
  // `RenderFrameHostImpl` that initiated the navigation, but shouldn't be used
  // in a different StoragePartition.
  // Note that this set may contain in-flight navigations for different
  // RenderFrameHosts, and furthermore, there may even be multiple in-flight
  // navigations for a single RenderFrameHost.
  // Lookups should not be done from this set. Accessing PolicyContainerHosts
  // kept alive by NavigationStateKeepAlive should be done through
  // PolicyContainerHost::FromFrameToken.
  mojo::UniqueReceiverSet<blink::mojom::NavigationStateKeepAliveHandle>
      keep_alive_handles_receiver_set_;

#if DCHECK_IS_ON()
  bool on_browser_context_will_be_destroyed_called_ = false;
#endif

  // A copy of the network revocation nonces in `NetworkContext`. It is used for
  // restoring the network revocation states of fenced frames when there is a
  // `NetworkService` crash.
  std::set<base::UnguessableToken> network_revocation_nonces_;

  // We need to delay deleting stale session cookies until after the cookie db
  // has initialized, otherwise we will bypass lazy loading and block.
  // See crbug.com/40285083 for more info.
  base::TimeDelta delete_stale_session_only_cookies_delay_{base::Minutes(1)};

  // We need a delay when removing fenced frame nonces from here and from the
  // network service, to avoid races where a fenced frame could regain network
  // access during destruction. See the comment on
  // `ClearNoncesInNetworkContextAfterDelay` for more info.
  base::TimeDelta clear_nonces_in_network_context_delay_{base::Minutes(1)};
  // Because removing the nonces after a delay is async, we need a callback to
  // execute when the task completes in order to test it.
  base::RepeatingClosure clear_nonces_in_network_context_callback_for_testing_ =
      base::DoNothing();

  base::WeakPtrFactory<StoragePartitionImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_STORAGE_PARTITION_IMPL_H_
