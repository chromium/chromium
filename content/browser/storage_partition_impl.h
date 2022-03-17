// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_STORAGE_PARTITION_IMPL_H_
#define CONTENT_BROWSER_STORAGE_PARTITION_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/services/storage/public/mojom/partition.mojom.h"
#include "components/services/storage/public/mojom/storage_service.mojom-forward.h"
#include "content/browser/background_sync/background_sync_context_impl.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/content_index/content_index_context_impl.h"
#include "content/browser/devtools/devtools_background_services_context_impl.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/notifications/platform_notification_context_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/browser/worker_host/dedicated_worker_service_impl.h"
#include "content/browser/worker_host/shared_worker_service_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/common/trust_tokens.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_settings.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/dom_storage/dom_storage.mojom.h"

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace net {
class IsolationInfo;
}  // namespace net

namespace content {

class AggregationServiceImpl;
class BackgroundFetchContext;
class BlobRegistryWrapper;
class BluetoothAllowedDevicesMap;
class BroadcastChannelService;
class BucketContext;
class CacheStorageControlWrapper;
class ComputePressureManager;
class AttributionManagerImpl;
class CookieStoreManager;
class FileSystemAccessEntryFactory;
class FileSystemAccessManagerImpl;
class FontAccessManagerImpl;
class GeneratedCodeCacheContext;
class HostZoomLevelContext;
class IndexedDBControlWrapper;
class InterestGroupManagerImpl;
class LockManager;
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
class MediaLicenseManager;
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)
class NativeIOContextImpl;
class PaymentAppContextImpl;
class PrefetchURLLoaderService;
class PushMessagingContext;
class QuotaContext;
class SharedStorageWorkletHostManager;

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

  // Forces Storage Service instances to be run in-process, ignoring the
  // StorageServiceOutOfProcess feature setting.
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
  void OverrideAggregationServiceForTesting(
      std::unique_ptr<AggregationServiceImpl> aggregation_service);

  // Returns the StoragePartitionConfig that represents this StoragePartition.
  const StoragePartitionConfig& GetConfig();

  // StoragePartition interface.
  base::FilePath GetPath() override;
  base::FilePath GetBucketBasePath() override;
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
  mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
  CreateURLLoaderNetworkObserverForFrame(int process_id,
                                         int routing_id) override;
  mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
  CreateURLLoaderNetworkObserverForNavigationRequest(
      int frame_tree_id) override;
  storage::QuotaManager* GetQuotaManager() override;
  BackgroundSyncContextImpl* GetBackgroundSyncContext() override;
  storage::FileSystemContext* GetFileSystemContext() override;
  storage::DatabaseTracker* GetDatabaseTracker() override;
  DOMStorageContextWrapper* GetDOMStorageContext() override;
  storage::mojom::LocalStorageControl* GetLocalStorageControl() override;
  LockManager* GetLockManager();  // override; TODO: Add to interface
  // TODO(https://crbug.com/1218540): Add this method to the StoragePartition
  // interface, which would also require making SharedStorageWorkletHostManager
  // an interface accessible in //content/public/.
  SharedStorageWorkletHostManager*
  GetSharedStorageWorkletHostManager();  // override;
  storage::mojom::IndexedDBControl& GetIndexedDBControl() override;
  FileSystemAccessEntryFactory* GetFileSystemAccessEntryFactory() override;
  storage::mojom::CacheStorageControl* GetCacheStorageControl() override;
  ServiceWorkerContextWrapper* GetServiceWorkerContext() override;
  DedicatedWorkerServiceImpl* GetDedicatedWorkerService() override;
  SharedWorkerServiceImpl* GetSharedWorkerService() override;
  GeneratedCodeCacheContext* GetGeneratedCodeCacheContext() override;
  DevToolsBackgroundServicesContextImpl* GetDevToolsBackgroundServicesContext()
      override;
  ContentIndexContextImpl* GetContentIndexContext() override;
  NativeIOContext* GetNativeIOContext() override;
  HostZoomMap* GetHostZoomMap() override;
  HostZoomLevelContext* GetHostZoomLevelContext() override;
  ZoomLevelDelegate* GetZoomLevelDelegate() override;
  PlatformNotificationContextImpl* GetPlatformNotificationContext() override;
  InterestGroupManager* GetInterestGroupManager() override;
  BrowsingTopicsSiteDataManager* GetBrowsingTopicsSiteDataManager() override;
  leveldb_proto::ProtoDatabaseProvider* GetProtoDatabaseProvider() override;
  void SetProtoDatabaseProvider(
      std::unique_ptr<leveldb_proto::ProtoDatabaseProvider> proto_db_provider)
      override;
  leveldb_proto::ProtoDatabaseProvider* GetProtoDatabaseProviderForTesting()
      override;
  void ClearDataForOrigin(uint32_t remove_mask,
                          uint32_t quota_storage_remove_mask,
                          const GURL& storage_origin,
                          base::OnceClosure callback) override;
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

  base::WeakPtr<StoragePartition> GetWeakPtr();
  BackgroundFetchContext* GetBackgroundFetchContext();
  PaymentAppContextImpl* GetPaymentAppContext();
  BroadcastChannelService* GetBroadcastChannelService();
  BluetoothAllowedDevicesMap* GetBluetoothAllowedDevicesMap();
  BlobRegistryWrapper* GetBlobRegistry();
  PrefetchURLLoaderService* GetPrefetchURLLoaderService();
  CookieStoreManager* GetCookieStoreManager();
  FileSystemAccessManagerImpl* GetFileSystemAccessManager();
  BucketContext* GetBucketContext();
  QuotaContext* GetQuotaContext();
  AttributionManagerImpl* GetAttributionManager();
  void SetFontAccessManagerForTesting(
      std::unique_ptr<FontAccessManagerImpl> font_access_manager);
  ComputePressureManager* GetComputePressureManager();
  std::string GetPartitionDomain();
  AggregationServiceImpl* GetAggregationService();
  FontAccessManagerImpl* GetFontAccessManager();
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  MediaLicenseManager* GetMediaLicenseManager();
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

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
                             OnFileUploadRequestedCallback callback) override;
  void OnCanSendReportingReports(
      const std::vector<url::Origin>& origins,
      OnCanSendReportingReportsCallback callback) override;
  void OnCanSendDomainReliabilityUpload(
      const GURL& origin,
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
  void OnTrustTokenIssuanceDivertedToSystem(
      network::mojom::FulfillTrustTokenIssuanceRequestPtr request,
      OnTrustTokenIssuanceDivertedToSystemCallback callback) override;
  void OnCanSendSCTAuditingReport(
      OnCanSendSCTAuditingReportCallback callback) override;
  void OnNewSCTAuditingReportSent() override;

  // network::mojom::URLLoaderNetworkServiceObserver interface.
  void OnSSLCertificateError(const GURL& url,
                             int net_error,
                             const net::SSLInfo& ssl_info,
                             bool fatal,
                             OnSSLCertificateErrorCallback response) override;
  void OnCertificateRequested(
      const absl::optional<base::UnguessableToken>& window_id,
      const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
      mojo::PendingRemote<network::mojom::ClientCertificateResponder>
          cert_responder) override;
  void Clone(
      mojo::PendingReceiver<network::mojom::URLLoaderNetworkServiceObserver>
          listener) override;
  void OnAuthRequired(
      const absl::optional<base::UnguessableToken>& window_id,
      uint32_t request_id,
      const GURL& url,
      bool first_auth_attempt,
      const net::AuthChallengeInfo& auth_info,
      const scoped_refptr<net::HttpResponseHeaders>& head_headers,
      mojo::PendingRemote<network::mojom::AuthChallengeResponder>
          auth_challenge_responder) override;
  void OnClearSiteData(
      const GURL& url,
      const std::string& header_value,
      int load_flags,
      const absl::optional<net::CookiePartitionKey>& cookie_partition_key,
      OnClearSiteDataCallback callback) override;
  void OnLoadingStateUpdate(network::mojom::LoadInfoPtr info,
                            OnLoadingStateUpdateCallback callback) override;
  void OnDataUseUpdate(int32_t network_traffic_annotation_id_hash,
                       int64_t recv_bytes,
                       int64_t sent_bytes) override;

  scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter() {
    return url_loader_factory_getter_;
  }

  // Can return nullptr while `this` is being destroyed.
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

  // Tracks whether this StoragePartition is for guests (e.g., for a <webview>
  // tag).  This is needed to properly create a SiteInstance for a
  // service worker or a shared worker in a guest. Typically one would use the
  // script URL of the worker (e.g., "https://example.com/sw.js"), but if this
  // StoragePartition is for guests, one must create the SiteInstance via
  // guest-specific helpers that ensure that the worker stays in the same
  // StoragePartition.
  void set_is_guest() { is_guest_ = true; }
  bool is_guest() const { return is_guest_; }

  // Use the network context to retrieve the origin policy manager.
  network::mojom::OriginPolicyManager*
  GetOriginPolicyManagerForBrowserProcess();

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

  mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
  CreateAuthCertObserverForServiceWorker();

  std::vector<std::string> GetCorsExemptHeaderList();

  // Empties the collection `pending_trust_token_issuance_callbacks_` of
  // callbacks pending responses from `local_trust_token_fulfiller_`, providing
  // each callback a suitable error response.
  void OnLocalTrustTokenFulfillerConnectionError();

  void OpenLocalStorageForProcess(
      int process_id,
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<blink::mojom::StorageArea> receiver);
  void BindSessionStorageAreaForProcess(
      int process_id,
      const blink::StorageKey& storage_key,
      const std::string& namespace_id,
      mojo::PendingReceiver<blink::mojom::StorageArea> receiver);

  class URLLoaderNetworkContext {
   public:
    enum class Type {
      // A network context for a RenderFrameHost.
      kRenderFrameHostContext,
      // A network context for a navigation request or a service worker.
      kNavigationRequestContext,
    };

    ~URLLoaderNetworkContext();

    // Creates a URLLoaderNetworkContext for the render frame host.
    static URLLoaderNetworkContext CreateForRenderFrameHost(
        GlobalRenderFrameHostId global_render_frame_host_id);

    // Creates a URLLoaderNetworkContext for the navigation request.
    static URLLoaderNetworkContext CreateForNavigation(int frame_tree_node_id);

    // Returns true if `type` is `kNavigationRequestContext`.
    bool IsNavigationRequestContext();

    Type type() const { return type_; }

    GlobalRenderFrameHostId render_frame_host_id() const {
      return render_frame_host_id_;
    }

    int frame_tree_node_id() const { return frame_tree_node_id_; }

   private:
    URLLoaderNetworkContext(URLLoaderNetworkContext::Type type,
                            GlobalRenderFrameHostId global_render_frame_host_id,
                            int frame_tree_node_id);

    Type type_;
    // Used for kRenderFrameHostContext.
    GlobalRenderFrameHostId render_frame_host_id_;
    // Used for kNavigationRequestContext.
    int frame_tree_node_id_;
  };

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

  // `relative_partition_path` is the relative path under `profile_path` to the
  // StoragePartition's on-disk-storage.
  //
  // If `in_memory` is true, the `relative_partition_path` is (ab)used as a way
  // of distinguishing different in-memory partitions, but nothing is persisted
  // on to disk.
  //
  // Initialize() must be called on the StoragePartitionImpl before using it.
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

  // Called to initialize `network_context_` when `GetNetworkContext()` is
  // first called or there is an error.
  void InitNetworkContext();

  bool is_in_memory() { return config_.in_memory(); }

  network::mojom::URLLoaderFactory*
  GetURLLoaderFactoryForBrowserProcessInternal(bool corb_enabled);

  // If `local_trust_token_fulfiller_` is bound, returns immediately.
  //
  // Otherwise, if it's supported by the environment, attempts to bind
  // `local_trust_token_fulfiller_`. In this case,
  // local_trust_token_fulfiller_.is_bound() will return true after this method
  // returns. This does NOT guarantee that `local_trust_token_fulfiller_` will
  // ever find an implementation of the interface to talk to. If downstream code
  // rejects the connection, this will be reflected asynchronously by a call to
  // OnLocalTrustTokenFulfillerConnectionError.
  void ProvisionallyBindUnboundLocalTrustTokenFulfillerIfSupportedBySystem();

  // Raw pointer that should always be valid. The BrowserContext owns the
  // StoragePartitionImplMap which then owns StoragePartitionImpl. When the
  // BrowserContext is destroyed, `this` will be destroyed too.
  raw_ptr<BrowserContext> browser_context_;

  const base::FilePath partition_path_;

  // `config_` and `relative_partition_path_` are cached from
  // `StoragePartitionImpl::Create()` in order to re-create `NetworkContext`.
  const StoragePartitionConfig config_;
  const base::FilePath relative_partition_path_;

  // Until a StoragePartitionImpl is initialized using Initialize(), only
  // querying its path abd BrowserContext is allowed.
  bool initialized_ = false;

  mojo::Remote<storage::mojom::Partition> remote_partition_;
  scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter_;
  scoped_refptr<QuotaContext> quota_context_;
  scoped_refptr<storage::QuotaManager> quota_manager_;
  scoped_refptr<storage::FileSystemContext> filesystem_context_;
  scoped_refptr<storage::DatabaseTracker> database_tracker_;
  scoped_refptr<DOMStorageContextWrapper> dom_storage_context_;
  std::unique_ptr<LockManager> lock_manager_;
  std::unique_ptr<SharedStorageWorkletHostManager>
      shared_storage_worklet_host_manager_;
  std::unique_ptr<IndexedDBControlWrapper> indexed_db_control_wrapper_;
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
  std::unique_ptr<PrefetchURLLoaderService> prefetch_url_loader_service_;
  std::unique_ptr<CookieStoreManager> cookie_store_manager_;
  scoped_refptr<BucketContext> bucket_context_;
  scoped_refptr<GeneratedCodeCacheContext> generated_code_cache_context_;
  scoped_refptr<DevToolsBackgroundServicesContextImpl>
      devtools_background_services_context_;
  scoped_refptr<FileSystemAccessManagerImpl> file_system_access_manager_;
  std::unique_ptr<leveldb_proto::ProtoDatabaseProvider>
      proto_database_provider_;
  scoped_refptr<ContentIndexContextImpl> content_index_context_;
  scoped_refptr<NativeIOContextImpl> native_io_context_;
  std::unique_ptr<AttributionManagerImpl> attribution_manager_;
  std::unique_ptr<FontAccessManagerImpl> font_access_manager_;
  std::unique_ptr<InterestGroupManagerImpl> interest_group_manager_;
  std::unique_ptr<BrowsingTopicsSiteDataManager>
      browsing_topics_site_data_manager_;
  std::unique_ptr<AggregationServiceImpl> aggregation_service_;
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  std::unique_ptr<MediaLicenseManager> media_license_manager_;
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

  // TODO(crbug.com/1205695): ComputePressureManager should live elsewher. The
  //                          Compute Pressure API does not store data.
  std::unique_ptr<ComputePressureManager> compute_pressure_manager_;

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
  // by `network_context_owner_`.
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

  mojo::ReceiverSet<network::mojom::URLLoaderNetworkServiceObserver,
                    URLLoaderNetworkContext>
      url_loader_network_observers_;

  // `local_trust_token_fulfiller_` provides responses to certain Trust Tokens
  // operations, for instance via the content embedder calling into a system
  // service ("platform-provided Trust Tokens operations").
  //
  // Binding the interface might not succeed, and failures could involve costly
  // operations in other processes, so we attempt at most once to bind it.
  bool attempted_to_bind_local_trust_token_fulfiller_ = false;
  mojo::Remote<mojom::LocalTrustTokenFulfiller> local_trust_token_fulfiller_;
  // Maintain pending callbacks provided to OnTrustTokenIssuanceDivertedToSystem
  // so that we can provide them error responses if the Mojo pipe breaks. One
  // likely common case where this happens is when the content embedder declines
  // to provide an implementation when we attempt to bind the
  // LocalTrustTokenFulfiller interface, for instance because the embedder
  // hasn't implemented support for mediating Trust Tokens operations.
  base::flat_map<int, OnTrustTokenIssuanceDivertedToSystemCallback>
      pending_trust_token_issuance_callbacks_;
  int next_pending_trust_token_issuance_callback_key_ = 0;

  base::WeakPtrFactory<StoragePartitionImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_STORAGE_PARTITION_IMPL_H_
